// arguments.cpp
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#define ARG_HELP                    "--help"
#define ARG_MUX                     "--mux"
#define ARG_TAG                     "--tag"
#define ARG_COVER_IMAGE             "--cover"
#define ARG_MOVIE_ID                "--movie"
#define ARG_CONFIG_PATH             "--config"
#define ARG_SERIES_ID               "--series"
#define ARG_RENAME_FILES            "--rename"
#define ARG_MANUAL_SEASON           "--season"
#define ARG_MANUAL_EPISODE          "--episode"
#define ARG_DRY_RUN                 "--dry-run"
#define ARG_TVDB_API_KEY            "--tvdb-api"
#define ARG_MOVIEDB_API_KEY         "--moviedb-api"
#define ARG_NO_PROGRESS             "--no-progress"
#define ARG_AUDIO_LANGS             "--audio-langs"
#define ARG_OUTPUT_FOLDER           "--output-folder"
#define ARG_NO_AUTO_COVER           "--no-auto-cover"
#define ARG_STOP_ON_ERROR           "--stop-on-error"
#define ARG_SUBTITLE_LANGS          "--subtitle-langs"
#define ARG_SKIP_NCOP_NCED          "--skip-ncop-nced"
#define ARG_PREFER_SDH_SUBS         "--prefer-sdh-subs"
#define ARG_PREFER_TEXT_SUBS        "--prefer-text-subs"
#define ARG_PREFER_ENGLISH_TITLE    "--prefer-eng-title"
#define ARG_PREFER_ORIGINAL_TITLE   "--prefer-orig-title"
#define ARG_EXTRA_SUBS_FOLDER       "--extra-subs-folder"
#define ARG_PREFER_ONE_STREAM       "--prefer-one-stream"
#define ARG_OVERRIDE_MOVIE_NAME     "--override-movie-name"
#define ARG_OVERRIDE_SERIES_NAME    "--override-series-name"
#define ARG_OVERRIDE_EPISODE_NAME   "--override-episode-name"
#define ARG_DELETE_EXISTING_OUTPUT  "--delete-existing-output"
#define ARG_PREFER_SIGN_SONG_SUBS   "--prefer-signs-songs-subs"
#define ARG_NO_SMART_REPLACE_COVER  "--no-smart-replace-cover-art"


