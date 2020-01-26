/* $Header: d:/cvsroot/tads/html/win32/w32dbwiz.h,v 1.1 1999/07/11 00:46:50 MJRoberts Exp $ */

/*
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32dbwiz.h - TADS debugger wizards
Function

Notes

Modified
  06/08/99 MJRoberts  - Creation
*/

#ifndef W32DBWIZ_H
#define W32DBWIZ_H

#include <Windows.h>

#include "tadsdlg.h"

/* ------------------------------------------------------------------------ */
/*
 *   Startup Dialog
 */

class CHtmlDbgWiz: public CTadsDialog
{
public:
    CHtmlDbgWiz(class CHtmlDebugConfig *config, int last_game_avail);
    ~CHtmlDbgWiz();

    /* run the startup dialog */
    static int run_start_dlg(HWND parent, class CHtmlDebugConfig *config,
                             const textchar_t *last_game);

    /* initialize */
    void init();

    /* process a command */
    int do_command(WPARAM id, HWND ctl);

    /* generic message handler */
    BOOL do_dialog_msg(HWND dlg_hwnd, UINT message,
                       WPARAM wpar, LPARAM lpar);

protected:
    /* draw our background image */
    void draw_bkg(HDC hdc, POINT ofs);

    /* button bitmaps */
    HBITMAP bmp_new_;
    HBITMAP bmp_open_;
    HBITMAP bmp_open_last_;

    /* background bitmap */
    HBITMAP bkg_bmp_;

    /* debugger global configuration object */
    class CHtmlDebugConfig *global_config_;

    /* is there a recent game available? */
    int last_game_avail_;
};

#endif /* W32DBWIZ_H */

