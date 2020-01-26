/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32webui.cpp - TADS Web UI window
Function

Notes

Modified
  05/12/10 MJRoberts  - Creation
*/

#include <WinSock2.h>
#include <Windows.h>
#include <exdisp.h>
#include <oleauto.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <MsHtmcid.h>
#include <ctype.h>
#include <htmlhelp.h>
#include <DocObj.h>
#include <Shobjidl.h>
#include <string.h>

#include "tadshtml.h"
#include "tadsapp.h"
#include "w32webui.h"
#include "htmlw32.h"
#include "htmlpref.h"
#include "tadscom.h"
#include "htmlres.h"
#include "tadswebctl.h"
#include "tadsmidi.h"
#include "w32main.h"
#include "tadsstat.h"
#include "tadshtmldlg.h"

#include "t3std.h"
#include "osifc.h"
#include "osifcnet.h"
#include "osnet-comm.h"


/* ------------------------------------------------------------------------ */
/*
 *   forward declarations
 */
static DWORD get_olecmd(int command_id, OLECMDEXECOPT *optp);

/*
 *   globals
 */
static HINSTANCE G_hinstance;
static char default_saved_game_ext[OSFNMAX];
static char S_open_file_dir[OSFNMAX];
static char G_os_gamename[OSFNMAX];

/* ------------------------------------------------------------------------ */
/*
 *   Private messages
 */
#define WEBM_CONNECT (WM_USER + 1)

#define NWMF_USERINITED  0x0002
#define NWMF_FIRST       0x0004


/* ------------------------------------------------------------------------ */
/*
 *   Pipe message processor
 */
class WebUICommThreadChi: public WebUICommThread
{
public:
    WebUICommThreadChi(HANDLE proc, HANDLE pipe, CHtmlSys_webuiwin *win)
        : WebUICommThread(proc, pipe)
    {
        /* remember the window object */
        (this->win = win)->AddRef();
    }

    ~WebUICommThreadChi()
    {
        /* release the window object */
        win->Release();
    }

    void thread_main()
    {
        /* initialize OLE on this thread */
        OleInitialize(0);

        /* run the main thread */
        WebUICommThread::thread_main();

        /* done with OLE */
        OleUninitialize();
    }

    void process_request(int id, char *cmd)
    {
        /* check the request */
        if (memcmp(cmd, "connect ", 8) == 0)
        {
            /*
             *   Send the request to the main thread.  (The web browser
             *   control doesn't seem to be thread-safe - calling it from a
             *   separate thread pretty reliably crashes it, especially if
             *   the control is busy handling an earlier navigation request.)
             */
            SendMessage(win->get_handle(), WEBM_CONNECT,
                        0, (LPARAM)(cmd + 8));

            /* navigate to the new URL */
            //win->go_to_url(cmd + 8);
            send_reply(id, "ok");
        }
        else if (memcmp(cmd, "close", 5) == 0)
        {
            /* note that we've received a disconnect request */
            disconnecting = TRUE;

            /* close the UI window */
            SendMessage(win->get_handle(), WM_CLOSE, 0, 0);
        }
        else if (memcmp(cmd, "disconnect", 10) == 0)
        {
            /* note that we've received a disconnect request */
            disconnecting = TRUE;

            /* tell the UI window that we're disconnecting */
            win->on_server_disconnect();
        }
        else if (memcmp(cmd, "yield-fg ", 9) == 0)
        {
            /* yield the foreground to the indicated PID */
            tads_AllowSetForegroundWindow(atol(cmd + 9));
            send_reply(id, "ok");
        }
        else if (memcmp(cmd, "to-fg", 5) == 0)
        {
            /* set my window as the foreground window */
            SetForegroundWindow(win->get_handle());
            send_reply(id, "ok");
        }
        else if (memcmp(cmd, "tadsver ", 8) == 0)
        {
            /* save the TADS version string from the peer */
            win->set_tads_ver(cmd + 8);
        }
        else if (memcmp(cmd, "gamedir ", 8) == 0)
        {
            /* set the initial file selector folder */
            oss_set_open_file_dir(cmd + 8);
        }
        else if (memcmp(cmd, "saveext ", 8) == 0)
        {
            /* set the custom saved game extension */
            lib_strcpy(default_saved_game_ext,
                       sizeof(default_saved_game_ext),
                       cmd + 8);
        }
        else if (memcmp(cmd, "askfile ", 8) == 0)
        {
            /* askfile request - go process it */
            char buf[OSFNMAX + 1];
            int ret = win->askfile(buf + 1, sizeof(buf) - 1, cmd + 8);

            /* add the return value as a digit at the start of the buffer */
            buf[0] = '0' + ret;

            /* send back the reply */
            send_reply(id, buf);
        }

        /* inherit the base class handling */
        WebUICommThread::process_request(id, cmd);
    }

    /* our main window */
    CHtmlSys_webuiwin *win;
};

/* pipe thread */
static WebUICommThreadChi *comm_thread = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Javascript "external" interface.  This provides access from javascript
 *   in the web page to functions in the CHtmlSys_webuiwin host window.
 */
class WebWinExt: public TadsDispatch
{
public:
    WebWinExt(CHtmlSys_webuiwin *win)
    {
        /* remember our window object */
        (win_ = win)->AddRef();
    }

    ~WebWinExt()
    {
        /* release our window object */
        win_->Release();
    }

    void IsConnected(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_BOOL;
        ret->boolVal = win_->is_connected() ? VARIANT_TRUE : VARIANT_FALSE;
    }

protected:
    /* the web window object */
    CHtmlSys_webuiwin *win_;

    TADSDISP_DECL_MAP;
};

TADSDISP_MAP_BEGIN(WebWinExt)
    TADSDISP_MAP(WebWinExt, IsConnected)
TADSDISP_MAP_END;


/* ------------------------------------------------------------------------ */
/*
 *   Host command target.  This allows the embedded MSHTML control to send
 *   commands to the enclosing application frame.
 */
class WebWinCmdTarg: public CBaseOle, public IOleCommandTarget
{
public:
    WebWinCmdTarg(CHtmlSys_webuiwin *win)
    {
        /* remember our window object */
        (win_ = win)->AddRef();
    }

    ~WebWinCmdTarg()
    {
        /* release our window object */
        win_->Release();
    }

