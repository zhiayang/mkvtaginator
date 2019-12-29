// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include <set>
#include <deque>

// what the fuck? i shouldn't have to do this manually...
extern "C" {
	#include <libavutil/log.h>
	#include <libavutil/timestamp.h>
	#include <libavformat/avformat.h>
}

namespace misc
{
	// lmao
	extern std::unordered_map<std::string, std::vector<std::string>> languageCodeMap;
}

namespace mux
{
	template <typename... Args>
	static void error(const std::string& fmt, Args&&... args)
	{
		util::error(fmt, args...);
		if(config::shouldStopOnError())
		{
			util::error("stopping on first error");
			exit(-1);
		}
	}

	static std::pair<double, std::string> get_prefix(double x)
	{
		size_t idx = 0;
		constexpr const char* prefixes = "\0kMGTPE";

		while(x > 1000)
			idx++, x /= 1000;

		return { x, idx == 0 ? "" : std::string(1, prefixes[idx]) };
	}

	static std::string dict_get_value(AVDictionary* dict, const std::string& key)
	{
		if(auto x = av_dict_get(dict, key.c_str(), nullptr, 0); x && x->value)
			return x->value;

		return "";
	}


	// refer: https://github.com/FFmpeg/FFmpeg/blob/10bcc41bb40ba479bfc5ad29b1650a6b335437a8/doc/examples/remuxing.c
	static bool writeOutput(const std::fs::path& outfile, AVFormatContext* inctx, AVFormatContext* ssctx,
		const std::vector<AVStream*>& finalStreams, std::unordered_map<AVStream*, size_t>& finalStreamMap)
	{
		AVFormatContext* outctx = 0;
		if(avformat_alloc_output_context2(&outctx, nullptr, "matroska", outfile.string().c_str()) < 0)
		{
			error("failed to allocate output context");
			return false;
		}

		av_dict_copy(&outctx->metadata, inctx->metadata, 0);

		// make new streams for the output
		for(auto instrm : finalStreams)
		{
			auto outstrm = avformat_new_stream(outctx, nullptr);
			if(!outstrm)
			{
				error("failed to allocate output stream");
				return false;
			}

			avcodec_parameters_copy(outstrm->codecpar, instrm->codecpar);
			outstrm->start_time = instrm->start_time;
			outstrm->codecpar->codec_tag = 0;

			av_dict_copy(&outstrm->metadata, instrm->metadata, 0);

			if(outstrm->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
				outstrm->disposition |= AV_DISPOSITION_DEFAULT;
		}

		// av_dump_format(outctx, 0, "url", 1);

		// short circuiting. open + write header
		if(avio_open(&outctx->pb, outfile.string().c_str(), AVIO_FLAG_WRITE) < 0
			|| avformat_write_header(outctx, nullptr) < 0)
		{
			error("failed to open output file for writing");
			return false;
		}

		// start copying, i guess.
		int64_t maxPts = 0;
		size_t frameCount = 0;

		auto copy_frames = [&maxPts, &frameCount, &finalStreamMap](AVFormatContext* inctx, AVFormatContext* ssctx,
			AVFormatContext* outctx)
		{
			// is this even advisable??? subtitle files should be small, right??
			std::deque<AVPacket*> ss_pkts;
			if(ssctx)
			{
				util::log("fetching subtitle frames");
				while(ssctx)
				{
					AVPacket pkt;
					if(av_read_frame(ssctx, &pkt) < 0)
						break;

					auto ssstrm = ssctx->streams[pkt.stream_index];
					if(finalStreamMap.find(ssstrm) == finalStreamMap.end())
					{
						av_packet_unref(&pkt);
						continue;
					}

					ss_pkts.push_back(av_packet_clone(&pkt));
					av_packet_unref(&pkt);


					if(!config::disableProgress())
					{
						fprintf(stderr, "%s", zpr::sprint("\x1b[2K\r%s %s*%s frame %d\r", std::string(2 * util::get_log_indent(), ' '),
							COLOUR_MAGENTA_BOLD, COLOUR_RESET, ss_pkts.size()).c_str());
					}
				}

				if(!config::disableProgress())
					fprintf(stderr, "\n");
			}

			// sort the packets by pts?
			std::sort(ss_pkts.begin(), ss_pkts.end(), [](const AVPacket* a, const AVPacket* b) -> bool {
				return a->dts < b->dts;
			});

			auto copy_packet = [&frameCount, &maxPts, &finalStreamMap](AVFormatContext* outctx, AVStream* istrm, AVPacket* pkt) {

				// looks like we're re-using the same packet.
				pkt->stream_index = finalStreamMap[istrm];
				auto ostrm = outctx->streams[pkt->stream_index];

				// c++ enums are fucking stupid
				auto rounding = static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);

				maxPts = std::max(pkt->pts, maxPts);
				auto ts = (maxPts * istrm->time_base.num / istrm->time_base.den) * 1000 * 1000 * 1000;

				// fixup timings:
				pkt->pts = av_rescale_q_rnd(pkt->pts, istrm->time_base, ostrm->time_base, rounding);
				pkt->dts = av_rescale_q_rnd(pkt->dts, istrm->time_base, ostrm->time_base, rounding);
				pkt->duration = av_rescale_q(pkt->duration, istrm->time_base, ostrm->time_base);
				pkt->pos = -1;

				if(pkt->dts == AV_NOPTS_VALUE)
				{
					util::error("frame %d (s:%d) has no dts", frameCount, pkt->stream_index);
					pkt->dts = 0;
				}

				if(av_interleaved_write_frame(outctx, pkt) < 0)
					util::error("frame error");


				if(!config::disableProgress())
				{
					fprintf(stderr, "%s", zpr::sprint("\x1b[2K\r%s %s*%s time: %s\r", std::string(2 * util::get_log_indent(), ' '),
						COLOUR_MAGENTA_BOLD, COLOUR_RESET, util::uglyPrintTime(ts, /* ms: */ false)
					).c_str());
				}

				frameCount++;
			};

			int64_t prevDts = 0;
			while(true)
			{
				AVPacket* pkt = 0;
				AVStream* istrm = 0;

				if(!ss_pkts.empty())
				{
					auto f = ss_pkts.front();
					// if((f->pts >= maxPts - max_dist) && (f->pts <= maxPts + max_dist))
					if(f->dts <= prevDts)
					{
						pkt = f;
						ss_pkts.pop_front();

						istrm = ssctx->streams[pkt->stream_index];
					}
				}

				if(!pkt)
				{
					AVPacket _pkt;
					if(av_read_frame(inctx, &_pkt) < 0)
						break;

					pkt = av_packet_clone(&_pkt);
					av_packet_unref(&_pkt);

					istrm = inctx->streams[pkt->stream_index];
				}

				if(finalStreamMap.find(istrm) == finalStreamMap.end())
				{
					av_packet_unref(pkt);
					continue;
				}

				prevDts = pkt->dts;
				copy_packet(outctx, istrm, pkt);

				av_packet_free(&pkt);
			}


			// welp.
			if(ss_pkts.size() > 0)
			{
				util::warn("warn: flushing subtitle stream (%d packets remained)", ss_pkts.size());
				for(AVPacket* pkt : ss_pkts)
				{
					copy_packet(outctx, ssctx->streams[pkt->stream_index], pkt);
					av_packet_free(&pkt);
				}
			}
		};

		copy_frames(inctx, ssctx, outctx);

		if(!config::disableProgress())
			fprintf(stderr, "\n");

		// ok, write the trailer
		av_write_trailer(outctx);

		// close the output
		avio_closep(&outctx->pb);
		avformat_free_context(outctx);

		return true;
	}

