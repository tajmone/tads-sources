/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  mditab.cpp - custom "flat" tab control
Function

Notes

Modified
  10/02/06 MJRoberts  - Creation
*/

#include <windows.h>
#include <commctrl.h>

#include <stddef.h>
#include <stdlib.h>

#include "tadsapp.h"
#include "htmlres.h"
#include "mditab.h"

/* ------------------------------------------------------------------------ */
/*
 *   Constants
 */

/* toolbar button and icon size */
#define TB_ICON_CX  10
#define TB_ICON_CY  10

/* ------------------------------------------------------------------------ */
/*
 *   Tab item
 */
class MdiTabItem
{
public:
    MdiTabItem(const textchar_t *title, size_t len, void *param)
    {
        /* remember the caller's parameter value */
        this->param = param;

        /* set our title */
        set_title(title, len);

        /* we're not in a list yet */
        nxt = 0;
    }

    /* next in line */
    MdiTabItem *nxt;

    /* parameter - this is arbitrary context data for the owner's use */
    void *param;

    /* get/set the title */
    const textchar_t *get_title() const { return title.get(); }
    void set_title(const textchar_t *t) { set_title(t, get_strlen(t)); }
    void set_title(const textchar_t *t, size_t len)
    {
        /* remember the title */
        title.set(t, len);

        /* re-measure our width */
        width = measure_width(FALSE);
        width_bold = measure_width(TRUE);
    }

    /* get my width in the given selection mode */
    int get_width(int is_selected) const
        { return is_selected ? width_bold : width; }

    /* get my natural height */
    static int get_height() { return height_; }

    /* draw this item; returns the width of the item as drawn */
    void draw(HDC hdc, int x, int is_selected)
    {
        int width;
        RECT rc;

        /* get my width in the current mode */
        width = get_width(is_selected);

        /* if this is the selected item, highlight it */
        if (is_selected)
        {
            /* switch to bold font and separator line */
            SelectObject(hdc, CTadsApp::bold_menu_font);
            SelectObject(hdc, bold_sep_pen_);

            /* highlight the background */
            SetRect(&rc, x, 3, x + width, height_);
            FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNHIGHLIGHT));
        }
        else
        {
            /* not selected - use the base font and pen */
            SelectObject(hdc, CTadsApp::menu_font);
            SelectObject(hdc, sep_pen_);
        }

        /* set up our title rectangle */
        SetRect(&rc, x, 3, x + width, height_);

        /* draw the title */
        DrawTextEx(hdc, title.get(), get_strlen(title.get()),
                   &rc, DT_VCENTER | DT_CENTER | DT_SINGLELINE, 0);

        /* draw the separator */
        MoveToEx(hdc, x + width - 1, 3, 0);
        LineTo(hdc, x + width - 1, height_);
    }

    /* initialize statics */
    static void static_init()
    {
        /* create our separator-line pens */
        if (sep_pen_ == 0)
            sep_pen_ = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
        if (bold_sep_pen_ == 0)
            bold_sep_pen_ = CreatePen(PS_SOLID, 1,
                                      GetSysColor(COLOR_3DDKSHADOW));

        /* initialize the fonts, if we haven't already */
        if (height_ == 0)
        {
            /* figure the height we need for our window - use the menu font */
            HDC dc = GetWindowDC(0);
            HFONT old_font = (HFONT)SelectObject(dc, CTadsApp::menu_font);
            SIZE sz;
            GetTextExtentPoint32(dc, "X", 1, &sz);

            /*
             *   use the font height plus a 3 pixel top border, plus a pixel
             *   at the top and bottom of the text area
             */
            height_ = sz.cy + 5;

            /* clean up */
            ReleaseDC(0, dc);
        }
    }