    BaseIUnknown;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID FAR *obj)
    {
        /* we're just an IUnknown and an IOleCommandTarget */
        if (iid == IID_IUnknown || iid == IID_IOleCommandTarget)
        {
            *obj = (IOleCommandTarget *)this;
            AddRef();
            return S_OK;
        }

        /* we don't provide any other interfaces */
        *obj = 0;
        return E_NOINTERFACE;
    }

    /* get command status */
    HRESULT STDMETHODCALLTYPE QueryStatus(
        const GUID *group, ULONG ncmds, OLECMD *cmds, OLECMDTEXT *txt)
    {
        /* assume we won't provide a description for the command */
        if (txt != 0)
        {
            txt->cmdtextf = OLECMDTEXTF_NONE;
            txt->cwActual = 0;
        }

        /* if it's not the default command group, ignore it */
        if (group != 0)
            return OLECMDERR_E_UNKNOWNGROUP;

        /* make sure we have a command array */
        if (cmds == 0)
            return E_POINTER;

        /* success */
        return S_OK;
    }

    /* execute a command */
    HRESULT STDMETHODCALLTYPE Exec(
        const GUID *group, DWORD id, DWORD exec_opt,
        VARIANT *args, VARIANT *retval)
    {
        /* if it's not the default command group, ignore it */
        if (group != 0)
            return OLECMDERR_E_UNKNOWNGROUP;

        /* success */
        return S_OK;
    }

protected:
    /* our window object */
    CHtmlSys_webuiwin *win_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Get the initial window position from the preferences
 */
