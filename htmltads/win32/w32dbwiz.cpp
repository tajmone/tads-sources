#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32dbwiz.cpp,v 1.1 1999/07/11 00:46:50 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32dbwiz.cpp - debugger wizards
Function

Notes

Modified
  06/08/99 MJRoberts  - Creation
*/

#include <Windows.h>

#include "tadsapp.h"
#include "tadsdlg.h"
#include "w32dbwiz.h"
#include "w32tdb.h"
#include "htmlres.h"
#include "htmldcfg.h"

/* ------------------------------------------------------------------------ */
/*
 *   Debugger Startup Dialog
 */

/*
 *   run the dialog
 */
int CHtmlDbgWiz::run_start_dlg(HWND parent, CHtmlDebugConfig *config,
                               const textchar_t *last_game)
{
    CHtmlDbgWiz *dlg;
    int ret;

    /* create the dialog */
    dlg = new CHtmlDbgWiz(config, last_game != 0 && last_game[0] != '\0');

    /* run it */
    ret = dlg->run_modal(DLG_DBGWIZ_START, parent,
                         CTadsApp::get_app()->get_instance());

    /* delete the dialog now that we're done with it */
    delete dlg;

    /* return the dialog result */
    return ret;
}

/*
 *   create
 */
CHtmlDbgWiz::CHtmlDbgWiz(CHtmlDebugConfig *config, int last_game_avail)
{
    /* remember the debugger main window and global configuration object */
    global_config_ = config;
    last_game_avail_ = last_game_avail;

    /* load our bitmaps */
    bmp_new_ = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDB_BMP_START_NEW), IMAGE_BITMAP, 0, 0,
        LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_LOADMAP3DCOLORS | LR_SHARED);

    bmp_open_ = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDB_BMP_START_OPEN), IMAGE_BITMAP, 0, 0,
        LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_LOADMAP3DCOLORS | LR_SHARED);

    bmp_open_last_ = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDB_BMP_START_OPENLAST), IMAGE_BITMAP, 0, 0,
        LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_LOADMAP3DCOLORS | LR_SHARED);

    bkg_bmp_ = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDB_BMP_START_BKG), IMAGE_BITMAP, 0, 0,
        LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_SHARED);
}

/*
 *   destroy
 */
CHtmlDbgWiz::~CHtmlDbgWiz()
{
}

/*
 *   initialize
 */
