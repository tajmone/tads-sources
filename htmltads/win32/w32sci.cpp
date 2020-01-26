/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32sci.cpp - TADS Workbench Scintilla interface
Function

Notes

Modified
  09/23/06 MJRoberts  - Creation
*/

#include <Windows.h>
#include <commctrl.h>
#include <assert.h>

#include <ctype.h>
#include <stdio.h>

#include <platform.h>
#include <scintilla.h>

#include "htmlsys.h"
#include "tadsapp.h"
#include "tadswin.h"
#include "tadshtml.h"
#include "w32sci.h"
#include "htmlres.h"


/* ------------------------------------------------------------------------ */
/*
 *   The mapping array mapping our ID_SCI_xxx command codes to SCI_xxx
 *   message codes.  This array is indexed by (cmd - ID_SCI_FIRST), where cmd
 *   is a Workbench ID_SCI_xxx command code.
 */
UINT ScintillaWin::cmd_to_sci_[ID_SCI_LAST - ID_SCI_FIRST + 1];
int ScintillaWin::cmd_to_sci_inited_ = FALSE;

/* subclassing 'self' property name */
const char *ScintillaWin::self_prop_ = "tads.ScintillaWin.self";

/* splitter cursor */
HCURSOR ScintillaWin::split_csr_ = 0;

/* clipboard stack ring */
sci_cb_ele ScintillaWin::cb_stack_[ScintillaWin::CB_STACK_SIZE];
int ScintillaWin::cbs_top_ = 0;
int ScintillaWin::cbs_cnt_ = 0;

/* custom named message IDs */
UINT ScintillaWin::msg_create_spot = 0;
UINT ScintillaWin::msg_delete_spot = 0;
UINT ScintillaWin::msg_get_spot_pos = 0;
UINT ScintillaWin::msg_set_spot_pos = 0;

/*
 *   initialize the ID_SCI_xxx-to-SCI_xxx command mapping array
 */
void ScintillaWin::init_cmd_to_sci()
{
    /* set each entry via the generated mapping list */
#define W32COMMAND(id, name, desc, sys, sci) \
    assert(sci == 0 || (id >= ID_SCI_FIRST && id <= ID_SCI_LAST)); \
    if (sci != 0) cmd_to_sci_[id - ID_SCI_FIRST] = sci;
#include "w32tdbcmd.h"
#undef W32COMMAND

    /* remember that it's initialized */
    cmd_to_sci_inited_ = TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Marker IDs.  These have to line up with the HTMLTDB_xxx constants, such
 *   that HTMLTDB_STAT_xxx equals (1 << MARKER_xxx).
 */

#define MARKER_FIRST_STAT 3                 /* first debugger status marker */
#define MARKER_BP         3                                   /* breakpoint */
#define MARKER_BPCOND     4                      /* conditional gbreakpoint */
#define MARKER_BPDIS      5                          /* disabled breakpoint */
#define MARKER_CTXLINE    6                                 /* context line */
#define MARKER_CURLINE    7                                 /* current line */
#define MARKER_LAST_STAT  7                           /* last status marker */
#define MARKER_ALL_STAT  \
    (((1 << (MARKER_LAST_STAT + 1)) - 1) \
     & ~((1 << MARKER_FIRST_STAT) - 1))

/*
 *   Extra internal markers - these don't correspond to any HTMLTDB_xxx
 *   codes, so the only requirement is that they don't overlap with any of
 *   those.  Note that Scintilla displays markers in ascending order, so
 *   higher-numbered markers go in front of lower-numbered markers when a
 *   line has multiple markers.
 */
#define MARKER_BOOKMARK  0                                      /* bookmark */
#define MARKER_DROPLINE  8     /* temporary drop-target current-line marker */



/* ------------------------------------------------------------------------ */
/*
 *   Translate a COLORREF to the Scintilla encoding
 */
inline DWORD COLORREF_to_sci(COLORREF c)
{
    return ((GetRValue(c))
            | (GetGValue(c) << 8)
            | (GetBValue(c) << 16));
}

/* ------------------------------------------------------------------------ */
/*
 *   Statics and globals
 */
HMODULE ScintillaWin::hmod_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Load scintilla
 */
int ScintillaWin::load_dll()
{
    /* load the DLL */
    hmod_ = LoadLibrary("SciLexer.DLL");

    /* register our custom named messages */
    msg_create_spot = RegisterWindowMessage("w32tdb.sci.CreateSpot");
    msg_delete_spot = RegisterWindowMessage("w32tdb.sci.DeleteSpot");
    msg_get_spot_pos = RegisterWindowMessage("w32tdb.sci.GetSpotPos");
    msg_set_spot_pos = RegisterWindowMessage("w32tdb.sci.SetSpotPos");

    /* return true on success, false on failure */
    return (hmod_ != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   create a new Scintilla control
 */
ScintillaWin::ScintillaWin(IScintillaParent *parent)
{
    RECT rc;
    int i;

    /* count one reference on behalf of the caller */
    refcnt_ = 1;

    /* initialize our command-mapping array if we haven't already */
    if (!cmd_to_sci_inited_)
        init_cmd_to_sci();

    /* remember my parent window */
    parent_ = parent;

    /* we have no extension interface yet */
    ext_ = 0;

    /* there are no spots yet */
    spots_ = 0;

    /* not loading anything yet */
    loading_ = FALSE;

    /* not in any selection mode yet */
    selmode_ = FALSE;

    /* nothing in the cut-eol buffer yet */
    cut_eol_len_ = 0;

    /* no cut-to-eol position yet */
    last_cut_eol_pos_ = -1;

    /* we're not in a position to perform a paste-pop */
    paste_range_start_ = paste_range_end_ = -1;

    /* not showing a splitter yet */
    show_splitter_ = FALSE;

    /* no repeat count yet */
    repeat_count_ = 1;

    /* start out not in auto-wrap mode */
    auto_wrap_ = FALSE;

    /* load the splitter cursor */
    if (split_csr_ == 0)
        split_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(IDC_SPLIT_NS_CSR));

    /* the Scintilla control takes up the parent's whole client area */
    GetClientRect(parent->isp_get_handle(), &rc);

    /*
     *   Create the Scintilla window.  Don't set the WS_VSCROLL | WS_HSCROLL
     *   styles, so that the window uses separate controls for the scrollbars
     *   - we need this to allow for our special splitter button handling.
     */
    hwnd_ = CreateWindowEx(
        0, "Scintilla", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP
        | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, rc.right, rc.bottom, parent->isp_get_handle(),
        0, CTadsApp::get_app()->get_instance(), 0);

    /* get the window's vertical scrollbar */
    HWND w;
    for (vscroll_ = 0, w = GetWindow(hwnd_, GW_CHILD) ; w != 0 ;
         w = GetWindow(w, GW_HWNDNEXT))
    {
        char buf[20];

        /* if this is a vertical scrollbar, we've found it */
        GetClassName(w, buf, sizeof(buf));
        if (strcmp(buf, "ScrollBar") == 0
            && (GetClassLong(w, GCL_STYLE) & SBS_VERT) != 0)
        {
            /* got it */
            vscroll_ = w;
            break;
        }
    }

    /* create our internal splitter control */
    splitter_ = CreateWindow("STATIC", "",
                             WS_CHILD | SS_NOTIFY | SS_OWNERDRAW,
                             0, 0, 1, 1, hwnd_, 0, 0, 0);

    /* get the direct function pointers we need */
    call_sci_ = (int (__cdecl *)(void *, int, int, int))SendMessage(
        hwnd_, SCI_GETDIRECTFUNCTION, 0, 0);
    call_sci_ctx_ = (void *)SendMessage(hwnd_, SCI_GETDIRECTPOINTER, 0, 0);

    /* set Scintilla to use the current ANSI code page */
    call_sci(SCI_SETCODEPAGE, GetACP(), 0);

    /* set up margin 1 for symbols */
    call_sci(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);

    /* we want clicks in margin 1 */
    call_sci(SCI_SETMARGINSENSITIVEN, 1, TRUE);

    /*
     *   turn on folding in Scintilla, so that we have the folding data ready
     *   if we decide to turn it on
     */
    call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
    call_sci(SCI_SETPROPERTY, (WPARAM)"fold.compact", (LPARAM)"0");

    /* set up the folding-control margin, in case we want to use it */
    call_sci(SCI_SETMARGINWIDTHN, 2, 0);
    call_sci(SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
    call_sci(SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
    call_sci(SCI_SETMARGINWIDTHN, 2, 20);
    call_sci(SCI_SETMARGINSENSITIVEN, 2, TRUE);

    /* set up the folding symbols */
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_PLUS);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_PLUS);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_MINUS);
    call_sci(SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);

    /* make sure all the markers line up with the HTMLTDB_STAT_xxx's */
    assert(HTMLTDB_STAT_BP == (1 << MARKER_BP)
           && HTMLTDB_STAT_BP_DIS == (1 << MARKER_BPDIS)
           && HTMLTDB_STAT_BP_COND == (1 << MARKER_BPCOND)
           && HTMLTDB_STAT_CTXLINE == (1 << MARKER_CTXLINE)
           && HTMLTDB_STAT_CURLINE == (1 << MARKER_CURLINE));

    /* define markers to match the HTMLTDB_STAT_xxx constants */
    load_marker(MARKER_BOOKMARK, IDX_DBG_BOOKMARK);
    load_marker(MARKER_BP, IDX_DBG_BP);
    load_marker(MARKER_BPDIS, IDX_DBG_BPDIS);
    load_marker(MARKER_BPCOND, IDX_DBG_BPCOND);
    load_marker(MARKER_CTXLINE, IDX_DBG_CTXLINE);
    load_marker(MARKER_CURLINE, IDX_DBG_CURLINE);
    load_marker(MARKER_DROPLINE, IDX_DBG_DRAGLINE);

    /* set the default style bits */
    call_sci(SCI_SETSTYLEBITS, call_sci(SCI_GETSTYLEBITSNEEDED));

    /* set the standard caret policy */
    set_std_caret_policy();

    /*
     *   clear all keyboard mappings - we use our own keyboard accelerator
     *   mechanism, so we don't need Scintilla to do this
     */
    call_sci(SCI_CLEARALLCMDKEYS);

    /*
     *   set all of the control characters to do nothing (otherwise they'll
     *   actually insert the control character values)
     */
    for (i = 'A' ; i <= 'Z' ; ++i)
        call_sci(SCI_ASSIGNCMDKEY, i | (SCMOD_CTRL << 16), SCI_NULL);

    /*
     *   don't use the Scintilla context menu - we want to let the container
     *   use its own
     */
    call_sci(SCI_USEPOPUP, FALSE);

    /* create our tooltip */
    tooltip_ = CreateWindowEx(0, TOOLTIPS_CLASS, 0, 0,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              hwnd_, 0,
                              CTadsApp::get_app()->get_instance(), 0);

    /*
     *   add the "tool" - the entire display area; we'll figure out where we
     *   actually are when we need to get tooltip text
     */
    TOOLINFO ti;
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = hwnd_;
    ti.uId = 1;
    ti.hinst = CTadsApp::get_app()->get_instance();
    ti.lpszText = LPSTR_TEXTCALLBACK;
    SetRect(&ti.rect, 0, 0, 32767, 32767);
    SendMessage(tooltip_, TTM_ADDTOOL, 0, (LPARAM)&ti);

    /*
     *   Set our tooltip initial-display delay time to zero.  We rely on
     *   Scintilla to send us "dwell" notifications, so we want to display
     *   the tip as soon as Scintilla says we should - we thus don't need any
     *   additional delay from the tooltip control itself.
     */
    SendMessage(tooltip_, TTM_SETDELAYTIME, TTDT_INITIAL, 0);

    /*
     *   leave it unactivated initially - we'll activate on a mouse "dwell"
     *   notification from Scintilla
     */
    SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);

    /*
     *   set up our dwell time - we use this to show the tooltip, so set the
     *   dwell time to the standard tooltip initial time, which is equal to
     *   the system double-click interval
     */
    call_sci(SCI_SETMOUSEDWELLTIME, (int)GetDoubleClickTime());

    /* set the default wrapping settings */
    auto_wrap_ = FALSE;
    call_sci(SCI_SETEDGECOLUMN, right_margin_ = 80);

    /* set up our subclassing */
    SetProp(hwnd_, self_prop_, this);
    oldwinproc_ = (WNDPROC)GetWindowLongPtr(hwnd_, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd_, GWLP_WNDPROC, (DWORD_PTR)&s_winproc);

    /* request file drop messages */
    DragAcceptFiles(hwnd_, TRUE);

    /*
     *   add a reference on behalf of the system window, which points to us
     *   via the property we just set
     */
    AddRef();

    /* create the special spot called 'mark' */
    mark_ = create_spot(0);
}

/*
 *   Delete
 */
ScintillaWin::~ScintillaWin()
{
    /* release any interface extension */
    set_extension(0);

    /* delete all spots */
    while (spots_ != 0)
        delete_spot(spots_);
}

/*
 *   Static termination
 */
void ScintillaWin::static_term()
{
    /* clear out the clipboard stack */
    for (int i = 0 ; i < CB_STACK_SIZE ; ++i)
        cb_stack_[i].txt.clear();
}

/* ------------------------------------------------------------------------ */
/*
 *   Plug in a shared view of another window's underlying document
 */
