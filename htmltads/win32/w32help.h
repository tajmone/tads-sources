/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32help.h - help/documentation viewer window
Function
  This class implements the Workbench help/doc viewer window.  This is a
  docking window that contains an MSHTML browser control, which we use to
  view HTML documentation files.
Notes

Modified
  12/29/06 MJRoberts  - Creation
*/

#ifndef W32HELP_H
#define W32HELP_H

#include "tadshtml.h"
#include "htmlw32.h"
#include "w32tdb.h"
#include "tadsdock.h"
#include "tadscom.h"
#include "tadswebctl.h"


/* ------------------------------------------------------------------------ */
/*
 *   Help window
 */
class CHtmlSys_dbghelpwin: public CTadsWin, public IDebugSysWin,
    public CHtmlDbgSubWin, public CBrowserFrame
{
public:
    CHtmlSys_dbghelpwin(class CHtmlPreferences *prefs,
                        class CHtmlDebugHelper *helper,
                        class CHtmlSys_dbgmain *dbgmain);

    ~CHtmlSys_dbghelpwin();

    /* route command handling to the main debug window */
    TadsCmdStat_t check_command(const check_cmd_info *);
    int do_command(int notify_code, int command_id, HWND ctl);

    /* handle a left/right-click on the title bar */
    int do_nc_leftbtn_down(int keys, int x, int y, int clicks, int hit_type);
    int do_nc_rightbtn_down(int keys, int x, int y, int clicks, int hit_type);

    /* IDebugSysWin implementation */
    virtual void idw_add_ref() { AddRef(); }
    virtual void idw_release_ref() { Release(); }
    virtual void idw_close()
    {
        /* close our docking parent */
        SendMessage(GetParent(handle_), DOCKM_DESTROY, 0, 0);
    }
    virtual int idw_query_close(struct ask_save_ctx *ctx) { return TRUE; }
    virtual int idw_is_dirty() { return FALSE; }
    virtual void idw_reformat(const CHtmlRect *new_scroll_pos) { }
    virtual void idw_post_load_init() { }
    virtual void idw_show_source_line(int linenum) { }
    virtual CHtmlSysWin *idw_as_syswin() { return 0; }
    virtual HWND idsw_get_handle() { return handle_; }
    virtual HWND idsw_get_frame_handle() { return handle_; }
    virtual void idsw_select_line() { }
    virtual void idsw_bring_to_front()
    {
        HWND f;

        /* I'm a docking window, so bring my parent to the top */
        BringWindowToTop(GetParent(handle_));

        /* if I don't have focus, set focus in the browser window */
        f = GetFocus();
        if (f != handle_ && !IsChild(handle_, f))
            SetFocus(f); //$$$
    }
    virtual void idsw_note_debug_format_changes(
        HTML_color_t bkg_color, HTML_color_t text_color,
        int use_windows_colors, int use_windows_bgcolor,
        HTML_color_t sel_bkg_color, HTML_color_t sel_text_color,
        int use_windows_sel_colors, int use_windows_sel_bgcolor)
        { }
    virtual void idsw_set_untitled() { }
    virtual const textchar_t *idsw_get_cfg_path() { return ":help"; }
    virtual const textchar_t *idsw_get_cfg_title() { return ""; }

    /* CHtmlDbgSubWin implementation */
    virtual TadsCmdStat_t active_check_command(const check_cmd_info *);
    virtual int active_do_command(int notify_code, int command_id, HWND ctl);
    virtual HWND active_get_handle() { return handle_; }
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end)
        { return FALSE; }
    virtual int get_sel_text(CStringBuf *buf, unsigned int flags,
                             size_t maxlen)
        { return FALSE; }
    virtual int set_sel_range(unsigned long start, unsigned long end)
        { return FALSE; }
    void active_to_front() { idsw_bring_to_front(); }

    /*
     *   note a URL change - the event receiver calls this to let us know
     *   when we've navigated to a new page
     */
    void on_url_change(const textchar_t *url)
    {
        /* remember our new URL */
        url_.set(url);
    }

    /* note a title change */
    virtual void note_new_title(const textchar_t *) { }

    /* note a status bar change */
    virtual void note_new_status_text(const textchar_t *) { }

    /* note that the document has loaded */
    virtual void on_doc_done(const textchar_t *) { }

    /*
     *   Open a new window.  Force the navigation to occur within the current
     *   help window.
     */
    void open_new_window(VARIANT_BOOL *cancel, IDispatch **disp,
                         const char *url, const char *src_url, NWMF flags)
    {
        IWebBrowser2 *new_iwb2;

        /* create a new embedded browser window */
        new_iwb2 = new_browser_ctl(handle_, IDR_HELP_POPUP_MENU, 0, 0);

        /* if that succeeded, use its IDispatch as the new target */
        if (new_iwb2 != 0)
        {
            /* get its IDispatch */
            new_iwb2->QueryInterface(IID_IDispatch, (void **)disp);

            /* done with the new browser interface */
            new_iwb2->Release();
        }
    }

protected:
    /* this is a docking window - make it a child */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
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

    /* handle activation from the parent */
    void do_parent_activate(int flag);

    /* focus gain/loss */
    void do_setfocus(HWND prv);
    void do_killfocus(HWND nxt);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* store my window position in the preferences file */
    void store_winpos_prefs();

    /* handle timer messages */
    virtual int do_timer(int timer_id);

    /* main htmltads preferences object */
    class CHtmlPreferences *prefs_;

    /* debugger helper object */
    class CHtmlDebugHelper *helper_;

    /* main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* our current URL */
    CStringBuf url_;

    /* timer ID for watching for focus changes */
    int focus_timer_;

    /* did we have focus when last we checked? */
    int has_focus_;
};

#endif /* W32HELP_H */