static void get_prefs_win_pos(
    RECT *rc, CHtmlPreferences *prefs, CHtmlSys_webuiwin *parent)
{
    /*
     *   Get the saved position from the preferences.  For the top-level
     *   window, use the main window position; for anything else, use the
     *   child window position.
     */
    const CHtmlRect *prefpos =
        (parent == 0 ? prefs->get_win_pos() : prefs->get_subwin_pos());

    /* set the rect, forcing any out-of-bounds values to defaults */
    if (prefpos == 0)
        SetRect(rc, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT);
    else
        SetRect(rc,
                prefpos->left < 0 ? CW_USEDEFAULT : prefpos->left,
                prefpos->top < 0 ? CW_USEDEFAULT : prefpos->top,
                (prefpos->right < 0 || prefpos->left < 0
                 ? CW_USEDEFAULT : prefpos->right),
                (prefpos->bottom < 0 || prefpos->top < 0
                 ? CW_USEDEFAULT : prefpos->bottom));

    /* if the window is off the screen, use the default sizes */
    RECT deskrc;
    GetWindowRect(GetDesktopWindow(), &deskrc);
    if (rc->left < deskrc.left || rc->right > deskrc.right
        || rc->top < deskrc.top || rc->bottom > deskrc.bottom
        || rc->right < 20 || rc->bottom < 20)
    {
        /* the saved values look invalid; use a default position */
        SetRect(rc, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Web UI window
 */

/*
 *   construction
 */
CHtmlSys_webuiwin::CHtmlSys_webuiwin(CHtmlSys_webuiwin *parent,
                                     class CHtmlPreferences *prefs)
{
    /* remember my parent window */
    if ((parent_ = parent) != 0)
        parent->AddRef();

    /* no children or siblings yet */
    first_child_ = next_sibling_ = 0;

    /* remember our prefs */
    prefs_ = prefs;

    /* no status line yet */
    statusline_ = 0;

    /* we haven't loaded a document yet */
    first_load_ok_ = FALSE;

    /* no disconnect message from the server yet */
    srv_disconnected_ = FALSE;

    /* the timer is running by default */
    timer_paused_ = FALSE;

    /* set the starting time for on-screen the timer */
    starting_ticks_ = GetTickCount();

    /* game isn't loaded yet */
    game_running_ = FALSE;

    /* no statusbar menu yet */
    statusline_menu_ = 0;
}

/*
 *   deletion
 */
CHtmlSys_webuiwin::~CHtmlSys_webuiwin()
{
    /* notify my parent that I'm closing */
    if (parent_ != 0)
    {
        parent_->on_close_child(this);
        parent_->Release();
    }

    /* done with our status line */
    if (statusline_ != 0)
        delete statusline_;
}

/*
 *   system window creation
 */
void CHtmlSys_webuiwin::do_create()
{
    /* inherit the default handling */
    CTadsWin::do_create();

    /* set our icon */
    HICON ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                 MAKEINTRESOURCE(IDI_EXE_ICON),
                                 IMAGE_ICON, 32, 32, 0);
    SendMessage(handle_, WM_SETICON, ICON_BIG, (LPARAM)ico);

    /* create our status line */
    statusline_ = new CTadsStatusline(this, TRUE, HTML_MAINWIN_ID_STATUS);
    statusline_->main_part()->register_source(this);

    /* create the 'external' interface */
    WebWinExt *ext = new WebWinExt(this);

    /* create our command target */
    WebWinCmdTarg *cmd_targ = new WebWinCmdTarg(this);

    /*
     *   Create our embedded browser control.  If we have a parent window, it
     *   means that we're handling a NewWindow2 or NewWindow3 event, in which
     *   case we're not supposed to show the control initially.  If we're the
     *   top-level window, it's our own control, so show it immediately.
     */
    embed_browser(
        parent_ == 0,
        handle_,
        parent_ == 0 ? IDR_WEBUI_POPUP : IDR_WEBUI_CHILD_POPUP,
        ext, cmd_targ);

    /*
     *   we don't need to keep a copy of the external interface or command
     *   target
     */
    ext->Release();
    cmd_targ->Release();

    /* set up a timer to watch for focus changes */
    focus_timer_ = alloc_timer_id();
    SetTimer(handle_, focus_timer_, 500, 0);

    /* if we're the top window, set up the statusline timer */
    if (parent_ == 0)
    {
        /* set up the status line layout */
        adjust_statusbar_layout();

        /* load the statusline popup menu */
        statusline_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                                    MAKEINTRESOURCE(IDR_STATUSBAR_POPUP));
    }
    else
    {
        /* child windows don't need the timer */
        statusline_menu_ = 0;
    }

    /* assume we don't have focus */
    has_focus_ = FALSE;
}

/*
 *   system window destruction
 */
void CHtmlSys_webuiwin::do_destroy()
{
    /* delete our browser control */
    unembed_browser();

    /* if we have any child windows, close them all */
    if (first_child_ != 0)
    {
        /* count the children */
        int i, cnt;
        CHtmlSys_webuiwin *chi;
        for (cnt = 0, chi = first_child_ ; chi != 0 ;
             chi = chi->next_sibling_, ++cnt) ;

        /* make a private list of children */
        CHtmlSys_webuiwin **chi_list = new CHtmlSys_webuiwin*[cnt];
        for (i = 0, chi = first_child_ ; chi != 0 ; chi = chi->next_sibling_)
            chi_list[i++] = chi;

        /* close each child */
        for (i = 0 ; i < cnt ; ++i)
            SendMessage(chi_list[i]->get_handle(), WM_CLOSE, 0, 0);

        /* done with the child list */
        delete [] chi_list;
    }

    /* delete our timer */
    KillTimer(handle_, focus_timer_);
    free_timer_id(focus_timer_);

    /* if we're the top-level window, shut down */
    int post_quit = FALSE;
    if (parent_ == 0)
    {
        /* done with the statusline popup menu */
        DestroyMenu(statusline_menu_);

        /* disconnect the VM communicator thread */
        if (comm_thread != 0)
            comm_thread->disconnect(FALSE);

        /* make a note to post the 'quit' message */
        post_quit = TRUE;
    }

    /* do the inherited work */
    CTadsWin::do_destroy();

    /* if we decided to post the Quit message to close the UI, do so */
    if (post_quit)
        PostQuitMessage(0);
}

/*
 *   check command status
 */
TadsCmdStat_t CHtmlSys_webuiwin::check_command(const check_cmd_info *info)
{
    OLECMD olecmd;

    /* check for our own commands before any OLE translation */
    switch (info->command_id)
    {
    case ID_FILE_PAGE_SETUP:
        /*
         *   don't allow this - we don't have a way to pass the page setup
         *   information to the browser control, so make it clear that our
         *   page setup settings don't apply to the this window by disabling
         *   the dialog entirely
         */
        return TADSCMD_DISABLED;

    case ID_VIEW_TIMER:
        return prefs_->get_show_timer() ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_TIMER_PAUSE:
        return (game_running_
                ? (timer_paused_ ? TADSCMD_CHECKED : TADSCMD_ENABLED)
                : TADSCMD_DISABLED);

    case ID_TIMER_RESET:
        return (game_running_ ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_VIEW_TIMER_HHMMSS:
        return (prefs_->get_show_timer()
                ? (prefs_->get_show_timer_seconds()
                   ? TADSCMD_CHECKED : TADSCMD_ENABLED)
                : TADSCMD_DISABLED);

    case ID_VIEW_TIMER_HHMM:
        return (prefs_->get_show_timer()
                ? (prefs_->get_show_timer_seconds()
                   ? TADSCMD_ENABLED : TADSCMD_CHECKED)
                : TADSCMD_DISABLED);

    case ID_FILE_EXIT:
    case ID_HELP_ABOUT:
    // case ID_HELP_CONTENTS:
        /* these are always enabled */
        return TADSCMD_ENABLED;
    }

    /* check for a translation to an OLE command */
    if (icmd_ != 0
        && (olecmd.cmdID = get_olecmd(info->command_id, 0)) != IDM_UNKNOWN
        && SUCCEEDED(icmd_->QueryStatus(&CGID_MSHTML, 1, &olecmd, 0)))
    {
        /* return the OLE command status */
        if ((olecmd.cmdf & OLECMDF_LATCHED) != 0)
            return ((olecmd.cmdf & OLECMDF_ENABLED) != 0
                    ? TADSCMD_CHECKED : TADSCMD_DISABLED_CHECKED);
        else
            return ((olecmd.cmdf & OLECMDF_ENABLED) != 0
                    ? TADSCMD_ENABLED : TADSCMD_DISABLED);
    }

    /* inherit the default behavior */
    return CTadsWin::check_command(info);
}

/*
 *   About Box arguments
 */
class AboutBoxArgs: public TadsDispatch
{
public:
    AboutBoxArgs(CHtmlSys_webuiwin *win)
    {
        (win_ = win)->AddRef();
    }

    ~AboutBoxArgs()
    {
        win_->Release();
    }

    void tadsVersion(DISPPARAMS *params, VARIANT *ret)
    {
        const char *vsn = win_->get_tads_ver();
        if (vsn != 0)
        {
            ret->vt = VT_BSTR;
            ret->bstrVal = bstr_from_ansi(vsn);
        }
        else
        {
            ret->vt = VT_NULL;
        }
    }

    CHtmlSys_webuiwin *win_;

    TADSDISP_DECL_MAP;
};

/* dispatch map for AboutBox arguments */
TADSDISP_MAP_BEGIN(AboutBoxArgs)
    TADSDISP_MAP(AboutBoxArgs, tadsVersion)
TADSDISP_MAP_END;


/*
 *   execute a command
 */
int CHtmlSys_webuiwin::do_command(int notify_code, int command_id, HWND ctl)
{
    DWORD olecmd;
    OLECMDEXECOPT oleopt;

    /* check for commands we implement directly */
    switch (command_id)
    {
    case ID_FILE_EXIT:
        /* terminate by closing our window */
        SendMessage(handle_, WM_CLOSE, 0, 0);
        return TRUE;

#if 0 // omit help
        // The help file is essentially worthless, so I've decided
        // to just leave it out.  I'm leaving the skeleton code in,
        // with ifdefs, in case I ever add useful content to
        // the help file in the future.
    case ID_HELP_CONTENTS:

        /* show help */
        {
            char buf[OSFNMAX];

            /* build the help file's name */
            GetModuleFileName(CTadsApp::get_app()->get_instance(),
                              buf, sizeof(buf));
            strcpy(os_get_root_name(buf), "tadsweb.chm::/overview.htm");

            /* show the help */
            HtmlHelp(handle_, buf, HH_DISPLAY_TOPIC, 0);
        }
        return TRUE;
#endif // omit help

    case ID_HELP_ABOUT:
        /* show the about box */
        {
            /* create our arguments */
            AboutBoxArgs *args = new AboutBoxArgs(this);

            /* show the dialog */
            modal_html_dialog(handle_, "aboutbox.htm",
                              "478px", "279px", FALSE, args);

            /* done with our arguments */
            args->Release();
        }
        return TRUE;

    case ID_VIEW_TIMER:
        /* invert the timer visibility in the preferences */
        prefs_->set_show_timer(!prefs_->get_show_timer());

        /* adjust the status display accordingly */
        adjust_statusbar_layout();

        /* handled */
        return TRUE;

    case ID_TIMER_RESET:
        /* reset the timer to the current time */
        starting_ticks_ = GetTickCount();

        /* in case we're paused, reset the pause starting time as well */
        pause_starting_ticks_ = GetTickCount();

        /* handled */
        return TRUE;

    case ID_TIMER_PAUSE:
        /* invert the current pause status */
        timer_paused_ = !timer_paused_;

        /* are we entering or leaving a pause? */
        if (timer_paused_)
        {
            /*
             *   entering a pause - note the time when the pause started;
             *   this will effectively freeze the timer here, and will tell
             *   us where to back-date the timer when we come out of the
             *   pause
             */
            pause_starting_ticks_ = GetTickCount();
        }
        else
        {
            /*
             *   leaving a pause - reset the starting position of the timer
             *   so that subsequent interval calculations will omit the time
             *   we were paused
             */
            starting_ticks_ = GetTickCount()
                              - (pause_starting_ticks_ - starting_ticks_);
        }

        /* handled */
        return TRUE;

    case ID_VIEW_TIMER_HHMMSS:
        /* turn on seconds in the timer */
        prefs_->set_show_timer_seconds(TRUE);

        /* handled */
        return TRUE;

    case ID_VIEW_TIMER_HHMM:
        /* turn off seconds in the timer */
        prefs_->set_show_timer_seconds(FALSE);

        /* handled */
        return TRUE;
    }

    /* check for translations to OLECMDID codes */
    if (icmd_ != 0
        && (olecmd = get_olecmd(command_id, &oleopt)) != IDM_UNKNOWN)
    {
        /* send the OLE command */
        icmd_->Exec(&CGID_MSHTML, olecmd, oleopt, 0, 0);

        /* handled */
        return TRUE;
    }

    /* inherit the default behavior */
    return CTadsWin::do_command(notify_code, command_id, ctl);
}

/*
 *   translate an HTML TADS ID_xxx command to an OLE command ID
 */
static DWORD get_olecmd(int command_id, OLECMDEXECOPT *optp)
{
    OLECMDEXECOPT opt = OLECMDEXECOPT_DONTPROMPTUSER;
    DWORD oleid = IDM_UNKNOWN;

    switch (command_id)
    {
    case ID_HELP_REFRESH:
        oleid = IDM_REFRESH;
        break;

    case ID_EDIT_COPY:
        oleid = IDM_COPY;
        break;

    case ID_EDIT_PASTE:
        oleid = IDM_PASTE;
        break;

    case ID_EDIT_CUT:
        oleid = IDM_CUT;
        break;

    case ID_EDIT_SELECTALL:
        oleid = IDM_SELECTALL;
        break;

    case ID_EDIT_FIND:
        oleid = IDM_FIND;
        opt = OLECMDEXECOPT_PROMPTUSER;
        break;

    case ID_EDIT_FINDNEXT:
        /* find next - do a Find without prompting the user */
        oleid = IDM_FIND;
        break;

    case ID_EDIT_UNDO:
        oleid = IDM_UNDO;
        break;

    case ID_EDIT_DELETE:
        oleid = IDM_DELETE;
        break;

    case ID_FILE_PRINT:
        oleid = IDM_PRINT;
        opt = OLECMDEXECOPT_PROMPTUSER;
        break;
    }

    /* return the options, if desired */
    if (optp != 0)
        *optp = opt;

    /* return the OLE command ID */
    return oleid;
}

/*
 *   Adjust the status bar layout
 */
void CHtmlSys_webuiwin::adjust_statusbar_layout()
{
    /* proceed only if we have a status line */
    if (statusline_ != 0)
    {
        int widths[2];
        int parts;

        /* assume only one part */
        parts = 1;

        /* assume the first part gets the whole display */
        widths[0] = -1;

        /* add the timer, if desired */
        if (parent_ == 0 && prefs_->get_show_timer())
        {
            RECT cli_rc;
            HGDIOBJ oldfont;
            SIZE txtsiz;
            HDC dc;

            /* count the timer in the parts list */
            ++parts;

            /* get the available area */
            GetClientRect(handle_, &cli_rc);

            /* get the desktop dc */
            dc = GetDC(handle_);

            /* calculate the width we need for the time panel */
            oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            ht_GetTextExtentPoint32(dc, "X00:00:00X", 10, &txtsiz);

            /* include the scrollbar size */
            txtsiz.cx += GetSystemMetrics(SM_CXVSCROLL);

            /* restore drawing context */
            SelectObject(dc, oldfont);
            ReleaseDC(handle_, dc);

            /*
             *   add the timer to the widths - give the timer a fixed portion
             *   at the right large enough to show the time, and give
             *   everything else to the preceding parts
             */
            widths[0] = cli_rc.right - txtsiz.cx;
            widths[1] = -1;
        }

        /* reset the panel layout */
        SendMessage(statusline_->get_handle(), SB_SETPARTS,
                    parts, (LPARAM)widths);
    }
}

/*
 *   Open a new window
 */
void CHtmlSys_webuiwin::open_new_window(
    VARIANT_BOOL *cancel, IDispatch **disp,
    const char *url, const char *src_url, enum NWMF flags)
{
    /*
     *   if we're opening the window in response to a user click rather than
     *   a scripting action, open an actual browser window
     */
    if ((flags & (NWMF_USERINITED | NWMF_FIRST))
        == (NWMF_USERINITED | NWMF_FIRST))
        return;

    /* create the Web UI window object */
    CHtmlSys_webuiwin *win = new CHtmlSys_webuiwin(this, prefs_);

    /* get the subwindow position from the preferences */
    RECT rc;
    get_prefs_win_pos(&rc, prefs_, this);

    /*
     *   if this isn't our first child, use defaults for the top left, so
     *   that it doesn't exactly overlap the current child window
     */
    if (first_child_ != 0)
        rc.top = rc.left = CW_USEDEFAULT;

    /* link it into our child list */
    win->next_sibling_ = first_child_;
    first_child_ = win;

    /*
     *   create the system window - make it initially invisible, so that the
     *   browser can display it after getting it set up
     */
    win->create_system_window(0, FALSE, "TADS", &rc);

    /* get the new window's embedded browser control */
    IWebBrowser2 *wb2 = win->get_browser_ctl();
    if (wb2 != 0)
    {
        /* get its IDispatch */
        wb2->QueryInterface(IID_IDispatch, (void **)disp);

        /* done with our reference */
        wb2->Release();
    }
}

/*
 *   receive notification that we're closing a child window
 */
void CHtmlSys_webuiwin::on_close_child(CHtmlSys_webuiwin *chi)
{
    /* find it in our child list */
    CHtmlSys_webuiwin *cur, *prv;
    for (cur = first_child_, prv = 0 ; cur != 0 ; cur = cur->next_sibling_)
    {
        /* if this is the child we're removing, unlink it */
        if (cur == chi)
        {
            /* unlink it from the list */
            if (prv != 0)
                prv->next_sibling_ = cur->next_sibling_;
            else
                first_child_ = next_sibling_;

            /* release our list reference on it */
            chi->Release();

            /* we're done */
            return;
        }
    }
}

/*
 *   handle a non-client left click
 */
int CHtmlSys_webuiwin::do_nc_leftbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* proceed to the standard system handling */
    return FALSE;
}


/*
 *   handle a non-client right-click
 */
int CHtmlSys_webuiwin::do_nc_rightbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* proceed to the standard system handling */
    return FALSE;
}

