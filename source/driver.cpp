// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include <fstream>

#include "defs.h"

#include "picojson.h"
namespace pj = picojson;

#include "tinyxml2.h"
#include "tinyprocesslib/tinyprocess.h"


namespace std
{
	namespace fs = filesystem;
}

namespace driver
{
	static constexpr const char* MKVMERGE_PROGRAM       = "mkvmerge";
	static constexpr const char* MKVEXTRACT_PROGRAM     = "mkvextract";
	static constexpr const char* MKVPROPEDIT_PROGRAM    = "mkvpropedit";

	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		util::error(fmt, args...);
		if(args::isStopOnError())
		{
			util::error("stopping on first error");
			exit(-1);
		}
	}

	struct TmpAttachment
	{
		int id = 0;
		std::string name;
		std::string mime;

		std::string extractedFile;
	};

	static TmpAttachment extractFirstAttachmentIfNecessary(const std::fs::path& filepath, std::vector<std::string>& cleanupList)
	{
		// the strategy here is to extract the first attachment,
		// replace it with the cover image, then append it back to the end.
		// this keeps the cover art as the first attachment in the mkv. (which is apparently important?)


		TmpAttachment attachment;

		std::string sout;

		tinyproclib::Process proc(zpr::sprint("%s --identify \"%s\"", MKVMERGE_PROGRAM, filepath.string()), "",
			[&sout](const char* bytes, size_t n) {
				sout += std::string(bytes, n);
			}
		);

		proc.get_exit_status();

		auto lines = util::splitString(sout);
		for(const auto& l : lines)
		{
			if(l.find("Attachment ID") == 0)
			{
				auto regex = std::regex("Attachment ID (\\d+): type '(.+?)'.+? file name '(.+?)'");

				std::smatch sm;
				std::regex_match(l, sm, regex);
				assert(sm.size() == 4);

				attachment.id = std::stoi(sm[1]);
				attachment.mime = sm[2];
				attachment.name = sm[3];
				attachment.extractedFile = ".tmp-mkvinator-attachment-file";

				// extract it.
				tinyproclib::Process proc(zpr::sprint("%s -q \"%s\" attachments %d:%s", MKVEXTRACT_PROGRAM,
					filepath.string(), attachment.id, attachment.extractedFile));

				proc.get_exit_status();
				cleanupList.push_back(attachment.extractedFile);

				break;
			}
		}

		return attachment;
	}



	static std::pair<std::string, tinyxml2::XMLDocument*> getMetadataXML(const std::fs::path& filepath,
		std::vector<std::string>& coverArtNames)
	{
		// try tv series
		auto [ series, season, episode, title ] = misc::parseTVShow(filepath.stem().string());
		if(!series.empty())
		{
			auto metadata = tvdb::fetchEpisodeMetadata(series, season, episode, title, args::getManualSeriesId());
			if(!metadata.valid)
			{
				error("failed to find metadata");
				return { "", nullptr };
			}

			util::info("tv:  %s S%02dE%02d - %s", metadata.seriesMeta.name, metadata.seasonNumber,
				metadata.episodeNumber, metadata.name);

			auto xml = tags::serialiseMetadata(metadata);

			coverArtNames.push_back("season");
			coverArtNames.push_back("Season");
			coverArtNames.push_back(zpr::sprint("season%d", metadata.seasonNumber));
			coverArtNames.push_back(zpr::sprint("Season%d", metadata.seasonNumber));
			coverArtNames.push_back(zpr::sprint("season%02d", metadata.seasonNumber));
			coverArtNames.push_back(zpr::sprint("Season%02d", metadata.seasonNumber));

			return {
				zpr::sprint(".tmp-mkvinator-tags-s%02d-e%02d.xml", metadata.seasonNumber, metadata.episodeNumber),
				xml
			};
		}
		else
		{
			// try movie
			return { "", nullptr };
		}
	}

	static void writeXML(const std::string& path, tinyxml2::XMLDocument* xml)
	{
		auto printer = new tinyxml2::XMLPrinter();
		xml->Print(printer);

		auto out = std::ofstream(path);
		out.write(printer->CStr(), printer->CStrSize());

		out.close();

		delete printer;
		delete xml;
	}


	static std::fs::path findCoverArt(const std::fs::path& filepath, const std::vector<std::string>& coverArtNames)
	{
		std::vector<std::string> exts = { "png", "jpg", "jpeg" };
		auto tries = util::map(util::cartesian(coverArtNames, exts), [](auto x) -> auto {
			return zpr::sprint("%s.%s", x.first, x.second);
		});

		std::fs::path cover;
		if(auto c = args::getManualCoverPath(); !c.empty())
		{
			// prefer the cover art relative to the file path.
			if(std::fs::exists(filepath.parent_path() / c))
			{
				cover = filepath.parent_path() / c;
			}
			else
			{
				if(!std::fs::exists(c))
				{
					// this isn't actually fatal, so don't exit.
					util::error("skipping nonexistent cover art file '%s'", c);
				}
				else
				{
					cover = c;
				}
			}
		}

		if(cover.empty() && !args::isNoAutoCover())
		{
			// try relative to the file first.
			for(const auto& x : tries)
			{
				if(std::fs::exists(filepath.parent_path() / x))
				{
					cover = filepath.parent_path() / x;
					break;
				}
			}

			if(cover.empty())
			{
				for(const auto& x : tries)
				{
					if(std::fs::exists(x))
					{
						cover = x;
						break;
					}
				}
			}
		}

		return cover;
	}

	static std::vector<std::string> attachCoverArt(const std::fs::path& filepath, const std::fs::path& cover,
		const TmpAttachment& firstAttachment)
	{
		std::vector<std::string> arguments;

		arguments.push_back("--attachment-name");
		arguments.push_back("\"cover\"");
		arguments.push_back("--attachment-mime-type");
		arguments.push_back(zpr::sprint("\"image/%s\"", cover.extension() == "png" ? "png" : "jpeg"));

		// if there was a first attachment, replace it instead.
		if(firstAttachment.id > 0)
		{
			arguments.push_back("--replace-attachment");
			arguments.push_back(zpr::sprint("%d:\"%s\"", firstAttachment.id, cover.string()));

			// reattach the first one at the end.
			arguments.push_back("--attachment-name");
			arguments.push_back(zpr::sprint("\"%s\"", firstAttachment.name));
			arguments.push_back("--attachment-mime-type");
			arguments.push_back(zpr::sprint("\"%s\"", firstAttachment.mime));
			arguments.push_back("--add-attachment");
			arguments.push_back(zpr::sprint("\"%s\"", firstAttachment.extractedFile));
		}
		else
		{
			// else, we can just append.
			arguments.push_back("--add-attachment");
			arguments.push_back(zpr::sprint("\"%s\"", cover.string()));
		}

		return arguments;
	}

	static std::string createOutputFile(const std::fs::path& filepath, const std::string& outname)
	{
		auto outpath = std::fs::canonical(std::fs::path(outname)) / filepath.filename();

		if(!args::isDryRun())
		{
			if(std::fs::exists(outpath) && args::isDeletingExistingOutput())
				std::fs::remove(outpath);

			if(!std::fs::exists(outpath))
			{
				util::log("copying output file");
				auto x = std::fs::copy_file(filepath, outpath);
				if(!x)
				{
					error("error: failed to copy file to '%s'", outpath.string());
					return "";
				}
			}
			else
			{
				util::log("updating existing output in-place");
			}
		}

		return zpr::sprint("\"%s\"", outpath.string());
	}






	bool processOneFile(const std::fs::path& filepath)
	{
		util::log("%s", filepath.filename().string()); util::indent_log();
		defer(zpr::println(""));
		defer(util::unindent_log());


		std::vector<std::string> arguments;
		std::vector<std::string> filesToCleanup;

		auto firstAttachment = extractFirstAttachmentIfNecessary(filepath, filesToCleanup);

		arguments.push_back(MKVPROPEDIT_PROGRAM);
		arguments.push_back(zpr::sprint("\"%s\"", filepath.string()));

		std::vector<std::string> coverArtNames = {
			"cover",
			"poster"
		};

		// get the metadata
		auto [ xmlname, xml ] = getMetadataXML(filepath, coverArtNames);
		{
			if(!xml) return false;

			writeXML(xmlname, xml);

			arguments.push_back("--tags");
			arguments.push_back(zpr::sprint("all:\"%s\"", xmlname));

			filesToCleanup.push_back(xmlname);
		}


		// get the cover art
		{
			auto cover = findCoverArt(filepath, coverArtNames);

			if(!cover.empty())
			{
				util::info("art: %s", cover.string());

				auto args = attachCoverArt(filepath, cover, firstAttachment);
				arguments.insert(arguments.end(), args.begin(), args.end());
			}
		}

		// if we specified an alternative output path,
		// copy the file over, and use that as the input to mkvpropedit instead.
		if(auto out = args::getOutputFolder(); !out.empty())
		{
			// this will quote it for us.
			if(auto outfile = createOutputFile(filepath, out); !outfile.empty())
				arguments[1] = outfile;

			else
				return false;
		}



		auto cmdline = util::listToString(arguments, util::identity(), false, " ");
		if(args::isDryRun())
		{
			util::log("dryrun: cmdline would have been:");
			util::info(cmdline);
		}
		else
		{
			std::string sout;
			std::string serr;

			tinyproclib::Process proc(cmdline, "", [&sout](const char* bytes, size_t n) {
				sout += std::string(bytes, n);
			}, [&serr](const char* bytes, size_t n) {
				serr += std::string(bytes, n);
			});

			// note: this waits for the process to finish.
			int status = proc.get_exit_status();

			if(status != 0)
			{
				util::error("mkvpropedit returned non-zero (status = %d)\n", status);
				util::error("cmdline was: %s\n", cmdline.c_str());

				if(!sout.empty()) util::error("%s\n", sout);
				if(!serr.empty()) util::error("%s\n", serr);

				if(args::isStopOnError())
					exit(-1);

				return false;
			}
		}

		for(const auto& f : filesToCleanup)
		{
			if(std::fs::exists(f))
				std::fs::remove(f);
		}

		util::log("done");
		return true;
	}



	void createOutputFolder()
	{
		if(auto out = args::getOutputFolder(); !out.empty())
		{
			auto path = std::fs::path(out);
			if(!std::fs::exists(path))
			{
				bool res = std::fs::create_directory(path);
				if(res) { util::info("creating output folder '%s'", out); }
				else    { util::error("failed to create output folder '%s'", out); exit(-1); }
			}
			else if(!std::fs::is_directory(path))
			{
				util::error("error: specified output path '%s' is not a directory", out);
				exit(-1);
			}
		}
	}



	std::vector<std::fs::path> collectFiles(const std::vector<std::string>& files)
	{
		std::vector<std::fs::path> ret;

		for(auto file : files)
		{
			auto filepath = std::fs::path(file);

			if(!std::fs::exists(filepath))
			{
				error("skipping nonexistent file '%s'", filepath.string());
				continue;
			}
			else if(filepath.extension() != ".mkv")
			{
				error("ignoring non-mkv file (extension was '%s')",
					filepath.extension().string());
				continue;
			}

			if(auto out = args::getOutputFolder(); !out.empty())
			{
				if(std::fs::equivalent(filepath.parent_path(), std::fs::path(out)))
				{
					error("skipping input file overlapping with output folder");
					continue;
				}
			}

			ret.push_back(filepath);
		}

		return ret;
	}
}







