/* Adventure Creator v2 Run-time engine
   Started 27-May-99 (c) 1999-2011 Chris Jones

  Adventure Game Studio source code Copyright 1999-2011 Chris Jones.
  All rights reserved.

  The AGS Editor Source Code is provided under the Artistic License 2.0
  http://www.opensource.org/licenses/artistic-license-2.0.php

  You MAY NOT compile your own builds of the engine without making it EXPLICITLY
  CLEAR that the code has been altered from the Standard Version.

*/

//
// Quit game procedure
//

#include "util/wgt2allg.h"
#include "gfx/ali3d.h"
#include "ac/cdaudio.h"
#include "ac/gamesetup.h"
#include "ac/gamesetupstruct.h"
#include "ac/record.h"
#include "ac/roomstatus.h"
#include "ac/translation.h"
#include "debug/agseditordebugger.h"
#include "debug/debug_log.h"
#include "debug/debugger.h"
#include "main/main.h"
#include "main/mainheader.h"
#include "main/quit.h"
#include "ac/spritecache.h"
#include "gfx/graphicsdriver.h"
#include "gfx/bitmap.h"

using AGS::Common::IBitmap;
namespace Bitmap = AGS::Common::Bitmap;

extern GameSetupStruct game;
extern int spritewidth[MAX_SPRITES],spriteheight[MAX_SPRITES];
extern SpriteCache spriteset;
extern RoomStatus troom;    // used for non-saveable rooms, eg. intro
extern int our_eip;
extern GameSetup usetup;
extern char pexbuf[STD_BUFFER_SIZE];
extern int proper_exit;
extern char check_dynamic_sprites_at_exit;
extern int editor_debugging_initialized;
extern IAGSEditorDebugger *editor_debugger;
extern int need_to_stop_cd;
extern IBitmap *_old_screen;
extern IBitmap *_sub_screen;
extern int use_cdplayer;
extern IGraphicsDriver *gfxDriver;

bool handledErrorInEditor;

void quit_tell_editor_debugger(char *qmsg)
{
    if (editor_debugging_initialized)
    {
        if ((qmsg[0] == '!') && (qmsg[1] != '|'))
        {
            handledErrorInEditor = send_exception_to_editor(&qmsg[1]);
        }
        send_message_to_editor("EXIT");
        editor_debugger->Shutdown();
    }
}

void quit_stop_cd()
{
    if (need_to_stop_cd)
        cd_manager(3,0);
}

void quit_shutdown_scripts()
{
    ccUnregisterAllObjects();
}

void quit_check_dynamic_sprites(char *qmsg)
{
    if ((qmsg[0] == '|') && (check_dynamic_sprites_at_exit) && 
        (game.options[OPT_DEBUGMODE] != 0)) {
            // game exiting normally -- make sure the dynamic sprites
            // have been deleted
            for (int i = 1; i < spriteset.elements; i++) {
                if (game.spriteflags[i] & SPF_DYNAMICALLOC)
                    debug_log("Dynamic sprite %d was never deleted", i);
            }
    }
}

void quit_shutdown_platform(char *qmsg)
{
    platform->AboutToQuitGame();

    our_eip = 9016;

    platform->ShutdownPlugins();

    quit_check_dynamic_sprites(qmsg);    

    // allegro_exit assumes screen is correct
	if (_old_screen) {
		Bitmap::SetScreenBitmap( _old_screen );
	}

    platform->FinishedUsingGraphicsMode();

    if (use_cdplayer)
        platform->ShutdownCDPlayer();
}

void quit_shutdown_audio()
{
    our_eip = 9917;
    game.options[OPT_CROSSFADEMUSIC] = 0;
    stopmusic();
#ifndef PSP_NO_MOD_PLAYBACK
    if (opts.mod_player)
        remove_mod_player();
#endif

    // Quit the sound thread.
    update_mp3_thread_running = false;

    remove_sound();
}

void quit_check_for_error_state(char *qmsg, char *alertis)
{
    if (qmsg[0]=='|') ; //qmsg++;
    else if (qmsg[0]=='!') { 
        qmsg++;

        if (qmsg[0] == '|')
            strcpy (alertis, "Abort key pressed.\n\n");
        else if (qmsg[0] == '?') {
            strcpy(alertis, "A fatal error has been generated by the script using the AbortGame function. Please contact the game author for support.\n\n");
            qmsg++;
        }
        else
            strcpy(alertis,"An error has occurred. Please contact the game author for support, as this "
            "is likely to be a scripting error and not a bug in AGS.\n"
            "(ACI version " ACI_VERSION_TEXT ")\n\n");

        strcat (alertis, get_cur_script(5) );

        if (qmsg[0] != '|')
            strcat(alertis,"\nError: ");
        else
            qmsg = "";
    }
    else if (qmsg[0] == '%') {
        qmsg++;

        sprintf(alertis, "A warning has been generated. This is not normally fatal, but you have selected "
            "to treat warnings as errors.\n"
            "(ACI version " ACI_VERSION_TEXT ")\n\n%s\n", get_cur_script(5));
    }
    else strcpy(alertis,"An internal error has occurred. Please note down the following information.\n"
        "If the problem persists, post the details on the AGS Technical Forum.\n"
        "(ACI version " ACI_VERSION_TEXT ")\n"
        "\nError: ");
}

