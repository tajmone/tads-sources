/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsstat.h - Windows status line object
Function
  The status line object uses a Windows status line control to place a
  status line at the bottom of a window, optionally with a size-box.

  Ownership and deletion: the parent window must delete this object when
  the parent window is deleted.

  Notifications: the parent window must pass certain notifications to the
  status line object.  For example, when the parent window is resized, it
  must call the status line object so that it can resposition itself
  correctly for the new parent window size.

  Setting the status text:

  The status line object is designed to be shared by several objects, and
  so that the objects sharing the status line need not know about one another
  or coordinate their activities.  To accomplish this, we define an
  interface, CTadsStatusSource, that each object wishing to provide status
  messages must implement.

  At initialization time, each object that will share the status line must
  call register_source() in the status line, passing a pointer to its
  CTadsStatusSource interface.  It must also keep a pointer to the status
  line.  The status line object keeps a list of the registered sources.
  Whenever the status line needs to update its display, it goes through its
  list of sources, asking each source for the current message; if a source
  provides a message, that message is used, otherwise the status line asks
  the next source, continuing until a source provides a message or all
  sources have been asked.  Initially, the sources are ordered in reverse of
  the registration order -- the last source registered is the first source
  called.  However, at any time a source can bring itself to the front of the
  list.

  When a source begins an operation that requires a temporary status line
  message (such as to note that a time-consuming operation is in progress),
  it should bring itself to the front of the list, and note internally the
  operation.  The status line will then run through the list to find the new
  message; the source at the front of the list, because the operation is in
  progress, will report its message.  When the operation is finished, the
  source notes that the operation is finished, and tells the status line to
  update its message; the status line once again looks through the sources,
  but this time the first item (since it's done with its operation) doesn't
  provide a message, hence the message reverts to the one being displayed
  previously.

  When the status of an operation changes (for example, the operation is
  completed, or moves on to a new stage of the same operation), the source
  should simply call the status line's update() routine; this will simply
  cause the status line to ask the sources for a new message.
Notes

Modified
  10/26/97 MJRoberts  - Creation
*/

#ifndef TADSSTAT_H
#define TADSSTAT_H

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

class CTadsStatusline
{
public:
    /*
     *   Create the status line within a given parent window.  If sizebox
     *   is true, we'll create the status line with a sizing box at the
     *   right corner.  id specifies the control ID for the status line
     *   control within the parent window.
     */
    CTadsStatusline(class CTadsWin *parent, int sizebox, int id);

    /* delete the status line */
    ~CTadsStatusline();

    /* get the handle of the status line's control window */
    HWND get_handle() { return handle_; }

    /*
     *   Add a part.  We always create one initial part representing the main
     *   status message area.
     */
    void add_part(class CTadsStatusPart *part, int before_index);

    /* get the main part - this is the default part we initially create */
    class CTadsStatusPart *main_part() const { return main_part_; }

    /*
     *   Handle a WM_DRAWITEM (owner draw event).  If the status line
     *   includes any owner-drawn items, the owner window must call this when
     *   it receives a WM_DRAWITEM message.  We'll dispatch the drawing to
     *   the item that triggered the owner-draw event.  Returns true if
     *   handled, false if not.
     *
     *   The window event handler can simply call this routine as its first
     *   step, without even inspecting the event parameters; we'll determine
     *   if it the event really was intended for us, and handle it and return
     *   TRUE if so.  If the event isn't for us, we'll simply return FALSE so
     *   that the caller can look at other possibilities.
     */
    int owner_draw(int ctl_id, DRAWITEMSTRUCT *di);

    /*
     *   handle a WM_MENUSELECT message - the application object calls this
     *   in each registered statusline whenever a WM_MENUSELECT is processed
     */
    void menu_select_msg(HWND hwnd, WPARAM wparam, LPARAM lparam);


    /* ----------------------------------------------------------------- */
    /*
     *   Update operations.  When a source has a new status message to
     *   display, it should call one of these routines.  When one of these
     *   routines is called, we'll go through the list of status sources
     *   to get a message, and display the first message we find.
     */

    /*
     *   Update the status line, using existing source list ordering.  A
     *   source should call this when the state of an existing operation
     *   changes (for example, the operation is finished, or a new stage
     *   of the operation begins).
     */
    void update();


    /* ----------------------------------------------------------------- */
    /*
     *   Notifications - the parent window must call these routines to
     *   notify the status line object of certain events.
     */

    /* notify status line that the parent window was resized */
    void notify_parent_resize();

private:
    /*
     *   Remove a source from the list, returning a pointer to the list
     *   item container for the source item.  This doesn't delete
     *   anything; it just unlinks it from the list.
     */
    class CTadsStatusSourceListitem *unlink(class CTadsStatusSource *source);

    /* statusline control window handle */
    HWND handle_;

    /* part list */
    class CTadsStatusPart *parts_;

    /* main (default) part */
    class CTadsStatusPart *main_part_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Status source.  Objects that provide status messages must implement
 *   this interface.
 */
class CTadsStatusSource
{
public:
    virtual ~CTadsStatusSource() { }

    /*
     *   Get the current message for this source.  If the source does not
     *   currently have anything to report, it should return null.  If the
     *   character array returned was allocated, this routine must set
     *   *caller_deletes to true; in this case, the caller (i.e., the status
     *   line object) will use th_free() to free the memory returned.  We'll
     *   treat this as a (const textchar_t *) if we don't have to delete it.
     *
     *   Return the special value OWNER_DRAW to create an owner-drawn status
     *   area.
     */
    virtual textchar_t *get_status_message(int *caller_deletes) = 0;

    /*
     *   Do owner drawing.  When get_status_message() returns OWNER_DRAW,
     *   this will be called by the window event handler when the window
     *   needs to be refreshed.  Returns true if the drawing was handled,
     *   false if not.
     */
    virtual int status_owner_draw(DRAWITEMSTRUCT *) { return FALSE; }

    /*
     *   Receive notification of a change in the layout of the status bar.
     *   This allows an owner-drawn part to move any controls it creates to
     *   the new position within the status bar.
     */
    virtual void status_change_pos(const RECT *) { }

    /*
     *   Handle a menu item select message.  This gives our status source a
     *   chance to set up a help message to display in the status line.  If
     *   this returns TRUE, we'll update the status line automatically;
     *   otherwise we'll leave it as-is.
     */
    virtual int do_status_menuselect(WPARAM, LPARAM) { return FALSE; }

    /* special get_status_message() return value: the status is owner-drawn */
    static textchar_t OWNER_DRAW[];
};


/* ------------------------------------------------------------------------ */
/*
 *   Status source container.  The status line uses this class to build a
 *   list of status source objects.
 */
class CTadsStatusSourceListitem
{
public:
    CTadsStatusSourceListitem(CTadsStatusSource *item)
    {
        item_ = item;
        nxt_ = 0;
    }

    /* source at this item */
    CTadsStatusSource *item_;

    /* next item in the list */
    CTadsStatusSourceListitem *nxt_;
};

/*
 *   Status line part.  A status line can be partitioned visually into
 *   sections that show different information; each section is called a
 *   "part" in Windows parlance.  This represents one part.  Each part can
 *   have a group of sources that provide the display data.
 */
class CTadsStatusPart
{
    friend class CTadsStatusline;

public:
    CTadsStatusPart()
    {
        /* no statusline object yet */
        stat_ = 0;

        /* no sources yet */
        sources_ = 0;

        /* not in a list yet */
        nxt_ = 0;
    }

    virtual ~CTadsStatusPart()
    {
        /* delete our sources */
        while (sources_ != 0)
        {
            /* unlink the head of the list */
            CTadsStatusSourceListitem *cur = sources_;
            sources_ = cur->nxt_;

            /* delete the unlinked item */
            delete cur;
        }
    }

    /*
     *   Calculate the width of this part.  If the return value is positive,
     *   it's the number of pixels that this part claims.  Zero makes the
     *   part invisible.  If the return value is negative, this part will use
     *   the remaining space after subtracting all of the parts that return
     *   positive widths.  If multiple parts return negative values, they'll
     *   share the remaining space by dividing it proportionally to their
     *   negative values.  For example, if part A returns -1, and part B
     *   returns -2, part A will take 1/3 of the leftover space and part B
     *   will take 2/3.  By default, we return -1 to claim one share of the
     *   leftover space.
     */
    virtual int calc_width() const { return -1; }

    /* register a new status source */
    void register_source(class CTadsStatusSource *source)
    {
        /* create a new list item */
        CTadsStatusSourceListitem *i = new CTadsStatusSourceListitem(source);

        /* put it at the head of the list */
        i->nxt_ = sources_;
        sources_ = i;
    }

    /* unregister a source */
    void unregister_source(class CTadsStatusSource *source)
    {
        /* find this item and unlink it from the list */
        CTadsStatusSourceListitem *item = unlink(source);

        /*
         *   if we found the list container, delete it, since we no longer
         *   have any use for it
         */
        if (item != 0)
            delete item;
    }

    /*
     *   Bring a source to the front of the source list, and update the
     *   message
     */
    void source_to_front(CTadsStatusSource *source)
    {
        /* find this source in the list and unlink it from the list */
        CTadsStatusSourceListitem *cur = unlink(source);

        /* if we found it, link it back in at the start of the list */
        if (cur != 0)
        {
            cur->nxt_ = sources_;
            sources_ = cur;
        }

        /* update the status line message to account for the change */
        stat_->update();
    }

protected:
    /*
     *   Unlink an item from the list
     */
    CTadsStatusSourceListitem *unlink(CTadsStatusSource *source)
    {
        CTadsStatusSourceListitem *cur, *prv;

        /* find the item in the list */
        for (prv = 0, cur = sources_ ; cur != 0 ; prv = cur, cur = cur->nxt_)
        {
            /* check if this is the item we're looking for */
            if (cur->item_ == source)
            {
                /* unlink it */
                if (prv != 0)
                    prv->nxt_ = cur->nxt_;
                else
                    sources_ = cur->nxt_;

                /* return it */
                return cur;
            }
        }

        /* didn't find it */
        return 0;
    }

    /* our status line container */
    CTadsStatusline *stat_;

    /* head of source list */
    CTadsStatusSourceListitem *sources_;

    /* next part in the master list of parts */
    CTadsStatusPart *nxt_;
};

#endif /* TADSSTAT_H */