/*
 *   move
 */
void CHtmlSys_webuiwin::do_move(int x, int y)
{
    /* remember the change in the preferences */
    store_winpos_prefs();
}

/*
 *   resize
 */
void CHtmlSys_webuiwin::do_resize(int mode, int x, int y)
{
    RECT statrc;

    switch (mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother recalculating the layout on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        statrc.bottom = 0;
        if (statusline_ != 0)
        {
            /* notify the statusline of the change */
            statusline_->notify_parent_resize();

            /* get the statusline size */
            GetClientRect(statusline_->get_handle(), &statrc);
        }

        /* resize the html control to match */
        if (iwb2_ != 0)
            resize_browser_ctl(-1, -1, x, y - statrc.bottom);

        /* adjust the statusbar layout for the resize */
        adjust_statusbar_layout();

        /* remember the change in the preferences */
        store_winpos_prefs();

        /* handled */
        break;
    }
}

/*
 *   handle a user message
 */
int CHtmlSys_webuiwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    switch (msg)
    {
    case WEBM_CONNECT:
        /* connect to new URL - the URL string is in the lparam */
        go_to_url((const char *)lpar);

        /* if wpar is TRUE, it means we're supposed to free the buffer */
        if (wpar)
            th_free((char *)lpar);

        /* handled */
        return TRUE;

    default:
        break;
    }

    /* inherit the default handling */
    return CTadsWin::do_user_message(msg, wpar, lpar);
}