static std::vector<std::pair<std::string, std::string>> helpList;
static void setupMap()
{
	helpList.push_back({ ARG_HELP,
		"show this help"
	});

	helpList.push_back({ ARG_MUX,
		"enable re-muxing file streams"
	});

	helpList.push_back({ ARG_TAG,
		"enable metadata tagging"
	});

	helpList.push_back({ ARG_EXTRA_SUBS_FOLDER,
		"automatically find/extract subtitles from matching files in the given folder"
	});

	helpList.push_back({ ARG_MANUAL_SEASON,
		"specify the season number manually (most useful together with '--series')"
	});

	helpList.push_back({ ARG_MANUAL_EPISODE,
		"specify the episode number manually (most useful together with '--series' and '--season')"
	});

	helpList.push_back({ ARG_SERIES_ID,
		"specify the series id (tvdb) (implies tv series -- disables movie matching)"
	});

	helpList.push_back({ ARG_MOVIE_ID,
		"specify the movie id (moviedb) (implies movie -- disables tv matching)"
	});

	helpList.push_back({ ARG_AUDIO_LANGS,
		"a comma-separated list of languages (ISO 639-2) for audio tracks (eg. '--audio-langs eng,jpn')"
	});

	helpList.push_back({ ARG_SUBTITLE_LANGS,
		"a comma-separated list of languages (ISO 639-2) for subtitles (eg. '--subtitle-langs eng,jpn')"
	});

	helpList.push_back({ ARG_PREFER_SDH_SUBS,
		"prefer SDH-friendly subtitles"
	});

	helpList.push_back({ ARG_PREFER_TEXT_SUBS,
		"prefer text-based subtitle formats (eg. ssa/ass/srt)"
	});

	helpList.push_back({ ARG_PREFER_SIGN_SONG_SUBS,
		"prefer subtitles with (possibly only) signs/songs"
	});

	helpList.push_back({ ARG_PREFER_ONE_STREAM,
		"only have one of each type (video, audio, subtitle -- excluding attachments) of stream in the output."
	});

	helpList.push_back({ ARG_CONFIG_PATH,
		"set the path to the configuration file to use"
	});

	helpList.push_back({ ARG_NO_PROGRESS,
		"disable progress indication (for muxing)"
	});

	helpList.push_back({ ARG_RENAME_FILES,
		"rename the output file to the canonical format; for TV shows, 'SERIES S01E01 - EP TITLE'; for movies, 'TITLE (YEAR)'"
	});

	helpList.push_back({ ARG_COVER_IMAGE + std::string(" <path_to_image>"),
		"set the cover/poster image"
	});

	helpList.push_back({ ARG_COVER_IMAGE + std::string(" <series_id>"),
		"manually specify the series ID for all files"
	});

	helpList.push_back({ ARG_OUTPUT_FOLDER + std::string(" <path_to_folder>"),
		"specify the output folder to prevent overwriting input files; will be created if it doesn't exist"
		" (cannot be the same as the input)"
	});

	helpList.push_back({ ARG_OVERRIDE_MOVIE_NAME,
		"use the movie title from the filename (instead of online metadata)"
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
		"when " ARG_OUTPUT_FOLDER " is specified, delete existing files instead of updating them in-place"
	});

	helpList.push_back({ ARG_NO_SMART_REPLACE_COVER,
		"always reattach attachments, even those that are detected to be existing cover art attachments"
	});

	helpList.push_back({ ARG_PREFER_ORIGINAL_TITLE,
		"prefer the original-language title (if available) rather than the anglicised one (for foreign content) -- this is the default"
	});

	helpList.push_back({ ARG_PREFER_ENGLISH_TITLE,
		"prefer the anglicised (english-language) title for foreign content"
	});

	helpList.push_back({ ARG_TVDB_API_KEY + std::string(" <api_key>"),
		"specify the api key for authenticating with the tvdb api"
	});

	helpList.push_back({ ARG_MOVIEDB_API_KEY + std::string(" <api_key>"),
		"specify the api key for authenticating with the moviedb api"
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
	static std::vector<std::string> parseCommaSep(const std::string& s)
	{
		bool warned = false;

		std::vector<std::string> ret;
		auto xs = util::splitString(s, ',');

		for(const auto& x : xs)
		{
			if(x.size() != 3)
			{
				if(!warned)
					util::warn("invalid language code '%s'. see https://w.wiki/EXG for a list.");

				warned = true;
			}
			else
			{
				ret.push_back(x);
			}
		}

		return ret;
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
						config::setManualCoverPath(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected path after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_MOVIE_ID))
				{
					if(i != argc - 1)
					{
						i++;
						config::setManualMovieId(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected id after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_SERIES_ID))
				{
					if(i != argc - 1)
					{
						i++;
						config::setManualSeriesId(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected id after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_EXTRA_SUBS_FOLDER))
				{
					if(i != argc - 1)
					{
						i++;
						config::setExtraSubsPath(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected string after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_OVERRIDE_MOVIE_NAME))
				{
					config::setIsOverridingMovieName(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_OVERRIDE_SERIES_NAME))
				{
					config::setIsOverridingSeriesName(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_OVERRIDE_EPISODE_NAME))
				{
					config::setIsOverridingEpisodeName(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_NO_SMART_REPLACE_COVER))
				{
					config::setDisableSmartReplaceCoverArt(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_DRY_RUN))
				{
					config::setIsDryRun(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_NO_AUTO_COVER))
				{
					config::setDisableAutoCoverSearch(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_STOP_ON_ERROR))
				{
					config::setShouldStopOnError(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_DELETE_EXISTING_OUTPUT))
				{
					config::setShouldDeleteExistingOutput(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_ORIGINAL_TITLE))
				{
					config::setIsPreferEnglishTitle(false);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_ENGLISH_TITLE))
				{
					// so uncivilised.
					config::setIsPreferEnglishTitle(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_RENAME_FILES))
				{
					config::setShouldRenameFiles(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_NO_PROGRESS))
				{
					config::setDisableProgress(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_SDH_SUBS))
				{
					config::setPreferSDHSubs(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_SIGN_SONG_SUBS))
				{
					config::setPreferSignSongSubs(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_TEXT_SUBS))
				{
					config::setPreferTextSubs(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_PREFER_ONE_STREAM))
				{
					config::setPreferOneStream(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_SKIP_NCOP_NCED))
				{
					config::setSkipNCOPNCED(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_AUDIO_LANGS))
				{
					if(i != argc - 1)
					{
						i++;
						config::setAudioLangs(parseCommaSep(argv[i]));
						continue;
					}
					else
					{
						util::error("%serror:%s expected string after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_SUBTITLE_LANGS))
				{
					if(i != argc - 1)
					{
						i++;
						config::setSubtitleLangs(parseCommaSep(argv[i]));
						continue;
					}
					else
					{
						util::error("%serror:%s expected string after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_MUX))
				{
					config::setIsMuxing(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_TAG))
				{
					config::setIsTagging(true);
					continue;
				}
				else if(!strcmp(argv[i], ARG_MANUAL_SEASON))
				{
					if(i != argc - 1)
					{
						i++;
						std::string str = argv[i];

						for(char c : str)
						{
							if(c < '0' || c > '9')
								goto not_number;
						}

						config::setSeasonNumber(std::stoi(str));
						continue;
					}
					else
					{
					not_number:
						util::error("%serror:%s expected (positive) integer after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_MANUAL_EPISODE))
				{
					if(i != argc - 1)
					{
						i++;
						std::string str = argv[i];

						for(char c : str)
						{
							if(c < '0' || c > '9')
								goto not_number;    // note: this jumps to the not_number above, which i guess is fine.
						}

						config::setEpisodeNumber(std::stoi(str));
						continue;
					}
					else
					{
						util::error("%serror:%s expected (positive) integer after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_OUTPUT_FOLDER))
				{
					if(i != argc - 1)
					{
						i++;
						config::setOutputFolder(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected path after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_CONFIG_PATH))
				{
					if(i != argc - 1)
					{
						i++;
						config::setConfigPath(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected path after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_TVDB_API_KEY))
				{
					if(i != argc - 1)
					{
						i++;
						config::setTVDBApiKey(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected string after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(!strcmp(argv[i], ARG_MOVIEDB_API_KEY))
				{
					if(i != argc - 1)
					{
						i++;
						config::setMovieDBApiKey(argv[i]);
						continue;
					}
					else
					{
						util::error("%serror:%s expected string after '%s' option", COLOUR_RED_BOLD, COLOUR_RESET,
							argv[i]);
						exit(-1);
					}
				}
				else if(argv[i][0] == '-')
				{
					util::error("%serror:%s unrecognised option '%s'", COLOUR_RED_BOLD, COLOUR_RESET,
						argv[i]);
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
			util::error("%serror:%s no input files",
				COLOUR_RED_BOLD, COLOUR_RESET);
			exit(-1);
		}

		if(!config::isMuxing() && !config::isTagging())
		{
			util::error("%serror:%s one or both of '--mux' or '--tag' must be specified",
				COLOUR_RED_BOLD, COLOUR_RESET);
			exit(-1);
		}
		else if(config::getOutputFolder().empty() && config::isMuxing())
		{
			util::error("%serror:%s output folder must be specified ('--output-folder') when muxing",
				COLOUR_RED_BOLD, COLOUR_RESET);
			exit(-1);
		}

		return filenames;
	}
}










