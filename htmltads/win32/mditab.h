/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  mditab.h - custom tab control for maximized MDI windows
Function

Notes

Modified
  10/02/06 MJRoberts  - Creation
*/

#ifndef MDITAB_H
#define MDITAB_H

#include <Windows.h>
#include <commctrl.h>

#include "tadshtml.h"
#include "tadswin.h"

class MdiTabCtrl: public CTadsWin
{
public:
    MdiTabCtrl(HWND mdiclient);
    ~MdiTabCtrl();

    /* get the number of tabs */
    int get_count();

    /*
     *   add an item before the given index, or at the end of the list if idx
     *   is -1; returns the new item's actual index
     */
    int insert_item(const textchar_t *title, size_t len, void *param, int idx);
    int insert_item(const textchar_t *title, void *param, int idx)
        { return insert_item(title, get_strlen(title), param, idx); }

    /* append an item; returns the new item's index */
    int append_item(const textchar_t *title, size_t len, void *param)
        { return insert_item(title, len, param, -1); }
    int append_item(const textchar_t *title, void *param)
        { return insert_item(title, get_strlen(title), param, -1); }

    /* move an item to a new position */
    void move_item(int old_idx, int new_idx);

    /* get or set the selection */
    int get_selection() const;
    void select_item(int idx);

    /* delete an item */
    void delete_item(int idx);

    /* find an item by name; returns the index, or -1 if not found */
    int find_item(const textchar_t *title, size_t len) const;
    int find_item(const textchar_t *title) const
        { return find_item(title, get_strlen(title)); }

    /* find an item by the context parameter associated at creation */
    int find_item_by_param(void *param) const;

    /* get/set the title of the item at the given index */
    const textchar_t *get_item_title(int idx) const;
    void set_item_title(int idx, const textchar_t *title, size_t len);
    void set_item_title(int idx, const textchar_t *title)
        { set_item_title(idx, title, get_strlen(title)); }

    /* get the parameter associated with an item at creation */
    void *get_item_param(int idx) const;

    /* get our natural height */
    int get_natural_height() const { return height_; }

private:
    /* static initialization */
    void static_init();

    /* link/unlink an item in the list */
    void link_before(class MdiTabItem *item, class MdiTabItem *other);
    class MdiTabItem *unlink(class MdiTabItem *item);

    /* move focus to the MDI client window, if it's not within it already */
    void focus_to_mdiclient();

    /* find an item by a window x position */
    class MdiTabItem *find_item_by_x(int x) const;

    /* find an item by index */
    class MdiTabItem *find_item_by_index(int idx) const;

    /* get an item's index */
    int get_item_index(class MdiTabItem *item) const;

    /* select an item */
    void select_item(class MdiTabItem *item);

    /* update the document width */
    void update_doc_width();

    /*
     *   get the display width - this is the window width minus the toolbar
     *   width, since the toolbar covers part of the tab area
     */
    int get_disp_width() const
    {
        RECT crc, trc;

        /* get our window and toolbar areas */
        GetClientRect(handle_, &crc);
        GetWindowRect(toolbar_, &trc);

        /* return the window width minus the toolbar width */
        return crc.right - (trc.right - trc.left);
    }

    /* scroll left (negative dx) or right (positive dx) */
    void scroll_by(int dx, int animate);

    /* scroll to a given position */
    void scroll_to(int x, int animate)
        { scroll_by(x - scroll_pos_, animate); }

    /* scroll the selection into view */
    void scroll_sel_into_view();

    /* window creation */
    virtual void do_create();

    /* resize the window */
    virtual void do_resize(int mode, int x, int y);

    /* paint */
    virtual void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* handle mouse clicks and movement */
    virtual int do_leftbtn_down(int keys, int x, int y, int clicks);
    virtual int do_leftbtn_up(int keys, int x, int y);
    virtual int do_mousemove(int keys, int x, int y);
    virtual int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* handle a capture change */
    virtual int do_capture_changed(HWND hwnd);

    /* handle a timer event */
    int do_timer(int timer_id);

    /* process commands */
    virtual TadsCmdStat_t check_command(const check_cmd_info *);
    virtual int do_command(int notify_code, int cmd, HWND ctl);

    /* we're a child window */
    DWORD get_winstyle()
    {
        return WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE;
    }

    /* MDI client handle */
    HWND mdiclient_;

    /* linked list of tab items */
    class MdiTabItem *items_;

    /* the item we're dragging, if in drag-tracking */
    class MdiTabItem *dragging_tab_;

    /* number of time events since dragging started */
    int dragging_ticks_;

    /* drag timer */
    int drag_timer_;

    /* scroll position - pixel offset of left edge of window */
    int scroll_pos_;

    /* total "document" width (for scrolling) */
    int doc_width_;

    /* the selected item */
    class MdiTabItem *sel_;

    /* our natural height */
    int height_;

    /* the scroller/MDI-control toolbar */
    HWND toolbar_;

    /* drag cursor */
    static HCURSOR drag_csr_;
};

#endif /* MDITAB_H */
