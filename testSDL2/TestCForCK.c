//
//  TestCForCK.c
//  testSDL2
//
//  Created by chengkang on 11/3/20.
//  Copyright © 2020 MIGU. All rights reserved.
//

#include "TestCForCK.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>


static int num = 9;
pthread_t thread1;

static Uint32 handleTimer(Uint32 interval, void *opaque) {
    num --;
    printf("num:%d",num);
    return num; /* 0 means stop timer */
}

void testSDLAddTimer(){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    SDL_AddTimer(40, handleTimer, &num);
    int a=0;
    scanf("%d",&a);
}