void CHtmlDbgWiz::init()
{
    int val;
    CTadsFont *font;

    /* center the dialog */
    center_dlg_win(handle_);

    /* set the button bitmaps */
    SendMessage(GetDlgItem(handle_, IDC_BTN_CREATE), BM_SETIMAGE,
                (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_new_);
    SendMessage(GetDlgItem(handle_, IDC_BTN_OPEN), BM_SETIMAGE,
                (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_open_);
    SendMessage(GetDlgItem(handle_, IDC_BTN_OPENLAST), BM_SETIMAGE,
                (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_open_last_);

    /*
     *   hide the 'open last game' button (and the related captions) if
     *   there's no previous game to open
     */
    if (!last_game_avail_)
    {
        ShowWindow(GetDlgItem(handle_, IDC_BTN_OPENLAST), SW_HIDE);
        ShowWindow(GetDlgItem(handle_, IDC_ST_OPENLAST), SW_HIDE);
        ShowWindow(GetDlgItem(handle_, IDC_ST_OPENLAST2), SW_HIDE);
    }

    /* set up a font for a bold version of the dialog font */
    font = CTadsApp::get_app()->get_font("Arial", 10, TRUE, FALSE,
                                         GetSysColor(COLOR_BTNTEXT),
                                         FF_SWISS | VARIABLE_PITCH);

    /* embolden some of the text items */
    SendMessage(GetDlgItem(handle_, IDC_ST_WELCOME), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));
    SendMessage(GetDlgItem(handle_, IDC_ST_CREATE), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));
    SendMessage(GetDlgItem(handle_, IDC_ST_OPEN), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));
    SendMessage(GetDlgItem(handle_, IDC_ST_OPENLAST), WM_SETFONT,
                (WPARAM)font->get_handle(), MAKELPARAM(TRUE, 0));

    /*
     *   Check the checkbox if "startup:action" is set to 1 (for the "show
     *   welcome dialog" mode) or is undefined.  (This config variable
     *   indicates the startup action; it defaults to "show welcome dialog,"
     *   which has value 1.)
     */
    if (global_config_->getval("startup", "action", &val))
        val = 1;
    CheckDlgButton(handle_, IDC_CK_SHOWAGAIN,
                   val == 1 ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   handle a message
 */
BOOL CHtmlDbgWiz::do_dialog_msg(HWND hwnd, UINT message,
                                WPARAM wpar, LPARAM lpar)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        /* erase - draw our background bitmap */
        {
            /* draw our bitmap into the dialog DC */
            POINT pt;
            pt.x = pt.y = 0;
            draw_bkg((HDC)wpar, pt);
        }
        return TRUE;

    case WM_CTLCOLORSTATIC:
        /* draw with black text and no background */
        SetTextColor((HDC)wpar, RGB(0x00,0x00,0x00));
        SetBkMode((HDC)wpar, TRANSPARENT);
        return (BOOL)GetStockObject(HOLLOW_BRUSH);
        break;

    case WM_NOTIFY:
        /*
         *   Check for custom draw notifications from our checkbox.  This
         *   works around a bug in the XP custom controls in "themed" mode.
         *   In particular, the controls always erase the background area,
         *   even when WM_CTLCOLORxxx tells them not to.  This appears to be
         *   a bug in the "custom draw" implementation, and the way to work
         *   around it seems to be to intercept the custom-draw post-paint
         *   notification and explicitly restore our background area at that
         *   time.  The regular control drawing will then proceed on the
         *   corrected background area.
         */
        {
            NMHDR *nm = (NMHDR *)lpar;
            NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW *)lpar;
            if (nm->hwndFrom == GetDlgItem(hwnd, IDC_CK_SHOWAGAIN)
                && nm->code == NM_CUSTOMDRAW
                && nmcd->dwDrawStage & CDDS_POSTPAINT)
            {
                POINT pt;

                /* get the dialog area relative to the control */
                pt.x = pt.y = 0;
                MapWindowPoints(hwnd, nm->hwndFrom, &pt, 1);

                /* draw our background into the control DC */
                draw_bkg(nmcd->hdc, pt);

                /* skip the default handling */
                return CDRF_SKIPDEFAULT;
            }
        }
        break;
    }

    /* inherit the default handling */
    return CTadsDialog::do_dialog_msg(hwnd, message, wpar, lpar);
}

/*
 *   Draw our background into a DC
 */
void CHtmlDbgWiz::draw_bkg(HDC hdc, POINT ofs)
{
    /* set up a memory DC for the bitmap */
    HDC memdc = CreateCompatibleDC(hdc);
    HGDIOBJ oldbmp = SelectObject(memdc, bkg_bmp_);

    /* get the size of the background bitmap */
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    GetDIBits(memdc, bkg_bmp_, 0, 0, 0, &info, DIB_RGB_COLORS);

    /* draw the image */
    RECT rc;
    GetClientRect(handle_, &rc);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, 0);
    StretchBlt(hdc, ofs.x, ofs.y, rc.right, rc.bottom,
               memdc, 0, 0, info.bmiHeader.biWidth, info.bmiHeader.biHeight,
               SRCCOPY);

    /* clean up */
    SelectObject(memdc, oldbmp);
    DeleteDC(memdc);
}


/*
 *   process a command
 */
int CHtmlDbgWiz::do_command(WPARAM id, HWND ctl)
{
    switch(id)
    {
    case IDC_CK_SHOWAGAIN:
        /*
         *   If they've checked the button, set the startup action to "show
         *   welcome dialog" (code 1); otherwise, set it to "do nothing"
         *   (code 3).
         *
         *   Note that we don't have to worry about other possible states; if
         *   we're showing the dialog at all, then the checkbox will have
         *   been initially checked.  We can thus arbitrarily choose the mode
         *   selected by unchecking the checkbox.
         */
        global_config_->setval("startup", "action",
                               (IsDlgButtonChecked(handle_, IDC_CK_SHOWAGAIN)
                                == BST_CHECKED) ? 1 : 3);
        return TRUE;

    case IDC_BTN_CREATE:
    case IDC_BTN_OPEN:
    case IDC_BTN_OPENLAST:
        /* dismiss the dialog and return this result code */
        EndDialog(handle_, id);
        return TRUE;

    default:
        /* use default processing */
        break;
    }

    /* inherit default handling */
    return CTadsDialog::do_command(id, ctl);
}