	static std::vector<AVStream*> select_streams(AVFormatContext* ctx, const std::vector<AVStream*>& strms, const std::string& type)
	{
		auto print_stream_heading = [](AVFormatContext* ctx, AVStream* strm, bool idx = true) -> std::pair<std::string, std::string> {

			auto duration_to_ns = [&ctx, &strm]() -> uint64_t {

				bool ctxTime = strm->duration == INT64_MIN;

				auto x = static_cast<double>(ctxTime ? ctx->duration : strm->duration);
				double dur = 0;

				if(ctxTime) dur = x / AV_TIME_BASE;
				else        dur = (strm->time_base.num * x / strm->time_base.den);

				dur *= (1000.0 * 1000.0 * 1000.0);
				return static_cast<uint64_t>(dur);
			};

			auto cp = strm->codecpar;

			auto lang = dict_get_value(strm->metadata, "language");
			if(lang.empty()) lang = "und";

			auto codec = avcodec_get_name(cp->codec_id);
			auto title = zpr::sprint("%s", codec);
			auto subtitle = zpr::sprint("%s%s, %s", idx ? zpr::sprint("idx %d, ", strm->index) : "", lang,
				util::uglyPrintTime(duration_to_ns()));

			return { title, subtitle };
		};

		util::info("multiple %s streams:", type);
		std::vector<misc::Option> opts;
		for(auto strm : strms)
		{
			misc::Option opt;

			auto cp = strm->codecpar;

			std::tie(opt.title, opt.subTitle) = print_stream_heading(ctx, strm);

			if(auto name = dict_get_value(strm->metadata, "title"); !name.empty())
			{
				misc::Option::Info info;
				info.heading = "name:";
				info.subheading = dict_get_value(strm->metadata, "title");

				opt.infos.push_back(info);
			}

			if(cp->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				misc::Option::Info info;
				info.heading = "res:";
				info.subheading = zpr::sprint("%dx%d, %.2f fps", cp->width, cp->height,
					static_cast<double>(strm->r_frame_rate.num) / static_cast<double>(strm->r_frame_rate.den));

				opt.infos.push_back(info);
			}
			else if(cp->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				misc::Option::Info info;
				info.heading = "res:";

				char buf[64] = { 0 };
				av_get_channel_layout_string(buf, 63, cp->channels, cp->channel_layout);
				std::string layout = buf;

				std::string format = av_get_sample_fmt_name(static_cast<AVSampleFormat>(cp->format));

				info.subheading = zpr::sprint("%d Hz, %d ch (%s), %s", cp->sample_rate,
					cp->channels, layout, format);

				if(cp->bits_per_raw_sample > 0)
					info.subheading += zpr::sprint(" (%d-bit)", cp->bits_per_raw_sample);


				opt.infos.push_back(info);
			}

			if(auto br = cp->bit_rate; br > 0 && br != INT64_MIN)
			{
				misc::Option::Info info;
				info.heading = "bit:";

				auto x = get_prefix(static_cast<double>(br));
				info.subheading = zpr::sprint("%.1f %sb/s", x.first, x.second);

				opt.infos.push_back(info);
			}


			opts.push_back(opt);
		}

		return util::map(misc::userChoiceMultiple(opts), [&strms](size_t x) -> AVStream* {
			return strms[x - 1];
		});
	}










