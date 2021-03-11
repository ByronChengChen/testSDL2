#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "helloSDL.h"


int helloSDL(int argc, char* argv[])
{
    SDL_Window *window = NULL;
    SDL_Renderer *render = NULL;
    SDL_Texture *texture = NULL;
    SDL_Rect rect;
    rect.w = 30;
    rect.h = 30;
    
    int quit = 1;
    SDL_Event event; // SDL窗口事件
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Log("hello sdl2");
    window = SDL_CreateWindow("tubasa", 200, 400, 640, 480, SDL_WINDOW_SHOWN);
    if(!window){
        SDL_Log("创建window失败");
        goto __EXIT;
    }
    SDL_SetRenderDrawColor(render, 255, 0, 0, 0);
    render = SDL_CreateRenderer(window, -1, 0);
    if(!render){
        SDL_Log("创建render 失败");
        goto __DWINDOW;
    }
    
#ifdef RELEASE
    SDL_RenderClear(render);

    SDL_RenderPresent(render);
#endif
    
    texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 640, 480);
    
    if(!texture){
        goto __RENDER;
    }
    
#ifdef RELEASE
    while(true) {
        if (SDL_PollEvent(&event)) { // 对当前待处理事件进行轮询。
            if (SDL_QUIT == event.type) { // 如果事件为推出SDL，结束循环。
                printf("SDL quit");
                break;
            }else{
                SDL_Log("event type:%d",event.type);
            }
        }
    }
    
#endif
    
    do{
//        SDL_WaitEvent(&event);
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                quit = 0;
                break;

            default:
                SDL_Log("event.type:%d",event.type);
                break;
        }

        rect.x = rand()%600;
        rect.y = rand()%450;

        SDL_SetRenderTarget(render, texture);
        SDL_SetRenderDrawColor(render, 0, 0, 0, 0);
        SDL_RenderClear(render);

        SDL_RenderDrawRect(render, &rect);
        SDL_SetRenderDrawColor(render, 255, 0, 0, 0 );
        SDL_RenderFillRect(render, &rect);

        SDL_SetRenderTarget(render, NULL);
        SDL_RenderCopy(render, texture, NULL, NULL);

        SDL_RenderPresent(render);

    }while(quit);
    
    
    SDL_DestroyTexture(texture);
    
__RENDER:
    SDL_DestroyRenderer(render);
    
__DWINDOW:
    SDL_DestroyWindow(window);
    
__EXIT:
    SDL_Quit();
    return 0;
}