void quit_destroy_subscreen()
{
    // close graphics mode (Win) or return to text mode (DOS)
    delete _sub_screen;
	_sub_screen = NULL;
}

void quit_shutdown_graphics()
{
    // Release the display mode (and anything dependant on the window)
    if (gfxDriver != NULL)
        gfxDriver->UnInit();

    // Tell Allegro that we are no longer in graphics mode
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
}

void quit_message_on_exit(char *qmsg, char *alertis)
{
    // successful exit displays no messages (because Windoze closes the dos-box
    // if it is empty).
    if (qmsg[0]=='|') ;
    else if (!handledErrorInEditor)
    {
        // Display the message (at this point the window still exists)
        sprintf(pexbuf,"%s\n",qmsg);
        strcat(alertis,pexbuf);
        platform->DisplayAlert(alertis);
    }
}

void quit_release_gfx_driver()
{
    if (gfxDriver != NULL)
    {
        delete gfxDriver;
        gfxDriver = NULL;
    }
}

void quit_release_data()
{
    // wipe all the interaction structs so they don't de-alloc the children twice
    resetRoomStatuses();
    memset (&troom, 0, sizeof(RoomStatus));

    /*  _CrtMemState memstart;
    _CrtMemCheckpoint(&memstart);
    _CrtMemDumpStatistics( &memstart );*/
}

void quit_print_last_fps(char *qmsg)
{
    /*  // print the FPS if there wasn't an error
    if ((play.debug_mode!=0) && (qmsg[0]=='|'))
    printf("Last cycle fps: %d\n",fps);*/
}

void quit_delete_temp_files()
{
    al_ffblk	dfb;
    int	dun = al_findfirst("~ac*.tmp",&dfb,FA_SEARCH);
    while (!dun) {
        unlink(dfb.name);
        dun = al_findnext(&dfb);
    }
    al_findclose (&dfb);
}

// TODO: move to test unit
#include "gfx/allegrobitmap.h"
using AGS::Common::CAllegroBitmap;
extern CAllegroBitmap *test_allegro_bitmap;
extern IDriverDependantBitmap *test_allegro_ddb;
void allegro_bitmap_test_release()
{
	delete test_allegro_bitmap;
	if (test_allegro_ddb)
		gfxDriver->DestroyDDB(test_allegro_ddb);
}

char return_to_roomedit[30] = "\0";
char return_to_room[150] = "\0";
// quit - exits the engine, shutting down everything gracefully
// The parameter is the message to print. If this message begins with
// an '!' character, then it is printed as a "contact game author" error.
// If it begins with a '|' then it is treated as a "thanks for playing" type
// message. If it begins with anything else, it is treated as an internal
// error.
// "!|" is a special code used to mean that the player has aborted (Alt+X)
void quit(char*quitmsg) {

    // Need to copy it in case it's from a plugin (since we're
    // about to free plugins)
    char qmsgbufr[STD_BUFFER_SIZE];
    strncpy(qmsgbufr, quitmsg, STD_BUFFER_SIZE);
    qmsgbufr[STD_BUFFER_SIZE - 1] = 0;
    char *qmsg = &qmsgbufr[0];

	allegro_bitmap_test_release();

    handledErrorInEditor = false;

    quit_tell_editor_debugger(qmsg);

    our_eip = 9900;

    stop_recording();

    quit_stop_cd();

    our_eip = 9020;

    quit_shutdown_scripts();

    quit_shutdown_platform(qmsg);

    our_eip = 9019;

    quit_shutdown_audio();
    
    our_eip = 9901;

    char alertis[1500]="\0";
    quit_check_for_error_state(qmsg, alertis);

    shutdown_font_renderer();
    our_eip = 9902;

    quit_destroy_subscreen();

    our_eip = 9907;

    close_translation();

    our_eip = 9908;

    quit_shutdown_graphics();

    quit_message_on_exit(qmsg, alertis);

    // remove the game window
    allegro_exit();

    quit_release_gfx_driver();

    platform->PostAllegroExit();

    our_eip = 9903;

    quit_release_data();

    quit_print_last_fps(qmsg);

    quit_delete_temp_files();

    proper_exit=1;

    write_log_debug("***** ENGINE HAS SHUTDOWN");

    shutdown_debug_system();

    our_eip = 9904;
    exit(EXIT_NORMAL);
}

extern "C" {
    void quit_c(char*msg) {
        quit(msg);
    }
}
