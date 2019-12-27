// driver.cpp
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include <set>

// what the fuck? i shouldn't have to do this manually...
extern "C" {
	#include <libavutil/log.h>
	#include <libavutil/timestamp.h>
	#include <libavformat/avformat.h>
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

	static std::string lowercase(std::string xs)
	{
		for(size_t i = 0; i < xs.size(); i++)
			xs[i] = tolower(xs[i]);

		return xs;
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
	static bool writeOutput(const std::fs::path& outfile, AVFormatContext* ctx, const std::vector<size_t>& finalStreams,
		std::unordered_map<size_t, size_t>& finalStreamMap)
	{
		AVFormatContext* outctx = 0;
		if(avformat_alloc_output_context2(&outctx, nullptr, "matroska", outfile.string().c_str()) < 0)
		{
			error("failed to allocate output context");
			return false;
		}

		av_dict_copy(&outctx->metadata, ctx->metadata, 0);

		// make new streams for the output
		for(auto idx : finalStreams)
		{
			auto outstrm = avformat_new_stream(outctx, nullptr);
			if(!outstrm)
			{
				error("failed to allocate output stream");
				return false;
			}

			avcodec_parameters_copy(outstrm->codecpar, ctx->streams[idx]->codecpar);
			outstrm->start_time = ctx->streams[idx]->start_time;
			outstrm->codecpar->codec_tag = 0;

			av_dict_copy(&outstrm->metadata, ctx->streams[idx]->metadata, 0);

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
		while(true)
		{
			AVPacket pkt;
			if(av_read_frame(ctx, &pkt) < 0)
				break;

			auto istrm = ctx->streams[pkt.stream_index];
			if(finalStreamMap.find(istrm->index) == finalStreamMap.end())
			{
				av_packet_unref(&pkt);
				continue;
			}

			// looks like we're re-using the same packet.
			pkt.stream_index = finalStreamMap[istrm->index];
			auto ostrm = outctx->streams[pkt.stream_index];

			// c++ enums are fucking stupid
			auto rounding = static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);

			maxPts = std::max(pkt.pts, maxPts);
			auto ts = (maxPts * istrm->time_base.num / istrm->time_base.den) * 1000 * 1000 * 1000;

			// fixup timings:
			pkt.pts = av_rescale_q_rnd(pkt.pts, istrm->time_base, ostrm->time_base, rounding);
			pkt.dts = av_rescale_q_rnd(pkt.dts, istrm->time_base, ostrm->time_base, rounding);
			pkt.duration = av_rescale_q(pkt.duration, istrm->time_base, ostrm->time_base);
			pkt.pos = -1;

			if(pkt.dts == AV_NOPTS_VALUE)
			{
				util::error("frame %d (s:%d) has no dts", frameCount, pkt.stream_index);
				pkt.dts = 0;
			}

			if(av_interleaved_write_frame(outctx, &pkt) < 0)
				util::error("frame error");


			if(!config::disableProgress())
			{
				fprintf(stderr, "%s", zpr::sprint("\x1b[2K\r%s %s*%s time: %s\r", std::string(2 * util::get_log_indent(), ' '),
					COLOUR_MAGENTA_BOLD, COLOUR_RESET, util::uglyPrintTime(ts, /* ms: */ false)
				).c_str());
			}


			frameCount++;
			av_packet_unref(&pkt);
		}

		if(!config::disableProgress())
			fprintf(stderr, "\n");

		// ok, write the trailer
		av_write_trailer(outctx);

		// close the output
		avio_closep(&outctx->pb);
		avformat_free_context(outctx);

		return true;
	}















	bool muxOneFile(std::fs::path& inputfile)
	{
		// the entire state is stored in 'ctx', i think -- we just call more functions
		// to populate the fields inside.
		auto ctx = avformat_alloc_context();
		if(avformat_open_input(&ctx, inputfile.string().c_str(), nullptr, nullptr) != 0)
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

		auto preferredAudioLangs = config::getAudioLangs();
		auto preferredSubtitleLangs = config::getSubtitleLangs();

		bool preferSDH      = config::isPreferSDHSubs();
		bool preferSigns    = config::isPreferSignSongSubs();
		bool preferTextSubs = config::isPreferTextSubs();
		bool onlyOneStream  = config::isPreferOneStream();


		std::set<int> selectedStreams;

		std::vector<size_t> videoStrms;
		std::vector<size_t> audioStrms;
		std::vector<size_t> subtitleStrms;

		std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> audioStreamLangs;
		std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> subtitleStreamLangs;

		{
			util::log("found %d %s", ctx->nb_streams, util::plural("stream", ctx->nb_streams));
			for(auto i = 0; i < ctx->nb_streams; i++)
			{
				auto strm = ctx->streams[i];
				if(strm->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					// if this is a picture, then add it as an attachment.
					if(util::match(avcodec_get_name(strm->codecpar->codec_id), "mjpeg", "png"))
					{
						selectedStreams.insert(strm->index);
					}
					else
					{
						// always select video.
						// we could end up with more than once, since there might be cover art
						// and images are encoded as video, lmao.
						videoStrms.push_back(strm->index);
					}
				}
				else if(strm->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
				{
					// hold off on selecting audio
					std::string lang = dict_get_value(strm->metadata, "language");
					std::string name = dict_get_value(strm->metadata, "title");

					audioStreamLangs[lang].push_back({ strm->index, lowercase(name) });
				}
				else if(strm->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE)
				{
					// hold off on selecting subs
					std::string lang = dict_get_value(strm->metadata, "language");
					std::string name = dict_get_value(strm->metadata, "title");

					subtitleStreamLangs[lang].push_back({ strm->index, lowercase(name) });
				}
				else
				{
					// select everything else (attachments, fonts, cover art, etc.)
					selectedStreams.insert(strm->index);
				}
			}

			// select audio streams
			for(const auto& lang : preferredAudioLangs)
			{
				if(auto it = audioStreamLangs.find(lang); it != audioStreamLangs.end())
				{
					bool found = false;

					auto strms = it->second;
					for(const auto& [ idx, name ] : strms)
					{
						// if there's commentary, don't select it.
						if(name.find("commentary") != -1 || name.find("director") != -1)
							continue;

						// idk what else to filter on, so just include it.
						audioStrms.push_back(idx);
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
					for(const auto& [ idx, name ] : strms)
					{
						// if there's commentary, don't select it.
						if(name.find("commentary") != -1 || name.find("director") != -1)
							continue;

						// if you don't prefer SDH, don't use SDH.
						if(name.find("sdh") != -1 && !preferSDH)
							continue;

						// if you don't prefer sign/song subs, then don't. (this assumes nobody puts
						// the words "signs" or "songs" in the name...)
						else if((name.find("signs") != -1 || name.find("songs") != -1) && !preferSigns)
							continue;

						// if you prefer text subs, make sure it's ass or srt.
						else if(preferTextSubs && !util::match(ctx->streams[idx]->codecpar->codec_id,
							AV_CODEC_ID_SSA, AV_CODEC_ID_ASS, AV_CODEC_ID_MOV_TEXT, AV_CODEC_ID_SRT, AV_CODEC_ID_SUBRIP))
							continue;

						// ok then, it *should* be ok?
						subtitleStrms.push_back(idx);
						found = true;
					}

					if(found)
						break;
				}
			}
		}

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
			if(lang.empty()) lang = "unk";

			auto codec = avcodec_get_name(cp->codec_id);
			auto title = zpr::sprint("%s", codec);
			auto subtitle = zpr::sprint("%s%s, %s", idx ? zpr::sprint("idx %d, ", strm->index) : "", lang,
				util::uglyPrintTime(duration_to_ns()));

			return { title, subtitle };
		};

		auto select_streams = [&ctx, &print_stream_heading](const std::vector<size_t>& strms,
			const std::string& type) -> std::vector<size_t>
		{
			util::info("multiple %s streams:", type);
			std::vector<misc::Option> opts;
			for(auto idx : strms)
			{
				misc::Option opt;

				auto strm = ctx->streams[idx];
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

			return util::map(misc::userChoiceMultiple(opts), [&strms](size_t x) -> size_t {
				return strms[x - 1];
			});
		};

		{
			if(videoStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple video streams found");

				else
					videoStrms = select_streams(videoStrms, "video");
			}

			if(audioStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple audio streams found");

				else
					audioStrms = select_streams(audioStrms, "audio");
			}

			if(subtitleStrms.size() > 1)
			{
				if(!onlyOneStream)
					util::warn("warn: multiple subtitle streams found");

				else
					subtitleStrms = select_streams(subtitleStrms, "subtitle");
			}
		}

		// the final order.
		std::vector<size_t> finalStreams;
		std::unordered_map<size_t, size_t> finalStreamMap;

		// video, audio, subtitle, attachments.
		for(auto s : videoStrms)
		{
			finalStreamMap[s] = finalStreams.size();
			finalStreams.push_back(s);
		}
		for(auto s : audioStrms)
		{
			finalStreamMap[s] = finalStreams.size();
			finalStreams.push_back(s);
		}
		for(auto s : subtitleStrms)
		{
			finalStreamMap[s] = finalStreams.size();
			finalStreams.push_back(s);
		}
		for(auto s : selectedStreams)
		{
			finalStreamMap[s] = finalStreams.size();
			finalStreams.push_back(s);
		}


		util::log("selected %d streams: %s", finalStreams.size(), util::listToString(finalStreams, util::tostring()));

		// make the output file
		assert(!config::getOutputFolder().empty());
		auto outfile = std::fs::canonical(std::fs::path(config::getOutputFolder())) / inputfile.filename();

		util::log("output: '%s'", outfile.string());



		// make the output file:
		if(!config::isDryRun())
		{
			if(!writeOutput(outfile, ctx, finalStreams, finalStreamMap))
				return false;
		}




		avformat_close_input(&ctx);
		return true;
	}
}
















