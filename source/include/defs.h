// defs.h
// Copyright (c) 2014 - 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <filesystem>

#include "zpr.h"
#include "utils.h"

#define COLOUR_RESET			"\033[0m"
#define COLOUR_BLACK			"\033[30m"			// Black
#define COLOUR_RED				"\033[31m"			// Red
#define COLOUR_GREEN			"\033[32m"			// Green
#define COLOUR_YELLOW			"\033[33m"			// Yellow
#define COLOUR_BLUE				"\033[34m"			// Blue
#define COLOUR_MAGENTA			"\033[35m"			// Magenta
#define COLOUR_CYAN				"\033[36m"			// Cyan
#define COLOUR_WHITE			"\033[37m"			// White
#define COLOUR_BLACK_BOLD		"\033[1m"			// Bold Black
#define COLOUR_RED_BOLD			"\033[1m\033[31m"	// Bold Red
#define COLOUR_GREEN_BOLD		"\033[1m\033[32m"	// Bold Green
#define COLOUR_YELLOW_BOLD		"\033[1m\033[33m"	// Bold Yellow
#define COLOUR_BLUE_BOLD		"\033[1m\033[34m"	// Bold Blue
#define COLOUR_MAGENTA_BOLD		"\033[1m\033[35m"	// Bold Magenta
#define COLOUR_CYAN_BOLD		"\033[1m\033[36m"	// Bold Cyan
#define COLOUR_WHITE_BOLD		"\033[1m\033[37m"	// Bold White
#define COLOUR_GREY_BOLD		"\033[30;1m"		// Bold Grey



namespace tinyxml2
{
	class XMLDocument;
}

// https://stackoverflow.com/questions/28367913/how-to-stdhash-an-unordered-stdpair

template<typename T>
void _hash_combine(std::size_t& seed, const T& key)
{
	std::hash<T> hasher;
	seed ^= hasher(key) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
	namespace fs = filesystem;

	template<typename T1, typename T2>
	struct hash<std::pair<T1, T2>>
	{
		size_t operator () (const std::pair<T1, T2>& p) const
		{
			size_t seed = 0;
			_hash_combine(seed, p.first);
			_hash_combine(seed, p.second);
			return seed;
		}
	};
}

namespace util
{
	void indent_log(int n = 1);
	void unindent_log(int n = 1);

	int get_log_indent();

	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		for(int i = 0; i < get_log_indent(); i++)
			fprintf(stderr, "  ");

		fprintf(stderr, " %s*%s %s\n", COLOUR_RED_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}