void ScintillaWin::set_doc(ScintillaWin *win)
{
    void *doc;

    /* get the other window's document pointer */
    doc = (void *)win->call_sci(SCI_GETDOCPOINTER);

    /* set our document to point to the other window's document */
    call_sci(SCI_SETDOCPOINTER, 0, (int)doc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Subclassed window procedure
 */
LRESULT ScintillaWin::winproc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    /*
     *   if we have an extension, pass it to the extension; otherwise just
     *   invoke our handler directly
     */
    if (ext_ != 0)
        return ext_->MsgFilter(hwnd, msg, wpar, lpar, this);
    else
        return CallMsgHandler(hwnd, msg, wpar, lpar);
}

/*
 *   Handle a message to the Scintilla window.  We reach this either directly
 *   from our subclassed window procedure, or indirectly by way of the
 *   extension object's message filter, when it calls the default handler.
 */
LRESULT STDMETHODCALLTYPE ScintillaWin::CallMsgHandler(
    HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    LRESULT ret;
    int i;

    /* check the message */
    switch (msg)
    {
    case WM_CHAR:
        /* repeat the character command */
        for (i = 0 ; i < repeat_count_ ; ++i)
        {
            /* send this character to Scintilla */
            ret = CallWindowProc(oldwinproc_, hwnd, msg, wpar, lpar);

            /* check for auto-wrapping */
            check_auto_wrap();
        }

        /* handled */
        return ret;

    case WM_DROPFILES:
        /* file drag and drop from Windows Explorer */
        {
            /* get the HDROP object */
            HDROP hdrop = (HDROP)wpar;

            /* run through the files in the drop */
            int cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
            for (int i = 0 ; i < cnt ; ++i)
            {
                /* get this filename */
                char fname[256];
                DragQueryFile(hdrop, i, fname, sizeof(fname));

                /* handle the file drop */
                parent_->isp_drop_file(fname);
            }
        }

        /* handled */
        return 0;

    case WM_DESTROY:
        /* on deletion, remove our subclassing */
        RemoveProp(hwnd, self_prop_);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (DWORD_PTR)oldwinproc_);

        /*
         *   call the old window procedure - do this before releasing our
         *   self-reference, as the Release() could delete us
         */
        ret = CallWindowProc(oldwinproc_, hwnd, msg, wpar, lpar);

        /* release our implied reference from the system window */
        Release();

        /* return the result */
        return ret;

    case WM_SETCURSOR:
        /*
         *   if the mouse is over our internal splitter, show the splitter
         *   cursor
         */
        if (show_splitter_ && LOWORD(lpar) == HTCLIENT
            && (HWND)wpar == splitter_)
        {
            SetCursor(split_csr_);
            return TRUE;
        }
        break;

    case WM_DRAWITEM:
        /* if it's the splitter, draw it */
        {
            DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lpar;
            if (di->hwndItem == splitter_)
            {
                RECT rc;

                /* draw it */
                GetClientRect(splitter_, &rc);
                FillRect(di->hDC, &rc, GetSysColorBrush(COLOR_3DFACE));
                InflateRect(&rc, -1, -1);
                DrawEdge(di->hDC, &rc, BDR_RAISEDINNER, BF_RECT);

                /* handled */
                return TRUE;
            }
        }
        break;

    case WM_NOTIFY:
        /* handle a notification */
        if (sc_notify((int)wpar, (LPNMHDR)lpar, &ret))
            return ret;
        break;

    case WM_COMMAND:
        /* check for notifications from the splitter button */
        if ((HWND)lpar == splitter_ && show_splitter_
            && (HIWORD(wpar) == STN_CLICKED || HIWORD(wpar) == STN_DBLCLK))
        {
            POINT pt;

            /* let the parent track the click */
            GetCursorPos(&pt);
            ScreenToClient(GetParent(hwnd_), &pt);
            return SendMessage(GetParent(hwnd_), WM_LBUTTONDOWN,
                               MK_LBUTTON, MAKELPARAM(pt.x, 0));
        }
        break;

    case WM_SIZE:
        /* do our inherited sizing first */
        ret = CallWindowProc(oldwinproc_, hwnd, msg, wpar, lpar);

        /*
         *   adjust the vertical scrollbar to make room for the splitter, if
         *   necessary
         */
        if (show_splitter_)
            adjust_vscroll();

        /* return the result from the inherited routine */
        return ret;

    default:
        /* check for our custom named messages */
        if (msg == msg_create_spot)
            return (LPARAM)create_spot(wpar);
        if (msg == msg_delete_spot)
            return delete_spot((void *)lpar), 0;
        if (msg == msg_get_spot_pos)
            return get_spot_pos((void *)lpar);
        if (msg == msg_set_spot_pos)
            return set_spot_pos((void *)lpar, wpar), 0;

        /* not one of ours */
        break;
    }

    /* pass the message to the original window procedure */
    return CallWindowProc(oldwinproc_, hwnd, msg, wpar, lpar);
}

/*
 *   adjust the vertical scrollbar to make room for the splitter
 */
void ScintillaWin::adjust_vscroll()
{
    RECT rc;
    int wid = GetSystemMetrics(SM_CXVSCROLL);
    int ht = GetSystemMetrics(SM_CYHSCROLL);

    /* if there's no scrollbar, skip this */
    if (vscroll_ == 0)
        return;

    /* get our scrollbar area */
    GetClientRect(hwnd_, &rc);
    rc.left = rc.right - GetSystemMetrics(SM_CXVSCROLL);
    rc.bottom -= ht;
    if (show_splitter_)
        rc.top += 7;

    /* move it */
    MoveWindow(vscroll_, rc.left, rc.top,
               rc.right - rc.left, rc.bottom - rc.top, TRUE);

    /* move the splitter if it's visible */
    if (show_splitter_)
        MoveWindow(splitter_, rc.left, 0, rc.right - rc.left, rc.top, TRUE);
}

/*
 *   subclassed WM_NOTIFY handler
 */
