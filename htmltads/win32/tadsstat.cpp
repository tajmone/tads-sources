#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsstat.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsstat.cpp - status line implementation
Function

Notes

Modified
  10/26/97 MJRoberts  - Creation
*/

#include <windows.h>
#include <commctrl.h>

#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Status line
 */

CTadsStatusline::CTadsStatusline(CTadsWin *parent, int sizebox, int id)
{
    /* make sure common controls are initialized */
    InitCommonControls();

    /* create and show the status line control */
    handle_ = CreateStatusWindow(WS_CHILD | (sizebox ? SBARS_SIZEGRIP : 0),
                                 "", parent->get_handle(), id);
    ShowWindow(handle_, SW_SHOW);

    /* create our main default part */
    parts_ = main_part_ = new CTadsStatusPart();

    /* register with the application object */
    CTadsApp::get_app()->register_statusline(this);
}

CTadsStatusline::~CTadsStatusline()
{
    /* unregister ourselves with the application object */
    CTadsApp::get_app()->unregister_statusline(this);

    /* delete all of our parts */
    while (parts_ != 0)
    {
        /* unlink the first part */
        CTadsStatusPart *cur = parts_;
        parts_ = cur->nxt_;

        /* delete this part */
        delete cur;
    }
}

/*
 *   Update the status line.  Run through the list of sources until we
 *   find a message, then display that message.
 */
void CTadsStatusline::update()
{
    /* go through each part */
    int partno = 0;
    for (CTadsStatusPart *part = parts_ ; part != 0 ;
         part = part->nxt_, ++partno)
    {
        /* no message yet for this part */
        textchar_t *msg = 0;
        int caller_deletes = FALSE;

        /* go through each source until we find a message for this part */
        CTadsStatusSourceListitem *cur;
        for (cur = part->sources_ ; cur != 0 ; cur = cur->nxt_)
        {
            /* ask this item for a message; if we found one, stop looking */
            msg = cur->item_->get_status_message(&caller_deletes);
            if (msg != 0)
                break;
        }

        /* set up the SB_SETTEXT parameters for the part index and message */
        WPARAM wpar = partno;
        LPARAM lpar = (LPARAM)msg;

        /* check for missing messages and special flags */
        if (msg == 0)
        {
            /* no one provided a message - use an empty string */
            lpar = (LPARAM)"";
        }
        else if (msg == CTadsStatusSource::OWNER_DRAW)
        {
            /*
             *   set the owner-drawn flags, using the source item as the
             *   window message LPARAM
             */
            wpar |= SBT_OWNERDRAW;
            lpar = (LPARAM)cur->item_;
        }

        /* display the message */
        SendMessage(handle_, SB_SETTEXT, wpar, lpar);

        /* we're done with the message - if we're to delete it, do so */
        if (caller_deletes)
            th_free(msg);
    }

    /*
     *   update the status line immediately, in case we're in the middle
     *   of a long-running operation and we won't check for events for a
     *   while
     */
    UpdateWindow(handle_);
}

/*
 *   Do owner drawing
 */
int CTadsStatusline::owner_draw(int ctl_id, DRAWITEMSTRUCT *di)
{
    /*
     *   check to see if the message was triggered by the status line - if
     *   not, it's for someone else to handle
     */
    if (di->CtlType == ODT_MENU || di->hwndItem != handle_)
        return FALSE;

    /*
     *   the itemData in the DRAWITEMSTRUCT is the status source object
     *   triggered the event
     */
    CTadsStatusSource *src = (CTadsStatusSource *)di->itemData;

    /* let the source object handle it */
    return src->status_owner_draw(di);
}

/*
 *   Add a part
 */
void CTadsStatusline::add_part(CTadsStatusPart *newpart, int before_index)
{
    /* link the part to the statusline object */
    newpart->stat_ = this;

    /* find the location in the list where we're inserting the new item */
    CTadsStatusPart *cur, *prv;
    for (prv = 0, cur = parts_ ;
         cur != 0 && before_index > 0 ;
         prv = cur, cur = cur->nxt_, --before_index) ;

    /* link in the new item */
    newpart->nxt_ = cur;
    if (prv != 0)
        prv->nxt_ = newpart;
    else
        parts_ = newpart;

    /* figure the new layout */
    notify_parent_resize();
}

