/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32help.cpp - Workbench help/doc viewer tool window
Function

Notes

Modified
  12/29/06 MJRoberts  - Creation
*/

#include <Windows.h>
#include <exdisp.h>
#include <oleauto.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <MsHtmcid.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "w32tdb.h"
#include "w32help.h"
#include "htmlw32.h"
#include "htmldbg.h"
#include "htmldcfg.h"
#include "tadscom.h"
#include "tadswebctl.h"


/* ------------------------------------------------------------------------ */
/*
 *   The CHtmlSys_dbghelpwin window class.  This is the Workbench window
 *   object; it's a dockable window, so the actual system window lives inside
 *   another system window (which is an MDI child or a docking window,
 *   depending on the current docking state).
 */

/*
 *   construction
 */
CHtmlSys_dbghelpwin::CHtmlSys_dbghelpwin(
    class CHtmlPreferences *prefs,
    class CHtmlDebugHelper *helper,
    class CHtmlSys_dbgmain *dbgmain)
{
    /* remember our prefs, helper, and main window objects */
    prefs_ = prefs;
    helper_ = helper;
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();
}

/*
 *   deletion
 */
CHtmlSys_dbghelpwin::~CHtmlSys_dbghelpwin()
{
}

/*
 *   system window creation
 */
void CHtmlSys_dbghelpwin::do_create()
{
    /* inherit the default handling */
    CTadsWin::do_create();

    /* create our embedded browser control */
    embed_browser(TRUE, handle_, IDR_HELP_POPUP_MENU, 0, 0);

    /* set up a timer to watch for focus changes */
    focus_timer_ = alloc_timer_id();
    SetTimer(handle_, focus_timer_, 500, 0);

    /* assume we don't have focus */
    has_focus_ = FALSE;
}

/*
 *   system window destruction
 */
void CHtmlSys_dbghelpwin::do_destroy()
{
    /* delete our browser control */
    unembed_browser();

    /* notify the debugger interface */
    dbgmain_->dbghostifc_on_destroy(this, this);

    /* tell the debugger we're closing an active window */
    dbgmain_->on_close_dbg_win(this);

    /* release the interface */
    dbgmain_->Release();

    /* delete our timer */
    KillTimer(handle_, focus_timer_);
    free_timer_id(focus_timer_);

    /* do the inherited work */
    CTadsWin::do_destroy();
}

/*
 *   check command status
 */
TadsCmdStat_t CHtmlSys_dbghelpwin::check_command(const check_cmd_info *info)
{
    /* route it to the main window */
    return dbgmain_->check_command(info);
}

/*
 *   execute a command
 */
