// tvmaze.cpp
// Copyright (c) 2022, zhiayang
// SPDX-License-Identifier: Apache-2.0

#include <regex>

#include "defs.h"
#include "cpr/cpr.h"
#include "picojson.h"

namespace pj = picojson;

namespace tag::tvmaze
{
	static constexpr const char* API_URL = "https://api.tvmaze.com";

	static std::string format_date(const std::string& str)
	{
		std::smatch sm;
		std::regex_match(str, sm, std::regex("(\\d+)-(\\d+)-(\\d+)"));
		if(sm.size() < 4) return str;

		int y = std::stoi(sm[1]);
		int m = std::stoi(sm[2]);
		int d = std::stoi(sm[3]);

		return zpr::sprint("%04d-%02d-%02d", y, m, d);
	}

	static SeriesMetadata fetchSeriesMetadata(const std::string& name, const std::string& manualSeriesId)
	{
		SeriesMetadata ret {};
		std::string seriesId {};

		if(manualSeriesId.empty())
		{
			if(auto id = cache::getSeriesId(name); !id.empty())
			{
				seriesId = id;
			}
			else
			{
				// for searching, "sanitise" the input by replacing '.' and '-' with spaces
				auto search_name = name;
				for(char& c : search_name)
					if(c == '.' || c == '-')
						c = ' ';

				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/search/shows", API_URL)),
					cpr::Parameters({{ "q", search_name }})
				);

				if(r.status_code != 200)
				{
					util::error("http request failed (searching series by name) - '%s'", r.url); util::indent_log();
					util::info("status: %d", r.status_code);
					if(r.status_code != 404) util::info("body: %s", r.text);

					util::unindent_log();
					goto fail;
				}

				pj::value resp {};
				pj::parse(resp, r.text);

				auto& results = resp.as_arr();
				if(results.empty())
					goto fail;

				size_t sel = 0;
				if(results.size() > 1)
				{
					std::vector<misc::Option> options {};
					for(auto& x : results)
					{
						auto& show_info = x.get("show").as_obj();

						misc::Option opt {};

						opt.title = show_info["name"].as_str();

						opt.infos.push_back({
							.heading = "match confidence",
							.body = std::to_string(x.get("score").get<double>())
						});

						if(auto airdate = show_info["premiered"]; !airdate.is<pj::null>())
						{
							opt.infos.push_back({
								.heading = "aired:",
								.subheading = format_date(airdate.as_str())
							});
						}

						opt.infos.push_back({
							.heading = "id:",
							.subheading = std::to_string(show_info["id"].as_int())
						});

						if(auto overview = show_info["summary"]; !overview.is<pj::null>())
						{
							opt.infos.push_back({
								.heading = "overview:",
								.body = overview.as_str()
							});
						}

						options.push_back(std::move(opt));
					}


					// TODO: make this configurable
					constexpr size_t limit = 3;

					util::info("multiple matches (detected series: %s):", name);

					bool more = false;
					sel = misc::userChoice(options, &more, 0, limit);

					// if they wanted more, print the rest.
					if(more)
					{
						zpr::println("");
						sel = misc::userChoice(options, &more, limit);
					}

					if(sel == 0)
						goto fail;

					// 'sel' here is 1-indexed.
					sel -= 1;
				}

				seriesId = std::to_string(results[sel].as_obj()["show"].as_obj()["id"].as_int());
				cache::setSeriesId(name, seriesId);
			}
		}
		else
		{
			seriesId = manualSeriesId;
		}

		if(seriesId.empty())
			goto fail;

		if(cache::haveSeriesMeta(seriesId))
		{
			ret = cache::getSeriesMeta(seriesId);
		}
		else
		{
			ret.id = seriesId;

			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/shows/%s", API_URL, seriesId))
			);

			if(r.status_code != 200)
			{
				util::error("http request failed (get series info) - '%s'", r.url); util::indent_log();
				util::info("status: %d", r.status_code);
				if(r.status_code != 404) util::info("body: %s", r.text);

				util::unindent_log();
				goto fail;
			}


			pj::value resp {};
			pj::parse(resp, r.text);
			auto& data = resp.as_obj();

			ret.dbName  = data["name"].as_str();
			ret.airDate = format_date(data["premiered"].as_str());
			ret.genres  = util::map(data["genres"].as_arr(), [](const pj::value& x) -> std::string {
				return x.as_str();
			});

			// actors come from elsewhere:
			{
				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/shows/%s/cast", API_URL, seriesId))
				);

				if(r.status_code != 200)
				{
					util::error("http failed (get series actors) - '%s'", r.url); util::indent_log();
					util::info("status: %d", r.status_code);
					if(r.status_code != 404) util::info("body: %s", r.text);

					util::unindent_log();

					// actors aren't that important, so just skip it. no fail.
				}
				else
				{
					pj::value resp;
					pj::parse(resp, r.text);
					auto& data = resp.as_arr();

					ret.actors = util::map(data, [](const pj::value& v) -> auto {
						auto& foo = v.get("person").as_obj();
						return foo.at("name").as_str();
					});
				}
			}

			ret.name = ret.dbName;

			if(auto x = config::getManualSeriesTitle(); !x.empty())
				ret.name = x;

			else if(config::isOverridingSeriesName())
				ret.name = name;
		}

		ret.valid = true;

	fail:
		return ret;
	}




	EpisodeMetadata fetchEpisodeMetadata(const std::string& series, int season, int episode, const std::string& title,
		const std::string& manualSeriesId)
	{
		EpisodeMetadata ret {};
		ret.seriesMeta = fetchSeriesMetadata(series, manualSeriesId);
		if(!ret.seriesMeta.valid)
			return ret;

		{
			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/shows/%s/episodebynumber", API_URL, ret.seriesMeta.id)),
				cpr::Parameters({
					{ "season", std::to_string(season) },
					{ "number", std::to_string(episode) }
				})
			);

			if(r.status_code != 200)
			{
				util::error("http failed (get episode info) - '%s'", r.url); util::indent_log();
				util::info("status: %d", r.status_code);
				if(r.status_code != 404) util::info("body: %s", r.text);

				util::unindent_log();
				goto fail;
			}


			pj::value resp {};
			pj::parse(resp, r.text);

			auto& data = resp.as_obj();

			ret.id              = std::to_string(data["id"].as_int());
			ret.airDate         = format_date(data["airdate"].as_str());
			ret.seasonNumber    = season;
			ret.episodeNumber   = episode;

			if(auto ovv = data["summary"]; !ovv.is<pj::null>())
				ret.description = ovv.as_str();

			ret.synopsis = ret.description.empty()
				? "" : ret.description.substr(0, 250);

			if(ret.description.size() > 250)
				ret.synopsis += "...";

			if(auto epName = data["name"]; !epName.is<pj::null>())
				ret.dbName = epName.get<std::string>();

			ret.actors = ret.seriesMeta.actors;
			ret.name = ret.dbName;

			if(config::isOverridingEpisodeName() && !title.empty())
				ret.name = title;
		}

		ret.valid = true;
	fail:
		return ret;
	}
}