	static std::fs::path getExtraSubtitleSource(const std::string& name)
	{
		if(config::getExtraSubsPath().empty())
			return "";

		std::fs::path path = config::getExtraSubsPath();
		if(!std::fs::exists(path) || !std::fs::is_directory(path))
		{
			error("extra subs directory '%s' does not exist, or is not a directory", path.string());
			config::setExtraSubsPath("");
			return "";
		}

		// get all files:
		std::vector<std::fs::path> files;
		for(auto dirent : std::fs::directory_iterator(path))
		{
			if((dirent.is_regular_file() || dirent.is_symlink()))
			{
				auto p = dirent.path();

				if(util::match(p.extension(), ".mkv", ".ssa", ".ass", ".srt"))
					files.push_back(p);
			}
		}

		if(files.empty())
		{
			error("no compatible files found");
			return "";
		}

		auto select_matches = [&name](const std::vector<std::fs::path>& matches) -> std::fs::path {
			if(matches.empty())
			{
				error("could not match subtitles for file '%s'", name);
				return "";
			}

			if(matches.size() > 1)
			{
				util::info("multiple matches:");
				auto sel = misc::userChoice(util::map(matches, [](const std::fs::path& p) -> misc::Option {
					misc::Option opt;
					opt.title = p.filename().stem();

					return opt;
				}));

				if(sel == 0)
					return "";

				return matches[sel - 1];
			}

			return matches[0];
		};

		// first, we need to scan the input filename.

		// try tv series
		if(!config::getManualSeriesId().empty() || config::getManualMovieId().empty())
		{
			auto [ series, season, episode, title ] = tag::parseTVShow(name);
			if(series.empty())
				goto fail;

			// for this, we need season/episode info, so even if you give the series id there's no point.
			if(!series.empty())
			{
				if(int x = config::getSeasonNumber(); x != -1)
					season = x;
			}

			// check all the files.
			std::vector<std::fs::path> matches;
			for(const auto& subfile : files)
			{
				auto [ srs, ssn, ep, ttl ] = tag::parseTVShow(subfile.filename().stem());
				if(srs == series && (ssn == season || (season == -1 || ssn == -1)) && ep == episode)
					matches.push_back(subfile);
			}

			return select_matches(matches);
		}


		if(!config::getManualMovieId().empty() || config::getManualSeriesId().empty())
		{
			// try movie
			auto [ title, year ] = tag::parseMovie(name);
			if(title.empty())
				goto fail;

			std::vector<std::fs::path> matches;
			for(const auto& subfile : files)
			{
				auto [ ttl, yr ] = tag::parseMovie(subfile.filename().stem());
				if(title == ttl && year == yr)
					matches.push_back(subfile);
			}

			return select_matches(matches);
		}


	fail:
		error("could not match subtitles for file '%s'", name);
		return "";
	}










