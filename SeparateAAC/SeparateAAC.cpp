// SeparateAAC.cpp: 定义控制台应用程序的入口点。
//

#include <cstdio>
#include <iostream>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
};

using namespace std;


int main() {
	AVOutputFormat *ofmtAudio = nullptr;

	AVFormatContext *ifmtCtx = nullptr;
	AVFormatContext *ofmtCtxAudio = nullptr;

	AVPacket pkt;

	int vindex = -1;
	int aindex = -1;
	int indexFrame = 0;

	const char *inFileName = "../files/mp4/yangzi.mp4";
	const char *outAudioFileName = "../files/aac/yangzi.aac";

	remove(outAudioFileName);

	av_register_all();

	if (avformat_open_input(&ifmtCtx, inFileName, 0, 0) < 0) {
		std::cout << "Could not open input file." << std::endl;
		goto end;
	}

	if (avformat_find_stream_info(ifmtCtx, 0) < 0) {
		std::cout << "Failed to retrieve input stream information." << std::endl;
		goto end;
	}

	avformat_alloc_output_context2(&ofmtCtxAudio, NULL, NULL, outAudioFileName);
	if (!ofmtCtxAudio) {
		std::cout << "Could not create context." << std::endl;
		goto end;
	}
	ofmtAudio = ofmtCtxAudio->oformat;


	for (unsigned int i = 0; i < ifmtCtx->nb_streams; i++) {
		AVFormatContext *ofmtCtx;
		AVStream *inStream = ifmtCtx->streams[i];
		AVStream *outStream = nullptr;

		if (ifmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			continue;
		}else if (ifmtCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			aindex = i;
			outStream = avformat_new_stream(ofmtCtxAudio, inStream->codec->codec);
			ofmtCtx = ofmtCtxAudio;
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
	std::cout << "=============Output Audio=============" << std::endl;
	av_dump_format(ofmtCtxAudio, 0, outAudioFileName, 1);

	if (!(ofmtAudio->flags&AVFMT_NOFILE))
		if (avio_open(&ofmtCtxAudio->pb, outAudioFileName, AVIO_FLAG_WRITE) < 0) {
			std::cout << "Could not open output file " << outAudioFileName << std::endl;
			goto end;
		}

	if (avformat_write_header(ofmtCtxAudio, NULL) < 0) {
		std::cout << "Error occurred when opening audio output file." << std::endl;
		goto end;
	}

	while (1) {
		AVFormatContext *ofmtCtx;
		AVStream *inStream;
		AVStream *outStream;

		if (av_read_frame(ifmtCtx, &pkt) < 0)
			break;

		inStream = ifmtCtx->streams[pkt.stream_index];

		if (pkt.stream_index == aindex) {
			outStream = ofmtCtxAudio->streams[0];
			ofmtCtx = ofmtCtxAudio;
			printf("Write Audio Packet. size:%d\tpts:%lld\n", pkt.size, pkt.pts);
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

	av_write_trailer(ofmtCtxAudio);

end:

	avformat_close_input(&ifmtCtx);

	if (ofmtCtxAudio && !(ofmtAudio->flags & AVFMT_NOFILE))
		avio_close(ofmtCtxAudio->pb);

	avformat_free_context(ofmtCtxAudio);

	return 0;
}