protected:
    /* measure my width, in normal or selected mode */
    int measure_width(int is_selected)
    {
        HDC dc;
        HFONT old_font;
        SIZE sz;

        /* get a screen DC for measuring */
        dc = GetWindowDC(0);

        /* use the appropriate font */
        old_font = (HFONT)SelectObject(
            dc, is_selected ? CTadsApp::bold_menu_font : CTadsApp::menu_font);

        /* measure it */
        GetTextExtentPoint32(dc, title.get(), get_strlen(title.get()), &sz);

        /* clean up */
        SelectObject(dc, old_font);
        ReleaseDC(0, dc);

        /*
         *   return the width of the text, plus padding, plus space for the
         *   separator line
         */
        return sz.cx + 5 + 5 + 1;
    }

    /* the title string */
    CStringBuf title;

    /* width in pixels, in normal and selected modes */
    int width;
    int width_bold;

    /* statics */
    static HPEN sep_pen_;
    static HPEN bold_sep_pen_;
    static int height_;
};

/* statics */
HPEN MdiTabItem::sep_pen_ = 0;
HPEN MdiTabItem::bold_sep_pen_ = 0;
int MdiTabItem::height_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Statics
 */
HCURSOR MdiTabCtrl::drag_csr_ = 0;

/*
 *   Static initialization
 */
void MdiTabCtrl::static_init()
{
    /* load our drag-cursor */
    drag_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                           MAKEINTRESOURCE(IDC_DRAG_TAB_CSR));

    /* static-initialize the item object as well */
    MdiTabItem::static_init();
}

/* ------------------------------------------------------------------------ */
/*
 *   Create
 */
