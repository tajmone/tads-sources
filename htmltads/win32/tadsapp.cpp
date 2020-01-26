#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsapp.cpp,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsapp.cpp - TADS Windows application object
Function

Notes

Modified
  09/16/97 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include <Ole2.h>
#include <Windows.h>
#include <shlobj.h>

#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSKB_H
#include "tadskb.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Application object
 */

/* static containing the app object singleton */
CTadsApp *CTadsApp::app_ = 0;

/* system menu font, in normal and bold renditions */
HFONT CTadsApp::menu_font = 0;
HFONT CTadsApp::bold_menu_font = 0;

/* the TLS index for our pre-thread storage object */
DWORD CTadsApp::tls_index_ = 0;

/* the keyboard utilities singleton */
CTadsKeyboard *CTadsApp::kb_ = 0;

/* registered statusline list entry */
struct statline_listele
{
    statline_listele *nxt;
    CTadsStatusline *stat;

    statline_listele(CTadsStatusline *s, statline_listele *n)
        { stat = s; nxt = n; }
};

/* registered menu handler list entry */
struct imh_listele
{
    imh_listele *nxt;
    IMenuHandler *imh;

    imh_listele(IMenuHandler *i, imh_listele *n)
        { imh = i; nxt = n; }
};


/*
 *   initialize
 */
