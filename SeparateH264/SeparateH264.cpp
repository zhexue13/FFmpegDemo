// SeparateH264.cpp: 定义控制台应用程序的入口点。
//

#include <cstdio>
#include <iostream>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
};

#define USE_H264BSF 1

using namespace std;


int main() {
	AVOutputFormat *ofmtVideo = nullptr;

	AVFormatContext *ifmtCtx = nullptr;
	AVFormatContext *ofmtCtxVideo = nullptr;

#if USE_H264BSF
	AVBitStreamFilterContext *h264bsfCtx;
#endif

	AVPacket pkt;

	int vindex = -1;
	int aindex = -1;
	int indexFrame = 0;

	const char *inFileName = "../files/mp4/yangzi.mp4";
	const char *outVideoFileName = "../files/h264/yangzi.h264";

	remove(outVideoFileName);

	av_register_all();

	if (avformat_open_input(&ifmtCtx, inFileName, 0, 0) < 0) {
		std::cout << "Could not open input file." << std::endl;
		goto end;
	}

	if (avformat_find_stream_info(ifmtCtx, 0) < 0) {
		std::cout << "Failed to retrieve input stream information." << std::endl;
		goto end;
	}

	avformat_alloc_output_context2(&ofmtCtxVideo, NULL, NULL, outVideoFileName);
	if (!ofmtCtxVideo) {
		std::cout << "Could not create context." << std::endl;
		goto end;
	}
	ofmtVideo = ofmtCtxVideo->oformat;

	for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) {
		AVFormatContext *ofmtCtx;
		AVStream *inStream = ifmtCtx->streams[i];
		AVStream *outStream = nullptr;

		if (ifmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			vindex = i;
			outStream = avformat_new_stream(ofmtCtxVideo, inStream->codec->codec);
			ofmtCtx = ofmtCtxVideo;
		}
		else {
			break;
		}

		if (!outStream) {
			std::cout << "Failed allocating output stream." << std::endl;
			goto end;
		}

		if (avcodec_copy_context(outStream->codec, inStream->codec) < 0) {
			std::cout << "Failed to copy context from input to output stream codec context." << std::endl;
			goto end;
		}

		outStream->codec->codec_tag = 0;

		if (ofmtCtx->oformat->flags&AVFMT_GLOBALHEADER)
			outStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	std::cout << "=============Input Video=============" << std::endl;
	av_dump_format(ifmtCtx, 0, inFileName, 0);
	std::cout << "=============Output Video=============" << std::endl;
	av_dump_format(ofmtCtxVideo, 0, outVideoFileName, 1);

	if (!(ofmtVideo->flags&AVFMT_NOFILE))
		if (avio_open(&ofmtCtxVideo->pb, outVideoFileName, AVIO_FLAG_WRITE) < 0) {
			std::cout << "Could not open output file " << outVideoFileName << std::endl;
			goto end;
		}

	if (avformat_write_header(ofmtCtxVideo, NULL) < 0) {
		std::cout << "Error occurred when opening video output file." << std::endl;
		goto end;
	}

#if USE_H264BSF
	h264bsfCtx = av_bitstream_filter_init("h264_mp4toannexb");
#endif

	while (1) {
		AVFormatContext *ofmtCtx;
		AVStream *inStream;
		AVStream *outStream;

		if (av_read_frame(ifmtCtx, &pkt) < 0)
			break;

		inStream = ifmtCtx->streams[pkt.stream_index];

		if (pkt.stream_index == vindex) {
			outStream = ofmtCtxVideo->streams[0];
			ofmtCtx = ofmtCtxVideo;
			printf("Write Video Packet. size:%d\tpts:%lld\n", pkt.size, pkt.pts);
#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfCtx, inStream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
		}
		else {
			continue;
		}

		pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = static_cast<int>(av_rescale_q(pkt.duration, inStream->time_base, outStream->time_base));
		pkt.pos = -1;
		pkt.stream_index = 0;

		if (av_interleaved_write_frame(ofmtCtx, &pkt) < 0) {
			std::cout << "Error muxing packet" << std::endl;
			break;
		}

		av_free_packet(&pkt);
		indexFrame++;
	}

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfCtx);
#endif

	av_write_trailer(ofmtCtxVideo);

end:

	avformat_close_input(&ifmtCtx);

	if (ofmtCtxVideo && !(ofmtVideo->flags & AVFMT_NOFILE))
		avio_close(ofmtCtxVideo->pb);

	avformat_free_context(ofmtCtxVideo);

	return 0;
}