int ScintillaWin::sc_notify(int control_id, NMHDR *nm, LRESULT *ret)
{
    switch (nm->code)
    {
    case TTN_NEEDTEXT:
        /*
         *   Get the tooltip text.  This attempts to evaluate the text under
         *   the mouse as an expression; if successful, we show the value of
         *   the expression as the tooltip text.
         */
        {
            TOOLTIPTEXT *ttt = (TOOLTIPTEXT *)nm;
            int pos;
            int expr_good = FALSE;
            int len;
            TextRange tr;
            char *p, *dst;
            int qu;
            static textchar_t tipbuf[512];
            POINT pt;

            /* no text buffer yet */
            tr.lpstrText = 0;

            /* get the mouse position */
            GetCursorPos(&pt);
            ScreenToClient(hwnd_, &pt);

            /* find the document position at the mouse */
            pos = call_sci(SCI_POSITIONFROMPOINTCLOSE, pt.x, pt.y);
            if (pos == -1)
                goto needtext_done;

            /* get the selection range */
            tr.chrg.cpMin = call_sci(SCI_GETSELECTIONSTART);
            tr.chrg.cpMax = call_sci(SCI_GETSELECTIONEND);

            /*
             *   if the mouse is over the selection, use the entire selection
             *   as the source text; otherwise use the expression at the
             *   cursor (not including any function call suffix - tooltip
             *   evaluation can't call functions anyway, so there's no point
             *   in trying)
             */
            if (pos < tr.chrg.cpMin || pos > tr.chrg.cpMax)
                get_expr_boundaries(&tr.chrg.cpMin, &tr.chrg.cpMax, pos, FALSE);

            /* get the text */
            len = tr.chrg.cpMax - tr.chrg.cpMin;
            tr.lpstrText = new char[len + 1];
            call_sci(SCI_GETTEXTRANGE, 0, (int)&tr);

            /*
             *   convert newlines and other controls to spaces, and remove
             *   redundant spaces outside of strings
             */
            for (p = dst = tr.lpstrText, qu = 0 ; *p != '\0' ; ++p)
            {
                char c = *p;

                /* convert control characters and whitespace to spaces */
                if (!isprint(c) || isspace(c))
                    c = ' ';

                /* note strings */
                if (qu != 0)
                {
                    if (*p == '\\')
                        *dst++ = c, c = *++p;
                    else if (c == qu)
                        qu = 0;
                }
                else if (c == '\'' || c == '"')
                    qu = c;

                /* remove redundant spaces outside of strings */
                if (qu == 0
                    && c == ' '
                    && dst > tr.lpstrText
                    && *(dst-1) == ' ')
                    continue;

                /* store the output charater */
                *dst++ = c;
            }

            /* null-terminate the string */
            *dst = 0;

            /* remove trailing spaces */
            for ( ; p > tr.lpstrText && isspace(*(p-1)) ; *--p = '\0') ;

            /* skip leading spaces */
            for (p = tr.lpstrText ; isspace(*p) ; ++p) ;

            /* if it's all spaces, ignore it */
            if (*p == '\0')
                goto needtext_done;

            /*
             *   if it's relatively short, show the value as "expr = val";
             *   otherwise, just show the value
             */
            if (strlen(p) < sizeof(tipbuf)/4)
            {
                /* copy the "expr = " part */
                strcpy(tipbuf, p);
                strcat(tipbuf, " = ");

                /* set up to copy the expression after the "expr = " part */
                p = tipbuf + strlen(tipbuf);
            }
            else
            {
                /* use the whole buffer for the expression */
                p = tipbuf;
            }

            /*
             *   Evaluate it as an expression.  If we get a valid result,
             *   compare it to the source text to make sure it's not
             *   identical; if it is, don't bother displaying it, as
             *   tautologies aren't useful to the user.  If we get a valid,
             *   non-tautological result, display it as the tip text.
             */
            if (parent_->isp_eval_tip(
                tr.lpstrText, p, sizeof(tipbuf) - (p - tipbuf))
                && get_strcmp(tr.lpstrText, p) != 0)
            {
                /* success - use the tip buffer as the result */
                ttt->lpszText = tipbuf;
                expr_good = TRUE;
            }

            /*
             *   If we didn't get a valid expression, don't show anything.
             *   To avoid popping up the tip, simply deactivate it.
             */
       needtext_done:
            if (!expr_good)
                SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);

            /* done with the source text buffer */
            if (tr.lpstrText != 0)
                delete [] tr.lpstrText;
        }
        *ret = TRUE;
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set our standard caret policy for editing
 */
void ScintillaWin::set_std_caret_policy()
{
    call_sci(SCI_SETYCARETPOLICY, CARET_EVEN, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a marker
 */
void ScintillaWin::load_marker(int id, int res)
{
    HRSRC hres;
    HGLOBAL hgl;
    void *mem;

    /* find the resource */
    if ((hres = FindResource(0, MAKEINTRESOURCE(res), "XPIXMAP")) != 0
        && (hgl = LoadResource(0, hres)) != 0
        && (mem = LockResource(hgl)) != 0)
    {
        /* set up the pixmap with Scintilla */
        call_sci(SCI_MARKERDEFINEPIXMAP, id, (int)mem);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Reset to the "freshly loaded" state.
 */
void ScintillaWin::reset_state()
{
    /* tell Scintilla to reset its undo state */
    call_sci(SCI_EMPTYUNDOBUFFER);

    /* tell our parent that the document is clean */
    parent_->isp_set_dirty(FALSE);
}

/*
 *   Set read-only mode
 */
void ScintillaWin::set_readonly(int f)
{
    call_sci(SCI_SETREADONLY, f);
}

/*
 *   mark the document as clean (unmodified)
 */
void ScintillaWin::set_clean()
{
    /* set a savepoint, which indicates that this is a clean point */
    call_sci(SCI_SETSAVEPOINT);
}


/* ------------------------------------------------------------------------ */
/*
 *   Handle a Workbench command.  If this is an ID_SCI_xxx command, translate
 *   it to the corresponding SCI_xxx command and process it.  Otherwise, it's
 *   not for us to handle.
 */
int ScintillaWin::handle_workbench_cmd(int id)
{
    /* if it's within the translatable range, we can handle it */
    if (id >= ID_SCI_FIRST && id <= ID_SCI_LAST)
    {
        int i;

        /* translate it to a SCI_xxx command */
        id = cmd_to_sci_[id - ID_SCI_FIRST];

        /* do some pre-Sci work */
        switch (id)
        {
        case SCI_CANCEL:
            /* do our own selection-mode cancelation */
            cancel_sel_mode(TRUE);
            break;
        }

        /* certain commands obey the repeat count */
        switch (id)
        {
        case SCI_LINEDOWN:
        case SCI_LINEDOWNEXTEND:
        case SCI_LINEDOWNRECTEXTEND:
        case SCI_LINESCROLLDOWN:
        case SCI_LINEUP:
        case SCI_LINEUPEXTEND:
        case SCI_LINEUPRECTEXTEND:
        case SCI_LINESCROLLUP:
        case SCI_PARADOWN:
        case SCI_PARADOWNEXTEND:
        case SCI_PARAUP:
        case SCI_PARAUPEXTEND:
        case SCI_CHARLEFT:
        case SCI_CHARLEFTEXTEND:
        case SCI_CHARLEFTRECTEXTEND:
        case SCI_CHARRIGHT:
        case SCI_CHARRIGHTEXTEND:
        case SCI_CHARRIGHTRECTEXTEND:
        case SCI_WORDLEFT:
        case SCI_WORDLEFTEXTEND:
        case SCI_WORDRIGHT:
        case SCI_WORDRIGHTEXTEND:
        case SCI_WORDPARTLEFT:
        case SCI_WORDPARTLEFTEXTEND:
        case SCI_WORDPARTRIGHT:
        case SCI_WORDPARTRIGHTEXTEND:
        case SCI_PAGEUP:
        case SCI_PAGEUPEXTEND:
        case SCI_PAGEUPRECTEXTEND:
        case SCI_PAGEDOWN:
        case SCI_PAGEDOWNEXTEND:
        case SCI_PAGEDOWNRECTEXTEND:
        case SCI_STUTTEREDPAGEUP:
        case SCI_STUTTEREDPAGEUPEXTEND:
        case SCI_STUTTEREDPAGEDOWN:
        case SCI_STUTTEREDPAGEDOWNEXTEND:
        case SCI_DELETEBACK:
        case SCI_DELETEBACKNOTLINE:
        case SCI_DELWORDLEFT:
        case SCI_DELWORDRIGHT:
        case SCI_LINEDELETE:
        case SCI_LINECUT:
        case SCI_LOWERCASE:
        case SCI_UPPERCASE:
        case SCI_NEWLINE:
        case SCI_FORMFEED:
        case SCI_TAB:
        case SCI_BACKTAB:
            /* these commands repeat - group for undo */
            call_sci(SCI_BEGINUNDOACTION);

            /* perform the repeated command */
            for (i = 0 ; i < repeat_count_ ; ++i)
                call_sci(id);

            /* end the undo group */
            call_sci(SCI_ENDUNDOACTION);
            break;

        case SCI_DOCUMENTSTART:
        case SCI_DOCUMENTSTARTEXTEND:
        case SCI_DOCUMENTEND:
        case SCI_DOCUMENTENDEXTEND:
            /* these commands set the mark */
            set_mark_pos(call_sci(SCI_GETCURRENTPOS));

            /* do the normal work */
            call_sci(id);
            break;

        default:
            /* other commands ignore the repeat count */
            call_sci(id);
            break;
        }

        /* do some post-Sci work */
        switch (id)
        {
        case SCI_DELETEBACK:
        case SCI_DELETEBACKNOTLINE:
        case SCI_DELWORDLEFT:
        case SCI_DELWORDRIGHT:
        case SCI_DELLINELEFT:
        case SCI_DELLINERIGHT:
        case SCI_LINEDELETE:
        case SCI_LINECUT:
        case SCI_LINECOPY:
        case SCI_LINETRANSPOSE:
        case SCI_LINEDUPLICATE:
        case SCI_NEWLINE:
        case SCI_FORMFEED:
        case SCI_TAB:
        case SCI_BACKTAB:
        case SCI_COPY:
            /* cancel selection mode on these commands */
            cancel_sel_mode(TRUE);
            break;
        }

        /* we handled it */
        return TRUE;
    }

    /* didn't handle it */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   handle a notification
 */
int ScintillaWin::handle_notify(int notify_code, LPNMHDR nm)
{
    SCNotification *scn = (SCNotification *)nm;
    int pos;
    int line, col;
    int markers;
    int x, wid;
    int linenum;

    /* if it's not from our Scintilla control, it's not ours to handle */
    if (nm->hwndFrom != hwnd_)
        return FALSE;

    /* check what we have */
    switch (notify_code)
    {
    case SCN_MARGINCLICK:
        /* check which margin */
        switch (scn->margin)
        {
        case 1:
            /*
             *   margin 1 is the debugger control margin, where we set
             *   breakpoints and can move the current execution pointer
             */

            /* temporarily move the current position to the clicked line */
            pos = call_sci(SCI_GETCURRENTPOS);
            call_sci(SCI_SETCURRENTPOS, scn->position);

            /* get the markers for this line */
            linenum = call_sci(SCI_LINEFROMPOSITION, scn->position);
            markers = call_sci(SCI_MARKERGET, linenum);

            /* if the 'current line' marker is set, let the parent drag it */
            if ((markers & HTMLTDB_STAT_CURLINE) != 0)
            {
                /* cancel any mouse capture that Scintilla obtained */
                if (GetCapture() != 0)
                    ReleaseCapture();

                /* tell the parent to track the current line position */
                parent_->isp_drag_cur_line(this);
            }
            else
            {
                /* toggle the breakpoint here */
                parent_->isp_toggle_breakpoint(this);
            }

            /* restore the old position */
            call_sci(SCI_SETCURRENTPOS, pos);

            /* done */
            break;

        case 2:
            /*
             *   margin 2 is the folding margin - a click here inverts the
             *   folding state of the clicked line
             */

            /* get the line they clicked on, and toggle the folding */
            linenum = call_sci(SCI_LINEFROMPOSITION, scn->position);
            call_sci(SCI_TOGGLEFOLD, linenum);

            /* done */
            break;
        }
        break;

    case SCN_SAVEPOINTLEFT:
        /*
         *   If we're not loading a file, tell Workbench that the document is
         *   now dirty.  Changes made during loading a file don't count as
         *   modifications, for obvious reasons.
         */
        if (!loading_)
            parent_->isp_set_dirty(TRUE);
        break;

    case SCN_SAVEPOINTREACHED:
        /* tell Workbench that the document is now clean */
        parent_->isp_set_dirty(FALSE);
        break;

    case SCN_CHARADDED:
        /* pass this along to the extension */
        if (ext_ != 0)
            ext_->OnCharAdded(scn->ch);
        break;

    case SCN_UPDATEUI:
        /* the selection or text has changed - tell the container */
        pos = call_sci(SCI_GETCURRENTPOS);
        line = call_sci(SCI_LINEFROMPOSITION, pos);
        col = call_sci(SCI_GETCOLUMN, pos);
        parent_->isp_update_cursorpos(line, col);

        /* try matching braces */
        match_braces();

        /*
         *   note if the position has changed since the last cut-eol; if so,
         *   invalidate the cut-eol record
         */
        if (pos != last_cut_eol_pos_)
            last_cut_eol_pos_ = -1;

        /*
         *   any cursor movement or document modification invalidates the
         *   paste-pop position
         */
        if (pos != paste_range_end_
            || call_sci(SCI_GETANCHOR) != paste_range_end_)
            paste_range_start_ = paste_range_end_ = -1;

        /*
         *   If the current line is wider than the horizontal scrollbar
         *   width, adjust the scroll width to compensate.
         *
         *   This is a bit of a workaround for Scintilla's usual constant
         *   scrollbar policy.  Scintilla intentionally skips checking the
         *   actual maximum line width, since that can be slow for a large
         *   document.  However, it's quick enough to measure a single line,
         *   so we use this "lazy" approach as a compromise - we won't scan
         *   the whole document, per Scintilla's policy; but as soon as we
         *   move the text to a line that's too wide, we'll notice it and fix
         *   the scrollbar.
         */
        x = call_sci(SCI_POINTXFROMPOSITION, 0,
                     call_sci(SCI_GETLINEENDPOSITION, line)) + 100;
        wid = call_sci(SCI_GETSCROLLWIDTH);
        if (x > wid)
            call_sci(SCI_SETSCROLLWIDTH, x);

        /* done */
        break;

    case SCN_DWELLSTART:
        /* when the mouse dwells in one place, activate the tooltip */
        SendMessage(tooltip_, TTM_ACTIVATE, TRUE, 0);

        /*
         *   Explicitly relay a mouse-move event to the tooltip.  This is
         *   necessary because the tip has been ignoring mouse messages while
         *   deactivated; we're now ready to show the tooltip, so we want to
         *   make it think that a mouse-move just occurred.
         */
        {
            MSG msg;
            POINT pt;

            /* set up a WM_MOUSEMOVE message for the current cursor pos */
            GetCursorPos(&pt);
            ScreenToClient(hwnd_, &pt);
            msg.message = WM_MOUSEMOVE;
            msg.hwnd = hwnd_;
            msg.wParam = 0;
            msg.lParam = MAKELPARAM(pt.x, pt.y);

            /* relay the WM_MOUSEMOVE to the tooltip */
            SendMessage(tooltip_, TTM_RELAYEVENT, 0, (LPARAM)&msg);
        }
        break;

    case SCN_DWELLEND:
        /* when we stop dwelling in one place, hide the tooltip */
        SendMessage(tooltip_, TTM_ACTIVATE, FALSE, 0);
        break;

    case SCN_MODIFIED:
        /* update any spots affected by the change */
        if ((scn->modificationType
             & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) != 0)
        {
            sci_spot *spot;

            /* scan the spot list */
            for (spot = spots_ ; spot != 0 ; spot = spot->nxt)
            {
                /* check for deletion or insertion */
                if ((scn->modificationType & SC_MOD_DELETETEXT) != 0)
                {
                    /*
                     *   deleting - if the spot is within the range being
                     *   deleted, move it to the start of the range; if it's
                     *   after the range, move it back by the amount of text
                     *   being removed
                     */
                    if (spot->pos >= scn->position)
                    {
                        if (spot->pos < scn->position + scn->length)
                            spot->pos = scn->position;
                        else
                            spot->pos -= scn->length;
                    }
                }
                if ((scn->modificationType & SC_MOD_INSERTTEXT) != 0)
                {
                    /*
                     *   inserting - if the spot is after the insertion
                     *   point, move it ahead by the amount of text being
                     *   added
                     */
                    if (spot->pos > scn->position)
                        spot->pos += scn->length;
                }
            }
        }

        /* notify the container of the change */
        parent_->isp_note_change();

        /* done */
        break;

    case SCN_DROPFILE:
        /* open the file */
        parent_->isp_drop_file(scn->text);
        break;
    }

    /*
     *   the notification was from the Scintilla control, so it was meant for
     *   us; indicate that the event has been fully handled, even if we
     *   didn't actually do anything in response
     */
    return TRUE;
}

/*
 *   handle a WM_COMMAND sent to the parent from the control
 */
int ScintillaWin::handle_command_from_child(int notify_code)
{
    switch (notify_code)
    {
    case SCEN_SETFOCUS:
    case SCEN_KILLFOCUS:
        /* notify the parent of the focus change */
        parent_->isp_on_focus_change(this, notify_code == SCEN_SETFOCUS);
        return TRUE;
    }

    /* we didn't handle it specially */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Try matching braces
 */
void ScintillaWin::match_braces()
{
    int curpos, pos;
    int match;

    /* look at the character before the caret */
    curpos = call_sci(SCI_GETCURRENTPOS);
    pos = call_sci(SCI_POSITIONBEFORE, curpos);
    match = call_sci(SCI_BRACEMATCH, pos, 0);

    /* if we didn't find a match, try again to the right of the caret */
    if (match == -1)
        match = call_sci(SCI_BRACEMATCH, pos = curpos, 0);

    /* if we found a match, highlight the braces */
    if (match != -1)
        call_sci(SCI_BRACEHIGHLIGHT, pos, match);
    else
        call_sci(SCI_BRACEHIGHLIGHT, -1, -1);
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to see if a point is in the margin
 */
int ScintillaWin::pt_in_margin(int x, int y)
{
    int wid;
    int i;

    /* run through the margins and add up their display widths */
    for (i = 0, wid = 0 ; i < 5 ; ++i)
        wid += call_sci(SCI_GETMARGINWIDTHN, i);

    /* if the x coordinate is within the margin width, we're in a margin */
    return (x <= wid);
}

/*
 *   Move the cursor to the start of the line containing the given mouse
 *   coordinates
 */
void ScintillaWin::cursor_to_line(int x, int y)
{
    /* get the character position at the point */
    int pos = call_sci(SCI_POSITIONFROMPOINT, x, y);

    /* get the start of this line */
    pos = call_sci(SCI_POSITIONFROMLINE, call_sci(SCI_LINEFROMPOSITION, pos));

    /* go there */
    call_sci(SCI_SETSEL, pos, pos);
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a margin drag operation.  We invisibly move the current position
 *   to the mouse position and return TRUE, filling in *oldpos with the
 *   previous cursor position.  Otherwise we'll return FALSE.
 */
int ScintillaWin::check_margin_drag(int x, int y, int *oldpos, int *linenum)
{
    POINT pt;
    RECT rc;
    int newpos;

    /* first, make sure the mouse is within our window */
    pt.x = x;
    pt.y = y;
    GetClientRect(hwnd_, &rc);
    if (!PtInRect(&rc, pt))
        return FALSE;

    /* save the old position */
    *oldpos = call_sci(SCI_GETCURRENTPOS);

    /* move to this position */
    newpos = call_sci(SCI_POSITIONFROMPOINT, x, y);
    call_sci(SCI_SETCURRENTPOS, newpos);

    /* tell the caller the line number */
    *linenum = call_sci(SCI_LINEFROMPOSITION, newpos);

    /* indicate that the drag can proceed */
    return TRUE;
}

/*
 *   Temporarily drag the current position to the given line
 */
void ScintillaWin::do_margin_drag(int linenum, int *oldpos)
{
    /* save the old position */
    *oldpos = call_sci(SCI_GETCURRENTPOS);

    /* move to the new line */
    call_sci(SCI_SETCURRENTPOS, call_sci(SCI_POSITIONFROMLINE, linenum));
}


/*
 *   End a margin drag.  This restores the cursor position set with
 *   check_margin_drag().
 */
void ScintillaWin::end_margin_drag(int oldpos)
{
    call_sci(SCI_SETCURRENTPOS, oldpos);
}

/*
 *   Show or hide the drag line status icon
 */
void ScintillaWin::show_drag_line(int linenum, int show)
{
    /* get the current margin icons */
    if (show)
        call_sci(SCI_MARKERADD, linenum, MARKER_DROPLINE);
    else
        call_sci(SCI_MARKERDELETE, linenum, MARKER_DROPLINE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Search
 */
int ScintillaWin::search(const textchar_t *txt, int exact, int from_top,
                         int wrap, int dir, int regex, int whole_word,
                         int first_match_a, int first_match_b)
{
    TextToFind ttf;
    int start;

    /* make sure our match range is in ascending order */
    if (first_match_a > first_match_b)
    {
        int tmp = first_match_a;
        first_match_a = first_match_b;
        first_match_b = tmp;
    }

    /* start at the current position */
    ttf.chrg.cpMin = ttf.chrg.cpMax = start = call_sci(SCI_GETCURRENTPOS);

    /* do the search */
    if (search(txt, exact, from_top, wrap, dir, regex, whole_word, &ttf))
    {
        /* check for looping */
        if (dir > 0)
        {
            /*
             *   Going forwards.  Check for wrapping: if our ending location
             *   is below our starting location, we've looped, so consider
             *   the search to have started at the beginning of the file.
             */
            if (ttf.chrgText.cpMin < start)
                start = 0;

            /*
             *   if our start location is below the first match, and our
             *   ending location is at or above the first match, we've looped
             */
            if (start <= first_match_a
                && start < first_match_b
                && ttf.chrgText.cpMax >= first_match_a)
                return FALSE;
        }
        else
        {
            /* going backwards - check for wrapping */
            if (ttf.chrgText.cpMin > start)
                start = call_sci(SCI_GETTEXTLENGTH);

            /* check for looping */
            if (start >= first_match_b
                && start > first_match_a
                && ttf.chrgText.cpMin <= first_match_b)
                return FALSE;
        }

        /*
         *   Successful: set the selection range to the text we found.  If
         *   we're searching forward, put the caret at the end of the range;
         *   otherwise put the caret at the start of the range - this will
         *   ensure that the next search will proceed from the appropriate
         *   end of the range.
         */
        if (dir > 0)
            set_sel_range(ttf.chrgText.cpMin, ttf.chrgText.cpMax);
        else
            set_sel_range(ttf.chrgText.cpMax, ttf.chrgText.cpMin);

        /* indicate success */
        return TRUE;
    }

    /* not found */
    return FALSE;
}

/*
 *   perform a search, filling in TextToFind with the target information
 */
int ScintillaWin::search(const textchar_t *txt, int exact, int from_top,
                         int wrap, int dir, int regex, int whole_word,
                         TextToFind *ttf)
{
    int flags;
    int found;
    int textlen = call_sci(SCI_GETTEXTLENGTH);

    /* set up the search range based on the starting selection */
    if (dir > 0)
    {
        /* search from the current position to the end of the text */
        ttf->chrg.cpMax = textlen;
    }
    else
    {
        /* search from just before the selection to the start of the text */
        ttf->chrg.cpMin = ttf->chrg.cpMin;
        ttf->chrg.cpMax = 0;
    }

    /* if searching from the top, set up there */
    if (from_top)
        ttf->chrg.cpMin = (dir > 0 ? 0 : textlen);

    /* set the search text */
    ttf->lpstrText = (textchar_t *)txt;

    /* build the search flags */
    flags = 0;
    if (exact)
        flags |= SCFIND_MATCHCASE;
    if (regex)
        flags |= SCFIND_REGEXP | SCFIND_POSIX;
    if (whole_word)
        flags |= SCFIND_WHOLEWORD;

    /* do the search */
    found = call_sci(SCI_FINDTEXT, flags, (LPARAM)ttf);

    /* if we didn't find it, try wrapping */
    if (found == -1 && wrap)
    {
        /*
         *   reset the search so we search from the beginning of the document
         *   (if going forward) or the end (if searching backward),
         *   continuing to the original position
         */
        ttf->chrg.cpMax = dir > 0 ? textlen : 0;
        ttf->chrg.cpMin = dir > 0 ? 0 : textlen;

        /* re-run the search in the new range */
        found = call_sci(SCI_FINDTEXT, flags, (LPARAM)ttf);
    }

    /* return true if found, false if not found */
    return (found != -1);
}

/*
 *   replace
 */
void ScintillaWin::find_replace_sel(const textchar_t *txt, int dir, int regex)
{
    /* set the target to the current selection */
    call_sci(SCI_TARGETFROMSELECTION);

    /* replace the target */
    call_sci(regex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
             -1, (LPARAM)txt);

    /*
     *   move the selection to the beginning of the replacement if going
     *   backwards, or to the end if going forwards
     */
    int pos = call_sci(dir > 0 ? SCI_GETTARGETEND : SCI_GETTARGETSTART);
    call_sci(SCI_SETSEL, pos, pos);
}

/*
 *   replace all
 */
void ScintillaWin::find_replace_all(
    const textchar_t *txt, const textchar_t *repl,
    int exact, int wrap, int dir, int regex, int whole_word)
{
    TextToFind ttf;

    /* group this for undo purposes */
    call_sci(SCI_BEGINUNDOACTION);

    /* start with the current selection */
    ttf.chrg.cpMin = ttf.chrg.cpMax = call_sci(SCI_GETCURRENTPOS);

    /* repeatedly search and replace until we can't find a match */
    for (;;)
    {
        /* do the search; if we don't find a match, we're done */
        if (!search(txt, exact, FALSE, wrap, dir, regex, whole_word, &ttf))
            break;

        /* set the target to the match range */
        call_sci(SCI_SETTARGETSTART, ttf.chrgText.cpMin);
        call_sci(SCI_SETTARGETEND, ttf.chrgText.cpMax);

        /* replace it */
        call_sci(regex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                 -1, (LPARAM)repl);

        /*
         *   set the next search to the start or end of the target, depending
         *   on direction
         */
        ttf.chrg.cpMin =
            (dir > 0
             ? call_sci(SCI_POSITIONAFTER, ttf.chrgText.cpMax)
             : call_sci(SCI_POSITIONBEFORE, ttf.chrgText.cpMin));
    }

    /* end the undo group */
    call_sci(SCI_ENDUNDOACTION);
}

/* ------------------------------------------------------------------------ */
/*
 *   Incremental search
 */

/* begin an i-search */
void ScintillaWin::isearch_begin(SciIncSearchCtx *ctx)
{
    /* remember the starting position; remove any selection */
    ctx->startpos = call_sci(SCI_GETCURRENTPOS);
    call_sci(SCI_SETSEL, ctx->startpos, ctx->startpos);

    /* clear out the mode flags */
    ctx->dir = 1;
    ctx->regex = FALSE;
    ctx->exact = FALSE;
    ctx->wrapcnt = 0;
    ctx->found = TRUE;
    ctx->whole_word = FALSE;
}

/* update the search text in an i-search in progress */
int ScintillaWin::isearch_update(SciIncSearchCtx *ctx,
                                 const textchar_t *query,
                                 int dir, int regex, int exact,
                                 int whole_word)
{
    /* if the query is empty, just return to the starting position */
    if (query == 0 || query[0] == '\0')
    {
        call_sci(SCI_SETSEL, ctx->startpos, ctx->startpos);
        ctx->found = TRUE;
        return TRUE;
    }

    /* remember the new mode flags */
    ctx->dir = dir;
    ctx->regex = regex;
    ctx->exact = exact;
    ctx->whole_word = whole_word;

    /* search from the starting position */
    return isearch(ctx, query, ctx->startpos);
}


/* go the next/previous match */
int ScintillaWin::isearch_next(struct SciIncSearchCtx *ctx,
                               const textchar_t *query, int dir)
{
    long pos;

    /* if we're reversing direction, just stay put for now */
    if (ctx->dir != dir)
    {
        ctx->dir = dir;
        return ctx->found;
    }

    /* if the last attempt failed, try a wrapped search */
    if (!ctx->found)
    {
        /* count the wrapping */
        ctx->wrapcnt += 1;

        /* search from the beginning or end of the buffer */
        pos = (dir > 0 ? 0 : call_sci(SCI_GETTEXTLENGTH));
    }
    else
    {
        /* run the search from the start or end of the current selection */
        long a, b;
        get_sel_range(&a, &b);
        pos = (dir > 0 ? b : a);
    }

    /* run the search */
    return isearch(ctx, query, pos);
}

/*
 *   Run an incremental search from the given starting point
 */
int ScintillaWin::isearch(SciIncSearchCtx *ctx, const textchar_t *query,
                          int startpos)
{
    int flags;
    TextToFind ttf;

    /* set up the search flags */
    flags = 0;
    if (ctx->regex)
        flags |= SCFIND_REGEXP | SCFIND_POSIX;
    if (ctx->exact)
        flags |= SCFIND_MATCHCASE;
    if (ctx->whole_word)
        flags |= SCFIND_WHOLEWORD;

    /* set up the search range - start over at the starting position */
    if (ctx->dir > 0)
    {
        ttf.chrg.cpMin = startpos;
        ttf.chrg.cpMax = call_sci(SCI_GETTEXTLENGTH);
    }
    else
    {
        ttf.chrg.cpMin = startpos;
        ttf.chrg.cpMax = 0;
    }

    /* set the text */
    ttf.lpstrText = (char *)query;

    /* run the search */
    ctx->found = (call_sci(SCI_FINDTEXT, flags, (LPARAM)&ttf) != -1);

    /* if we found a match, select it */
    if (ctx->found)
    {
        /* select the match */
        call_sci(SCI_SETSEL, ttf.chrgText.cpMin, ttf.chrgText.cpMax);
    }
    else
    {
        /*
         *   otherwise, cancel the current selection, leaving the caret at
         *   the last position
         */
        int pos = call_sci(SCI_GETCURRENTPOS);
        call_sci(SCI_SETSEL, pos, pos);
    }

    /* return the 'found' indication */
    return ctx->found;
}

/* cancel the i-search and return to the starting position */
void ScintillaWin::isearch_cancel(struct SciIncSearchCtx *ctx)
{
    call_sci(SCI_SETSEL, ctx->startpos, ctx->startpos);
}


/* ------------------------------------------------------------------------ */
/*
 *   Print the document
 */
void ScintillaWin::print_doc(
    HDC hdc, const POINT *pgsize, const RECT *margins,
    const RECT *phys_margins, int selection,
    int first_page, int last_page)
{
    RangeToFormat rtf;
    int pagenum;
    long start, end;

    /* set printing to colored text on a white background */
    call_sci(SCI_SETPRINTCOLOURMODE, SC_PRINT_COLOURONWHITE);

    /*
     *   figure the length to print - this is either the whole document or
     *   just the selection, depending on the option settings
     */
    start = 0;
    end = call_sci(SCI_GETLENGTH);
    if (selection)
    {
        long a, b;
        get_sel_range(&a, &b);
        if (a <= b)
        {
            start = max(a, 0);
            end = min(b, end);
        }
        else
        {
            start = max(b, 0);
            end = min(a, end);
        }
    }

    /* set up the print range structure */
    rtf.hdc = hdc;
    rtf.hdcTarget = hdc;
    rtf.rc.left = margins->left - phys_margins->left;
    rtf.rc.top = margins->top - phys_margins->top;
    rtf.rc.right = pgsize->x - margins->right - phys_margins->left;
    rtf.rc.bottom = pgsize->y - margins->bottom - phys_margins->top;
    rtf.rcPage.left = 0;
    rtf.rcPage.top = 0;
    rtf.rcPage.right =
        pgsize->x - phys_margins->left - phys_margins->right - 1;
    rtf.rcPage.bottom =
        pgsize->y - phys_margins->top - phys_margins->bottom - 1;

    /* print the pages */
    for (pagenum = 1 ; start < end && pagenum <= last_page ; ++pagenum)
    {
        char msg[128];

        /* print this page only if it's in the page range */
        int print_page = (pagenum >= first_page && pagenum <= last_page);

        /* show a status message, in case this is a long document */
        sprintf(msg, "%sing page %d...",
                print_page ? "Print" : "Skipp", pagenum);
        parent_->isp_lrp_status(msg);

        /* start the page if it's in the printing range */
        if (print_page)
            StartPage(hdc);

        /* format the page */
        rtf.chrg.cpMin = start;
        rtf.chrg.cpMax = end;
        start = call_sci(SCI_FORMATRANGE, print_page, (LPARAM)&rtf);

        /* end the page if we printed it */
        if (print_page)
            EndPage(hdc);
    }

    /* reset the printing in the editor */
    call_sci(SCI_FORMATRANGE, FALSE, 0);

    /* turn off the status message */
    parent_->isp_lrp_status(0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a suitable caret color for a given background color.  For most
 *   colors, we'll use the RGB inverse.  However, this doesn't work well for
 *   mid-level grays, because the RGB inverse of a mid-level gray is another
 *   mid-level gray; so for those we'll simply use black instead.
 */
static COLORREF caret_for_bkg(COLORREF bkg)
{
    /* if it's a mid-level gray, use black; otherwise use the RGB inverse */
    unsigned r = (bkg >> 16) & 0xff;
    unsigned g = (bkg >> 8) & 0xff;
    unsigned b = bkg & 0xff;
    if (r >= 0x70 && g >= 0x70 && b >= 0x70
        && r <= 0x90 && g <= 0x90 && b <= 0x90)
        return RGB(0, 0, 0);
    else
        return ~bkg & 0x00ffffff;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the base font - this sets the font for the 'default' style, and
 *   resets all other styles to this setting.
 */
void ScintillaWin::set_base_font(
    const char *font_name, int point_size,
    COLORREF fg_color, COLORREF bkg_color,
    int override_selection_colors,
    COLORREF sel_fg_color, COLORREF sel_bg_color)
{
    /* apply the settings as the default style */
    call_sci(SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)font_name);
    call_sci(SCI_STYLESETSIZE, STYLE_DEFAULT, point_size);
    call_sci(SCI_STYLESETFORE, STYLE_DEFAULT, COLORREF_to_sci(fg_color));
    call_sci(SCI_STYLESETBACK, STYLE_DEFAULT, COLORREF_to_sci(bkg_color));

    /* set all styles to the default style */
    call_sci(SCI_STYLECLEARALL);

    /* set the selected text colors */
    call_sci(SCI_SETSELFORE, override_selection_colors,
             COLORREF_to_sci(sel_fg_color));
    call_sci(SCI_SETSELBACK, TRUE,
             COLORREF_to_sci(override_selection_colors
                             ? sel_bg_color : RGB(0xc0,0xc0,0xc0)));

    /* set the caret to the inverse of the background color */
    call_sci(SCI_SETCARETFORE, COLORREF_to_sci(caret_for_bkg(bkg_color)));
}

void ScintillaWin::set_base_font(const CHtmlFontDesc *desc,
                                 const CHtmlFontDesc *sel_desc,
                                 int override_selection_colors)
{
    set_base_font(desc->face, desc->pointsize,
                  desc->default_color
                  ? GetSysColor(COLOR_WINDOWTEXT)
                  : HTML_color_to_COLORREF(desc->color),
                  desc->default_bgcolor
                  ? GetSysColor(COLOR_WINDOW)
                  : HTML_color_to_COLORREF(desc->bgcolor),
                  override_selection_colors,
                  sel_desc->default_color
                  ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                  : HTML_color_to_COLORREF(sel_desc->color),
                  sel_desc->default_bgcolor
                  ? GetSysColor(COLOR_HIGHLIGHT)
                  : HTML_color_to_COLORREF(sel_desc->bgcolor));
}

/*
 *   set indent and tab options
 */
void ScintillaWin::set_indent_options(int indent_size,
                                      int tab_size, int use_tabs)
{
    /* set our default indentation mode */
    call_sci(SCI_SETUSETABS, use_tabs);
    call_sci(SCI_SETTABWIDTH, tab_size);
    call_sci(SCI_SETINDENT, indent_size);
}

/*
 *   set wrapping options
 */
void ScintillaWin::set_wrapping_options(
    int right_margin, int auto_wrap, int show_guide)
{
    /* set the edge column to the right margin */
    call_sci(SCI_SETEDGECOLUMN, right_margin_ = right_margin);

    /* set the edge mode to show the guide if desired */
    call_sci(SCI_SETEDGEMODE, show_guide ? EDGE_LINE : EDGE_NONE);

    /* remember the auto-wrap mode */
    auto_wrap_ = auto_wrap;
}

/*
 *   show/hide line numbers
 */
void ScintillaWin::show_line_numbers(int show)
{
    /*
     *   line numbers are in margin 0 regardless of whether we're showing
     *   them; show or hide the margin by manipulating its display width
     */
    call_sci(SCI_SETMARGINWIDTHN, 0,
             show
             ? call_sci(SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)"_99999")
             : 0);
}

/*
 *   show/hide the folding controls
 */
void ScintillaWin::show_folding(int show)
{
    /*
     *   the folding controls are in margin 2; show or hide the margin by
     *   manipulating its width
     */
    call_sci(SCI_SETMARGINWIDTHN, 2, show ? 16 : 0);

    /* if we're turning off folding, make sure everything's visible */
    if (!show)
    {
        int cnt = call_sci(SCI_GETLINECOUNT);

        /* show all lines */
        call_sci(SCI_SHOWLINES, 0, cnt - 1);

        /* set every line to expanded */
        for (int i = 0 ; i < cnt ; ++i)
            call_sci(SCI_SETFOLDEXPANDED, i, TRUE);
    }
}

/*
 *   set a custom style
 */
void ScintillaWin::set_custom_style(
    int stylenum, const char *font_name, int point_size,
    COLORREF fgcolor, COLORREF bgcolor, int bold, int italic, int ul)
{
    /* set the font, if specified */
    if (font_name != 0 && font_name[0] != '\0')
        call_sci(SCI_STYLESETFONT, stylenum, (LPARAM)font_name);

    /* set the point size, if specified */
    if (point_size != 0)
        call_sci(SCI_STYLESETSIZE, stylenum, point_size);

    /* set the color */
    if (fgcolor != 0xffffffff)
        call_sci(SCI_STYLESETFORE, stylenum, COLORREF_to_sci(fgcolor));
    if (bgcolor != 0xffffffff)
    {
        /* set the background color */
        call_sci(SCI_STYLESETBACK, stylenum, COLORREF_to_sci(bgcolor));

        /*
         *   if this is the main "default" style, set the caret to the
         *   inverse of the background color
         */
        if (stylenum == STYLE_DEFAULT)
            call_sci(SCI_SETCARETFORE, COLORREF_to_sci(caret_for_bkg(bgcolor)));
    }

    /* set the attributes */
    call_sci(SCI_STYLESETBOLD, stylenum, bold);
    call_sci(SCI_STYLESETITALIC, stylenum, italic);
    call_sci(SCI_STYLESETUNDERLINE, stylenum, ul);
}

void ScintillaWin::set_custom_style(int stylenum, const CHtmlFontDesc *desc)
{
    set_custom_style(stylenum, desc->face, desc->pointsize,
                     desc->default_color
                     ? 0xFFFFFFFF : HTML_color_to_COLORREF(desc->color),
                     desc->default_bgcolor
                     ? 0xFFFFFFFF : HTML_color_to_COLORREF(desc->bgcolor),
                     desc->weight >= 700, desc->italic, desc->underline);
}

/*
 *   set the default for the custom styles
 */
void ScintillaWin::set_base_custom_style(
    const char *font_name, int point_size,
    COLORREF fgcolor, COLORREF bgcolor, int bold, int italic, int ul)
{
    /* set style STYLE_DEFAULT */
    set_custom_style(STYLE_DEFAULT, font_name, point_size,
                     fgcolor, bgcolor, bold, italic, ul);

    /* set this as the default */
    call_sci(SCI_STYLECLEARALL);
}

void ScintillaWin::set_base_custom_style(const CHtmlFontDesc *desc)
{
    set_base_custom_style(
        desc->face, desc->pointsize,
        desc->default_color
        ? 0xFFFFFFFF : HTML_color_to_COLORREF(desc->color),
        desc->default_bgcolor
        ? 0xFFFFFFFF : HTML_color_to_COLORREF(desc->bgcolor),
        desc->weight >= 700, desc->italic, desc->underline);
}

/* ------------------------------------------------------------------------ */
/*
 *   Append text
 */
void ScintillaWin::append_text(const char *txt, size_t len)
{
    /* add the text to the control */
    call_sci(SCI_APPENDTEXT, len, (int)txt);
}

/*
 *   open a line at the current position
 */
void ScintillaWin::open_line()
{
    static const char *nl[] = { "\r\n", "\r", "\n" };

    int pos = call_sci(SCI_GETCURRENTPOS);
    call_sci(SCI_SETSEL, pos, pos);
    call_sci(SCI_INSERTTEXT, pos,
             (LPARAM)nl[call_sci(SCI_GETEOLMODE)]);
    call_sci(SCI_SETSEL, pos, pos);
}

/*
 *   transpose characters around cursor
 */
void ScintillaWin::transpose_chars()
{
    /* get the next character */
    int pos = call_sci(SCI_GETCURRENTPOS);
    int ch = call_sci(SCI_GETCHARAT, pos);
    if (ch != 0)
    {
        /* group the operations for undo purposes */
        call_sci(SCI_BEGINUNDOACTION);

        /* remove any selection */
        int pos = call_sci(SCI_GETCURRENTPOS);
        call_sci(SCI_SETSEL, pos, pos);

        /* delete the next character */
        call_sci(SCI_CHARRIGHT);
        call_sci(SCI_DELETEBACK);
        call_sci(SCI_CHARLEFT);

        char str[1] = { (char)ch };
        call_sci(SCI_ADDTEXT, 1, (LPARAM)str);

        /* end of undo group */
        call_sci(SCI_ENDUNDOACTION);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Clear the entire document
 */
void ScintillaWin::clear_document()
{
    /* tell scintilla to clear the document contents */
    call_sci(SCI_CLEARALL);
}

/* ------------------------------------------------------------------------ */
/*
 *   Loading context.  This lets us save some state across a file reload, for
 *   restoration once the new copy is in memory.
 */
struct file_load_ctx
{
    /* current selection range */
    long sel_a;
    long sel_b;

    /* scrolling offset */
    int scroll_x;
    int scroll_y;
};

/*
 *   Handle the start and end of a file load
 */
void ScintillaWin::begin_file_load(void *&ctx0)
{
    /* flag that we're loading */
    loading_ = TRUE;

    /* allocate our context */
    file_load_ctx *ctx = new file_load_ctx();

    /* save it in the caller's stack */
    ctx0 = ctx;

    /* remember the current selection range */
    get_sel_range(&ctx->sel_a, &ctx->sel_b);

    /* remember the current scrolling position */
    ctx->scroll_x = call_sci(SCI_GETXOFFSET);
    ctx->scroll_y = call_sci(
        SCI_LINEFROMPOSITION, call_sci(SCI_POSITIONFROMPOINT, 0, 0));

    /* clear our document */
    clear_document();
}

void ScintillaWin::end_file_load(void *ctx0)
{
    /* reset scintilla's undo and modified states */
    reset_state();

    /* get our load context properly cast */
    file_load_ctx *ctx = (file_load_ctx *)ctx0;

    /* restore the original scroll position */
    call_sci(SCI_LINESCROLL, ctx->scroll_x, ctx->scroll_y);

    /* restore the original selection range */
    set_sel_range(ctx->sel_a, ctx->sel_b);

    /* done with the context */
    delete ctx;

    /* we're no longer loading */
    loading_ = FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Save the file
 */
int ScintillaWin::save_file(const char *fname)
{
    FILE *fp;
    TextRange tr;
    char buf[32768];
    int rem;
    int ok;

    /* open the file */
    if ((fp = fopen(fname, "wb")) == 0)
        return FALSE;

    /* get the total length of the document */
    rem = call_sci(SCI_GETTEXTLENGTH);

    /* initialize our text range to get the first chunk */
    tr.lpstrText = buf;
    tr.chrg.cpMin = 0;

    /* write the contents */
    for (rem = call_sci(SCI_GETTEXTLENGTH) ; rem != 0 ; )
    {
        int cur;

        /*
         *   load up one buffer-full, or the rest of the document, whichever
         *   is less
         */
        cur = rem;
        if (cur > (int)sizeof(buf) - 1)
            cur = sizeof(buf) - 1;

        /* read this chunk */
        tr.chrg.cpMax = tr.chrg.cpMin + cur;
        call_sci(SCI_GETTEXTRANGE, 0, (int)&tr);

        /* write it out */
        if (fwrite(buf, cur, 1, fp) != 1)
        {
            fclose(fp);
            return FALSE;
        }

        /* advance to the next chunk */
        tr.chrg.cpMin += cur;
        rem -= cur;
    }

    /* close the file and note the status */
    ok = (fclose(fp) != EOF);

    /* if successful, we're now synchronized with the disk copy */
    if (ok)
        call_sci(SCI_SETSAVEPOINT);

    /* return the success status */
    return ok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the line number at the cursor
 */
long ScintillaWin::get_cursor_linenum()
{
    long pos;

    /* get the cursor position */
    pos = (long)call_sci(SCI_GETCURRENTPOS);

    /* get the line number from the position, and return the result */
    return (long)call_sci(SCI_LINEFROMPOSITION, pos);
}

/*
 *   get the cursor column
 */
long ScintillaWin::get_cursor_column()
{
    long pos;

    /* get the cursor position */
    pos = (long)call_sci(SCI_GETCURRENTPOS);

    /* get the line number from the position, and return the result */
    return (long)call_sci(SCI_GETCOLUMN, pos);
}

/*
 *   get the document offset of the cursor position
 */
long ScintillaWin::get_cursor_pos()
{
    return call_sci(SCI_GETCURRENTPOS);
}

void ScintillaWin::set_cursor_pos(long pos)
{
    call_sci(SCI_SETCURRENTPOS, pos);
}

/*
 *   get the anchor position
 */
long ScintillaWin::get_anchor_pos()
{
    return call_sci(SCI_GETANCHOR);
}

void ScintillaWin::set_anchor_pos(long pos)
{
    call_sci(SCI_SETANCHOR, pos);
}

/*
 *   get the current selection range
 */
void ScintillaWin::get_sel_range(long *a, long *b)
{
    /* get the position and anchor */
    *a = call_sci(SCI_GETANCHOR);
    *b = call_sci(SCI_GETCURRENTPOS);
}

/*
 *   set the current selection
 */
void ScintillaWin::set_sel_range(long a, long b)
{
    /* treat ~0 as the end of the document */
    if ((unsigned long)a == ~0U)
        a = call_sci(SCI_GETTEXTLENGTH);
    if ((unsigned long)b == ~0U)
        b = call_sci(SCI_GETTEXTLENGTH);

    /* tell scintilla to center the cursor after the jump */
    call_sci(SCI_SETYCARETPOLICY, CARET_JUMPS | CARET_EVEN);

    /* scroll the current position into view, then set the anchor */
    call_sci(SCI_GOTOPOS, b);
    call_sci(SCI_SETANCHOR, a);
    call_sci(SCI_CHOOSECARETX);

    /* restore our standard caret policy */
    set_std_caret_policy();
}

/*
 *   select the current line
 */
void ScintillaWin::select_current_line()
{
    /* get the bounds of the current line */
    long l = get_cursor_linenum();
    long start = call_sci(SCI_POSITIONFROMLINE, l);
    long end = call_sci(SCI_GETLINEENDPOSITION, l);

    /* select this range */
    set_sel_range(start, end);
}

/*
 *   get the word boundaries around the given position
 */
void ScintillaWin::get_word_boundaries(long *start, long *end, long pos)
{
    *start = call_sci(SCI_WORDSTARTPOSITION, pos, TRUE);
    *end = call_sci(SCI_WORDENDPOSITION, pos, TRUE);
}

/*
 *   get the expression boundaries around the given position
 */
void ScintillaWin::get_expr_boundaries(long *start, long *end, long pos,
                                       int func)
{
    int endpos;
    int ch;
    long dummy;

    /* first, get the simple expression boundaries */
    get_simple_expr_boundaries(start, end, pos);

    /*
     *   Now add prefix expressions until we either get too large a selection
     *   or we run out of prefix expressions.
     */
    for (endpos = (*start > 512 ? *start - 512 : 0) ; *start > endpos ; )
    {
        long start0 = *start;

        /* find the first non-blank character before the current start */
        pos = get_prev_non_blank(*start);
        ch = call_sci(SCI_GETCHARAT, pos);

        /*
         *   If the result range starts with '[', and the first non-blank
         *   character before the '[' is a non-operator, get the expression
         *   boundaries of the simple expression before the '['.
         */
        if (call_sci(SCI_GETCHARAT, *start) == '['
            && pos > 0
            && (isalpha(ch) || isdigit(ch) || strchr("'[]()", ch) != 0))
        {
            /* extend the expression to include this prefix */
            get_simple_expr_boundaries(start, &dummy, pos);

            /*
             *   go back for another round, in case there's another prefix
             *   expression before this one
             */
            if (*start != start0)
                continue;
        }

        /*
         *   If the first non-blank character before the result range is '.',
         *   get the expression boundaries of the simple expression before
         *   the '.'.
         */
        if (pos > 0 && ch == '.')
        {
            /* back up to the first non-blank character before the '.' */
            long pos1 = get_prev_non_blank(pos);

            /* get the prefix expression boundaries */
            if (pos1 > 0)
            {
                /* get the bounds of the preceding expression */
                get_simple_expr_boundaries(start, &dummy, pos1);

                /* go back for another round */
                if (*start != start0)
                    continue;
            }
        }

        /*
         *   if we got this far, it means that we didn't find any prefix to
         *   add to the expression on this round, so we're done
         */
        break;
    }

    /*
     *   If they want function extension, check for parens after the end of
     *   the expression.
     */
    if (func)
    {
        /* start at the end of the result range */
        pos = *end;

        /* look ahead a ways, but keep it within reason */
        endpos = call_sci(SCI_GETLENGTH);
        if (pos + 512 < endpos)
            endpos = pos + 512;

        /* find the next non-blank character */
        for ( ; pos < endpos ; pos = call_sci(SCI_POSITIONAFTER, pos))
        {
            ch = call_sci(SCI_GETCHARAT, pos);
            if (isprint(ch) && !isspace(ch))
                break;
        }

        /* if we're at an open paren, include the parenthesized suffix */
        if (ch == '(')
            get_simple_expr_boundaries(&dummy, end, pos);
    }
}

/*
 *   get the position of the first non-blank character before the given
 *   position
 */
long ScintillaWin::get_prev_non_blank(long pos)
{
    /*
     *   keep going until we reach the beginning of the buffer or find our
     *   non-blank character
     */
    while (pos > 0)
    {
        /* move back one position */
        pos = call_sci(SCI_POSITIONBEFORE, pos);

        /* if it's printable and not a space, it's what we're looking for */
        int ch = call_sci(SCI_GETCHARAT, pos);
        if (isprint(ch) && !isspace(ch))
            break;
    }

    /* return the result */
    return pos;
}

/*
 *   Get the simple expression boundaries.  If the character at pos is a
 *   paren or square bracket, the simple expression is the part between pos
 *   and the matching delimiter; otherwise, it's simply the word containing
 *   pos.
 */
void ScintillaWin::get_simple_expr_boundaries(long *start, long *end, long pos)
{
    /*
     *   if we're at a parenthesis, find the matching paren; otherwise, find
     *   the boundaries of the current word
     */
    if (strchr("[]()", call_sci(SCI_GETCHARAT, pos)) != 0)
        get_paren_boundaries(start, end, pos);
    else
        get_word_boundaries(start, end, pos);
}

/*
 *   Get the boundaries of the parenthesized or square-bracketed expression
 *   at pos.  We'll seek forward or backward to find the matching delimiter;
 *   the bounds are the part between and including the matched delimiters.
 */
void ScintillaWin::get_paren_boundaries(long *start, long *end, long pos)
{
    int delim, open, close, cur;
    int qu = 0;
    int endpos;
    int p;
    int level = 1;
    int nxtcmd;
    int dir;
    const int limit = 255;

    /* get the initial delimiter */
    delim = call_sci(SCI_GETCHARAT, pos);
    if (strchr("[]", delim) != 0)
        open = '[', close = ']';
    else if (strchr("()", delim) != 0)
        open = '(', close = ')';
    else
    {
        /* we're not at a delimiter after all - just get the word bounds */
        get_word_boundaries(start, end, pos);
        return;
    }

    /* now work forward or backward to find the matching delimiter */
    if (delim == open)
    {
        /* scan forward for the matching delimiter */
        nxtcmd = SCI_POSITIONAFTER;
        dir = 1;

        /*
         *   limit the scan to a reasonable distance, to avoid pathological
         *   contexts where we'd loop for a really long time
         */
        endpos = call_sci(SCI_GETLENGTH);
        if (pos + limit < endpos)
            endpos = pos + limit;
    }
    else
    {
        /* scan backwards for the matching delimiter */
        nxtcmd = SCI_POSITIONBEFORE;
        dir = -1;
        endpos = (pos > limit ? pos - limit : 0);
    }

    /* scan for the matching delimiter */
    for (p = pos ; level != 0 && (dir > 0 ? p < endpos : p > endpos) ; )
    {
        /* move ahead/back one character */
        p = call_sci(nxtcmd, p);

        /* check what we have here */
        switch (cur = call_sci(SCI_GETCHARAT, p))
        {
        case '"':
        case '\'':
            /* ignore the quote if we're in a string and it's not a match */
            if (qu != 0 && cur != qu)
                break;

            /* if we're in a string, ignore escaped quotes */
            if (qu != 0
                && call_sci(SCI_GETCHARAT,
                            call_sci(SCI_POSITIONBEFORE, p)) == '\\')
                break;

            /* it's a significant quote - switch string states */
            qu = cur - qu;
            break;

        case '(':
        case '[':
            /* consider our opening delimiter outside of strings */
            if (cur == open && qu == 0)
                level += dir;
            break;

        case ')':
        case ']':
            /* consider our closing delimiter outside of strings */
            if (cur == close && qu == 0)
                level -= dir;
            break;
        }
    }

    /*
     *   If we found the matching delimiter (we did if we reached level 0),
     *   the expression boundaries are from pos to p (or vice versa).
     *
     *   If we stopped before finding the matching delimiter, we must have
     *   run out of buffer, or out of patience.  (This whole process is a bit
     *   heuristic, since we don't know our actual token context when we
     *   start - we could be in the middle of a string or comment, for
     *   instance; to avoid pathological cases in weird contexts, we limit
     *   the scan to an arbitrary maximum distance from our starting point.)
     *   In either case, simply fall back on the word boundaries as our
     *   result.
     */
    if (level == 0)
    {
        /* found the match - the range is from p to pos or pos to p */
        if (p <= pos)
            *start = p, *end = call_sci(SCI_POSITIONAFTER, pos);
        else
            *start = pos, *end = call_sci(SCI_POSITIONAFTER, p);
    }
    else
    {
        /* didn't find the match - just use the word boundaries */
        get_word_boundaries(start, end, pos);
    }
}

/*
 *   retrieve the text in the given range
 */
void ScintillaWin::get_text(char *buf, long start, long end)
{
    TextRange tr;

    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = buf;
    call_sci(SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
}

/*
 *   get the number of lines in the document
 */
long ScintillaWin::get_line_count()
{
    /* return the line number containing the end of the document */
    return call_sci(SCI_LINEFROMPOSITION, call_sci(SCI_GETLENGTH));
}

/*
 *   get the text of the given line
 */
void ScintillaWin::get_line_text(CStringBuf *buf, long linenum)
{
    TextRange tr;

    /* get teh boundaries of the given line */
    tr.chrg.cpMin = call_sci(SCI_POSITIONFROMLINE, linenum);
    tr.chrg.cpMax = call_sci(SCI_GETLINEENDPOSITION, linenum);

    /* make sure the buffer is big enough to hold the text */
    buf->ensure_size(tr.chrg.cpMax - tr.chrg.cpMin + 1);

    /* retrieve the text */
    tr.lpstrText = buf->get();
    call_sci(SCI_GETTEXTRANGE, 0, (LPARAM)&tr);
}


/* ------------------------------------------------------------------------ */
/*
 *   Create a spot
 */
void *ScintillaWin::create_spot(long pos)
{
    sci_spot *spot;

    /* create the spot */
    spot = new sci_spot(pos);

    /* link it into the spot list */
    spot->nxt = spots_;
    spots_ = spot;

    /* return the new spot object */
    return spot;
}

/*
 *   delete a spot
 */
void ScintillaWin::delete_spot(void *spot)
{
    sci_spot *cur, *prv;

    /* unlink it */
    for (prv = 0, cur = spots_ ; cur != 0 ; prv = cur, cur = cur->nxt)
    {
        /* if this is the one, unlink it */
        if (cur == (sci_spot *)spot)
        {
            /* unlink it from the list */
            if (prv != 0)
                prv->nxt = cur->nxt;
            else
                spots_ = cur->nxt;

            /* delete the memory */
            delete cur;

            /* no need to look any further */
            break;
        }
    }
}

/*
 *   set the position of a spot
 */
void ScintillaWin::set_spot_pos(void *spot, long pos)
{
    sci_spot *cur;

    /* find the spot in our list */
    for (cur = spots_ ; cur != 0 ; cur = cur->nxt)
    {
        /* if this is the spot, set its position and stop looking */
        if (cur == (sci_spot *)spot)
        {
            cur->pos = pos;
            break;
        }
    }
}

/*
 *   get the position of a spot
 */
long ScintillaWin::get_spot_pos(void *spot)
{
    sci_spot *cur;

    /* find the spot in our list */
    for (cur = spots_ ; cur != 0 ; cur = cur->nxt)
    {
        /* if this is the spot, return its position */
        if (cur == (sci_spot *)spot)
            return cur->pos;
    }

    /* didn't find it */
    return -1;
}


/* ------------------------------------------------------------------------ */
/*
 *   Go to the given line
 */
void ScintillaWin::go_to_line(long l)
{
    /* tell scintilla to center the cursor after the jump */
    call_sci(SCI_SETYCARETPOLICY, CARET_JUMPS | CARET_EVEN);

    /* go to the line */
    call_sci(SCI_GOTOLINE, (WPARAM)l);

    /* restore our standard caret policy */
    set_std_caret_policy();
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the line status
 */
void ScintillaWin::set_line_status(long linenum, unsigned int stat)
{
    /* clear the existing markers */
    for (int i = MARKER_FIRST_STAT ; i <= MARKER_LAST_STAT ; ++i)
        call_sci(SCI_MARKERDELETE, linenum, i);

    /* set the markers */
    call_sci(SCI_MARKERADDSET, linenum, stat);
}

/*
 *   clear all line status indicators
 */
void ScintillaWin::reset_line_status()
{
    /* find each line with a marker, and delete its markers */
    for (int l = 0 ;
         (l = call_sci(SCI_MARKERNEXT, l, MARKER_ALL_STAT)) != -1 ; ++l)
    {
        /* delete markers on this line */
        for (int j = MARKER_FIRST_STAT ; j <= MARKER_LAST_STAT ; ++j)
            call_sci(SCI_MARKERDELETE, l, j);
    }
}

/*
 *   find the next line containing any of the given markers
 */
int ScintillaWin::next_marker_line(int startline, unsigned int mask,
                                   unsigned int *found_markers)
{
    /* find the next line */
    int l = call_sci(SCI_MARKERNEXT, startline, mask);

    /* if we found it, get the markers for this line */
    if (l != -1 && found_markers != 0)
        *found_markers = call_sci(SCI_MARKERGET, l);

    /* return the line number */
    return l;
}



/* ------------------------------------------------------------------------ */
/*
 *   Add a bookmark at the current cursor position.  Returns the bookmark
 *   handle.
 */
int ScintillaWin::add_bookmark(int linenum)
{
    return call_sci(SCI_MARKERADD, linenum, MARKER_BOOKMARK);
}

/*
 *   Get the line number for a given bookmark handle
 */
int ScintillaWin::get_bookmark_line(int handle)
{
    return call_sci(SCI_MARKERLINEFROMHANDLE, handle);
}

/*
 *   delete a bookmark
 */
void ScintillaWin::delete_bookmark(int handle)
{
    call_sci(SCI_MARKERDELETEHANDLE, handle);
}

/*
 *   jump to the next bookmark after the current cursor positionn
 */
int ScintillaWin::jump_to_next_bookmark(int dir)
{
    /* find the next bookmark marker after the current line */
    int l = get_cursor_linenum();
    l = call_sci(dir > 0 ? SCI_MARKERNEXT : SCI_MARKERPREVIOUS,
                 l + (dir > 0 ? 1 : -1),
                 1 << MARKER_BOOKMARK);

    /* if we found it, go there */
    if (l != -1)
    {
        /* we found a bookmark - jump to it */
        l = call_sci(SCI_POSITIONFROMLINE, l);
        call_sci(SCI_SETSEL, l, l);

        /* tell the caller we found a bookmark */
        return TRUE;
    }
    else
    {
        /* no more bookmarks - just tell the caller */
        return FALSE;
    }
}

/*
 *   jump to the first bookmark in the file
 */
void ScintillaWin::jump_to_first_bookmark()
{
    /* find the first bookmark marker */
    int l = call_sci(SCI_MARKERNEXT, 0, 1 << MARKER_BOOKMARK);
    if (l != -1)
    {
        /* we found a bookmark - go there */
        l = call_sci(SCI_POSITIONFROMLINE, l);
        call_sci(SCI_SETSEL, l, l);
    }
}

/*
 *   jump to the last bookmark in the file
 */
void ScintillaWin::jump_to_last_bookmark()
{
    /* find the first bookmark marker */
    int l = call_sci(SCI_GETLINECOUNT);
    l = call_sci(SCI_MARKERPREVIOUS, l, 1 << MARKER_BOOKMARK);
    if (l != -1)
    {
        /* we found a bookmark - go there */
        l = call_sci(SCI_POSITIONFROMLINE, l);
        call_sci(SCI_SETSEL, l, l);
    }
}

/*
 *   jump to the given bookmark
 */
void ScintillaWin::jump_to_bookmark(int handle)
{
    /* find the bookmark by handle */
    int l = call_sci(SCI_MARKERLINEFROMHANDLE, handle);

    /* if we found it, go there */
    if (l != -1)
    {
        l = call_sci(SCI_POSITIONFROMLINE, l);
        call_sci(SCI_SETSEL, l, l);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Set the wrapping mode
 */
void ScintillaWin::set_wrap_mode(char mode)
{
    int m;

    switch (mode)
    {
    case 'N':
    default:
        m = SC_WRAP_NONE;
        break;

    case 'C':
        m = SC_WRAP_CHAR;
        break;

    case 'W':
        m = SC_WRAP_WORD;
        break;
    }

    /*
     *   set the wrap mode; show the flags at the end of the wrapping line
     *   near the border
     */
    call_sci(SCI_SETWRAPMODE, m);
    call_sci(SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END);
    call_sci(SCI_SETWRAPVISUALFLAGSLOCATION, SC_WRAPVISUALFLAGLOC_DEFAULT);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clipboard/selection operations
 */

void ScintillaWin::select_all()
{
    call_sci(SCI_SELECTALL);
}

void ScintillaWin::selection_mode(int mode)
{
    /* cancel any previous mode, and set the new mode */
    cancel_sel_mode(TRUE);
    call_sci(SCI_SETSELECTIONMODE, mode);

    /* note that we're in selection mode */
    selmode_ = TRUE;
}

int ScintillaWin::get_selection_mode()
{
    return (selmode_ ? call_sci(SCI_GETSELECTIONMODE) : -1);
}

void ScintillaWin::cancel_sel_mode(int cancel_sel)
{
    /*
     *   if we're in a selection mode, and they want to cancel the selection,
     *   collapse the selection to the current caret position
     */
    if (selmode_ && cancel_sel)
    {
        int pos = call_sci(SCI_GETCURRENTPOS);
        call_sci(SCI_SETSEL, pos, pos);
    }

    /* cancel the mode */
    call_sci(SCI_CANCEL);
    selmode_ = FALSE;
}

void ScintillaWin::cut()
{
    /* cut the selected text onto the clipboard */
    cut_or_copy(TRUE);
}

void ScintillaWin::cut_eol()
{
    int pos = call_sci(SCI_GETCURRENTPOS);
    int combining;

    /* if we're not combining the cut with the last one, clear the buffer */
    if (pos != last_cut_eol_pos_)
    {
        cut_eol_buf_.clear();
        cut_eol_len_ = 0;
    }

    /*
     *   If the cut buffer is non-empty, we're combining this cut with a
     *   previous cut.  This is effectively a continuation of the previous
     *   cut-eol command, we don't want to create a new clipboard stack level
     *   for the new text - instead, we simply want to replace the current
     *   top-of-stack with the new combined cut text.
     */
    combining = (cut_eol_len_ != 0);

    /* get the end of this line */
    int l = call_sci(SCI_LINEFROMPOSITION, pos);
    int endpos = call_sci(SCI_GETLINEENDPOSITION, l);

    /*
     *   if we're at EOL, the text is the newline; otherwise, it's the rest
     *   of the line up to but not including the newline
     */
    if (endpos == pos)
        endpos = call_sci(SCI_POSITIONAFTER, endpos);

    /* note the length of the text we're cutting */
    size_t len = endpos - pos;

    /* get this text */
    TextRange tr;
    tr.chrg.cpMin = pos;
    tr.chrg.cpMax = endpos;
    tr.lpstrText = new char[len + 1];
    call_sci(SCI_GETTEXTRANGE, 0, (LPARAM)&tr);

    /* append it to our cut buffer */
    cut_eol_buf_.append_at(cut_eol_len_, tr.lpstrText, len);

    /* note the new length */
    cut_eol_len_ += len;

    /* done with the text range buffer */
    delete [] tr.lpstrText;

    /*
     *   push it onto the clipboard stack - unless we're combining this with
     *   a previous cut-eol, in which case just replace the existing
     *   top-of-stack
     */
    add_clipboard(cut_eol_buf_.get(), cut_eol_len_, !combining, FALSE);

    /* select the text */
    call_sci(SCI_SETSEL, pos, endpos);

    /* if the selection is non-empty, delete it */
    clear_selection();

    /* remember the last cut-eol position, for combining next time */
    last_cut_eol_pos_ = call_sci(SCI_GETCURRENTPOS);
}

/*
 *   Cut or copy the current selection
 */
void ScintillaWin::cut_or_copy(int cut)
{
    CStringBuf buf;
    size_t len = call_sci(SCI_GETSELTEXT, 0, 0);

    /* retrieve the selected text */
    buf.ensure_size(len);
    call_sci(SCI_GETSELTEXT, 0, (LPARAM)buf.get());

    /* push it onto the clipboard stack and copy to the system clipboard */
    add_clipboard(buf.get(), len - 1, TRUE, FALSE);

    /* if cutting, and the selection is non-empty, clear the selection */
    if (cut && len != 0)
        call_sci(SCI_CLEAR);
}

/*
 *   Copy the system clipboard into our clipboard stack, if applicable
 */
void ScintillaWin::read_sys_clipboard()
{
    HANDLE hdl = 0;
    const char *buf = 0;
    size_t len;

    /* get access to the clipboard */
    if (!OpenClipboard(hwnd_))
        return;

    /* if there's no text in the clipboard, we can't do any pasting */
    hdl = GetClipboardData(CF_TEXT);
    if (hdl == 0)
        goto done;

    /* lock the handle to get the text */
    buf = (const char *)GlobalLock(hdl);
    if (buf == 0)
        goto done;

    /* get the length of the data */
    len = strlen(buf);

    /*
     *   If the current top of our clipboard stack matches what's on the
     *   system clipboard, leave our stack as it is, since we must have put
     *   this data there in the first place.  (And even if we didn't, the
     *   effect is the same, so don't bother changing anything.)
     *
     *   Otherwise, just add the clipboard data to the stack.
     */

    /*
     *   If it's an exact match for what's at the top of the stack, don't
     *   bother replacing it.
     */
    if (cbs_cnt_ != 0
        && cb_stack_[cbs_top_].len == len
        && memcmp(cb_stack_[cbs_top_].txt.get(), buf, len) == 0)
    {
        /* it's an exact match - don't bother saving it again */
        goto done;
    }

    /* add the data to the stack */
    add_clipboard(buf, len, TRUE, TRUE);

done:
    /* done with the clipboard data */
    if (hdl != 0)
        GlobalUnlock(hdl);

    /* done with the clipboard */
    CloseClipboard();
}

/*
 *   Add text to the clipboard stack
 */
void ScintillaWin::add_clipboard(const textchar_t *txt, size_t len,
                                 int push, int from_sys)
{
    /*
     *   If the data at the top of the stack came from the system clipboard,
     *   always replace it, even if the caller wanted to push the new data.
     *   System clipboard data never stacks; as soon as we copy anything new
     *   to the clipboard, whether from within this app or from another app,
     *   we always replace any system clipboard data that was present.
     */
    if (cbs_cnt_ != 0 && cb_stack_[cbs_top_].from_sys)
        push = FALSE;

    /* check for pushing or replacing */
    if (push)
    {
        /* pushing the text - create a new stack level */
        if (++cbs_top_ == CB_STACK_SIZE)
            cbs_top_ = 0;

        /*
         *   count it; if the stack is already full, leave the count alone,
         *   which will effectively discard the oldest existing element
         */
        if (cbs_cnt_ < CB_STACK_SIZE)
            ++cbs_cnt_;
    }
    else
    {
        /*
         *   we're simply replacing the top element; the only thing we need
         *   to do is to make sure the stack isn't empty, and note that we
         *   now have one element if it is
         */
        if (cbs_cnt_ == 0)
        {
            cbs_top_ = 0;
            cbs_cnt_ = 1;
        }
    }

    /* put the text in the stack */
    cb_stack_[cbs_top_].set(txt, len, from_sys);

    /*
     *   if this clipping didn't come from the system clipboard to start
     *   with, copy it into the system clipboard as well
     */
    if (!from_sys)
        call_sci(SCI_COPYTEXT, len, (LPARAM)txt);
}

void ScintillaWin::copy()
{
    /* copy the selection */
    cut_or_copy(FALSE);

    /*
     *   cancel the selection mode - copying fairly explicitly makes use of
     *   the selection, so it should be safe to assume that we're done with a
     *   sticky selection mode
     */
    cancel_sel_mode(TRUE);
}

void ScintillaWin::paste()
{
    /*
     *   check for new contents on the system clipboard - if the system
     *   clipboard has changed since the last time we checked, we need to
     *   copy its contents into our stack
     */
    read_sys_clipboard();

    /* check for something in our clipboard stack */
    if (cbs_cnt_ == 0)
    {
        /*
         *   our internal clipboard stack is empty, so do an ordinary
         *   Scintilla Paste, since there might be something in the system
         *   clipboard
         */
        call_sci(SCI_PASTE);
    }
    else
    {
        /*
         *   Our private clipboard stack has data, so paste from there.
         */

        /* we need to make a couple of Sci calls, so group them for undo */
        call_sci(SCI_BEGINUNDOACTION);

        /* if the selection is non-empty, clear it */
        clear_selection();

        /* note where we're inserting the new text */
        int pos = call_sci(SCI_GETCURRENTPOS);

        /* insert the text from the top of the clipboard stack */
        call_sci(SCI_ADDTEXT,
                 cb_stack_[cbs_top_].len,
                 (LPARAM)cb_stack_[cbs_top_].txt.get());

        /* note the new paste range */
        paste_range_start_ = pos;
        paste_range_end_ = call_sci(SCI_GETCURRENTPOS);

        /* done with the undo group */
        call_sci(SCI_ENDUNDOACTION);
    }
}

int ScintillaWin::can_paste_pop()
{
    /*
     *   We can do a paste-pop if all of the following are true:
     *
     *   - the last paste range is still valid (it will have been cleared if
     *   we've moved the cursor or modified the buffer)
     *
     *   - we'are at the end of the last paste range
     *
     *   - there are at least two items in the clipboard stack
     */
    return (paste_range_start_ != -1
            && paste_range_end_ != -1
            && call_sci(SCI_GETCURRENTPOS) == paste_range_end_
            && cbs_cnt_ >= 2);
}

void ScintillaWin::paste_pop()
{
    /* if paste-pop isn't allowed right now, ignore it */
    if (!can_paste_pop())
        return;

    /* perform the paste-pop as a grouped operation for undo */
    call_sci(SCI_BEGINUNDOACTION);

    /* remove the last pasted text if it's non-empty */
    call_sci(SCI_SETSEL, paste_range_start_, paste_range_end_);
    clear_selection();

    /* pull the top element off the clipboard stack */
    const textchar_t *p = cb_stack_[cbs_top_].txt.get();
    size_t len = cb_stack_[cbs_top_].len;
    int from_sys = cb_stack_[cbs_top_].from_sys;

    /* pop the stack */
    if (cbs_top_-- == 0)
        cbs_top_ = CB_STACK_SIZE - 1;

    /* go to the bottom element */
    int bot = cbs_top_ - cbs_cnt_ + 1;
    if (bot < 0)
        bot += CB_STACK_SIZE;

    /* re-insert the top element at the bottom of the stack */
    cb_stack_[bot].set(p, len, from_sys);

    /* note where we're inserting the new text */
    int pos = call_sci(SCI_GETCURRENTPOS);

    /* insert it */
    call_sci(SCI_ADDTEXT,
             cb_stack_[cbs_top_].len,
             (LPARAM)cb_stack_[cbs_top_].txt.get());

    /* copy this text to the system clipboard as well, for consistency */
    call_sci(SCI_COPYTEXT, cb_stack_[cbs_top_].len,
             (LPARAM)cb_stack_[cbs_top_].txt.get());

    /* done with the grouped undo */
    call_sci(SCI_ENDUNDOACTION);

    /* note the new paste range */
    paste_range_start_ = pos;
    paste_range_end_ = call_sci(SCI_GETCURRENTPOS);
}

void ScintillaWin::clear_selection()
{
    if (call_sci(SCI_GETSELECTIONSTART) != call_sci(SCI_GETSELECTIONEND))
        call_sci(SCI_CLEAR);
}

int ScintillaWin::can_paste()
{
    return call_sci(SCI_CANPASTE);
}

int ScintillaWin::can_copy()
{
    return call_sci(SCI_GETSELECTIONSTART) != call_sci(SCI_GETSELECTIONEND);
}

int ScintillaWin::can_cut()
{
    return can_copy() && !call_sci(SCI_GETREADONLY);
}

void ScintillaWin::del_char()
{
    long a, b;

    /* if there's no selection, select the next character */
    get_sel_range(&a, &b);
    if (a == b)
    {
        /* with no selection, this obeys the repeat count - group for undo */
        call_sci(SCI_BEGINUNDOACTION);

        /* repeate the delete-character command */
        for (int i = 0 ; i < repeat_count_ ; ++i)
        {
            /* select the next character */
            call_sci(SCI_SETSEL, a, call_sci(SCI_POSITIONAFTER, a));

            /* clear it */
            call_sci(SCI_CLEAR);
        }

        /* end the undo group */
        call_sci(SCI_ENDUNDOACTION);
    }
    else
    {
        /*
         *   there's a selection already, so simply clear it; in this case,
         *   there's no point in repeating the clear, as there's no selection
         *   after we do the delete
         */
        call_sci(SCI_CLEAR);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   insert a character
 */
void ScintillaWin::insert_char(int c)
{
    /* do this in an undo group */
    call_sci(SCI_BEGINUNDOACTION);

    /* clear any existing selection, since the insertion will replace it */
    call_sci(SCI_REPLACESEL, 0, (LPARAM)"");

    /* add the character per the repeat count */
    char txt[2] = { (char)c, '\0' };
    for (int i = 0 ; i < repeat_count_ ; ++i)
    {
        /* add the character */
        call_sci(SCI_ADDTEXT, 1, (LPARAM)txt);

        /* check for auto-wrapping */
        check_auto_wrap();
    }

    /* done with the undo group */
    call_sci(SCI_ENDUNDOACTION);
}


/* ------------------------------------------------------------------------ */
/*
 *   Undo/redo
 */

int ScintillaWin::can_undo()
{
    return call_sci(SCI_CANUNDO);
}

void ScintillaWin::undo()
{
    call_sci(SCI_UNDO);
    cancel_sel_mode(FALSE);
}

int ScintillaWin::can_redo()
{
    return call_sci(SCI_CANREDO);
}

void ScintillaWin::redo()
{
    call_sci(SCI_REDO);
    cancel_sel_mode(FALSE);
}

void ScintillaWin::undo_group(int begin)
{
    call_sci(begin ? SCI_BEGINUNDOACTION : SCI_ENDUNDOACTION);
}

/* ------------------------------------------------------------------------ */
/*
 *   Scroll the window
 */
void ScintillaWin::scroll_by(int lines, int columns)
{
    call_sci(SCI_LINESCROLL, columns, lines);
}

/*
 *   get the approximate page width in columns
 */
int ScintillaWin::page_width()
{
    RECT rc;

    /* get the window width */
    GetClientRect(hwnd_, &rc);

    /* get the approximate average character width - use "0" as average */
    int avg = call_sci(SCI_TEXTWIDTH, STYLE_DEFAULT, (LPARAM)"0");

    /* return the page width in terms of character widths */
    return (rc.right - rc.left) / (avg != 0 ? avg : 1);
}

/*
 *   attempt to vertically center the caret
 */
void ScintillaWin::vcenter_caret()
{
    RECT rc;
    int lcur, ly;

    /* get the current position as a line number */
    lcur = call_sci(SCI_LINEFROMPOSITION, call_sci(SCI_GETCURRENTPOS));

    /* get the center-of-window position as a line number */
    GetClientRect(hwnd_, &rc);
    ly = call_sci(SCI_LINEFROMPOSITION,
                  call_sci(SCI_POSITIONFROMPOINT, 0, (rc.bottom - rc.top)/2));

    /* scroll to put lcur at the same position as ly */
    call_sci(SCI_LINESCROLL, 0, lcur - ly);
}

void ScintillaWin::window_home()
{
    int pos, l;

    /* go to the start of the first line in the window */
    pos = call_sci(SCI_POSITIONFROMPOINT, 0, 0);
    l = call_sci(SCI_LINEFROMPOSITION, pos);
    pos = call_sci(SCI_POSITIONFROMLINE, l);
    call_sci(SCI_SETSEL, pos, pos);
}

void ScintillaWin::window_end()
{
    RECT rc;
    int pos, l;

    /* get the window area */
    GetClientRect(hwnd_, &rc);

    /* go to the end of the last line in the window */
    l = call_sci(SCI_GETFIRSTVISIBLELINE) + call_sci(SCI_LINESONSCREEN) - 1;
    pos = call_sci(SCI_GETLINEENDPOSITION, l);
    call_sci(SCI_SETSEL, pos, pos);
}

/* ------------------------------------------------------------------------ */
/*
 *   Is the given line blank (i.e., empty or all whitespace)?
 */
int ScintillaWin::is_blank_line(int l)
{
    int blank;
    char *p;

    /* retrieve the text of the line */
    size_t len = call_sci(SCI_LINELENGTH, l);
    char *txt = new char[len + 1];
    call_sci(SCI_GETLINE, l, (LPARAM)txt);

    /* look for non-spaces */
    for (p = txt, blank = TRUE ; len != 0 ; ++p, --len)
    {
        if (!isspace(*p))
        {
            blank = FALSE;
            break;
        }
    }

    /* done with the text */
    delete [] txt;

    /* return our all-blank indication */
    return blank;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check for auto-wrapping
 */
void ScintillaWin::check_auto_wrap()
{
    int curpos = get_cursor_pos();
    int pos;
    int sp;

    /* if we're not in auto-wrap mode, there's nothing to do */
    if (!auto_wrap_)
        return;

    /* if the cursor isn't past the right margin, don't wrap */
    if (get_cursor_column() < right_margin_)
        return;

    /*
     *   check the line for trailing whitespace - if everything after the
     *   margin point is whitespace, we don't need to wrap it after all
     */
    int l = call_sci(SCI_LINEFROMPOSITION, curpos);
    int marginpos = call_sci(SCI_FINDCOLUMN, l, right_margin_);
    int maxpos = call_sci(SCI_GETTEXTLENGTH);
    for (sp = TRUE, pos = marginpos ; pos < maxpos ;
         pos = call_sci(SCI_POSITIONAFTER, pos))
    {
        /* get this character */
        int ch = call_sci(SCI_GETCHARAT, pos);

        /* stop at the newline */
        if (ch == '\n' || ch == '\r')
            break;

        /* if this isn't a space, we need to wrap */
        if (!isspace(ch))
        {
            sp = FALSE;
            break;
        }
    }

    /* if it's all spaces, don't wrap */
    if (sp)
        return;

    /* set a spot at the current position so we can come back here */
    void *spota = create_spot(curpos);
    void *spotb = create_spot(call_sci(SCI_GETANCHOR));

    /* find the nearest space preceding the margin on the line */
    for (pos = marginpos ; pos > 0 ; pos = call_sci(SCI_POSITIONBEFORE, pos))
    {
        /* get this character */
        int ch = (int)call_sci(SCI_GETCHARAT, pos);

        /* if we're at a newline, there's nowhere to break the line */
        if (ch == '\n' || ch == '\r')
            break;

        /* if it's a space, we can break the line here */
        if (isspace(ch))
        {
            /* group this for undo */
            call_sci(SCI_BEGINUNDOACTION);

            /* remove the space and insert a newline in its place */
            call_sci(SCI_SETSEL, pos, call_sci(SCI_POSITIONAFTER, pos));
            call_sci(SCI_CLEAR);
            call_sci(SCI_NEWLINE);

            /* done with the undo group */
            call_sci(SCI_ENDUNDOACTION);

            /* our work here is done */
            break;
        }
    }

    /* restore our starting position from the spots */
    call_sci(SCI_SETCURRENTPOS, get_spot_pos(spota));
    call_sci(SCI_SETANCHOR, get_spot_pos(spotb));

    /* we're done with the spots */
    delete_spot(spota);
    delete_spot(spotb);
}

/* ------------------------------------------------------------------------ */
/*
 *   Fill a paragraph or range of paragraphs
 */
void ScintillaWin::fill_paragraph()
{
    int sela = call_sci(SCI_GETSELECTIONSTART);
    int selb = call_sci(SCI_GETSELECTIONEND);
    int l, startl, endl;
    void *end_spot;
    void *next_spot = create_spot(0);

    /* do the formatting in an undo group */
    call_sci(SCI_BEGINUNDOACTION);

    /*
     *   If we have a non-empty selection, fill the selected lines;
     *   otherwise, find the beginning and end of the paragraph by looking
     *   for the nearest blank lines.
     */
    if (sela != selb)
    {
        /*
         *   We have a selection range - use it as the limits of the fill.
         *   The selection range marks the lines we actually want to include
         *   in the fill, but 'endl' is the line after the last line to
         *   include, so it's one past the last selected line.
         */
        startl = call_sci(SCI_LINEFROMPOSITION, sela);
        endl = call_sci(SCI_LINEFROMPOSITION, selb) + 1;
    }
    else
    {
        /* seek back to the nearest blank line */
        for (startl = call_sci(SCI_LINEFROMPOSITION, sela) ;
             startl > 0 ; --startl)
        {
            /* if it's a blank line, start on the next line */
            if (is_blank_line(startl))
            {
                ++startl;
                break;
            }
        }

        /* seek forward to the next blank line */
        int lc = call_sci(SCI_GETLINECOUNT);
        for (endl = startl + 1 ; endl < lc ; ++endl)
        {
            /* if it's a blank line, end on the previous line */
            if (is_blank_line(endl))
                break;
        }
    }

    /*
     *   Word-wrap the comment text within the bounds of the current line's
     *   indentation point and the fill column.  If there's a repeat count
     *   (other than the default setting of 1), use it as the fill column,
     *   and set it as the new default for subsequent fill operations;
     *   otherwise use the current default fill column.  If the repeat count
     *   is zero, fill to infinite width.
     */
    int right = (repeat_count_ == 1 ? right_margin_ : repeat_count_);
    int infinite = (repeat_count_ == 0);

    /*
     *   set up a prototype line of this character width,for Scintilla to
     *   measure
     */
    char *proto = new char[right+1];
    int i;
    for (i = 0 ; i < right ; ++i)
        proto[i] = 'X';
    proto[i] = '\0';

    /* measure the pixel width for the wrapping, in the default style */
    int pixwid = call_sci(SCI_TEXTWIDTH, STYLE_DEFAULT, (int)proto);

    /* done with the prototype line */
    delete [] proto;

    /* set up a spot marking the end of the fill range */
    end_spot = create_spot(call_sci(SCI_POSITIONFROMLINE, endl));

    /* fill each paragraph in the range */
    for (l = startl ; ; )
    {
        /* note if this is a blank line */
        int blank = is_blank_line(l);

        /*
         *   if this is a blank line, or we've reached the end of the fill
         *   region or of the overall buffer, fill between the start line and
         *   here
         */
        if (blank || l >= endl || l >= call_sci(SCI_GETLINECOUNT))
        {
            /* mark the position where we'll resume formatting */
            set_spot_pos(next_spot, call_sci(SCI_POSITIONFROMLINE, l));

            /*
             *   mark the range - from the start of the first line to the end
             *   of the last line in the fill region
             */
            call_sci(SCI_SETTARGETSTART,
                     call_sci(SCI_POSITIONFROMLINE, startl));
            call_sci(SCI_SETTARGETEND,
                     call_sci(SCI_POSITIONBEFORE,
                              call_sci(SCI_POSITIONFROMLINE, l)));

            /* join the lines */
            call_sci(SCI_LINESJOIN);

            /* if the width isn't infinite, wrap to the fill width */
            if (!infinite)
                call_sci(SCI_LINESSPLIT, pixwid);

            /*
             *   set up at our resumption position - this is the next line to
             *   look at, and also the start of the next paragraph
             */
            l = call_sci(SCI_LINEFROMPOSITION, get_spot_pos(next_spot));

            /* since we reformatted the buffer, refigure the ending line */
            endl = call_sci(SCI_LINEFROMPOSITION, get_spot_pos(end_spot));

            /* skip blank lines */
            while (l < call_sci(SCI_GETLINECOUNT) && is_blank_line(l))
                ++l;

            /*
             *   if we're at the end of the fill range or of the buffer,
             *   we're done
             */
            if (l >= endl || l >= call_sci(SCI_GETLINECOUNT))
                break;

            /* this is the start of the next paragraph */
            startl = l;
        }
        else
        {
            /*
             *   we're going to include this line in the paragraph, so simply
             *   move on to the next
             */
            ++l;
        }
    }

    /* done with the undo group */
    call_sci(SCI_ENDUNDOACTION);

    /* delete our spots */
    delete_spot(end_spot);
    delete_spot(next_spot);
}
