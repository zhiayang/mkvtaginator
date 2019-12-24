// utils.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN 1

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#include <windows.h>
#else
	#include <errno.h>
	#include <unistd.h>
	#include <sys/stat.h>
#endif

#include <fstream>

namespace util
{
	size_t getFileSize(const std::string& path)
	{
		#ifdef _WIN32

			// note: jesus christ this thing is horrendous

			HANDLE hd = CreateFile((LPCSTR) path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(hd == INVALID_HANDLE_VALUE)
				error("failed to get filesize for '%s' (error code %d)", path, GetLastError());

			// ok, presumably it exists. so, get the size
			LARGE_INTEGER sz;
			bool success = GetFileSizeEx(hd, &sz);
			if(!success)
				error("failed to get filesize for '%s' (error code %d)", path, GetLastError());

			CloseHandle(hd);

			return (size_t) sz.QuadPart;

		#else

			struct stat st;
			if(stat(path.c_str(), &st) != 0)
			{
				char buf[128] = { 0 };
				strerror_r(errno, buf, 127);
				util::error("failed to get filesize for '%s' (error code %d / %s)", path, errno, buf);

				return -1;
			}

			return st.st_size;

		#endif
	}

	std::pair<uint8_t*, size_t> readEntireFile(const std::string& path)
	{
		auto bad = std::pair(nullptr, 0);;

		auto sz = getFileSize(path);
		if(sz == -1) return bad;

		// i'm lazy, so just use fstreams.
		auto fs = std::fstream(path);
		if(!fs.good()) return bad;


		uint8_t* buf = new uint8_t[sz + 1];
		fs.read(reinterpret_cast<char*>(buf), sz);
		fs.close();

		return std::pair(buf, sz);
	}












	static int log_indent = 0;
	void indent_log(int n)    { log_indent += n; }
	void unindent_log(int n)  { log_indent = std::max(0, log_indent - n); }
	int get_log_indent()      { return log_indent; }
}
