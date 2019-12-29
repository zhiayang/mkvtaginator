// misc.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#include <set>
#include <sstream>
#include <iostream>

namespace misc
{
	static std::vector<std::string_view> split_words(std::string& s)
	{
		std::vector<std::string_view> ret;

		size_t word_start = 0;
		for(size_t i = 0; i < s.size(); i++)
		{
			if(s[i] == ' ')
			{
				ret.push_back(std::string_view(s.c_str() + word_start, i - word_start));
				word_start = i + 1;
			}
			else if(s[i] == '-')
			{
				ret.push_back(std::string_view(s.c_str() + word_start, i - word_start + 1));
				word_start = i + 1;
			}
		}

		ret.push_back(std::string_view(s.c_str() + word_start));
		return ret;
	}

	static inline std::vector<std::string_view> splitString(std::string_view view, char delim = '\n')
	{
		std::vector<std::string_view> ret;

		while(true)
		{
			size_t ln = view.find(delim);

			if(ln != std::string_view::npos)
			{
				ret.emplace_back(view.data(), ln);
				view.remove_prefix(ln + 1);
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

	// ripped from ztmu. don't really need to include the whole thing anyway.
	static void pretty_print_text_block(const std::string& block, const char* leftMargin, const char* rightMargin, size_t maxLines = 0)
	{
		auto tw = util::getTerminalWidth();
		tw = std::min(tw, tw - strlen(leftMargin) - strlen(rightMargin));

		auto disp_len = util::displayedTextLength;

		std::vector<std::string> paragraphs = util::mapFilter(splitString(block), [](std::string_view sv) -> std::string {
			auto s = std::string(sv);
			if(auto it = std::remove(s.begin(), s.end(), '\r'); it != s.end())
				s.erase(it);

			return s;
		}, [](const std::string& s) -> bool {
			return !s.empty();
		});

		size_t lines = 1;
		size_t paras = 0;
		for(auto& l : paragraphs)
		{
			size_t remaining = tw;

			// sighs.
			auto ss = std::stringstream();
			ss << leftMargin;

			// each "line" is actually a paragraph. we want to be nice, so pad on the right by a few spaces
			// and hyphenate split words.

			// first split into words
			auto words = split_words(l);
			for(const auto& word : words)
			{
				if(maxLines > 0 && lines == maxLines && remaining <= word.size() + 4)
				{
					// don't. just quit.
					ss << "...";
					break;
				}

				auto len = disp_len(word);
				if(remaining >= len)
				{
					ss << word << (word.back() != '-' ? " " : "");

					remaining -= len;

					if(remaining > 0)
					{
						remaining -= 1;
					}
					else
					{
						ss << "\n" << leftMargin;
						lines++;

						remaining = tw;
					}
				}
				else if(remaining < 3 || len < 5)
				{
					// for anything less than 5 chars, put it on the next line -- don't hyphenate.
					ss << "\n" << leftMargin << word << (word.back() != '-' ? " " : "");

					remaining = tw - (len + 1);
					lines++;
				}
				else
				{
					auto thisline = remaining - 2;

					// if we end up making a fragment 3 letters or shorter,
					// push it to the next line instead.
					if(std::min(word.size(), thisline ) <= 3)
					{
						thisline = 0;
						ss << "\n" << leftMargin << word << (word.back() != '-' ? " " : "");
					}
					else
					{
						// split it.
						ss << word.substr(0, thisline) << "-" << "\n";
						ss << leftMargin << word.substr(thisline) << " ";
					}

					remaining = tw - word.substr(thisline).size();

					lines++;
				}
			}

			zpr::println(ss.str());

			// if the paragraph has some space remaining at the end, then
			// don't print the next one, just stop here.
			if(maxLines > 0 && lines == maxLines && paras < paragraphs.size())
				break;

			paras++;
		}
	}


	static bool print_options(const std::vector<Option>& options, size_t first, size_t limit)
	{
		size_t size = (limit == 0 ? options.size() : std::min(limit, options.size()));
		for(size_t i = first; i < size; i++)
		{
			zpr::println("%s  %s[%0*d]%s: %s%s%s%s%s", std::string(2 * util::get_log_indent(), ' '), COLOUR_GREEN_BOLD,
				options.size() > 9 ? 2 : 1, i + 1, COLOUR_RESET, COLOUR_BLACK_BOLD, options[i].title, COLOUR_RESET,
				options[i].altTitle.empty() ? "" : zpr::sprint(" (alt: %s%s%s)", COLOUR_BLACK_BOLD, options[i].altTitle, COLOUR_RESET),
				options[i].subTitle.empty() ? "" : zpr::sprint(" - %s", options[i].subTitle));

			auto pad = std::string(3 + (2 * util::get_log_indent()) + (options.size() > 9 ? 2 : 1), ' ');

			if(options[i].infos.size() > 0)
			{
				for(const auto& info : options[i].infos)
				{
					zpr::println("%s%s* %s%s%s%s %s", std::string(3 + (2 * util::get_log_indent()), ' '),
						COLOUR_YELLOW_BOLD, COLOUR_RESET, COLOUR_GREY_BOLD, info.heading, COLOUR_RESET, info.subheading);

					for(const auto& item : info.items)
					{
						zpr::println("%s%s• %s%s", pad, COLOUR_GREY_BOLD, COLOUR_RESET, item);
					}

					if(!info.body.empty())
					{
						auto lm = pad;
						auto rm = std::string(5, ' ');

						pretty_print_text_block(info.body, lm.c_str(), rm.c_str(), /* max_lines: */ 3);
					}
				}

				zpr::println("");
			}
			else if(i + 1 == size)
			{
				zpr::println("");
			}
		}

		bool more = false;
		if(size < options.size())
		{
			more = true;
			zpr::println("%s  %s[..]%s: %s%d more results...%s\n", std::string(2 * util::get_log_indent(), ' '),
				COLOUR_GREEN_BOLD, COLOUR_RESET, COLOUR_BLACK_BOLD, options.size() - size, COLOUR_RESET);
		}

		return more;
	}

	size_t userChoice(const std::vector<Option>& options, bool* showmore, size_t first, size_t limit)
	{
		size_t size = (limit == 0 ? options.size() : std::min(limit, options.size()));

		auto more = print_options(options, first, limit);

		zpr::print("%s %s*%s selection [%d - %d, 0 to skip%s]: ", std::string(2 * util::get_log_indent(), ' '),
			COLOUR_BLUE_BOLD, COLOUR_RESET, 1, size, more ? ", 'm' for more" : "");

		std::string input;
		std::getline(std::cin, input);
		input = util::trim(input);

		int x = 0;
		int n = sscanf(input.c_str(), "%d", &x);
		if(n <= 0 || x <= 0 || static_cast<size_t>(x) > options.size())
		{
			if(!input.empty() && input[0] == 'm' && showmore)
			{
				*showmore = true;
				return 0;
			}
			else
			{
				util::error("invalid option '%d', assuming 0", x);
				x = 0;
			}
		}

		zpr::println("");
		return static_cast<size_t>(x);
	}

	std::vector<size_t> userChoiceMultiple(const std::vector<Option>& options)
	{
		print_options(options, 0, options.size());

		auto pad = std::string(2 * util::get_log_indent(), ' ');

		auto how = zpr::sprint("%s[eg. %s%s1 2 3%s, %s%s1-3%s, %s%s0%s for none, %s%sa%s for all]%s", COLOUR_GREY_BOLD, COLOUR_RESET,
			COLOUR_BLACK_BOLD, COLOUR_GREY_BOLD, COLOUR_RESET, COLOUR_BLACK_BOLD, COLOUR_GREY_BOLD, COLOUR_RESET, COLOUR_BLACK_BOLD,
			COLOUR_GREY_BOLD, COLOUR_RESET, COLOUR_BLACK_BOLD, COLOUR_GREY_BOLD, COLOUR_RESET);

		zpr::print("%s %s*%s selection %s: ", pad, COLOUR_BLUE_BOLD, COLOUR_RESET, how);

		std::string input;
		std::getline(std::cin, input);

		input = util::trim(input);


		auto all = util::rangeClosed(size_t(1), options.size());

		util::indent_log();
		defer(util::unindent_log());
		defer(zpr::println(""));

		if(input.empty() || input[0] == 'a')    return all;
		else if(input[0] == '0')                return { };

		// ok.
		auto xs = util::filter(util::splitString(input, ' '), [](auto x) -> bool { return !x.empty(); });

		std::set<int> opts;
		for(const auto& x : xs)
		{
			// idw handle exceptions, so use scanf.
			int a = 0;
			int b = 0;
			int n = sscanf(x.c_str(), "%d-%d", &a, &b);

			if(n < 2)
			{
				n = sscanf(x.c_str(), "%d", &a);

				if(n <= 0 || static_cast<size_t>(a) > options.size())
					util::error("ignoring invalid input '%s'", x);

				else
					opts.insert(a);
			}
			else
			{
				if(b < a) std::swap(a, b);

				if(a < 1 || static_cast<size_t>(b) > options.size())
					util::error("truncating invalid range");

				a = std::max(a, 1);
				b = std::min(b, static_cast<int>(options.size()));

				for(int i = a; i <= b; i++)
					opts.insert(i);
			}
		}

		std::vector<size_t> ret;
		for(auto x : opts)
			ret.push_back(x);

		return ret;
	}
}








namespace util
{
	std::string prettyPrintTime(uint64_t ns, bool ms)
	{
		auto hours = ns / (1000ULL * 1000 * 1000 * 60 * 60);
		auto mins = (ns / (1000ULL * 1000 * 1000 * 60)) % 60;
		auto secs = (ns / (1000ULL * 1000 * 1000)) % 60;
		auto mils = (ns / (1000ULL * 1000)) % 1000;

		std::string ret;
		if(hours >= 1)      ret += zpr::sprint("%dh ", hours);
		if(mins >= 1)       ret += zpr::sprint("%dm ", mins);
		if(secs >= 1)       ret += zpr::sprint("%ds ", secs);
		if(ms && mils >= 1) ret += zpr::sprint("%dms", mils);

		return ret;
	}

	std::string uglyPrintTime(uint64_t ns, bool ms)
	{
		auto hours = ns / (1000ULL * 1000 * 1000 * 60 * 60);
		auto mins = (ns / (1000ULL * 1000 * 1000 * 60)) % 60;
		auto secs = (ns / (1000ULL * 1000 * 1000)) % 60;
		auto mils = (ns / (1000ULL * 1000)) % 1000;

		if(ms)  return zpr::sprint("%02d:%02d:%02d.%03d", hours, mins, secs, mils);
		else    return zpr::sprint("%02d:%02d:%02d", hours, mins, secs);
	}
}













