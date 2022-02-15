# Makefile for Orion-X3/Orion-X4/mx and derivatives
# Written in 2011
# This makefile is licensed under the WTFPL



WARNINGS        := -Wno-unused-parameter -Wno-sign-conversion -Wno-padded -Wno-conversion -Wno-shadow -Wno-missing-noreturn -Wno-unused-macros -Wno-switch-enum -Wno-deprecated -Wno-format-nonliteral -Wno-trigraphs -Wno-unused-const-variable -Wno-deprecated-declarations -Wno-missing-field-initializers

OUTPUT          := build/mkvtaginator

CC              ?= "clang"
CXX             ?= "clang++"

CXXSRC          := $(shell find source external -iname "*.cpp")
CXXOBJ          := $(CXXSRC:.cpp=.cpp.o)
CXXDEPS         := $(CXXSRC:.cpp=.cpp.d)

NUMFILES        := $$(($(words $(CXXSRC))))

DEFINES         := -D__USE_MINGW_ANSI_STDIO=1
SANITISE        :=

CXXFLAGS        += -std=c++17 -fvisibility=hidden -O3 -c -Wall -Wextra -fno-omit-frame-pointer $(SANITISE) $(DEFINES)
LDFLAGS         += $(SANITISE) -fvisibility=hidden

PRECOMP_HDRS    := source/include/precompile.h
PRECOMP_GCH     := $(PRECOMP_HDRS:.h=.h.gch)

UNAME_IDENT     := $(shell uname)
COMPILER_IDENT  := $(shell $(CC) --version | head -n 1)

CURL_CFLAGS     := $(shell pkg-config --cflags libcurl)
CURL_LDFLAGS    := $(shell pkg-config --libs libcurl)

LIBAV_CFLAGS    := $(shell pkg-config --cflags libavformat libavutil libavcodec)
LIBAV_LDFLAGS   := $(shell pkg-config --libs libavformat libavutil libavcodec)



CXXFLAGS += $(CURL_CFLAGS) $(LIBAV_CFLAGS)
LDFLAGS  += $(CURL_LDFLAGS) $(LIBAV_LDFLAGS)


ifeq ("$(UNAME_IDENT)","Darwin")
else
	LDFLAGS += -lpthread
endif


.DEFAULT_GOAL = all
-include $(CXXDEPS)


.PHONY: clean all

build: all
all: $(OUTPUT)


$(OUTPUT): $(PRECOMP_GCH) $(CXXOBJ)
	@printf "# linking\n"
	@mkdir -p $(dir $(OUTPUT))
	@$(CXX) -o $@ $(CXXOBJ) $(LDFLAGS)

%.cpp.o: %.cpp
	@$(eval DONEFILES += "CPP")
	@printf "# compiling [$(words $(DONEFILES))/$(NUMFILES)] $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -include source/include/precompile.h -Isource/include -Iexternal/include -MMD -MP -o $@ $<

%.h.gch: %.h
	@printf "# precompiling header $<\n"
	@$(CXX) $(CXXFLAGS) $(WARNINGS) -o $@ $<



# haha
clena: clean
clean:
	@rm -f $(OUTPUT)
	@find source -name "*.o" | xargs rm -f
	@find source -name "*.gch*" | xargs rm -f
	@find source -name "*.pch*" | xargs rm -f

	@find source -name "*.c.m" | xargs rm -f
	@find source -name "*.c.d" | xargs rm -f
	@find source -name "*.cpp.m" | xargs rm -f
	@find source -name "*.cpp.d" | xargs rm -f









