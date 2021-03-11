//
//  byronEasyPlayer1.c
//  testSDL2
//
//  Created by chengkang on 9/16/20.
//  Copyright © 2020 MIGU. All rights reserved.
//
#include "byronEasyPlayer1.h"

#include <SDL2/SDL.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#define logAndGFail(e) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,(e));\
goto __FAIL;

typedef void(*failCBack)(void);

int byronEasyPlayer(int argc ,char *argv[]){
    int ret = -1;
    int i,videoStream,frameFinished,w_width=640,w_height = 480;
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtxOrg = NULL;
    AVCodecContext *pCodecCtx = NULL;
    
    struct SwsContext *swsCtx = NULL;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVPacket packet;
    AVPicture *pict = NULL;
    SDL_Rect rect;
    Uint32 pixFormat;
    
    SDL_Window *win = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    
    if(argc < 2){
        logAndGFail("usage : command <file>");
    }
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
        logAndGFail("初始化sdl失败");
    }
    
//    av_register_all();
    argv[1] = "tsubasa.MP4";
    if(0!= avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)){
       logAndGFail("打开文件失败");
    }
    
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
       logAndGFail("找不到多媒体流");
    }
    //打印多媒体文件信息
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    videoStream = -1;
    for(i=0;i<pFormatCtx->nb_streams;i++){
        if(AVMEDIA_TYPE_VIDEO == pFormatCtx->streams[i]->codec->codec_type){
            videoStream = i;
            break;
        }
    }
    
    if(-1 == videoStream){
       logAndGFail("多媒体无视频流");
    }
    
    //编码器上下文
    pCodecCtxOrg = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtxOrg->codec_id);
    if(NULL == pCodec){
       logAndGFail("该视频编码格式不被支持");
    }
    
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(0 != avcodec_copy_context(pCodecCtx, pCodecCtxOrg)){
        logAndGFail("编码器无法被拷贝");
    }
    
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0){
        logAndGFail("打开解码器失败");
    }
    
    pFrame = av_frame_alloc();
    w_width = pCodecCtx->width;
    w_height = pCodecCtx->height;
    
    win = SDL_CreateWindow("byron's easyPlayer1",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           w_width,
                           w_height,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    if(!win){
        logAndGFail("sdl 创建window失败");
    }
    renderer = SDL_CreateRenderer(win, -1, 0);
    if(!renderer){
        logAndGFail("sdl create render failed");
    }
    pixFormat = SDL_PIXELFORMAT_IYUV;
    texture = SDL_CreateTexture(renderer,
                                pixFormat,
                                SDL_TEXTUREACCESS_STREAMING,
                                w_width,
                                w_height);
    swsCtx = sws_getContext(pCodecCtx->width,
                            pCodecCtx->height,
                            pCodecCtx->pix_fmt,
                            pCodecCtx->width,
                            pCodecCtx->height,
                            AV_PIX_FMT_YUV420P,
                            SWS_BILINEAR,
                            NULL, NULL, NULL);
    
    pict = (AVPicture*)malloc(sizeof(AVPicture));
    avpicture_alloc(pict, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
    
    while (av_read_frame(pFormatCtx, &packet)>=0) {
        if(packet.stream_index == videoStream){
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            if(frameFinished){
                sws_scale(swsCtx,
                          (const uint8_t *const *)pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecCtx->height,
                          pict->data,
                          pict->linesize);
                SDL_UpdateYUVTexture(texture, NULL,
                                     pict->data[0], pict->linesize[0],
                                     pict->data[1], pict->linesize[1],
                                     pict->data[2], pict->linesize[2]);
                
                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &rect);
                SDL_RenderPresent(renderer);
            }
            
            av_free_packet(&packet);
            
            SDL_Event event;
            SDL_PollEvent(&event);
            switch (event.type) {
                case SDL_QUIT:
                    goto __QUIT;
                    break;

                default:
                    break;
            }
        }
    }
    
__QUIT:
    ret = 0;
    
__FAIL:
    
    
    if(pict){
        avpicture_free(pict);
    }
    
    if(texture){
        SDL_DestroyTexture(texture);
    }
    
    if(swsCtx){
        sws_freeContext(swsCtx);
    }
    
    if(renderer){
        SDL_DestroyRenderer(renderer);
    }
    
    if(win){
        SDL_DestroyWindow(win);
    }
    
    if(pFrame){
        av_frame_free(&pFrame);
    }
    
    if(pCodecCtx){
        avcodec_close(pCodecCtx);
    }
    
    if(pFormatCtx){
        avformat_free_context(pFormatCtx);
    }
    
    SDL_Quit();
    
    return ret;
}
