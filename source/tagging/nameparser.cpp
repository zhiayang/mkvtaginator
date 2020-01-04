// nameparser.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include "defs.h"

namespace tag
{
	static std::string sanitiseName(std::string name)
	{
		// last step: see if there's no spaces but a whole bunch of periods in the title...
		if(name.find(' ') == std::string::npos && name.find('.') != std::string::npos)
		{
			std::replace(name.begin(), name.end(), '.', ' ');
			name = util::trim(name);
		}
		else if(name.find(' ') == std::string::npos && name.find('_') != std::string::npos)
		{
			std::replace(name.begin(), name.end(), '_', ' ');
			name = util::trim(name);
		}

		return name;
	}

	static std::tuple<std::string, int, int, std::string> parseStrangeTVShow(const std::string& _filename)
	{
		// morally questionable sources, i say?

		std::string series;
		std::string title;
		int episode = 0;

		// callers know how to handle this -- if -1, then assume 1.
		int season = -1;

		// this is a very dirty hack, but it works.
		std::string filename = _filename;
		constexpr const char* hacky_marker = "____________________________________";
		if(auto mst = config::getManualSeriesTitle(); !mst.empty() && filename.find(mst) != std::string::npos)
		{
			// ok, replace that shit with a special identifer, so it doesn't affect the regex.
			// this problem was discovered when trying to process Steins;Gate 0, which, as you
			// might guess, messes up the regex due to '0'.

			filename.replace(filename.find(mst), mst.size(), hacky_marker);
		}


		// these typically don't come with seasons, so don't try to look for them.
		// (at least, i haven't encountered them before)
		auto regex = std::regex("(?:\\[.+?\\] *?)?(.+?)(?: +-)?(?: +|E|EP|e|ep|-|_)(\\d+)(?:.*)");
		{
			std::smatch sm;
			std::regex_match(filename, sm, regex);

			// the whole thing is considered one match. we need the series name and episode number,
			// so 2+1 = 3.
			if(sm.size() < 3)
				return { };

			series = util::trim(sm[1]);
			episode = std::stoi(sm[2]);


			// special: see if there's actually a season in the title.
			auto reg2 = std::regex("(.+?) +S(\\d+)");
			{
				std::smatch sm;
				std::regex_match(series, sm, reg2);

				if(sm.size() > 2)
				{
					// alright.
					series = util::trim(sm[1]);
					season = std::stoi(sm[2]);
				}
			}
		}

		if(auto mst = config::getManualSeriesTitle(); !mst.empty())
			series = mst;

		series = sanitiseName(series);
		return std::tuple(series, season, episode, title);
	}

	std::tuple<std::string, int, int, std::string> parseTVShow(const std::string& filename)
	{
		std::string series;
		std::string title;
		int season = 0;
		int episode = 0;

		auto regex = std::regex("(.+?) S(\\d+)E(\\d+)(?: - (.*))?");
		{
			std::smatch sm;
			std::regex_match(filename, sm, regex);

			// the whole thing is considered one match. we need the series name,
			// season number and episode number, so 3+1 = 4.
			if(sm.size() < 4)
				return parseStrangeTVShow(filename);

			series = util::trim(sm[1]);
			season = std::stoi(sm[2]);
			episode = std::stoi(sm[3]);

			if(sm.size() > 4)
				title = util::trim(sm[4]);
		}

		return std::tuple(series, season, episode, title);
	}


	std::tuple<std::string, int> parseMovie(const std::string& filename)
	{
		std::string title;
		int year = 0;

		auto regex = std::regex("(.+?) \\((\\d+)\\)");
		{
			std::smatch sm;
			std::regex_match(filename, sm, regex);

			// the whole thing is considered one match. we need the title and year, so 2+1 = 3.
			if(sm.size() < 3)
				return { };

			title = util::trim(sm[1]);
			year = std::stoi(sm[2]);
		}

		return std::tuple(title, year);
	}
}







