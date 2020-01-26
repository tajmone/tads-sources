/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32webui.h - local browser frame for TADS Web UI
Function
  The TADS Web UI allows a game to use a Web browser as its user interface.
  There are two ways to run such a game: as a true client/server app across
  the network, or as a standalone app on your PC.  To run in the client/server
  configuration, you set up the game on a Web server, and simply point your
  Web browser to the server; the Web browser acts as the user interface for
  the game, so you don't need to install any other software on your PC.  For
  the standalone version, in contrast, the whole app runs on your PC, both
  server and client.  Rather than using a separate Web browser program, the
  standalone interpreter uses an integrated, embedded browser control.  This
  file defines the window frame that contains the embedded browser.
Notes

Modified
  05/12/10 MJRoberts  - Creation
*/

#ifndef W32WEBUI_H
#define W32WEBUI_H

#include "tadshtml.h"
#include "htmlw32.h"
#include "tadswebctl.h"
#include "tadswin.h"
#include "tadsstat.h"


/* ------------------------------------------------------------------------ */
/*
 *   Web UI window
 */
class CHtmlSys_webuiwin: public CTadsWin, public CBrowserFrame,
    public CTadsStatusSource
{
public:
    CHtmlSys_webuiwin(CHtmlSys_webuiwin *parent,
                      class CHtmlPreferences *prefs);
    ~CHtmlSys_webuiwin();

    /* route command handling to the main debug window */
    TadsCmdStat_t check_command(const check_cmd_info *);
    int do_command(int notify_code, int command_id, HWND ctl);

    /* handle a left/right-click on the title bar */
    int do_nc_leftbtn_down(int keys, int x, int y, int clicks, int hit_type);
    int do_nc_rightbtn_down(int keys, int x, int y, int clicks, int hit_type);

    /*
     *   note a URL change - the event receiver calls this to let us know
     *   when we've navigated to a new page
     */
    void on_url_change(const textchar_t *url)
    {
        /* remember our new URL */
        url_.set(url);
    }

    /* receive notification that the document has finished loading */
    void on_doc_done(const textchar_t *url)
    {
        /* note that we've loaded at least one document */
        first_load_ok_ = TRUE;

        /* assume this means the game is running */
        game_running_ = TRUE;
    }

    /* open a new window, in response to a request from the web page */
    virtual void open_new_window(
        VARIANT_BOOL *cancel, IDispatch **disp,
        const char *url, const char *src_url, enum NWMF flags);

    /* note a new document title */
    virtual void note_new_title(const textchar_t *title)
    {
        SetWindowText(handle_, title);
    }

    /* note a status bar change */
    virtual void note_new_status_text(const textchar_t *txt)
    {
        // Note - ignore status updates for now.  MSHTML blocks script
        // access to the status line, which is the main thing we'd want
        // it for.  (The user can enable script access, but this is an
        // extra step that most users won't want to do, since access is
        // disabled for security reasons.)

        /* save the new text */
        // status_from_ctl_.set(txt);

        /* update our status line display */
        //if (statusline_ != 0)
        //    statusline_->update();
    }

    /* hide/show the window, controlled from the browser control */
    virtual void on_visible(int show)
    {
        /* show or hide the window */
        ShowWindow(handle_, show ? SW_SHOW : SW_HIDE);
    }

    /* adjust window dimensions to fit the given browser control size */
    virtual void adjust_window_dimensions(long *width, long *height)
    {
        /* figure the height of the status bar, if present */
        RECT statrc;
        statrc.bottom = 0;
        if (statusline_ != 0)
            GetClientRect(statusline_->get_handle(), &statrc);

        /* add the status bar height to the required window height */
        *height += statrc.bottom;
    }

    /* set the width from the browser control */
    virtual void browser_set_width(long width)
    {
        /* get the current client rectangle */
        RECT rc;
        GetClientRect(handle_, &rc);

        /*
         *   calculate the window rectangle for the client rectangle at the
         *   new width
         */
        rc.right = rc.left + width;
        AdjustWindowRectEx(&rc, get_winstyle(), FALSE, get_winstyle_ex());

        /* if the width has changed, move the window */
        RECT wrc;
        GetWindowRect(handle_, &wrc);
        if (wrc.right - wrc.left != rc.right - rc.left)
            MoveWindow(handle_, wrc.left, wrc.top,
                       rc.right - rc.left, wrc.bottom - wrc.top, TRUE);
    }

    /* set the window height from the browser control */
    virtual void browser_set_height(long height)
    {
        /* get the current client rectangle */
        RECT rc;
        GetClientRect(handle_, &rc);

        /* add the statusbar height, if present */
        if (statusline_ != 0)
        {
            RECT sbrc;
            GetClientRect(statusline_->get_handle(), &sbrc);
            height += sbrc.bottom;
        }

        /*
         *   calculate the window rectangle for the client rectangle at the
         *   new height
         */
        rc.bottom = rc.top + height;
        AdjustWindowRectEx(&rc, get_winstyle(), FALSE, get_winstyle_ex());

        /* if the width has changed, move the window */
        RECT wrc;
        GetWindowRect(handle_, &wrc);
        if (wrc.bottom - wrc.top != rc.bottom - rc.top)
            MoveWindow(handle_, wrc.left, wrc.top,
                       wrc.right - wrc.left, rc.bottom - rc.top, TRUE);
    }

    /* note that the server is disconnecting */
    void on_server_disconnect()
    {
        /* note that we've received a disconnect message */
        srv_disconnected_ = TRUE;

        /* update the status line */
        if (statusline_ != 0)
            statusline_->update();

        /*
         *   If the server aborted before we were able to load our first
         *   document, the window is kind of useless - it'll never manage to
         *   navigate to its initial page, so it'll never be able to show
         *   anything but a blank screen.  Any error information is thus
         *   available only in the server's own UI window.  We can simply
         *   close this window to avoid confusing the user.
         */
        if (!first_load_ok_)
            PostMessage(handle_, WM_CLOSE, 0, 0);

        /* the game is no longer running */
        game_running_ = FALSE;
    }

    /* are we connected to the server? */
    int is_connected() const { return !srv_disconnected_; }

    /* closing a child */
    void on_close_child(CHtmlSys_webuiwin *chi);

    /*
     *   set the TADS version string - the host communicator thread calls
     *   this when it receives the information from the host process
     */
    void set_tads_ver(const char *ver) { tads_ver_.set(ver); }

    /* get the version string, if known */
    const char *get_tads_ver() const { return tads_ver_.get(); }

    /*
     *   Process an askfile request from the interpreter.  In standalone
     *   mode, we want to store files in the local file system, just like the
     *   conventional interpreter does, which means we want to use the
     *   regular Windows file dialog for the "askfile" UI.  The snag is that
     *   the interpreter can't show the dialog directly because it's running
     *   in a separate process from the UI.  To solve this, the interpreter
     *   uses its pipe control channel to send us a request to show the
     *   askfile UI, and we send the result back through our return pipe.
     */
    int askfile(char *buf, size_t buflen, char *params);

    /*
     *   Statusline source interface
     */
    virtual textchar_t *get_status_message(int *caller_deletes);
    virtual int do_status_menuselect(WPARAM wpar, LPARAM lpar);

protected:
    /* this is a top-level application window */
    DWORD get_winstyle()
    {
        return (WS_OVERLAPPEDWINDOW | WS_BORDER
                | WS_CAPTION | WS_SYSMENU
                | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
                | WS_SIZEBOX);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* get the browser control's window handle */
    HWND get_browser_hwnd()
    {
        IOleInPlaceObject *ipo;
        HWND hwnd = 0;

        /* ask the browser control for its in-place object interface */
        if (browser_ != 0
            && SUCCEEDED(browser_->QueryInterface(
                IID_IOleInPlaceObject, (void **)&ipo))
            && ipo != 0)
        {
            /* get the window handle from the in-place object */
            if (!SUCCEEDED(ipo->GetWindow(&hwnd)))
                hwnd = 0;

            /* done with the interface */
            ipo->Release();
        }

        /* return what we found, if anything */
        return hwnd;
    }

    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* our HTML control covers the whole area, so no need to erase anything */
    virtual int do_erase_bkg(HDC) { return TRUE; }

    /* move */
    void do_move(int x, int y);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* handle non-client activation */
    int do_ncactivate(int flag);

    /* handle notifications */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* focus gain/loss */
    void do_setfocus(HWND prv);
    void do_killfocus(HWND nxt);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* store my window position in the preferences file */
    void store_winpos_prefs();

    /* handle timer messages */
    virtual int do_timer(int timer_id);

    /* adjust the statusline layout */
    void adjust_statusbar_layout();

    /* our parent window, if we're a subwindow */
    CHtmlSys_webuiwin *parent_;

    /* head of child list; next sibling */
    CHtmlSys_webuiwin *first_child_;
    CHtmlSys_webuiwin *next_sibling_;

    /* main htmltads preferences object */
    class CHtmlPreferences *prefs_;

    /* our current URL */
    CStringBuf url_;

    /* TADS version string */
    CStringBuf tads_ver_;

    /* status line */
    class CTadsStatusline *statusline_;

    /* status message as set by the embedded browser control */
    CStringBuf status_from_ctl_;

    /* timer ID for watching for focus changes */
    int focus_timer_;

    /* did we have focus when last we checked? */
    int has_focus_;

    /* have we successfully loaded our first document? */
    int first_load_ok_;

    /* true -> the server has disconnected */
    int srv_disconnected_;

    /* on-screen timer - starting time */
    DWORD starting_ticks_;

    /* on-screen timer - paused */
    int timer_paused_;

    /*
     *   on-screen timer - starting time of current pause, for adjusting the
     *   starting time when we restart the timer
     */
    DWORD pause_starting_ticks_;

    /* is the game running? */
    int game_running_;

    /* statusline popup menu */
    HMENU statusline_menu_;
};

#endif /* W32HELP_H */
