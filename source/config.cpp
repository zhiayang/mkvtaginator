// config.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#include "picojson.h"

namespace pj = picojson;

namespace config
{
	static std::fs::path getDefaultConfigPath()
	{
		auto home = std::fs::path(util::getEnvironmentVar("HOME"));
		if(!home.empty())
		{
			auto x = home / ".config" / "mkvtaginator" / "config.json";
			if(std::fs::exists(x))
				return x;
		}

		if(std::fs::exists("mkvtaginator-config.json"))
			return std::fs::path("mkvtaginator-config.json");

		if(std::fs::exists(".mkvtaginator-config.json"))
			return std::fs::path(".mkvtaginator-config.json");

		return "";
	}

	template <typename... Args>
	void error(const std::string& fmt, Args&&... args)
	{
		util::error(fmt, args...);
	}

	void readConfig()
	{
		// if there's a manual one, use that.
		std::fs::path path;
		if(auto cp = getConfigPath(); !cp.empty())
		{
			path = cp;
			if(!std::fs::exists(path))
			{
				// ...
				util::error("specified configuration file '%s' does not exist", cp);
				return;
			}
		}

		if(auto cp = getDefaultConfigPath(); !cp.empty())
		{
			path = cp;

			// read it.
			uint8_t* buf = 0; size_t sz = 0;
			std::tie(buf, sz) = util::readEntireFile(path.string());
			if(!buf || sz == 0)
			{
				error("failed to read file");
				return;
			}

			// util::log("reading config file '%s'", path.string());


			pj::value config;

			auto begin = buf;
			auto end = buf + sz;
			std::string err;
			pj::parse(config, begin, end, &err);
			if(!err.empty())
			{
				error("%s", err);
				return;
			}

			// the top-level object should be "options".
			if(auto options = config.get("options"); !options.is<pj::null>())
			{

				auto opts = options.get<pj::object>();

				auto get_string = [&opts](const std::string& key, const std::string& def) -> std::string {
					if(auto it = opts.find(key); it != opts.end())
					{
						if(it->second.is<std::string>())
							return it->second.get<std::string>();

						else
							error("expected string value for '%s'", key);
					}

					return def;
				};

				auto get_array = [&opts](const std::string& key) -> std::vector<pj::value> {
					if(auto it = opts.find(key); it != opts.end())
					{
						if(it->second.is<pj::array>())
							return it->second.get<pj::array>();

						else
							error("expected array value for '%s'", key);
					}

					return { };
				};

				auto get_bool = [&opts](const std::string& key, bool def) -> bool {
					if(auto it = opts.find(key); it != opts.end())
					{
						if(it->second.is<bool>())
							return it->second.get<bool>();

						else
							error("expected boolean value for '%s'", key);
					}

					return def;
				};

				if(auto x = get_string("tvdb-api-key", ""); !x.empty())
					setTVDBApiKey(x);

				if(auto x = get_string("moviedb-api-key", ""); !x.empty())
					setMovieDBApiKey(x);

				if(auto x = get_string("output-folder", ""); !x.empty())
					setOutputFolder(x);


				auto get_langs = [](const std::vector<pj::value>& xs, const std::string& foo) -> std::vector<std::string> {

					return util::mapFilter(xs, [&foo](const pj::value& v) -> std::string {
						if(v.is<std::string>())
							return v.get<std::string>();

						else
							util::error("expected string value in '%s'", foo);

						return "";
					}, [](const std::string& s) -> bool {
						if(s.empty()) return false;
						if(s.size() != 3)
						{
							util::error("invalid language code '%s'. see https://w.wiki/EXG for a list.");
							return false;
						}

						return true;
					});
				};

				if(auto x = get_array("preferred-audio-languages"); !x.empty())
					setAudioLangs(get_langs(x, "preferred-audio-languages"));

				else
					setAudioLangs({ "eng" });

				if(auto x = get_array("preferred-subtitle-languages"); !x.empty())
					setSubtitleLangs(get_langs(x, "preferred-subtitle-languages"));

				else
					setSubtitleLangs({ "eng" });



				// these are simply the default values without a config file.
				// some are true and some are false, because of the way the boolean is
				// named -- eg. in code we have disableAutoCoverSearch (negative), but in
				// the config it's "automatic-cover-search" (positive).

				setDisableProgress(!get_bool("show-progress", true));
				setShouldStopOnError(get_bool("stop-on-first-error", false));
				setIsPreferEnglishTitle(get_bool("prefer-english-title", false));
				setIsPreferEnglishTitle(!get_bool("prefer-original-title", true));
				setDisableAutoCoverSearch(!get_bool("automatic-cover-search", true));
				setShouldDeleteExistingOutput(get_bool("delete-existing-output", false));
				setDisableSmartReplaceCoverArt(!get_bool("smart-cover-art-replacement", true));

				setShouldRenameFiles(get_bool("automatic-rename-files", false));
				setIsOverridingMovieName(get_bool("override-movie-name", false));
				setIsOverridingSeriesName(get_bool("override-series-name", false));
				setIsOverridingEpisodeName(get_bool("override-episode-name", false));
				setShouldRenameWithoutEpisodeTitle(!get_bool("rename-with-episode-title", true));

				setPreferOneStream(get_bool("prefer-one-stream", true));
				setPreferSDHSubs(get_bool("prefer-sdh-subtitles", false));
				setPreferTextSubs(get_bool("prefer-text-subtitles", true));
				setPreferSignSongSubs(get_bool("prefer-signs-and-songs-subs", false));
				setSkipNCOPNCED(get_bool("skip-ncop-nced", false));
			}
			else
			{
				error("no top-level 'options' object");
			}


			delete[] buf;
		}

		// it's ok not to have one.
	}

