/*
 *   Store my position in the preferences.  This is invoked from our
 *   self-posted follow-up message, which we post during a size/move event
 *   for deferred processing after the triggering event has finished.
 */
void CHtmlSys_webuiwin::store_winpos_prefs()
{
    /* get the window rectangle */
    RECT rc;
    GetWindowRect(handle_, &rc);

    /* save it in the preferences */
    CHtmlRect hrc;
    hrc.set(rc.left, rc.top, rc.right, rc.bottom);
    if (parent_ == 0)
    {
        /* we're the top window - save it as the main window position */
        prefs_->set_win_pos(&hrc);
    }
    else
    {
        /* we're a child windiow - save as the child window position */
        prefs_->set_subwin_pos(&hrc);
    }

    /* write the preferences */
    prefs_->save();
}

/*
 *   handle notifications
 */
int CHtmlSys_webuiwin::do_notify(
    int control_id, int notify_code, LPNMHDR nm)
{
    int new_focus;

    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* check which it is */
        new_focus = (notify_code == NM_SETFOCUS);

        /* note the new status */
        has_focus_ = new_focus;

        /* inherit the default handling as well */
        break;

    case NM_RCLICK:
        /* right click in a control - check the status bar */
        if (statusline_ != 0 && nm->hwndFrom == statusline_->get_handle())
        {
            /* get the mouse position */
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(handle_, &pt);

            /* run the statusline popup menu */
            track_context_menu(GetSubMenu(statusline_menu_, 0), pt.x, pt.y);

            /* handled */
            return TRUE;
        }

        /* no special handling */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CTadsWin::do_notify(control_id, notify_code, nm);
}

/*
 *   handle non-client activation
 */
