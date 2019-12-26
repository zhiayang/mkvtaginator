// nameparser.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include "defs.h"

namespace misc
{
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
				return { };

			series = sm[1];
			season = std::stoi(sm[2]);
			episode = std::stoi(sm[3]);

			if(sm.size() > 4)
				title = sm[4];
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

			title = sm[1];
			year = std::stoi(sm[2]);
		}

		return std::tuple(title, year);
	}
}
