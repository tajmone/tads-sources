/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tbarmbar.h - toolbar menubar control
Function
  This implements a menubar using a toolbar.  This is a pile of fairly
  rote but somewhat nasty Windows code; we group it here for reusability.
Notes

Modified
  09/29/06 MJRoberts  - Creation
*/

#ifndef TBARMBAR_H
#define TBARMBAR_H

#include "tadsapp.h"

class ToolbarMenubar
{
public:
    /*
     *   Create the toolbar menu.  We'll create a toolbar control as a child
     *   of the given parent window, and then create a button corresponding
     *   to each menu in the given menu bar.  Each button will have a command
     *   ID equal to 'base_cmd_id' plus the button's index.  'targetwin' is
     *   the window to which we'll send menu-related notifications when the
     *   menu is activated.
     */
    ToolbarMenubar(HINSTANCE inst, HWND parent,
                   HWND targetwin, HMENU menu, int base_cmd_id);

    /* delete */
    ~ToolbarMenubar();

    /*
     *   Handle a notification from our toolbar.  Returns the WM_NOTIFY
     *   result.  The parent window's window procedure should call this in
     *   response to a WM_NOTIFY when nmhdr->hwndFrom is our handle.
     */
    int do_notify(int code, NMHDR *nmhdr);

    /* handle a WM_SYSCHAR notification */
    int do_menuchar(TCHAR ch, unsigned int menu_type, HMENU menu,
                    LRESULT *ret);

    /* handle a WM_SYSCOMMAND notification of type SC_KEYMENU */
    int do_syskeymenu(TCHAR ch);

    /* get the toolbar window handle */
    HWND get_handle() const { return handle_; }

private:
    /* enter tracking mode */
    void begin_tracking(int idx);

    /* exit tracking mode */
    void end_tracking(int restore_focus);

    /* show the menu for the current "hot" item in the toolbar */
    void show_menu();

    /* subclassed window procedure for the toolbar */
    LRESULT subclass_proc(
        HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);
    static LRESULT CALLBACK S_subclass_proc(
        HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);

    /* original window procedure for the toolbar */
    LONG defproc_;

    /* message hook procedure for menu message filtering */
    void menu_hook(int code, WPARAM wpar, LPARAM lpar);
    static LRESULT CALLBACK S_menu_hook(int code, WPARAM wpar, LPARAM lpar);

    /* our toolbar handle */
    HWND handle_;

    /* target window for menu notifications */
    HWND targetwin_;

    /* our menu handle */
    HMENU menu_;

    /* current hot item in toolbar */
    int hot_idx_;

    /* our base command ID */
    int base_cmd_id_;

    /* are we in tracking mode? */
    int tracking_mode_;
    int tracking_old_accel_enabled_;

    /* flag: open a new menu after dismissing the current one */
    int drop_again_;

    /* flag: on the next menu open, select the first item */
    int select_on_open_;

    /* flag: we closed the current menu via the escape key */
    int escape_;

    /* old focus window in tracking mode */
    HWND old_focus_;

    /* hook handle, and the instance that set the hook */
    static HHOOK hook_;
    static ToolbarMenubar *hook_owner_;

    /* last mouse position in hook routine */
    POINT last_mouse_pos_;

    /* menu ID of open menu */
    int open_menu_id_;

    /* is the current active menu the primary menu (not a submenu)? */
    int active_menu_is_primary_;

    /* does the current active menu item have a submenu? */
    int active_item_has_submenu_;
};

#endif /* TBARMBAR_H */
