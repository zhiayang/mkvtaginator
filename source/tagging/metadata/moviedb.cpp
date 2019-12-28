// moviedb.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>

#include "cpr/cpr.h"
#include "defs.h"
#include "picojson.h"

namespace pj = picojson;

namespace tag::moviedb
{
	static constexpr const char* API_URL = "https://api.themoviedb.org/3";

	MovieMetadata fetchMovieMetadata(const std::string& title, int year, const std::string& manualId)
	{
		moviedb::login();

		MovieMetadata ret;

		std::string movieId;

		if(manualId.empty())
		{
			// note: there's no need to cache this, since we don't do multiple movies
			// with the same title (what's the point of that?) unlike tv shows.

			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/search/movie", API_URL)),
				cpr::Parameters({
					{ "api_key", getToken() },
					{ "query", title },
					// { "year", year },
					{ "include_adult", true }
				})
			);

			if(r.status_code != 200)
			{
				util::error("http request failed (searching movie) - '%s'", r.url); util::indent_log();
				util::info("status: %d", r.status_code);
				if(r.status_code != 404) util::info("body: %s", r.text);

				util::unindent_log();
				goto fail;
			}

			pj::value resp;
			pj::parse(resp, r.text);

			auto results = resp.get("results").get<pj::array>();
			if(results.empty())
				goto fail;

			size_t sel = 0;

			if(results.size() > 1)
			{
				std::vector<misc::Option> options;
				for(const auto& x : results)
				{
					auto id = std::to_string(static_cast<size_t>(x.get("id").get<double>()));

					misc::Option opt;

					opt.title = x.get("title").get<std::string>();
					if(auto orig = x.get("original_title"); !orig.is<pj::null>())
					{
						opt.altTitle = orig.get<std::string>();

						// obviously we use the original name, cos we're cultured
						if(!config::isPreferEnglishTitle())
							std::swap(opt.title, opt.altTitle);
					}

					{
						misc::Option::Info info;
						info.heading = "date:";

						// it looks like moviedb formats dates correctly (ie. pads to 2 digits)
						info.subheading = x.get("release_date").get<std::string>();

						opt.infos.push_back(info);
					}

					{
						misc::Option::Info info;

						info.heading = "id:";
						info.subheading = id;

						opt.infos.push_back(info);
					}

					// get alternative titles if we can:
					{
						misc::Option::Info info;
						info.heading = "aliases:";

						auto r = cpr::Get(
							cpr::Url(zpr::sprint("%s/movie/%s/alternative_titles", API_URL, id)),
							cpr::Parameters({{ "api_key", getToken() }})
						);

						// ignore errors here since it's not important.
						if(r.status_code == 200)
						{
							pj::value resp;
							pj::parse(resp, r.text);

							auto titles = resp.get("titles").get<pj::array>();
							for(const auto& val : titles)
							{
								// only english alt titles are interesting to us. also, if the type is empty then it
								// was the original title anyway, so just skip that too.
								if(val.get("iso_3166_1").get<std::string>() == "US"
									&& !val.get("type").get<std::string>().empty())
								{
									info.items.push_back(val.get("title").get<std::string>());
								}
							}
						}

						if(!info.items.empty())
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

				util::info("multiple matches:");

				bool more = false;
				size_t sel = misc::userChoice(options, &more, 0, limit);

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

			movieId = std::to_string(static_cast<size_t>(results[sel].get("id").get<double>()));
		}
		else
		{
			movieId = manualId;
		}

		if(movieId.empty())
			goto fail;


		ret.id = movieId;
		{
			auto r = cpr::Get(
				cpr::Url(zpr::sprint("%s/movie/%s", API_URL, movieId)),
				cpr::Parameters({{ "api_key", getToken() }})
			);

			if(r.status_code != 200)
			{
				util::error("http request failed (get movie info) - '%s'", r.url); util::indent_log();
				util::info("status: %d", r.status_code);
				if(r.status_code != 404) util::info("body: %s", r.text);

				util::unindent_log();
				goto fail;
			}

			pj::value data;
			pj::parse(data, r.text);

			ret.title           = data.get("title").get<std::string>();
			ret.originalTitle   = data.get("original_title").get<std::string>();
			ret.airDate         = data.get("release_date").get<std::string>();
			{
				std::smatch sm;
				std::regex_match(ret.airDate, sm, std::regex("(\\d+)-(\\d+)-(\\d+)"));

				if(sm.size() != 4)  ret.year = year, util::error("malformed date '%s'!", ret.airDate);
				else                ret.year = std::stoi(sm[1]);
			}

			if(auto overview = data.get("overview"); !overview.is<pj::null>())
				ret.synopsis = overview.get<std::string>();

			ret.genres = util::map(data.get("genres").get<pj::array>(), [](const pj::value& x) -> std::string {
				return x.get("name").get<std::string>();
			});

			ret.productionStudios = util::map(data.get("production_companies").get<pj::array>(), [](const pj::value& x) -> std::string {
				return x.get("name").get<std::string>();
			});


			// actors is a separate thing:
			{
				auto r = cpr::Get(
					cpr::Url(zpr::sprint("%s/movie/%s/credits", API_URL, movieId)),
					cpr::Parameters({{ "api_key", getToken() }})
				);

				if(r.status_code != 200)
				{
					util::error("http request failed (get movie cast) - '%s'", r.url); util::indent_log();
					util::info("status: %d", r.status_code);
					if(r.status_code != 404) util::info("body: %s", r.text);

					util::unindent_log();

					// cast isn't that important, it's not a hard error.
				}
				else
				{
					pj::value data;
					pj::parse(data, r.text);

					ret.cast = util::map(data.get("cast").get<pj::array>(), [](const pj::value& v)
						-> std::pair<std::string, std::string>
					{
						auto actor = v.get("name").get<std::string>();
						std::string played;
						if(!v.get("character").is<pj::null>())
							played = v.get("character").get<std::string>();

						return { actor, played };
					});


					ret.writers = util::filterMap(data.get("crew").get<pj::array>(), [](const pj::value& v) -> bool {
						return util::match(v.get("job").get<std::string>(),
							"writer", "Writer");
					}, [](const pj::value& v) -> std::string {
						return v.get("name").get<std::string>();
					});

					ret.directors = util::filterMap(data.get("crew").get<pj::array>(), [](const pj::value& v) -> bool {
						return util::match(v.get("job").get<std::string>(),
							"director", "Director");
					}, [](const pj::value& v) -> std::string {
						return v.get("name").get<std::string>();
					});

					ret.producers = util::filterMap(data.get("crew").get<pj::array>(), [](const pj::value& v) -> bool {
						return util::match(v.get("job").get<std::string>(),
							"producer", "Producer");
					}, [](const pj::value& v) -> std::string {
						return v.get("name").get<std::string>();
					});

					ret.coproducers = util::filterMap(data.get("crew").get<pj::array>(), [](const pj::value& v) -> bool {
						return util::match(v.get("job").get<std::string>(),
							"coproducer", "Coproducer", "co-producer", "Co-producer", "Co-Producer");
					}, [](const pj::value& v) -> std::string {
						return v.get("name").get<std::string>();
					});

					ret.execProducers = util::filterMap(data.get("crew").get<pj::array>(), [](const pj::value& v) -> bool {
						return util::match(v.get("job").get<std::string>(),
							"executive producer", "Executive producer", "Executive Producer");
					}, [](const pj::value& v) -> std::string {
						return v.get("name").get<std::string>();
					});
				}
			}

			// ok. if we're overriding, then override:
			if(config::isOverridingMovieName() && !title.empty())
				ret.title = title;

			else if(!config::isPreferEnglishTitle())
				std::swap(ret.title, ret.originalTitle);
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
		auto key = config::getMovieDBApiKey();
		if(key.empty())
		{
			util::error("%serror:%s missing api-key for TheMovieDB (use '--moviedb-api <api_key>', see '--help')",
				COLOUR_RED_BOLD, COLOUR_RESET);
			exit(-1);
		}

		// there's actually no need to login, lmao.
		setToken(key);
	}
}