/*
 *   Notify the status line that the parent window has been resized
 */
void CTadsStatusline::notify_parent_resize()
{
    /*
     *   tell the status line control to resize itself; we don't need to tell
     *   it anything more (it would ignore us if we did), since the control
     *   is able to figure out where it should go based on the parent window
     *   size
     */
    MoveWindow(handle_, 0, 0, 0, 0, TRUE);

    /* get the new client area size */
    RECT rc;
    GetClientRect(get_handle(), &rc);

    /*
     *   get the width of the sizebox, which we always put at the right edge
     *   of the status bar
     */
    int cxvscroll = GetSystemMetrics(SM_CXVSCROLL);

    /* count the parts */
    int part_cnt = 0;
    CTadsStatusPart *part;
    for (part = parts_ ; part != 0 ; part = part->nxt_, ++part_cnt) ;

    /* create the part width array */
    int *widths = new int[part_cnt];

    /* run through the parts and calculate the fixed and proportional widths */
    int i, fixed_width = 0, pro_width = 0, pro_cnt = 0;
    for (part = parts_, i = 0 ; part != 0 ; part = part->nxt_, ++i)
    {
        /* calculate this part's width, and store it in the widths array */
        int wid = widths[i] = part->calc_width();

        /*
         *   if it's positive, it's a fixed width in pixels; otherwise it's a
         *   proportional share of the leftover space
         */
        if (wid >= 0)
            fixed_width += wid;
        else
        {
            pro_width += wid;
            ++pro_cnt;
        }
    }

    /* figure the leftover space available for the proportional widths */
    int avail = rc.right - rc.left - cxvscroll - fixed_width;
    int rem = avail;

    /* divvy up the leftover space among the proportional items */
    for (i = 0 ; i < part_cnt ; ++i)
    {
        /* if this is a proportional item, allocate space */
        if (widths[i] < 0 && pro_width != 0)
        {
            /*
             *   If this is the last proportional item, simply allocate it
             *   all of the remaining proportional space; this avoids being
             *   off by a pixel one way or the other due to rounding.  If
             *   it's not the last proportional item, allocate its pro rata
             *   space.
             */
            widths[i] = (pro_cnt == 1 ? rem : (widths[i] * avail) / pro_width);

            /* deduct this allocation from the remaining space */
            rem -= widths[i];

            /* count this proportional item */
            --pro_cnt;
        }
    }

    /* add the sizebox width into the space given to the last item */
    widths[part_cnt - 1] += cxvscroll;

    /* convert the widths to right-edge positions for SB_SETPARTS */
    for (i = 1 ; i < part_cnt ; ++i)
        widths[i] += widths[i-1];

    /* set the new size */
    SendMessage(get_handle(), SB_SETPARTS, part_cnt, (LPARAM)widths);

    /* done with the part width array */
    delete [] widths;

    /* notify owner-drawn sources of the change in layout */
    for (part = parts_, i = 0 ; part != 0 ; part = part->nxt_, ++i)
    {
        /* get this part's layout information */
        RECT rc;
        SendMessage(get_handle(), SB_GETRECT, i, (LPARAM)&rc);

        /* notify each source of the change */
        for (CTadsStatusSourceListitem *src = part->sources_ ;
             src != 0 ; src = src->nxt_)
        {
            /* notify this item */
            src->item_->status_change_pos(&rc);
        }
    }

    /*
     *   immediately redraw the status line, in case we're in the middle
     *   of an operation that will suspend event handling for a while
     */
    UpdateWindow(handle_);
}

/*
 *   Handle a WM_MENUSELECT message
 */
void CTadsStatusline::menu_select_msg(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    /* presume we won't need to update anything */
    int upd = FALSE;

    /* run through all parts */
    for (CTadsStatusPart *part = parts_ ; part != 0 ; part = part->nxt_)
    {
        /* run through all sources in this part */
        for (CTadsStatusSourceListitem *src = part->sources_ ;
             src != 0 ; src = src->nxt_)
        {
            /* give this source a chance to handle it */
            upd |= src->item_->do_status_menuselect(wparam, lparam);
        }
    }

    /* if necessary, do an update */
    if (upd)
        update();
}

/* ------------------------------------------------------------------------ */
/*
 *   Status source interface statics
 */

textchar_t CTadsStatusSource::OWNER_DRAW[1] = "";
