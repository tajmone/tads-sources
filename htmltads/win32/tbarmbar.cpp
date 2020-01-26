/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tbarmbar.cpp - toolbar menubar implementation
Function

Notes

Modified
  09/29/06 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <memory.h>
#include <Windows.h>
#include <commctrl.h>

#include "tadsapp.h"
#include "tadswin.h"
#include "tbarmbar.h"
#include "hos_w32.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics and constants
 */
HHOOK ToolbarMenubar::hook_ = 0;
ToolbarMenubar *ToolbarMenubar::hook_owner_ = 0;

/* private message: show the menu for the current hot item in the toolbar */
#define MSG_SHOW_MENU   (WM_USER + 1)

/* window property for our "self" pointer */
#define WINPROP_SELF    "ToolbarMenubar.this"


/* ------------------------------------------------------------------------ */
/*
 *   Construction
 */
ToolbarMenubar::ToolbarMenubar(HINSTANCE inst, HWND parent,
                               HWND targetwin, HMENU menu, int base_cmd_id)
{
    HDC dc;
    HFONT oldfont;

    /* get the desktop DC */
    dc = GetDC(GetDesktopWindow());

    /* set up with the menu font */
    oldfont = (HFONT)SelectObject(dc, CTadsApp::menu_font);

    /* remember our base command ID */
    base_cmd_id_ = base_cmd_id;

    /* remember our menu */
    menu_ = menu;

    /* remember the target window for menu commands */
    targetwin_ = targetwin;

    /* not in tracking mode yet */
    tracking_mode_ = FALSE;
    hot_idx_ = -1;
    old_focus_ = 0;
    last_mouse_pos_.x = last_mouse_pos_.y = -1;

    /* create the toolbar control */
    handle_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NOPARENTALIGN
        | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, parent, 0, inst, 0);

    /* do the required structure/DLL initialization */
    SendMessage(handle_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    /*
     *   set the bitmap to zero width, for older platforms that don't support
     *   I_IMAGENONE in the bitmap specification
     */
    SendMessage(handle_, TB_SETBITMAPSIZE, 0, MAKELONG(0, 15));

    /* translate the menu list into a button list */
    for (int i = 0 ; i < GetMenuItemCount(menu) ; ++i)
    {
        TBBUTTON button;
        MENUITEMINFO mi;
        char title[256];
        SIZE sz;

        /* get the menu's title */
        mi.cbSize = CTadsWin::menuiteminfo_size_;
        mi.fMask = MIIM_STRING;
        mi.dwTypeData = title;
        mi.cch = sizeof(title) - 1;
        GetMenuItemInfo(menu, i, TRUE, &mi);

        /* set up this button */
        button.iBitmap = I_IMAGENONE;
        button.idCommand = base_cmd_id_ + i;
        button.fsState = TBSTATE_ENABLED;
        button.fsStyle = TBSTYLE_DROPDOWN;
        button.dwData = 0;
        button.iString = (INT_PTR)title;

        /* measure the string width */
        GetTextExtentPoint32(dc, title, mi.cch, &sz);

        /* add the button */
        SendMessage(handle_, TB_ADDBUTTONS, (WPARAM)1, (LPARAM)&button);

        /*
         *   set the width explicitly - use the native width of the text,
         *   plus a few extra pixels, to match the standard Windows menubar
         *   styling
         */
        TBBUTTONINFO tbi;
        tbi.cbSize = sizeof(tbi);
        tbi.dwMask = TBIF_SIZE;
        tbi.cx = (WORD)sz.cx + 5;
        SendMessage(handle_, TB_SETBUTTONINFO,
                    base_cmd_id_ + i, (LPARAM)&tbi);
    }

    /* subclass the toolbar, so that we can handle private messages */
    SetProp(handle_, WINPROP_SELF, (HANDLE)this);
    defproc_ = SetWindowLong(handle_, GWL_WNDPROC, (LONG)&S_subclass_proc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Deletion
 */
ToolbarMenubar::~ToolbarMenubar()
{
    /* remove our subclassing of the toolbar window */
    SetWindowLong(handle_, GWL_WNDPROC, (LONG)defproc_);
    RemoveProp(handle_, WINPROP_SELF);
}

/* ------------------------------------------------------------------------ */
/*
 *   Handle a notification
 */
int ToolbarMenubar::do_notify(int code, NMHDR *nmhdr)
{
    /* make sure it's really directed to us */
    if (nmhdr->hwndFrom != handle_)
        return 0;

    /* check which notification this is */
    switch (code)
    {
    case TBN_HOTITEMCHANGE:
        {
            NMTBHOTITEM *hi = (NMTBHOTITEM *)nmhdr;
            int idx;

            /* get the new index */
            idx = ((hi->dwFlags & HICF_LEAVING) != 0)
                  ? -1 : hi->idNew - base_cmd_id_;

            /*
             *   if we're in tracking mode, and the index is changing, we
             *   might need to close a menu
             */
            if (tracking_mode_ && idx != hot_idx_)
            {
                /* track it only if it wasn't our own TB_SETHOTITEM call */
                if ((hi->dwFlags
                     & (HICF_ACCELERATOR | HICF_ARROWKEYS
                        | HICF_DUPACCEL | HICF_MOUSE)) != 0)
                {
                    /*
                     *   make sure there's a valid new item - if there isn't,
                     *   don't allow the change: return a non-zero result to
                     *   tell the toolbar to ignore the change
                     */
                    if (idx == -1)
                        return 1;

                    /*
                     *   If there's a menu currently open in this toolbar,
                     *   close it and open the new item's menu.  Otherwise,
                     *   if we're moving via an arrow key, open the new menu.
                     */
                    if (hook_owner_ == this && hi->idNew != open_menu_id_)
                    {
                        /* remove the current menu */
                        SendMessage(targetwin_, WM_CANCELMODE, 0, 0);

                        /* flag that we want to open the next menu */
                        drop_again_ = TRUE;

                        /*
                         *   If we changed the selected button via the
                         *   keyboard interface rather than the mouse, select
                         *   the first item on the new menu as soon as it
                         *   opens, for consistency with the standard Windows
                         *   menu behavior in the keyboard interface.
                         */
                        select_on_open_ = !(hi->dwFlags & HICF_MOUSE);
                    }
                }
            }

            /* note the new item */
            hot_idx_ = idx;
        }

        /* proceed with normal highlighting */
        return 0;

    case TBN_DROPDOWN:
        /*
         *   Drop-down button click - show our menu.  Show it via a posted
         *   message, to ensure that the toolbar has a chance to finish its
         *   processing for the drop-down.
         */
        {
            /* get the item being dropped */
            NMTOOLBAR *nmtb = (NMTOOLBAR *)nmhdr;

            /*
             *   note whether the mouse button is down - if not, the menu is
             *   being opened via the keyboard interface, which means that we
             *   should select the menu's first item immediately after
             *   opening it, for consistency with the standard Windows
             *   keyboard interface for menus
             */
            int kb = (GetKeyState(VK_LBUTTON) >= 0);

            /* post our private show-menu message */
            PostMessage(handle_, MSG_SHOW_MENU, nmtb->iItem, kb);
        }

        /* handled - use the default final processing in the toolbar */
        return TBDDRET_DEFAULT;

    case NM_CUSTOMDRAW:
        /* custom draw */
        {
            NMCUSTOMDRAW *cd = (NMCUSTOMDRAW *)nmhdr;
            char *txt;
            size_t len;

            /* check the drawing stage */
            switch (cd->dwDrawStage)
            {
            case CDDS_PREPAINT:
                /* ask for per-item notifications */
                return CDRF_NOTIFYITEMDRAW;

            case CDDS_ITEMPREPAINT:
                /* use the menu hilite color for the "hot" item */
                if ((cd->uItemState & CDIS_HOT) != 0
                    || (hook_owner_ == this
                        && cd->dwItemSpec == (DWORD)open_menu_id_))
                {
                    /* draw the label with menu highlighting */
                    SetTextColor(cd->hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
                    FillRect(cd->hdc, &cd->rc,
                             GetSysColorBrush(COLOR_HIGHLIGHT));
                }
                else
                {
                    /* draw with normal menu colors */
                    SetTextColor(cd->hdc, GetSysColor(COLOR_MENUTEXT));
                }

                /* get the item's label from the menu */
                len = SendMessage(handle_, TB_GETBUTTONTEXT,
                                  (WPARAM)cd->dwItemSpec, 0);
                txt = new char[len + 1];
                SendMessage(handle_, TB_GETBUTTONTEXT,
                            (WPARAM)cd->dwItemSpec, (LPARAM)txt);

                /* draw the label */
                SetBkMode(cd->hdc, TRANSPARENT);
                SelectObject(cd->hdc, CTadsApp::menu_font);
                DrawTextEx(cd->hdc, txt, len, &cd->rc,
                           DT_VCENTER | DT_CENTER | DT_SINGLELINE, 0);

                /* dispose of the label buffer */
                delete [] txt;

                /* no more drawing is needed on this pass */
                return CDRF_SKIPDEFAULT;
            }
        }

        /* we didn't handle it, so use the default handling */
        return CDRF_DODEFAULT;
    }

    /* anything else is unhandled */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Handle a WM_MENUCHAR notification from the parent window
 */
int ToolbarMenubar::do_menuchar(TCHAR ch, unsigned int menu_type,
                                HMENU menu, LRESULT *ret)
{
    UINT cmd;

    /*
     *   if it's going to the system menu by default, check to see if it's
     *   really a shortcut for our menubar instead; if we can find a shortcut
     *   among the toolbar buttons, activate the associated menu
     */
    if ((menu_type & MF_SYSMENU) != 0
        && SendMessage(handle_, TB_MAPACCELERATOR, ch, (LPARAM)&cmd))
    {
        /*
         *   we found a translation - post a message to ourselves to open
         *   this menu (use a Post rather than a Send, since this will let
         *   the caller finish processing the key event before we recurse
         *   into the menu loop)
         */
        PostMessage(handle_, MSG_SHOW_MENU, cmd, TRUE);

        /* we handled it */
        *ret = MAKELPARAM(0, MNC_CLOSE);
        return TRUE;
    }

    /* we didn't handle it */
    return FALSE;
}

/*
 *   Handle a WM_SYSCOMMAND notification of type SC_MENUKEY
 */
int ToolbarMenubar::do_syskeymenu(TCHAR ch)
{
    /*
     *   If the character is zero, it means that we just pressed the ALT key
     *   by itself.  In this case, just enter tracking mode with the first
     *   button highlighted, but don't actually drop down the menu; this is
     *   consistent with the standard Windows menu keyboard interface.
     */
    if (ch == 0)
    {
        begin_tracking(base_cmd_id_);
        return TRUE;
    }

    /* not handled */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Subclassed message handler, static version: call our member function
 */
LRESULT CALLBACK ToolbarMenubar::S_subclass_proc(
    HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    /* get our "self" pointer */
    ToolbarMenubar *self = (ToolbarMenubar *)GetProp(hwnd, WINPROP_SELF);

    /* call the member routine, if available */
    return (self != 0 ? self->subclass_proc(hwnd, msg, wpar, lpar) : 0);
}

/*
 *   Subclassed message handler for our toolbar window
 */
LRESULT ToolbarMenubar::subclass_proc(
    HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    /* check the message */
    switch (msg)
    {
    case WM_KEYDOWN:
        /*
         *   if they press Escape while we're in tracking mode with no menu
         *   open, cancel tracking
         */
        if (wpar == VK_ESCAPE)
        {
            /* cancel tracking */
            end_tracking(TRUE);

            /* handled */
            return 0;
        }

        /* not handled */
        break;

    case MSG_SHOW_MENU:
        /* enter tracking mode */
        begin_tracking((int)wpar);

        /* set the select-on-open flag according to the lparam */
        select_on_open_ = (lpar != 0);

        /* show the menu */
        show_menu();

        /* handled */
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        /* clicking outside the menu bar ends tracking mode */
        if (tracking_mode_)
        {
            POINT pt;
            HWND hwnd;

            /* check to see if we're outside the menu window */
            pt.x = LOWORD(lpar);
            pt.y = HIWORD(lpar);
            ClientToScreen(handle_, &pt);
            if ((hwnd = WindowFromPoint(pt)) != handle_)
            {
                /* terminate tracking */
                end_tracking(FALSE);

                /* forward the click to the window we clicked in */
                if (hwnd != 0)
                    mouse_event(MOUSEEVENTF_ABSOLUTE
                                | (msg == WM_LBUTTONDOWN
                                   ? MOUSEEVENTF_LEFTDOWN
                                   : MOUSEEVENTF_RIGHTDOWN),
                                pt.x, pt.y, 0, 0);
            }

            /* we've handled it */
            return 0;

        case WM_KILLFOCUS:
            /* on losing focus, end tracking mode */
            end_tracking(TRUE);
            break;
        }
        break;
    }

    /* inherit the default handling */
    return CallWindowProc((WNDPROC)defproc_, hwnd, msg, wpar, lpar);
}


/* ------------------------------------------------------------------------ */
/*
 *   Enter tracking mode
 */
void ToolbarMenubar::begin_tracking(int cmd)
{
    /* if we're already in tracking mode, do nothing */
    if (tracking_mode_)
        return;

    /* set tracking mode */
    tracking_mode_ = TRUE;

    /*
     *   temporarily disable accelerators while we're tracking - we want the
     *   standard menu keyboard commands to apply
     */
    tracking_old_accel_enabled_ = CTadsApp::get_app()->enable_accel(FALSE);

    /* grab focus */
    old_focus_ = 0;
    if (GetFocus() != handle_)
        old_focus_ = SetFocus(handle_);

    /* capture the mouse */
    if (GetCapture() != handle_)
    {
        SetCapture(handle_);
        SendMessage(handle_, WM_SETCURSOR, (WPARAM)handle_,
                    MAKELPARAM(HTCLIENT, 0));
    }

    /* set the hot item in the toolbar */
    SendMessage(handle_, TB_SETHOTITEM, cmd - base_cmd_id_, 0);
}

/*
 *   Exit tracking mode
 */
void ToolbarMenubar::end_tracking(int restore_focus)
{
    /* if we're not in tracking mode, do nothing */
    if (!tracking_mode_)
        return;

    /* we're no longer in tracking mode */
    tracking_mode_ = FALSE;

    /* re-enable the accelerators */
    CTadsApp::get_app()->enable_accel(tracking_old_accel_enabled_);

    /* clear the hot item */
    SendMessage(handle_, TB_SETHOTITEM, (WPARAM)-1, 0);

    /* release capture */
    if (GetCapture() == handle_)
        ReleaseCapture();

    /* restore focus */
    if (restore_focus && GetFocus() == handle_ && IsWindow(old_focus_))
    {
        SetFocus(old_focus_);
        old_focus_ = 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Show a menu
 */
void ToolbarMenubar::show_menu()
{
    HMENU m;
    RECT rc;
    RECT rcbar;
    POINT pt;

    /* keep going until we dismiss the menu */
    for (drop_again_ = TRUE ; drop_again_ ; )
    {
        /* if there isn't one, there's no menu to open */
        if (hot_idx_ == -1)
            return;

        /* if the menu is already open, ignore it */
        if (hook_owner_ != 0)
            return;

        /* get the corresponding menu from our menu bar */
        m = GetSubMenu(menu_, hot_idx_);

        /* make the button pressed */
        SendMessage(handle_, TB_PRESSBUTTON, hot_idx_ + base_cmd_id_,
                    MAKELONG(TRUE, 0));

        /* get the position of the toolbar, and of the button within it */
        GetWindowRect(handle_, &rcbar);
        SendMessage(handle_, TB_GETITEMRECT, hot_idx_, (LPARAM)&rc);

        /* calculate the bottom left coordinate of the button */
        pt.x = rc.left + rcbar.left;
        pt.y = rcbar.bottom;

        /* remember the open menu */
        open_menu_id_ = hot_idx_ + base_cmd_id_;

        /* we need to hook messages while the menu is showing */
        hook_owner_ = this;
        hook_ = SetWindowsHookEx(WH_MSGFILTER, (HOOKPROC)&S_menu_hook,
                                 0, GetCurrentThreadId());

        /* initially, the primary menu is open with no item selected */
        active_menu_is_primary_ = TRUE;
        active_item_has_submenu_ = FALSE;

        /* clear the keyboard flags */
        escape_ = FALSE;

        /* assume we won't want to open another menu after closing this one */
        drop_again_ = FALSE;

        /*
         *   if we're supposed to select the first item on open, synthesize a
         *   down-arrow keystroke - this will use the menu's keyboard
         *   interface to select the menu's first item as soon as it opens
         */
        if (select_on_open_)
        {
            keybd_event(VK_DOWN, 0, 0, 0);
            keybd_event(VK_DOWN, 0, KEYEVENTF_KEYUP, 0);
        }

        /* track the menu */
        TrackPopupMenu(m, TPM_TOPALIGN | TPM_LEFTALIGN,
                       pt.x, pt.y, 0, targetwin_, 0);

        /* delete our hook */
        UnhookWindowsHookEx(hook_);
        hook_ = 0;
        hook_owner_ = 0;
        open_menu_id_ = 0;

        /*
         *   Invalidate the screen area of the outgoing toolbar item.  We
         *   artifically draw the toolbar button associated with the open
         *   menu as "hot," even though the toolbar might not think it is.
         *   This means that the toolbar won't think the item is being drawn
         *   as hot, so it won't think it needs to redraw it as non-hot now.
         *   We thus need to do this redrawing manually.
         */
        InvalidateRect(handle_, &rc, TRUE);
    }

    /* if they didn't exit with the Escape key, we're done tracking */
    if (!escape_)
        end_tracking(TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Menu message hook, static version: call our member function.
 */
LRESULT CALLBACK ToolbarMenubar::S_menu_hook(
    int code, WPARAM wpar, LPARAM lpar)
{
    /* call our member routine, if available */
    if (hook_owner_ != 0)
        hook_owner_->menu_hook(code, wpar, lpar);

    /* pass along to the next hook */
    return CallNextHookEx(hook_, code, wpar, lpar);
}

/*
 *   Menu message hook - we use this to filter messages while we have a menu
 *   popped up, to simulate normal Windows menu behavior.
 */
void ToolbarMenubar::menu_hook(int code, WPARAM wpar, LPARAM lpar)
{
    /* check to see that it's within our menu */
    if (code == MSGF_MENU)
    {
        MSG *msg;
        POINT pt;

        /* get the message */
        msg = (MSG *)lpar;

        /* check the message */
        switch (msg->message)
        {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDBLCLK:
            /* get the mouse position in screen coordinates */
            pt.x = LOWORD(msg->lParam);
            pt.y = HIWORD(msg->lParam);

            /*
             *   ignore mouse-move messages that don't represent actual
             *   movement - Windows seems to generate a bunch of these around
             *   focus and capture changes, and the extra messages confuse
             *   things for keyboard navigation
             */
            if (msg->message == WM_MOUSEMOVE
                && pt.x == last_mouse_pos_.x
                && pt.y == last_mouse_pos_.y)
                break;

            /* remember this as the last mouse position we relayed */
            last_mouse_pos_ = pt;

            /* convert it to toolbar local coordinates */
            ScreenToClient(handle_, &pt);

            /* send it to the toolbar */
            SendMessage(handle_, WM_MOUSEMOVE,
                        msg->wParam, MAKELPARAM(pt.x, pt.y));
            break;

        case WM_KEYDOWN:
            /* check which key we have */
            switch (msg->wParam)
            {
            case VK_LEFT:
                /* send it to the toolbar if the primary menu is selected */
                if (active_menu_is_primary_)
                    SendMessage(handle_, WM_KEYDOWN, msg->wParam, msg->lParam);
                break;

            case VK_RIGHT:
                /*
                 *   send it to the toolbar if the current menu item doesn't
                 *   open into a submenu
                 */
                if (!active_item_has_submenu_)
                    SendMessage(handle_, WM_KEYDOWN, msg->wParam, msg->lParam);
                break;

            case VK_ESCAPE:
                /* stay in tracking mode even after dismissing the menu */
                if (active_menu_is_primary_)
                    escape_ = TRUE;
                break;
            }
            break;

        case WM_MENUSELECT:
            /* note if we're selecting the primary menu */
            active_menu_is_primary_ =
                ((HMENU)msg->lParam ==
                 GetSubMenu(menu_, open_menu_id_ - base_cmd_id_));

            /* note if this item has a submenu */
            active_item_has_submenu_ = ((HIWORD(msg->wParam) & MF_POPUP) != 0);
            break;
        }
    }
}
