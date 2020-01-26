#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsdock.cpp,v 1.2 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsdock.cpp - TADS docking window
Function

Notes

Modified
  05/22/99 MJRoberts  - Creation
*/

#include <Windows.h>
#include <commctrl.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSDOCK_H
#include "tadsdock.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   statics
 */

/* docking port registry list head */
CTadsDockport *CTadsWinDock::ports_ = 0;

/* default docking port */
CTadsDockport *CTadsWinDock::default_port_ = 0;

/* default MDI parent, for converting windows to MDI */
CTadsWin *CTadsWinDock::default_mdi_parent_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Instantiate
 */
CTadsWinDock::CTadsWinDock(CTadsWin *subwin,
                           int docked_width, int docked_height,
                           tads_dock_align docked_align,
                           CTadsDockModelWinSpec model_spec)
{
    CTadsDockModelWin *model_win;
    HBITMAP hbmp;

    /*
     *   if the model window was specified by guid, create an actual model
     *   window object now; otherwise, just use the model window given
     */
    model_win = (model_spec.model_win_ != 0
                 ? model_spec.model_win_
                 : new CTadsDockModelWin(model_spec));

    /* remember my subwindow */
    subwin_ = subwin;

    /* remember my model window, and add a reference to it */
    model_win_ = model_win;
    model_win_->add_ref();

    /* tell the model window about me */
    model_win_->win_ = this;

    /* remember the default docked width, height, and alignment */
    docked_width_ = docked_width;
    docked_height_ = docked_height;
    docked_align_ = docked_align;

    /* initialize flags */
    in_title_drag_ = FALSE;
    dock_on_unhide_ = FALSE;
    mdi_on_unhide_ = FALSE;

    /* create the pattern brush for drawing the drag rectangle */
    hbmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                      MAKEINTRESOURCE(IDB_BMP_PAT50));
    hbrush_ = CreatePatternBrush(hbmp);
    DeleteObject(hbmp);
}

/*
 *   Delete
 */