int CHtmlSys_webuiwin::do_ncactivate(int flag)
{
    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   gain focus
 */
void CHtmlSys_webuiwin::do_setfocus(HWND prv)
{
    IDispatch *doc;

    /* inherit default */
    CTadsWin::do_setfocus(prv);

    /* set focus on the html document */
    if (iwb2_ != 0 && SUCCEEDED(iwb2_->get_Document(&doc)) && doc != 0)
    {
        IHTMLDocument4 *doc4;

        /* get the IHtmlDocument4 interface from the document */
        if (SUCCEEDED(doc->QueryInterface(
            IID_IHTMLDocument4, (void **)&doc4)))
        {
            /* set focus on the document */
            doc4->focus();

            /* done with the doc4 */
            doc4->Release();
        }

        /* done with the document */
        doc->Release();
    }

    /* note the new status */
    has_focus_ = TRUE;
}

/*
 *   lose focus
 */
void CHtmlSys_webuiwin::do_killfocus(HWND nxt)
{
    /* note the new status */
    has_focus_ = FALSE;

    /* inherit default */
    CTadsWin::do_killfocus(nxt);
}

/*
 *   handle timers
 */
int CHtmlSys_webuiwin::do_timer(int timer_id)
{
    /* check for our focus timer */
    if (timer_id == focus_timer_)
    {
        int new_focus;

        /* check to see if focus is in this window or a child */
        HWND f = GetFocus();
        new_focus = (f == handle_ || IsChild(handle_, f));

        /* check to see if this is changed from last time */
        if (new_focus != has_focus_)
        {
            if (new_focus)
                do_setfocus(0);
            else
                do_killfocus(0);
        }

        /* while we're here, also update the statusline timer, if active */
        if (prefs_->get_show_timer() && statusline_ != 0 && game_running_)
        {
            char buf[128];
            DWORD elapsed;

            /* check to see if we're in a pause */
            if (timer_paused_)
            {
                /* get the elapsed time until the pause */
                elapsed = (pause_starting_ticks_ - starting_ticks_)/1000;
            }
            else
            {
                /* get the elapsed seconds since the start of the game */
                elapsed = (GetTickCount() - starting_ticks_)/1000;
            }

            /* format into hours, minutes, and seconds */
            if (prefs_->get_show_timer_seconds())
            {
                /* format the full display, with seconds */
                sprintf(buf, " %d:%02d:%02d",
                        (int)(elapsed / 3600),
                        (int)((elapsed % 3600) / 60),
                        (int)(elapsed % 60));
            }
            else
            {
                /* only include the hours and minutes */
                sprintf(buf, " %d:%02d",
                        (int)(elapsed / 3600),
                        (int)((elapsed % 3600) / 60));
            }

            /* set the text in panel 1 */
            SendMessage(statusline_->get_handle(),
                        SB_SETTEXT, 1, (LPARAM)buf);
        }
    }

    /* inherit default */
    return CTadsWin::do_timer(timer_id);
}

/*
 *   Statusline source interface: get the current status message
 */
textchar_t *CHtmlSys_webuiwin::get_status_message(int *caller_deletes)
{
    /* assume we'll hand back a static message */
    *caller_deletes = FALSE;

    /* check for a disconnect */
    if (srv_disconnected_)
        return "The game has ended.";

    /* if there's a status message from the control, display it */
    if (status_from_ctl_.get() != 0 && status_from_ctl_.get()[0] != '\0')
    {
        *caller_deletes = TRUE;
        return status_from_ctl_.get_copy();
    }

    /* we didn't find anything to display, so show a blank status line */
    return "";
}

/*
 *   Statusline source interface: select a menu item
 */
int CHtmlSys_webuiwin::do_status_menuselect(WPARAM wpar, LPARAM lpar)
{
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   dialog hook for standard file dialog - centers it on the screen
 */
static UINT APIENTRY filedlg_hook(HWND dlg, UINT msg,
                                  WPARAM wpar, LPARAM lpar)
{
    /* if this is the post-initialization message, center the dialog */
    if (msg == WM_NOTIFY
        && ((NMHDR *)lpar)->code == CDN_INITDONE)
    {
        RECT deskrc;
        RECT dlgrc;
        int deskwid, deskht;
        int dlgwid, dlght;

        /* get the desktop area */
        GetWindowRect(GetDesktopWindow(), &deskrc);
        deskwid = deskrc.right - deskrc.left;
        deskht = deskrc.bottom - deskrc.top;

        /* get the dialog box area */
        GetWindowRect(((NMHDR *)lpar)->hwndFrom, &dlgrc);
        dlgwid = dlgrc.right - dlgrc.left;
        dlght = dlgrc.bottom - dlgrc.top;

        /* center the dialog on the screen */
        MoveWindow(((NMHDR *)lpar)->hwndFrom,
                   deskrc.left + (deskwid - dlgwid)/2,
                   deskrc.top + (deskht - dlght)/2, dlgwid, dlght, TRUE);

        /* message handled */
        return 1;
    }

    /* not handled */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the initial directory for os_askfile dialogs
 */
void oss_set_open_file_dir(const char *dir)
{
    /*
     *   if there's a string, copy it to our buffer; otherwise, clear our
     *   buffer
     */
    if (dir != 0)
        strcpy(S_open_file_dir, dir);
    else
        S_open_file_dir[0] = '\0';
}

/* ------------------------------------------------------------------------ */
/*
 *   Show the file selection dialog
 */
static int _askfile(HWND parent, const char *prompt,
                    char *fname_buf, int fname_buf_len,
                    int prompt_type, int file_type)
{
    OPENFILENAME info;
    int ret;
    char filter[256];
    const char *def_ext;
    const char *save_def_ext;
    static char lastsave[OSFNMAX] = "";

    /* presume we won't need a default extension */
    def_ext = 0;
    save_def_ext = 0;

    /*
     *   Build the appropriate filter.  Most of the filters are boilerplate,
     *   but the "saved game" filter (for both TADS 2 and TADS 3 saved game
     *   files) is special because we need to use the dynamically-selected
     *   default saved game extension for our filter.  This means that the
     *   "unknown" type is equally complicated, because it needs to include
     *   the dynamic saved game extension as well.
     */
    if (file_type == OSFTSAVE
        || file_type == OSFTT3SAV
        || file_type == OSFTUNK)
    {
        static const char filter_start[] =
            "Saved Game Positions\0*.";
        static const char filter_end_sav[] =
            "All Files\0*.*\0\0";
        static const char filter_end_unk[] =
            "TADS Games\0*.GAM\0"
            "Text Files\0*.TXT\0"
            "Log Files\0*.LOG\0"
            "All Files\0*.*\0\0";
        size_t totlen;
        size_t extlen;
        const char *save_filter;

        /*
         *   Get the default saved game extension.  If the game program has
         *   explicitly set a custom extension, use that.  Otherwise, use the
         *   appropriate extension for the file type.
         */
        if (default_saved_game_ext[0] != '\0')
        {
            /*
             *   there's an explicit filter, so use it as the filter and as
             *   the default extension
             */
            save_filter = default_saved_game_ext;
            save_def_ext = default_saved_game_ext;
        }
        else if (file_type == OSFTSAVE)
        {
            /* tads 2 saved game, so use the ".sav" suffix */
            save_filter = "sav";
            save_def_ext = "sav";
        }
        else if (file_type == OSFTT3SAV)
        {
            /* tads 3 saved game, so use the ".t3v" suffix */
            save_filter = "t3v";
            save_def_ext = "t3v";
        }
        else
        {
            /*
             *   Unknown file type - use BOTH tads 2 and tads 3 saved game
             *   extensions for the filter, but do not apply any default
             *   extension to files we're creating.
             *
             *   Note that we will append this filter to "*.", so the first
             *   extension in our list doesn't need the "*.", but the second
             *   one does.
             */
            save_filter = "sav;*.t3v";
            save_def_ext = 0;
        }

        /*
         *   Build the filter string for saved game files.  Note that we
         *   need to use memcpy rather than strcpy, because we need to
         *   include a bunch of embedded null characters within the
         *   string.  First, copy the prefix up to the saved game
         *   extension...
         */
        memcpy(filter, filter_start, sizeof(filter_start) - 1);
        totlen = sizeof(filter_start) - 1;

        /* now add the saved game extension that we're to use... */
        extlen = strlen(save_filter) + 1;
        memcpy(filter + totlen, save_filter, extlen);
        totlen += extlen;

        /* finish with the whole rest of it */
        if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
        {
            /* saved game: use the saved game extra filter list */
            memcpy(filter + totlen, filter_end_sav, sizeof(filter_end_sav));

            /*
             *   if we're creating a new saved game file, apply the default
             *   suffix to the name the user enters for the new file
             */
            if (prompt_type == OS_AFP_SAVE)
                def_ext = save_def_ext;
        }
        else
        {
            /* not a saved game: use the unknown file type filter list */
            memcpy(filter + totlen, filter_end_unk, sizeof(filter_end_unk));
        }
    }
    else
    {
        static const struct
        {
            const char *disp;
            const char *pat;
            const char *def_ext;
        }
        filters[] =
        {
            { "TADS Games",      "*.gam", "gam" },              /* OSFTGAME */
            { 0, 0 },                                  /* unused - OSFTSAVE */
            { "Transcripts",     "*.log", "log" },               /* OSFTLOG */
            { "Swap Files",      "*.dat", "dat" },              /* OSFTSWAP */
            { "Data Files",      "*.dat", "dat" },              /* OSFTDATA */
            { "Command Scripts", "*.cmd", "cmd" },               /* OSFTCMD */
            { "Message Files",   "*.msg", "msg" },              /* OSFTERRS */
            { "Text Files",      "*.txt", "txt" },              /* OSFTTEXT */
            { "Binary Files",    "*.dat", "dat" },               /* OSFTBIN */
            { "Character Maps",  "*.tcp", "tcp" },              /* OSFTCMAP */
            { "Preference Files","*.dat", "dat" },              /* OSFTPREF */
            { 0, 0, 0 },                                /* unused - OSFTUNK */
            { "T3 Applications", "*.t3" /* was t3x */, "t3" }, /* OSFTT3IMG */
            { "T3 Object Files", "*.t3o", "t3o" },             /* OSFTT3OBJ */
            { "T3 Symbol Files", "*.t3s", "t3s" },             /* OSFTT3SYM */
            { 0, 0, 0 }                               /* unused - OSFTT3SAV */
        };
        static const char filter_end[] = "All Files\0*.*\0\0";
        char *p;

        /*
         *   We have an ordinary, static filter -- choose the appropriate
         *   filter from our list.  Start with the display name...
         */
        strcpy(filter, filters[file_type].disp);

        /* add the file filter pattern after the null character */
        p = filter + strlen(filter) + 1;
        strcpy(p, filters[file_type].pat);

        /* add the filter terminator after the null */
        p += strlen(p) + 1;
        memcpy(p, filter_end, sizeof(filter_end));

        /* remember the default extension */
        save_def_ext = def_ext = filters[file_type].def_ext;
    }

    /* set up the open-file structure */
    info.lStructSize = sizeof(info);
    info.hwndOwner = parent;
    info.hInstance = G_hinstance;
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname_buf;
    fname_buf[0] = '\0';
    info.nMaxFile = fname_buf_len;
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = (S_open_file_dir[0] != '\0' ? S_open_file_dir : 0);
    info.lpstrTitle = prompt;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = def_ext;
    info.lCustData = 0;
    info.lpfnHook = filedlg_hook;
    info.Flags = OFN_ENABLEHOOK | OFN_EXPLORER | OFN_NOCHANGEDIR;
    info.lpTemplateName = 0;

    /*
     *   If we're asking for a saved game file, pick a default.  If we've
     *   asked before, use the same default as last time; if not, use the
     *   game name as the default.  If we're opening a saved game rather then
     *   saving one, don't apply the default if the file we'd get by applying
     *   the default doesn't exist.
     */
    if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
    {
        /* try using the last saved game name */
        strncpy(fname_buf, lastsave, fname_buf_len);
        fname_buf[fname_buf_len - 1] = '\0';

        /* if that's empty, generate a default name based on the game name */
        if (fname_buf[0] == '\0' && G_os_gamename[0] != '\0')
        {
            /* copy the game name as the base of the save-file name */
            strncpy(fname_buf, G_os_gamename, fname_buf_len);
            fname_buf[fname_buf_len - 1] = '\0';

            /* remove the old extension */
            os_remext(fname_buf);

            /* ... and replace it with the saved-game extension */
            if (save_def_ext != 0
                && strlen(fname_buf) + strlen(save_def_ext) + 1
                   < (size_t)fname_buf_len)
                os_defext(fname_buf, save_def_ext);
        }

        /*
         *   if we're asking about a file to open, make sure our default
         *   exists - if not, don't offer it as the default, since it's a
         *   waste of time and attention for the user to have to change it
         */
        if (prompt_type == OS_AFP_OPEN && osfacc(fname_buf))
            fname_buf[0] = '\0';
    }

    /* display the dialog */
    if (prompt_type == OS_AFP_SAVE)
    {
        info.Flags |= OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
        ret = GetSaveFileName(&info);
    }
    else
    {
        /* ask for a file to open */
        ret = GetOpenFileName(&info);
    }

    /* translate the result code to an OS_AFE_xxx value */
    if (ret == 0)
    {
        /*
         *   an error occurred - check to see what happened: if the extended
         *   error is zero, it means that the user simply canceled the
         *   dialog, otherwise it means that an error occurred
         */
        ret = (CommDlgExtendedError() == 0 ? OS_AFE_CANCEL : OS_AFE_FAILURE);
    }
    else
    {
        char path[OSFNMAX];

        /* success */
        ret = OS_AFE_SUCCESS;

        /*
         *   remember the file's directory for the next file dialog (we need
         *   to do this explicitly because we're not allowing the dialog to
         *   set the working directory)
         */
        os_get_path_name(path, sizeof(path), fname_buf);
        oss_set_open_file_dir(path);

        /*
         *   if this is a saved game prompt, remember the response as the
         *   default for next time
         */
        if (file_type == OSFTSAVE || file_type == OSFTT3SAV)
            strcpy(lastsave, fname_buf);
    }

    /* return the result */
    return ret;
}

/*
 *   Show the file selection dialog.
 */
int CHtmlSys_webuiwin::askfile(char *buf, size_t buflen, char *params)
{
    /*
     *   Parse the parameters:
     *
     *.    prompt,promptType,fileType
     *
     *   The prompt string has special characters escaped with % signs, as
     *   follows.  Note that we use '%' rather than backslash just to avoid
     *   any confusion about actual C++ escape codes.  Any null characters in
     *   the prompt string should be deleted by the caller rather than
     *   escaped.
     *
     *.    %,   for comma
     *.    %%   for percent
     *
     *   The promptType and fileType are integer values represented in
     *   decimal.
     *
     *   For simplicity, we modify the parameter buffer in place.
     */

    /* process the prompt - translate escapes and find the end */
    char *src, *dst;
    char *prompt = params;
    for (src = dst = params ; *src != '\0' && *src != ',' ; ++src)
    {
        /* if it's an escape, remove the '%' */
        if (*src == '%' && *(src+1) != '\0')
            ++src;

        /* copy the next character */
        *dst++ = *src;
    }

    /* skip the prompt delimiter, if we found one */
    if (*src == ',')
        ++src;

    /* null-terminate the prompt */
    *dst = '\0';

    /* parse the prompt type */
    int prompt_type = atoi(src);

    /* find the delimiter and parse the file type */
    src = strchr(src, ',');
    int file_type = (src != 0 ? atoi(src + 1) : 0);

    /* run the regular askfile routine, and return the result */
    return _askfile(handle_, prompt, buf, buflen, prompt_type, file_type);
}


/* ------------------------------------------------------------------------ */
/*
 *   Our framework classes (CTadsApp, CTadsWin) have a couple of dependencies
 *   on subsystems that we don't need in the browser version.  We'll provide
 *   stubs for them here.  This isn't the cleanest solution; it would be
 *   better to factor out the dependencies more abstractly.  One day we might
 *   do that, but for now we'll take the hacky but expedient approach.
 */

/* this configuration never generates MIDI messages */
void CTadsMidiFilePlayer::do_midi_cb(UINT) { }

/* we never need the DirectMusic loader in this build */
IDirectMusicLoader8 *CTadsDirectMusic::loader_ = 0;

/* dummy entrypoint for HTTPRequest intrinsic class construction */
vm_obj_id_t TadsHttpRequest::prep_event_obj(VMG_ int *, int *)
{
    return VM_INVALID_OBJ;
}


/* ------------------------------------------------------------------------ */
/*
 *   Preferences registry key settings
 */

const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\3.0\\Settings";


/* ------------------------------------------------------------------------*/
/*
 *   Debug code - displays messages to a console window
 */

#ifdef TADSHTML_DEBUG

void init_debug_console()
{
    AllocConsole();
}

void close_debug_console()
{
    INPUT_RECORD inrec;
    DWORD cnt;

    /*
     *   Before exiting, wait for a keystroke, so that the user can see the
     *   contents of the console buffer
     */
    oshtml_dbg_printf("\nPress any key to exit...");

    /* clear out any keyboard events in the console buffer already */
    for (;;)
    {
        if (!PeekConsoleInput(GetStdHandle(STD_INPUT_HANDLE),
                              &inrec, 1, &cnt)
            || cnt == 0)
            break;
        ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inrec, 1, &cnt);
    }

    /* wait for a key from the console input buffer */
    for (;;)
    {
        if (!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE),
                              &inrec, 1, &cnt))
            break;
        if (inrec.EventType == KEY_EVENT)
            break;
    }
}