	static std::string guessLanguageFromTitle(const std::vector<std::string>& preferredLangs, std::string title)
	{
		title = util::lowercase(title);

		for(const auto& lang : preferredLangs)
		{
			auto names = misc::languageCodeMap[lang];
			for(const auto& x : names)
			{
				auto lx = util::lowercase(x);
				if(title.find(lx) != std::string::npos)
					return lang;
			}
		}

		return "";
	}


	bool muxOneFile(std::fs::path& inputfile)
	{
		// the entire state is stored in 'ctx', i think -- we just call more functions
		// to populate the fields inside.
		auto ctx = avformat_alloc_context();
		if(avformat_open_input(&ctx, inputfile.string().c_str(), nullptr, nullptr) < 0)
		{
			error("failed to open input file '%s'", inputfile.string());
			return false;
		}

		// get the stream info.
		if(avformat_find_stream_info(ctx, nullptr) < 0)
		{
			error("failed to read streams");
			return false;
		}

		std::string ss_filename;
		AVFormatContext* ssctx = 0;
		if(auto ss = getExtraSubtitleSource(inputfile.filename().stem().string()); !ss.empty())
		{
			ssctx = avformat_alloc_context();
			if(avformat_open_input(&ssctx, ss.string().c_str(), nullptr, nullptr) < 0)
			{
				error("failed to open subtitle file '%s'", ss.string());
				avformat_free_context(ssctx);
				ssctx = nullptr;
			}
			else
			{
				if(avformat_find_stream_info(ssctx, nullptr))
				{
					avformat_close_input(&ssctx);

					error("failed to read streams");
					avformat_free_context(ssctx);
					ssctx = nullptr;
				}
				else
				{
					ss_filename = ss;
				}
			}
		}










		auto preferredAudioLangs    = config::getAudioLangs();
		auto preferredSubtitleLangs = config::getSubtitleLangs();
		bool onlyOneStream          = config::isPreferOneStream();


		std::set<AVStream*> selectedStreams;

		std::vector<AVStream*> videoStrms;
		std::vector<AVStream*> audioStrms;
		std::vector<AVStream*> subtitleStrms;

		using StreamLangMap_t = std::unordered_map<std::string, std::vector<std::pair<AVStream*, std::string>>>;
		StreamLangMap_t audioStreamLangs;
		StreamLangMap_t subtitleStreamLangs;

		util::log("found %d %s", ctx->nb_streams, util::plural("stream", ctx->nb_streams));


		auto pick_streams = [&preferredAudioLangs, &preferredSubtitleLangs](AVFormatContext* ctx, std::set<AVStream*>& selectedStreams,
			std::vector<AVStream*>& videoStrms, std::vector<AVStream*>& audioStrms, std::vector<AVStream*>& subtitleStrms,
			StreamLangMap_t& audioStreamLangs, StreamLangMap_t& subtitleStreamLangs)
		{
			bool preferSDH      = config::isPreferSDHSubs();
			bool preferSigns    = config::isPreferSignSongSubs();
			bool preferTextSubs = config::isPreferTextSubs();

			for(unsigned int i = 0; i < ctx->nb_streams; i++)
			{
				auto strm = ctx->streams[i];
				if(strm->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					// if this is a picture, then add it as an attachment.
					if(util::match(avcodec_get_name(strm->codecpar->codec_id), "mjpeg", "png"))
					{
						selectedStreams.insert(strm);
					}
					else
					{
						// always select video.
						// we could end up with more than once, since there might be cover art
						// and images are encoded as video, lmao.
						videoStrms.push_back(strm);
					}
				}
				else if(strm->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
				{
					// hold off on selecting audio
					std::string lang = dict_get_value(strm->metadata, "language");
					std::string name = dict_get_value(strm->metadata, "title");

					// you motherfucker, releases files properly!!!
					if(lang.empty()) lang = guessLanguageFromTitle(preferredAudioLangs, name);

					audioStreamLangs[lang].push_back({ strm, util::lowercase(name) });
				}
				else if(strm->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
				{
					// hold off on selecting subs
					std::string lang = dict_get_value(strm->metadata, "language");
					std::string name = dict_get_value(strm->metadata, "title");

					// you motherfucker, releases files properly!!!
					if(lang.empty()) lang = guessLanguageFromTitle(preferredSubtitleLangs, name);

					subtitleStreamLangs[lang].push_back({ strm, util::lowercase(name) });
				}
				else
				{
					// select everything else (attachments, fonts, cover art, etc.)
					selectedStreams.insert(strm);
				}
			}

			// select audio streams
			for(const auto& lang : preferredAudioLangs)
			{
				if(auto it = audioStreamLangs.find(lang); it != audioStreamLangs.end())
				{
					bool found = false;

					auto strms = it->second;
					for(const auto& [ strm, name ] : strms)
					{
						// if there's commentary, don't select it.
						if(name.find("commentary") != std::string::npos || name.find("director") != std::string::npos)
							continue;

						// idk what else to filter on, so just include it.
						audioStrms.push_back(strm);
						found = true;
					}

					// note: we put this here, so we select all audio of a given language,
					// but not more than one language.
					if(found)
						break;
				}
			}


			// select subtitle streams
			for(const auto& lang : preferredSubtitleLangs)
			{
				if(auto it = subtitleStreamLangs.find(lang); it != subtitleStreamLangs.end())
				{
					bool found = false;

					auto strms = it->second;
					for(const auto& [ strm, name ] : strms)
					{
						// if there's commentary, don't select it.
						if(name.find("commentary") != std::string::npos || name.find("director") != std::string::npos)
							continue;

						// if you don't prefer SDH, don't use SDH.
						if(name.find("sdh") != std::string::npos && !preferSDH)
							continue;

						// if you don't prefer sign/song subs, then don't. (this assumes nobody puts
						// the words "signs" or "songs" in the name...)
						else if((name.find("signs") != std::string::npos || name.find("songs") != std::string::npos) && !preferSigns)
							continue;

						// if you prefer text subs, make sure it's ass or srt.
						else if(preferTextSubs && !util::match(strm->codecpar->codec_id,
							AV_CODEC_ID_SSA, AV_CODEC_ID_ASS, AV_CODEC_ID_MOV_TEXT, AV_CODEC_ID_SRT, AV_CODEC_ID_SUBRIP))
							continue;

						// ok then, it *should* be ok?
						subtitleStrms.push_back(strm);
						found = true;
					}

					if(found)
						break;
				}
			}
		};

		{
			// pick streams from the primary source.
			pick_streams(ctx, selectedStreams, videoStrms, audioStrms, subtitleStrms, audioStreamLangs, subtitleStreamLangs);


			if(ssctx)
			{
				util::log("using '%s' for subtitles", ss_filename);
				if(!subtitleStrms.empty() || !subtitleStreamLangs.empty())
				{
					util::warn("ignoring all subtitle streams from input file");

					subtitleStrms.clear();
					subtitleStreamLangs.clear();
				}

				// pick streams from the secondary source.
				std::vector<AVStream*> ss_videoStrms;
				std::vector<AVStream*> ss_audioStrms;
				StreamLangMap_t ss_audioStreamLangs;

				// we use fresh copies of lists for audio and video, since we will just discard them.
				// use the main list for subtitles & attachments.
				pick_streams(ssctx, selectedStreams, ss_videoStrms, ss_audioStrms, subtitleStrms, ss_audioStreamLangs,
					subtitleStreamLangs);
			}
		}





		{
			if(videoStrms.empty())
				error("error: no video streams!");

			if(audioStrms.empty())
			{
				// if nothing matched, then give options.
				if(audioStreamLangs.empty())
					error("error: no audio streams!");

				// add them all.
				for(const auto& x : audioStreamLangs)
					for(const auto& y : x.second)
						audioStrms.push_back(y.first);

				if(audioStrms.size() == 1)
					util::warn("warn: selected default audio stream");

				else
					util::warn("warn: ambiguous audio streams (none matching language)");
			}

			if(subtitleStrms.empty())
			{
				// if nothing matched, then give options.
				if(subtitleStreamLangs.empty())
					error("error: no subtitle streams!");

				// add them all.
				for(const auto& x : subtitleStreamLangs)
					for(const auto& y : x.second)
						subtitleStrms.push_back(y.first);

				if(subtitleStrms.size() == 1)
					util::warn("warn: selected default subtitle stream");

				else
					util::warn("warn: ambiguous subtitle streams (none matching language)");
			}



			if(videoStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple video streams found");

				else
					videoStrms = select_streams(ctx, videoStrms, "video");
			}

			if(audioStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple audio streams found");

				else
					audioStrms = select_streams(ctx, audioStrms, "audio");
			}

			if(subtitleStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple subtitle streams found");

				else
					subtitleStrms = select_streams(ctx, subtitleStrms, "subtitle");
			}
		}

		// the final order.
		std::vector<AVStream*> finalStreams;
		std::unordered_map<AVStream*, size_t> finalStreamMap;

		// video, audio, subtitle, attachments.
		finalStreams.insert(finalStreams.end(), videoStrms.begin(), videoStrms.end());
		finalStreams.insert(finalStreams.end(), audioStrms.begin(), audioStrms.end());
		finalStreams.insert(finalStreams.end(), subtitleStrms.begin(), subtitleStrms.end());
		finalStreams.insert(finalStreams.end(), selectedStreams.begin(), selectedStreams.end());

		for(size_t i = 0; i < finalStreams.size(); i++)
			finalStreamMap[finalStreams[i]] = i;

		util::log("selected %d streams: %s", finalStreams.size(), util::listToString(finalStreams, [](auto x) -> auto {
			return std::to_string(x->index);
		}));

		util::indent_log();
		for(auto strm : finalStreams)
		{
			auto lang = dict_get_value(strm->metadata, "language");
			auto name = dict_get_value(strm->metadata, "title");

			auto filename = dict_get_value(strm->metadata, "filename");

			std::string type;
			if(strm->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)        type = "vid";
			if(strm->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)        type = "aud";
			if(strm->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)     type = "sub";
			if(strm->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT)   type = "att";

			if(!type.empty())
				type = zpr::sprint("%s%s:%s ", COLOUR_GREY_BOLD, type, COLOUR_RESET);

			util::info("%2d%s: %s%s%s%s%s", strm->index, lang.empty() ? "" : zpr::sprint(" (%s)", lang),
				type, COLOUR_BLACK_BOLD, avcodec_get_name(strm->codecpar->codec_id), COLOUR_RESET,
				strm->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT
					? filename.empty() ? "" : zpr::sprint(" - %s", filename)
					: name.empty() ? "" : zpr::sprint(" - %s", name));
		}
		util::unindent_log();

		// make the output file
		assert(!config::getOutputFolder().empty());
		auto outfile = std::fs::canonical(std::fs::path(config::getOutputFolder())) / inputfile.filename();

		util::log("output: '%s'", outfile.string());



		// make the output file:
		if(!config::isDryRun())
		{
			if(!writeOutput(outfile, ctx, ssctx, finalStreams, finalStreamMap))
				return false;
		}




		avformat_close_input(&ctx);
		avformat_free_context(ctx);

		if(ssctx)
		{
			avformat_close_input(&ssctx);
			avformat_free_context(ssctx);
		}

		return true;
	}
}
















