// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include <fstream>

#include "defs.h"

#include "picojson.h"
namespace pj = picojson;

#include "tinyxml2.h"
#include "tinyprocess.h"


namespace tag
{
	static constexpr const char* MKVMERGE_PROGRAM       = "mkvmerge";
	static constexpr const char* MKVEXTRACT_PROGRAM     = "mkvextract";
	static constexpr const char* MKVPROPEDIT_PROGRAM    = "mkvpropedit";

	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		util::error(fmt, args...);
		if(config::shouldStopOnError())
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
		bool doNotReattach = false;
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

				// if this is already a cover image, then just replace it -- we don't need to extract and
				// reattach it. prevents cover art from spamming the file when you run mkvtaginator multiple times.
				if(!config::disableSmartReplaceCoverArt() && util::match(attachment.mime, "image/jpeg", "image/png")
					&& util::match(attachment.name, "cover", "cover.jpg", "cover.jpeg", "cover.png"))
				{
					attachment.doNotReattach = true;
				}
				else if(!config::isDryRun())
				{
					// extract it.
					tinyproclib::Process proc(zpr::sprint("%s -q \"%s\" attachments %d:%s", MKVEXTRACT_PROGRAM,
						filepath.string(), attachment.id, attachment.extractedFile));

					proc.get_exit_status();
					cleanupList.push_back(attachment.extractedFile);
				}

