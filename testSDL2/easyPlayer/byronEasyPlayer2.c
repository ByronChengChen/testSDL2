//
//  byronEasyPlayer2.c
//  testSDL2
//
//  Created by chengkang on 9/16/20.
//  Copyright © 2020 MIGU. All rights reserved.
//

#include "byronEasyPlayer2.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#define logAndGFail(e) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,(e));\
goto __FAIL;

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

struct SwrContext *audio_conver_ctx = NULL;

typedef struct PacketQueue{
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int sizes;
    SDL_mutex *mutex;   //互斥
    SDL_cond *cond;     //同步
} PacketQueue;

PacketQueue byronAudioq;

int byronQuit = 0;

void byron_packet_queue_init(PacketQueue *q){
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int byron_packet_queue_put(PacketQueue *q, AVPacket *pkt){
    AVPacketList *pkt1;
    if(av_dup_packet(pkt)<0){
        return -1;
    }
    pkt1 = av_malloc(sizeof(AVPacketList));
    if(!pkt1){
        return -1;
    }
    
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    SDL_LockMutex(q->mutex);
    if(!q->last_pkt){
        q->first_pkt = pkt1;
    }else{
        q->last_pkt->next = pkt1;
    }
    
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->sizes += pkt1->pkt.size;
    SDL_CondSignal(q->cond);
    
    SDL_UnlockMutex(q->mutex);
    return 0;;
}

int byron_packet_queue_get(PacketQueue *q, AVPacket *pkt, int block){
    int ret = 0;
    AVPacketList *pkt1;
    SDL_LockMutex(q->mutex);
    
    for (;;) {
        if(byronQuit){
            ret = -1;
            break;
        }
        
        pkt1 = q->first_pkt;
        if(pkt1){
            q->first_pkt = pkt1->next;
            if(!q->first_pkt){
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->sizes -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        }else if(!block){
            ret = 0;
            break;
        }else{
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int byron_audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size){
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;
    
    int len1,data_size=0;
    
    for (;;) {
        while (audio_pkt_size>0) {
            int got_frame =0;
            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            if(len1 < 0){
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size = 0;
            if(got_frame){
                data_size = 2*2*frame.nb_samples;
                assert(data_size <= buf_size);
                swr_convert(audio_conver_ctx, &audio_buf, MAX_AUDIO_FRAME_SIZE*3/2, (const uint8_t **)frame.data, frame.nb_samples);
            }
            if(data_size<=0){
                continue;
            }
            return data_size;
        }
        if(pkt.data){
            av_free_packet(&pkt);
        }
        if(byronQuit){
            return -1;
        }
        if(byron_packet_queue_get(&byronAudioq, &pkt, 1)<0){
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
}

void audio_callBack(void *userData, uint8_t *stream, int len){
    AVCodecContext *aCodecCtx = (AVCodecContext *)userData;
    int len1,audio_size;
    
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE *3)/2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;
    
    while (len>0) {
        if(audio_buf_index >= audio_buf_size){
            audio_size = byron_audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            if(audio_size<0){
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            }else{
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len){
            len1 = len;
        }
        fprintf(stderr, "index=%d,len1=%d,len=%d\n",audio_buf_index,len1,len1);
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int byronEasyPlayer2(int argc ,char *argv[]){
    int ret = -1;
    int i,videoStream,frameFinished,w_width=640,w_height = 480,audioStream=-1;
    
    //video
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
    
    //audio
    AVCodecContext  *aCodecCtxOrg = NULL;
    AVCodecContext  *aCodecCtx = NULL;
    AVCodec         *aCodec = NULL;
    int64_t in_channel_layout;
    int64_t out_channel_layout;
    SDL_AudioSpec   wanted_spec, spec;
    
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
        }
        if(AVMEDIA_TYPE_AUDIO == pFormatCtx->streams[i]->codec->codec_type){
            audioStream = i;
        }
    }
    
    if(-1 == videoStream){
       logAndGFail("多媒体无视频流");
    }
    if(-1 == audioStream){
        logAndGFail("多媒体无音频流");
    }
    
    aCodecCtxOrg = pFormatCtx->streams[audioStream]->codec;
    aCodec = avcodec_find_decoder(aCodecCtxOrg->codec_id);
    if(!aCodec){
        logAndGFail("该音频编码格式不被支持")
    }
    aCodecCtx = avcodec_alloc_context3(aCodec);
    if(0 != avcodec_copy_context(aCodecCtx, aCodecCtxOrg)){
        logAndGFail("编码器音频无法被拷贝");
    }
    
    //set audio settings from codec info
    wanted_spec.freq = aCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = aCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callBack;
    wanted_spec.userdata = aCodecCtx;
    
    if(SDL_OpenAudio(&wanted_spec, &spec)<0){
        logAndGFail("打开音频失败");
    }
    avcodec_open2(aCodecCtx, aCodec, NULL);
    byron_packet_queue_init(&byronAudioq);
    
    in_channel_layout = av_get_default_channel_layout(aCodecCtx->channels);
    out_channel_layout = in_channel_layout;
    fprintf(stderr, "in layout:%lld,outlayout:%lld \n",in_channel_layout, out_channel_layout);
    audio_conver_ctx = swr_alloc();
    if(audio_conver_ctx){
        swr_alloc_set_opts(audio_conver_ctx,
                           out_channel_layout,
                           AV_SAMPLE_FMT_S16,
                           aCodecCtx->sample_rate,
                           in_channel_layout,
                           aCodecCtx->sample_fmt,
                           aCodecCtx->sample_rate,
                           0, NULL);
    }
    
    swr_init(audio_conver_ctx);
    SDL_PauseAudio(0);

    
    //编码器上下文
    pCodecCtxOrg = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtxOrg->codec_id);
    if(NULL == pCodec){
       logAndGFail("该视频编码格式不被支持");
    }
    
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(0 != avcodec_copy_context(pCodecCtx, pCodecCtxOrg)){
        logAndGFail("编码器视频无法被拷贝");
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
        }else if(packet.stream_index == audioStream){
            byron_packet_queue_put(&byronAudioq, &packet);
        }else{
            av_free_packet(&packet);
        }
        
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
    
__QUIT:
    ret = 0;
    
__FAIL:
    if(aCodecCtxOrg){
        avcodec_close(aCodecCtxOrg);
    }
    
    if(aCodecCtx){
        avcodec_close(aCodecCtx);
    }
    
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