MdiTabCtrl::MdiTabCtrl(HWND mdiclient)
{
    /* do static initialization */
    static_init();

    /* remember the MDI client window */
    mdiclient_ = mdiclient;

    /* get our natural height from the tab items */
    height_ = MdiTabItem::get_height();

    /* render off-screen for flicker-free drawing */
    off_screen_render_ = TRUE;

    /* start scrolled all the way to the left */
    scroll_pos_ = 0;
    doc_width_ = 0;

    /* no items yet */
    items_ = 0;
    sel_ = 0;
    dragging_tab_ = 0;
    drag_timer_ = 0;

    /* we haven't set up our MDI toolbar yet */
    toolbar_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete
 */
MdiTabCtrl::~MdiTabCtrl()
{
    /* delete our item list */
    sel_ = 0;
    while (items_ != 0)
    {
        /* remember the next item */
        MdiTabItem *nxt = items_->nxt;

        /* delete this one */
        delete items_;

        /* unlink it */
        items_ = nxt;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Window creation
 */
void MdiTabCtrl::do_create()
{
    TBBUTTON buttons[10];
    int i;

    /* do the default work first */
    CTadsWin::do_create();

    /* create our toolbar */
    toolbar_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_TOOLTIPS | TBSTYLE_LIST
        | CCS_NOPARENTALIGN | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, handle_, 0, CTadsApp::get_app()->get_instance(), 0);

    /* do the required structure/DLL initialization */
    SendMessage(toolbar_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    /* set the icon and button sizes */
    SendMessage(toolbar_, TB_SETBITMAPSIZE, 0,
                MAKELONG(TB_ICON_CX, TB_ICON_CY));
    SendMessage(toolbar_, TB_SETBUTTONSIZE, 0,
                MAKELONG(TB_ICON_CX + 4, TB_ICON_CY + 4));

    /* load the button images */
    TBADDBITMAP tba;
    tba.hInst = CTadsApp::get_app()->get_instance();
    tba.nID = IDB_MDI_TAB_TOOLBAR;
    SendMessage(toolbar_, TB_ADDBITMAP, 4, (LPARAM)&tba);

    /* set up the toolbar buttons */
    i = 0;

    buttons[i].iBitmap = 0;
    buttons[i].idCommand = ID_MDITB_LEFT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    buttons[i].iBitmap = 1;
    buttons[i].idCommand = ID_MDITB_RIGHT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    buttons[i].iBitmap = 3;
    buttons[i].idCommand = ID_WINDOW_RESTORE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    buttons[i].iBitmap = 2;
    buttons[i].idCommand = ID_FILE_CLOSE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* add the buttons */
    SendMessage(toolbar_, TB_ADDBUTTONS, (WPARAM)i, (LPARAM)buttons);

    /* set up the toolbar to do idle status updating */
    add_toolbar_proc(toolbar_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Resize
 */
void MdiTabCtrl::do_resize(int mode, int x, int y)
{
    /* do any inherited work */
    CTadsWin::do_resize(mode, x, y);

    /* move our toolbar if we're doing a normal resize */
    if (mode == SIZE_MAXIMIZED || mode == SIZE_RESTORED)
    {
        SIZE sz;

        /* get the toolbar size */
        SendMessage(toolbar_, TB_GETMAXSIZE, 0, (LPARAM)&sz);

        /* keep the toolbar at the right edge, and use all vertical space */
        MoveWindow(toolbar_, x - sz.cx, 0, sz.cx, y, TRUE);

        /* keep the scrolling position within bounds */
        scroll_by(0, FALSE);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Scroll by the given amount left (negative) or right (positive)
 */
void MdiTabCtrl::scroll_by(int dx, int animate)
{
    int new_pos;
    int dispwid = get_disp_width();
    int inc;

    /* figure the raw new scrolling position */
    new_pos = scroll_pos_ + dx;

    /*
     *   limit the rightward scrolling to the document width (plus a bit of
     *   blank space to the right, so that the user can see that there are no
     *   more tabs to the right of the last one)
     */
    if (new_pos + dispwid > doc_width_ + 16)
        new_pos = doc_width_ + 16 - dispwid;

    /* limit the position so that the scroll offset is never negative */
    if (new_pos < 0)
        new_pos = 0;

    /* if that doesn't change the position, there's nothing to do */
    if (new_pos == scroll_pos_)
        return;

    /* animate the scrolling, if desired and we're moving far enough */
    if (!animate || (inc = (new_pos - scroll_pos_) / 5) == 0)
    {
        /* the distance is too short to animate - do it in one go */
        scroll_pos_ = new_pos;
    }
    else
    {
        /* do four partial scrolls */
        for (int i = 0 ; i < 4 ; ++i)
        {
            scroll_pos_ += inc;
            InvalidateRect(handle_, 0, TRUE);
            UpdateWindow(handle_);

            Sleep(10);
        }

        /* set the final position */
        scroll_pos_ = new_pos;
    }

    /* make sure we redraw at the new position */
    InvalidateRect(handle_, 0, TRUE);
}

/*
 *   Scroll the selection into view
 */
void MdiTabCtrl::scroll_sel_into_view()
{
    MdiTabItem *cur;
    int selwid;
    int x;
    int dispwid = get_disp_width();

    /* if there's no selection, there's nothing to do */
    if (sel_ == 0)
        return;

    /* find the document x position of the selection */
    for (cur = items_, x = 0 ; cur != 0 && cur != sel_ ;
         x += cur->get_width(FALSE), cur = cur->nxt) ;

    /* get the width of the selection */
    selwid = sel_->get_width(TRUE);

    /*
     *   If this item is at the left edge of the display area, it's as in
     *   view as we can make it, even if it's so large that it extends past
     *   the right edge.  Otherwise, if it's entirely within the display
     *   width, it's fully in view.  In either case, we don't need to scroll.
     */
    if (x == scroll_pos_
        || (x > scroll_pos_ && x + selwid <= scroll_pos_ + dispwid))
        return;

    /*
     *   if this item is too far left, move it to the left; otherwise move it
     *   to the right
     */
    if (x < scroll_pos_)
        scroll_to(x - 32, TRUE);
    else
        scroll_to(x + selwid + 32 - dispwid, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Update command status
 */
TadsCmdStat_t MdiTabCtrl::check_command(const check_cmd_info *info)
{
    /* check the command */
    switch (info->command_id)
    {
    case ID_MDITB_LEFT:
        /* allow left scrolling any time we're scrolled at all right */
        return (scroll_pos_ != 0 ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_MDITB_RIGHT:
        /*
         *   allow right scrolling if there's any document past the right
         *   extent of the display area
         */
        return (doc_width_ - scroll_pos_ >= get_disp_width()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    default:
        /* send others to the parent window */
        return parent_->check_command(info);
    }
}


/*
 *   Handle a command notification
 */
int MdiTabCtrl::do_command(int notify_code, int cmd, HWND ctl)
{
    /* check the command */
    switch (cmd)
    {
    case ID_MDITB_LEFT:
        /* scroll left by 90% of the display width */
        scroll_by(-get_disp_width()*9/10, TRUE);
        return TRUE;

    case ID_MDITB_RIGHT:
        /* scroll right by 90% of the display width */
        scroll_by(get_disp_width()*9/10, TRUE);
        return TRUE;

    default:
        /* send others to the parent window */
        return parent_->do_command(notify_code, cmd, handle_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Paint
 */
void MdiTabCtrl::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    int x;
    MdiTabItem *cur;
    HFONT old_font;
    HPEN old_pen;
    int old_bk;

    /* select the base font and pen */
    old_font = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    old_pen = (HPEN)GetCurrentObject(hdc, OBJ_PEN);
    old_bk = SetBkMode(hdc, TRANSPARENT);

    /* erase the background */
    FillRect(hdc, area_to_draw, GetSysColorBrush(COLOR_BTNFACE));

    /* draw each item */
    for (x = -scroll_pos_, cur = items_ ;
         x < area_to_draw->right && cur != 0 ;
         cur = cur->nxt)
    {
        int wid;

        /* get the item's width */
        wid = cur->get_width(cur == sel_);

        /* if this item overlaps the drawing area, draw it */
        if (x + wid >= area_to_draw->left)
            cur->draw(hdc, x, cur == sel_);

        /* advance past it */
        x += wid;
    }

    /* restore the old font and pen */
    SelectObject(hdc, old_font);
    SelectObject(hdc, old_pen);
    SetBkMode(hdc, old_bk);
}

/*
 *   Find the item containing the given window x position
 */
MdiTabItem *MdiTabCtrl::find_item_by_x(int x) const
{
    int xcur;
    MdiTabItem *cur;

    /* find the item containing the point */
    for (xcur = -scroll_pos_, cur = items_ ; cur != 0 ; cur = cur->nxt)
    {
        int wid;

        /* measure this item's width */
        wid = cur->get_width(cur == sel_);

        /* if the point is within this item, we've found it */
        if (x >= xcur && x < xcur + wid)
            return cur;

        /* advance past this item */
        xcur += wid;
    }

    /* we didn't find a match */
    return 0;
}

/*
 *   Find an item by index
 */
MdiTabItem *MdiTabCtrl::find_item_by_index(int idx) const
{
    int curidx;
    MdiTabItem *cur;

    /* scan the list for the item at the given index */
    for (curidx = 0, cur = items_ ; cur != 0 && curidx != idx ;
         cur = cur->nxt, ++curidx) ;

    /* return the item, if we found one */
    return cur;
}

/*
 *   Given an item, find its index
 */
int MdiTabCtrl::get_item_index(MdiTabItem *item) const
{
    int curidx;
    MdiTabItem *cur;

    /* scan the list for the item */
    for (curidx = 0, cur = items_ ; cur != 0 && cur != item ;
         cur = cur->nxt, ++curidx) ;

    /* if we found the item, return the index, otherwise -1 */
    return (cur == 0 ? -1 : curidx);
}

/*
 *   Select the given item
 */
void MdiTabCtrl::select_item(MdiTabItem *item)
{
    NMHDR nm;

    /* if the item isn't actually changing, there's nothing to do */
    if (sel_ == item)
        return;

    /* activate this item */
    sel_ = item;

    /* make sure we redraw with the new selection */
    InvalidateRect(handle_, 0, TRUE);

    /* notify our parent of the change */
    nm.hwndFrom = handle_;
    nm.idFrom = 0;
    nm.code = TCN_SELCHANGE;
    SendMessage(parent_->get_handle(), WM_NOTIFY, 0, (LPARAM)&nm);

    /* update the document width */
    update_doc_width();

    /* keep the selection in view */
    scroll_sel_into_view();
}

/*
 *   Update the document width
 */
void MdiTabCtrl::update_doc_width()
{
    MdiTabItem *cur;

    /* run through the items, adding up their current widths */
    for (cur = items_, doc_width_ = 0 ; cur != 0 ; cur = cur->nxt)
        doc_width_ += cur->get_width(cur == sel_);

    /* make sure the scrolling position is valid for the new width */
    scroll_by(0, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   get the number of items
 */
int MdiTabCtrl::get_count()
{
    int i;
    MdiTabItem *item;

    /* run through the list and count the items */
    for (i = 0, item = items_ ; item != 0 ; ++i, item = item->nxt) ;

    /* return the count */
    return i;
}

/*
 *   add an item; returns the item's index
 */
int MdiTabCtrl::insert_item(const textchar_t *title, size_t len,
                            void *param, int idx)
{
    MdiTabItem *new_item;
    MdiTabItem *cur, *prv;
    int curidx;

    /* create the new item structure */
    new_item = new MdiTabItem(title, len, param);

    /* scan for the index position */
    for (prv = 0, cur = items_, curidx = 0 ;
         cur != 0 && curidx != idx ;
         prv = cur, cur = cur->nxt, ++curidx) ;

    /* set the link from the previous item */
    if (prv != 0)
        prv->nxt = new_item;
    else
        items_ = new_item;

    /* set the link to the next item */
    new_item->nxt = cur;

    /* if there's no selection, select this tab */
    if (sel_ == 0)
        select_item(new_item);
    else
        update_doc_width();

    /* make sure we redraw the bar in its new state */
    InvalidateRect(handle_, 0, TRUE);

    /* return the current index, which is where we inserted the item */
    return curidx;
}

/*
 *   unlink an item
 */
MdiTabItem *MdiTabCtrl::unlink(MdiTabItem *item)
{
    MdiTabItem *cur, *prv;

    /* find the item in the list */
    for (prv = 0, cur = items_ ; cur != 0 ; prv = cur, cur = cur->nxt)
    {
        /* if this is the item, unlink it */
        if (cur == item)
        {
            /* unlink it */
            if (prv != 0)
                prv->nxt = cur->nxt;
            else
                items_ = cur->nxt;

            /* return the previous item */
            return prv;
        }
    }

    /* didn't find it; there's no previous item */
    return 0;
}

/*
 *   link an item into the list
 */
void MdiTabCtrl::link_before(MdiTabItem *item, MdiTabItem *other)
{
    MdiTabItem *cur, *prv;

    /* find the 'other' item in the list */
    for (prv = 0, cur = items_ ; ; prv = cur, cur = cur->nxt)
    {
        /* if this is the 'other' item, link 'item' here */
        if (cur == other)
        {
            /* link it before 'other' */
            item->nxt = other;
            if (prv != 0)
                prv->nxt = item;
            else
                items_ = item;

            /* we're done */
            return;
        }

        /* stop if this is the last item */
        if (cur == 0)
            return;
    }
}

/*
 *   move an item to a new position in the list
 */
void MdiTabCtrl::move_item(int old_idx, int new_idx)
{
    MdiTabItem *item;
    MdiTabItem *before;

    /* if it's not moving, there's nothing to do */
    if (old_idx == new_idx)
        return;

    /* if the new index is -1, treat it as a delection */
    if (new_idx == -1)
    {
        delete_item(old_idx);
        return;
    }

    /* find the item we're moving */
    if ((item = find_item_by_index(old_idx)) == 0)
        return;

    /*
     *   Find the item currently at the new index - we'll re-insert the item
     *   we're moving just before this item.  If the index is past the end of
     *   the list, we'll get null for this item, which happens to be the
     *   right thing when we re-insert it, because inserting before null
     *   means appending at the end of the list.
     */
    before = find_item_by_index(new_idx);

    /* unlink it */
    unlink(item);

    /* reinsert it at its new position */
    link_before(item, before);
}

/*
 *   delete an item
 */
void MdiTabCtrl::delete_item(int idx)
{
    MdiTabItem *item, *prv;

    /* find the item */
    if ((item = find_item_by_index(idx)) == 0)
        return;

    /* unlink it */
    prv = unlink(item);

    /* if this is the selected item, select another one */
    if (item == sel_)
    {
        /*
         *   select the next item if there is one, otherwise the previous one
         *   if there is one, otherwise nothing
         */
        select_item(item->nxt != 0 ? item->nxt : prv);
    }
    else
    {
        /* no change in selection, but do update the doc width */
        update_doc_width();
    }

    /* delete it */
    delete item;

    /* scroll the current selection into view */
    scroll_sel_into_view();

    /* make sure we redraw the bar in its new state */
    InvalidateRect(handle_, 0, TRUE);
}

/*
 *   find an item by name; returns the index, or -1 if not found
 */
int MdiTabCtrl::find_item(const textchar_t *title, size_t len) const
{
    MdiTabItem *cur;
    int idx;

    /* scan for a match */
    for (idx = 0, cur = items_ ; cur != 0 ; cur = cur->nxt, ++idx)
    {
        /* check for a match */
        if (get_strlen(cur->get_title()) == len
            && memcmp(cur->get_title(), title, len) == 0)
            return idx;
    }

    /* didn't find a match */
    return -1;
}

/*
 *   find an item by the parameter associated at creation
 */
int MdiTabCtrl::find_item_by_param(void *param) const
{
    MdiTabItem *cur;
    int idx;

    /* scan for a match */
    for (idx = 0, cur = items_ ; cur != 0 ; cur = cur->nxt, ++idx)
    {
        /* check for a match */
        if (cur->param == param)
            return idx;
    }

    /* didn't find a match */
    return -1;
}

/*
 *   Get an item's text
 */
const textchar_t *MdiTabCtrl::get_item_title(int idx) const
{
    MdiTabItem *item;

    /* get the item at the given index */
    item = find_item_by_index(idx);

    /* if we found it, return its title; otherwise return null */
    return (item != 0 ? item->get_title() : 0);
}

/*
 *   Set an item's title
 */
void MdiTabCtrl::set_item_title(int idx, const textchar_t *title, size_t len)
{
    MdiTabItem *item;

    /* get the item at the given index */
    item = find_item_by_index(idx);

    /* if we found it, set the new title */
    if (item != 0)
        item->set_title(title, len);

    /* redraw for the change */
    InvalidateRect(handle_, 0, TRUE);
}

/*
 *   get an item's parameter value
 */
void *MdiTabCtrl::get_item_param(int idx) const
{
    MdiTabItem *item;

    /* get the item at the given index */
    item = find_item_by_index(idx);

    /* if we found it, return its parameter value; otherwise return null */
    return (item != 0 ? item->param : 0);
}


/*
 *   Get the selected item
 */
int MdiTabCtrl::get_selection() const
{
    /* return the selection's index, or -1 if there's no selection */
    return (sel_ == 0 ? -1 : get_item_index(sel_));
}

/*
 *   Set the selected item
 */
void MdiTabCtrl::select_item(int idx)
{
    /*
     *   if the index is -1, set the selection to null; otherwise, set the
     *   selection to the item at the given index
     */
    if (idx == -1)
        select_item((MdiTabItem *)0);
    else
    {
        MdiTabItem *item;

        /* if there's an item at this index, select it */
        if ((item = find_item_by_index(idx)) != 0)
            select_item(item);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Handle a click
 */
int MdiTabCtrl::do_leftbtn_down(int keys, int x, int y, int clicks)
{
    MdiTabItem *item;

    /* check to see if it's in one of our items */
    if ((item = find_item_by_x(x)) != 0)
    {
        /*
         *   if this tab is already selected, and the MDI client window
         *   doesn't have focus, explicitly move focus into the MDI client -
         *   this lets the user move focus back to the main window from a
         *   docked window simply by clicking on the tab
         */
        if (item == sel_)
            focus_to_mdiclient();

        /* select this tab */
        select_item(item);

        /* capture the mouse, to allow dragging to rearrange the tabs */
        SetCapture(handle_);
        dragging_tab_ = item;
        dragging_ticks_ = 0;

        /* start the drag timer */
        SetTimer(handle_, drag_timer_ = alloc_timer_id(), 50, 0);
    }
    else
    {
        /* it's out in the unused area - treat it as a focus click */
        focus_to_mdiclient();
    }

    /* handled */
    return TRUE;
}

/*
 *   move focus to our MDI client window, if it's not in the parent or one of
 *   its children already
 */
void MdiTabCtrl::focus_to_mdiclient()
{
    /* get the current focus */
    HWND f = GetFocus();

    /*
     *   if focus isn't in our MDI client or a child thereof, move focus to
     *   the parent explicitly
     */
    if (f != mdiclient_ && !IsChild(mdiclient_, f))
        SetFocus(mdiclient_);
}

/*
 *   Handle a mouse-move event
 */
int MdiTabCtrl::do_mousemove(int keys, int x, int y)
{
    RECT rc;
    POINT pt;

    /* get our tab display bounds */
    GetClientRect(handle_, &rc);
    rc.right = rc.left + get_disp_width();

    pt.x = x;
    pt.y = y;

    /*
     *   if we're dragging a tab, and we're within bounds, check for a new
     *   position
     */
    if (dragging_tab_ != 0 && PtInRect(&rc, pt))
    {
        MdiTabItem *item;

        /* find the item we're currently over */
        item = find_item_by_x(x);

        /* if we're off the edge to the left, use the first item */
        if (item == 0 && x < 0)
            item = items_;

        /* if it's not the same item or the next item, we can drop here */
        if (item != dragging_tab_)
        {
            /* if they're over the next item, move one item further right */
            if (item == dragging_tab_->nxt && item != 0)
                item = item->nxt;

            /* unlink it from the list and re-insert it at the new position */
            unlink(dragging_tab_);
            link_before(dragging_tab_, item);

            /* redraw immediately */
            InvalidateRect(handle_, 0, TRUE);
            UpdateWindow(handle_);

            /* definitely set the drag cursor once we've moved something */
            SetCursor(drag_csr_);
        }
    }

    /* handled */
    return TRUE;
}

/*
 *   Handle a button-up
 */
int MdiTabCtrl::do_leftbtn_up(int keys, int x, int y)
{
    /* end any tracking we're doing */
    if (dragging_tab_ != 0)
        ReleaseCapture();

    /* handled */
    return TRUE;
}

/*
 *   Handle a capture change
 */
int MdiTabCtrl::do_capture_changed(HWND)
{
    /* if we're not tracking anything, there's nothing for us to do */
    if (dragging_tab_ == 0)
        return FALSE;

    /* set the arrow cursor */
    SetCursor(arrow_cursor_);

    /* delete the drag timer */
    KillTimer(handle_, drag_timer_);
    free_timer_id(drag_timer_);
    drag_timer_ = 0;

    /* forget the tracking item */
    dragging_tab_ = 0;

    /* handled */
    return TRUE;
}

/*
 *   handle a right-click
 */
int MdiTabCtrl::do_rightbtn_down(int keys, int x, int y, int clicks)
{
    NMHDR nm;
    MdiTabItem *item;

    /* check to see if it's in one of our items */
    if ((item = find_item_by_x(x)) != 0)
    {
        /* select this tab */
        select_item(item);

        /* send an NM_RCLICK notification to our parent */
        nm.hwndFrom = handle_;
        nm.idFrom = 0;
        nm.code = NM_RCLICK;
        SendMessage(parent_->get_handle(), WM_NOTIFY, 0, (LPARAM)&nm);
    }

    /* handled */
    return TRUE;
}

/*
 *   timers
 */
int MdiTabCtrl::do_timer(int timer_id)
{
    if (timer_id == drag_timer_)
    {
        int wid;
        POINT pt;

        /* set the drag cursor if it's been long enough */
        if (++dragging_ticks_ >= 5)
            SetCursor(drag_csr_);

        /* get the mouse position in local coordinates */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);

        /* if we're outside of the visible area, scroll a bit */
        wid = get_disp_width();
        if (pt.x < 0 || pt.x > wid)
            scroll_by(pt.x < 0 ? -32 : 32, FALSE);

        /* handled */
        return TRUE;
    }

    /* inherit default handling */
    return CTadsWin::do_timer(timer_id);
}
