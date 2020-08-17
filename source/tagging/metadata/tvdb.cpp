// tvdb.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>

#include "cpr/cpr.h"
#include "defs.h"
#include "picojson.h"

namespace pj = picojson;

// https://matroska.org/technical/specs/tagging/example-video.html#episode

namespace tag::tvdb
{
	static constexpr const char* API_URL = "https://api.thetvdb.com";

	static std::string format_date(const std::string& str)
	{
		std::smatch sm;
		std::regex_match(str, sm, std::regex("(\\d+)-(\\d+)-(\\d+)"));
		if(sm.size() < 4) return str;

		int y = std::stoi(sm[1]);
		int m = std::stoi(sm[2]);
		int d = std::stoi(sm[3]);

		return zpr::sprint("%d-%02d-%02d", y, m, d);
	}

	static SeriesMetadata fetchSeriesMetadata(const std::string& name, const std::string& manualSeriesId)
	{
		SeriesMetadata ret;

		std::string seriesId;
		if(manualSeriesId.empty())
		{
			if(auto id = cache::getSeriesId(name); !id.empty())
			{
				seriesId = id;
			}
			else
			{
				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/search/series", API_URL)),
					cpr::Parameters({{ "name", name }}),
					cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }})
				);

				if(r.status_code != 200)
				{
					util::error("http request failed (searching series by name) - '%s'", r.url); util::indent_log();
					util::info("status: %d", r.status_code);
					if(r.status_code != 404) util::info("body: %s", r.text);

					util::unindent_log();
					goto fail;
				}

				pj::value resp;
				pj::parse(resp, r.text);

				auto results = resp.get("data").get<pj::array>();
				if(results.empty())
					goto fail;


				size_t sel = 0;
				if(results.size() > 1)
				{
					std::vector<misc::Option> options;
					for(const auto& x : results)
					{
						misc::Option opt;

						opt.title = x.get("seriesName").get<std::string>();

						if(auto airdate = x.get("firstAired"); !airdate.is<pj::null>())
						{
							misc::Option::Info info;

							info.heading = "aired:";
							info.subheading = format_date(airdate.get<std::string>());

							opt.infos.push_back(info);
						}

						{
							misc::Option::Info info;

							info.heading = "id:";
							info.subheading = std::to_string(static_cast<size_t>(x.get("id").get<double>()));

							opt.infos.push_back(info);
						}

						if(auto aliases = x.get("aliases").get<pj::array>(); !aliases.empty())
						{
							misc::Option::Info info;

							info.heading = "aliases:";
							info.items = util::map(aliases, [](const pj::value& x) -> auto {
								return x.get<std::string>();
							});

							opt.infos.push_back(info);
						}


						if(auto overview = x.get("overview"); !overview.is<pj::null>())
						{
							misc::Option::Info info;
							info.heading = "overview:";

							info.body = overview.get<std::string>();
							opt.infos.push_back(info);
						}

						options.push_back(opt);
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

				seriesId = std::to_string(static_cast<size_t>(results[sel].get("id").get<double>()));
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
				cpr::Url(zpr::sprint("%s/series/%s", API_URL, seriesId)),
				cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }})
			);

			if(r.status_code != 200)
			{
				util::error("http request failed (get series info) - '%s'", r.url); util::indent_log();
				util::info("status: %d", r.status_code);
				if(r.status_code != 404) util::info("body: %s", r.text);

				util::unindent_log();
				goto fail;
			}


			pj::value resp;
			pj::parse(resp, r.text);
			auto data = resp.get("data");

			ret.dbName  = data.get("seriesName").get<std::string>();
			ret.airDate = format_date(data.get("firstAired").get<std::string>());
			ret.genres  = util::map(data.get("genre").get<pj::array>(), [](const pj::value& x) -> std::string {
				return x.get<std::string>();
			});


			// actors come from elsewhere:
			{
				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/series/%s/actors", API_URL, seriesId)),
					cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }})
				);

				if(r.status_code != 200)
				{
					util::error("http failed (get series actors) - '%s'", r.url); util::indent_log();
					util::info("status: %d", r.status_code);
					if(r.status_code != 404) util::info("body: %s", r.text);

					util::unindent_log();

					// actors isn't that important, so just skip it. no fail.
				}
				else
				{
					pj::value data;
					pj::parse(data, r.text);
					data = data.get("data");

					ret.actors = util::map(data.get<pj::array>(), [](const pj::value& v) -> auto {
						return v.get("name").get<std::string>();
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
		tvdb::login();


		EpisodeMetadata ret;
		ret.seriesMeta = fetchSeriesMetadata(series, manualSeriesId);
		if(!ret.seriesMeta.valid)
			return ret;

		{
			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/series/%s/episodes/query", API_URL, ret.seriesMeta.id)),
				cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }}),
				cpr::Parameters({
					{ "airedSeason", std::to_string(season) },
					{ "airedEpisode", std::to_string(episode) }
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


			pj::value resp;
			pj::parse(resp, r.text);

			pj::value data;
			if(auto xs = resp.get("data").get<pj::array>(); xs.empty())
				goto fail;

			else
				data = xs[0];

			ret.id              = std::to_string(data.get("id").get<double>());
			ret.airDate         = data.get("firstAired").get<std::string>();
			ret.seasonNumber    = season;
			ret.episodeNumber   = episode;

			if(auto ovv = data.get("overview"); !ovv.is<pj::null>())
				ret.description = ovv.get<std::string>();

			ret.synopsis = ret.description.empty()
				? "" : ret.description.substr(0, 250);

			if(ret.description.size() > 250)
				ret.synopsis += "...";

			if(auto epName = data.get("episodeName"); !epName.is<pj::null>())
				ret.dbName = epName.get<std::string>();

			ret.writers = util::map(data.get("writers").get<pj::array>(),
				[](const pj::value& v) -> auto { return v.get<std::string>(); });

			ret.directors = util::map(data.get("directors").get<pj::array>(),
				[](const pj::value& v) -> auto { return v.get<std::string>(); });

			ret.actors = ret.seriesMeta.actors;
			ret.name = ret.dbName;

			if(config::isOverridingEpisodeName() && !title.empty())
				ret.name = title;
		}

		ret.valid = true;
	fail:
		return ret;
	}










	static std::string authToken;
	static void setToken(const std::string& token)
	{
		authToken = token;
	}

	std::string getToken()
	{
		return authToken;
	}

	void login()
	{
		if(!authToken.empty())
			return;

		auto key = config::getTVDBApiKey();
		if(key.empty())
		{
			util::error("%serror:%s missing api-key for theTVDB (use '--tvdb-api <api_key>', see '--help')",
				COLOUR_RED_BOLD, COLOUR_RESET);
			exit(-1);
		}

		auto r = cpr::Post(
			cpr::Url(zpr::sprint("%s/login", API_URL)),
			cpr::Body(zpr::sprint("{\"apikey\":\"%s\"}", key)),
			cpr::Header{{"Content-Type", "application/json"}}
		);

		if(r.status_code != 200)
		{
			util::error("failed to login to thetvdb!"); util::indent_log();
			util::info("status: %d", r.status_code);
			if(r.status_code != 404) util::info("body: %s", r.text);

			exit(-1);
		}

		// then get the token:
		{
			pj::value val;
			pj::parse(val, r.text);

			setToken(val.get("token").get<std::string>());
		}
	}
}