int CHtmlSys_dbghelpwin::do_command(int notify_code, int command_id, HWND ctl)
{
    /* route it to the main window */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   translate a Workbench ID_xxx command to an OLE command ID
 */
static DWORD get_olecmd(int command_id, OLECMDEXECOPT *optp)
{
    OLECMDEXECOPT opt = OLECMDEXECOPT_DONTPROMPTUSER;
    DWORD oleid = IDM_UNKNOWN;

    switch (command_id)
    {
    case ID_HELP_GOBACK:
        oleid = IDM_GOBACKWARD;
        break;

    case ID_HELP_GOFORWARD:
        oleid = IDM_GOFORWARD;
        break;

    case ID_HELP_REFRESH:
        oleid = IDM_REFRESH;
        break;

    case ID_EDIT_COPY:
        oleid = IDM_COPY;
        break;

    case ID_EDIT_SELECTALL:
        oleid = IDM_SELECTALL;
        break;

    case ID_EDIT_FIND:
        oleid = IDM_FIND;
        opt = OLECMDEXECOPT_PROMPTUSER;
        break;

    case ID_FILE_PRINT:
        oleid = IDM_PRINT;
        opt = OLECMDEXECOPT_PROMPTUSER;
        break;
    }

    /* return the options, if desired */
    if (optp != 0)
        *optp = opt;

    /* return the OLE command ID */
    return oleid;
}

/*
 *   check command status
 */
TadsCmdStat_t CHtmlSys_dbghelpwin::active_check_command(
    const check_cmd_info *info)
{
    OLECMD olecmd;

    /* check for our own commands before any OLE translation */
    switch (info->command_id)
    {
    case ID_EDIT_FIND:
        /* update the menu to remove any past filename indication */
        check_command_change_menu(info, IDS_TPL_FIND_BLANK);
        break;

    case ID_FILE_PAGE_SETUP:
        /*
         *   don't allow this - we don't have a way to pass the page setup
         *   information to the browser control, so make it clear that our
         *   page setup settings don't apply to the Help/Doc window by
         *   disabling the dialog entirely
         */
        return TADSCMD_DISABLED;
    }

    /* check for a translation to an OLE command */
    if (icmd_ != 0
        && (olecmd.cmdID = get_olecmd(info->command_id, 0)) != IDM_UNKNOWN
        && SUCCEEDED(icmd_->QueryStatus(&CGID_MSHTML, 1, &olecmd, 0)))
    {
        /* return the OLE command status */
        if ((olecmd.cmdf & OLECMDF_LATCHED) != 0)
            return ((olecmd.cmdf & OLECMDF_ENABLED) != 0
                    ? TADSCMD_CHECKED : TADSCMD_DISABLED_CHECKED);
        else
            return ((olecmd.cmdf & OLECMDF_ENABLED) != 0
                    ? TADSCMD_ENABLED : TADSCMD_DISABLED);
    }

    /* inherit base class behavior for other commands */
    return CTadsWin::check_command(info);
}

int CHtmlSys_dbghelpwin::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    DWORD olecmd;
    OLECMDEXECOPT oleopt;

    /* check for translations to OLECMDID codes */
    if (icmd_ != 0
        && (olecmd = get_olecmd(command_id, &oleopt)) != IDM_UNKNOWN)
    {
        /* send the OLE command */
        icmd_->Exec(&CGID_MSHTML, olecmd, oleopt, 0, 0);

        /* handled */
        return TRUE;
    }

    /* inherit base class behavior for other commands */
    return CTadsWin::do_command(notify_code, command_id, ctl);
}

/*
 *   handle a non-client left click
 */
int CHtmlSys_dbghelpwin::do_nc_leftbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* if it's a title-bar hit, track the context menu */
    if (hit_type == HTCAPTION)
    {
        /*
         *   if focus isn't within our window, move focus here - this makes
         *   it easy to move focus back here from a docked window
         */
        HWND f = GetFocus();
        if (f != handle_ && !IsChild(handle_, f))
            SetFocus(handle_);
    }

    /* proceed to the standard system handling */
    return FALSE;
}


/*
 *   handle a non-client right-click
 */
int CHtmlSys_dbghelpwin::do_nc_rightbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* if it's a title-bar hit, track the context menu */
    if (hit_type == HTCAPTION)
    {
        POINT pt;

        /* convert the location to client coordinates */
        pt.x = x;
        pt.y = y;
        ScreenToClient(handle_, &pt);

        /* show the system menu as a context menu */
        track_system_context_menu(pt.x, pt.y);

        /* handled */
        return TRUE;
    }

    /* we didn't handle it */
    return FALSE;
}

/*
 *   move
 */
void CHtmlSys_dbghelpwin::do_move(int x, int y)
{
    /* remember the change in the preferences after the move has completed */
    PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
}

/*
 *   resize
 */
void CHtmlSys_dbghelpwin::do_resize(int mode, int x, int y)
{
    HWND active;
    int is_active, maxed;

    /*
     *   If this is a "restore," and the active MDI window is currently
     *   maximized, don't resize the control.  When switching away from a
     *   maximized MDI child to a different child, Windows restores the
     *   outgoing child to its normal size.  If we switch back to the current
     *   window later, while still in MDI-maximized mode, we will have done
     *   two resizes on this window.  These resizes effectively cancel each
     *   other out, but the WebBrowser control tends to move around the
     *   scroll position on resize in a non-reversible way, such that a
     *   series of resizes that takes the window back to its original
     *   position still changes the scrolling position from where we started.
     *   This is undesirable in MDI-max mode, since merely switching away
     *   from this window for a moment and then switching back can cause the
     *   contents to scroll.
     */
    active = (HWND)SendMessage(dbgmain_->get_parent_of_children(),
                               WM_MDIGETACTIVE, 0, (LPARAM)&maxed);
    is_active = (active == handle_ || IsChild(active, handle_));
    if (!maxed || is_active)
        resize_browser_ctl(-1, -1, x, y);

    /* remember the change in the preferences after the move has completed */
    PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
}

/*
 *   handle a user message
 */