CTadsApp::CTadsApp(HINSTANCE inst, HINSTANCE pinst,
                   LPSTR /*cmdline*/, int /*cmdshow*/)
{
    OSVERSIONINFO osver;

    /* remember the application instance handle */
    instance_ = inst;

    /* we don't have an MDI frame window yet */
    mdi_win_ = 0;

    /* no fonts allocated yet */
    fonts_allocated_ = 0;
    fontlist_ = 0;
    fontcount_ = 0;

    /* no accelerators yet */
    waccel_ = 0;
    taccel_ = 0;
    accel_win_ = 0;
    accel_enabled_ = TRUE;

    /* no registered statuslines yet */
    statusline_ = 0;

    /* no registered menu handlers yet */
    menu_handlers_ = 0;

    /* initialize OLE */
    switch(OleInitialize(0))
    {
    case S_OK:
    case S_FALSE:
        /*
         *   note that OLE was initialized, so we'll need to uninitialize
         *   it before we shut down
         */
        ole_inited_ = TRUE;
        break;

    default:
        /* OLE didn't initialize properly */
        ole_inited_ = FALSE;
        break;
    }

    /* allocate our thread local storage index */
    tls_index_ = TlsAlloc();

    /* no file-dialog directory yet */
    open_file_dir_ = 0;

    /* we don't have a message filter installed yet */
    msg_filter_ = 0;

    /* allocate some space for modeless dialogs */
    modeless_dlg_cnt_ = 0;
    modeless_dlg_alloc_ = 10;
    modeless_dlgs_ = (HWND *)th_malloc(modeless_dlg_alloc_ * sizeof(HWND));

    /* get OS version information */
    memset(&osver, 0, sizeof(osver));
    osver.dwOSVersionInfoSize = sizeof(osver);
    GetVersionEx(&osver);

    /* note the windows version identifiers */
    win_sys_id_ = osver.dwPlatformId;
    win_ver_major_ = osver.dwMajorVersion;
    win_ver_minor_ = osver.dwMinorVersion;

    /* register our window classes if necessary */
    if (pinst == 0)
    {
        /* register the basic window class */
        CTadsWin::register_win_class(this);

        /* perform static class initialization */
        CTadsWin::class_init(this);
        CTadsWinScroll::class_init(this);
    }

    /* get the system menu font */
    {
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(ncm);

        /*
         *   get the font from the system parameters, which works for newer
         *   versions of Windows; if that doesn't work, we're on an older
         *   version, so simply use the DEFAULT_GUI_FONT stock object
         */
        if (SystemParametersInfo(
            SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            menu_font = CreateFontIndirect(&ncm.lfMenuFont);
        else
            menu_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    /* create a boldface version of the system menu font */
    bold_menu_font = make_bold_font(menu_font);

    /* set up our keyboard utilities singleton */
    kb_ = new CTadsKeyboard();

    /* create an empty accelerator table */
    ACCEL a[1];
    empty_accel_ = CreateAcceleratorTable(a, 0);
}

CTadsApp::~CTadsApp()
{
    int i;
    CTadsFont **p;

    /* destroy our empty accelerator table */
    DestroyAcceleratorTable(empty_accel_);

    /* perform static termination */
    CTadsWin::class_terminate();
    CTadsWinScroll::class_terminate();
    CTadsDirectMusic::class_terminate();

    /* done with our keyboard utilities object */
    delete kb_;

    /* delete any fonts we've allocated */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
        delete *p;

    /* delete the font list */
    if (fontlist_)
    {
        th_free(fontlist_);
        fontlist_ = 0;
        fontcount_ = 0;
        fonts_allocated_ = 0;
    }

    /* delete the open-file buffer, if we have one */
    if (open_file_dir_ != 0)
        th_free(open_file_dir_);

    /* delete our modeless dialog list */
    th_free(modeless_dlgs_);

    /* deinitialize OLE if necessary */
    if (ole_inited_)
        OleUninitialize();

    /* delete the main thread's TLS object */
    on_thread_exit();

    /* delete our TLS index */
    TlsFree(tls_index_);
}

/*
 *   Get my thread-local storage object
 */
CTadsAppTls *CTadsApp::get_tls_obj()
{
    CTadsAppTls *obj;

    /* get the existing object; if we have one, simply return it */
    if ((obj = (CTadsAppTls *)TlsGetValue(tls_index_)) != 0)
        return obj;

    /* we don't have one - allocate one */
    obj = new CTadsAppTls();

    /* store it */
    TlsSetValue(tls_index_, obj);

    /* return it */
    return obj;
}

/*
 *   Exit a thread.  This deletes the TLS object, if any, associated with the
 *   thread.
 */
void CTadsApp::on_thread_exit()
{
    /* if we have a TLS object for this thread, delete it */
    CTadsAppTls *obj;

    /* get the existing object; if we have one, simply return it */
    if ((obj = (CTadsAppTls *)TlsGetValue(tls_index_)) != 0)
    {
        /* clear our TLS slot */
        TlsSetValue(tls_index_, 0);

        /* delete the object */
        delete obj;
    }
}

/*
 *   create the application object
 */
void CTadsApp::create_app(HINSTANCE inst, HINSTANCE pinst,
                          LPSTR cmdline, int cmdshow)
{
    /* if there's not already an app, create one */
    if (app_ == 0)
        app_ = new CTadsApp(inst, pinst, cmdline, cmdshow);
}

/*
 *   delete the application object
 */
void CTadsApp::delete_app()
{
    if (app_ != 0)
    {
        delete app_;
        app_ = 0;
    }
}

/*
 *   Add a modeless dialog to our list of modeless dialogs
 */
void CTadsApp::add_modeless(HWND win)
{
    /* allocate more space in our list if necessary */
    if (modeless_dlg_cnt_ == modeless_dlg_alloc_)
    {
        modeless_dlg_alloc_ += 10;
        modeless_dlgs_ = (HWND *)
                         th_realloc(modeless_dlgs_,
                                    modeless_dlg_alloc_ * sizeof(HWND));
    }

    /* add this to the end of our list */
    modeless_dlgs_[modeless_dlg_cnt_++] = win;
}

/*
 *   Remove a modeless dialog from our list of modeless dialogs
 */
void CTadsApp::remove_modeless(HWND win)
{
    HWND *winp;
    int i;

    /* find the window in our list */
    for (i = 0, winp = modeless_dlgs_ ; i < modeless_dlg_cnt_ ; ++i, ++winp)
    {
        /* see if this is the one */
        if (*winp == win)
        {
            /* move everything following down a slot */
            if (i + 1 < modeless_dlg_cnt_)
                memmove(winp, winp + 1,
                        (modeless_dlg_cnt_ - i - 1) * sizeof(HWND *));

            /* that's one less dialog in our list */
            --modeless_dlg_cnt_;

            /* done */
            return;
        }
    }
}

/*
 *   main event loop
 */
int CTadsApp::event_loop(int *flag)
{
    BOOL stat;

    /* if there's a flag, initialize it to false */
    if (flag != 0)
        *flag = 0;

    /* get and dispatch messages */
    for (;;)
    {
        MSG msg;

        /*
         *   if a flag variable was provided, and the flag variable has
         *   become true, we're done -- return true to indicate that the
         *   application is to continue running
         */
        if (flag != 0 && *flag != 0)
            return TRUE;

#if 0
        /* check for an empty message queue */
        while (!PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
            do_idle();
#endif

        /* process the next message */
        stat = GetMessage(&msg, 0, 0, 0);
        if (stat == -1)
        {
            /* error reading a message - terminate */
            return FALSE;
        }
        else if (stat != 0)
        {
            /* process the message */
            process_message(&msg);
        }
        else
        {
            /* the application is quitting */
            return FALSE;
        }
    }
}

/*
 *   Apply our standard message processing to a message read from the Windows
 *   system event queue.
 */
void CTadsApp::process_message(MSG *msg)
{
    int done;
    HWND *winp;
    int i;

    /*
     *   If there's a message filter, give it the first shot at the message.
     *   If the message filter marks the message as handled, then skip all
     *   remaining processing on it.
     */
    if (msg_filter_ != 0 && msg_filter_->filter_system_message(msg))
        done = TRUE;

    /*
     *   scan the modeless dialog list, and see if any of the dialogs want to
     *   process the message as a dialog navigation event
     */
    for (i = 0, winp = modeless_dlgs_, done = FALSE ;
         i < modeless_dlg_cnt_ ; ++i, ++winp)
    {
        /* see if this is a message for the given dialog */
        if (IsDialogMessage(*winp, msg))
        {
            /* it is - don't process the message any further */
            done = TRUE;
            break;
        }
    }

    /*
     *   try treating it as an MDI message, if we have an MDI main frame
     *   window
     */
    if (!done
        && mdi_win_ != 0
        && TranslateMDISysAccel(mdi_win_->get_client_handle(), msg))
        done = TRUE;

    /* try the accelerators, if they're enabled */
    if (!done && accel_translate(msg))
        done = TRUE;

    /* if we didn't already handle it, dispatch it normally */
    if (!done)
    {
        /* translate and dispatch the message */
        TranslateMessage(msg);
        DispatchMessage(msg);
    }
}


/*
 *   Get the accelerator translation for a message
 */
int CTadsApp::get_accel_translation(MSG *msg)
{
    /*
     *   if we have an accelerator, and its enabled, let it do the
     *   translation; otherwise there's no translation
     */
    return (taccel_ != 0
            && accel_enabled_
            && taccel_->get_translation(accel_win_, msg));
}

/*
 *   Translate and execute a command via accelerators
 */
int CTadsApp::accel_translate(MSG *msg)
{
    /*
     *   If accelerators are translated, try the windows native accelerator
     *   and then our custom accelerator.
     */
    return (accel_enabled_
            && ((waccel_ != 0
                 && TranslateAccelerator(
                     accel_win_->get_handle(), waccel_, msg))
                || (taccel_ != 0
                    && taccel_->translate(accel_win_, msg))));
}

/* ------------------------------------------------------------------------ */
/*
 *   Register a statusline
 */
void CTadsApp::register_statusline(class CTadsStatusline *sl)
{
    /* link in a new list entry for the statusline */
    statusline_ = new statline_listele(sl, statusline_);
}

/*
 *   unregister a statusline
 */
void CTadsApp::unregister_statusline(class CTadsStatusline *sl)
{
    statline_listele *cur, *prv;

    /* find the list element */
    for (prv = 0, cur = statusline_ ; cur != 0 ; prv = cur, cur = cur->nxt)
    {
        /* if this is the one, unlink it */
        if (cur->stat == sl)
        {
            /* unlink it */
            if (prv == 0)
                statusline_ = cur->nxt;
            else
                prv->nxt = cur->nxt;

            /* delete this entry */
            delete cur;

            /* that's all we need to do */
            return;
        }
    }
}

/*
 *   Process a WM_MENUSELECT through each status line
 */
void CTadsApp::send_menuselect_to_statuslines(
    HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    statline_listele *cur;

    /* pass it to each status line */
    for (cur = statusline_ ; cur != 0 ; cur = cur->nxt)
        cur->stat->menu_select_msg(hwnd, wparam, lparam);
}


/* ------------------------------------------------------------------------ */
/*
 *   Register an icon menu handler
 */
void CTadsApp::register_menu_handler(class IMenuHandler *imh)
{
    /* link in a new list entry */
    menu_handlers_ = new imh_listele(imh, menu_handlers_);
}

/*
 *   unregister a statusline
 */
void CTadsApp::unregister_menu_handler(class IMenuHandler *imh)
{
    imh_listele *cur, *prv;

    /* find the list element */
    for (prv = 0, cur = menu_handlers_ ; cur != 0 ; prv = cur, cur = cur->nxt)
    {
        /* if this is the one, unlink it */
        if (cur->imh == imh)
        {
            /* unlink it */
            if (prv == 0)
                menu_handlers_ = cur->nxt;
            else
                prv->nxt = cur->nxt;

            /* delete this entry */
            delete cur;

            /* that's all we need to do */
            return;
        }
    }
}

/*
 *   Process a WM_INITMENUPOPUP through the menu handlers
 */
void CTadsApp::send_initmenupopup_to_menu_handlers(
    HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    imh_listele *cur;
    HMENU menu = (HMENU)wparam;
    int pos = LOWORD(lparam);
    int sysmenu = HIWORD(lparam);

    /* pass it to each handle */
    for (cur = menu_handlers_ ; cur != 0 ; cur = cur->nxt)
        cur->imh->imh_init_menu_popup(menu, pos, sysmenu);

    /* pass it to the active accelerator, if any */
    if (taccel_ != 0)
        taccel_->init_menu_popup(menu, pos, sysmenu);
}

/*
 *   Process a WM_MEASUREITEM through the menu handlers
 */
void CTadsApp::send_measureitem_to_menu_handlers(
    HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    imh_listele *cur;
    int ctl_id = (int)wparam;
    MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)lparam;

    /* if this isn't for a menu, ignore it */
    if (mi->CtlType != ODT_MENU)
        return;

    /* pass it to each menu handler */
    for (cur = menu_handlers_ ; cur != 0 ; cur = cur->nxt)
        cur->imh->imh_measure_item(ctl_id, mi);
}

/*
 *   Process a WM_DRAWITEM through the menu handlers
 */
void CTadsApp::send_drawitem_to_menu_handlers(
    HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    imh_listele *cur;
    int ctl_id = (int)wparam;
    DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lparam;

    /* if this isn't a menu item, ignore it */
    if (di->CtlType != ODT_MENU)
        return;

    /* pass it to each menu handler */
    for (cur = menu_handlers_ ; cur != 0 ; cur = cur->nxt)
        cur->imh->imh_draw_item(ctl_id, di);
}


/* ------------------------------------------------------------------------ */
/*
 *   idle-time processing
 */
void CTadsApp::do_idle()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the active accelerator
 */
void CTadsApp::set_accel(HACCEL accel, class CTadsWin *win)
{
    /* remember the new accelerator settings */
    taccel_ = 0;
    waccel_ = accel;
    set_accel_win(win);
}

void CTadsApp::set_accel(CTadsAccelerator *accel, class CTadsWin *win)
{
    /* remember the new settings */
    taccel_ = accel;
    waccel_ = 0;
    set_accel_win(win);

    /* reset the accelerator's key state */
    accel->reset_keystate();
}

void CTadsApp::clear_accel()
{
    taccel_ = 0;
    waccel_ = 0;
    set_accel_win(0);
}

/* set the accelerator window */
void CTadsApp::set_accel_win(CTadsWin *win)
{
    /* add a reference to the new window */
    if (win != 0)
        win->AddRef();

    /* release any previous accelerator window */
    if (accel_win_ != 0)
        accel_win_->Release();

    /* remember the new window */
    accel_win_ = win;
}

/* receive notification that a window with an accelerator is closing */
void CTadsApp::closing_accel_win(CTadsWin *win)
{
    /* if it's our current accelerator window, forget it */
    if (accel_win_ == win)
    {
        /* forget our accelerator */
        taccel_ = 0;
        waccel_ = 0;
        set_accel_win(0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a font given the font description.
 */
class CTadsFont *CTadsApp::get_font(const char *font_name, int point_size,
                                    int bold, int italic, int underline,
                                    COLORREF fgcolor, int use_fgcolor,
                                    COLORREF bgcolor, int use_bgcolor,
                                    int attributes)
{
    CTadsLOGFONT tlf;

    /* set up the font description structure */
    tlf.lf.lfWidth = 0;
    tlf.lf.lfEscapement = 0;
    tlf.lf.lfOrientation = 0;
    tlf.lf.lfCharSet = DEFAULT_CHARSET;
    tlf.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    tlf.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    tlf.lf.lfQuality = DEFAULT_QUALITY;
    tlf.lf.lfHeight = CTadsFont::calc_lfHeight(point_size);
    tlf.lf.lfItalic = (italic != 0);
    tlf.lf.lfUnderline = (underline != 0);
    tlf.lf.lfStrikeOut = 0;
    tlf.superscript = 0;
    tlf.subscript = 0;
    tlf.color = fgcolor;
    tlf.color_is_input = FALSE;
    tlf.color_set_explicitly = use_fgcolor;
    tlf.bgcolor = bgcolor;
    tlf.bgcolor_set = use_bgcolor;
    tlf.lf.lfWeight = (bold ? FW_BOLD : FW_NORMAL);
    tlf.lf.lfPitchAndFamily = (BYTE)attributes;
    tlf.codepage = GetACP();

    /* copy the font name if provided */
    if (font_name == 0)
    {
        tlf.face_set_explicitly = FALSE;
        tlf.lf.lfFaceName[0] = '\0';
    }
    else
    {
        tlf.face_set_explicitly = TRUE;
        strcpy(tlf.lf.lfFaceName, font_name);
    }

    /* get the font */
    return get_font(&tlf, 0, 0);
}


/*
 *   Get a font given a description.  We'll try to find a matching font
 *   in our cache, and return that object; if we don't already have a font
 *   matching this description, we'll create a new one and add it to our
 *   list.
 */
class CTadsFont *CTadsApp::get_font(
    const CTadsLOGFONT *logfont,
    CTadsFont *(*create_func)(const CTadsLOGFONT *),
    int *created)
{
    int i;
    CTadsFont **p;
    CTadsFont *newfont;

    /* run through my list of fonts and see if we can find an exact match */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
    {
        /* if this font matches the description, return it */
        if ((*p)->matches(logfont))
        {
            if (created != 0)
                *created = FALSE;
            return *p;
        }
    }

    /* we'll need to create a new font - ensure the table is big enough */
    if (fonts_allocated_ == fontcount_)
    {
        /* expand the table */
        fonts_allocated_ += 100;
        if (fontlist_ == 0)
            fontlist_ = (CTadsFont **)th_malloc(fonts_allocated_
                                                * sizeof(CTadsFont *));
        else
            fontlist_ = (CTadsFont **)th_realloc(fontlist_,
                fonts_allocated_ * sizeof(CTadsFont *));
    }

    /* create a new font */
    if (created != 0)
        *created = TRUE;
    newfont = (create_func != 0 ? (*create_func)(logfont)
                           : new CTadsFont(logfont));

    /* add it to the table */
    fontlist_[fontcount_++] = newfont;

    /* return it */
    return newfont;
}

/*
 *   Create a bold version of a given font
 */
HFONT CTadsApp::make_bold_font(HFONT font)
{
    LOGFONT lf;
    HDC dc = GetWindowDC(0);

    /* clear out the LOGFONT for starters */
    memset(&lf, 0, sizeof(lf));

    /* select the font */
    HFONT oldfont = (HFONT)SelectObject(dc, font);

    /* get the text metrics for the font */
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);

    /* get the font name */
    GetTextFace(dc, LF_FACESIZE, lf.lfFaceName);

    /* set up the LOGFONT from the TEXTMETRIC */
    lf.lfHeight = tm.tmHeight;
    lf.lfWeight = tm.tmWeight + 300;
    lf.lfCharSet = tm.tmCharSet;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = tm.tmPitchAndFamily;

    /* create the font */
    HFONT bf = CreateFontIndirect(&lf);

    /* clean up */
    SelectObject(dc, oldfont);
    ReleaseDC(0, dc);

    /* return the new font */
    return bf;
}

/*
 *   Set the open-file directory to the directory of the given file
 */
void CTadsApp::set_openfile_dir_or_fname(const char *fname, int is_filename)
{
    size_t fname_len;

    /* if we were given a filename, find the path prefix */
    if (is_filename)
    {
        const char *last_sep;
        const char *p;

        /* find the last path separator in the filename */
        for (p = fname, last_sep = 0 ; *p != 0 ; ++p)
        {
            /* if this is a path separator, remember it */
            if (*p == ':' || *p == '/' || *p == '\\')
                last_sep = p;
        }

        /* see if we found a separator */
        if (last_sep == 0)
        {
            /*
             *   no separators at all - the file is in the current
             *   directory, so there's nothing we need to do
             */
            return;
        }

        /* the length we want is that of the path prefix portion only */
        fname_len = last_sep - fname;
    }
    else
    {
        /* we were given a directory name - use the entire string */
        fname_len = strlen(fname);
    }

    /* if we don't have a buffer yet, allocate one */
    if (open_file_dir_ == 0)
        open_file_dir_ = (char *)th_malloc(256);

    /* save everything up to the last separator */
    memcpy(open_file_dir_, fname, fname_len);
    open_file_dir_[fname_len] = '\0';

    /* tell the OS layer to use this as the open file directory */
    oss_set_open_file_dir(open_file_dir_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the "My Documents" folder path
 */
int CTadsApp::get_my_docs_path(char *fname, size_t fname_len)
{
    IMalloc *imal;
    ITEMIDLIST *idlist;

    /* get a windows shell ITEMIDLIST for the "My Documents" folder */
    if (!SUCCEEDED(SHGetSpecialFolderLocation(0, CSIDL_PERSONAL, &idlist)))
    {
        /* couldn't get the folder location - return an empty path */
        fname[0] = '\0';
        return FALSE;
    }

    /* convert the ID list to a path */
    SHGetPathFromIDList(idlist, fname);

    /* done with the ITEMIDLIST - free it using the shell's allocator */
    SHGetMalloc(&imal);
    imal->Free(idlist);
    imal->Release();

    /* indicate success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   command-to-menu accelerator string entry for accelerator hash table
 */

/*
 *   Link in our list of bound keys.  We keep this in reverse order of
 *   definition, so the first link in the list is the last defined, hence is
 *   the one to show.  We keep the whole list, rather than just the latest,
 *   because a key can be unbound later by an overriding definition, in which
 *   case we'll want to revert to the earlier definition, so we'll want the
 *   earlier definition to remain in the list.
 */
struct AccelHashEntryKey
{
    AccelHashEntryKey(const textchar_t *nm, size_t len)
        : keyname(nm, len)
    {
        nxt = 0;
    }

    /* Match my name.  We match on the whole name or a complete prefix. */
    int match(const textchar_t *nm, size_t len)
    {
        const char *mykey = keyname.get();
        size_t mylen = get_strlen(mykey);
        return (len == mylen || (len > mylen && nm[mylen] == ' '))
            && memicmp(nm, mykey, mylen) == 0;
    }

    AccelHashEntryKey *nxt;
    CStringBuf keyname;
};


/* the hash entry */
class AccelHashEntry: public CHtmlHashEntryUInt
{
public:
    AccelHashEntry(unsigned int cmd, const textchar_t *keyname, size_t keylen)
        : CHtmlHashEntryUInt(cmd, 0)
    {
        /* we have no keys yet */
        keys = 0;

        /* bind our first key */
        bind_key(keyname, keylen);
    }
    ~AccelHashEntry()
    {
        while (keys != 0)
        {
            AccelHashEntryKey *nxt = keys->nxt;
            delete keys;
            keys = nxt;
        }
    }

    /* our linked list of key names */
    AccelHashEntryKey *keys;

    /* get the key name - this returns the latest key name defined */
    const textchar_t *keyname() const
        { return keys != 0 ? keys->keyname.get() : ""; }

    /* bind a key */
    void bind_key(const char *nm, size_t len)
    {
        /* create a new list entry */
        AccelHashEntryKey *k = new AccelHashEntryKey(nm, len);

        /* link it at the head of our list */
        k->nxt = keys;
        keys = k;
    }

    /* unbind a key */
    void unbind_key(const char *nm, size_t len)
    {
        AccelHashEntryKey *cur, *prv, *nxt;

        /* find the entry with this name */
        for (prv = 0, cur = keys ; cur != 0 ; cur = nxt)
        {
            /* get the next entry */
            nxt = cur->nxt;

            /* if this one matches my name, remove it */
            if (cur->match(nm, len))
            {
                /* unlink it */
                if (prv != 0)
                    prv->nxt = cur->nxt;
                else
                    keys = cur->nxt;

                /* delete it */
                delete cur;
            }
            else
            {
                /* we're keeping this one, so it's the next 'prv' */
                prv = cur;
            }
        }
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Accelerator
 */

/* construct */
CTadsAccelerator::CTadsAccelerator()
{
    /* we don't have a statusline yet */
    statusline = 0;

    /* we always have a first-level table, so allocate it */
    tab[0] = new KeyMapTable();

    /*
     *   clear out the rest of the entries for now; we'll allocate them on
     *   demand
     */
    for (int i = 1 ; i < sizeof(tab)/sizeof(tab[0]) ; ++i)
        tab[i] = 0;

    /* initially, incoming keys go to the first-level table */
    reset_keystate();

    /* create the hash table for the current menu accelerator keys */
    menu_hash = new CHtmlHashTable(128, new CHtmlHashFuncUInt());
}

/* destroy */
CTadsAccelerator::~CTadsAccelerator()
{
    /* delete our mapping tables */
    for (int i = 0 ; i < sizeof(tab)/sizeof(tab[0]) ; ++i)
    {
        if (tab[i] != 0)
            delete tab[i];
    }

    /* done with the menu hash table */
    delete menu_hash;
}

/*
 *   enumerate the mapped keys
 */
void CTadsAccelerator::enum_keys(CTadsAccEnum *e)
{
    /*
     *   enumerate commands from the top-level table - that will
     *   automatically pick up all second- and nth-level tables, since we'll
     *   traverse into the subtables as we find keys that point to them
     */
    tab[0]->enum_keys(this, e, "");
}

/*
 *   map a command (single or multi-key) based on the key name string
 */
int CTadsAccelerator::map(const textchar_t *keyname, size_t keylen,
                          unsigned short cmd)
{
    unsigned short subtab;
    const textchar_t *p;
    size_t rem;

    /* skip leading spaces */
    for (p = keyname, rem = keylen ; isspace(*p) ; ++p, --rem) ;

    /* scan the key name until we run out of keys */
    for (p = keyname, subtab = 0 ; ; )
    {
        int shift;
        int vkey = 0;

        /* parse the name */
        if (!CTadsApp::kb_->parse_key_name(&p, &rem, &vkey, &shift))
            return FALSE;

        /* skip spaces after this key to determine if another follows */
        for ( ; rem != 0 && isspace(*p) ; ++p, --rem) ;

        /*
         *   if we found another key after this one, we need to enter the
         *   current key as a prefix key, meaning we map it to a subtable;
         *   otherwise map it to a command
         */
        if (rem != 0)
        {
            /*
             *   another key follows - create a subkey table for the key; if
             *   that fails, the whole mapping fails
             */
            if ((subtab = map_to_subtab(subtab, vkey, shift)) == 0)
                return FALSE;
        }
        else
        {
            /* no more keys follow - map this key to the command */
            map_into_table(subtab, vkey, shift, cmd);

            /* add an entry to our menu accelerator table for the key */
            add_to_menu_hash(keyname, keylen, cmd);

            /* success */
            return TRUE;
        }
    }
}

/*
 *   map a key into the given table
 */
void CTadsAccelerator::map_into_table(
    unsigned short tabidx, int vkey, int shiftkeys, unsigned short cmd)
{
    /* delete the existing mapping for the key */
    del_key(tabidx, vkey, shiftkeys);

    /* set the new command */
    tab[tabidx]->set_cmd(vkey, shiftkeys, cmd);
}

/*
 *   map a key into a table to point to a subtable, allocate a subtable if
 *   necessary
 */
unsigned short CTadsAccelerator::map_to_subtab(
    unsigned short tabidx, int vkey, int shiftkeys)
{
    unsigned short subtab;

    /*
     *   if the key's entry in the table already points to a subtable, simply
     *   keep the existing subtable mapping
     */
    subtab = tab[tabidx]->get_cmd(vkey, shiftkeys);
    if (subtab > 0 && subtab < 100)
        return subtab;

    /*
     *   there's no existing subtable on this key, so we need to allocate a
     *   new subtable - look for a free slot
     */
    for (subtab = 1 ; subtab < 100 && tab[subtab] != 0 ; ++subtab) ;

    /* if there are no free slots, we can't map the key */
    if (subtab >= 100)
        return 0;

    /* create the subtable */
    tab[subtab] = new KeyMapTable();

    /* map the key into the subtable */
    map_into_table(tabidx, vkey, shiftkeys, subtab);

    /* return the new subtable index */
    return subtab;
}

/*
 *   Unbind-for-override callback - we enumerate existing hash entries
 *   through this callback before binding each new key to remove any
 *   overridden entries for the same key and for prefixes used in the key
 */
struct unbind_ctx
{
    unbind_ctx(const textchar_t *k, size_t l) : keyname(k), keylen(l) { }
    const textchar_t *keyname;
    size_t keylen;
};
static void unbind_key_overrides(void *ctx0, CHtmlHashEntry *e)
{
    AccelHashEntry *entry = (AccelHashEntry *)e;
    unbind_ctx *ctx = (unbind_ctx *)ctx0;

    /* unbind this key and its prefixes from this entry */
    entry->unbind_key(ctx->keyname, ctx->keylen);
}

/*
 *   add a key to our accelerator under construction
 */
void CTadsAccelerator::add_to_menu_hash(
    const textchar_t *keyname, size_t keylen, unsigned short cmd)
{
    AccelHashEntry *entry;
    unsigned int icmd = cmd;
    textchar_t keybuf[256];
    const textchar_t *src;
    textchar_t *dst;

    /* convert any "Num0" type sequences to "Num 0" for better readability */
    for (src = keyname, dst = keybuf ;
         src < keyname + keylen && dst < keybuf + sizeof(keybuf) - 1 ; )
    {
        /* check for NumN keys */
        if (keylen >= 4
            && memicmp(src, "Num", 3) == 0
            && (isdigit(src[3]) || strchr("/*-+", src[3]) != 0))
        {
            /* add the "Num" and a space */
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = *src++;
            *dst++ = ' ';
        }

        /* add this character */
        *dst++ = *src++;
    }

    /* null-terminate it and note the new length */
    *dst = '\0';
    keylen = dst - keybuf;

    /* unbind this key from all previous commands */
    unbind_ctx ctx(keybuf, keylen);
    menu_hash->enum_entries(&unbind_key_overrides, &ctx);

    /*
     *   look for an existing entry in the menu accelerator hash table - if
     *   it already exists, update it, since we want to display whichever one
     *   was added last in cases of multiple key bindings for a given command
     */
    entry = (AccelHashEntry *)menu_hash->find(
        (const textchar_t *)&icmd, sizeof(icmd));

    /* if it exists, update it; otherwise create and add a new entry */
    if (entry != 0)
        entry->bind_key(keybuf, keylen);
    else
        menu_hash->add(new AccelHashEntry(cmd, keybuf, keylen));
}


/*
 *   delete a mapping from a table
 */
void CTadsAccelerator::del_key(
    unsigned short table_index, int vkey, int shiftkeys)
{
    KeyMapTable *t = tab[table_index];
    unsigned short subtab;

    /*
     *   if the existing entry points to another table, remove the reference
     *   to the table
     */
    subtab = t->get_cmd(vkey, shiftkeys);
    if (subtab > 0 && subtab < 100)
        delete_subtab(subtab);

    /* clear out the entry */
    t->set_cmd(vkey, shiftkeys, 0);
}

/*
 *   delete a subtable
 */
void CTadsAccelerator::delete_subtab(unsigned short subtab)
{
    KeyMapTable *t = tab[subtab];
    int i, j;

    /* if the table isn't in use, ignore this */
    if (t == 0)
        return;

    /*
     *   delete each command in the table - this will ensure that we delete
     *   any next-level subtables we reference
     */
    for (i = 0 ; i < 256 ; ++i)
        for (j = 0 ; j < 7 ; ++j)
            del_key(subtab, i, j);

    /* delete the table */
    delete t;
    tab[subtab] = 0;
}

/*
 *   reset the key state
 */
void CTadsAccelerator::reset_keystate()
{
    int oldstate = keystate;

    /* go to the initial (first-key) table */
    keystate = 0;

    /* clear out the chord-so-far */
    chord_so_far.set("");

    /*
     *   if we had a partial key, update the status line, to remove the
     *   partial-chord status message
     */
    if (oldstate != 0 && statusline != 0)
        statusline->update();
}

/*
 *   Set the key state
 */
void CTadsAccelerator::set_keystate(int vkey, int shift, int tab_idx)
{
    char buf[128];

    /* remember the new active mapping table */
    keystate = tab_idx;

    /* add this key to the chord list */
    if (CTadsApp::kb_->get_key_name(buf, sizeof(buf), vkey, shift))
    {
        /*
         *   append this key name to the chord, and a space in case there's
         *   another prefix key to come
         */
        chord_so_far.append(buf);
        chord_so_far.append(" ");

        /* update the statusline with the new partial-chord message */
        if (statusline != 0)
            statusline->update();
    }
}


/*
 *   Get a status message showing the chord state, if any
 */
textchar_t *CTadsAccelerator::get_status_message(int *caller_deletes)
{
    /* get the current chord state */
    textchar_t *chord = chord_so_far.get();

    /* if we're in the initial state, we don't have any chord state to show */
    if (keystate == 0 || chord == 0 || chord[0] == '\0')
        return 0;

    /* allocate space for the message - the caller will need to delete it */
    textchar_t *ret = (textchar_t *)th_malloc(get_strlen(chord) + 50);
    *caller_deletes = TRUE;

    /* build the message */
    sprintf(ret, "(%spressed - waiting for next key of chord...)", chord);

    /* return it */
    return ret;
}


/*
 *   Get (but don't execute) a key translation
 */
int CTadsAccelerator::get_translation(CTadsWin *win, MSG *msg)
{
    unsigned int cmd;

    /*
     *   get the command; if it's not translatable, or there's no
     *   translation, indicate this with a FALSE return
     */
    if (!msg_to_command(msg, &cmd) || cmd == 0)
        return FALSE;

    /* check for a next-level key table mapping */
    if (cmd < 100)
    {
        /*
         *   it maps to another key table - return a null message to indicate
         *   that this is an active key but not a command key by itself
         */
        msg->message = WM_NULL;
        msg->hwnd = win->get_handle();
        msg->wParam = 0;
        msg->lParam = 0;
        return TRUE;
    }

    /* test the command to make sure it's enabled */
    return test_command(win, cmd, msg);
}

/*
 *   Translate a message
 */
int CTadsAccelerator::translate(CTadsWin *win, MSG *msg)
{
    unsigned int cmd;

    /*
     *   get the command translation, if any; if it's not translatable,
     *   ignore it
     */
    if (!msg_to_command(msg, &cmd))
        return FALSE;

    /*
     *   If the command is zero, it means there's no mapping, hence no
     *   accelerator translation.  In this case, check for a system
     *   accelerator.
     */
    if (cmd == 0)
    {
        /*
         *   If we're not in the initial key state, we were waiting for the
         *   second (or nth) key of a multi-key "chord"; the user pressed a
         *   key that doesn't combine with the earlier keys to make a valid
         *   chord.  In this case, eat the new key by considering it
         *   translated, even though it doesn't have a translation.
         */
        if (keystate != 0)
        {
            /* reset to the initial key state */
            reset_keystate();

            /*
             *   consider the key translated, even though we're not carrying
             *   out a command - it effectively translates to a null command,
             *   since it doesn't make a valid chord
             */
            return TRUE;
        }

        /*
         *   as a last resort, check for a system accelerator by attempting
         *   to translate the key through an empty native accelerator table
         */
        return TranslateAccelerator(
            win->get_handle(), CTadsApp::get_app()->get_empty_accel(), msg);
    }

    /*
     *   if the command is from 1 to 99, this is a prefix key, and the
     *   command is actually an index to the next-level key table
     */
    if (cmd < 100)
    {
        /*
         *   if the specified next-level key table exists, set the new key
         *   state to that table - this will let us interpret the next key
         *   event as the next key in this "chord"
         */
        if (tab[cmd] != 0)
        {
            /* get the key and shift states */
            int vkey = (int)msg->wParam;
            int shift = (CTadsWin::get_shift_key() ? CTKB_SHIFT : 0)
                        | (CTadsWin::get_ctl_key() ? CTKB_CTRL : 0)
                        | ((msg->lParam & (1 << 29)) ? CTKB_ALT : 0);

            /* set the indicated next-level key table */
            set_keystate(vkey, shift, cmd);
        }
        else
        {
            /*
             *   This key is not mapped - reset the key state.  Note that we
             *   treat this key as translated even though it doesn't do
             *   anything, since it *is* mapped as an accelerator - it's just
             *   that the accelerator doesn't happen to be valid.
             */
            reset_keystate();
        }

        /* in any case, this key has been handled */
        return TRUE;
    }

    /*
     *   Otherwise, it's a WM_COMMAND or WM_SYSCOMMAND mapping - reset the
     *   key state (since a key that translates to a command is necessarily
     *   the last key of its chord), send the command message, and indicate
     *   that we found a translation.
     */
    reset_keystate();
    send_command(win, cmd);
    return TRUE;
}

/*
 *   Get the command translation for a given message.  Returns true if it's a
 *   translatable message, false if not.
 */
int CTadsAccelerator::msg_to_command(const MSG *msg, unsigned int *cmd)
{
    int vkey = (int)msg->wParam;

    /*
     *   if it's not a WM_KEYDOWN or WM_SYSKEYDOWN, it's not for us to
     *   translate
     */
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN)
        return FALSE;

    /*
     *   ignore shift keys, mouse keys, and others that don't contribute to
     *   key chords
     */
    switch (vkey)
    {
    case VK_LBUTTON:
    case VK_RBUTTON:
    case VK_MBUTTON:
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_CAPITAL:
    case VK_LWIN:
    case VK_RWIN:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
        return FALSE;
    }

    /* get the shift state */
    int shift = (CTadsWin::get_shift_key() ? CTKB_SHIFT : 0)
                | (CTadsWin::get_ctl_key() ? CTKB_CTRL : 0)
                | ((msg->lParam & (1 << 29)) ? CTKB_ALT : 0);

    /* look up the key in the current table, and return the result */
    *cmd = tab[keystate]->get_cmd(vkey, shift);

    /* it's a translatable message (whether or not it has a translation) */
    return TRUE;
}


/*
 *   Send a translated accelerator command to a window
 */
void CTadsAccelerator::send_command(CTadsWin *win, unsigned int cmd)
{
    MSG msg;

    /* test it; if it's enabled, send it */
    if (test_command(win, cmd, &msg))
        SendMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
}

/*
 *   Test a translated accelerator message to see if we can send it; if so,
 *   we'll fill in *msgp with the message to send and return true, otherwise
 *   we'll return false.
 */
int CTadsAccelerator::test_command(CTadsWin *win, unsigned int cmd, MSG *msg)
{
    UINT msgid;
    HMENU menu, popup;
    int popup_index;
    MENUITEMINFO mii;
    HWND hwnd = win->get_handle();

    /*
     *   Determine whether this is a regular command or a system-menu
     *   command.  If the command is defined on the Window's system menu,
     *   it's a system command; otherwise it's a regular command.
     */
    menu = popup = GetSystemMenu(hwnd, FALSE);
    popup_index = 0;
    mii.cbSize = CTadsWin::menuiteminfo_size_;
    mii.fMask = MIIM_STATE;
    if (GetMenuItemInfo(menu, cmd, FALSE, &mii))
    {
        /* found it on the system menu - it's a system command */
        msgid = WM_SYSCOMMAND;
    }
    else
    {
        /* it's a regular command */
        msgid = WM_COMMAND;

        /* use the regular menu to handle it */
        menu = win->get_win_menu();

        /* find the popup containing the command, if any */
        popup = find_menu_command(&popup_index, menu, cmd);
    }

    /*
     *   if the window is minimized, and this command appears on the window's
     *   main menu (NOT the system menu, and NOT on no menu at all), do not
     *   send the WM_COMMAND, in keeping with TranslateAccelerator()
     */
    if (IsIconic(hwnd) && menu != 0 && msgid == WM_COMMAND)
        return FALSE;

    /*
     *   If we found the menu, send the menu initialization messages.  Do
     *   this using the same rules that the native TranslateAccelerator()
     *   routine uses: send these only if (a) the window is enabled, AND (b)
     *   the menu is the system menu OR the window isn't minimized, AND (c)
     *   mouse capture isn't in effect.
     */
    if (menu != 0 && popup != 0
        && IsWindowEnabled(hwnd)
        && (msgid == WM_SYSCOMMAND || !IsIconic(hwnd))
        && GetCapture() == 0)
    {
        /* send WM_INITMENU to initialize the overall menu */
        SendMessage(hwnd, WM_INITMENU, (WPARAM)menu, 0);

        /* send WM_INITMENUPOPUP to initialize the popup menu */
        SendMessage(hwnd, WM_INITMENUPOPUP, (WPARAM)popup,
                    MAKELPARAM(popup_index, msgid == WM_SYSCOMMAND));
    }

    /*
     *   Get the menu item's state: if it's grayed or disabled, the
     *   corresponding accelerator command is disabled - do not send the
     *   command.
     */
    mii.cbSize = CTadsWin::menuiteminfo_size_;
    mii.fMask = MIIM_STATE;
    if (popup != 0
        && GetMenuItemInfo(popup, cmd, FALSE, &mii)
        && (mii.fState & (MFS_DISABLED | MFS_GRAYED)) != 0)
    {
        /* it's grayed/disabled - do not send the command */
        return FALSE;
    }

    /*
     *   It's a valid and enabled command message.  Set up the MSG structure
     *   with the message to send.  Note the high word of the WPARAM must be
     *   1 to indicate that this is an accelerator.
     */
    msg->hwnd = hwnd;
    msg->message = msgid;
    msg->wParam = MAKEWPARAM(cmd, 1);
    msg->lParam = 0;

    /* indicate that it's a valid message */
    return TRUE;
}

/*
 *   main implementation of find_menu_command()
 */
static HMENU find_menu_command_main(int *index, HMENU menu, unsigned int cmd)
{
    int i, cnt;
    HMENU submenu;

    /* first, scan this menu for a match to the command ID */
    for (i = 0, cnt = GetMenuItemCount(menu) ; i < cnt ; ++i)
    {
        unsigned int id;

        /* get this item's ID */
        id = GetMenuItemID(menu, i);

        /*
         *   if it matches our target command, return this menu; if the ID
         *   -1, it means that the menu pops up a submenu, so search
         *   recursively in the submenu
         */
        if (id == cmd)
        {
            /* this is the one we're looking for */
            return menu;
        }
        else if (id == -1)
        {
            /* assume we'll find it here */
            *index = i;

            /* this item has a submenu - search it recursively */
            if ((submenu = find_menu_command_main(
                index, GetSubMenu(menu, i), cmd)) != 0)
                return submenu;
        }
    }

    /* didn't find it */
    return 0;
}

/*
 *   find a command in a menu, recursively searching submenus
 */
HMENU CTadsAccelerator::find_menu_command(int *index, HMENU menu,
                                          unsigned int cmd)
{
    /* assume we won't find it on a submenu */
    *index = 0;

    /* find it */
    return find_menu_command_main(index, menu, cmd);
}

/*
 *   Initialize a menu.  We'll scan the menu for ordinary string items, and
 *   update each one's displayed accelerator key with the current accelerator
 *   key for its command, if any.  This ensures that menus are always
 *   up-to-date with the current keyboard bindings.
 */
void CTadsAccelerator::init_menu_popup(HMENU menu, unsigned int pos,
                                       int sysmenu)
{
    int i, cnt;

    /* ignore the system menu */
    if (sysmenu)
        return;

    /* scan the items */
    for (i = 0, cnt = GetMenuItemCount(menu) ; i < cnt ; ++i)
    {
        MENUITEMINFO mii;
        char buf[256];
        char *p;
        const textchar_t *key;
        AccelHashEntry *entry;
        unsigned int icmd;

        /* get the item's information */
        memset(&mii, 0, sizeof(mii));
        mii.cbSize = CTadsWin::menuiteminfo_size_;
        mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID;
        mii.dwTypeData = buf;
        mii.cch = sizeof(buf);
        if (!GetMenuItemInfo(menu, i, TRUE, &mii))
            continue;

        /* if it's not a regular string item, ignore it */
        if ((mii.fType & (MFT_STRING | MFT_BITMAP | MFT_SEPARATOR))
            != MFT_STRING)
            continue;

        /*
         *   find the current accelerator, if any; if there isn't one, set up
         *   at the end of the string
         */
        if ((p = strchr(buf, '\t')) == 0)
            p = buf + strlen(buf);

        /* find the command in our accelerator table */
        icmd = mii.wID;
        entry = (AccelHashEntry *)menu_hash->find(
            (const textchar_t *)&icmd, sizeof(icmd));

        /*
         *   if there's no entry, there's no accelerator for this key;
         *   otherwise, use the accelerator from the entry
         */
        key = (entry != 0 ? entry->keyname() : "");

        /* check to see if we need to change the string */
        if (strcmp(key, p) != 0)
        {
            /* add the new accelerator */
            *p++ = '\t';
            strcpy(p, key);

            /* set the menu title */
            mii.fMask = MIIM_STRING;
            SetMenuItemInfo(menu, i, TRUE, &mii);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Accelerator key map table implementation
 */

void KeyMapTable::enum_keys(CTadsAccelerator *acc, CTadsAccEnum *e,
                            const textchar_t *prefix)
{
    int i;

    /* enumerate the keys in each row */
    for (i = 0 ; i < sizeof(row)/sizeof(row[0]) ; ++i)
        row[i].enum_keys(acc, e, prefix, i);
}


/* ------------------------------------------------------------------------ */
/*
 *   Key map row implementation
 */

void KeyMapRow::enum_keys(CTadsAccelerator *acc, CTadsAccEnum *e,
                          const textchar_t *prefix, int vkey)
{
    textchar_t *buf;
    size_t prefixlen;
    const size_t minlen = 50;
    int i;

    /*
     *   build the prefix string: start with the prefix, and add a space if
     *   there's anything there
     */
    if (prefix != 0 && prefix[0] != '\0')
    {
        /* allocate a buffer big enough for the prefix plus any key name */
        prefixlen = get_strlen(prefix);
        buf = new textchar_t[prefixlen + minlen];

        /* copy the prefix string and add a space after it */
        memcpy(buf, prefix, prefixlen);
        buf[prefixlen++] = ' ';
    }
    else
    {
        /* there's no prefix - allocate a buffer big enough for one key */
        prefixlen = 0;
        buf = new textchar_t[minlen];
    }

    /* process each key in this row */
    for (i = 0 ; i < sizeof(cmd)/sizeof(cmd[0]) ; ++i)
    {
        unsigned short c = cmd[i];
        KeyMapTable *subtab;

        /*
         *   Determine if this key is mapped.  If the command code is zero,
         *   the key simply isn't mapped.  If the command code is from 1 to
         *   99, it's a prefix key, and the command code is actually the
         *   index of the subtable.  Otherwise, it's mapped to the command
         *   given by the command code.
         */
        if (c >= 1 && c <= 99 && (subtab = acc->get_subtable(c)) != 0)
        {
            /*
             *   It's mapped to a subtable.  Add this key to the prefix, then
             *   enumerate the keys in the subtable.
             */
            CTadsApp::kb_->get_key_name(buf + prefixlen, minlen, vkey, i);
            subtab->enum_keys(acc, e, buf);
        }
        else if (c >= 100)
        {
            /*
             *   this key is mapped to an actual command - get the key name,
             *   and send it to the enumerator for processing
             */
            CTadsApp::kb_->get_key_name(buf + prefixlen, minlen, vkey, i);
            e->do_key(buf, c);
        }
    }

    /* we're done with our key name buffer */
    delete [] buf;
}

