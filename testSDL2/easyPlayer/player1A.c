//
//  easyPlayer1.c
//  testSDL2
//
//  Created by chengkang on 9/10/20.
//  Copyright © 2020 MIGU. All rights reserved.
//
#include <stdio.h>
#include <SDL2/SDL.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>

#include "player1A.h"

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

//这个文件对应的是 63.实现最简单的播放器-1 64.实现最简单的播放器-2 其实是第一个简单的播放器 对应李超的代码是 player2.c
int easyPlayer(int argc, char *argv[]) {
    
    int ret = -1;
    int             i, videoStream;
    int             frameFinished;
    //set defualt size of window
    int w_width = 640;
    int w_height = 480;
    
    AVFormatContext *pFormatCtx = NULL; //for opening multi-media file
    AVCodecContext  *pCodecCtxOrig = NULL; //codec context
    AVCodecContext  *pCodecCtx = NULL;
    
    struct SwsContext *sws_ctx = NULL;
    AVCodec         *pCodec = NULL; // the codecer
    AVFrame         *pFrame = NULL;
    AVPacket        packet;
    AVPicture        *pict  = NULL;
    SDL_Rect        rect;
    Uint32       pixformat;
    
    //for render
    SDL_Window       *win = NULL;
    SDL_Renderer    *renderer = NULL;
    SDL_Texture     *texture = NULL;
    
    if(argc < 2) {
        //fprintf(stderr, "Usage: command <file>\n");
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: command <file>");
        return ret;
    }
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL - %s\n", SDL_GetError());
        return ret;
    }
    
    //Register all formats and codecs
    av_register_all();
    
    // Open video file
    argv[1] = "tsubasa.MP4";
    //!!!: ck info  简单播放器1 1.ffmpeg打开文件,保存媒体文件
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open video file!");
        goto __FAIL; // Couldn't open file
    }
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find stream infomation!");
        goto __FAIL; // Couldn't find stream information
    }
    
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    // Find the first video stream
    //!!!: ck info  简单播放器1 2.根据媒体文件找到视频流
    videoStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            videoStream=i;
            break;
        }
    }
    
    if(videoStream==-1){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Din't find a video stream!");
        goto __FAIL;// Didn't find a video stream
    }
    
    // Get a pointer to the codec context for the video stream
    //!!!: ck info  简单播放器1 3.根据媒体文件找到视频流的编码器上下文
    pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
    
    // Find the decoder for the video stream
    //!!!: ck info  简单播放器1 4.根据编码器上下文查找解码器
    pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec==NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec!\n");
        goto __FAIL; // Codec not found
    }
    
    // Copy context
    //!!!: ck info  简单播放器1 5.根据编码填充默认值
    pCodecCtx = avcodec_alloc_context3(pCodec);
    //!!!: ck info  简单播放器1 5.1 拷贝AVCodecContext 的设置 Copy the settings
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,  "Couldn't copy codec context");
        goto __FAIL;// Error copying codec context
    }
    
    // Open codec
    //!!!: ck info  简单播放器1 6.打开解码器，初始化解码器上下文 Initialize the AVCodecContext to use the given AVCodec.
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open decoder!\n");
        goto __FAIL; // Could not open codec
    }
    
    // Allocate video frame
    //Allocate an AVFrame and set its fields to default values.
    pFrame=av_frame_alloc();
    
    w_width = pCodecCtx->width;
    w_height = pCodecCtx->height;
    //!!!: ck info  简单播放器1 7.创建sdl的window
    win = SDL_CreateWindow( "Media Player",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           w_width, w_height,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!win){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window by SDL");
        goto __FAIL;
    }
    //!!!: ck info  简单播放器1 8. 创建sdl的渲染器
    renderer = SDL_CreateRenderer(win, -1, 0);
    if(!renderer){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Renderer by SDL");
        goto __FAIL;
    }
    
    pixformat = SDL_PIXELFORMAT_IYUV;
    //!!!: ck info  简单播放器1 9. 创建sdl 的纹理
    texture = SDL_CreateTexture(renderer,
                                pixformat,
                                SDL_TEXTUREACCESS_STREAMING,
                                w_width,
                                w_height);
    
    // initialize SWS context for software scaling
    //用于视屏图像转换
    sws_ctx = sws_getContext(
                             pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL
                             );
    
    //!!!: ck info  简单播放器1 9.1 创建并初始化图像
    pict = (AVPicture*)malloc(sizeof(AVPicture));
    avpicture_alloc(pict,
                    AV_PIX_FMT_YUV420P,
                    pCodecCtx->width,
                    pCodecCtx->height);
//
    
    // Read frames and save first five frames to disk
    //av_read_frame Return the next frame of a stream.0 if OK, < 0 on error or end of file. On error, pkt will be blank
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode the video frame of size avpkt->size from avpkt->data into picture.
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            
            // Did we get a video frame?
            if(frameFinished) {
                
                // Convert the image into YUV format that SDL uses
                //!!!: ck info  简单播放器1 10. 图片转换成yuv格式
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pict->data, pict->linesize);
                
                //!!!: ck info  简单播放器1 11. yuv图片数据填充到纹理
                SDL_UpdateYUVTexture(texture, NULL,
                                     pict->data[0], pict->linesize[0],
                                     pict->data[1], pict->linesize[1],
                                     pict->data[2], pict->linesize[2]);
                
                // Set Size of Window
                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;
                
                //!!!: ck info  简单播放器1 12. 用渲染器渲染纹理
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &rect);
                SDL_RenderPresent(renderer);
            }
        }
        
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        
        SDL_Event event;
        SDL_PollEvent(&event);
        switch(event.type) {
            case SDL_QUIT:
                goto __QUIT;
                break;
            default:
                break;
        }
    }
    
__QUIT:
    ret = 0;
    
__FAIL:
    // Free the YUV frame
    if(pFrame){
        av_frame_free(&pFrame);
    }
    
    // Close the codec
    if(pCodecCtx){
        avcodec_close(pCodecCtx);
    }
    
    if(pCodecCtxOrig){
        avcodec_close(pCodecCtxOrig);
    }
    
    // Close the video file
    if(pFormatCtx){
        avformat_close_input(&pFormatCtx);
    }
    
    if(pict){
        avpicture_free(pict);
        free(pict);
    }
    
    if(win){
        SDL_DestroyWindow(win);
    }
    
    if(renderer){
        SDL_DestroyRenderer(renderer);
    }
    
    if(texture){
        SDL_DestroyTexture(texture);
    }
    
    SDL_Quit();
    
    return ret;
}

//int testEasyPlayer(void){
//    printf("%s",__func__);
//    return 0;
//}
