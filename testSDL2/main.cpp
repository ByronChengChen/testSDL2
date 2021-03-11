// ============================================================================
// [Include Section]
// ============================================================================
#include "CApp.h"
#include <SDL2/SDL.h>
#include "helloSDL.h"
#include "yuvPlayer.h"
#include "pcm_player.h"

extern "C"{
#include "player1A.h"
#include "byronEasyPlayer1.h"

#include "player2A&V.h"
#include "byronEasyPlayer2.h"
#include "player3A&V&T.h"
#include "player4A_V_Sync.h"
#include "player5A_V_Sync_plus.h"
#include "TestCForCK.h"
#include "avio_list_dir.h"
}


// ============================================================================
// [Entry-Point]
// ============================================================================
int main(int argc, char* argv[])
{
    //    return helloSDL(argc, argv);
    //    return testYuv2(argc,argv);
    //    return pcmPlayer(argc, argv);
    
//        return easyPlayer(argc, argv);
    //    return byronEasyPlayer(argc,argv);
    
//    return easyPlayer2(argc, argv);
//    return byronEasyPlayer2(argc,argv);

//    return playerA_V_T(argc, argv);
//    return playerA_V_Sync(argc, argv);
//    testSDLAddTimer();
    //音画同步的播放器
    return player5A_V_Sync_plus(argc, argv);
//    return decode_video(argc, argv);
//    return test_avio_list_dic(argc, argv);
//    return 0;
}