	template <typename... Args>
	static void log(const std::string& fmt, Args&&... args)
	{
		for(int i = 0; i < get_log_indent(); i++)
			printf("  ");

		printf(" %s*%s %s\n", COLOUR_GREEN_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}

	template <typename... Args>
	static void info(const std::string& fmt, Args&&... args)
	{
		for(int i = 0; i < get_log_indent(); i++)
			printf("  ");

		printf(" %s*%s %s\n", COLOUR_BLUE_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}

	template <typename... Args>
	static void warn(const std::string& fmt, Args&&... args)
	{
		for(int i = 0; i < get_log_indent(); i++)
			printf("  ");

		printf(" %s*%s %s\n", COLOUR_YELLOW_BOLD, COLOUR_RESET, zpr::sprint(fmt, args...).c_str());
	}



	size_t getTerminalWidth();
	size_t displayedTextLength(const std::string_view& sv);
	std::wstring corruptUTF8ToWChar(const std::string& s);

	size_t getFileSize(const std::string& path);
	std::pair<uint8_t*, size_t> readEntireFile(const std::string& path);

	static inline std::vector<std::string> splitString(std::string view, char delim = '\n')
	{
		std::vector<std::string> ret;

		while(true)
		{
			size_t ln = view.find(delim);

			if(ln != std::string_view::npos)
			{
				ret.emplace_back(view.data(), ln);
				view = view.substr(ln + 1);
			}
			else
			{
				break;
			}
		}

		// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
		if(!view.empty())
			ret.emplace_back(view.data(), view.length());

		return ret;
	}

	static inline std::string trim(const std::string& s)
	{
		auto ltrim = [](std::string_view& s) -> std::string_view& {
			auto i = s.find_first_not_of(" \t\n\r\f\v");
			if(i != std::string::npos) s.remove_prefix(i);

			return s;
		};

		auto rtrim = [](std::string_view& s) -> std::string_view& {
			auto i = s.find_last_not_of(" \t\n\r\f\v");
			if(i != std::string::npos) s = s.substr(0, i + 1);

			return s;
		};

		std::string_view sv = s;
		return std::string(ltrim(rtrim(sv)));
	}

	static inline std::string lowercase(std::string xs)
	{
		for(size_t i = 0; i < xs.size(); i++)
			xs[i] = tolower(xs[i]);

		return xs;
	}

	// 1h 3m 14s
	std::string prettyPrintTime(uint64_t ns, bool ms = true);

	// 01:03:14.51
	std::string uglyPrintTime(uint64_t ns, bool ms = true);

	std::string getEnvironmentVar(const std::string& name);

	std::string sanitiseFilename(std::string name);
}

namespace args
{
	std::vector<std::string> parseCmdLineOpts(int argc, char** argv);
}

namespace config
{
	void readConfig();


	std::string getOutputFolder();
	std::string getManualMovieId();
	std::string getManualSeriesId();
	std::string getManualCoverPath();

	std::string getMovieDBApiKey();
	std::string getTVDBApiKey();
	std::string getConfigPath();
	std::string getExtraSubsPath();
	std::string getManualSeriesTitle();

	std::vector<std::string> getAudioLangs();
	std::vector<std::string> getSubtitleLangs();

	bool isOverridingMovieName();
	bool isOverridingSeriesName();
	bool isOverridingEpisodeName();
	bool shouldDeleteExistingOutput();
	bool disableSmartReplaceCoverArt();
	bool shouldRenameWithoutEpisodeTitle();

	bool isPreferSDHSubs();
	bool isPreferTextSubs();
	bool isPreferOneStream();
	bool isPreferSignSongSubs();

	bool shouldSkipNCOPNCED();

	bool isDryRun();
	bool disableProgress();
	bool shouldRenameFiles();
	bool shouldStopOnError();
	bool isPreferEnglishTitle();
	bool disableAutoCoverSearch();
	int getSeasonNumber();
	int getEpisodeNumber();


	bool isMuxing();
	bool isTagging();


	void setAudioLangs(const std::vector<std::string>& xs);
	void setSubtitleLangs(const std::vector<std::string>& xs);

	void setPreferSDHSubs(bool x);
	void setPreferTextSubs(bool x);
	void setPreferOneStream(bool x);
	void setPreferSignSongSubs(bool x);
	void setSkipNCOPNCED(bool x);
	void setSeasonNumber(int x);
	void setEpisodeNumber(int x);

	void setManualSeriesTitle(const std::string& x);
	void setOutputFolder(const std::string& x);
	void setManualMovieId(const std::string& x);
	void setManualSeriesId(const std::string& x);
	void setManualCoverPath(const std::string& x);
	void setMovieDBApiKey(const std::string& x);
	void setTVDBApiKey(const std::string& x);
	void setConfigPath(const std::string& x);
	void setExtraSubsPath(const std::string& x);
	void setIsOverridingMovieName(bool x);
	void setIsOverridingSeriesName(bool x);
	void setIsOverridingEpisodeName(bool x);
	void setShouldDeleteExistingOutput(bool x);
	void setDisableSmartReplaceCoverArt(bool x);
	void setShouldRenameWithoutEpisodeTitle(bool x);
	void setIsDryRun(bool x);
	void setIsMuxing(bool x);
	void setIsTagging(bool x);
	void setDisableProgress(bool x);
	void setShouldRenameFiles(bool x);
	void setShouldStopOnError(bool x);
	void setIsPreferEnglishTitle(bool x);
	void setDisableAutoCoverSearch(bool x);
}

namespace driver
{
	void createOutputFolder();
	std::vector<std::filesystem::path> collectFiles(const std::vector<std::string>& files);
	bool processOneFile(const std::filesystem::path& filepath);
}

namespace misc
{
	struct Option
	{
		std::string title;
		std::string altTitle;
		std::string subTitle;

		struct Info
		{
			std::string heading;
			std::string subheading;

			std::vector<std::string> items;

			std::string body;
		};

		std::vector<Info> infos;
	};

	size_t userChoice(const std::vector<Option>& options, bool* showmore = 0, size_t first = 0, size_t limit = 0);
	std::vector<size_t> userChoiceMultiple(const std::vector<Option>& options);
}




namespace mux
{
	bool muxOneFile(std::fs::path& filepath);
}






namespace tag
{
	struct GenericMetadata
	{
		bool valid = false;

		std::string id;
		std::string normalTitle;
		std::string canonicalTitle;
		std::string episodeTitle;
	};

	struct SeriesMetadata : GenericMetadata
	{
		std::string name;
		std::string airDate;

		std::vector<std::string> genres;
		std::vector<std::string> actors;
	};

	struct EpisodeMetadata : GenericMetadata
	{
		SeriesMetadata seriesMeta;

		std::string name;
		std::string airDate;

		int seasonNumber = 0;
		int episodeNumber = 0;

		std::string synopsis;
		std::string description;

		std::vector<std::string> actors;
		std::vector<std::string> artists;
		std::vector<std::string> writers;
		std::vector<std::string> directors;
	};

	struct MovieMetadata : GenericMetadata
	{
		std::string title;
		std::string originalTitle;

		std::string airDate;
		std::string synopsis;

		// { actor name, character played }
		std::vector<std::pair<std::string, std::string>> cast;

		std::vector<std::string> genres;
		std::vector<std::string> writers;
		std::vector<std::string> directors;
		std::vector<std::string> producers;
		std::vector<std::string> coproducers;
		std::vector<std::string> execProducers;
		std::vector<std::string> productionStudios;

		int year = 0;
	};

	namespace tvdb
	{
		void login();
		std::string getToken();

		EpisodeMetadata fetchEpisodeMetadata(const std::string& series, int season, int episode, const std::string& title,
			const std::string& manualSeriesId);
	}

	namespace moviedb
	{
		void login();
		std::string getToken();

		MovieMetadata fetchMovieMetadata(const std::string& title, int year, const std::string& manualId);
	}

	namespace cache
	{
		std::string getSeriesId(const std::string& name);
		void setSeriesId(const std::string& name, const std::string& id);

		SeriesMetadata getSeriesMeta(const std::string& id);
		void addSeriesMeta(const std::string& id, const SeriesMetadata& meta);
		bool haveSeriesMeta(const std::string& id);
	}

	// { series, season, episode, title }
	std::tuple<std::string, int, int, std::string> parseTVShow(const std::string& filename);

	// { name, year }
	std::tuple<std::string, int> parseMovie(const std::string& filename);


	bool tagOneFile(const std::filesystem::path& filepath);
	tinyxml2::XMLDocument* serialiseMetadata(const MovieMetadata& meta);
	tinyxml2::XMLDocument* serialiseMetadata(const EpisodeMetadata& meta);
}





// defer implementation
// credit: gingerBill
// shamelessly stolen from https://github.com/gingerBill/gb


namespace __dontlook
{
	// NOTE(bill): Stupid fucking templates
	template <typename T> struct gbRemoveReference       { typedef T Type; };
	template <typename T> struct gbRemoveReference<T &>  { typedef T Type; };
	template <typename T> struct gbRemoveReference<T &&> { typedef T Type; };

	/// NOTE(bill): "Move" semantics - invented because the C++ committee are idiots (as a collective not as indiviuals (well a least some aren't))
	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &t)  { return static_cast<T &&>(t); }
	template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &&t) { return static_cast<T &&>(t); }
	template <typename T> inline T &&gb_move   (T &&t)                                   { return static_cast<typename gbRemoveReference<T>::Type &&>(t); }
	template <typename F>
	struct gbprivDefer {
		F f;
		gbprivDefer(F &&f) : f(gb_forward<F>(f)) {}
		~gbprivDefer() { f(); }
	};
	template <typename F> gbprivDefer<F> gb__defer_func(F &&f) { return gbprivDefer<F>(gb_forward<F>(f)); }
}

#define GB_DEFER_1(x, y) x##y
#define GB_DEFER_2(x, y) GB_DEFER_1(x, y)
#define GB_DEFER_3(x)    GB_DEFER_2(x, __COUNTER__)
#define defer(code) auto GB_DEFER_3(_defer_) = __dontlook::gb__defer_func([&]()->void{code;})






