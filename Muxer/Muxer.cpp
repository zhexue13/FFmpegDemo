// Muxer.cpp: 定义控制台应用程序的入口点。
//

#include <iostream>
#include <string>
#include <cstdio>
extern "C"{
#include "libavformat/avformat.h"
};

using namespace std;

#define USE_H264BSF 1
#define USE_AACBSF 1

int main() {

	AVOutputFormat *ofmt = nullptr;
	AVFormatContext *ifmtCtxVideo = nullptr;
	AVFormatContext *ifmtCtxAudio = nullptr;
	AVFormatContext *ofmtCtx = nullptr;
	AVPacket pkt;

	AVBitStreamFilterContext *h264bsfc;
	AVBitStreamFilterContext *aacbsfc;

	int vindex = -1;
	int aindex = -1;
	int ovindex = -1;
	int oaindex = -1;

	int frameIndex = 0;

	int64_t curVideoPts = 0;
	int64_t curAudioPts = 0;

	const char *inVideoFileName = "../files/h264/yangzi.h264";
	const char *inAudioFileName = "../files/aac/yangzi.aac";
	const char *outFileName = "../files/outfiles/yangzi.mp4";

	remove(outFileName);

	av_register_all();

	if (avformat_open_input(&ifmtCtxVideo, inVideoFileName, 0, 0) < 0) {
		std::cout << "Could not open input file." << std::endl;
		goto end;
	}

	if (avformat_find_stream_info(ifmtCtxVideo, 0) < 0) {
		std::cout << "Failed to retrieve input stream information." << std::endl;
		goto end;
	}

	if (avformat_open_input(&ifmtCtxAudio, inAudioFileName, 0, 0) < 0) {
		std::cout << "Could not open input file." << std::endl;
		goto end;
	}

	if (avformat_find_stream_info(ifmtCtxAudio, 0) < 0) {
		std::cout << "Failed to retrieve input stream information." << std::endl;
		goto end;
	}

	std::cout << "===========Input Information==========" << std::endl;
	av_dump_format(ifmtCtxVideo, 0, inVideoFileName, 0);
	av_dump_format(ifmtCtxAudio, 0, inAudioFileName, 0);
	std::cout << "======================================" << std::endl;
	
	avformat_alloc_output_context2(&ofmtCtx, NULL, NULL, outFileName);
	if (!ofmtCtx) {
		std::cout << "Could not create output context." << std::endl;
		goto end;
	}
	ofmt = ofmtCtx->oformat;

	for (unsigned int i = 0; i < ifmtCtxVideo->nb_streams; i++) {
		if (ifmtCtxVideo->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			AVStream *inStream = ifmtCtxVideo->streams[i];
			AVStream *outStream = avformat_new_stream(ofmtCtx, inStream->codec->codec);
			vindex = i;
			if (!outStream) {
				std::cout << "Failed allocating output stream." << std::endl;
				goto end;
			}
			ovindex = outStream->index;
			if (avcodec_copy_context(outStream->codec, inStream->codec) < 0) {
				std::cout << "Failed to copy context from input to output stream codec context." << std::endl;
				goto end;
			}
			outStream->codec->codec_tag = 0;
			if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
				outStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}	
	}

	for (unsigned int i = 0; i < ifmtCtxAudio->nb_streams; i++) {
		if (ifmtCtxAudio->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			AVStream *inStream = ifmtCtxAudio->streams[i];
			AVStream *outStream = avformat_new_stream(ofmtCtx, inStream->codec->codec);
			aindex = i;
			if (!outStream) {
				std::cout << "Failed allocating output stream." << std::endl;
				goto end;
			}
			oaindex = outStream->index;
			if (avcodec_copy_context(outStream->codec, inStream->codec) < 0) {
				std::cout << "Failed to copy context from input to output stream codec context." << std::endl;
				goto end;
			}
			outStream->codec->codec_tag = 0;
			if (ofmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
				outStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			break;
		}
	}

	std::cout << "==========Output Information==========" << std::endl;
	av_dump_format(ofmtCtx, 0, outFileName, 1);
	std::cout << "======================================" << std::endl;

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmtCtx->pb, outFileName, AVIO_FLAG_WRITE) < 0) {
			std::cout << "Could not open output file." << outFileName << std::endl;
			goto end;
		}
	}

	if (avformat_write_header(ofmtCtx, NULL) < 0) {
		std::cout << "Error occurred when opening output file." << std::endl;
		goto end;
	}


