// arguments.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#define ARG_HELP                    "--help"
#define ARG_COVER_IMAGE             "--cover"
#define ARG_SERIES_ID               "--series"
#define ARG_DRY_RUN                 "--dry-run"
#define ARG_TVDB_API_KEY            "--tvdb-api"
#define ARG_OUTPUT_FOLDER           "--output-folder"
#define ARG_NO_AUTO_COVER           "--no-auto-cover"
#define ARG_STOP_ON_ERROR           "--stop-on-error"
#define ARG_DELETE_EXISTING_OUTPUT  "--delete-existing-output"
#define ARG_OVERRIDE_SERIES_NAME    "--override-series-name"
#define ARG_OVERRIDE_EPISODE_NAME   "--override-episode-name"


static std::vector<std::pair<std::string, std::string>> helpList;
static void setupMap()
{
	helpList.push_back({ ARG_COVER_IMAGE + std::string(" <path_to_image>"),
		"set the cover/poster image"
	});

	helpList.push_back({ ARG_COVER_IMAGE + std::string(" <series_id>"),
		"manually specify the series ID for all files"
	});

	helpList.push_back({ ARG_OUTPUT_FOLDER + std::string(" <path_to_folder>"),
		"specify the output folder to prevent overwriting input files; will be created if it doesn't exist."
		" (cannot be the same as the input)"
	});

	helpList.push_back({ ARG_OVERRIDE_SERIES_NAME,
		"use the series title from the filename (instead of online metadata)"
	});

	helpList.push_back({ ARG_OVERRIDE_EPISODE_NAME,
		"use the episode title from the filename (instead of online metadata)"
	});

	helpList.push_back({ ARG_DRY_RUN,
		"do everything normally, but do not modify the input files"
	});

	helpList.push_back({ ARG_NO_AUTO_COVER,
		"do not automatically detect cover art in the current folder"
	});

	helpList.push_back({ ARG_STOP_ON_ERROR,
		"exit immediately without processing further files, if any error is encountered"
	});

	helpList.push_back({ ARG_DELETE_EXISTING_OUTPUT,
		"when " ARG_OUTPUT_FOLDER " is specified, delete existing files instead of updating them in-place."
	});

	helpList.push_back({ ARG_TVDB_API_KEY + std::string(" <api_key>"),
		"specify the api key for authenticating with the tvdb api"
	});
}

static void printHelp()
{
	if(helpList.empty())
		setupMap();

	printf("usage: mkvtaginator [options] <inputs>\n\n");

	printf("options:\n");

	size_t maxl = 0;
	for(const auto& p : helpList)
	{
		if(p.first.length() > maxl)
			maxl = p.first.length();
	}

	maxl += 4;

	// ok
	for(const auto& [ opt, desc ] : helpList)
		printf("  %s%s%s\n", opt.c_str(), std::string(maxl - opt.length(), ' ').c_str(), desc.c_str());

	printf("\n");
}




std::string parseQuotedString(char** argv, int& i)
{
	std::string ret;
	if(strlen(argv[i]) > 0)
	{
		if(argv[i][0] == '"' || argv[i][0] == '\'')
		{
			while(std::string(argv[i]).back() != '\'' && std::string(argv[i]).back() != '"')
			{
				ret += " " + std::string(argv[i]);
				i++;
			}
		}
		else
		{
			ret = argv[i];
		}
	}
	return ret;
}






namespace args
{
	static std::string coverPathName;
	static std::string seriesId;
	static std::string outputFolder;
	static std::string tvdbApiKey;

	static bool dryrun = false;
	static bool noAutoCover = false;
	static bool stopOnError = false;
	static bool overrideSeriesName = false;
	static bool overrideEpisodeName = false;
	static bool deleteExistingOutput = false;

	std::string getManualSeriesId()
	{
		return seriesId;
	}

	std::string getManualCoverPath()
	{
		return coverPathName;
	}

	std::string getOutputFolder()
	{
		return outputFolder;
	}

	std::string getTVDBApiKey()
	{
		return tvdbApiKey;
	}

	bool isOverridingSeriesName()
	{
		return overrideSeriesName;
	}

	bool isOverridingEpisodeName()
	{
		return overrideEpisodeName;
	}

	bool isDryRun()
	{
		return dryrun;
	}

	bool isStopOnError()
	{
		return stopOnError;
	}

	bool isNoAutoCover()
	{
		return noAutoCover;
	}

	bool isDeletingExistingOutput()
	{
		return deleteExistingOutput;
	}



	std::vector<std::string> parseCmdLineOpts(int argc, char** argv)
	{
		// quick thing: usually programs will not do anything if --help or --version is anywhere in the flags.
		for(int i = 1; i < argc; i++)
		{
			if(!strcmp(argv[i], ARG_HELP))
			{
				printHelp();
				exit(0);
			}
		}

		std::vector<std::string> filenames;
		if(argc > 1)
		{
			// parse the command line opts
			for(int i = 1; i < argc; i++)
			{
				if(!strcmp(argv[i], ARG_COVER_IMAGE))
				{
					if(i != argc - 1)
					{
						i++;
						coverPathName = argv[i];
						continue;
					}
					else
					{
						util::error("error: expected path after '%s' option", argv[i]);
						exit(-1);
					}
				}
				if(!strcmp(argv[i], ARG_SERIES_ID))
				{
					if(i != argc - 1)
					{
						i++;
						seriesId = argv[i];
						continue;
					}
					else
					{
						util::error("error: expected id after '%s' option", argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_OVERRIDE_SERIES_NAME))
				{
					overrideSeriesName = true;
					continue;
				}
				else if(!strcmp(argv[i], ARG_OVERRIDE_EPISODE_NAME))
				{
					overrideEpisodeName = true;
					continue;
				}
				else if(!strcmp(argv[i], ARG_DRY_RUN))
				{
					dryrun = true;
					continue;
				}
				else if(!strcmp(argv[i], ARG_NO_AUTO_COVER))
				{
					noAutoCover = true;
					continue;
				}
				else if(!strcmp(argv[i], ARG_STOP_ON_ERROR))
				{
					stopOnError = true;
					continue;
				}
				else if(!strcmp(argv[i], ARG_DELETE_EXISTING_OUTPUT))
				{
					deleteExistingOutput = true;
					continue;
				}
				if(!strcmp(argv[i], ARG_OUTPUT_FOLDER))
				{
					if(i != argc - 1)
					{
						i++;
						outputFolder = argv[i];
						continue;
					}
					else
					{
						util::error("error: expected path after '%s' option", argv[i]);
						exit(-1);
					}
				}
				if(!strcmp(argv[i], ARG_TVDB_API_KEY))
				{
					if(i != argc - 1)
					{
						i++;
						tvdbApiKey = argv[i];
						continue;
					}
					else
					{
						util::error("error: expected string after '%s' option", argv[i]);
						exit(-1);
					}
				}
				else if(argv[i][0] == '-')
				{
					util::error("error: unrecognised option '%s'", argv[i]);
					exit(-1);
				}
				else
				{
					filenames.push_back(argv[i]);
				}
			}
		}

		if(filenames.empty())
		{
			util::error("error: no input files");
			exit(-1);
		}

		return filenames;
	}
}










