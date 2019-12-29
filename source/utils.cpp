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
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/stat.h>
	#include <sys/ioctl.h>
#endif

#include <stdlib.h>
#include <fstream>

extern int wcwidth(wchar_t ucs);

namespace util
{
	std::string getEnvironmentVar(const std::string& name)
	{
	#if OS_WINDOWS
		char buffer[256] = { 0 };
		size_t len = 0;

		if(getenv_s(&len, buffer, name.c_str()) != 0)
			return "";

		else
			return std::string(buffer, len);
	#else
		if(char* val = getenv(name.c_str()); val)
			return std::string(val);

		else
			return "";
	#endif
	}


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
		if(sz == static_cast<size_t>(-1)) return bad;

		// i'm lazy, so just use fstreams.
		auto fs = std::fstream(path);
		if(!fs.good()) return bad;


		uint8_t* buf = new uint8_t[sz + 1];
		fs.read(reinterpret_cast<char*>(buf), sz);
		fs.close();

		return std::pair(buf, sz);
	}

	std::wstring corruptUTF8ToWChar(const std::string& str)
	{
	    std::mbstate_t state = std::mbstate_t();
	    const char* s = str.c_str();

	    size_t len = 1 + std::mbsrtowcs(NULL, &s, 0, &state);
	    auto buf = new wchar_t[len];

	    std::mbsrtowcs(buf, &s, len, &state);

	    auto ret = std::wstring(buf, len - 1);
	    delete[] buf;

	    return ret;
	}

	size_t displayedTextLength(const std::string_view& str)
	{
		size_t len = 0;
		auto ws = corruptUTF8ToWChar(std::string(str));

		for(wchar_t wc : ws)
			len += wcwidth(wc);

		return len;
	}

	std::string sanitiseFilename(std::string name)
	{
		const std::vector<char> blacklist = {
			'/', '\\', ':', '<', '>', '|', '"', '?', '*'
		};

		for(size_t i = 0; i < name.size(); i++)
		{
			if(util::matchAny(blacklist, util::equals_to(name[i])))
				name[i] = '_';
		}

		return name;
	}




#ifdef _MSC_VER
#else
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

	size_t getTerminalWidth()
	{
		#if OS_WINDOWS
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
			return csbi.srWindow.Right - csbi.srWindow.Left + 1;
		}
		#else
		{
			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			return w.ws_col;
		}
		#endif
	}

#ifdef _MSC_VER
#else
	#pragma GCC diagnostic pop
#endif


















	static int log_indent = 0;
	void indent_log(int n)    { log_indent += n; }
	void unindent_log(int n)  { log_indent = std::max(0, log_indent - n); }
	int get_log_indent()      { return log_indent; }
}
