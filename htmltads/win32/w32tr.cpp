#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32tr.cpp,v 1.4 1999/07/11 00:46:52 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32tr.cpp - win32 configuration file for HTML TADS Runtime
Function

Notes

Modified
  01/31/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef W32VER_H
#include "w32ver.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Pre-show-window processing.  We don't need to do anything here.
 */
int w32_pre_show_window(int argc, char **argv)
{
    /* tell the caller to start up normally */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pre-start processing.  Do nothing; let the TADS engine handle it.
 */
int w32_pre_start(int iter, int argc, char **argv)
{
    CHtmlSys_mainwin *mainwin;

    /*
     *   if we're not on the first iteration, or we have any arguments,
     *   start up normally - this will enter the TADS engine and parse the
     *   arguments
     */
    if (iter != 0 || argc > 1)
        return TRUE;

    /* get the main window; if we have one, check the startup preferences */
    mainwin = CHtmlSys_mainwin::get_main_win();
    if (mainwin != 0)
    {
        CHtmlPreferences *prefs;
        osfildef *exe_fp;

        /* get the preferences object */
        prefs = mainwin->get_prefs();

        /* set the initial "open" dialog directory */
        if (prefs != 0
            && prefs->get_init_open_folder() != 0)
        {
            /* set the open file path */
            CTadsApp::get_app()
                ->set_openfile_path(prefs->get_init_open_folder());
        }

        /*
         *   if we have a game file attached to the executable itself,
         *   simply return so that we'll start up the engine and load the
         *   attached game
         */
        if (argc == 1
            && (exe_fp = os_exeseek(argv[0], "TGAM")) != 0)
        {
            long curpos, endpos;

            /* determine where we are relative to the end of the file */
            curpos = osfpos(exe_fp);
            osfseek(exe_fp, 0, OSFSK_END);
            endpos = osfpos(exe_fp);

            /* done with the exe file - close it */
            osfcls(exe_fp);

            /*
             *   if os_exeseek() left us somewhere other than at the end
             *   of the exe file, then we seem to have a valid game file
             *   attached - simply return so that the caller will launch
             *   TADS
             */
            if (curpos != endpos)
                return TRUE;
        }

        /*
         *   if we're meant to ask for a game at startup, simply return so
         *   that the caller will ask for the game and launch TADS
         */
        if (mainwin->get_prefs() == 0
            || mainwin->get_prefs()->get_init_ask_game())
            return TRUE;

        /*
         *   wait until the player loads a game, and return the result,
         *   which has the same meaning vis-a-vis app termination as our
         *   own return code
         */
        return mainwin->wait_for_new_game(FALSE);
    }

    /* tell the caller to proceed */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Perform post-quit processing.  Returns true if we should start over
 *   for another loop through the TADS main entrypoint, false if we should
 *   terminate the program.
 */
int w32_post_quit(int main_ret_code)
{
    /* clean up the network layer */
    w32_cleanup();

    /*
     *   if a new game has already been selected, return immediately so
     *   that we can go load it
     */
    if (CHtmlSys_mainwin::get_main_win() != 0
        && CHtmlSys_mainwin::get_main_win()->get_pending_new_game() != 0)
        return TRUE;

    /* if we successfully invoked a game, pause if desired */
    if (main_ret_code == OSEXSUCC && CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->pause_after_game_quit();

    /* wait for the user to quit or select a new game */
    if (CHtmlSys_mainwin::get_main_win() == 0
        || !CHtmlSys_mainwin::get_main_win()->wait_for_new_game(TRUE))
    {
        /*
         *   they're quitting - return false to indicate that we should
         *   terminate
         */
        return FALSE;
    }

    /*
     *   a new game has been selected - tell the caller to run TADS again
     *   with the new game
     */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a message box to show a message from the VM.  For the run-time,
 *   simply show a standard system message box.
 */
void w32_msgbox(const char *msg, const char *url)
{
    /*
     *   If there's a URL, display the special message box with an offer to
     *   check for updates.  Otherwise, just display the standard error
     *   alert.
     */
    if (url != 0)
    {
        /* generate the full message */
        char *buf = (char *)th_malloc(strlen(msg) + strlen(url) + 256);
        sprintf(buf, "%s\r\n\r\n"
                "This error can usually be solved by updating to the "
                "latest version of HTML TADS. Would you like to go to "
                "the tads.org Web site now to check for an update?",
                msg);

        /* show the check-for-updates box */
        int btn = MessageBox(0, buf, "TADS",
                             MB_YESNO | MB_ICONEXCLAMATION | MB_TASKMODAL);

        /* if they selected Yes, go to the update URL */
        if (btn == IDYES)
            ShellExecute(0, "open", url, 0, 0, SW_SHOWNORMAL);

        /* done with our allocated buffer */
        th_free(buf);
    }
    else
    {
        /* use the standard box (just the message and an "OK" button) */
        MessageBox(0, msg, "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
}