CTadsWinDock::~CTadsWinDock()
{
    /* delete my brush */
    if (hbrush_ != 0)
        DeleteObject(hbrush_);

    /* delete my context menu */
    if (ctx_menu_ != 0)
        DestroyMenu(ctx_menu_);

    /* if I have a model window, delete it */
    if (model_win_ != 0)
    {
        /* tell the model window to forget about me */
        if (model_win_->win_ == this)
            model_win_->forget_window();

        /* remove our reference from the model window */
        model_win_->remove_ref();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set our model window
 */
void CTadsWinDock::set_model_win(CTadsDockModelWin *win)
{
    /* remember our old model window for a moment */
    CTadsDockModelWin *oldwin = model_win_;

    /* store the new model window, and add a reference */
    if ((model_win_ = win) != 0)
        win->add_ref();

    /* remove the reference from the old window */
    if (oldwin != 0)
        oldwin->remove_ref();

    /* tell the new model window that I'm its actual window */
    if (win != 0)
        win->win_ = this;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a docking window.
 */
void CTadsWinDock::do_create()
{
    /* inherit default processing */
    CTadsWin::do_create();

    /* load my context menu */
    ctx_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(get_ctx_menu_id()));
    ctx_menu_popup_ = GetSubMenu(ctx_menu_, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Close a docking window.
 */
int CTadsWinDock::do_close()
{
    HWND mdipar = default_mdi_parent_->get_parent_of_children();
    int need_restore = FALSE;

    /*
     *   if I don't have a subwindow, let the default processing really
     *   close the window
     */
    if (subwin_ == 0)
        return TRUE;

    /*
     *   If we're in MDI mode, and we're the active MDI window, explicitly
     *   switch to the next MDI window.  This ensures that we go through the
     *   proper activation procedure for the next window, which doesn't
     *   happen on merely hiding the active window.
     */
    if (!is_win_dockable())
    {
        HWND act;
        BOOL maxed;

        /* check to see if we're the active window to start with */
        act = (HWND)SendMessage(mdipar, WM_MDIGETACTIVE, 0, (LPARAM)&maxed);
        if (act == handle_)
        {
            /* switch to the next window */
            SendMessage(mdipar, WM_MDINEXT, 0, 0);

            /*
             *   If that leaves us active, and we're maximized, note that we
             *   need to un-maximize after hiding.  MDI can't deal with a
             *   maximized hidden window, and this gives us behavior
             *   consistent with normal MDI behavior anyway, as the client
             *   area's 'maximized' status doesn't survive when no windows
             *   are open.
             */
            act = (HWND)SendMessage(
                mdipar, WM_MDIGETACTIVE, 0, (LPARAM)&maxed);
            need_restore = (act == handle_ && maxed);
        }
    }

    /* on closing a docking window, we merely hide it */
    ShowWindow(handle_, SW_HIDE);

    /* if we need to unmaximize, do so now */
    if (need_restore)
    {
        /* restore the window */
        SendMessage(mdipar, WM_MDIRESTORE, (WPARAM)handle_, 0);

        /*
         *   hide it again - restoring the single MDI window shows it, which
         *   seems a bit buggy in MDI, but what can you do
         */
        ShowWindow(handle_, SW_HIDE);
    }

    /* tell the caller not to proceed with actual closing */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Destroy a docking window.
 */
void CTadsWinDock::do_destroy()
{
    /* inherit default */
    CTadsWin::do_destroy();
}


/* ------------------------------------------------------------------------ */
/*
 *   Reparent my subwindow
 */
void CTadsWinDock::reparent_subwin(CTadsWinDock *new_parent)
{
    RECT rc;
    CTadsWin *subwin;

    /* remember the subwindow, since we're going to unlink from it */
    subwin = subwin_;

    /*
     *   give my model window to the new parent, then forget about it,
     *   since it no longer belongs to me
     */
    new_parent->set_model_win(model_win_);
    set_model_win(0);

    /* set the subwindow in the new parent */
    new_parent->set_subwin(subwin);

    /* if there's a subwindow, set its internal parent object */
    if (subwin != 0)
        subwin->set_parent(new_parent);

    /* unlink from my subwindow - it belongs to the other parent now */
    subwin_ = 0;

    /* move the Windows handle to the new parent */
    if (subwin != 0 && subwin->get_handle() != 0)
        SetParent(subwin->get_handle(), new_parent->get_handle());

    /*
     *   Make sure we resize the window's contents for its new parent.  If
     *   the parent was created with its final size, there won't be any
     *   WM_SIZE messages until the parent is manually resized; ensure
     *   that the new parent has a chance to fix up the client by sending
     *   it a WM_SIZE immediately.
     */
    GetClientRect(new_parent->get_handle(), &rc);
    SendMessage(new_parent->get_handle(), WM_SIZE,
                (WPARAM)(IsZoomed(new_parent->get_handle())
                         ? SIZE_MAXIMIZED : SIZE_RESTORED),
                MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

    /* adjust for loss of our subwindow */
    adjust_for_no_subwin();
}

/* ------------------------------------------------------------------------ */
/*
 *   Show our context menu
 */
void CTadsWinDock::show_ctx_menu(int x, int y)
{
    track_context_menu(ctx_menu_popup_, x, y);
}

/* ------------------------------------------------------------------------ */
/*
 *   process commands
 */
int CTadsWinDock::do_command(int notify_code, int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_DOCKWIN_HIDE:
        /* hide the window */
        do_close();
        return TRUE;

    case ID_DOCKWIN_DOCK:
        /* auto-dock */
        auto_dock();
        return TRUE;

    case ID_DOCKWIN_DOCKVIEW:
        /* convert to an MDI view if possible */
        cvt_to_mdi(TRUE);
        return TRUE;

    default:
        /* not handled; inherit default */
        return CTadsWin::do_command(notify_code, command_id, ctl);
    }
}

/*
 *   command status
 */
TadsCmdStat_t CTadsWinDock::check_command(const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_DOCKWIN_DOCK:
        /* enable this as long as we have a default dock port */
        return (default_port_ != 0 ? TADSCMD_DEFAULT : TADSCMD_DISABLED);

    case ID_DOCKWIN_HIDE:
        /* enabled as long as we have a closebox */
        return (has_closebox() ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DOCKWIN_UNDOCK:
        /* we're already undocked */
        return TADSCMD_DISABLED;

    case ID_DOCKWIN_DOCKVIEW:
        /*
         *   This is a docking view, so checkmark this item.  If we have a
         *   default MDI parent for converting to an MDI view, enable this
         *   item, otherwise disable it.
         */
        return (default_mdi_parent_ != 0
                ? TADSCMD_CHECKED : TADSCMD_DISABLED_CHECKED);

    default:
        /* not handled; inherit default */
        return CTadsWin::check_command(info);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   show the window
 */
int CTadsWinDock::do_showwindow(int show, int status)
{
    /*
     *   if I'm being explicitly un-hidden (status == 0), and we're set to
     *   dock or convert to MDI on being shown, make the change
     */
    if (show && status == 0)
    {
        RECT rc;

        if (dock_on_unhide_)
        {
            /* auto-dock it */
            auto_dock();
        }
        else if (mdi_on_unhide_)
        {
            /* convert to an MDI window and show the result */
            cvt_to_mdi(TRUE);
        }
        else
        {
            /*
             *   no special handling is required - tell the caller to
             *   proceed with default handling
             */
            return FALSE;
        }

        /*
         *   The window is going to be shown; there's nothing we can do
         *   about that.  To avoid flashing, move the window off the
         *   screen.
         */
        GetWindowRect(GetDesktopWindow(), &rc);
        MoveWindow(handle_, rc.right, rc.bottom, 10, 10, FALSE);

        /* indicate that the message was handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a user message
 */
int CTadsWinDock::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* check the message */
    switch(msg)
    {
    case DOCKM_DESTROY:
        /* destroy myself */
        DestroyWindow(handle_);
        return TRUE;

    default:
        /* it's not one of ours */
        return CTadsWin::do_user_message(msg, wpar, lpar);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Adjust for removal of our subwindow.  For an undocked window, when
 *   the subwindow is removed, the containing window is no longer needed,
 *   so we can simply destroy it.
 */
void CTadsWinDock::adjust_for_no_subwin()
{
    /*
     *   This window is no longer needed, since its contents are now
     *   docked in another window.  Rather than destroying myself
     *   immediately, post a close message to myself; this will give us a
     *   chance to unwind the stack from any Windows message we're
     *   currently processing.
     */
    PostMessage(handle_, WM_CLOSE, 0, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   set focus - transfer focus to the subwindow
 */
void CTadsWinDock::do_setfocus(HWND /*previous*/)
{
    /* if we're not tracking a caption drag, send focus to our subwindow */
    if (!in_title_drag_ && subwin_ != 0)
        SetFocus(subwin_->get_handle());
}

/* ------------------------------------------------------------------------ */
/*
 *   Activate a docking window.
 */
int CTadsWinDock::do_activate(int flag, int minimized, HWND other_win)
{
    /* set focus on the subwindow */
    if (subwin_ != 0)
        SetFocus(subwin_->get_handle());

    /* inherit the default */
    return CTadsWin::do_activate(flag, minimized, other_win);
}

/* ------------------------------------------------------------------------ */
/*
 *   Resize
 */
void CTadsWinDock::do_resize(int mode, int x, int y)
{
    /* inherit default */
    CTadsWin::do_resize(mode, x, y);

    /* if we're resizing to a normal size, resize the subwindow */
    switch(mode)
    {
    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* if there's a subwindow, resize it */
        if (subwin_ != 0)
            MoveWindow(subwin_->get_handle(), 0, 0, x, y, TRUE);
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   key down
 */
int CTadsWinDock::do_keydown(int virtual_key, long keydata)
{
    /*
     *   if we're tracking a resize, and this is a CTRL key, modify the
     *   keystate for the tracking
     */
    if (in_title_drag_ && virtual_key == VK_CONTROL)
    {
        /* continue title tracking with the control key down */
        continue_title_tracking(drag_keys_ | MK_CONTROL,
                                drag_x_, drag_y_, FALSE);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_keydown(virtual_key, keydata);
}

/*
 *   key up
 */
int CTadsWinDock::do_keyup(int virtual_key, long keydata)
{
    /*
     *   if we're tracking a resize, and this is a CTRL key, modify the
     *   keystate for the tracking
     */
    if (in_title_drag_ && virtual_key == VK_CONTROL)
    {
        /* continue title tracking with the control key down */
        continue_title_tracking(drag_keys_ & ~MK_CONTROL,
                                drag_x_, drag_y_, FALSE);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_keydown(virtual_key, keydata);
}

/* ------------------------------------------------------------------------ */
/*
 *   non-client right mouse button click
 */
int CTadsWinDock::do_nc_rightbtn_down(int, int x, int y, int, int hittype)
{
    POINT pt;

    /* if we're not in the title bar, ignore it */
    if (hittype != HTCAPTION)
        return FALSE;

    /* adjust to client-local coordinates */
    pt.x = x;
    pt.y = y;
    ScreenToClient(handle_, &pt);

    /* show our context menu */
    show_ctx_menu(pt.x, pt.y);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   system character
 */
int CTadsWinDock::do_syschar(TCHAR ch, unsigned long keydata)
{
    /* if it's alt-space, show our own system menu */
    if (ch == ' ' && (keydata & (1 << 29)) != 0)
    {
        /* show the menu */
        show_ctx_menu(0, 0);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_syschar(ch, keydata);
}


/* ------------------------------------------------------------------------ */
/*
 *   non-client left mouse button click
 */
int CTadsWinDock::do_nc_leftbtn_down(int keys, int x, int y, int clicks,
                                     int hit_type)
{
    RECT win_rc;

    /* check the hit type */
    switch(hit_type)
    {
    case HTCAPTION:
        /* if we're not currently dockable, use default handling */
        if (!is_win_dockable())
            break;

        /* if we're double-clicking, dock the window */
        if (clicks == 2)
        {
            /* dock it at the last docking position */
            auto_dock();

            /* handled */
            return TRUE;
        }

        /* activate me */
        SetForegroundWindow(handle_);

        /* get the current window rectangle - this is the undocked shape */
        GetWindowRect(handle_, &win_rc);

        /*
         *   Start title dragging.  The undocked shape is the simply
         *   current window rectangle, since we're currently undocked.
         */
        begin_title_tracking(&win_rc, x - win_rc.left, y - win_rc.top);

        /* handled */
        return TRUE;

    default:
        /* we don't override this case */
        break;
    }

    /* we didn't handle it - inherit default */
    return CTadsWin::do_nc_leftbtn_down(keys, x, y, clicks, hit_type);
}

/* ------------------------------------------------------------------------ */
/*
 *   left mouse button release
 */
int CTadsWinDock::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're tracking a title-drag operation, finish it */
    if (in_title_drag_)
    {
        /* note the final mouse and key state for tracking */
        drag_keys_ = keys;
        drag_x_ = x;
        drag_y_ = y;

        /*
         *   release capture - we'll finish the operation when we receive
         *   notification of the capture change
         */
        ReleaseCapture();

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_leftbtn_up(keys, x, y);
}

/* ------------------------------------------------------------------------ */
/*
 *   mouse move
 */
int CTadsWinDock::do_mousemove(int keys, int x, int y)
{
    /* if we're tracking a title-drag operation, continue with it */
    if (in_title_drag_)
    {
        /* continue title dragging */
        continue_title_tracking(keys, x, y, FALSE);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_mousemove(keys, x, y);
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert the window to MDI
 */
void CTadsWinDock::cvt_to_mdi(int visible)
{
    RECT rc;
    RECT clirc;
    POINT ofs;
    CTadsWinDockableMDI *mdiwin;
    HWND mdi_client;
    char title[256];

    /* if there's no MDI parent, we can't convert */
    if (default_mdi_parent_ == 0)
        return;

    /* activate the parent window */
    SetForegroundWindow(get_undocked_parent_hdl());

    /* create the new MDI window */
    mdiwin = new CTadsWinDockableMDI(
        0, docked_width_, docked_height_, docked_align_,
        CTadsDockModelWinSpec(model_win_),
        get_undocked_parent(), get_undocked_parent_hdl());

    /* get the MDI client handle */
    mdi_client = default_mdi_parent_->get_parent_of_children();

    /* get my undocked window area */
    get_undocked_pos(&rc);

    /* translate the rectangle to the MDI client area. */
    ofs.x = ofs.y = 0;
    ClientToScreen(mdi_client, &ofs);
    OffsetRect(&rc, -ofs.x, -ofs.y);

    /*
     *   If the window is outside the client area, reposition it.  First,
     *   try just moving it so that it stays within the window bounds.
     */
    GetClientRect(mdi_client, &clirc);
    if (rc.right > clirc.right)
        OffsetRect(&rc, clirc.right - rc.right - 20, 0);
    if (rc.bottom > clirc.bottom)
        OffsetRect(&rc, 0, clirc.bottom - rc.bottom - 20);
    if (rc.left < 0)
        OffsetRect(&rc, -rc.left + 20, 0);
    if (rc.top < 0)
        OffsetRect(&rc, 0, -rc.top + 20);

    /* get my title */
    get_undocked_title(title, sizeof(title));

    /* create the MDI window */
    mdiwin->create_system_window(default_mdi_parent_, visible, title, &rc,
                                 new CTadsSyswinMdiChild(mdiwin));

    /* reparent my subwindow into the new window */
    reparent_subwin(mdiwin);

    /* force a resize to fix up the window size */
    GetClientRect(mdiwin->get_handle(), &rc);
    mdiwin->do_resize(SIZE_RESTORED, rc.right, rc.bottom);

    /*
     *   If we're showing the window, try to make sure it gets activated.
     *   This is a bit of a hack, in that the window *should* be activated
     *   automatically when it's created; for reasons that are not clear
     *   (but seem to have something to do with the order of events in
     *   activating the MDI frame itself while creating a new MDI child),
     *   the new child will not end up being activated after we create it
     *   above.
     */
    if (visible)
        SendMessage(mdi_client, WM_MDIACTIVATE,
                    (WPARAM)mdiwin->get_handle(), 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Auto-dock the window - dock the window at the last docking position
 */
void CTadsWinDock::auto_dock()
{
    /* if there's a default port, ask the default port to dock the window */
    if (default_port_ != 0)
        default_port_->dockport_auto_dock(this);
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a docking port under a given point (in screen coordinates)
 */
CTadsDockport *CTadsWinDock::find_docking_port(POINT pt)
{
    HWND targ;

    /* find the window under the mouse */
    targ = WindowFromPoint(pt);

    /*
     *   look for the target and the target's parents in the docking port
     *   registry; scan up the parent list until we find a docking port or
     *   reach the top-level window
     */
    for ( ; targ != 0 ; targ = GetParent(targ))
    {
        CTadsDockport *cur;

        /* search the docking port list for this handle */
        for (cur = ports_ ; cur != 0 ; cur = cur->next_dock_port_)
        {
            /* if this docking port has the handle, we found it */
            if (cur->dockport_get_handle() == targ)
            {
                /* return the window */
                return cur;
            }
        }
    }

    /* we didn't find a docking port under the mouse */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin tracking a title bar drag
 */
void CTadsWinDock::begin_title_tracking(const RECT *undock_rc,
                                        int start_xofs, int start_yofs)
{
    /*
     *   note the initial dragging position - this is simply the original
     *   window rectangle, since if we don't move the mouse we should end
     *   up where we started
     */
    GetWindowRect(handle_, &drag_win_rect_);

    /* remember the original undocked window shape */
    drag_orig_win_rect_ = *undock_rc;

    /* note the starting offset from the upper left of the window */
    drag_mouse_xofs_ = start_xofs;
    drag_mouse_yofs_ = start_yofs;

    /* note that we're tracking a drag of the window position */
    in_title_drag_ = TRUE;

    /*
     *   set focus on my window during the tracking, so that we can
     *   immediately respond visually to changes in the CTRL key state
     */
    SetFocus(handle_);

    /* capture the mouse until the button is released */
    SetCapture(handle_);

    /* draw the initial outline */
    xor_drag_rect(&drag_win_rect_);
}

/* ------------------------------------------------------------------------ */
/*
 *   End tracking of window movement
 */
void CTadsWinDock::end_title_tracking()
{
    /* if we're not tracking a window drag, there's nothing we need to do */
    if (!in_title_drag_)
        return;

    /* end drag mode */
    in_title_drag_ = FALSE;

    /*
     *   now that we're done tracking, set focus to my subwindow - we
     *   stole focus during tracking, so we want to restore things now
     */
    if (subwin_ != 0)
        SetFocus(subwin_->get_handle());

    /* continue with title tracking for the final time */
    continue_title_tracking(drag_keys_, drag_x_, drag_y_, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Continue title-bar tracking.  If 'done' is true, the mouse has been
 *   released and we should finish the job; otherwise, we should expect
 *   more tracking to come.
 */
void CTadsWinDock::continue_title_tracking(int keys, int x, int y, int done)
{
    POINT pt;
    CTadsDockport *port;
    POINT port_lclpt;
    RECT old_xor_rc;

    /* remember the current parameters for next time */
    drag_x_ = x;
    drag_y_ = y;
    drag_keys_ = keys;

    /*
     *   if we're done, always un-draw the drag rectangle; otherwise,
     *   leave it on the screen for now and check to see if we're going to
     *   move it before un-drawing and re-drawing it, to avoid flicker
     */
    if (done)
        xor_drag_rect(&drag_win_rect_);

    /* remember where the previous xor rectangle was drawn */
    old_xor_rc = drag_win_rect_;

    /* adjust to global coordinates */
    pt.x = x;
    pt.y = y;
    ClientToScreen(handle_, &pt);
    x = pt.x;
    y = pt.y;

    /*
     *   If the control key is down, do not dock under any circumstances.
     *   Otherwise, look for a docking port under the mouse.
     */
    if ((keys & MK_CONTROL) != 0)
    {
        /* the control key is down - do not dock */
        port = 0;
    }
    else
    {
        /* look for a docking port under the mouse */
        port = find_docking_port(pt);
    }

    /* if we found a docking port, ask it what to do */
    if (port != 0)
    {
        /* translate the point to the window's coordinates */
        port_lclpt = pt;
        ScreenToClient(port->dockport_get_handle(), &port_lclpt);

        /* ask the port if it wants to dock here */
        if (port->dockport_query_dock(this, port_lclpt, &drag_win_rect_))
        {
            POINT ofs;

            /* convert the coordinates to screen coordinates */
            ofs.x = 0;
            ofs.y = 0;
            ClientToScreen(port->dockport_get_handle(), &ofs);
            OffsetRect(&drag_win_rect_, ofs.x, ofs.y);
        }
        else
        {
            /* it doesn't want to dock; forget it */
            port = 0;
        }
    }

    /* if we're not docking, move the window by the mouse offset */
    if (port == 0)
    {
        int new_x, new_y;

        /* move the window to the new coordinates relative to the mouse */
        new_x = x - drag_mouse_xofs_;
        new_y = y - drag_mouse_yofs_;
        SetRect(&drag_win_rect_, new_x, new_y,
                (new_x + drag_orig_win_rect_.right
                 - drag_orig_win_rect_.left),
                (new_y + drag_orig_win_rect_.bottom
                 - drag_orig_win_rect_.top));
    }

    /* if we're done, finalize the change */
    if (done)
    {
        /* if we have a dock port, tell it to dock the window */
        if (port != 0)
        {
            POINT ofs;

            /*
             *   convert the destination rectangle to client-local
             *   coordinates for the docking port
             */
            ofs.x = ofs.y = 0;
            ScreenToClient(port->dockport_get_handle(), &ofs);
            OffsetRect(&drag_win_rect_, ofs.x, ofs.y);

            /* tell the dock port to dock the window */
            port->dockport_dock(this, port_lclpt);
        }
        else
        {
            /* tell myself to undock the window */
            undock_window(&drag_win_rect_, TRUE);
        }
    }
    else
    {
        /*
         *   We're going to keep going.  If the xor rectangle position has
         *   changed, un-draw the old one and draw the new one.
         */
        if (!EqualRect(&old_xor_rc, &drag_win_rect_))
        {
            /* remove the old one */
            xor_drag_rect(&old_xor_rc);

            /* draw the new one */
            xor_drag_rect(&drag_win_rect_);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Undock the window.  Since this type of window started off undocked,
 *   all we need to do is move the window to its new position.
 */
void CTadsWinDock::undock_window(const RECT *pos, int /*show*/)
{
    /* move the window to its new position */
    MoveWindow(handle_, pos->left, pos->top,
               pos->right - pos->left, pos->bottom - pos->top, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Receive notification of capture change
 */
int CTadsWinDock::do_capture_changed(HWND new_capture_win)
{
    /* end title dragging if necessary */
    end_title_tracking();

    /* inherit default */
    return CTadsWin::do_capture_changed(new_capture_win);
}

/* ------------------------------------------------------------------------ */
/*
 *   xor the drag rectangle onto the screen
 */
void CTadsWinDock::xor_drag_rect(const RECT *rc) const
{
    HDC hdc;
    HBRUSH oldbr;
    HPEN oldpen;
    int oldrop;
    const int wid = 3;

    /* get the desktop dc */
    hdc = GetWindowDC(GetDesktopWindow());

    /* set up our brush and pen */
    oldbr = (HBRUSH)SelectObject(hdc, hbrush_);
    oldpen = (HPEN)SelectObject(hdc, (HPEN)GetStockObject(NULL_PEN));

    /* set up the drawing mode */
    oldrop = SetROP2(hdc, R2_XORPEN);

    /* draw the drag outline */
    Rectangle(hdc, rc->left, rc->top,
              rc->right + 1, rc->top + wid + 1);
    Rectangle(hdc, rc->left, rc->bottom - wid,
              rc->right + 1, rc->bottom + 1);
    Rectangle(hdc, rc->left, rc->top + wid,
              rc->left + wid + 1, rc->bottom - wid + 1);
    Rectangle(hdc, rc->right - wid, rc->top + wid,
              rc->right + 1, rc->bottom - wid + 1);

    /* restore drawing mode */
    SetROP2(hdc, oldrop);

    /* restore pen and brush */
    SelectObject(hdc, oldpen);
    SelectObject(hdc, oldbr);

    /* release the dc */
    ReleaseDC(GetDesktopWindow(), hdc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a window to the docking port list
 */
void CTadsWinDock::add_port(CTadsDockport *port)
{
    /* link it into our list */
    port->next_dock_port_ = ports_;
    ports_ = port;
}

/*
 *   Remove a window from the docking port list
 */
void CTadsWinDock::remove_port(CTadsDockport *port)
{
    CTadsDockport *cur;
    CTadsDockport *prv;

    /* if this is the default port, forget the default port */
    if (port == default_port_)
        default_port_ = 0;

    /* find it in the list */
    for (prv = 0, cur = ports_ ; cur != 0 ;
         prv = cur, cur = cur->next_dock_port_)
    {
        /* if this is the one, unlink it */
        if (cur == port)
        {
            /* unlink it from the list */
            if (prv != 0)
                prv->next_dock_port_ = port->next_dock_port_;
            else
                ports_ = port->next_dock_port_;

            /* done - no need to look further */
            return;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Dockable MDI Window Implementation
 */

/*
 *   Process a user message
 */
int CTadsWinDockableMDI::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* check the message */
    switch(msg)
    {
    case DOCKM_DESTROY:
        /* tell the MDI client to destroy me */
        SendMessage(GetParent(handle_), WM_MDIDESTROY, (WPARAM)handle_, 0);
        return TRUE;

    case DOCKM_MDIMAXCHANGE:
        /*
         *   The MDI child-maximized status has changed.  Some windows don't
         *   want to apply internal resizing when displayed as background
         *   maximized MDI windows - MDI sets all background windows back to
         *   their non-maximized size when they're in the background, so
         *   switching between child windows causes a lot of spurious
         *   resizing that can be problematic for things like scroll-position
         *   stability.  So, some of our windows defer any resizing until
         *   either (a) they become the active maximized MDI child, or (b) we
         *   return to non-maximized for all MDI child windows.  We catch the
         *   latter case here: our host window sends us this message to let
         *   us know when the max status is changing, so that we can pass
         *   along a resize notification to our content windows.
         */
        if (subwin_ != 0)
        {
            RECT rc;

            GetClientRect(handle_, &rc);
            SendMessage(subwin_->get_handle(), WM_SIZE, SIZE_RESTORED,
                        MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
        }
        return TRUE;

    default:
        /* it's not one of ours */
        return CTadsWinDock::do_user_message(msg, wpar, lpar);
    }
}

/*
 *   process a command
 */
int CTadsWinDockableMDI::do_command(int notify_code,
                                    int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_DOCKWIN_DOCKVIEW:
        /* convert to a docking view */
        cvt_to_docking_view();
        return TRUE;

    case ID_WINDOW_RESTORE:
        SendMessage(handle_, WM_SYSCOMMAND, SC_RESTORE, 0);
        return TRUE;

    case ID_WINDOW_MINIMIZE:
        SendMessage(handle_, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        return TRUE;

    case ID_WINDOW_MAXIMIZE:
        SendMessage(handle_, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        return TRUE;

    default:
        /* inherit default */
        return CTadsWinDock::do_command(notify_code, command_id, ctl);
    }
}

/*
 *   check a command
 */
TadsCmdStat_t CTadsWinDockableMDI::check_command(const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_DOCKWIN_DOCKVIEW:
        /*
         *   this type of window is not currently a docking view, but is
         *   convertible to a docking view - enable but uncheck this item
         */
        return TADSCMD_ENABLED;

    case ID_DOCKWIN_DOCK:
        /*
         *   this is enabled, but it's not the default command for an MDI
         *   child (which it is for a regular docking view)
         */
        return (default_port_ != 0 ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_WINDOW_RESTORE:
        return (IsIconic(handle_) || IsZoomed(handle_)
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_WINDOW_MAXIMIZE:
        return IsZoomed(handle_) ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_WINDOW_MINIMIZE:
        return IsIconic(handle_) ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    default:
        /* inherit default */
        return CTadsWinDock::check_command(info);
    }
}

/*
 *   convert to a docking view
 */
void CTadsWinDockableMDI::cvt_to_docking_view()
{
    CTadsWinDock *new_win;
    RECT rc;
    char title[256];

    /*
     *   put the window at the same position it's in now - it's currently
     *   an MDI child, and we're making it a top-level window, so all we
     *   need are the screen coordinates
     */
    GetWindowRect(handle_, &rc);

    /* get my window title, to use for the new window's title */
    get_undocked_title(title, sizeof(title));

    /* create a new undocked container window */
    new_win = new CTadsWinDock(
        0, docked_width_, docked_height_, docked_align_,
        CTadsDockModelWinSpec(model_win_));

    /* create the system window as a child of the MDI frame */
    new_win->create_system_window(undocked_parent_, undocked_parent_hdl_,
                                  TRUE, title, &rc,
                                  new CTadsSyswin(new_win));

    /* reparent my subwindow into the new window */
    reparent_subwin(new_win);

    /* force a resize to fix up the window size */
    GetClientRect(new_win->get_handle(), &rc);
    new_win->do_resize(SIZE_RESTORED, rc.right, rc.bottom);
}

/*
 *   Adjust for disappearance of our subwindow
 */
void CTadsWinDockableMDI::adjust_for_no_subwin()
{
    /* send myself a close message */
    PostMessage(handle_, WM_CLOSE, 0, 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Docked Window implementation
 */

/* size of our title bar with controls (close box, etc) */
const int DOCKED_TITLE_BAR_SIZE_LARGE = 17;

/* size of title bar with no controls */
const int DOCKED_TITLE_BAR_SIZE_SMALL = 10;

/* docked window border size */
const int DOCKED_BORDER_SIZE = 2;

/* size of our close box control */
const int DOCKED_CLOSEBOX_WID = 12;
const int DOCKED_CLOSEBOX_HT = 12;

/*
 *   create
 */
CTadsWinDocked::CTadsWinDocked(CTadsDockedCont *cont,
                               tads_dock_orient orientation,
                               int docked_width, int docked_height,
                               const RECT *undocked_rc,
                               const RECT *mdi_rc,
                               const char *undocked_title,
                               CTadsWin *undocked_parent,
                               HWND undocked_parent_hdl,
                               int has_closebox,
                               CTadsDockModelWin *model_win)
    : CTadsWinDock(0, docked_width, docked_height,
                   cont->get_docked_alignment(),
                   CTadsDockModelWinSpec(model_win))
{
    int angle;

    /* remember our undocked position */
    undocked_rc_ = *undocked_rc;
    mdi_rc_ = *mdi_rc;

    /* remember our undocked title */
    undocked_title_.set(undocked_title);

    /* remember the parent to use for our undocked counterpart */
    undocked_parent_ = undocked_parent;
    undocked_parent_hdl_ = undocked_parent_hdl;

    /* remember whether or not we have a close box */
    has_closebox_ = has_closebox;

    /* remember our container */
    cont_ = cont;

    /* remember our orientation */
    orientation_ = orientation;

    /* we're not in a list yet */
    next_docked_ = 0;

    /* create drawing objects */
    face_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DFACE));
    hilite_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
    shadow_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    dkshadow_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));

    /* if we have a close box, load the close-box button bitmap */
    if (has_closebox)
        closebox_bmp_ = LoadBitmap(CTadsApp::get_app()->get_instance(),
                                   MAKEINTRESOURCE(IDB_BMP_SMALL_CLOSEBOX));
    else
        closebox_bmp_ = 0;

    /* not tracking the closebox */
    tracking_closebox_ = FALSE;
    closebox_down_ = FALSE;

    /*
     *   a docked window is effectively always maximized - set our
     *   internal maximized flag so that we draw any adornments (such as
     *   grow boxes) accordingly
     */
    maximized_ = TRUE;

    /* create our title bar font */
    angle = (orientation == TADS_DOCK_ORIENT_HORIZ ? 900 : 0);
    title_font_ = CreateFont(DOCKED_CLOSEBOX_HT + 2, 0, angle, angle,
                             FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, "Arial");
}

CTadsWinDocked::~CTadsWinDocked()
{
    /* delete drawing objects */
    DeleteObject(face_pen_);
    DeleteObject(shadow_pen_);
    DeleteObject(hilite_pen_);
    DeleteObject(dkshadow_pen_);
    DeleteObject(title_font_);
    if (closebox_bmp_ != 0)
        DeleteObject(closebox_bmp_);

    /*
     *   Tell my container that I'm undocking.  (If I don't have a
     *   container, it means that the container was deleted first, in
     *   which case we don't have to worry about notifying it of my
     *   deletion.)
     */
    if (cont_ != 0)
        cont_->remove_docked_win(this);
}

/*
 *   Adjust for the removal of our subwindow
 */
void CTadsWinDocked::adjust_for_no_subwin()
{
    /* inherit default */
    CTadsWinDock::adjust_for_no_subwin();
}

/*
 *   Undock the window.
 */
void CTadsWinDocked::undock_window(const RECT *pos, int show)
{
    CTadsWinDock *new_win;
    RECT rc;
    RECT desk_rc;

    /* make a modifiable copy of the position rectangle */
    rc = *pos;

    /* get the desktop area */
    GetWindowRect(GetDesktopWindow(), &desk_rc);

    /* give the window a minimum size */
    if (rc.right < rc.left + 30)
        rc.right = rc.left + 30;
    if (rc.bottom < rc.top + 20)
        rc.bottom = rc.top + 20;

    /*
     *   if we're out of the desktop area, move the window to make sure it
     *   shows up on the desktop
     */
    if (rc.right < desk_rc.left)
        OffsetRect(&rc, desk_rc.left - rc.left + 20, 0);
    else if (rc.left > desk_rc.right)
        OffsetRect(&rc, desk_rc.right - rc.left - 50, 0);
    else if (rc.bottom < desk_rc.top)
        OffsetRect(&rc, 0, desk_rc.top - rc.bottom + 20);
    else if (rc.top > desk_rc.bottom)
        OffsetRect(&rc, 0, desk_rc.bottom - rc.top - 50);

    /* create a new undocked container window */
    new_win = new CTadsWinDock(0, docked_width_, docked_height_,
                               cont_->get_docked_alignment(),
                               CTadsDockModelWinSpec(model_win_));
    new_win->create_system_window(undocked_parent_, undocked_parent_hdl_,
                                  show, undocked_title_.get(), &rc,
                                  new CTadsSyswin(new_win));

    /* reparent my subwindow into the new window */
    reparent_subwin(new_win);

    /* force a resize to fix up the window size */
    GetClientRect(new_win->get_handle(), &rc);
    new_win->do_resize(SIZE_RESTORED, rc.right, rc.bottom);

    /*
     *   if we're hiding the window, set its dock-on-unhide flag, so that
     *   it re-docks when it's next shown
     */
    if (!show)
        new_win->set_dock_on_unhide();
}

/*
 *   process a notification message
 */
int CTadsWinDocked::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* on a focus change, redraw our title bar with the new status */
        inval();

        /* notify my child of an artificial activation */
        if (subwin_ != 0)
            subwin_->do_parent_activate(notify_code == NM_SETFOCUS);

        /* inherit the default handling as well */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CTadsWinDock::do_notify(control_id, notify_code, nm);
}

/*
 *   Resize
 */
void CTadsWinDocked::do_resize(int mode, int x, int y)
{
    /* inherit default */
    CTadsWin::do_resize(mode, x, y);

    /* if we're resizing to a normal size, resize the subwindow */
    switch(mode)
    {
    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* if there's a subwindow, resize it */
        if (subwin_ != 0)
        {
            RECT rc;
            RECT trc;

            /* start out with the whole client area */
            SetRect(&rc, 0, 0, x, y);

            /* calculate the title-bar area */
            trc = rc;
            get_title_bar_rect(&trc);

            /* subtract the title-bar area */
            SubtractRect(&rc, &rc, &trc);

            /* take out space for our border on all edges */
            rc.left += DOCKED_BORDER_SIZE;
            rc.right -= DOCKED_BORDER_SIZE;
            rc.top += DOCKED_BORDER_SIZE;
            rc.bottom -= DOCKED_BORDER_SIZE;

            /* move the window */
            MoveWindow(subwin_->get_handle(), rc.left, rc.top,
                       rc.right - rc.left, rc.bottom - rc.top, TRUE);

            /*
             *   Remember our new docked width or height, depending on our
             *   orientation.
             */
            switch(orientation_)
            {
            case TADS_DOCK_ORIENT_VERT:
                docked_width_ = x + MDI_WIDGET_SIZE + 2;
                break;

            case TADS_DOCK_ORIENT_HORIZ:
                docked_height_ = y + MDI_WIDGET_SIZE + 2;
                break;
            }
        }
        break;
    }
}

/*
 *   handle commands
 */
int CTadsWinDocked::do_command(int notify_code, int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_DOCKWIN_HIDE:
        /* undock and hide the window */
        undock_window(&undocked_rc_, FALSE);
        return TRUE;

    case ID_DOCKWIN_UNDOCK:
        /* undock and show the window */
        undock_window(&undocked_rc_, TRUE);
        return TRUE;

    default:
        /* not handled; inherit default */
        return CTadsWinDock::do_command(notify_code, command_id, ctl);
    }
}

/*
 *   get command status
 */
TadsCmdStat_t CTadsWinDocked::check_command(const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_DOCKWIN_UNDOCK:
        /* it's the default command */
        return TADSCMD_DEFAULT;

    case ID_DOCKWIN_HIDE:
        /* enable it */
        return TADSCMD_ENABLED;

    case ID_DOCKWIN_DOCK:
        /* we're already docked */
        return TADSCMD_DISABLED;

    default:
        /* not handled; inherit default */
        return CTadsWinDock::check_command(info);
    }
}

/*
 *   left click
 */
int CTadsWinDocked::do_leftbtn_down(int keys, int x, int y, int clicks)
{
    RECT trc;
    RECT crc;
    POINT pt;

    /* make a point from the mouse position */
    pt.x = x;
    pt.y = y;

    /* get the title-bar and close-box rectanlges */
    GetClientRect(handle_, &trc);
    crc = trc;
    get_title_bar_rect(&trc);
    get_closebox_rect(&crc);

    /* if we're in the closebox, track it */
    if (has_closebox_ && PtInRect(&crc, pt))
    {
        /* start tracking the closebox */
        tracking_closebox_ = TRUE;
        closebox_down_ = TRUE;

        /* capture the mouse while tracking */
        SetCapture(handle_);

        /* draw the closebox in the down state */
        draw_closebox();

        /* handled */
        return TRUE;
    }

    /* if it's in the title bar, track it */
    if (PtInRect(&trc, pt))
    {
        RECT docked_rc;

        /* if this is a double-click, undock the window */
        if (clicks == 2)
        {
            /* undock the window, making the new window visible */
            undock_window(&undocked_rc_, TRUE);

            /* handled */
            return TRUE;
        }

        /*
         *   use the current size for the new docked configuration - use
         *   the size of our parent window, since the docking size is in
         *   terms of the docking bar rather than the docked content
         */
        GetWindowRect(GetParent(handle_), &docked_rc);
        switch(orientation_)
        {
        case TADS_DOCK_ORIENT_VERT:
            docked_width_ = docked_rc.right - docked_rc.left;
            break;

        case TADS_DOCK_ORIENT_HORIZ:
            docked_height_ = docked_rc.bottom - docked_rc.top;
            break;
        }

        /* begin title dragging */
        begin_title_tracking(&undocked_rc_, x, y);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWinDock::do_leftbtn_down(keys, x, y, clicks);
}

/*
 *   left button up
 */
int CTadsWinDocked::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're tracking the closebox, release capture */
    if (tracking_closebox_)
    {
        ReleaseCapture();
        return TRUE;
    }

    /* inherit default */
    return CTadsWinDock::do_leftbtn_up(keys, x, y);
}

/*
 *   right click
 */
int CTadsWinDocked::do_rightbtn_down(int keys, int x, int y, int clicks)
{
    POINT pt;
    RECT trc;
    RECT crc;

    /* make a point from the mouse position */
    pt.x = x;
    pt.y = y;

    /* get the title-bar and close-box rectanlges */
    GetClientRect(handle_, &trc);
    crc = trc;
    get_title_bar_rect(&trc);
    get_closebox_rect(&crc);

    /* if it's in the closebox, ignore it */
    if (has_closebox_ && PtInRect(&crc, pt))
        return TRUE;

    /* if it's over the title bar, show the context menu */
    if (PtInRect(&trc, pt))
    {
        /* show the context menu */
        show_ctx_menu(x, y);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWinDock::do_rightbtn_down(keys, x, y, clicks);
}


/*
 *   mouse move
 */
int CTadsWinDocked::do_mousemove(int keys, int x, int y)
{
    /* if tracking the closebox, continue it */
    if (tracking_closebox_)
    {
        RECT crc;
        POINT pt;
        int old_down = closebox_down_;

        /* check if we're still in the closebox */
        GetClientRect(handle_, &crc);
        get_closebox_rect(&crc);
        pt.x = x;
        pt.y = y;
        closebox_down_ = (PtInRect(&crc, pt) != 0);

        /* if the state has changed, redraw it */
        if ((int)closebox_down_ != old_down)
            draw_closebox();

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWinDock::do_mousemove(keys, x, y);
}

/*
 *   handle a capture change
 */
int CTadsWinDocked::do_capture_changed(HWND new_capture_win)
{
    /* if tracking the closebox, end it */
    if (tracking_closebox_)
    {
        /* we're no longer tracking it */
        tracking_closebox_ = FALSE;

        /* if the closebox is still down, close the window */
        if (closebox_down_)
        {
            /* no longer tracking */
            closebox_down_ = FALSE;
            draw_closebox();

            /* close it */
            do_close();
        }
    }

    /* inherit default */
    return CTadsWinDock::do_capture_changed(new_capture_win);
}

/*
 *   close
 */
int CTadsWinDocked::do_close()
{
    /* if I have a subwindow, treat this as undock-and-hide */
    if (subwin_ != 0)
    {
        /* undock the window, hiding the new undocked window */
        undock_window(&undocked_rc_, FALSE);

        /* we've handled the close fully - stop the default handling */
        return FALSE;
    }

    /* inherit the default handling */
    return CTadsWinDock::do_close();
}

/*
 *   get the title bar rectangle
 */
void CTadsWinDocked::get_title_bar_rect(RECT *rc) const
{
    int siz;

    /*
     *   get our title bar size - it depends on whether we have any
     *   controls or not
     */
    if (has_closebox_)
    {
        /* we have controls - use the large title bar */
        siz = DOCKED_TITLE_BAR_SIZE_LARGE;
    }
    else
    {
        /* no controls - use the small title bar */
        siz = DOCKED_TITLE_BAR_SIZE_SMALL;
    }

    /* the position of the title bar depends on our orientation */
    switch(orientation_)
    {
    case TADS_DOCK_ORIENT_HORIZ:
        /* horizontal - title bar goes at the left */
        rc->right = rc->left + siz;
        break;

    case TADS_DOCK_ORIENT_VERT:
        /* vertical - title bar goes at the top */
        rc->bottom = rc->top + siz;
        break;
    }
}

/*
 *   get the closebox rectangle
 */
void CTadsWinDocked::get_closebox_rect(RECT *rc) const
{
    /* get the title bar rectangle */
    get_title_bar_rect(rc);

    /* figure the location based on the orientation */
    switch(orientation_)
    {
    case TADS_DOCK_ORIENT_VERT:
        /*
         *   the closebox is at the top right corner - center it
         *   vertically in the title bar
         */
        OffsetRect(rc, -(DOCKED_BORDER_SIZE + 1),
                   (rc->bottom - rc->top - DOCKED_CLOSEBOX_HT)/2
                   + DOCKED_BORDER_SIZE + 1);
        rc->left = rc->right - DOCKED_CLOSEBOX_WID;
        break;

    case TADS_DOCK_ORIENT_HORIZ:
        /*
         *   the closebox is at the top left corner - center it
         *   horizontally in the title bar
         */
        OffsetRect(rc,
                   (rc->right - rc->left - DOCKED_CLOSEBOX_WID)/2
                   + DOCKED_BORDER_SIZE + 1,
                   DOCKED_BORDER_SIZE + 1);
        break;
    }

    /*
     *   set the rectangle for the closebox width and height, now that we
     *   know the upper left position
     */
    rc->right = rc->left + DOCKED_CLOSEBOX_WID;
    rc->bottom = rc->top + DOCKED_CLOSEBOX_HT;
}

/*
 *   draw the window
 */
void CTadsWinDocked::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    RECT clirc;
    RECT trc;
    HPEN oldpen;
    SIZE sz;
    HFONT oldfont;
    int oldmode;
    UINT oldalign;
    RECT textrc;
    COLORREF oldtextclr;
    int has_focus;
    int show_title_lines;

    /* note if focus is in me or a child */
    has_focus = (GetFocus() == handle_ || IsChild(handle_, GetFocus()));

    /* select our font */
    oldfont = (HFONT)SelectObject(hdc, title_font_);

    /* determine how much space the title will take up */
    ht_GetTextExtentPoint32(hdc, undocked_title_.get(),
                            strlen(undocked_title_.get()), &sz);

    /* fill our background with the button face color */
    clirc = *area_to_draw;
    clirc.right++;
    clirc.bottom++;
    FillRect(hdc, &clirc, GetSysColorBrush(COLOR_3DFACE));

    /* get our entire area */
    GetClientRect(handle_, &clirc);

    /* get our title bar area */
    trc = clirc;
    get_title_bar_rect(&trc);

    /* if we're active, draw the title bar in the active caption color */
    if (has_focus)
        FillRect(hdc, &trc, GetSysColorBrush(COLOR_ACTIVECAPTION));

    /* take out the space needed for the close box if we have one */
    if (has_closebox_)
    {
        switch(orientation_)
        {
        case TADS_DOCK_ORIENT_VERT:
            /* close box is at the right */
            trc.right -= DOCKED_CLOSEBOX_WID + 5;
            break;

        case TADS_DOCK_ORIENT_HORIZ:
            /* close box is at the top */
            trc.top += DOCKED_CLOSEBOX_HT + 5;
            break;
        }
    }

    /* center our seven-pixel structure within the available space */
    switch(orientation_)
    {
    case TADS_DOCK_ORIENT_VERT:
        /* center it and offset for the border at the left */
        OffsetRect(&trc, 0,
                   DOCKED_BORDER_SIZE + (trc.bottom - trc.top - 7)/2);

        /* shrink the ends */
        InflateRect(&trc, -3, 0);
        break;

    case TADS_DOCK_ORIENT_HORIZ:
        /* center it and offset for the border above */
        OffsetRect(&trc,
                   DOCKED_BORDER_SIZE + (trc.right - trc.left - 7)/2, 0);

        /* shrink the ends */
        InflateRect(&trc, 0, -3);
        break;
    }

    /* top border (white) */
    oldpen = (HPEN)SelectObject(hdc, hilite_pen_);
    MoveToEx(hdc, 0, 0, 0);
    LineTo(hdc, clirc.right, 0);

    /* left border (white) */
    MoveToEx(hdc, 0, 0, 0);
    LineTo(hdc, 0, clirc.bottom);

    /* bottom border (dark gray) */
    SelectObject(hdc, shadow_pen_);
    MoveToEx(hdc, 0, clirc.bottom - 1, 0);
    LineTo(hdc, clirc.right, clirc.bottom - 1);

    /* right border (dark gray) */
    MoveToEx(hdc, clirc.right - 1, 0, 0);
    LineTo(hdc, clirc.right - 1, clirc.bottom);

    /* start with the text occupying the title area */
    textrc = trc;

    /* set transparent/bottom mode for drawing the caption */
    oldmode = SetBkMode(hdc, TRANSPARENT);
    oldalign = SetTextAlign(hdc, TA_TOP);
    oldtextclr = SetTextColor(hdc, GetSysColor(has_focus
                                               ? COLOR_CAPTIONTEXT
                                               : COLOR_BTNTEXT));

    /* presume we'll draw the lines in the title bar */
    show_title_lines = TRUE;

    /* take space out of the title control area for the title caption */
    switch(orientation_)
    {
    case TADS_DOCK_ORIENT_VERT:
        /* title runs horizontally from top left */
        trc.left += sz.cx + 5;

        /* if the size has gone negative, don't draw the lines */
        if (trc.right < trc.left)
            show_title_lines = FALSE;

        /* center the title vertically in the space */
        textrc.top = (textrc.bottom - sz.cy)/2 - 2;
        textrc.bottom = textrc.top + sz.cy;

        /* draw it */
        TextOut(hdc, textrc.left, textrc.top,
                undocked_title_.get(), strlen(undocked_title_.get()));
        break;

    case TADS_DOCK_ORIENT_HORIZ:
        /* title runs vertically from the bottom left */
        trc.bottom -= sz.cx + 5;

        /* if the size has gone negative, don't draw the lines */
        if (trc.bottom < trc.top)
            show_title_lines = FALSE;

        /* center the title horizontally in the space */
        textrc.left = (textrc.right - sz.cy)/2 - 2;
        textrc.right = textrc.left + sz.cy;

        /* draw it */
        TextOut(hdc, textrc.left, textrc.bottom,
                undocked_title_.get(), strlen(undocked_title_.get()));
        break;
    }

    /* restore the drawing modes */
    SetBkMode(hdc, oldmode);
    SetTextAlign(hdc, oldalign);
    SetTextColor(hdc, oldtextclr);

    /* draw our title bar lines, if we have room for them */
    if (show_title_lines)
    {
        switch(orientation_)
        {
        case TADS_DOCK_ORIENT_VERT:
            /* draw the two title-bar lines */
            draw_title_line_h(hdc, &trc);
            OffsetRect(&trc, 0, 4);
            draw_title_line_h(hdc, &trc);
            break;

        case TADS_DOCK_ORIENT_HORIZ:
            /* draw the two title-bar lines */
            draw_title_line_v(hdc, &trc);
            OffsetRect(&trc, 4, 0);
            draw_title_line_v(hdc, &trc);
            break;
        }
    }

    /* if there's a close box, draw it */
    if (has_closebox_)
        draw_closebox();

    /* restore drawing objects */
    SelectObject(hdc, oldpen);
    SelectObject(hdc, oldfont);
}

/*
 *   draw the closebox
 */
void CTadsWinDocked::draw_closebox()
{
    RECT srcrc;
    RECT clsrc;
    int got_dc = FALSE;

    /* get the area for drawing it */
    GetClientRect(handle_, &clsrc);
    get_closebox_rect(&clsrc);

    /*
     *   pick the image - if the closebox is up, draw the first image in
     *   our two-image bitmap array; otherwise draw the second
     */
    SetRect(&srcrc, 0, 0, DOCKED_CLOSEBOX_WID, DOCKED_CLOSEBOX_HT);
    if (closebox_down_)
        OffsetRect(&srcrc, DOCKED_CLOSEBOX_WID, 0);

    /* get the window's dc if we're not painting */
    if (hdc_ == 0)
    {
        got_dc = TRUE;
        hdc_ = GetWindowDC(handle_);
    }

    /* draw the bitmap */
    draw_hbitmap(closebox_bmp_, &clsrc, &srcrc);

    /* drop our dc if we got it */
    if (got_dc)
    {
        ReleaseDC(handle_, hdc_);
        hdc_ = 0;
    }
}

/*
 *   draw a horizontal title-bar line
 */
void CTadsWinDocked::draw_title_line_h(HDC hdc, const RECT *rc) const
{
    SelectObject(hdc, hilite_pen_);
    MoveToEx(hdc, rc->left, rc->top, 0);
    LineTo(hdc, rc->right - 1, rc->top);

    SelectObject(hdc, face_pen_);
    MoveToEx(hdc, rc->left, rc->top + 1, 0);
    LineTo(hdc, rc->right - 1, rc->top + 1);

    SelectObject(hdc, shadow_pen_);
    MoveToEx(hdc, rc->left, rc->top + 2, 0);
    LineTo(hdc, rc->right, rc->top + 2);
    LineTo(hdc, rc->right, rc->top);
}

/*
 *   draw a vertical title-bar line
 */
void CTadsWinDocked::draw_title_line_v(HDC hdc, const RECT *rc) const
{
    SelectObject(hdc, hilite_pen_);
    MoveToEx(hdc, rc->left, rc->top, 0);
    LineTo(hdc, rc->left, rc->bottom - 1);

    SelectObject(hdc, face_pen_);
    MoveToEx(hdc, rc->left + 1, rc->top, 0);
    LineTo(hdc, rc->left + 1, rc->bottom + 1);

    SelectObject(hdc, shadow_pen_);
    MoveToEx(hdc, rc->left + 2, rc->top, 0);
    LineTo(hdc, rc->left + 2, rc->bottom);
    LineTo(hdc, rc->left, rc->bottom);
}

/*
 *   reparent my subwindow
 */
void CTadsWinDocked::reparent_subwin(CTadsWinDock *new_parent)
{
    /* inherit the default to move the window to a new parent */
    CTadsWinDock::reparent_subwin(new_parent);
}

/* ------------------------------------------------------------------------ */
/*
 *   Docking Port base class implementation
 */

/*
 *   delete
 */
CTadsDockport::~CTadsDockport()
{
    /* unregister myself */
    CTadsWinDock::remove_port(this);
}


/* ------------------------------------------------------------------------ */
/*
 *   MDI Client Docking Port implementation
 */

/*
 *   create
 */
CTadsDockportMDI::CTadsDockportMDI(CTadsWin *mdi_frame, HWND client_handle)
{
    /* remember our frame window */
    mdi_frame_ = mdi_frame;

    /* remember our MDI client handle */
    client_handle_ = client_handle;

    /* no docking bars yet */
    first_bar_ = last_bar_ = 0;

    /* create our model frame */
    model_frame_ = new CTadsDockModelFrame();
}

/*
 *   destroy
 */
CTadsDockportMDI::~CTadsDockportMDI()
{
    CTadsDockportMDIBar *bar;

    /* destroy our model frame */
    delete model_frame_;

    /* tell our docking bars to forget about us */
    for (bar = first_bar_ ; bar != 0 ; bar = bar->next_mdi_bar_)
        bar->forget_frame();
}

/*
 *   get docking information for the given mouse position
 */
int CTadsDockportMDI::get_dock_info(CTadsWinDock *win,
                                    POINT mousepos, RECT *dock_pos,
                                    tads_dock_align *align) const
{
    RECT rc;
    const int range = 48;

    /* get the MDI client area */
    GetClientRect(client_handle_, &rc);

    /* if the mouse is within docking range of any edge, dock the window */
    if (mousepos.y <= range)
    {
        /* dock top */
        *dock_pos = rc;
        dock_pos->bottom = dock_pos->top + win->get_docked_height();
        *align = TADS_DOCK_ALIGN_TOP;
        return TRUE;
    }
    else if (mousepos.y >= rc.bottom - range)
    {
        /* dock bottom */
        *dock_pos = rc;
        dock_pos->top = dock_pos->bottom - win->get_docked_height();
        *align = TADS_DOCK_ALIGN_BOTTOM;
        return TRUE;
    }
    else if (mousepos.x <= range)
    {
        /* dock left */
        *dock_pos = rc;
        dock_pos->right = dock_pos->left + win->get_docked_width();
        *align = TADS_DOCK_ALIGN_LEFT;
        return TRUE;
    }
    else if (mousepos.x >= rc.right - range)
    {
        /* dock right */
        *dock_pos = rc;
        dock_pos->left = dock_pos->right - win->get_docked_width();
        *align = TADS_DOCK_ALIGN_RIGHT;
        return TRUE;
    }
    else
    {
        /* it's not within range - do not dock */
        return FALSE;
    }
}

/*
 *   query docking at a given mouse position
 */
int CTadsDockportMDI::dockport_query_dock(CTadsWinDock *win_to_dock,
                                          POINT mousepos,
                                          RECT *dock_pos) const
{
    tads_dock_align align;

    /* get the docking information for this point */
    return get_dock_info(win_to_dock, mousepos, dock_pos, &align);
}

/*
 *   Create a new docking bar
 */
CTadsDockportMDIBar *CTadsDockportMDI::
   create_bar(tads_dock_align align, const RECT *pos,
              CTadsDockModelCont *model_cont)
{
    CTadsDockportMDIBar *bar;

    /*
     *   determine the size - this is the unconstrained dimension, so we
     *   need to check the alignment to figure out which dimension to use
     */
    switch(align)
    {
    case TADS_DOCK_ALIGN_LEFT:
        bar = new CTadsDockportMDIBarLeft(this,
                                          pos->right - pos->left,
                                          model_cont);
        break;

    case TADS_DOCK_ALIGN_RIGHT:
        bar = new CTadsDockportMDIBarRight(this,
                                           pos->right - pos->left,
                                           model_cont);
        break;

    case TADS_DOCK_ALIGN_TOP:
        bar = new CTadsDockportMDIBarTop(this,
                                         pos->bottom - pos->top,
                                         model_cont);
        break;

    case TADS_DOCK_ALIGN_BOTTOM:
        bar = new CTadsDockportMDIBarBottom(this,
                                            pos->bottom - pos->top,
                                            model_cont);
        break;

    case TADS_DOCK_ALIGN_NONE:
        assert(FALSE);
        break;
    }

    /* create the bar's window, with my MDI frame as the parent */
    bar->create_system_window(mdi_frame_, mdi_frame_->get_handle(),
                              TRUE, "Docking Bar", pos,
                              new CTadsSyswin(bar));

    /*
     *   Add the bar to our list.  If a model container was specified, we
     *   must figure out where to insert the bar based on the model;
     *   otherwise, simply insert the new bar at the end of our list.
     */
    if (model_cont == 0)
    {
        /* add the bar at the end of our list */
        add_docking_bar(bar);
    }
    else
    {
        /* add the bar according to the model */
        add_docking_bar(bar, model_cont);
    }

    /* return the new bar */
    return bar;
}

/*
 *   Create a bar with the desired orientation, using the given width or
 *   height, as appropriate.
 */
CTadsDockportMDIBar *CTadsDockportMDI::
   create_bar(tads_dock_align align, int width, int height,
              class CTadsDockModelCont *model_cont)
{
    CTadsDockportMDIBar *bar;
    RECT rc;

    /* set up the rectangle with the bar's default location */
    GetClientRect(client_handle_, &rc);
    switch(align)
    {
    case TADS_DOCK_ALIGN_LEFT:
        rc.right = rc.left + width;
        break;

    case TADS_DOCK_ALIGN_RIGHT:
        rc.left = rc.right - width;
        break;

    case TADS_DOCK_ALIGN_TOP:
        rc.bottom = rc.top + height;
        break;

    case TADS_DOCK_ALIGN_BOTTOM:
        rc.top = rc.bottom - height;
        break;

    case TADS_DOCK_ALIGN_NONE:
        assert(FALSE);
        break;
    }

    /* create the bar */
    bar = create_bar(align, &rc, model_cont);

    /* return the new bar */
    return bar;
}

/*
 *   create a docking bar based on a model docking bar
 */
CTadsDockportMDIBar *CTadsDockportMDI::
   create_bar(CTadsDockModelCont *model_cont, int width, int height)
{
    /* create a bar with the container's alignment */
    return create_bar(model_cont->align_, width, height, model_cont);
}


/*
 *   dock a window
 */
void CTadsDockportMDI::dockport_dock(CTadsWinDock *win_to_dock,
                                     POINT mousepos)
{
    CTadsDockportMDIBar *bar;
    RECT dock_pos;
    tads_dock_align align;

    /* figure out where we're docking */
    if (!get_dock_info(win_to_dock, mousepos, &dock_pos, &align))
        return;

    /* create a docking bar */
    bar = create_bar(align, &dock_pos, 0);

    /* add the window as the first item to the bar */
    bar->add_docked_win_to_bar(win_to_dock, 0, mdi_frame_, TRUE);

    /* adjust the containing frame's layout for the additional docking bar */
    adjust_frame_layout(mdi_frame_->get_handle());
}

/*
 *   Auto-dock a window
 */
void CTadsDockportMDI::dockport_auto_dock(class CTadsWinDock *win_to_dock)
{
    CTadsDockportMDIBar *bar;
    int docked;

    /* we haven't figured out where to dock the window yet */
    docked = FALSE;

    /*
     *   if the window has a model window, and the model window is in a
     *   model container, put it back where it was according to the model
     *   -- this will restore the window to the same position it had (or
     *   as close to it as possible) the last time it was docked
     */
    if (win_to_dock->get_model_win() != 0
        && win_to_dock->get_model_win()->get_container() != 0)
    {
        CTadsDockModelCont *targ_cont;

        /*
         *   Search our list of docking bars to find the existing bar that
         *   corresponds to the model container.
         */
        targ_cont = win_to_dock->get_model_win()->get_container();
        for (bar = first_bar_ ; bar != 0 ; bar = bar->next_mdi_bar_)
        {
            /*
             *   if this bar is associated with the desired model bar,
             *   it's the one we want
             */
            if (bar->model_cont_ == targ_cont)
                break;
        }

        /* if we found the model bar, put the window back into it */
        if (bar != 0)
        {
            CTadsWinDocked *realwin;
            int realidx;
            CTadsDockModelWin *modwin;

            /*
             *   Search the bar's list of active windows to figure out
             *   where the model window goes in the list.  For each real
             *   window, scan the model list to determine if the target
             *   model window precedes the real model window in the model
             *   list.  When we find the last real window preceding the
             *   model window, we'll insert the new window after the model
             *   window.
             *
             *   Note that we might have to scan past the end of the real
             *   window list, since the model window could come after the
             *   last real window in the list.
             */
            for (realwin = bar->docked_win_first_, realidx = 0,
                 modwin = targ_cont->first_win_ ; ;
                 realwin = realwin->get_next_docked(), ++realidx)
            {
                CTadsDockModelWin *realmod;

                /*
                 *   note the model window associated with the real
                 *   window; if we've reached the end of the real window
                 *   list, continue scanning to the end of the model list
                 */
                realmod = (realwin == 0 ? 0 : realwin->get_model_win());

                /*
                 *   Skip real windows that don't have model windows.  If a
                 *   window doesn't have a model, it must mean that it's in
                 *   the process of being closed, in which case we can just
                 *   act like it's not here at all.
                 */
                if (realwin != 0 && realmod == 0)
                    continue;

                /*
                 *   scan ahead in the model window list until we get to
                 *   the target model window or the current real window's
                 *   model
                 */
                for ( ; modwin != 0 && modwin != realmod
                      && modwin != win_to_dock->get_model_win() ;
                      modwin = modwin->next_) ;

                /*
                 *   if we've run out of model windows without finding the
                 *   desired model, we have an inconsistency in the model
                 *   list -- a model window must always be in its
                 *   container's list
                 */
                assert(modwin != 0);

                /* decide what to do based on what we found */
                if (modwin == win_to_dock->get_model_win())
                {
                    /*
                     *   We found the model window before the current real
                     *   window -- this means that the model window
                     *   precedes the current real window in the model
                     *   list, hence we want the new real window to
                     *   precede the current real window in the real bar.
                     *
                     *   We now know exactly where to put the window: add
                     *   it to the target bar at the current index.  Note
                     *   that we don't need to update the model, since
                     *   we're simply restoring the window to the
                     *   condition specified in the model - this should
                     *   have no effect on the model, hence there's no
                     *   need to update it.
                     */
                    bar->add_docked_win_to_bar(win_to_dock, realidx,
                                               mdi_frame_, FALSE);

                    /* we're done */
                    docked = TRUE;
                    break;
                }

                /*
                 *   When we get here, it means that we've just found the
                 *   current real window in the model list.  This means
                 *   that the target model window is somewhere after this
                 *   real window.  Move on to the next real window, and
                 *   keep looking.
                 */

                /*
                 *   we should never get here with a null real window,
                 *   since we should always find the model window after
                 *   the final pass once we get to the end of the real
                 *   window list
                 */
                assert(realwin != 0);
            }
        }
        else
        {
            /*
             *   We didn't find a real docking bar corresponding to the
             *   model docking bar, so the docking bar must have been
             *   emptied out.  Create a new docking bar based on the
             *   target.
             */
            bar = create_bar(targ_cont,
                             win_to_dock->get_docked_width(),
                             win_to_dock->get_docked_height());

            /*
             *   Add the window to the bar; since we just created the bar,
             *   we know the bar is empty, hence we can just insert the
             *   new window at index 0.  Do not update the model for the
             *   bar, because we're inserting based on the model.
             */
            bar->add_docked_win_to_bar(win_to_dock, 0, mdi_frame_, FALSE);

            /* note that we've done the docking */
            docked = TRUE;
        }
    }

    /*
     *   If we haven't docked yet, it means that we don't have a model
     *   record indicating where the window should go.  Choose a docking
     *   position according to the window's alignment preference.
     */
    if (!docked)
    {
        /*
         *   Look at the window's docking alignment preference, and try to
         *   find a dockport in our list with the same alignment.  If we can't
         *   find anything that matches, create a new port that matches.
         */
        for (bar = first_bar_ ; bar != 0 ; bar = bar->next_mdi_bar_)
        {
            /*
             *   if this bar's alignment matches the window's preference,
             *   dock it in this bar
             */
            if (bar->get_docked_alignment()
                == win_to_dock->get_docked_align())
                break;
        }

        /* if we didn't find a bar, create one */
        if (bar == 0)
        {
            /* create a bar with the desired orientation */
            bar = create_bar(win_to_dock->get_docked_align(),
                             win_to_dock->get_docked_width(),
                             win_to_dock->get_docked_height(), 0);
        }

        /* add the window as the last item on the bar */
        bar->add_docked_win_to_bar(win_to_dock, bar->get_docked_win_cnt(),
                                   mdi_frame_, TRUE);
    }

    /* adjust the containing frame's layout for the additional docking bar */
    adjust_frame_layout(mdi_frame_->get_handle());
}

/*
 *   tell the parent MDI window to adjust its layout
 */
void CTadsDockportMDI::adjust_frame_layout(HWND frame_hdl)
{
    /* if the MDI window isn't minimized, adjust its size */
    if (!IsIconic(frame_hdl))
    {
        RECT rc;

        /* get the frame's current size */
        GetClientRect(frame_hdl, &rc);

        /*
         *   send a resize message to the frame so that it refigures its
         *   internal layout
         */
        PostMessage(frame_hdl, WM_SIZE,
                    (WPARAM)(IsZoomed(frame_hdl)
                             ? SIZE_MAXIMIZED : SIZE_RESTORED),
                    MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    }
}

/*
 *   handle MDI frame resizing
 */
void CTadsDockportMDI::dockport_mdi_resize(RECT *cli_rc)
{
    CTadsDockportMDIBar *cur;

    /* go through our docking bars and move each one */
    for (cur = first_bar_ ; cur != 0 ; cur = cur->next_mdi_bar_)
    {
        /*
         *   tell this bar to adjust its position and take its space out
         *   of the available space rectangle
         */
        cur->dockport_mdi_bar_resize(cli_rc);
    }
}

/*
 *   add a docking bar
 */
void CTadsDockportMDI::add_docking_bar(CTadsDockportMDIBar *bar)
{
    /* add it to the tail of the list */
    if (last_bar_ != 0)
        last_bar_->next_mdi_bar_ = bar;
    else
        first_bar_ = bar;
    last_bar_ = bar;
}

/*
 *   add a docking bar before the given bar
 */
void CTadsDockportMDI::add_docking_bar_before(CTadsDockportMDIBar *bar,
                                              CTadsDockportMDIBar *before_bar)
{
    CTadsDockportMDIBar *prv;
    CTadsDockportMDIBar *cur;

    /*
     *   if the item to add it before is null, just add it at the end of
     *   the list
     */
    if (before_bar == 0)
    {
        /* add the bar at the end of our list */
        add_docking_bar(bar);
        return;
    }

    /* find the item preceding before_bar */
    for (prv = 0, cur = first_bar_ ; cur != 0 ;
         prv = cur, cur = cur->next_mdi_bar_)
    {
        /* if this is the place, add it here */
        if (cur == before_bar)
        {
            /* link it into the list here */
            bar->next_mdi_bar_ = cur;
            if (prv != 0)
                prv->next_mdi_bar_ = bar;
            else
                first_bar_ = bar;

            /* done */
            return;
        }
    }
}

/*
 *   add a docking bar at the position indicated by the model
 */
void CTadsDockportMDI::add_docking_bar(CTadsDockportMDIBar *bar,
                                       CTadsDockModelCont *model_cont)
{
    CTadsDockportMDIBar *realbar;
    CTadsDockModelCont *modbar;

    /*
     *   We must now fix up the ordering of the new bar to match the
     *   model.  To do this, find the last actual bar whose model follows
     *   the new model bar, and then re-insert the new bar in the list
     *   just ahead of that actual bar.
     */
    for (realbar = first_bar_, modbar = model_frame_->first_cont_ ; ;
         realbar = realbar->next_mdi_bar_)
    {
        /*
         *   note the model bar associated with the real bar; if we've
         *   reached the end of the real bar list, continue scanning to
         *   the end of the model list
         */
        CTadsDockModelCont *realmod =
            (realbar == 0 ? 0 : realbar->model_cont_);

        /*
         *   scan ahead in the model bar list until we get to the target
         *   model bar or the current real bar's model
         */
        for ( ; modbar != 0 && modbar != realmod && modbar != model_cont ;
              modbar = modbar->next_) ;

        /*
         *   if we've run out of model bars without finding the desired
         *   model, we have an inconsistency in the model list -- a model
         *   bar must always be in our list somewhere
         */
        assert(modbar != 0);

        /* decide what to do based on what we found */
        if (modbar == model_cont)
        {
            /*
             *   We found the model bar before the current real bar --
             *   this means that the model bar precedes the current real
             *   bar in the model list, hence we want the new real bar to
             *   precede the current real bar in our list.
             *
             *   We now know exactly where to put the bar: add it before
             *   the current real bar.
             */
            add_docking_bar_before(bar, realbar);

            /* no need to look any further */
            break;
        }

        /*
         *   When we get here, it means that we've just found the current
         *   real bar in the model list.  This means that the target model
         *   container is somewhere after this real bar.  Move on to the
         *   next real bar, and keep looking.
         */

        /*
         *   we should never get here with a null real bar, since we
         *   should always find the model after the final pass once we get
         *   to the end of the real bar list
         */
        assert(realbar != 0);
    }
}

/*
 *   remove a docking bar
 */
void CTadsDockportMDI::remove_docking_bar(CTadsDockportMDIBar *bar)
{
    CTadsDockportMDIBar *cur;
    CTadsDockportMDIBar *prv;

    /* find it in our list */
    for (prv = 0, cur = first_bar_ ; cur != 0 ;
         prv = cur, cur = cur->next_mdi_bar_)
    {
        /* if this is the one, remove it */
        if (cur == bar)
        {
            /* unlink it */
            if (prv != 0)
                prv->next_mdi_bar_ = cur->next_mdi_bar_;
            else
                first_bar_ = cur->next_mdi_bar_;

            /* adjust the tail pointer if removing the last element */
            if (last_bar_ == cur)
                last_bar_ = prv;

            /* adjust the containing frame's layout */
            adjust_frame_layout(mdi_frame_->get_handle());

            /* no need to keep looking */
            return;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   MDI Docking Bar
 */

/*
 *   create
 */
CTadsDockportMDIBar::CTadsDockportMDIBar(class CTadsDockportMDI *frame,
                                         int siz, tads_dock_align alignment,
                                         CTadsDockModelCont *model_cont)
{
    HBITMAP hbmp;

    /* remember my frame */
    frame_ = frame;

    /* save the size */
    siz_ = siz;

    /* we're not in a list of bars yet */
    next_mdi_bar_ = 0;

    /* create our pens */
    hilite_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
    shadow_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));
    dkshadow_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));

    /* load our bitmap for drawing the size bar during tracking */
    hbmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                      MAKEINTRESOURCE(IDB_BMP_PAT50));
    size_brush_ = CreatePatternBrush(hbmp);
    DeleteObject(hbmp);

    /* not yet tracking */
    tracking_size_ = FALSE;
    tracking_split_ = FALSE;

    /* register as a docking port */
    CTadsWinDock::add_port(this);

    /* create our model container, or use the caller's if provided */
    if (model_cont != 0)
    {
        /* use the caller's container, and add a reference */
        model_cont_ = model_cont;
        model_cont_->add_ref();
    }
    else
    {
        /* create a new container (which will count our reference) */
        model_cont_ = new CTadsDockModelCont(frame->get_model_frame(),
                                             alignment);
    }
}

/*
 *   destroy
 */
CTadsDockportMDIBar::~CTadsDockportMDIBar()
{
    CTadsWinDocked *cur;

    /* unregister as a docking port */
    CTadsWinDock::remove_port(this);

    /* delete our drawing objects */
    DeleteObject(hilite_pen_);
    DeleteObject(shadow_pen_);
    DeleteObject(dkshadow_pen_);
    DeleteObject(size_brush_);

    /* tell the frame I'm gone, if it hasn't been deleted already */
    if (frame_ != 0)
        frame_->remove_docking_bar(this);

    /*
     *   when I'm deleted, tell any remaining children to forget about me
     *   -- when I get deleted before my children do, there's no need for
     *   them to notify me when they're deleted
     */
    for (cur = docked_win_first_ ; cur != 0 ; cur = cur->get_next_docked())
        cur->forget_dock_container();

    /*
     *   Note that we do not delete our model container, because we want the
     *   model to survive even after the actual docking bar is deleted; this
     *   allows a window that was once docked in this frame to come back to
     *   the same place it was previously if it's re-docked using
     *   auto-docking.
     *
     *   This does not cause a memory leak, because the model container is
     *   tracked by the model frame, and will be deleted when the model frame
     *   is deleted.  In addition, the model container will be deleted
     *   automatically if it becomes empty, which will happen if all of its
     *   model windows are eventually redocked somewhere else; the amount of
     *   memory we're tying up is thus limited by the number of docking
     *   windows that were ever docked here.
     *
     *   So, all we need to do is remove our reference from the model
     *   container.
     */
    if (model_cont_ != 0)
        model_cont_->release_ref();
}

/*
 *   resize
 */
void CTadsDockportMDIBar::dockport_mdi_bar_resize(RECT *avail_area)
{
    RECT rc;

    /* start off using the full available area */
    rc = *avail_area;

    /* don't let our size go below a minimum */
    if (siz_ < 20)
        siz_ = 20;

    /* calculate where we go based on the available area */
    calc_resize(&rc);

    /* subtract the area we're using from the remaining available area */
    SubtractRect(avail_area, avail_area, &rc);

    /* move to our new position */
    MoveWindow(handle_, rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

/*
 *   Resize
 */
int CTadsDockportMDIBar::do_windowposchanging(WINDOWPOS *wp)
{
    RECT new_rc;
    RECT old_rc;
    CTadsWinDocked *cur;

    /*
     *   Find the current position of our last window - this will tell us the
     *   current layout size, which is our reference point for calculating
     *   shares of the new size.  We don't always change the subwindow layout
     *   when we resize the bar window (for example, when minimizing), so our
     *   basis isn't necessarily the actual window size, but rather the
     *   layout size reflected in our subwindow.
     */
    for (cur = docked_win_first_ ; cur != 0 && cur->get_next_docked() != 0 ;
         cur = cur->get_next_docked()) ;
    if (cur != 0)
    {
        /* use the position of the last window in terms of our client area */
        GetWindowRect(cur->get_handle(), &old_rc);
        MapWindowPoints(HWND_DESKTOP, handle_, (POINT *)&old_rc, 2);

        /* that's the bottom right point; our top left is always 0,0 */
        old_rc.left = old_rc.top = 0;
    }
    else
    {
        /* no subwindows; just use our own original size */
        GetClientRect(handle_, &old_rc);
    }

    /* set up a rect for the new size */
    SetRect(&new_rc, 0, 0, wp->cx, wp->cy);

    /*
     *   If the size is changing, refigure the layout.  However, don't
     *   refigure the layout if the size is going down to zero or coming back
     *   from zero, which means that the window is being minimized or
     *   restored from being minimized.  The zero size when minimized would
     *   set the proportional base to zero, which screws up the layout; it's
     *   easiest to just ignore minimization transitions.
     */
    if ((wp->cx != old_rc.right || wp->cy != old_rc.bottom)
        && wp->cx != 0 && wp->cy != 0
        && old_rc.right != 0 && old_rc.bottom != 0)
        calc_win_resize(old_rc, new_rc);

    /* inherit default windows behavior */
    return FALSE;
}

/*
 *   Calculate the layout of our subwindows for a change in the overall
 *   bar size, from the given old size to the given new size.
 */
void CTadsDockportMDIBar::calc_win_resize(RECT old_rc, RECT new_rc)
{
    CTadsWinDocked *cur;
    RECT rem;

    /* inset both rectangles for borders and controls */
    calc_insets(&old_rc, docked_win_cnt_);
    calc_insets(&new_rc, docked_win_cnt_);

    /* start out with the whole new bar available */
    rem = new_rc;

    /*
     *   Resize our windows.  Keep the free dimension the same; add or
     *   subtract space from each subwindow along the constrained
     *   dimension in proportion to its current size.
     */
    for (cur = docked_win_first_ ; cur != 0 ; cur = cur->get_next_docked())
    {
        RECT new_win_rc;

        /*
         *   if this is the last window, give it all of the remaining
         *   space; otherwise, give it new space proportionally to its old
         *   share
         */
        if (cur->get_next_docked() == 0)
        {
            /* this is the last window - give it the remaining space */
            new_win_rc = rem;
        }
        else
        {
            RECT old_win_rc;

            /* get this window's old size */
            GetWindowRect(cur->get_handle(), &old_win_rc);

            /* there's another window - allocate this window its share */
            alloc_win_share(&new_win_rc, &old_win_rc,
                            &new_rc, &old_rc, &rem);
        }

        /* move this window to its new position */
        MoveWindow(cur->get_handle(),
                   new_win_rc.left, new_win_rc.top,
                   new_win_rc.right - new_win_rc.left,
                   new_win_rc.bottom - new_win_rc.top, TRUE);
    }
}

/*
 *   paint the window's contents
 */
void CTadsDockportMDIBar::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    RECT rc;
    RECT bar_rc;
    HPEN oldpen;
    int ofs;
    CTadsWinDocked *cur;

    /* fill the background with gray */
    FillRect(hdc, area_to_draw, GetSysColorBrush(COLOR_3DFACE));

    /* get our client area */
    GetClientRect(handle_, &rc);
    rc.right++;
    rc.bottom++;

    /* start drawing the hilite line */
    oldpen = (HPEN)SelectObject(hdc, hilite_pen_);

    /* draw the sizing bar */
    bar_rc = rc;
    get_sizing_bar_rect(&bar_rc);
    draw_sizing_bar(hdc, &bar_rc);

    /*
     *   draw the splitter bars - there's a splitter after each window
     *   except the last one
     */
    for (ofs = 0, cur = docked_win_first_ ;
         cur != 0 && cur->get_next_docked() != 0 ;
         cur = cur->get_next_docked())
    {
        /* move past this window */
        ofs += cur->get_docked_extent();

        /* draw the splitter after this window */
        draw_splitter(hdc, ofs);

        /* advance past this splitter */
        ofs += MDI_SPLITTER_SIZE + 2;
    }

    /* restore the old pen */
    SelectObject(hdc, oldpen);
}

/*
 *   left click
 */
int CTadsDockportMDIBar::do_leftbtn_down(int keys, int x, int y, int clicks)
{
    /* if it's in the sizing bar, start a resize */
    if (over_sizing_widget(x, y))
    {
        /* capture the mouse */
        SetCapture(handle_);

        /* note that we're tracking */
        tracking_size_ = TRUE;

        /* set the initial mouse position */
        track_x_ = x;
        track_y_ = y;

        /* start tracking at the original size */
        track_siz_ = siz_;

        /* draw the sizing bar at the current location */
        xor_size_bar();

        /* keep the sizing cursor during tracking */
        SetCursor(sizing_csr_);

        /* handled */
        return TRUE;
    }

    /* if it's over a splitter, start a split resize */
    if ((track_splitwin_ = find_splitter(x, y)) != 0)
    {
        /* capture the mouse */
        SetCapture(handle_);

        /* note that we're tracking */
        tracking_split_ = TRUE;

        /* set the initial mouse position */
        track_x_ = track_start_x_ = x;
        track_y_ = track_start_y_ = y;

        /* draw the splitter bar at the current location */
        xor_splitter();

        /* keep the splitter cursor during tracking */
        SetCursor(split_csr_);

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_leftbtn_down(keys, x, y, clicks);
}

/*
 *   left button release
 */
int CTadsDockportMDIBar::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're tracking something, release capture */
    if (tracking_size_ || tracking_split_)
    {
        /* release the capture */
        ReleaseCapture();

        /* handled */
        return TRUE;
    }

    /* inherit default */
    return CTadsWin::do_leftbtn_up(keys, x, y);
}

/*
 *   end capture
 */
int CTadsDockportMDIBar::do_capture_changed(HWND new_capture_win)
{
    /* if we're tracking the size bar, finish it */
    if (tracking_size_)
    {
        /* remove the sizer drawing */
        xor_size_bar();

        /* no longer tracking the sizer */
        tracking_size_ = FALSE;

        /* establish the size at the end of the tracking */
        siz_ = track_siz_;

        /* resize the parent */
        CTadsDockportMDI::adjust_frame_layout(GetParent(handle_));
    }

    /* if we're tracking a splitter, finish it */
    if (tracking_split_)
    {
        RECT rc;
        RECT rc_prv;
        RECT rc_nxt;
        CTadsWinDocked *nxtwin;

        /* remove the splitter drawing */
        xor_splitter();

        /* no longer tracking */
        tracking_split_ = FALSE;

        /*
         *   invalidate the whole docking bar - the only thing we really
         *   need to redraw is the splitters, since our subwindows cover
         *   up everything else
         */
        inval();

        /* get the current size of the preceding window */
        GetClientRect(track_splitwin_->get_handle(), &rc_prv);

        /* get the next window */
        nxtwin = track_splitwin_->get_next_docked();

        /* get the current size of the next window */
        GetClientRect(nxtwin->get_handle(), &rc_nxt);

        /* apply the splitter to each window */
        resize_for_split_track(&rc_prv, &rc_nxt);

        /* move the preceding window */
        SetWindowPos(track_splitwin_->get_handle(), 0, 0, 0,
                     rc_prv.right, rc_prv.bottom,
                     SWP_NOMOVE | SWP_NOZORDER);

        /* set its height or width */
        track_splitwin_->splitter_resize(rc_prv.right, rc_prv.bottom);

        /* move the following window */
        SetWindowPos(nxtwin->get_handle(), 0, 0, 0,
                     rc_nxt.right, rc_nxt.bottom,
                     SWP_NOMOVE | SWP_NOZORDER);

        /* set its height or width */
        nxtwin->splitter_resize(rc_nxt.right, rc_nxt.bottom);

        /* refigure the layout */
        GetClientRect(handle_, &rc);
        calc_win_resize(rc, rc);
    }

    /* inherit default */
    return CTadsWin::do_capture_changed(new_capture_win);
}

/*
 *   xor the sizing bar into the frame window to show the new size during
 *   tracking
 */
void CTadsDockportMDIBar::xor_size_bar()
{
    POINT pt;
    RECT prc;
    RECT rc;
    HDC hdc;
    HPEN oldpen;
    HBRUSH oldbr;
    int oldrop;

    /* get my client area */
    GetClientRect(handle_, &rc);

    /* figure out where it goes */
    get_sizing_bar_rect(&rc);
    offset_sizing_bar_rect_tracking(&rc);

    /*
     *   adjust to window-relative coordinates in the parent window, since
     *   we'll be drawing the parent's window DC
     */
    GetWindowRect(GetParent(handle_), &prc);
    pt.x = pt.y = 0;
    ClientToScreen(handle_, &pt);
    OffsetRect(&rc, pt.x - prc.left, pt.y - prc.top);

    /* get the parent's drawing dc */
    hdc = GetWindowDC(GetParent(handle_));

    /* set up our brush, pen, and XOR drawing mode */
    oldbr = (HBRUSH)SelectObject(hdc, size_brush_);
    oldpen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    oldrop = SetROP2(hdc, R2_XORPEN);

    /* draw the sizing bar */
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    /* restore drawing modes */
    SetROP2(hdc, oldrop);
    SelectObject(hdc, oldpen);
    SelectObject(hdc, oldbr);

    /* release the parent's dc */
    ReleaseDC(GetParent(handle_), hdc);
}

/*
 *   xor the splitter during mouse tracking
 */
void CTadsDockportMDIBar::xor_splitter()
{
    POINT pt;
    RECT prc;
    RECT rc;
    HDC hdc;
    HPEN oldpen;
    HBRUSH oldbr;
    int oldrop;
    CTadsWinDocked *nxtwin;
    RECT rc_prv, rc_prv_orig, rc_nxt;

    /* get the current sizes of the windows affected by the split */
    GetClientRect(track_splitwin_->get_handle(), &rc_prv);
    rc_prv_orig = rc_prv;
    nxtwin = track_splitwin_->get_next_docked();
    GetClientRect(nxtwin->get_handle(), &rc_nxt);

    /* calculate the new size after the split */
    resize_for_split_track(&rc_prv, &rc_nxt);

    /*
     *   Get the splitter's location - it's after the preceding window.
     *   First, get the preceding window in coordinates relative to our
     *   bar window.
     */
    GetWindowRect(handle_, &prc);
    GetWindowRect(track_splitwin_->get_handle(), &rc);
    OffsetRect(&rc, -prc.left, -prc.top);

    /* now move the rectangle to the end of the preceding window */
    offset_by_win_extent(&rc, track_splitwin_->get_docked_extent());

    /* set the rectangle to the size of a splitter */
    set_splitter_size(&rc);

    /* offset by the current tracking offset */
    offset_splitter_rect_tracking(
        &rc, rc_prv.right - rc_prv_orig.right,
        rc_prv.bottom - rc_prv_orig.bottom);

    /*
     *   adjust to window-relative coordinates in the parent window, since
     *   we'll be drawing the parent's window DC
     */
    GetWindowRect(GetParent(handle_), &prc);
    pt.x = pt.y = 0;
    ClientToScreen(handle_, &pt);
    OffsetRect(&rc, pt.x - prc.left, pt.y - prc.top);

    /* get the parent's drawing dc */
    hdc = GetWindowDC(GetParent(handle_));

    /* set up our brush, pen, and XOR drawing mode */
    oldbr = (HBRUSH)SelectObject(hdc, size_brush_);
    oldpen = (HPEN)SelectObject(hdc, (HPEN)GetStockObject(NULL_PEN));
    oldrop = SetROP2(hdc, R2_XORPEN);

    /* draw the splitter */
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

    /* restore drawing modes */
    SetROP2(hdc, oldrop);
    SelectObject(hdc, oldpen);
    SelectObject(hdc, oldbr);

    /* release the parent's dc */
    ReleaseDC(GetParent(handle_), hdc);
}


/*
 *   track mouse movement
 */
int CTadsDockportMDIBar::do_mousemove(int keys, int x, int y)
{
    /* if we're tracking the size bar, continue tracking it */
    if (tracking_size_)
    {
        /* remove the old size bar */
        xor_size_bar();

        /* adjust for the movement of the mouse */
        adjust_size_tracking(x - track_x_, y - track_y_);

        /* draw in the new position */
        xor_size_bar();

        /* remember the new size */
        track_x_ = x;
        track_y_ = y;

        /* keep the split cursor during tracking */
        SetCursor(sizing_csr_);

        /* handled */
        return TRUE;
    }

    /* if tracking the pslitter, continue tracking it */
    if (tracking_split_)
    {
        /* remove the old bar */
        xor_splitter();

        /* update the tracking position */
        track_x_ = x;
        track_y_ = y;

        /* draw in the new position */
        xor_splitter();

        /* handled */
        return TRUE;
    }

    /* if we're over the sizing widget, set a sizing cursor */
    if (over_sizing_widget(x, y))
    {
        /* set the appropriate sizing cursor */
        SetCursor(sizing_csr_);

        /* handled */
        return TRUE;
    }

    /* if we're over a splitter, set a splitting cursor */
    if (find_splitter(x, y) != 0)
    {
        /* set the appropriate splitting cursor */
        SetCursor(split_csr_);

        /* handled */
        return TRUE;
    }

    /* inherit the default */
    return CTadsWin::do_mousemove(keys, x, y);
}

/*
 *   Determine if we're in a splitter bar.  Returns the window preceding
 *   the splitter in the list if the point is indeed within a splitter.
 *   Returns null if the point is not within a splitter.
 */
CTadsWinDocked *CTadsDockportMDIBar::find_splitter(int x, int y) const
{
    CTadsWinDocked *cur;
    RECT rc;
    POINT pt;

    /* set up a point with the mouse position */
    pt.x = x;
    pt.y = y;

    /* see if we're over a sizing bar */
    GetClientRect(handle_, &rc);
    set_splitter_size(&rc);
    for (cur = docked_win_first_ ; cur != 0 ; cur = cur->get_next_docked())
    {
        /* offset our rectangle past this window */
        offset_by_win_extent(&rc, cur->get_docked_extent());

        /* is the point within this splitter? */
        if (PtInRect(&rc, pt))
            return cur;

        /* offset our rectangle past this splitter */
        offset_by_win_extent(&rc, MDI_SPLITTER_SIZE + 2);
    }

    /* we didn't find a splitter containing the point */
    return 0;
}

/*
 *   determine if a mouse position is over the sizing widget
 */
int CTadsDockportMDIBar::over_sizing_widget(int x, int y) const
{
    RECT rc;
    POINT pt;

    /* get the area of the sizing widget */
    GetClientRect(handle_, &rc);
    get_sizing_bar_rect(&rc);

    /* if it's in this rectangle, it's in the widget */
    pt.x = x;
    pt.y = y;
    return PtInRect(&rc, pt);
}

/*
 *   receive notification that we've just removed a docked window from my
 *   list
 */
void CTadsDockportMDIBar::remove_docked_win(CTadsWinDocked *win)
{
    /* inherit default */
    CTadsDockedCont::remove_docked_win(win);

    /* if this is the last window, delete myself */
    if (docked_win_cnt_ == 0)
    {
        /*
         *   We've removed my last child window, so this docking bar must
         *   now disappear - close myself.  Don't simply destroy this
         *   window, because it can create ordering problems when we're
         *   destroying children in the course of destroying; instead,
         *   post a close message to myself.
         */
        PostMessage(handle_, WM_CLOSE, 0, 0);

        /*
         *   Even though we're not destroying the window immediately,
         *   remove myself from my frame right away - this will ensure
         *   that no one tries to add something to this window when it's
         *   about to be deleted.  A user shouldn't be able to add
         *   anything, but it's possible that another layer of code could
         *   try to add something programmatically, such as during loading
         *   of a saved configuration.
         */
        if (frame_ != 0)
        {
            /* tell my frame not to consider me a valid target any more */
            frame_->remove_docking_bar(this);

            /* we're unlinked from the frame now, so forget about it */
            frame_ = 0;
        }

        /* we're done */
        return;
    }

    /* recalculate the layout for the removal of this window */
    calc_layout_for_removal(win);
}

/*
 *   Recalculate the layout after adding or removing windows.
 *
 *   If any windows have target sizes smaller than even proportional shares,
 *   we leave the smaller windows at their target sizes and distribute the
 *   remaining space to the windows with target sizes larger than the
 *   proportional shares.  Otherwise, we just give the windows proportional
 *   shares.
 */
void CTadsDockportMDIBar::recalc_layout()
{
    RECT client_rc;
    int extent, prop, proprem;
    int tot, extra, extrarem, extracnt;
    CTadsWinDocked *cur, *maxwin;
    RECT rc;

    /* if we have no windows, there's nothing to do */
    if (docked_win_cnt_ == 0)
        return;

    /* get our available space, omitting splitters and the title bar */
    GetClientRect(handle_, &client_rc);
    calc_insets(&client_rc, docked_win_cnt_);
    extent = get_bar_extent(&client_rc);

    /* calculate the proportional size, giving each window equal shares */
    prop = extent / docked_win_cnt_;
    proprem = extent % docked_win_cnt_;

    /*
     *   Add up the target sizes, to see if the windows all fit at their
     *   target sizes.
     */
    for (tot = 0, maxwin = 0, cur = docked_win_first_ ; cur != 0 ;
         cur = cur->get_next_docked())
    {
        /* get this window's size */
        int siz = cur->get_target_extent();

        /* if this is the largest window, note it */
        if (maxwin == 0 || siz > maxwin->get_target_extent())
            maxwin = cur;

        /* add this size to the total */
        tot += siz;
    }

    /*
     *   If all the windows fit at their target sizes, simply set all the
     *   windows to their target sizes and give any extra space to the
     *   largest window.
     */
    if (tot <= extent)
    {
        /* give each window its target size */
        rc = client_rc;
        for (cur = docked_win_first_ ; cur != 0 ;
             cur = cur->get_next_docked())
        {
            int siz;

            /* give this window its exact target size */
            siz = cur->get_target_extent();

            /* if this is the largest window, give it any extra space */
            if (cur == maxwin)
                siz += extent - tot;

            /* figure the window rectangle */
            set_win_extent(&rc, siz);

            /* resize the system window */
            MoveWindow(cur->get_handle(), rc.left, rc.top,
                       rc.right - rc.left, rc.bottom - rc.top, TRUE);

            /* move past this window */
            offset_by_win_extent(&rc, siz + MDI_SPLITTER_SIZE + 2);
        }

        /* done */
        return;
    }

    /*
     *   Look to see if there are any windows with individual target sizes
     *   smaller than their proportional shares.  If there are, we'll leave
     *   those windows at their target sizes, and distribute the extra space
     *   to the remaining windows.  Otherwise, we'll simply give each window
     *   equal shares.
     */
    for (extra = 0, extracnt = 0, maxwin = 0, cur = docked_win_first_ ;
         cur != 0 ; cur = cur->get_next_docked())
    {
        int siz, curprop;

        /* get the target size for this window */
        siz = cur->get_target_extent();

        /* use another pixel of remainder, if we have any left */
        curprop = prop;
        if (proprem != 0)
            ++curprop, --proprem;

        /*
         *   If this target size is smaller than the proportional size, give
         *   the extra back to the pool to use in windows that are larger.
         *   Otherwise, count this as a big window.
         */
        if (siz < curprop)
            extra += curprop - siz;
        else
            extracnt += 1;
    }

    /* reset the proportional remainder counter*/
    proprem = extent % docked_win_cnt_;

    /* calculate the proportional extra shares, if any */
    if (extracnt != 0)
    {
        extrarem = extra % extracnt;
        extra /= extracnt;
    }
    else
    {
        /*
         *   there are no windows bigger than the proportional shares, so
         *   give all the extra space to the largest window
         */
        extrarem = 0;
    }

    /* now distribute space to each window */
    rc = client_rc;
    for (cur = docked_win_first_ ; cur != 0 ; cur = cur->get_next_docked())
    {
        int siz, curprop;

        /* get the target size for this window */
        siz = cur->get_target_extent();

        /* use another pixel of remainder, if we have any left */
        curprop = prop;
        if (proprem != 0)
            ++curprop, --proprem;

        /* check the target size against the proportional size */
        if (siz < curprop)
        {
            /*
             *   This window's target size is smaller than the available
             *   proportional share, so just use this window's exact target
             *   size.  If there are no windows larger than their
             *   proportional sizes, give all of the extra space to the
             *   largest window.
             */
            if (extracnt == 0 && cur == maxwin)
                siz += extra;
        }
        else
        {
            /*
             *   This window's target size is larger than the proportional
             *   share available.  Give the window its proportional size plus
             *   its share of the extra space, if any.
             */
            siz = curprop + extra;
            if (extrarem != 0)
                ++siz, --extrarem;
        }

        /* figure the window rectangle */
        set_win_extent(&rc, siz);

        /* resize the system window */
        MoveWindow(cur->get_handle(), rc.left, rc.top,
                   rc.right - rc.left, rc.bottom - rc.top, TRUE);

        /* move past this window */
        offset_by_win_extent(&rc, siz + MDI_SPLITTER_SIZE + 2);
    }
}

/*
 *   Calculate the layout after the removal of the given window
 */
void CTadsDockportMDIBar::calc_layout_for_removal(CTadsWinDocked *win)
{
#if 1
    recalc_layout();
#else
    // old way - not needed now that we do the generic layout calculation
    RECT removed_rc;
    RECT full_win_rc;
    RECT old_win_rc;
    int target;
    int extent;

    /* note the size of the window we're removing */
    GetClientRect(win->get_handle(), &removed_rc);

    /*
     *   Adjust the window layout for the new size after removing the
     *   given window.  To do this, act as though we're stretching the
     *   window from a bar big enough to contain only the remaining
     *   windows, to the current actual bar size; this will give back
     *   space to the windows in proportion to their sizes.
     */
    GetClientRect(handle_, &full_win_rc);
    old_win_rc = full_win_rc;
    sub_win_extent(&old_win_rc, &removed_rc);
    calc_win_resize(old_win_rc, full_win_rc);
#endif
}

/*
 *   Get docking information for docking a window in this bar.  Provides
 *   the index of the new window.  Returns true if we can dock here, false
 *   if not.
 */
int CTadsDockportMDIBar::get_dock_info(CTadsWinDock *win_to_dock,
                                       POINT mousepos, RECT *dock_pos,
                                       int *idx) const
{
    int new_cnt;
    int i;
    int ofs;
    CTadsWinDocked *cur;
    int is_ours;

    /* determine if this is one of our own windows */
    is_ours = ((const CTadsDockedCont *)win_to_dock->get_dock_container()
               == this);

    /*
     *   Figure out how many windows we'll have docked after docking this
     *   window.  If the window is already among our docked windows, we're
     *   just moving it around, in which case the total number of windows
     *   won't change; otherwise, the total number of windows will
     *   increase by one.
     */
    new_cnt = docked_win_cnt_;
    if (!is_ours)
        ++new_cnt;

    /*
     *   Set the initial docking size.  If this is one of our own windows,
     *   use its current shape; otherwise, set the initial docking size to
     *   1/n of the available window size, where n is the number of
     *   windows we'll have after docking this new window.
     */
    if (is_ours)
    {
        /*
         *   it's one of ours - we're just going to shuffle it to a new
         *   position if it ends up back in our own bar, so use its
         *   current size as the drag shape
         */
        GetClientRect(win_to_dock->get_handle(), dock_pos);
    }
    else
    {
        /*
         *   this window is not already in our bar, so figure out what it
         *   would look like if we were to add it
         */
        get_new_win_shape(dock_pos, new_cnt,
                          win_to_dock->get_docked_width(),
                          win_to_dock->get_docked_height());
    }

    /*
     *   Now figure out where the window goes.  First, determine which
     *   existing subwindow it's over.  Next, determine whether it's in
     *   the first or second half of that subwindow; if it's in the first
     *   half, it goes before that subwindow, otherwise after it.
     */
    for (i = 0, ofs = 0, cur = docked_win_first_ ; cur != 0 ;
         cur = cur->get_next_docked(), ++i)
    {
        int second_half;
        int old_ofs;

        /* remember the previous offset */
        old_ofs = ofs;

        /* see if it's in the extent of this window */
        if (pt_in_win_extent(cur, mousepos, &ofs, &second_half))
        {
            /*
             *   the mouse is in this window's extent -- assume we'll put
             *   the new window just before this window
             */
            *idx = i;

            /*
             *   If this is the last current window in the bar, and the
             *   mouse is in the second half of this window, put the new
             *   window after the current window rather than before it.
             */
            if (second_half && cur->get_next_docked() == 0)
            {
                RECT bar_rc;

                /* go to the next index position */
                ++(*idx);

                /* advance the window to the next position */
                offset_by_win_extent(dock_pos, ofs - old_ofs);

                /*
                 *   if this puts us past the end of bar, show it at the
                 *   right or bottom edge of the bar
                 */
                GetClientRect(handle_, &bar_rc);
                if (dock_pos->right > bar_rc.right)
                    OffsetRect(dock_pos, bar_rc.right - dock_pos->right, 0);
                else if (dock_pos->bottom > bar_rc.bottom)
                    OffsetRect(dock_pos, 0, bar_rc.bottom - dock_pos->bottom);
            }

            /* indicate that we found a docking position */
            return TRUE;
        }

        /* offset the destination location past this window */
        offset_by_win_extent(dock_pos, ofs - old_ofs);
    }

    /* we didn't find a spot for it to dock */
    return FALSE;
}

/*
 *   try docking
 */
int CTadsDockportMDIBar::dockport_query_dock(CTadsWinDock *win_to_dock,
                                             POINT mousepos,
                                             RECT *dock_pos) const
{
    int idx;

    /* get our docking information for this position */
    return get_dock_info(win_to_dock, mousepos, dock_pos, &idx);
}

/*
 *   do docking
 */
void CTadsDockportMDIBar::dockport_dock(CTadsWinDock *win_to_dock,
                                        POINT mousepos)
{
    int idx;
    RECT docked_pos;

    /* get my docking information */
    if (!get_dock_info(win_to_dock, mousepos, &docked_pos, &idx))
        return;

    /* add the new window */
    add_docked_win_to_bar(win_to_dock, idx, frame_->get_frame_win(), TRUE);
}

/*
 *   Add a docked window at the given index.  The window is inserted
 *   before any existing window at the given index, so that, when we're
 *   done, the new window has the given index.
 */
void CTadsDockportMDIBar::add_docked_win_to_bar(CTadsWinDock *win_to_dock,
                                                int idx,
                                                CTadsWin *frame_win,
                                                int update_model)
{
    RECT undocked_pos;
    RECT mdi_pos;
    int has_closebox;
    char title_buf[256];
    RECT docked_pos;

    /*
     *   if this window isn't already one of mine, create a new container
     *   window for it
     */
    if (win_to_dock->get_dock_container() != this)
    {
        CTadsWinDocked *docked_win;
        RECT cli_rc;
        RECT small_cli_rc;

        /* get the original undocked size of the window */
        win_to_dock->get_undocked_pos(&undocked_pos);
        win_to_dock->get_mdi_pos(&mdi_pos);

        /* determine if the undocked window has its close box enabled */
        has_closebox = win_to_dock->has_closebox();

        /* get the undocked title */
        win_to_dock->get_undocked_title(title_buf, sizeof(title_buf));

        /* create a docked window within the bar */
        docked_win =
            new CTadsWinDocked(this, get_win_orientation(),
                               win_to_dock->get_docked_width(),
                               win_to_dock->get_docked_height(),
                               &undocked_pos, &mdi_pos, title_buf,
                               frame_win, frame_win->get_handle(),
                               has_closebox, win_to_dock->get_model_win());

        /* figure out the size to allocate to the new window */
        get_new_win_shape(&docked_pos, docked_win_cnt_ + 1,
                          win_to_dock->get_docked_width(),
                          win_to_dock->get_docked_height());

        /*
         *   if it's not going to be the last window, add space for the
         *   splitter that will be right after it
         */
        if (idx < docked_win_cnt_)
            add_win_extent(&docked_pos, MDI_SPLITTER_SIZE + 2);

        /*
         *   resize the existing windows to make room - compute how much
         *   room we have left by subtracting the new window's extent from
         *   the full client area, then do a normal resize to adjust the
         *   existing windows to the smaller available space
         */
        GetClientRect(handle_, &cli_rc);
        small_cli_rc = cli_rc;
        sub_win_extent(&small_cli_rc, &docked_pos);

        /*
         *   This is a bit confusing, but if this is the last window, we
         *   need to compensate for the fact that we're adding another
         *   splitter bar to the existing windows.  This is only an issue
         *   when we're adding a new window at the last position and there
         *   will be more than one window when we're done: if this is
         *   going to be the only window, there will be no splitter, and
         *   if this is not going to be the last window, we already
         *   counted its splitter in its size hence don't need to subtract
         *   it out again.  In this one case where we're adding a window
         *   at the end of the list and we'll have more than one window
         *   when we're done, subtract out the size of a splitter from the
         *   remaining window extents for the existing windows.
         */
        if (idx >= docked_win_cnt_ && docked_win_cnt_ > 0)
            add_win_extent(&small_cli_rc, -(MDI_SPLITTER_SIZE + 2));

        /* calculate the resize to the smaller size */
        calc_win_resize(cli_rc, small_cli_rc);

        /* create the docked system window */
        docked_win->create_system_window(this, TRUE, "Docked Window",
                                         &docked_pos);

        /* add the docked window to my list at the desired index */
        add_docked_win(docked_win, idx);

        /* bring the frame window to the front */
        BringWindowToTop(frame_win->get_handle());

        /* move the contents to the new docked window */
        win_to_dock->reparent_subwin(docked_win);

        /* if desired, update the model with the window's new position */
        if (update_model)
        {
            CTadsWinDocked *docked_nxt;

            /* find the next docked window in the actual list */
            docked_nxt = docked_win->get_next_docked();

            /*
             *   add the model window to the model container before the
             *   model window associated with the next real docked window
             */
            model_cont_->add_win_before(docked_win->get_model_win(),
                                        (docked_nxt == 0
                                         ? 0 : docked_nxt->get_model_win()));
        }

        /* recalculate the layout */
        recalc_layout();

        /*
         *   do a final resize to fix everything up to the correct layout,
         *   now that we've added the new window to our list
         */
        calc_win_resize(cli_rc, cli_rc);
    }
    else
    {
        int i;
        RECT cli_rc;
        CTadsWinDocked *docked_win;
        CTadsWinDocked *docked_nxt;

        /*
         *   This window is already in this docking bar, so we're merely
         *   re-ordering the docking bar.  Remove the window from its
         *   current position in the list and re-insert it at the desired
         *   new position, then fix up the layout of the windows in the
         *   bar.  Nothing needs to be resized, since we're just shuffling
         *   the existing layout.
         */

        /*
         *   the window is docked in this bar, so it must be a docked
         *   window subclass
         */
        docked_win = (CTadsWinDocked *)win_to_dock;

        /* unlink this window from the list */
        i = unlink_docked_win(docked_win);

        /*
         *   If the new index is greater than the old index, decrement the
         *   new index to compensate for the removal of a window at a
         *   lower index.  However, there is one special case: if the new
         *   index is one greater than the old index, do not decrement it
         *   - in this case, it is intuitive to the user that the window
         *   should swap places with the one that immediately follows it,
         *   hence we want effectively an insert-after, thus we want the
         *   index to be one higher than we have it now, hence we do not
         *   want to decrement it.
         */
        if (idx > i && idx != i + 1)
            --idx;

        /* re-insert the element at the desired index */
        add_docked_win(docked_win, idx);

        /*
         *   Move the model window to its new position in the model list.
         *   If the index didn't actually change, don't update the model.
         */
        if (i != idx)
        {
            /* get the next docked window in our list */
            docked_nxt = docked_win->get_next_docked();

            /* re-insert the model before the next real window's model */
            model_cont_->add_win_before(docked_win->get_model_win(),
                                        (docked_nxt == 0
                                         ? 0 : docked_nxt->get_model_win()));
        }

        /*
         *   recalculate the layout without changing the size, so that the
         *   windows move to their proper positions for the new ordering
         */
        GetClientRect(handle_, &cli_rc);
        calc_win_resize(cli_rc, cli_rc);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Horizontal MDI Docking Bar
 */

CTadsDockportMDIBarH::CTadsDockportMDIBarH(class CTadsDockportMDI *frame,
                                           int siz, tads_dock_align alignment,
                                           CTadsDockModelCont *model_cont)
    : CTadsDockportMDIBar(frame, siz, alignment, model_cont)
{
    /* load our cursors */
    sizing_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                             MAKEINTRESOURCE(IDC_SPLIT_NS_CSR));
    split_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDC_SPLIT_EW_CSR));
}

/*
 *   determine if a point is within a subwindow's extent
 */
int CTadsDockportMDIBarH::pt_in_win_extent(class CTadsWinDocked *win,
                                           POINT mousepos, int *ofs,
                                           int *second_half) const
{
    RECT rc;
    int wid;
    int ret;

    /* get this window's width, including the splitter bar */
    GetClientRect(win->get_handle(), &rc);
    wid = rc.right - rc.left + MDI_SPLITTER_SIZE + 2;

    /* check if the mouse position is horizontally within this window */
    ret = (mousepos.x >= *ofs && mousepos.x < *ofs + wid);

    /* if we're in the window, determine which half we're in horizontally */
    *second_half = (mousepos.x >= *ofs + wid/2);

    /* advance the offset by this width */
    *ofs += wid;

    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Vertical MDI Docking Bar
 */

CTadsDockportMDIBarV::CTadsDockportMDIBarV(class CTadsDockportMDI *frame,
                                           int siz, tads_dock_align alignment,
                                           CTadsDockModelCont *model_cont)
    : CTadsDockportMDIBar(frame, siz, alignment, model_cont)
{
    /* load our cursor */
    sizing_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                             MAKEINTRESOURCE(IDC_SPLIT_EW_CSR));
    split_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDC_SPLIT_NS_CSR));
}

/*
 *   determine if a point is within a subwindow's extent
 */
int CTadsDockportMDIBarV::pt_in_win_extent(class CTadsWinDocked *win,
                                           POINT mousepos, int *ofs,
                                           int *second_half) const
{
    RECT rc;
    int ht;
    int ret;

    /* get this window's height, including the splitter bar */
    GetClientRect(win->get_handle(), &rc);
    ht = rc.bottom - rc.top + MDI_SPLITTER_SIZE + 2;

    /* check if the mouse position is vertically within this window */
    ret = (mousepos.y >= *ofs && mousepos.y < *ofs + ht);

    /* if we're in the window, determine which half we're in vertically */
    *second_half = (mousepos.y >= *ofs + ht/2);

    /* advance the offset by this height */
    *ofs += ht;

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Docking Container
 */

/*
 *   Add a subwindow
 */
void CTadsDockedCont::add_docked_win(CTadsWinDocked *win)
{
    /* add the window to our list */
    if (docked_win_last_ != 0)
        docked_win_last_->next_docked_ = win;
    else
        docked_win_first_ = win;
    docked_win_last_ = win;
    win->next_docked_ = 0;

    /* count it */
    ++docked_win_cnt_;
}

/*
 *   Add a subwindow at the given index
 */
void CTadsDockedCont::add_docked_win(CTadsWinDocked *win, int idx)
{
    int i;
    CTadsWinDocked *prv;
    CTadsWinDocked *cur;

    /* find the previous element */
    for (i = 0, prv = 0, cur = docked_win_first_ ; cur != 0 ;
         prv = cur, cur = cur->next_docked_, ++i)
    {
        /* if this is the desired index, insert it here */
        if (i == idx)
        {
            /* link it into the list */
            win->next_docked_ = cur;
            if (prv != 0)
                prv->next_docked_ = win;
            else
                docked_win_first_ = win;

            /* if it's the last element, fix up the tail pointer */
            if (win->next_docked_ == 0)
                docked_win_last_ = win;

            /* count it */
            ++docked_win_cnt_;

            /* we're done */
            return;
        }
    }

    /* we didn't find the desired index; simply add it in at the end */
    add_docked_win(win);
}

/*
 *   Remove a subwindow
 */
void CTadsDockedCont::remove_docked_win(CTadsWinDocked *win)
{
    /* unlink the element from the list */
    unlink_docked_win(win);
}

/*
 *   Unlink a window from the list, returning the window's index in the
 *   list (first element = 0).  This routine has no side effects; it
 *   merely manipulates the list.  Returns -1 if the element is not in the
 *   list.
 */
int CTadsDockedCont::unlink_docked_win(CTadsWinDocked *win)
{
    CTadsWinDocked *cur;
    CTadsWinDocked *prv;
    int idx;

    /* find it in our list */
    for (idx = 0, prv = 0, cur = docked_win_first_ ; cur != 0 ;
         prv = cur, cur = cur->next_docked_, ++idx)
    {
        /* if this is the one, remove it */
        if (cur == win)
        {
            /* unlink it */
            if (prv != 0)
                prv->next_docked_ = cur->next_docked_;
            else
                docked_win_first_ = cur->next_docked_;

            /* adjust the tail pointer if removing the last element */
            if (cur == docked_win_last_)
                docked_win_last_ = prv;

            /* count it */
            --docked_win_cnt_;

            /* return the window's index */
            return idx;
        }
    }

    /* didn't find it - return failiure */
    return -1;
}

/* ------------------------------------------------------------------------ */
/*
 *   Model docking window
 */

CTadsDockModelWin *CTadsDockModelWin::top_list_ = 0;

/*
 *   initialize
 */
void CTadsDockModelWin::init(CTadsDockModelWinSpec spec)
{
    /* add me to the top-level window list */
    next_top_ = top_list_;
    top_list_ = this;

    /* not in a container list yet */
    next_ = 0;

    /* no container yet */
    cont_ = 0;

    /* no references yet */
    refcnt_ = 0;

    /* no associated real window yet */
    win_ = 0;

    /* we don't have a valid serialization ID yet */
    serial_id_ = -1;

    /* set the GUID if we got one */
    if (spec.guid_ != 0)
        guid_.set(spec.guid_);
}

/*
 *   delete
 */
CTadsDockModelWin::~CTadsDockModelWin()
{
    /* tell my container I'm gone */
    remove_from_container();

    /* unlink me from the top-level window list */
    CTadsDockModelWin *cur, *prv;
    for (prv = 0, cur = top_list_ ; cur != 0 ; prv = cur, cur = cur->next_top_)
    {
        /* if this is me, unlink me */
        if (cur == this)
        {
            /* unlink me */
            if (prv != 0)
                prv->next_top_ = next_top_;
            else
                top_list_ = next_top_;

            /* done */
            break;
        }
    }
}

/*
 *   set my container
 */
void CTadsDockModelWin::set_container(CTadsDockModelCont *cont)
{
    /* save the old container for a moment */
    CTadsDockModelCont *oldcont = cont_;

    /* set the new container, and add a reference */
    if ((cont_ = cont) != 0)
        cont->add_ref();

    /* remove our reference on the old container */
    if (oldcont != 0)
        oldcont->release_ref();
}

/*
 *   remove me from my container
 */
void CTadsDockModelWin::remove_from_container()
{
    /* if I have a container, tell it to remove me */
    if (cont_ != 0)
    {
        /* remove myself from the container's child list */
        cont_->remove_win(this);

        /* remove our reference on the container */
        cont_->release_ref();
        cont_ = 0;
    }
}

/*
 *   forget my real window
 */
void CTadsDockModelWin::forget_window()
{
    /* clear my real window */
    win_ = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Model Docking Container
 */

/*
 *   create
 */
CTadsDockModelCont::CTadsDockModelCont(CTadsDockModelFrame *frame,
                                       tads_dock_align align)
{
    /* remember my frame and alignment */
    frame_ = frame;
    align_ = align;

    /* no windows yet */
    first_win_ = 0;

    /* not in any list yet */
    next_ = 0;

    /* add myself to the frame */
    frame->append_container(this);

    /* we don't have a valid serialization ID yet */
    serial_id_ = -1;

    /* start with one reference for the caller */
    refcnt_ = 1;
}

/*
 *   delete
 */
CTadsDockModelCont::~CTadsDockModelCont()
{
    /* remove myself from my parent list */
    if (frame_ != 0)
        frame_->remove_container(this);
}

/*
 *   Prepare for serialization
 */
void CTadsDockModelCont::prep_for_serialization(int *id)
{
    CTadsDockModelWin *win;

    /* take the next ID for ourself */
    serial_id_ = (*id)++;

    /* assign an ID to each model window */
    for (win = first_win_ ; win != 0 ; win = win->next_)
        win->serial_id_ = (*id)++;
}

/*
 *   add a model window to my list before the given model window
 */
void CTadsDockModelCont::add_win_before(CTadsDockModelWin *win_to_add,
                                        CTadsDockModelWin *before_win)
{
    /*
     *   add a reference to myself, in case we're removing the window from
     *   one place in my list and reinserting it
     */
    add_ref();

    /* remove the window from its current container */
    win_to_add->remove_from_container();

    /* find the window before which we're to add this window */
    CTadsDockModelWin *cur, *prv;
    for (prv = 0, cur = first_win_ ; cur != 0 || before_win == 0 ;
         prv = cur, cur = cur->next_)
    {
        /* if this is the one we're inserting before, add it here */
        if (cur == before_win)
        {
            /* tell the window about its new container */
            win_to_add->set_container(this);

            /* link it into the list */
            win_to_add->next_ = cur;
            if (prv != 0)
                prv->next_ = win_to_add;
            else
                first_win_ = win_to_add;

            /* no need to look any further */
            break;
        }
    }

    /* release our self-reference */
    release_ref();
}

/*
 *   find and remove a window, deleting the container if it leaves nothing
 *   in the list
 */
int CTadsDockModelCont::remove_win(class CTadsDockModelWin *win)
{

    /* find the window we're removing */
    CTadsDockModelWin *cur, *prv;
    for (prv = 0, cur = first_win_ ; cur != 0 ; prv = cur, cur = cur->next_)
    {
        /* if this is the one we're removing, remove it */
        if (cur == win)
        {
            /* unlink it from my list */
            if (prv != 0)
                prv->next_ = cur->next_;
            else
                first_win_ = cur->next_;

            /* indicate that we found the window */
            return TRUE;
        }
    }

    /* we didn't find the window */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Model container
 */

CTadsDockModelFrame::CTadsDockModelFrame()
{
    /* no docking containers yet */
    first_cont_ = 0;
}

/*
 *   delete
 */
CTadsDockModelFrame::~CTadsDockModelFrame()
{
    /* delete everything */
    clear_model();
}

/*
 *   append a container
 */
void CTadsDockModelFrame::append_container(CTadsDockModelCont *cont)
{
    CTadsDockModelCont *cur;

    /* find the last element */
    for (cur = first_cont_ ; cur != 0 && cur->next_ != 0 ; cur = cur->next_) ;

    /* add the new element after the current last element */
    if (cur != 0)
        cur->next_ = cont;
    else
        first_cont_ = cont;

    /* the new element is now the last element */
    cont->next_ = 0;
}

/*
 *   remove a container
 */
void CTadsDockModelFrame::remove_container(CTadsDockModelCont *cont)
{
    CTadsDockModelCont *prv;
    CTadsDockModelCont *cur;

    /* find the container we're removing */
    for (prv = 0, cur = first_cont_ ; cur != 0 ; prv = cur, cur = cur->next_)
    {
        /* if this is the one we're removing, remove it */
        if (cur == cont)
        {
            /* unlink it from my list */
            if (prv != 0)
                prv->next_ = cur->next_;
            else
                first_cont_ = cur->next_;
        }
    }
}

/*
 *   Prepare for serialization by assigning ID numbers to all of our model
 *   elements
 */
void CTadsDockModelFrame::prep_for_serialization()
{
    int id;
    CTadsDockModelWin *win;
    CTadsDockModelCont *cont;

    /* reset all window serial IDs */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
        win->serial_id_ = -1;

    /* start the ID numbers at zero */
    id = 0;

    /* number each container, and tell each container to number its windows */
    for (cont = first_cont_ ; cont != 0 ; cont = cont->next_)
        cont->prep_for_serialization(&id);

    /* assign serial numbers to all containerless windows */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        if (win->serial_id_ == -1)
            win->serial_id_ = id++;
    }
}

/*
 *   Clear the model in preparation for loading
 */
void CTadsDockModelFrame::clear_model()
{
    CTadsDockModelWin *win, *nxt;

    /* delete all of my containers */
    while (first_cont_ != 0)
    {
        /* remember the next one */
        CTadsDockModelCont *nxt = first_cont_->next_;

        /* tell this container to forget about me */
        first_cont_->forget_frame();

        /* release our reference on the container */
        first_cont_->release_ref();

        /* move on to the next */
        first_cont_ = nxt;
    }

    /* delete any unreferenced containerless windows */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = nxt)
    {
        /* remember the next window */
        nxt = win->next_top_;

        /* if this one is unreferenced, delete it */
        if (win->refcnt_ == 0)
            delete win;
    }
}

/*
 *   Serialize the model
 */
void CTadsDockModelFrame::
   serialize(void (*cbfunc)(void *cbctx, int id, const char *info),
             void *cbctx)
{
    CTadsDockModelCont *cont;
    CTadsDockModelWin *win;

    /* prepare for serialization - assign a unique ID number to everything */
    prep_for_serialization();

    /* write out information on the model containers */
    for (cont = first_cont_ ; cont != 0 ; cont = cont->next_)
        serialize_cont(cont, cbfunc, cbctx);

    /* write out the uncontained windows */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        /* if this window doesn't have a container, write it */
        if (win->get_container() == 0)
            serialize_win(win, 0, cbfunc, cbctx);
    }
}

/*
 *   Serialize a container
 */
void CTadsDockModelFrame::serialize_cont(
    CTadsDockModelCont *cont,
    void (*cbfunc)(void *cbctx, int id, const char *info),
    void *cbctx)
{
    char buf[128];
    CTadsDockModelWin *model_win;

    /* indicate that this is a container */
    buf[0] = 'C';

    /* store the container's alignment type */
    switch(cont->align_)
    {
    case TADS_DOCK_ALIGN_LEFT:
        buf[1] = 'L';
        break;

    case TADS_DOCK_ALIGN_RIGHT:
        buf[1] = 'R';
        break;

    case TADS_DOCK_ALIGN_TOP:
        buf[1] = 'T';
        break;

    case TADS_DOCK_ALIGN_BOTTOM:
        buf[1] = 'B';
        break;

    case TADS_DOCK_ALIGN_NONE:
        buf[1] = 'N';
        break;
    }

    /* null-terminate the string */
    buf[2] = '\0';

    /* write out the information on this item */
    (*cbfunc)(cbctx, cont->serial_id_, buf);

    /* write out this container's model windows */
    for (model_win = cont->first_win_ ; model_win != 0 ;
         model_win = model_win->next_)
        serialize_win(model_win, cont, cbfunc, cbctx);
}

/*
 *   Serialize a window
 */
void CTadsDockModelFrame::serialize_win(
    CTadsDockModelWin *model_win, CTadsDockModelCont *cont,
    void (*cbfunc)(void *cbctx, int id, const char *info),
    void *cbctx)
{
    CTadsWinDock *win;
    int docked_height, docked_width;
    tads_dock_align align;
    char align_ch;
    RECT undocked_pos;
    int docked;
    int dockable;
    char buf[128];

    /* get the actual docking window */
    win = model_win->win_;

    /*
     *   If there's no actual window, we must have loaded the model for the
     *   window but never opened it.  In this case, we want to keep the old
     *   information despite not having used it in this run, since we might
     *   want it again on a future run; simply write out the loaded model
     *   configuration.
     */
    if (win == 0)
    {
        /*
         *   if there's no GUID, there's no point in writing it, since we
         *   won't be able to track it down on the next load if the
         *   serialization ID changed
         */
        if (model_win->guid_.get() == 0)
            return;

        /* get the window's information from the model */
        docked_width = model_win->dock_width_;
        docked_height = model_win->dock_height_;
        docked = model_win->start_docked_;
        dockable = model_win->start_dockable_;
        undocked_pos = model_win->undock_pos_;
        align = model_win->dock_align_;
    }
    else
    {
        /* get the window's docked and undocked positions */
        docked_width = win->get_docked_width();
        docked_height = win->get_docked_height();
        win->get_undocked_pos(&undocked_pos);
        align = win->get_docked_align();

        /* is the window docked, or even dockable? */
        docked = win->is_win_docked();
        dockable = win->is_win_dockable();
    }

    /* get the alignment type code */
    switch (align)
    {
    case TADS_DOCK_ALIGN_LEFT:   align_ch = 'L'; break;
    case TADS_DOCK_ALIGN_RIGHT:  align_ch = 'R'; break;
    case TADS_DOCK_ALIGN_TOP:    align_ch = 'T'; break;
    case TADS_DOCK_ALIGN_BOTTOM: align_ch = 'B'; break;
    default: align_ch = 'N';
    }

    /*
     *   build the information string for the window: we need to know the
     *   container's serialization ID, so we can relate the model window back
     *   to its proper model container; the docked and undocked positions of
     *   the window; and the current docking status
     */
    sprintf(buf, "W%d,%d,%d,%d,%d,%d,%d,%c,%c",
            cont != 0 ? cont->serial_id_ : -1,
            docked_width, docked_height,
            (int)undocked_pos.left, (int)undocked_pos.top,
            (int)undocked_pos.right, (int)undocked_pos.bottom,
            docked ? 'D' : dockable ? 'U' : 'M', align_ch);

    /* add the GUID if we have one */
    if (model_win->guid_.get() != 0)
        sprintf(buf + strlen(buf), ",%s", model_win->guid_.get());

    /* write the information on this item */
    (*cbfunc)(cbctx, model_win->serial_id_, buf);
}

/*
 *   Load a docking configuration
 */
void CTadsDockModelFrame::
   load(int (*cbfunc)(void *cbctx, int id, char *buf, size_t buflen),
        void *cbctx, int string_count)
{
    int i;
    char buf[128];
    tads_dock_align align;
    CTadsDockModelCont *cont, *prvcont, *nxtcont;

    /* delete everything in the existing model */
    clear_model();

    /* run through the strings and create the containers and windows */
    for (i = 0 ; i < string_count ; ++i)
    {
        /* get the next string from the callback; on error, skip this one */
        if ((*cbfunc)(cbctx, i, buf, sizeof(buf)))
            continue;

        /* determine what type of object we have */
        switch(buf[0])
        {
        case 'C':
            /* it's a container - get the alignment type */
            switch(buf[1])
            {
            case 'L':
                align = TADS_DOCK_ALIGN_LEFT;
                break;
            case 'R':
                align = TADS_DOCK_ALIGN_RIGHT;
                break;
            case 'T':
                align = TADS_DOCK_ALIGN_TOP;
                break;
            case 'B':
                align = TADS_DOCK_ALIGN_BOTTOM;
                break;
            }

            /* create the new container and insert it into the frame */
            cont = new CTadsDockModelCont(this, align);
            append_container(cont);

            /* set its serialization ID to the loaded ID */
            cont->serial_id_ = i;
            break;

        case 'W':
            /* it's a window */
            {
                int cont_id;
                int docked_width;
                int docked_height;
                RECT undocked_pos, mdi_pos;
                int left, right, top, bottom;
                char docked_ch;
                char align_ch;
                int is_docked;
                int is_dockable;
                int fldcnt;
                char guidbuf[128];
                const char *guid;
                CTadsDockModelWin *model_win;

                /* parse the string */
                fldcnt = sscanf(buf + 1, "%d,%d,%d,%d,%d,%d,%d,%c,%c,%[^,]",
                                &cont_id, &docked_width, &docked_height,
                                &left, &top, &right, &bottom, &docked_ch,
                                &align_ch, guidbuf);
                is_docked = (docked_ch == 'D');
                is_dockable = (docked_ch != 'M');
                SetRect(&undocked_pos, left, top, right, bottom);
                mdi_pos = undocked_pos;

                /* get the container, if any */
                cont = find_cont_by_id(cont_id);

                /* note the alignment */
                align = TADS_DOCK_ALIGN_LEFT;
                if (cont != 0 && cont->align_ != TADS_DOCK_ALIGN_NONE)
                {
                    /* we have a container - use its alignment */
                    align = cont->align_;
                }
                else if (fldcnt >= 9)
                {
                    /* we have an explicit alignment - use it */
                    static const char *mapstr = "LRTBN";
                    static const tads_dock_align map[] = {
                        TADS_DOCK_ALIGN_LEFT, TADS_DOCK_ALIGN_RIGHT,
                        TADS_DOCK_ALIGN_TOP, TADS_DOCK_ALIGN_BOTTOM,
                        TADS_DOCK_ALIGN_NONE };
                    const char *p = strchr(mapstr, align_ch);
                    if (p != 0)
                        align = map[p - mapstr];
                }

                /* note whether or not we got a GUID for the window */
                guid = (fldcnt >= 10 ? guidbuf : 0);

                /*
                 *   If the window has a GUID, and there's already a model
                 *   window with the same GUID, ignore the repeat.  Some
                 *   older versions created multiple entries per GUID due to
                 *   a bug; the redundant information is mostly harmless, but
                 *   we'll cut it out anyway to remove the unneeded bloat and
                 *   to eliminate any confusion the redundancy might cause.
                 */
                if (guid != 0 && find_model_by_guid(guid) != 0)
                    break;

                /* create the model window */
                model_win = new CTadsDockModelWin(
                    docked_width, docked_height, align,
                    &undocked_pos, &mdi_pos, is_docked, is_dockable,
                    CTadsDockModelWinSpec(guid));

                /* set the serialization ID */
                model_win->serial_id_ = i;

                /* if we have a container, add the window to its list */
                if (cont != 0)
                    cont->add_win_before(model_win, 0);
            }
            break;
        }
    }

    /* remove any childless containers */
    for (prvcont = 0, cont = first_cont_ ; cont != 0 ; cont = nxtcont)
    {
        /* remember the next container */
        nxtcont = cont->next_;

        /* if this container has no children, delete it */
        if (cont->first_win_ == 0)
        {
            /* unlink it */
            if (prvcont != 0)
                prvcont->next_ = nxtcont;
            else
                first_cont_ = nxtcont;

            /* unreference it */
            cont->release_ref();
        }
        else
        {
            /*
             *   this container has children, so we're keeping it, so
             *   remember it as the previous container for the next iteration
             */
            prvcont = cont;
        }
    }
}

/*
 *   find a model container by ID
 */
CTadsDockModelCont *CTadsDockModelFrame::find_cont_by_id(int id)
{
    CTadsDockModelCont *cont;

    /* search our list */
    for (cont = first_cont_ ; cont != 0 ; cont = cont->next_)
    {
        /* if this is the one we want, return it */
        if (cont->serial_id_ == id)
            return cont;
    }

    /* failed */
    return 0;
}

/*
 *   Given a child window handle, find the serial ID of the containing
 *   docking window.
 */
int CTadsDockModelFrame::find_window_serial_id(HWND child_hwnd)
{
    CTadsDockModelWin *win;

    /* scan the master window list */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        /*
         *   if this model window has an associated real window, and the
         *   window handle matches the given window or is a parent of the
         *   given window, this is the one they're looking for
         */
        if (win->win_ != 0
            && (win->win_->get_handle() == child_hwnd
                || IsChild(win->win_->get_handle(), child_hwnd)))
        {
            /* this is the one - return this model window's serial id */
            return win->serial_id_;
        }
    }

    /* didn't find it - return failure */
    return -1;
}

/*
 *   Assign a GUID to a model window based on the serial number.  This should
 *   be used if the loaded model doesn't contain GUID's, to bring it up to
 *   date with the modern model, which stores a GUID for each window.  The
 *   only need for this routine is for bringing old configurations up to date
 *   with the new storage format.
 */
void CTadsDockModelFrame::assign_guid(int serial_id, const char *guid)
{
    CTadsDockModelWin *win;

    /* scan the window list */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        /* if this window matches the given ID, store the GUID */
        if (win->serial_id_ == serial_id)
        {
            /* store the GUID */
            win->guid_.set(guid);

            /* we're done */
            return;
        }
    }
}

/*
 *   Find the docking window that contains the focus
 */
CTadsWinDock *CTadsDockModelFrame::find_window_by_focus()
{
    HWND focus;
    CTadsDockModelWin *win;
    CTadsWin *mdi_parent = CTadsWinDock::get_default_mdi_parent();

    /* get the focus window; if there's no focus, there's no window to find */
    if ((focus = GetFocus()) == 0)
        return 0;

    /* scan our list for the model window containing the focus window */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        /* if this window hasn't been created yet, skip it */
        if (win->win_ == 0)
            continue;

        /*
         *   if this window equals the focus window, or is a parent of the
         *   focus window, it's the one we're looking for
         */
        if (win->win_->get_handle() == focus
            || IsChild(win->win_->get_handle(), focus))
            return win->win_;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Find a real window given the global identifier for the window.  This can
 *   be used after loading a configuration to identify the docking window
 *   created for a particular saved window.
 *
 *   If 'show' is true, and the system window hasn't yet been created, we'll
 *   create and show the window.  Otherwise, we'll create it but leave it
 *   hidden.  If the system window already exists, we won't show or hide it,
 *   regardless of the 'show' setting.  Similarly, we'll set the new window's
 *   title to 'title', but only if we create a window - we'll ignore 'title'
 *   if we return an existing window.
 */
CTadsWinDock *CTadsDockModelFrame::
   find_window_by_guid(CTadsWin *parent_win, const char *guid, int show,
                       const textchar_t *title)
{
    CTadsDockModelWin *win;

    /* look up the model window by guid */
    if ((win = find_model_by_guid(guid)) != 0)
    {
        /* if this window has already been created, return it */
        if (win->win_ != 0)
            return win->win_;

        /*
         *   We haven't created the system window yet, so create it now from
         *   the saved parameters.  Create it without a content window for
         *   now; the caller will have to fill this in.
         */
        win->win_ = create_sys_win(
            0, win->dock_width_, win->dock_height_, win->dock_align_,
            &win->undock_pos_, &win->mdi_pos_, title,
            CTadsDockModelWinSpec(win),
            parent_win, parent_win->get_handle(),
            win->start_docked_, !win->start_dockable_, show);

        /* return the new window */
        return win->win_;
    }

    /* didn't find it - return failure */
    return 0;
}

/*
 *   Find a model window by GUID
 */
CTadsDockModelWin *CTadsDockModelFrame::find_model_by_guid(
    const textchar_t *guid)
{
    CTadsDockModelWin *win;

    /* scan the master window list */
    for (win = CTadsDockModelWin::top_list_ ; win != 0 ; win = win->next_top_)
    {
        /* if this is the one we're looking for, return it */
        if (win->guid_.get() != 0 && strcmp(win->guid_.get(), guid) == 0)
            return win;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Create a docking window
 */
CTadsWinDock *CTadsDockModelFrame::create_sys_win(
    CTadsWin *subwin, int dock_wid, int dock_ht, tads_dock_align dock_align,
    const RECT *undock_rc, const RECT *mdi_rc, const textchar_t *title,
    CTadsDockModelWinSpec model_win, CTadsWin *parent_win, HWND parent_hdl,
    int docked, int mdi, int show)
{
    CTadsWinDock *dwin;

    /*
     *   Start as an undocked dockable window or as MDI, as requested.
     *
     *   If we're not allowed to show the window, though, start it as
     *   dockable, even if it was an MDI window previously.  This is because
     *   of a weirdness in the MDI handling in Windows - maybe a bug, but MDI
     *   is especially poorly documented, so who can tell - where Windows
     *   seems to lose track of MDI children that are created as hidden.  To
     *   work around this, we'll create it as a dockable (but undocked)
     *   window, and mark it to be converted to MDI as soon as it's made
     *   visible.
     */
    if (!mdi || !show)
    {
        /* non-MDI (at least for now) - create an undocked window */
        dwin = new CTadsWinDock(
            subwin, dock_wid, dock_ht, dock_align, model_win);
        dwin->create_system_window(
            parent_win, parent_hdl, FALSE, title, undock_rc,
            new CTadsSyswin(dwin));

        /*
         *   if it's initially docked, set the window to dock as soon as it's
         *   shown
         */
        if (docked)
            dwin->set_dock_on_unhide();

        /*
         *   if it's really an MDI window, and we're only creating it as a
         *   docking window because it's initially hidden, set it to convert
         *   to MDI as soon as it's shown
         */
        if (mdi)
            dwin->set_mdi_on_unhide();
    }
    else
    {
        /* get the MDI parent */
        CTadsWin *mdi_parent = CTadsWinDock::get_default_mdi_parent();

        /* if there's no MDI parent, we can't proceed */
        if (mdi_parent == 0)
            return 0;

        /* create it as an MDI window */
        dwin = new CTadsWinDockableMDI(
            subwin, dock_wid, dock_ht, dock_align, model_win,
            parent_win, parent_hdl);
        dwin->create_system_window(
            mdi_parent, mdi_parent->get_parent_of_children(),
            TRUE, title, mdi_rc, new CTadsSyswinMdiChild(dwin));
    }

    /* return the new window */
    return dwin;
}
