/* $Header: d:/cvsroot/tads/html/win32/w32tdb.h,v 1.4 1999/07/11 00:46:52 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32tdb.h - html tads win32 debugger
Function

Notes

Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32TDB_H
#define W32TDB_H

#include <WinInet.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef W32DBWIZ_H
#include "w32dbwiz.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef W32SCI_H
#include "w32sci.h"
#endif
#ifndef ITADSWORKBENCH_H
#include "itadsworkbench.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Extended tool window types.  These are debug window types that aren't
 *   handled in the portable debug helper code.
 */
enum HtmlDbg_syswintype_t
{
    HTMLDBG_SYSWINTYPE_EXPR = HTMLDBG_WINTYPE_sys_base,
    HTMLDBG_SYSWINTYPE_LOCALS,
    HTMLDBG_SYSWINTYPE_PROJECT,
    HTMLDBG_SYSWINTYPE_SCRIPT
};


/* ------------------------------------------------------------------------ */
/*
 *   The command descriptor list.  This is a global static array that gives a
 *   name and description for each menu/toolbar/keyboard command.
 */

#define HTML_SYSID_TADS2  0x0001
#define HTML_SYSID_TADS3  0x0002

struct HtmlDbgCommandInfo
{
    HtmlDbgCommandInfo(unsigned short id, textchar_t *sym,
                       textchar_t *tiptext, unsigned int sysid)
    {
        this->id = id;
        this->sym = sym;
        this->tiptext = tiptext;
        this->sysid = sysid;
    }
    HtmlDbgCommandInfo() { }
    virtual ~HtmlDbgCommandInfo() { }

    /* the command ID code */
    unsigned short id;

    /* the symbolic identifier for the command ("File.Open") */
    const textchar_t *sym;

    /*
     *   command list tooltip description - this is of the form
     *   "Symbol\r\nDescription"
     */
    const textchar_t *tiptext;

    /*
     *   a short "help" description of the command - this is the part of the
     *   tooltip text after the newline
     */
    const textchar_t *get_desc() const
        { return tiptext + get_strlen(sym) + 2; }

    /*
     *   Set my description.  This doesn't do anything for a pre-defined
     *   command, but it's allowed for dynamic extension commands.
     */
    virtual void set_desc(const textchar_t *desc, size_t desclen) { }

    /* version support flags (HTML_SYSID_TADSn) */
    unsigned int sysid;
};

/*
 *   Subclass for extension items.  Unlike the base class, these are
 *   dynamically allocated.
 */
struct HtmlDbgCommandInfoExt: HtmlDbgCommandInfo
{
    HtmlDbgCommandInfoExt(unsigned short id,
                          const textchar_t *name, size_t namelen,
                          const textchar_t *desc, size_t desclen)
    {
        /* remember the basic information */
        this->id = id;
        this->sysid = HTML_SYSID_TADS2 | HTML_SYSID_TADS3;
        this->nxt_ = 0;

        /* store the name */
        symbuf_.set(name, namelen);

        /* store the tip text: "name \r\n desc", or just "name" if no desc */
        tipbuf_.set(name, namelen);
        if (desc != 0)
        {
            tipbuf_.append("\r\n");
            tipbuf_.append(desc, desclen);
        }
        else
            tipbuf_.append("\0\0\0", 3);

        /* set the string pointers */
        this->sym = symbuf_.get();
        this->tiptext = tipbuf_.get();
    }

    /* change the description */
    virtual void set_desc(const textchar_t *desc, size_t desclen)
    {
        /* update the description */
        tipbuf_.set(sym);
        tipbuf_.append("\r\n");
        tipbuf_.append(desc, desclen);

        /* update the tip pointer */
        this->tiptext = tipbuf_.get();
    }

    /* our symbol and tip buffers */
    CStringBuf symbuf_;
    CStringBuf tipbuf_;

    /* next entry in the list */
    HtmlDbgCommandInfoExt *nxt_;
};


/*
 *   system identifier of this build - this is linked in from the
 *   version-specific module
 */
extern int w32_sysid;


/* ------------------------------------------------------------------------ */
/*
 *   Key table structure.  Workbench maintains multiple key tables; the key
 *   table currently in use depends on the mode.  A key table has a name,
 *   which is displayed to the user and is also used to identify the table in
 *   the stored configuration; a parent table, from which the table inherits
 *   bindings for keys not defined within the table itself; and an
 *   accelerator object.
 */
struct HtmlKeyTable
{
    /* the name of the table */
    const textchar_t *name;

    /* the parent table (by name) */
    const textchar_t *parent;

    /* the current accelerator for the table */
    class CTadsAccelerator *accel;
};


/* ------------------------------------------------------------------------ */
/*
 *   Build targets - this identifies which targets we're planning to build
 */
struct TdbBuildTargets
{
    /*  main compilation type */
    enum
    {
        BUILD_NONE,
        BUILD_DEBUG,
        BUILD_RELEASE,
        BUILD_CLEAN
    } build_type;

    /* packaging options - these can be combined */
    int build_exe;
    int build_zip;
    int build_html;
    int build_srczip;
    int build_setup;

    /* IFDC credentials and email address, for publication */
    const char *ifdb_user;
    const char *ifdb_psw;
    const char *email;
};


/* ------------------------------------------------------------------------ */
/*
 *   MDI/SDI selector.  This block of code encapsulates all of the
 *   differences between running the debugger as an SDI application vs. as
 *   an MDI application.
 *
 *   This is currently set up so that the MDI/SDI selection is made at
 *   compile time.  The default is MDI.  Define W32TDB_SDI on the compiler
 *   command line to compile an SDI version of the debugger.
 *
 *   It would be possible to make the MDI/SDI selection at run-time, by
 *   changing the code below so that the MDI and SDI versions of each
 *   function are integrated into a single function that selects the
 *   appropriate behavior based on a global variable.  For now, it seems
 *   sufficient to support only a single mode, since MDI seems clearly
 *   superior to SDI for this application, but this could be changed if
 *   users are divided over which mode they prefer and want to be able to
 *   select a mode at run-time.
 */

#ifndef W32TDB_SDI
#define W32TDB_MDI
#endif

#ifdef W32TDB_MDI

/*
 *   MDI mode
 */

/*
 *   Debugger MDI client window class
 */
class CHtmlWinDbgMdiClient: public CTadsWin
{
public:
    CHtmlWinDbgMdiClient(class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlWinDbgMdiClient();

    /*
     *   refresh the menu - we'll handle this ourselves here by doing
     *   nothing at all, since we rebuild the menu each time it's opened
     */
    virtual int do_mdirefreshmenu() { return TRUE; }

    /* window styles for MDI client window */
    virtual DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL
                | MDIS_ALLCHILDSTYLES);
    }
    virtual DWORD get_winstyle_ex() { return WS_EX_CLIENTEDGE; }

    /* let the MDI client handle painting without our intervention */
    int do_paint() { return FALSE; }
    int do_paint_iconic() { return FALSE; }

    /* right button release - show context menu */
    virtual int do_rightbtn_up(int /*keys*/, int x, int y)
    {
        /* show our context menu */
        track_context_menu(GetSubMenu(ctx_menu_, 0), x, y);

        /* handled */
        return TRUE;
    }

    /* command handling */
    int do_command(int notify_code, int command_id, HWND ctl);
    TadsCmdStat_t check_command(const check_cmd_info *);

    /* reflect MDI child create/delete events to the frame */
    virtual void mdi_child_event(CTadsWin *win, UINT msg,
                                 WPARAM wpar, LPARAM lpar);

protected:
    /* my context menu handle */
    HMENU ctx_menu_;

    /* main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;
};

/* use the MDI child system interface object for children */
inline CTadsSyswin *w32tdb_new_child_syswin(CTadsWin *win)
{
    return new CTadsSyswinMdiChild(win);
}

/*
 *   Set the default frame window rectangle.  In MDI mode, make the frame
 *   window take up most of the desktop window.
 */
inline void w32tdb_set_default_frame_rc(RECT *rc)
{
    /* get the desktop window area, and inset it a bit */
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)rc, 0);
    InflateRect(rc, -16, -16);
}

/*
 *   Get the desktop window area to use for debugger child windows.  For
 *   MDI mode, we'll return the MDI client area.
 */
inline void w32tdb_get_desktop_rc(CTadsWin *frame_win, RECT *rc)
{
    GetClientRect(frame_win->get_parent_of_children(), rc);
}

/*
 *   Set the default stack window position.  In MDI mode, we'll put the
 *   stack window at the top left of the client area.
 */
inline void w32tdb_set_default_stk_rc(RECT *rc)
{
    SetRect(rc, 2, 2, 250, 350);
}

/*
 *   style for main frame window - in MDI mode, make it a normal top-level
 *   window with all of the min/max controls
 */
inline DWORD w32tdb_main_win_style()
{
    return (WS_OVERLAPPED | WS_BORDER | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
            | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SIZEBOX);
}

/*
 *   Bring the debugger window group to the front.  In MDI mode, we simply
 *   bring the main frame to the front, since all other debugger windows
 *   are subwindows in the frame window.
 */
inline void w32tdb_bring_group_to_top(HWND frame_handle)
{
    BringWindowToTop(frame_handle);
}

#else /* W32TDB_MDI */

/*
 *   SDI mode
 */

/* use the standard system interface object for child windows */
inline CTadsSyswin *w32tdb_new_child_syswin(CTadsWin *win)
{
    return new CTadsSyswin(win);
}

/*
 *   Set the default frame window rectangle.  In SDI mode, this window
 *   only contains the main menu and toolbar, so only make it big enough
 *   for the toolbar.
 */
inline void w32tdb_set_default_frame_rc(RECT *rc)
{
    SetRect(rc, 2, 2, 442, 74);
}

/*
 *   Get the desktop window area to use for debugger child windows.  For
 *   SDI mode, we'll return the entire Windows desktop, since the debugger
 *   child windows are all top-level windows.
 */
inline void w32tdb_get_desktop_rc(CTadsWin * /*frame_win*/, RECT *rc)
{
    /* get the desktop window working area */
    SystemParametersInfo(SPI_GETWORKAREA, 0, (void *)rc, 0);
}
/*
 *   Set the default stack window position.  In SDI mode, we'll put the
 *   stack window at the left of the screen, a ways below the main toolbar
 *   window.
 */
inline void w32tdb_set_default_stk_rc(RECT *rc)
{
    SetRect(rc, 2, 120, 250, 350);
}

/*
 *   style for main frame window - in SDI mode, there's no point in
 *   allowing this window to be maximized, so leave off the maximize
 *   button
 */
inline DWORD w32tdb_main_win_style()
{
    return (WS_OVERLAPPED | WS_BORDER | WS_THICKFRAME
            | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
            | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SIZEBOX);
}


/*
 *   Bring the debugger window group to the front.  In SDI mode, we don't
 *   need to do anything here; we'll automatically bring the correct
 *   source window to the top, which will bring the entire window group to
 *   the top, since all debugger windows are part of a common ownership
 *   group.
 */
