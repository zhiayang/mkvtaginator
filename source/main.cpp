// main.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

int main(int argc, char** argv)
{
	config::readConfig();
	auto files = args::parseCmdLineOpts(argc, argv);


	util::info("received %zu %s", files.size(), util::plural("file", files.size()));

	if(!config::getManualSeriesId().empty())
		util::info("skipping search: using series id '%s'", config::getManualSeriesId());

	driver::createOutputFolder();

	auto paths = driver::collectFiles(files);

	size_t doneFiles = 0;
	for(const auto& filepath : paths)
	{
		auto ok = driver::processOneFile(filepath);

		if(ok) doneFiles += 1;
	}

	util::info("processed %d %s", doneFiles, util::plural("file", doneFiles));
}








namespace driver
{
	bool processOneFile(const std::fs::path& filepath)
	{
		bool ok = true;

		util::log("%s", filepath.filename().string());
		util::indent_log();

		std::fs::path targetFile = filepath;

		if(config::isMuxing())
		{
			util::info("muxing");
			util::indent_log();

			ok &= mux::muxOneFile(targetFile);

			util::unindent_log();
		}

		if(config::isTagging())
		{
			util::info("tagging");
			util::indent_log();

			ok &= tag::tagOneFile(targetFile);

			util::unindent_log();
		}

		util::unindent_log();
		zpr::println("");
		return ok;
	}






	void createOutputFolder()
	{
		if(auto out = config::getOutputFolder(); !out.empty())
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
				util::error("%serror:%s specified output path '%s' is not a directory", out,
					COLOUR_RED_BOLD, COLOUR_RESET);
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
				util::error("skipping nonexistent file '%s'", filepath.string());
				if(config::shouldStopOnError()) exit(-1);
				continue;
			}
			else if(filepath.extension() != ".mkv")
			{
				util::error("ignoring non-mkv file (extension was '%s')", filepath.extension().string());
				if(config::shouldStopOnError()) exit(-1);
				continue;
			}

			if(auto out = config::getOutputFolder(); !out.empty())
			{
				if(std::fs::equivalent(std::fs::canonical(filepath.parent_path()),
					std::fs::canonical(std::fs::path(out))))
				{
					util::error("skipping input file overlapping with output folder");
					if(config::shouldStopOnError()) exit(-1);
					continue;
				}
			}

			ret.push_back(filepath);
		}

		return ret;
	}
}