#if USE_H264BSF
	h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif
#if USE_AACBSF
	aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif

	while (1) {
		AVFormatContext *ifmtCtx;
		int streamIndex = 0;
		AVStream *inStream;
		AVStream *outStream;
		//Get an AVPacket
		if (av_compare_ts(curVideoPts, ifmtCtxVideo->streams[vindex]->time_base, curAudioPts, ifmtCtxAudio->streams[aindex]->time_base) <= 0) {
			ifmtCtx = ifmtCtxVideo;
			streamIndex = ovindex;

			if (av_read_frame(ifmtCtx, &pkt) >= 0) {
				do {
					inStream = ifmtCtx->streams[pkt.stream_index];
					outStream = ofmtCtx->streams[streamIndex];

					if (pkt.stream_index == vindex) {
						//FIX：No PTS (Example: Raw H.264)
						//Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE) {
							//Write PTS
							AVRational timeBase1 = inStream->time_base;
							//Duration between 2 frames (us)
							int64_t calcDuration = static_cast<int64_t>((double)AV_TIME_BASE / av_q2d(inStream->r_frame_rate));
							//Parameters
							pkt.pts = static_cast<int64_t>((double)(frameIndex*calcDuration) / (double)(av_q2d(timeBase1)*AV_TIME_BASE));
							pkt.dts = pkt.pts;
							pkt.duration = static_cast<int>((double)calcDuration / (double)(av_q2d(timeBase1)*AV_TIME_BASE));
							frameIndex++;
						}

						curVideoPts = pkt.pts;
						break;
					}
				} while (av_read_frame(ifmtCtx, &pkt) >= 0);
			}
			else {
				break;
			}
		}
		else {
			ifmtCtx = ifmtCtxAudio;
			streamIndex = oaindex;
			if (av_read_frame(ifmtCtx, &pkt) >= 0) {
				do {
					inStream = ifmtCtx->streams[pkt.stream_index];
					outStream = ofmtCtx->streams[streamIndex];

					if (pkt.stream_index == aindex) {

						//FIX：No PTS
						//Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE) {
							//Write PTS
							AVRational timeBase1 = inStream->time_base;
							//Duration between 2 frames (us)
							int64_t calcDuration = static_cast<int64_t>((double)AV_TIME_BASE / av_q2d(inStream->r_frame_rate));
							//Parameters
							pkt.pts = static_cast<int64_t>((double)(frameIndex*calcDuration) / (double)(av_q2d(timeBase1)*AV_TIME_BASE));
							pkt.dts = pkt.pts;
							pkt.duration = static_cast<int>((double)calcDuration / (double)(av_q2d(timeBase1)*AV_TIME_BASE));
							frameIndex++;
						}
						curAudioPts = pkt.pts;

						break;
					}
				} while (av_read_frame(ifmtCtx, &pkt) >= 0);
			}
			else {
				break;
			}
		}

#if USE_H264BSF
		av_bitstream_filter_filter(h264bsfc, inStream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
		av_bitstream_filter_filter(aacbsfc, outStream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = static_cast<int>(av_rescale_q(pkt.duration, inStream->time_base, inStream->time_base));
		pkt.pos = -1;
		pkt.stream_index = streamIndex;

		printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmtCtx, &pkt) < 0) {
			std::cout << "Error muxing packet." << std::endl;
			break;
		}
		av_free_packet(&pkt);

	}
	//Write file trailer
	av_write_trailer(ofmtCtx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

end:
	avformat_close_input(&ifmtCtxVideo);
	avformat_close_input(&ifmtCtxAudio);
	/* close output */
	if (ofmtCtx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmtCtx->pb);
	avformat_free_context(ofmtCtx);

    return 0;
}

