// cache.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

namespace tag::cache
{
	static std::unordered_map<std::string, std::string> seriesIdCache;

	std::string getSeriesId(const std::string& name)
	{
		return seriesIdCache[name];
	}

	void setSeriesId(const std::string& name, const std::string& id)
	{
		seriesIdCache[name] = id;
	}




	static std::unordered_map<std::string, SeriesMetadata> metaCache;

	SeriesMetadata getSeriesMeta(const std::string& id)
	{
		return metaCache[id];
	}

	void addSeriesMeta(const std::string& id, const SeriesMetadata& meta)
	{
		metaCache[id] = meta;
	}

	bool haveSeriesMeta(const std::string& id)
	{
		return metaCache.find(id) != metaCache.end();
	}
}