				break;
			}
		}

		return attachment;
	}


	// { title, xmlfilename, xml }
	static std::tuple<GenericMetadata, std::string, tinyxml2::XMLDocument*> getMetadataXML(const std::fs::path& filepath,
		std::vector<std::string>& coverArtNames)
	{
		// try tv series
		if(!config::getManualSeriesId().empty() || config::getManualMovieId().empty())
		{
			auto [ series, season, episode, title ] = parseTVShow(filepath.stem().string());

			// for this, we need season/episode info, so even if you give the series id there's no point.
			if(!series.empty())
			{
				if(int x = config::getSeasonNumber(); x != -1)
					season = x;

				if(int x = config::getEpisodeNumber(); x != -1)
					episode = x;

				// if there's no season, assume 1.
				if(season == -1)
					season = 1;

				// auto metadata = tvdb::fetchEpisodeMetadata(series, season, episode, title, config::getManualSeriesId());
				auto metadata = tvmaze::fetchEpisodeMetadata(series, season, episode, title, config::getManualSeriesId());
				if(!metadata.valid)
				{
					error("failed to find metadata");
					return { GenericMetadata(), "", nullptr };
				}

				util::info("tv: %s S%02dE%02d%s", metadata.seriesMeta.dbName, metadata.seasonNumber,
					metadata.episodeNumber, metadata.dbName.empty() ? "" : zpr::sprint(" - %s", metadata.dbName));

				auto xml = serialiseMetadata(metadata);

				coverArtNames.insert(coverArtNames.begin(), "season");
				coverArtNames.insert(coverArtNames.begin(), "Season");
				coverArtNames.insert(coverArtNames.begin(), zpr::sprint("season%d", metadata.seasonNumber));
				coverArtNames.insert(coverArtNames.begin(), zpr::sprint("Season%d", metadata.seasonNumber));
				coverArtNames.insert(coverArtNames.begin(), zpr::sprint("season%02d", metadata.seasonNumber));
				coverArtNames.insert(coverArtNames.begin(), zpr::sprint("Season%02d", metadata.seasonNumber));

				metadata.normalTitle = zpr::sprint("%s S%02dE%02d%s", metadata.seriesMeta.name, metadata.seasonNumber,
					metadata.episodeNumber, metadata.name.empty() ? "" : zpr::sprint(" - %s", metadata.name));

				metadata.canonicalTitle = zpr::sprint("%s S%02dE%02d", metadata.seriesMeta.name, metadata.seasonNumber,
					metadata.episodeNumber);

				metadata.episodeTitle = metadata.name;

				return {
					static_cast<GenericMetadata>(metadata),
					zpr::sprint(".tmp-mkvinator-tags-s%02d-e%02d.xml", metadata.seasonNumber, metadata.episodeNumber),
					xml
				};
			}
		}


		if(!config::getManualMovieId().empty() || config::getManualSeriesId().empty())
		{
			// try movie
			auto [ title, year ] = parseMovie(filepath.stem().string());

			// for movies, as long as we have the ID it's ok.
			if(title.empty() && config::getManualMovieId().empty())
				goto fail;

			auto metadata = moviedb::fetchMovieMetadata(title, year, config::getManualMovieId());
			if(!metadata.valid)
			{
				error("failed to find metadata");
				return { GenericMetadata(), "", nullptr };
			}

			util::info("movie: %s (%d)", metadata.dbTitle, metadata.year);

			metadata.normalTitle = zpr::sprint("%s", metadata.title);
			metadata.canonicalTitle = zpr::sprint("%s (%d)", metadata.title, metadata.year);

			auto xml = serialiseMetadata(metadata);
			return {
				static_cast<GenericMetadata>(metadata),
				zpr::sprint(".tmp-mkvinator-tags-movie-%s.xml", metadata.id),
				xml
			};
		}

	fail:
		error("unparsable filename '%s'", filepath.filename().string());
		return { GenericMetadata(), "", nullptr };
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
		if(auto c = config::getManualCoverPath(); !c.empty())
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

		if(cover.empty() && !config::disableAutoCoverSearch())
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

			// then, relative to the current directory:
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

			// then, relative to the output folder:
			if(cover.empty() && !config::getOutputFolder().empty())
			{
				auto of = std::fs::path(config::getOutputFolder());
				for(const auto& x : tries)
				{
					if(std::fs::exists(of / x))
					{
						cover = of / x;
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
		arguments.push_back(zpr::sprint("\"cover.%s\"", cover.extension() == ".png" ? "png" : "jpg"));
		arguments.push_back("--attachment-mime-type");
		arguments.push_back(zpr::sprint("\"image/%s\"", cover.extension() == ".png" ? "png" : "jpeg"));

		// if there was a first attachment, replace it instead.
		if(firstAttachment.id > 0)
		{
			arguments.push_back("--replace-attachment");
			arguments.push_back(zpr::sprint("%d:\"%s\"", firstAttachment.id, cover.string()));

			if(!firstAttachment.doNotReattach)
			{
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
				util::log("replacing existing cover art in output file");
			}
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

		if(!config::isDryRun())
		{
			if(std::fs::exists(outpath) && config::shouldDeleteExistingOutput() && !std::fs::equivalent(outpath, filepath))
				std::fs::remove(outpath);

			if(!std::fs::exists(outpath))
			{
				util::log("copying output file");
				auto x = std::fs::copy_file(filepath, outpath);
				if(!x)
				{
					error("%serror:%s failed to copy file to '%s'", outpath.string(),
						COLOUR_RED_BOLD, COLOUR_RESET);
					return "";
				}
			}
			else
			{
				util::log("updating existing output in-place");
			}
		}

		return outpath.string();
	}






	bool tagOneFile(const std::fs::path& filepath)
	{
		std::fs::path inputFile = filepath;



		std::vector<std::string> arguments;
		std::vector<std::string> filesToCleanup;

		auto firstAttachment = extractFirstAttachmentIfNecessary(filepath, filesToCleanup);

		arguments.push_back(MKVPROPEDIT_PROGRAM);
		arguments.push_back(zpr::sprint("\"%s\"", inputFile.string()));



		std::vector<std::string> coverArtNames = {
			"cover",
			"poster"
		};

		// get the metadata
		auto [ meta, xmlname, xml ] = getMetadataXML(filepath, coverArtNames);
		{
			if(!xml) return false;

			writeXML(xmlname, xml);

			arguments.push_back("--tags");
			arguments.push_back(zpr::sprint("all:\"%s\"", xmlname));

			filesToCleanup.push_back(xmlname);

			// set the overall title
			arguments.push_back("--edit");
			arguments.push_back("info");
			arguments.push_back("--set");
			arguments.push_back(zpr::sprint("\"title=%s\"", meta.normalTitle));
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
		if(auto out = config::getOutputFolder(); !out.empty())
		{
			if(auto outfile = createOutputFile(filepath, out); !outfile.empty())
			{
				arguments[1] = zpr::sprint("\"%s\"", outfile);
				inputFile = outfile;
			}
			else
			{
				return false;
			}
		}



		auto cmdline = util::listToString(arguments, util::identity(), false, " ");
		if(config::isDryRun())
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

				if(config::shouldStopOnError())
					exit(-1);

				return false;
			}
		}

		// finally, after all this, we can rename the file.
		if(config::shouldRenameFiles())
		{
			auto path = inputFile;

			auto newname = meta.canonicalTitle;
			if(!config::shouldRenameWithoutEpisodeTitle() && !meta.episodeTitle.empty())
				newname += zpr::sprint(" - %s", meta.episodeTitle);

			newname = util::sanitiseFilename(newname);

			auto newpath = path.parent_path() / zpr::sprint("%s.mkv", newname);

			if(!config::isDryRun())
			{
				util::log("renaming file: '%s'", newpath.filename().string());
				std::fs::rename(path, newpath);
			}
			else
			{
				util::log("dryrun: rename '%s' -> '%s'", std::fs::relative(path).string(),
					std::fs::relative(newpath).string());
			}
		}


		for(const auto& f : filesToCleanup)
		{
			if(std::fs::exists(f))
				std::fs::remove(f);
		}

		util::info("done");
		return true;
	}
}







