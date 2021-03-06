// VideoPlayer.cpp: 定义控制台应用程序的入口点。
//

#include <iostream>
#include <cstdio>
#include <string>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "sdl2/SDL.h"
}

using namespace std;

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit = 0;
int thread_pause = 0;

int sfp_refresh_thread(void *opaque) {
	thread_exit = 0;
	thread_pause = 0;

	while (!thread_exit) {
		if (!thread_pause) {
			SDL_Event event;
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(40);
	}
	thread_exit = 0;
	thread_pause = 0;
	//Break
	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


int main(int argc, char* argv[]){

	AVFormatContext	*pFormatCtx;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame, *pFrameYUV;
	unsigned char *out_buffer;
	AVPacket *packet;
	int ret, got_picture;

	//------------SDL----------------
	int screen_w, screen_h;
	SDL_Window *screen;
	SDL_Renderer *sdlRenderer;
	SDL_Texture *sdlTexture;
	SDL_Rect sdlRect;
	SDL_Thread *video_tid;
	SDL_Event event;

	struct SwsContext *img_convert_ctx;

	const char *filepath = "../files/h264/yangzi.h264";

	av_register_all();
	avformat_network_init();

	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) < 0) {
		std::cout << "Couldn't not open input stream." << std::endl;
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		std::cout << "Couldn't find stream information." << std::endl;
		return -1;
	}

    int videoindex = -1;
	//找到视频流
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}
	}
		
	if (videoindex == -1) {
		std::cout << "Didn't find a video stream." << std::endl;
		return -1;
	}

	//打开编译码器
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		std::cout << "Codec not found." << std::endl;
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		std::cout << "Could not open codec." << std::endl;
		return -1;
	}

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();

	out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, 
		pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	//Output Info-----------------------------
	std::cout << "============= File Information =============" << std::endl;
	av_dump_format(pFormatCtx, 0, filepath, 0);
	std::cout << "============================================" << std::endl;

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	//初始化SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		std::cout << "Could not initialize SDL - "<< SDL_GetError() << std::endl;
		return -1;
	}
	//创建窗口
	//SDL 2.0 Support for multiple windows
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	screen = SDL_CreateWindow("VideoPlayer Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_OPENGL);

	if (!screen) {
		std::cout << "SDL: could not create window - exiting:" << SDL_GetError() << std::endl;
		return -1;
	}

	//创建渲染器
	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//创建纹理
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	
	//创建线程
	video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, NULL);
	//------------SDL End------------
	//Event Loop

	for (;;) {
		//Wait
		SDL_WaitEvent(&event);
		if (event.type == SFM_REFRESH_EVENT) {
			while (1) {
				if (av_read_frame(pFormatCtx, packet)<0)
					thread_exit = 1;

				if (packet->stream_index == videoindex)
					break;
			}
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0) {
				std::cout << "Decode Error." << std::endl;
				return -1;
			}
			if (got_picture) {
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
				//SDL---------------------------
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
				SDL_RenderClear(sdlRenderer);
				//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
				SDL_RenderPresent(sdlRenderer);
				//SDL End-----------------------
			}
			av_free_packet(packet);
		}
		else if (event.type == SDL_KEYDOWN) {
			//Pause
			if (event.key.keysym.sym == SDLK_SPACE)
				thread_pause = !thread_pause;
		}
		else if (event.type == SDL_QUIT) {
			thread_exit = 1;
		}
		else if (event.type == SFM_BREAK_EVENT) {
			break;
		}

	}

	sws_freeContext(img_convert_ctx);

	SDL_Quit();
	//--------------
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}


