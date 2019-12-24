// tvdb.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>

#include "cpr/cpr.h"
#include "defs.h"
#include "picojson.h"

namespace pj = picojson;

// https://matroska.org/technical/specs/tagging/example-video.html#episode

namespace tvdb
{
	static constexpr const char* API_URL = "https://api.thetvdb.com";


	SeriesMetadata fetchSeriesMetadata(const std::string& name, const std::string& manualSeriesId)
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
					util::error("failed to find '%s'", name); util::indent_log();
					util::info("status: %d", r.status_code);
					util::info("body: %s", r.text);

					goto fail;
				}

				pj::value resp;
				pj::parse(resp, r.text);

				auto results = resp.get("data").get<pj::array>();
				if(results.empty())
					goto fail;


				// take the first one for now...
				seriesId = results[0].get("id").get<std::string>();
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
			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/series/%s", API_URL, seriesId)),
				cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }})
			);

			if(r.status_code != 200)
			{
				util::error("failed to find '%s'", name); util::indent_log();
				util::info("status: %d", r.status_code);
				util::info("body: %s", r.text);

				goto fail;
			}


			pj::value resp;
			pj::parse(resp, r.text);
			auto data = resp.get("data");

			ret.name    = data.get("seriesName").get<std::string>();
			ret.airDate = data.get("firstAired").get<std::string>();
			ret.genres  = util::map(data.get("genre").get<pj::array>(), [](const pj::value& x) -> std::string {
				return x.get<std::string>();
			});

			if(args::isOverridingSeriesName())
				ret.name = name;
		}

		ret.id = seriesId;
	fail:
		return ret;
	}


	EpisodeMetadata fetchEpisodeMetadata(const std::string& series, int season, int episode, const std::string& title,
		const std::string& manualSeriesId)
	{
		EpisodeMetadata ret;
		ret.seriesMeta = fetchSeriesMetadata(series, manualSeriesId);

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
				util::error("failed to find '%s' (s: %02d, e: %02d)", series, season, episode); util::indent_log();
				util::info("status: %d", r.status_code);
				util::info("body: %s", r.text);

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
				ret.description     = ovv.get<std::string>();

			ret.synopsis        = ret.description.empty()
				? "" : ret.description.substr(0, 250);

			if(ret.description.size() > 250)
				ret.synopsis += "...";

			if(auto epName = data.get("episodeName"); !epName.is<pj::null>())
				ret.name            = epName.get<std::string>();


			ret.writers         = util::map(data.get("writers").get<pj::array>(),
				[](const pj::value& v) -> auto { return v.get<std::string>(); });

			ret.directors       = util::map(data.get("directors").get<pj::array>(),
				[](const pj::value& v) -> auto { return v.get<std::string>(); });


			// actors come from elsewhere:
			{
				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/series/%s/actors", API_URL, ret.seriesMeta.id)),
					cpr::Header({{ "Authorization", zpr::sprint("Bearer %s", getToken()) }})
				);

				if(r.status_code != 200)
				{
					util::error("failed to find '%s' (s: %02d, e: %02d)", series, season, episode); util::indent_log();
					util::info("status: %d", r.status_code);
					util::info("body: %s", r.text);

					goto fail;
				}

				pj::value data;
				pj::parse(data, r.text);
				data = data.get("data");

				ret.actors = util::map(data.get<pj::array>(), [](const pj::value& v) -> auto {
					return v.get("name").get<std::string>();
				});
			}

			if(args::isOverridingEpisodeName())
				ret.name = title;
		}

	fail:
		return ret;
	}










	static std::string authToken;
	void setToken(const std::string& token)
	{
		authToken = token;
	}

	std::string getToken()
	{
		return authToken;
	}

	void login()
	{
		auto key = args::getTVDBApiKey();
		if(key.empty())
		{
			util::error("error: missing api-key for theTVDB (use '--tvdb-api')");
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
			util::info("body: %s", r.text);

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







