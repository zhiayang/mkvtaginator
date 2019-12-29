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

	static std::tuple<std::string, int, int, std::string> parseStrangeTVShow(const std::string& filename)
	{
		// morally questionable sources, i say?

		std::string series;
		std::string title;
		int episode = 0;

		int season = 1;

		// these typically don't come with seasons, so we'll assume it's 1
		// (at least, i haven't encountered them before)
		auto regex = std::regex("(?:\\[.+\\] *)?(.+?)(?: +-)?(?: +|E|EP|e|ep|-|_)(\\d+)(?:.*)");
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






