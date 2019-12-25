// main.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include <fstream>
#include <filesystem>

#include "defs.h"
#include "cpr/cpr.h"

#include "picojson.h"
namespace pj = picojson;

#include "tinyprocesslib/tinyprocess.h"

namespace std
{
	namespace fs = std::filesystem;
}

int main(int argc, char** argv)
{
	auto files = args::parseCmdLineOpts(argc, argv);
	util::log("received %zu %s", files.size(), util::plural("file", files.size()));
	if(!args::getManualSeriesId().empty())
	{
		util::info("skipping search: using series id '%s'", args::getManualSeriesId());
	}

	std::string specCoverFile;
	if(auto c = args::getManualCoverPath(); !c.empty())
	{
		if(!std::fs::exists(c))
		{
			util::error("error: cover-art file '%s' does not exist", c);
			exit(-1);
		}
		else
		{
			specCoverFile = c;
		}
	}


	if(auto out = args::getOutputFolder(); !out.empty())
	{
		auto path = std::fs::path(out);
		if(!std::fs::exists(path))
		{
			std::fs::create_directory(path);
			util::info("creating output folder '%s'", out);
		}
		else if(!std::fs::is_directory(path))
		{
			util::error("error: specified output path '%s' is not a directory", out);
			exit(-1);
		}
	}





	tvdb::login();

	size_t doneFiles = 0;
	for(auto file : files)
	{
		auto filepath = std::fs::path(file);

		util::log("%s", filepath.filename().string()); util::indent_log();
		defer(zpr::println(""));
		defer(util::unindent_log());

		if(!std::fs::exists(filepath))
		{
			util::error("error: nonexistent file '%s'", filepath.string());
			continue;
		}
		else if(filepath.extension() != ".mkv")
		{
			util::error("error: ignoring non-mkv file (extension was '%s')",
				filepath.extension().string());
			continue;
		}

		if(auto out = args::getOutputFolder(); !out.empty())
		{
			if(std::fs::equivalent(filepath.parent_path(), std::fs::path(out)))
			{
				util::error("error: output folder cannot overlap with input file");
				continue;
			}
		}

		auto newpath = std::fs::canonical(args::getOutputFolder()) / filepath;

		// the strategy here is to extract the first attachment,
		// replace it with the cover image, then append it back to the end.
		// this keeps the cover art as the first attachment in the mkv. (which is apparently important?)

		struct {
			int id = 0;
			std::string name;
			std::string mime;

			std::string extractedFile;
		} firstAttachment;

		std::vector<std::string> filesToCleanup;

		{
			std::string sout;

			tinyproclib::Process proc(zpr::sprint("mkvmerge --identify \"%s\"", filepath.string()), "",
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

					firstAttachment.id = std::stoi(sm[1]);
					firstAttachment.mime = sm[2];
					firstAttachment.name = sm[3];
					firstAttachment.extractedFile = ".tmp-mkvinator-attachment-file";

					// extract it.
					tinyproclib::Process proc(zpr::sprint("mkvextract -q \"%s\" attachments %d:%s",
						filepath.string(), firstAttachment.id, firstAttachment.extractedFile));

					proc.get_exit_status();
					filesToCleanup.push_back(firstAttachment.extractedFile);

					break;
				}
			}
		}





		std::vector<std::string> arguments;
		arguments.push_back("mkvpropedit");

		// file:
		arguments.push_back(zpr::sprint("\"%s\"", filepath.string()));

		int ssn = 0;
		{
			auto [ series, season, episode, title ] = misc::parseTVShow(filepath.stem().string());
			if(!series.empty())
			{
				auto metadata = tvdb::fetchEpisodeMetadata(series, season, episode, title, args::getManualSeriesId());
				if(!metadata.valid)
				{
					util::error("failed to find metadata");
					continue;
				}

				auto xml = tags::serialiseMetadata(metadata);

				auto printer = new tinyxml2::XMLPrinter();
				xml->Print(printer);

				auto tagfilename = zpr::sprint(".tmp-mkvinator-tags-s%02d-e%02d.xml", season, episode);

				auto out = std::ofstream(tagfilename);
				out.write(printer->CStr(), printer->CStrSize());

				out.close();

				delete printer;
				delete xml;

				ssn = season;

				util::info("tv:  %s S%02dE%02d - %s", metadata.seriesMeta.name, metadata.seasonNumber,
					metadata.episodeNumber, metadata.name);

				arguments.push_back("--tags");
				arguments.push_back(zpr::sprint("all:\"%s\"", tagfilename));

				filesToCleanup.push_back(tagfilename);
			}
			else
			{
				// try movie?
			}
		}



		// call the stuff to tag the file.
		{
			// try {cover, SeasonN, Season0N, poster}.{jpg, png}
			std::vector<std::string> names = {
				"cover",
				zpr::sprint("season%d", ssn),
				zpr::sprint("season%02d", ssn),
				zpr::sprint("Season%d", ssn),
				zpr::sprint("Season%02d", ssn),
				"season",
				"Season",
				"poster"
			};

			std::vector<std::string> exts = { "png", "jpg", "jpeg" };
			auto tries = util::map(util::cartesian(names, exts), [](auto x) -> auto {
				return zpr::sprint("%s.%s", x.first, x.second);
			});

			std::fs::path cover = specCoverFile;
			if(cover.empty())
			{
				for(const auto& x : tries)
				{
					if(std::fs::exists(x))
						cover = x;
				}
			}

			if(!cover.empty())
			{
				util::info("art: %s", cover.string());


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
			}
		}


		// if we specified an alternative output path,
		// copy the file over, and use that as the input to mkvpropedit instead.
		if(auto out = args::getOutputFolder(); !out.empty())
		{
			auto outpath = std::fs::canonical(std::fs::path(out)) / filepath.filename();
			arguments[1] = zpr::sprint("\"%s\"", outpath.string());

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
						util::error("error: failed to copy file to '%s'", outpath.string());
						continue;
					}
				}
				else
				{
					util::log("updating existing output in-place");
				}
			}
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
				if(!sout.empty()) fprintf(stderr, "%s\n", sout.c_str());
				if(!serr.empty()) fprintf(stderr, "%s\n", serr.c_str());

				fprintf(stderr, "mkvpropedit returned non-zero (status = %d)\n", status);
				fprintf(stderr, "cmdline was: %s\n", cmdline.c_str());
				continue;
			}
		}


		for(const auto& f : filesToCleanup)
		{
			if(std::fs::exists(f))
				std::fs::remove(f);
		}

		util::log("done");
		doneFiles += 1;
	}


	util::info("processed %d %s", doneFiles, util::plural("file", doneFiles));
}