	static std::string coverPathName;
	static std::string movieId;
	static std::string seriesId;
	static std::string episodeId;
	static std::string outputFolder;

	static std::string configPath;
	static std::string tvdbApiKey;
	static std::string moviedbApiKey;
	static std::string extraSubsPath;
	static std::string manualSubsPath;
	static std::string manualSeriesTitle;

	static std::vector<std::string> audioLangs;
	static std::vector<std::string> subtitleLangs;

	static bool dryrun = false;
	static bool muxing = false;
	static bool tagging = false;
	static bool noprogress = false;
	static bool noAutoCover = false;
	static bool stopOnError = false;
	static bool renameFiles = false;
	static bool skipNCOPNCED = false;
	static bool preferEnglishTitle = false;
	static bool noSmartReplaceCoverArt = false;
	static bool renameWithoutEpisodeTitle = false;

	static bool preferSDHSubs = false;
	static bool preferTextSubs = true;
	static bool preferOneStream = true;
	static bool preferSignSongSubs = false;

	static bool overrideMovieName = false;
	static bool overrideSeriesName = false;
	static bool overrideEpisodeName = false;
	static bool deleteExistingOutput = false;

	// 0 is a valid season, lmao
	static int manualSeasonNumber = -1;
	static int manualEpisodeNumber = -1;

	static double subtitleDelay = 0;


	void setAudioLangs(const std::vector<std::string>& xs)      { audioLangs = xs; }
	void setSubtitleLangs(const std::vector<std::string>& xs)   { subtitleLangs = xs; }

	std::vector<std::string> getAudioLangs()    { return audioLangs; }
	std::vector<std::string> getSubtitleLangs() { return subtitleLangs; }