int CHtmlSys_dbghelpwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see what we have */
    switch (msg)
    {
    case HTMLM_SAVE_WIN_POS:
        /* do the deferred window position saving */
        store_winpos_prefs();
        return TRUE;
    }

    /* inherit the default handling */
    return CTadsWin::do_user_message(msg, wpar, lpar);
}

/*
 *   Store my position in the preferences.  This is invoked from our
 *   self-posted follow-up message, which we post during a size/move event
 *   for deferred processing after the triggering event has finished.
 */
void CHtmlSys_dbghelpwin::store_winpos_prefs()
{
    RECT rc;
    CHtmlRect pos;
    HWND hwnd, mdihwnd;
    CHtmlDebugConfig *lc;

    /* get my frame position */
    hwnd = get_frame_pos(&rc);

    /* ignore this if we're not the active MDI window, or we're maximized */
    mdihwnd = dbgmain_->get_active_mdi_child();
    if ((mdihwnd != hwnd && !IsChild(mdihwnd, hwnd)) || is_win_maximized())
        return;

    /* convert to our local position structure */
    pos.set(rc.left, rc.top, rc.right, rc.bottom);

    /* store the position under our key */
    if ((lc = dbgmain_->get_local_config()) != 0)
        lc->setval("help win pos", 0, &pos);
}

/*
 *   handle notifications
 */
int CHtmlSys_dbghelpwin::do_notify(
    int control_id, int notify_code, LPNMHDR nm)
{
    int new_focus;

    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* check which it is */
        new_focus = (notify_code == NM_SETFOCUS);

        /* notify myself of an artificial activation */
        do_parent_activate(new_focus);

        /* note the new status */
        has_focus_ = new_focus;

        /* inherit the default handling as well */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CTadsWin::do_notify(control_id, notify_code, nm);
}

/*
 *   handle non-client activation
 */
int CHtmlSys_dbghelpwin::do_ncactivate(int flag)
{
    /* set active/inactive with the main window */
    if (dbgmain_ != 0)
        dbgmain_->notify_active_dbg_win(this, flag);

    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent
 */
void CHtmlSys_dbghelpwin::do_parent_activate(int flag)
{
    /* set active/inactive with the main window */
    if (dbgmain_ != 0)
        dbgmain_->notify_active_dbg_win(this, flag);

    /* note the new status */
    has_focus_ = flag;

    /* inherit default handling */
    CTadsWin::do_parent_activate(flag);
}

/*
 *   gain focus
 */
void CHtmlSys_dbghelpwin::do_setfocus(HWND prv)
{
    IDispatch *doc;

    /* inherit default */
    CTadsWin::do_setfocus(prv);

    /* set active/inactive with the main window */
    if (dbgmain_ != 0)
        dbgmain_->notify_active_dbg_win(this, TRUE);

    /* set focus on the html document */
    if (iwb2_ != 0
        && SUCCEEDED(iwb2_->get_Document(&doc))
        && doc != 0)
    {
        IHTMLDocument4 *doc4;

        /* get the IHtmlDocument4 interface from the document */
        if (SUCCEEDED(doc->QueryInterface(
            IID_IHTMLDocument4, (void **)&doc4)))
        {
            /* set focus on the document */
            doc4->focus();

            /* done with the doc4 */
            doc4->Release();
        }

        /* done with the document */
        doc->Release();
    }

    /* note the new status */
    has_focus_ = TRUE;
}

/*
 *   lose focus
 */
void CHtmlSys_dbghelpwin::do_killfocus(HWND nxt)
{
    /* set active/inactive with the main window */
    if (dbgmain_ != 0)
        dbgmain_->notify_active_dbg_win(this, FALSE);

    /* note the new status */
    has_focus_ = FALSE;

    /* inherit default */
    CTadsWin::do_killfocus(nxt);
}

/*
 *   handle timers
 */
int CHtmlSys_dbghelpwin::do_timer(int timer_id)
{
    /* check for our focus timer */
    if (timer_id == focus_timer_)
    {
        int new_focus;

        /* check to see if focus is in this window or a child */
        HWND f = GetFocus();
        new_focus = (f == handle_ || IsChild(handle_, f));

        /* check to see if this is changed from last tim e*/
        if (new_focus != has_focus_)
        {
            if (new_focus)
                do_setfocus(0);
            else
                do_killfocus(0);
        }
    }

    /* inherit default */
    return CTadsWin::do_timer(timer_id);
}

