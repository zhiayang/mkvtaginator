// main.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include <regex>
#include <fstream>

#include "defs.h"
#include "cpr/cpr.h"

#include "picojson.h"
namespace pj = picojson;

#include "tinyxml2.h"
#include "tinyprocesslib/tinyprocess.h"

namespace std
{
	namespace fs = std::filesystem;
}

int main(int argc, char** argv)
{
	auto files = args::parseCmdLineOpts(argc, argv);
	util::log("received %zu %s", files.size(), util::plural("file", files.size()));

	if(!args::getManualSeriesId().empty())
		util::info("skipping search: using series id '%s'", args::getManualSeriesId());

	tvdb::login();
	moviedb::login();

	driver::createOutputFolder();

	auto paths = driver::collectFiles(files);

	size_t doneFiles = 0;
	for(const auto& filepath : paths)
	{
		auto ok = driver::processOneFile(filepath);

		if(ok) doneFiles += 1;
	}

	util::info("processed %d %s", doneFiles, util::plural("file", doneFiles));
}