	std::string getManualMovieId()          { return movieId; }
	std::string getManualSeriesId()         { return seriesId; }
	std::string getManualEpisodeId()        { return episodeId; }
	std::string getManualCoverPath()        { return coverPathName; }
	std::string getOutputFolder()           { return outputFolder; }
	std::string getTVDBApiKey()             { return tvdbApiKey; }
	std::string getMovieDBApiKey()          { return moviedbApiKey; }
	std::string getConfigPath()             { return configPath; }
	std::string getExtraSubsPath()          { return extraSubsPath; }
	std::string getManualSubsPath()         { return manualSubsPath; }
	std::string getManualSeriesTitle()      { return manualSeriesTitle; }
	bool isOverridingMovieName()            { return overrideMovieName; }
	bool isOverridingSeriesName()           { return overrideSeriesName; }
	bool isOverridingEpisodeName()          { return overrideEpisodeName; }
	bool isDryRun()                         { return dryrun; }
	bool isMuxing()                         { return muxing; }
	bool isTagging()                        { return tagging; }
	bool shouldRenameFiles()                { return renameFiles; }
	bool isPreferEnglishTitle()             { return preferEnglishTitle; }
	bool shouldStopOnError()                { return stopOnError; }
	bool disableAutoCoverSearch()           { return noAutoCover; }
	bool disableProgress()                  { return noprogress; }
	bool disableSmartReplaceCoverArt()      { return noSmartReplaceCoverArt; }
	bool shouldDeleteExistingOutput()       { return deleteExistingOutput; }
	bool shouldRenameWithoutEpisodeTitle()  { return renameWithoutEpisodeTitle; }
	bool isPreferSDHSubs()                  { return preferSDHSubs; }
	bool isPreferTextSubs()                 { return preferTextSubs; }
	bool isPreferOneStream()                { return preferOneStream; }
	bool isPreferSignSongSubs()             { return preferSignSongSubs; }
	bool shouldSkipNCOPNCED()               { return skipNCOPNCED; }
	int getSeasonNumber()                   { return manualSeasonNumber; }
	int getEpisodeNumber()                  { return manualEpisodeNumber; }
	double getSubtitleDelay()               { return subtitleDelay; }

	void setManualMovieId(const std::string& x)     { movieId = x; }
	void setManualSeriesId(const std::string& x)    { seriesId = x; }
	void setManualEpisodeId(const std::string& x)   { episodeId = x; }
	void setManualCoverPath(const std::string& x)   { coverPathName = x; }
	void setOutputFolder(const std::string& x)      { outputFolder = x; }
	void setTVDBApiKey(const std::string& x)        { tvdbApiKey = x; }
	void setMovieDBApiKey(const std::string& x)     { moviedbApiKey = x; }
	void setExtraSubsPath(const std::string& x)     { extraSubsPath = x; }
	void setManualSubsPath(const std::string& x)    { manualSubsPath = x; }
	void setManualSeriesTitle(const std::string& x) { manualSeriesTitle = x; }
	void setDisableAutoCoverSearch(bool x)          { noAutoCover = x; }
	void setIsDryRun(bool x)                        { dryrun = x; }
	void setIsMuxing(bool x)                        { muxing = x;}
	void setIsTagging(bool x)                       { tagging = x;}
	void setDisableProgress(bool x)                 { noprogress = x; }
	void setShouldStopOnError(bool x)               { stopOnError = x; }
	void setShouldRenameFiles(bool x)               { renameFiles = x; }
	void setIsPreferEnglishTitle(bool x)            { preferEnglishTitle = x; }
	void setIsOverridingMovieName(bool x)           { overrideMovieName = x; }
	void setIsOverridingSeriesName(bool x)          { overrideSeriesName = x; }
	void setIsOverridingEpisodeName(bool x)         { overrideEpisodeName = x; }
	void setShouldDeleteExistingOutput(bool x)      { deleteExistingOutput = x; }
	void setDisableSmartReplaceCoverArt(bool x)     { noSmartReplaceCoverArt = x; }
	void setShouldRenameWithoutEpisodeTitle(bool x) { renameWithoutEpisodeTitle = x; }
	void setPreferSDHSubs(bool x)                   { preferSDHSubs = x; }
	void setPreferTextSubs(bool x)                  { preferTextSubs = x; }
	void setPreferOneStream(bool x)                 { preferOneStream = x; }
	void setPreferSignSongSubs(bool x)              { preferSignSongSubs = x; }
	void setSkipNCOPNCED(bool x)                    { skipNCOPNCED = x; }
	void setSeasonNumber(int x)                     { manualSeasonNumber = x; }
	void setEpisodeNumber(int x)                    { manualEpisodeNumber = x; }
	void setSubtitleDelay(double x)                 { subtitleDelay = x; }

	void setConfigPath(const std::string& x)
	{
		// this one is special. once we set it, we wanna re-read the config.
		configPath = x;
		readConfig();
	}
}








