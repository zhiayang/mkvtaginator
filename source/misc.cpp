// misc.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#include <sstream>

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
			s.erase(std::remove(s.begin(), s.end(), '\r'));

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


	size_t userChoice(const std::vector<Option>& options, bool* showmore, size_t first, size_t limit)
	{
		if(first == 0)  util::info("multiple matches:");
		else            zpr::println("");


		size_t size = (limit == 0 ? options.size() : std::min(limit, options.size()));
		for(size_t i = first; i < size; i++)
		{
			zpr::println("%s  %s[%0*d]%s: %s%s%s%s", std::string(2 * util::get_log_indent(), ' '), COLOUR_GREEN_BOLD,
				options.size() > 9 ? 2 : 1, i + 1, COLOUR_RESET, COLOUR_BLACK_BOLD, options[i].title, COLOUR_RESET,
				options[i].altTitle.empty() ? "" : zpr::sprint(" (alt: %s%s%s)", COLOUR_BLACK_BOLD, options[i].altTitle, COLOUR_RESET));

			auto pad = std::string(3 + (2 * util::get_log_indent()) + (options.size() > 9 ? 2 : 1), ' ');

			if(options[i].infos.size() > 0)
			{
				for(const auto& info : options[i].infos)
				{
					zpr::println("%s%s* %s%s%s%s %s", std::string(1 + (2 * util::get_log_indent()) + (options.size() > 9 ? 2 : 1), ' '),
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
			}

			zpr::println("");
		}

		bool more = false;
		if(size < options.size())
		{
			more = true;
			zpr::println("%s  %s[..]%s: %s%d more results...%s\n", std::string(2 * util::get_log_indent(), ' '),
				COLOUR_GREEN_BOLD, COLOUR_RESET, COLOUR_BLACK_BOLD, options.size() - size, COLOUR_RESET);
		}

		zpr::print("%s %s*%s selection [%d - %d, 0 to skip%s]: ", std::string(2 * util::get_log_indent(), ' '),
			COLOUR_GREEN_BOLD, COLOUR_RESET, 1, size, more ? ", 'm' for more" : "");

		int x = 0;
		int n = fscanf(stdin, "%d", &x);
		if(n <= 0 || x <= 0 || x > options.size())
		{
			if(fgetc(stdin) == 'm' && showmore)
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

		return static_cast<size_t>(x);
	}
}