inline void w32tdb_bring_group_to_top(HWND frame_handle)
{
#if 0 // $$$ no longer necessary - debugger windows are an ownership group now
    CHtmlSys_mainwin *mainwin;
    if ((mainwin = CHtmlSys_mainwin::get_main_win()) != 0
        && GetActiveWindow() == mainwin->get_handle())
        SetWindowPos(mainwin->get_handle(),
                     HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

#endif /* W32TDB_MDI */

/*
 *   Create the main MDI frame/control window
 */
CTadsSyswin *w32tdb_new_frame_syswin(class CHtmlSys_dbgmain *win,
                                     int winmnuidx);

/*
 *   Create the docking port in the MDI frame, if we're running in MDI
 *   mode
 */
class CTadsDockportMDI
   *w32tdb_create_dockport(class CHtmlSys_dbgmain *mainwin,
                           HWND mdi_client);



/* ------------------------------------------------------------------------ */
/*
 *   New project template types
 */
enum w32tdb_tpl_t
{
    W32TDB_TPL_INTRO,                          /* introductory starter game */
    W32TDB_TPL_ADVANCED,                           /* advanced starter game */
    W32TDB_TPL_NOLIB,                       /* non-library project template */
    W32TDB_TPL_CUSTOM                             /* custom source template */
};

/*
 *   New project user interface types
 */
enum w32tdb_ui_t
{
    W32TDB_UI_TRAD,                                       /* traditional UI */
    W32TDB_UI_WEB                                                 /* web UI */
};


/* ------------------------------------------------------------------------ */
/*
 *   private structure for toolbar creation in the rebar
 */
struct tb_create_info;


/* ------------------------------------------------------------------------ */
/*
 *   A saved selection range from a window.
 */
struct HtmlWinRange
{
    HtmlWinRange()
    {
        /* no window or spots yet */
        win = 0;
        a = b = 0;
    }

    ~HtmlWinRange()
    {
        /* forget our window and its spots */
        set_win(0);
    }

    /* set the window - forgets any saved ranged from a previous window */
    void set_win(CHtmlDbgSubWin *w)
    {
        /* if the window isn't changing, there's nothing to do */
        if (win == w)
            return;

        /* if we have spots from a previous window, delete them */
        if (win != 0)
            clear_spots();

        /* remember the new window */
        win = w;
    }

    /* forget the saved range */
    void clear_spots()
    {
        if (win != 0)
        {
            if (a != 0)
            {
                win->delete_spot(a);
                a = 0;
            }
            if (b != 0)
            {
                win->delete_spot(b);
                b = 0;
            }
        }
    }

    /* load with the current range in the given window */
    void load_range(CHtmlDbgSubWin *w)
    {
        /* set the window, and load its range */
        set_win(w);
        load_range();
    }

    /* load with the current range in the current window */
    void load_range()
    {
        /* delete any existing spots */
        clear_spots();

        /* get the current selection range from the current window */
        if (win != 0)
            win->get_sel_range_spots(&a, &b);
    }

    /* get the current range endpoints, as document character positions */
    unsigned long get_a_pos() const
        { return (win != 0 && a != 0 ? win->get_spot_pos(a) : ~0U); }
    unsigned long get_b_pos() const
        { return (win != 0 && b != 0 ? win->get_spot_pos(b) : ~0U); }

    /* do we match the current range in the given window? */
    int is_current(CHtmlDbgSubWin *w)
    {
        /* if the window doesn't match, we definitely don't match */
        if (win != w)
            return FALSE;

        /* we match if our range matches the current range */
        return is_current();
    }

    /* do we match the current range in the current window? */
    int is_current()
    {
        class CHtmlFormatter *fmt;
        unsigned long ca, cb;

        /* if there's no window or range loaded, there's nothing to match */
        if (win == 0 || a == 0 || b == 0)
            return FALSE;

        /* get the current selection range */
        if (!win->get_sel_range(&fmt, &ca, &cb))
            return FALSE;

        /* check for a match */
        return (win->get_spot_pos(a) == ca && win->get_spot_pos(b) == cb);
    }

    /* the window containing the position */
    CHtmlDbgSubWin *win;

    /* the spot handles */
    void *a;
    void *b;
};


/* ------------------------------------------------------------------------ */
/*
 *   Status line source for the cursor line/column panel
 */
class CTadsStatusSourceCursorPos: public CTadsStatusSource
{
public:
    CTadsStatusSourceCursorPos()
    {
        txt_[0] = '\0';
    }

    virtual textchar_t *get_status_message(int *caller_deletes)
        { return txt_; }

    /* set the text */
    void set(const char *txt) { safe_strcpy(txt_, sizeof(txt_), txt); }

    /* our buffer */
    char txt_[50];
};

/*
 *   Status line source for the progress bar panel
 */
class CTadsStatusSourceProgress: public CTadsStatusSource
{
public:
    CTadsStatusSourceProgress()
    {
        pbar_ = 0;
        running_ = FALSE;
        pct_done_ = 0;
    }

    /*
     *   initialize the window - the parent window calls this during its own
     *   creation to let us create our progress bar control
     */
    void init_win(HWND parent);

    /*
     *   update the progress status; returns true if the visibility is
     *   changing, false if not
     */
    int set_progress(int running, int pct_done)
    {
        /* if the running status is changing, show or hide the bar */
        int ret = (running != running_);

        /* note the new statistics */
        running_ = running;
        pct_done_ = pct_done;

        /* update the bar position */
        SendMessage(pbar_, PBM_SETPOS, pct_done, 0);

        /* return the visibility change flag */
        return ret;
    }

    /* on layout changs, move the progress bar */
    virtual void status_change_pos(const RECT *rc);

    /* we do our own drawing via the progress bar control */
    virtual textchar_t *get_status_message(int *caller_deletes)
        { return OWNER_DRAW; }

    /*
     *   we don't need to do any actual drawing, as the progress bar control
     *   handles that for us
     */
    virtual int status_owner_draw(DRAWITEMSTRUCT *) { return TRUE; }

    /* Windows progress bar object */
    HWND pbar_;

    /*
     *   Progress statistics.  This is used to set the status display in the
     *   control.  'running_' indicates whether a process is running at all;
     *   'pct_done_' contains the percentage completion.
     */
    int running_;
    int pct_done_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger main control window.  This is a top-level frame window that
 *   we create to hold the debugger menu and toolbar, and to control
 *   operations while the debugger is active.
 */

/* private message: refresh source windows */
static const int HTMLM_REFRESH_SOURCES = HTMLM_LAST + 1;

/*
 *   private message: build done (posted from compiler thread to the main
 *   window to tell it the compilation is finished)
 */
static const int HTMLM_BUILD_DONE = HTMLM_LAST + 2;

/* private message: save window position in preferences after move/size */
static const int HTMLM_SAVE_WIN_POS = HTMLM_LAST + 3;

/* private message: update the MDI maximized status after a child resize */
static const int HTMLM_UPDATE_MDI_STATUS = HTMLM_LAST + 4;

/* private message: show a message box */
static const int HTMLM_MSGBOX = HTMLM_LAST + 5;

/* private message: game execution starting */
static const int HTMLM_GAME_STARTING = HTMLM_LAST + 6;


class CHtmlSys_dbgmain:
   public CHtmlSys_framewin, public CHtmlDebugSysIfc_win,
   public CHtmlSysWin_win32_owner_null, public CHtmlSys_dbglogwin_hostifc,
   public CTadsStatusSource, public CHtmlSysWinGroupDbg,
   public ITadsWorkbench, public ITadsFindDialogOwner
{
    friend class twb_editor_mode_iter;
    friend class twb_command_iter;
    friend class PublishDialogArgs;

public:
    CHtmlSys_dbgmain(struct dbgcxdef *ctx, const char *argv_last);
    ~CHtmlSys_dbgmain();

    /* get the application-wide debugger main window object */
    static CHtmlSys_dbgmain *get_main_win();

    /* activate or deactivate */
    int do_activate(int flag, int minimized, HWND other_win);

    /*
     *   perform additional initialization required after the .GAM file
     *   has been loaded
     */
    void post_load_init(struct dbgcxdef *ctx);

    /* get the reload execution status */
    int get_reloading_exec() const { return reloading_exec_ != 0; }

    /* load the global configuration */
    static CHtmlDebugConfig *load_global_config(CStringBuf *fname);

    /* save the global configuration */
    static void save_global_config(CHtmlDebugConfig *gc, CStringBuf *fname);

    /* get the name of the loaded game */
    const textchar_t *get_gamefile_name() const
        { return game_filename_.get(); }

    /* get the local configuration filename */
    const textchar_t *get_local_config_filename() const
        { return local_config_filename_.get(); }

    /*
     *   Debugger event loop.  This is called whenever we enter the
     *   debugger.  We retain control and handle events until the user
     *   resumes execution of the game.  Returns true if execution should
     *   continue, false if the application was terminated from within the
     *   event loop.
     *
     *   If *restart is set to true on return, the caller should signal a
     *   TADS restart condition to cause the game to start over from the
     *   beginning.  If *abort is set to true on return, the caller should
     *   signal a TADS abort-command condition to cause the game to abort
     *   the current command and start a new command.
     *
     *   If *terminate is set to true on return, the caller should signal
     *   a TADS quit-game condition to cause the game to terminate.
     *
     *   If *reload is set to true on return, the caller should signal
     *   TADS to quit, and exit to the outer loader loop.  The outer
     *   loader loop will re-start TADS with a new game.
     */
    int dbg_event_loop(struct dbgcxdef *ctx, int bphit, int err,
                       int *restart, int *reload,
                       int *terminate, int *abort,
                       void *exec_ofs);

    /*
     *   Terminate the debugger.  Closes all debugger windows.
     */
    void terminate_debugger();

    /* run the wait-for-build dialog to wait for the build to finish */
    void run_wait_for_build_dlg();

    /* load the configuration for the given game */
    int load_config(const textchar_t *game_file, int game_loaded);

    /* save our configuration */
    void save_config();

    /* save a tool window's configuration */
    void save_toolwin_config(IDebugWin *win, const textchar_t *prefix);

    /*
     *   get my menu - we use a custom toolbar to implement the menu so that
     *   it can be part of the rebar, so GetMenu() won't work, so we have to
     *   override this to return our menu handle
     */
    virtual HMENU get_win_menu() { return main_menu_; }

    /* update the recent-game list in the file menu */
    void update_recent_game_menu();

    /* add a game to the list of recent games in the file menu */
    void add_recent_game(const char *new_game);

    /* save the internal source directory list to the configuration */
    void srcdir_list_to_config();

    /* rebuild the internal source directory list from the configuration */
    void rebuild_srcdir_list();

    /* receive notification that TADS is shutting down */
    void notify_tads_shutdown();

    /* get the icon menu handler */
    class IconMenuHandler *get_icon_menu() const { return iconmenu_; }

    /* initialize a popup menu that's about to be displayed */
    void init_menu_popup(HMENU menuhdl, unsigned int pos, int sysmenu);

    /* receive notification that we've closed a menu */
    int menu_close(unsigned int item);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* process notification */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* handle a right-click on the title bar */
    int do_nc_rightbtn_down(int keys, int x, int y, int clicks, int hit_type);

    /* handle a WM_MENUCHAR notification */
    int do_menuchar(TCHAR ch, unsigned int menu_type, HMENU menu,
                    LRESULT *ret);

    /* handle a system menu key (WM_SYSCOMMAND of type SC_KEYMENU) */
    int do_syskeymenu(TCHAR ch);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* do owner drawing of controls */
    int do_draw_item(int ctl_id, DRAWITEMSTRUCT *di);

    /* check a command status */
    TadsCmdStat_t check_command(const check_cmd_info *);

    /* get my statusline object */
    class CTadsStatusline *get_statusline() const { return statusline_; }

    /*
     *   CHtmlDebugSysIfc_win implementation.  This is an interface that
     *   allows the portable helper code to call back into the
     *   system-dependent user interface code to create new windows.
     */
    virtual class IDebugWin
        *dbgsys_create_win(class IDebugSrcMgr **srcmgr,
                           const textchar_t *win_title,
                           const textchar_t *full_path,
                           HtmlDbg_wintype_t win_type);

    /*
     *   get the source font description - this returns the current style
     *   settings, but always returns the default background color, since we
     *   use transparent text by default
     */
    virtual void dbgsys_set_srcfont(class CHtmlFontDesc *font_desc);

    /* compare filenames */
    virtual int dbgsys_fnames_eq(const char *fname1, const char *fname2);

    /* compare a filename to a path */
    virtual int dbgsys_fname_eq_path(const char *path, const char *fname);

    /* find a source file */
    virtual int dbgsys_find_src(const char *origname, int origlen,
                                char *fullname, size_t full_len,
                                int must_find_file);

    /*
     *   CHtmlSys_win32_owner implementation.  We only override a few
     *   methods, since we implement most of this interface with the null
     *   version.
     */
    virtual void bring_to_front();

    /* set a timer to bring the game window to the front */
    void sched_game_to_front();

    /* get/set the TADS debugger context */
    struct dbgcxdef *get_dbg_ctx() const { return dbgctx_; }
    void set_dbg_ctx(struct dbgcxdef *ctx) { dbgctx_ = ctx; }

    /* get the pointer to the current execution offset in the debugger */
    void *get_exec_ofs_ptr() const { return exec_ofs_ptr_; }

    /* get the debugger helper object */
    class CHtmlDebugHelper *get_helper() const { return helper_; }

    /* get the configuration objects */
    class CHtmlDebugConfig *get_local_config() const
        { return local_config_; }
    class CHtmlDebugConfig *get_global_config() const
        { return global_config_; }

    /* get an integer variable value from the global configuration data */
    int get_global_config_int(const char *varname, const char *subvar,
                              int default_val) const;

    /* get the active debugger window for command routing */
    class CHtmlDbgSubWin *get_active_dbg_win();

    /* get the most recent active debugger window */
    class CHtmlDbgSubWin *get_last_active_dbg_win() const
        { return last_active_dbg_win_; }

    /*
     *   Get the last command target window.  This is the active debug
     *   window, or if there's no active debug window, the last command
     *   target window.  The command target window remains active even if
     *   focus or activation leaves the application.
     */
    class CHtmlDbgSubWin *get_last_cmd_target_win()
    {
        class CHtmlDbgSubWin *win;

        /* if there's an active window, return it */
        if ((win = get_active_dbg_win()) != 0)
            return win;

        /* otherwise, return the last command target window */
        return last_cmd_target_win_;
    }

    /*
     *   notify me of a change in active debugger window status; a debugger
     *   window should call this whenever it's activated or deactivated
     */
    void notify_active_dbg_win(class CHtmlDbgSubWin *win, int active)
    {
        /* activate or deactivate, as appropriate */
        if (active)
            set_active_dbg_win(win);
        else
            clear_active_dbg_win();
    }

    /*
     *   Set the active debugger window.  Source, stack, and other
     *   debugger windows should set themselves active when they are
     *   activated, and should clear the active window when they're
     *   deactivated.
     */
    void set_active_dbg_win(class CHtmlDbgSubWin *win);
    void clear_active_dbg_win();

    /*
     *   Update the cursor position to the given line/column position, to an
     *   empty display, or to a given text string.
     */
    void update_cursorpos(int lin, int col);
    void update_cursorpos() { update_cursorpos(""); }
    void update_cursorpos(const char *txt);

    /*
     *   Set a "null" active debug window.  This differs from
     *   clear_active_dbg_win() in that this explicitly forgets the last
     *   active window.  This should be used when a window becomes active,
     *   and that window behaves like an active window for the purposes of
     *   command routing and accelerators, but doesn't actually implement the
     *   CHtmlDbgSubWin interface.  We thus remove any trace of another
     *   active debug window.
     */
    void set_null_active_dbg_win();

    /* get the search box control */
    HWND get_searchbox() const { return searchbox_; }

    /*
     *   Find a tool window by window type (HTMLDBG_WINTYPE_xxx).  If
     *   'create' is true, we'll create the window if it doesn't already
     *   exist.  This returns the frame window handle.
     */
    HWND find_tool_window(int win_type, int create);

    /*
     *   Receive notification that a debugger window is closing.  If I'm
     *   keeping track of the window as a background window for command
     *   routing, I'll forget about it now.
     */
    void on_close_dbg_win(class CHtmlDbgSubWin *win);

    /* determine if the debugger has control */
    int get_in_debugger() const { return in_debugger_; }

    /*
     *   Start a build.  This checks that the selected build type is allowed
     *   right now and that we're configured for it, prepares the debug log
     *   window for the build output, then invokes the version-dependent
     *   build process.
     */
    int start_build(int command_id, const TdbBuildTargets *t = 0);

    /*
     *   run the compiler; returns true if the build successfully started,
     *   false if not
     */
    int run_compiler(int command_id, const TdbBuildTargets &t);

    /* static entrypoint for compiler thread */
    static DWORD WINAPI run_compiler_entry(void *ctx);

    /* member function entrypoint for compiler thread */
    void run_compiler_th();

    /* run the publishing wizard */
    void do_publish();

    /* run a command line tool */
    int run_command(const textchar_t *exe_printable_name,
                    const textchar_t *exe_file, const textchar_t *cmdline,
                    const textchar_t *cmd_dir);

    /*
     *   Run a command - returns but does not display the error code.  The
     *   return value indicates the system result: zero indicates success,
     *   and non-zero indicates failure.  If the return value is zero
     *   (success), (*process_result) will contain the result code from
     *   the process itself; the interpretation of of the process result
     *   code is dependent upon the program invoked, but generally zero
     *   indicates success and non-zero indicates failure.
     */
    int run_command_sub(const textchar_t *exe_printable_name,
                        const textchar_t *exe_file, const textchar_t *cmdline,
                        const textchar_t *cmd_dir, DWORD *process_result);

    /*
     *   Load a new game, or reload the current game.  The caller must
     *   return from the debugger event loop after invoking this in order
     *   to force the main loop to start over with the new game.
     */
    void load_game(const textchar_t *fname);

    /* ask for a game to load */
    static int ask_load_game(
        HWND dlg_parent, textchar_t *fname, size_t fname_len);

    /*
     *   Load a recent game, given the ID_GAME_RECENT_n command ID of the
     *   game to load.  Returns true if successful, false if there is no such
     *   recent game to be loaded.
     */
    int load_recent_game(int command_id);

    /*
     *   Create and load a new game - old interface.  We can optionally
     *   create the source file, by copying a templatefile to the given new
     *   source file.  We'll create a configuration for the new game based on
     *   the current configuration as a default, add build settings for the
     *   given source file and game file, then load the new game and kick off
     *   a compile.
     *
     *   'custom_template' is the filename of a source file to use if the
     *   template type is W32TDB_TPL_CUSTOM.
     *
     *   This is the old interface, now used only by TADS 2 Workbench.
     */
    void create_new_game(const textchar_t *srcfile,
                         const textchar_t *gamefile, int create_source,
                         w32tdb_tpl_t template_type, w32tdb_ui_t ui_type,
                         const textchar_t *custom_template,
                         const class CNameValTab *biblio);

    /*
     *   Create and load a new game - new interface.  This version is used by
     *   the updated project wizard for TADS 3.  This version can only create
     *   new projects from scratch; TADS 3 doesn't need the variation in the
     *   old interface that allowed creating a project file given a source
     *   file, since in TADS 3 we assume that every project has a project
     *   file out of the gate.
     *
     *   'tpl' is the parsed project starter template file, as a list of
     *   name/value pairs.  'tpldir' is the absolute path to the template
     *   folder; 'tplrel' is the template folder path relative to the system
     *   library path (e.g., the 'extensions' folder, or one of the folders
     *   in the 'srcdir' system library list).
     */
    void create_new_game(
        const textchar_t *path, const textchar_t *fname,
        const char *tpldir, const char *tplrel, const CNameValTab *tpl,
        const class CNameValTab *biblio);

    /*
     *   Add a file to the t3 build configuration.  'domain' is "user" for a
     *   user file, "system" for a system file.
     */
    void add_file_to_build(int idx, const char *path, const char *domain);

    /* copy a file, fixing up $xxx$ fields found in the text */
    int copy_insert_fields(const char *srcfile, const char *dstfile,
                           const class CNameValTab *biblio);

    /* get or create the various tool windows we directly manage */
    class CHtmlDbgProjWin *create_proj_win(const RECT *deskrc);
    class CHtmlDbgWatchWin *create_watch_win(const RECT *deskrc);
    class CHtmlDbgWatchWin *create_locals_win(const RECT *deskrc);
    class CHtmlDbgScriptWin *create_script_win(const RECT *deskrc);

    /* update expression windows */
    void update_expr_windows();

    /*
     *   Add a new watch expression.  This will add the new expression at
     *   the end of the currently selected panel of the watch window.
     */
    void add_watch_expr(const textchar_t *expr);

    /*
     *   Get the text for a Find command.  If the command is "Find", we'll
     *   run the dialog to get text from the user.  If the command is
     *   "Find Next," we'll simply return the text from the last Find
     *   command, if there was one; if not, we'll run the dialog as though
     *   this were a "Find" command.  If we run the dialog and the user
     *   cancels, we'll return a null pointer.
     */
    const textchar_t *get_find_text(int command_id, int *exact_case,
                                    int *start_at_top, int *wrap, int *dir)
    {
        return get_find_text_ex(command_id, exact_case, start_at_top,
                                wrap, dir, 0, 0, 0);
    }

    /*
     *   Extended Get Find Text - this version allows entering a regular
     *   expression search.  If 'regex' is null, we'll run the normal search
     *   dialog without the regular expression option.
     */
    const textchar_t *get_find_text_ex(int command_id, int *exact_case,
                                       int *start_at_top, int *wrap,
                                       int *dir, int *regex,
                                       int *whole_word,
                                       itfdo_scope *scope);

    /*
     *   Get the last Find text and options.  This returns the next search
     *   for a Find Next command.
     */
    const textchar_t *get_find_next(
        int *exact_case, int *start_at_top, int *wrap, int *dir, int *regex);

    /* open the Find dialog */
    HWND open_find_dlg();

    /* open the Replace dialog */
    HWND open_replace_dlg();

    /*
     *   Receive notification that source window preferences have changed.
     *   We'll update the source windows accordingly.
     */
    void on_update_srcwin_options();

    /*
     *   Resume execution.  This is provided as a public method so that
     *   subwindows can tell the main window to start running.
     */
    void exec_go();

    /*
     *   Resume execution for a single step.
     */
    void exec_step_into();

    /*
     *   Generate a name for a new automatic script file.  hoc scripts.
     *   Every time we run in the tads 3 version, we automatically generate a
     *   script, to allow easy re-running of a past session.
     */
    int name_auto_script(textchar_t *fname, size_t fname_size);

    /* add a script file to the list in the UI */
    void add_file_to_script_win(const textchar_t *fname);

    /* remove a script file from the list in the UI */
    void remove_file_from_script_win(const textchar_t *fname);

    /* refresh the script window, for a change to the script directory */
    void refresh_script_win(int clear_first);

    /* prune the script directory, if the option settings so dictate */
    void prune_script_dir();

    /* get the replay script information */
    const textchar_t *get_replay_script() const
        { return replay_script_.get(); }
    int is_replay_script_temp() const
        { return is_replay_script_temp_; }

    /* is there a replay script active? */
    int is_replay_script_set() const
        { return replay_script_.get() != 0 && *replay_script_.get() != '\0'; }

    /* clear the replay script */
    void clear_replay_script()
        { replay_script_.set((const textchar_t *)0); }

    /*
     *   Replay a script.  If a run is already in progress, we'll terminate
     *   it; then, we'll start a new run using the given script as the
     *   command input file.
     */
    void run_script(const textchar_t *fname, int is_temp_file);

    /*
     *   Determine if we have a game workspace open.  If we have yet to
     *   open a game, we have no workspace.  Note that we can have a
     *   workspace open even when the associated game isn't loaded - this
     *   allows the user to modify a game's configuration (breakpoints,
     *   window layout, etc) even while the game isn't running.
     */
    int get_workspace_open() const { return workspace_open_; }
    void set_workspace_open(int flag) { workspace_open_ = flag; }

    /*
     *   Get/Set the "game loaded" flag.  This flag is true when the game
     *   is loaded and running.  After the game terminates and the
     *   debugger context is no longer valid, this is false.
     */
    int get_game_loaded() const { return game_loaded_; }
    void set_game_loaded(int flag);

    /*
     *   get the "quitting" flag - if this is true, the debugger is in the
     *   process of terminating in response to an explicit user command to
     *   quit
     */
    int is_quitting() const { return quitting_; }

    /* get the "reloading" flag */
    int is_reloading() const { return reloading_; }

    /* get the "terminating game" flag */
    int is_terminating() const { return term_in_progress_; }

    /* schedule an unconditional reload of all libraries */
    void schedule_lib_reload();

    /*
     *   Build the filter string for a GetOpen|SaveFileName dialog for a text
     *   editor file.  This constructs a filter string that includes the
     *   custom filter strings for all of the extension modes.
     */
    void build_text_file_filter(CStringBuf *buf, int for_save);

    /* handle a file drag-and-drop from Windows Explorer */
    void drop_shell_file(const char *fname);

    /* open a source or other text file in its own MDI child window */
    class IDebugSysWin *open_text_file(const textchar_t *fname);

    /* is the given file open in an editor window? */
    int is_file_open(const textchar_t *fname);

    /*
     *   Is the given file local to the project?  This returns true if the
     *   file is in the same directory as the project file or in a directory
     *   that's local to the project.
     */
    int is_local_project_file(const textchar_t *fname);

    /*
     *   Is the given file in the given directory?  If the file is given as a
     *   relative path, it's assumed to be relative to the project folder.
     */
    int is_file_in_dir(const textchar_t *fname, const textchar_t *dir);

    /* get the project directory */
    void get_project_dir(textchar_t *buf, size_t buflen);

    /*
     *   Receive notification that a file has been renamed.  We'll look for
     *   any open window on the file, and update the window's title.
     */
    void file_renamed(const textchar_t *old_name, const textchar_t *new_name);

    /* create a new text file */
    void new_text_file()
        { new_text_file(0, 0, 0); }
    void new_text_file(const textchar_t *initial_text)
        { new_text_file(0, 0, initial_text); }
    void new_text_file(int mode_cmd, const textchar_t *defext,
                       const textchar_t *initial_text);

    /* receive notification of a change to a child window title */
    void update_win_title(class CTadsWin *win);

    /* get the current active MDI child */
    HWND get_active_mdi_child() const
    {
        return (HWND)SendMessage(get_parent_of_children(),
                                 WM_MDIGETACTIVE, 0, 0);
    }

    /* maintain the MDI tab control on child window creation/deletion */
    virtual void mdi_child_event(CTadsWin *win, UINT msg,
                                 WPARAM wpar, LPARAM lpar);

    /*
     *   Open a file in the external text editor.  Returns true on
     *   success, false on failure.
     *
     *   If 'local_if_no_linenum' is true, and the configured text editor
     *   doesn't have a line number setting in its configuration, we won't
     *   attempt to open the file in an external text editor at all, but
     *   will instead open the file in the local source viewer.  This can
     *   be used if it's more important that the actual line be shown,
     *   rather than that the file be opened in the external text editor;
     *   we use this to choose the priority when a choice must be made.
     *   When the editor is configured with a "%n" in its command line, we
     *   can have both, so we'll always open the file in the editor in
     *   this case.
     */
    int open_in_text_editor(const textchar_t *fname,
                            long linenum, long colnum,
                            int local_if_no_linenum);

    /*
     *   Note that a file has been saved from the text editor.  This will
     *   check to see if the file is part of the project, and if so, it'll
     *   set the maybe_need_build_ flag to indicate that we've saved changes
     *   to project files since the last build.
     */
    void note_file_save(const textchar_t *fname);

    /* notify a window of formatting option changes */
    void note_formatting_changes(class IDebugSysWin *win);

    /*
     *   Get the global style settings.  'desc' is the base font plus the
     *   current window background color; 'sel_desc' gives the colors for
     *   selected text (other fields are ignored for sel_desc - only the
     *   colors are meaningful here).
     */
    void get_global_style(CHtmlFontDesc *desc, CHtmlFontDesc *sel_desc);

    /* save the given style as the new global style */
    void set_global_style(const CHtmlFontDesc *desc,
                          const CHtmlFontDesc *sel_desc);

    /* get settings for the syntax styles for an editor mode */
    void get_syntax_style_info(ITadsWorkbenchEditorMode *mode,
                               int *syntax_coloring_enabled,
                               CHtmlFontDesc *default_style,
                               CHtmlFontDesc *styles, int style_cnt);

    /* save settings for the syntax styles for an editor mode */
    void save_syntax_style_info(ITadsWorkbenchEditorMode *mode,
                                int syntax_coloring_enabled,
                                const CHtmlFontDesc *default_style,
                                const CHtmlFontDesc *styles, int style_cnt);

    /* get the project window, if any */
    class CHtmlDbgProjWin *get_proj_win() const { return projwin_; }

    /* get the script window, if any */
    class CHtmlDbgScriptWin *get_script_win() const { return scriptwin_; }

    /* navigate to the given help URL */
    void show_help(const textchar_t *url);

    /* get the name of the main workbench help file */
    void get_main_help_file_name(textchar_t *buf, size_t buflen);

    /* update the game file name for a change in the preferences */
    void vsn_change_pref_gamefile_name(textchar_t *fname);

    /* get/set the search toolbar's search button's mode */
    int get_searchbar_mode();
    void set_searchbar_mode(int idx);

    /* get the search toolbar's current command setting */
    int get_searchbar_cmd() { return searchbarcmd_; }

    /* set a long-running process status message */
    void set_lrp_status(const textchar_t *str);

    /*
     *   Update the build progress bar in the status line with the current
     *   values of the build progress variables (stat_build_step_, etc).
     *   'running' is true if the build is currently running, false if not.
     */
    void update_build_progress_bar(int running);

    /* add the doc toolbar buttons relevant to this version */
    int vsn_add_doc_buttons(TBBUTTON *buttons, int i);

    /*
     *   CHtmlSys_dbglogwin_hostifc implementation
     */

    /* add a reference */
    virtual void dbghostifc_add_ref() { AddRef(); }

    /* remember a reference */
    virtual void dbghostifc_remove_ref() { Release(); }

    /* open a source file at a given line number */
    virtual void dbghostifc_open_source(
        const textchar_t *fname, long linenum, long colnum, int select_line);

    /* receive notification that the window is being destroyed */
    virtual void dbghostifc_on_destroy(class CHtmlDbgSubWin *subwin,
                                       class IDebugSysWin *syswin);

    /* receive notification of child window activation change */
    virtual void dbghostifc_notify_active_dbg_win(
        class CHtmlDbgSubWin *win, int active)
        { notify_active_dbg_win(win, active); }

    /* get find text */
    const textchar_t *dbghostifc_get_find_text(
        int command_id, int *exact_case, int *start_at_top,
        int *wrap, int *dir)
    {
        /* use our normal FIND dialog */
        return get_find_text(command_id, exact_case, start_at_top, wrap, dir);
    }

    /* show/hide the status line */
    void show_statusline(int show);

    /*
     *   CTadsStatusSource implementation
     */
    virtual textchar_t *get_status_message(int *caller_deletes);
    virtual int do_status_menuselect(WPARAM wpar, LPARAM lpar);

    /*
     *   ITadsFindDialogOwner implementation
     */
    virtual void itfdo_activate() { set_global_key_bindings(); }
    virtual int itfdo_allow_find(int regex);
    virtual int itfdo_allow_replace();
    virtual void itfdo_new_search();
    virtual int itfdo_find_text(
        const textchar_t *txt,
        int exact_case, int wrap, int direction, int regex, int whole_word,
        itfdo_scope scope);
    virtual int itfdo_replace_sel(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex, int whole_word,
        itfdo_scope scope);
    virtual int itfdo_replace_all(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex, int whole_word,
        itfdo_scope scope);
    virtual void itfdo_save_pos(const CHtmlRect *rc);
    virtual int itfdo_load_pos(CHtmlRect *rc);

    /* general find-and-replace handler */
    int find_and_replace(
        const textchar_t *txt, const textchar_t *repl, int repl_all,
        int exact_case, int wrap, int direction, int regex, int whole_word,
        itfdo_scope scope, IDebugSysWin **found_win);

    /* load the search start position for a given window */
    void get_find_start_for_win(class CHtmlDbgSubWin *win,
                                unsigned long *a, unsigned long *b)
    {
        if (find_start_.win == win)
        {
            *a = find_start_.get_a_pos();
            *b = find_start_.get_b_pos();
        }
        else
            *a = *b = 0;
    }

    /* IDropTarget implementation */
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject __RPC_FAR *pDataObj,
                                        DWORD grfKeyState, POINTL pt,
                                        DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                       DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragLeave();
    HRESULT STDMETHODCALLTYPE Drop(IDataObject __RPC_FAR *pDataObj,
                                   DWORD grfKeyState, POINTL pt,
                                   DWORD __RPC_FAR *pdwEffect);

    /*
     *   ITadsWorkbench implementation.  We inherit our IUnknown interface
     *   from our CTadsWin base class, but we still need to define the
     *   methods here explicitly to avoid ambiguity.
     */
    ULONG STDMETHODCALLTYPE AddRef() { return CHtmlSys_framewin::AddRef(); }
    ULONG STDMETHODCALLTYPE Release() { return CHtmlSys_framewin::Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc);

    BSTR STDMETHODCALLTYPE GetPropValStr(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey, UINT idx);
    UINT STDMETHODCALLTYPE GetPropValStrCount(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey);
    INT STDMETHODCALLTYPE GetPropValInt(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey,
        INT default_val);
    void STDMETHODCALLTYPE ClearPropValStr(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey);
    void STDMETHODCALLTYPE SetPropValStr(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey, UINT idx,
        const OLECHAR *val);
    void STDMETHODCALLTYPE SetPropValInt(
        BOOL global, const OLECHAR *key, const OLECHAR *subkey, INT val);
    INT STDMETHODCALLTYPE ReMatchA(const char *txt, UINT txtlen,
                                   const char *pat, UINT patlen);
    INT STDMETHODCALLTYPE ReMatchW(const OLECHAR *txt, UINT txtlen,
                                   const OLECHAR *pat, UINT patlen);
    INT STDMETHODCALLTYPE ReSearchA(const char *txt, UINT txtlen,
                                    const char *pat, UINT patlen,
                                    twb_regex_match *match);
    INT STDMETHODCALLTYPE ReSearchW(const OLECHAR *txt, UINT txtlen,
                                    const OLECHAR *pat, UINT patlen,
                                    twb_regex_match *match);

    UINT STDMETHODCALLTYPE GetCommandID(const OLECHAR *command_name);
    UINT STDMETHODCALLTYPE DefineCommand(const OLECHAR *name,
                                         const OLECHAR *desc);

    /* enumeration callback: close windows with the given file path */
    static void enum_win_close_path_cb(void *ctx, int idx, IDebugWin *win,
                                       int srcid, HtmlDbg_wintype_t typ);

    /* reload the keyboard accelerator table from the configuration */
    void load_keys_from_config();

    /* look up a command by name */
    const HtmlDbgCommandInfo *find_command(
        const textchar_t *name, size_t namelen, int add_if_undef);
    const HtmlDbgCommandInfo *find_command(
        const textchar_t *name, int add_if_undef)
        { return find_command(name, get_strlen(name), add_if_undef); }

    /* find a key table by name */
    HtmlKeyTable *get_key_table(const textchar_t *name);

    /* define a new extension command */
    HtmlDbgCommandInfoExt *define_ext_command(
        const textchar_t *sym, size_t symlen,
        const textchar_t *desc, size_t desclen);

    /* rebuild the Tools menu with the current list of external tools */
    void rebuild_tools_menu();

    /* execute an external tool command */
    void execute_extern_tool(int cmd);

    /* get the current CTadsAccelerator object */
    class CTadsAccelerator *get_taccelerator() const { return taccel_; }

    /* name-keyed and ID-keyed hash table to the debugger command list */
    static class CHtmlHashTable *cmdname_hash;
    static class CHtmlHashTable *command_hash;

    /* the list of key tables */
    static HtmlKeyTable keytabs_[];

    /* print setup options */
    HGLOBAL print_devmode_;
    HGLOBAL print_devnames_;
    RECT print_margins_;
    int print_margins_set_;
    DWORD print_psd_flags_;

    /* discard bookmark tracking objects */
    void delete_bookmarks();

    /* add a bookmark */
    void add_bookmark(const textchar_t *fname, int handle,
                      int linenum, int name);

    /* delete a bookmark */
    void delete_bookmark(struct tdb_bookmark *b);

    /*
     *   jump to the first bookmark in the next file after the given file, or
     *   the last bookmark in the file before the given file
     */
    void jump_to_next_bookmark(const textchar_t *fname, int dir);

    /* jump to the given bookmark */
    void jump_to_bookmark(struct tdb_bookmark *b);

    /* ask for a bookmark name, then jump to that bookmark */
    void jump_to_named_bookmark();

    /* pop a bookmark */
    void pop_bookmark();

    /* ask for a bookmark name */
    int ask_bookmark_name();

    /* the list of bookmarks */
    struct tdb_bookmark *bookmarks_;

    /* the list of files containing bookmarks */
    struct tdb_bookmark_file *bookmark_files_;

protected:
    /* load extensions DLLs */
    void load_extensions();

    /* save the toolbar configuration */
    void save_toolbar_config(CHtmlDebugConfig *config);

    /* load the toolbar configuration */
    void load_toolbar_config(CHtmlDebugConfig *config);

    /* save/load the searchbox history */
    void save_searchbox_history(CHtmlDebugConfig *config);
    void load_searchbox_history(CHtmlDebugConfig *config);

    /* searchbar combo box edit field subclass window proc */
    static LRESULT CALLBACK searchbox_winproc(
        HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar);

    /* read/write an individual syntax style entry */
    void read_syntax_style(
        ITadsWorkbenchEditorMode *mode,
        const textchar_t *mode_key, const textchar_t *subkey, int idx,
        CHtmlFontDesc *stylep);
    void write_syntax_style(
        ITadsWorkbenchEditorMode *mode,
        const textchar_t *mode_key, const textchar_t *subkey, int idx,
        const CHtmlFontDesc *stylep);

    /* load the default key bindings */
    void load_default_keys();

    /* load a key table from the configuration */
    void load_key_table(class CTadsAccelerator *accel, HtmlKeyTable *tab);

    /* update the key bindings for a change in the active window */
    void update_key_bindings(int activating);

    /* set the global key bindings */
    void set_global_key_bindings();

    /*
     *   close all of the tool windows, in preparation for terminating or for
     *   loading a new configuration
     */
    void close_tool_windows();

    /* save the docking configuration */
    void save_dockwin_config(class CHtmlDebugConfig *config);

    /* load the docking configuration */
    void load_dockwin_config(class CHtmlDebugConfig *config);

    /* load the docking configuration for a given window */
    void load_dockwin_config_win(class CHtmlDebugConfig *config,
                                 const char *guid,
                                 class CTadsWin *subwin,
                                 const char *win_title,
                                 int visible,
                                 enum tads_dock_align default_dock_align,
                                 int default_docked_width,
                                 int default_docked_height,
                                 const RECT *default_undocked_pos,
                                 int default_is_docked,
                                 int default_is_mdi);

    /*
     *   Assign a docking window its GUID based on the loaded serial number.
     *   This is used for reverse compatibility, to convert a file from the
     *   old "dock id" serial numbering scheme to the new GUID scheme, where
     *   we store a global identifier for each docking window in the docking
     *   model itself.
     */
    void assign_dockwin_guid(const char *guid);

    /* show/hide tool windows for a design/debug mode switch */
    void tool_window_mode_switch(int debug);

    /* window style - make it a document window without a maximize box */
    DWORD get_winstyle() { return w32tdb_main_win_style(); }

    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* close the window */
    int do_close();

    /* erase the background */
    int do_erase_bkg(HDC hdc);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* resize the MDI client window */
    void resize_mdi_client();

    /* add all of the toolbars to the rebar */
    void add_rebar_toolbars(tb_create_info *info, int cnt);

    /* is the given toolbar (xxx_TOOLBAR_ID) visible? */
    int is_toolbar_visible(int id);

    /* show or hide the given toolbar (xxx_TOOLBAR_ID) */
    void set_toolbar_visible(int id, int vis);

    /* ask the user if they want to quit - returns true if so */
    int ask_quit();

    /* store a position in the appropriate preferences entry */
    virtual void set_winpos_prefs(const class CHtmlRect *pos);

    /*
     *   if we're currently waiting for the user to enter a command in the
     *   game, abort the command entry and force entry into the debugger
     */
    void abort_game_command_entry();

    /*
     *   Terminate the game.  This can be used in the main debugger event
     *   loop to set the necessary state so that the game is terminated as
     *   soon as the debugger event loop returns.  This doesn't have
     *   immediate effect; the caller must return from the debugger event
     *   loop before the game will actually be terminated.  In the
     *   do_command() routine or another message handler, the caller can
     *   simply return after calling this, and the debugger event loop
     *   will terminate.
     */
    void terminate_current_game()
    {
        /* set the game-termination flag */
        terminating_ = TRUE;
        term_in_progress_ = TRUE;

        /* set the "go" flag so that we leave our event loop */
        go_ = TRUE;

        /* wrest control from the game if a command is being read */
        abort_game_command_entry();
    }

    /* clear non-default configuration information */
    void clear_non_default_config();

    /*
     *   build service routine - get the name of an external resource
     *   file, and determine if it should be included in the build;
     *   returns true if so, false if not
     */
    int build_get_extres_file(char *buf, int cur_extres, int build_command,
                              const char *extres_base_name);

    /* delete a temporary directory and all of its contents */
    int del_temp_dir(const char *dirname);

    /* execute a ZIP command line */
    int zip(const textchar_t *cmdline, const textchar_t *cmddir);

    /* execute an FTP upload command line */
    int ftp_upload(const textchar_t *local_name,
                   const textchar_t *remote_name,
                   const textchar_t *siteID,
                   const textchar_t *email, DWORD flags);

    /* internet status callback */
    static void CALLBACK inet_status_cb(
        HINTERNET hnet, DWORD_PTR ctx, DWORD stat, VOID *info, DWORD infoLen);

    /* execute an "ifdbput" command */
    int ifdbput(const textchar_t *gamefile,
                const textchar_t *ifdb_username,
                const textchar_t *ifdb_password);

    /* get the next token on a command line */
    int get_cmdline_tok(const textchar_t **cmdline, CStringBuf *tok);

    /* run a documentation search */
    void search_doc();

    /* run a project-wide text search */
    void search_project_files(const textchar_t *query, int exact_case,
                              int regex, int whole_word, int collapse_sp);

    /* prompt for search text, then run a project-wide text search */
    void search_project_files();

    /*
     *   Get the next text file for a multi-file search.  If we can find
     *   another file, we fill in 'fname' with the name of the next file in
     *   line and return TRUE.  If there are no more files, we return FALSE.
     *   'direction' is >0 for a forward search, <0 for a backward search.
     *   If 'local_only' is TRUE, we'll only find local project files (i.e.,
     *   files within the local project directory); otherwise we'll find
     *   files throughout the project, including libraries and extensions.
     */
    int get_next_search_file(char *fname, size_t fname_size,
                             const char *curfile, int direction, int wrap,
                             int local_only);

    /*
     *   Check for auto-saving before a Build or Run command.  Returns true
     *   if it's okay to proceed, false if the user pressed Cancel from our
     *   dialog box.
     */
    int save_on_build_or_run();

    /*
     *   Check for auto-saving and auto-building before a Run command.
     *   Returns true if it's okay to proceed with the command, false if (a)
     *   the user presses Cancel from a dialog we show, or (b) we initiate a
     *   build command.  In case (b), we'll remember the Run command so that
     *   we'll re-initiate the same command automatically if the build is
     *   successful.
     */
    int build_on_run(int command_id);

private:
    /*
     *   Methods that vary based on the debugger version.  The TADS 2 and
     *   TADS 3 debuggers have some minor UI differences; we factor these
     *   differences into these routines, the appropriate implementations
     *   of which can be chosen with the linker when building the
     *   debugger executable.
     */

    /*
     *   adjust a program argument to the proper file type for initial
     *   loading via command-line parameters
     */
    void vsn_adjust_argv_ext(textchar_t *fname);

    /* set my window title, based on the loaded project */
    void vsn_set_win_title();

    /*
     *   determine if we need the "srcfiles" list (this is passed to the
     *   CHtmlDebugHelper object during construction)
     */
    int vsn_need_srcfiles() const;

    /*
     *   Validate a project file we're attempting to load via "Open Project."
     *   If this routine fully handles the loading, returns true; otherwise,
     *   returns false.  Note that if the project cannot be loaded (because
     *   the file type is invalid, for example), this routine should show an
     *   appropriate error alert and then return true; it returns true on
     *   failure because displaying the error message is all that's needed in
     *   such cases, and thus fully handles the load.
     */
    int vsn_validate_project_file(const textchar_t *fname);

    /*
     *   Determine if a project file is a valid project file.  Returns true
     *   if so, false if not.  This differs from vsn_validate_project_file()
     *   in that there is no interactive attempt to load the correct file or
     *   show an error; we simply indicate whether or not we'll be able to
     *   proceed with loading this type of project file.
     */
    int vsn_is_valid_proj_file(const textchar_t *fname);

    /* get the "load profile" */
    int vsn_get_load_profile(char **game_filename,
                             char *game_filename_buf,
                             char *config_filename,
                             const char *load_filename,
                             int game_loaded);

    /* get the game filename from the loaded profile */
    int vsn_get_load_profile_gamefile(char *buf);

    /* load a configuration file */
    static int vsn_load_config_file(CHtmlDebugConfig *config,
                                    const textchar_t *fname);

    /* save a configuration file */
    static int vsn_save_config_file(CHtmlDebugConfig *config,
                                    const textchar_t *fname);

    /*
     *   during configuration loading, close windows specific to this
     *   debugger version (common windows, such as the local variables and
     *   watch expressions windows, will be closed by the common code)
     */
    void vsn_load_config_close_windows();

    /* during configuration loading, open version-specific windows */
    void vsn_load_config_open_windows(const RECT *descrc);

    /* save version-specific configuration data */
    void vsn_save_config();

    /* main debug window version-specific command processing */
    TadsCmdStat_t vsn_check_command(const check_cmd_info *);
    int vsn_do_command(int notify_code, int command_id, HWND ctl);

    /*
     *   check to see if it's okay to proceed with a compile command - if
     *   so, return true; if not, show an error message and return false
     */
    int vsn_ok_to_compile();

    /* clear version-specific non-default configuration information */
    void vsn_clear_non_default_config();

    /* run the debugger about box */
    void vsn_run_dbg_about();

    /* run the program-arguments dialog box */
    void vsn_run_dbg_args_dlg();

    /*
     *   Process captured build output - this is called with each complete
     *   line of text read from the standard output of the build subprocess
     *   as it runs.  The routine can modify the text in the buffer if
     *   desired.  If the text is to be displayed, return true; otherwise,
     *   return false.
     */
    int vsn_process_build_output(CStringBuf *txt);

private:
    /*
     *   Load the file menu - we'll call this on debugger entry if we
     *   haven't yet loaded the file menu.  This sets up the quick-open
     *   file submenu to include the game's source files.
     */
    void load_file_menu();

    /* find the popup menu that contains the given command item */
    HMENU find_parent_menu(HMENU menu, int cmd, int *idx);

    /* callback for building the file menu by enumerating line sources */
    static void load_menu_cb(void *ctx, const textchar_t *fname,
                             int line_source_id);

    /* callback for enumerating open windows during activation */
    static void enum_win_activate_cb(void *ctx0, int idx, IDebugWin *win,
                                     int line_source_id,
                                     HtmlDbg_wintype_t win_type);

    /* callback for enumerating to choose an UntitledN name for a new file */
    static void enum_win_untitled_cb(void *ctx0, int idx, IDebugWin *win,
                                     int line_source_id,
                                     HtmlDbg_wintype_t win_type);

    /* ask to save changes in any modified source windows */
    static void enum_win_ask_save_cb(void *ctx, int idx, IDebugWin *win,
                                     int line_source_id,
                                     HtmlDbg_wintype_t win_type);

    /* check for windows with unsaved changes */
    void enum_win_check_save_cb(
        void *ctx0, int idx, IDebugWin *win, int line_source_id,
        HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows to close the source windows */
    static void enum_win_close_cb(void *ctx0, int idx, IDebugWin *win,
                                  int line_source_id,
                                  HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows for saving layout to config */
    static void enum_win_savelayout_cb(void *ctx0, int idx, IDebugWin *win,
                                       int line_source_id,
                                       HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows for saving all modified files */
    static void enum_win_savefile_cb(void *ctx0, int idx, IDebugWin *win,
                                     int line_source_id,
                                     HtmlDbg_wintype_t win_type);

    /* callback for enumerating open windows for building the Window menu */
    static void winmenu_win_enum_cb(void *ctx0, int idx, IDebugWin *win,
                                    int line_source_id,
                                    HtmlDbg_wintype_t win_type);

    /* callback for enumerating windows for updating option settings */
    static void options_enum_srcwin_cb(void *ctx, int idx,
                                       class IDebugWin *win,
                                       int line_source_id,
                                       HtmlDbg_wintype_t win_type);

    /* clear the list of build comands */
    void clear_build_cmds();

    /*
     *   Add a build command at the end of the list.  If 'required' is
     *   true, the command will be executed even if a previous command
     *   fails; otherwise, the command will be skipped if any previous
     *   command fails.
     */
    void add_build_cmd(const textchar_t *desc,
                       const textchar_t *exe_name,
                       const textchar_t *exe,
                       const textchar_t *cmdline,
                       const textchar_t *cmd_dir,
                       int required);

    /*
     *   Add an in-memory temp file to the build list.  These are simply
     *   named strings that can be read back for use by intrinsic command
     *   tools.  These are all deleted by clear_build_cmds().
     */
    void add_build_memfile(const textchar_t *name, const textchar_t *val);

    /* append data to a build memfile */
    void append_build_memfile(const textchar_t *name, const textchar_t *val);

    /* get the contents of the named in-memory temp file */
    const textchar_t *get_build_memfile(const textchar_t *name);
    class CHtmlDbgBuildFile *find_build_memfile(const textchar_t *name);

    /* save configuration information for the 'find' dialog */
    int save_find_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* load configuration information for the 'find' dialog */
    int load_find_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* enumeration callback for writing profiling data */
    static void write_prof_data(void *ctx0, const char *func_name,
                                unsigned long time_direct,
                                unsigned long time_in_children,
                                unsigned long call_cnt);

    int do_timer(int id);

    /* add/remove an MDI window's tab in the tab control */
    void add_mdi_win_tab(class CTadsWin *win);
    void del_mdi_win_tab(class CTadsWin *win);

    /*
     *   Context structure from TADS debugger.  We set this on each call
     *   from TADS, so that we have the correct TADS debugger context
     *   whenever we're active.
     */
    struct dbgcxdef *dbgctx_;

    /*
     *   Pointer to the execution location in the debugger.  By changing
     *   this, we can change the next instruction that will be executed.
     *   Note that we must not change this so that it's outside of the
     *   object.method or function that contained the original location upon
     *   the most recent entry into the debugger.
     */
    void *exec_ofs_ptr_;

    /*
     *   Debugger helper
     */
    class CHtmlDebugHelper *helper_;

    /*
     *   Main HTMLTADS preferences object
     */
    class CHtmlPreferences *prefs_;

    /*
     *   The Loading dialog - this is shown while we're loading a new
     *   project, so it doesn't look like we've frozen up if we have a lot of
     *   text to load into Scintilla.  Scintilla is none too speedy with its
     *   initial text loading, so it's good to provide some visual progress
     *   indication if we're going to be doing a lot of it.
     */
    class CTadsDialogLoading *loading_dlg_;

    /*
     *   The Find-and-Replace dialog object.  The Replace dialog is a
     *   superset of the basic Find dialog, so we use a single object to
     *   handle both, which lets us keep our past search history in common to
     *   both dialogs.
     */
    class CTadsDialogFindReplace *find_dlg_;

    /* the project search dialog */
    class CTadsDialogFindReplace *projsearch_dlg_;

    /* text of current/last find */
    textchar_t find_text_[512];

    /* text of current/last replace */
    textchar_t replace_text_[512];

    /*
     *   The file and selection range of the first match of the current
     *   search.  We use these to detect when a search has wrapped: when a
     *   new search yields the same result, we know that we've wrapped around
     *   to the starting position, so there are no more matches.
     */
    HtmlWinRange find_start_;

    /* the file and selection range of the most recent search match */
    HtmlWinRange find_last_;

    /* name of the game file */
    CStringBuf game_filename_;

    /* debugger global configuration object, and configuration filename */
    class CHtmlDebugConfig *global_config_;
    CStringBuf global_config_filename_;

    /* local (program-specific) configuration */
    class CHtmlDebugConfig *local_config_;
    CStringBuf local_config_filename_;

    /*
     *   If a menu item is currently selected during menu navigation, we'll
     *   store a pointer to the command descriptor here.  This lets us show
     *   the menu's description in the status line.
     */
    const HtmlDbgCommandInfo *statusline_menu_cmd_;

    /*
     *   if we're working on a long-running process, we can put status
     *   information here to keep the user apprised of what's going on
     */
    CStringBuf lrp_status_;

    /*
     *   Timer for bringing the game window to the front after starting or
     *   resuming execution.  Whenever we return to the VM, we set a brief
     *   timeout; when the timeout expires, we check to see if the game is
     *   running, and if so we bring its window to the front.  This helps
     *   make sure the game UI is in front whenever the game is active.
     */
    int game_to_front_timer_;

    /*
     *   Flag indicating that we're stepping execution.  We use this to
     *   exit the debugger event loop when the user steps or runs, so that
     *   control leaves the debugger event loop and returns to TADS to
     *   continue game execution.
     */
    int go_;

    /*
     *   Flag indicating that we're in the debugger.  This is set to true
     *   whenever the debugger UI takes control, and set to false when the
     *   debugger returns control to TADS.
     */
    int in_debugger_ : 1;

    /*
     *   Flag indicating that we're quitting.  If a quit command is
     *   entered, we'll set this flag and then exit the event loop as
     *   though we were about to resume execution, aborting the program at
     *   the last moment.  (We can't simply quit, because we need to be
     *   sure to properly exit out of the event loop.)
     */
    int quitting_ : 1;

    /*
     *   Flag indicating that we're restarting.  If a restart command is
     *   entered, we'll set this flag and then exit the event loop,
     *   flagging to the event loop caller that the game is to be
     *   restarted.
     */
    int restarting_ : 1;

    /*
     *   Flag indicating that we have a pending reload.
     */
    int reloading_ : 1;

    /*
     *   flag indicating that the reload is a different game than was
     *   previously loaded
     */
    int reloading_new_ : 1;

    /*
     *   Flag indicating how we're to proceed upon reloading a new game.
     *   If reloading_go_ is true, we'll jump right in and start executing
     *   the newly-loaded game until we hit a breakpoint, or something
     *   else interrupts execution; otherwise, we'll stop at the first
     *   instruction upon loading the new game.
     */
    int reloading_go_ : 1;

    /*
     *   Flag indicating whether or not we're to begin execution at all
     *   when loading or reloading.  This is set only when the user enters
     *   a Go, Step, or other command that explicitly begins execution.
     */
    int reloading_exec_ : 1;

    /*
     *   flag indicating that we're forcing termination of the game (but
     *   not quitting out of the debugger itself)
     */
    int terminating_ : 1;

    /* flag indicating termination is in progress (for status messages) */
    int term_in_progress_ : 1;

    /*
     *   Flag indicating that we're aborting the current command.  This
     *   works like the 'quitting' and 'restarting' flags.
     */
    int aborting_cmd_ : 1;

    /*
     *   Flag: we're in the process of refreshing our sources after
     *   re-activation of our window
     */
    int refreshing_sources_ : 1;

    /*
     *   Flag: we might need to rebuild the project.  We set this to true
     *   each time we save a file that's part of the project list, since we
     *   might then need to rebuild the project to bring the compiled version
     *   up to date with the source.  We set this to false each time we
     *   perform a build.
     */
    int maybe_need_build_ : 1;

    /* Flag: the profiler is running */
    int profiling_ : 1;

    /* my system accelerator table */
    HACCEL waccel_;

    /* my custom accelerator table */
    CTadsAccelerator *taccel_;

    /*
     *   Current active debugger window.  Each debugger window that wants
     *   to receive commands must register itself here when it becomes
     *   active, and deregister itself on becoming inactive.
     */
    class CHtmlDbgSubWin *active_dbg_win_;

    /*
     *   Last active debugger window.  We keep track of this so that we
     *   can route commands that are intended for debugger windows to the
     *   appropriate window when the main window itself is active.  When
     *   the main window is active, we'll route commands to the last
     *   active window if it's the window immediately below the main
     *   window in Z order.
     */
    class CHtmlDbgSubWin *last_active_dbg_win_;

    /*
     *   The last active debugger window for certain special command target
     *   purposes.  This is effectively just the last active debug window,
     *   but we don't clear this just becuase of a focus change.
     */
    class CHtmlDbgSubWin *last_cmd_target_win_;

    /*
     *   Menu delegator window.  If one of the debugger windows sent us a
     *   menu keystroke (with the ALT key) to run our menu, it wants to be
     *   re-activated as soon as the menu is finished.  We'll remember the
     *   window here, and re-activate it again when we're finished.
     */
    HWND post_menu_active_win_;

    /*
     *   Watch window, local variable window
     */
    class CHtmlDbgWatchWin *watchwin_;
    class CHtmlDbgWatchWin *localwin_;

    /* project window (TADS 3 only) */
    class CHtmlDbgProjWin *projwin_;

    /* script window (TADS 3 only) */
    class CHtmlDbgScriptWin *scriptwin_;

    /* MDI client dock port */
    class CTadsDockportMDI *mdi_dock_port_;

    /* the MDI "Window" menu handle */
    HMENU win_menu_hdl_;

    /* rebar handle */
    HWND rebar_;

    /* main menu toolbar */
    class ToolbarMenubar *menubar_;

    /* menu icon handler - this adds toolbar-type icons to the menus */
    class IconMenuHandler *iconmenu_;

    /* the actual main menu */
    HMENU main_menu_;

    /* the "Tools" menu handle */
    HMENU tools_menu_;

    /* main debug toolbar handle */
    HWND toolbar_;

    /* edit toolbar */
    HWND editbar_;

    /* doc toolbar */
    HWND docbar_;

    /* search toolbar handle */
    HWND searchbar_;

    /* search toolbar combo field */
    HWND searchbox_;

    /* current searchbar search command */
    int searchbarcmd_;

    /* original window procedure for the search box edit field */
    static WNDPROC searchbox_orig_winproc_;

    /* statusline */
    class CTadsStatusline *statusline_;

    /* statusline data source for the cursor line/col panel */
    CTadsStatusSourceCursorPos cursor_pos_source_;

    /* statusline data source for the build progress bar */
    CTadsStatusSourceProgress progress_source_;

    /* MDI tab control */
    class MdiTabCtrl *mdi_tab_;

    /* flag: status line is visible */
    int show_statusline_;

    /*
     *   Build thread handle.  While a build is running, this is the
     *   handle of the background thread handling the build.
     */
    HANDLE build_thread_;

    /* compiler process handle and PID - valid while a build is running */
    HANDLE compiler_hproc_;
    DWORD compiler_pid_;

    /* current compiler status message */
    CStringBuf compile_status_;

    /* head and tail of list of build commands */
    class CHtmlDbgBuildCmd *build_cmd_head_;
    class CHtmlDbgBuildCmd *build_cmd_tail_;
    int build_cmd_cnt_, build_cmd_cur_;

    /* list of build mem files */
    class CHtmlDbgBuildFile *build_files_;

    /*
     *   Build wait dialog.
     *
     *   This dialog is displayed when the user attempts to close the
     *   program and a build is still running; we use the dialog to keep the
     *   UI open until the build completes, at which point we terminate the
     *   program normally.
     *
     *   In addition, this can be used to run a build modally.  Most builds
     *   run modelessly, so that the user can continue operating most parts
     *   of the user interface while a build proceeds in the background.
     *   This is not always desirable; for example, when we scan for
     *   includes, we want to run a build modally.  By showing a modal
     *   dialog while building, the code can effectively make a build modal.
     */
    class CTadsDialog *build_wait_dlg_;

    /*
     *   The pending build command queue.  We fire these commands when the
     *   current game terminates.  If the user tries to start a build while
     *   the game is still running, we'll set this to the desired build
     *   command, then set flags to terminate the game, then give control
     *   back to the game.  The game should see the termination flags and
     *   exit, at which point we'll fire this build command.
     */
    static const int PENDING_BUILD_MAX = 10;
    int pending_build_cmd_[PENDING_BUILD_MAX];
    int pending_build_cnt_;

    /* add a pending build command */
    void add_pending_build_cmd(int cmd)
    {
        if (pending_build_cnt_ < PENDING_BUILD_MAX)
            pending_build_cmd_[pending_build_cnt_++] = cmd;
    }

    /* set a new pending build command, clearing any previous list */
    void set_pending_build_cmd(int cmd)
    {
        pending_build_cmd_[0] = cmd;
        pending_build_cnt_ = 1;
    }

    /* clear the pending build command list */
    void clear_pending_build_cmds() { pending_build_cnt_ = 0; }

    /*
     *   the command ID of the running build, if any (this is valid only if
     *   build_running_ is true)
     */
    int build_cmd_;

    /*
     *   the command ID that initiated the current build, when the build is
     *   triggered as a side effect of a GO or STEP command
     */
    int run_after_build_cmd_;

    /*
     *   If we're starting the game by replaying a script, this is the name
     *   of the script to replay.
     */
    CStringBuf replay_script_;

    /*
     *   Is the replay script a temporary file?  If so, we'll automatically
     *   delete it upon terminating the run.
     */
    int is_replay_script_temp_ : 1;

    /*
     *   Flag indicating that the game is loaded.  This is set while the
     *   game is running, and cleared when the game quits and the TADS
     *   engine shuts down.
     */
    int game_loaded_ : 1;

    /*
     *   Flag indicating that a workspace is open.  The workspace is open
     *   even after a game terminates, since it can be reloaded with the
     *   current workspace context (breakpoints, window placement, etc).
     */
    int workspace_open_ : 1;

    /*
     *   Flag indicating that a build is in progress (in a background
     *   thread).  We can't do certain things when we're building.
     */
    int build_running_ : 1;

    /*
     *   flag indicating that we're capturing build output for internal
     *   processing rather than displaying it on the console - if this is
     *   set, we won't show any build output, but just run it all through our
     *   filter routine
     */
    int capture_build_output_ : 1;

    /*
     *   flag indicating that we're filtering build output through our filter
     *   routine - if this is set, we'll show all build output that the
     *   filter function says we should show
     */
    int filter_build_output_ : 1;

    /*
     *   flag indicating that we should execute the program as soon as
     *   we're finished building it
     */
    int run_after_build_ : 1;

    /*
     *   Flag indicating whether or not we automatically flush game window
     *   output upon entering the debugger.  This is a preference setting,
     *   but we keep it cached here since we need to read the setting
     *   frequently.
     */
    unsigned int auto_flush_gamewin_ : 1;

    /*
     *   Flag: we're in the process of loading a configuration.  If this
     *   is set, we'll defer showing MDI windows until all of the frame
     *   adornments have been created, so that the MDI window positions
     *   relative to the MDI client will be correctly adjusted for all the
     *   frame adornments.
     */
    unsigned int loading_win_config_ : 1;

    /*
     *   Flag: the MDI child window is maximized.  We keep this flag
     *   ourselves, rather than asking the MDI client for this status,
     *   because we want to retain the status even when no MDI child windows
     *   are open.  The MDI client interface keeps this information with the
     *   individual child windows, so when there are no windows open, it
     *   resets the status to non-maximized.  We want the maximized status to
     *   stick even when no child windows are currently open, so we keep
     *   track of it ourselves.  We update this whenever an MDI child is
     *   maximized or restored.
     */
    unsigned int mdi_child_maximized_ : 1;

    /*
     *   Is a Windows Explorer file drag-drop in progress?  We set this to
     *   true on DragEnter if a file (or set of files) is being dragged over
     *   the window.
     */
    unsigned int drop_file_ok_ : 1;

    /*
     *   Build status.  This records the current build action and the overall
     *   progress, as an estimated "% done" indication.  We keep this
     *   information primarily for the HTML build dialogs (e.g., Publish),
     *   since HTML dialogs have to poll for updates due to Javascript's
     *   single-threadedness.
     *
     *   The mutex is used to serialize access to this information, since
     *   it's for communicating across threads.
     *
     *   The build step is the step number (starting at 0) in the overall
     *   command list.  The description is the command's descriptive text,
     *   which we set while constructing the build command list.  The
     *   sub-description is an additional description message giving the
     *   status within the step.
     *
     *   The step progress and step length give the degree of progress into
     *   the current step, in arbitrary units.  For example, for the
     *   compiler, the step length can simply be the number of files to
     *   compile, and the step progress can be the current file index.  For
     *   an upload, the length can be the total number of bytes to transfer
     *   and the progress can be the number of bytes sent so far.  To
     *   calculate the progress through a step, simply take the ratio of the
     *   step progress to the step length.
     *
     *   The 'success' flag is set at the end of the build process to TRUE if
     *   the build was success, FALSE if an error occurred.
     */
    HANDLE stat_mutex_;
    int stat_build_step_;
    CStringBuf stat_desc_;
    CStringBuf stat_subdesc_;
    DWORD stat_step_progress_;
    DWORD stat_step_len_;
    int stat_success_;

    /*
     *   the original TADSLIB environment variable we inherited from our
     *   parent environment
     */
    CStringBuf orig_tadslib_var_;

    /* head of list of extensions DLLs */
    struct twb_extension *extensions_;

    /*
     *   The next available custom command ID.  These command IDs are
     *   assigned to extensions on request as they add new commands.  These
     *   are assigned from the range ID_CUSTOM_FIRST to ID_CUSTOM_LAST.
     */
    unsigned short next_custom_cmd_;

    /* the global list of pre-defined debugger commands */
    static HtmlDbgCommandInfo command_list_[];

    /* our linked list of extension-defined custom commands */
    struct HtmlDbgCommandInfoExt *ext_commands_;
};

/*
 *   Window list entry
 */
struct html_dbg_win_list
{
    html_dbg_win_list(HWND h)
    {
        hwnd = h;
        nxt = 0;
    }

    /* current window */
    HWND hwnd;

    /* next entry in list */
    html_dbg_win_list *nxt;
};

/*
 *   TADS Workbench Extension DLL tracker.  We create one of these for each
 *   extension library we load, and keep the trackers in a linked list.
 */
struct twb_extension
{
    /* create */
    twb_extension(HMODULE hmod, IUnknown *unk);

    /* release extension interfaces */
    void release_interfaces()
    {
        /* release our IUnknown (we always have this one) */
        unk->Release();
        unk = 0;

        /* release our extension interface */
        if (iext != 0)
        {
            iext->Release();
            iext = 0;
        }
    }

    /* our module handle */
    HMODULE hmod;

    /* our basic IUnknown interface */
    IUnknown *unk;

    /* the extension interface */
    ITadsWorkbenchExtension *iext;

    /* the next extension in the list */
    twb_extension *nxt;
};


/* ------------------------------------------------------------------------ */
/*
 *   Build Command list entry.  When we prepare to run a build, we create
 *   a list of these build command entries, which we pass to the
 *   background thread for execution.
 *
 *   We build these in the main thread, so that the background process
 *   doesn't have to look at configuration data or perform memory
 *   management; this removes the need to worry about threading issues
 *   with any of our major subsystems.
 */
class CHtmlDbgBuildCmd
{
public:
    CHtmlDbgBuildCmd(const textchar_t *desc,
                     const textchar_t *exe_name,
                     const textchar_t *exe,
                     const textchar_t *cmdline,
                     const textchar_t *cmd_dir, int required)
    {
        /* remember the build data */
        desc_.set(desc);
        exe_name_.set(exe_name);
        exe_.set(exe);
        args_.set(cmdline);
        dir_.set(cmd_dir);
        required_ = required;

        /* we're not in a list yet */
        next_ = 0;
    }

    /* next build command in the list */
    CHtmlDbgBuildCmd *next_;

    /* status message description of the step */
    CStringBuf desc_;

    /* display name of the command */
    CStringBuf exe_name_;

    /* executable filename */
    CStringBuf exe_;

    /* command line arguments */
    CStringBuf args_;

    /* directory in which to execute the command */
    CStringBuf dir_;

    /* flag: execute the command even if a prior command fails */
    int required_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Build memory file list entry
 */
class CHtmlDbgBuildFile
{
public:
    CHtmlDbgBuildFile(const textchar_t *name, const textchar_t *val)
    {
        this->name.set(name);
        this->val.set(val);
        nxt = 0;
    }

    /* do I match the given name? */
    int match_name(const textchar_t *name)
    {
        return get_strcmp(name, this->name.get()) == 0;
    }

    /* the next file in the list */
    CHtmlDbgBuildFile *nxt;

    /* the name of the file */
    CStringBuf name;

    /* the contents */
    CStringBuf val;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base window frame class for debugger windows.
 */
class CHtmlSys_tdbwin: public CHtmlSys_dbgwin
{
public:
    CHtmlSys_tdbwin(class CHtmlParser *parser,
                    class CHtmlPreferences *prefs,
                    class CHtmlDebugHelper *helper,
                    class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlSys_tdbwin();

    /* initialize the HTML panel */
    virtual void init_html_panel(class CHtmlFormatter *formatter);

    /*
     *   Store my position in the preferences, on changing the window
     *   position.  We don't actually store anything here; we save our
     *   position on certain other events, such as closing the window or
     *   closing the project.
     *
     *   We don't store anything here because the order of events in MDI mode
     *   makes it impossible to tell whether or not we're maximized when a
     *   move/size event fires.  We can tell later on, after the overall
     *   size/move has finished, so we post a message to ourselves to save
     *   our preferences later.
     */
    void set_winpos_prefs(const class CHtmlRect *)
    {
        /* post ourselves our private deferred-save-position message */
        PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
    }

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);

    /* destroy the window */
    void do_destroy();

    /* get the frame handle */
    virtual HWND get_owner_frame_handle() const { return handle_; }

    /* non-client activate or deactivate */
    int do_ncactivate(int flag);

    /* artificial activation from parent */
    void do_parent_activate(int active);

    /* check the status of a command */
    TadsCmdStat_t check_command(const check_cmd_info *);

    /* execute a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command routed from the main window */
    TadsCmdStat_t active_check_command(const check_cmd_info *);

    /* execute a command routed from the main window */
    int active_do_command(int notify_code, int command_id, HWND ctl);

    /* get my window handle */
    HWND active_get_handle() { return get_handle(); }

    /* receive notifications */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* get the find text via the main debugger interface */
    const textchar_t *get_find_text(int command_id, int *exact_case,
                                    int *start_at_top, int *wrap, int *dir)
    {
        return dbgmain_->get_find_text(command_id, exact_case,
                                       start_at_top, wrap, dir);
    }

protected:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter) = 0;

    /*
     *   Do the work of storing my window position in the preferences, on
     *   closing the window or closing the project.  If we are the current
     *   maximized MDI child window, we will not store the information - the
     *   maximized size is entirely dependent on the frame window size, so we
     *   don't need to remember it, and we do want to remember the 'restored'
     *   size without overwriting it with the maximized size.
     */
    void set_winpos_prefs_deferred();

    /*
     *   internal routine: store the given MDI-frame-relative position in the
     *   preferences as our window position; this must be overridden per
     *   subclass, since each window type defines its own serialization
     */
    virtual void store_winpos_prefs(const CHtmlRect *pos) = 0;

    /* my debugger helper object */
    class CHtmlDebugHelper *helper_;

    /* the main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;
};

/*
 *   HTML panel base class for debugger HTML panels
 */
class CHtmlSysWin_win32_tdb: public CDebugSysWin
{
public:
    CHtmlSysWin_win32_tdb(class CHtmlFormatter *formatter,
                          class CHtmlSysWin_win32_owner *owner,
                          int has_vscroll, int has_hscroll,
                          class CHtmlPreferences *prefs,
                          class CHtmlDebugHelper *helper,
                          class CHtmlSys_dbgmain *dbgmain);

    ~CHtmlSysWin_win32_tdb();

    /* route command handling to the main debug window */
    TadsCmdStat_t check_command(const check_cmd_info *);
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(const check_cmd_info *info);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id, HWND ctl);

protected:
    /* carry out a "Find" command */
    void do_find(int command_id);

    /* show the 'not found' message for a 'find' command */
    void find_not_found();

    /* window creation */
    void do_create();

    /* debugger helper */
    class CHtmlDebugHelper *helper_;

    /* the main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Source window frame class.  This is a container for a Scintilla edit
 *   control, which does all of the editing work.
 */
class CHtmlSys_dbgsrcwin: public CTadsWin, public IDebugSysWin,
    public CHtmlDbgSubWin, public IScintillaParent
{
public:
    CHtmlSys_dbgsrcwin(class CHtmlPreferences *prefs,
                       class CHtmlDebugHelper *helper,
                       class CHtmlSys_dbgmain *dbgmain,
                       const textchar_t *win_title,
                       const textchar_t *full_path);

    ~CHtmlSys_dbgsrcwin();

    /* get our active Scintilla control */
    class ScintillaWin *get_scintilla() const { return asci_; }

    /* set my editor mode */
    void set_editor_mode(int cmd_id);
    void set_editor_mode(ITadsWorkbenchEditorMode *mode, int cmd_id);

    /* set my default extension */
    void set_default_ext(const textchar_t *defext) { defext_.set(defext); }

    /* get the full path to the file, if known */
    const textchar_t *get_path() const { return path_.get(); }

    /*
     *   get the base window title (not including modifiers, like the "*"
     *   dirty flag)
     */
    const textchar_t *get_title() const { return title_.get(); }

    /*
     *   is the file dirty (i.e., has it been modified in memory since
     *   loading or since the last save)?
     */
    int is_dirty() const { return dirty_; }

    /* get/set the read-only flag */
    int is_readonly() const { return readonly_; }
    void set_readonly(int f);

    /* save the file; returns TRUE on success, FALSE on failure or cancel */
    int save_file();

    /* ask the user for a new name and save under the new name */
    int save_file_as_prompt();

    /* save the file under the given filename */
    int save_file_as(const textchar_t *fname);

    /* handle a change to the name of my underlying file */
    void file_renamed(const textchar_t *new_name);

    /*
     *   check the file's current timestamp against the timestamp when we
     *   loaded the file; returns true if the file has changed since we
     *   loaded it
     */
    int file_changed_on_disk() const;

    /* update our copy of the file timestamp to its current timestamp */
    void update_file_time();

    /* update our window title for a change in our filename or dirty status */
    void update_win_title();

    /*
     *   Check the file to see if it's been explicitly opened for editing.
     *   If it has, we won't prompt for reloading, but will simply reload; if
     *   the user explicitly edited the file, they almost certainly want to
     *   reload it.
     */
    int was_file_opened_for_editing() const { return opened_for_editing_; }

    /* left click */
    virtual int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* on releasing the mouse button, end any drag mode */
    virtual int do_leftbtn_up(int keys, int x, int y);

    /* show our context menu */
    virtual int do_context_menu(HWND hwnd, int x, int y);

    /* mouse movement */
    virtual int do_mousemove(int keys, int x, int y);

    /* set the cursor shape */
    virtual int do_setcursor(HWND, int hittest, int mousemsg);

    /* handle a capture change */
    virtual int do_capture_changed(HWND hwnd);

    /* handle a timer event */
    int do_timer(int timer_id);

    /* route command handling to the main debug window */
    TadsCmdStat_t check_command(const check_cmd_info *);
    int do_command(int notify_code, int command_id, HWND ctl);

    /* handle a left/right-click on the title bar */
    int do_nc_leftbtn_down(int keys, int x, int y, int clicks, int hit_type);
    int do_nc_rightbtn_down(int keys, int x, int y, int clicks, int hit_type);

    /* resolve all bookmarks associated with this file to line numbers */
    void resolve_bookmarks(int closing);

    /* delete a bookmark */
    void delete_bookmark(tdb_bookmark *b);

    /* delete all bookmarks in this file */
    void delete_file_bookmarks();

    /* set scintilla's indicators from our internal records */
    void restore_indicators();

    /* set scintilla's bookmarks from our internal records */
    void restore_bookmarks();

    /* IDebugSysWin implementation */
    virtual void idw_add_ref() { AddRef(); }
    virtual void idw_release_ref() { Release(); }
    virtual void idw_close() { destroy_handle(); }
    virtual int idw_query_close(struct ask_save_ctx *ctx)
        { return query_close(ctx); }
    virtual int idw_is_dirty() { return is_dirty(); }
    virtual void idw_reformat(const CHtmlRect *new_scroll_pos);
    virtual void idw_post_load_init();
    virtual void idw_show_source_line(int linenum);
    virtual CHtmlSysWin *idw_as_syswin() { return 0; }
    virtual HWND idsw_get_handle() { return handle_; }
    virtual HWND idsw_get_frame_handle() { return handle_; }
    virtual void idsw_bring_to_front()
    {
        HWND f;

        /* bring me to the top */
        w32_webui_yield_foreground();
        SetForegroundWindow(handle_);
        BringWindowToTop(handle_);

        /* if I don't have focus, set focus in the Scintilla window */
        f = GetFocus();
        if (f != handle_ && !IsChild(handle_, f))
            SetFocus(asci_->get_handle());
    }
    virtual void idsw_note_debug_format_changes(
        HTML_color_t bkg_color, HTML_color_t text_color,
        int use_windows_colors, int use_windows_bgcolor,
        HTML_color_t sel_bkg_color, HTML_color_t sel_text_color,
        int use_windows_sel_colors, int use_windows_sel_bgcolor);
    virtual void idsw_select_line();
    virtual void idsw_set_untitled() { untitled_ = TRUE; }
    virtual const textchar_t *idsw_get_cfg_path() { return get_path(); }
    virtual const textchar_t *idsw_get_cfg_title() { return get_title(); }
    virtual class CHtmlDbgSubWin *idsw_as_dbgsubwin() { return this; }
    virtual class CHtmlSys_dbgsrcwin *idsw_as_srcwin() { return this; }

    /* CHtmlDbgSubWin implementation */
    virtual TadsCmdStat_t active_check_command(const check_cmd_info *);
    virtual int active_do_command(int notify_code, int command_id, HWND ctl);
    virtual HWND active_get_handle() { return handle_; }
    virtual void active_to_front() { idsw_bring_to_front(); }
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end);
    virtual int get_sel_range_spots(void **a, void **b);
    virtual unsigned long get_spot_pos(void *spot);
    virtual void delete_spot(void *spot);
    virtual int get_sel_text(CStringBuf *buf, unsigned int flags,
                             size_t maxlen);
    virtual int set_sel_range(unsigned long start, unsigned long end);
    virtual int active_get_tooltip(TOOLTIPTEXT *ttt);
    ITadsWorkbenchEditorMode *get_editor_mode() const { return mode_; }
    virtual const textchar_t *active_get_key_table() { return "Text Editor"; }
    virtual const textchar_t *active_get_filename() const
        { return path_.get(); }
    virtual int active_get_caretpos(int *line, int *col);
    virtual int active_get_caretpos(unsigned long *pos);
    virtual void active_undo_group(int begin);
    virtual int active_allow_find(int regex) { return TRUE; }
    virtual int active_allow_replace() { return TRUE; }
    virtual int active_find_text(
        const textchar_t *txt,
        int exact_case, int wrap, int direction, int regex, int whole_word,
        unsigned long first_match_a, unsigned long first_match_b);
    virtual void active_replace_sel(
        const textchar_t *txt, int direction, int regex);
    virtual void active_replace_all(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex, int whole_word);

    /* IScintillaWin implementation */
    virtual HWND isp_get_handle() { return handle_; }
    virtual void isp_toggle_breakpoint(ScintillaWin *sci);
    virtual void isp_set_dirty(int dirty);
    virtual void isp_note_change()
    {
        /* remember the timestamp of the last change */
        last_mod_time_ = GetTickCount();
        mod_pending_ = TRUE;
    }
    virtual void isp_drag_cur_line(ScintillaWin *sci);
    virtual void isp_on_focus_change(ScintillaWin *win, int gaining_focus);
    virtual void isp_update_cursorpos(int lin, int col);
    virtual int isp_eval_tip(const textchar_t *expr,
                             textchar_t *valbuf, size_t vallen);
    virtual void isp_lrp_status(const textchar_t *msg)
        { dbgmain_->set_lrp_status(msg); }
    virtual void isp_drop_file(const char *fname)
        { dbgmain_->drop_shell_file(fname); }

protected:
    /* is this a script file? */
    int is_script_file();

    /* replay a script file to the cursor position */
    void replay_to_cursor();

    /* set my filename */
    void set_path(const textchar_t *fname);

    /* window creation */
    void do_create();

    /* window destruction */
    void do_destroy();

    /* close */
    int do_close();

    /* query for closing */
    int query_close(struct ask_save_ctx *ctx);

    /* draw the content area */
    void do_paint_content(HDC hdc, const RECT *area_to_draw);

    /* our painting covers the whole area, so no need to erase anything */
    virtual int do_erase_bkg(HDC) { return TRUE; }

    /* move */
    void do_move(int x, int y);

    /* resize */
    void do_resize(int mode, int x, int y);

    /* handle a 'go to line' command */
    void do_goto_line();

    /* handle non-client activation */
    int do_ncactivate(int flag);

    /* handle activation from the parent */
    void do_parent_activate(int flag);

    /* focus gain */
    void do_setfocus(HWND prv);

    /* process a user message */
    int do_user_message(int msg, WPARAM wpar, LPARAM lpar);
#define SRCM_AFTER_CREATE  (HTMLM_LAST + 1)

    /* handle a notification */
    int do_notify(int control_id, int notify_code, LPNMHDR nm);

    /* store my window position in the preferences file */
    void store_winpos_prefs();

    /* update my line-wrap mode with Scintilla */
    void update_wrap_mode();

    /* perform a "find" command */
    void do_find(int command_id);

    /* perform an incremental search */
    void do_inc_search(int command_id);

    /* find the definition of the symbol at the cursor */
    void do_find_symdef();

    /* read and process a repeat count command */
    void read_repeat_count();

    /* read and process a quoted keystroke command */
    void read_quoted_key();

    /* perform a "print" command */
    void do_print();

    /* figure the current layout based on the splitter size */
    void split_layout();

    /*
     *   Synchronize our internal line status records with the Scintilla line
     *   indicators.  We have to do this after the user edits the document
     *   because Scintilla can move around its status indicators to
     *   compensate for insertions and deletions.  We need to update our
     *   internal records so that they match the Scintilla indicators after
     *   such changes.
     */
    void sync_indicators();

    /* toggle a bookmark at the current line */
    void toggle_bookmark();

    /* find a bookmark at the given line */
    tdb_bookmark *find_bookmark(int line);

    /* prompt for a bookmark name, and add a bookmark for the name */
    void add_named_bookmark();

    /* add a bookmark */
    void add_bookmark(int name);

    /* jump to the next/previous bookmark in source order */
    void jump_to_next_bookmark(int dir);

    /*
     *   our context menu for ordinary files, for script files, and for
     *   margin clicks
     */
    HMENU ctx_menu_;
    HMENU script_menu_;
    HMENU margin_menu_;

    /* main htmltads preferences object */
    class CHtmlPreferences *prefs_;

    /* debugger helper object */
    class CHtmlDebugHelper *helper_;

    /* main debugger window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* our full filename path */
    CStringBuf path_;

    /*
     *   Our default Save As extension.  If we have a valid filename, this is
     *   simply the current filename extension.  If this is instead an
     *   untitled new file that hasn't been saved yet, it's the extension for
     *   the file type that the user selected when creating the new window.
     */
    CStringBuf defext_;

    /* our base window title (minus any flags, like the "*" dirty flag) */
    CStringBuf title_;

    /* our top and bottom Scintilla controls (separated by the splitter) */
    class ScintillaWin *sci_;
    class ScintillaWin *sci2_;

    /*
     *   The splitter-bar position.  We store this as the ratio of the height
     *   of the top window to the height of the whole window - the fractional
     *   representation lets us keep it smoothly proportional as the window
     *   changes size.
     */
    double split_;

    /*
     *   our *active* Scintilla control - this is the last Scintilla control
     *   that had keyboard focus
     */
    class ScintillaWin *asci_;

    /* the line wrapping mode (N=none, C=char, W=word) */
    char wrap_mode_;

    /* our editor mode, and the associated command ID */
    ITadsWorkbenchEditorMode *mode_;
    int mode_cmd_id_;

    /*
     *   modification date of the file, if we have a path -- we use this to
     *   track when the file is changed externally when we're activated, so
     *   that we can reload it when the user modifies it from another process
     */
    FILETIME file_time_;

    /*
     *   previous i-search - we'll resume this search if the first thing in
     *   the search is a Ctrl+S or a Ctrl+R
     */
    CStringBuf last_isearch_query_;
    unsigned int last_isearch_regex_ : 1;
    unsigned int last_isearch_exact_ : 1;
    unsigned int last_isearch_word_ : 1;

    /* UI sync timer */
    int ui_sync_timer_;

    /* the Scintilla window in which the drag is taking place */
    ScintillaWin *drag_sci_;

    /* drag timer */
    int drag_timer_;

    /* starting line number of curent-line dragging */
    int drag_start_line_;

    /* line number of temporary drag line pointer */
    int drag_target_line_;

    /* number of timer events while dragging the current line */
    int drag_line_ticks_;

    /* have we moved the drag line at all in the drag? */
    int drag_line_ever_moved_;

    /* in splitter tracking mode, the original mouse y offset */
    int drag_splitter_yofs_;

    /* flag: the file was opened for editing since last loaded */
    unsigned int opened_for_editing_ : 1;

    /* time of last change to Scintilla document or UI */
    DWORD last_mod_time_;

    /* flag: a modification is pending for the purposes of our UI syncing */
    unsigned int mod_pending_ : 1;

    /* flag: is the document dirty (i.e., modified since last load/save)? */
    unsigned int dirty_ : 1;

    /* flag: is the window in read-only mode? */
    unsigned int readonly_ : 1;

    /* are we dragging the current-line pointer? */
    unsigned int dragging_cur_line_ : 1;

    /* are we tracking the splitter? */
    unsigned int tracking_splitter_ : 1;

    /* flag: this is an untitled file */
    unsigned int untitled_ : 1;

    /* our splitter cursor */
    static HCURSOR split_csr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base HTML-panel subclass for debugger information windows - the stack
 *   window, the trace history window, and the doc search results window.
 *   This type of window is almost the same as the basic HTML panel class,
 *   but has some additional features, such as the ability to show icons in
 *   the margin (for the stack window's context indicators, for example).
 */
class CHtmlSysWin_win32_dbginfo: public CHtmlSysWin_win32_tdb
{
public:
    CHtmlSysWin_win32_dbginfo(class CHtmlFormatter *formatter,
                              class CHtmlSysWin_win32_owner *owner,
                              int has_vscroll, int has_hscroll,
                              class CHtmlPreferences *prefs,
                              class CHtmlDebugHelper *helper,
                              class CHtmlSys_dbgmain *dbgmain);

    ~CHtmlSysWin_win32_dbginfo();

    /* draw a debugger source line icon */
    void draw_dbgsrc_icon(const CHtmlRect *pos, unsigned int stat);

    /* measure a debugger source window margin icon */
    CHtmlPoint measure_dbgsrc_icon();

    /* set the background cursor */
    virtual int do_setcursor_bkg();

    /* note a change in the debugger color/font preferences */
    virtual void note_debug_format_changes(
        HTML_color_t bkg_color, HTML_color_t text_color,
        int use_windows_colors, int use_windows_bgcolor,
        HTML_color_t sel_bkg_color, HTML_color_t sel_text_color,
        int use_windows_sel_colors, int use_windows_sel_bgcolor);

    /*
     *   ignore HTML-based color changes in source windows - we use our
     *   own direct color settings
     */
    void set_html_bg_color(HTML_color_t, int) { }
    void set_html_text_color(HTML_color_t, int) { }

    /* handle keystrokes */
    int do_keydown(int virtual_key, long keydata);

    /* handle an arrow keystroke */
    unsigned long do_keydown_arrow(int vkey, unsigned long caret_pos,
                                   int cnt);

    /* receive notification that the parent is being destroyed */
    void on_destroy_parent()
    {
        /* inherit default */
        CHtmlSysWin_win32_tdb::on_destroy_parent();

        /*
         *   forget my formatter - the debug helper will delete the
         *   formatter as soon as it receives notification that the source
         *   window is being destroyed, so we need to forget about the
         *   formatter when we hear about it
         */
        formatter_ = 0;
    }

protected:
    /*
     *   calculate the icon size based on the current font, if we haven't
     *   already done so
     */
    void calc_icon_size();

    /* process a mouse click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* process a right mouse click */
    int do_rightbtn_down(int keys, int x, int y, int clicks);

    /* process a common left/right click */
    int common_btn_down(int keys, int x, int y, int clicks);

    /* draw the margin background */
    virtual void draw_margin_bkg(const RECT *rc);

    /* normal (16x16) icons for debugger source windows */
    HICON ico_dbg_curline_;
    HICON ico_dbg_ctxline_;
    HICON ico_dbg_bp_;
    HICON ico_dbg_bpdis_;
    HICON ico_dbg_curbp_;
    HICON ico_dbg_curbpdis_;
    HICON ico_dbg_ctxbp_;
    HICON ico_dbg_ctxbpdis_;

    /* small (8x8) icons for debugger source windows */
    HICON smico_dbg_curline_;
    HICON smico_dbg_ctxline_;
    HICON smico_dbg_bp_;
    HICON smico_dbg_bpdis_;
    HICON smico_dbg_curbp_;
    HICON smico_dbg_curbpdis_;
    HICON smico_dbg_ctxbp_;
    HICON smico_dbg_ctxbpdis_;

    /* icon size */
    int icon_size_;
#define W32TDB_ICON_UNKNOWN   1                    /* font not yet measured */
#define W32TDB_ICON_SMALL     2                        /* small (8x8) icons */
#define W32TDB_ICON_NORMAL    3                     /* normal (16x16) icons */

    /*
     *   target column for up/down arrow operation - this is only valid
     *   after an up/down operation when there has been no invervening
     *   keystroke
     */
    int target_col_valid_ : 1;
    unsigned int target_col_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Base class for dockable frame windows (stack, doc search results,
 *   history)
 */
class CHtmlSys_dbgwin_dockable: public CHtmlSys_tdbwin
{
public:
    CHtmlSys_dbgwin_dockable(class CHtmlParser *parser,
                             class CHtmlPreferences *prefs,
                             class CHtmlDebugHelper *helper,
                             class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_tdbwin(parser, prefs, helper, dbgmain)
    {
    }

    /* this is a docking window - make it a child */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* bring the window to the front */
    void bring_owner_to_front()
    {
        /* this is a docking window - bring my parent to the front */
        ShowWindow(GetParent(handle_), SW_SHOW);
        BringWindowToTop(GetParent(handle_));

        /* inherit default */
        CHtmlSys_tdbwin::bring_owner_to_front();
    }

    /* close the owner window */
    int close_owner_window(int force);

protected:
    /* format my line/column information for statusline display */
    virtual void format_linecol(char *buf, size_t)
    {
        /* don't display a cursor position in the stack window */
        buf[0] = '\0';
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Project file search results frame window
 */
class CHtmlSys_filesearchwin: public CHtmlSys_dbgwin_dockable
{
public:
    CHtmlSys_filesearchwin(class CHtmlParser *parser,
                          class CHtmlPreferences *prefs,
                          class CHtmlDebugHelper *helper,
                          class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgwin_dockable(parser, prefs, helper, dbgmain)
    {
    }

    /* process clicks on hyperlinks in the search window */
    virtual void process_command(const textchar_t *, size_t, int, int, int);

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   Project file search HTML panel subclass
 */
class CHtmlSysWin_win32_filesearch: public CHtmlSysWin_win32_tdb
{
public:
    CHtmlSysWin_win32_filesearch(class CHtmlFormatter *formatter,
                                 class CHtmlSysWin_win32_owner *owner,
                                 int has_vscroll, int has_hscroll,
                                 class CHtmlPreferences *prefs,
                                 class CHtmlDebugHelper *helper,
                                 class CHtmlSys_dbgmain *dbgmain);

    /* check the status of a command */
    TadsCmdStat_t active_check_command(const check_cmd_info *info);
    int active_do_command(int notify_code, int command_id, HWND ctl);

    /* begin a new search - clear the window if applicable */
    void begin_search();

    /* end a search - position the window to the new search results */
    void end_search();

    /* configuration path and title */
    virtual const textchar_t *idsw_get_cfg_path() { return ":filesearch"; }
    virtual const textchar_t *idsw_get_cfg_title() { return ""; }

protected:
    /* clear the window */
    void clear_window();

    /* do we auto-clear the window on each new search? */
    int auto_clear_;

    /* scroll position to set at the end of the search */
    long end_search_scroll_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Doc Search Results frame window
 */
class CHtmlSys_docsearchwin: public CHtmlSys_dbgwin_dockable
{
public:
    CHtmlSys_docsearchwin(class CHtmlParser *parser,
                          class CHtmlPreferences *prefs,
                          class CHtmlDebugHelper *helper,
                          class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgwin_dockable(parser, prefs, helper, dbgmain)
    {
    }

    /* process clicks on hyperlinks in the search window */
    virtual void process_command(const textchar_t *, size_t, int, int, int);

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

    /* process clicks on "file:", "more:" links */
    void process_file_command(const textchar_t *cmd, size_t len);
    void process_more_command(const textchar_t *cmd, size_t len);

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   Doc search HTML panel subclass
 */
class CHtmlSysWin_win32_docsearch: public CHtmlSysWin_win32_tdb
{
public:
    CHtmlSysWin_win32_docsearch(class CHtmlFormatter *formatter,
                                class CHtmlSysWin_win32_owner *owner,
                                int has_vscroll, int has_hscroll,
                                class CHtmlPreferences *prefs,
                                class CHtmlDebugHelper *helper,
                                class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSysWin_win32_tdb(formatter, owner, has_vscroll,
                                has_hscroll, prefs, helper, dbgmain)
    {
        /*
         *   do not automatically scroll - when we show new results, we want
         *   to keep the top results showing
         */
        auto_vscroll_ = FALSE;

        /* load my custom context menu */
        load_context_popup(IDR_DOCSEARCH_POPUP_MENU);
    }

    /* check the status of a command */
    TadsCmdStat_t active_check_command(const check_cmd_info *info)
    {
        /* inherit default */
        return CHtmlSysWin_win32_tdb::active_check_command(info);
    }

    /* execute a search */
    void execute_search(const textchar_t *query, int start_idx);

    /* configuration path and title */
    virtual const textchar_t *idsw_get_cfg_path() { return ":docsearch"; }
    virtual const textchar_t *idsw_get_cfg_title() { return ""; }

protected:
};

/* ------------------------------------------------------------------------ */
/*
 *   Stack window frame.  This is the same as a source window frame,
 *   except that it uses a stack panel.
 */
class CHtmlSys_dbgstkwin: public CHtmlSys_dbgwin_dockable
{
public:
    CHtmlSys_dbgstkwin(class CHtmlParser *parser,
                       class CHtmlPreferences *prefs,
                       class CHtmlDebugHelper *helper,
                       class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgwin_dockable(parser, prefs, helper, dbgmain)
    {
    }

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   HTML window subclass for debugger stack window.  This is the same as
 *   a source window, except for a slightly different display style.
 */
class CHtmlSysWin_win32_dbgstk: public CHtmlSysWin_win32_dbginfo
{
public:
    CHtmlSysWin_win32_dbgstk(class CHtmlFormatter *formatter,
                             class CHtmlSysWin_win32_owner *owner,
                             int has_vscroll, int has_hscroll,
                             class CHtmlPreferences *prefs,
                             class CHtmlDebugHelper *helper,
                             class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSysWin_win32_dbginfo(formatter, owner, has_vscroll,
                                    has_hscroll, prefs, helper, dbgmain)
    {
        /*
         *   do not automatically scroll - when we show new results, we want
         *   to keep the top results showing
         */
        auto_vscroll_ = FALSE;
    }

    virtual void do_create()
    {
        /* inherit default handling */
        CHtmlSysWin_win32_dbginfo::do_create();

        /*
         *   register as a drop target specifically to suppress drag/drop
         *   operations in this window
         */
        drop_target_register();
    }

    /* process a mouse click */
    int do_leftbtn_down(int keys, int x, int y, int clicks);

    /* check the status of a command routed from the main window */
    TadsCmdStat_t active_check_command(const check_cmd_info *);

    /* execute a command routed from the main window */
    int active_do_command(int notify_code, int command_id, HWND ctl);

    /* configuration path and title */
    virtual const textchar_t *idsw_get_cfg_path() { return ":stack"; }
    virtual const textchar_t *idsw_get_cfg_title() { return ""; }

protected:
    /*
     *   draw the margin background - we'll just leave it in the window's
     *   background color
     */
    virtual void draw_margin_bkg(const RECT *) { }
};


/* ------------------------------------------------------------------------ */
/*
 *   History window frame.  This is the based on the source window frame,
 *   but has some differences for call history.
 */
class CHtmlSys_dbghistwin: public CHtmlSys_dbgwin_dockable
{
public:
    CHtmlSys_dbghistwin(class CHtmlParser *parser,
                        class CHtmlPreferences *prefs,
                        class CHtmlDebugHelper *helper,
                        class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSys_dbgwin_dockable(parser, prefs, helper, dbgmain)
    {
    }

protected:
    /* store the window position in the preferences */
    void store_winpos_prefs(const CHtmlRect *pos);

private:
    /* create the HTML panel - called during initialization */
    virtual void create_html_panel(class CHtmlFormatter *formatter);
};

/*
 *   HTML window subclass for history trace window.  This is the same as a
 *   source window, except for a slightly different display style.
 */
class CHtmlSysWin_win32_dbghist: public CHtmlSysWin_win32_dbginfo
{
public:
    CHtmlSysWin_win32_dbghist(class CHtmlFormatter *formatter,
                              class CHtmlSysWin_win32_owner *owner,
                              int has_vscroll, int has_hscroll,
                              class CHtmlPreferences *prefs,
                              class CHtmlDebugHelper *helper,
                              class CHtmlSys_dbgmain *dbgmain)
        : CHtmlSysWin_win32_dbginfo(formatter, owner, has_vscroll,
                                    has_hscroll, prefs, helper, dbgmain)
    {
    }

    /* configuration path and title */
    virtual const textchar_t *idsw_get_cfg_path() { return ":trace"; }
    virtual const textchar_t *idsw_get_cfg_title() { return ""; }

protected:
    /*
     *   draw the margin background - we'll just leave it in the window's
     *   background color
     */
    virtual void draw_margin_bkg(const RECT *) { }
};


/* ------------------------------------------------------------------------ */
/*
 *   Dialog for the debugger about box
 */

class CTadsDlg_dbgabout: public CTadsDialog
{
public:
    /* initialize */
    void init();
};

/* ------------------------------------------------------------------------ */
/*
 *   Dialog for waiting for the build to terminate
 */
class CTadsDlg_buildwait: public CTadsDialog
{
public:
    CTadsDlg_buildwait(HANDLE comp_hproc)
    {
        /* remember the compiler's process handle */
        comp_hproc_ = comp_hproc;
    }

    /* handle a command */
    virtual int do_command(WPARAM id, HWND ctl);

private:
    /* process handle of the compiler subprocess */
    HANDLE comp_hproc_;
};

/* ------------------------------------------------------------------------ */
/*
 *   special CStringBuf subclass, to add a little functionality for
 *   convenience in building command lines
 */
class CStringBufCmd: public CStringBuf
{
public:
    CStringBufCmd(size_t siz) : CStringBuf(siz) { }

    /* append a string, adding double quotes around the string */
    void append_qu(const textchar_t *str, int leading_space)
    {
        size_t len;

        /* add a optional leading space and the quote */
        append_inc(leading_space ? " \"" : "\"", 512);

        /* add the string */
        append_inc(str, 512);

        /*
         *   If the last character of the string is a backslash, add an extra
         *   backslash.  The VC++ argv parsing rules are a bit weird and
         *   inconsistent: a backslash quotes a quote, or quotes a backslash,
         *   but only if the double-backslash is immediately before a quote.
         *   Backslashes aren't needed to quote backslashes elsewhere - only
         *   before quotes.  So, to avoid having the backslash quote the
         *   quote, quote the backslash.
         */
        if ((len = strlen(str)) != 0 && str[len-1] == '\\')
            append_inc("\\", 512);

        /* add the closing quote */
        append_inc("\"", 512);
    }

    /*
     *   Append an option an and argument.  We place the argument in
     *   quotes, and add a leading space before the option specifier and
     *   another between the option specifier and its argument.
     */
    void append_opt(const textchar_t *opt, const textchar_t *arg)
    {
        /* append the leading space */
        append_inc(" ", 512);

        /* append the option */
        append_inc(opt, 512);

        /* append the argument, in quotes, with a leading space */
        append_qu(arg, TRUE);
    }

    /* append with formatting */
    void appendf(const char *fmt, ...)
    {
        va_list argp;
        va_start(argp, fmt);
        vappendf(fmt, argp);
        va_end(argp);
    }
    void appendf_inc(int alloc_len, const char *fmt, ...)
    {
        va_list argp;
        va_start(argp, fmt);
        vappendf(fmt, argp, alloc_len);
        va_end(argp);
    }
    void vappendf(const char *fmt, va_list argp, int alloc_len = 0)
    {
        va_list orig_argp = argp;

        /* calculate the amount of space we need */
        int plen = _vscprintf(fmt, argp);

        /* if that worked, format the data */
        if (plen >= 0)
        {
            /* reserve the required space */
            ensure_added_size(plen, alloc_len);

            /* format the text */
            size_t idx = slen_;
            plen = _vsnprintf(buf_ + idx, len_ - idx, fmt, orig_argp);

            /* if that worked, advance the buffer pointers */
            if (plen >= 0)
                idx += plen;

            /* null-terminate at the new length */
            buf_[idx] = '\0';
            slen_ = idx;
        }
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Command iterator.  This iterates through all of the built-in and
 *   extension-defined commands.
 */
class twb_command_iter
{
public:
    twb_command_iter(class CHtmlSys_dbgmain *dbgmain)
    {
        dbgmain_ = dbgmain;
        reset();
    }

    /* get the first command */
    const HtmlDbgCommandInfo *first()
    {
        reset();
        return next();
    }

    /* reset to the start of the list */
    void reset()
    {
        predef_idx_ = 0;
        cur_ = dbgmain_->ext_commands_;
    }

    /* get the next command */
    const HtmlDbgCommandInfo *next()
    {
        const HtmlDbgCommandInfo *ret;

        /* if we're still in the predef list, return the next entry */
        if (dbgmain_->command_list_[predef_idx_].sym != 0)
            return &dbgmain_->command_list_[predef_idx_++];

        /* otherwise, return the next extension list item */
        if ((ret = cur_) != 0)
            cur_ = cur_->nxt_;

        /* return the result */
        return ret;
    }

    CHtmlSys_dbgmain *dbgmain_;
    int predef_idx_;
    const HtmlDbgCommandInfoExt *cur_;
};

/* ------------------------------------------------------------------------ */
/*
 *   editor mode iterator
 */
class twb_editor_mode_iter
{
public:
    twb_editor_mode_iter(class CHtmlSys_dbgmain *dbgmain)
    {
        dbgmain_ = dbgmain;
        mode_ = 0;
    }

    ~twb_editor_mode_iter()
    {
        /* if we have a mode left over from the iteration, release it */
        if (mode_ != 0)
            mode_->Release();
    }

    /* get the current mode */
    ITadsWorkbenchEditorMode *mode() { return mode_; }

    /* get the first mode; returns null if not found */
    ITadsWorkbenchEditorMode *first()
    {
        /* start at the start of the first extension */
        mode_ = 0;
        ext_ = dbgmain_->extensions_;
        idx_ = 0;

        /* get the next mode */
        return next();
    }

    /* get the next mode */
    ITadsWorkbenchEditorMode *next()
    {
        ITadsWorkbenchEditorMode *mode = 0;

        /* keep going until we find a mode or run out of options */
        while (ext_ != 0)
        {
            /* ask for the next mode */
            if (ext_->iext != 0
                && (mode = ext_->iext->GetMode(idx_++)) != 0)
                break;

            /* move on to the next extension and keep trying */
            ext_ = ext_->nxt;
            idx_ = 0;
        }

        /* release the previous mode */
        if (mode_ != 0)
            mode_->Release();

        /*
         *   store the new mode; note that no AddRef is needed, as we got a
         *   reference via GetMode()
         */
        mode_ = mode;

        /* return the new mode */
        return mode;
    }

    /* find a mode by name */
    static ITadsWorkbenchEditorMode *find_by_name(
        CHtmlSys_dbgmain *dbgmain, const textchar_t *name)
    {
        twb_editor_mode_iter iter(dbgmain);
        ITadsWorkbenchEditorMode *mode;

        /* search for the name */
        for (mode = iter.first() ; mode != 0 ; mode = iter.next())
        {
            /* get this mode's name */
            BSTR b = mode->GetModeName();
            if (b != 0)
            {
                int match;

                /* compare the name */
                textchar_t *s = ansi_from_bstr(b, TRUE);
                match = (strcmp(s, name) == 0);
                th_free(s);

                /* if we matched, return this mode */
                if (match)
                {
                    mode->AddRef();
                    return mode;
                }
            }
        }

        /* not found */
        return 0;
    }

private:
    /* our dbgmain object */
    class CHtmlSys_dbgmain *dbgmain_;

    /* the current mode descriptor */
    ITadsWorkbenchEditorMode *mode_;

    /* the current extension we're checking, and the mode index it */
    struct twb_extension *ext_;
    int idx_;
};


#endif /* W32TDB_H */