/*
 *   Display a debug message to the system console
 */
void os_dbg_sys_msg(const textchar_t *msg)
{
    DWORD cnt;

    /* display the message on the debug console */
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg,
                 get_strlen(msg), &cnt, 0);
}

#else /* TADSHTML_DEBUG */

/*
 *   Debugging not enabled - provide dummy versions of the debug functions
 */
void init_debug_console() { }
void close_debug_console() { }
void os_dbg_sys_msg(const textchar_t *) { }

#endif /* TADSHTML_DEBUG */

/*
 *   format a debug message
 */
void oshtml_dbg_printf(const char *msg, ...)
{
    char *buf;

    /* measure the buffer size */
    va_list args;
    va_start(args, msg);
    int len = _vscprintf(msg, args) + 1;
    va_end(args);

    /* allocate the buffer */
    buf = new char[len];

    /* format the message */
    va_start(args, msg);
    _vsnprintf(buf, len, msg, args);
    va_end(args);

    /* print the message */
    os_dbg_sys_msg(buf);

    /* done with the buffer */
    delete [] buf;
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse a command-line token
 */
static const char *parse_tok(char *&p)
{
    /* skip leading spaces */
    for ( ; isspace(*p) ; ++p) ;

    /* if we're at the end of the line, there are no more tokens */
    if (*p == '\0')
        return 0;

    /* note if we're at an open quote */
    char *tok;
    if (*p == '"')
    {
        /* the token starts after the quote */
        tok = ++p;

        /* find the close quote, removing stuttered quotes */
        char *dst = p;
        while (*p != '\0')
        {
            /* check for quotes */
            if (*p == '"')
            {
                /*
                 *   if another quote follows, it's a stuttered quote,
                 *   meaning we only want to keep a single quote; otherwise
                 *   it's the end of the token
                 */
                if (*(p+1) == '"')
                {
                    /* stutter - copy one quote and keep going */
                    *dst++ = *p++;
                    p++;
                }
                else
                {
                    /* end of the token */
                    break;
                }
            }
            else
            {
                /* copy the character verbatim */
                *dst++ = *p++;
            }
        }

        /* if we stopped before the end of the line, skip the close quote */
        if (*p != '\0')
            ++p;

        /* null-terminate the token */
        *dst = '\0';
    }
    else
    {
        /* the token starts here */
        tok = p;

        /* the token ends at the next whitespace */
        for ( ; *p != '\0' && !isspace(*p) ; ++p) ;

        /* if we stopped before the end of the line, null-terminate */
        if (*p != '\0')
            *p++ = '\0';
    }

    /* return the start of the token */
    return tok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Main entrypoint
 */
int PASCAL WinMain(HINSTANCE inst, HINSTANCE pinst,
                   LPSTR cmdline, int cmdshow)
{
    const char *url = 0;
    HANDLE parent_proc = INVALID_HANDLE_VALUE;
    HANDLE pipe = INVALID_HANDLE_VALUE;

    /* remember the instance handle globally */
    G_hinstance = inst;

    /* parse the command line */
    const char *tok;
    while ((tok = parse_tok(cmdline)) != 0)
    {
        /* check what we have */
        if (memcmp(tok, "url=", 4) == 0)
        {
            /* remember the URL */
            url = tok + 4;
        }
        else if (memcmp(tok, "pipe=", 5) == 0)
        {
            /* open the pipe */
            pipe = CreateFile(
                tok + 5, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED | FILE_FLAG_WRITE_THROUGH,
                0);
        }
        else if (memcmp(tok, "ppid=", 5) == 0)
        {
            /* parse the PID */
            DWORD pid = atol(tok + 5);

            /* get a handle to the process */
            parent_proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
        }
    }

    /* if there's no URL, abort */
    if (url == 0)
    {
        MessageBox(0, "This program is part of the TADS Interpreter. Please "
                   "run the main Interpreter program if you'd like to "
                   "play a TADS game.",
                   "TADS", MB_OK);
        return 0;
    }

    /* initialize the debug console */
    init_debug_console();

    /* create the application object */
    CTadsApp::create_app(inst, pinst, cmdline, cmdshow);

    /* create preferences object */
    class CHtmlPreferences *prefs = new CHtmlPreferences();

    /*
     *   read stored preference settings; this is the initial load, so load
     *   all properties, synchronized or not
     */
    prefs->restore(FALSE);

    /*
     *   create the Web UI window object (this is the top window, so it has
     *   no parent)
     */
    CHtmlSys_webuiwin *win = new CHtmlSys_webuiwin(0, prefs);
    win->AddRef();

    /* create the pipe monitor thread */
    comm_thread = new WebUICommThreadChi(parent_proc, pipe, win);
    comm_thread->launch();

    /* set up the initial position from the preferences */
    RECT pos;
    get_prefs_win_pos(&pos, prefs, 0);

    /* display the window */
    win->create_system_window(0, TRUE, "TADS", &pos);
    BringWindowToTop(win->get_handle());

    /* go to the starting address */
    win->go_to_url(url);

    /* process messages */
    for (int done = FALSE ; !done ; )
    {
        MSG msg;
        IWebBrowser2 *iwb;

        switch (GetMessage(&msg, 0, 0, 0))
        {
        case -1:
            /* error reading a message - terminate */
            done = TRUE;
            break;

        case 0:
            /* quitting */
            done = TRUE;
            break;

        default:
            /* check for special dispatch to the browser control */
            if (msg.message >= WM_KEYDOWN && msg.message <= WM_KEYLAST
                //&& msg.hwnd == win->get_handle()
                && (iwb = win->get_browser_ctl()) != 0)
            {
                /* assume the accelerator won't fully handle the event */
                int result = 1;

                /* get the in-place object */
                IOleInPlaceActiveObject *obj;
                if (SUCCEEDED(iwb->QueryInterface(
                    IID_IOleInPlaceActiveObject, (void **)&obj)))
                {
                    /* translate the accelerator */
                    result = obj->TranslateAccelerator(&msg);

                    /* done with the in-place object */
                    obj->Release();
                }

                /* done with the browser for now */
                iwb->Release();

                /* stop if the accelerator told us it fully handled it */
                if (result == 0)
                    break;
            }

            /* translate and dispatch the message */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;
        }
    }

    /* done with the pipe communicator thread */
    comm_thread->wait();
    comm_thread->release_ref();

    /* done with our window and preferences objects */
    if (win->Release() != 0) __asm int 3;
    prefs->release_ref();

    /* done with the application object */
    CTadsApp::delete_app();

    /* check memory */
    HTML_IF_DEBUG(th_list_memory_blocks());

    /* close the debug console, making sure the user acknowledges it */
    close_debug_console();

    /* done */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Process messages
 */
void osnet_host_process_message(MSG *msg)
{
    /* send the message to the main application object */
    CTadsApp *app;
    if ((app = CTadsApp::get_app()) != 0)
        app->process_message(msg);
}
