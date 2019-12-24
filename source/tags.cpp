// tags.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "tinyxml2.h"

namespace tags
{
	tinyxml2::XMLDocument* serialiseMetadata(const EpisodeMetadata& meta)
	{
		auto xml = new tinyxml2::XMLDocument();
		auto decl = xml->InsertFirstChild(xml->NewDeclaration());
		auto dtd = xml->InsertAfterChild(decl, xml->NewUnknown("DOCTYPE Tags SYSTEM \"matroskatags.dtd\""));

		auto tags = xml->InsertAfterChild(dtd, xml->NewElement("Tags"));

		auto make_tag = [&xml](const std::string& name, const std::string& contents) -> tinyxml2::XMLElement* {
			auto simple = xml->NewElement("Simple");
			auto n = xml->NewElement("Name");
			auto s = xml->NewElement("String");

			n->SetText(name.c_str());
			s->SetText(contents.c_str());

			simple->InsertFirstChild(n);
			simple->InsertAfterChild(n, s);

			return simple;
		};


		#if 1

		auto tag = tags->InsertFirstChild(xml->NewElement("Tag"));
		auto n = tag->InsertFirstChild(xml->NewElement("Targets"));

		{
			n = tag->InsertAfterChild(n, make_tag("SHOW", meta.seriesMeta.name));
			n = tag->InsertAfterChild(n, make_tag("COLLECTION/TITLE", meta.seriesMeta.name));
			n = tag->InsertAfterChild(n, make_tag("ACTOR", util::listToString(meta.actors, util::identity(), false)));
			n = tag->InsertAfterChild(n, make_tag("GENRE", util::listToString(meta.seriesMeta.genres, util::identity(), false)));
			n = tag->InsertAfterChild(n, make_tag("CONTENT_TYPE", "TV Show"));
		}

		{
			n = tag->InsertAfterChild(n, make_tag("SEASON.PART_NUM", std::to_string(meta.seasonNumber)));
			n = tag->InsertAfterChild(n, make_tag("SEASON/PART_NUMBER", std::to_string(meta.seasonNumber)));
		}

		{
			n = tag->InsertAfterChild(n, make_tag("EPISODE/TITLE", meta.name));
			n = tag->InsertAfterChild(n, make_tag("DIRECTOR", util::listToString(meta.directors, util::identity(), false)));
			n = tag->InsertAfterChild(n, make_tag("WRITTEN_BY", util::listToString(meta.writers, util::identity(), false)));
			n = tag->InsertAfterChild(n, make_tag("SCREENPLAY_BY", util::listToString(meta.writers, util::identity(), false)));

			n = tag->InsertAfterChild(n, make_tag("DATE_RELEASE", meta.airDate));
			n = tag->InsertAfterChild(n, make_tag("DATE_RELEASED", meta.airDate));

			n = tag->InsertAfterChild(n, make_tag("EPISODE.PART_NUM", std::to_string(meta.episodeNumber)));
			n = tag->InsertAfterChild(n, make_tag("EPISODE/PART_NUMBER", std::to_string(meta.episodeNumber)));

			// apparently, according to MetaX, DESCRIPTION is the short one,
			// and SYNOPSIS is the long one. this goes against my better judgement, so the internal
			// naming reflects meta::synopsis as the short version, and meta::description as the long version.
			n = tag->InsertAfterChild(n, make_tag("SYNOPSIS", meta.description));
			n = tag->InsertAfterChild(n, make_tag("SUMMARY", meta.description));
			n = tag->InsertAfterChild(n, make_tag("DESCRIPTION", meta.synopsis));
		}

		#else
		// make a tag.
		auto seriesTag = tags->InsertFirstChild(xml->NewElement("Tag"));
		{
			auto tgt = seriesTag->InsertFirstChild(xml->NewElement("Targets"));
			auto ttv = xml->NewElement("TargetTypeValue");
			auto tt = xml->NewElement("TargetType");

			// series
			ttv->SetText(std::to_string(70).c_str());
			tgt->InsertFirstChild(ttv);

			tt->SetText("COLLECTION");
			tgt->InsertAfterChild(ttv, tt);

			auto n = tgt;

			n = seriesTag->InsertAfterChild(n, make_tag("COLLECTION/TITLE", meta.seriesMeta.name));
			n = seriesTag->InsertAfterChild(n, make_tag("ACTOR", util::listToString(meta.actors, util::identity(), false)));
			n = seriesTag->InsertAfterChild(n, make_tag("GENRE", util::listToString(meta.seriesMeta.genres, util::identity(), false)));
			n = seriesTag->InsertAfterChild(n, make_tag("CONTENT_TYPE", "TV Show"));
		}

		auto seasonTag = tags->InsertAfterChild(seriesTag, xml->NewElement("Tag"));
		{
			auto tgt = seasonTag->InsertFirstChild(xml->NewElement("Targets"));
			auto ttv = xml->NewElement("TargetTypeValue");
			auto tt = xml->NewElement("TargetType");

			// series
			ttv->SetText(std::to_string(60).c_str());
			tgt->InsertFirstChild(ttv);

			tt->SetText("SEASON");
			tgt->InsertAfterChild(ttv, tt);

			seasonTag->InsertAfterChild(tgt, make_tag("SEASON/PART_NUMBER", std::to_string(meta.seasonNumber)));
		}


		auto episodeTag = tags->InsertAfterChild(seasonTag, xml->NewElement("Tag"));
		{
			auto tgt = episodeTag->InsertFirstChild(xml->NewElement("Targets"));
			auto ttv = xml->NewElement("TargetTypeValue");
			auto tt = xml->NewElement("TargetType");

			// series
			ttv->SetText(std::to_string(50).c_str());
			tgt->InsertFirstChild(ttv);

			tt->SetText("EPISODE");
			tgt->InsertAfterChild(ttv, tt);

			auto n = tgt;

			n = episodeTag->InsertAfterChild(n, make_tag("EPISODE/TITLE", meta.name));
			n = episodeTag->InsertAfterChild(n, make_tag("DIRECTOR", util::listToString(meta.directors, util::identity(), false)));
			n = episodeTag->InsertAfterChild(n, make_tag("WRITTEN_BY", util::listToString(meta.writers, util::identity(), false)));
			n = episodeTag->InsertAfterChild(n, make_tag("SCREENPLAY_BY", util::listToString(meta.writers, util::identity(), false)));

			n = episodeTag->InsertAfterChild(n, make_tag("DATE_RELEASE", meta.airDate));
			n = episodeTag->InsertAfterChild(n, make_tag("DATE_RELEASED", meta.airDate));

			n = episodeTag->InsertAfterChild(n, make_tag("EPISODE/PART_NUMBER", std::to_string(meta.seasonNumber)));

			n = episodeTag->InsertAfterChild(n, make_tag("SYNOPSIS", meta.synopsis));
			n = episodeTag->InsertAfterChild(n, make_tag("DESCRIPTION", meta.description));
		}

		#endif


		return xml;
	}
}
