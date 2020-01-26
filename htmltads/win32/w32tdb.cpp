#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32tdb.cpp,v 1.4 1999/07/11 00:46:51 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32tdb.cpp - win32 HTML TADS debugger configuration
Function

Notes

Modified
  01/31/98 MJRoberts  - Creation
*/

#include <ctype.h>
#include <stdio.h>
#include <assert.h>

#include <Windows.h>
#include <commctrl.h>
#include <oleauto.h>
#include <shlwapi.h>
#include <mshtmhst.h>
#include <WinInet.h>

/* include TADS OS interface */
#include "os.h"

#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLW32_H
#include "htmlw32.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef HTMLPREF_H
#include "htmlpref.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSKB_H
#include "tadskb.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLFMT_H
#include "htmlfmt.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef W32EXPR_H
#include "w32expr.h"
#endif
#ifndef W32PRJ_H
#include "w32prj.h"
#endif
#ifndef W32SCRIPT_H
#include "w32script.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef W32BPDLG_H
#include "w32bpdlg.h"
#endif
#ifndef W32FNDLG_H
#include "w32fndlg.h"
#endif
#ifndef HTMLTXAR_H
#include "htmltxar.h"
#endif
#ifndef W32DBGPR_H
#include "w32dbgpr.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef TADSDOCK_H
#include "tadsdock.h"
#endif
#ifndef TADSDDE_H
#include "tadsdde.h"
#endif
#ifndef TADSIFID_H
#include "tadsifid.h"
#endif
#ifndef W32SCI_H
#include "w32sci.h"
#endif
#ifndef TBARMBAR_H
#include "tbarmbar.h"
#endif
#ifndef ICONMENU_H
#include "iconmenu.h"
#endif
#ifndef MDI_H
#include "mditab.h"
#endif
#ifndef ITADSWORKBENCH_H
#include "itadsworkbench.h"
#endif
#ifndef TADSCOM_H
#include "tadscom.h"
#endif
#ifndef TADSHTMLDLG_H
#include "tadshtmldlg.h"
#endif
#ifndef W32HELP_H
#include "w32help.h"
#endif
#ifndef TINYXML_H
#include "tinyxml.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif

/* include the TADS 3 regular expression parser */
#include "utf8.h"
#include "charmap.h"
#include "vmregex.h"

/* include the Doc Search engine interface */
#include "docsearch.h"

/* include the Babel interface */
#include "babelifc.h"

/* include the XZip interface */
#include "xzip.h"


/* ------------------------------------------------------------------------ */
/*
 *   initialization timer ID
 */
static int S_init_timer_id;


/* ------------------------------------------------------------------------ */
/*
 *   case-insensitive strstr
 */
static char *stristr(const char *str, const char *sub)
{
    size_t lstr = strlen(str);
    size_t lsub = strlen(sub);

    for ( ; lstr >= lsub ; --lstr, ++str)
    {
        if (memicmp(str, sub, lsub) == 0)
            return (char *)str;
    }

    return 0;
}

/*
 *   Create a UTF-8 string from a wchar_t string
 */
static char *utf8_from_olestr(const OLECHAR *str, UINT len)
{
    CCharmapToUniUcs2Little cm;
    char *buf;
    char *p;
    size_t buflen, ulen;

    /*
     *   allocate a buffer - UTF-8 can use up to three bytes per character;
     *   for simplicity and faster execution, don't bother measuring the
     *   actual space needs, but rather simply allocate the maximum, plus
     *   space for a null terminator
     */
    buflen = len*3 + 1;
    p = buf = (char *)th_malloc(buflen);
    if (p == 0)
        return 0;

    /*
     *   map the string; note that the mapper needs the *byte* length of the
     *   string, which is len times the size of a wide character
     */
    ulen = cm.map(&p, &buflen, (const char *)str, len * sizeof(str[0]));

    /* null-terminate it */
    if (ulen >= buflen)
        --ulen;
    buf[ulen] = '\0';

    /* return the new buffer */
    return buf;
}

/* get the character before/after the given offset in a utf-8 string */
inline static wchar_t utf8_prevchar(char *p)
{
    return utf8_ptr::s_getch(utf8_ptr::s_dec(p));
}
inline static wchar_t utf8_nextchar(char *p)
{
    return utf8_ptr::s_getch(utf8_ptr::s_inc(p));
}



/*
 *   Create UTF-8 from an ANSI string
 */
static char *utf8_from_ansi(const textchar_t *str, UINT len)
{
    char *utf8;

    /* first, convert the ANSI string to wide characters */
    BSTR bstr = bstr_from_ansi(str, len);

    /* convert the unicode to UTF-8 format */
    utf8 = utf8_from_olestr(bstr, SysStringLen(bstr));

    /* done with the BSTR */
    SysFreeString(bstr);

    /* return the utf8 */
    return utf8;
}

/*
 *   Create an ANSI string from UTF-8
 */
static char *ansi_from_utf8(const textchar_t *str, UINT len)
{
    CCharmapToLocalUcs2Little cm;
    char *buf, *abuf;
    size_t buflen, ulen, used;

    /*
     *   First, convert to UCS2.  We need two bytes per character in UCS2,
     *   whereas UTF8 can use as little as one byte per character.
     */
    buflen = len*2;
    buf = (char *)th_malloc(buflen);
    if (buf == 0)
        return 0;

    /* map to UCS2 */
    ulen = cm.CCharmapToLocal::map_utf8(buf, buflen, str, len, &used);

    /* convert to ANSI */
    abuf = ansi_from_olestr((const OLECHAR *)buf, ulen/2);

    /* done with the UCS2 buffer */
    th_free(buf);

    /* return the ANSI buffer */
    return abuf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Scintilla implementation of the source manager interface.  Scintilla
 *   maintains the source code, so we simply need to provide an
 *   implementation of the interface on the underlying Scintilla control.
 */
class CSciSrcMgr: public IDebugSrcMgr
{
public:
    CSciSrcMgr(CHtmlSys_dbgsrcwin *win)
    {
        /* remember the source window */
        win_ = win;
        win_->AddRef();
    }

    ~CSciSrcMgr()
    {
        /* release our scintilla window object */
        win_->Release();
    }

    /* clear the document */
    virtual void clear_document()
    {
        win_->get_scintilla()->clear_document();
    }

    /*
     *   clear the document for reload - we don't have to do anything here,
     *   because the reload starter will take care of this
     */
    virtual void clear_document_for_reload() { }

    /* prepare for reformatting */
    virtual void prepare_for_reformat(CHtmlDebugSysIfc_win *)
    {
        /* we don't need to do anything to reformat */
    }

    /* get the line number of the start of the current selection */
    virtual long get_selection_linenum()
    {
        /* get the Scintilla line number, adjusted to our 1-based system */
        return win_->get_scintilla()->get_cursor_linenum() + 1;
    }

    /* update the display status of a line on screen */
    virtual void update_line_on_screen(IDebugWin *win,
                                       long linenum, unsigned int stat)
    {
        /* set the status */
        win_->get_scintilla()->set_line_status(linenum - 1, stat);
    }

    /* append a line of text to the source */
    virtual void append_line(const textchar_t *txt, size_t len)
    {
        /* this should never be used with a Scintilla window */
        assert(FALSE);
    }

    /* update the tab size; return true if the source must be reloaded */
    virtual int set_tab_size(int n)
    {
        /*
         *   we do not need to reload the file - Scintilla maintains tabs
         *   internally, so it can handle tab size changes on the fly
         */
        return FALSE;
    }

    /* begin a file load */
    void begin_file_load(void *&ctx)
    {
        /* tell the Scintilla interface we're loading */
        win_->get_scintilla()->begin_file_load(ctx);
    }

    /* load text */
    void load_text(const textchar_t *txt, size_t len)
    {
        /* load the text into Scintilla */
        win_->get_scintilla()->append_text(txt, len);
    }

    /* end file load */
    void end_file_load(void *ctx)
    {
        /* tell scintilla */
        win_->get_scintilla()->end_file_load(ctx);

        /* restore breakpoints and other status markers in the window */
        win_->restore_indicators();

        /* restore bookmarks */
        win_->restore_bookmarks();
    }

protected:
    /* our source window */
    CHtmlSys_dbgsrcwin *win_;
};


/* ------------------------------------------------------------------------ */
/*
 *   twb_extension implementation
 */

twb_extension::twb_extension(HMODULE hmod, IUnknown *unk)
{
    /* remember our module hadnle */
    this->hmod = hmod;

    /* remember the basic extension interface */
    this->unk = unk;

    /* we're not in a list yet */
    nxt = 0;

    /* ask about ITadsWorkbenchExtension */
    if (!SUCCEEDED(unk->QueryInterface(
        IID_ITadsWorkbenchExtension, (void **)&iext)))
        iext = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   "Plain Text" mode.  This is a built-in mode that we implement for source
 *   file windows that don't have any other mode.
 */
class CPlainTextMode: public ITadsWorkbenchEditorMode
{
public:
    CPlainTextMode()
    {
        refcnt_ = 1;
    }

    virtual ~CPlainTextMode() { }

    /* IUnknown reference counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret;
        if ((ret = --refcnt_) == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchEditorMode)
            *ifc = (void *)(ITadsWorkbenchEditorMode *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"Plain Text");
    }

    /* get file type information */
    BOOL STDMETHODCALLTYPE GetFileTypeInfo(UINT index, wb_ftype_info *info)
    {
        switch (index)
        {
        case 0:
            /* Plain Text mode */
            info->icon = LoadIcon(CTadsApp::get_app()->get_instance(),
                                  MAKEINTRESOURCE(IDI_TXTFILE_ICON));
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"Text File");
            wcscpy(info->desc, L"A file containing any text");
            wcscpy(info->defext, L"txt");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"Text Files (*.txt)\0*.txt\0";

        *priority = 10000;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* get my default style settings */
    void STDMETHODCALLTYPE GetStyleInfo(UINT idx, wb_style_info *info)
    {
        /* we only have the "Text" style */
        if (idx == 0)
        {
            /* our "text" style uses defaults for everything */
            info->sci_index = 0;
            info->font_name[0] = '\0';
            info->ptsize = 0;
            info->fgcolor = WBSTYLE_DEFAULT_COLOR;
            info->bgcolor = WBSTYLE_DEFAULT_COLOR;
            info->styles = 0;
            info->keyword_set = 0;
            wcscpy(info->title, L"Text");
            wcscpy(info->sample, L"Sample Text");
        }
    }

    /* we just have the one style */
    UINT STDMETHODCALLTYPE GetStyleCount() { return 1; }

    /* check to see if we want to provide editor customizations for a file */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /*
         *   we can handle any file, but since we're so undiscriminating, use
         *   the lowest possible affinity
         */
        return 1;
    }

    /* create our editor extension */
    ITadsWorkbenchEditor * STDMETHODCALLTYPE CreateEditorExtension(HWND hwnd)
    {
        /* we don't have an associated mode */
        return 0;
    }

private:
    /* COM reference count */
    ULONG refcnt_;
};

/*
 *   A pseudo-extension for the built-in modes
 */
class CBuiltInModes: public ITadsWorkbenchExtension
{
public:
    CBuiltInModes()
    {
        /* count an initial reference on behalf of the caller */
        refcnt_ = 1;
    }

    virtual ~CBuiltInModes() { }

    /* IUnknown reference-counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret;
        if ((ret = --refcnt_) == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchExtension)
            *ifc = (void *)(ITadsWorkbenchExtension *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

    /* get my nth editor mode */
    ITadsWorkbenchEditorMode * STDMETHODCALLTYPE GetMode(UINT index)
    {
        switch (index)
        {
        case 0:
            return new CPlainTextMode();

        default:
            /* no other modes */
            return 0;
        }
    }

protected:
    /* my reference count */
    ULONG refcnt_;
};




/* ------------------------------------------------------------------------ */
/*
 *   The global command descriptor list.  We build this from the generated
 *   command info file - see the makefile (tads/html/win32/makefile.vc5) for
 *   information on how it's generated.
 */
HtmlDbgCommandInfo CHtmlSys_dbgmain::command_list_[] =
{
    /* set up the initializer macro, and include the command list */
#define W32COMMAND(id, name, desc, sysid, sci) \
    HtmlDbgCommandInfo(id, name, name "\r\n" desc, sysid),
#include "w32tdbcmd.h"
#undef W32COMMAND

    /* add the end-of-array marker */
    HtmlDbgCommandInfo(0, 0, 0, 0)
};

/* command name-keyed and ID-keyed hash tables */
CHtmlHashTable *CHtmlSys_dbgmain::cmdname_hash = 0;
CHtmlHashTable *CHtmlSys_dbgmain::command_hash = 0;

/* command-name-keyed hash entry */
class CmdNameEntry: public CHtmlHashEntryCI
{
public:
    CmdNameEntry(HtmlDbgCommandInfo *inf)
        : CHtmlHashEntryCI(inf->sym, get_strlen(inf->sym), FALSE)
    {
        this->inf = inf;
    }

    /* the hash info entry */
    HtmlDbgCommandInfo *inf;
};

/* ------------------------------------------------------------------------ */
/*
 *   The key tables
 */

HtmlKeyTable CHtmlSys_dbgmain::keytabs_[] =
{
    /* note - "Global" must always be in first place */
    { "Global", 0, 0 },

    { "Text Editor", "Global", 0 },
    { "Incremental Search", "Text Editor", 0 },

    /* list terminator */
    { 0, 0, 0 }
};


/* ------------------------------------------------------------------------ */
/*
 *   Rebar constants
 */

/* toolbar icon dimensions */
#define TOOLBAR_ICON_WIDTH   16
#define TOOLBAR_ICON_HEIGHT  15

/* main debugger toolbar ID */
#define DEBUG_TOOLBAR_ID   1

/* search toolbar ID */
#define SEARCH_TOOLBAR_ID  2

/* menu toolbar ID */
#define MENU_TOOLBAR_ID  3

/* edit toolbar ID */
#define EDIT_TOOLBAR_ID  4

/* documentation toolbar ID */
#define DOC_TOOLBAR_ID  5

/* toolbar config IDs */
static const char *tb_config_id[] =
{
    0,                 /* index zero is not used (we use it as a 'null' ID) */
    "DebugToolbar",                                     /* DEBUG_TOOLBAR_ID */
    "SearchToolbar",                                   /* SEARCH_TOOLBAR_ID */
    "MenuBar",                                           /* MENU_TOOLBAR_ID */
    "EditToolbar",                                       /* EDIT_TOOLBAR_ID */
    "DocToolbar"                                          /* DOC_TOOLBAR_ID */
};

/* search modes - for the search toolbar dropdown button */
struct search_mode_info
{
    /* the command ID (ID_xxx) for this search mode */
    int cmd;

    /* the toolbar bitmap index for the search button in this mode */
    int image;
};

/* index of the "search" button in the search toolbar */
const int SEARCHBAR_SEARCH_BTN_IDX = 1;

/*
 *   array of search modes, indexed by the index we save under the
 *   "searchMode" key in the configuration file
 */
static search_mode_info search_mode_table[] =
{
    { 0, 0 },          /* index zero is not used (we use it as a 'null' ID) */
    { ID_SEARCH_DOC, 20 },                     /* documentation search mode */
    { ID_SEARCH_PROJECT, 21 },                  /* profile-file-search mode */
    { ID_SEARCH_FILE, 22 }                              /* file search mode */
};

/* find a search mode by command */
static int search_mode_from_cmd(int cmd)
{
    /* if there's no command, return the 'null' index */
    if (cmd == 0)
        return 0;

    /* search the table */
    for (int i = 1 ;
         i < sizeof(search_mode_table)/sizeof(search_mode_table[0]) ;
         ++i)
    {
        /* if this is our entry, return the index */
        if (search_mode_table[i].cmd == cmd)
            return i;
    }

    /* not found */
    return 0;
}

/*
 *   Doc Search temporary page cleanup object.  Each time we create a
 *   temporary frameset page to enframe a document that came up as a search
 *   result, we add one of these cleanup objects to our list.  We set up each
 *   cleanup object so that we try to delete the temp file after about five
 *   minutes, which should quite long enough for the browser to have
 *   launched.
 *
 *   We assume that a temp page will only be needed long enough to launch the
 *   browser; once the page is loaded, the browser typically will close the
 *   file and won't need it again.  The only time the browser might need the
 *   file again is in case of a manual refresh by the user; but since the
 *   files we're viewing are local and static, there shouldn't be much reason
 *   for a user to do that.
 */
struct temp_page_cleaner
{
    /* next link in list */
    temp_page_cleaner *nxt;

    /* system-tick-count time of creation of the page */
    DWORD t0;

    /* name of the temporary page file */
    char fname[1];
};

/* head of list of cleanup objects */
static temp_page_cleaner *S_temp_pages;

/* page cleanup timer */
static int S_cleanup_timer_id;

/*
 *   Page cleanup interval, in milliseconds.  We want to give the browser
 *   plenty of time to load the page; we assume that once the browser has
 *   loaded the page, it won't need it again.  A few seconds ought to be long
 *   enough for the browser to load the page, but this task is not at all
 *   urgent, and we don't want to burn up a lot of time scanning our list.
 *   So do this quite infrequently.
 */
#define TEMP_PAGE_CLEANUP_INTERVAL  (5*60*1000)

/*
 *   Clean up temp pages.  If 'force' is true, it means that we're shutting
 *   down, so we should delete files whether or not they've expired, and we
 *   should delete the memory trackers in any case because we won't be trying
 *   to clean these up again.
 */
static void clean_up_temp_pages(int force)
{
    temp_page_cleaner *cur, *prv, *nxt;
    DWORD tcur = GetTickCount();

    /* scan the page cleanup list, deleting any expired files */
    for (cur = S_temp_pages, prv = 0 ; cur != 0 ; cur = nxt)
    {
        /* remember the next item in the list */
        nxt = cur->nxt;

        /* if this page has expired, or we're in 'force' mode, delete it */
        if (tcur - cur->t0 >= TEMP_PAGE_CLEANUP_INTERVAL || force)
        {
            int del_ok;

            /* delete the underlying file */
            del_ok = (remove(cur->fname) == 0);

            /*
             *   if we successfully deleted the file, or we're in 'force'
             *   mode, delete the memory object
             */
            if (del_ok || force)
            {
                /* successfully removed the file - delete the link */
                th_free(cur);
                cur = 0;

                /* unlink it from the list */
                if (prv != 0)
                    prv->nxt = nxt;
                else
                    S_temp_pages = nxt;
            }
        }

        /*
         *   if we didn't delete the current link, it's the previous link for
         *   the next iteration
         */
        if (cur != 0)
            prv = cur;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Bookmarks
 */

/*
 *   A bookmark file identifier.
 */
struct tdb_bookmark_file
{
    tdb_bookmark_file(CHtmlSys_dbgmain *dbgmain, const textchar_t *fname)
    {
        this->dbgmain = dbgmain;
        this->fname.set(fname);
        nxt = 0;
        cnt = 0;
    }

    /* add a reference from a bookmark */
    void add_ref() { ++cnt; }

    /* release a reference from a bookmark */
    void release()
    {
        /* decrement my reference count; if that makes it zero, delete me */
        if (--cnt == 0)
        {
            tdb_bookmark_file *cur, *prv;

            /* unlink myself from the master list of files */
            for (prv = 0, cur = dbgmain->bookmark_files_ ; cur != 0 ;
                 prv = cur, cur = cur->nxt)
            {
                /* if this is me, delete myself */
                if (cur == this)
                {
                    /* unlink from the list */
                    if (prv != 0)
                        prv->nxt = nxt;
                    else
                        dbgmain->bookmark_files_ = nxt;

                    /* delete me */
                    delete this;

                    /* we're done */
                    return;
                }
            }
        }
    }

    /* find or add a bookmark file for the given path */
    static tdb_bookmark_file *find(
        CHtmlSys_dbgmain *dbgmain, const textchar_t *fname, int add)
    {
        tdb_bookmark_file *f;

        /* scan the list */
        for (f = dbgmain->bookmark_files_ ; f != 0 ; f = f->nxt)
        {
            /* if the name matches, return it */
            if (stricmp(f->fname.get(), fname) == 0)
                return f;
        }

        /* didn't find it - create a new one if desired */
        if (add)
        {
            /* create the file tracker */
            f = new tdb_bookmark_file(dbgmain, fname);

            /* link it into the master list */
            f->nxt = dbgmain->bookmark_files_;
            dbgmain->bookmark_files_ = f;
        }

        /* return the result */
        return f;
    }

    /* next file in the master list of files */
    tdb_bookmark_file *nxt;

    /* the path to the file */
    CStringBuf fname;

    /*
     *   The number of bookmarks associated with this file.  When this drops
     *   to zero, we delete the file object.
     */
    int cnt;

    /* our debugger main object */
    CHtmlSys_dbgmain *dbgmain;
};

/*
 *   The main debugger window maintains a global list of bookmarks, but the
 *   list is mostly managed by the source windows.
 */
struct tdb_bookmark
{
    tdb_bookmark(tdb_bookmark_file *file, int handle, int line, int name)
    {
        /* remember my location */
        this->file = file;
        this->handle = handle;
        this->line = line;
        this->name = name;

        /* we're not in a list yet */
        this->nxt = 0;

        /* add a reference to our file */
        file->add_ref();
    }

    ~tdb_bookmark()
    {
        /* release our file reference */
        file->release();
    }

    /*
     *   The next bookmark in the list.  The bookmark list is in stack order,
     *   with the most recently defined bookmark at the head.
     */
    struct tdb_bookmark *nxt;

    /* the file containing the bookmark */
    struct tdb_bookmark_file *file;

    /*
     *   The bookmark handle.  When the file is open in a Scintilla window,
     *   this is the Scintilla marker handle for the bookmark.  When the
     *   window is closed, this is -1.
     */
    int handle;

    /*
     *   The bookmark line number.  This is meaningful only if handle is -1.
     *   When there's a valid handle, we have to use that instead, since the
     *   bookmark's location can change as the file is edited.
     */
    int line;

    /*
     *   The bookmark's name.  If the bookmark was added anonymously, this is
     *   simply zero.  Otherwise, it's the ASCII code for letter used to name
     *   the bookmark.
     */
    int name;
};



/* ------------------------------------------------------------------------ */
/*
 *   Loading dialog
 */
class CTadsDialogLoading: public CTadsDialog
{
public:
    CTadsDialogLoading() : CTadsDialog()
    {
        bkg_ = (HBITMAP)LoadImage(
            CTadsApp::get_app()->get_instance(),
            MAKEINTRESOURCE(IDB_BMP_START_BKG), IMAGE_BITMAP, 0, 0,
            LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_DEFAULTCOLOR
            | LR_SHARED);
    }

    /* update the status message */
    void status(const char *msg, ...)
    {
        char buf[1024];
        va_list args;

        va_start(args, msg);
        _vsnprintf(buf, sizeof(buf), msg, args);
        va_end(args);

        /* clear the old message */
        SetDlgItemText(handle_, IDC_TXT_LOADSTAT, "");

        /*
         *   refresh the background explicitly - Windows won't do this with a
         *   control that's drawn transparently
         */
        RECT rc;
        GetClientRect(GetDlgItem(handle_, IDC_TXT_LOADSTAT), &rc);
        MapWindowPoints(GetDlgItem(handle_, IDC_TXT_LOADSTAT), handle_,
                        (POINT *)&rc, 2);
        InvalidateRect(handle_, &rc, TRUE);
        UpdateWindow(handle_);

        /* show the new text */
        SetDlgItemText(handle_, IDC_TXT_LOADSTAT, buf);
    }

    BOOL do_dialog_msg(HWND hwnd, UINT message, WPARAM wpar, LPARAM lpar)
    {
        switch (message)
        {
        case WM_ERASEBKGND:
            /* erase - draw our background bitmap */
            {
                /* draw our bitmap into the dialog DC */
                POINT pt;
                pt.x = pt.y = 0;
                draw_bkg((HDC)wpar, pt);
            }
            return TRUE;

        case WM_CTLCOLORSTATIC:
            /* draw with black text and no background */
            SetTextColor((HDC)wpar, RGB(0x00,0x00,0x00));
            SetBkMode((HDC)wpar, TRANSPARENT);
            return (BOOL)GetStockObject(HOLLOW_BRUSH);

        default:
            break;
        }

        /* inherit the default handling */
        return CTadsDialog::do_dialog_msg(hwnd, message, wpar, lpar);
    }

    /* draw the background image */
    void draw_bkg(HDC hdc, POINT ofs)
    {
        /* set up a memory DC for the bitmap */
        HDC memdc = CreateCompatibleDC(hdc);
        HGDIOBJ oldbmp = SelectObject(memdc, bkg_);

        /* get the size of the background bitmap */
        BITMAPINFO info;
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        GetDIBits(memdc, bkg_, 0, 0, 0, &info, DIB_RGB_COLORS);

        /* draw the image */
        RECT rc;
        GetClientRect(handle_, &rc);
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, 0);
        BitBlt(hdc, ofs.x, ofs.y, rc.right, rc.bottom,
               memdc, 0, 0,/*info.bmiHeader.biWidth, info.bmiHeader.biHeight,*/
               SRCCOPY);

        /* clean up */
        SelectObject(memdc, oldbmp);
        DeleteDC(memdc);
    }

    /* close the dialog */
    void close_window()
    {
        DestroyWindow(handle_);
    }

    /* background bitmap */
    HBITMAP bkg_;
};


/* ------------------------------------------------------------------------ */
/*
 *   MDI/SDI conditional code
 */

#ifdef W32TDB_MDI

/*
 *   MDI version
 */

/*
 *   create the main docking port in the MDI frame
 */
CTadsDockportMDI *w32tdb_create_dockport(CHtmlSys_dbgmain *mainwin,
                                         HWND mdi_client)
{
    /* create the port */
    CTadsDockportMDI *port = new CTadsDockportMDI(mainwin, mdi_client);

    /* register the port and make it the default port */
    CTadsWinDock::add_port(port);
    CTadsWinDock::set_default_port(port);
    CTadsWinDock::set_default_mdi_parent(mainwin);

    /* return the new port */
    return port;
}

/* use the MDI frame interface object for the system window */
CTadsSyswin *w32tdb_new_frame_syswin(CHtmlSys_dbgmain *win,
                                     int winmnuidx)
{
    return new CTadsSyswinMdiFrame(win, new CHtmlWinDbgMdiClient(win),
                                   winmnuidx, ID_WINDOW_FIRST, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   MDI client window for debugger
 */

/*
 *   create
 */
CHtmlWinDbgMdiClient::CHtmlWinDbgMdiClient(CHtmlSys_dbgmain *dbgmain)
{
    /* load my context menu */
    ctx_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_MDICLIENT_POPUP));

    /* remember the debugger main window */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();
}

/*
 *   destroy
 */
CHtmlWinDbgMdiClient::~CHtmlWinDbgMdiClient()
{
    /* delete my context menu */
    DestroyMenu(ctx_menu_);

    /* release our reference on the debugger main window */
    dbgmain_->Release();
}

/*
 *   command status check
 */
TadsCmdStat_t CHtmlWinDbgMdiClient::check_command(const check_cmd_info *info)
{
    /* send the command to the main debugger window for processing */
    return dbgmain_->check_command(info);
}

/*
 *   command processing
 */
int CHtmlWinDbgMdiClient::do_command(int notify_code,
                                     int command_id, HWND ctl)
{
    /* send the command to the main debugger window for processing */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   reflect MDI child event notifications to the frame window
 */
void CHtmlWinDbgMdiClient::mdi_child_event(CTadsWin *win, UINT msg,
                                           WPARAM wpar, LPARAM lpar)
{
    /* send it to the main debugger window */
    dbgmain_->mdi_child_event(win, msg, wpar, lpar);
}



#else /* ------------------------ W32TDB_MDI ----------------------------- */

/*
 *   non-MDI version
 */

/*
 *   Create the main docking port.  The SDI version does not support
 *   docking, since there's no main window for a docking port.
 */
CTadsDockportMDI *w32tdb_create_dockport(CHtmlSys_dbgmain *mainwin,
                                         HANDLE mdi_client)
{
    /* no docking port in SDI mode */
    return 0;
}

/* use the standard system interface object for the frame */
CTadsSyswin *w32tdb_new_frame_syswin(CHtmlSys_dbgmain *win,
                                     int /*winmnuidx*/)
{
    return new CTadsSyswin(win);
}

#endif /* W32TDB_MDI */

/* ------------------------------------------------------------------------ */
/*
 *   Main debugger window.  We create only one of these for the entire
 *   application.
 */
static CHtmlSys_dbgmain *S_main_win = 0;

/* delete the static source directory list */
static void delete_source_dir_list();

/* original window procedure for the searchbar combo box */
WNDPROC CHtmlSys_dbgmain::searchbox_orig_winproc_;

/* cursors */
static HCURSOR line_drag_csr_;

/* startup mode, and the startup command from the Welcome dialog, if shown */
static int S_startup_mode_ = 1;
static int S_startup_cmd_;

/* name of the game selected from the welcome dialog */
static textchar_t S_startup_proj_[OSFNMAX];


/* ------------------------------------------------------------------------ */
/*
 *   Pre-show-window start-up routine.
 */
int w32_pre_show_window(int argc, char **argv)
{
    CHtmlDebugConfig *gc = 0;
    int ret;
    CHtmlPreferences *prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();
    int id;
    const textchar_t *order = prefs->get_recent_dbg_game_order();
    CStringBuf gcfile;

    /* if there are arguments, proceed without showing a dialog */
    if (argc > 1)
        return TRUE;

    /* load the global configuration */
    gc = CHtmlSys_dbgmain::load_global_config(&gcfile);

    /* get the startup mode from the configuration */
    S_startup_mode_ = gc->getval_int("startup", "action", 1);

    /* check the startup mode */
    switch (S_startup_mode_)
    {
    case 1:
    ask_again:
        /* mode 1: show the "welcome" dialog */
        id = CHtmlDbgWiz::run_start_dlg(
            0, gc, prefs->get_recent_dbg_game(order[0]));

        /* the dialog could have updated the global config - save it */
        CHtmlSys_dbgmain::save_global_config(gc, &gcfile);

        /* process the selected dialog action */
        switch (id)
        {
        case IDOK:
        case IDCANCEL:
            /* closed the dialog without a selection - simply terminate */
            ret = FALSE;
            break;

        case IDC_BTN_OPEN:
            /* ask for a file to open */
            if (!CHtmlSys_dbgmain::ask_load_game(
                0, S_startup_proj_, sizeof(S_startup_proj_)))
            {
                /* canceled - back to the welcome dialog */
                goto ask_again;
            }

            /* they want to load the game we selected */
            S_startup_cmd_ = id;
            ret = TRUE;
            break;

        default:
            /* they pressed a button - save it as the startup command */
            S_startup_cmd_ = id;
            ret = TRUE;
            break;
        }
        break;

    default:
        /* no "welcome" dialog - just proceed to startup with no project */
        ret = TRUE;
        break;
    }

    /* delete the global config */
    delete gc;

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pre-start processing.  If there are no arguments, simply start up in
 *   idle mode, with no workspace.
 */
int w32_pre_start(int iter, int argc, char **argv)
{
    int restart;
    int reload;
    int terminate;
    int abort;
    CHtmlSys_mainwin *win;

    /* seed the CRTL random number generator */
    srand((unsigned int)time(0));

    /* load Scintilla */
    if (!ScintillaWin::load_dll())
    {
        /* explain what happened */
        MessageBox(0, "An error occurred loading the Scintilla text "
                   "editor module. Workbench cannot run without this "
                   "module. Your Workbench installation might be "
                   "corrupted; you might need to reinstall.",
                   "TADS Workbench", MB_OK | MB_ICONERROR | MB_TASKMODAL);

        /* tell the caller to abort */
        return FALSE;
    }

    /*
     *   On the first time through, create the debugger window with no
     *   engine context.  If there's a game name command line argument,
     *   load the game's configuration, but not the game.  If there's no
     *   game on the command line, simply load the default configuration.
     */
    if (iter == 0)
        S_main_win = new CHtmlSys_dbgmain(0, argc > 1 ? argv[argc-1] : 0);

    /* load cursors */
    line_drag_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(IDC_DRAG_MOVE_CSR));

    /* get the game window */
    win = CHtmlSys_mainwin::get_main_win();

    /* set the initial "open" dialog directory on the first iteration */
    if (iter == 0 && win != 0 && win->get_prefs() != 0
        && win->get_prefs()->get_init_open_folder() != 0)
    {
        /* set the open file path */
        CTadsApp::get_app()
            ->set_openfile_path(win->get_prefs()->get_init_open_folder());
    }

    /* read events until the user tells us to start running the game */
    for (;;)
    {
        /*
         *   if there's a game pending loading, load its configuration
         *   (this will have no effect if we've already loaded this game's
         *   configuration on another pass through this loop, since the
         *   configuration loader always checks for and skips loading the
         *   same file)
         */
        if (win != 0 && win->get_pending_new_game() != 0)
            S_main_win->load_config(win->get_pending_new_game(), FALSE);

        /* run the event loop */
        if (!S_main_win->dbg_event_loop(0, 0, 0, &restart, &reload,
                                        &terminate, &abort, 0))
        {
            /* they want to shut down - delete everything */
            S_main_win->terminate_debugger();
            delete_source_dir_list();

            /* tell the caller to terminate */
            return FALSE;
        }

        /*
         *   if they wish to begin execution, return so that the caller
         *   can enter TADS; otherwise, keep going, since we do not yet
         *   wish to begin execution
         */
        if (S_main_win->get_reloading_exec())
            return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Perform post-quit processing.  Returns true if we should start over
 *   for another loop through the TADS main entrypoint, false if we should
 *   terminate the program.
 */
int w32_post_quit(int main_ret_code)
{
    /* if there's no debugger main window, terminate */
    if (S_main_win == 0)
    {
        /*
         *   if an error occurred, pause for a key so that the user has a
         *   chance to view the error
         */
        if (main_ret_code != OSEXSUCC)
            os_expause();

        /* tell the caller to terminate */
        return FALSE;
    }

    /* the game is no longer loaded */
    S_main_win->set_game_loaded(FALSE);

    /* tell the caller to start over */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Display a message box to show a message from the VM.
 *
 *   Workbench sends itself command messages, so it's sometimes problematic
 *   to let a message box come up at random times.  The system message box
 *   will sit in a loop dispatching messages in the foreground window's
 *   queue, but this loop will never terminate in some cases because we keep
 *   posting follow-up messages to carry out deferred actions.  To prevent
 *   this from happening, we simply need to integrate the message box into
 *   the same message queue, by posting a custom message to our main window
 *   to display the message box.
 */
void w32_msgbox(const char *msg, const char *url)
{
    /* get the game window */
    CHtmlSys_dbgmain *win = CHtmlSys_dbgmain::get_main_win();

    /* if there's a main window, post the request to the window */
    if (win != 0)
    {
        /* allocate a copy of the string, for the posted message */
        char *alomsg = 0;
        if (msg != 0)
        {
            alomsg = (char *)th_malloc(strlen(msg) + 1);
            strcpy(alomsg, msg);
        }

        /* post the request */
        PostMessage(win->get_handle(), HTMLM_MSGBOX, 0, (LPARAM)alomsg);
    }
    else
    {
        /* no debugger window - show the message box directly */
        MessageBox(0, msg, "TADS", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Static list of directories we've seen in dbgu_find_src().  Each time
 *   we find a file, we'll remember the directory where we found it, since
 *   it's likely that there will be more files in the same place.  Then,
 *   if we're asked to locate another file, we'll check in all of the
 *   places we've found files previously before bothering the user with a
 *   new dialog.
 */
class CDbg_win32_srcdir
{
public:
    /* the directory prefix */
    CStringBuf dir_;

    /* next item in our list */
    CDbg_win32_srcdir *next_;
};

/* head of our list of directories */
static CDbg_win32_srcdir *srcdirs_ = 0;

/* ------------------------------------------------------------------------ */
/*
 *   Delete static variables
 */
static void delete_source_dir_list()
{
    /* delete all of the source directories we might have allocated */
    while (srcdirs_ != 0)
    {
        CDbg_win32_srcdir *nxt;

        /* remember the next item */
        nxt = srcdirs_->next_;

        /* delete this item */
        delete srcdirs_;

        /* move on to the next item */
        srcdirs_ = nxt;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger user interface entrypoints.  The TADS code calls these
 *   entrypoints to let the debugger user interface take control at
 *   appropriate points.
 */

/*
 *   This UI implementation allows moving the instruction pointer, so we
 *   can resume from an error.
 */
int html_dbgui_err_resume(dbgcxdef *, void *)
{
    return TRUE;
}

/*
 *   Debugger user interface initialization.  The debugger calls this
 *   routine during startup to let the user interface set itself up.
 */
void *html_dbgui_ini(dbgcxdef *ctx, const char *game_filename)
{
    /*
     *   if we've already created the debugger window, use the existing
     *   one; otherwise, create a new one
     */
    if (S_main_win != 0)
    {
        /* use the existing window */
        S_main_win->set_dbg_ctx(ctx);

        /* load the configuration for the new game */
        S_main_win->load_config(game_filename, TRUE);
    }
    else
    {
        /* allocate the main debugger control window */
        S_main_win = new CHtmlSys_dbgmain(ctx, game_filename);
    }

    /* return the main control window as the UI object */
    return S_main_win;
}

/*
 *   Debugger user interface initialization, phase two.  The debugger
 *   calls this routine during startup, after the .GAM file has been
 *   loaded, to let the user interface do any additional setup that
 *   depends on the .GAM file having been loaded.
 */
void html_dbgui_ini2(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* tell the main control window to finish initialization */
    dbgmain->post_load_init(ctx);
}

/*
 *   Debugger user interface termination
 */
void html_dbgui_term(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* if we found the main window, shut it down */
    if (dbgmain != 0)
    {
        /* add a reference to the main window while we're working */
        dbgmain->AddRef();

        /*
         *   close the main debugger control window only if we're quitting
         *   out of the debugger
         */
        if (dbgmain->is_quitting())
        {
            /* terminate the debugger */
            dbgmain->terminate_debugger();

            /* delete the static source directory list */
            delete_source_dir_list();
        }

        /*
         *   since TADS is terminating, we must tell the main window to
         *   forget about its context
         */
        dbgmain->notify_tads_shutdown();

        /* release our reference to the main window */
        dbgmain->Release();
    }
}

/*
 *   Debugger user interface - display an error
 */
void html_dbgui_err(dbgcxdef *ctx, void * /*ui_object*/, int errnum, char *msg)
{
    char buf[256];

    /* display the message in the debug log window */
    oshtml_dbg_printf("[TADS-%d: %s]\n", errnum, msg);

    /* display it in the main game window as well */
    sprintf(buf, "[TADS-%d: %s]\n", errnum, msg);
    os_printz(buf);
}

/*
 *   Callback for the folder selector dialog.  We'll look in the selected
 *   path for the desired file, and if it exists allow immediate dismissal
 *   of the dialog.  If they clicked the OK button on a folder that
 *   doesn't contain our file, we'll show a message box complaining about
 *   it and refuse to allow dismissal of the dialog.
 */
static int dbgu_find_src_cb(void *ctx, const char *path, int okbtn, HWND dlg)
{
    const char *origname;
    char buf[OSFNMAX + 128];
    size_t len;

    /* get the original name - we used this as the callback context */
    origname = (const char *)ctx;

    /* get the path */
    len = strlen(path);
    memcpy(buf, path, len);

    /* append backslash if it doesn't end in one already */
    if (len == 0 || buf[len-1] != '\\')
        buf[len++] = '\\';

    /* append the filename */
    strcpy(buf + len, origname);

    /*
     *   check to see if the file exists - if it does, dismiss the dialog
     *   by returning true
     */
    if (!osfacc(buf))
        return TRUE;

    /*
     *   We didn't find the file - we can't dismiss the dialog yet.  If
     *   the user clicked OK, complain about it.  (Don't complain if they
     *   merely double-clicked a directory path, since they may just be
     *   navigating.)
     */
    if (okbtn)
    {
        sprintf(buf, "This folder does not contain the file \"%s\".",
                origname);
        MessageBox(dlg, buf, "File Not Found", MB_OK | MB_ICONEXCLAMATION);
    }

    /* don't allow dismissal */
    return FALSE;
}

/*
 *   Find a source file using UI-dependent mechanisms.  If we aren't
 *   required to find the file, we'll do nothing except return success, to
 *   tell the caller to defer asking us to find the file until they really
 *   need it.  If we are required to find the file, we'll use a standard
 *   file open dialog to ask the user to locate the file.
 */
int html_dbgui_find_src(const char *origname, int origlen,
                        char *fullname, size_t full_len, int must_find_file)
{
    char title_buf[OSFNMAX + 128];
    char prompt_buf[OSFNMAX + 128];
    CDbg_win32_srcdir *srcdir;
    size_t len;
    CHtmlDbgProjWin *projwin;

    /* check for an absolute path */
    if (os_is_file_absolute(origname))
    {
        /*
         *   we have an absolute path - first, check to see if the file
         *   exists at the path, and if so, just use the given path
         */
        if (!osfacc(origname))
        {
            /* this is the correct path - use the original name as-is */
            strcpy(fullname, origname);
            return TRUE;
        }

        /*
         *   The file doesn't exist at this path.  Drop the path part and
         *   look for the filename in the usual manner.
         */
        origname = os_get_root_name((char *)origname);
    }

    /*
     *   They do require the file.  First, scan the current project list to
     *   see if we can find a file with the same root name.  If we can, then
     *   use the path to that file.
     */
    if (S_main_win != 0 && (projwin = S_main_win->get_proj_win()) != 0)
    {
        /* ask the project window to find the file */
        if (projwin->find_file_path(fullname, full_len, origname))
            return TRUE;
    }

    /*
     *   We didn't find the file in the project list, so the next place to
     *   look is the build source/include path.
     */
    CHtmlDebugConfig *lc;
    if (S_main_win != 0 && (lc = S_main_win->get_local_config()) != 0)
    {
        int i, cnt;
        char buf[OSFNMAX + 128];

        /* run through the include directories */
        cnt = lc->get_strlist_cnt("build", "includes");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this include directory, and build out the full filename */
            lc->getval("build", "includes", i, buf, sizeof(buf));
            os_build_full_path(fullname, full_len, buf, origname);

            /* if the file exists, it's the one we're looking for */
            if (!osfacc(fullname))
                return TRUE;
        }
    }

    /*
     *   The file isn't in any of the explicit project directories.  Check to
     *   see if it's in our cached list of directories we've already visited
     *   when looking for other files.
     */
    for (srcdir = srcdirs_ ; srcdir != 0 ; srcdir = srcdir->next_)
    {
        /* build the full path to this file */
        os_build_full_path(fullname, full_len, srcdir->dir_.get(), origname);

        /* if the file exists, return success */
        if (!osfacc(fullname))
            return TRUE;
    }

    /*
     *   if we don't *really* need to find it, return success, to indicate to
     *   the caller that they should ask us again when it's actually
     *   important -- this way, we'll avoid pestering the user with a bunch
     *   of file-open requests on startup, and instead ask the user for files
     *   one at a time as they're actually needed, which is much less
     *   irritating
     */
    if (!must_find_file)
    {
        fullname[0] = '\0';
        return TRUE;
    }

    /*
     *   We couldn't find it in our cached directory list.  Put up the
     *   folder-browser dialog to ask the user to locate the file.  Since
     *   we're asking for the name, always ask for the *root* filename, in
     *   case there's a relative path in the original version.
     */
    origname = os_get_root_name((char *)origname);
    sprintf(title_buf, "%s - Please Locate File", origname);
    sprintf(prompt_buf, "&Path to file \"%s\":", origname);
    if (!CTadsDialogFolderSel2::
        run_select_folder(S_main_win != 0 ? S_main_win->get_handle() : 0,
                          CTadsApp::get_app()->get_instance(),
                          prompt_buf, title_buf, fullname, full_len, 0,
                          dbgu_find_src_cb, (void *)origname))
    {
        /* they cancelled - return failure */
        fullname[0] = '\0';
        return FALSE;
    }

    /* if the path doesn't end in a backslash, add one */
    len = get_strlen(fullname);
    if (len == 0 || (fullname[len-1] != '\\' && fullname[len-1] != '/'))
        fullname[len++] = '\\';

    /* append the filename to the path */
    strcpy(fullname + len, origname);

    /* add the prefix to our list */
    srcdir = new CDbg_win32_srcdir;
    srcdir->dir_.set(fullname, len);
    srcdir->next_ = srcdirs_;
    srcdirs_ = srcdir;

    /* success */
    return TRUE;
}


/*
 *   Debugger user interface main command entrypoint
 */
void html_dbgui_cmd(dbgcxdef *ctx, void *ui_object,
                    int bphit, int err, void *exec_ofs)
{
    int restart;
    int abort;
    int reload;
    int terminate;
    CHtmlSys_dbgmain *dbgmain;

    /* get the main control window from the debugger context */
    dbgmain = (CHtmlSys_dbgmain *)ui_object;

    /* invoke the debugger event loop in the debugger control window */
    restart = abort = FALSE;
    if (dbgmain != 0
        && !dbgmain->dbg_event_loop(ctx, bphit, err, &restart,
                                    &reload, &terminate, &abort, exec_ofs))
    {
        /*
         *   the application was terminated within the event loop - signal
         *   a TADS error so that TADS shuts itself down
         */
        CHtmlDebugHelper::signal_quit(ctx);
    }

    /*
     *   if the event loop told us to reload a new game, shut down TADS so
     *   that we can start over with the new game file
     */
    if (reload || terminate)
        CHtmlDebugHelper::signal_quit(ctx);

    /* if the event loop said to restart, do so now */
    if (restart)
        CHtmlDebugHelper::signal_restart(ctx);

    /* if the event loop said to abort, do so now */
    if (abort)
        CHtmlDebugHelper::signal_abort(ctx);

    /*
     *   check in a few moments to see if we should bring the game window to
     *   the front
     */
    if (dbgmain != 0)
        dbgmain->sched_game_to_front();
}

/*
 *   Quitting.  Enter the debugger user interface event loop as normal.
 *   If the event loop says to keep going, restart the game.
 */
void html_dbgui_quitting(dbgcxdef *ctx, void *ui_object)
{
    CHtmlSys_dbgmain *mainwin;

    /* get our UI main window from the engine context */
    mainwin = (CHtmlSys_dbgmain *)ui_object;

    /*
     *   show a message about the game quitting; if the debugger itself is
     *   quitting, there's no need to do this, since the user already
     *   knows that we're quitting
     */
    if (!mainwin->is_quitting() && !mainwin->is_reloading()
        && !mainwin->is_terminating()
        && mainwin->get_global_config_int("gameover dlg", 0, TRUE))
        MessageBox(mainwin->get_handle(),
                   "The game has terminated.", "TADS Workbench",
                   MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);

    /*
     *   if a re-load or game termination is in progress, simply return,
     *   so that TADS can shut down, allowing the outer loop to start a
     *   new TADS session
     */
    if (mainwin->is_reloading() || mainwin->is_terminating())
    {
        /* put the game in single-step mode */
        mainwin->exec_step_into();

        /* let TADS shut down so we can start over again */
        return;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Build a full filename from a special folder location.  Returns true on
 *   success, false if the CSIDL couldn't be resolved.
 *
 *   If 'relative' is true, we'll use a location relative to the CSIDL path,
 *   using the same relation as the exe file's relation to the standard
 *   Program Files folder.  For example, if the exe file is in C:\Program
 *   Files\TADS 3, we'll return the path as {CSIDL}\TADS 3.  If the exe isn't
 *   in the standard Program Files folder at all, we'll simply return the exe
 *   directory.
 *
 *   The relative location algorithm is designed to allow multiple versions
 *   to be installed on one computer, by mimicking the Program Files path in
 *   the other special paths we use.  When we're not installed in Program
 *   Files at all, the assumption is that we're installed in a user
 *   directory, so we can keep everything self-contained within that
 *   directory without the need for the application data folders.
 *
 *   If 'relative' is false, we'll build the exact path as given.
 *
 *   If 'mkdir' is true, we'll create the final resolved target directory if
 *   it doesn't already exist.
 */
static int build_special_path(char *buf, size_t buflen, int csidl,
                              const char *fname, int relative, int mkdir)
{
    /* presume we won't build a path */
    buf[0] = '\0';

    /* get the file system path for the desired special folder */
    char path[OSFNMAX];
    if (!SUCCEEDED(SHGetFolderPath(0, csidl, 0, 0, path)))
    {
        buf[0] = '\0';
        return FALSE;
    }

    /* if a relative path was requested, figure the relative base path */
    if (relative)
    {
        /* get the Program Files folder */
        char progf[OSFNMAX];
        if (!SUCCEEDED(SHGetFolderPath(0, CSIDL_PROGRAM_FILES, 0, 0, progf)))
        {
            buf[0] = '\0';
            return FALSE;
        }

        /* get the .exe filename path */
        char exe[OSFNMAX], exepath[OSFNMAX];
        GetModuleFileName(0, exe, sizeof(exe));
        os_get_path_name(exepath, sizeof(exepath), exe);

        /* if the .exe is in Program Files, figure the relative path */
        if (os_is_file_in_dir(exe, progf, TRUE, TRUE))
        {
            /*
             *   We're installed in Program Folders, so append the relative
             *   subdirectory from our Program Files path to the CSIDL path.
             */
            PathAppend(path, exepath + strlen(progf));
        }
        else
        {
            /*
             *   We're not installed within the Program Files tree, so assume
             *   that we're installed in a private user folder.  In this
             *   case, simply use that private install folder as the result
             *   path.
             */
            strcpy(path, exepath);
        }
    }

    /* build the full filename */
    os_build_full_path(buf, buflen, path, fname);

    /* if desired, create the result directory if it doesn't exist */
    if (mkdir)
    {
        /*
         *   extract the path from the finished filename - note that we pull
         *   it from the final filename rather than using the existing 'path'
         *   buffer, because 'fname' itself might have included a
         *   subdirectory
         */
        os_get_path_name(path, sizeof(path), buf);
        if (osfacc(path) && !os_mkdir(path, TRUE))
        {
            buf[0] = '\0';
            return FALSE;
        }
    }

    /* success */
    return TRUE;
}



/* ------------------------------------------------------------------------ */
/*
 *   Debugger main frame window implementation
 */

/*
 *   Get the application-wide debugger main window object
 */
CHtmlSys_dbgmain *CHtmlSys_dbgmain::get_main_win()
{
    return S_main_win;
}

/*
 *   instantiate
 */
CHtmlSys_dbgmain::CHtmlSys_dbgmain(dbgcxdef *ctx, const char *argv_last)
{
    /*
     *   Make sure all of the ITadsWorkbench command status codes match the
     *   ones we use internally.  This is just a sanity check to make sure we
     *   didn't accidentally make a change to our codes without updating the
     *   ITadsWorkbench definitions.
     */
    assert(TWB_CMD_UNKNOWN == TADSCMD_UNKNOWN);
    assert(TWB_CMD_ENABLED == TADSCMD_ENABLED);
    assert(TWB_CMD_DISABLED == TADSCMD_DISABLED);
    assert(TWB_CMD_CHECKED == TADSCMD_CHECKED);
    assert(TWB_CMD_DISABLED_CHECKED == TADSCMD_DISABLED_CHECKED);
    assert(TWB_CMD_MIXED == TADSCMD_INDETERMINATE);
    assert(TWB_CMD_DISABLED_MIXED == TADSCMD_DISABLED_INDETERMINATE);
    assert(TWB_CMD_DEFAULT == TADSCMD_DEFAULT);
    assert(TWB_CMD_DO_NOT_CHANGE == TADSCMD_DO_NOT_CHANGE);

    /* create and load the name-keyed command hash table */
    cmdname_hash = new CHtmlHashTable(128, new CHtmlHashFuncCI());
    for (HtmlDbgCommandInfo *inf = command_list_ ; inf->id != 0 ; ++inf)
    {
        /* add the entry only if it's valid for this version */
        if ((inf->sysid & w32_sysid) != 0)
            cmdname_hash->add(new CmdNameEntry(inf));
    }

    /* create and load the ID-keyed command hash table */
    command_hash = new CHtmlHashTable(128, new CHtmlHashFuncUInt());
    for (const HtmlDbgCommandInfo *inf = command_list_ ; inf->id != 0 ; ++inf)
    {
        /* add the entry only if it's valid for this version */
        if ((inf->sysid & w32_sysid) != 0)
            command_hash->add(new CHtmlHashEntryUInt(inf->id, (void *)inf));
    }

    /* start at the first custom command ID */
    next_custom_cmd_ = ID_CUSTOM_FIRST;
    ext_commands_ = 0;

    /*
     *   fetch and remember the original TADSLIB variable we inherit from our
     *   parent environment
     */
    char buf[OSFNMAX];
    size_t len = GetEnvironmentVariable("TADSLIB", buf, sizeof(buf));
    if (len != 0)
    {
        char *bufp;

        /*
         *   if the value was too long for our trial-size buffer, allocate a
         *   bigger buffer
         */
        bufp = buf;
        if (len >= sizeof(buf))
        {
            /* allocate the space */
            bufp = (char *)th_malloc(len);

            /* get the full value */
            len = GetEnvironmentVariable("TADSLIB", bufp, len);
        }

        /* remember it */
        orig_tadslib_var_.set(bufp);

        /* free the buffer if we allocated one */
        if (bufp != buf)
            th_free(bufp);
    }
    else
        orig_tadslib_var_.set("");

    /* no bookmarks yet */
    bookmarks_ = 0;
    bookmark_files_ = 0;

    /* load the global configuration */
    global_config_ = load_global_config(&global_config_filename_);

    /* set the default Extensions folder if it's not already set */
    if (global_config_->getval("extensions dir", 0, 0, buf, sizeof(buf))
        || strlen(buf) == 0)
    {
        /* set the default - {My Documents}\TADS 3\Extensions */
        if (build_special_path(buf, sizeof(buf), CSIDL_PERSONAL,
                               "TADS 3\\Extensions", FALSE, FALSE))
            global_config_->setval("extensions dir", 0, 0, buf);
    }

    /* read and cache the auto-flush preference setting */
    int int_val;
    if (global_config_->getval("gamewin auto flush", 0, &int_val))
        int_val = TRUE;
    auto_flush_gamewin_ = int_val;

    /* no MDI docking port yet */
    mdi_dock_port_ = 0 ;

    /* no toolbar yet */
    rebar_ = 0;
    toolbar_ = 0;
    menubar_ = 0;
    searchbar_ = 0;
    searchbox_ = 0;

    /* create our menu icon handler */
    iconmenu_ = new IconMenuHandler(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT);

    /*
     *   register it as the global icon menu handler - this lets us skip
     *   routing the measure/draw messages for popup menus in every window
     *   class
     */
    CTadsApp::get_app()->register_menu_handler(iconmenu_);

    /* no statusline yet */
    statusline_ = 0;
    statusline_menu_cmd_ = 0;

    /* get the statusline visibility from the configuration */
    if (global_config_->getval("statusline", "visible", &int_val))
        int_val = TRUE;
    show_statusline_ = int_val;

    /* remember the context */
    dbgctx_ = ctx;

    /* create a helper object (it'll implicitly add a reference to itself) */
    helper_ = new CHtmlDebugHelper(vsn_need_srcfiles());

    /* we don't have a local configuration object yet */
    local_config_ = 0;

    /* assume we won't need to maximize the MDI child */
    mdi_child_maximized_ = FALSE;

    /* no drag/drop in progress yet */
    drop_file_ok_ = FALSE;

    /* no accelerators yet */
    waccel_ = 0;
    taccel_ = 0;

    /* no watch/local/find/project/script windows yet */
    watchwin_ = 0;
    localwin_ = 0;
    projwin_ = 0;
    scriptwin_ = 0;

    /* not loading a window configuration yet */
    loading_win_config_ = FALSE;
    loading_dlg_ = 0;

    /* no find dialogs yet */
    find_dlg_ = 0;
    projsearch_dlg_ = 0;

    /* laod the default keyboard mappings if we haven't already */
    load_default_keys();

    /* load the custom keyboard mappings */
    load_keys_from_config();

    /* we have no workspace yet */
    workspace_open_ = FALSE;

    /* no game is loaded */
    game_loaded_ = FALSE;

    /* not building yet */
    compiler_hproc_ = 0;
    build_running_ = FALSE;
    capture_build_output_ = FALSE;
    filter_build_output_ = FALSE;
    run_after_build_ = FALSE;
    pending_build_cnt_ = 0;

    /* not waiting for a build yet */
    build_wait_dlg_ = 0;

    /* no build commands in the list yet */
    build_cmd_head_ = build_cmd_tail_ = 0;
    build_cmd_cnt_ = 0;
    build_files_ = 0;

    /* clear various status flags */
    in_debugger_ = FALSE;
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;
    refreshing_sources_ = FALSE;
    profiling_ = FALSE;

    /*
     *   When we first load, assume that we might need to build before
     *   running.  The project might have been modified externally since the
     *   last build, so it's safest to let t3make do a full dependency check
     *   the first time we run.  After that, we monitor the project files for
     *   changes, so we should be able to detect when a t3make is needed.
     */
    maybe_need_build_ = TRUE;

    /*
     *   allocate a timer for bringing the game window to the front after
     *   resuming execution
     */
    game_to_front_timer_ = alloc_timer_id();

    /* use the main window's preferences object */
    CHtmlSys_mainwin *main_win = CHtmlSys_mainwin::get_main_win();
    prefs_ = (main_win != 0 ? main_win->get_prefs() : 0);

    /* make the "default" profile the active one */
    if (prefs_ != 0)
    {
        /* make the "default" profile active */
        prefs_->set_active_profile_name("Default");

        /* load it */
        prefs_->restore(FALSE);
    }

    /* no active window yet */
    active_dbg_win_ = 0;
    last_active_dbg_win_ = 0;
    last_cmd_target_win_ = 0;
    post_menu_active_win_ = 0;

    /* set the initial key bindings based on no active window */
    update_key_bindings(FALSE);

    /* create the Loading dialog */
    if (loading_dlg_ == 0)
        loading_dlg_ = new CTadsDialogLoading();

    /* create the find dialog */
    if (find_dlg_ == 0)
        find_dlg_ = new CTadsDialogFindReplace(this);

    /* create the project search dialog */
    if (projsearch_dlg_ == 0)
        projsearch_dlg_ = new CTadsDialogFindReplace(this);

    /* no "Find" text yet (so we can't "find next") */
    find_text_[0] = '\0';
    replace_text_[0] = '\0';

    /* no extensions yet */
    extensions_ = 0;

    /* load extension DLLs */
    load_extensions();

    /* initialize the printer page setup */
    print_devmode_ = 0;
    print_devnames_ = 0;
    print_margins_set_ = FALSE;
    print_psd_flags_ = 0;

    /* initialize the build status mutex */
    stat_mutex_ = CreateMutex(0, FALSE, 0);
    stat_desc_.set("");
    stat_subdesc_.set("");
    stat_success_ = FALSE;

    /*
     *   load the configuration for the game, if a game was specified on the
     *   command line
     */
    if (argv_last != 0)
    {
        char fname[OSFNMAX];
        char *root;

        /* get the full path name to the file */
        GetFullPathName(argv_last, sizeof(fname), fname, &root);

        /* adjust to the proper extension */
        vsn_adjust_argv_ext(fname);

        /*
         *   set the initial Open File dialog directory to the game's
         *   directory
         */
        CTadsApp::get_app()->set_openfile_dir(fname);

        /* load the game's configuration */
        if (load_config(fname, ctx != 0) == 0)
        {
            /* we've successfully loaded the configuration, so we're done */
            return;
        }
        else
        {
            char msg[256 + OSFNMAX];

            /* couldn't load the configuration - mention this */
            sprintf(msg, "Unable to open project file \"%s\".", fname);
            MessageBox(main_win->get_handle(), msg, "Error", MB_OK);

            /* proceed with startup mode 3, "start with no project" */
            S_startup_mode_ = 3;
        }
    }

    /*
     *   We either didn't get a command-line argument telling us the game to
     *   load, or we were unsuccessful loading the specified game.  In either
     *   case, proceed according to the startup mode we've selected.
     */
    switch(S_startup_mode_)
    {
    case 1:
        /*
         *   use the startup command from the welcome dialog, which we
         *   already showed in w32_pre_show_window()
         */
        switch (S_startup_cmd_)
        {
        case IDC_BTN_CREATE:
            /* load the default configuration, then run the New Game Wizard */
            load_config(0, FALSE);
            run_new_game_wiz(handle_, this);
            break;

        case IDC_BTN_OPEN:
            /* load an initial default configuration, in case they cancel */
            load_config(0, FALSE);

            /* run validation on the selected file */
            if (!vsn_validate_project_file(S_startup_proj_))
            {
                /*
                 *   validation succeeded, but didn't do the loading itself -
                 *   open the game they selected from the dialog
                 */
                load_game(S_startup_proj_);
            }
            break;

        case IDC_BTN_OPENLAST:
            /* post a "load recent game #1" command to the main window */
            if (!load_recent_game(ID_GAME_RECENT_1))
            {
                /* failed - load the default config instead */
                load_config(0, FALSE);
            }
            break;
        }

        /* done */
        break;

    case 2:
        /* load the most recent project */
        if (!load_recent_game(ID_GAME_RECENT_1))
        {
            /* failed to load - start with the default config */
            load_config(0, FALSE);
        }
        break;

    case 3:
        /* start with no project - simply load the default config */
        load_config(0, FALSE);
        break;
    }
}

/*
 *   Update the build progress bar in the status line to reflect the current
 *   build percentage, as indicated by stat_build_step_ et al.
 */
void CHtmlSys_dbgmain::update_build_progress_bar(int running)
{
    /* figure the new percentage done */
    double ncmds = build_cmd_cnt_ >= 1 ? build_cmd_cnt_ : 1;
    double pct = 100.0 * (double)stat_build_step_ / ncmds;
    if (stat_step_len_ != 0)
        pct += (100.0 * (double)stat_step_progress_ / stat_step_len_) / ncmds;

    /* get the percentage in integer terms */
    int ipct = (int)pct;

    /*
     *   If the progress bar isn't already showing, don't update it until
     *   we're more than 0%, and don't bother showing it for the first time
     *   if the first update puts us past 50%.  This ensures that we only
     *   show the bar at all for builds where we're going to get fairly
     *   granular updates.  A build that does one update that's already at
     *   50% or more is going to complete so quickly that a progress bar
     *   isn't helpful, and in fact will be on-screen so briefly that its
     *   momentary appearance would be distracting by causing a bunch of
     *   redraws for no apparent purpose.
     */
    if (running && !progress_source_.running_ && (ipct == 0 || ipct > 50))
        return;

    /*
     *   Set the statistics in the progress bar source object.  If this
     *   changes the visibility of the progress bar panel, refigure the
     *   status bar layout.
     */
    if (progress_source_.set_progress(running, ipct) && statusline_ != 0)
        statusline_->notify_parent_resize();
}

/*
 *   Load the global configuration
 */
CHtmlDebugConfig *CHtmlSys_dbgmain::load_global_config(CStringBuf *f)
{
    /*
     *   Build the name of the default configuration file - this is a file
     *   with a name given by w32_tdb_global_prefs_file.  Look first in the
     *   Application Data folder; if we don't find it there, look in the
     *   install (.exe) folder.
     *
     *   Older versions simply stored this in the exe folder, but that's not
     *   good for users who don't have admin privileges, since the exe folder
     *   is usually under the system-wide C:\Program Files folder, which is
     *   read-only for non-admin users.  So we moved this to Application Data
     *   starting in Win117 (3.1.1 - Jan 2012), which is per-user and
     *   user-writable.
     *
     *   The scheme we use is upwardly compatible, as follows.  If we have an
     *   old install, the file will still be in the .exe folder, so our
     *   search in App Data won't find it and we'll fall back on the existing
     *   exe folder file.  If it's a newer installation, we'll find the file
     *   in App Data and use that one.  In either case, we'll write updates
     *   to App Data, so we'll convert an old installation into a new
     *   installation the first time the new version is run.
     */

    /* look first in Application Data */
    int fset = FALSE;
    char fname[OSFNMAX] = "";
    if (build_special_path(fname, sizeof(fname), CSIDL_LOCAL_APPDATA,
                           w32_tdb_global_prefs_file, TRUE, TRUE))
    {
        /*
         *   success - use this as the rewrite path, even if the file doesn't
         *   exist yet
         */
        f->set(fname);
        fset = TRUE;
    }

    /* if we didn't find $AppData, look in the exe folder */
    if (fname[0] == '\0' || osfacc(fname))
    {
        /* look in the exe folder */
        GetModuleFileName(0, fname, sizeof(fname));
        *os_get_root_name(fname) = '\0';
        PathAppend(fname, w32_tdb_global_prefs_file);

        /* if we didn't set the filename before, use this one */
        if (f != 0 && !fset)
            f->set(fname);
    }

    /* create and load the global configuration object */
    CHtmlDebugConfig *gc = new CHtmlDebugConfig();
    vsn_load_config_file(gc, fname);

    /* return the object */
    return gc;
}

/*
 *   Save the global configuration
 */
void CHtmlSys_dbgmain::save_global_config(
    CHtmlDebugConfig *gc, CStringBuf *fname)
{
    vsn_save_config_file(gc, fname->get());
}


/*
 *   Load extension DLLs
 */
void CHtmlSys_dbgmain::load_extensions()
{
    HANDLE h;
    WIN32_FIND_DATA info;
    char pat[MAX_PATH];

    /* create an entry for the built-in modes */
    extensions_ = new twb_extension(0, new CBuiltInModes());

    /*
     *   build the filename search pattern - this is all files with names
     *   like "*.twbAddIn" in the folder containing the executable
     */
    GetModuleFileName(
        CTadsApp::get_app()->get_instance(), pat, sizeof(pat));
    strcpy(os_get_root_name(pat), "*.twbAddIn");

    /* search for files */
    h = FindFirstFile(pat, &info);
    if (h != INVALID_HANDLE_VALUE)
    {
        /* scan all matching files */
        do
        {
            HMODULE hmod;
            char fname[MAX_PATH];

            /* build the full filename */
            strcpy(fname, pat);
            strcpy(os_get_root_name(fname), info.cFileName);

            /* try loading this as a DLL */
            if ((hmod = LoadLibrary(fname)) != 0)
            {
                typedef IUnknown *cfunc_t(ITadsWorkbench *);
                cfunc_t *proc;
                IUnknown *unk;

                /*
                 *   Try looking up the connect function.  If we find it,
                 *   call it, passing in ourself as the ITadsWorkbench
                 *   interface implementation.
                 */
                proc = (cfunc_t *)GetProcAddress(hmod, "twb_connect");
                if (proc != 0 && (unk = proc(this)) != 0)
                {
                    /* it's valid - create an extension tracker for it */
                    twb_extension *ext = new twb_extension(hmod, unk);

                    /* link it into our list */
                    ext->nxt = extensions_;
                    extensions_ = ext;
                }
                else
                {
                    /* it's not an extension library - unload it */
                    FreeLibrary(hmod);
                }
            }
        }
        while (FindNextFile(h, &info));

        /* done with the search handle - close it */
        FindClose(h);
    }
}

/*
 *   Get an integer variable value from the configuration file
 */
int CHtmlSys_dbgmain::get_global_config_int(const char *varname,
                                            const char *subvar,
                                            int default_val) const
{
    int val;

    /* if there's no configuration, return the default */
    if (global_config_ == 0)
        return default_val;

    /* read the variable; if it doesn't exist, return the default */
    if (global_config_->getval(varname, subvar, &val))
        return default_val;

    /* return the value we read */
    return val;
}

void CHtmlSys_dbgmain::add_recent_game(const char *new_game)
{
    int i;
    int j;
    char order[10];
    char first;
    CHtmlPreferences *prefs;

    /*
     *   get the main window's preferences object (we store the recent
     *   game list here, rather than in the debug config, because this
     *   information is global and so doesn't belong with a single game)
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();

    /* get the order string */
    strncpy(order, prefs->get_recent_dbg_game_order(), 4);
    order[4] = '\0';

    /*
     *   First, search the current list to see if the game is already
     *   there.  If it is, we simply shuffle the list to move this game to
     *   the front of the list.
     */
    for (i = 0 ; i < 4 ; ++i)
    {
        const textchar_t *fname;

        /* get this item */
        fname = prefs->get_recent_dbg_game(order[i]);
        if (fname == 0 || fname[0] == '\0')
            break;

        /* if this is the same game, we can simply shuffle the list */
        if (get_stricmp(new_game, fname) == 0)
        {
            /*
             *   It's already in the list - simply reorder the list to put
             *   this item at the front of the list.  To do this, we
             *   simply move all the items down one until we get to its
             *   old position.  First, note the new first item.
             */
            first = order[i];

            /* move everything down one slot */
            for (j = i ; j > 0 ; --j)
                order[j] = order[j - 1];

            /* fill in the first item */
            order[0] = first;

            /* we're done */
            goto done;
        }
    }

    /*
     *   Okay, it's not already in the list, so we need to add it to the
     *   list.  To do this, drop the last item off the list, and move
     *   everything else down one slot.  First, note the current last slot
     *   - we're going to drop the file that's currently there and make it
     *   the new first slot.
     */
    first = order[3];

    /* move everything down one position */
    for (j = 3 ; j > 0 ; --j)
        order[j] = order[j - 1];

    /* re-use the old last slot as the new first slot */
    order[0] = first;

    /* set the new first filename */
    prefs->set_recent_dbg_game(order[0], new_game);

done:
    /* set the new ordering */
    prefs->set_recent_dbg_game_order(order);

    /*
     *   save the updated settings immediately, to ensure they're safely on
     *   disk in case we should eventually crash
     */
    prefs->save();

    /* rebuild the game list */
    update_recent_game_menu();
}

/*
 *   Update the menu showing recent games
 */
void CHtmlSys_dbgmain::update_recent_game_menu()
{
    HMENU file_menu;
    int i;
    int found;
    int id;
    const textchar_t *order;
    const textchar_t *fname;
    MENUITEMINFO info;
    const textchar_t *working_dir;
    HMENU recent_menu;
    CHtmlPreferences *prefs;

    /*
     *   get the main window's preferences object (we store the recent
     *   game list here, rather than in the debug config, because this
     *   information is global and so doesn't belong with a single game)
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();

    /* get the File menu - it's the first submenu */
    file_menu = GetSubMenu(main_menu_, 0);

    /*
     *   Find the recent game submenu in the File menu.  This is the item
     *   that contains subitems; all of the subitems will have ID's in the
     *   range ID_GAME_RECENT_1 to ID_GAME_RECENT_4, or
     *   ID_GAME_RECENT_NONE.
     */
    for (i = 0, found = FALSE ; i < GetMenuItemCount(file_menu) ; ++i)
    {
        /*
         *   get this item's submenu - if it doesn't have one, it's not
         *   what we're looking for
         */
        recent_menu = GetSubMenu(file_menu, i);
        if (recent_menu == 0)
            continue;

        /* get the ID of the first item in the submenu */
        id = GetMenuItemID(recent_menu, 0);

        /* if this is one of the one's we're looking for, we're done */
        if ((id >= ID_GAME_RECENT_1 && id <= ID_GAME_RECENT_4)
            || id == ID_GAME_RECENT_NONE)
        {
            /* note that we found it */
            found = TRUE;

            /* stop searching */
            break;
        }
    }

    /* if we found a recent game item, delete all of them */
    if (found)
    {
        /* delete all of the recent game items */
        while (GetMenuItemCount(recent_menu) != 0)
            DeleteMenu(recent_menu, 0, MF_BYPOSITION);
    }

    /* get the game ordering string */
    order = prefs->get_recent_dbg_game_order();

    /* set up the basic MENUITEMINFO entries */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;

    /*
     *   check to see if we have any recent games in the prefs - if the
     *   first entry is blank, there aren't any games
     */
    fname = prefs->get_recent_dbg_game(order[0]);
    if (fname == 0 || fname[0] == '\0')
    {
        /* re-insert the "no recent games" placeholder */
        info.wID = ID_GAME_RECENT_NONE;
        info.dwTypeData = "No Recent Games";
        info.cch = strlen((char *)info.dwTypeData);
        InsertMenuItem(recent_menu, 0, TRUE, &info);

        /* we're done */
        return;
    }

    /*
     *   Get the open-file directory - we'll show all of the filenames in
     *   the recent file list relative to this directory.  Use the current
     *   open-file dialog start directory.
     */
    working_dir = CTadsApp::get_app()->get_openfile_dir();

    /* insert as many items as we have defined */
    for (i = 0 ; i < 4 && i < (int)get_strlen(order) ; ++i)
    {
        char buf[OSFNMAX + 10];

        /* get this filename */
        fname = prefs->get_recent_dbg_game(order[i]);
        if (fname == 0 || fname[0] == '\0')
            break;

        /* build the menu string prefix */
        sprintf(buf, "&%d ", i + 1);

        /* get the filename relative to the open-file-dialog directory */
        if (working_dir != 0)
        {
            /* get the path relative to the working directory */
            os_get_rel_path(buf + strlen(buf), sizeof(buf) - strlen(buf),
                            working_dir, fname);
        }
        else
        {
            /* we have no working directory - use the absolute path */
            strncpy(buf + strlen(buf), fname, sizeof(buf) - strlen(buf));
            buf[sizeof(buf) - 1] = '\0';
        }

        /* add this item */
        info.wID = ID_GAME_RECENT_1 + i;
        info.dwTypeData = buf;
        info.cch = strlen(buf);
        InsertMenuItem(recent_menu, i, TRUE, &info);
    }
}

/*
 *   Load the configuration for a game file.  Returns zero on success,
 *   non-zero if we fail to load the game.
 */
int CHtmlSys_dbgmain::load_config(const char *load_filename, int game_loaded)
{
    RECT rc;
    RECT deskrc;
    CHtmlRect winpos;
    int winmax;
    textchar_t game_filename_buf[OSFNMAX];
    textchar_t *game_filename;
    textchar_t config_filename[OSFNMAX];
    int vis;
    int default_pos;
    int had_old_config;
    int new_config_exists;
    int i, cnt;
    HWND *zlist = 0;
    const textchar_t *p;
    int dlg_open = FALSE;

    /* get the filenames for the game and configuration files */
    if (!vsn_get_load_profile(&game_filename, game_filename_buf,
                              config_filename, load_filename, game_loaded))
        return 1;

    /* set the busy cursor while working */
    HCURSOR oldcsr = SetCursor(wait_cursor_);

    /* note whether or not the new configuration file exists */
    new_config_exists = (load_filename != 0 && !osfacc(config_filename));

    /* note whether we had an old configuration */
    had_old_config = (local_config_ != 0);

    /* if we have a configuration, save it, since we're about to replace it */
    if (local_config_ != 0 && local_config_filename_.get() != 0)
        vsn_save_config_file(local_config_, local_config_filename_.get());

    /* set the new config file name */
    if (load_filename != 0)
    {
        char full_path[OSFNMAX];
        char *root_name;

        /* set the new configuration filename */
        local_config_filename_.set(config_filename);

        /* we now have a workspace open */
        workspace_open_ = TRUE;

        /*
         *   Set the working directory to the directory containing the
         *   config file.  First, get the absolute path to the config
         *   file, then set the working directory to the path prefix part.
         */
        if (GetFullPathName(config_filename, sizeof(full_path), full_path,
                            &root_name) != 0)
        {
            /*
             *   remember the full path of the file, so that we have no
             *   dependency on the working directory
             */
            local_config_filename_.set(full_path);

            /* clear out everything from the final separator on */
            if (root_name != 0)
            {
                /* if it ends with a '\' or '/', remove the trailing slash */
                if (root_name != full_path
                    && (*(root_name - 1) == '\\' || *(root_name - 1) == '/'))
                    *--root_name = '\0';

                /*
                 *   if it's now empty, or it ends with a ':', add a '\' -
                 *   unlike any other directory path, the root directory
                 *   of a drive ends with a backslash
                 */
                if (root_name == full_path || *(root_name - 1) == ':')
                    strcpy(root_name, "\\");

                /* set the working directory */
                SetCurrentDirectory(full_path);
            }
        }
    }


    /* delete our old source directory list */
    delete_source_dir_list();

    /* close all source windows */
    helper_->enum_source_windows(&enum_win_close_cb, this);

    /* remember the new game filename */
    if (game_filename != 0)
        game_filename_.set(game_filename);

    /*
     *   If we had a previous configuration, and the new configuration
     *   doesn't exist, don't do anything - just clear out some stuff from
     *   the old configuration, but keep the rest as the starting point of
     *   the new configuration.
     */
    if (had_old_config && !new_config_exists)
    {
        /* clear out the parts that don't stay around in a default config */
        clear_non_default_config();

        /* clear any breakpoints that are hanging around */
        helper_->delete_internal_bps();

        /* clear any line sources that were loaded */
        helper_->delete_internal_sources();

        /* set the debugger defaults for the new configuration */
        set_dbg_defaults(this);

        /* set the build defaults for the new configuration */
        set_dbg_build_defaults(this, get_gamefile_name());

        /*
         *   skip the configuration loading, since there's no prior
         *   configuration to load; just skip ahead to where we finish up
         *   with the initialization
         */
        goto finish_load;
    }

    /*
     *   if we already have a configuration object, delete it, so that we
     *   can start fresh with a new configuration
     */
    if (local_config_ != 0)
    {
        /* delete the old configuration */
        delete local_config_;
        local_config_ = 0;
    }

    /*
     *   Close the docking windows, so that we can open them in their new
     *   state.
     */
    close_tool_windows();

    /*
     *   Next, set up for the new game
     */

    /* create a new local configuration object */
    local_config_ = new CHtmlDebugConfig();

    /*
     *   read the configuration file - if the new configuration file
     *   exists, read it; otherwise, load the default configuration file
     */
    if (new_config_exists)
    {
        /* load the configuration file */
        vsn_load_config_file(local_config_, local_config_filename_.get());
    }
    else if (global_config_filename_.get() != 0)
    {
        /* no game configuration file - read the default configuration */
        vsn_load_config_file(local_config_, global_config_filename_.get());
    }

    /*
     *   now that we've loaded the configuration, get the name of the game
     *   file; for t3 workbench, we can't know the game filename until here
     */
    if (new_config_exists
        && vsn_get_load_profile_gamefile(game_filename_buf))
        game_filename_.set(game_filename_buf);

    /* mark that configuration loading is in progress */
    loading_win_config_ = TRUE;

    /* clear out old bookmarks */
    delete_bookmarks();

    /*
     *   Load bookmarks.  Note that we do this in reverse order - we save
     *   them in list order, which is most recently created to least recently
     *   created, so we need to create them from the least recent end to get
     *   the same recency order.
     */
    cnt = local_config_->get_strlist_cnt("editor", "bookmark");
    for (i = cnt ; i > 0 ; )
    {
        int line, name = 0;
        const textchar_t *val, *p;

        /* read this entry */
        --i;
        val = local_config_->getval_strptr("editor", "bookmark", i);

        /* parse the line number */
        line = atoi(val);

        /* find and parse the name */
        if ((p = strchr(val, ',')) != 0)
            ++p, name = atoi(p);

        /* if we can find the filename, add the bookmark */
        if (p != 0 && (p = strchr(p, ',')) != 0)
            add_bookmark(p + 1, -1, line, name);
    }

    /* load the source directory list */
    rebuild_srcdir_list();

    /* set initial configuration options */
    on_update_srcwin_options();

    /* read my stored position from the preferences */
    if (!local_config_->getval("main win pos", 0, &winpos)
        && !local_config_->getval("main win max", 0, &winmax))
    {
        /* got it - use the stored position */
        SetRect(&rc, winpos.left, winpos.top, winpos.right, winpos.bottom);
        default_pos = FALSE;

        /* if the window is off the screen, use the default sizes */
        GetClientRect(GetDesktopWindow(), &deskrc);
        if (!winmax
            && (rc.left < deskrc.left || rc.left > deskrc.right
                || rc.top < deskrc.top || rc.top > deskrc.bottom
                || rc.right < 20 || rc.bottom < 20))
        {
            /* the stored position looks invalid - use a default position */
            w32tdb_set_default_frame_rc(&rc);
            default_pos = TRUE;
        }
    }
    else
    {
        /* no configuration - use the default position */
        w32tdb_set_default_frame_rc(&rc);
        winmax = FALSE;
        default_pos = TRUE;
    }

    /*
     *   open my window if we haven't already; otherwise, move the window
     *   to the new position if we have a saved position
     */
    if (handle_ == 0)
    {
        /* create the window in the default location */
        create_system_window(0, !winmax, "TADS Workbench", &rc,
                             w32tdb_new_frame_syswin(this, -2));

        /* set it up with our special icons - large... */
        HICON ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                     MAKEINTRESOURCE(IDI_DBGWIN),
                                     IMAGE_ICON, 32, 32, 0);
        SendMessage(handle_, WM_SETICON, ICON_BIG, (LPARAM)ico);

        /* ... and small */
        ico = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDI_DBGWIN),
                               IMAGE_ICON, 16, 16, 0);
        SendMessage(handle_, WM_SETICON, ICON_SMALL, (LPARAM)ico);

        /* create and register the MDI client docking port */
        mdi_dock_port_ = w32tdb_create_dockport(this,
                                                get_parent_of_children());

        /* maximize it if appropriate */
        if (winmax)
            ShowWindow(handle_, SW_SHOWMAXIMIZED);
    }
    else if (!default_pos)
    {
        /* move the existing window to the saved position */
        if (winmax)
            ShowWindow(handle_, SW_SHOWMAXIMIZED);
        else
            MoveWindow(handle_, rc.left, rc.top,
                       rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }

    /* put up the "loading" dialog box */
    dlg_open = TRUE;
    loading_dlg_->open_modeless(
        DLG_LOADING, handle_, CTadsApp::get_app()->get_instance());

    /* set the initial status message */
    loading_dlg_->status("Loading %s...",
                         load_filename != 0
                         ? os_get_root_name((char *)load_filename)
                         : "Project");

    /*
     *   Hide the MDI client while we're doing the load, to speed things up.
     *   This avoids letting each new text window's title bar pop up as we
     *   load it.  These loads can take a while with large files; this looks
     *   visually cluttered, especially if the windows will eventually be
     *   maximized, in which case the title bars won't be visible in the end
     *   at all.  By hiding the client we just get a blank window while
     *   loading; when we reshow the window, everything's in its finished
     *   state.
     */
    UpdateWindow(handle_);
    ShowWindow(get_parent_of_children(), SW_HIDE);

    /* Don't maximize while loading - we'll do this when we're done */
    mdi_child_maximized_ = FALSE;

    /* load the local toolbar configuration */
    loading_dlg_->status("Setting up toolbars...");
    load_toolbar_config(local_config_);

    /*
     *   load the docking configuration - note that we need to finish
     *   creating the main window before we do this, because we can't load
     *   the docking configuration until we have a dockport, and we don't
     *   have a dockport until we have a main window
     */
    loading_dlg_->status("Setting up window layout...");
    load_dockwin_config(local_config_);

    /*
     *   Get the desktop window area, so that we can set up default
     *   positions for our windows at the edges of the desktop area.  This
     *   will retrieve the area of the entire Windows desktop in SDI mode,
     *   or of the MDI client area in MDI mode.
     */
    w32tdb_get_desktop_rc(this, &deskrc);

    /*
     *   Load the MDI windows first.  This ensures that we'll restore the Z
     *   order of any MDI tool windows by creating them along with the source
     *   windows.
     *
     *   The list is saved in Z order from front to back.  To get the same
     *   window arrangement, we need to create the list in order from back to
     *   front.  So, scan the list in reverse order.
     */
    cnt = local_config_->get_strlist_cnt("mdi win list", 0);
    zlist = new HWND[cnt];
    for (i = cnt - 1 ; i >= 0 ; --i)
    {
        textchar_t path[OSFNMAX];
        textchar_t fname[OSFNMAX];

        /* get the next window's name */
        if (!local_config_->getval("mdi win list", 0, i, path, sizeof(path))
            && !local_config_->getval("mdi win title", 0, i,
                                      fname, sizeof(fname)))
        {
            IDebugWin *iw = 0;
            CTadsWin *tw = 0;

            /* check for special windows */
            if (path[0] == ':')
            {
                /*
                 *   It's a tool window - look up the name and load the
                 *   window.
                 */
                loading_dlg_->status("Restoring window layout...");
                if (strcmp(path, ":log") == 0)
                    iw = helper_->create_debuglog_win(this);
                else if (strcmp(path, ":filesearch") == 0)
                    iw = helper_->create_file_search_win(this);
                else if (strcmp(path, ":docsearch") == 0)
                    iw = helper_->create_doc_search_win(this);
                else if (strcmp(path, ":stack") == 0)
                    iw = helper_->create_stack_win(this);
                else if (strcmp(path, ":trace") == 0)
                    iw = helper_->create_hist_win(this);
                else if (strcmp(path, ":watch") == 0)
                    tw = create_watch_win(&deskrc);
                else if (strcmp(path, ":locals") == 0)
                    tw = create_locals_win(&deskrc);
                else if (strcmp(path, ":project") == 0)
                    tw = create_proj_win(&deskrc);
                else if (strcmp(path, ":script") == 0)
                    tw = create_script_win(&deskrc);
                else if (strcmp(path, ":help") == 0)
                    iw = helper_->create_help_win(this);
            }
            else
            {
                /* it's an ordinary source file - open it */
                loading_dlg_->status("Loading file %s...", fname);
                iw = helper_->open_text_file(this, fname, path);
            }

            /* save the window in the z-order list */
            zlist[i] = (iw != 0 ? ((IDebugSysWin *)iw)->idsw_get_handle() :
                        tw != 0 ? tw->get_handle() : 0);
        }
    }

    /*
     *   Put the MDI tab list back into its saved order.  The "mdi tab order"
     *   string is a list of integers.  The index in this list is the same as
     *   the tab index - i.e., the nth entry in the list gives the identity
     *   of the nth MDI tab.  The value of the list entry is the index in the
     *   z-order list of the HWND of that tab.
     */
    loading_dlg_->status("Restoring window layout...");
    if ((p = local_config_->getval_strptr("mdi tab order", 0, 0)) != 0)
    {
        int tab_cnt = mdi_tab_->get_count();

        /* set the tab order for each window's tab */
        for (i = 0 ; *p != '\0' ; ++i)
        {
            /* the list entry is the Z-order index - parse it */
            int z_idx = atoi(p);

            /*
             *   if it's in range, and we managed to open a window for this
             *   entry in the z-list, find the window in the tab list
             */
            if (z_idx >= 0 && z_idx < cnt && zlist[z_idx] != 0)
            {
                HWND hwnd = zlist[z_idx];
                int old_idx;

                /* find the tab with this window handle */
                for (old_idx = 0 ; old_idx < tab_cnt ; ++old_idx)
                {
                    CTadsWin *win;

                    /* if this is the one, move it */
                    win = (CTadsWin *)mdi_tab_->get_item_param(old_idx);
                    if (win != 0
                        && (hwnd == win->get_handle()
                            || IsChild(win->get_handle(), hwnd)))
                    {
                        /* this is our tab - move it to the saved position */
                        mdi_tab_->move_item(old_idx, i);

                        /* no need to keep looking for it */
                        break;
                    }
                }
            }

            /* find the next entry */
            for ( ; *p != '\0' && *p != ',' ; ++p) ;
            if (*p == ',')
                ++p;
        }
    }

    /* create the local variables and watch expressions windows */
    create_locals_win(&deskrc);
    create_watch_win(&deskrc);

    /* create the tool windows that were left visible in the last run */
    if (!local_config_->getval("stack win vis", 0, &vis) && vis)
        helper_->create_stack_win(this);
    if (!local_config_->getval("hist win vis", 0, &vis) && vis)
        helper_->create_hist_win(this);
    if (!local_config_->getval("log win vis", 0, &vis) && vis)
        helper_->create_debuglog_win(this);
    if (!local_config_->getval("docsearch win vis", 0, &vis) && vis)
        helper_->create_doc_search_win(this);
    if (!local_config_->getval("filesearch win vis", 0, &vis) && vis)
        helper_->create_file_search_win(this);
    if (!local_config_->getval("help win vis", 0, &vis) && vis)
        helper_->create_help_win(this);

    /* create version-specific windows */
    vsn_load_config_open_windows(&deskrc);

    /* load search configuration */
    if (local_config_->getval("search", "exact case", &find_dlg_->exact_case))
        find_dlg_->exact_case = FALSE;
    if (local_config_->getval("search", "wrap", &find_dlg_->wrap_around))
        find_dlg_->wrap_around = TRUE;
    if (local_config_->getval("search", "dir", &find_dlg_->direction))
        find_dlg_->direction = 1;
    if (local_config_->getval("search", "start at top",
                              &find_dlg_->start_at_top))
        find_dlg_->start_at_top = FALSE;
    if (local_config_->getval("search", "regex", &find_dlg_->regex))
        find_dlg_->regex = FALSE;
    if (local_config_->getval("search", "whole word", &find_dlg_->whole_word))
        find_dlg_->whole_word = FALSE;
    local_config_->getval("search", "text", 0, find_text_, sizeof(find_text_));
    local_config_->getval("search", "text", 1,
                          replace_text_, sizeof(replace_text_));
    load_find_config(local_config_, "search dlg");

    /* load the project search configuration */
    projsearch_dlg_->exact_case = local_config_->getval_int(
        "proj search", "exact case", FALSE);
    projsearch_dlg_->regex =  local_config_->getval_int(
        "proj search", "regex", FALSE);
    projsearch_dlg_->whole_word =  local_config_->getval_int(
        "proj search", "whole word", FALSE);
    projsearch_dlg_->collapse_sp = local_config_->getval_int(
        "proj search", "collapse spaces", FALSE);

    /*
     *   If there's no game loaded, load the source file list from the
     *   configuration.  Don't do this if there's a game loaded, because
     *   we'll want to use the up-to-date source list in the compiled game
     *   file instead in this case.
     */
    if (!game_loaded)
        helper_->init_after_load(0, this, local_config_, FALSE);

    /* set the debugger defaults */
    set_dbg_defaults(this);

    /* set the build defaults */
    set_dbg_build_defaults(this, get_gamefile_name());

finish_load:
    /* remove the loading dialog if we displayed it */
    if (dlg_open)
        loading_dlg_->close_window();

    /*
     *   load the file menu with the list of sources we just loaded, if we
     *   are not yet running the game
     */
    if (!game_loaded)
        load_file_menu();

    /* not yet quitting, restarting, reloading, or aborting */
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;

    /* not yet refreshing sources */
    refreshing_sources_ = FALSE;

    /* done loading */
    loading_win_config_ = FALSE;

    /*
     *   Assume we need to rebuild the project, since some of the source
     *   files could have been modified outside of the Workbench environment
     *   since the last time Workbench saw the project.  We don't actually
     *   bother with a dependency check here, since that's exactly what
     *   t3make will do when it runs.  So, we can simply assume that the
     *   project is dirty, to ensure that we invoke t3make on the next
     *   auto-build-on-run.
     */
    maybe_need_build_ = TRUE;

    /*
     *   get the MDI child-window-maximized status; if it's not stored,
     *   default to not-maximized
     */
    int maxed;
    if (local_config_->getval("main win mdi max", 0, &maxed)
        && global_config_->getval("main win mdi max", 0, &maxed))
        maxed = FALSE;

    /* set our internal memory of the MDI child max status */
    mdi_child_maximized_ = maxed;

    /*
     *   If we left things in the saved configuration with the MDI child
     *   window maximized, re-maximize the window now; if not, restore now in
     *   case we inherited a maximized state from the prior configuration.
     */
    SendMessage(get_parent_of_children(),
                mdi_child_maximized_ ? WM_MDIMAXIMIZE : WM_MDIRESTORE,
                (WPARAM)(HWND)SendMessage(get_parent_of_children(),
                                          WM_MDIGETACTIVE, 0, 0), 0);

    /* add this game to the list of recently-opened games */
    if (load_filename != 0)
        add_recent_game(local_config_filename_.get());

    /* update the window title */
    vsn_set_win_title();

    /*
     *   post an MDI status update to process after things settle down from
     *   all this window creation activity
     */
    PostMessage(handle_, HTMLM_UPDATE_MDI_STATUS, MAKEWPARAM(maxed, TRUE), 0);

    /* show the MDI client again */
    ShowWindow(get_parent_of_children(), SW_SHOW);

    /* restore the cursor */
    SetCursor(oldcsr);

    /* free the z-order list */
    if (zlist != 0)
        delete [] zlist;

    /* success */
    return 0;
}

/*
 *   Close all tool windows
 */
void CHtmlSys_dbgmain::close_tool_windows()
{
    /* close the watch window */
    if (watchwin_ != 0)
    {
        SendMessage(GetParent(watchwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        watchwin_ = 0;
    }

    /* close the locals window */
    if (localwin_ != 0)
    {
        SendMessage(GetParent(localwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        localwin_ = 0;
    }

    /* close any extra tool windows that this debugger version uses */
    vsn_load_config_close_windows();

    /* close the common tool windows */
    if (helper_->get_stack_win() != 0)
        ((IDebugSysWin *)helper_->get_stack_win())->idw_close();
    if (helper_->get_hist_win() != 0)
        ((IDebugSysWin *)helper_->get_hist_win())->idw_close();
    if (helper_->get_doc_search_win() != 0)
        ((IDebugSysWin *)helper_->get_doc_search_win())->idw_close();
    if (helper_->get_file_search_win() != 0)
        ((IDebugSysWin *)helper_->get_file_search_win())->idw_close();
    if (helper_->get_debuglog_win() != 0)
        SendMessage(GetParent(GetParent(
            ((IDebugSysWin *)helper_->get_debuglog_win())
            ->idsw_get_handle())), DOCKM_DESTROY, 0, 0);
    if (helper_->get_help_win() != 0)
        helper_->get_help_win()->idw_close();
}

/*
 *   Create the local variables window
 */
CHtmlDbgWatchWin *CHtmlSys_dbgmain::create_locals_win(const RECT *deskrc)
{
    /* if we don't already have a locals window, create it */
    if (localwin_ == 0)
    {
        RECT rc;
        int vis = FALSE;

        /* create and initialize the window */
        localwin_ = new CHtmlDbgWatchWinLocals();
        localwin_->init_panels(this, helper_);

        /* load its saved configuration */
        SetRect(&rc, deskrc->left + 2, deskrc->bottom - 2 - 150, 300, 150);
        rc.right += rc.left;
        rc.bottom += rc.top;
        localwin_->load_win_config(local_config_, "locals win", &rc, &vis);
        load_dockwin_config_win(local_config_, "locals win", localwin_,
                                "Locals", vis, TADS_DOCK_ALIGN_BOTTOM,
                                270, 170, &rc, TRUE, FALSE);
        localwin_->load_panel_config(local_config_, "locals win");
    }

    /* return the window */
    return localwin_;
}

/*
 *   Create the watch expressions window
 */
CHtmlDbgWatchWin *CHtmlSys_dbgmain::create_watch_win(const RECT *deskrc)
{
    /* if we don't already have the window, create it */
    if (watchwin_ == 0)
    {
        RECT rc;
        int vis = FALSE;

        /* create the watch window */
        watchwin_ = new CHtmlDbgWatchWinExpr();
        watchwin_->init_panels(this, helper_);

        /* load its saved configuration */
        SetRect(&rc, deskrc->left+2+300+2, deskrc->bottom-2-150, 300, 150);
        rc.right += rc.left;
        rc.bottom += rc.top;
        watchwin_->load_win_config(local_config_, "watch win", &rc, &vis);
        load_dockwin_config_win(local_config_, "watch win", watchwin_,
                                "Watch Expressions", vis,
                                TADS_DOCK_ALIGN_BOTTOM,
                                270, 170, &rc, TRUE, FALSE);
        watchwin_->load_panel_config(local_config_, "watch win");
    }

    /* return it */
    return watchwin_;
}

/*
 *   Save configuration information
 */
int CHtmlSys_dbgmain::save_find_config(CHtmlDebugConfig *config,
                                       const textchar_t *varname)
{
    int i;
    const char *str;

    /* forget any old list of search strings in the configuration */
    config->clear_strlist(varname, "old searches");
    config->clear_strlist(varname, "old search opts");

    /* save the new ones */
    for (i = 0 ; i < find_dlg_->get_max_old_search_strings() ; ++i)
    {
        char opts[10];

        /* get this string */
        str = find_dlg_->get_old_search_string(i, opts, sizeof(opts));

        /* if it's null, we've reached the end of the list */
        if (str == 0)
            break;

        /* save the string */
        config->setval(varname, "old searches", i, str);
        config->setval(varname, "old search opts", i, opts);
    }

    /* save the replacement strings as well */
    config->clear_strlist(varname, "old repl text");
    for (i = 0 ; i < find_dlg_->get_max_old_repl_strings() ; ++i)
    {
        /* get the next string */
        if ((str = find_dlg_->get_old_repl_string(i)) == 0)
            break;

        /* save it */
        config->setval(varname, "old repl text", i, str);
    }

    /* success */
    return 0;
}

/*
 *   Load configuration information for the 'find' dialog
 */
int CHtmlSys_dbgmain::load_find_config(CHtmlDebugConfig *config,
                                       const textchar_t *varname)
{
    int i;
    int cnt;
    const textchar_t *p;
    const textchar_t *opts;

    /* forget any old strings */
    find_dlg_->forget_old_search_strings();

    /* get the number of strings to load */
    cnt = config->get_strlist_cnt(varname, "old searches");

    /* load them */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this string - return an error if we couldn't load it */
        if ((p = config->getval_strptr(varname, "old searches", i)) == 0)
            return 1;

        /* get the options as well */
        if ((opts = config->getval_strptr(varname, "old search opts", i)) == 0)
            opts = "";

        /* save it */
        find_dlg_->set_old_search_string(i, p, get_strlen(p), opts);
    }

    /* load the replacement strings as well */
    cnt = config->get_strlist_cnt(varname, "old repl text");
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this string - return an error if we couldn't load it */
        if ((p = config->getval_strptr(varname, "old repl text", i)) == 0)
            return 1;

        /* save it */
        find_dlg_->set_old_repl_string(i, p, get_strlen(p));
    }

    /* success */
    return 0;
}

/*
 *   Load the source directory list from the configuration object
 */
void CHtmlSys_dbgmain::rebuild_srcdir_list()
{
    int i;
    int cnt;

    /* delete our old source directory list */
    delete_source_dir_list();

    /* get the number of entries in the configuration list */
    cnt = local_config_->get_strlist_cnt("srcdir", 0);

    /* load each entry */
    for (i = 0 ; i < cnt ; ++i)
    {
        textchar_t buf[OSFNMAX];
        CDbg_win32_srcdir *srcdir;

        /* load the next source directory */
        if (!local_config_->getval("srcdir", 0, i, buf, sizeof(buf)))
        {
            /* create the new entry */
            srcdir = new CDbg_win32_srcdir;
            srcdir->dir_.set(buf);

            /* add it to the list */
            srcdir->next_ = srcdirs_;
            srcdirs_ = srcdir;
        }
    }
}

/*
 *   Save the source directory list to the configuration object
 */
void CHtmlSys_dbgmain::srcdir_list_to_config()
{
    CDbg_win32_srcdir *srcdir;
    int i;

    /* clear the old list */
    local_config_->clear_strlist("srcdir", 0);

    /* traverse our list and store each entry in the config object */
    for (i = 0, srcdir = srcdirs_ ; srcdir != 0 ; srcdir = srcdir->next_, ++i)
        local_config_->setval("srcdir", 0, i, srcdir->dir_.get());
}

/* enum_win_untitled_cb context */
struct enum_untitled_ctx
{
    /* available title */
    char title[50];

    /* current title number */
    int n;

    /* never regress during a session */
    static int first_n;

    enum_untitled_ctx()
    {
        /* start at the next "UntitledN" name for this session */
        n = first_n++;
        make_title();
    }

    /* check to see if a window has an "UntitledN" filename */
    void test_fname(const textchar_t *fname)
    {
        int i;

        /*
         *   if the name starts with "Untitled" and ends in a number, it's
         *   one of our windows
         */
        fname = os_get_root_name((textchar_t *)fname);
        if (memicmp(fname, "Untitled", 8) == 0
            && (i = atoi(fname + 8)) != 0)
        {
            const textchar_t *p;

            /* make sure it's really all digits after "Untitled" */
            for (p = fname + 8 ; isdigit(*p) ; ++p) ;
            if (*p == '\0')
            {
                /*
                 *   It really is our kind of filename.  If this is the
                 *   high-water mark so far, bump our proposed title to the
                 *   next integer.
                 */
                if (i >= n)
                {
                    /* do them one better */
                    n = i + 1;

                    /* next time, start at the next UntitledN after this */
                    first_n = n + 1;

                    /* generate the new title */
                    make_title();
                }
            }
        }
    }

    /* build the title for the current n */
    void make_title() { sprintf(title, "Untitled%d", n); }
};
int enum_untitled_ctx::first_n = 1;

/*
 *   window enumeration callback - find the next available "UntitledN" window
 */
void CHtmlSys_dbgmain::enum_win_untitled_cb(void *ctx0, int /*idx*/,
                                            IDebugWin *win,
                                            int /*line_source_id*/,
                                            HtmlDbg_wintype_t win_type)
{
    enum_untitled_ctx *ctx = (enum_untitled_ctx *)ctx0;

    /* ignore everything but source windows */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /*
     *   run it past the context to see if we need to choose another Untitled
     *   name for the new file
     */
    CHtmlSys_dbgsrcwin *srcwin = (CHtmlSys_dbgsrcwin *)win;
    ctx->test_fname(srcwin->get_path());
}

/*
 *   ask to save changes in any modified source windows
 */
struct ask_save_ctx
{
    ask_save_ctx(int dlg)
    {
        dlg_id = dlg;
        cancel = save_all = save_none = FALSE;
    }
    int dlg_id;
    int cancel;
    int save_all;
    int save_none;
};
void CHtmlSys_dbgmain::enum_win_ask_save_cb(
    void *ctx0, int idx, IDebugWin *win, int line_source_id,
    HtmlDbg_wintype_t win_type)
{
    ask_save_ctx *ctx = (ask_save_ctx *)ctx0;

    /* if we're already canceling, don't bother asking again */
    if (ctx->cancel)
        return;

    /* ask it about closing */
    if (!win->idw_query_close(ctx))
        ctx->cancel = TRUE;
}

/*
 *   check open windows for unsaved changes
 */
struct check_save_ctx
{
    check_save_ctx() { unsaved_count = 0; }
    int unsaved_count;
};
void CHtmlSys_dbgmain::enum_win_check_save_cb(
    void *ctx0, int idx, IDebugWin *win, int line_source_id,
    HtmlDbg_wintype_t win_type)
{
    check_save_ctx *ctx = (check_save_ctx *)ctx0;

    /* if the window has unsaved changes, count it */
    if (win->idw_is_dirty())
        ctx->unsaved_count += 1;
}

/*
 *   window enumeration callback - close source windows
 */
void CHtmlSys_dbgmain::enum_win_close_cb(void *ctx0, int /*idx*/,
                                         IDebugWin *win,
                                         int /*line_source_id*/,
                                         HtmlDbg_wintype_t win_type)
{
    /* if this is a source file, close it */
    if (win_type == HTMLDBG_WINTYPE_SRCFILE)
    {
        /* close the window */
        win->idw_close();
    }
}

/*
 *   window enumeration - close existing source windows with given filename
 */
void CHtmlSys_dbgmain::enum_win_close_path_cb(void *ctx, int idx,
                                              IDebugWin *win, int srcid,
                                              HtmlDbg_wintype_t typ)
{
    /* if it's a source window, check for a name match */
    if (typ == HTMLDBG_WINTYPE_SRCFILE)
    {
        CHtmlSys_dbgsrcwin *srcwin = (CHtmlSys_dbgsrcwin *)win;
        const textchar_t *path = (const textchar_t *)ctx;

        /* if the name matches, close the window */
        if (stricmp(srcwin->get_path(), path) == 0)
            srcwin->idw_close();
    }
}


/*
 *   additional initialization after the .GAM file has been loaded
 */
void CHtmlSys_dbgmain::post_load_init(dbgcxdef *ctx)
{
    /*
     *   Initialize the helper.  Keep the breakpoint configuration if and
     *   only if we're reloading the same game.
     */
    switch(helper_->init_after_load(ctx, this, local_config_,
                                    reloading_ && !reloading_new_))
    {
    case HTMLDBG_LBP_OK:
        /* okay - nothing to report */
        break;

    case HTMLDBG_LBP_NOT_SET:
        /* breakpoints not set */
        if (get_global_config_int("bpmove dlg", 0, TRUE))
            MessageBox(handle_,
                       "One or more breakpoints from the saved configuration "
                       "could not be set.  This usually is because a source "
                       "file has been renamed or removed from the game.",
                       "TADS Workbench", MB_OK);
        break;

    case HTMLDBG_LBP_MOVED:
        /* breakpoints moved */
        if (get_global_config_int("bpmove dlg", 0, TRUE))
            MessageBox(handle_,
                       "One or more breakpoints from the saved configuration "
                       "are not on valid lines.  Each of these breakpoints "
                       "has been moved to the next executable line.",
                       "TADS Workbench", MB_OK);
        break;
    }

    /* if we're reloading, start in the correct mode */
    if (reloading_)
    {
        /*
         *   set the appropriate run mode: if we're to start running, set
         *   the state to "go", otherwise to "step"
         */
        if (reloading_go_)
            exec_go();
        else
            exec_step_into();
    }

    /* not yet quitting, restarting, reloading, etc. */
    quitting_ = FALSE;
    restarting_ = FALSE;
    reloading_ = FALSE;
    reloading_new_ = FALSE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;
    terminating_ = FALSE;
    term_in_progress_ = FALSE;
    aborting_cmd_ = FALSE;

    /* the game is now loaded */
    set_game_loaded(TRUE);

    /* we're not in the debugger yet */
    in_debugger_ = FALSE;

    /*
     *   load the source file menu, now that we know the source files
     *   (references are embedded in the .GAM file, which we've now
     *   loaded)
     */
    load_file_menu();

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();
}

/*
 *   callback context structure for source window enumeration for saving
 *   source windows
 */
struct enum_win_save_ctx_t
{
    /* mapping between an HWND and a source window */
    struct win_map
    {
        win_map() { hwnd = 0; path = 0; title = 0; }
        HWND hwnd;
        const textchar_t *path;
        const textchar_t *title;
    };

    enum_win_save_ctx_t(CHtmlDebugConfig *config, HWND mdiclient)
    {
        int i;
        HWND chi;

        config_ = config;
        z_list_ = 0;
        z_list_cnt_ = 0;

        /* count the MDI children */
        for (i = 0, chi = GetWindow(mdiclient, GW_CHILD) ; chi != 0 ;
             ++i, chi = GetWindow(chi, GW_HWNDNEXT)) ;

        /* allocate the list */
        z_list_cnt_ = i;
        z_list_ = new win_map[i];

        /* fill in the list */
        for (i = 0, chi = GetWindow(mdiclient, GW_CHILD) ; chi != 0 ;
             z_list_[i++].hwnd = chi, chi = GetWindow(chi, GW_HWNDNEXT)) ;
    }
    ~enum_win_save_ctx_t()
    {
        if (z_list_ != 0)
            delete [] z_list_;
    }

    void add_window(IDebugSysWin *win)
    {
        add_window(win->idsw_get_handle(),
                   win->idsw_get_cfg_path(), win->idsw_get_cfg_title());
    }

    void add_window(HWND hwnd, const textchar_t *path,
                    const textchar_t *title)
    {
        /* look for the HWND in the Z-order list */
        for (int i = 0 ; i < z_list_cnt_ ; ++i)
        {
            /*
             *   If this is the one, plug it in.  The enumerated window might
             *   be a child of the actual MDI container window, so match if
             *   win's handle is either the MDI window or a child of it.
             */
            if (z_list_[i].hwnd == hwnd || IsChild(z_list_[i].hwnd, hwnd))
            {
                z_list_[i].path = path;
                z_list_[i].title = title;
                break;
            }
        }
    }

    /* find a window handle's index in the z-order list */
    int find_z_index(HWND hwnd)
    {
        int i;

        /* search the list for the window handle */
        for (i = 0 ; i < z_list_cnt_ ; ++i)
        {
            if (z_list_[i].hwnd == hwnd || IsChild(z_list_[i].hwnd, hwnd))
                return i ;
        }

        /* not found */
        return -1;
    }

    /*
     *   compress the list to remove windows that we won't be able to restore
     *   when we reload the config: invisible windows, and HWNDs that we
     *   couldn't match to our own window objects
     */
    void compress_z_list()
    {
        int src, dst;
        win_map *z;

        /* scan the list to remove unusable elements */
        for (src = dst = 0, z = z_list_ ; src < z_list_cnt_ ; ++src, ++z)
        {
            /*
             *   keep this item if it's visible and we found a window object
             *   for it
             */
            if (z->path != 0 && IsWindowVisible(z->hwnd))
                z_list_[dst++] = *z;
        }

        /* note the new (possibly smaller) list size */
        z_list_cnt_ = dst;
    }

    /* save the z-order list to the configuration */
    void save_z_list()
    {
        win_map *z;
        int i;

        /* write the list of windows in Z order */
        for (i = 0, z = z_list_ ; i < z_list_cnt_ ; ++i, ++z)
        {
            /* write out the path and title */
            config_->setval("mdi win list", 0, i, z->path);
            config_->setval("mdi win title", 0, i, z->title);
        }
    }

    CHtmlDebugConfig *config_;
    win_map *z_list_;
    int z_list_cnt_;
};

CHtmlSys_dbgmain::~CHtmlSys_dbgmain()
{
    twb_extension *cur, *nxt;

    /* close out the local configuration object, if we have one */
    if (local_config_ != 0)
    {
        /* save the local configuration */
        if (local_config_filename_.get() != 0)
            vsn_save_config_file(local_config_, local_config_filename_.get());

        /* done with the configuration object */
        delete local_config_;
        local_config_ = 0;
    }

    /* close out the global configuration as well */
    if (global_config_ != 0)
    {
        /* save the global configuration */
        if (global_config_filename_.get() != 0)
            vsn_save_config_file(global_config_,
                                 global_config_filename_.get());

        /* done with the global configuration */
        delete global_config_;
        global_config_ = 0;
    }

    /* delete our toolbar menubar and menu icon handler */
    if (menubar_ != 0)
        delete menubar_;
    if (iconmenu_ != 0)
        delete iconmenu_;

    /* release my helper reference */
    helper_->remove_ref();

    /* delete the "Loading" dialog */
    delete loading_dlg_;
    loading_dlg_ = 0;

    /* delete the "Find" dialogs */
    delete find_dlg_;
    delete projsearch_dlg_;
    find_dlg_ = 0;
    projsearch_dlg_ = 0;

    /* delete my MDI docking port */
    delete mdi_dock_port_;

    /* delete the build command list */
    clear_build_cmds();

    /*
     *   Delete our extension trackers.  Note that we don't explicitly unload
     *   the DLLs here (via FreeLibrary()), because the sequence of events
     *   during termination can be such that we're reached here via a call
     *   from a destructor in one of these DLLs - a COM object in an
     *   extension DLL could have the last reference to 'this' and could thus
     *   cause this destructor to be called.  Windows will happily unload all
     *   of our DLLs when the process terminates, so there's really no need
     *   to do so explicitly.
     */
    for (cur = extensions_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next extension */
        nxt = cur->nxt;

        /* we're done with this tracker */
        delete cur;
    }

    /* done with the command-mapping hash tables */
    delete command_hash;
    delete cmdname_hash;
    command_hash = 0;
    cmdname_hash = 0;

    /* delete our extension commands */
    while (ext_commands_ != 0)
    {
        HtmlDbgCommandInfoExt *nxt = ext_commands_->nxt_;
        delete ext_commands_;
        ext_commands_ = nxt;
    }

    /* done with our custom accelerators */
    for (HtmlKeyTable *t = keytabs_ ; t->name != 0 ; ++t)
    {
        /* if this table has an accelerator, delete it */
        if (t->accel != 0)
        {
            delete t->accel;
            t->accel = 0;
        }
    }
    taccel_ = 0;

    /* discard the bookmark trackers */
    delete_bookmarks();

    /* do static termination in our Scintilla interface */
    ScintillaWin::static_term();

    /* done with the build status mutex */
    CloseHandle(stat_mutex_);

    /* done with our game-to-front timer */
    free_timer_id(game_to_front_timer_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a bookmark
 */
void CHtmlSys_dbgmain::add_bookmark(const textchar_t *fname, int handle,
                                    int linenum, int name)
{
    tdb_bookmark *b;
    tdb_bookmark_file *f;

    /* if there's a name, delete any existing bookmark with the same name */
    if (name != 0)
    {
        tdb_bookmark *cur, *nxt;

        /* scan for an existing bookmark with the same name */
        for (cur = bookmarks_ ; cur != 0 ; cur = nxt)
        {
            /* remember the next in line */
            nxt = cur->nxt;

            /* if it has the same name, delete it */
            if (cur->name == name)
            {
                /* delete this bookmark */
                delete_bookmark(cur);
            }
        }
    }

    /* find or add a file tracker for this window */
    f = tdb_bookmark_file::find(this, fname, TRUE);

    /* create a new bookmark object */
    b = new tdb_bookmark(f, handle, linenum, name);

    /* link it at the head of the bookmark list */
    b->nxt = bookmarks_;
    bookmarks_ = b;
}

/*
 *   Delete a bookmark.  This removes the bookmark tracker and also deletes
 *   it from its source window, if it's open.
 */
void CHtmlSys_dbgmain::delete_bookmark(tdb_bookmark *b)
{
    IDebugSysWin *win;
    CHtmlSys_dbgsrcwin *srcwin;
    tdb_bookmark *prv, *cur;

    /*
     *   if there's a window open for the bookmark's file, tell it to remove
     *   the bookmark marker from the editor window
     */
    win = (IDebugSysWin *)helper_->find_text_file(
        this, b->file->fname.get(), b->file->fname.get());
    if (win != 0 && (srcwin = win->idsw_as_srcwin()) != 0)
    {
        /* there's an editor window - delete the visual marker */
        srcwin->delete_bookmark(b);
    }

    /* find the previous tracker object */
    for (prv = 0, cur = bookmarks_ ; cur != 0 && cur != b ;
         prv = cur, cur = cur->nxt) ;

    /* if we found it in the list, unlink it */
    if (cur != 0)
    {
        /* unlink the tracker */
        if (prv != 0)
            prv->nxt = b->nxt;
        else
            bookmarks_ = b->nxt;
    }

    /* delete the tracker */
    delete b;
}

/*
 *   Delete bookmark trackers.  This doesn't remove the bookmarks from the
 *   saved configuration - it simply clears out our internal list, in
 *   preparation for terminating or loading a new project.
 */
void CHtmlSys_dbgmain::delete_bookmarks()
{
    /* delete all bookmark trackers */
    while (bookmarks_ != 0)
    {
        tdb_bookmark *nxt = bookmarks_->nxt;
        delete bookmarks_;
        bookmarks_ = nxt;
    }
}

/*
 *   jump to the first bookmark in the next file after the given file
 */
void CHtmlSys_dbgmain::jump_to_next_bookmark(const textchar_t *fname, int dir)
{
    tdb_bookmark_file *f;
    IDebugSysWin *win;
    CHtmlSys_dbgsrcwin *srcwin;

    /* if there are no bookmarks, there's nothing to do */
    if (bookmarks_ == 0)
        return;

    /*
     *   Find the next file after the starting file in the master bookmark
     *   list.  If there's no starting file, start at the first file in the
     *   list.
     */
    f = 0;
    if (fname != 0)
    {
        /* find this file */
        f = tdb_bookmark_file::find(this, fname, FALSE);

        /* if we found the file, go to the next in the list */
        if (f != 0)
        {
            if (dir > 0)
            {
                /* go to the next in the list */
                f = f->nxt;
            }
            else
            {
                tdb_bookmark_file *prv, *cur;

                /* find the previous entry in the list */
                for (prv = 0, cur = bookmark_files_ ; cur != 0 && cur != f ;
                     prv = cur, cur = cur->nxt) ;

                /* use the previous item, if we found one */
                f = prv;
            }
        }
    }

    /* if we didn't find a file, or it was the last, cycle to the first */
    if (f == 0)
    {
        /* cycle around to the first or last file */
        if (dir > 0)
        {
            /* going forward - cycle back to the start of the list */
            f = bookmark_files_;
        }
        else
        {
            /* going backward - cycle to the end of the list */
            for (f = bookmark_files_ ; f != 0 && f->nxt != 0 ; f = f->nxt) ;
        }
    }

    /* find or open the window for this file */
    win = (IDebugSysWin *)helper_->open_text_file(this, f->fname.get());

    /* if we got a source window, go to its first bookmark */
    if (win != 0 && (srcwin = win->idsw_as_srcwin()) != 0)
    {
        /* bring it to the front */
        win->idsw_bring_to_front();

        /* go to the first or last bookmark in the file */
        if (dir > 0)
            srcwin->get_scintilla()->jump_to_first_bookmark();
        else
            srcwin->get_scintilla()->jump_to_last_bookmark();
    }
}

/*
 *   "Pop" a bookmark
 */
void CHtmlSys_dbgmain::pop_bookmark()
{
    tdb_bookmark *b, *cur;

    /* if we have any bookmarks, pop the first one */
    if ((b = bookmarks_) != 0)
    {
        /* jump to the first bookmark in the list - it's the most recent */
        jump_to_bookmark(b);

        /* unlink it from the list */
        bookmarks_ = b->nxt;
        b->nxt = 0;

        /* find the last bookmark in the list */
        for (cur = bookmarks_ ; cur != 0 && cur->nxt != 0 ; cur = cur->nxt) ;

        /*
         *   re-link our bookmark at the end of the list - this effectively
         *   rotates the list so that the most recent becomes the least
         *   recent, so that repeated popping will eventually cycle back to
         *   this bookmark
         */
        if (cur != 0)
            cur->nxt = b;
        else
            bookmarks_ = b;
    }
}

/*
 *   Prompt for a bookmark name, then jump to that bookmark
 */
void CHtmlSys_dbgmain::jump_to_named_bookmark()
{
    int name;
    tdb_bookmark *b;

    /* ask for the bookmark name; if we don't get one, cancel the command */
    if ((name = ask_bookmark_name()) == 0)
        return;

    /* find the bookmark by name */
    for (b = bookmarks_ ; b != 0 ; b = b->nxt)
    {
        /* if it's the one we're looking for, jump to it */
        if (b->name == name)
        {
            /* jump to it */
            jump_to_bookmark(b);

            /* we're done looking */
            break;
        }
    }
}

/*
 *   Jump to the given bookmark
 */
void CHtmlSys_dbgmain::jump_to_bookmark(tdb_bookmark *b)
{
    IDebugSysWin *win;
    CHtmlSys_dbgsrcwin *srcwin;

    /* find or open the window for the file */
    win = (IDebugSysWin *)helper_->open_text_file(this, b->file->fname.get());

    /* if we got a source window, go to this bookmark by handle */
    if (win != 0 && (srcwin = win->idsw_as_srcwin()) != 0)
    {
        /* bring it to the front */
        win->idsw_bring_to_front();

        /* go to the bookmark by handle */
        srcwin->get_scintilla()->jump_to_bookmark(b->handle);
    }
}

/*
 *   Ask for a bookmark name.  A bookmark name is simply a single letter.
 */
int CHtmlSys_dbgmain::ask_bookmark_name()
{
    /* assume they'll cancel before entering a name */
    int name = 0;

    /* set up the status message */
    show_statusline(TRUE);
    set_lrp_status("Bookmark name (A-Z):");

    /* enter a nested event loop to read the quoted key */
    for (int done = FALSE ; !done ; )
    {
        MSG msg;
        int upd = FALSE;
        int handled = FALSE;

        switch (GetMessage(&msg, 0, 0, 0))
        {
        case -1:
            /* failed - abort */
            done = TRUE;
            break;

        case 0:
            /* quitting - repost the WM_QUIT and exit the loop */
            PostQuitMessage(msg.wParam);
            done = TRUE;
            break;

        default:
            /* check the message */
            switch (msg.message)
            {
            case WM_CHAR:
                /* if it's a letter, it's the bookmark name */
                if (toupper(msg.wParam) >= 'A' && toupper(msg.wParam) <= 'Z')
                {
                    /* remember the name */
                    name = toupper(msg.wParam);

                    /* we've handled the key - do not dispatch it further */
                    handled = TRUE;
                    done = TRUE;
                }
                else if (msg.wParam == 27)
                {
                    /* escape - cancel */
                    handled = TRUE;
                    done = TRUE;
                }
                else
                {
                    /* ignore other characters */
                    handled = TRUE;
                }
                break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                /* translate accelerators as normal */
                if (CTadsApp::get_app()->accel_translate(&msg))
                {
                    /* any command cancels the prompt */
                    handled = TRUE;
                    done = TRUE;
                }
                break;

            case WM_COMMAND:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
                /* on any command or mouse click, cancel the prompt */
                done = TRUE;
                break;
            }
            break;
        }

        /* if we didn't handle it here, dispatch it normally */
        if (!handled)
            CTadsApp::get_app()->process_message(&msg);
    }

    /* clear our status message */
    set_lrp_status(0);

    /* return the name we read, if any */
    return name;
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear configuration information that isn't suitable for a default
 *   configuration.
 */
void CHtmlSys_dbgmain::clear_non_default_config()
{
    /* forget the source window list for the default config */
    local_config_->clear_strlist("mdi win list", 0);
    local_config_->clear_strlist("mdi win title", 0);
    local_config_->clear_strlist("srcfiles", "fname");
    local_config_->clear_strlist("srcfiles", "path");

    /* forget the source directory list as well */
    local_config_->clear_strlist("srcdir", 0);

    /* forget all breakpoints */
    helper_->clear_bp_config(local_config_);

    /* clear the internal line source list */
    helper_->clear_internal_source_config(local_config_);

    /* clear the internal line status list */
    helper_->delete_line_status(TRUE);

    /* reset the auto-script count */
    local_config_->setval("auto-script", "next-id", 1);

    /* clear all build config info */
    clear_dbg_build_info(local_config_);

    /* delete bookmarks */
    delete_bookmarks();
    local_config_->clear_strlist("editor", "bookmark");

    /* clear version-specific info */
    vsn_clear_non_default_config();
}

/* ------------------------------------------------------------------------ */
/*
 *   Switch between Design and Debug modes
 */
void CHtmlSys_dbgmain::set_game_loaded(int flag)
{
    /* if the state is changing, hide/show tools windows per preferences */
    if ((flag != 0) != (game_loaded_ != 0))
    {
        /* remember the new game-loaded state */
        game_loaded_ = flag;

        /* adjust tool windows for the mode switch */
        tool_window_mode_switch(flag);

        /*
         *   If the game is now loaded, post a "game starting" message to
         *   myself.  When we process this message, it'll bring either the
         *   game window or the debugger window to the foreground, depending
         *   on who has UI focus at the time.  Do this as a posted message,
         *   since the tool window mode switch could have queued up some
         *   posted messages that would re-activate the debugger window if we
         *   switched away from it now.
         */
        if (flag)
            PostMessage(get_handle(), HTMLM_GAME_STARTING, 0, 0);
    }

    /* profiling necessarily ends when the game ends */
    if (!flag)
        profiling_ = FALSE;
}

/*
 *   Show/hide tool windows per preferences for a Debug/Design mode switch
 */
void CHtmlSys_dbgmain::tool_window_mode_switch(int debug)
{
    static const struct
    {
        int win_type;
        const char *key_prefix;
        char design_default;
        char debug_default;
    }
    *m, map[] =
    {
        { HTMLDBG_WINTYPE_STACK, "stack", 'h', 's' },
        { HTMLDBG_SYSWINTYPE_EXPR, "expr", 'h', 's' },
        { HTMLDBG_SYSWINTYPE_LOCALS, "locals", 'h', 's' },
        { HTMLDBG_SYSWINTYPE_PROJECT, "project", 'n', 'n' },
        { HTMLDBG_SYSWINTYPE_SCRIPT, "scripts", 'n', 'n' },
        { HTMLDBG_WINTYPE_DEBUGLOG, "log", 'h', 'h' },
        { HTMLDBG_WINTYPE_DOCSEARCH, "search", 'n', 'n' },
        { HTMLDBG_WINTYPE_FILESEARCH, "search", 'n', 'n' },
        { HTMLDBG_WINTYPE_HISTORY, "trace", 'h', 'n' },
        { HTMLDBG_WINTYPE_HELP, "help", 'n', 'n' },
        { 0, 0, 0, 0 }
    };
    char key_suffix[64];

    /* choose the key suffix, according to the new mode */
    strcpy(key_suffix, debug ? ".debug" : ".design");

    /* run through the list and show or hide each window, as specified */
    for (m = map ; m->key_prefix != 0 ; ++m)
    {
        char key[128];
        int val;
        HWND frame;

        /* generate the full preferences key name */
        strcpy(key, m->key_prefix);
        strcat(key, key_suffix);

        /* look up the stored value, or use the default if not found */
        if (global_config_->getval("tool window mode switch", key, &val))
            val = (debug ? m->debug_default : m->design_default);

        /* if we're leaving it unchanged, move on to the next window */
        if (val == 'n')
            continue;

        /* get the frame window; if showing, create it if not found */
        frame = find_tool_window(m->win_type, val == 's');

        /* show or hide the window, depending on the mode */
        if (frame != 0)
        {
            /*
             *   we actually want to operate on the parent of the frame,
             *   since tool windows are always embedded in MDI or docking
             *   parent windows
             */
            frame = GetParent(frame);

            /*
             *   show or hide the window if it's not already in the desired
             *   state
             */
            if (val == 'h' && IsWindowVisible(frame))
                SendMessage(frame, WM_CLOSE, 0, 0);
            else if (val == 's' && !IsWindowVisible(frame))
                ShowWindow(frame, SW_SHOWNOACTIVATE);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Find a tool window by type
 */
HWND CHtmlSys_dbgmain::find_tool_window(int win_type, int create)
{
    IDebugSysWin *win;

    /* check for our extended types */
    switch (win_type)
    {
    case HTMLDBG_SYSWINTYPE_EXPR:
        /* return the expression-watch window */
        return (watchwin_ != 0 ? watchwin_->get_handle() : 0);

    case HTMLDBG_SYSWINTYPE_LOCALS:
        /* return the local variables window (this always exists) */
        return (localwin_ != 0 ? localwin_->get_handle() : 0);

    case HTMLDBG_SYSWINTYPE_PROJECT:
        /* return the project window (this always exists) */
        return (projwin_ != 0 ? projwin_->get_handle() : 0);

    case HTMLDBG_SYSWINTYPE_SCRIPT:
        return (scriptwin_ != 0 ? scriptwin_->get_handle() : 0);

    default:
        /*
         *   it's not one of our system-specific types, so ask the debug
         *   helper to find it
         */
        win = (IDebugSysWin *)helper_->find_window_by_type(
            (HtmlDbg_wintype_t)win_type);

        /* if we didn't find it, and they want to create it, create it */
        if (win == 0 && create)
            win = (IDebugSysWin *)helper_->create_window_by_type(
                this, (HtmlDbg_wintype_t)win_type);

        /* return the window */
        return (win != 0 ? win->idsw_get_frame_handle() : 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   window destruction
 */
void CHtmlSys_dbgmain::do_destroy()
{
    twb_extension *cur;

    /* done with our menu */
    DestroyMenu(main_menu_);

    /* release extension interfaces */
    for (cur = extensions_ ; cur != 0 ; cur = cur->nxt)
        cur->release_interfaces();

    /* forcibly clean up any remaining temp pages */
    clean_up_temp_pages(TRUE);

    /* remove the toolbar timer */
    if (toolbar_ != 0)
        rem_toolbar_proc(toolbar_);
    if (searchbar_ != 0)
        rem_toolbar_proc(searchbar_);

    /* forget the MDI tab - it'll self-destruct as part of window deletion */
    mdi_tab_ = 0;

    /* remove our statusline */
    if (statusline_ != 0)
    {
        /* unregister as a status source */
        statusline_->main_part()->unregister_source(this);

        /* unplug it from the accelerators */
        for (HtmlKeyTable *t = keytabs_ ; t->name != 0 ; ++t)
        {
            if (t->accel != 0)
                t->accel->set_statusline(0);
        }

        /* delete the statusline object */
        delete statusline_;
    }

    /* inherit default handling */
    CHtmlSys_framewin::do_destroy();
}

/*
 *   callback for enumerating open source windows to save modified files
 */
void CHtmlSys_dbgmain::enum_win_savefile_cb(void *ctx, int /*idx*/,
                                            IDebugWin *win,
                                            int /*line_source_id*/,
                                            HtmlDbg_wintype_t win_type)
{
    CHtmlSys_dbgmain *dbgmain = (CHtmlSys_dbgmain *)ctx;
    CHtmlSys_dbgsrcwin *srcwin;

    /* only save source files */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* it's a source window - cast it */
    srcwin = (CHtmlSys_dbgsrcwin *)win;

    /* if it's been modified, save it */
    if (srcwin->is_dirty())
        srcwin->save_file();
}

/*
 *   callback for enumerating open source windows and saving them
 */
void CHtmlSys_dbgmain::enum_win_savelayout_cb(void *ctx0, int /*idx*/,
                                              IDebugWin *win,
                                              int /*line_source_id*/,
                                              HtmlDbg_wintype_t win_type)
{
    enum_win_save_ctx_t *ctx = (enum_win_save_ctx_t *)ctx0;

    /* add the window to the list */
    ctx->add_window((IDebugSysWin *)win);
}


/*
 *   our private toolbar creation structure
 */
struct tb_create_info
{
    /* set up the toolbar information */
    void init(HWND hwnd, int id, const char *title, int extra_wid,
              int dflt_idx, int dflt_vis, int dflt_newline)
    {
        this->hwnd = hwnd;
        this->id = id;
        this->title = title;
        this->extra_wid = extra_wid;

        this->rbstate.idx = dflt_idx;
        this->rbstate.new_line = dflt_newline;
        this->rbstate.vis = dflt_vis;
        this->rbstate.width = -1;
    }

    /* the toolbar window */
    HWND hwnd;

    /* the toolbar ID */
    int id;

    /* the toolbar title */
    const char *title;

    /* extra fixed width needed beyond what TB_GETMAXSIZE reports */
    int extra_wid;

    /* saved rebar state: index, line break flag, visibility flag */
    struct
    {
        int idx;
        int new_line;
        int vis;
        int width;
    } rbstate;
};

/* qsort callback - sort by rebar index */
static int sort_by_rebar_index(const void *a0, const void *b0)
{
    tb_create_info *a = (tb_create_info *)a0;
    tb_create_info *b = (tb_create_info *)b0;

    /* sort by saved index; for the same index, sort by toolbar ID */
    if (a->rbstate.idx != b->rbstate.idx)
        return a->rbstate.idx - b->rbstate.idx;
    else
        return a->id - b->id;
}

/*
 *   Add the toolbars to the rebar.  During program startup, we use this to
 *   set up the rebars and restore their saved states.
 */
void CHtmlSys_dbgmain::add_rebar_toolbars(tb_create_info *info, int cnt)
{
    int i;
    tb_create_info *ip;

    /* read each toolbar's configuration data */
    for (i = cnt, ip = info ; i != 0 ; ++ip, --i)
    {
        /* get the config ID string for this toolbar */
        const char *id = tb_config_id[ip->id];

        /* look up the saved state for this toolbar */
        global_config_->getval(id, "rebarIndex", &ip->rbstate.idx);
        global_config_->getval(id, "rebarNewLine", &ip->rbstate.new_line);
        global_config_->getval(id, "rebarVis", &ip->rbstate.vis);
        global_config_->getval(id, "rebarWidth", &ip->rbstate.width);
    }

    /* sort by rebar index */
    qsort(info, cnt, sizeof(*info), sort_by_rebar_index);

    /* insert the toolbars */
    for (i = cnt, ip = info ; i != 0 ; ++ip, --i)
    {
        SIZE sz;
        REBARBANDINFO rbbi;

        /* get the width and height of the toolbar */
        SendMessage(ip->hwnd, TB_GETMAXSIZE, 0, (LPARAM)&sz);

        /* initialize the rebar band for the toolbar */
        rbbi.cbSize = rebarbandinfo_size_;
        rbbi.fMask = RBBIM_CHILD | RBBIM_TEXT | RBBIM_STYLE | RBBIM_ID
                     | RBBIM_SIZE | RBBIM_CHILDSIZE | RBBIM_IDEALSIZE;
        rbbi.fStyle = RBBS_CHILDEDGE | RBBS_VARIABLEHEIGHT
                      | RBBS_GRIPPERALWAYS;
        rbbi.lpText = (char *)ip->title;
        rbbi.wID = ip->id;
        rbbi.hwndChild = ip->hwnd;
        rbbi.cx = ip->rbstate.width;
        rbbi.cxMinChild = rbbi.cxIdeal = sz.cx + ip->extra_wid;
        rbbi.cyChild = rbbi.cyMinChild = rbbi.cyMaxChild = sz.cy;
        rbbi.cyIntegral = 1;

        /* add the line-break flag if applicable */
        if (ip->rbstate.new_line)
            rbbi.fStyle |= RBBS_BREAK;

        /* add the hidden flag if applicable */
        if (!ip->rbstate.vis)
            rbbi.fStyle |= RBBS_HIDDEN;

        /* add the band */
        SendMessage(rebar_, RB_INSERTBAND, ip->rbstate.idx, (LPARAM)&rbbi);

        /*
         *   if this one didn't have a saved index, note the actual index: it
         *   will have been inserted last, so its actual index is simply the
         *   last index, which is the number of items minus 1
         */
        if (ip->rbstate.idx == -1)
            ip->rbstate.idx = SendMessage(rebar_, RB_GETBANDCOUNT, 0, 0) - 1;

        /* set up the toolbar to do idle status updating */
        add_toolbar_proc(ip->hwnd);

        /* show this toolbar */
        ShowWindow(ip->hwnd, SW_SHOW);
    }

    /* go back and set each toolbar to its saved width */
    for (i = cnt, ip = info ; i != 0 ; ++ip, --i)
    {
        /*
         *   if this is the last item, or the last item on the line, don't
         *   bother setting its width, as the last item just takes up
         *   whatever's left over
         */
        if (i == 0 || (ip+1)->rbstate.new_line)
            continue;

        /* if its has a saved width, apply it; otherwise, minimize it */
        if (ip->rbstate.width != -1)
        {
            REBARBANDINFO rbbi;

            rbbi.cbSize = rebarbandinfo_size_;
            rbbi.fMask = RBBIM_SIZE;
            rbbi.cx = ip->rbstate.width;
            SendMessage(rebar_, RB_SETBANDINFO,
                        ip->rbstate.idx, (LPARAM)&rbbi);
        }
        else
        {
            /* for a band that hasn't been saved, just minimize it */
            SendMessage(rebar_, RB_MINIMIZEBAND, ip->rbstate.idx, 0);
        }
    }
}

/*
 *   "Subclassed" window procedure for the searchbar edit field.  We subclass
 *   this control so that we can handle the Enter key specially.
 */
LRESULT CALLBACK CHtmlSys_dbgmain::searchbox_winproc(
    HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        /*
         *   if it's the Enter key, process it as a click on the search
         *   button in the current mode
         */
        if (wpar == VK_RETURN)
        {
            CHtmlSys_dbgmain *win = CHtmlSys_dbgmain::get_main_win();

            /* send the window the command for the current search mode */
            PostMessage(win->get_handle(), WM_COMMAND,
                        search_mode_table[win->get_searchbar_mode()].cmd, 0);

            /* we've fully handled this key */
            return 0;
        }
        break;

    case WM_KEYUP:
    case WM_CHAR:
        /*
         *   if it's the Enter key, ignore it, as we've already handled this
         *   key in our WM_KEYDOWN handler
         */
        if (wpar == VK_RETURN)
            return 0;
        break;

    case WM_SETFOCUS:
        /*
         *   on gaining focus, set the null active debug window - we don't
         *   want commands routed to the old active window while we have
         *   focus, and in particular we don't want a non-global accelerator
         *   table to be in effect, since the text editor accelerator table
         *   is likely to contain command mappings for the basic cursor keys
         */
        CHtmlSys_dbgmain::get_main_win()->set_null_active_dbg_win();
        break;
    }

    /* we didn't handle this message, so inherit the default handling */
    return CallWindowProc(searchbox_orig_winproc_, hwnd, msg, wpar, lpar);
}

/*
 *   Get the current searchbar mode
 */
int CHtmlSys_dbgmain::get_searchbar_mode()
{
    return search_mode_from_cmd(searchbarcmd_);
}

/*
 *   Set the search toolbar to use the given mode.  The mode is an index into
 *   the search_mode_table[] array.
 */
void CHtmlSys_dbgmain::set_searchbar_mode(int idx)
{
    TBBUTTONINFO tbbi;

    /* if the index is out of range, force to the default */
    if (idx < 1 ||
        idx >= sizeof(search_mode_table)/sizeof(search_mode_table[0]))
        idx = search_mode_from_cmd(ID_SEARCH_DOC);

    /* set the new command */
    searchbarcmd_ = search_mode_table[idx].cmd;

    /* set the new image */
    tbbi.cbSize = sizeof(tbbi);
    tbbi.dwMask = TBIF_IMAGE;
    tbbi.iImage = search_mode_table[idx].image;
    SendMessage(searchbar_, TB_SETBUTTONINFO, ID_SEARCH_GO, (LPARAM)&tbbi);

    /* make sure we redraw the window with the new button */
    InvalidateRect(searchbar_, 0, TRUE);
}

/*
 *   Load a saved toolbar configuration
 */
void CHtmlSys_dbgmain::load_toolbar_config(CHtmlDebugConfig *config)
{
    int i, cnt;
    REBARBANDINFO info;
    const char *id;

    /* if there's no rebar, skip this */
    if (rebar_ == 0)
        return;

    /* get the number of bands */
    cnt = SendMessage(rebar_, RB_GETBANDCOUNT, 0, 0);

    /* first, get the bands into the saved order */
    for (i = cnt ; i != 0 ; )
    {
        int idx;

        /* move to this index */
        --i;

        /* get the band's ID */
        info.cbSize = rebarbandinfo_size_;
        info.fMask = RBBIM_ID;
        SendMessage(rebar_, RB_GETBANDINFO, i, (LPARAM)&info);

        /* if the toolbar ID isn't in our config name list, skip it */
        if (info.wID < 1
            || info.wID > sizeof(tb_config_id)/sizeof(tb_config_id[0]))
            continue;

        /* get the config ID name */
        id = tb_config_id[info.wID];

        /* get the saved index - skip it if we don't have a saved index */
        if (config->getval(id, "rebarIndex", &idx))
            continue;

        /* if the index has changed, reinsert the band at its new index */
        if (idx != i)
        {
            /* move it to the new index */
            SendMessage(rebar_, RB_MOVEBAND, i, idx);

            /* rescan this index, since we always insert earlier */
            ++i;
        }
    }

    /* now restore each band's appearance */
    for (i = 0 ; i < cnt ; ++i)
    {
        int vis, nl, wid;

        /* get the current style and ID */
        info.cbSize = rebarbandinfo_size_;
        info.fMask = RBBIM_ID | RBBIM_STYLE | RBBIM_SIZE;
        SendMessage(rebar_, RB_GETBANDINFO, i, (LPARAM)&info);

        /* if the toolbar ID isn't in our config name list, skip it */
        if (info.wID < 1
            || info.wID > sizeof(tb_config_id)/sizeof(tb_config_id[0]))
            continue;

        /* get the config ID name */
        id = tb_config_id[info.wID];

        /* get the config settings - skip it if we can't find any */
        if (config->getval(id, "rebarNewLine", &nl)
            || config->getval(id, "rebarVis", &vis)
            || config->getval(id, "rebarWidth", &wid))
            continue;

        /* restore the saved settings */
        info.fMask = RBBIM_STYLE | RBBIM_SIZE;
        info.cx = wid;

        if (vis)
            info.fStyle &= ~RBBS_HIDDEN;
        else
            info.fStyle |= RBBS_HIDDEN;

        if (nl)
            info.fStyle |= RBBS_BREAK;
        else
            info.fStyle &= ~RBBS_BREAK;

        /* set the new settings */
        SendMessage(rebar_, RB_SETBANDINFO, i, (LPARAM)&info);
    }

    /* read the search mode command; use Doc Search by default */
    if (config->getval("searchMode", 0, &i))
        i = 0;

    /* set the command and image for this mode */
    set_searchbar_mode(i);
}

/*
 *   Determine if the given toolbar is visible
 */
int CHtmlSys_dbgmain::is_toolbar_visible(int id)
{
    int idx;
    REBARBANDINFO info;

    /* find the band by ID; assume it's not visible if we can't find it */
    if ((idx = SendMessage(rebar_, RB_IDTOINDEX, id, 0)) == -1)
        return FALSE;

    /* get the band style settings */
    info.cbSize = rebarbandinfo_size_;
    info.fMask = RBBIM_STYLE;
    SendMessage(rebar_, RB_GETBANDINFO, idx, (LPARAM)&info);

    /* it's visible if the 'hidden' style isn't set */
    return ((info.fStyle & RBBS_HIDDEN) == 0);
}

/*
 *   Hide or show the given toolbar
 */
void CHtmlSys_dbgmain::set_toolbar_visible(int id, int vis)
{
    int idx;
    REBARBANDINFO info;

    /* find the band by ID; give up if we can't find it */
    if ((idx = SendMessage(rebar_, RB_IDTOINDEX, id, 0)) == -1)
        return;

    /* get the current style flags */
    info.cbSize = rebarbandinfo_size_;
    info.fMask = RBBIM_STYLE;
    SendMessage(rebar_, RB_GETBANDINFO, idx, (LPARAM)&info);

    /* set the new visibility status */
    if (vis)
        info.fStyle &= ~RBBS_HIDDEN;
    else
        info.fStyle |= RBBS_HIDDEN;

    /* set the new style */
    SendMessage(rebar_, RB_SETBANDINFO, idx, (LPARAM)&info);
}

/*
 *   mode menu item - this encapsulates the name and command ID for a mode in
 *   the language-mode menu
 */
struct mode_menu_info
{
    ~mode_menu_info() { th_free(name); }
    textchar_t *name;
    int cmd;
};

/* compare two mode_menu_info items by mode name */
static int sort_by_mode_name(const void *a0, const void *b0)
{
    mode_menu_info *a = (mode_menu_info *)a0;
    mode_menu_info *b = (mode_menu_info *)b0;

    /* compare by name */
    return stricmp(a->name, b->name);
}

/*
 *   Status line part definition for the text editor line/column display
 */
class CTadsStatusPartLineCol: public CTadsStatusPart
{
public:
    virtual int calc_width() const
    {
        /* measure the space needed for a sample display string */
        HDC dc = GetDC(stat_->get_handle());
        HGDIOBJ oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SIZE txtsiz;
        ht_GetTextExtentPoint32(dc, " Ln 123456  Col 12345 ", 23, &txtsiz);

        /* restore the drawing context */
        SelectObject(dc, oldfont);
        ReleaseDC(stat_->get_handle(), dc);

        /* return the calculated width */
        return txtsiz.cx;
    }
};

/*
 *   initialize the progress bar window
 */
void CTadsStatusSourceProgress::init_win(HWND parent)
{
    /* create the progress bar object */
    pbar_ = CreateWindowEx(
        0, PROGRESS_CLASS, 0, WS_CHILD, 0, 0, 1, 1, parent, 0,
        CTadsApp::get_app()->get_instance(), 0);
}

/*
 *   position the progress bar
 */
void CTadsStatusSourceProgress::status_change_pos(const RECT *rc)
{
    /* hide the progress bar window if the panel is zero-width */
    ShowWindow(pbar_, rc->right - rc->left > 1 ? SW_SHOW : SW_HIDE);

    /* move the window to fill the panel, with a 1-pixel margin */
    MoveWindow(pbar_, rc->left + 1, rc->top + 1,
               rc->right - rc->left - 2, rc->bottom - rc->top - 2, TRUE);
}

/*
 *   Status line part for the build progress bar
 */
class CTadsStatusPartProgress: public CTadsStatusPart
{
public:
    CTadsStatusPartProgress()
    {
        psrc_ = 0;
    }

    virtual int calc_width() const
    {
        /* if no progress-bar-worthy process is running, disappear */
        if (psrc_ == 0 || !psrc_->running_)
            return 0;

        /* size in terms of the scrollbar width */
        return 10*GetSystemMetrics(SM_CXVSCROLL);
    }

    /* set the status source */
    void set_source(CTadsStatusSourceProgress *src)
    {
        psrc_ = src;
        register_source(src);
    }

    /*
     *   Our progress status source - we keep the special subclass item
     *   (rather than just using the fact that it's already stored in our
     *   generic source list in the base class) because it contains the
     *   statistics we use to figure whether we're visible or not.
     */
    CTadsStatusSourceProgress *psrc_;
};


/* ------------------------------------------------------------------------ */
/*
 *   IDropTarget implementation
 */

HRESULT STDMETHODCALLTYPE CHtmlSys_dbgmain::DragEnter(
    IDataObject __RPC_FAR *dataObj,
    DWORD grfKeyState, POINTL ptl, DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Enter(dataObj, grfKeyState, ptl, pdwEffect);

    /* assume we won't allow it */
    *pdwEffect = DROPEFFECT_NONE;
    drop_file_ok_ = FALSE;

    /* check to see if it's a format we recognize */
    FORMATETC fmt;
    fmt.cfFormat = CF_HDROP;
    fmt.ptd = 0;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;
    if (dataObj->QueryGetData(&fmt) == S_OK)
    {
        /* it's a file drop from Windows Explorer - allow it */
        drop_file_ok_ = TRUE;
        *pdwEffect = DROPEFFECT_COPY;
    }

    /*
     *   whether or not we handle the drop, we successfully responded to the
     *   drag-drop request
     */
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CHtmlSys_dbgmain::DragOver(
    DWORD grfKeyState, POINTL ptl, DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Over(grfKeyState, ptl, pdwEffect);

    /* presume we won't show anything */
    *pdwEffect = DROPEFFECT_NONE;

    /* if it's a file drop from Windows Explorer, allow it */
    if (drop_file_ok_)
        *pdwEffect = DROPEFFECT_COPY;

    /* handled */
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CHtmlSys_dbgmain::DragLeave()
{
    /* let the helper object do its work, if any */
    DropHelper_Leave();

    /* handled */
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CHtmlSys_dbgmain::Drop(
    IDataObject __RPC_FAR *dataObj,
    DWORD grfKeyState, POINTL pt, DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Drop(dataObj, grfKeyState, pt, pdwEffect);

    /* assume we won't allow it */
    *pdwEffect = DROPEFFECT_NONE;

    /* get the HDROP describing the files being dropped */
    FORMATETC fmt;
    STGMEDIUM stg;
    fmt.cfFormat = CF_HDROP;
    fmt.ptd = 0;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;
    if (dataObj->GetData(&fmt, &stg) == S_OK)
    {
        /* it's a file drop from Windows Explorer - allow it */
        *pdwEffect = DROPEFFECT_COPY;

        /* get the HDROP from the storage medium */
        HDROP hdrop = (HDROP)stg.hGlobal;

        /* open each file in the text editor */
        int cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
        for (int i = 0 ; i < cnt ; ++i)
        {
            /* get this file's name */
            char fname[OSFNMAX];
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* process the drop */
            drop_shell_file(fname);
        }

        /* done with the data */
        ReleaseStgMedium(&stg);
    }

    /* handled */
    return S_OK;
}



/* ------------------------------------------------------------------------ */
/*
 *   window creation
 */
void CHtmlSys_dbgmain::do_create()
{
    /* inherit default */
    CHtmlSys_framewin::do_create();

    /* register as a drop target */
    drop_target_register();

    /* create the MDI tab control */
    mdi_tab_ = new MdiTabCtrl(get_parent_of_children());

    /* create its system window */
    RECT rc;
    GetClientRect(get_parent_of_children(), &rc);
    rc.bottom = rc.top + mdi_tab_->get_natural_height();
    mdi_tab_->create_system_window(this, handle_, TRUE, "", &rc,
                                   new CTadsSyswin(mdi_tab_));

    /* add the toolbar icons to the menu icon handler */
    int menu_icon_base = iconmenu_->add_bitmap(
        CTadsApp::get_app()->get_instance(), IDB_DEBUG_TOOLBAR);

    /* create the rebar */
    rebar_ = CreateWindowEx(0, REBARCLASSNAME, 0,
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS
                            | WS_CLIPCHILDREN | WS_BORDER
                            | CCS_NOPARENTALIGN | CCS_NODIVIDER
                            | RBS_BANDBORDERS | RBS_DBLCLKTOGGLE,
                            0, 0, 1, 1, handle_,
                            0, CTadsApp::get_app()->get_instance(), 0);

    /* initialize the rebar */
    REBARINFO rbi;
    rbi.cbSize = sizeof(rbi);
    rbi.fMask = 0;
    rbi.himl = 0;
    SendMessage(rebar_, RB_SETBARINFO, 0, (LPARAM)&rbi);

    /* load my menu */
    main_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                          MAKEINTRESOURCE(IDR_DEBUGMAIN_MENU));

    /* remember the Windows menu handle for our own purposes */
    win_menu_hdl_ = GetSubMenu(main_menu_, GetMenuItemCount(main_menu_) - 2);

    /* rebuild the Tools menu */
    rebuild_tools_menu();

    /*
     *   Menu bar
     */

    /* create the menu toolbar */
    menubar_ = new ToolbarMenubar(CTadsApp::get_app()->get_instance(),
                                  rebar_, handle_, main_menu_,
                                  ID_MENUBAR_FIRST);

    /* set up the creation descriptor for the main toolbar */
    tb_create_info tbci[10];
    int tbcnt = 0;
    tbci[tbcnt].init(menubar_->get_handle(), MENU_TOOLBAR_ID, "", 0,
                     tbcnt, TRUE, FALSE);
    ++tbcnt;

    /* find the "language mode" placeholder in the menu */
    HMENU langmenu = find_parent_menu(main_menu_, ID_EDIT_LANGMODE, 0);
    if (langmenu != 0)
    {
        twb_editor_mode_iter iter(this);
        ITadsWorkbenchEditorMode *mode;
        int i, cnt;
        mode_menu_info *modes;

        /* remove the placeholder */
        DeleteMenu(langmenu, ID_EDIT_LANGMODE, MF_BYCOMMAND);

        /* first, count the modes */
        for (cnt = 0, mode = iter.first() ; mode != 0 ;
             ++cnt, mode = iter.next()) ;

        /* now make an array of the names */
        modes = new mode_menu_info[cnt];
        for (i = 0, mode = iter.first() ; mode != 0 && i < cnt ;
             ++i, mode = iter.next())
        {
            modes[i].name = ansi_from_bstr(mode->GetModeName(), TRUE);
            modes[i].cmd = ID_EDIT_LANGMODE + i;
        }

        /* sort them by name */
        qsort(modes, cnt, sizeof(modes[0]), sort_by_mode_name);

        /* limit the menu to the preallocated range of command IDs */
        if (cnt > ID_EDIT_LANGLAST - ID_EDIT_LANGMODE + 1)
            cnt = ID_EDIT_LANGLAST - ID_EDIT_LANGMODE + 1;

        /* add the modes to the menu */
        for (i = 0 ; i < cnt ; ++i)
        {
            MENUITEMINFO mii;

            mii.cbSize = menuiteminfo_size_;
            mii.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID | MIIM_STATE;
            mii.fType = MFT_STRING | MFT_RADIOCHECK;
            mii.fState = MFS_ENABLED;
            mii.wID = modes[i].cmd;
            mii.dwTypeData = modes[i].name;
            InsertMenuItem(langmenu, i, TRUE, &mii);
        }

        /* free the mode names */
        delete [] modes;
    }

    /* set the line-wrapping menu items to radio checkboxes */
    set_menuitem_radiocheck(main_menu_, ID_EDIT_WRAPNONE);
    set_menuitem_radiocheck(main_menu_, ID_EDIT_WRAPCHAR);
    set_menuitem_radiocheck(main_menu_, ID_EDIT_WRAPWORD);

    /*
     *   Edit toolbar
     */

    /* create the toolbar */
    editbar_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NOPARENTALIGN
        | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, rebar_, 0, CTadsApp::get_app()->get_instance(), 0);

    /* do the required structure/DLL initialization */
    SendMessage(editbar_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    /* set the button and bitmap sizes */
    SendMessage(editbar_, TB_SETBITMAPSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));
    SendMessage(editbar_, TB_SETBUTTONSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));

    /* add the button bitmaps */
    TBADDBITMAP tba;
    tba.hInst = CTadsApp::get_app()->get_instance();
    tba.nID = IDB_DEBUG_TOOLBAR;
    SendMessage(editbar_, TB_ADDBITMAP, 70, (LPARAM)&tba);

    /* set up the buttons, starting at the first button */
    TBBUTTON buttons[30];
    int i = 0;

    /* create a project */
    buttons[i].iBitmap = 24;
    buttons[i].idCommand = ID_FILE_CREATEGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* open a project */
    buttons[i].iBitmap = 25;
    buttons[i].idCommand = ID_FILE_LOADGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* create a file */
    buttons[i].iBitmap = 36;
    buttons[i].idCommand = ID_FILE_NEW;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* open a file */
    buttons[i].iBitmap = 4;
    buttons[i].idCommand = ID_FILE_OPEN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* save */
    buttons[i].iBitmap = 34;
    buttons[i].idCommand = ID_FILE_SAVE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* save all */
    buttons[i].iBitmap = 35;
    buttons[i].idCommand = ID_FILE_SAVE_ALL;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* cut */
    buttons[i].iBitmap = 28;
    buttons[i].idCommand = ID_EDIT_CUT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* copy */
    buttons[i].iBitmap = 29;
    buttons[i].idCommand = ID_EDIT_COPY;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* paste */
    buttons[i].iBitmap = 30;
    buttons[i].idCommand = ID_EDIT_PASTE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* undo */
    buttons[i].iBitmap = 26;
    buttons[i].idCommand = ID_EDIT_UNDO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* redo */
    buttons[i].iBitmap = 27;
    buttons[i].idCommand = ID_EDIT_REDO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* toggle bookmark */
    buttons[i].iBitmap = 63;
    buttons[i].idCommand = ID_EDIT_TOGGLEBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* add named bookmark */
    buttons[i].iBitmap = 66;
    buttons[i].idCommand = ID_EDIT_SETNAMEDBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* pop bookmark */
    buttons[i].iBitmap = 65;
    buttons[i].idCommand = ID_EDIT_POPBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* jump to next bookmark */
    buttons[i].iBitmap = 64;
    buttons[i].idCommand = ID_EDIT_JUMPNEXTBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* jump to previous bookmark */
    buttons[i].iBitmap = 69;
    buttons[i].idCommand = ID_EDIT_JUMPPREVBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* jump to named bookmark */
    buttons[i].iBitmap = 67;
    buttons[i].idCommand = ID_EDIT_JUMPNAMEDBOOKMARK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* comment region */
    buttons[i].iBitmap = 68;
    buttons[i].idCommand = ID_EDIT_ADDCOMMENT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* add the buttons to the toolbar */
    SendMessage(editbar_, TB_ADDBUTTONS, (WPARAM)i, (LPARAM)buttons);

    /* apply the same icon/command associations to our menus */
    iconmenu_->map_commands(buttons, i, menu_icon_base);

    /* set up the descriptor for the search bar */
    tbci[tbcnt].init(editbar_, EDIT_TOOLBAR_ID, "", 0,
                     tbcnt, TRUE, TRUE);
    ++tbcnt;


    /*
     *   Search bar
     */

    /* create the search bar */
    searchbar_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NOPARENTALIGN
        | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, rebar_, 0, CTadsApp::get_app()->get_instance(), 0);

    /* do the required structure/DLL initialization */
    SendMessage(searchbar_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    /* set the extended styles */
    SendMessage(searchbar_, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);

    /* set the button and bitmap sizes */
    SendMessage(searchbar_, TB_SETBITMAPSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));
    SendMessage(searchbar_, TB_SETBUTTONSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));

    /* add the button bitmaps */
    SendMessage(searchbar_, TB_ADDBITMAP, 70, (LPARAM)&tba);

    /* set up the buttons, starting at the first button */
    i = 0;

    /*
     *   Calculate the default size for the combo box field, based on a
     *   template string.  We also add in a few extra pixels explicitly to
     *   make room for the combo box drop-down icon.
     */
    HDC dc = GetDC(GetDesktopWindow());
    HFONT oldfont = (HFONT)SelectObject(dc, CTadsApp::menu_font);
    SIZE cb_sz;
    GetTextExtentPoint32(dc, "XXXXXXXXXXXXXXXXXXXXXXXXX", 25, &cb_sz);
    cb_sz.cx += 16;
    SelectObject(dc, oldfont);
    ReleaseDC(GetDesktopWindow(), dc);

    /* separator button, to reserve space for the combo box */
    buttons[i].iBitmap = cb_sz.cx;     /* this is the width for a separator */
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* search documentation/files button */
    buttons[i].iBitmap = 20;
    buttons[i].idCommand = ID_SEARCH_GO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_DROPDOWN;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* add the buttons */
    SendMessage(searchbar_, TB_ADDBUTTONS, (WPARAM)i, (LPARAM)buttons);

    /* create the combo box */
    searchbox_ = CreateWindow(WC_COMBOBOX, "",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP
                              | CBS_AUTOHSCROLL | CBS_DROPDOWN | WS_VSCROLL,
                              0, 0, cb_sz.cx, cb_sz.cy * 20, searchbar_,
                              0, CTadsApp::get_app()->get_instance(), 0);

    /* get info on the combo */
    COMBOBOXINFO cbi;
    cbi.cbSize = sizeof(cbi);
    GetComboBoxInfo(searchbox_, &cbi);

    /* "subclass" the combo box's edit field */
    searchbox_orig_winproc_ = (WNDPROC)SetWindowLong(
        cbi.hwndItem, GWL_WNDPROC, (DWORD)&searchbox_winproc);

    /* use the system font in the combo */
    SendMessage(searchbox_, WM_SETFONT,
                (WPARAM)CTadsApp::menu_font, MAKELPARAM(TRUE, 0));

    /* load the saved history from the global configuration */
    load_searchbox_history(global_config_);

    /*
     *   some versions of windows don't seem to count the size of the toolbar
     *   properly when we use a custom separator; if the toolbar size as
     *   reported by TB_GETMAXSIZE is less than the combo box size,
     *   explicitly reserve the extra space for the combo box
     */
    int extra_wid = 0;
    SendMessage(searchbar_, TB_GETMAXSIZE, 0, (LPARAM)&cb_sz);
    GetWindowRect(searchbox_, &rc);
    if (cb_sz.cx <= rc.right - rc.left)
        extra_wid = rc.right - rc.left + 10;

    /* set up the descriptor for the search bar */
    tbci[tbcnt].init(searchbar_, SEARCH_TOOLBAR_ID, "Search", extra_wid,
                     tbcnt, TRUE, FALSE);
    ++tbcnt;


    /*
     *   Doc toolbar
     */

    /* create the toolbar */
    docbar_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NOPARENTALIGN
        | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, rebar_, 0, CTadsApp::get_app()->get_instance(), 0);

    /* do the required structure/DLL initialization */
    SendMessage(docbar_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    /* set the button and bitmap sizes */
    SendMessage(docbar_, TB_SETBITMAPSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));
    SendMessage(docbar_, TB_SETBUTTONSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));

    /* add the button bitmaps */
    SendMessage(docbar_, TB_ADDBITMAP, 70, (LPARAM)&tba);

    /* set up the buttons, starting at the first button */
    i = 0;

    /* 'workbench help' button */
    buttons[i].iBitmap = 23;
    buttons[i].idCommand = ID_HELP_TOPICS;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;


    /* add the buttons relevant to this version */
    i = vsn_add_doc_buttons(buttons, i);

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* 'back' button */
    buttons[i].iBitmap = 60;
    buttons[i].idCommand = ID_HELP_GOBACK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* 'forward' button */
    buttons[i].iBitmap = 61;
    buttons[i].idCommand = ID_HELP_GOFORWARD;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* refresh button */
    buttons[i].iBitmap = 62;
    buttons[i].idCommand = ID_HELP_REFRESH;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* add the buttons */
    SendMessage(docbar_, TB_ADDBUTTONS, (WPARAM)i, (LPARAM)buttons);

    /* apply the same icon/command associations to our menus */
    iconmenu_->map_commands(buttons, i, menu_icon_base);

    /* set up the descriptor for the search bar */
    tbci[tbcnt].init(docbar_, DOC_TOOLBAR_ID, "", 0, tbcnt, TRUE, FALSE);
    ++tbcnt;


    /*
     *   Debug toolbar
     */

    /* start at the first button */
    i = 0;

    /* GO button */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = ID_DEBUG_GO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* REPLAY button */
    buttons[i].iBitmap = 58;
    buttons[i].idCommand = ID_DEBUG_REPLAY_SESSION;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP INTO button */
    buttons[i].iBitmap = 1;
    buttons[i].idCommand = ID_DEBUG_STEPINTO;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP OVER button */
    buttons[i].iBitmap = 2;
    buttons[i].idCommand = ID_DEBUG_STEPOVER;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* STEP OUT button */
    buttons[i].iBitmap = 14;
    buttons[i].idCommand = ID_DEBUG_STEPOUT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* run to cursor */
    buttons[i].iBitmap = 5;
    buttons[i].idCommand = ID_DEBUG_RUNTOCURSOR;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set next statement */
    buttons[i].iBitmap = 15;
    buttons[i].idCommand = ID_DEBUG_SETNEXTSTATEMENT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* break execution */
    buttons[i].iBitmap = 11;
    buttons[i].idCommand = ID_DEBUG_BREAK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* terminate program */
    buttons[i].iBitmap = 18;
    buttons[i].idCommand = ID_FILE_TERMINATEGAME;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* set/clear breakpoint */
    buttons[i].iBitmap = 3;
    buttons[i].idCommand = ID_DEBUG_SETCLEARBREAKPOINT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* enable/disable breakpoint */
    buttons[i].iBitmap = 6;
    buttons[i].idCommand = ID_DEBUG_DISABLEBREAKPOINT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* evaluate expression */
    buttons[i].iBitmap = 12;
    buttons[i].idCommand = ID_DEBUG_EVALUATE;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show next statement */
    buttons[i].iBitmap = 13;
    buttons[i].idCommand = ID_DEBUG_SHOWNEXTSTATEMENT;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show game window */
    buttons[i].iBitmap = 7;
    buttons[i].idCommand = ID_VIEW_GAMEWIN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show locals window */
    buttons[i].iBitmap = 8;
    buttons[i].idCommand = ID_VIEW_LOCALS;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show watch window */
    buttons[i].iBitmap = 9;
    buttons[i].idCommand = ID_VIEW_WATCH;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* show stack window */
    buttons[i].iBitmap = 10;
    buttons[i].idCommand = ID_VIEW_STACK;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* separator */
    buttons[i].iBitmap = 0;
    buttons[i].idCommand = 0;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_SEP;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* compile for debugging */
    buttons[i].iBitmap = 16;
    buttons[i].idCommand = ID_BUILD_COMPDBG;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* compile and run */
    buttons[i].iBitmap = 17;
    buttons[i].idCommand = ID_BUILD_COMP_AND_RUN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* stop build */
    buttons[i].iBitmap = 19;
    buttons[i].idCommand = ID_BUILD_STOP;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* apply the same icon/command associations to our menus */
    iconmenu_->map_commands(buttons, i, menu_icon_base);

    /* set up the main toolbar */
    toolbar_ = CreateWindowEx(
        0, TOOLBARCLASSNAME, 0,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE
        | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | CCS_NOPARENTALIGN
        | CCS_NOMOVEY | CCS_NORESIZE | CCS_NODIVIDER,
        0, 0, 1, 1, rebar_, 0, CTadsApp::get_app()->get_instance(), 0);

    /* initialize it */
    SendMessage(toolbar_, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(toolbar_, TB_SETEXTENDEDSTYLE, 0, 0);
    SendMessage(toolbar_, TB_SETBITMAPSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));
    SendMessage(toolbar_, TB_SETBUTTONSIZE, 0,
                MAKELONG(TOOLBAR_ICON_WIDTH, TOOLBAR_ICON_HEIGHT));

    /* add the buttons */
    SendMessage(toolbar_, TB_ADDBITMAP, 71, (LPARAM)&tba);
    SendMessage(toolbar_, TB_ADDBUTTONS, (WPARAM)i, (LPARAM)buttons);

    /* set up the creation descriptor for the main toolbar */
    tbci[tbcnt].init(toolbar_, DEBUG_TOOLBAR_ID, "", 0,
                     tbcnt, TRUE, TRUE);
    ++tbcnt;


    /*
     *   we have all of the toolbars set up, so go add them to the rebar
     */
    add_rebar_toolbars(tbci, tbcnt);


    /*
     *   Status line
     */

    /* create the statusline control */
    statusline_ = new CTadsStatusline(this, TRUE, 1999);

    /* register as a source of status information */
    statusline_->main_part()->register_source(this);

    /* add our text editor line/column display part */
    CTadsStatusPart *lcpart = new CTadsStatusPartLineCol();
    statusline_->add_part(lcpart, 1);
    lcpart->register_source(&cursor_pos_source_);

    /* add the build progress bar display part */
    CTadsStatusPartProgress *progpart = new CTadsStatusPartProgress();
    statusline_->add_part(progpart, 0);
    progpart->set_source(&progress_source_);

    /* initialize the status line progress bar window */
    progress_source_.init_win(statusline_->get_handle());

    /* plug it in to all of our key tables */
    for (HtmlKeyTable *t = keytabs_ ; t->name != 0 ; ++t)
    {
        if (t->accel != 0)
            t->accel->set_statusline(statusline_);
    }

    /* hide the statusline window if the prefs don't want a statusline */
    if (!show_statusline_)
         ShowWindow(statusline_->get_handle(), SW_HIDE);

    /* initialize the recent-games menu */
    update_recent_game_menu();

    /*
     *   Set a timer to handle some initialization work that we have to defer
     *   until after the window is fully displayed and we're reading event
     *   messages.  (This is a kludge to work around weird Windows issues
     *   that don't seem to have any better solutions.)
     */
    SetTimer(handle_, S_init_timer_id = alloc_timer_id(), 10, 0);

    /* set a timer for Doc Search temporary frame page cleanup */
    SetTimer(handle_, S_cleanup_timer_id = alloc_timer_id(),
             TEMP_PAGE_CLEANUP_INTERVAL, 0);

    /* add some extra icon associations */
    iconmenu_->map_command(ID_HELP_SEARCH_DOC, menu_icon_base + 20);
    iconmenu_->map_command(ID_HELP_WWWTADSORG, menu_icon_base + 71);
    iconmenu_->map_command(ID_EDIT_FIND, menu_icon_base + 22);
    iconmenu_->map_command(ID_HELP_TOPICS, menu_icon_base + 23);
    iconmenu_->map_command(ID_EDIT_DELETE, menu_icon_base + 31);
    iconmenu_->map_command(ID_HELP_TADSMAN, menu_icon_base + 32);
    iconmenu_->map_command(ID_HELP_T3LIB, menu_icon_base + 33);
    iconmenu_->map_command(ID_BUILD_COMPEXE, menu_icon_base + 37);
    iconmenu_->map_command(ID_BUILD_COMPINST, menu_icon_base + 38);
    iconmenu_->map_command(ID_SEARCH_DOC, menu_icon_base + 20);
    iconmenu_->map_command(ID_SEARCH_PROJECT, menu_icon_base + 21);
    iconmenu_->map_command(ID_SEARCH_FILE, menu_icon_base + 22);
    iconmenu_->map_command(ID_BUILD_SETTINGS, menu_icon_base + 40);
    iconmenu_->map_command(ID_DEBUG_OPTIONS, menu_icon_base + 39);
    iconmenu_->map_command(ID_WINDOW_RESTORE, menu_icon_base + 41);
    iconmenu_->map_command(ID_WINDOW_MAXIMIZE, menu_icon_base + 42);
    iconmenu_->map_command(ID_WINDOW_MINIMIZE, menu_icon_base + 43);
    iconmenu_->map_command(ID_FILE_CLOSE, menu_icon_base + 44);
    iconmenu_->map_command(ID_VIEW_HELPWIN, menu_icon_base + 45);
    iconmenu_->map_command(ID_VIEW_DOCSEARCH, menu_icon_base + 59);
    iconmenu_->map_command(ID_VIEW_PROJECT, menu_icon_base + 46);
    iconmenu_->map_command(ID_SHOW_DEBUG_WIN, menu_icon_base + 47);
    iconmenu_->map_command(ID_PROFILER_START, menu_icon_base + 48);
    iconmenu_->map_command(ID_PROFILER_STOP, menu_icon_base + 49);
    iconmenu_->map_command(ID_VIEW_FILESEARCH, menu_icon_base + 50);
    iconmenu_->map_command(ID_FILE_PAGE_SETUP, menu_icon_base + 51);
    iconmenu_->map_command(ID_FILE_PRINT, menu_icon_base + 52);
    iconmenu_->map_command(ID_EDIT_REPLACE, menu_icon_base + 53);
    iconmenu_->map_command(ID_BUILD_RELEASEZIP, menu_icon_base + 54);
    iconmenu_->map_command(ID_BUILD_WEBPAGE, menu_icon_base + 55);
    iconmenu_->map_command(ID_HELP_TUTORIAL, menu_icon_base + 56);
    iconmenu_->map_command(ID_VIEW_SCRIPT, menu_icon_base + 57);
    iconmenu_->map_command(ID_SCRIPT_DELETE, menu_icon_base + 31);
    iconmenu_->map_command(ID_SCRIPT_REPLAY, menu_icon_base + 0);
    iconmenu_->map_command(ID_SCRIPT_REPLAY_TO_CURSOR, menu_icon_base + 5);
#ifdef NOFEATURE_PUBLISH
    iconmenu_->map_command(ID_BUILD_PUBLISH, menu_icon_base + 70);
#endif
}

/*
 *   Process window activation
 */
int CHtmlSys_dbgmain::do_activate(int flag, int, HWND)
{
    /* if we're activating, establish our accelerator */
    if (flag != WA_INACTIVE)
    {
        /* establish our accelerator */
        update_key_bindings(TRUE);

        /*
         *   If we're not already refreshing outdated files, post a
         *   message to myself so that we do this after we've finished
         *   activation.  (We post a separate message, because activation
         *   is too delicate to do it in-line.  We test for recursion,
         *   because we may pop up message boxes during the refresh
         *   process, which will result in re-activations when the message
         *   boxes are dismissed.)
         */
        if (!refreshing_sources_)
            PostMessage(handle_, HTMLM_REFRESH_SOURCES, 0, 0);

        /*
         *   If we're not in the debugger, clear the stack window.  This
         *   will ensure that, if the user activates the debugger window
         *   while the game is running, we won't give a misleading
         *   impression of the current execution state by showing them
         *   what was in effect the last time we were in the debugger.
         */
        if (!in_debugger_)
        {
            helper_->update_stack_window(0, TRUE);
            helper_->forget_current_line();
        }
    }

    /* proceed with default activation */
    return FALSE;
}

/*
 *   Set the long-running-process status
 */
void CHtmlSys_dbgmain::set_lrp_status(const textchar_t *str)
{
    /* set the new status text */
    lrp_status_.set(str != 0 ? str : "");

    /* update the status line */
    if (statusline_ != 0)
    {
        /* update the status */
        statusline_->update();

        /*
         *   force a redraw, since we're probably ignoring messages until the
         *   long process completes
         */
        UpdateWindow(handle_);
        UpdateWindow(statusline_->get_handle());
    }
}

/*
 *   show or hide the status line
 */
void CHtmlSys_dbgmain::show_statusline(int show)
{
    /* remember the new status */
    show_statusline_ = show;

    /* remember the new setting in the configuration */
    global_config_->setval("statusline", "visible", show_statusline_);

    /* show or hide the control window */
    if (statusline_ != 0)
        ShowWindow(statusline_->get_handle(),
                   show_statusline_ ? SW_SHOW : SW_HIDE);

    /* act as though we've been resized, to regenerate the layout */
    synth_resize();
}

/*
 *   Get our statusline status
 */
textchar_t *CHtmlSys_dbgmain::get_status_message(int *caller_deletes)
{
    textchar_t *msg;

    /* presume we'll return a static string that won't need to be deleted */
    *caller_deletes = FALSE;

    /* if we have a partial menu key, show it */
    if (taccel_ != 0
        && (msg = taccel_->get_status_message(caller_deletes)) != 0)
        return msg;

    /* if there's a long-running process, show its status */
    if (lrp_status_.get() != 0 && lrp_status_.get()[0] != '\0')
        return lrp_status_.get();

    /* if a menu item is selected, show its description */
    if (statusline_menu_cmd_ != 0)
        return (textchar_t *)statusline_menu_cmd_->get_desc();

    /* get our status message */
    if (quitting_)
        return "Terminating";
    if (!workspace_open_)
        return "No Project Loaded";
    if (compiler_hproc_ != 0)
    {
        if (compile_status_.get() != 0 && compile_status_.get()[0] != 0)
            return compile_status_.get();
        else
            return "Build in Progress";
    }
    if (!game_loaded_)
        return "Ready";
    if (in_debugger_)
        return "Program Paused";

    /*
     *   we have a game loaded and we're not in the debugger, so the game
     *   must be running
     */
    return "Program Running";
}

/*
 *   Process a menu item selection for the status line
 */
int CHtmlSys_dbgmain::do_status_menuselect(WPARAM wpar, LPARAM lpar)
{
    UINT id = LOWORD(wpar);
    UINT flags = HIWORD(wpar);

    /*
     *   if we're closing the previous menu, clear the menu status message;
     *   otherwise, if it's a regular command menu item (not a submenu, and
     *   not a command in a system menu), and we have a description for the
     *   command, show the description on the status line
     */
    if (HIWORD(wpar) == 0xFFFF && lpar == 0)
    {
        /* closing the menu - forget any previous status message */
        if (statusline_menu_cmd_ != 0)
        {
            statusline_menu_cmd_ = 0;
            return TRUE;
        }
    }
    else if ((flags & (MF_POPUP | MF_SYSMENU)) == 0 && command_hash != 0)
    {
        CHtmlHashEntryUInt *entry;
        unsigned int idui = id;

        /* find the command in the ID-keyed hash table */
        entry = (CHtmlHashEntryUInt *)command_hash->find(
            (const textchar_t *)&idui, sizeof(idui));
        if (entry != 0)
        {
            /* remember it in the status line */
            statusline_menu_cmd_ = (HtmlDbgCommandInfo *)entry->val_;

            /* tell the caller we updated the status line contents */
            return TRUE;
        }
    }

    /*
     *   if we get this far, it means we didn't find a menu command to show,
     *   so clear any previous menu command from the status line
     */
    statusline_menu_cmd_ = 0;
    return TRUE;
}

/*
 *   Schedule a full library reload.  If there's already a full reload
 *   scheduled, we'll ignore the redundant request.
 */
void CHtmlSys_dbgmain::schedule_lib_reload()
{
    MSG msg;

    /*
     *   Remove any pending library reload request.  Remove any existing one
     *   (rather than simply not schedling a new one if there's already one
     *   there) to ensure that we upgrade the request to an unconditional
     *   reload, overriding any conditional (modified-files-only) reload that
     *   might have already been scheduled.  (An unconditional request can
     *   simply supersede a modified-only request, since the unconditional
     *   request will load any modified files along with everything else.)
     */
    PeekMessage(&msg, handle_, HTMLM_REFRESH_SOURCES, HTMLM_REFRESH_SOURCES,
                PM_REMOVE);

    /*
     *   Schedule the new message.  Pass TRUE for the wparam to indicate
     *   that the reload is unconditional.
     */
    PostMessage(handle_, HTMLM_REFRESH_SOURCES, TRUE, 0);
}

/*
 *   Do owner drawing
 */
int CHtmlSys_dbgmain::do_draw_item(int ctl_id, DRAWITEMSTRUCT *di)
{
    /* if the window is the status line, pass it along to the status object */
    if (statusline_ != 0 && statusline_->owner_draw(ctl_id, di))
        return TRUE;

    /* not handled */
    return FALSE;
}

/*
 *   Process a user message
 */
int CHtmlSys_dbgmain::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    switch(msg)
    {
    case HTMLM_REFRESH_SOURCES:
        /*
         *   Refresh libraries.  If 'wpar' is true it means that we're to
         *   refresh unconditionally; otherwise, it means that we're merely
         *   to refresh any libraries that have changed on disk.
         *
         *   Note that we're in the process of refreshing our windows, so
         *   that we don't try to do recursive refreshing in response to
         *   dismissal of message boxes that we may display during this
         *   process.
         */
        refreshing_sources_ = TRUE;

        /*
         *   go through our open source windows and reload any that have been
         *   changed since we last loaded them
         */
        helper_->enum_source_windows(&enum_win_activate_cb, this);

        /* if we have a project window, refresh it as well */
        if (projwin_ != 0)
        {
            /*
             *   refresh modified libraries or unconditionally, according to
             *   the 'wpar'
             */
            if (wpar)
                projwin_->refresh_all_libs();
            else
                projwin_->refresh_for_activation();
        }

        /* we're done refreshing the sources */
        refreshing_sources_ = FALSE;

        /* handled */
        return TRUE;

    case HTMLM_BUILD_DONE:
        /*
         *   "Build done" message - this is posted to us by the background
         *   thread doing the build when it finishes.  wpar is the exit
         *   code of the build process.
         */

        /* the build is finished - clear the build flag */
        stat_success_ = (wpar == 0);
        build_running_ = FALSE;

        /*
         *   wait for the build thread to exit (which should be
         *   immediately, since posting us is the last thing it does),
         *   then close its handle
         */
        WaitForSingleObject(build_thread_, INFINITE);
        CloseHandle(build_thread_);

        /* if a build dialog is running, dismiss it */
        if (build_wait_dlg_ != 0)
        {
            /*
             *   post our special command message to the dialog to tell it
             *   that the build is finished - when the dialog receives
             *   this, it will dismiss itself, allowing us to proceed with
             *   whatever we were waiting for while the build was running
             */
            PostMessage(build_wait_dlg_->get_handle(), WM_COMMAND,
                        IDC_BUILD_DONE, 0);
        }

        /*
         *   If we're meant to run the program when the compilation is
         *   finished, and the build was successful, run the program by
         *   posting a "go" command to ourselves.
         */
        if (wpar == 0 && run_after_build_)
            PostMessage(handle_, WM_COMMAND, ID_DEBUG_BUILDGO, 0);

        /*
         *   if the build was unsuccessful, forget any script we were
         *   supposed to execute
         */
        if (wpar != 0 && is_replay_script_set())
        {
            /* if it's a temporary script, delete the temp file */
            if (is_replay_script_temp())
                remove(get_replay_script());

            /* clear the script, if any */
            clear_replay_script();
        }

        /* turn auto-vscroll back on in the debug log window */
        if (helper_->create_debuglog_win(this) != 0)
            ((CHtmlSysWin_win32_dbglog *)helper_->get_debuglog_win())
                ->set_auto_vscroll(TRUE);

        /* handled */
        return TRUE;

    case HTMLM_UPDATE_MDI_STATUS:
        /*
         *   If we have any visible MDI children, note the current maximized
         *   status.  Don't bother if there are no visible children; in this
         *   case, we want the status from the last visible window to stick.
         */
        {
            /* get the current MDI status */
            BOOL maxed = FALSE;
            HWND hwnd = (HWND)SendMessage(get_parent_of_children(),
                                          WM_MDIGETACTIVE, 0, (LPARAM)&maxed);

            /* if a new setting was passed in explicitly, use it instead */
            if (HIWORD(wpar))
                maxed = LOWORD(wpar);

            /*
             *   if the window is visible, make sure its maximized status
             *   matches our own records of what it should be - the Windows
             *   MDI handler forgets the status when there are no visible MDI
             *   children, so we want to restore it as soon as there are
             *   windows visible again
             */
            if (hwnd != 0
                && IsWindowVisible(hwnd)
                && maxed != (BOOL)mdi_child_maximized_)
            {
                HWND chi;

                /* note the new status */
                mdi_child_maximized_ = maxed;

                /* notify the MDI child windows of the change */
                for (chi = GetWindow(get_parent_of_children(), GW_CHILD) ;
                     chi != 0 ; chi = GetWindow(chi, GW_HWNDNEXT))
                    SendMessage(chi, DOCKM_MDIMAXCHANGE, maxed, 0);
            }

            /* update the MDI tab bar to make sure it's in sync */
            resize_mdi_client();
        }

        /* handled */
        return TRUE;

    case HTMLM_MSGBOX:
        /* show the message box - the lparam is a copy of the message */
        MessageBox(0, (char *)lpar, "TADS",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

        /* free the allocated copy of the message */
        if (lpar != 0)
            th_free((char *)lpar);

        /* handled */
        return TRUE;

    case HTMLM_GAME_STARTING:
        /*
         *   The game has started running.  We post this to ourselves after
         *   launching the game under the debugger.  This tells us that we've
         *   started the game and cleared queued messages up to that point.
         *
         *   At this point, the game might be executing, in which case we
         *   should bring its window to the front; OR we could already have
         *   hit a breakpoint or single-step interrupt, in which case the
         *   debugger should be displayed.
         */
        if (in_debugger_)
            bring_to_front();
        else
        {
            /* post the bring-to-front message unless this is a web UI game */
            CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();
            if (mainwin != 0 && !mainwin->is_webui_game())
                PostMessage(CHtmlSys_mainwin::get_main_win()->get_handle(),
                            HTMLM_BRING_TO_FRONT, 0, 0);
        }
        return TRUE;

    default:
        /* it's not one of ours; inherit default behavior */
        return CHtmlSys_framewin::do_user_message(msg, wpar, lpar);
    }
}

/*
 *   Handle a timer
 */
int CHtmlSys_dbgmain::do_timer(int id)
{
    /* if it's the initialization timer, do some deferred work */
    if (id == S_init_timer_id)
    {
        /*
         *   set the searchbar mode button's icon - this seems not to work
         *   unless we wait until a while after the window has been set up
         */
        set_searchbar_mode(get_searchbar_mode());

        /* we don't need this timer again */
        KillTimer(handle_, S_init_timer_id);
        free_timer_id(S_init_timer_id);
        S_init_timer_id = 0;

        /* handled */
        return TRUE;
    }
    else if (id == S_cleanup_timer_id)
    {
        /* clean up any expired files */
        clean_up_temp_pages(FALSE);

        /* handled */
        return TRUE;
    }
    else if (id == game_to_front_timer_)
    {
        /* if the game is running, bring its window to the front */
        if (!in_debugger_ && workspace_open_
            && game_loaded_ && !build_running_)
        {
            /* bring the game window to the front */
            CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();
            if (mainwin != 0 && !mainwin->is_webui_game())
                CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();

            /* bring the web UI to the foreground, if present */
            w32_webui_to_foreground();
        }

        /* remove the timer - we only want it to fire once per 'go' */
        KillTimer(handle_, game_to_front_timer_);
    }

    /* inherit the default handling */
    return CHtmlSys_framewin::do_timer(id);
}



/*
 *   Callback for enumerating source windows during debugger activation.
 *   For each window that corresponds to an external file, we'll check the
 *   external file's modification date to see if it's been modified since
 *   we originally loaded it, and reload the file if so.
 */
void CHtmlSys_dbgmain::enum_win_activate_cb(void *ctx0, int /*idx*/,
                                            IDebugWin *win0,
                                            int /*line_source_id*/,
                                            HtmlDbg_wintype_t win_type)
{
    CHtmlSys_dbgmain *ctx = (CHtmlSys_dbgmain *)ctx0;
    CHtmlSys_dbgsrcwin *win;

    /* we don't need to worry about anything but source files */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* appropriately cast the window */
    win = (CHtmlSys_dbgsrcwin *)win0;

    /* ask the window to compare its dates */
    if (win->file_changed_on_disk())
    {
        const char *msg = 0;
        int update;

        /* presume we won't update the file */
        update = FALSE;

        /*
         *   The file has changed.  If the loaded copy is "dirty" (i.e., it
         *   has unsaved changes), definitely prompt the user - we have a
         *   three-way-merge problem, and we need the user to resolve it for
         *   us.  If the loaded copy has no unsaved changes, prompt the user
         *   only if the config options say we should AND the file wasn't
         *   explicitly opened in an external editor.
         */
        if (win->is_dirty())
        {
            /*
             *   we have unsaved changes to our in-memory copy, so definitely
             *   alert the user to the problem
             */
            msg = "This file has changed on disk since you opened it in "
                  "Workbench. If you reload it, the changes you've made "
                  "to the Workbench copy will be lost. Do you want to "
                  "reload the file?";
        }
        else if (!win->was_file_opened_for_editing()
                 && ctx->get_global_config_int("ask reload src", 0, TRUE))
        {
            /*
             *   The config options say we should generally ask before
             *   reloading external updates, and we don't have the special
             *   case for this file that it was sent to the external text
             *   editor explicitly, so alert the user.
             */
            msg = "This file has changed on disk since you opened it in "
                  "Workbench. Fortunately, you haven't made any unsaved "
                  "changes to the Workbench copy. Do you want to reload "
                  "the file?";
        }
        else
        {
            /*
             *   The memory copy is clean, so there are no local changes to
             *   lose; and furthermore, either (a) the config options say we
             *   should silently reload in this case, or (b) the file was
             *   explicitly opened for editing in the external editor, in
             *   which case the user pretty much expressly told us to expect
             *   external updates.  In either case, just silently reload the
             *   file.
             */
            update = TRUE;
        }

        /* if we decided to ask, ask */
        if (msg != 0)
        {
            char msgbuf[MAX_PATH + 200];

            /* build the full message */
            sprintf(msgbuf, "%s\r\n%s", win->get_path(), msg);
            switch (MessageBox(ctx->get_handle(), msgbuf, "TADS Workbench",
                               MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1))
            {
            case IDYES:
            case 0:
                /* reload the file */
                update = TRUE;
                break;

            default:
                /* ignore this file */
                break;
            }
        }

        /* if we do indeed want to update the file, reload it */
        if (update)
            ctx->helper_->reload_source_window(ctx, win);
    }

    /*
     *   bring the window's internal timestamp up to date, so that we
     *   don't ask again until the file changes again
     */
    win->update_file_time();
}


/*
 *   Process a notification message.  We receive notifications from the
 *   toolbar to get tooltip text.
 */
int CHtmlSys_dbgmain::do_notify(int control_id, int notify_code,
                                LPNMHDR nmhdr)
{
    /* check the notify code */
    switch(notify_code)
    {
    case TTN_NEEDTEXT:
        /* retreive tooltip text for a button */
        {
            TOOLTIPTEXT *ttt = (TOOLTIPTEXT *)nmhdr;
            int id = ttt->hdr.idFrom;
            CHtmlDbgSubWin *active_win = get_active_dbg_win();

            /* check to see if the active window wants to handle it */
            if (active_win != 0 && active_win->active_get_tooltip(ttt))
                return TRUE;

            /* get the strings from my application instance */
            ttt->hinst = CTadsApp::get_app()->get_instance();

            /* determine which button was pressed */
            switch(id)
            {
            case ID_DEBUG_GO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_GO);
                break;

            case ID_DEBUG_REPLAY_SESSION:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_REPLAY_SESSION);
                break;

            case ID_DEBUG_STEPINTO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPINTO);
                break;

            case ID_DEBUG_STEPOVER:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPOVER);
                break;

            case ID_DEBUG_RUNTOCURSOR:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_RUNTOCURSOR);
                break;

            case ID_DEBUG_BREAK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_BREAK);
                break;

            case ID_FILE_TERMINATEGAME:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_TERMINATEGAME);
                break;

            case ID_DEBUG_SETCLEARBREAKPOINT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SETCLEARBREAKPOINT);
                break;

            case ID_DEBUG_DISABLEBREAKPOINT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_DISABLEBREAKPOINT);
                break;

            case ID_DEBUG_EVALUATE:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_EVALUATE);
                break;

            case ID_VIEW_GAMEWIN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_GAMEWIN);
                break;

            case ID_VIEW_WATCH:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_WATCH);
                break;

            case ID_VIEW_LOCALS:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_LOCALS);
                break;

            case ID_VIEW_STACK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_VIEW_STACK);
                break;

            case ID_DEBUG_SHOWNEXTSTATEMENT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SHOWNEXTSTATEMENT);
                break;

            case ID_DEBUG_SETNEXTSTATEMENT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_SETNEXTSTATEMENT);
                break;

            case ID_DEBUG_STEPOUT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_DEBUG_STEPOUT);
                break;

            case ID_BUILD_COMPDBG:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_COMPDBG);
                break;

            case ID_BUILD_STOP:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_STOP);
                break;

            case ID_BUILD_COMP_AND_RUN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_BUILD_COMP_AND_RUN);
                break;

            case ID_HELP_GOBACK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_GOBACK);
                break;

            case ID_HELP_GOFORWARD:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_GOFORWARD);
                break;

            case ID_HELP_REFRESH:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_REFRESH);
                break;

            case ID_HELP_TADSMAN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_TADSMAN);
                break;

            case ID_HELP_T3LIB:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_T3LIB);
                break;

            case ID_HELP_TUTORIAL:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_TUTORIAL);
                break;

            case ID_HELP_TOPICS:
                ttt->lpszText = MAKEINTRESOURCE(IDS_HELP_TOPICS);
                break;

            case ID_SEARCH_GO:
                /* reflect the current search mode */
                switch (searchbarcmd_)
                {
                case ID_SEARCH_DOC:
                    ttt->lpszText = MAKEINTRESOURCE(IDS_SEARCH_DOC);
                    break;

                case ID_SEARCH_PROJECT:
                    ttt->lpszText = MAKEINTRESOURCE(IDS_SEARCH_PROJECT);
                    break;

                case ID_SEARCH_FILE:
                    ttt->lpszText = MAKEINTRESOURCE(IDS_SEARCH_FILE);
                    break;
                }
                break;

            case ID_FILE_CREATEGAME:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_CREATEGAME);
                break;

            case ID_FILE_LOADGAME:
                ttt->lpszText = MAKEINTRESOURCE(IDSDB_FILE_LOADGAME);
                break;

            case ID_FILE_NEW:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_NEW);
                break;

            case ID_FILE_OPEN:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_OPEN);
                break;

            case ID_FILE_SAVE:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_SAVE);
                break;

            case ID_FILE_SAVE_ALL:
                ttt->lpszText = MAKEINTRESOURCE(IDS_FILE_SAVE_ALL);
                break;

            case ID_EDIT_CUT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_CUT);
                break;

            case ID_EDIT_COPY:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_COPY);
                break;

            case ID_EDIT_PASTE:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_PASTE);
                break;

            case ID_EDIT_UNDO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_UNDO);
                break;

            case ID_EDIT_REDO:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_REDO);
                break;

            case ID_EDIT_TOGGLEBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_TOGGLEBOOKMARK);
                break;

            case ID_EDIT_SETNAMEDBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_SETNAMEDBOOKMARK);
                break;

            case ID_EDIT_POPBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_POPBOOKMARK);
                break;

            case ID_EDIT_JUMPNEXTBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_JUMPNEXTBOOKMARK);
                break;

            case ID_EDIT_JUMPPREVBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_JUMPPREVBOOKMARK);
                break;

            case ID_EDIT_JUMPNAMEDBOOKMARK:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_JUMPNAMEDBOOKMARK);
                break;

            case ID_EDIT_ADDCOMMENT:
                ttt->lpszText = MAKEINTRESOURCE(IDS_EDIT_ADDCOMMENT);
                break;

            default:
                /* not handled */
                return FALSE;
            }
        }

        /* handled */
        return TRUE;

    case RBN_HEIGHTCHANGE:
        /* rebar height change - resize the MDI client */
        resize_mdi_client();

        /* handled */
        return TRUE;

    case NM_RCLICK:
        /* handle right clicks on the toolbar */
        if (nmhdr->hwndFrom == rebar_ || IsChild(rebar_, nmhdr->hwndFrom))
        {
            HMENU m;
            POINT pt;

            /* load the toolbar menu */
            m = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_TOOLBAR_POPUP));

            /* get the mouse position */
            GetCursorPos(&pt);
            ScreenToClient(handle_, &pt);

            /* show the toolbar context menu */
            track_context_menu(GetSubMenu(m, 0), pt.x, pt.y);

            /* delete the menu */
            DestroyMenu(m);

            /* handled */
            return TRUE;
        }

        /* handle right-clicks on the MDI tab */
        if (nmhdr->hwndFrom == mdi_tab_->get_handle())
        {
            int idx;
            CTadsWin *win;

            /* if there's a selected tab, send it the click */
            if ((idx = mdi_tab_->get_selection()) != -1
                && (win = (CTadsWin *)mdi_tab_->get_item_param(idx)) != 0)
            {
                POINT pt;
                POINTS pts;

                /* send this as a non-client right-click in the title bar */
                GetCursorPos(&pt);
                pts.x = (SHORT)pt.x;
                pts.y = (SHORT)pt.y;
                SendMessage(win->get_handle(), WM_NCRBUTTONDOWN,
                            HTCAPTION, *(LPARAM *)&pts);
            }

            /* handled */
            return TRUE;
        }

        /* others aren't handled */
        return FALSE;

    case TBN_DROPDOWN:
        /* toolbar dropdown button click */
        if (nmhdr->hwndFrom == searchbar_)
        {
            NMTOOLBAR *t = (NMTOOLBAR *)nmhdr;
            HMENU m;
            RECT rc;
            RECT rcbar;
            POINT pt;
            int cmd;

            /* load the toolbar menu */
            m = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_SEARCH_MODE_POPUP));

            /* get the position of the toolbar, and of the button within it */
            GetWindowRect(searchbar_, &rcbar);
            SendMessage(searchbar_, TB_GETITEMRECT, SEARCHBAR_SEARCH_BTN_IDX,
                        (LPARAM)&rc);

            /* calculate the bottom left coordinate of the button */
            pt.x = rc.left + rcbar.left;
            pt.y = rcbar.bottom;

            /* we need this in our client coordinates */
            ScreenToClient(handle_, &pt);

            /* show the toolbar context menu under the button */
            cmd = track_context_menu_ext(GetSubMenu(m, 0), pt.x, pt.y,
                                         TPM_TOPALIGN | TPM_LEFTALIGN
                                         | TPM_RETURNCMD);

            /* if we got a command, carry it out */
            if (cmd != 0)
                set_searchbar_mode(search_mode_from_cmd(cmd));

            /* delete the menu */
            DestroyMenu(m);

            /* handled */
            return TBDDRET_DEFAULT;
        }

        /* not ours */
        break;

    case TCN_SELCHANGE:
        /* tab control selection change - check for the MDI tab control */
        if (nmhdr->hwndFrom == mdi_tab_->get_handle())
        {
            int idx;
            CTadsWin *win;

            /* select the MDI window based on the tab */
            if ((idx = mdi_tab_->get_selection()) != -1
                && (win = (CTadsWin *)mdi_tab_->get_item_param(idx)) != 0)
            {
                /* tell the MDI client to select this window */
                SendMessage(get_parent_of_children(), WM_MDIACTIVATE,
                            (WPARAM)win->get_handle(), 0);
            }

            /* handled */
            return TRUE;
        }

        /* not ours */
        break;
    }

    /*
     *   if we didn't handle it, and it's for the menu bar, try running it
     *   past the menu bar's notify handler
     */
    if (menubar_ != 0 && nmhdr->hwndFrom == menubar_->get_handle())
        return menubar_->do_notify(notify_code, nmhdr);

    /* we didn't handle it */
    return FALSE;
}

/*
 *   close the main window
 */
int CHtmlSys_dbgmain::do_close()
{
    /*
     *   this will stop the debugging session -- ask the user for
     *   confirmation
     */
    ask_quit();

    /*
     *   Even if we're quitting, don't actually close the window for now;
     *   the quitting flag will make us shut down, so we don't need to
     *   close the window for real right now
     */
    return FALSE;
}

/*
 *   resize the window
 */
void CHtmlSys_dbgmain::do_resize(int mode, int x, int y)
{
    RECT rc;

    /* inherit default */
    CHtmlSys_framewin::do_resize(mode, x, y);

    /* resize the rebar window */
    GetWindowRect(rebar_, &rc);
    MoveWindow(rebar_, 0, 0, x, rc.bottom - rc.top, FALSE);

    /* notify the statusline, to move it to the right spot */
    if (statusline_ != 0)
        statusline_->notify_parent_resize();

    /*
     *   If we're not minimizing the window, resize the MDI client area
     *   and its adornments.
     */
    if (mode == SIZE_MAXIMIZED || mode == SIZE_RESTORED)
        resize_mdi_client();
}

/*
 *   on MDI child window creation and deletion, maintain the tab control
 */
void CHtmlSys_dbgmain::mdi_child_event(CTadsWin *win, UINT msg,
                                       WPARAM wpar, LPARAM lpar)
{
    int idx;

    /* ignore these events if the tab control doesn't exit */
    if (mdi_tab_ == 0)
        return;

    /* check the message to see if there's other processing to do */
    switch (msg)
    {
    case WM_CREATE:
        /*
         *   If the window is visible, add a tab for it.  Don't do this if
         *   it's created as invisible; we'll wait until the WM_SHOWWINDOW or
         *   WM_MDIACTIVATE to add the tab in that case, since we don't want
         *   to offer a tab for a window that the user can't see.
         */
        if (IsWindowVisible(win->get_handle()))
            add_mdi_win_tab(win);

        /*
         *   if our MDI-maximized status is set, and the window is initially
         *   visible, post a message to ensure that this window gets
         *   maximized
         */
// this isn't necessary per-window, as we control this globally - doing
// it per-window slows things down during loading due to animations, so
// it's better to just do it once after we finish loading
//
//        if (mdi_child_maximized_ && IsWindowVisible(win->get_handle()))
//            PostMessage(get_parent_of_children(), WM_MDIMAXIMIZE,
//                        (WPARAM)win->get_handle(), 0);
        break;

    case WM_DESTROY:
        /* remove the tab */
        del_mdi_win_tab(win);
        break;

    case WM_SHOWWINDOW:
        /* add or remove the tab according to the new visibility status */
        if (wpar)
        {
            /* adding the window - add its tab */
            add_mdi_win_tab(win);

            /* make sure it's maximized if that's what our records indicate */
            if (mdi_child_maximized_)
                PostMessage(get_parent_of_children(), WM_MDIMAXIMIZE,
                            (WPARAM)win->get_handle(), 0);
        }
        else
        {
            /* hiding the window - remove its tab */
            del_mdi_win_tab(win);
        }

        /*
         *   update the MDI status, to make sure we fix things up if this
         *   window is the only MDI window - in this case, we'll need to
         *   maximize it if we're showing it, since MDI will have forgotten
         *   the status when all windows were hidden; or we'll need to hide
         *   the MDI bar if we're hiding the window
         */
        PostMessage(handle_, HTMLM_UPDATE_MDI_STATUS, 0, 0);
        break;

    case WM_MDIACTIVATE:
        /*
         *   Add a tab for the window if we don't have one already.  For the
         *   most part, we'll add tabs as windows are created or made
         *   visible, but there's a weird special case in Windows where this
         *   won't work: when a window is created as maximized, it never gets
         *   a WM_SHOWWINDOW.  So, we have to check here and add a tab for
         *   the window if we haven't already.
         */
        if (IsWindowVisible(win->get_handle()))
            add_mdi_win_tab(win);

        /* activate the tab for this window */
        if ((idx = mdi_tab_->find_item_by_param(win)) != -1)
            mdi_tab_->select_item(idx);
        break;

    case WM_SIZE:
        /*
         *   Post ourselves a message to check the MDI maximized status.  We
         *   do this via a posted message rather than right here, because the
         *   sequence of events in MDI resizing sometimes makes it impossible
         *   to tell what the final outcome will be.
         */
        PostMessage(handle_, HTMLM_UPDATE_MDI_STATUS, 0, 0);
        break;
    }
}

/*
 *   Add an MDI tab
 */
void CHtmlSys_dbgmain::add_mdi_win_tab(CTadsWin *win)
{
    /* add a tab for the window if we don't already have one */
    if (mdi_tab_->find_item_by_param(win) == -1)
    {
        char buf[256];
        char *title;
        int idx;

        /* use the root filename of the window title as the tab title */
        GetWindowText(win->get_handle(), buf, sizeof(buf));
        title = os_get_root_name(buf);

        /* add a new tab for this item, and select the new tab */
        idx = mdi_tab_->append_item(title, win);
        mdi_tab_->select_item(idx);
    }
}

/*
 *   Delete a window's MDI tab
 */
void CHtmlSys_dbgmain::del_mdi_win_tab(CTadsWin *win)
{
    int idx;

    /* get the tab for the window; if we find it, delete it */
    if ((idx = mdi_tab_->find_item_by_param(win)) != -1)
        mdi_tab_->delete_item(idx);
}

/*
 *   Resize the MDI client window for a change in the frame layout
 */
void CHtmlSys_dbgmain::resize_mdi_client()
{
    POINT orig;
    RECT cli_rc;
    RECT rc;
    HWND mdichild;
    BOOL maxed;
    WINDOWPLACEMENT wp;

    /* get the main client rectangle */
    GetClientRect(handle_, &cli_rc);

    /* get our window placement information */
    wp.length = sizeof(wp);
    GetWindowPlacement(handle_, &wp);

    /*
     *   If we're minimized, use our RESTORED placement.  There's no point in
     *   resizing our contents down to zero size when we're minimized, since
     *   our entire contents are hidden anyway, and it causes all sorts of
     *   havoc in our docking model to shrink the proportionality bases down
     *   to zero.
     */
    if (wp.showCmd == SW_MINIMIZE
        || wp.showCmd == SW_SHOWMINIMIZED
        || wp.showCmd == SW_SHOWMINNOACTIVE
        || wp.showCmd == SW_FORCEMINIMIZE)
        cli_rc = wp.rcNormalPosition;

    /* resize the rebar window */
    GetWindowRect(rebar_, &rc);
    MoveWindow(rebar_, 0, 0, cli_rc.right, rc.bottom - rc.top, FALSE);

    /* get my client rectangle's area in absolute screen coordinates */
    orig.x = orig.y = 0;
    ClientToScreen(handle_, &orig);

    /* subtract the space occupied by the rebar */
    GetWindowRect(rebar_, &rc);
    OffsetRect(&rc, -orig.x, -orig.y);
    SubtractRect(&cli_rc, &cli_rc, &rc);

    /* subtract the space occupied by the statusline */
    if (statusline_ != 0 && show_statusline_)
    {
        /* get the statusline area, relative to my area */
        GetWindowRect(statusline_->get_handle(), &rc);
        OffsetRect(&rc, -orig.x, -orig.y);

        /* deduct the statusline area from my client area */
        SubtractRect(&cli_rc, &cli_rc, &rc);
    }

    /* fix up the positions of the docked windows */
    if (mdi_dock_port_ != 0)
        mdi_dock_port_->dockport_mdi_resize(&cli_rc);

    /* get the active MDI child */
    mdichild = (HWND)SendMessage(get_parent_of_children(),
                                 WM_MDIGETACTIVE, 0, (LPARAM)&maxed);

    /*
     *   if the MDI child is maximized, AND the active window is visible,
     *   show the MDI tab bar; otherwise hide it
     */
    if (mdi_child_maximized_ && mdichild != 0 && IsWindowVisible(mdichild))
    {
        /* show the tab bar if it's not already visible */
        if (!IsWindowVisible(mdi_tab_->get_handle()))
            ShowWindow(mdi_tab_->get_handle(), SW_SHOW);

        /* move the tab bar to the top of the client area */
        GetClientRect(mdi_tab_->get_handle(), &rc);
        MoveWindow(mdi_tab_->get_handle(), cli_rc.left, cli_rc.top,
                   cli_rc.right - cli_rc.left, rc.bottom - rc.top, TRUE);

        /* take the MDI tab out of the client area */
        cli_rc.top += rc.bottom - rc.top;
    }
    else
    {
        /* hide the tab bar if it's showing */
        if (IsWindowVisible(mdi_tab_->get_handle()))
            ShowWindow(mdi_tab_->get_handle(), SW_HIDE);
    }

    /*
     *   tell the system window the new client size - don't let the MDI
     *   system window handler do the work itself, because its default layout
     *   calculator doesn't have enough information to get it right all the
     *   time
     */
    MoveWindow(get_parent_of_children(), cli_rc.left, cli_rc.top,
               cli_rc.right - cli_rc.left, cli_rc.bottom - cli_rc.top, TRUE);
}

/*
 *   paint the background
 */
int CHtmlSys_dbgmain::do_erase_bkg(HDC hdc)
{
    /* fill in the background with button face color */
    RECT rc;
    GetClientRect(handle_, &rc);
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_3DFACE));
    return TRUE;
}

/*
 *   handle a non-client right-click
 */
int CHtmlSys_dbgmain::do_nc_rightbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /*
     *   if it's a title-bar hit, and the main menu bar is hidden, show the
     *   main menu as a context menu; otherwise, do nothing, allowing the
     *   normal system window menu to be shown
     */
    if (hit_type == HTCAPTION && !is_toolbar_visible(MENU_TOOLBAR_ID))
    {
        POINT pt;
        HMENU popup;

        /* convert the location to client coordinates */
        pt.x = x;
        pt.y = y;
        ScreenToClient(handle_, &pt);

        /* create a popup to contain the main menu */
        popup = CreatePopupMenu();
        AppendMenu(popup, MF_POPUP | MF_STRING, (UINT_PTR)main_menu_, "");

        /* show our menu */
        track_context_menu(GetSubMenu(popup, 0), pt.x, pt.y);

        /* done with the popup menu */
        RemoveMenu(popup, 0, MF_BYPOSITION);
        DestroyMenu(popup);

        /* handled */
        return TRUE;
    }

    /* we didn't handle it */
    return FALSE;
}

/*
 *   Ask the user if they want to quit.  Return true if so, false if not.
 */
int CHtmlSys_dbgmain::ask_quit()
{
    /*
     *   put up a message box asking about quitting (unless the game is
     *   over, in which case we can quit without asking)
     */
    if (game_loaded_ && get_global_config_int("ask exit dlg", 0, TRUE))
    {
        switch(MessageBox(handle_,
                          "This will terminate your debugging session.  "
                          "Do you really want to quit?", "TADS Workbench",
                          MB_YESNO | MB_ICONQUESTION
                          | MB_DEFBUTTON2 | MB_TASKMODAL))
        {
        case IDYES:
        case 0:
            /* go ahead and quit */
            break;

        default:
            /* don't quit */
            return FALSE;
        }
    }

    /* check for any unsaved source files */
    ask_save_ctx asctx(DLG_ASK_SAVE_MULTI);
    helper_->enum_source_windows(&enum_win_ask_save_cb, &asctx);

    /* if they wanted to cancel, cancel */
    if (asctx.cancel)
        return FALSE;

    /* set the flag to indicate that we should quit the program */
    quitting_ = TRUE;

    /*
     *   If we have a game loaded, switch back to design mode now, before we
     *   actually start deleting windows.  This will ensure that it's the
     *   design-mode window configuration that we save in the preferences.
     */
    if (game_loaded_)
        tool_window_mode_switch(FALSE);

    /* tell the main window not to pause for exit */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->enable_pause_for_exit(FALSE);

    /* tell the event loop to relinquish control */
    go_ = TRUE;

    /* abort any command entry that the game is doing */
    abort_game_command_entry();

    /* set the end-of-file flag in the main game window */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->end_current_game();

    /* indicate that we are quitting */
    return TRUE;
}

/*
 *   Abort command entry in the game and force entry into the debugger
 */
void CHtmlSys_dbgmain::abort_game_command_entry()
{
    /*
     *   If we're not in the debugger, set stepping mode, and tell the
     *   main window to abort the current command.  This will force
     *   debugger entry.
     */
    if (!in_debugger_)
    {
        /*
         *   turn off script recording, so that we don't record the program
         *   interruption as an in-game event
         */
        helper_->cancel_script_recording(dbgctx_);

        /* break into debug mode in the VM */
        helper_->set_exec_state_break(dbgctx_);

        /* tell the main window to abort the current command */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->abort_command();
    }
}

/*
 *   store my window position preferences
 */
void CHtmlSys_dbgmain::set_winpos_prefs(const CHtmlRect *winpos)
{
    /* save the position in the debugger configuration */
    local_config_->setval("main win pos", 0, winpos);
    local_config_->setval("main win max", 0, IsZoomed(handle_));

    /* save it in the global configuration as well */
    global_config_->setval("main win pos", 0, winpos);
    global_config_->setval("main win max", 0, IsZoomed(handle_));
}

/*
 *   Search for the parent menu of the given command item
 */
HMENU CHtmlSys_dbgmain::find_parent_menu(HMENU menu, int cmd, int *idx)
{
    int i;
    int cnt;

    /* scan each item */
    for (i = 0, cnt = GetMenuItemCount(menu) ; i < cnt ; ++i)
    {
        /* get this item's ID */
        int id = GetMenuItemID(menu, i);

        /* if it matches, we've found our parent */
        if (id == cmd)
        {
            /* tell the caller the index of this item, if they care */
            if (idx != 0)
                *idx = i;

            /* return the parent menu */
            return menu;
        }

        /* if it's -1, it means this is a submenu - search it recursively */
        if (id == -1)
        {
            /* get the submenu */
            HMENU sub = GetSubMenu(menu, i);

            /* search it; if we find a match, return it */
            if ((sub = find_parent_menu(sub, cmd, idx)) != 0)
                return sub;
        }
    }

    /* didn't find a match */
    return 0;
}


/*
 *   context structure for source file enumerator callback
 */
struct HtmlDbg_srcenum_t
{
    /* number of items inserted into the menu so far */
    int count_;

    /* handle of the menu into which we're inserting */
    HMENU menu_;
};

/*
 *   Set up the file menu.  We'll invoke this the first time we enter the
 *   debugger (after the game is loaded) to load the source files into the
 *   quick-open file submenu.  Note that we can't do this during debugger
 *   initialization, because that happens before the game file is loaded,
 *   so we don't know any of the source files at that time.
 */
void CHtmlSys_dbgmain::load_file_menu()
{
    HMENU mnu;
    HMENU filemnu;
    HMENU submnu;
    HtmlDbg_srcenum_t cbctx;
    int i;
    MENUITEMINFO info;
    static const char no_files[] = "No Files";

    /* get the File menu (first menu) */
    mnu = main_menu_;
    filemnu = GetSubMenu(mnu, 0);

    /*
     *   Find the first item in the file menu with its own submenu whose
     *   first menu item has a command ID in the range we expect (we
     *   search here to insulate against changes in the menu layout; as
     *   long as we don't add another item with a submenu, we'll be safe)
     */
    for (submnu = 0, i = 0 ; i < GetMenuItemCount(filemnu) ; ++i)
    {
        /* get the next item's submenu */
        submnu = GetSubMenu(filemnu, i);

        /* if this item has a submenu, consider it further */
        if (submnu != 0)
        {
            int id;

            /* get the command for the first submenu item */
            id = GetMenuItemID(submnu, 0);

            /*
             *   if this item has an ID we'd expect in this submenu, this
             *   is the one we're looking for
             */
            if (id == ID_FILE_OPEN_NONE
                || (id >= ID_FILE_OPENLINESRC_FIRST
                    && id <= ID_FILE_OPENLINESRC_LAST))
            {
                /* this is our menu - stop searching */
                break;
            }
        }
    }

    /* if we couldn't find a submenu, we can't build a file menu */
    if (submnu == 0)
        return;

    /* delete everything from the menu */
    while (GetMenuItemCount(submnu) != 0)
        DeleteMenu(submnu, 0, MF_BYPOSITION);

    /* insert the placeholder item */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_FILE_OPEN_NONE;
    info.dwTypeData = (char *)no_files;
    info.cch = sizeof(no_files) - 1;
    InsertMenuItem(submnu, 0, TRUE, &info);

    /* load the source files into the menu */
    cbctx.count_ = 0;
    cbctx.menu_ = submnu;
    helper_->enum_source_files(load_menu_cb, &cbctx);

    /*
     *   if we added any items, remove the "no files" placeholder item
     *   that's in the menu resource -- it will still be the first item,
     *   so simply remove the first item from the menu
     */
    if (cbctx.count_ > 0)
    {
        /* delete the placeholder */
        DeleteMenu(submnu, 0, MF_BYPOSITION);
    }
}

/*
 *   Callback for source file enumerator - loads the current source file
 *   into the menu.
 */
void CHtmlSys_dbgmain::load_menu_cb(void *ctx0, const textchar_t *fname,
                                    int line_source_id)
{
    MENUITEMINFO info;
    HtmlDbg_srcenum_t *ctx = (HtmlDbg_srcenum_t *)ctx0;
    char caption[260];

    /* show the root name portion only */
    fname = os_get_root_name((char *)fname);

    /*
     *   Generate the menu item caption.  Prepend a keyboard shortcut key
     *   to the name using the current item number.
     */
    ctx->count_++;
    caption[0] = '&';
    caption[1] = ctx->count_ + (ctx->count_ < 10 ? '0' : 'a' - 10);
    caption[2] = ' ';
    do_strcpy(caption + 3, fname);

    /*
     *   Set up the menu item information structure.  Generate the command
     *   ID by adding the line source ID to the base file command ID.
     */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_FILE_OPENLINESRC_FIRST + line_source_id;
    info.dwTypeData = caption;
    info.cch = get_strlen(caption);

    /* insert the menu at the end of the menu */
    InsertMenuItem(ctx->menu_, ctx->count_, TRUE, &info);
}

/*
 *   save our configuration
 */
void CHtmlSys_dbgmain::save_config()
{
    int i;
    int cnt;
    tdb_bookmark *bk;

    /* if there's no local configuration, there's nothing to save */
    if (local_config_ == 0)
        return;

    /*
     *   If there's no main window, don't bother saving the configuration,
     *   since the window layout is part of the configuration.
     *
     *   We can try to save the configuration after destroying the main
     *   window if we terminate the debugger with the program still running:
     *   we'll save the config, destroy the main window, and then wait for
     *   the program to exit.  When the program exits, it will try to save
     *   the configuration; this is no longer possible because the main
     *   window has been destroyed, and no longer necessary because we will
     *   already have saved the configuration anyway.
     */
    if (S_main_win == 0)
        return;

    /* remember whether or not the active MDI child is maximized */
    local_config_->setval("main win mdi max", 0, (int)mdi_child_maximized_);
    global_config_->setval("main win mdi max", 0, (int)mdi_child_maximized_);

    /* save the docking configuration locally and globally */
    save_dockwin_config(local_config_);
    save_dockwin_config(global_config_);

    /* save the toolbar configuration locally and globally */
    save_toolbar_config(local_config_);
    save_toolbar_config(global_config_);

    /* save the search box history in the global configuration */
    save_searchbox_history(global_config_);

    /* clear out the MDI child list */
    local_config_->clear_strlist("mdi win list", 0);
    local_config_->clear_strlist("mdi win title", 0);

    /* build the list of MDI child windows - start with the helper's list */
    enum_win_save_ctx_t cbctx(local_config_, get_parent_of_children());
    helper_->enum_source_windows(&enum_win_savelayout_cb, &cbctx);

    /* add the windows we manage directly */
    if (watchwin_ != 0)
        cbctx.add_window(watchwin_->get_handle(), ":watch", "");
    if (localwin_ != 0)
        cbctx.add_window(localwin_->get_handle(), ":locals", "");
    if (projwin_ != 0)
        cbctx.add_window(projwin_->get_handle(), ":project", "");
    if (scriptwin_ != 0)
        cbctx.add_window(scriptwin_->get_handle(), ":script", "");

    /* compress the list, to remove invisible and unmatched windows */
    cbctx.compress_z_list();

    /* now write out the windows we found in Z order */
    cbctx.save_z_list();

    /*
     *   Save the MDI tab configuration.  To do this, set the key "mdi tab
     *   order" to a list of integers giving the Z-order position for each
     *   window in the tab list.
     */
    cnt = mdi_tab_->get_count();
    char *p, *tabbuf = new char[cnt*10 + 1];
    for (i = 0, p = tabbuf, *p = '\0' ; i < cnt ; ++i)
    {
        CTadsWin *win;
        int zidx = -1;

        /* get this window from the tab list - the param is a CTadsWin */
        if ((win = (CTadsWin *)mdi_tab_->get_item_param(i)) != 0)
            zidx = cbctx.find_z_index(win->get_handle());

        /* add it to the list */
        sprintf(p, "%s%d", i != 0 ? "," : "", zidx);
        p += strlen(p);
    }

    /* save the key */
    local_config_->setval("mdi tab order", 0, 0, tabbuf);
    delete [] tabbuf;

    /* save the watch and local configurations locally and globally */
    if (watchwin_ != 0)
    {
        watchwin_->save_config(local_config_, "watch win");
        watchwin_->save_config(global_config_, "watch win");
    }
    if (localwin_ != 0)
    {
        localwin_->save_config(local_config_, "locals win");
        localwin_->save_config(global_config_, "locals win");
    }

    /* save the visibility and position of the tool windows */
    save_toolwin_config(helper_->get_debuglog_win(), "log");
    save_toolwin_config(helper_->get_stack_win(), "stack");
    save_toolwin_config(helper_->get_hist_win(), "hist");
    save_toolwin_config(helper_->get_doc_search_win(), "docsearch");
    save_toolwin_config(helper_->get_file_search_win(), "filesearch");
    save_toolwin_config(helper_->get_help_win(), "help");

    /* save breakpoint list */
    helper_->save_bp_config(local_config_);

    /* save the search configuration locally */
    local_config_->setval("search", "exact case", find_dlg_->exact_case);
    local_config_->setval("search", "wrap", find_dlg_->wrap_around);
    local_config_->setval("search", "dir", find_dlg_->direction);
    local_config_->setval("search", "start at top", find_dlg_->start_at_top);
    local_config_->setval("search", "regex", find_dlg_->regex);
    local_config_->setval("search", "whole word", find_dlg_->whole_word);
    local_config_->setval("search", "text", 0, find_text_);
    local_config_->setval("search", "text", 1, replace_text_);
    save_find_config(local_config_, "search dlg");

    /* save the project search configuration */
    local_config_->setval("proj search", "exact case",
                          projsearch_dlg_->exact_case);
    local_config_->setval("proj search", "regex",
                          projsearch_dlg_->regex);
    local_config_->setval("proj search", "whole word",
                          projsearch_dlg_->whole_word);
    local_config_->setval("proj search", "collapse spaces",
                          projsearch_dlg_->collapse_sp);

    /* save the source search directory list */
    srcdir_list_to_config();

    /* save the source file configuration */
    helper_->save_internal_source_config(local_config_);

    /* resolve bookmark handles for all open windows */
    struct
    {
        static void enumcb(void *ctx, int idx, IDebugWin *win,
                           int srcid, HtmlDbg_wintype_t win_type)
        {
            CHtmlSys_dbgsrcwin *srcwin;

            /* if it's a source window, resolve its bookmarks */
            if ((srcwin = ((IDebugSysWin *)win)->idsw_as_srcwin()) != 0)
                srcwin->resolve_bookmarks(FALSE);
        }
    } bkctx;
    helper_->enum_source_windows(&bkctx.enumcb, &bkctx);

    /* save the bookmarks */
    local_config_->clear_strlist("editor", "bookmark");
    for (i = 0, bk = bookmarks_ ; bk != 0 ; bk = bk->nxt, ++i)
    {
        char buf[OSFNMAX + 30];

        /* build the save information: "linenum,bookmarkName,filename" */
        sprintf(buf, "%d,%d,%s", bk->line, bk->name, bk->file->fname.get());

        /* save it */
        local_config_->setval("editor", "bookmark", i, buf);
    }

    /* save version-specific configuration information */
    vsn_save_config();
}

/*
 *   save a tool window configuration
 */
void CHtmlSys_dbgmain::save_toolwin_config(
    IDebugWin *win, const textchar_t *prefix)
{
    char poskey[128];
    char viskey[128];

    /* build the keys */
    sprintf(poskey, "%s win pos", prefix);
    sprintf(viskey, "%s win vis", prefix);

    /* if there's a window, save its settings; otherwise save as hidden */
    if (win != 0)
    {
        RECT rc;
        CHtmlRect hrc;
        HWND hwnd;

        /* get it as a system window */
        IDebugSysWin *syswin = (IDebugSysWin *)win;

        /* get its position */
        hwnd = get_frame_pos(syswin->idsw_get_frame_handle(), &rc);

        /* save it */
        hrc.set(rc.left, rc.top, rc.right, rc.bottom);
        local_config_->setval(poskey, 0, &hrc);
        global_config_->setval(poskey, 0, &hrc);

        /* save its visibility */
        local_config_->setval(viskey, 0, IsWindowVisible(hwnd));
        global_config_->setval(viskey, 0, IsWindowVisible(hwnd));
    }
    else
    {
        /* the window doesn't exist; save as not visible */
        local_config_->setval(viskey, 0, FALSE);
        global_config_->setval(viskey, 0, FALSE);
    }
}

/*
 *   save the search box history
 */
void CHtmlSys_dbgmain::save_searchbox_history(CHtmlDebugConfig *config)
{
    int i, cnt;
    size_t buflen;
    char *buf;

    /* clear the old list */
    config->clear_strlist("searchbar history", 0);

    /* allocate an initial buffer to hold the strings */
    buflen = 256;
    buf = (char *)th_malloc(buflen);

    /* get the number of strings in the combo */
    cnt = SendMessage(searchbox_, CB_GETCOUNT, 0, 0);

    /* save the strings */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get the size of the current string */
        size_t curlen = SendMessage(searchbox_, CB_GETLBTEXTLEN, i, 0) + 1;

        /* expand the buffer if necessary */
        if (curlen > buflen)
        {
            /* reallocate the buffer at the larger size */
            char *newbuf = (char *)th_realloc(buf, curlen);

            /* if that worked, replace our buffer with the new buffer */
            if (newbuf != 0)
            {
                buflen = curlen;
                buf = newbuf;
            }
        }

        /* if we have a buffer that's big enough, get and save the string */
        if (buf != 0 && buflen >= curlen)
        {
            /* get the string */
            SendMessage(searchbox_, CB_GETLBTEXT, i, (LPARAM)buf);

            /* save it */
            config->setval("searchbar history", 0, i, buf);
        }
    }

    /* if we have a buffer, free it */
    if (buf != 0)
        th_free(buf);
}

/*
 *   load the searchbox history
 */
void CHtmlSys_dbgmain::load_searchbox_history(CHtmlDebugConfig *config)
{
    int i, cnt;

    /* get the number of saved strings */
    cnt = config->get_strlist_cnt("searchbar history", 0);

    /* delete any strings in the combo list currently */
    while (SendMessage(searchbox_, CB_GETCOUNT, 0, 0) != 0)
        SendMessage(searchbox_, CB_DELETESTRING, 0, 0);

    /* load the saved strings */
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[256];

        /* get this string */
        if (!config->getval("searchbar history", 0, i, buf, sizeof(buf)))
            SendMessage(searchbox_, CB_ADDSTRING, 0, (LPARAM)buf);
    }
}

/*
 *   Process a menu key
 */
int CHtmlSys_dbgmain::do_menuchar(TCHAR ch, unsigned int menu_type,
                                  HMENU menu, LRESULT *ret)
{
    /* try mapping through the menu */
    return (menubar_->do_menuchar(ch, menu_type, menu, ret)
            || CHtmlSys_framewin::do_menuchar(ch, menu_type, menu, ret));
}

/*
 *   Process a system menu key
 */
int CHtmlSys_dbgmain::do_syskeymenu(TCHAR ch)
{
    /* try running it past the menu bar */
    return (menubar_->do_syskeymenu(ch)
            || CHtmlSys_framewin::do_syskeymenu(ch));
}

/*
 *   save the toolbar configuration
 */
void CHtmlSys_dbgmain::save_toolbar_config(CHtmlDebugConfig *config)
{
    int i;
    int cnt;

    /* if there's no rebar, there's nothing to do */
    if (rebar_ == 0)
        return;

    /* process each band in the rebar */
    cnt = SendMessage(rebar_, RB_GETBANDCOUNT, 0, 0);
    for (i = 0 ; i < cnt ; ++i)
    {
        REBARBANDINFO info;
        const char *id;

        /* get this band's information */
        info.cbSize = rebarbandinfo_size_;
        info.fMask = RBBIM_STYLE | RBBIM_ID | RBBIM_SIZE;
        SendMessage(rebar_, RB_GETBANDINFO, i, (LPARAM)&info);

        /* if the toolbar ID isn't in our config name list, skip it */
        if (info.wID < 1
            || info.wID > sizeof(tb_config_id)/sizeof(tb_config_id[0]))
            continue;

        /* get the config ID name */
        id = tb_config_id[info.wID];

        /* save the settings */
        config->setval(id, "rebarIndex", i);
        config->setval(id, "rebarNewLine", (info.fStyle & RBBS_BREAK) != 0);
        config->setval(id, "rebarVis", (info.fStyle & RBBS_HIDDEN) == 0);
        config->setval(id, "rebarWidth", info.cx);
    }

    /* save the current search mode */
    config->setval("searchMode", 0, get_searchbar_mode());
}

/*
 *   callback for saving the docking configuration
 */
static void save_dockwin_cb(void *ctx, int id, const char *info)
{
    CHtmlDebugConfig *config = (CHtmlDebugConfig *)ctx;

    /* write the string to the "docking config" string */
    config->setval("docking config", 0, id, info);
}

/*
 *   Save the docking window configuration
 */
void CHtmlSys_dbgmain::save_dockwin_config(CHtmlDebugConfig *config)
{
    /* if we don't have a docking port, do nothing */
    if (mdi_dock_port_ == 0)
        return;

    /* clear out the old docking window configuration list */
    config->clear_strlist("docking config", 0);

    /* assign ID's to the model elements */
    mdi_dock_port_->get_model_frame()->serialize(save_dockwin_cb, config);
}

/*
 *   callback for docking configuration loader
 */
static int load_dockwin_cb(void *ctx, int id, char *buf, size_t buflen)
{
    CHtmlDebugConfig *config = (CHtmlDebugConfig *)ctx;

    /* load the desired string */
    return config->getval("docking config", 0, id, buf, buflen);
}

/*
 *   Load the docking window configuration
 */
void CHtmlSys_dbgmain::load_dockwin_config(CHtmlDebugConfig *config)
{
    int cnt;

    /* if we don't have a docking port, do nothing */
    if (mdi_dock_port_ == 0)
        return;

    /*
     *   determine how many configuration strings we have for the docking
     *   configuration
     */
    cnt = config->get_strlist_cnt("docking config", 0);

    /*
     *   if there are no docking configuration strings, don't bother
     *   loading a new configuration - just keep the existing model as it
     *   is currently set up
     */
    if (cnt == 0)
        return;

    /* ask the frame model to load the information */
    mdi_dock_port_->get_model_frame()->load(load_dockwin_cb,
                                            config, cnt);

    /*
     *   In case we loaded a file saved under the old scheme, which used
     *   docking model serial numbers to associate GUID's with windows,
     *   assign GUID's to all of the model windows.
     */
    assign_dockwin_guid("locals win");
    assign_dockwin_guid("watch win");
    assign_dockwin_guid("stack win");
    assign_dockwin_guid("hist win");
    assign_dockwin_guid("log win");
}

/*
 *   Assign a docking window GUID to a window based on its serial number.
 *   This is used to convert a file that uses the old serial numbering scheme
 *   to the new GUID scheme.
 */
void CHtmlSys_dbgmain::assign_dockwin_guid(const char *guid)
{
    int id;

    /*
     *   If we have configuration data for this "dock id" serial number, it's
     *   the mapping of the GUID to the docking window serial number.
     */
    if (!local_config_->getval(guid, "dock id", &id))
    {
        /*
         *   We have the old information, so we need to assign the GUID to
         *   the window identified by this serial number.
         */
        mdi_dock_port_->get_model_frame()->assign_guid(id, guid);

        /*
         *   We've now resolved the serial number to the window and assigned
         *   the GUID, so the serial numbering information is obsolete.
         *   Drop it from the configuration.
         */
        local_config_->delete_val(guid, "dock id");
    }
}

/*
 *   load the docking window configuration for a given window
 */
void CHtmlSys_dbgmain::
   load_dockwin_config_win(CHtmlDebugConfig *config, const char *guid,
                           CTadsWin *subwin, const char *win_title,
                           int visible, tads_dock_align default_dock_align,
                           int default_docked_width,
                           int default_docked_height,
                           const RECT *default_undocked_pos,
                           int default_is_docked, int default_is_mdi)
{
    CTadsWinDock *dock_win;
    RECT rc;
    int need_default;

    /* presume we'll need a default docking container */
    need_default = TRUE;

    /*
     *   Find the window in our docking model.  We're only loading the
     *   configuration at this point, so if the system window hasn't been
     *   created already, leave it hidden if we have to create it.
     */
    dock_win = mdi_dock_port_->get_model_frame()->find_window_by_guid(
        this, guid, visible, win_title);

    /*
     *   If we found a window, and it doesn't already have a subwindow, use
     *   it.  (If the window already has a subwindow, something must be wrong
     *   with the configuration file, since another window must have been
     *   saved with the same serial ID.  Work around this by forcing creation
     *   of a new parent window.)
     */
    if (dock_win != 0 && dock_win->get_subwin() == 0)
    {
        /* set the docking window's subwindow */
        dock_win->set_subwin(subwin);

        /* set the docking window's title */
        SetWindowText(dock_win->get_handle(), win_title);

        /* we don't need a default window now */
        need_default = FALSE;
    }

    /*
     *   If we didn't manage to find any stored configuration information,
     *   create a default docking parent for the window.
     */
    if (need_default)
    {
        /* create the window */
        dock_win = mdi_dock_port_->get_model_frame()->create_sys_win(
            subwin, default_docked_width, default_docked_height,
            default_dock_align, default_undocked_pos, default_undocked_pos,
            win_title, guid, this, get_parent_of_children(),
            default_is_docked, default_is_mdi, visible);
    }

    /*
     *   create the subwindow's system window inside the docking parent
     *   window
     */
    GetClientRect(dock_win->get_handle(), &rc);
    subwin->create_system_window(dock_win, TRUE, "", &rc);

    /* show the window now if it's to be visible */
    ShowWindow(dock_win->get_handle(), visible ? SW_SHOW : SW_HIDE);
}



/*
 *   Terminate the debugger
 */
void CHtmlSys_dbgmain::terminate_debugger()
{
    /*
     *   Before we do anything else, save the configuration.  We need to
     *   do this before destroying the main debugger window, because all
     *   of the other debugger windows are owned by the main window, hence
     *   destroying the main window will destroy all of them first.
     */
    save_config();

    /* close the tool windows */
    close_tool_windows();

    /* if a build is still running, run the "waiting for build" dialog */
    if (build_running_)
        run_wait_for_build_dlg();

    /* delete the main debugger window */
    DestroyWindow(handle_);

    /* forget the main debugger window */
    S_main_win = 0;
}

/*
 *   Run the wait-for-build dialog.  This dialog simply shows that we're
 *   waiting for the build to complete, and keeps running until the build
 *   is finished.
 */
void CHtmlSys_dbgmain::run_wait_for_build_dlg()
{
    /* create the dialog */
    build_wait_dlg_ = new CTadsDlg_buildwait(compiler_hproc_);

    /* display the dialog */
    build_wait_dlg_->run_modal(DLG_BUILD_WAIT, handle_,
                               CTadsApp::get_app()->get_instance());

    /* done with the dialog */
    delete build_wait_dlg_;
    build_wait_dlg_ = 0;
}

/*
 *   Receive notification of TADS shutdown.  This will be invoked when
 *   TADS is closing down, but the debugger will remain active.
 */
void CHtmlSys_dbgmain::notify_tads_shutdown()
{
    /* save the current configuration */
    save_config();

    /* forget our TADS debugger context */
    dbgctx_ = 0;

    /* the game is no longer loaded */
    set_game_loaded(FALSE);

    /* clear the stack window */
    helper_->update_stack_window(0, TRUE);

    /* remove any current line displays */
    helper_->forget_current_line();
}

/*
 *   Debugger main event loop
 */
int CHtmlSys_dbgmain::dbg_event_loop(dbgcxdef *ctx, int bphit, int err,
                                     int *restart, int *reload,
                                     int *terminate, int *abort,
                                     void *exec_ofs)
{
    int ret;
    int old_in_dbg;
    IDebugSysWin *win;
    CHtmlSys_mainwin *mainwin;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* remember the current debugger context */
    dbgctx_ = ctx;

    /* remember the pointer to the debugger's execution offset */
    exec_ofs_ptr_ = exec_ofs;

    /* presume we won't want to reload or terminate */
    *reload = FALSE;
    *terminate = FALSE;

    /*
     *   if the 'quitting' flag is already set, it means they closed the
     *   debugger while the game was running, and we're just now getting
     *   control - immediately return and indicate that we should
     *   terminate
     */
    if (quitting_)
        return FALSE;

    /*
     *   add a self-reference to keep our event loop variable in memory
     *   (it's a part of 'this', hence we need to keep 'this' around)
     */
    AddRef();

    /*
     *   if we're terminating the game, set the caller's terminate flag,
     *   but tell the caller to keep the debugger running
     */
    if (terminating_)
        goto do_terminate;

    /*
     *   If the 'restarting' flag is already set, it means they restarted
     *   the game while the debugger was inactive, and we're just now
     *   getting control - immediately return and indicate that we should
     *   restart
     */
    if (restarting_)
        goto do_restart;

    /* remember whether we were in the debugger when we entered */
    old_in_dbg = in_debugger_;

    /* we're in the debugger */
    in_debugger_ = TRUE;

    /* flush the game window if appropriate */
    if (auto_flush_gamewin_)
        CHtmlDebugHelper::flush_main_output(ctx);

    /*
     *   If the game window is in front, move it behind all of the
     *   debugger windows, since we probably are entering the debugger
     *   after having started reading a command (which brings the game
     *   window to the front).
     */
    w32tdb_bring_group_to_top(get_handle());

    /* clear any temporary breakpoint that we had set */
    helper_->clear_temp_bp(ctx);

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();

    /* tell the main window that the game is paused */
    if (mainwin != 0)
        mainwin->set_game_paused(TRUE);

    /* ask the web UI window to yield the foreground to us */
    w32_webui_yield_foreground();

    /*
     *   Display the current source file.  If we can't find one, bring the
     *   main debugger window (i.e., myself) to the front.
     */
    win = (IDebugSysWin *)helper_->update_for_entry(ctx, this);
    if (win != 0)
        win->idsw_bring_to_front();
    else
        bring_to_front();

    /* update the expressions window */
    update_expr_windows();

    /* check why we stopped, and display a message about it if appropriate */
    if (err != 0)
    {
        char msgbuf[1024];

        /*
         *   we stopped due to a run-time error -- display a message box
         *   with the error text
         */
        helper_->format_error(ctx, err, msgbuf, sizeof(msgbuf));
        MessageBox(handle_, msgbuf, "TADS Runtime Error",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
    else
    {
        /*
         *   Check the breakpoint - if it's a global breakpoint, pop up a
         *   message box telling the user what happened, since it's not
         *   immediately obvious just looking at the code why we entered
         *   the debugger.
         */
        if (helper_->is_bp_global(bphit))
        {
            textchar_t cond[HTMLDBG_MAXCOND + 100];
            int stop_on_change;
            size_t len;

            /*
             *   it's a global breakpoint -- get the text of the
             *   condition, and display it so the user can see why we
             *   stopped
             */
            strcpy(cond, "Stopped at global breakpoint: ");
            len = get_strlen(cond);
            if (!helper_->get_bp_cond(bphit, cond + len, sizeof(cond) - len,
                                      &stop_on_change))
            {
                /*
                 *   if it's a stop-on-true breakpoint, the VM will have
                 *   automatically disabled it; note this
                 */
                if (!stop_on_change)
                    strcat(cond, "\r\n\r\n(Note: This breakpoint "
                           "is now disabled.)");

                /* show the message */
                MessageBox(handle_, cond, "TADS Workbench",
                           MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
            }
        }
    }

    /*
     *   if the game isn't running, and we have a pending build command, we
     *   can now start the build - re-post the build command
     */
    if (!game_loaded_ && pending_build_cnt_ != 0)
    {
        /* post each message in the list */
        for (int i = 0 ; i < pending_build_cnt_ ; ++i)
            PostMessage(get_handle(), WM_COMMAND, pending_build_cmd_[i], 0);

        /* clear the list */
        pending_build_cnt_ = 0;
    }

    /* process events until we start stepping */
    go_ = FALSE;
    ret = CTadsApp::get_app()->event_loop(&go_);

    /* pop debugger status flag */
    in_debugger_ = old_in_dbg;

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();

    /*
     *   if we're quitting, force the return value to FALSE so that we
     *   terminate the game
     */
    if (quitting_)
        ret = FALSE;

    /*
     *   if we're restarting, tell the caller to restart
     */
    if (restarting_)
    {
    do_restart:
        /* start a new page in the main output window */
        if (mainwin != 0)
        {
            CHtmlDebugHelper::flush_main_output(ctx);
            mainwin->clear_all_pages();
        }

        /* tell the caller to do the restart */
        *restart = TRUE;

        /* if we're reloading, tell the caller about it */
        *reload = reloading_;

        /* only restart once per request */
        restarting_ = FALSE;

        /* make sure we re-enter the debugger as soon as the game restarts */
        helper_->set_exec_state_step_into(ctx);
    }

    /*
     *   If the 'aborting' flag is set, tell the caller to abort the
     *   current command
     */
    if (aborting_cmd_)
    {
        /* only abort once per request */
        aborting_cmd_ = FALSE;

        /* make sure we re-enter the debugger on the next command */
        helper_->set_exec_state_step_into(ctx);

        /* tell the caller to do the abort */
        *abort = TRUE;
    }

    /*
     *   if we're terminating the game, set the caller's terminate flag,
     *   but tell the caller to keep the debugger running
     */
    if (terminating_)
    {
    do_terminate:
        /* tell the caller we're terminating */
        *terminate = TRUE;

        /* clear our flag now that we've told the caller about it */
        terminating_ = FALSE;

        /* release our self-reference */
        Release();

        /* tell the caller to keep the debugger loaded */
        return TRUE;
    }

    /* tell the main window that the game is running again, if it is */
    if (mainwin != 0)
        mainwin->set_game_paused(in_debugger_);

    /* release our self-reference */
    Release();

    /* return the result */
    return ret;
}

/*
 *   Update expression windows
 */
void CHtmlSys_dbgmain::update_expr_windows()
{
    /* update the watch window */
    if (watchwin_ != 0)
        watchwin_->update_all();

    /* update the local variables */
    if (localwin_ != 0)
        localwin_->update_all();
}

/*
 *   Add a new watch expression.  This will add the new expression at the
 *   end of the currently selected panel of the watch window.
 */
void CHtmlSys_dbgmain::add_watch_expr(const textchar_t *expr)
{
    if (watchwin_ != 0)
    {
        /* add the expression */
        watchwin_->add_expr(expr, TRUE, TRUE);

        /* make sure the watch window is visible */
        ShowWindow(GetParent(watchwin_->get_handle()), SW_SHOW);
    }
}

/*
 *   Get the text for a Find command
 */
const textchar_t *CHtmlSys_dbgmain::
   get_find_text_ex(int command_id, int *exact_case, int *start_at_top,
                    int *wrap, int *dir, int *regex, int *whole_word,
                    itfdo_scope *scope)
{
    /* check for a search toolbar entry */
    if (command_id == ID_SEARCH_FILE)
    {
        /* get the combo box text */
        SendMessage(searchbox_, WM_GETTEXT,
                    (WPARAM)sizeof(find_text_), (LPARAM)find_text_);

        /* if we got the text, set up the parameters */
        if (find_text_[0] != '\0')
        {
            /* treat this as a continuation search */
            *start_at_top = FALSE;

            /* use a fixed set of parameters for these searches */
            find_dlg_->exact_case = *exact_case = FALSE;
            find_dlg_->wrap_around = *wrap = TRUE;
            find_dlg_->direction = *dir = 1;
            find_dlg_->regex = FALSE;
            find_dlg_->whole_word = FALSE;
            find_dlg_->scope = ITFDO_SCOPE_FILE;

            if (regex != 0)
                *regex = FALSE;
            if (whole_word != 0)
                *whole_word = FALSE;

            /* return the text */
            return find_text_;
        }

        /* the search box is empty; treat this as a new Find */
        command_id = ID_EDIT_FIND;
    }

    /*
     *   If this is a Find command, or if we don't have any text from the
     *   last "Find" command, run the dialog.
     */
    if (command_id == ID_EDIT_FIND || find_text_[0] == '\0')
    {
        /* run the dialog modelessly */
        open_find_dlg();

        /*
         *   return nothing - since the dialog is running modelessly, it's
         *   now up to the dialog to actually carry out any searching
         */
        return 0;
    }
    else
    {
        /*
         *   continuing an old search, so don't start over at the top,
         *   regardless of the last dialog settings
         */
        *start_at_top = FALSE;
    }

    /*
     *   fill in the flags from the last dialog settings, whether this is
     *   a new search or a continued search
     */
    *exact_case = find_dlg_->exact_case;
    *wrap = find_dlg_->wrap_around;
    *dir = find_dlg_->direction;
    if (regex != 0)
        *regex = find_dlg_->regex;
    if (whole_word != 0)
        *whole_word = find_dlg_->whole_word;
    if (scope != 0)
        *scope = find_dlg_->scope;

    /* return the text to find */
    return find_text_;
}

/*
 *   Get the parameters for a Find Next command
 */
const textchar_t *CHtmlSys_dbgmain::get_find_next(
    int *exact_case, int *start_at_top, int *wrap, int *dir, int *regex)
{
    /* fill in the flags from the dialog settings */
    *start_at_top = FALSE;
    *exact_case = find_dlg_->exact_case;
    *wrap = find_dlg_->wrap_around;
    *dir = find_dlg_->direction;
    if (regex != 0)
        *regex = find_dlg_->regex;

    /* return the find text buffer */
    return find_text_;
}


/*
 *   Open the Find dialog
 */
HWND CHtmlSys_dbgmain::open_find_dlg()
{
    /*
     *   if the dialog is already open, but it's not the basic Find dialog,
     *   close the dialog
     */
    if (find_dlg_->get_handle() != 0
        && find_dlg_->get_dlg_id() != DLG_REGEXFIND)
        DestroyWindow(find_dlg_->get_handle());

    /* run the dialog */
    return find_dlg_->run_find_modeless(handle_, DLG_REGEXFIND,
                                        find_text_, sizeof(find_text_));
}

/*
 *   Open the Find-and-Replace dialog
 */
HWND CHtmlSys_dbgmain::open_replace_dlg()
{
    /*
     *   if the dialog is already open, but it's not the Replace dialog,
     *   close the dialog
     */
    if (find_dlg_->get_handle() != 0
        && find_dlg_->get_dlg_id() != DLG_REPLACE)
        DestroyWindow(find_dlg_->get_handle());

    /* run the dialog */
    return find_dlg_->run_replace_modeless(
        handle_, DLG_REPLACE,
        find_text_, sizeof(find_text_),
        replace_text_, sizeof(replace_text_));
}

/* ------------------------------------------------------------------------ */
/*
 *   File searcher object.  This performs a "grep"-style search on a text
 *   file, looking for either a regular expression or a literal string.
 */
class FileSearcher
{
public:
    FileSearcher(const textchar_t *query,
                 int exact_case, int regex, int whole_word, int collapse_sp)
    {
        /* remember the settings */
        this->query = query;
        this->exact_case = exact_case;
        this->regex = regex;
        this->whole_word = whole_word;
        this->collapse_sp = collapse_sp;

        /* if it's a regular expression, compile it */
        if (regex)
        {
            char *utf8;

            /* convert the query to utf-8 */
            utf8 = utf8_from_ansi(query, get_strlen(query));

            /* compile the regular expression */
            rexstat = rexprs.compile_pattern(utf8, get_strlen(utf8), &rexpat);

            /* done with the utf8 form of the query */
            th_free(utf8);
        }
    }

    virtual ~FileSearcher()
    {
        /* delete our pattern, if we have one */
        if (regex)
            rexprs.free_pattern(rexpat);
    }

    /* report an error in a file I/O operation */
    virtual void report_file_error(
        const textchar_t *fname, const textchar_t *errmsg)
    {
    }

    /*
     *   Report a match in a file.  'linenum' is the line number in which the
     *   match appears, and 'linetxt' is the text of the line.  'matchnum' is
     *   the number of the match within this file (1 for the first match,
     *   etc).
     *
     *   We call this only once for each line containing a match, even if the
     *   line contains multiple matches for the search term.
     *
     *   If this returns TRUE, the searcher will continue looking for
     *   additional matches in the same file; if this returns FALSE, the
     *   searcher will simply close the file and return.  So, if the client
     *   is only interested in knowing whether or not a file contains
     *   matches, and doesn't care about finding every individual match, it
     *   should return FALSE here to save time by skipping the rest of the
     *   file.
     */
    virtual int report_match(const textchar_t *fname, int linenum,
                             int matchnum,
                             const textchar_t *linetxt, size_t linelen)
    {
        return TRUE;
    }

    /*
     *   Search a file.  Returns the number of matches found, or -1 if an
     *   error occurs.
     */
    int search_file(const textchar_t *fname)
    {
        /*
         *   If we're not differentiating whitespace, we need to do a full
         *   in-memory search, so that we treat newlines as ordinary
         *   whitespace.  Otherwise, we can do a simple line-by-line search.
         */
        if (collapse_sp)
            return search_collapse_sp(fname);
        else
            return search_line_by_line(fname);
    }

    /*
     *   Search a file without differentiating whitespace.  We treat each run
     *   of whitespace, including newlines, as equivalent to a single space.
     *   To do this, we must load the entire file into memory first, so that
     *   we can strip out spaces.
     */
    int search_collapse_sp(const textchar_t *fname)
    {
        FILE *fp;
        long len;
        int linecnt;
        char **lines;
        char *buf;
        char *src, *dst;
        int matches = 0;
        int qlen = strlen(query);
        static const char mem_err[] = "insufficient memory to load this "
                                      "file - cannot search this file with "
                                      "'collapse spaces' option";

        /* open the file */
        if ((fp = fopen(fname, "r")) == 0)
        {
            report_file_error(fname, "unable to open file");
            return -1;
        }

        /* seek to the end to figure the file's size */
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        /* allocate memory to load the file */
        buf = (char *)th_malloc(len + 1);
        if (buf == 0)
        {
            report_file_error(fname, mem_err);
            fclose(fp);
            return -1;
        }

        /* load the entire file */
        len = fread(buf, 1, len, fp);

        /* done with the file object */
        fclose(fp);

        /* if we're doing a regex search, we need it as utf-8 */
        if (regex)
        {
            /* make a utf-8 version of the buffer */
            char *utf8 = utf8_from_ansi(buf, len);

            /* make sure that succeeded */
            if (utf8 == 0)
            {
                report_file_error(fname, mem_err);
                th_free(buf);
                return -1;
            }

            /* we don't need the original any longer - switch to utf8 only */
            th_free(buf);
            buf = utf8;
        }

        /*
         *   Count lines.  Note that we always have at least two virtual line
         *   markers: one for the beginning of the buffer, and one for the
         *   end (i.e., the byte in memory following the last character in
         *   the buffer).
         */
        for (linecnt = 2, src = buf ; src < buf + len ; ++src)
        {
            if (*src == '\n')
                ++linecnt;
        }

        /* allocate a line start array */
        lines = (char **)th_malloc(linecnt * sizeof(*lines));
        if (lines == 0)
        {
            report_file_error(fname, mem_err);
            th_free(buf);
            return -1;
        }

        /* the first line starts at the start of the buffer */
        lines[0] = buf;

        /* collapse spaces and note line starts */
        for (linecnt = 1, src = dst = buf ; src < buf + len ; ++src)
        {
            char c = *src;

            /* check for whitespace */
            if (c == '\n' || (isascii(c) && isspace(c)) || c == 0)
            {
                /*
                 *   we have a newline or some kind of space: convert it to a
                 *   space, but keep it only if there's not a space preceding
                 *   it
                 */
                if (dst == buf || *(dst-1) != ' ')
                    *dst++ = ' ';
            }
            else
            {
                /* it's not whitespace, so keep it as-is */
                *dst++ = c;
            }

            /* if this is a newline, note the line start */
            if (c == '\n')
                lines[linecnt++] = dst;
        }

        /* null-terminate the overall buffer */
        *dst = '\0';

        /* set a marker for the end of the reduced buffer */
        lines[linecnt] = dst;

        /* note the shortened buffer length */
        len = dst - buf;

        /* now scan for matches */
        for (src = buf ; ; )
        {
            char *matchp;

            /* assume we won't find a match on this round */
            int match = FALSE;

            /* find the next match */
            if (regex)
            {
                CRegexSearcher searcher;
                re_group_register regs[RE_GROUP_REG_CNT];
                int matchlen;
                int matchofs;

                /* search for the pattern */
                while (src < lines[linecnt])
                {
                    /* try finding from the current offset */
                    matchofs = searcher.search_for_pattern(
                        rexpat, src, src, len - (src - buf), &matchlen, regs);

                    /* check for a match */
                    match = (matchofs >= 0);

                    /* if we didn't find a raw match, give up */
                    if (!match)
                        break;

                    /* if we're in word match mode, check for a word match */
                    matchp = src + matchofs;
                    if (whole_word
                        && ((src + matchofs > buf
                             && is_word_char(
                                 utf8_prevchar(matchp)))
                            || (src + matchofs + matchlen < buf + len
                                && is_word_char(
                                    utf8_ptr::s_getch(
                                        matchp + matchlen)))))
                    {
                        /* we don't match after all */
                        match = FALSE;

                        /* move to the next character */
                        src += utf8_ptr::s_charsize(*src);

                        /* keep looking */
                        continue;
                    }

                    /* got a match - stop looking */
                    break;
                }
            }
            else
            {
                while (src < lines[linecnt])
                {
                    /*
                     *   look for a match from here, with or without case
                     *   sensitivity as directed
                     */
                    matchp = (exact_case
                         ? strstr(src, query)
                         : stristr(src, query));

                    /* we matched if we found the substring */
                    match = (matchp != 0);

                    /* if we didn't find a raw match, give up */
                    if (!match)
                        break;

                    /* check for whole-word matching if needed */
                    if (whole_word
                        && ((matchp > buf
                             && is_word_char(*(matchp-1)))
                            || is_word_char(matchp[qlen])))
                    {
                        /* it's not a match after all */
                        match = FALSE;

                        /* move on to the next character */
                        ++src;

                        /* keep looking */
                        continue;
                    }

                    /* got a match - we can stop looking */
                    break;
                }
            }

            /* if we found a match, report it */
            if (match)
            {
                int linenum, lo, hi;
                char *linetxt;
                size_t linelen;
                int cont;

                /* count it */
                ++matches;

                /* binary-search for the line containing our match */
                for (lo = 0, hi = linecnt ; ; )
                {
                    /* check to see if we're done */
                    if (lo > hi)
                    {
                        linenum = 0;
                        break;
                    }

                    /* split the difference */
                    linenum = lo + (hi - lo)/2;

                    /* is this what we're looking for? */
                    if (matchp >= lines[linenum]
                        && matchp < lines[linenum + 1])
                    {
                        /* got it */
                        break;
                    }
                    else if (matchp < lines[linenum])
                    {
                        /* too high - move down */
                        hi = (linenum == hi ? hi - 1 : linenum);
                    }
                    else
                    {
                        /* too low - move up */
                        lo = (linenum == lo ? lo + 1 : linenum);
                    }
                }

                /* get the line text for reporting */
                linetxt = lines[linenum];
                linelen = lines[linenum+1] - lines[linenum];

                /* if in regex mode, get the line text as ansi */
                if (regex)
                {
                    linetxt = ansi_from_utf8(linetxt, linelen);
                    linelen = strlen(linetxt);
                }

                /* report it */
                cont = report_match(fname, linenum + 1, matches,
                                    linetxt, linelen);

                /* free the ansi buffer if necessary */
                if (regex)
                    th_free(linetxt);

                /*
                 *   if the callback doesn't want us to continue looking for
                 *   other matches, break out of the file scan loop
                 */
                if (!cont)
                    break;

                /* move on to the next line */
                if (linenum < linecnt)
                    src = lines[linenum + 1];
                else
                    break;
            }
            else
            {
                /* no more matches - no need to keep looking */
                break;
            }
        }

        /* done with the buffer and line-start array */
        th_free(buf);
        th_free(lines);

        /* return the match count */
        return matches;
    }

    /*
     *   Search a file line-by-line.  This loads one line at a time into
     *   memory, and searches each line individually.  This version doesn't
     *   take a lot of memory, but can't search across multiple lines.
     */
    int search_line_by_line(const textchar_t *fname)
    {
        FILE *fp;
        textchar_t buf[32768];
        int linenum;
        int matches = 0;

        /* open the file */
        if ((fp = fopen(fname, "r")) == 0)
        {
            report_file_error(fname, "unable to open file");
            return -1;
        }

        /* scan the file */
        for (linenum = 1 ; ; ++linenum)
        {
            char *p;
            int match;
            int ofs;

            /* get the next line; stop at eof */
            if (fgets(buf, sizeof(buf), fp) == 0)
                break;

            /* remove trailing newlines */
            for (p = buf + strlen(buf) ;
                 p > buf && (*(p-1) == '\n' || *(p-1) == '\r') ;
                 *--p = '\0') ;

            /* search this line for the pattern */
            if (regex)
            {
                char *utf8;
                CRegexSearcher searcher;
                re_group_register regs[RE_GROUP_REG_CNT];
                int len;
                int matchlen;
                int matchofs;

                /* set our default case sensitivity for the expression */
                searcher.set_default_case_sensitive(exact_case);

                /* convert the input to utf8 */
                utf8 = utf8_from_ansi(buf, p - buf);
                len = get_strlen(utf8);

                /* search for the pattern */
                for (match = FALSE, ofs = 0 ; ofs < len ; )
                {
                    /* try finding from the current offset */
                    matchofs = searcher.search_for_pattern(
                        rexpat, utf8, utf8 + ofs, len - ofs, &matchlen, regs);

                    /* check for a match */
                    match = (matchofs >= 0);

                    /* if we didn't find a raw match, give up */
                    if (!match)
                        break;

                    /* if we're in word match mode, check for a word match */
                    char *matchp = utf8 + ofs + matchofs;
                    if (whole_word
                        && ((ofs + matchofs != 0
                             && is_word_char(
                                 utf8_prevchar(matchp)))
                            || (ofs + matchofs + matchlen < len
                                && is_word_char(
                                    utf8_ptr::s_getch(
                                        matchp + matchlen)))))
                    {
                        /* we don't match after all */
                        match = FALSE;

                        /* move to the next character */
                        ofs += utf8_ptr::s_charsize(utf8[ofs]);

                        /* keep looking */
                        continue;
                    }

                    /* got a match - stop looking */
                    break;
                }

                /* done with the utf8 */
                th_free(utf8);
            }
            else
            {
                const char *p;
                int len = strlen(buf);
                int qlen = strlen(query);

                for (match = FALSE, ofs = 0 ; ofs < len ; ++ofs)
                {
                    /*
                     *   look for a match from here, with or without case
                     *   sensitivity as directed
                     */
                    p = (exact_case
                         ? strstr(buf + ofs, query)
                         : stristr(buf + ofs, query));

                    /* we matched if we found the substring */
                    match = (p != 0);

                    /* if we didn't find a raw match, give up */
                    if (!match)
                        break;

                    /* check for whole-word matching if needed */
                    if (whole_word
                        && ((p > buf
                             && is_word_char(*(p-1)))
                            || is_word_char(p[qlen])))
                    {
                        /* it's not a match after all */
                        match = FALSE;

                        /* move on to the next character */
                        ++ofs;

                        /* keep looking */
                        continue;
                    }

                    /* got a match - we can stop looking */
                    break;
                }
            }

            /* if we found a match, report it */
            if (match)
            {
                /* count it */
                ++matches;

                /*
                 *   report it; if the report callback doesn't want us to
                 *   continue looking for other matches, break out of the
                 *   file scan loop
                 */
                if (!report_match(fname, linenum, matches, buf, strlen(buf)))
                    break;
            }
        }

        /* done with the file */
        fclose(fp);

        /* return the number of matches we found */
        return matches;
    }

    /*
     *   is the given character a word character, for the purposes of
     *   whole-word matches?
     */
    static int is_word_char(int c)
    {
        return (c < 127 && (isalpha((char)c) || isdigit((char)c)));
    }

    /* our query parameters */
    const textchar_t *query;
    int exact_case;
    int regex;
    int whole_word;
    int collapse_sp;

    /* our compiled regular expression, and the compile status */
    CRegexParser rexprs;
    re_compiled_pattern *rexpat;
    re_status_t rexstat;
};


/* ------------------------------------------------------------------------ */
/*
 *   Find/Replace dialog owner callback interface
 */

/* is Find allowed in the current context? */
int CHtmlSys_dbgmain::itfdo_allow_find(int regex)
{
    CHtmlDbgSubWin *win;

    /* if there's an active window, ask it about the Find */
    if ((win = get_last_cmd_target_win()) != 0)
        return win->active_allow_find(regex);

    /* we can't do the Find */
    return FALSE;

}

/* is Replace allowed in the current context? */
int CHtmlSys_dbgmain::itfdo_allow_replace()
{
    CHtmlDbgSubWin *win;

    /* if there's an active window, ask it about the Replace */
    if ((win = get_last_cmd_target_win()) != 0)
        return win->active_allow_replace();

    /* we can't do the Find */
    return FALSE;
}

/* note that we're starting a new search */
void CHtmlSys_dbgmain::itfdo_new_search()
{
    /* forget any previous match */
    find_start_.set_win(0);
    find_last_.set_win(0);
}

/*
 *   General handler for Find, Replace, and Replace All.  If the replace text
 *   'repl' is null, we just do a find, otherwise we do a replace.  If
 *   'repl_all' is true, we replace all remaining occurrences of the text,
 *   otherwise we just replace the current occurrence and find the next one.
 *
 *   Returns the number of matches found and/or replaced.
 */
int CHtmlSys_dbgmain::find_and_replace(
    const textchar_t *txt, const textchar_t *repl, int repl_all,
    int exact_case, int wrap, int direction, int regex, int whole_word,
    itfdo_scope scope, IDebugSysWin **found_win)
{
    CHtmlDbgSubWin *orig_win;
    CHtmlDbgSubWin *win;
    int repl_cnt = 0;
    IDebugSysWin *new_win;

    /* if there's an active window, tell it to do the find */
    if ((orig_win = win = get_last_cmd_target_win()) != 0)
    {
        int looped = FALSE;
        int opened_win = FALSE;
        int switched_win = FALSE;
        CHtmlFormatter *fmt;
        unsigned long oldsela, oldselb;
        int started_undo_group = FALSE;

        /* go until we find a match or run out of continuation options */
        for (;;)
        {
            unsigned long starta, startb;
            textchar_t fname[OSFNMAX];

            /*
             *   If the user has manually changed the selection since the
             *   last Find, forget the starting Find location - the manual
             *   cursor movement cancels the continuity of a repeated Find.
             *   Ignore this if we just switched to a new window.
             */
            if (!switched_win && !find_last_.is_current(win))
                find_start_.set_win(0);

            /*
             *   if we're doing a Replace operation, and the selection range
             *   in the window matches the last Find, replace the current
             *   selection
             */
            if (repl != 0 && find_last_.is_current(win))
            {
                /*
                 *   if this is a Replace All, start an undo group in the
                 *   window if we haven't already
                 */
                if (repl_all && !started_undo_group)
                {
                    win->active_undo_group(TRUE);
                    started_undo_group = TRUE;
                }

                /* replace the selection */
                win->active_replace_sel(repl, direction, regex);

                /* count the replacement */
                ++repl_cnt;
            }

            /*
             *   If we're searching in the same window as the start of the
             *   search, tell the window the wrap-around location.
             */
            if (find_start_.win == win)
            {
                starta = find_start_.get_a_pos();
                startb = find_start_.get_b_pos();
            }
            else
                starta = startb = ~0U;

            /*
             *   Look for the next match in the current file.  Note that if
             *   we're doing a project-wide search, the 'wrap' option applies
             *   to the list of files, not within each individual file.
             */
            if (win->active_find_text(
                txt, exact_case, wrap && scope == ITFDO_SCOPE_FILE,
                direction, regex, whole_word, starta, startb))
            {
                /* we found a match - note the location if it's the first */
                if (find_start_.win == 0)
                    find_start_.load_range(win);

                /* note the location of the last match */
                find_last_.load_range(win);

                /*
                 *   if we're doing a Replace All, simply loop back
                 *   immediately and replace the match
                 */
                if (repl_all)
                    continue;

                /* check to see if we switched to a new window */
                if (switched_win)
                {
                    /* make the new window active */
                    win->active_to_front();

                    /* ...but make sure the dialog stays activated */
                    BringWindowToTop(find_dlg_->get_handle());

                    /* tell the caller about the new window, if desired */
                    if (found_win != 0)
                        *found_win = new_win;
                }

                /*
                 *   indicate success - for Find or Replace, we always return
                 *   on the first successful match, so our match count is
                 *   always 1 on success
                 */
                return 1;
            }

            /*
             *   The search failed.  We'll now either just return failure, if
             *   this is a single-file search, or we'll move on to the next
             *   file in a multi-file search.
             */

            /*
             *   if we just switched to this window, restore the original
             *   selection in the window
             */
            if (switched_win)
            {
                /* restore the original selection */
                win->set_sel_range(oldsela, oldselb);

                /*
                 *   To avoid indefinite looping, if we never set a starting
                 *   location, use the current location as the starting
                 *   location.  This is particularly important for a Replace
                 *   All that won't find any matches, because otherwise we'd
                 *   never set a starting point.
                 */
                if (find_start_.win == 0)
                    find_start_.load_range(win);
            }

            /*
             *   If this is a one-file search, we've searched everywhere
             *   without success, so we can stop and return failure.  If it's
             *   an all-file search, but the last file we opened looped back
             *   to the first file of the search, we're also done, since
             *   we've now looped through the whole project.
             */
            if (scope == ITFDO_SCOPE_FILE || looped)
                break;

            /*
             *   The search of this file failed, and this is a multi-file
             *   search, so proceed to the next file.  Keep going until we
             *   find (a) a file containing the search text or (b) a file
             *   that's already open in a window.
             */
            const textchar_t *curname = win->active_get_filename();
            textchar_t firstname[OSFNMAX] = "";
            int found_file = FALSE;
            for (;;)
            {
                /* get the name of the next file in line */
                if (!get_next_search_file(
                    fname, sizeof(fname), curname, direction, wrap,
                    scope == ITFDO_SCOPE_LOCAL))
                    break;

                /*
                 *   Note the first file we find in our loop.  If we already
                 *   have an earlier file, make sure we haven't come back to
                 *   it - if we have, we've looped, so we won't find what
                 *   we're looking for.
                 */
                if (firstname[0] == '\0')
                    safe_strcpy(firstname, sizeof(firstname), fname);
                else if (strcmp(firstname, fname) == 0)
                    break;

                /*
                 *   look for an existing window on the file; if there's
                 *   already a window, just go search the window directly
                 */
                if (helper_->find_text_file(this, fname, fname) != 0)
                {
                    found_file = TRUE;
                    break;
                }

                /*
                 *   We don't have a window on the file yet.  Before we open
                 *   the window, look for a match in the text file.  That way
                 *   we won't clutter the UI by opening a bunch of windows
                 *   that don't have any results to show.
                 */
                FileSearcher fs(txt, exact_case, regex, whole_word, FALSE);
                if (fs.search_file(fname) > 0)
                {
                    /* found a match - go open this window */
                    found_file = TRUE;
                    break;
                }

                /* continue our project scan from this file */
                curname = fname;
            }

            /* if we didn't find another file to search, give up */
            if (!found_file)
                break;

            /*
             *   If we started an undo group in the current window, close the
             *   group.
             */
            if (started_undo_group)
            {
                win->active_undo_group(FALSE);
                started_undo_group = FALSE;
            }

            /* get the window */
            new_win = (IDebugSysWin *)helper_->find_text_file(
                this, fname, fname);

            /* if it's not already open, open it */
            if (new_win == 0)
            {
                /* open the window */
                new_win = open_text_file(fname);

                /* note that we opened a new window */
                opened_win = TRUE;
            }

            /*
             *   if there's no window, or it's not convertible to our
             *   required interface, give up
             */
            if (new_win == 0 || (win = new_win->idsw_as_dbgsubwin()) == 0)
                break;

            /* remember the original selection range in the window */
            win->get_sel_range(&fmt, &oldsela, &oldselb);

            /* go to the top or bottom of the file (per direction) */
            if (direction > 0)
                win->set_sel_range(0, 0);
            else
                win->set_sel_range(~0U, ~0U);

            /* if this is the starting window, we've looped */
            if (win == find_start_.win)
                looped = TRUE;

            /* note that we switched to a new window */
            switched_win = TRUE;
        }

        /*
         *   If we make it out of the loop, it means that we've failed to
         *   find a match and are giving up.
         *
         *   First, reset the search memory - if they search again, it starts
         *   a fresh search.
         */
        find_start_.set_win(0);
        find_last_.set_win(0);

        /* if there's an active undo group, terminate it */
        if (started_undo_group)
        {
            win->active_undo_group(FALSE);
            started_undo_group = FALSE;
        }

        /*
         *   if we were doing a Replace All, return the replacement count;
         *   otherwise just return a zero match count to indicate our failure
         *   to find another match
         */
        return (repl_all ? repl_cnt : 0);
    }

    /* we couldn't find a window to search, so just return failure */
    return 0;
}

/* find the given text */
int CHtmlSys_dbgmain::itfdo_find_text(
    const textchar_t *txt,
    int exact_case, int wrap, int direction, int regex, int whole_word,
    itfdo_scope scope)
{
    return find_and_replace(txt, 0, FALSE, exact_case, wrap, direction,
                            regex, whole_word, scope, 0);
}

/* replace the current Find selection */
int CHtmlSys_dbgmain::itfdo_replace_sel(
    const textchar_t *txt, const textchar_t *repl,
    int exact_case, int wrap, int direction, int regex, int whole_word,
    itfdo_scope scope)
{
    return find_and_replace(txt, repl, FALSE, exact_case, wrap, direction,
                            regex, whole_word, scope, 0);
}

/* replace all remaining Find selections */
int CHtmlSys_dbgmain::itfdo_replace_all(
    const textchar_t *txt, const textchar_t *repl,
    int exact_case, int wrap, int direction, int regex, int whole_word,
    itfdo_scope scope)
{
    return find_and_replace(txt, repl, TRUE, exact_case, wrap, direction,
                            regex, whole_word, scope, 0);
}

/* save the dialog position in the configuration file */
void CHtmlSys_dbgmain::itfdo_save_pos(const CHtmlRect *rc)
{
    /* save the position both locally and globally */
    local_config_->setval("find dialog", "pos", rc);
    global_config_->setval("find dialog", "pos", rc);
}

/* load the dialog position from the configuration file */
int CHtmlSys_dbgmain::itfdo_load_pos(CHtmlRect *rc)
{
    /* try loading first from the local config, then from the global config */
    return !local_config_->getval("find dialog", "pos", rc)
        || !global_config_->getval("find dialog", "pos", rc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Check command status
 */
TadsCmdStat_t CHtmlSys_dbgmain::check_command(const check_cmd_info *info)
{
    CHtmlDbgSubWin *active_win;
    TadsCmdStat_t result;

    /*
     *   the main menu buttons are always enabled - these aren't actual
     *   commands, but are just placeholders to drop down the submenus
     */
    if (info->command_id >= ID_MENUBAR_FIRST
        && info->command_id <= ID_MENUBAR_LAST)
        return TADSCMD_ENABLED;

    /* if it's a source file command, always enable it */
    if (info->command_id >= ID_FILE_OPENLINESRC_FIRST
        && info->command_id <= ID_FILE_OPENLINESRC_LAST)
        return TADSCMD_ENABLED;

    /*
     *   if it's a window command, leave the status unchanged - the window
     *   menu builder will set these up properly
     */
    if (info->command_id >= ID_WINDOW_FIRST
        && info->command_id <= ID_WINDOW_LAST)
        return TADSCMD_DO_NOT_CHANGE;

    /*
     *   if it's in the custom external command range, check to see if it's
     *   our Tools menu; if so, enable it, otherwise leave it for extension
     *   DLLs to decide upon
     */
    if (info->command_id >= ID_CUSTOM_FIRST
        && info->command_id <= ID_CUSTOM_LAST)
    {
        /* try finding it in our Tools menu */
        MENUITEMINFO mii;
        mii.cbSize = menuiteminfo_size_;
        mii.fMask = MIIM_ID;
        if (GetMenuItemInfo(tools_menu_, info->command_id, FALSE, &mii))
            return TADSCMD_ENABLED;
    }

    /* check the command */
    switch(info->command_id)
    {
    case ID_DEBUG_SHOWHIDDENOUTPUT:
        return (!game_loaded_
                ? TADSCMD_DISABLED
                : helper_->get_dbg_hid() ? TADSCMD_CHECKED : TADSCMD_ENABLED);
        break;

    case ID_FILE_OPEN_NONE:
    case ID_WINDOW_NONE:
        /* dummy entry for when there are no source files; always disabled */
        return TADSCMD_DISABLED;

    case ID_DEBUG_EDITBREAKPOINTS:
    case ID_BUILD_SETTINGS:
    case ID_DEBUG_PROGARGS:
    case ID_PROJ_SEARCH:
        /* enabled as long as a workspace is open */
        return workspace_open_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_BUILD_STOP:
        /* we can try to halt a build if a build is running */
        return build_running_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_BUILD_COMPDBG_FULL:
    case ID_BUILD_COMPRLS:
    case ID_BUILD_COMPEXE:
    case ID_BUILD_RELEASEZIP:
    case ID_BUILD_WEBPAGE:
    case ID_BUILD_SRCZIP:
    case ID_BUILD_COMPINST:
    case ID_BUILD_ALLPACKAGES:
    case ID_BUILD_COMP_AND_RUN:
    case ID_BUILD_CLEAN:
    case ID_BUILD_PUBLISH:
    case ID_PROJ_SCAN_INCLUDES:
        /*
         *   these are enabled as long as a workspace is open and we're
         *   not already building
         */
        return !build_running_ && workspace_open_
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_FILE_RELOADGAME:
    case ID_FILE_RESTARTGAME:
    case ID_FILE_TERMINATEGAME:
        /* these are active as long as a game is loaded */
        return !game_loaded_ ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_FILE_QUIT:
    case ID_FILE_OPEN:
    case ID_FILE_NEW:
    case ID_FILE_SAVE_ALL:
    case ID_DEBUG_OPTIONS:
    case ID_TOOLS_NEWIFID:
    case ID_TOOLS_READIFID:
    case ID_TOOLS_EDITEXTERN:
        /* always enabled */
        return TADSCMD_ENABLED;

    case ID_EDIT_POPBOOKMARK:
    case ID_EDIT_JUMPNEXTBOOKMARK:
    case ID_EDIT_JUMPPREVBOOKMARK:
    case ID_EDIT_JUMPNAMEDBOOKMARK:
    case ID_EDIT_CLEARALLBOOKMARKS:
        /* these are enabled as long as there are any bookmarks */
        return bookmarks_ != 0 ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_GAME_RECENT_NONE:
        /* this is a placeholder only - it's always disabled */
        return TADSCMD_DISABLED;

    case ID_FILE_CREATEGAME:
    case ID_FILE_LOADGAME:
    case ID_GAME_RECENT_1:
    case ID_GAME_RECENT_2:
    case ID_GAME_RECENT_3:
    case ID_GAME_RECENT_4:
        /* enabled as long as a build isn't running */
        return build_running_ ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_DEBUG_GO:
    case ID_DEBUG_BUILDGO:
        /*
         *   "Go" works while a workspace is open, and we're not building.
         *   We can use "go" even if the game is already running, in which
         *   case we simply activate the game window.
         */
        return (workspace_open_ && !build_running_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_REPLAY_SESSION:
        /*
         *   "Replay session" works while a worksapce is open, and we're not
         *   building, and there's at least one script.
         */
        return (workspace_open_ && !build_running_
                && scriptwin_ != 0 && scriptwin_->get_script_count() != 0
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
        /*
         *   these only work while the debugger is active, a workspace is
         *   open, and we're not building (we can go, step, etc., even if
         *   a game isn't currently loaded - this will simply reload the
         *   current game)
         */
        return (in_debugger_ && workspace_open_ && !build_running_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_STEPOUT:
        /*
         *   step out only works when a game is loaded and the debugger
         *   has control
         */
        return (in_debugger_ && game_loaded_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_ABORTCMD:
    case ID_DEBUG_SHOWNEXTSTATEMENT:
        /*
         *   these only work when the game is running and the debugger is
         *   active
         */
        return (in_debugger_ && game_loaded_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_SETNEXTSTATEMENT:
    case ID_DEBUG_RUNTOCURSOR:
        /*
         *   These work only while the debugger is active and the game is
         *   loaded and running, but may not always work when the debugger
         *   is active - disallow them here if the debugger is not active;
         *   otherwise allow them for now, but proceed with additional
         *   checks.
         */
        if (!in_debugger_ || !game_loaded_)
            return TADSCMD_DISABLED;
        break;

    case ID_DEBUG_CLEARHISTORYLOG:
        /*
         *   this doesn't work when the debugger isn't active; when the
         *   debugger is active, only allow it when there's a non-empty
         *   call history log
         */
        return (in_debugger_
                && helper_->get_call_trace_len(dbgctx_) != 0
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_DEBUG_HISTORYLOG:
        /*
         *   this works only while the debugger is active, but check it or
         *   not depending on whether tracing is on
         */
        if (!game_loaded_)
            return TADSCMD_DISABLED;
        else if (helper_->is_call_trace_active(dbgctx_))
            return in_debugger_ ? TADSCMD_CHECKED
                                : TADSCMD_DISABLED_CHECKED;
        else
            return in_debugger_ ? TADSCMD_ENABLED
                                : TADSCMD_DISABLED;

    case ID_DEBUG_BREAK:
        /* this only works while the game is active */
        return !game_loaded_ || in_debugger_
            ? TADSCMD_DISABLED : TADSCMD_ENABLED;

    case ID_VIEW_STACK:
    case ID_VIEW_WATCH:
    case ID_VIEW_TRACE:
    case ID_VIEW_LOCALS:
    case ID_VIEW_PROJECT:
    case ID_VIEW_SCRIPT:
    case ID_VIEW_DOCSEARCH:
    case ID_VIEW_FILESEARCH:
    case ID_VIEW_HELPWIN:
    case ID_SHOW_DEBUG_WIN:
    case ID_VIEW_GAMEWIN:
    case ID_HELP_ABOUT:
    case ID_HELP_SEARCH_DOC:
    case ID_HELP_WWWTADSORG:
    case ID_HELP_TOPICS:
    case ID_HELP_TADSMAN:
    case ID_HELP_TUTORIAL:
    case ID_HELP_TADSPRSMAN:
    case ID_HELP_T3LIB:
        return TADSCMD_ENABLED;

    case ID_SEARCH_GO:
        return TADSCMD_ENABLED;

    case ID_SEARCH_DOC:
    case ID_SEARCH_PROJECT:
    case ID_SEARCH_FILE:
        return (info->command_id == searchbarcmd_ ?
                TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_PROFILER_START:
        /*
         *   enable it if a game is loaded, and the profiler isn't already
         *   running
         */
        return (game_loaded_ && !profiling_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROFILER_STOP:
        /* enable it only if the profiler is running */
        return (profiling_ ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_VIEW_STATUSLINE:
        return (show_statusline_ ? TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_VIEW_MENU_TOOLBAR:
        return is_toolbar_visible(MENU_TOOLBAR_ID)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_DEBUG_TOOLBAR:
        return is_toolbar_visible(DEBUG_TOOLBAR_ID)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_SEARCH_TOOLBAR:
        return is_toolbar_visible(SEARCH_TOOLBAR_ID)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_EDIT_TOOLBAR:
        return is_toolbar_visible(EDIT_TOOLBAR_ID)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_VIEW_DOC_TOOLBAR:
        return is_toolbar_visible(DOC_TOOLBAR_ID)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_FLUSH_GAMEWIN:
        /* this only works when the debugger has control */
        return game_loaded_ && in_debugger_
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_FLUSH_GAMEWIN_AUTO:
        /* check or uncheck according to the current setting */
        return auto_flush_gamewin_ ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_FILE_CLOSE:
    case ID_WINDOW_RESTORE:
    case ID_WINDOW_CLOSE_ALL:
    case ID_WINDOW_NEXT:
    case ID_WINDOW_PREV:
    case ID_WINDOW_CASCADE:
    case ID_WINDOW_TILE_HORIZ:
    case ID_WINDOW_TILE_VERT:
        /*
         *   these all work as long as we have one or more children in the
         *   MDI client window, and the active window is visible
         */
        {
            HWND hwnd = (HWND)SendMessage(get_parent_of_children(),
                                          WM_MDIGETACTIVE, 0, 0);

            return (hwnd != 0 && IsWindowVisible(hwnd)
                    ? TADSCMD_ENABLED : TADSCMD_DISABLED);
        }

    default:
        break;
    }

    /*
     *   we didn't handle it -- see if the active debugger window wants to
     *   handle it
     */
    active_win = get_active_dbg_win();
    if (active_win != 0)
    {
        /* run it past the active debugger window */
        result = active_win->active_check_command(info);

        /* if the active window handled the command, return the result */
        if (result != TADSCMD_UNKNOWN)
            return result;
    }

    /* try version-specific handling */
    if ((result = vsn_check_command(info)) != TADSCMD_UNKNOWN)
        return result;

    /* check for some commands after giving the active window a chance */
    switch (info->command_id)
    {
    case ID_FILE_SAVE:
        /* update the menu to remove any past filename indication */
        check_command_change_menu(info, IDS_TPL_SAVE_BLANK);

        /* this is disabled unless the active window allows it */
        return TADSCMD_DISABLED;

    case ID_FILE_SAVE_AS:
        /* update the menu to remove any past filename indication */
        check_command_change_menu(info, IDS_TPL_SAVE_BLANK_AS);

        /* this is disabled unless the active window allows it */
        return TADSCMD_DISABLED;

    case ID_EDIT_FIND:
        /* update the menu to remove any past filename indication */
        check_command_change_menu(info, IDS_TPL_FIND_BLANK);

        /* this is disabled unless the active window allows it */
        return TADSCMD_DISABLED;

    case ID_FILE_PRINT:
    case ID_EDIT_CUT:
    case ID_EDIT_COPY:
    case ID_EDIT_DELETE:
    case ID_EDIT_PASTE:
    case ID_EDIT_PASTEPOP:
    case ID_EDIT_UNDO:
    case ID_EDIT_REDO:
    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_SETNEXTSTATEMENT:
    case ID_HELP_GOBACK:
    case ID_HELP_GOFORWARD:
    case ID_HELP_REFRESH:
        /* these commands are disabled unless the active window wants them */
        return TADSCMD_DISABLED;

    case ID_FILE_PAGE_SETUP:
        /*
         *   these commands are enabled unless the active window explicitly
         *   disables them
         */
        return TADSCMD_ENABLED;

    case ID_FILE_EDIT_TEXT:
        /* this is disabled unless the active window wants it */
        check_command_change_menu(info, IDS_TPL_EDITTEXT_BLANK);
        return TADSCMD_DISABLED;
    }

    /* inherit default behavior */
    return CHtmlSys_framewin::check_command(info);
}

/* callback context for enumerating source windows */
struct winmenu_win_enum_cb_ctx_t
{
    /* number of windows inserted into the menu so far */
    int count_;

    /* the debugger main window object */
    CHtmlSys_dbgmain *dbgmain_;

    /* handle to the Window menu */
    HMENU menuhdl_;

    /* handle of active MDI child window */
    HWND active_;
};

/*
 *   Receive notification that we've closed a menu
 */
int CHtmlSys_dbgmain::menu_close(unsigned int /*item*/)
{
    /*
     *   if we opened the menu from the keyboard while another window was
     *   active, make the original window active again
     */
    if (post_menu_active_win_ != 0)
    {
        BringWindowToTop(post_menu_active_win_);
        post_menu_active_win_ = 0;
    }

    /* return false to proceed with system default handling */
    return FALSE;
}

/*
 *   Initialize a popup menu that's about to be displayed
 */
void CHtmlSys_dbgmain::init_menu_popup(HMENU menuhdl, unsigned int pos,
                                       int sysmenu)
{
    winmenu_win_enum_cb_ctx_t cbctx;
    int i;

    /* if this is the Window menu, initialize it */
    if (!sysmenu && menuhdl == win_menu_hdl_)
    {
        /* erase all of the window entries in the menu */
        for (i = GetMenuItemCount(menuhdl) ; i != 0 ; )
        {
            int id;

            /* move to the previous element */
            --i;

            /* get the item's command ID */
            id = GetMenuItemID(menuhdl, i);

            /* if it refers to a child window, delete it */
            if ((id >= ID_WINDOW_FIRST && id <= ID_WINDOW_LAST)
                || id == ID_WINDOW_NONE)
                DeleteMenu(menuhdl, i, MF_BYPOSITION);
        }

        /* insert all of the open source windows into the menu */
        cbctx.count_ = 0;
        cbctx.dbgmain_ = this;
        cbctx.menuhdl_ = menuhdl;
        cbctx.active_ = (HWND)SendMessage(get_parent_of_children(),
                                          WM_MDIGETACTIVE, 0, 0);
        helper_->enum_source_windows(&winmenu_win_enum_cb, &cbctx);

        /* if we didn't add any window item at all, add a placeholder item */
        if (cbctx.count_ == 0)
        {
            MENUITEMINFO info;
            static const textchar_t *itemname = "No Windows";

            /* set up the new menu item descriptor */
            info.cbSize = menuiteminfo_size_;
            info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
            info.fType = MFT_STRING;
            info.wID = ID_WINDOW_NONE;
            info.dwTypeData = (char *)itemname;
            info.cch = get_strlen(itemname);

            /* insert the new menu item */
            InsertMenuItem(menuhdl, GetMenuItemCount(menuhdl), TRUE, &info);
        }
    }

    /* inherit the default handling now that we've built the menu */
    CHtmlSys_framewin::init_menu_popup(menuhdl, pos, sysmenu);
}

/*
 *   window enumerator: build the Window menu
 */
void CHtmlSys_dbgmain::winmenu_win_enum_cb(void *ctx0, int idx,
                                           IDebugWin *win,
                                           int /*line_source_id*/,
                                           HtmlDbg_wintype_t win_type)
{
    winmenu_win_enum_cb_ctx_t *ctx = (winmenu_win_enum_cb_ctx_t *)ctx0;
    MENUITEMINFO info;
    textchar_t win_title[260];
    char *p;
    int menu_idx;

    /*
     *   ignore everything but regular file windows (the special windows,
     *   such as the stack window and the history window, are already in
     *   the "view" menu)
     */
    if (win_type != HTMLDBG_WINTYPE_SRCFILE)
        return;

    /* if we're on number 1 through 9, include a numeric shortcut */
    if (ctx->count_ + 1 <= 9)
    {
        sprintf(win_title, "&%d ", ctx->count_ + 1);
        p = win_title + get_strlen(win_title);
    }
    else
    {
        /*
         *   don't include a shortcut prefix - put the title at the start
         *   of the buffer
         */
        p = win_title;
    }

    /* get the window's title - use this as the name in the menu */
    GetWindowText(((IDebugSysWin *)win)->idsw_get_frame_handle(),
                  p, sizeof(win_title) - (p - win_title));

    /* set up the new menu item descriptor for the window */
    info.cbSize = menuiteminfo_size_;
    info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
    info.fType = MFT_STRING;
    info.wID = ID_WINDOW_FIRST + idx;
    info.dwTypeData = win_title;
    info.cch = get_strlen(win_title);

    /* insert the new menu item */
    menu_idx = GetMenuItemCount(ctx->menuhdl_),
    InsertMenuItem(ctx->menuhdl_, menu_idx, TRUE, &info);

    /* always enable these items */
    EnableMenuItem(ctx->menuhdl_, menu_idx, MF_BYPOSITION | MF_ENABLED);

    /*
     *   If this is the active window, check it.  Note that the active MDI
     *   window might be a generic container window containing the actual
     *   source window, so we must check to see if the source window handle
     *   is a child of the active MDI window.
     */
    HWND hwnd = ((IDebugSysWin *)win)->idsw_get_handle();
    if (ctx->active_ == hwnd || IsChild(ctx->active_, hwnd))
        CheckMenuItem(ctx->menuhdl_, menu_idx, MF_BYPOSITION | MF_CHECKED);

    /* count it */
    ctx->count_++;
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context for profile data enumerator
 */
struct write_prof_ctx
{
    /* file to which we're writing the data */
    FILE *fp;
};

/*
 *   Write profiling data - enumeration callback
 */
void CHtmlSys_dbgmain::write_prof_data(void *ctx0, const char *func_name,
                                       unsigned long time_direct,
                                       unsigned long time_in_children,
                                       unsigned long call_cnt)
{
    write_prof_ctx *ctx = (write_prof_ctx *)ctx0;

    /* write the data, tab-delimited */
    fprintf(ctx->fp, "%s\t%lu\t%lu\t%lu\n",
            func_name, time_direct, time_in_children, call_cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Workbench "Credits" dialog.  This is a simple subclass of the standard
 *   HTML TADS credits dialog with some superficial customizations.
 */
class CHtmlSys_wbcreditswin: public CHtmlSys_creditswin
{
public:
    CHtmlSys_wbcreditswin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_creditswin(mainwin) { }

protected:
    /* get our window title */
    virtual const char *get_dlg_title() const
        { return "About TADS Workbench"; }

    /* get the program title for the dialog's display text */
    virtual const char *get_prog_title() const
    {
        return "<font size=5><b>TADS Workbench</b></font><br><br>"
            "for Windows 95/98/Me/NT/2000/XP";
    }

    /* get extra library credits to show at the start of the list */
    virtual const char *xtra_lib_credits() const
    {
        return "<b>Integrated Text Editor</b><br>"
            "<i>Scintilla</i><br>"
            "by Neil Hodgson<br><br>"
            "<b>Treaty of Babel Interface</b><br>"
            "by L. Ross Raszewski<br><br>"
            "<b>Diff Engine</b><br>"
            "<i>libmba</i><br>"
            "by Michael B Allen<br><br>"
            "<b>ZIP File Handling</b><br>"
            "<i>XZip</i><br>"
            "by Hans Dietrich, Lucian Wischik, Mark Adler, <i>et. al.</i>"
            "<br><br>";
    }

    /* get extra copyright messages */
    virtual const char *xtra_copyrights() const
    {
        return "This software incorporates the Scintilla text editor, "
            "copyright &copy;1998-2003 by Neil Hodgson "
            "&lt;neilh@scintilla.org&gt;.<br><br>";
    }
};

/*
 *   "About Workbench" dialog.  This is a simple subclass of the standard
 *   HTML TADS "About" dialog with some superficial customizations.
 */
class CHtmlSys_aboutwbwin: public CHtmlSys_abouttadswin
{
public:
    CHtmlSys_aboutwbwin(class CHtmlSys_mainwin *mainwin)
        : CHtmlSys_abouttadswin(mainwin) { }

protected:
    /* get the dialog title */
    virtual const char *get_dlg_title() const
        { return "About TADS Workbench"; }

    /* get the name of the background picture */
    virtual const char *get_bkg_name() const { return "aboutwb.jpg"; }

    /* show the "Credits" dialog */
    virtual void show_credits_dlg()
    {
        /* create and run our Credits dialog subclass */
        (new CHtmlSys_wbcreditswin(mainwin_))->run_dlg(this);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   New File Type dialog
 */

/* list item data structure */
struct nfd_item
{
    nfd_item(int mode_cmd, wb_ftype_info *info)
    {
        this->mode_cmd = mode_cmd;
        icon = info->icon;
        need_destroy_icon = info->need_destroy_icon;
        desc = ansi_from_olestr(info->desc);
        ansi_from_olestr(defext, sizeof(defext), info->defext);
    }

    ~nfd_item()
    {
        /* destroy the icon if necessary */
        if (need_destroy_icon)
            DestroyIcon(icon);

        /* delete the desription */
        th_free(desc);
    }

    /* the command ID of the editor mode selection */
    int mode_cmd;

    /* the icon handle */
    HICON icon;

    /* does this icon need to be explicitly destroyed? */
    int need_destroy_icon;

    /* the type desription */
    textchar_t *desc;

    /* the default extension */
    textchar_t defext[32];
};

/* the dialog class */
class NewFileDialog: public CTadsDialog
{
public:
    /*
     *   Information on the selected file type.  This will be filled in after
     *   the user dismisses the dialog with the OK button.
     */
    struct
    {
        /* the command ID of the mode selection */
        int mode_cmd;

        /* the default extension for the file type */
        textchar_t defext[32];
    } ftype;

    NewFileDialog(CHtmlSys_dbgmain *dm)
    {
        /* set up our image list */
        himl = ImageList_Create(cx, cy, ILC_COLOR24, 10, 10);

        /* remember the main window object */
        dbgmain = dm;

        /* we don't know the selected type yet */
        ftype.mode_cmd = 0;
        ftype.defext[0] = '\0';
    }

    ~NewFileDialog()
    {
    }

    void init()
    {
        HWND lv = GetDlgItem(handle_, IDC_LV_TYPE);
        twb_editor_mode_iter iter(dbgmain);
        ITadsWorkbenchEditorMode *mode;
        int mode_cmd;

        /* center the dialog */
        center_dlg_win(handle_);

        /* disable the OK button until we have a selection */
        EnableWindow(GetDlgItem(handle_, IDOK), FALSE);

        /* set the image list in the list view */
        ListView_SetImageList(lv, himl, LVSIL_NORMAL);

        /* run through our modes and ask about their files types */
        for (mode_cmd = ID_EDIT_LANGMODE, mode = iter.first() ; mode != 0 ;
             mode = iter.next(), ++mode_cmd)
        {
            int i;

            /* ask this mode about each of its file types */
            for (i = 0 ; ; ++i)
            {
                wb_ftype_info info;
                int icon_idx;
                LVITEM lvi;
                char title[128];
                int item_idx;

                /* ask about this file type */
                if (!mode->GetFileTypeInfo(i, &info))
                {
                    /* no more types in this mode - we're done */
                    break;
                }

                /* add the icon to the image list */
                icon_idx = ImageList_ReplaceIcon(himl, -1, info.icon);

                /* add the list entry */
                memset(&lvi, 0, sizeof(lvi));
                lvi.mask = LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT;
                lvi.iItem = 0;
                ansi_from_olestr(title, sizeof(title), info.name);
                lvi.pszText = title;
                lvi.iImage = icon_idx;
                lvi.lParam = (LPARAM)new nfd_item(mode_cmd, &info);
                item_idx = ListView_InsertItem(lv, &lvi);

                /*
                 *   if this is the "TADS source file" item, select it by
                 *   default
                 */
                if (wcsicmp(info.defext, L"t") == 0)
                    ListView_SetItemState(
                        lv, item_idx, LVIS_SELECTED, LVIS_SELECTED);
            }
        }
    }

    void destroy()
    {
        /*
         *   make sure we delete all of our items in the list view, so that
         *   we free up our memory for them
         */
        ListView_DeleteAllItems(GetDlgItem(handle_, IDC_LV_TYPE));

        /* do the normal work */
        CTadsDialog::destroy();
    }

    int do_command(WPARAM id, HWND hwnd)
    {
        HWND lv = GetDlgItem(handle_, IDC_LV_TYPE);
        LVITEM lvi;

        switch (LOWORD(id))
        {
        case IDOK:
            /* OK button - remember the selected item's settings in 'ftype' */
            lvi.iItem = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
            if (lvi.iItem == -1)
                return TRUE;

            /* get the item's information */
            lvi.mask = LVIF_PARAM;
            if (ListView_GetItem(lv, &lvi))
            {
                /* get the nfd_item from the lParam */
                nfd_item *info = (nfd_item *)lvi.lParam;

                /* store the new selected item information */
                ftype.mode_cmd = info->mode_cmd;
                safe_strcpy(ftype.defext, sizeof(ftype.defext), info->defext);

                /* dismiss the dialog */
                EndDialog(handle_, IDOK);
            }

            /* handled */
            return TRUE;
        }

        /* inherit the default */
        return CTadsDialog::do_command(id, hwnd);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        NMLISTVIEW *nmlv;
        NMITEMACTIVATE *nmact;
        int idx;

        switch (ctl)
        {
        case IDC_LV_TYPE:
            switch (nm->code)
            {
            case NM_DBLCLK:
                /* treat a double-click on an item as an OK click */
                nmact = (NMITEMACTIVATE *)nm;
                if (nmact->iItem != -1)
                    PostMessage(handle_, WM_COMMAND, IDOK, 0);
                break;

            case LVN_DELETEITEM:
                /* delete the nfd_item associated with the item */
                nmlv = (NMLISTVIEW *)nm;
                delete (nfd_item *)nmlv->lParam;
                return TRUE;

            case LVN_ITEMCHANGED:
                /*
                 *   enable or disable the OK button based on whether or not
                 *   there's a selected type
                 */
                idx = ListView_GetNextItem(nm->hwndFrom, -1, LVNI_SELECTED);
                EnableWindow(GetDlgItem(handle_, IDOK), idx != -1);
                return TRUE;
            }
            break;
        }

        /* inherit the default */
        return CTadsDialog::do_notify(nm, ctl);
    }

    /* our main window object */
    CHtmlSys_dbgmain *dbgmain;

    /* our image list */
    HIMAGELIST himl;

    /* icon size we want */
    static const int cx = 48, cy = 48;
};

/* ------------------------------------------------------------------------ */
/*
 *   Get the main workbench help file
 */
void CHtmlSys_dbgmain::get_main_help_file_name(textchar_t *buf, size_t buflen)
{
    textchar_t *rootname;

    /* get the executable's filename so we can get its directory */
    GetModuleFileName(CTadsApp::get_app()->get_instance(), buf, buflen);

    /* find the directory part */
    rootname = os_get_root_name(buf);
    size_t rem = buflen - (rootname - buf);

    /* add the help file name */
    safe_strcpy(rootname, rem, "wbcont.htm");

    /* if it's not there, try the doc/wb subdirectory */
    if (GetFileAttributes(buf) == 0xffffffff)
        safe_strcpy(rootname, rem, "doc\\wb\\wbcont.htm");
}

/*
 *   Show the help window and navigate to the given URL
 */
void CHtmlSys_dbgmain::show_help(const textchar_t *url)
{
    IDebugSysWin *win;

    /* create the help window, if it's not already open */
    win = (IDebugSysWin *)helper_->create_help_win(this);

    /* if we couldn't create it, give up */
    if (win == 0)
        return;

    /* show it and bring it to the front */
    ShowWindow(GetParent(win->idsw_get_handle()), SW_SHOW);
    win->idsw_bring_to_front();

    /* if there's a URL, navigate to the page */
    if (url != 0)
    {
        /* cast the window to our implementation type */
        CHtmlSys_dbghelpwin *hwin = (CHtmlSys_dbghelpwin *)win;

        /* navigate to the indicated page */
        hwin->go_to_url(url);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a command
 */
int CHtmlSys_dbgmain::do_command(int notify_code, int command_id, HWND ctl)
{
    CHtmlDbgSubWin *active_win;
    int need_build_on_run_check = TRUE;

    /* get the active debugger window */
    active_win = get_active_dbg_win();

    /* translate SEARCH_GO to the current search mode command */
    if (command_id == ID_SEARCH_GO)
        command_id = searchbarcmd_;

    /* if it's a search command, save the searchbox text in the combo list */
    if (command_id == ID_SEARCH_DOC
        || command_id == ID_SEARCH_FILE
        || command_id == ID_SEARCH_PROJECT)
    {
        char buf[256];

        /* save the current search in the combo box's string list */
        SendMessage(searchbox_, WM_GETTEXT, sizeof(buf), (LPARAM)buf);

        /* if it's not an empty string, save it */
        if (buf[0] != '\0')
        {
            /* look for the string in the current list */
            int idx = SendMessage(searchbox_, CB_FINDSTRINGEXACT,
                                  (WPARAM)-1, (LPARAM)buf);

            /* if the string is already in the list, delete the old copy */
            if (idx != CB_ERR)
                SendMessage(searchbox_, CB_DELETESTRING, idx, 0);

            /* add the string (or add it back) in first place */
            SendMessage(searchbox_, CB_INSERTSTRING, 0, (LPARAM)buf);

            /* if the list is getting too long, discard the oldest string */
            SendMessage(searchbox_, CB_DELETESTRING, 10, 0);

            /*
             *   make sure we leave the text intact if we just deleted the
             *   string that was selected
             */
            SendMessage(searchbox_, WM_SETTEXT, 0, (LPARAM)buf);
        }
    }

    /*
     *   most commands require that the debugger be active; if it's not,
     *   ignore such commands
     */
initial_checks:
    switch(command_id)
    {
    case ID_DOCKWIN_DOCK:
    case ID_DOCKWIN_UNDOCK:
    case ID_DOCKWIN_HIDE:
    case ID_DOCKWIN_DOCKVIEW:
        /*
         *   Docking-window command - if this is due to an accelerator key
         *   (indicated by a null control handle and 'notify_code' == 1),
         *   look for a docking window with focus, and explicitly try sending
         *   it to that window.  These commands must be sent to the docking
         *   window frame for processing, not to the child window, so we must
         *   explicitly reroute them.
         */
        if (ctl == 0 && notify_code == 1)
        {
            CTadsWinDock *dockwin;

            if (mdi_dock_port_ != 0
                && (dockwin = mdi_dock_port_->get_model_frame()->
                              find_window_by_focus()) != 0
                && dockwin->do_command(notify_code, command_id, ctl))
                return TRUE;
        }

        /* otherwise, proceed with our other processing */
        break;

    case ID_HELP_TOPICS:
    case ID_HELP_TADSMAN:
    case ID_HELP_TUTORIAL:
    case ID_HELP_TADSPRSMAN:
    case ID_HELP_T3LIB:
        {
            char buf[256];
            char *rootname;

            /* get the executable's filename so we can get its directory */
            GetModuleFileName(CTadsApp::get_app()->get_instance(),
                              buf, sizeof(buf));

            /* find the directory part */
            rootname = os_get_root_name(buf);

            /* see what we have */
            switch(command_id)
            {
            case ID_HELP_TOPICS:
                /* look for the help file in the executable's directory */
                get_main_help_file_name(buf, sizeof(buf));
                break;

            case ID_HELP_T3LIB:
                strcpy(rootname, "doc\\libref\\index.html");
                break;

            case ID_HELP_TADSMAN:
                strcpy(rootname, w32_tadsman_fname);
                goto look_for_tadsman;

            case ID_HELP_TUTORIAL:
                strcpy(rootname, "doc\\gsg\\index.html");
                goto look_for_tadsman;

            case ID_HELP_TADSPRSMAN:
                strcpy(rootname, "doc\\parser.htm");
                goto look_for_tadsman;

            look_for_tadsman:
                /* try opening the manual file to see if it exists */
                if (GetFileAttributes(buf) == 0xffffffff)
                {
                    /*
                     *   it doesn't exist - show the file explaining what
                     *   to do instead
                     */
                    strcpy(rootname, "getmanuals.htm");
                    if (GetFileAttributes(buf) == 0xffffffff)
                        strcpy(rootname, "doc\\wb\\getmanuals.htm");
                }
                break;
            }

            /* show the help file */
            show_help(buf);
        }
        return TRUE;

    case ID_SEARCH_DOC:
        /* run a documentation search */
        search_doc();
        return TRUE;

    case ID_SEARCH_PROJECT:
        /* run a project search from the search box */
        {
            char buf[256];

            /* get the search box's contents */
            SendMessage(searchbox_, WM_GETTEXT, sizeof(buf), (LPARAM)buf);

            /*
             *   if there's anything there, run the search from the search
             *   box; otherwise show the Find dialog
             */
            if (buf[0] != '\0')
                search_project_files(buf, FALSE, FALSE, FALSE, FALSE);
            else
                search_project_files();
        }

        /* handled */
        return TRUE;

    case ID_PROJ_SEARCH:
        /* run a project search from a dialog */
        search_project_files();
        return TRUE;

    case ID_HELP_ABOUT:
        (new CHtmlSys_aboutwbwin(CHtmlSys_mainwin::get_main_win()))
            ->run_dlg(this);
        return TRUE;

    case ID_HELP_WWWTADSORG:
        ShellExecute(0, 0, "http://www.tads.org", 0, 0, SW_SHOWNORMAL);
        return TRUE;

    case ID_HELP_SEARCH_DOC:
        /*
         *   This menu item doesn't actually conduct a search, but just gets
         *   everything set up to enter a doc search in the query toolbar.
         */
        {
            int idx;
            REBARBANDINFO info;

            /*
             *   First, if the search toolbar is currently hidden, show it.
             *   To do this, get its band's style in the rebar and check for
             *   the 'hidden flag.  Get the toolbar's index in the rebar...
             */
            idx = SendMessage(rebar_, RB_IDTOINDEX, SEARCH_TOOLBAR_ID, 0);

            /* fetch its style setting */
            info.cbSize = rebarbandinfo_size_;
            info.fMask = RBBIM_STYLE;
            SendMessage(rebar_, RB_GETBANDINFO, idx, (LPARAM)&info);

            /* if it's hidden, show it */
            if ((info.fStyle & RBBS_HIDDEN) != 0)
            {
                /* clear the style flag and update the settings */
                info.fStyle &= ~RBBS_HIDDEN;
                SendMessage(rebar_, RB_SETBANDINFO, idx, (LPARAM)&info);
            }

            /* set the search mode to Doc Search */
            set_searchbar_mode(search_mode_from_cmd(ID_SEARCH_DOC));

            /* move keyboard focus to the search box */
            SetFocus(searchbox_);
        }

        /* handled */
        return TRUE;

    case ID_DEBUG_GO:
    case ID_DEBUG_BUILDGO:
    case ID_DEBUG_REPLAY_SESSION:
        /* if a build is running, ignore it */
        if (build_running_)
        {
            Beep(1000, 100);
            return FALSE;
        }
        break;

    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
    case ID_DEBUG_STEPOUT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_CLEARHISTORYLOG:
    case ID_DEBUG_HISTORYLOG:
        /* these commands all require the debugger to be active */
        if (!in_debugger_ || build_running_)
        {
            /* provide some minimal feedback in the form of an alert sound */
            Beep(1000, 100);

            /* ignore it */
            return FALSE;
        }
        break;

    case ID_DEBUG_ABORTCMD:
    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_SHOWNEXTSTATEMENT:
    case ID_DEBUG_SETNEXTSTATEMENT:
        /*
         *   these commands require the debugger to be active and the game
         *   to be running
         */
        if (!in_debugger_ || !game_loaded_)
        {
            /* provide some minimal feedback in the form of an alert sound */
            Beep(1000, 100);

            /* ignore it */
            return FALSE;
        }
        break;
    }

    /*
     *   If it's any sort of GO or STEP command, and the game isn't already
     *   running, check for auto-save and auto-build.
     */
    switch (command_id)
    {
    case ID_DEBUG_GO:
    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
    case ID_DEBUG_STEPOUT:
    case ID_DEBUG_RUNTOCURSOR:
        /*
         *   If we're not already running, check for auto-save/-build.  Don't
         *   do this if we're already running, since these commands will
         *   simply continue execution in that case.
         */
        if (!game_loaded_
            && need_build_on_run_check && !build_on_run(command_id))
            return FALSE;

        /* we're cleared to proceed with the command */
        break;

    case ID_DEBUG_REPLAY_SESSION:
        /*
         *   this command stops the current game if it's in progress and
         *   starts a new one, so check for auto-save/-build whether or not
         *   we're already running
         */
        if (need_build_on_run_check && !build_on_run(command_id))
            return FALSE;

        /* we're cleared to proceed with the command */
        break;

    case ID_DEBUG_BUILDGO:
        /*
         *   BUILD-GO means that we've completed a build that was triggered
         *   implicitly by some kind of GO or STEP command, or explicitly by
         *   a build-and-run command.  The GO command that we're to execute
         *   after the build is complete is in run_after_build_cmd_.
         *
         *   The BUILD-GO command is issued when the build has completed, so
         *   at this point we don't need to check again for auto-save or
         *   auto-build.  We're past those checks now, so we can simply
         *   proceed with the original GO or STEP command as though it were
         *   the command that was actually just issued.
         */
        command_id = run_after_build_cmd_;

        /*
         *   go back and re-apply the initial checks; but this time we can
         *   skip the build-on-run check, as just explained
         */
        need_build_on_run_check = FALSE;
        goto initial_checks;
    }


    /*
     *   Check for a source-file open command
     */
    if (command_id >= ID_FILE_OPENLINESRC_FIRST
        && command_id <= ID_FILE_OPENLINESRC_LAST)
    {
        IDebugSysWin *win;

        /* open the line source and bring it to the front */
        win = (IDebugSysWin *)helper_
              ->open_line_source(this,
                                 command_id - ID_FILE_OPENLINESRC_FIRST);
        if (win != 0)
            win->idsw_bring_to_front();

        /* handled */
        return TRUE;
    }

    /*
     *   Check for Window commands
     */
    if (command_id >= ID_WINDOW_FIRST
        && command_id <= ID_WINDOW_LAST)
    {
        IDebugSysWin *win;

        /* get the correct window */
        win = (IDebugSysWin *)helper_
              ->get_source_window_by_index(command_id - ID_WINDOW_FIRST);

        /* bring it to the front */
        if (win != 0)
            win->idsw_bring_to_front();

        /* handled */
        return TRUE;
    }

    /* check the command */
    switch(command_id)
    {
    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_ADDTOWATCH:
        /*
         *   if the active window wants to handle it, let it; otherwise,
         *   provide a default implementation here
         */
        if (active_win != 0
            && active_win->active_do_command(notify_code, command_id, ctl))
        {
            /* it handled it */
            return TRUE;
        }
        else
        {
            CStringBuf buf;

            /*
             *   get the selected text; if the selection range is empty,
             *   extend to the expression boundaries; if the selection
             *   range is large, don't bother, since these commands are
             *   generally only useful with relatively small selections
             */
            if (active_win != 0)
            {
                textchar_t *p;
                int all_spaces;

                /* get the text */
                active_win->get_sel_text(&buf, DSW_EXPR_EXTEND, 256);

                /* convert any control characters in the text to spaces */
                for (p = buf.get(), all_spaces = TRUE ;
                     p != 0 && *p != '\0' ; ++p)
                {
                    /* if it's not printable, convert it to a space */
                    if (!isprint(*p))
                        *p = ' ';

                    /* if it's not a space, so note */
                    if (!isspace(*p))
                        all_spaces = FALSE;
                }

                /*
                 *   if the buffer is nothing but spaces, consider it to be
                 *   blank
                 */
                if (all_spaces)
                    buf.set("");
            }

            switch(command_id)
            {
            case ID_DEBUG_EVALUATE:
                /* run the dialog */
                CHtmlDbgWatchWinDlg::run_as_dialog(this, helper_, buf.get());
                break;

            case ID_DEBUG_ADDTOWATCH:
                add_watch_expr(buf.get());
                break;
            }
        }
        return TRUE;

    case ID_DEBUG_BREAK:
        /* force debugger entry */
        abort_game_command_entry();
        return TRUE;

    case ID_FILE_TERMINATEGAME:
        /* terminate the game */
        terminate_current_game();

        /* handled */
        return TRUE;

    case ID_GAME_RECENT_NONE:
        /* this is a placeholder only - ignore it */
        return TRUE;

    case ID_FILE_CREATEGAME:
    case ID_FILE_LOADGAME:
    case ID_FILE_RELOADGAME:
    case ID_GAME_RECENT_1:
    case ID_GAME_RECENT_2:
    case ID_GAME_RECENT_3:
    case ID_GAME_RECENT_4:
        /* can't do these if a compile is running */
        if (build_running_)
            return TRUE;

        /* if a game is already active, warn about it */
        if (get_game_loaded()
            && get_global_config_int("ask load dlg", 0, TRUE))
        {
            switch(MessageBox(handle_, "This will end the current debugging "
                              "session.  Are you sure you want to proceed?",
                              "TADS Workbench",
                              MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2))
            {
            case IDYES:
            case 0:
                /* proceed */
                break;

            default:
                /* ignore the request */
                return TRUE;
            }
        }

        /* if a game is currently running, terminate it */
        if (game_loaded_)
        {
            /* terminate the game */
            terminate_current_game();

            /*
             *   set the pending build command, to be executed when the game
             *   exits, then return so that the game can terminate
             */
            set_pending_build_cmd(command_id);
            return TRUE;
        }

        {
            /* ask about any unsaved changes */
            ask_save_ctx asctx(DLG_ASK_SAVE_MULTI);
            helper_->enum_source_windows(&enum_win_ask_save_cb, &asctx);

            /* if they decided to cancel, we're done */
            if (asctx.cancel)
                return TRUE;
        }

        /* ask for the new game file if appropriate */
        if (command_id == ID_FILE_LOADGAME)
        {
            char fname[OSFNMAX];

            /* ask for a game to load, then load it */
            if (ask_load_game(handle_, fname, sizeof(fname)))
            {
                /* validate the type of the file we're opening */
                if (vsn_validate_project_file(fname))
                {
                    /* handled (or failed) - we're done */
                    return TRUE;
                }

                /* set up to load the new game */
                load_game(fname);
            }

            /* handled */
            return TRUE;
        }
        else if (command_id == ID_FILE_CREATEGAME)
        {
            /* run the new game wizard */
            run_new_game_wiz(handle_, this);

            /* handled */
            return TRUE;
        }
        else if (command_id >= ID_GAME_RECENT_1
                 && command_id <= ID_GAME_RECENT_4)
        {
            /* load the given recent game */
            load_recent_game(command_id);
        }
        else
        {
            /*
             *   They want to reload the current game - set up the reload
             *   name with the name of the current configuration file.
             */
            load_game(local_config_filename_.get());
        }

        /* handled */
        return TRUE;

    case ID_FILE_OPEN:
        {
            OPENFILENAME5 info;
            char fname[256];
            IDebugSysWin *win;
            CHtmlSys_dbgsrcwin *srcwin;
            CStringBuf filter;

            /* build the filter list */
            build_text_file_filter(&filter, FALSE);

            /* ask for a file to open */
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = filter.get();
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            fname[0] = '\0';
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            info.lpstrTitle = "Select a text file";
            info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                         | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
            if (!GetOpenFileName((OPENFILENAME *)&info))
                return TRUE;

            /*
             *   save the new directory as the initial directory for the
             *   next file open operation -- we do this in lieu of
             *   allowing the dialog to change the current directory,
             *   since we need to stay in the initial game directory at
             *   all times (since the game incorporates relative filename
             *   paths for source files)
             */
            CTadsApp::get_app()->set_openfile_dir(fname);

            /* open the file */
            win = (IDebugSysWin *)helper_->open_text_file(this, fname);
            if (win != 0 && (srcwin = win->idsw_as_srcwin()) != 0)
            {
                /* bring it to the front */
                win->idsw_bring_to_front();

                /* if we're in read-only mode, tell the window */
                if ((info.Flags & OFN_READONLY) != 0)
                    srcwin->set_readonly(TRUE);
            }
        }
        return TRUE;

    case ID_FILE_SAVE_ALL:
        /* enumerate all child windows through the 'save file' callback */
        helper_->enum_source_windows(&enum_win_savefile_cb, this);
        return TRUE;

    case ID_FILE_PAGE_SETUP:
        /* run the page-setup dialog */
        {
            PAGESETUPDLG psd;

            memset(&psd, 0, sizeof(psd));
            psd.lStructSize = sizeof(psd);
            psd.hwndOwner = handle_;
            psd.hDevMode = print_devmode_;
            psd.hDevNames = print_devnames_;
            psd.Flags = (print_psd_flags_
                         & (PSD_INHUNDREDTHSOFMILLIMETERS
                            | PSD_INTHOUSANDTHSOFINCHES));
            if (print_margins_set_)
            {
                psd.Flags |= PSD_MARGINS;
                psd.rtMargin = print_margins_;
            }
            if (PageSetupDlg(&psd))
            {
                print_devmode_ = psd.hDevMode;
                print_devnames_ = psd.hDevNames;
                print_margins_ = psd.rtMargin;
                print_margins_set_ = TRUE;
                print_psd_flags_ = (psd.Flags
                                    & (PSD_INHUNDREDTHSOFMILLIMETERS
                                       | PSD_INTHOUSANDTHSOFINCHES));
            }
        }
        return TRUE;

    case ID_FILE_NEW:
        /* open a new file */
        {
            NewFileDialog dlg(this);

            /* run the "new file type" dialog */
            if (dlg.run_modal(DLG_NEWFILETYPE, handle_,
                              CTadsApp::get_app()->get_instance()) == IDOK)
            {
                /* create the file */
                new_text_file(dlg.ftype.mode_cmd, dlg.ftype.defext, 0);
            }
        }
        return TRUE;

    case ID_DEBUG_SHOWNEXTSTATEMENT:
        if (get_game_loaded())
        {
            IDebugSysWin *win;

            /* tell the helper to activate the top item on the stack */
            win = (IDebugSysWin *)helper_->activate_stack_win_line(
                0, dbgctx_, this);

            /* update expressions */
            update_expr_windows();

            /* bring the window to the front */
            if (win != 0)
                win->idsw_bring_to_front();
        }
        return TRUE;

    case ID_DEBUG_SHOWHIDDENOUTPUT:
        helper_->toggle_dbg_hid();
        return TRUE;

    case ID_FILE_QUIT:
        /* make sure they really want to quit */
        ask_quit();
        return TRUE;

    case ID_FILE_RESTARTGAME:
        /*
         *   If a session is already running, terminate the current session,
         *   and post ourselves another RESTARTGAME command so that we'll try
         *   again after the session ends.  Otherwise, just treat this as a
         *   GO.
         */
        if (game_loaded_)
        {
            /* terminate the game */
            terminate_current_game();

            /* set Restart as a pending command for after the game exits */
            set_pending_build_cmd(ID_FILE_RESTARTGAME);
        }
        else
        {
            /* we're not already running, so this is equivalent to GO */
            PostMessage(handle_, WM_COMMAND, ID_DEBUG_GO, 0);
        }

        /* handled */
        return TRUE;

    case ID_DEBUG_OPTIONS:
        /* run the options dialog */
        run_dbg_prefs_dlg(handle_, this);
        return TRUE;

    case ID_TOOLS_EDITEXTERN:
        /* run the options dialog, going straight to the extern tools page */
        run_dbg_prefs_dlg(handle_, this, DLG_DEBUG_EXTERNTOOLS);
        return TRUE;

    case ID_TOOLS_READIFID:
        {
            OPENFILENAME5 info;
            char fname[OSFNMAX];

            /* ask for a filename for dumping the profiler data */
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "TADS Games\0*.t3;*.gam\0All Files\0*.*\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            fname[0] = '\0';
            info.lpstrFile = fname;
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrTitle = "Select a TADS Game";
            info.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST
                         | OFN_ENABLESIZING;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = 0;
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
            if (GetOpenFileName((OPENFILENAME *)&info))
            {
                char ifid[128];
                char buf[512];

                /* read the IFID from the file */
                babel_read_ifid(fname, ifid, sizeof(ifid));

                /* set up a new file with the information */
                sprintf(buf, "/* IFID for %s */\r\nIFID = '%s'\r\n",
                        fname, ifid);
                new_text_file(buf);
            }
        }
        return TRUE;

    case ID_TOOLS_NEWIFID:
        {
            char tbuf[30], dbuf[30];
            char seed[128];
            char ifid[128];
            char buf[2048];

            /* generate an IFID */
            sprintf(seed, "%s%s%lx%lx%lx%lx",
                    _strtime(tbuf), _strdate(dbuf),
                    GetTickCount(), (long)&buf,
                    GetCurrentProcessId(), GetCurrentThreadId());
            tads_generate_ifid(ifid, sizeof(ifid), seed);

            /* build the full text */
            sprintf(buf,
                    "/*\r\n"
                    " *   An IFID is a large random number that's used "
                    "to uniquely identify\r\n"
                    " *   a work of interactive fiction. Refer to the "
                    "GameInfo documentation\r\n"
                    " *   for details. The IFID below was generated "
                    "randomly for your use.\r\n"
                    " *\r\n"
                    "%s"
                    " */\r\n"
                    "%s '%s'\r\n",
                    w32_ifid_instructions, w32_ifid_prefix, ifid);

            /* create a new file to hold the text */
            new_text_file(buf);
        }
        return TRUE;

    case ID_BUILD_SETTINGS:
        /* run the build settings dialog if there's a workspace open */
        if (workspace_open_)
            run_dbg_build_dlg(handle_, this, 0, 0);
        return TRUE;

    case ID_DEBUG_PROGARGS:
        /* run the arguments dialog if there's a workspace open */
        if (workspace_open_)
            vsn_run_dbg_args_dlg();
        return TRUE;

    case ID_BUILD_STOP:
        /* if a build is running, terminate it */
        if (build_running_)
            TerminateProcess(compiler_hproc_, (UINT)-1);

        /* handled */
        return TRUE;

    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMP_AND_RUN:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_BUILD_COMPDBG_FULL:
    case ID_BUILD_COMPRLS:
    case ID_BUILD_COMPEXE:
    case ID_BUILD_RELEASEZIP:
    case ID_BUILD_WEBPAGE:
    case ID_BUILD_SRCZIP:
    case ID_BUILD_COMPINST:
    case ID_BUILD_ALLPACKAGES:
    case ID_BUILD_CLEAN:
    case ID_PROJ_SCAN_INCLUDES:
        /* start the given type of build; there's no run afterwards */
        start_build(command_id);
        return TRUE;

#ifdef NOFEATURE_PUBLISH
    case ID_BUILD_PUBLISH:
        do_publish();
        return TRUE;
#endif

    case ID_DEBUG_ABORTCMD:
        /* set the "abort" flag */
        aborting_cmd_ = TRUE;

        /* exit the debugger event loop, so that we can signal the abort */
        go_ = TRUE;
        return TRUE;

    case ID_DEBUG_REPLAY_SESSION:
        /* replay the latest script */
        if (scriptwin_ != 0)
            scriptwin_->run_latest_script();
        break;

    case ID_DEBUG_GO:
        /* ignore this if a build is running */
        if (build_running_)
            return TRUE;

        /* if the program is already running, just show the game window */
        if (!in_debugger_)
        {
            /* bring the game window and/or web UI to the front */
            CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();
            if (mainwin != 0 && !mainwin->is_webui_game())
                CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();
            w32_webui_to_foreground();

            /* handled */
            return TRUE;
        }

        /* set the new execution state */
        helper_->set_exec_state_go(dbgctx_);

    continue_execution:
        /*
         *   If we the game isn't currently loaded, we need to load the
         *   game in order to begin execution.
         */
        if (!game_loaded_)
        {
            /* get the main game window */
            CHtmlSys_mainwin *gamewin = CHtmlSys_mainwin::get_main_win();

            /*
             *   presume this won't be a Web UI game - we'll revisit this
             *   after loading the game and set the flag at that point if
             *   it's a TADS game that links the tads-net functions
             */
            gamewin->set_webui_game(FALSE);

            /*
             *   Save the project file configuration - do this just in case
             *   Workbench should do something unexpected (like crash) while
             *   we're running the game.  First, save the current settings to
             *   our in-memory configuration object, then save the
             *   configuration object to the disk file.
             */
            save_config();
            vsn_save_config_file(local_config_, local_config_filename_.get());

            /*
             *   set the RESTART and RELOAD flags to indicate that we need
             *   to load the game and start execution
             */
            restarting_ = TRUE;
            reloading_ = TRUE;

            /* we're loading the same game whose workspace is already open */
            reloading_new_ = FALSE;

            /*
             *   if the command was "go", start running as soon as we
             *   reload the game; otherwise, stop at the first instruction
             */
            reloading_go_ = (command_id == ID_DEBUG_GO);

            /* begin execution */
            reloading_exec_ = TRUE;

            /* set the game to reload */
            if (gamewin != 0)
            {
                /* set the pending game */
                gamewin->set_pending_new_game(game_filename_.get());

                /*
                 *   show the main window (do this immediately, so that the
                 *   window is visible while we're setting it up initially -
                 *   this is sometimes important to get correct sizing for
                 *   the toolbars and so on)
                 */
                gamewin->show_normal();
            }
        }

        /* tell the event loop to relinquish control */
        go_ = TRUE;

        /* handled */
        return TRUE;

    case ID_DEBUG_STEPINTO:
        /* ignore if a build is running */
        if (build_running_)
            return TRUE;

        /* set new execution state */
        helper_->set_exec_state_step_into(dbgctx_);

        /* resume execution of the game */
        goto continue_execution;

    case ID_DEBUG_STEPOVER:
        /* ignore if a build is running */
        if (build_running_)
            return TRUE;

        /* set the new execution state */
        helper_->set_exec_state_step_over(dbgctx_);

        /* resume execution of the game */
        goto continue_execution;

    case ID_DEBUG_STEPOUT:
        /* set the new execution state */
        helper_->set_exec_state_step_out(dbgctx_);

        /* done with debugging for now */
        go_ = TRUE;
        return TRUE;

    case ID_VIEW_MENU_TOOLBAR:
        /* invert the menu toolbar status */
        set_toolbar_visible(MENU_TOOLBAR_ID,
                            !is_toolbar_visible(MENU_TOOLBAR_ID));
        break;

    case ID_VIEW_DEBUG_TOOLBAR:
        /* invert the debug toolbar status */
        set_toolbar_visible(DEBUG_TOOLBAR_ID,
                            !is_toolbar_visible(DEBUG_TOOLBAR_ID));
        break;

    case ID_VIEW_SEARCH_TOOLBAR:
        /* invert the search toolbar status */
        set_toolbar_visible(SEARCH_TOOLBAR_ID,
                            !is_toolbar_visible(SEARCH_TOOLBAR_ID));
        break;

    case ID_VIEW_EDIT_TOOLBAR:
        /* invert the edit toolbar status */
        set_toolbar_visible(EDIT_TOOLBAR_ID,
                            !is_toolbar_visible(EDIT_TOOLBAR_ID));
        break;

    case ID_VIEW_DOC_TOOLBAR:
        /* invert the doc toolbar status */
        set_toolbar_visible(DOC_TOOLBAR_ID,
                            !is_toolbar_visible(DOC_TOOLBAR_ID));
        break;

    case ID_VIEW_STACK:
        /* show the stack window */
        {
            /* create and/or show the stack window */
            IDebugSysWin *win = (IDebugSysWin *)helper_->create_stack_win(this);

            /* make sure it's up to date */
            helper_->update_stack_window(dbgctx_, FALSE);

            /* bring it to the front */
            if (win != 0)
                win->idsw_bring_to_front();
        }
        return TRUE;

    case ID_VIEW_DOCSEARCH:
        /* show the doc search window */
        {
            IDebugSysWin *win;

            /* create and/or show the doc search window */
            win = (IDebugSysWin *)helper_->create_doc_search_win(this);

            /* bring it to the front */
            if (win != 0)
            {
                ShowWindow(GetParent(win->idsw_get_handle()), SW_SHOW);
                win->idsw_bring_to_front();
            }
        }
        return TRUE;

    case ID_VIEW_FILESEARCH:
        /* show the file search window */
        {
            IDebugSysWin *win;

            /* create and/or show the file search window */
            win = (IDebugSysWin *)helper_->create_file_search_win(this);

            /* bring it to the front */
            if (win != 0)
            {
                ShowWindow(GetParent(win->idsw_get_handle()), SW_SHOW);
                win->idsw_bring_to_front();
            }
        }
        return TRUE;

    case ID_VIEW_HELPWIN:
        /* show the help/documentation viewer window at the current page */
        show_help(0);
        return TRUE;

    case ID_VIEW_TRACE:
    show_trace_window:
        /* show the history trace window */
        {
            IDebugSysWin *win;

            /* create and/or show the history trace window */
            win = (IDebugSysWin *)helper_->create_hist_win(this);

            /* make sure it's up to date */
            helper_->update_hist_window(dbgctx_);

            /* bring it to the front */
            if (win != 0)
                win->idsw_bring_to_front();
        }
        return TRUE;

    case ID_PROFILER_START:
        /* start the profiler */
        if (!profiling_)
        {
            /* start the profiler */
            helper_->start_profiling(dbgctx_);

            /* remember that it's running */
            profiling_ = TRUE;
        }
        return TRUE;

    case ID_PROFILER_STOP:
        if (profiling_)
        {
            OPENFILENAME5 info;
            char fname[OSFNMAX];

            /* stop the profiler */
            helper_->end_profiling(dbgctx_);

            /* remember that it's no longer running */
            profiling_ = FALSE;

            /* ask for a filename for dumping the profiler data */
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = fname;
            info.nMaxFile = sizeof(fname);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrTitle = "Select a file for saving profiling data";
            info.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST
                         | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING;
            info.nFileOffset = 0;
            info.nFileExtension = 0;
            info.lpstrDefExt = "txt";
            info.lCustData = 0;
            info.lpfnHook = 0;
            info.lpTemplateName = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
            strcpy(fname, "profile.txt");
            if (GetSaveFileName((OPENFILENAME *)&info))
            {
                FILE *fp;

                /* open the file */
                fp = fopen(fname, "w");

                /* if we got a file, write the data */
                if (fp != 0)
                {
                    write_prof_ctx ctx;

                    /* set up our context */
                    ctx.fp = fp;

                    /* write the data */
                    helper_->get_profiling_data(dbgctx_, &write_prof_data,
                                                &ctx);

                    /* close the file */
                    fclose(fp);
                }
                else
                {
                    MessageBox(handle_, "Unable to open file - profiling "
                               "data not written.", "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);
                }
            }
        }
        return TRUE;

    case ID_VIEW_WATCH:
        /* show the watch window and bring it to the front */
        if (watchwin_ != 0)
        {
            ShowWindow(GetParent(watchwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(watchwin_->get_handle()));
        }
        return TRUE;

    case ID_VIEW_LOCALS:
        /* show the local variables window and bring it to the front */
        if (localwin_ != 0)
        {
            ShowWindow(GetParent(localwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(localwin_->get_handle()));
        }
        return TRUE;

    case ID_VIEW_STATUSLINE:
        /* toggle the statusline */
        show_statusline(!show_statusline_);

        /* handled */
        return TRUE;

    case ID_VIEW_PROJECT:
        /* show the project window and bring it to the front */
        if (projwin_ != 0)
        {
            ShowWindow(GetParent(projwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(projwin_->get_handle()));
        }
        return TRUE;

    case ID_VIEW_SCRIPT:
        /* show the script window and bring it to the front */
        if (scriptwin_ != 0)
        {
            ShowWindow(GetParent(scriptwin_->get_handle()), SW_SHOW);
            BringWindowToTop(GetParent(scriptwin_->get_handle()));
        }
        return TRUE;

    case ID_SHOW_DEBUG_WIN:
        /* show the debug window and bring it to the front */
        if (helper_->create_debuglog_win(this) != 0)
        {
            IDebugSysWin *win = (IDebugSysWin *)helper_->get_debuglog_win();
            ShowWindow(GetParent(GetParent(win->idsw_get_handle())), SW_SHOW);
            BringWindowToTop(GetParent(GetParent(win->idsw_get_handle())));
        }
        return TRUE;

    case ID_VIEW_GAMEWIN:
        /* bring the game window and/or web UI to the front */
        w32_webui_to_foreground();
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->bring_owner_to_front();
        return TRUE;

    case ID_FLUSH_GAMEWIN:
        /* manually flush the game window if the debugger has control */
        if (game_loaded_ && in_debugger_)
            CHtmlDebugHelper::flush_main_output(dbgctx_);
        return TRUE;

    case ID_FLUSH_GAMEWIN_AUTO:
        /* reverse the current setting */
        auto_flush_gamewin_ = !auto_flush_gamewin_;

        /* save the new setting in the global preferences */
        global_config_->setval("gamewin auto flush", 0, auto_flush_gamewin_);

        /* handled */
        return TRUE;

    case ID_DEBUG_EDITBREAKPOINTS:
        /* run the breakpoints dialog */
        CTadsDialogBreakpoints::run_bp_dlg(this);
        return TRUE;

    case ID_DEBUG_HISTORYLOG:
        /* reverse history logging state */
        helper_->set_call_trace_active
            (dbgctx_, !helper_->is_call_trace_active(dbgctx_));

        /*
         *   if we just turned on call tracing, show the trace window and
         *   bring it to the front
         */
        if (helper_->is_call_trace_active(dbgctx_))
            goto show_trace_window;

        /* handled */
        return TRUE;

    case ID_DEBUG_CLEARHISTORYLOG:
        /* clear the history log */
        helper_->clear_call_trace_log(dbgctx_);
        return TRUE;

    case ID_FILE_CLOSE:
        /* close the active window */
        {
            HWND hwnd;

            /* find the active window, and send it a close command */
            if ((hwnd = get_active_mdi_child()) != 0)
                SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return TRUE;

    case ID_WINDOW_RESTORE:
        /* restore the active window */
        {
            /* find the active window, and ask the MDI client to restore it */
            HWND hwnd;

            /* find the active window, and send it a 'restore' command */
            if ((hwnd = get_active_mdi_child()) != 0)
                SendMessage(get_parent_of_children(), WM_MDIRESTORE,
                            (WPARAM)hwnd, 0);
        }
        return TRUE;

    case ID_WINDOW_CLOSE_ALL:
        /* close all widndows */
        {
            /* ask about saving any unsaved changes */
            ask_save_ctx asctx(DLG_ASK_SAVE_MULTI);
            helper_->enum_source_windows(&enum_win_ask_save_cb, &asctx);

            /* if they didn't cancel, proceed with the close */
            if (!asctx.cancel)
                helper_->enum_source_windows(&enum_win_close_cb, this);
        }
        return TRUE;

    case ID_WINDOW_NEXT:
        /* tell the client to activate the next window after the active one */
        SendMessage(get_parent_of_children(), WM_MDINEXT, 0, 0);
        return TRUE;

    case ID_WINDOW_PREV:
        /* tell the client to activate the previous window */
        SendMessage(get_parent_of_children(), WM_MDINEXT, 0, 1);
        return TRUE;

    case ID_WINDOW_CASCADE:
        /* tell the client to cascade windows */
        SendMessage(get_parent_of_children(), WM_MDICASCADE, 0, 0);
        return TRUE;

    case ID_WINDOW_TILE_HORIZ:
        /* tell the client to tile windows */
        SendMessage(get_parent_of_children(), WM_MDITILE,
                    MDITILE_HORIZONTAL, 0);
        return TRUE;

    case ID_WINDOW_TILE_VERT:
        /* tell the client to tile windows */
        SendMessage(get_parent_of_children(), WM_MDITILE,
                    MDITILE_VERTICAL, 0);
        return TRUE;

    case ID_EDIT_POPBOOKMARK:
        /* pop the last-set bookmark */
        pop_bookmark();
        return TRUE;

    case ID_EDIT_JUMPNAMEDBOOKMARK:
        /* prompt for a bookmark name, and jump there */
        jump_to_named_bookmark();
        return TRUE;

    default:
        break;
    }

    /*
     *   we didn't handle it -- run the command by the active debugger
     *   window, and see if it can handle the command
     */
    if (active_win != 0
        && active_win->active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* try version-specific handling */
    if (vsn_do_command(notify_code, command_id, ctl))
        return TRUE;

    /*
     *   if it's in the custom external command range, check to see if it's
     *   our Tools menu; if so, execute the external tool
     */
    if (command_id >= ID_CUSTOM_FIRST && command_id <= ID_CUSTOM_LAST)
    {
        /* try finding it in our Tools menu */
        MENUITEMINFO mii;
        mii.cbSize = menuiteminfo_size_;
        mii.fMask = MIIM_ID;
        if (GetMenuItemInfo(tools_menu_, command_id, FALSE, &mii))
        {
            /* got it - execute it */
            execute_extern_tool(command_id);
            return TRUE;
        }
    }

    /*
     *   some commands require global follow-up if they didn't get executed
     *   by an active debugger window
     */
    switch (command_id)
    {
    case ID_SEARCH_FILE:
        /* try the last active command target window */
        if (get_last_cmd_target_win() != 0
            && get_last_cmd_target_win()->active_do_command(
                notify_code, command_id, ctl))
            return TRUE;

        /* there's no one to handle this; explain why */
        MessageBox(handle_, "This command searches the current source file "
                   "window, so you must first open the file you wish "
                   "to search (or bring its window to the front).",
                   "TADS Workbench",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        return TRUE;

    case ID_EDIT_JUMPNEXTBOOKMARK:
        /*
         *   the active window didn't handle this, so it must not be a source
         *   window; simply jump to the first bookmark in the first window in
         *   the bookmark list
         */
        jump_to_next_bookmark(0, 1);
        return TRUE;

    case ID_EDIT_JUMPPREVBOOKMARK:
        jump_to_next_bookmark(0, -1);
        return TRUE;

    case ID_EDIT_CLEARALLBOOKMARKS:
        /* delete all defined bookmarks */
        while (bookmarks_ != 0)
            delete_bookmark(bookmarks_);

        /* handled */
        return TRUE;
    }

    /* inherit default behavior */
    return CHtmlSys_framewin::do_command(notify_code, command_id, ctl);
}

/*
 *   Check for auto-build on a command that starts a new run
 */
int CHtmlSys_dbgmain::build_on_run(int command_id)
{
    /*
     *   first, check to see if we need to save any files; if that requires a
     *   dialog query, and the user responds with the Cancel option, simply
     *   return false to tell the caller to abandon the Run command
     */
    if (!save_on_build_or_run())
        return FALSE;

    /*
     *   If there have been any changes since our last build, and the
     *   build-on-run option is set, initiate a build.  If the command is
     *   explicitly build-and-run, don't bother, since we're explicitly doing
     *   a build anyway.
     */
    if (command_id != ID_BUILD_COMP_AND_RUN
        && maybe_need_build_
        && global_config_->getval_int("build on run", 0, TRUE))
    {
        /*
         *   kick off a debug build; when we're done with the build, reissue
         *   the original command, so that we execute the game in the mode
         *   requested after the build completes
         */
        start_build(command_id);

        /*
         *   tell the caller not to actually start the run yet - we'll do
         *   that after the build has completed
         */
        return FALSE;
    }

    /* no build is necessary/desired - we're cleared to run */
    return TRUE;
}

/*
 *   Save modified files on save/run
 */
int CHtmlSys_dbgmain::save_on_build_or_run()
{
    /* check to see if we're supposed to save at all */
    if (global_config_->getval_int("save on build", 0, TRUE))
    {
        ask_save_ctx ctx(DLG_ASK_SAVE_MULTI2);

        /*
         *   if we're supposed to save without asking, set the "save all"
         *   flag in the context to true - this bypasses the query; otherwise
         *   leave it defaulted, which will show the prompt
         */
        if (!global_config_->getval_int("ask save on build", 0, TRUE))
            ctx.save_all = TRUE;

        /* save modified windows */
        helper_->enum_source_windows(&enum_win_ask_save_cb, &ctx);

        /* if they canceled, stop the build */
        if (ctx.cancel)
            return FALSE;
    }

    /* okay to proceed */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   filter list entry
 */
struct filter_info
{
    UINT priority;
    textchar_t *str;
} *filters;

/* sort a filter list by priority */
static int sort_by_filter_priority(const void *a0, const void *b0)
{
    const filter_info *a = (const filter_info *)a0;
    const filter_info *b = (const filter_info *)b0;

    /* compare by priority */
    return (int)a->priority - (int)b->priority;
}

/*
 *   Build a file dialog filter list for a text file
 */
void CHtmlSys_dbgmain::build_text_file_filter(CStringBuf *buf, int for_save)
{
    twb_editor_mode_iter iter(this);
    ITadsWorkbenchEditorMode *cur;
    textchar_t *p;
    int cnt;

    /* count the modes */
    for (cnt = 0, cur = iter.first() ; cur != 0 ; cur = iter.next(), ++cnt) ;

    /* if there are any filters, read them in */
    if (cnt != 0)
    {
        int i;

        /* allocate space for the filters */
        filters = new filter_info[cnt];

        /* load the filters */
        for (i = 0, cur = iter.first() ; cur != 0 && i < cnt ;
             cur = iter.next(), ++i)
        {
            BSTR b;
            UINT pri;

            /* ask the mode for its dialog filter */
            if ((b = cur->GetFileDialogFilter(for_save, &pri)) != 0)
            {
                textchar_t *s;

                /* convert to ANSI */
                s = ansi_from_bstr(b, TRUE);

                /* find the double-null that marks the end of the list */
                for (p = s ; *p != '\0' || *(p+1) != '\0' ; ++p)
                {
                    /*
                     *   for now, convert the nulls to newlines - CStringBuf
                     *   doesn't handle embedded nulls properly in append and
                     *   prepend, so we can't use them while constructing the
                     *   string
                     */
                    if (*p == '\0')
                        *p = '\n';
                }

                /* convert the first trailing null to a newline as well */
                *p++ = '\n';

                /* save this filter in our filter list */
                filters[i].str = s;
                filters[i].priority = pri;
            }
        }

        /* sort the filter list by priority */
        qsort(filters, cnt, sizeof(filters[0]), &sort_by_filter_priority);

        /* build the list */
        for (i = 0 ; i < cnt ; ++i)
        {
            /* append this filter string */
            buf->append(filters[i].str);

            /* done with this filter string - free it */
            th_free(filters[i].str);
        }

        /* done with the filter list */
        delete [] filters;
    }

    /* add "All Files", including its double-null terminator */
    static const textchar_t all_files[] = "All Files (*.*)\n*.*\n";
    buf->append(all_files, sizeof(all_files)/sizeof(all_files[0]));

    /* convert our temporary construction newlines back to nulls */
    for (p = buf->get() ; *p != '\0' ; ++p)
    {
        if (*p == '\n')
            *p = '\0';
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Process a file drag-and-drop from Windows Explorer.
 */
void CHtmlSys_dbgmain::drop_shell_file(const char *fname)
{
    /* if it's a regular file (not a directory), open it */
    DWORD attr = GetFileAttributes(fname);
    if (attr != INVALID_FILE_ATTRIBUTES
        && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
        open_text_file(fname);
}

/* ------------------------------------------------------------------------ */
/*
 *   Open a source or text file
 */
IDebugSysWin *CHtmlSys_dbgmain::open_text_file(const textchar_t *fname)
{
    IDebugSysWin *win;
    textchar_t path[OSFNMAX];

    /* if the name isn't absolute, assume it's in the project directory */
    if (!os_is_file_absolute(fname))
    {
        textchar_t projdir[OSFNMAX];

        get_project_dir(projdir, sizeof(projdir));
        os_build_full_path(path, sizeof(path), projdir, fname);
        fname = path;
    }

    /* ask the helper to open the file */
    win = (IDebugSysWin *)helper_->open_text_file(this, fname);

    /* bring the window to the front if we successfully opened the file */
    if (win != 0)
    {
        /* bring the window to the top */
        win->idsw_bring_to_front();

        /* move focus to the window */
        SetFocus(win->idsw_get_handle());
    }

    /* return the new window, if any */
    return win;
}

/*
 *   is the given file open?
 */
int CHtmlSys_dbgmain::is_file_open(const textchar_t *fname)
{
    /*
     *   look for a window viewing the given file; if we find one, the file
     *   is open, otherwise it's not
     */
    return (helper_->find_text_file(this, fname, fname) != 0);
}

/*
 *   Receive notification that a file has been renamed
 */
void CHtmlSys_dbgmain::file_renamed(const textchar_t *old_name,
                                    const textchar_t *new_name)
{
    struct enumcb
    {
        enumcb(CHtmlSys_dbgmain *dbgmain,
               const textchar_t *old_name, const textchar_t *new_name)
        {
            this->dbgmain = dbgmain;
            this->old_name = old_name;
            this->new_name = new_name;
        }

        /* the main debugger object */
        CHtmlSys_dbgmain *dbgmain;

        /* the old and new filenames */
        const textchar_t *old_name;
        const textchar_t *new_name;

        /* window enumeration callback */
        static void cbfunc(
            void *ctx, int idx, class IDebugWin *win,
            int line_source_id, HtmlDbg_wintype_t win_type)
        {
            /* if this isn't a source file, skip it */
            if (win_type != HTMLDBG_WINTYPE_SRCFILE)
                return;

            /* the context is 'this' */
            enumcb *self = (enumcb *)ctx;

            /* get the source file object, suitably cast */
            CHtmlSys_dbgsrcwin *srcwin =
                (CHtmlSys_dbgsrcwin *)(IDebugSysWin *)win;

            /* if its name matches, set the new name */
            if (self->dbgmain->dbgsys_fnames_eq(
                self->old_name, srcwin->get_path()))
            {
                /* set the new name */
                srcwin->file_renamed(self->new_name);
            }
        }
    };

    /* look for a matching window through our enumerator */
    enumcb cb(this, old_name, new_name);
    helper_->enum_source_windows(&cb.cbfunc, &cb);
}


/*
 *   create a new text file
 */
void CHtmlSys_dbgmain::new_text_file(int mode_cmd,
                                     const textchar_t *defext,
                                     const textchar_t *initial_text)
{
    char path[MAX_PATH];
    IDebugSysWin *win;

    /* keep looping until we find a name we can use */
    for (;;)
    {
        enum_untitled_ctx ctx;

        /* select a name for the new file */
        helper_->enum_source_windows(&enum_win_untitled_cb, &ctx);

        /*
         *   build the full path - put the file in the project directory if
         *   there is one, or the working directory if not
         */
        if (local_config_filename_.get() != 0)
        {
            strcpy(path, local_config_filename_.get());
            strcpy(os_get_root_name(path), ctx.title);
        }
        else
        {
            char pwd[MAX_PATH];
            GetCurrentDirectory(sizeof(pwd), pwd);
            os_build_full_path(path, sizeof(path), pwd, ctx.title);
        }

        /* if this doesn't match an existing project file, we can use it */
        if (!helper_->is_file_in_project(this, path))
            break;
    }

    /* create a new window */
    win = (IDebugSysWin *)helper_->new_text_file(this, path);

    /* bring the window to the front if we successfully opened the file */
    if (win != 0)
    {
        CHtmlSys_dbgsrcwin *srcwin = (CHtmlSys_dbgsrcwin *)win;

        /* set the initial contents, if specified */
        if (initial_text != 0)
            srcwin->get_scintilla()->append_text(
                initial_text, strlen(initial_text));

        /* set the editor mode, if specified */
        if (mode_cmd != 0)
            srcwin->set_editor_mode(mode_cmd);

        /* set the default extension */
        if (defext != 0)
            srcwin->set_default_ext(defext);

        /* mark it as untitled */
        win->idsw_set_untitled();

        /* bring the window to the top */
        win->idsw_bring_to_front();

        /* move focus to the window */
        SetFocus(win->idsw_get_handle());
    }
}

/* ------------------------------------------------------------------------ */
/*


/*
 *   Load a recent game, given the ID_GAME_RECENT_n command ID of the game to
 *   load.  Returns true if we loaded a game successfully, false if there is
 *   no such recent game in the MRU list.
 */
int CHtmlSys_dbgmain::load_recent_game(int command_id)
{
    const textchar_t *order;
    const textchar_t *fname;
    CHtmlPreferences *prefs;
    int idx;

    /*
     *   get the main window preferences object, where our recent game list
     *   is stored
     */
    if (CHtmlSys_mainwin::get_main_win() == 0)
        return FALSE;
    prefs = CHtmlSys_mainwin::get_main_win()->get_prefs();

    /* get the order string - if there isn't one, return failure */
    order = prefs->get_recent_dbg_game_order();
    if (order == 0)
        return FALSE;

    /* get the appropriate game, based on the order string */
    idx = command_id - ID_GAME_RECENT_1;
    fname = prefs->get_recent_dbg_game(order[idx]);

    /* if there's no such entry, return failure */
    if (fname == 0 || fname[0] == '\0')
        return FALSE;

    /* set the open-file directory to this game's path */
    CTadsApp::get_app()->set_openfile_dir(fname);

    /* if the entry isn't a valid project file, indicate failure */
    if (!vsn_is_valid_proj_file(fname))
        return FALSE;

    /* load the file */
    load_game(fname);

    /* success */
    return TRUE;
}

/*
 *   Ask for a game to load
 */
int CHtmlSys_dbgmain::ask_load_game(
    HWND parent, textchar_t *buf, size_t buflen)
{
    OPENFILENAME5 info;

    /* ask for a file to open */
    info.hwndOwner = parent;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = w32_debug_opendlg_filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = buf;
    buf[0] = '\0';
    info.nMaxFile = buflen;
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.lpstrTitle = "Select a game to load";
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                 | OFN_NOCHANGEDIR | OFN_HIDEREADONLY
                 | OFN_ENABLESIZING;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
    if (!GetOpenFileName((OPENFILENAME *)&info))
        return FALSE;

    /*
     *   save the new directory as the initial directory for the next file
     *   open operation -- we do this in lieu of allowing the dialog to
     *   change the current directory, since we need to stay in the initial
     *   game directory at all times (since the game incorporates relative
     *   filename paths for source files)
     */
    CTadsApp::get_app()->set_openfile_dir(buf);

    /* successfully selected a file */
    return TRUE;
}


/*
 *   Load a game
 */
void CHtmlSys_dbgmain::load_game(const textchar_t *fname)
{
    /*
     *   Note whether we're opening a new game.  If the filename is
     *   different than our original filename, assume we're loading a new
     *   game.
     */
    reloading_new_ = !dbgsys_fnames_eq(fname, game_filename_.get());

    /*
     *   set the new game as the pending game to load in the main window
     */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->set_pending_new_game(fname);

    /* save the outgoing configuration */
    save_config();

    /* set the RESTART and RELOAD flags */
    restarting_ = TRUE;
    reloading_ = TRUE;

    /* do not start execution - simply reload */
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;

    /* tell the event loop to relinquish control */
    go_ = TRUE;

    /* if we're entering a command, abort that */
    abort_game_command_entry();
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute a build command.  This makes a bunch of checks (to make sure
 *   it's okay to do the build right now, and that the configuration options
 *   for the selected type of build are all set), then kicks off the
 *   version-dependent build process.
 */
int CHtmlSys_dbgmain::start_build(int command_id,
                                  const TdbBuildTargets *pub_targets)
{
    TdbBuildTargets t;
    CHtmlDebugConfig *lc = local_config_;
    textchar_t buf[OSFNMAX];

    /*
     *   if no workspace is open, or a build is already in progress, we can't
     *   proceed
     */
    if (!workspace_open_ || lc == 0 || build_running_)
        return FALSE;

    /* initialize the build settings */
    memset(&t, 0, sizeof(t));
    t.build_type = TdbBuildTargets::BUILD_NONE;

    /*
     *   Figure out which combination of targets we're building, based on the
     *   command.
     */
    switch (command_id)
    {
    case ID_BUILD_COMPDBG:
    case ID_BUILD_COMPDBG_AND_SCAN:
    case ID_PROJ_SCAN_INCLUDES:
    case ID_BUILD_COMP_AND_RUN:
        /* compile for debug - build the debugging game file */
        t.build_type = TdbBuildTargets::BUILD_DEBUG;
        break;

    case ID_DEBUG_GO:
    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
    case ID_DEBUG_STEPOUT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_REPLAY_SESSION:
        /* implicit build on go - build the debugging game file */
        t.build_type = TdbBuildTargets::BUILD_DEBUG;
        break;

    case ID_BUILD_COMPDBG_FULL:
        /* full recompile for debugging - build the debug game */
        t.build_type = TdbBuildTargets::BUILD_DEBUG;
        break;

    case ID_BUILD_COMPRLS:
        /* compile for release - build the release game file */
        t.build_type = TdbBuildTargets::BUILD_RELEASE;
        break;

    case ID_BUILD_COMPEXE:
        /* build the .exe */
        t.build_exe = TRUE;
        break;

    case ID_BUILD_RELEASEZIP:
        /* release ZIP */
        t.build_zip = TRUE;
        break;

#ifdef NOFEATURE_PUBLISH
    case ID_BUILD_PUBLISH:
        /*
         *   publish: build everything selected in the publication targets
         *   argument
         */
        t.build_zip = pub_targets->build_zip;
        t.build_setup = pub_targets->build_setup;
        t.build_srczip = pub_targets->build_srczip;

        /* copy the credentials */
        t.ifdb_user = pub_targets->ifdb_user;
        t.ifdb_psw = pub_targets->ifdb_psw;
        t.email = pub_targets->email;

        /* this is a release build */
        t.build_type = TdbBuildTargets::BUILD_RELEASE;
        break;
#endif

    case ID_BUILD_WEBPAGE:
        /* build the web page */
        t.build_html = TRUE;
        break;

    case ID_BUILD_SRCZIP:
        /* build the source zip file */
        t.build_type = TdbBuildTargets::BUILD_NONE;
        t.build_srczip = TRUE;
        break;

    case ID_BUILD_COMPINST:
        /* build the Windows SETUP program */
        t.build_setup = TRUE;
        break;

    case ID_BUILD_ALLPACKAGES:
        /*
         *   Build all enabled packaging options - release game, .EXE, SETUP,
         *   ZIP, and Web page.  Run through the configuration to determine
         *   which packages are enabled, and select the targets for each one.
         */
        t.build_zip = lc->getval_int("build", "zip in all pk", TRUE);
        t.build_setup = lc->getval_int("build", "installer in all pk", TRUE);
        t.build_html = lc->getval_int("build", "webpage in all pk", TRUE);
        t.build_srczip = lc->getval_int("build", "src in all pk", TRUE);
        break;

    case ID_BUILD_CLEAN:
        /* clean */
        t.build_type = TdbBuildTargets::BUILD_CLEAN;
        break;
    }

    /*
     *   Figure dependencies among the targets:
     *
     *   If we're building the web page, and the option is set to include the
     *   Windows SETUP program among the web page's downloads, build the
     *   SETUP.  Likewise, if the option is set to include the source code,
     *   build the source ZIP.
     *
     *   In any case, building the web page requires building the release
     *   ZIP.
     *
     *   If we're building SETUP, we need the release game; the setup maker
     *   automatically creates the EXE file from the release game, so we
     *   don't need to do that separately.
     *
     *   If we're building the .EXE or the ZIP file or the web page, we need
     *   the release game file.
     */
    if (t.build_html && lc->getval_int("build", "installer in webpage", FALSE))
        t.build_setup = TRUE;
    if (t.build_html && lc->getval_int("build", "src in webpage", FALSE))
        t.build_srczip = TRUE;
    if (t.build_html)
        t.build_zip = TRUE;
    if (t.build_setup || t.build_exe || t.build_zip || t.build_html)
        t.build_type = TdbBuildTargets::BUILD_RELEASE;

    /* bring the log window to the front, since the build output goes there */
    SendMessage(handle_, WM_COMMAND, ID_SHOW_DEBUG_WIN, 0);

    /*
     *   if a game is currently running, and we're going to rebuild the debug
     *   version or clean the debug build, we need to terminate the game
     */
    if (get_game_loaded()
        && (t.build_type == TdbBuildTargets::BUILD_DEBUG
            || t.build_type == TdbBuildTargets::BUILD_CLEAN))
    {
        int btn;

        /* ask them about it only if we're configured to do so */
        if (get_global_config_int("ask load dlg", 0, TRUE))
        {
            /* ask them about it */
            btn = MessageBox(handle_,
                             "Compiling requires that the game be "
                             "terminated.  Do you wish to terminate "
                             "the game and proceed with the "
                             "compilation?",
                             "TADS Workbench",
                             MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

            /* if they didn't say yes, abort */
            if (btn != IDYES && btn != 0)
                return FALSE;
        }

        /*
         *   Terminate the game .  This doesn't actually stop the game, since
         *   it's currently paused calling the debugger event loop
         *   recursively; instead, it sets flags that tell the game to exit
         *   immediately as soon as it gets control back, which will happen
         *   as soon as we return from the event loop, below...w
         */
        terminate_current_game();

        /*
         *   Don't do the build now.  Instead, save the current command as
         *   the pending build command and return from the event loop.  This
         *   will give control back to the game.  The game should see the
         *   termination flags we just set and exit in short order.  Once it
         *   exits, we'll get control again automatically, at which point
         *   we'll apply the pending build command.
         */
        set_pending_build_cmd(command_id);
        return FALSE;
    }

    /* make version-specific checks to see if we can compile */
    if (!vsn_ok_to_compile())
        return FALSE;

    /*
     *   If they're building the release game, and they haven't set the
     *   release filename, or the release filename is the same as the game
     *   filename, they need to configure before they can build
     */
    if (t.build_type == TdbBuildTargets::BUILD_RELEASE)
    {
        const char *msg;

        msg = 0;
        if (lc->getval("build", "rlsgam", 0, buf, sizeof(buf))
            || strlen(buf) == 0)
        {
            msg = "Before you can build this package, you "
                  "must select the name of the release game file.  "
                  "Please enter the release game name, then run "
                  "the build command again.";
        }
        else if (stricmp(get_gamefile_name(), buf) == 0)
        {
            msg = "The name of the release game is the same as the "
                  "debugging game.  You must use a different name "
                  "for the release version of the game so that it "
                  "doesn't overwrite the debug version.  Please "
                  "change the name, then run the build command again.";
        }

        /* if we found a reason to complain, do so */
        if (msg != 0)
        {
            /* show the error */
            MessageBox(handle_, msg, "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* run the configuration dialog */
            run_dbg_build_dlg(handle_, this,
                              DLG_BUILD_OUTPUT, IDC_FLD_RLSGAM);

            /* we're done for now */
            return FALSE;
        }
    }

    /*
     *   if they're building the .EXE, and they haven't set the executable
     *   filename, they need to configure before they can run this
     */
    if (t.build_exe
        && (lc->getval("build", "exe", 0, buf, sizeof(buf))
            || strlen(buf) == 0))
    {
        /* tell them what's wrong */
        MessageBox(handle_, "Before you can build this package, "
                   "you must select the name of the executable "
                   "file in the build configuration.  Please enter an "
                   "executable name, then run the build command again. ",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* show the dialog */
        run_dbg_build_dlg(handle_, this, DLG_BUILD_OUTPUT, IDC_FLD_EXE);

        /* we're done for now */
        return FALSE;
    }

    /*
     *   if they're building an installer, they must specify the name of the
     *   installation file
     */
    if (t.build_setup
        && (lc->getval("build", "installer prog", 0, buf, sizeof(buf))
            || strlen(buf) == 0))
    {
        /* tell them what's wrong */
        MessageBox(handle_, "Before you can build this package, "
                   "you must enter a name for the SETUP program to be "
                   "generated.  Enter the information, then run the "
                   "build command again.", "TADS Workbench",
                   MB_OK | MB_ICONEXCLAMATION);

        /* show the dialog */
        run_dbg_build_dlg(handle_, this,
                          DLG_BUILD_INSTALL, IDC_FLD_INSTALL_EXE);

        /*
         *   don't proceed with compilation right now - wait for the user to
         *   retry explicitly
         */
        return FALSE;
    }

    /* if building a release ZIP file, make sure a filename is set */
    if (t.build_zip
        && (lc->getval("build", "zipfile", 0, buf, sizeof(buf))
            || strlen(buf) == 0))
    {
        MessageBox(handle_, "Before you can build this package, "
                   "you must set the Release ZIP filename.  Please enter "
                   "the information, then run the build command again.",
                   "TADS Workbench", MB_OK | MB_ICONINFORMATION);

        run_dbg_build_dlg(handle_, this, DLG_BUILD_ZIP, IDC_FLD_ZIP);
        return FALSE;
    }

    /* if building a web page, make sure the output is set */
    if (t.build_html
        && (lc->getval("build", "webpage dir", 0, buf, sizeof(buf))
            || strlen(buf) == 0))
    {
        MessageBox(handle_, "Before you can build this package, you must "
                   "set the web page output directory.  Please enter the "
                   "information, then run the build command again.",
                   "TADS Workbench", MB_OK | MB_ICONINFORMATION);

        run_dbg_build_dlg(handle_, this, DLG_BUILD_WEBPAGE, IDC_FLD_DIR);
        return FALSE;
    }

    /* if building a source ZIP, make sure the output is set */
    if (t.build_srczip
        && (lc->getval("build", "srczipfile", 0, buf, sizeof(buf))
            || strlen(buf) == 0))
    {
        MessageBox(handle_, "Before you build this package, "
                   "you must enter the filename for the ZIP file. Please "
                   "enter the information, then run the build command "
                   "again.", "TADS Workbench", MB_OK | MB_ICONINFORMATION);

        run_dbg_build_dlg(handle_, this, DLG_BUILD_SRCZIP, IDC_FLD_ZIP);
        return FALSE;
    }

    /*
     *   ask about saving modified files (except when we're doing a 'clean',
     *   since that doesn't depend on the contents any source files)
     */
    if (t.build_type != TdbBuildTargets::BUILD_CLEAN
        && !save_on_build_or_run())
        return FALSE;

    /* clear the debug log window if desired */
    if (get_global_config_int("dbglog", "clear on build", TRUE))
    {
        /* clear the window */
        helper_->clear_debuglog_win();

        /*
         *   turn off auto-vscroll, since we're starting at the top - the
         *   first error messages are usually the most relevant
         */
        if (helper_->create_debuglog_win(this) != 0)
            ((CHtmlSysWin_win32_dbglog *)helper_->get_debuglog_win())
                ->set_auto_vscroll(FALSE);
    }

    /* add a tip about going to an error line */
    {
        CHtmlFontDesc fnt;

        dbgsys_set_srcfont(&fnt);
        fnt.weight = 700;
        fnt.italic = TRUE;
        CHtmlSys_mainwin::get_main_win()->dbg_printf(
            &fnt, TRUE,
            "Tip: you can go to the source location of a compile "
            "error\nby double-clicking on the error message in this "
            "window\n\n");
    }

    /*
     *   clear the flag indicating there are changes since the last build -
     *   since we're just kicking off a build, this is the zero point, so
     *   there are by definition no changes since this point yet
     */
    maybe_need_build_ = FALSE;

    /* we're set - go run the compiler */
    return run_compiler(command_id, t);
}

/* ------------------------------------------------------------------------ */
/*
 *   Thread static entrypoint for running a build
 */
DWORD WINAPI CHtmlSys_dbgmain::run_compiler_entry(void *ctx)
{
    CHtmlSys_dbgmain *mainwin = (CHtmlSys_dbgmain *)ctx;

    /* invoke the main compiler entrypoint */
    mainwin->run_compiler_th();

    /* the thread is finished */
    CTadsApp::on_thread_exit();
    return 0;
}

/*
 *   member function entrypoint for compiler thread
 */
void CHtmlSys_dbgmain::run_compiler_th()
{
    time_t timeval;
    struct tm *tm;
    CHtmlDbgBuildCmd *cmd;
    CHtmlSys_mainwin *mainwin;
    int err;
    CHtmlFontDesc border_font;
    int step;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* get the font for the epilogue and prologue lines */
    dbgsys_set_srcfont(&border_font);
    border_font.weight = 700;
    border_font.italic = TRUE;

    /* show the build prologue */
    if (!capture_build_output_)
    {
        time(&timeval);
        tm = localtime(&timeval);
        mainwin->dbg_printf(&border_font, TRUE,
                            "----- begin build: %.24s -----\n",
                            asctime(tm));
    }

    /* update the status bar with the progress setting */
    update_build_progress_bar(TRUE);

    /*
     *   Go through the command list.  The main thread won't touch the
     *   command list as long as we're running, so we don't need to worry
     *   about synchronizing with the main thread when reading this list:
     *   the main thread synchronizes with us instead.
     */
    for (err = FALSE, step = 0, cmd = build_cmd_head_ ; cmd != 0 ;
         cmd = cmd->next_, ++step)
    {
        /*
         *   if we've already encountered an error, and this command isn't
         *   marked as required, don't bother running it
         */
        if (err && !cmd->required_)
            continue;

        /* update the status with the current build step */
        if (WaitForSingleObject(stat_mutex_, 100) == WAIT_OBJECT_0)
        {
            /* set the current command's description */
            stat_desc_.set(cmd->desc_.get());
            stat_subdesc_.set("");

            /* set the current build step */
            stat_build_step_ = step;

            /*
             *   we haven't accomplished any part of this step yet, and we
             *   don't even know its length yet
             */
            stat_step_progress_ = 0;
            stat_step_len_ = 0;

            /* update the build progress bar with the new settings */
            update_build_progress_bar(TRUE);

            /* done with the status mutex */
            ReleaseMutex(stat_mutex_);
        }

        /* execute this command */
        if (run_command(cmd->exe_name_.get(), cmd->exe_.get(),
                        cmd->args_.get(), cmd->dir_.get()))
        {
            /*
             *   this command failed - note that we have an error, so that
             *   we skip any subsequent commands that aren't marked as
             *   required
             */
            err = TRUE;
        }
    }

    /* add completion information to the debug log, if not capturing */
    if (!capture_build_output_)
    {
        /* generate the completion message */
        const char *msg = (err == 0 ? "Build successfully completed" :
                           "Build failed");

        /* show the overall result */
        mainwin->dbg_printf(FALSE, "\n%s.\n", msg);

        /* set the final status message */
        if (WaitForSingleObject(stat_mutex_, 250) == WAIT_OBJECT_0)
        {
            /* set the status */
            stat_desc_.set(msg);
            stat_subdesc_.set("");

            /* update the build progress bar with the new settings */
            update_build_progress_bar(TRUE);

            /* done with the mutex */
            ReleaseMutex(stat_mutex_);
        }

        /* show the build epilogue */
        time(&timeval);
        tm = localtime(&timeval);
        mainwin->dbg_printf(&border_font, FALSE,
                            "----- end build: %.24s -----\n\n",
                            asctime(tm));
    }

    /* build completed - hide the build progress bar */
    update_build_progress_bar(FALSE);

    /* post the main window to tell it we're done with the build */
    PostMessage(handle_, HTMLM_BUILD_DONE, (WPARAM)err, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Run an external command.  Displays a message if the command fails.
 */
int CHtmlSys_dbgmain::run_command(const textchar_t *exe_name,
                                  const textchar_t *exe,
                                  const textchar_t *cmdline,
                                  const textchar_t *cmd_dir)
{
    int err;
    CHtmlSys_mainwin *mainwin;
    DWORD exit_code;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* run the command */
    err = run_command_sub(exe_name, exe, cmdline, cmd_dir, &exit_code);

    /* if the command failed, show the error */
    if (err != 0)
    {
        char msg[256];

        /*
         *   we couldn't start the command - check for the simplest case,
         *   which is that the executable we were trying to launch doesn't
         *   exist; in this case, provide a somewhat friendlier error than
         *   the system would normally do
         */
        if (osfacc(exe))
        {
            /* the executable doesn't exist - explain this */
            mainwin->dbg_printf(FALSE, "error: executable file \"%s\" does "
                                "not exist or could not be opened - you "
                                "might need to re-install TADS.\n", exe);
        }
        else
        {
            DWORD err;

            /* get the last error code */
            err = GetLastError();

            /*
             *   If the error is zero, it's hard to say for sure what the
             *   problem is.  One possibility that seems to arise in this
             *   case is that the working directory in which we tried to
             *   execute the command is invalid; check for this
             *   explicitly.  In other cases, show a generic failure
             *   message. (Don't show the actual Windows messages, because
             *   it will be "the operation completed successfully", which
             *   it obviously did not.)
             */
            if (err == 0)
            {
                /* check for an invalid working directory */
                if (GetFileAttributes(cmd_dir) == 0xffffffff)
                {
                    /* the working directory doesn't exist - say so */
                    sprintf(msg, "source directory \"%s\" is invalid",
                            cmd_dir);
                }
                else
                {
                    /*
                     *   somehow, we had a failure, but the Windows
                     *   internal error code didn't get set to anything
                     *   meaningful; show a generic error message
                     */
                    strcpy(msg, "unable to execute program");
                }
            }
            else
            {
                /*
                 *   we had a system error starting the process - get the
                 *   system error message and display it
                 */
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0,
                              GetLastError(), 0, msg, sizeof(msg), 0);
            }

            /* show the message */
            mainwin->dbg_printf(FALSE, "%s: error: %s\n", exe_name, msg);
        }
    }
    else if (exit_code != 0)
    {
        /* we got an error from the subprogram itself */
        mainwin->dbg_printf(FALSE, "%s: error code %ld\n",
                            exe_name, exit_code);

        /* flag it as an error to our caller */
        err = 1;
    }

    /* return the result */
    return err;
}

/* ------------------------------------------------------------------------ */
/*
 *   Parse a quoted token out of a command line string
 */
static void parse_quoted_token(CStringBuf &buf, const textchar_t *&p)
{
    /* skip leading spaces */
    for ( ; isspace(*p) ; ++p) ;

    /*
     *   if we're at a quote, scan to the next quote; otherwise scan to the
     *   next space
     */
    const textchar_t *start;
    if (*p == '"')
    {
        /* the token value starts after the quote; scan for the end quote */
        for (start = ++p ; *p != '\0' && *p != '"' ; ++p) ;

        /* pull out the token value */
        buf.set(start, p - start);

        /* skip the close quote */
        if (*p == '"')
            ++p;
    }
    else
    {
        /* the token starts here and ends at the next space */
        for (start = p ; *p != '\0' && !isspace(*p) ; ++p) ;

        /* pull out the token value */
        buf.set(start, p - start);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Run an external command.  Returns the return value of the external
 *   command.
 */
int CHtmlSys_dbgmain::run_command_sub(const textchar_t *exe_name,
                                      const textchar_t *exe,
                                      const textchar_t *cmdline,
                                      const textchar_t *cmd_dir,
                                      DWORD *exit_code)
{
    SECURITY_ATTRIBUTES sattr;
    HANDLE old_in;
    HANDLE old_out;
    HANDLE old_err;
    HANDLE chi_out_r, chi_out_r2;
    HANDLE chi_out_w;
    HANDLE chi_in_r;
    HANDLE chi_in_w;
    int result;
    PROCESS_INFORMATION pinfo;
    STARTUPINFO sinfo;
    CHtmlSys_mainwin *mainwin;
    const textchar_t *p;
    textchar_t endch;

    /* get the main window */
    mainwin = CHtmlSys_mainwin::get_main_win();

    /* clear out our command status on the statusline */
    compile_status_.set("");
    if (statusline_ != 0)
        statusline_->update();

    /*
     *   find the argument portion of the command line - skip the first
     *   quoted token, which is the full path to the program we're
     *   invoking
     */
    p = cmdline;
    if (*p == '"')
    {
        /* it starts with a quote, so find the closing quote */
        endch = '"';

        /* skip the open quote */
        ++p;
    }
    else
    {
        /* it doesn't start with a quote - just find the first space */
        endch = ' ';
    }

    /* seek the end of the first token */
    for ( ; *p != '\0' && *p != endch ; ++p) ;

    /* if we stopped at a quote, skip the quote */
    if (*p == '"')
        ++p;

    /* skip spaces */
    for ( ; isspace(*p) ; ++p) ;

    /* presume the child process will indicate success */
    *exit_code = 0;

    /* check for our special internal commands */
    if (stricmp(exe, "mkdir") == 0)
    {
        /* it's our special "mkdir" built-in command */
        mainwin->dbg_printf(FALSE, ">mkdir \"%s\"\n", cmdline);

        /* ignore this silently if the directory already exists */
        DWORD attr = GetFileAttributes(cmdline);
        if (attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
            return 0;

        /* create the directory */
        if (!CreateDirectory(cmdline, 0))
            *exit_code = GetLastError();

        /* successfully executed */
        return 0;
    }
    else if (stricmp(exe, "del") == 0)
    {
        /*
         *   display the special command syntax - the command line is just
         *   the filename to be deleted
         */
        mainwin->dbg_printf(FALSE, ">del \"%s\"\n", cmdline);

        /* try deleting the file - return an error if it fails */
        if (!DeleteFile(cmdline))
            *exit_code = GetLastError();

        /*
         *   we successfully ran the command (even if the command failed, we
         *   successfully *executed* it; a failure will be reflected in the
         *   child process exit code)
         */
        return 0;
    }
    else if (stricmp(exe, "rmdir/s") == 0)
    {
        /* it's a remove-directory command - display special syntax */
        mainwin->dbg_printf(FALSE, ">rmdir /s \"%s\"\n", cmdline);

        /*
         *   delete the directory and its contents; the result is the "child
         *   process" exit code, since we're running the operation in-line
         */
        *exit_code = del_temp_dir(cmdline);

        /*
         *   we successfully "ran the child process" (the rmdir/s could have
         *   failed, but if it did, that's a failure in the "child process"
         *   rather than in executing the command)
         */
        return 0;
    }
    else if (stricmp(exe, "zip") == 0)
    {
        /* it's a ZIP command - go run it */
        mainwin->dbg_printf(FALSE, ">zip %s\n", cmdline);
        *exit_code = zip(cmdline, cmd_dir);

        /* successfully executed the fake child process */
        return 0;
    }
    else if (stricmp(exe, "open") == 0)
    {
        /*
         *   it's an "open" command, meaning that we show the document named
         *   in the command line using a Windows shell open
         */
        mainwin->dbg_printf(FALSE, ">\"%s\"\n", cmdline);

        /* run the program */
        unsigned long code = (unsigned long)ShellExecute(
            0, 0, cmdline, 0, 0, SW_SHOWNORMAL);

        /* any result code <= 32 indicates an error */
        if (code <= 32)
            *exit_code = code + 1;

        /* successfully 'executed' */
        return 0;
    }
    else if (stricmp(exe, "upload") == 0)
    {
        /*
         *   It's an "upload" command, which uploads a file to a selected
         *   site.  The format of the command tail is "local file" "remote
         *   file" "site" "email", giving the name of the file to upload, the
         *   site ID, and the email address of the sender.  Currently, the
         *   only supported site is "ifarchive", for www.ifarchive.org.
         */
        mainwin->dbg_printf(FALSE, ">upload %s\n", cmdline);

        /* parse out the arguments */
        p = cmdline;
        CStringBuf lfile, rfile, site, email, ftype;
        parse_quoted_token(lfile, p);
        parse_quoted_token(rfile, p);
        parse_quoted_token(site, p);
        parse_quoted_token(email, p);
        parse_quoted_token(ftype, p);

        /* upload the file */
        *exit_code = ftp_upload(lfile.get(), rfile.get(),
                                site.get(), email.get(),
                                strcmp(ftype.get(), "binary") == 0
                                ? FTP_TRANSFER_TYPE_BINARY
                                : FTP_TRANSFER_TYPE_ASCII);

        /* success */
        return 0;
    }
#ifdef NOFEATURE_PUBLISH
    else if (stricmp(exe, "ifiction") == 0)
    {
        /*
         *   It's an "ifiction" command, which extracts the iFiction record
         *   from the compiled game and stores it in a memory temp file.
         */
        mainwin->dbg_printf(FALSE, ">ifiction %s\n", cmdline);

        /* synthesize the ifiction record */
        CStringBuf ific;
        if (!babel_synth_ifiction(cmdline, &ific))
        {
            mainwin->dbg_printf(
                FALSE, "ifiction: Error reading bibliographic data from "
                "game file\n");
            *exit_code = 1;
            return 0;
        }

        /* save it in a memory file */
        add_build_memfile("ifdbput/ifiction", ific.get());

        /* append it to the links file */
        append_build_memfile("upload/ifarchive/notes", ific.get());

        mainwin->dbg_printf(
            FALSE, "iFiction data successfully synthesized.\n");

        /* success */
        return 0;
    }
    else if (stricmp(exe, "ifdbput") == 0)
    {
        /*
         *   It's a "ifdb send" command, which uploads the listing data to
         *   IFDB.  The arguments are: "gamefile" "username" "password".  We
         *   extract the GameInfo data from the game file and babelize it
         *   (turn it into an iFiction record), then send it along with the
         *   cover art and a link to our IF Archive uploads to IFDB, via the
         *   putific API.
         */

        /* parse out the arguments */
        p = cmdline;
        CStringBuf file, user, psw;
        parse_quoted_token(file, p);
        parse_quoted_token(user, p);
        parse_quoted_token(psw, p);

        mainwin->dbg_printf(FALSE, ">ifdbput \"%s\" \"%s\" ******\n",
                            file.get(), user.get());
        *exit_code = ifdbput(file.get(), user.get(), psw.get());
        return 0;
    }
#endif /* NOFEATURE_PUBLISH */

    /*
     *   write the command line we're executing to the debug log; don't echo
     *   the command if we're capturing build output for our own purposes
     */
    if (!capture_build_output_)
        mainwin->dbg_printf(FALSE, ">%s %s\n", exe_name, p);

    /* no compiler status yet */
    compile_status_.set("");

    /* set up the security attributes to make an inheritable pipe */
    sattr.nLength = sizeof(sattr);
    sattr.bInheritHandle = TRUE;
    sattr.lpSecurityDescriptor = 0;

    /* save the original stdout and stderr */
    old_out = GetStdHandle(STD_OUTPUT_HANDLE);
    old_err = GetStdHandle(STD_ERROR_HANDLE);

    /* create the output pipe for the child */
    if (!CreatePipe(&chi_out_r, &chi_out_w, &sattr, 0))
        return -1;

    /*
     *   set standard out to the child write handle, so that the child
     *   inherits the pipe as its stdout
     */
    if (!SetStdHandle(STD_OUTPUT_HANDLE, chi_out_w)
        || !SetStdHandle(STD_ERROR_HANDLE, chi_out_w))
        return -1;

    /* make the read handle noninheritable */
    if (!DuplicateHandle(GetCurrentProcess(), chi_out_r,
                         GetCurrentProcess(), &chi_out_r2, 0,
                         FALSE, DUPLICATE_SAME_ACCESS))
        return -1;

    /* close the original (inheritable) child output read handle */
    CloseHandle(chi_out_r);

    /* save our old stdin */
    old_in = GetStdHandle(STD_INPUT_HANDLE);

    /* create a pipe for the child to use as stdin */
    if (!CreatePipe(&chi_in_r, &chi_in_w, &sattr, 0))
        return -1;

    /*
     *   set the standard in to the child read handle, so that the child
     *   inherits the pipe as its stdin
     */
    if (!SetStdHandle(STD_INPUT_HANDLE, chi_in_r))
        return -1;

    /*
     *   close our end of the child's standard input handle, since we
     *   can't provide any input to the child - the child will just get an
     *   eof if it tries to read from its stdin
     */
    CloseHandle(chi_in_w);

    /* set up our STARTUPINFO */
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    sinfo.dwFlags = STARTF_USESTDHANDLES;
    sinfo.hStdInput = chi_in_r;
    sinfo.hStdOutput = chi_out_w;
    sinfo.hStdError = chi_out_w;

    /* start the child program */
    result = CreateProcess(exe, (char *)cmdline, 0, 0, TRUE,
                           DETACHED_PROCESS, 0, cmd_dir, &sinfo, &pinfo);

    /*
     *   remember the compiler's process handle, in case we need to forcibly
     *   terminate the process
     */
    compiler_hproc_ = pinfo.hProcess;
    compiler_pid_ = pinfo.dwProcessId;

    /* update the statusline to indicate the compiler is running */
    if (statusline_ != 0)
        statusline_->update();

    /*
     *   we don't need the thread handle for anything - close it if we
     *   successfully started the process
     */
    if (result != 0)
        CloseHandle(pinfo.hThread);

    /* restore our original stdin and stdout */
    SetStdHandle(STD_INPUT_HANDLE, old_in);
    SetStdHandle(STD_OUTPUT_HANDLE, old_out);
    SetStdHandle(STD_ERROR_HANDLE, old_err);

    /* close our copies of the child's ends of the handles */
    CloseHandle(chi_in_r);
    CloseHandle(chi_out_w);

    /*
     *   if the process started successfully, read its standard output and
     *   wait for it to complete
     */
    if (result)
    {
        CStringBuf line_so_far;

        /* keep going until we get an eof on the pipe */
        for (;;)
        {
            char buf[256];
            DWORD bytes_read;
            char *src;
            char *dst;

            /* read from the pipe */
            if (!ReadFile(chi_out_r2, buf, sizeof(buf), &bytes_read, FALSE))
                break;

            /*
             *   scan for newlines - remove any '\r' characters (console
             *   apps tend to send us a "\r\n" sequence for each newline;
             *   we only need a C-style "\n" character, so eliminate all
             *   of the "\r" characters)
             */
            for (src = dst = buf ; src < buf + bytes_read ; ++src)
            {
                /* if this isn't a '\r' character, copy it to the output */
                if (*src != '\r')
                    *dst++ = *src;
            }

            /* set the new length, with the '\r's eliminated */
            bytes_read = dst - buf;

            /* if that leaves anything, process it */
            if (bytes_read != 0)
            {
                /*
                 *   write the text to the console, or process it if we're
                 *   capturing the output
                 */
                if (capture_build_output_ || filter_build_output_)
                {
                    const char *p;
                    const char *start;
                    size_t rem;

                    /*
                     *   We're capturing the build output.  Scan the text
                     *   for each newline.
                     */
                    for (start = p = buf, rem = bytes_read ; rem != 0 ;
                         ++p, --rem)
                    {
                        /* if we're at a newline, process this chunk */
                        if (*p == '\n')
                        {
                            /*
                             *   append this chunk to any partial line left
                             *   over at the end of the previous chunk
                             */
                            line_so_far.append_inc(start, p - start, 512);

                            /* process this line */
                            if (vsn_process_build_output(&line_so_far)
                                && !capture_build_output_)
                            {
                                /* they want to display the text */
                                mainwin->dbg_printf(FALSE, "%s\n",
                                    line_so_far.get());
                            }

                            /* flush the line buffer */
                            line_so_far.set("");

                            /*
                             *   move the starting point for the next line
                             *   to the character following the newline
                             */
                            start = p + 1;
                        }
                    }

                    /*
                     *   if we have anything left over after the last
                     *   newline, copy it to the partial line buffer so that
                     *   we'll process it next time
                     */
                    if (p != start)
                        line_so_far.append_inc(start, p - start, 512);
                }
                else
                {
                    /* send the output to the debug log window */
                    mainwin->dbg_printf(FALSE, "%.*s", (int)bytes_read, buf);
                }
            }
        }

        /* finish the output */
        if (capture_build_output_ || filter_build_output_)
        {
            /* process the last chunk */
            if (line_so_far.get() != 0
                && line_so_far.get()[0] != '\0'
                && vsn_process_build_output(&line_so_far))
            {
                /* show the line */
                mainwin->dbg_printf(FALSE, "%s\n", line_so_far.get());
            }

            /* add an extra blank line if not capturing everything */
            if (!capture_build_output_)
                mainwin->dbg_printf(FALSE, "\n");
        }
        else
        {
            /* add an extra blank line to the debug log output */
            mainwin->dbg_printf(FALSE, "\n");
        }

        /* clear out our command status on the statusline */
        compile_status_.set("");
        if (statusline_ != 0)
            statusline_->update();

        /* wait for the process to terminate */
        WaitForSingleObject(pinfo.hProcess, INFINITE);

        /* get the child result code */
        GetExitCodeProcess(pinfo.hProcess, exit_code);

        /* close the process handle */
        CloseHandle(pinfo.hProcess);

        /* success */
        result = 0;
    }
    else
    {
        /* we didn't start the process - return an error */
        result = -1;
    }

    /* forget the compiler process handle */
    compiler_hproc_ = 0;

    /* update the statusline */
    if (statusline_ != 0)
        statusline_->update();

    /* close our end of the stdout pipe */
    CloseHandle(chi_out_r2);

    /* return the result from the child process */
    return result;
}

/* ------------------------------------------------------------------------ */
/*
 *   Internet status callback
 */
void CALLBACK CHtmlSys_dbgmain::inet_status_cb(
    HINTERNET hnet, DWORD_PTR ctx, DWORD stat, VOID *info, DWORD infoLen)
{
    CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();
    CHtmlSys_dbgmain *self = (CHtmlSys_dbgmain *)ctx;

    /* get the status mutex - if we can't within a few ms, skip the update */
    if (WaitForSingleObject(self->stat_mutex_, 50) == WAIT_OBJECT_0)
    {
        char buf[128];

        /* check the status */
        switch (stat)
        {
        case INTERNET_STATUS_CONNECTED_TO_SERVER:
            self->stat_subdesc_.set("Connected to host");
            break;

        case INTERNET_STATUS_CONNECTING_TO_SERVER:
            self->stat_subdesc_.set("Connecting to host");
            break;

        case INTERNET_STATUS_REQUEST_SENT:
            /*
             *   if we know the total number of bytes in the file we're
             *   sending, count this as part of the total upload
             */
            if (self->stat_step_len_ != 0)
            {
                self->stat_step_progress_ += *(DWORD *)info;
                sprintf(buf, "Sent %lu of %lu bytes",
                        self->stat_step_progress_, self->stat_step_len_);
                self->stat_subdesc_.set(buf);
            }
            break;
        }

        /* update the build progress bar with the new settings */
        self->update_build_progress_bar(TRUE);

        /* done with the mutex */
        ReleaseMutex(self->stat_mutex_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Execute an FTP Upload command
 */
int CHtmlSys_dbgmain::ftp_upload(
    const textchar_t *lname, const textchar_t *rname,
    const textchar_t *siteID, const textchar_t *email, DWORD flags)
{
    const char *host = 0;
    const char *user = 0;
    const char *psw = 0;
    const char *dir = 0;
    HINTERNET hnet = 0, hconn = 0;
    int err = 1;
    CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();

    /* figure the site address based on the site ID */
    if (strcmp(siteID, "ifarchive") == 0)
    {
        host = "ftp.ifarchive.org";
        user = "anonymous";
        psw = email;
        dir = "/incoming";
    }
    else
    {
        /* unknown site */
        mainwin->dbg_printf(
            FALSE, "upload: unsupported site %s\n", siteID);
        return 1;
    }


    /* open the internet connection */
    hnet = InternetOpen(
        "TADS Workbench", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
    if (hnet == 0)
    {
        mainwin->dbg_printf(
            FALSE, "upload: Unable to open Internet connection\n");
        goto done;
    }

    /* set up a status callback */
    InternetSetStatusCallback(hnet, &inet_status_cb);

    /* connect to the Archive */
    hconn = InternetConnect(
        hnet, host, INTERNET_DEFAULT_FTP_PORT,
        user, psw, INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE,
        (DWORD_PTR)this);
    if (hconn == 0)
    {
        mainwin->dbg_printf(
            FALSE, "upload: Error connecting to %s\n", host);
        goto done;
    }

    /* go the /incoming directory */
    if (!FtpSetCurrentDirectory(hconn, dir))
    {
        mainwin->dbg_printf(
            FALSE, "upload: Error changing to %s directory\n", dir);
        goto done;
    }

    /*
     *   check the name - if it starts with ':', it's one of our special
     *   in-memory build temp files, otherwise it's an ordinary file system
     *   file
     */
    if (lname[0] == ':')
    {
        /* get the file */
        CHtmlDbgBuildFile *f = find_build_memfile(lname + 1);
        const textchar_t *contents = f->val.get();
        DWORD clen = f->val.getlen();

        /* note the byte length for the step progress counter */
        stat_step_len_ = clen;

        /* open a stream for writing the file */
        HINTERNET hfile = FtpOpenFile(hconn, rname, GENERIC_WRITE, flags, 0);

        if (hfile == 0)
        {
            mainwin->dbg_printf(
                FALSE, "upload: FTP transfer of %s to %s/%s failed on open\n",
                lname, host, rname);
            goto done;
        }

        /* copy the bytes */
        DWORD actual;
        int ok = InternetWriteFile(hfile, contents, clen, &actual);

        /* close the file */
        InternetCloseHandle(hfile);

        if (!ok || actual != clen)
        {
            mainwin->dbg_printf(
                FALSE, "upload: FTP transfer of %s to %s/%s failed on write "
                "(error %ld)\n",
                lname, host, rname, GetLastError());
            goto done;
        }
    }
    else
    {
        /* get the size of the file */
        WIN32_FILE_ATTRIBUTE_DATA info;
        GetFileAttributesEx(lname, GetFileExInfoStandard, &info);

        /* set the size as the step length */
        stat_step_len_ = info.nFileSizeLow;

        /* upload the file system file */
        if (!FtpPutFile(hconn, lname, rname, flags, (DWORD_PTR)this))
        {
            mainwin->dbg_printf(
                FALSE, "upload: FTP transfer of file %s to %s/%s failed "
                "(error %ld)\n",
                lname, host, rname, GetLastError());
            goto done;
        }
    }

    /* if we get this far, we've successfully completed the upload */
    mainwin->dbg_printf(
        FALSE, "FTP upload succeeded %s -> ftp://%s%s/%s\n",
        lname, host, dir, rname);
    err = 0;

done:
    /* close our handles */
    if (hconn)
        InternetCloseHandle(hconn);
    if (hnet)
        InternetCloseHandle(hnet);

    /* return the error indication */
    return err;
}


/* ------------------------------------------------------------------------ */
/*
 *   Multipart POST form handling
 */

/*
 *   Multipart form item.  This represents one form field; it's essentially a
 *   name/value pair, where the value is either a simple string value, or the
 *   contents of a file.
 */
class form_data_item
{
public:
    form_data_item() { }

    /* set to a simple string value (null-terminated) */
    void set_string(const textchar_t *name, const textchar_t *val)
    {
        this->name.set(name);
        this->val.set(val);
    }

    /* set to a simple string value (counted length) */
    void set_string(const textchar_t *name, const textchar_t *val, size_t len)
    {
        this->name.set(name);
        this->val.set(val, len);
    }

    /* set to the contents of a file system file; returns true on success */
    int set_file(const textchar_t *name, const textchar_t *fname,
                 const textchar_t *typ)
    {
        /* save the basic metadata */
        this->name.set(name);
        this->fname.set(fname);
        this->content_type.set(typ);

        /* presume failure */
        int ok = FALSE;

        /*
         *   load the file's contents; dispense with any newline translation,
         *   even for a text file, since our POST code does that itself as
         *   needed
         */
        FILE *fp = fopen(fname, "rb");
        if (fp != 0)
        {
            /* figure the size of the file */
            fseek(fp, 0, SEEK_END);
            long len = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            /* allocate space */
            val.ensure_size((size_t)len);

            /* load the file */
            ok = ((long)fread(val.get(), 1, len, fp) == len);
            val.setlen(len);

            /* done with the file */
            fclose(fp);
        }

        /* return the status indication */
        return ok;
    }

    /*
     *   set to a "file", but using data taken from a null-terminated string;
     *   this emulates a file in the POST, but the contents are simply taken
     *   from a string
     */
    void set_mem_file(const textchar_t *name, const textchar_t *fname,
                      const textchar_t *enc, const textchar_t *val)
    {
        set_mem_file(name, fname, enc, val, val != 0 ? get_strlen(val) : 0);
    }

    /* set to a "file" whose contents are in a memory buffer */
    void set_mem_file(const textchar_t *name, const textchar_t *fname,
                      const textchar_t *typ,
                      const textchar_t *val, size_t len)
    {
        this->content_type.set(typ);
        this->name.set(name);
        this->fname.set(fname);
        this->val.set(val, len);
    }

    /* name of the field */
    CStringBuf name;

    /* filename; the buffer is null if this is an ordinary string parameter */
    CStringBuf fname;

    /* for a file, the content's MIME type */
    CStringBuf content_type;

    /* value */
    CStringBuf val;
};

/*
 *   HTTP POST a multipart form.  Returns 0 on success, an error code on
 *   failure.
 */
static int http_post_multipart(
    const textchar_t *host, const textchar_t *resource,
    const form_data_item *items, int nitems, CStringBuf &reply)
{
    char boundary[128];
    int i;
    const char *p, *endp;
    int err = 0;
    HINTERNET hi = 0, hcon = 0, hreq = 0;
    DWORD status, statusLen = sizeof(status);

    /*
     *   Allocate a boundary marker for the multipart data.  Choose a string
     *   randomly, then confirm that it's not in any of the data items we're
     *   sending.  If we find it, pick another random marker and try again;
     *   repeat until we find something that's not in the data.  We pick a
     *   string that's long enough that the odds of finding it in the data
     *   are essentially zero, but we check anyway to be sure.
     */
    for (;;)
    {
        /* pick a random boundary string */
        sprintf(boundary, "%x%x%x-%x%x%x-%x%x%x-%x%x%x",
                rand() & 0xFFF, rand() & 0xFFF, rand() & 0xFFF,
                rand() & 0xFFF, rand() & 0xFFF, rand() & 0xFFF,
                rand() & 0xFFF, rand() & 0xFFF, rand() & 0xFFF,
                rand() & 0xFFF, rand() & 0xFFF, rand() & 0xFFF);

        /* assume we won't find a match */
        int ok = TRUE;

        /* scan each item value for a match */
        for (i = 0 ; i < nitems ; ++i)
        {
            /* scan for this string at the start of a line in this value */
            for (p = items[i].val.get(), endp = p + items[i].val.getlen() ;
                 p < endp ; )
            {
                /* check for a match to the start of this line */
                if (memcmp(p, boundary, strlen(boundary)) == 0
                    || (*p == '-'
                        && *(p+1) == '-'
                        && memcmp(p+2, boundary, strlen(boundary)) == 0))
                {
                    /* found a match - reject this proposed boundary */
                    ok = FALSE;
                    break;
                }

                /* find the end of this line */
                for ( ; p < endp && *p != '\n' ; ++p) ;

                /* if there's more, advance to the start of the next line */
                while (p < endp && (*p == '\r' || *p == '\n'))
                    ++p;
            }
        }

        /* if the boundary string is good, we're done searching */
        if (ok)
            break;
    }

    /* set up a buffer to hold the result */
    CStringBufCmd buf(4096);

    /* start with the opening boundary */
    buf.appendf("--%s", boundary);

    /* add each field */
    for (i = 0 ; i < nitems ; ++i)
    {
        /* get this item */
        const form_data_item *item = &items[i];

        /* add the content disposition */
        buf.appendf_inc(1024,
                        "\r\nContent-Disposition: form-data; name=\"%s\"",
                        item->name.get());

        /* add the filename and encoding for a file */
        if (item->fname.get() && item->fname.get()[0])
        {
            /*
             *   figure the encoding based on the type: send text/ anything
             *   as 8bit text, and send everything else as binary
             */
            const char *t = item->content_type.get();
            const char *enc = (memcmp(t, "text/", 5) == 0 ? "8bit" : "binary");

            /* add the data */
            buf.appendf_inc(1024, "; filename=\"%s\"\r\n"
                            "Content-type: %s\r\n"
                            "Content-transfer-encoding: %s",
                            item->fname.get(), t, enc);
        }

        /* end the header */
        buf.appendf_inc(1024, "\r\n\r\n");

        /* add the item's contents */
        buf.append_inc(item->val.get(), item->val.getlen(), 1024);

        /* add the end boundary */
        buf.appendf_inc(1024, "\r\n--%s", boundary);
    }

    /* add the final boundary flag */
    buf.append("--");

    /* set up the WinInet client */
    hi = InternetOpen("TADS Workbench",
                      INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
    if (hi == 0)
    {
        err = 1;
        goto done;
    }

    /* connect */
    hcon = InternetConnect(
        hi, host, INTERNET_DEFAULT_HTTP_PORT, "", "",
        INTERNET_SERVICE_HTTP, 0, 0);
    if (hcon == 0)
    {
        err = 2;
        goto done;
    }

    /* set up the request */
    hreq = HttpOpenRequest(
        hcon, "POST", "putific", "HTTP/1.1", 0, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE
        | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (hreq == 0)
    {
        err = 3;
        goto done;
    }

    /* build the headers */
    char headers[200];
    sprintf(headers, "Content-Type: multipart/form-data; boundary=%s\r\n",
            boundary);

    /* send the request */
    if (!HttpSendRequest(hreq, headers, (DWORD)-1, buf.get(), buf.getlen()))
    {
        err = 4;
        goto done;
    }

    /* read the response */
    if (!HttpQueryInfo(hreq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &status, &statusLen, 0))
    {
        err = 5;
        goto done;
    }

    /* make sure it's an HTTP 200 reply */
    if (status != 200)
    {
        err = 6;
        goto done;
    }

    /* read the reply */
    for (;;)
    {
        /* read a chunk */
        char tmp[1024];
        DWORD actual;
        if (!InternetReadFile(hreq, tmp, sizeof(tmp), &actual))
        {
            err = 7;
            goto done;
        }

        /* if we're out of data, we can stop now */
        if (actual == 0)
            break;

        /* save the data in the buffer */
        reply.append(tmp, actual);
    }

done:
    /* close handles */
    if (hreq != 0)
        InternetCloseHandle(hreq);
    if (hcon != 0)
        InternetCloseHandle(hcon);
    if (hi != 0)
        InternetCloseHandle(hi);

    /* return the error code, if any */
    return err;
}



/* ------------------------------------------------------------------------ */
/*
 *   Slight extension to TinyXML parser for convenient value extraction
 */
class TinyXML2: public TinyXML
{
public:
    int getval(const char *path, CStringBuf &val)
        { return getval(path, 0, val); }

    int getval(const char *path, int idx, CStringBuf &val)
    {
        TinyXMLElement *e = get(path, idx);
        if (e != 0)
        {
            val.set(e->contents, e->contentLen);
            return TRUE;
        }
        else
            return FALSE;
    }

    int getval(TinyXMLElement *ele, const char *path, CStringBuf &val)
        { return getval(ele, path, 0, val); }

    int getval(TinyXMLElement *ele, const char *path, int idx, CStringBuf &val)
    {
        if ((ele = ele->get(path, idx)) != 0)
        {
            val.set(ele->contents, ele->contentLen);
            return TRUE;
        }
        else
            return FALSE;
    }
};


#ifdef NOFEATURE_PUBLISH
/* ------------------------------------------------------------------------ */
/*
 *   Execute an ifdbput command.  This extracts the iFiction record from the
 *   release build of the game and sends it to IFDB.
 */
int CHtmlSys_dbgmain::ifdbput(const textchar_t *gamefile,
                              const textchar_t *ifdb_username,
                              const textchar_t *ifdb_password)
{
    CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();

    /* get the list of IF Archive upload links, if any */
    const textchar_t *links = get_build_memfile("ifdbput/links");

    /* check for a cover art file in the project */
    const textchar_t *artFile = local_config_->getval_strptr(
        "build special file", "cover art", 0);

    /* set up the multipart form data list - start with the base items */
    form_data_item items[10];
    int ni = 0;
    items[ni++].set_string("username", ifdb_username);
    items[ni++].set_string("password", ifdb_password);
    items[ni++].set_string("imageCopyrightStatus", "by permission");

    /* add the iFiction record */
    items[ni++].set_mem_file(
        "ifiction", "ifiction.xml", "text/xml",
        get_build_memfile("ifdbput/ifiction"));

    /* if there are links, add the links */
    if (links != 0)
        items[ni++].set_mem_file("links", "links.xml", "text/xml", links);

    /* if there's a cover art file, add it */
    if (artFile != 0)
    {
        if (!items[ni++].set_file("coverart", artFile, "image/jpeg"))
        {
            mainwin->dbg_printf(
                FALSE, "Error loading Cover Art file %s\n", artFile);
            return 1;
        }
    }

    /* send the form data */
    CStringBuf reply;
    TinyXML2 xprs;
    int err = http_post_multipart(
        "ifdb.tads.org", "putific", items, ni, reply);
    if (err != 0)
    {
        mainwin->dbg_printf(
            FALSE, "Error sending data to IFDB (code %d)\n", err);
        return 1;
    }
    else if (!xprs.parse(reply.get(), reply.getlen()))
    {
        mainwin->dbg_printf(
            FALSE, "Error parsing reply from IFDB (XML parsing "
            "error %d)\nXML reply follows:\n",
            (int)xprs.getError(), reply.get());
        return 1;
    }
    else
    {
        /* check the reply */
        CStringBuf val, val2;
        if (xprs.getval("error/message", val))
        {
            /* check for detail messages */
            mainwin->dbg_printf(
                FALSE, "IFDB returned the following error "
                "information:\n    %s\n", val.get());

            /* add field detail */
            TinyXMLElement *ele;
            for (int i = 0 ;
                 (ele = xprs.get("error/message/detail", i)) != 0 ;
                 ++i)
            {
                if (xprs.getval(ele, "desc", val)
                    && xprs.getval(ele, "error", val2))
                    mainwin->dbg_printf(
                        FALSE, "\n    Error in %s field: %s\n",
                        val.get(), val2.get());
            }

            /* failure */
            return 1;
        }
        else if (xprs.get("ok") != 0)
        {
            /* get a description of the action */
            const char *disposition = "(unknown action)";
            if (xprs.get("created") != 0)
                disposition = "new listing created";
            else if (xprs.get("edited") != 0)
                disposition = "existing listing updated";
            else if (xprs.get("unchanged") != 0)
                disposition = "no changes to existing listing";

            /* show the result */
            mainwin->dbg_printf(
                FALSE, "Successfully published to IFDB - %s\n",
                disposition);

            /* open the page */
            xprs.getval("viewUrl", val);
            ShellExecute(0, 0, val.get(), 0, 0, SW_SHOWNORMAL);
        }
        else
        {
            /* unrecognized reply format */
            mainwin->dbg_printf(
                FALSE, "Unrecognized reply format from IFDB "
                "(expected XML reply, got \"%s\")\n", reply.get());
            return 1;
        }
    }

    /* success */
    return 0;
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Execute a ZIP command
 */
int CHtmlSys_dbgmain::zip(const textchar_t *cmdline, const textchar_t *cmddir)
{
    CStringBuf tok;
    ZRESULT zerr = ZR_OK;
    CHtmlSys_mainwin *mainwin = CHtmlSys_mainwin::get_main_win();
    CHtmlHashTable filetab(128, new CHtmlHashFuncCI());
    textchar_t projdir[OSFNMAX];

    /* if the command directory wasn't specified, use the project directory */
    if (cmddir == 0)
    {
        os_get_path_name(projdir, sizeof(projdir),
                         local_config_filename_.get());
        cmddir = projdir;
    }

    /* get the ZIP file name */
    if (!get_cmdline_tok(&cmdline, &tok))
    {
        mainwin->dbg_printf(FALSE, "zip: no target ZIP file specified\n");
        return 1;
    }

    /* remove the file if it already exists */
    remove(tok.get());

    /* open it */
    HZIP hz = CreateZip(tok.get(), 0, ZIP_FILENAME);
    if (hz == 0)
    {
        mainwin->dbg_printf(FALSE, "error opening ZIP file %s\n", tok.get());
        return 2;
    }

    /* add each file */
    while (get_cmdline_tok(&cmdline, &tok))
    {
        CStringBuf outname;
        int expout = FALSE;
        char rel[OSFNMAX];
        char fullpath[OSFNMAX];

        /* get the file attributes */
        DWORD attr = GetFileAttributes(tok.get());
        int is_dir = (attr != INVALID_FILE_ATTRIBUTES
                      && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);

        /*
         *   if the name is given in relative notation, expand it relative to
         *   the base path
         */
        if (!os_is_file_absolute(tok.get()))
        {
            /* get the full path */
            os_build_full_path(fullpath, sizeof(fullpath), cmddir, tok.get());

            /* use this as the file token */
            tok.set(fullpath);
        }

        /* get the file's path relative to the base directory, if possible */
        os_get_rel_path(rel, sizeof(rel), cmddir, tok.get());

        /*
         *   if the next token is "::", an explicit output file name follows;
         *   otherwise, the output name is the same as the input name
         */
        if (memcmp(cmdline, ":: ", 3) == 0)
        {
            /* skip the "::" token, and read the output file name */
            cmdline += 3;
            if (!get_cmdline_tok(&cmdline, &outname))
            {
                mainwin->dbg_printf(FALSE, "invalid syntax: expected "
                                    "filename after \"::\"");
                CloseZip(hz);
                return 1;
            }

            /* we have an explicit output name */
            expout = TRUE;
        }
        else
        {
            /* check to see if this file is local to the base directory */
            if (!is_file_in_dir(tok.get(), cmddir))
            {
                /*
                 *   It's not part in the base folder, so strip the path - we
                 *   don't want to store non-local paths in the zip file.
                 */
                outname.set(os_get_root_name(tok.get()));
                expout = TRUE;

                /*
                 *   If it's a directory, don't bother including it at all.
                 *   There's no point in including a directory that isn't
                 *   local, since we'll flatten any contents in the directory
                 *   anyway.
                 */
                if (is_dir)
                    continue;
            }
            else
            {
                /* it's local - use the relative path in the ZIP file */
                outname.set(rel);
            }
        }

        /* if we've already added this input file, skip it */
        if (filetab.find(tok.get(), strlen(tok.get())) != 0)
            continue;

        /* add it to the table of files we've added */
        filetab.add(new CHtmlHashEntryCI(tok.get(), strlen(tok.get()), TRUE));

        /* add it as a file or directory, as applicable */
        if (is_dir)
        {
            /* there's no point in including '.' or '..' directories */
            if (strcmp(rel, ".") == 0 || strcmp(rel, "..") == 0)
                continue;

            /*
             *   It's a folder - add it.  Note that we don't recurse into
             *   folders; we merely add the folder object itself to the ZIP
             *   file list.  It's up to the caller to recurse into folders as
             *   desired and add their contents to the command list
             *   explicitly as individual files.
             */
            mainwin->dbg_printf(FALSE, ". adding %s\\\n", outname.get());
            if ((zerr = ZipAdd(hz, outname.get(), 0, 0, ZIP_FOLDER)) != ZR_OK)
                goto show_error;
        }
        else
        {
            /* ordinary file */
            mainwin->dbg_printf(
                FALSE, expout ? ". adding %s as %s\n" : ". adding %s\n",
                rel, outname.get());
            if ((zerr = ZipAdd(hz, outname.get(), tok.get(), 0, ZIP_FILENAME))
                != ZR_OK)
                goto show_error;
        }
    }

    /* close the file */
    if ((zerr = CloseZip(hz)) != ZR_OK)
    {
        hz = 0;
        goto show_error;
    }

    /* success */
    return 0;

show_error:
    /*
     *   close the zip file it's still open; ignore errors this time, since
     *   we already have an error to display
     */
    if (hz != 0)
        CloseZip(hz);

    /* format and display the error message from xzip */
    char msg[256];
    FormatZipMessage(zerr, msg, sizeof(msg));
    mainwin->dbg_printf(FALSE, "error: %s\n", msg);

    /* return failure */
    return 2;
}

/*
 *   Get the next token on a command line
 */
int CHtmlSys_dbgmain::get_cmdline_tok(const textchar_t **cmd, CStringBuf *tok)
{
    const textchar_t *p;
    int in_quote;

    /* skip leading whitespace */
    for (p = *cmd ; isspace(*p) ; ++p) ;

    /* if we're out of command line, there are no more tokens */
    if (*p == 0)
    {
        *cmd = p;
        return FALSE;
    }

    /*
     *   parse the token - it ends at the next whitespace, but could include
     *   quoted sections that can include spaces
     */
    for (in_quote = FALSE, tok->set("") ; *p != '\0' ; ++p)
    {
        /*
         *   if this is whitespace, and we're not in a quoted section, this
         *   is the end of the token
         */
        if (isspace(*p) && !in_quote)
            break;

        /* if it's a quote, enter or leave quotes */
        if (*p == '"')
        {
            /*
             *   if we're in a quoted section, and another quote immediately
             *   follows (i.e., the quote is "stuttered"), this counts as one
             *   literal quote
             */
            if (in_quote && *(p+1) == '"')
            {
                /* add one quote to the token */
                tok->append_inc(p, 1, 256);

                /* skip the extra quote */
                ++p;
            }
            else
            {
                /* it's not stuttered, so reverse the in-quote status */
                in_quote = !in_quote;
            }
        }
        else
        {
            /* include this character in the token */
            tok->append_inc(p, 1, 256);
        }
    }

    /* skip trailing spaces */
    for ( ; isspace(*p) ; ++p) ;

    /* tell the caller where the next token starts */
    *cmd = p;

    /* indicate that we found a token */
    return TRUE;
}

/*
 *   Is the given file local to the project?
 */
int CHtmlSys_dbgmain::is_local_project_file(const textchar_t *fname)
{
    char projdir[OSFNMAX];

    /* it's a local project file if it's in the project directory */
    get_project_dir(projdir, sizeof(projdir));
    return is_file_in_dir(fname, projdir);
}

/*
 *   Get the project directory
 */
void CHtmlSys_dbgmain::get_project_dir(textchar_t *buf, size_t buflen)
{
    /*
     *   if there's a local configuration file, its directory is the project
     *   directory; otherwise, just use the current working directory
     */
    if (local_config_filename_.get() != 0)
        os_get_path_name(buf, buflen, local_config_filename_.get());
    else
        GetCurrentDirectory(buflen, buf);
}

/*
 *   Is the given file in the given directory?
 */
int CHtmlSys_dbgmain::is_file_in_dir(
    const textchar_t *fname, const textchar_t *dir)
{
    textchar_t fullname[OSFNMAX];
    textchar_t rel[OSFNMAX];
    const textchar_t *p;
    int depth;

    /*
     *   if the file isn't given as an absolute path, get the full path
     *   relative to the project directory
     */
    if (!os_is_file_absolute(fname))
    {
        char projdir[OSFNMAX];

        /* build the full path, relative to the project folder */
        get_project_dir(projdir, sizeof(projdir));
        os_build_full_path(fullname, sizeof(fullname), projdir, fname);

        /* use this as the name we're scanning */
        fname = fullname;
    }

    /* get the path relative to the given directory */
    os_get_rel_path(rel, sizeof(rel), dir, fname);

    /*
     *   if it's still absolute, it must be on a different volume, so it's
     *   definitely not a local file
     */
    if (os_is_file_absolute(rel))
        return FALSE;

    /* parse the path to determine the depth */
    for (depth = 0, p = rel ; *p != '\0' ; )
    {
        const textchar_t *start;

        /* find the next path separator */
        for (start = p ; *p != '\0' && *p != '\\' ; ++p) ;
        size_t len = p - start;

        /* check what we have */
        if (len == 1 && start[0] == '.')
        {
            /* '.' is the current directory, so it doesn't affect depth */
        }
        else if (len == 2 && start[0] == '.' && start[1] == '.')
        {
            /* '..' goes up one level */
            --depth;

            /*
             *   if the depth ever goes negative, assume we won't get back
             *   into the target path at all
             */
            if (depth < 0)
                return FALSE;
        }
        else
        {
            /* any other directory goes down a level */
            ++depth;
        }

        /* skip the path separator if present */
        if (*p == '\\')
            ++p;
    }

    /*
     *   if we ended up with a negative depth, we're in a parent directory,
     *   so it's not in the target directory file; otherwise it is
     */
    return (depth >= 0);
}

/*
 *   Delete a temporary directory.  Returns zero on success, non-zero on
 *   failure.
 */
int CHtmlSys_dbgmain::del_temp_dir(const char *dir)
{
    char pat[MAX_PATH];
    HANDLE shnd;
    WIN32_FIND_DATA info;

    /* if the directory name is empty, ignore this */
    if (dir == 0 || dir[0] == '\0')
        return 0;

    /* set up to find all files in this directory */
    os_build_full_path(pat, sizeof(pat), dir, "*");

    /* start the search */
    shnd = FindFirstFile(pat, &info);
    if (shnd != INVALID_HANDLE_VALUE)
    {
        int err;

        /* keep going until we run out of files */
        for (;;)
        {
            /* delete all files other than "." and ".." */
            if (strcmp(info.cFileName, ".") != 0
                && strcmp(info.cFileName, "..") != 0)
            {
                char fullname[MAX_PATH];

                /* generate the full path to this file */
                os_build_full_path(fullname, sizeof(fullname),
                                   dir, info.cFileName);

                /* if it's a directory, delete it recursively */
                if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    /* delete the directory recursively */
                    if ((err = del_temp_dir(fullname)) != 0)
                        return err;
                }
                else
                {
                    /* delete this file */
                    if (!DeleteFile(fullname))
                        return GetLastError();
                }
            }

            /* move on to the next file */
            if (!FindNextFile(shnd, &info))
                break;
        }

        /* note the error that caused us to stop looping */
        err = GetLastError();

        /* close the search handle */
        FindClose(shnd);

        /* make sure we stopped because we were out of files */
        if (err != ERROR_NO_MORE_FILES)
            return err;
    }

    /* the directory should now be completely empty, so delete it */
    if (!RemoveDirectory(dir))
        return GetLastError();

    /* success */
    return 0;
}


/*
 *   Clear the list of build commands
 */
void CHtmlSys_dbgmain::clear_build_cmds()
{
    CHtmlDbgBuildCmd *cur;
    CHtmlDbgBuildCmd *nxt;

    /* delete each item in the list */
    for (cur = build_cmd_head_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next one */
        nxt = cur->next_;

        /* delete this one */
        delete cur;
    }

    /* clear the head and tail pointers */
    build_cmd_head_ = build_cmd_tail_ = 0;
    build_cmd_cnt_ = 0;

    /* clear the build file list */
    while (build_files_ != 0)
    {
        /* unlink the head item from the list */
        CHtmlDbgBuildFile *f = build_files_;
        build_files_ = build_files_->nxt;

        /* delete it */
        delete f;
    }
}


/*
 *   Add a build command at the end of the build command list
 */
void CHtmlSys_dbgmain::add_build_cmd(const textchar_t *desc,
                                     const textchar_t *exe_name,
                                     const textchar_t *exe,
                                     const textchar_t *cmdline,
                                     const textchar_t *cmd_dir,
                                     int required)
{
    CHtmlDbgBuildCmd *cmd;

    /* create a build command object */
    cmd = new CHtmlDbgBuildCmd(
        desc, exe_name, exe, cmdline, cmd_dir, required);

    /* link it in at the end of the list */
    cmd->next_ = 0;
    if (build_cmd_tail_ == 0)
        build_cmd_head_ = cmd;
    else
        build_cmd_tail_->next_ = cmd;
    build_cmd_tail_ = cmd;

    /* count it */
    ++build_cmd_cnt_;
}

/*
 *   Add a temporary in-memory file to the build list
 */
void CHtmlSys_dbgmain::add_build_memfile(const textchar_t *name,
                                         const textchar_t *val)
{
    /* create the object */
    CHtmlDbgBuildFile *f = new CHtmlDbgBuildFile(name, val);

    /* link it into the list */
    f->nxt = build_files_;
    build_files_ = f;
}

/*
 *   Append data to a temporary in-memory file
 */
void CHtmlSys_dbgmain::append_build_memfile(const textchar_t *name,
                                            const textchar_t *val)
{
    /* look up the file */
    CHtmlDbgBuildFile *f = find_build_memfile(name);

    /* append the data if we found it, otherwise create a new file */
    if (f != 0)
        f->val.append(val);
    else
        add_build_memfile(name, val);
}

/*
 *   Look up a build memory file
 */
CHtmlDbgBuildFile *CHtmlSys_dbgmain::find_build_memfile(const textchar_t *name)
{
    /* scan the list for the requested name */
    for (CHtmlDbgBuildFile *f = build_files_ ; f != 0 ; f = f->nxt)
    {
        if (f->match_name(name))
            return f;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Look up a build memory file
 */
const textchar_t *CHtmlSys_dbgmain::get_build_memfile(const textchar_t *name)
{
    /* look it up, and return the data if we find it */
    CHtmlDbgBuildFile *f = find_build_memfile(name);
    return (f != 0 ? f->val.get() : 0);
}

#ifdef NOFEATURE_PUBLISH
/* ------------------------------------------------------------------------ */
/*
 *   Publishing wizard
 */

/* HTML dialog arguments */
class PublishDialogArgs: public TadsDispatch
{
public:
    PublishDialogArgs(CHtmlSys_dbgmain *dbgmain, CHtmlDebugConfig *lc)
    {
        dbgmain_ = dbgmain;
        dbgmain_->AddRef();
        lc_ = lc;
        refcnt_ = 1;
        statobj_ = 0;
    }

    ~PublishDialogArgs()
    {
        if (dbgmain_ != 0)
            dbgmain_->Release();
        if (statobj_ != 0)
            statobj_->Release();
    }

    CHtmlSys_dbgmain *dbgmain_;
    CHtmlDebugConfig *lc_;
    IDispatch *statobj_;

    /*
     *   Remove the Close button from the active window.  The dialog calls
     *   this after it's loaded, at which point it will necessarily be the
     *   active window, since it's a modal dialog.  ShowHTMLDialog uses a
     *   window with a close box, but we want to hide that; this lets us do
     *   that by removing the SYSMENU style from the window, which has the
     *   side effect of removing the close box.
     */
    void unclose(DISPPARAMS *params, VARIANT *ret)
    {
        HWND hwnd = GetActiveWindow();
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style & ~WS_SYSMENU);
        SendMessage(hwnd, WM_NCPAINT, 1, 0);
    }

    /* get the name of the Release ZIP file */
    void zipName(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_BSTR;
        ret->bstrVal = bstr_from_ansi(lc_->getval_strptr(
            "build", "zipfile", 0));
    }

    /* run the Build Settings dialog opened to the Release ZIP file */
    void changeZip(DISPPARAMS *params, VARIANT *ret)
    {
        run_dbg_build_dlg(dbgmain_->get_handle(), dbgmain_,
                          DLG_BUILD_ZIP, IDC_FLD_ZIP);
    }

    /* get the name of the Windows SETUP file */
    void instName(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_BSTR;
        ret->bstrVal = bstr_from_ansi(lc_->getval_strptr(
            "build", "installer prog", 0));
    }

    /* run the Build Settings dialog opened to the Windows SETUP file */
    void changeInst(DISPPARAMS *params, VARIANT *ret)
    {
        run_dbg_build_dlg(dbgmain_->get_handle(), dbgmain_,
                          DLG_BUILD_INSTALL, IDC_FLD_INSTALL_EXE);
    }

    /* get the name of the Source ZIP file */
    void srcZipName(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_BSTR;
        ret->bstrVal = bstr_from_ansi(lc_->getval_strptr(
            "build", "srczipfile", 0));
    }

    /* run the Build Settings dialog opened to the Source ZIP file */
    void changeSrcZip(DISPPARAMS *params, VARIANT *ret)
    {
        run_dbg_build_dlg(dbgmain_->get_handle(), dbgmain_,
                          DLG_BUILD_SRCZIP, IDC_FLD_ZIP);
    }

    /* get the TADS major system version (2 or 3) */
    void tadsVer(DISPPARAMS *params, VARIANT *ret)
    {
        ret->vt = VT_I4;
        ret->lVal = w32_system_major_vsn;
    }

    /*
     *   publish(bool buildReleaseZip, bool buildSetup, bool buildSrcZip,
     *.          String ifdb_user, String ifdb_password, String email)
     *
     *   Execute the publication, with the selected files as IF Archive
     *   uploads.
     */
    void publish(DISPPARAMS *params, VARIANT *ret)
    {
        TdbBuildTargets t;

        /* set up the targets based on the arguments */
        t.build_zip = get_bool_arg(params, 0);
        t.build_setup = get_bool_arg(params, 1);
        t.build_srczip = get_bool_arg(params, 2);

        /* set up the IFDB credentials and email */
        t.ifdb_user = get_str_arg(params, 3);
        t.ifdb_psw = get_str_arg(params, 4);
        t.email = get_str_arg(params, 5);

        /* run the PUBLISH build */
        int ok = dbgmain_->start_build(ID_BUILD_PUBLISH, &t);

        /* free the allocated parameter strings */
        th_free((char *)t.ifdb_user);
        th_free((char *)t.ifdb_psw);
        th_free((char *)t.email);

        /* return the success indication */
        ret->vt = VT_BOOL;
        ret->boolVal = (ok != 0);
    }

    /* cancel - stop the publication process if in progress */
    void cancel(DISPPARAMS *params, VARIANT *ret)
    {
        // $$$
    }

    /*
     *   Get the current status.  Since Javascript is single-threaded, we
     *   can't send updates asynchronously to the dialog from the build
     *   thread.  Instead, the dialog sets up an interval timer to poll for
     *   updates, by calling this routine.  The build thread puts its updates
     *   into a little in-memory scratchpad area, and we simply return what's
     *   currently in the scratchpad area.
     */
    void getStatus(DISPPARAMS *params, VARIANT *ret)
    {
        CStringBufCmd buf(512);

        /* get the mutex before proceeding */
        if (WaitForSingleObject(dbgmain_->stat_mutex_, 50) == WAIT_OBJECT_0)
        {
            /*
             *   get the command count - this is the denominator in our
             *   percentage calculations, so to avoid divide-by-zero, make
             *   sure it's at least one
             */
            double ncmds = (dbgmain_->build_cmd_cnt_ >= 1
                            ? dbgmain_->build_cmd_cnt_ : 1);

            /*
             *   Calculate the progress as a percentage of the total work.
             *   It's obviously a poor approximation, but assume that each
             *   command takes an equal amount of time: so to start with,
             *   take as the base percentage the step number divided by the
             *   total number of steps.  (The first step is numbered 0, so
             *   this gives us the %done at the *start* of each step.)
             */
            double pct = 100.0 * (double)dbgmain_->stat_build_step_ / ncmds;

            /*
             *   Now add in the %done for the current step.  Each step can
             *   set this in arbitrary units, so start by calculating the
             *   percentage of those units.  Then scale this by the total
             *   length of each step in the overall doneness.  Do this only
             *   if we know the step length; the step length isn't always
             *   knowable in advance, so we'll just omit treat the whole step
             *   as atomic if we can't calculate internal progress.
             */
            if (dbgmain_->stat_step_len_ != 0)
            {
                pct += (100.0 * (double)dbgmain_->stat_step_progress_
                        / dbgmain_->stat_step_len_) / ncmds;
            }

            /* build the status string */
            buf.appendf("%s|%d|%s|%s",
                        dbgmain_->build_running_ ? "Y" : "N",
                        (int)pct,
                        dbgmain_->stat_desc_.get(),
                        dbgmain_->stat_subdesc_.get());

            /* done with the mutex */
            ReleaseMutex(dbgmain_->stat_mutex_);
        }

        /* return the buffer contents */
        ret->vt = VT_BSTR;
        ret->bstrVal = bstr_from_ansi(buf.get());
    }

    /*
     *   Get the build result.  Returns true if the build was successful,
     *   false if not.
     */
    void getResult(DISPPARAMS *params, VARIANT *ret)
    {
        /* return the buffer contents */
        ret->vt = VT_BOOL;
        ret->boolVal = (dbgmain_->stat_success_ != 0);
    }

    TADSDISP_DECL_MAP;
};

TADSDISP_MAP_BEGIN(PublishDialogArgs)
TADSDISP_MAP(PublishDialogArgs, unclose)
TADSDISP_MAP(PublishDialogArgs, zipName)
TADSDISP_MAP(PublishDialogArgs, changeZip)
TADSDISP_MAP(PublishDialogArgs, instName)
TADSDISP_MAP(PublishDialogArgs, changeInst)
TADSDISP_MAP(PublishDialogArgs, srcZipName)
TADSDISP_MAP(PublishDialogArgs, changeSrcZip)
TADSDISP_MAP(PublishDialogArgs, tadsVer)
TADSDISP_MAP(PublishDialogArgs, publish)
TADSDISP_MAP(PublishDialogArgs, cancel)
TADSDISP_MAP(PublishDialogArgs, getStatus)
TADSDISP_MAP(PublishDialogArgs, getResult)
TADSDISP_MAP_END;

/*
 *   Run the wizard
 */
void CHtmlSys_dbgmain::do_publish()
{
    /* make sure a workspace is open and we're not already building */
    if (!workspace_open_ || local_config_ == 0 || build_running_)
        return;

    /* create our arguments object */
    PublishDialogArgs *args = new PublishDialogArgs(this, local_config_);

    /* run the dialog */
    if (modal_html_dialog(handle_, "pubwiz.htm", 100, 32, args))
        MessageBox(handle_, "Error loading the Publishing Wizard",
                   "TADS Workbench", MB_OK);

    /* done with the arguments */
    args->Release();
}
#endif /* NOFEATURE_PUBLISH */


/* ------------------------------------------------------------------------ */
/*
 *   Receive notification that source window preferences have changed.
 *   We'll update the source windows accordingly.
 */
void CHtmlSys_dbgmain::on_update_srcwin_options()
{
    int new_tab_size;

    /* update each window's color settings */
    helper_->enum_source_windows(&options_enum_srcwin_cb, this);

    /* update tab size if necessary */
    if (!local_config_->getval("src win options", "tab size", &new_tab_size)
        && helper_->get_tab_size() != new_tab_size)
        helper_->set_tab_size(this, new_tab_size);

    /* reformat the source windows to account for the changes */
    helper_->reformat_source_windows(dbgctx_, this);
}

/* ------------------------------------------------------------------------ */
/*
 *   Encode a string for use as a CGI parameter.  This converts spaces to "+"
 *   marks, and encodes plus signs, quotes, ampersands, and percent signs as
 *   "%xx" sequences.
 */
static char *url_encode(const char *str)
{
    const char *p;
    char *dst;
    char *new_str;
    int i;

    /* first, calculate how much space we need for the result string */
    for (i = 0, p = str ; *p != '\0' ; ++p)
    {
        /* check for special characters */
        switch (*p)
        {
        case '"':
        case '&':
        case '%':
        case '+':
            /* encode these as %xx */
            i += 3;
            break;

        default:
            /* anything else just takes up one character */
            ++i;
            break;
        }
    }

    /* allocate a buffer */
    if ((new_str = (char *)th_malloc(i + 1)) == 0)
        return 0;

    /* encode it */
    for (dst = new_str, p = str ; *p != '\0' ; ++p)
    {
        /* check for special characters */
        switch (*p)
        {
        case '"':
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '2';
            break;

        case '&':
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '6';
            break;

        case '%':
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = '5';
            break;

        case '+':
            *dst++ = '%';
            *dst++ = '2';
            *dst++ = 'B';
            break;

        case ' ':
            *dst++ = '+';
            break;

        default:
            *dst++ = *p;
            break;
        }
    }
    *dst = '\0';

    /* return the encoded copy */
    return new_str;
}

/*
 *   Decode a URL-encoded string
 */
static char *url_decode(const char *str, size_t len)
{
    char *ret;
    char *dst;

    /*
     *   URL-encoded strings can only get shorter, since everything either
     *   gets shorter or stays the same on decode.  So, just allocate space
     *   equal to the original (plus a trailing null).
     */
    ret = (char *)th_malloc(len + 1);

    /* scan for characters requiring conversions */
    for (dst = ret ; len != 0 ; ++str, --len)
    {
        /* check what we have */
        switch (*str)
        {
        case '%':
            /* it's a %xx markup, as long as two hex digits follow */
            if (len >= 3 && isxdigit(*(str+1)) && isxdigit(*(str+2)))
            {
                /* read the two-digit sequence */
                char a = *(str+1);
                char b = *(str+2);
                a = (isdigit(a) ? a - '0' : toupper(a) - 'A' + 10);
                b = (isdigit(b) ? b - '0' : toupper(b) - 'A' + 10);

                /* calculate the character */
                *dst++ = (a << 4) | b;
            }
            else
            {
                /* it's not a valid %xx markup, so copy it literally */
                *dst++ = '%';
            }
            break;

        case '+':
            /* this indicates a space */
            *dst++ = ' ';
            break;

        default:
            /* everything else just converts literally */
            *dst++ = *str;
            break;
        }
    }

    /* null-terminate the result */
    *dst = '\0';

    /* return the new string */
    return ret;
}

/*
 *   Encode a string for HTML display.  This encodes markup-significant
 *   characters (&, <, and >) with their &entity; equivalents.
 */
static char *html_encode(const char *str, size_t len)
{
    const char *p;
    char *dst;
    char *new_str;
    int i;

    /* first, calculate how much space we need for the result string */
    for (i = 0, p = str ; p < str + len ; ++p)
    {
        /* check for special characters */
        switch (*p)
        {
        case '"':
        case '&':
            /* encode these as &#nn; */
            i += 5;
            break;

        case '<':
        case '>':
            /* encode these as &lt; and &gt; */
            i += 4;
            break;

        default:
            /* anything else goes as-is */
            ++i;
            break;
        }
    }

    /* allocate a buffer */
    if ((new_str = (char *)th_malloc(i + 1)) == 0)
        return 0;

    /* encode it */
    for (dst = new_str, p = str ; p < str + len ; ++p)
    {
        /* check for markup characters */
        switch (*p)
        {
        case '"':
            *dst++ = '&';
            *dst++ = '#';
            *dst++ = '3';
            *dst++ = '4';
            *dst++ = ';';
            break;

        case '&':
            *dst++ = '&';
            *dst++ = '#';
            *dst++ = '3';
            *dst++ = '8';
            *dst++ = ';';
            break;

        case '<':
            *dst++ = '&';
            *dst++ = 'l';
            *dst++ = 't';
            *dst++ = ';';
            break;

        case '>':
            *dst++ = '&';
            *dst++ = 'g';
            *dst++ = 't';
            *dst++ = ';';
            break;

        default:
            *dst++ = *p;
            break;
        }
    }
    *dst = '\0';

    /* return the encoded copy */
    return new_str;
}

static char *html_encode(const char *str)
{
    return html_encode(str, strlen(str));
}


/*
 *   Match a manifest prefix pattern to a filename.  The prefix pattern can
 *   contain "*" characters to match zero or more characters in the filename.
 */
static int match_prefix_pat(const char *pat, const char *fname)
{
    /* scan the strings */
    while (*fname != '\0')
    {
        /* check what this pattern character looks like */
        switch (*pat)
        {
        case '\0':
            /* we're at the end of the pattern, so we have a match */
            return TRUE;

        case '*':
            /*
             *   we have a match if we can match any of the rest of the
             *   filename to the rest of the pattern
             */
            for (++pat ; *fname != '\0' ; ++fname)
            {
                if (match_prefix_pat(pat, fname))
                    return TRUE;
            }

            /* we ran out of filename without finding a match */
            return FALSE;

        default:
            /* anything else must match exactly, but irrespective of case */
            if (tolower(*pat) != tolower(*fname))
                return FALSE;

            /* these characters match, so skip them and keep going */
            ++fname;
            ++pat;
            break;
        }
    }

    /* we ran out of filename first, so we don't have a match */
    return FALSE;
}


/*
 *   Find a documentation manifest entry for the given page
 */
static const doc_manifest_t *find_doc_manifest(
    const char *root_dir, const char *fname)
{
    const doc_manifest_t *m;
    size_t root_dir_len = strlen(root_dir);

    /* get the part of the filename relative to the root directory */
    if (strlen(fname) > root_dir_len
        && memicmp(root_dir, fname, root_dir_len) == 0
        && fname[root_dir_len] == '\\')
        fname += root_dir_len + 1;

    /* scan for a manifest that matches the path */
    for (m = w32_doc_manifest ; m->prefix != 0 ; ++m)
    {
        /* check for a prefix match to the filename */
        if (match_prefix_pat(m->prefix, fname))
        {
            /* it matches - this is our manifest entry */
            return m;
        }
    }

    /* we didn't find a match */
    return 0;
}

/*
 *   Run a documentation search
 */
void CHtmlSys_dbgmain::search_doc()
{
    char query[256];
    CHtmlSysWin_win32_docsearch *win;

    /* get the query text */
    SendMessage(searchbox_, WM_GETTEXT, sizeof(query), (LPARAM)query);

    /* if there's nothing in the search box, ignore it */
    if (query[0] == '\0')
        return;

    /* if local doc search isn't available, run a web query via tads.org */
    if (!docsearch_available())
    {
        char url[512];
        char *query_url = url_encode(query);

        /* build the URL */
        _snprintf(url, sizeof(url), w32_doc_search_url, query_url);
        url[sizeof(url)-1] = '\0';

        /* free the URL-encoded query string */
        th_free(query_url);

        /* open it in the help window */
        show_help(url);

        /* done */
        return;
    }

    /* open the search window */
    win = (CHtmlSysWin_win32_docsearch *)helper_->create_doc_search_win(this);

    /* make sure it's visible and in front */
    ShowWindow(GetParent(win->get_handle()), SW_SHOW);
    win->bring_to_front();

    /* start a new search */
    win->execute_search(query, 0);
}

/*
 *   Run a project-wide text search with parameters from a dialog
 */
void CHtmlSys_dbgmain::search_project_files()
{
    /* if no project is open, there's nothing to search */
    if (!workspace_open_)
        return;

    /* run the Find dialog, using the Project Search dialog */
    if (!projsearch_dlg_->run_find_dlg(get_handle(), DLG_FILEFIND,
                                       find_text_, sizeof(find_text_)))
    {
        /* the user canceled the dialog - we're done */
        return;
    }

    /* run the search */
    search_project_files(
        find_text_, projsearch_dlg_->exact_case,
        projsearch_dlg_->regex, projsearch_dlg_->whole_word,
        projsearch_dlg_->collapse_sp);
}

/*
 *   Run a project-wide text search
 */
void CHtmlSys_dbgmain::search_project_files(
    const textchar_t *query, int exact_case, int regex,
    int whole_word, int collapse_sp)
{
    /*
     *   if we have a project window, and a project is loaded, search the
     *   files in the project list
     */
    if (projwin_ != 0 && workspace_open_)
    {
        /* enumeration callback for 'grep' */
        class EnumCB: public FileSearcher, public IProjWinFileCallback
        {
        public:
            EnumCB(CHtmlSys_dbgmain *dbgmain, const textchar_t *query,
                   int exact_case, int regex, int whole_word, int collapse_sp)
                : FileSearcher(query, exact_case, regex, whole_word,
                               collapse_sp)
            {
                /* remember our debugger main object */
                this->dbgmain = dbgmain;
                helper = dbgmain->get_helper();

                /* open the search window */
                win = (CHtmlSysWin_win32_filesearch *)
                      helper->create_file_search_win(dbgmain);

                /* make sure it's visible and in front */
                ShowWindow(GetParent(win->get_handle()), SW_SHOW);
                win->bring_to_front();

                /* get some information from the helper object */
                link = helper->get_file_search_link();

                /* notify it of the start of a new search */
                win->begin_search();

                /* no matches yet */
                total_matches = 0;
            }

            ~EnumCB()
            {
                /* add a note if there are no matches */
                if (total_matches == 0)
                    report("No matches found.");

                /* flush any buffered reports */
                report("<br><br>");
                flush_report();

                /* notify our window that the search is over */
                win->end_search();

                /* clear the long-running process status */
                dbgmain->set_lrp_status(0);
            }

            /* process an enumerated file from the project list */
            void file_cb(const textchar_t *fname, const textchar_t *relname,
                         proj_item_type_t typ)
            {
                int matches;
                char buf[OSFNMAX + 30];

                /* show the filename in the lrp status */
                sprintf(buf, "Searching %s...", relname);
                dbgmain->set_lrp_status(buf);

                /* run the search on this file */
                matches = search_file(fname);

                /* if we found any matches, end the table */
                if (matches > 0)
                {
                    reportf("</table>");
                    flush_report();
                }

                /* add this file's matches into the overall total */
                total_matches += matches;
            }

            /* report a file error */
            void report_file_error(
                const textchar_t *fname, const textchar_t *errmsg)
            {
                report("<font bgcolor=red color=white><b>");
                report_literal(fname);
                reportf("</b>: %s</font><br>", errmsg);
            }

            /* report a match */
            int report_match(const textchar_t *fname, int linenum,
                             int matchnum,
                             const textchar_t *linetxt, size_t linelen)
            {
                /* if this is the first match, show the filename */
                if (matchnum == 1)
                {
                    reportf("<a plain href=\"file:%s\"><b>", fname);
                    report_literal(fname);
                    report("</b></a><br><table width=100%>");
                }

                /* report the match */
                reportf("<tr><td align=right valign=top width=64>"
                        "<a plain href=\"line:%d:%s\">%d</a>:"
                        "<td valign=top>"
                        "<a plain href=\"line:%d:%s\">",
                        linenum, fname, linenum,
                        linenum, fname);
                report_literal(linetxt, linelen);
                report("</a>");

                /*
                 *   keep looking for additional matches, since we want to
                 *   list each line containing a match
                 */
                return TRUE;
            }

            /* display a printf-style formatted message */
            void reportf(const textchar_t *fmt, ...)
            {
                va_list va;
                char buf[1024];

                va_start(va, fmt);
                _vsnprintf(buf, sizeof(buf), fmt, va);
                buf[sizeof(buf) - 1] = '\0';
                va_end(va);

                report(buf);
            }

            /* display a message, encoding any markup characters */
            void report_literal(const textchar_t *msg)
                { report_literal(msg, strlen(msg)); }
            void report_literal(const textchar_t *msg, size_t len)
            {
                char *h = html_encode(msg, len);
                report(h);
                th_free(h);
            }

            /* display a message */
            void report(const textchar_t *msg)
            {
                txtbuf.append(msg);
            }

            /* flush the report so far */
            void flush_report()
            {
                /* send the text to the source manager */
                link->srcmgr_->append_line(txtbuf.getbuf(), txtbuf.getlen());
                txtbuf.clear();

                /* format the window */
                win->do_formatting(FALSE, FALSE, TRUE);
                win->fmt_adjust_vscroll();
            }

            /* our dbgmain object and the debug helper */
            CHtmlSys_dbgmain *dbgmain;
            CHtmlDebugHelper *helper;

            /* the file search window */
            CHtmlSysWin_win32_filesearch *win;
            struct CHtmlDbg_win_link *link;

            /* html text output buffer */
            class CHtmlTextBuffer txtbuf;

            /* total number of matches */
            int total_matches;
        };

        /* set up the project file enumerator callback */
        EnumCB cb(this, query, exact_case, regex, whole_word, collapse_sp);

        /* get the query in HTML notation */
        char *query_html = html_encode(query);

        /* set up the header */
        cb.report("<body link=#0000ff vlink=#008080 alink=#ff00ff>"
                  "<p><table width=100% bgcolor=#e0e0e0><tr><td>"
                  "<font color=black>Search Results for <b>");
        cb.report(query_html);
        cb.report("</b></font></table><br><br>");

        /* done with the query text */
        th_free(query_html);

        /* if the regular expression didn't parse, fail */
        if (regex && cb.rexstat != RE_STATUS_SUCCESS)
        {
            cb.report("The regular expression syntax is invalid\n");
            return;
        }

        /* enumerate the project's source files through our callback object */
        projwin_->enum_text_files(&cb);
    }
}

/*
 *   Resume execution
 */
void CHtmlSys_dbgmain::exec_go()
{
    /* set the new execution state */
    helper_->set_exec_state_go(dbgctx_);

    /* tell the event loop to relinquish control */
    go_ = TRUE;
}

/*
 *   Resume execution for a single step
 */
void CHtmlSys_dbgmain::exec_step_into()
{
    /* set the new execution state */
    helper_->set_exec_state_step_into(dbgctx_);

    /* tell the event loop to return control to the run-time */
    go_ = TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   touch a file - set the last write time to the current time
 */
static void touch_file(const textchar_t *fname)
{
    /* get a handle to the file */
    HANDLE hf = CreateFile(fname, FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                           OPEN_EXISTING, 0, 0);
    if (hf != 0)
    {
        SYSTEMTIME st;
        FILETIME ft;

        /* set the file's mod date tot he current time */
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &ft);
        SetFileTime(hf, 0, 0, &ft);

        /* done with the file */
        CloseHandle(hf);
    }
}

/*
 *   Replay a script.  If a run is already in progress, we'll terminate it;
 *   then, we'll start a new run using the given script as the command input
 *   file.
 */
void CHtmlSys_dbgmain::run_script(const textchar_t *fname, int is_temp_file)
{
    /*
     *   remember the script file - we'll pass this along to the interpreter
     *   via the command-line option to set the command input file
     */
    replay_script_.set(fname);
    is_replay_script_temp_ = is_temp_file;

    /* 'touch' this script file, so that it sorts to the top of the list */
    touch_file(fname);

    /* refresh the list to account for the time change */
    if (scriptwin_ != 0)
    {
        const textchar_t *dir = local_config_->getval_strptr(
            "auto-script", "dir", 0);
        scriptwin_->sync_list(dir != 0 ? dir : "Scripts");
    }

    /*
     *   handle this as a 'restart' - this will terminate the current session
     *   if one is active, then start a new session
     */
    PostMessage(handle_, WM_COMMAND, ID_FILE_RESTARTGAME, 0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Generate a name for a new auto-script file
 */
int CHtmlSys_dbgmain::name_auto_script(textchar_t *fname, size_t fname_size)
{
    CHtmlDebugConfig *lc = local_config_;
    const textchar_t *dir;
    int id;
    int iter;

    /* if we're not using auto-scripts, skip this */
    if (!lc->getval_int("auto-script", "enabled", TRUE))
        return FALSE;

    /* get the script directory name */
    if ((dir = lc->getval_strptr("auto-script", "dir", 0)) == 0)
        dir = "Scripts";

    /* make sure the directory exists */
    if (GetFileAttributes(dir) == INVALID_FILE_ATTRIBUTES
        && !CreateDirectory(dir, 0))
        return FALSE;

    /* look for the next available filename */
    for (id = lc->getval_int("auto-script", "next-id", 1), iter = 0 ;
         iter < 100 ; ++id)
    {
        char buf[50];

        /* generate the filename */
        sprintf(buf, "Auto %d.cmd", id);
        os_build_full_path(fname, fname_size, dir, buf);

        /* if it doesn't exist, use this name */
        if (GetFileAttributes(fname) == INVALID_FILE_ATTRIBUTES)
        {
            /* remember the new high-water mark */
            lc->setval("auto-script", "next-id", id + 1);

            /* success */
            return TRUE;
        }
    }

    /* we ran too many iterations - give up */
    return FALSE;
}

/*
 *   Add a script file to the UI list
 */
void CHtmlSys_dbgmain::add_file_to_script_win(const textchar_t *fname)
{
    /* if we have a script window, ask it to add the file */
    if (scriptwin_ != 0)
    {
        /* add the script to the window */
        scriptwin_->add_script(fname);

        /* prune the list to remove older scripts, if desired */
        prune_script_dir();
    }
}

/*
 *   Remove a file from the script window
 */
void CHtmlSys_dbgmain::remove_file_from_script_win(const textchar_t *fname)
{
    if (scriptwin_ != 0)
        scriptwin_->remove_script(fname);
}

/* ------------------------------------------------------------------------ */
/*
 *   enumeration callback for applying options changes to source windows
 */
void CHtmlSys_dbgmain::options_enum_srcwin_cb(void *ctx, int /*idx*/,
                                              IDebugWin *win,
                                              int /*line_source_id*/,
                                              HtmlDbg_wintype_t win_type)
{
    /*
     *   if it's a source or information window, notify the window of the
     *   option changes
     */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
    case HTMLDBG_WINTYPE_STACK:
    case HTMLDBG_WINTYPE_HISTORY:
    case HTMLDBG_WINTYPE_DEBUGLOG:
    case HTMLDBG_WINTYPE_DOCSEARCH:
    case HTMLDBG_WINTYPE_FILESEARCH:
        /*
         *   these are all derived from the basic source window type, so
         *   these all need to be notified of formatting changes
         */
        ((CHtmlSys_dbgmain *)ctx)
            ->note_formatting_changes((IDebugSysWin *)win);
        break;

    case HTMLDBG_WINTYPE_HELP:
    default:
        /* other types are not affected by formatting changes */
        break;
    }
}

/*
 *   Receive notification of global formatting option changes, updating the
 *   given window accordingly.
 */
void CHtmlSys_dbgmain::note_formatting_changes(IDebugSysWin *win)
{
    HTML_color_t bkg_color, text_color, sel_bkg_color, sel_text_color;
    int use_win_colors, use_win_bg;
    int use_win_sel, use_win_sel_bg;

    /* get the color preferences */
    use_win_colors = global_config_->getval_int(
        "src win options", "use win colors", TRUE);
    use_win_bg = global_config_->getval_int(
        "src win options", "use win bgcolor", use_win_colors);

    if (global_config_->getval_color("src win options", "bkg color",
                                     &bkg_color))
        bkg_color = HTML_make_color(0xff, 0xff, 0xff);
    if (global_config_->getval_color("src win options", "text color",
                                     &text_color))
        text_color = HTML_make_color(0, 0, 0);

    /* get the selected text color preferences */
    use_win_sel = global_config_->getval_int(
        "src win options", "use win sel colors", TRUE);
    use_win_sel_bg = global_config_->getval_int(
        "src win options", "use win sel bgcolor", use_win_sel);

    if (global_config_->getval_color(
        "src win options", "sel bkg color", &sel_bkg_color))
        sel_bkg_color = HTML_make_color(0xff, 0xff, 0xff);
    if (global_config_->getval_color(
        "src win options", "sel text color", &sel_text_color))
        sel_text_color = HTML_make_color(0, 0, 0);

    /* apply the current color settings to the window */
    win->idsw_note_debug_format_changes(
        bkg_color, text_color, use_win_colors, use_win_bg,
        sel_bkg_color, sel_text_color, use_win_sel, use_win_sel_bg);
}

/*
 *   Note that a file has been saved from the text editor.  This will check
 *   to see if the file is part of the project, and if so, it'll set the
 *   maybe_need_build_ flag to indicate that we've saved changes to project
 *   files since the last build.
 */
void CHtmlSys_dbgmain::note_file_save(const textchar_t *fname)
{
    int found = FALSE;

    /*
     *   if the file is part of the project's file list, set the
     *   may-need-build flag to indicate that we've saved changes since the
     *   last build
     */
    if (projwin_ != 0)
    {
        /* we have a project window, so ask it about the file */
        found = projwin_->is_file_in_project(fname);

        /* if we found it, notify the project window of the update */
        if (found)
            projwin_->note_file_save(fname);
    }
    else
    {
        /*
         *   there's no project window, so look for the file among the
         *   debugger's list of source files
         */
        struct cbstruct
        {
            cbstruct(CHtmlSys_dbgmain *dbgmain, const textchar_t *fname)
            {
                /*
                 *   remember the main debugger object and the file we're
                 *   looking for
                 */
                this->dbgmain = dbgmain;
                this->fname = fname;

                /* we haven't found the file we're after yet */
                this->found = FALSE;
            }
            CHtmlSys_dbgmain *dbgmain;
            const textchar_t *fname;
            int found;

            static void cbfunc(void *ctx, const textchar_t *fname, int srcid)
            {
                /* the context is the 'this' object */
                cbstruct *self = (cbstruct *)ctx;

                /*
                 *   if this file matches the file we're looking for, note
                 *   that we've found the file
                 */
                if (self->dbgmain->dbgsys_fnames_eq(fname, self->fname))
                    self->found = TRUE;
            }
        };

        /* scan the list of source files mentioned in the compiled gamee */
        cbstruct cb(this, fname);
        helper_->enum_source_files(&cb.cbfunc, &cb);

        /* note whether we found the file */
        found = cb.found;
    }

    /* if we found the file in the project, set the need-build flag */
    if (found)
        maybe_need_build_ = TRUE;
}

/*
 *   Get the last active window for command routing.  If there's an active
 *   debugger window, we'll return that window.  If the debugger main
 *   window is itself the active window, and the previous active debugger
 *   window is the second window in Z order, we'll return that window,
 *   since we left it explicitly to come to the debugger main window,
 *   presumably for a toolbar or menu operation.
 */
CHtmlDbgSubWin *CHtmlSys_dbgmain::get_active_dbg_win()
{
    /* if we have an active debugger window, use it */
    if (active_dbg_win_ != 0)
        return active_dbg_win_;

#ifdef W32TDB_MDI
    /*
     *   if that last debug window wasn't an MDI child, but is a child of the
     *   main frame, it's a docked window, so use it as the top owned window
     */
    if (last_active_dbg_win_ != 0)
    {
        /* get its handle */
        HWND hlast = last_active_dbg_win_->active_get_handle();

        /* check to see if it's a non-MDI child of the frame */
        if (!IsChild(get_parent_of_children(), hlast)
            && IsChild(handle_, hlast))
            return last_active_dbg_win_;
    }
#endif

    /*
     *   If a debug window was previously active, and the main debugger
     *   window itself is the system's idea of the active window, and the
     *   previously active debug window is our main debug window's top
     *   "owned" window or a child of our top owned window, then consider
     *   that previous window to be still active.  In this case, route the
     *   command to that previous window.
     */
    if (last_active_dbg_win_ != 0 && GetActiveWindow() == handle_)
    {
        HWND top_owned;

        /*
         *   find the top "owned" window, according to our MDI/SDI mode
         */
#ifdef W32TDB_MDI
        /* ask the MDI client for its active window */
        top_owned = (HWND)SendMessage(get_parent_of_children(),
                                      WM_MDIGETACTIVE, 0, 0);

#else /* W32TDB_MDI */
        /* find the top window that I own */
        for (top_owned = GetTopWindow(0) ;
             top_owned != 0 && GetWindow(top_owned, GW_OWNER) != handle_ ;
             top_owned = GetWindow(top_owned, GW_HWNDNEXT)) ;
#endif /* W32TDB_MDI */

        /* check the top-owned window against the active window */
        if (top_owned != 0
            && (top_owned == last_active_dbg_win_->active_get_handle()
                || IsChild(top_owned,
                           last_active_dbg_win_->active_get_handle())))
            return last_active_dbg_win_;
    }

    /* there isn't an appropriate window */
    return 0;
}

/*
 *   Bring the window to the front
 */
void CHtmlSys_dbgmain::bring_to_front()
{
    w32_webui_yield_foreground();
    SetForegroundWindow(handle_);
    BringWindowToTop(handle_);
}

/*
 *   Schedule a timer to bring the game window to the front if it's running
 *   when the timer fires.
 */
void CHtmlSys_dbgmain::sched_game_to_front()
{
    if (handle_ != 0)
        SetTimer(handle_, game_to_front_timer_, 250, 0);
}

/*
 *   Receive notification of a change to a child window title
 */
void CHtmlSys_dbgmain::update_win_title(class CTadsWin *win)
{
    int idx;

    /* find the window in the MDI tab bar */
    if ((idx = mdi_tab_->find_item_by_param(win)) != -1)
    {
        char buf[MAX_PATH];

        /* get the window title */
        GetWindowText(win->get_handle(), buf, sizeof(buf));

        /* set the tab title to the root filename part of the window title */
        mdi_tab_->set_item_title(idx, os_get_root_name(buf));
    }
}

/*
 *   Set the active debugger window.  Source, stack, and other debugger
 *   windows should set themselves active when they're activated, and
 *   should clear the active window when they're deactivated.  We keep
 *   track of the active debugger window for command routine purposes.
 */
void CHtmlSys_dbgmain::set_active_dbg_win(CHtmlDbgSubWin *win)
{
    /* remember the active window */
    active_dbg_win_ = win;

    /*
     *   in case the new window isn't a source window, clear the source
     *   line/column number panel - if it does turn out to be a source
     *   window, it'll update the panel itself
     */
    update_cursorpos();

    /*
     *   Adjust the active key bindings.  Treat this as activation of the
     *   subwindow only if the subwindow is active (or a child of the active
     *   window) or I'm active.
     */
    HWND act = GetActiveWindow();
    update_key_bindings(act == get_handle()
                        || (win != 0
                            && (act == win->active_get_handle()
                                || IsChild(act, win->active_get_handle()))));

    /* update buttons in the Find dialog, if it's open */
    if (find_dlg_ != 0)
        find_dlg_->adjust_context_buttons();
}

/*
 *   Update the cursor position
 */
void CHtmlSys_dbgmain::update_cursorpos(int lin, int col)
{
    /* format the line/column information (use 1-based numbering) */
    char buf[50];
    sprintf(buf, " Ln %ld  Col %ld ", lin + 1, col + 1);

    /* update the position */
    update_cursorpos(buf);
}

void CHtmlSys_dbgmain::update_cursorpos(const char *txt)
{
    /* set the new text in our status line source */
    cursor_pos_source_.set(txt);

    /* update the status line */
    if (statusline_ != 0)
        statusline_->update();
}

/*
 *   Clear the active debugger window.  Windows that register themselves
 *   upon activation with set_active_dbg_win() should call this routine
 *   when they're deactivated.
 */
void CHtmlSys_dbgmain::clear_active_dbg_win()
{
    /* if there's already no active window, there's nothing to do */
    if (active_dbg_win_ == 0)
        return;

    /*
     *   Remember this as the previous active debugger window, and the last
     *   command target window.  When the debugger main window is the active
     *   window, and the previously active window is the next window in
     *   window order, we'll route commands that belong to debugger windows
     *   to the old active window.
     */
    last_active_dbg_win_ = last_cmd_target_win_ = active_dbg_win_;

    /* forget the window */
    set_active_dbg_win(0);
}

/*
 *   Set a null active debug window.  This clears all traces of an active
 *   window; it's used when a window becomes active, and that window behaves
 *   like an active window but doesn't implement the CHtmlDbgSubWin
 *   interface.
 */
void CHtmlSys_dbgmain::set_null_active_dbg_win()
{
    /* forget the active window, AND the last active window */
    last_active_dbg_win_ = 0;
    set_active_dbg_win(0);
}

/*
 *   Receive notification that we're closing a debugger window.  Any
 *   window that registers itself with set_active_dbg_win() on activation
 *   must call this routine when the window is destroyed, to ensure that
 *   we don't try to route any commands to the window after it's gone.
 */
void CHtmlSys_dbgmain::on_close_dbg_win(CHtmlDbgSubWin *win)
{
    /*
     *   if this is the old active window or the current active window,
     *   forget about it
     */
    if (active_dbg_win_ == win)
        active_dbg_win_ = 0;
    if (last_active_dbg_win_ == win)
        last_active_dbg_win_ = 0;
    if (last_cmd_target_win_ == win)
        last_cmd_target_win_ = 0;
    if (find_start_.win == win)
        find_start_.set_win(0);
    if (find_last_.win == win)
        find_last_.set_win(0);

    /* update buttons in the Find dialog, if it's open */
    if (find_dlg_ != 0)
        find_dlg_->adjust_context_buttons();

    /* update the key bindings */
    update_key_bindings(FALSE);
}

/*
 *   Open a source file on behalf of the debug log window
 */
void CHtmlSys_dbgmain::dbghostifc_open_source(
    const textchar_t *fname, long linenum, long colnum, int select_line)
{
    IDebugSysWin *win;
    char path[MAX_PATH];

    /* if the filename isn't absolute, look for it in the usual places */
    if (os_is_file_absolute(fname))
    {
        /* the name is already an absolute path - use it as-is */
        strcpy(path, fname);
    }
    else
    {
        /* look for the file - fail if we can't */
        if (!html_dbgui_find_src(fname, strlen(fname),
                                 path, sizeof(path), TRUE))
            return;
    }

    /* open the source file in a viewer window */
    win = (IDebugSysWin *)helper_
          ->open_text_file_at_line(this, fname, path, linenum);

    /* if we found the window, do some more work */
    if (win != 0)
    {
        /* bring the window to the front */
        win->idsw_bring_to_front();

        /* if desired, select the entire line */
        if (select_line)
            win->idsw_select_line();
    }
}

/*
 *   Open a file for editing in the external text editor.  Returns true on
 *   success, false on error.
 */
int CHtmlSys_dbgmain::open_in_text_editor(const textchar_t *fname,
                                          long linenum, long colnum,
                                          int local_if_no_linenum)
{
    char prog[1024];
    char args[1024];
    char *p;
    char *seg;
    CStringBuf expanded_args;
    int success;
    int found_pctf;
    int found_pctn;
    char numbuf[20];
    char path[OSFNMAX];

    /*
     *   if the filename isn't absolute, try to find it in the usual
     *   places
     */
    if (os_is_file_absolute(fname))
    {
        /* the name is already an absolute path - use it as-is */
        strcpy(path, fname);
    }
    else
    {
        /* look for the file - fail if we can't */
        if (!html_dbgui_find_src(fname, strlen(fname),
                                 path, sizeof(path), TRUE))
            return FALSE;
    }

    /* get the editor program - use NOTEPAD as the default */
    if (global_config_->getval("editor", "program", 0, prog, sizeof(prog))
        || strlen(prog) == 0)
        strcpy(prog, "Notepad.EXE");

    /* get the editor command line - use "%f" as the default */
    if (global_config_->getval("editor", "cmdline", 0, args, sizeof(args))
        || strlen(args) == 0)
        strcpy(args, "%f");

    /* presume we won't find a %f or %n substitution parameters */
    found_pctf = FALSE;
    found_pctn = FALSE;

    /* start the command line empty */
    expanded_args.set("");

    /* substitute the "%" parameters */
    for (seg = p = args ; *p != '\0' ; ++p)
    {
        /* if this is a percent sign, substitute the parameter */
        if (*p == '%')
        {
            long adjust_amt = 0;

            /* append the part up to this point */
            if (p > seg)
                expanded_args.append(seg, p - seg);

            /* skip the '%' */
            ++p;

        apply_subst_code:
            /* see what we have */
            switch(*p)
            {
            case 'f':
                /* substitute the filename */
                expanded_args.append(path);

                /* note that we found a filename */
                found_pctf = TRUE;
                break;

            case '0':
                /*
                 *   zero-base adjustment - if we have another character
                 *   following, note the -1 adjustment (since we want to pass
                 *   a zero-based value to the program, but we were passed
                 *   1-based values), and go back to apply the actual code
                 *   from the next character
                 */
                if (*(p+1) != '\0')
                {
                    /* adjust for the zero base */
                    adjust_amt = -1;

                    /* skip the '0' qualifier */
                    ++p;

                    /* go apply the real substitution code */
                    goto apply_subst_code;
                }
                break;

            case 'n':
                /* substitute the line number */
                sprintf(numbuf, "%lu", linenum + adjust_amt);
                expanded_args.append(numbuf);

                /* note that we found a line number specifier */
                found_pctn = TRUE;
                break;

            case 'c':
                /* substitute the column number */
                sprintf(numbuf, "%lu", colnum + adjust_amt);
                expanded_args.append(numbuf);
                break;

            case '%':
                /* substitute a single % */
                expanded_args.append("%");
                break;

            default:
                /* leave anything else intact */
                expanded_args.append("%");
                --p;
                break;
            }

            /* the next segment starts at the next character */
            seg = p + 1;
        }
    }

    /* append anything remaining after the last parameter */
    if (p > seg)
        expanded_args.append(seg, p - seg);

    /* if we didn't find a "%f", append the filename */
    if (!found_pctf)
        expanded_args.append(path);

    /*
     *   If we didn't find a line number specifier in the command string,
     *   we can't go to the selected line in the external editor.  If the
     *   caller requested that we open the file locally in such a case,
     *   open the file locally.
     */
    if (!found_pctn && local_if_no_linenum)
    {
        IDebugSysWin *win;

        /* open the source file in a viewer window */
        win = (IDebugSysWin *)helper_
              ->open_text_file_at_line(this, fname, path, linenum);

        /* bring the window to the front */
        if (win != 0)
            win->idsw_bring_to_front();

        /* done */
        return TRUE;
    }

    /* presume failure */
    success = FALSE;

    /*
     *   if it's a DDE command, execute it through DDE; otherwise, it's
     *   just a program to invoke as an application
     */
    if (strlen(prog) > 4 && memicmp(prog, "DDE:", 4) == 0)
    {
        dde_info_t dde_info;
        char *server;
        char *topic;
        char *exename;
        char *p;
        int tries;

        /*
         *   parse the string to find the server and topic names - the
         *   server is the part from the character after the "DDE:" prefix
         *   to the comma, and the topic is the part after the comma
         */
        server = prog + 4;
        for (p = server ; *p != '\0' && *p != ',' ; ++p) ;
        if (*p == ',')
            *p++ = '\0';
        topic = p;

        /*
         *   look for another comma, which if present introduces the program
         *   to launch if the DDE server isn't already running
         */
        for ( ; *p != '\0' && *p != ',' ; ++p) ;
        if (*p == ',')
            *p++ = '\0';
        exename = p;

        /*
         *   try connecting via DDE twice - once before launching the
         *   program, then again if we fail the first time and can launch
         *   the program explicitly
         */
        for (tries = 0 ; tries < 2 && !success ; ++tries)
        {
            /* initialize a DDE conversation */
            if (dde_open_conv(&dde_info, server, topic) == 0)
            {
                /* send the command string */
                if (dde_exec(&dde_info, expanded_args.get()) == 0)
                    success = TRUE;
            }

            /* close the conversation */
            dde_close_conv(&dde_info);

            /*
             *   If we failed, and the DDE string contained a third field
             *   with an executable name, launch the executable and try the
             *   conversation again.
             */
            if (!success && *exename != '\0')
            {
                SHELLEXECUTEINFO exinfo;

                /* try launching the program */
                exinfo.cbSize = sizeof(exinfo);
                exinfo.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
                exinfo.hwnd = 0;
                exinfo.lpVerb = "open";
                exinfo.lpFile = exename;
                exinfo.lpParameters = "";
                exinfo.lpDirectory = 0;
                exinfo.nShow = SW_SHOWNORMAL;
                if (!ShellExecuteEx(&exinfo)
                    || (unsigned long)exinfo.hInstApp <= 32
                    || exinfo.hProcess == 0)
                {
                    /*
                     *   we couldn't even do that - don't bother retrying
                     *   the DDE command
                     */
                    break;
                }
                else
                {
                    DWORD wait_stat;

                    /*
                     *   wait until the new program finishes initializing
                     *   before attempting to contact it again
                     */
                    wait_stat = WaitForInputIdle(exinfo.hProcess, 5000);

                    /* we don't need the process handle any more */
                    CloseHandle(exinfo.hProcess);

                    /* if the wait failed, don't bother trying DDE again */
                    if (wait_stat != 0)
                        break;
                }
            }
        }

        /* if we didn't succeed, show an error */
        if (!success)
        {
            MessageBox(handle_, "Error sending DDE command to text editor "
                       "application.  Check the Advanced settings "
                       "in the Editor page in the Options dialog.",
                       "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
    }
    else
    {
        char filedir[OSFNMAX];

        /* get the directory containing our file */
        os_get_path_name(filedir, sizeof(filedir), path);

        /* invoke the editor */
        if ((unsigned long)ShellExecute(0, "open", prog,
                                        expanded_args.get(), filedir,
                                        SW_SHOWNORMAL) <= 32)
        {
            /* show the error */
            MessageBox(handle_, "Error executing text editor application.  "
                       "Check the Editor settings in the Options "
                       "dialog.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
        else
        {
            /* note the success */
            success = TRUE;
        }
    }

    /* return success indication */
    return success;
}


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbench implementation
 */
HRESULT STDMETHODCALLTYPE CHtmlSys_dbgmain::QueryInterface(
    REFIID iid, void **ip)
{
    /* check for ITadsWorkbench */
    if (iid == IID_ITadsWorkbench)
    {
        /* hand the caller our interface pointer, suitably cast */
        *ip = (void *)(ITadsWorkbench *)this;

        /* add a reference on the caller's behalf */
        AddRef();

        /* indicate success */
        return S_OK;
    }

    /* we didn't handle it, so inherit our base class implementation */
    return CHtmlSys_framewin::QueryInterface(iid, ip);
}

BSTR STDMETHODCALLTYPE CHtmlSys_dbgmain::GetPropValStr(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey, UINT idx)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA;
    BSTR ret;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return 0;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);

    /* look up the value */
    ret = bstr_from_ansi(c->getval_strptr(keyA, subkeyA, (int)idx));

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);

    /* return the result */
    return ret;
}

UINT STDMETHODCALLTYPE CHtmlSys_dbgmain::GetPropValStrCount(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA;
    UINT ret;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return 0;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);

    /* get the string count */
    ret = c->get_strlist_cnt(keyA, subkeyA);

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);

    /* return the result */
    return ret;
}

void STDMETHODCALLTYPE CHtmlSys_dbgmain::ClearPropValStr(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);

    /* clear the string list */
    c->clear_strlist(keyA, subkeyA);

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);
}

void CHtmlSys_dbgmain::SetPropValStr(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey,
    UINT idx, const OLECHAR *val)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA, *valA;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);
    valA = ansi_from_olestr(val);

    /* set the value */
    c->setval(keyA, subkeyA, (int)idx, valA, get_strlen(valA));

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);
    th_free(valA);
}

/*
 *   Get an integer property
 */
INT STDMETHODCALLTYPE CHtmlSys_dbgmain::GetPropValInt(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey, INT default_val)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA;
    int val;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return FALSE;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);

    /* set the value */
    if (c->getval(keyA, subkeyA, &val))
        val = default_val;

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);

    /* return the value */
    return val;
}

/*
 *   Set an integer property
 */
void STDMETHODCALLTYPE CHtmlSys_dbgmain::SetPropValInt(
    BOOL global, const OLECHAR *key, const OLECHAR *subkey, INT val)
{
    CHtmlDebugConfig *c;
    textchar_t *keyA, *subkeyA;

    /* get the configuration object */
    if ((c = global ? global_config_ : local_config_) == 0)
        return;

    /* get ANSI copies of the strings */
    keyA = ansi_from_olestr(key);
    subkeyA = ansi_from_olestr(subkey);

    /* set the value */
    c->setval(keyA, subkeyA, val);

    /* free the strings */
    th_free(keyA);
    th_free(subkeyA);
}


UINT STDMETHODCALLTYPE CHtmlSys_dbgmain::GetCommandID(
    const OLECHAR *command_name)
{
    /* get the name in ANSI format */
    textchar_t *aname = ansi_from_olestr(command_name);

    /* find the hash table entry */
    CmdNameEntry *entry = (CmdNameEntry *)cmdname_hash->find(
        aname, get_strlen(aname));

    /* note the command ID, if we found it; if not, the result is zero */
    int ret = (entry != 0 ? entry->inf->id : 0);

    /* free the ANSI string */
    th_free(aname);

    /* return the ID we found */
    return ret;
}

UINT STDMETHODCALLTYPE CHtmlSys_dbgmain::DefineCommand(
    const OLECHAR *name, const OLECHAR *desc)
{
    int ret;

    /* get the strings in ANSI format */
    textchar_t *aname = ansi_from_olestr(name);
    textchar_t *adesc = ansi_from_olestr(desc);

    /* look for an existing entry */
    CmdNameEntry *entry = (CmdNameEntry *)cmdname_hash->find(
        aname, get_strlen(aname));

    /* if we found an entry, just return its ID; otherwise, create it */
    if (entry != 0)
    {
        /* got it - use the existing ID */
        ret = entry->inf->id;

        /*
         *   if the old description is empty, we must have created this as a
         *   placeholder before loading extensions; in this case, use the new
         *   description
         */
        if (get_strlen(entry->inf->get_desc()) == 0)
            entry->inf->set_desc(adesc, get_strlen(adesc));
    }
    else if (next_custom_cmd_ >= ID_CUSTOM_LAST)
    {
        /* we need a new entry, but we're out of IDs - reject it */
        ret = 0;
    }
    else
    {
        /* we need a new entry - create it */
        ret = define_ext_command(aname, get_strlen(aname),
                                 adesc, get_strlen(adesc))->id;
    }

    /* free the ANSI strings */
    th_free(aname);
    th_free(adesc);

    /* return the new ID */
    return ret;
}

/*
 *   Define an extension command
 */
HtmlDbgCommandInfoExt *CHtmlSys_dbgmain::define_ext_command(
    const textchar_t *sym, size_t symlen,
    const textchar_t *desc, size_t desclen)
{
    /* create a new command list entry */
    HtmlDbgCommandInfoExt *inf =
        new HtmlDbgCommandInfoExt(next_custom_cmd_++,
                                  sym, symlen, desc, desclen);

    /* link it into our master list of extension commands */
    inf->nxt_ = ext_commands_;
    ext_commands_ = inf;

    /* add it to the 'by name' and 'by ID' tables */
    cmdname_hash->add(new CmdNameEntry(inf));
    command_hash->add(new CHtmlHashEntryUInt(inf->id, (void *)inf));

    /* return the new entry */
    return inf;
}


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbench regular expression parser interface
 */
INT CHtmlSys_dbgmain::ReMatchA(const char *txt, UINT txtlen,
                               const char *pat, UINT patlen)
{
    /* convert the strings to unicode */
    BSTR btxt = bstr_from_ansi(txt, txtlen);
    BSTR bpat = bstr_from_ansi(pat, patlen);

    /* call the wide-character version */
    INT ret = ReMatchW(btxt, SysStringLen(btxt), bpat, SysStringLen(bpat));

    /* free the bstrs */
    SysFreeString(btxt);
    SysFreeString(bpat);

    /* return the result */
    return ret;
}

INT CHtmlSys_dbgmain::ReSearchA(const char *txt, UINT txtlen,
                                const char *pat, UINT patlen,
                                twb_regex_match *match)
{
    /* convert the strings to unicode */
    BSTR btxt = bstr_from_ansi(txt, txtlen);
    BSTR bpat = bstr_from_ansi(pat, patlen);

    /* call the wide-character version */
    INT ret = ReSearchW(
        btxt, SysStringLen(btxt), bpat, SysStringLen(bpat), match);

    /* free the bstrs */
    SysFreeString(btxt);
    SysFreeString(bpat);

    /* return the result */
    return ret;
}

INT CHtmlSys_dbgmain::ReSearchW(const OLECHAR *txt, UINT txtlen,
                                const OLECHAR *pat, UINT patlen,
                                twb_regex_match *match)
{
    /* get ANSI versions of the strings */
    textchar_t *utxt = utf8_from_olestr(txt, txtlen);
    textchar_t *upat = utf8_from_olestr(pat, patlen);

    /* do the search */
    CRegexParser parser;
    CRegexSearcherSimple s(&parser);
    match->start = s.compile_and_search(
        upat, strlen(upat), utxt, utxt, strlen(utxt), &match->len);

    /* free the strings */
    th_free(utxt);
    th_free(upat);

    /* return TRUE if we found a match, FALSE if not */
    return match->start >= 0;
}

INT CHtmlSys_dbgmain::ReMatchW(const OLECHAR *txt, UINT txtlen,
                               const OLECHAR *pat, UINT patlen)
{
    /* get ANSI versions of the strings */
    textchar_t *utxt = utf8_from_olestr(txt, txtlen);
    textchar_t *upat = utf8_from_olestr(pat, patlen);

    /* do the search */
    CRegexParser parser;
    CRegexSearcherSimple s(&parser);
    int ret = s.compile_and_match(
        upat, strlen(upat), utxt, utxt, strlen(utxt));

    /* convert the byte length to a character length */
    utf8_ptr uptr(utxt);
    ret = uptr.len(ret);

    /* free the strings */
    th_free(utxt);
    th_free(upat);

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Load the default key mappings if we haven't already.
 */
void CHtmlSys_dbgmain::load_default_keys()
{
    CHtmlDebugConfig *const gc = global_config_;

    /* if we've already loaded the default keys, do nothing */
    if (gc->getval_int("key bindings", "defaults loaded", FALSE))
        return;

    /* define the default keys */
    static const struct
    {
        const textchar_t *tabname;
        const textchar_t *binding;
    }
    *val, defkeys[] =
    {
            /* include the bindings from WorkbenchDefault.keymap */
#define W32KEYBINDING(tabname, key, cmd, binding) { tabname, binding },
#include "w32tdbkey.h"
#undef W32KEYBINDING

            /* add the list terminator */
        0
    };

    /* run through the list and load each binding */
    for (val = defkeys ; val->binding != 0 ; ++val)
        gc->appendval("key bindings", val->tabname, val->binding);

    /* remember that we've now set the default bindings */
    gc->setval("key bindings", "defaults loaded", TRUE);
}

/*
 *   Load or reload the keyboard accelerator table from the configuration
 */
void CHtmlSys_dbgmain::load_keys_from_config()
{
    /* load each of the key tables */
    for (HtmlKeyTable *t = keytabs_ ; t->name != 0 ; ++t)
    {
        /* delete any existing accelerator for this table */
        if (t->accel != 0)
            delete t->accel;

        /* create a new accelerator for it */
        t->accel = new CTadsAccelerator();
        t->accel->set_statusline(statusline_);

        /* load its bindings */
        load_key_table(t->accel, t);
    }
}

/*
 *   Find a key table by name
 */
HtmlKeyTable *CHtmlSys_dbgmain::get_key_table(const textchar_t *name)
{
    /* search our list of key tables */
    for (HtmlKeyTable *t = keytabs_ ; t->name != 0 ; ++t)
    {
        /* if the name matches, return this table */
        if (stricmp(t->name, name) == 0)
            return t;
    }

    /* no match */
    return 0;
}

/*
 *   Load a key table
 */
void CHtmlSys_dbgmain::load_key_table(
    CTadsAccelerator *accel, HtmlKeyTable *tab)
{
    int i, cnt;
    CHtmlDebugConfig *const gc = global_config_;

    /*
     *   If this table has a parent table, load the parent bindings first.
     *   We want to inherit bindings from the parent but override any that we
     *   have in common, which we can accomplish simply by loading our
     *   bindings later, thus replacing any overridden inherited bindings.
     */
    if (tab->parent != 0)
    {
        HtmlKeyTable *parent;

        /* find the parent table */
        parent = get_key_table(tab->parent);

        /* if we found the parent, load it */
        if (parent != 0)
            load_key_table(accel, parent);
    }

    /* get the saved key binding list for this table */
    cnt = gc->get_strlist_cnt("key bindings", tab->name);

    /*
     *   Load the configuration data.  We want the most recently assigned to
     *   take precedence in the menu listing, which simply means that we load
     *   it last; so all we have to do is walk through the list in forward
     *   order.
     */
    for (i = 0 ; i < cnt ; ++i)
    {
        const textchar_t *val, *p;
        const HtmlDbgCommandInfo *cmd;

        /* get the next binding */
        val = gc->getval_strptr("key bindings", tab->name, i);

        /* find the end of the key name; if we can't, skip the entry */
        if ((p = get_strrstr(val, " = ")) == 0)
            continue;

        /* look up or add the command; skip it if that fails */
        if ((cmd = find_command(p + 3, TRUE)) == 0)
            continue;

        /* add the mapping to the table */
        accel->map(val, p - val, cmd->id);
    }
}

/*
 *   Look up a command by name
 */
const HtmlDbgCommandInfo *CHtmlSys_dbgmain::find_command(
    const textchar_t *name, size_t namelen, int add_if_undef)
{
    /* skip leading and trailing spaces */
    for ( ; namelen != 0 && isspace(*name) ; ++name, --namelen) ;
    for ( ; namelen != 0 && isspace(name[namelen-1]) ; --namelen) ;

    /* find the hash table entry */
    CmdNameEntry *entry = (CmdNameEntry *)cmdname_hash->find(name, namelen);

    /* if we found it, return its info structure */
    if (entry != 0)
        return entry->inf;

    /* if we're allowed to add the command, add it */
    if (add_if_undef)
        return define_ext_command(name, namelen, 0, 0);

    /* indicate that it's undefined */
    return 0;
}

/*
 *   Update the key bindings for a change in the active window
 */
void CHtmlSys_dbgmain::update_key_bindings(int activating)
{
    const textchar_t *tabname;
    CHtmlDbgSubWin *active = get_active_dbg_win();
    HtmlKeyTable *t;

    /*
     *   Get the table from the active window.  If there's no active window,
     *   simply use the "Global" bindings.
     */
    tabname = (active != 0 ? active->active_get_key_table() : "Global");

    /* look it up */
    for (t = keytabs_ ;
         t->name != 0 && stricmp(tabname, t->name) != 0 ; ++t) ;

    /* if we didn't find it, use the first table, which is always Global */
    if (t->name == 0)
        t = &keytabs_[0];

    /*
     *   Set this table's accelerator as the active system-wide accelerator,
     *   if (a) it an accelerator, (b) our window is open, and (c) we're
     *   activating.
     */
    if ((taccel_ = t->accel) != 0 && handle_ != 0 && activating)
        CTadsApp::get_app()->set_accel(t->accel, this);
}

/*
 *   Activate the global key bindings
 */
void CHtmlSys_dbgmain::set_global_key_bindings()
{
    /* set the global accelerator table */
    if (keytabs_[0].accel != 0 && handle_ != 0)
        CTadsApp::get_app()->set_accel(keytabs_[0].accel, this);
}

/* ------------------------------------------------------------------------ */
/*
 *   Rebuild the Tools menu with the current list of external tool commands
 */
void CHtmlSys_dbgmain::rebuild_tools_menu()
{
    HMENU menu;
    int refidx;
    MENUITEMINFO mii;

    /*
     *   find the Tools menu - it's the one that contains the "External
     *   Tools..."  command
     */
    menu = find_parent_menu(main_menu_, ID_TOOLS_EDITEXTERN, &refidx);

    /* remember it */
    tools_menu_ = menu;

    /* if we didn't find it, there's nothing more we can do */
    if (menu == 0)
        return;

    /*
     *   The items before the External Tools command are the external
     *   commands themselves.  Delete each item until we find a separator,
     *   which is where these items start.
     */
    while (refidx > 1)
    {
        /* get the previous item */
        mii.cbSize = menuiteminfo_size_;
        mii.fMask = MIIM_TYPE;
        mii.dwTypeData = 0;
        mii.cch = 0;
        GetMenuItemInfo(menu, refidx - 1, TRUE, &mii);

        /* if it's a separator, we're done */
        if ((mii.fType & MFT_SEPARATOR) != 0)
            break;

        /* delete the item */
        --refidx;
        DeleteMenu(menu, refidx, MF_BYPOSITION);
    }

    /* now insert each external tool item */
    CHtmlDebugConfig *gc = global_config_;
    int cnt = gc->get_strlist_cnt("extern tools", "title");
    for (int i = 0 ; i < cnt ; ++i)
    {
        const textchar_t *title;
        const textchar_t *cmdid;
        const HtmlDbgCommandInfo *cmdinfo;

        /* get the title and command ID */
        title = gc->getval_strptr("extern tools", "title", i);
        cmdid = gc->getval_strptr("extern tools", "cmdid", i);

        /* look up the command, or add it if it's not yet defined */
        cmdinfo = find_command(cmdid, TRUE);

        /* add the menu item */
        mii.cbSize = menuiteminfo_size_;
        mii.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
        mii.fType = MFT_STRING;
        mii.wID = cmdinfo->id;
        mii.dwTypeData = (LPSTR)title;
        InsertMenuItem(menu, refidx++, TRUE, &mii);
    }
}

/*
 *   Context structure for run_tool_entry
 */
struct run_tool_ctx
{
    run_tool_ctx(CHtmlSys_dbgmain *mainwin, const textchar_t *prog,
                 const textchar_t *args, const textchar_t *dir)
        : mainwin(mainwin), prog(prog), args(args), dir(dir)
        { }

    CHtmlSys_dbgmain *mainwin;
    CStringBuf prog;
    CStringBuf args;
    CStringBuf dir;
};

/*
 *   Thread entrypoint for running an external tool
 */
static DWORD WINAPI run_tool_entry(void *ctx0)
{
    /* get the context, suitably cast */
    run_tool_ctx *ctx = (run_tool_ctx *)ctx0;

    /* run the command */
    ctx->mainwin->run_command(ctx->prog.get(), ctx->prog.get(),
                              ctx->args.get(), ctx->dir.get());

    /* done with the context - we own it, so delete it */
    delete ctx;

    /* successful completion */
    return 0;
}

/*
 *   Execute an external tool command
 */
void CHtmlSys_dbgmain::execute_extern_tool(int cmdid)
{
    /* find the command */
    CHtmlDebugConfig *gc = global_config_;
    int cnt = gc->get_strlist_cnt("extern tools", "cmdid");
    for (int i = 0 ; i < cnt ; ++i)
    {
        /* look up this command name */
        const textchar_t *cmdname = gc->getval_strptr(
            "extern tools", "cmdid", i);
        const HtmlDbgCommandInfo *cmdinfo = find_command(cmdname, FALSE);

        /* if it matches, it's the one to execute */
        if (cmdinfo != 0 && cmdinfo->id == cmdid)
        {
            CStringBuf args;
            CHtmlDbgSubWin *active = get_active_dbg_win();
            const textchar_t *item_path = 0;
            int line = 0, col = 0, pos_ok = FALSE;
            CStringBuf seltext;
            const textchar_t *proj_path =
                (local_config_ != 0 ? local_config_filename_.get() : 0);
            const textchar_t *param;
            size_t param_len;
            textchar_t parambuf[OSFNMAX];
            const textchar_t *initdir;
            int need_proj = FALSE, need_src = FALSE;

            /* get the active source file information, if applicable */
            if (active != 0)
            {
                item_path = active->active_get_filename();
                pos_ok = active->active_get_caretpos(&line, &col);
                active->get_sel_text(&seltext, 0, 1024);
            }

            /* get the program */
            const textchar_t *prog = gc->getval_strptr(
                "extern tools", "prog", i);

            /* make sure there's a program */
            for ( ; prog != 0 && isspace(*prog) ; ++prog) ;
            if (prog == 0 || *prog == '\0')
            {
                MessageBox(handle_, "This tool does not have a program "
                           "associated with it. Use the External Tools "
                           "page in the Options dialog to configure a "
                           "program for the tool.", "TADS Workbench",
                           MB_OK | MB_ICONEXCLAMATION);
                return;
            }

            /* get the argument list */
            const textchar_t *argsrc = gc->getval_strptr(
                "extern tools", "args", i);

            /*
             *   start the output arguments with a space - this tells Windows
             *   that the argument list doesn't include the program name as
             *   its first token
             */
            args.append(" ");

            /* parse it */
            for (const textchar_t *p = argsrc ; *p != '\0' ; )
            {
                /* scan for the next parameter */
                const textchar_t *start;
                for (start = p ; *p != '\0' && *p != '$' ; ++p) ;

                /* copy the part up to here to the output buffer */
                if (start != p)
                    args.append(start, p - start);

                /* if at end of line, we're done */
                if (*p == '\0')
                    break;

                /* if it's '$$', just copy one '$' */
                if (*p == '$' && *(p+1) == '$')
                {
                    args.append("$");
                    p += 2;
                    continue;
                }

                /* if it's not '$(', just copy the '$' and keep going */
                if (*(p+1) != '(')
                {
                    args.append("$");
                    p += 1;
                    continue;
                }

                /* scan the parameter name */
                for (p += 2, start = p ; *p != '\0' && *p != ')' ; ++p) ;

                /* if we didn't find the ')', just copy it verbatim */
                if (*p != ')')
                {
                    start -= 2;
                    args.append(start, p - start);
                    continue;
                }

                /* assume we won't find a parameter value */
                param = 0;
                param_len = 0;

                /* look up the parameter name */
                if (memicmp(start, "ItemPath", p - start) == 0)
                {
                    need_src = TRUE;
                    if (item_path != 0)
                    {
                        param = item_path;
                        param_len = get_strlen(item_path);
                    }
                }
                else if (memicmp(start, "ItemDir", p - start) == 0)
                {
                    need_src = TRUE;
                    if (item_path != 0)
                    {
                        param = item_path;
                        param_len = os_get_root_name((char *)item_path)
                                    - item_path;
                    }
                }
                else if (memicmp(start, "ItemFile", p - start) == 0)
                {
                    need_src = TRUE;
                    if (item_path != 0)
                    {
                        param = os_get_root_name((char *)item_path);
                        param_len = get_strlen(param);

                        const textchar_t *p = strrchr(param, '.');
                        if (p != 0)
                            param_len = p - param;
                    }
                }
                else if (memicmp(start, "ItemExt", p - start) == 0)
                {
                    need_src = TRUE;
                    if (item_path != 0)
                    {
                        param = os_get_root_name((char *)item_path);
                        param = strrchr(param, '.');
                        param_len = (param != 0 ? get_strlen(param) : 0);
                    }
                }
                else if (memicmp(start, "ProjDir", p - start) == 0)
                {
                    need_proj = TRUE;
                    if (proj_path != 0)
                    {
                        param = proj_path;
                        param_len = os_get_root_name((char *)proj_path)
                                    - proj_path;
                    }
                }
                else if (memicmp(start, "ProjFile", p - start) == 0)
                {
                    need_proj = TRUE;
                    if (proj_path != 0)
                    {
                        param = os_get_root_name((char *)proj_path);
                        param_len = get_strlen(param);
                    }
                }
                else if (memicmp(start, "CurLine", p - start) == 0)
                {
                    need_src = TRUE;
                    if (pos_ok)
                    {
                        sprintf(parambuf, "%d", line + 1);
                        param = parambuf;
                        param_len = get_strlen(param);
                    }
                }
                else if (memicmp(start, "CurCol", p - start) == 0)
                {
                    need_src = TRUE;
                    if (pos_ok)
                    {
                        sprintf(parambuf, "%d", col + 1);
                        param = parambuf;
                        param_len = get_strlen(param);
                    }
                }
                else if (memicmp(start, "SelText", p - start) == 0)
                {
                    need_src = TRUE;
                    if (seltext.get() != 0)
                    {
                        param = seltext.get();
                        param_len = get_strlen(param);
                    }
                    else if (item_path != 0)
                        param = "";
                }
                else
                {
                    char buf[256];

                    /* invalid parameter */
                    sprintf(buf, "This tool's argument list "
                            "contains an invalid substitution parameter, "
                            "\"$(%.*s)\".", (int)(p - start), start);
                    MessageBox(handle_, buf, "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);
                    return;
                }

                /* if we didn't find the parameter, explain the problem */
                if (param == 0)
                {
                    const char *msg;

                    if (need_src)
                        msg = "This tool can only be used when a "
                              "text editor window is active.";
                    else if (need_proj)
                        msg = "This tool can only be used when a "
                              "project is open.";
                    else
                        msg = "This tool cannot be used right now.";

                    MessageBox(handle_, msg, "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);
                    return;
                }

                /* append the parameter */
                args.append(param, param_len);

                /* skip the close paren, and keep going */
                ++p;
            }

            /* get the working directory */
            initdir = gc->getval_strptr("extern tools", "dir", i);
            if (stricmp(initdir, "$(ProjDir)") == 0)
            {
                os_get_path_name(parambuf, sizeof(parambuf), proj_path);
                initdir = parambuf;
            }
            else if (stricmp(initdir, "$(ItemDir)") == 0)
            {
                if (item_path != 0)
                {
                    os_get_path_name(parambuf, sizeof(parambuf), item_path);
                    initdir = parambuf;
                }
                else if (proj_path != 0)
                {
                    os_get_path_name(parambuf, sizeof(parambuf), proj_path);
                    initdir = parambuf;
                }
            }

            /*
             *   if they want to capture output, run in a thread as a
             *   subcommand; otherwise just launch the process and forget it
             */
            const textchar_t *capture =
                gc->getval_strptr("extern tools", "capture", i);
            if (capture != 0 && (capture[0] == 'y' || capture[0] == 'Y'))
            {
                /*
                 *   we need to capture output, so run this as though it were
                 *   a build tool, using a separate thread and capturing
                 *   output to the standard handles
                 */

                /* show the debug log window and bring it to front */
                SendMessage(handle_, WM_COMMAND, ID_SHOW_DEBUG_WIN, 0);

                /* start the thread */
                DWORD tid;
                HANDLE th = CreateThread(
                    0, 0, (LPTHREAD_START_ROUTINE)&run_tool_entry,
                    new run_tool_ctx(this, prog, args.get(), initdir),
                    0, &tid);

                /* we don't need the thread handle for anything */
                CloseHandle(th);
            }
            else
            {
                /* no capture - just launch the process */
                STARTUPINFO sinfo;
                PROCESS_INFORMATION pinfo;

                memset(&sinfo, 0, sizeof(sinfo));
                sinfo.cb = sizeof(sinfo);
                sinfo.dwFlags = 0;
                int result = CreateProcess(
                    prog, args.get(), 0, 0, TRUE,
                    CREATE_NEW_CONSOLE, 0, initdir, &sinfo, &pinfo);

                /* if that failed, say so */
                if (result == 0)
                {
                    MessageBox(handle_, "An error occurred launching "
                               "the tool program.", "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);
                    return;
                }

                /*
                 *   we're just going to let the process run, so we don't
                 *   need the process or thread handles - close them
                 */
                CloseHandle(pinfo.hThread);
                CloseHandle(pinfo.hProcess);
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   get settings for a syntax styles for an editor mode
 */
void CHtmlSys_dbgmain::get_syntax_style_info(
    ITadsWorkbenchEditorMode *mode,
    int *syntax_coloring_enabled, CHtmlFontDesc *default_style,
    CHtmlFontDesc *styles, int style_cnt)
{
    BSTR modename;
    char mode_key[128];
    CHtmlFontDesc *stylep;
    int i;

    /* get the editor mode name */
    modename = mode->GetModeName();

    /* build the mode key */
    _snprintf(mode_key, sizeof(mode_key), "%ls-editor-prefs", modename);
    mode_key[sizeof(mode_key)-1] = '\0';

    /* done with the mode name */
    SysFreeString(modename);

    /* read the syntax-coloring-enabled setting */
    if (global_config_->getval(mode_key, "syntax coloring enabled",
                               syntax_coloring_enabled))
        *syntax_coloring_enabled = TRUE;

    /* read the mode-wide default */
    read_syntax_style(mode, mode_key, "default style", 0, default_style);

    /* read the individual syntax styles */
    for (i = 0, stylep = styles ; i < style_cnt ; ++i, ++stylep)
        read_syntax_style(mode, mode_key, "styles", i, stylep);
}

/*
 *   read a syntax style for a mode
 */
void CHtmlSys_dbgmain::read_syntax_style(
    ITadsWorkbenchEditorMode *mode,
    const textchar_t *mode_key, const textchar_t *subkey, int idx,
    CHtmlFontDesc *stylep)
{
    wb_style_info info;
    char buf[256];
    CHtmlDebugConfig *const cfg = global_config_;

    /* make sure the style index is valid */
    if (idx < 0 || (UINT)idx >= mode->GetStyleCount())
        return;

    /* get the default style information from the mode extension */
    mode->GetStyleInfo(idx, &info);

    /* get the global configuration settings for the style */
    if (cfg->getval(mode_key, subkey, idx, buf, sizeof(buf))
        || buf[0] == '\0')
    {
        /* there are no saved settings; use the mode defaults */
        ansi_from_olestr(stylep->face, sizeof(stylep->face), info.font_name);
        stylep->pointsize = info.ptsize;
        if (info.fgcolor != WBSTYLE_DEFAULT_COLOR)
        {
            stylep->color = COLORREF_to_HTML_color(info.fgcolor);
            stylep->default_color = FALSE;
        }
        if (info.bgcolor != WBSTYLE_DEFAULT_COLOR)
        {
            stylep->bgcolor = COLORREF_to_HTML_color(info.bgcolor);
            stylep->default_bgcolor = FALSE;
        }

        stylep->weight = (info.styles & WBSTYLE_BOLD) != 0 ? 700 : 400;
        stylep->italic = (info.styles & WBSTYLE_ITALIC) != 0;
        stylep->underline = (info.styles & WBSTYLE_UNDERLINE) != 0;
    }
    else
    {
        char *p = buf;

        /* if there's a font name specified, override the default */
        if (*p != ',')
        {
            /* remember where the font name starts */
            char *start = p;

            /* find the end of the font name */
            for ( ; *p != ',' && *p != '\0' ; ++p) ;

            /* copy the font name */
            safe_strcpy(stylep->face, sizeof(stylep->face),
                        start, p - start);
        }

        /* skip the comma after the font name */
        if (*p == ',')
            ++p;

        /* parse the point size if specified */
        if (*p != ',')
            stylep->pointsize = atoi(p);

        /* skip the point size */
        for ( ; *p != ',' && *p != '\0' ; ++p) ;
        if (*p == ',')
            ++p;

        /* parse the foreground color if specified */
        if (*p != ',')
        {
            stylep->color = strtoul(p, 0, 16);
            stylep->default_color = FALSE;
        }

        /* skip the foreground color */
        for ( ; *p != ',' && *p != '\0' ; ++p) ;
        if (*p == ',')
            ++p;

        /* parse the background color if specified */
        if (*p != ',')
        {
            stylep->bgcolor = strtoul(p, 0, 16);
            stylep->default_bgcolor = FALSE;
        }

        /* skip the background color */
        for ( ; *p != ',' && *p != '\0' ; ++p) ;
        if (*p == ',')
            ++p;

        /* parse the style flags, if present */
        for ( ; *p != ',' && *p != '\0' ; ++p)
        {
            switch (*p)
            {
            case 'b':
                stylep->weight = 700;
                break;

            case 'i':
                stylep->italic = TRUE;
                break;

            case 'u':
                stylep->underline = TRUE;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   save settings for a syntax style for an editor mode
 */
void CHtmlSys_dbgmain::save_syntax_style_info(
    ITadsWorkbenchEditorMode *mode,
    int syntax_coloring_enabled,
    const CHtmlFontDesc *default_style,
    const CHtmlFontDesc *styles, int style_cnt)
{
    BSTR modename;
    char mode_key[128];
    const CHtmlFontDesc *stylep;
    int i;

    /* get the editor mode name */
    modename = mode->GetModeName();

    /* build the mode key */
    _snprintf(mode_key, sizeof(mode_key), "%ls-editor-prefs", modename);
    mode_key[sizeof(mode_key)-1] = '\0';

    /* done with the mode name */
    SysFreeString(modename);

    /* save the syntax-coloring-enabled setting */
    global_config_->setval(mode_key, "syntax coloring enabled",
                           syntax_coloring_enabled);

    /* save the default style */
    write_syntax_style(mode, mode_key, "default style", 0, default_style);

    /* save the individual syntax styles */
    for (i = 0, stylep = styles ; i < style_cnt ; ++i, ++stylep)
        write_syntax_style(mode, mode_key, "styles", i, stylep);
}

/*
 *   write a syntax style for a mode
 */
void CHtmlSys_dbgmain::write_syntax_style(
    ITadsWorkbenchEditorMode *mode,
    const textchar_t *mode_key, const textchar_t *subkey, int idx,
    const CHtmlFontDesc *stylep)
{
    CHtmlDebugConfig *const cfg = global_config_;
    char buf[256];
    char *p;

    /* start with an empty buffer */
    p = buf;

    /* if this style specifies a font, add the font name */
    if (stylep->face[0] != '\0')
    {
        strcpy(p, stylep->face);
        p += strlen(p);
    }

    /* add the comma after the font */
    *p++ = ',';

    /* if there's a point size, add it */
    if (stylep->pointsize != 0)
    {
        sprintf(p, "%d", stylep->pointsize);
        p += strlen(p);
    }

    /* add the field separator */
    *p++ = ',';

    /* if there's a foreground color, add it */
    if (!stylep->default_color)
    {
        sprintf(p, "%06lx", stylep->color);
        p += strlen(p);
    }

    /* add the field separator */
    *p++ = ',';

    /* if there's a background color, add it */
    if (!stylep->default_bgcolor)
    {
        sprintf(p, "%06lx", stylep->bgcolor);
        p += strlen(p);
    }

    /* add the field separator */
    *p++ = ',';

    /* add the flags */
    if (stylep->weight >= 700)
        *p++ = 'b';
    if (stylep->italic)
        *p++ = 'i';
    if (stylep->underline)
        *p++ = 'u';

    /* end the string */
    *p = '\0';

    /* write it out */
    cfg->setval(mode_key, subkey, idx, buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlDebugSysIfc_win implementation
 */

/*
 *   create a source window
 */
IDebugWin *CHtmlSys_dbgmain::dbgsys_create_win(
    IDebugSrcMgr **srcmgr,
    const textchar_t *win_title, const textchar_t *full_path,
    HtmlDbg_wintype_t win_type)
{
    CTadsWin *win;
    RECT pos;
    CHtmlRect hrc;
    int vis;
    int is_docking;
    tads_dock_align dock_align;
    int dock_width;
    int dock_height;
    int mdi = FALSE;
    const char *dockwin_guid;
    int preformat_mode = TRUE;
    IDebugSysWin *cwin = 0;
    CHtmlSys_dbgwin *dwin = 0;
    CHtmlSys_dbgsrcwin *swin = 0;
    CHtmlSys_dbghelpwin *hwin = 0;
    CHtmlDebugSrcMgr *hsm = 0;

    /* set up a CW_USEDEFAULT rectangle, for MDI windows */
    SetRect(&pos, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT);

    /* presume this won't be a docking window */
    is_docking = FALSE;

    /* create the new frame window */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
        /* if we don't have a full path, use the window title as the path */
        if (full_path == 0 || full_path[0] == '\0')
            full_path = win_title;

        /* create the window */
        swin = new CHtmlSys_dbgsrcwin(prefs_, helper_, this,
                                      win_title, full_path);
        win = swin;
        cwin = swin;

        /* load the window's configuration, if saved */
        if (!local_config_->getval("src win pos", full_path, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* start off showing the window */
        vis = TRUE;
        break;

    case HTMLDBG_WINTYPE_STACK:
        /* create the source manager */
        *srcmgr = hsm = new CHtmlDebugSrcMgr();

        /* create the window */
        win = dwin = new CHtmlSys_dbgstkwin(
            hsm->get_parser(), prefs_, helper_, this);

        /* get the position setting */
        if (!local_config_->getval("stack win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* note that we need to create a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "stack win";

        /* make this an MDI window by default */
        mdi = TRUE;
        break;

    case HTMLDBG_WINTYPE_HISTORY:
        /* create the source manager */
        *srcmgr = hsm = new CHtmlDebugSrcMgr();

        /* create the window */
        win = dwin = new CHtmlSys_dbghistwin(
            hsm->get_parser(), prefs_, helper_, this);

        /* get the position setting */
        if (!local_config_->getval("hist win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);
        else
            SetRect(&pos, 50, 50, 400, 550);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "hist win";

        /* note that we need to create a docking window */
        is_docking = TRUE;
        break;

    case HTMLDBG_WINTYPE_DOCSEARCH:
        /* create the source manager */
        *srcmgr = hsm = new CHtmlDebugSrcMgrHtml(this);

        /* create the window */
        win = dwin = new CHtmlSys_docsearchwin(
            hsm->get_parser(), prefs_, helper_, this);

        /* get the position setting */
        if (!local_config_->getval("docsearch win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_BOTTOM;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "docsearch win";

        /* make it MDI by default */
        mdi = TRUE;

        /* these are regular HTML container windows */
        preformat_mode = FALSE;
        break;

    case HTMLDBG_WINTYPE_FILESEARCH:
        /* create the source manager */
        *srcmgr = hsm = new CHtmlDebugSrcMgrHtml(this);

        /* create the window */
        win = dwin = new CHtmlSys_filesearchwin(
            hsm->get_parser(), prefs_, helper_, this);

        /* get the position setting */
        if (!local_config_->getval("filesearch win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_BOTTOM;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "filesearch win";

        /* make it MDI by default */
        mdi = TRUE;

        /* these are regular HTML container windows */
        preformat_mode = FALSE;
        break;

    case HTMLDBG_WINTYPE_HELP:
        /* create the source manager */
        *srcmgr = new CHtmlDebugSrcMgrSimple();

        /* create the window */
        hwin = new CHtmlSys_dbghelpwin(prefs_, helper_, this);
        win = hwin;
        cwin = hwin;

        /* get the position setting */
        if (!local_config_->getval("help win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "help win";

        /* make it MDI by default */
        mdi = TRUE;

        /* this isn't even an HTML window */
        preformat_mode = FALSE;
        break;

    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* create the source manager */
        *srcmgr = hsm = new CHtmlDebugSrcMgr();

        /* create the window */
        win = dwin = new CHtmlSys_dbglogwin(
            hsm->get_parser(), prefs_, this);

        /* get the position setting */
        if (!local_config_->getval("log win pos", 0, &hrc))
            SetRect(&pos, hrc.left, hrc.top, hrc.right, hrc.bottom);

        /* set up to create it as a docking window */
        is_docking = TRUE;
        dock_align = TADS_DOCK_ALIGN_RIGHT;
        dock_width = 170;
        dock_height = 170;
        dockwin_guid = "log win";

        /* make this an MDI window by default */
        mdi = TRUE;
        break;

    case HTMLDBG_WINTYPE_sys_base:
        /*
         *   we create system window types directly; they don't come through
         *   here
         */
        assert(FALSE);
        break;
    }

    /* initialize the HTML panel in the window, if applicable */
    if (dwin != 0)
    {
        /* initialize the panel */
        dwin->init_html_panel(hsm->get_formatter());

        /* the panel is the content window */
        cwin = (CDebugSysWin *)dwin->get_html_panel();

        /* tell the source manager about its window */
        hsm->init_win((CDebugSysWin *)cwin, this);

        /* if this shows only preformatted text, don't reformat on resize */
        if (preformat_mode)
            dwin->get_html_panel()->set_format_on_resize(FALSE);

        /* give the panel a sizebox */
        dwin->get_html_panel()->set_has_sizebox(TRUE);
    }

    /* open the system window */
    if (is_docking)
    {
        /* this is a docking window - create it as a docking window */
        load_dockwin_config_win(local_config_, dockwin_guid,
                                win, win_title,
                                TRUE, dock_align, dock_width, dock_height,
                                &pos, TRUE, mdi);
    }
    else
    {
        /* create an MDI child window for the new window */
        win->create_system_window(this, vis, win_title, &pos,
                                  w32tdb_new_child_syswin(win));
    }

    /* finish up, depending on the window type */
    switch(win_type)
    {
    case HTMLDBG_WINTYPE_SRCFILE:
        {
            CSciSrcMgr *scimgr;
            BSTR fname = bstr_from_ansi(full_path);
            ITadsWorkbenchEditorMode *mode = 0;
            UINT aff = 0;
            int i, cmd;
            twb_editor_mode_iter iter(this);
            int found;

            /* create the Scintilla source manager for the window */
            *srcmgr = scimgr = new CSciSrcMgr(swin);

            /* look for an extension that wants to handle this file */
            for (i = 0, found = (iter.first() != 0) ; found ;
                 ++i, found = (iter.next() != 0))
            {
                UINT new_aff;

                /* ask the mode if it wants the file */
                new_aff = iter.mode()->GetFileAffinity(fname);

                /* ask the mode if it wants the file */
                if (new_aff > aff)
                {
                    /* remember the new winning mode */
                    mode = iter.mode();
                    mode->AddRef();

                    /* remember the affinity */
                    aff = new_aff;

                    /*
                     *   remember the command ID - this is ID_EDIT_LANGMODE
                     *   plus the iteration index
                     */
                    cmd = ID_EDIT_LANGMODE + i;
                }
            }

            /* if we found an editor extension, use it */
            if (mode != 0)
            {
                /* remember the mode in the source window */
                swin->set_editor_mode(mode, cmd);

                /* we're done with our references to the mode */
                mode->Release();
            }

            /* clean up */
            SysFreeString(fname);
        }

        /* set the initial formatting */
        note_formatting_changes(swin);
        break;

    case HTMLDBG_WINTYPE_STACK:
    case HTMLDBG_WINTYPE_HISTORY:
    case HTMLDBG_WINTYPE_FILESEARCH:
        /*
         *   it's an HTML-panel window - synthesize an options change
         *   notification, so that we load the current options
         */
        note_formatting_changes((CDebugSysWin *)dwin->get_html_panel());
        break;

    case HTMLDBG_WINTYPE_HELP:
        /* navigate to the main help file */
        {
            char url[OSFNMAX];

            get_main_help_file_name(url, sizeof(url));
            hwin->go_to_url(url);
        }
        break;

    case HTMLDBG_WINTYPE_DEBUGLOG:
        /* set the initial formatting */
        note_formatting_changes((CDebugSysWin *)dwin->get_html_panel());

        /*
         *   Tell the main game window about the log window, so that it can
         *   send debug messages to the window.  Note that we do not want to
         *   add a menu for the debug log window to the game window, since
         *   the window is conceptually part of the debugger rather than of
         *   the run-time.
         */
        if (CHtmlSys_mainwin::get_main_win() != 0)
            CHtmlSys_mainwin::get_main_win()->
                set_debug_win((CHtmlSys_dbglogwin *)win, FALSE);
        break;

    case HTMLDBG_WINTYPE_DOCSEARCH:
        /* set the initial formatting */
        note_formatting_changes((CDebugSysWin *)dwin->get_html_panel());
        break;

    case HTMLDBG_WINTYPE_sys_base:
        /* other window types should never come through here */
        assert(FALSE);
        break;
    }

    /* return the system window object for the HTML panel in the window */
    return cwin;
}

/*
 *   adjust a font for use in source windows
 */
void CHtmlSys_dbgmain::dbgsys_set_srcfont(CHtmlFontDesc *font_desc)
{
    textchar_t buf[HTMLFONTDESC_FACE_MAX];
    int fontsize;

    /*
     *   use a smaller font size; the base size is too big for monospaced
     *   fonts
     */
    font_desc->htmlsize = 2;

    /* if there's a configuration setting, use its font */
    if (!global_config_->getval("src win options", "font name",
                                0, buf, sizeof(buf)))
        strcpy(font_desc->face, buf);
    else
        font_desc->face[0] = '\0';

    /* if there's a specific size setting, use it */
    if (!global_config_->getval("src win options", "font size", &fontsize))
    {
        /* use the given point size, rather than an abstract HTML size */
        font_desc->htmlsize = 0;
        font_desc->pointsize = fontsize;
    }

    /* always use the default window colors */
    font_desc->default_color = TRUE;
    font_desc->default_bgcolor = TRUE;
}

/*
 *   Get the global style settings.  This retrieves the font plus the window
 *   background color.
 */
void CHtmlSys_dbgmain::get_global_style(CHtmlFontDesc *desc,
                                        CHtmlFontDesc *sel_desc)
{
    int winclr, winbg;

    /* get the text font */
    dbgsys_set_srcfont(desc);

    /* determine whether we're using system colors or explicit colors */
    winclr = global_config_->getval_int(
        "src win options", "use win colors", TRUE);
    winbg = global_config_->getval_int(
        "src win options", "use win bgcolor", winclr);

    /* indicate this with the 'default' flags */
    desc->default_color = winclr;
    desc->default_bgcolor = winbg;

    /*
     *   fetch the saved colors, even if we're not using them - this will
     *   ensure that if we go from non-default colors to defaults and back
     *   again, the original non-default colors will stick
     */
    if (global_config_->getval_color(
        "src win options", "text color", &desc->color))
        desc->color = HTML_make_color(0,0,0);

    if (global_config_->getval_color(
        "src win options", "bkg color", &desc->bgcolor))
        desc->bgcolor = HTML_make_color(0xff,0xff,0xff);

    /* determine whether we're using system colors for selected text */
    winclr = global_config_->getval_int(
        "src win options", "use win sel colors", TRUE);
    winbg = global_config_->getval_int(
        "src win options", "use win sel bgcolor", winclr);

    /* save the values in sel_desc */
    sel_desc->default_color = winclr;
    sel_desc->default_bgcolor = winbg;

    /* fetch the color values */
    if (global_config_->getval_color(
        "src win options", "sel text color", &sel_desc->color))
        sel_desc->color = HTML_make_color(0,0,0);

    if (global_config_->getval_color(
        "src win options", "sel bkg color", &sel_desc->bgcolor))
        sel_desc->bgcolor = HTML_make_color(0xff,0xff,0xff);
}

/*
 *   Save the global style
 */
void CHtmlSys_dbgmain::set_global_style(const CHtmlFontDesc *desc,
                                        const CHtmlFontDesc *sel_desc)
{
    /* set the "use windows colors" flag according to the default setting */
    global_config_->setval("src win options", "use win colors",
                           desc->default_color);
    global_config_->setval("src win options", "use win bgcolor",
                           desc->default_bgcolor);

    /*
     *   save the colors (even if not using defaults - this way the last
     *   non-default settings will stick if we switch back to non-defaults
     *   later)
     */
    global_config_->setval_color(
        "src win options", "text color", desc->color);
    global_config_->setval_color(
        "src win options", "bkg color", desc->bgcolor);

    /* save the typeface name and size */
    global_config_->setval("src win options", "font name", 0, desc->face);
    global_config_->setval("src win options", "font size", desc->pointsize);

    /* set the "use windows colors" flag for the selected text color */
    global_config_->setval("src win options", "use win sel colors",
                           sel_desc->default_color);
    global_config_->setval("src win options", "use win sel bgcolor",
                           sel_desc->default_bgcolor);

    /* save the selected text colors */
    global_config_->setval_color(
        "src win options", "sel text color", sel_desc->color);
    global_config_->setval_color(
        "src win options", "sel bkg color", sel_desc->bgcolor);
}


/*
 *   Test a filename against a full filename path to see if the filename
 *   matches the path with its prefix.  For example, if the path is
 *   "c:\foo\bar\dir\file.c" and the filename is "dir\file.c", we would
 *   have a match, because "dir\file.c" is a trailing suffix of the path
 *   after a path separator character.
 */
static int path_prefix_matches(const char *path, const char *fname)
{
    size_t path_len;
    size_t fname_len;

    /* get the lengths */
    path_len = strlen(path);
    fname_len = strlen(fname);

    /*
     *   if the path name isn't any longer than the filename, there's
     *   nothing to check
     */
    if (path_len <= fname_len)
        return FALSE;

    /*
     *   if the path doesn't have a separator character just before where
     *   the suffix would go, it's not a match
     */
    if (path[path_len - fname_len - 1] != '\\'
        && path[path_len - fname_len - 1] != '/'
        && path[path_len - fname_len - 1] != ':')
        return FALSE;

    /* check to see if the suffix matches */
    return (strcmp(path + (path_len - fname_len), fname) == 0);
}

/*
 *   compare filenames
 */
int CHtmlSys_dbgmain::dbgsys_fnames_eq(const char *fname1,
                                       const char *fname2)
{
    char projdir[OSFNMAX];
    char buf1[OSFNMAX], buf2[OSFNMAX];

    /* if one or the other is null, we can't have a match */
    if (fname1 == 0 || fname2 == 0)
        return FALSE;

    /* assume relative names are in the project directory */
    get_project_dir(projdir, sizeof(projdir));
    if (!os_is_file_absolute(fname1))
    {
        os_build_full_path(buf1, sizeof(buf1), projdir, fname1);
        fname1 = buf1;
    }
    if (!os_is_file_absolute(fname2))
    {
        os_build_full_path(buf2, sizeof(buf2), projdir, fname2);
        fname2 = buf2;
    }

    /* if the root names match, consider it a match */
    return (stricmp(os_get_root_name((char *)fname1),
                    os_get_root_name((char *)fname2)) == 0);

#if 0
    char buf1[256], buf2[256];
    char *fpart1, *fpart2;

    /* if one or the other is null, we can't have a match */
    if (fname1 == 0 || fname2 == 0)
        return FALSE;

    /* clear the full path buffers */
    buf1[0] = '\0';
    buf2[0] = '\0';

    /*
     *   Get the full names and paths of the files.  This will expand any
     *   short (8.3) names into full names, and will resolve relative
     *   paths to provide the fully-qualified name with drive and absolute
     *   path.
     */
    GetFullPathName(fname1, sizeof(buf1), buf1, &fpart1);
    GetFullPathName(fname2, sizeof(buf2), buf2, &fpart2);

    /* see if they match, ignoring case - if so, indicate a match */
    if (stricmp(buf1, buf2) == 0)
        return TRUE;

    /*
     *   Test to see if one of the given filenames matches the other as a
     *   relative path.  If so, consider it a match as well.
     */
    if (path_prefix_matches(buf1, fname2)
        || path_prefix_matches(buf2, fname1))
        return TRUE;

    /* they don't appear to match */
    return FALSE;
#endif //0
}

/*
 *   compare a filename to a path
 */
int CHtmlSys_dbgmain::dbgsys_fname_eq_path(const char *path,
                                           const char *fname)
{
    char buf[256];
    char *fpart;
    size_t pathlen, fnamelen;
    int tries;

    /* if one or the other is null, we can't have a match */
    if (path == 0 || fname == 0)
        return FALSE;

    /* find the root name of the filename */
    fname = os_get_root_name((char *)fname);

    /* try it with the given path, then with the full path */
    for (tries = 0 ; ; ++tries)
    {
        /*
         *   Check to see if the filename is a suffix of the path.  If so,
         *   and the character immediately preceding the suffix of the
         *   path is a path separator, then we have a match.
         */
        pathlen = strlen(path);
        fnamelen = strlen(fname);
        if (fnamelen < pathlen
            && memicmp(path + pathlen - fnamelen, fname, fnamelen) == 0
            && strchr("/\\:", path[pathlen - fnamelen - 1]) != 0)
        {
            /* it's a match */
            return TRUE;
        }

        /* if we've already tried with the full name, we've failed */
        if (tries > 0)
            return FALSE;

        /*
         *   Get the full name of the path and try again.  It's possible
         *   that the path given is relative, or contains 8.3 short names,
         *   so it's worth trying again with the full path name.
         */
        GetFullPathName(path, sizeof(buf), buf, &fpart);

        /* try again with the expanded form of the path */
        path = buf;
    }
}

/*
 *   Find a source file
 */
int CHtmlSys_dbgmain::dbgsys_find_src(const char *origname, int origlen,
                                      char *fullname, size_t full_len,
                                      int must_find_file)
{
    /* use the same implementation as html_dbgui_find_src */
    return html_dbgui_find_src(origname, origlen, fullname, full_len,
                               must_find_file);
}


/*
 *   Receive notification that a debug log window is being destroyed
 */
void CHtmlSys_dbgmain::dbghostifc_on_destroy(CHtmlDbgSubWin *subwin,
                                             IDebugSysWin *syswin)
{
    /* make sure it's not the active window */
    if (active_dbg_win_ == subwin)
        active_dbg_win_ = 0;
    if (last_active_dbg_win_ == subwin)
        last_active_dbg_win_ = 0;
    if (last_cmd_target_win_ == subwin)
        last_cmd_target_win_ = 0;
    if (find_start_.win == subwin)
        find_start_.set_win(0);
    if (find_last_.win == subwin)
        find_last_.set_win(0);

    /* update buttons in the Find dialog, if it's open */
    if (find_dlg_ != 0)
        find_dlg_->adjust_context_buttons();

    /* tell the helper about it */
    helper_->on_close_srcwin(syswin);

    /* update the key bindings */
    update_key_bindings(FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Copy a source file, replacing $xxx$ fields with values for the new game.
 */
int CHtmlSys_dbgmain::copy_insert_fields(const char *srcfile,
                                         const char *dstfile,
                                         const CNameValTab *biblio)
{
    FILE *srcfp = 0;
    FILE *dstfp = 0;
    int ret = FALSE;
    char buf[1024];
    size_t len;

    /* open the two files */
    if ((srcfp = fopen(srcfile, "r")) == 0
        || (dstfp = fopen(dstfile, "w")) == 0)
        goto done;

    /* copy the files one line at a time */
    for (;;)
    {
        char *p;
        char *start;

        /* read the next line */
        if (fgets(buf, sizeof(buf), srcfp) == 0)
            break;

        /* search for $xxx$ fields to replace */
        for (p = start = buf ; *p != '\0' ; ++p)
        {
            /* check for a field */
            if (*p == '$')
            {
                const char *key = 0;
                const char *val = 0;
                const CNameValPair *pair;
                char ifid[128];

                /* find the end of the key */
                const char *pe;
                for (pe = p + 1 ; *pe != '\0' && *pe != '$' ; ++pe) ;

                /* check for IFID, and the various bibliographic keys */
                if (memcmp(p, "$IFID$", 6) == 0)
                {
                    /* it's the IFID */
                    key = "$IFID$";

                    /* generate the IFID value */
                    tads_generate_ifid(ifid, sizeof(ifid), dstfile);
                    val = ifid;
                }
                else if (biblio != 0
                         && (pair = biblio->find_key(p, pe - p + 1)) != 0)
                {
                    /* use this key/value pair */
                    key = pair->key_.get();
                    val = pair->val_.get();
                }

                /* if we found something, make the replacement */
                if (key != 0)
                {
                    const char *q1, *q2;

                    /* write the part up to here */
                    if ((len = (p - start)) != 0
                        && fwrite(start, 1, len, dstfp) != len)
                        goto done;

                    /* write the replacement text, escaping single quotes */
                    for (q1 = q2 = val ; ; ++q2)
                    {
                        /* if we're at a quote or backslash, escape it */
                        if (*q2 == '\'' || *q2 == '\\' || *q2 == '\0')
                        {
                            /* write the part up to the special character */
                            if ((len = q2 - q1) != 0
                                && fwrite(q1, 1, len, dstfp) != len)
                                goto done;

                            /* if at end of string, stop */
                            if (*q2 == '\0')
                                break;

                            /* write the escaping backslash */
                            if (fputs("\\", dstfp) == EOF)
                                goto done;

                            /* continue from the quote */
                            q1 = q2;
                        }
                    }

                    /* move to the last character of the $xxx$ string */
                    p += strlen(key) - 1;

                    /* the next segment starts just after this point */
                    start = p + 1;
                }
            }
        }

        /* write out the last part of the line, if any */
        if ((len = p - start) != 0
            && fwrite(start, 1, len, dstfp) != len)
            goto done;
    }

    /* successfully copied */
    ret = TRUE;

done:
    /* close the files, if we opened them */
    if (srcfp != 0)
        fclose(srcfp);
    if (dstfp != 0 && fclose(dstfp) == EOF)
        ret = FALSE;

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlSys_tdbwin implementation
 */

CHtmlSys_tdbwin::CHtmlSys_tdbwin(CHtmlParser *parser,
                                 CHtmlPreferences *prefs,
                                 CHtmlDebugHelper *helper,
                                 CHtmlSys_dbgmain *dbgmain)
    : CHtmlSys_dbgwin(parser, prefs)
{
    /* remember my helper object, and add a reference to it */
    helper_ = helper;
    helper_->add_ref();

    /* remember the main debugger window */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();
}

CHtmlSys_tdbwin::~CHtmlSys_tdbwin()
{
    /* release our reference on the main debugger window */
    dbgmain_->Release();
}

/*
 *   initialize the HTML panel
 */
void CHtmlSys_tdbwin::init_html_panel(CHtmlFormatter *formatter)
{
    /* create the HTML subwindow */
    create_html_panel(formatter);
}

/*
 *   receive notification that the window is being destroyed
 */
void CHtmlSys_tdbwin::do_destroy()
{
    CHtmlDebugHelper *helper;
    CDebugSysWin *win;

    /*
     *   tell the main window that I'm gone, in case it's keeping track of me
     *   as a background window for routing commands
     */
    dbgmain_->on_close_dbg_win(this);

    /*
     *   note my helper and HTML panel objects for later -- since 'this' will
     *   be destroyed when we inherit our superclass do_destroy(), we won't
     *   be able to access my members after the inherited do_destroy()
     *   returns
     */
    helper = helper_;
    win = (CDebugSysWin *)(CHtmlSysWin_win32 *)get_html_panel();

    /*
     *   inherit default handling - this will delete 'this', so we can't
     *   access any of my member variables after this call
     */
    CHtmlSys_dbgwin::do_destroy();

    /*
     *   Now that the window has been deleted and thus disentangled from the
     *   parser and formatter, tell the helper to delete its objects
     *   associated with the window.
     */
    helper->on_close_srcwin(win);

    /* release our reference on the helper, now that we're going away */
    helper->remove_ref();
}

/*
 *   handle a user message
 */
int CHtmlSys_tdbwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see what we have */
    switch (msg)
    {
    case HTMLM_SAVE_WIN_POS:
        /*
         *   It's our special deferred save-window-position message.
         *   Whatever event triggered the size/move has finished, so it
         *   should now be safe to save our new preference setting.
         */
        set_winpos_prefs_deferred();

        /* handled */
        return TRUE;
    }

    /* if we didn't handle it, inherit the default handling */
    return CHtmlSys_dbgwin::do_user_message(msg, wpar, lpar);
}

/*
 *   Store my position in the preferences.  This is invoked from our
 *   self-posted follow-up message, which we post during a size/move event
 *   for deferred processing after the triggering event has finished.
 */
void CHtmlSys_tdbwin::set_winpos_prefs_deferred()
{
    RECT rc;
    CHtmlRect pos;
    HWND hwnd, mdihwnd;

    /* get my frame position */
    hwnd = get_frame_pos(&rc);

    /*
     *   If we're not the current active MDI window, ignore this.  When the
     *   MDI child is maximized, switching among MDI children generates a
     *   "restore" for the outgoing window; we want to ignore that, since we
     *   only want to note actual resizes done by the user.
     */
    mdihwnd = dbgmain_->get_active_mdi_child();
    if (mdihwnd != hwnd && !IsChild(mdihwnd, hwnd))
        return;

    /*
     *   If we're the current maximized MDI child, do not save the settings.
     *   The maximized child's size is entirely dependent on the MDI frame
     *   size, so it obviously doesn't have any business being stored; what's
     *   more, we do want to store the 'restored' size, so if we saved the
     *   maximized size, we'd overwrite that information, which we don't want
     *   to do.
     */
    if (is_win_maximized())
        return;

    /* convert to our internal rectangle structure */
    pos.set(rc.left, rc.top, rc.right, rc.bottom);

    /* store the position */
    store_winpos_prefs(&pos);
}

/*
 *   Non-client Activate/deactivate.  Notify the debugger main window of the
 *   coming or going of this debugger window as the active debugger window,
 *   which will allow us to receive certain command messages from the main
 *   window.
 *
 *   We handle this in the non-client activation, since MDI doesn't send
 *   regular activation messages to child windows when the MDI frame itself
 *   is becoming active or inactive.
 */
int CHtmlSys_tdbwin::do_ncactivate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent
 */
void CHtmlSys_tdbwin::do_parent_activate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* inherit default handling */
    CHtmlSys_dbgwin::do_parent_activate(flag);
}

/*
 *   process a notification message
 */
int CHtmlSys_tdbwin::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    /* check the message */
    switch(notify_code)
    {
    case NM_SETFOCUS:
    case NM_KILLFOCUS:
        /* notify myself of an artificial activation */
        do_parent_activate(notify_code == NM_SETFOCUS);

        /* inherit the default handling as well */
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default handling */
    return CHtmlSys_dbgwin::do_notify(control_id, notify_code, nm);
}

/*
 *   Check a command status.  Pass the command to the main window for
 *   routine.
 */
TadsCmdStat_t CHtmlSys_tdbwin::check_command(const check_cmd_info *info)
{
    return dbgmain_->check_command(info);
}

/*
 *   Process a command.  Pass the command to the main window for routing.
 */
int CHtmlSys_tdbwin::do_command(int notify_code, int command_id, HWND ctl)
{
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Check a command status, for routing from the main window.  Pass the
 *   command to our HTML panel for processing.
 */
TadsCmdStat_t CHtmlSys_tdbwin::active_check_command(
    const check_cmd_info *info)
{
    return ((CHtmlSysWin_win32_tdb *)html_panel_)
        ->active_check_command(info);
}

/*
 *   Process a command, for routing from the main window.  Pass the command
 *   to our HTML panel.
 */
int CHtmlSys_tdbwin::active_do_command(int notify_code, int command_id,
                                       HWND ctl)
{
    return ((CHtmlSysWin_win32_tdb *)html_panel_)
        ->active_do_command(notify_code, command_id, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   CHtmlSys_dbgwin_dockable implementation
 */


/*
 *   close the owner window
 */
int CHtmlSys_dbgwin_dockable::close_owner_window(int force)
{
    /* we're inside a docking or MDI parent, so close the parent */
    if (force)
    {
        /*
         *   force the window closed with the special DOCK_DESTROY message
         *   (don't destroy it directly, since the window could be an MDI
         *   child
         */
        SendMessage(GetParent(handle_), DOCKM_DESTROY, 0, 0);
        return TRUE;
    }
    else
    {
        /* ask the parent window to close itself */
        return !SendMessage(GetParent(handle_), WM_CLOSE, 0, 0);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Go To Line dialog
 */
class CTadsDlgGoToLine: public CTadsDialog
{
public:
    CTadsDlgGoToLine(long init_linenum)
    {
        /* no line number yet */
        linenum_ = init_linenum;
    }

    void init()
    {
        char buf[20];

        /* inherit default initialization */
        CTadsDialog::init();

        /*
         *   if we have a valid line number, put it in the edit field;
         *   otherwise, disable the OK button, since we don't have a valid
         *   number in the field yet
         */
        if (linenum_ != 0)
        {
            /* we have a line number - initialize the edit field */
            sprintf(buf, "%ld", linenum_);
            SetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM), buf);
        }
        else
        {
            /* no line number in the field yet - disable the OK button */
            EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
        }
    }

    int do_command(WPARAM id, HWND ctl)
    {
        char buf[256];

        switch(LOWORD(id))
        {
        case IDC_EDIT_LINENUM:
            /* check for edit field notificaitons */
            if (HIWORD(id) == EN_CHANGE)
            {
                /*
                 *   it's a change notification - enable or disable the OK
                 *   button according to whether or not we have a valid
                 *   number in the field
                 */
                GetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM),
                              buf, sizeof(buf));
                EnableWindow(GetDlgItem(handle_, IDOK), atol(buf) != 0);
            }

            /* proceed with normal processing */
            break;

        case IDOK:
            /* read the line number from the control */
            GetWindowText(GetDlgItem(handle_, IDC_EDIT_LINENUM),
                          buf, sizeof(buf));

            /* convert it to an integer */
            linenum_ = atol(buf);

            /* proceed with normal processing */
            break;

        case IDCANCEL:
            /* cancelling - forget the line number */
            linenum_ = 0;

            /* proceed with normal processing */
            break;

        default:
            /* nothing special to us - use normal processing */
            break;
        }

        /* inherit default processing */
        return CTadsDialog::do_command(id, ctl);
    }

    /*
     *   get the line number - returns zero if no valid line number was
     *   entered or the dialog was cancelled
     */
    long get_linenum() const { return linenum_; }

private:
    /* the entered line number, or zero if no line number was entered */
    long linenum_;
};


/* ------------------------------------------------------------------------ */
/*
 *   CHtmlSys_dbgsrcwin - source file editor window implementation
 */

HCURSOR CHtmlSys_dbgsrcwin::split_csr_ = 0;

/*
 *   construction
 */
CHtmlSys_dbgsrcwin::CHtmlSys_dbgsrcwin(class CHtmlPreferences *prefs,
                                       class CHtmlDebugHelper *helper,
                                       class CHtmlSys_dbgmain *dbgmain,
                                       const textchar_t *win_title,
                                       const textchar_t *full_path)
{
    /* remember the vitals */
    prefs_ = prefs;
    helper_ = helper;
    dbgmain_ = dbgmain;
    title_.set(win_title);
    set_path(full_path);

    /* assume we're not untitled */
    untitled_ = FALSE;

    /* start out clean and writable */
    dirty_ = FALSE;
    readonly_ = FALSE;

    /* not in drag/track mode yet */
    dragging_cur_line_ = FALSE;
    drag_timer_ = 0;
    tracking_splitter_ = FALSE;

    /* set a reference on the main interface */
    dbgmain_->AddRef();

    /* mark the file as not yet opened for editing */
    opened_for_editing_ = FALSE;

    /* we don't have an editor extension mode yet */
    mode_ = 0;
    mode_cmd_id_ = 0;

    /* load our context menus */
    ctx_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDR_DEBUGSRCWIN_POPUP_MENU));
    margin_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDR_DEBUGSRCMARGIN_POPUP_MENU));
    script_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                            MAKEINTRESOURCE(IDR_DEBUGSCRIPTWIN_POPUP_MENU));

    /* remember the file's current timestamp */
    update_file_time();

    /* initialize the split percentage */
    split_ = 0.0;

    /* load our splitter cursor if we haven't already */
    if (split_csr_ == 0)
        split_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                                MAKEINTRESOURCE(IDC_SPLIT_NS_CSR));

    /* no saved isearch yet */
    last_isearch_regex_ = FALSE;
    last_isearch_exact_ = FALSE;
    last_isearch_word_ = FALSE;

    /* there are no UI changes to update yet */
    mod_pending_ = FALSE;
}

CHtmlSys_dbgsrcwin::~CHtmlSys_dbgsrcwin()
{
    /* done with our context menus */
    DestroyMenu(ctx_menu_);
    DestroyMenu(script_menu_);
    DestroyMenu(margin_menu_);

    /* release our COM objects */
    if (mode_ != 0)
        mode_->Release();
}

/*
 *   Is this a script file?
 */
int CHtmlSys_dbgsrcwin::is_script_file()
{
    size_t flen, dlen;
    CHtmlDebugConfig *lc = dbgmain_->get_local_config();
    const textchar_t *dir = lc->getval_strptr("auto-script", "dir", 0);
    char projdir[OSFNMAX];
    char dirbuf[OSFNMAX];

    /* apply the default auto-script directory */
    if (dir == 0)
        dir = "Scripts";

    /* get the full path to the script directory */
    if (!os_is_file_absolute(dir))
    {
        dbgmain_->get_project_dir(projdir, sizeof(projdir));
        os_build_full_path(dirbuf, sizeof(dirbuf), projdir, dir);
        dir = dirbuf;
    }

    /*
     *   if my file is within the script directory, and it ends in ".cmd",
     *   it's a script file
     */
    flen = strlen(path_.get());
    dlen = strlen(dir);
    return (flen > dlen
            && memicmp(path_.get(), dir, dlen) == 0
            && path_.get()[dlen] == '\\'
            && flen > 4
            && stricmp(path_.get() + flen - 4, ".cmd") == 0);
}

/*
 *   set my filename
 */
void CHtmlSys_dbgsrcwin::set_path(const textchar_t *fname)
{
    const textchar_t *p;
    textchar_t pathbuf[OSFNMAX];

    /*
     *   if the name isn't already an absolute path, get the full path
     *   relative to the project directory
     */
    if (!os_is_file_absolute(fname))
    {
        char projdir[OSFNMAX];
        dbgmain_->get_project_dir(projdir, sizeof(projdir));
        os_build_full_path(pathbuf, sizeof(pathbuf), projdir, fname);
        fname = pathbuf;
    }

    /* use the filename as our path and as the window title */
    path_.set(fname);

    /* find the extension, and remember it as the default for Save As */
    defext_.set("");
    for (p = fname + get_strlen(fname) ; p != fname ; )
    {
        --p;
        if (*p == '.')
        {
            /* the extension follows - note it and stop looking */
            defext_.set(p + 1);
            break;
        }
        if (*p == '\\' || *p == '/' || *p == ':')
        {
            /* path character - there's no extension */
            break;
        }
    }
}

/*
 *   Set my editor extension by command ID
 */
void CHtmlSys_dbgsrcwin::set_editor_mode(int cmd)
{
    twb_editor_mode_iter iter(dbgmain_);
    int i;
    int found;

    /*
     *   find the mode object - the command ID is ID_EDIT_LANGMODE plus the
     *   iteration index of the entry in our mode list
     */
    for (i = cmd - ID_EDIT_LANGMODE, found = (iter.first() != 0) ;
         i != 0 && found ; --i, found = (iter.next() != 0)) ;

    /* if we found it, activate it */
    if (found)
        set_editor_mode(iter.mode(), cmd);
}

/*
 *   Set my editor extension
 */
void CHtmlSys_dbgsrcwin::set_editor_mode(
    ITadsWorkbenchEditorMode *mode, int cmd)
{
    /* remember the mode */
    if (mode != 0)
        mode->AddRef();
    if (mode_ != 0)
        mode_->Release();
    mode_ = mode;

    /* set the extension in the Scintilla windows */
    if (mode_ != 0)
    {
        ITadsWorkbenchEditor *ed;

        /* set it in the first window */
        ed = mode->CreateEditorExtension(sci_->get_handle());
        sci_->set_extension(ed);
        if (ed != 0)
            ed->Release();

        /* set it in the second window */
        ed = mode->CreateEditorExtension(sci2_->get_handle());
        sci2_->set_extension(ed);
        if (ed != 0)
            ed->Release();
    }

    /* remember the mode command */
    mode_cmd_id_ = cmd;

    /* refresh my appearance, since we have a new set of styles */
    dbgmain_->note_formatting_changes(this);

}

void CHtmlSys_dbgsrcwin::idw_reformat(const CHtmlRect *new_scroll_pos)
{
    /*
     *   we don't need to do anything here - Scintilla keeps track of all of
     *   its own format information and redraws as needed
     */
}

void CHtmlSys_dbgsrcwin::idw_post_load_init()
{
    char sel[50];
    long sel1, sel2;
    const textchar_t *p;

    /* try restoring the window split fraction */
    if (!dbgmain_->get_local_config()->getval(
        "src win split", get_title(), 0, sel, sizeof(sel)))
    {
        /* parse it and set it */
        sscanf(sel, "%lf", &split_);

        /* if the top window is visible, make it initially active */
        if (split_ != 0.0)
            asci_ = sci_;
        else
            asci_ = sci2_;

        /* update the window layout for the new split */
        split_layout();
    }

    /* try restoring the selection range */
    if (!dbgmain_->get_local_config()->getval(
        "src win selrange", get_title(), 0, sel, sizeof(sel)))
    {
        /* parse it and set it */
        sscanf(sel, "%lu,%lu", &sel1, &sel2);
        sci_->set_sel_range(sel1, sel2);
    }

    /* likewise for the bottom panel */
    if (!dbgmain_->get_local_config()->getval(
        "src win selrange", get_title(), 1, sel, sizeof(sel)))
    {
        long sel1, sel2;

        /* parse it and set it */
        sscanf(sel, "%lu,%lu", &sel1, &sel2);
        sci2_->set_sel_range(sel1, sel2);
    }

    /* use the same editor mode we were last using */
    if (!dbgmain_->get_local_config()->getval(
        "src win edit mode", get_title(), 0, sel, sizeof(sel)))
    {
        twb_editor_mode_iter iter(dbgmain_);
        ITadsWorkbenchEditorMode *mode;
        int i;

        /* look up the mode */
        for (i = 0, mode = iter.first() ; mode != 0 ; ++i, mode = iter.next())
        {
            textchar_t name[128];

            /* get the name */
            ansi_from_bstr(name, sizeof(name), mode->GetModeName(), TRUE);

            /* if this is the one, stop looking */
            if (strcmp(name, sel) == 0)
            {
                /* this is the one - set this mode */
                set_editor_mode(mode, ID_EDIT_LANGMODE + i);

                /* no need to look further */
                break;
            }
        }
    }

    /* restore our wrapping mode */
    if ((p = dbgmain_->get_local_config()->getval_strptr(
        "line wrap", get_title(), 0)) == 0)
        p = "N";
    wrap_mode_ = *p;
    update_wrap_mode();

    /*
     *   if this isn't a local project file, and we're set to open non-local
     *   files as read-only, set it to readonly
     */
    if (!dbgmain_->is_local_project_file(path_.get())
        && dbgmain_->get_global_config()->getval_int(
            "editor", "open libs readonly", FALSE))
        set_readonly(TRUE);

    /* restore bookmarks from our internal records */
    restore_bookmarks();
}

/* update our wrapping mode in Scintilla */
void CHtmlSys_dbgsrcwin::update_wrap_mode()
{
    /* make sure the mode is valid */
    if (strchr("NCW", wrap_mode_) == 0)
        wrap_mode_ = 'N';

    /* set it in Scintilla */
    sci_->set_wrap_mode(wrap_mode_);
    sci2_->set_wrap_mode(wrap_mode_);
}

void CHtmlSys_dbgsrcwin::idw_show_source_line(int linenum)
{
    /* move the cursor to this line, adjusting from our 1-based numbering */
    asci_->go_to_line(linenum - 1);
}

void CHtmlSys_dbgsrcwin::idsw_select_line()
{
    asci_->select_current_line();
}

void CHtmlSys_dbgsrcwin::idsw_note_debug_format_changes(
    HTML_color_t bkg_color, HTML_color_t text_color,
    int use_windows_colors, int use_windows_bgcolor,
    HTML_color_t sel_bkg_color, HTML_color_t sel_text_color,
    int use_win_sel, int use_win_sel_bg)
{
    CHtmlFontDesc defstyle, gstyle, gselstyle;
    CHtmlFontDesc *styles, *s;
    int *sci_idx;
    int syntax;
    CHtmlDebugConfig *gc = dbgmain_->get_global_config();
    int tab_size, indent_size, use_tabs;
    int ovr;
    int i, cnt;

    /* allocate space for the syntax styles for the mode */
    cnt = mode_->GetStyleCount();
    styles = new CHtmlFontDesc[cnt];
    sci_idx = new int[cnt];

    /* get the style indices */
    for (i = 0 ; i < cnt ; ++i)
    {
        wb_style_info wi;

        /* get the settings from the mode */
        mode_->GetStyleInfo(i, &wi);

        /* save the index in our list */
        sci_idx[i] = wi.sci_index;
    }

    /* get the styles for the mode */
    dbgmain_->get_syntax_style_info(mode_, &syntax, &defstyle, styles, cnt);

    /* get the global style settings */
    dbgmain_->get_global_style(&gstyle, &gselstyle);

    /* apply defaults */
    if (gstyle.face[0] == '\0')
        strcpy(gstyle.face, "Courier New");
    if (gstyle.pointsize == 0)
        gstyle.pointsize = 10;

    /* note the "override scintilla selection colors" setting */
    ovr = gc->getval_int("src win options", "override sci sel colors", FALSE);

    /* set the base font */
    sci_->set_base_font(&gstyle, &gselstyle, ovr);
    sci2_->set_base_font(&gstyle, &gselstyle, ovr);

    /* set the default style for the mode */
    sci_->set_base_custom_style(&defstyle);
    sci2_->set_base_custom_style(&defstyle);

    /*
     *   if syntax coloring is enabled, set the individual syntax colors;
     *   otherwise just leave everything as is, with all styles set to the
     *   base style for the mode
     */
    if (syntax)
    {
        /* run through the mode's special styles and assign each one */
        for (i = 0, s = styles ; i < cnt ; ++i, ++s)
        {
            sci_->set_custom_style(sci_idx[i], s);
            sci2_->set_custom_style(sci_idx[i], s);
        }
    }

    /* done with the style and index lists */
    delete [] styles;
    delete [] sci_idx;

    /* update the tab and indent settings in Scintilla */
    if (gc->getval("src win options", "tab size", &tab_size))
        tab_size = 8;
    if (gc->getval("src win options", "indent size", &indent_size))
        indent_size = 4;
    if (gc->getval("src win options", "use tabs", &use_tabs))
        use_tabs = FALSE;

    /* set the indent options in scintilla */
    sci_->set_indent_options(indent_size, tab_size, use_tabs);
    sci2_->set_indent_options(indent_size, tab_size, use_tabs);

    /* set the wrapping & margin options */
    int margin = gc->getval_int("src win options", "right margin column", 80);
    int wrap = gc->getval_int("src win options", "auto wrap", FALSE);
    int show = gc->getval_int("src win options", "right margin guide", FALSE);
    sci_->set_wrapping_options(margin, wrap, show);
    sci2_->set_wrapping_options(margin, wrap, show);

    /* set the line number visibility */
    int showlines = gc->getval_int(
        "src win options", "show line numbers", FALSE);
    sci_->show_line_numbers(showlines);
    sci2_->show_line_numbers(showlines);

    /* set the folding visibility */
    int showfold = gc->getval_int(
        "src win options", "show folding", FALSE);
    sci_->show_folding(showfold);
    sci2_->show_folding(showfold);

    /* if we have an editor extension, notify it of the change */
    if (sci_->get_extension() != 0)
        sci_->get_extension()->OnSettingsChange();
    if (sci2_->get_extension() != 0)
        sci2_->get_extension()->OnSettingsChange();
}

/*
 *   on creation, instantiate the Scintilla control
 */
void CHtmlSys_dbgsrcwin::do_create()
{
    /* inherit the default handling */
    CTadsWin::do_create();

    /* create the Scintilla controls */
    sci_ = new ScintillaWin(this);
    sci2_ = new ScintillaWin(this);
    sci2_->set_doc(sci_);

    /*
     *   in the default configuration, the top scintilla window isn't
     *   visible, since the split gives the bottom one the whole window; so
     *   make the bottom one initially active
     */
    asci_ = sci2_;

    /*
     *   post ourselves a message to customize our window menu - do this via
     *   a posted message so that we wait until after the Windows MDI handler
     *   has had chance to do its own customization
     */
    PostMessage(handle_, SRCM_AFTER_CREATE, 0, 0);

    /* create our UI sync timer */
    SetTimer(handle_, ui_sync_timer_ = alloc_timer_id(), 50, 0);
}

/*
 *   deletion
 */
void CHtmlSys_dbgsrcwin::do_destroy()
{
    long sel1, sel2;
    char sel[50];

    /* save the current selection information */
    sci_->get_sel_range(&sel1, &sel2);
    sprintf(sel, "%lu,%lu", sel1, sel2);
    dbgmain_->get_local_config()->setval(
        "src win selrange", get_title(), 0, sel);

    sci2_->get_sel_range(&sel1, &sel2);
    sprintf(sel, "%lu,%lu", sel1, sel2);
    dbgmain_->get_local_config()->setval(
        "src win selrange", get_title(), 1, sel);

    /* save the split ratio */
    sprintf(sel, "%lf", split_);
    dbgmain_->get_local_config()->setval(
        "src win split", get_title(), 0, sel);

    /* save the editor mode */
    if (mode_ != 0)
    {
        ansi_from_bstr(sel, sizeof(sel), mode_->GetModeName(), TRUE);
        dbgmain_->get_local_config()->setval(
            "src win edit mode", get_title(), 0, sel);
    }

    /* save the line-wrap mode */
    sel[0] = wrap_mode_;
    sel[1] = '\0';
    dbgmain_->get_local_config()->setval("line wrap", get_title(), 0, sel);

    /*
     *   Translate our bookmark handles to actual line numbers.  The handles
     *   won't be valid after this point, because the Scintilla window is
     *   being destroyed; but by the same token, we don't need the handles
     *   any longer, since the line numbers will be effectively frozen by the
     *   fact that we won't be doing any more editing of the file.
     */
    resolve_bookmarks(TRUE);

    /* notify the debugger interface */
    dbgmain_->dbghostifc_on_destroy(this, this);

    /* tell the debugger we're closing an active window */
    dbgmain_->on_close_dbg_win(this);

    /* release the interface */
    dbgmain_->Release();

    /* release our scintilla objects */
    sci_->Release();
    sci2_->Release();

    /* forget the Scintilla objects */
    sci_ = sci2_ = asci_ = 0;

    /* done with our UI sync timer */
    KillTimer(handle_, ui_sync_timer_);
    free_timer_id(ui_sync_timer_);

    /* do the normal work */
    CTadsWin::do_destroy();
}

/*
 *   close/save dialog
 */
class CCloseSaveDialog: public CTadsDialog
{
public:
    CCloseSaveDialog(const textchar_t *fname)
    {
        fname_ = fname;
    }

    void init()
    {
        char buf[128];
        char newbuf[MAX_PATH + 128];
        CTadsDialog::init();

        /* initialize the filename string */
        GetDlgItemText(handle_, IDS_PROMPT, buf, sizeof(buf));
        sprintf(newbuf, buf, fname_);
        SetDlgItemText(handle_, IDS_PROMPT, newbuf);
    }

    /* handle a command */
    int do_command(WPARAM id, HWND ctl)
    {
        switch (id)
        {
        case IDYES:
        case IDYESALL:
        case IDNOALL:
        case IDNO:
        case IDCANCEL:
            /* dismiss the dialog */
            EndDialog(handle_, id);
            return TRUE;

        case IDOK:
            return FALSE;
        }

        /* inherit the default handling */
        return CTadsDialog::do_command(id, ctl);
    }

    /* the filename we're asking about */
    const textchar_t *fname_;
};

/*
 *   query for close
 */
int CHtmlSys_dbgsrcwin::query_close(struct ask_save_ctx *ctx)
{
    /* if we're dirty, prompt before closing */
    if (dirty_)
    {
        int dlg_id;
        int id;

        /*
         *   if there's no context, use the default save dialog; otherwise
         *   use the dialog from the context
         */
        dlg_id = (ctx != 0 ? ctx->dlg_id : DLG_ASK_SAVE);

        /*
         *   if they've already said "Save All", go ahead and try the save;
         *   otherwise, ask about it
         */
        if (ctx != 0 && ctx->save_all)
        {
            /* they pre-answered in the affirmative */
            id = IDYES;
        }
        else if (ctx != 0 && ctx->save_none)
        {
            /* they pre-answered in the negative */
            id = IDNO;
        }
        else
        {
            /* run the save-on-close dialog */
            CCloseSaveDialog dlg(path_.get());
            id = dlg.run_modal(
                dlg_id, handle_, CTadsApp::get_app()->get_instance());
        }

        /* check what they want to do */
        switch (id)
        {
        case IDYESALL:
            /* set the 'save all' flag */
            ctx->save_all = TRUE;

            /* FALL THROUGH to the normal save... */

        case IDYES:
            /* save the file; if that succeeds, proceed */
            return save_file();

        case IDNOALL:
            /* set the 'save none' flag */
            ctx->save_none = TRUE;
            break;

        case IDCANCEL:
            /* cancel the save */
            return FALSE;
        }
    }

    /* it's okay to proceed with the close */
    return TRUE;
}


/*
 *   close
 */
int CHtmlSys_dbgsrcwin::do_close()
{
    /* try querying for close; if that's okay, inherit the default handling */
    return (query_close(0) && CTadsWin::do_close());
}

/*
 *   paint
 */
void CHtmlSys_dbgsrcwin::do_paint_content(HDC hdc, const RECT *area_to_draw)
{
    RECT wrc, rc;
    int y;
    HBRUSH br1 = GetSysColorBrush(COLOR_3DFACE);
    HBRUSH br2 = GetSysColorBrush(COLOR_3DSHADOW);

    /*
     *   Draw the splitter bar.
     *
     *   If the split fraction is zero, and we're not tracking, then the
     *   splitter button is minimized to just the scrollbar area - the text
     *   window gets the full client height.  We don't draw anything in this
     *   case, since we leave it up to the scintilla window to do that
     *   drawing for us.
     *
     *   Otherwise, the splitter is drawn all the way across the window.
     */
    GetClientRect(handle_, &wrc);
    y = (int)(wrc.bottom * split_);
    if (y != 0 || tracking_splitter_)
    {
        /*
         *   we have a non-zero split, so draw the splitter across the whole
         *   window
         */
        SetRect(&rc, 0, y, wrc.right, y + 5);
        FillRect(hdc, &rc, br2);
        rc.top += 1;
        rc.bottom -= 1;
        FillRect(hdc, &rc, br1);
    }
}


/*
 *   handle a non-client left click
 */
int CHtmlSys_dbgsrcwin::do_nc_leftbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* if it's a title-bar hit, track the context menu */
    if (hit_type == HTCAPTION)
    {
        /*
         *   if focus isn't within our window, move focus here - this makes
         *   it easy to move focus back here from a docked window
         */
        HWND f = GetFocus();
        if (f != handle_ && !IsChild(handle_, f))
            SetFocus(handle_);
    }

    /* proceed to the standard system handling */
    return FALSE;
}


/*
 *   handle a non-client right-click
 */
int CHtmlSys_dbgsrcwin::do_nc_rightbtn_down(
    int keys, int x, int y, int clicks, int hit_type)
{
    /* if it's a title-bar hit, track the context menu */
    if (hit_type == HTCAPTION)
    {
        POINT pt;

        /* convert the location to client coordinates */
        pt.x = x;
        pt.y = y;
        ScreenToClient(handle_, &pt);

        /* show the system menu as a context menu */
        track_system_context_menu(pt.x, pt.y);

        /* handled */
        return TRUE;
    }

    /* we didn't handle it */
    return FALSE;
}

/*
 *   toggle a breakpoint at the current source position
 */
void CHtmlSys_dbgsrcwin::isp_toggle_breakpoint(ScintillaWin *sci)
{
    /* note that the originating window is the last active window */
    asci_ = sci;

    /* tell the helper to toggle the breakpoint */
    helper_->toggle_breakpoint(dbgmain_->get_dbg_ctx(), this);
}

/*
 *   evaluate an expression
 */
int CHtmlSys_dbgsrcwin::isp_eval_tip(
    const textchar_t *expr, textchar_t *valbuf, size_t vallen)
{
    /* if we're not stopped in the debugger, show nothing */
    if (!dbgmain_->get_in_debugger())
        return FALSE;

    /*
     *   Evaluate the expression.  Since this is for a tooltip, use
     *   "speculative" mode - these evaluations aren't anything the user
     *   asked for explicitly, so we don't want to trigger any side effects.
     */
    return !helper_->eval_expr(dbgmain_->get_dbg_ctx(), valbuf, vallen,
                               expr, 0, 0, 0, 0, TRUE);
}

/*
 *   begin dragging the current line pointer
 */
void CHtmlSys_dbgsrcwin::isp_drag_cur_line(ScintillaWin *sci)
{
    /* note that we're in current-line drag mode */
    dragging_cur_line_ = TRUE;
    drag_target_line_ = drag_start_line_ = sci->get_cursor_linenum();
    drag_line_ticks_ = 0;
    drag_line_ever_moved_ = FALSE;
    drag_sci_ = sci;

    /* set up a timer while we're dragging */
    SetTimer(handle_, drag_timer_ = alloc_timer_id(), 50, 0);

    /* grab capture */
    SetCapture(handle_);
}

/* set the cursor shape */
int CHtmlSys_dbgsrcwin::do_setcursor(HWND hwnd, int hittest, int mousemsg)
{
    /* only pay attention to hits in the client region */
    if (hittest == HTCLIENT)
    {
        POINT pt;
        RECT rc;
        int ht;

        /* if the mouse is over the splitter, set the splitter cursor */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);
        GetClientRect(handle_, &rc);
        ht = (int)(split_ * rc.bottom);
        if (pt.y >= ht && pt.y < ht + 5)
        {
            /* it's over the splitter - this is the cursor to use */
            SetCursor(split_csr_);
            return TRUE;
        }
    }

    /* inherit the default handling */
    return CTadsWin::do_setcursor(hwnd, hittest, mousemsg);
}

/* mouse movement */
int CHtmlSys_dbgsrcwin::do_mousemove(int keys, int x, int y)
{
    int oldpos;
    int linenum;

    /* if we're dragging, check for a new position */
    if (dragging_cur_line_
        && drag_sci_->check_margin_drag(x, y, &oldpos, &linenum))
    {
        /*
         *   if the line number is changing from the last drag location,
         *   check to see if the new position is valid and in the same
         *   function
         */
        if (linenum != drag_target_line_
            && helper_->is_valid_next_statement(
                dbgmain_->get_dbg_ctx(), this))
        {
            /* show the drag cursor as soon as we start moving */
            SetCursor(line_drag_csr_);

            /* remove any old drag line */
            drag_sci_->show_drag_line(drag_target_line_, FALSE);

            /* note the new target line */
            drag_target_line_ = linenum;

            /* set the 'drag line' status bit if this isn't the start line */
            if (linenum != drag_start_line_)
            {
                /* update the status bit */
                drag_sci_->show_drag_line(linenum, TRUE);

                /* note that we've moved from the start line */
                drag_line_ever_moved_ = TRUE;
            }
        }

        /* restore the old position */
        drag_sci_->end_margin_drag(oldpos);
    }
    else if (tracking_splitter_)
    {
        int ht;
        int newht;
        RECT rc;
        double new_split;

        /* calculate the old split height */
        GetClientRect(handle_, &rc);
        ht = (int)(split_ * rc.bottom);

        /* calculate the new split fraction */
        new_split = (double)(y - drag_splitter_yofs_) / (double)rc.bottom;

        /* constrain it to 90% */
        if (new_split > 0.9)
            new_split = 0.9;

        /* calculate the new window height for the split fraction */
        newht = (int)(new_split * rc.bottom);

        /*
         *   don't let the height get below zero, and don't let the bottom
         *   window go below a minimum size
         */
        if (newht < 0)
        {
            newht = 0;
            new_split = 0;
        }

        /* if that changes the layout, draw the new layout */
        if (newht != ht)
        {
            /* set the new split fraction */
            split_ = new_split;

            /* invalidate the splitter control in the old and new positions */
            rc.top = min(ht, newht);
            rc.bottom = max(ht + 5, newht + 5);
            InvalidateRect(handle_, &rc, TRUE);

            /* adjust the window layout */
            split_layout();
        }
    }

    /* inherit the base handling */
    return CTadsWin::do_mousemove(keys, x, y);
}

/* timer */
int CHtmlSys_dbgsrcwin::do_timer(int id)
{
    /* check for our drag timer */
    if (id == drag_timer_)
    {
        POINT pt;
        RECT rc;

        /* set the cursor if it's been a few moments */
        if (++drag_line_ticks_ >= 5)
            SetCursor(line_drag_csr_);

        /* get the mouse position in Scintilla client coordinates */
        GetCursorPos(&pt);
        ScreenToClient(drag_sci_->get_handle(), &pt);

        /* get the Scintilla window's bounds */
        GetClientRect(drag_sci_->get_handle(), &rc);

        /* if we're outside of the client area, scroll Scintilla */
        if (pt.y < rc.top || pt.y > rc.bottom)
        {
            /* scroll up or down a line, as appropriate */
            drag_sci_->scroll_by(pt.y < rc.top ? -1 : 1, 0);
        }

        /* handled */
        return TRUE;
    }

    /* check for our UI sync timer */
    if (id == ui_sync_timer_)
    {
        /*
         *   If we have a pending UI modification, and it's been long enough
         *   since the last change, sync the UI.  Wait until we've been idle
         *   for a while - the sync operation is *fairly* quick, but doing it
         *   repeatedly could start bogging things down, so we want to make
         *   sure the user is really pausing for a moment before starting.
         */
        if (mod_pending_ && (DWORD)(GetTickCount() - last_mod_time_) > 100)
        {
            /*
             *   synchronize our internal line status records with the
             *   Scintilla indicators
             */
            sync_indicators();

            /* we're synchronized now */
            mod_pending_ = FALSE;
        }

        /* handled */
        return TRUE;
    }

    /* not ours - inherit the base handling */
    return CTadsWin::do_timer(id);
}

/* ------------------------------------------------------------------------ */
/*
 *   line number sorter for line status records
 */
static int sort_by_linenum(const void *a0, const void *b0)
{
    CHtmlDbg_line_stat *a = (CHtmlDbg_line_stat *)a0;
    CHtmlDbg_line_stat *b = (CHtmlDbg_line_stat *)b0;
    return a->linenum - b->linenum;
}

/*
 *   Synchronize our internal line status records with the Scintilla line
 *   indicators.
 */
void CHtmlSys_dbgsrcwin::sync_indicators()
{
    /* count our internal line records */
    int cnt = helper_->get_line_status_list(this, 0, 0);

    /*
     *   if there are no internal line records, there can't be any indicators
     *   in the window, since we always create an internal record when
     *   setting a window indicator; so there's nothing to do in this case
     */
    if (cnt == 0)
        return;

    /*
     *   If we're in design mode, we can fix up the internal records so they
     *   match the Scintilla records.
     *
     *   If we're in debug mode, there's no good way to do fix the internal
     *   records, since the internal records are derived from the compiled
     *   game's debug records, which were in turn derived from the original
     *   source code (as it was at the last compile).  The debug records are
     *   for all practical purposes read-only, since they're embedded in the
     *.  gam/.t3 file; but even if they weren't, it would be hard to come up
     *   with a reliable algorithm for fixing them up based on the
     *   superficial changes to the source text, since the whole meaning of
     *   the code could be changed by even small source edits.  The only
     *   reliable way to update the debug records is to recompile, which is
     *   obviously way beyond the scope of what we can do here, since this
     *   routine needs to be about as fast as a "paint" operation to keep the
     *   UI responsive.  Since there's nothing we can do to fix up the
     *   debug records, the only good alternative is to do the opposite: fix
     *   up the Scintilla indicators so they match what's in the debug records.
     */
    if (!dbgmain_->get_game_loaded())
    {
        /*
         *   We're in design mode, so we can sync our internal records with
         *   the Scintilla indicators.
         */

        /* get the list of internal line records */
        CHtmlDbg_line_stat *stat = new CHtmlDbg_line_stat[cnt];
        helper_->get_line_status_list(this, stat, cnt);

        /* sort them by ascending line number */
        qsort(stat, cnt, sizeof(stat[0]), &sort_by_linenum);

        /*
         *   Allocate space for an equal size list of Scintilla markers.
         *   There shouldn't be more - the only Scintilla markers should
         *   correspond to our internal markers.
         */
        int *scilines = new int[cnt];

        /* build the list of scintilla lines */
        int i, l;
        const unsigned int markers = HTMLTDB_STAT_BP
                                     | HTMLTDB_STAT_BP_DIS
                                     | HTMLTDB_STAT_BP_COND;
        for (i = 0, l = 0;
             i < cnt && (l = asci_->next_marker_line(l, markers, 0)) != -1 ;
             ++i, ++l)
        {
            /* save this line */
            scilines[i] = l + 1;
        }

        /*
         *   note the number of Scintilla markers we actually found; this
         *   might be less than the number of intenal markers, because
         *   Scintilla can consolidate markers when lines are deleted
         */
        int scicnt = i;

        /* now sync up the two lists */
        for (i = 0 ; i < scicnt ; ++i)
        {
            /* move this breakpoint to match the Scintilla marker line */
            if (stat[i].linenum != scilines[i])
                helper_->move_breakpoint(
                    dbgmain_->get_dbg_ctx(), stat[i].bpnum, scilines[i]);
        }

        /* refresh the breakpoint status for this window */
        helper_->refresh_bp_line_status(this);

        /*
         *   if we have more line records than Scintilla markers, delete the
         *   excess line records
         */
        for ( ; i < cnt ; ++i)
            helper_->delete_breakpoint(
                dbgmain_->get_dbg_ctx(), stat[i].bpnum);

        /* done with the line status lists */
        delete [] stat;
        delete [] scilines;
    }

    /*
     *   In any case, reset Scintilla so that it's displaying our current
     *   internal records.  This ensures that any weirdness in merging the
     *   two sets of markers is resolved by forcing both to match the new
     *   internal set.
     *
     *   To sync Scintilla with our internal markers, simply clear out all of
     *   Scintilla's indicators, then update Scintilla with the current set
     *   of line status records.
     */
    asci_->reset_line_status();
    helper_->update_lines_on_screen(this);
}

/*
 *   Set Scintilla markers from our internal marker list.
 */
void CHtmlSys_dbgsrcwin::restore_indicators()
{
    /* clear everything from the scintilla window */
    asci_->reset_line_status();

    /* set scintilla's indicators from our internal records */
    helper_->update_lines_on_screen(this);
}

/*
 *   Restore Scintilla bookmarks from our internal records.
 */
void CHtmlSys_dbgsrcwin::restore_bookmarks()
{
    tdb_bookmark_file *f;

    /* set Scintilla markers for our bookmarks */
    if ((f = tdb_bookmark_file::find(dbgmain_, path_.get(), FALSE)) != 0)
    {
        /* scan the bookmark list for entries attached to this file */
        for (tdb_bookmark *b = dbgmain_->bookmarks_ ; b != 0 ; b = b->nxt)
        {
            /*
             *   If this bookmark is for our file, and we don't already have
             *   a valid marker handle for it, add a marker in the Scintilla
             *   window.  Remember the new marker handle in the bookmark
             *   object, so that we can keep track of the marker as it moves
             *   around during editing.
             *
             *   Note that we might have already created the bookmark.  There
             *   (currently) two distinct load-time notifications that reach
             *   this routine, and sometimes we get both notifications for
             *   the same window.  So if the bookmark already exists, just
             *   skip it, since we must have already restored bookmarks for
             *   this window.xs
             */
            if (b->file == f && b->handle == -1)
                b->handle = asci_->add_bookmark(b->line);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   context menu
 */
int CHtmlSys_dbgsrcwin::do_context_menu(HWND hwnd, int x, int y)
{
    POINT pt;
    HMENU m;
    int margin;

    /* get the position in client coordinates */
    pt.x = x;
    pt.y = y;
    ScreenToClient(hwnd, &pt);

    /* note if it's a margin click */
    margin = asci_->pt_in_margin(pt.x, pt.y);

    /* if the click is in the margin, move the cursor to this line */
    if (margin)
        asci_->cursor_to_line(pt.x, pt.y);

    /* select the context menu by file type and click location */
    if (is_script_file())
    {
        /* use the script file menu */
        m = script_menu_;
    }
    else
    {
        /* use the regular source file menu or margin menu, depending */
        m = (margin ? margin_menu_ : ctx_menu_);
    }

    /* show our context menu */
    track_context_menu(GetSubMenu(m, 0), pt.x, pt.y);

    /* handled */
    return TRUE;
}

/* left button click */
int CHtmlSys_dbgsrcwin::do_leftbtn_down(int keys, int x, int y, int clicks)
{
    RECT wrc;
    int ht;

    /* if it's in the splitter, track the splitter */
    GetClientRect(handle_, &wrc);
    ht = (int)(wrc.bottom * split_);
    if (y >= ht && y < ht + 5)
    {
        POINT pt;

        /*
         *   Now that we've qualified the click as being within bounds, get
         *   the real cursor position, in case this is from the internal
         *   splitter control in the scintilla window.  The scintilla
         *   window's version of the splitter might vary slightly in height
         *   from our own, so it forces the y position to zero to make sure
         *   we recognize the click as within our bounds.  But for tracking
         *   purposes, we want the true original position.
         */
        GetCursorPos(&pt);
        ScreenToClient(handle_, &pt);
        y = pt.y;

        /* if it's a double-click, toggle between half and zero split */
        if (clicks == 2)
        {
            /* change the split mode */
            split_ = ht != 0 ? 0.0 : 0.5;

            /* adjust the split layout */
            split_layout();
        }
        else
        {
            /* set tracking mode */
            tracking_splitter_ = TRUE;

            /*
             *   if the splitter was at zero, entering tracking mode slightly
             *   changes the layout, so show the new layout immediately
             */
            if (ht == 0)
                split_layout();

            /* remember the original y offset of the mouse in the control */
            drag_splitter_yofs_ = y - ht;

            /* capture the mouse while doing the tracking */
            SetCapture(handle_);
        }

        /* handled */
        return TRUE;
    }

    /* inherit the base handling */
    return CTadsWin::do_leftbtn_down(keys, x, y, clicks);
}

/* left button release */
int CHtmlSys_dbgsrcwin::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're dragging the current line pointer, finish the drag */
    if (dragging_cur_line_)
    {
        /*
         *   If we never dragged anywhere, toggle a breakpoint here.
         *   Otherwise, if we've selected a different line from the original,
         *   move here.
         */
        if (!drag_line_ever_moved_)
        {
            int oldpos;

            /* temporarily drag to this line */
            drag_sci_->do_margin_drag(drag_target_line_, &oldpos);

            /* toggle a breakpoint at the current location */
            helper_->toggle_breakpoint(dbgmain_->get_dbg_ctx(), this);

            /* end the temporary drag */
            drag_sci_->end_margin_drag(oldpos);
        }
        else if (drag_target_line_ != drag_start_line_)
        {
            int oldpos;
            int need_single_step;

            /* temporarily drag to this line */
            drag_sci_->do_margin_drag(drag_target_line_, &oldpos);

            /* move the next statement here */
            if (!helper_->set_next_statement(
                dbgmain_->get_dbg_ctx(), dbgmain_->get_exec_ofs_ptr(),
                this, &need_single_step) && need_single_step)
            {
                /* we need a single-step after the change */
                dbgmain_->exec_step_into();
            }

            /* end the temporary drag */
            drag_sci_->end_margin_drag(oldpos);
        }

        /* release capture to end the dragging */
        ReleaseCapture();
    }
    else if (tracking_splitter_)
    {
        /* release the split capture */
        ReleaseCapture();
    }

    /* inherit the base handling */
    return CTadsWin::do_leftbtn_up(keys, x, y);
}

/* end capture */
int CHtmlSys_dbgsrcwin::do_capture_changed(HWND hwnd)
{
    /* if we're dragging, end capture */
    if (dragging_cur_line_)
    {
        /* turn off the drag line indicator in the editor */
        drag_sci_->show_drag_line(drag_target_line_, FALSE);

        /* cancel our drag timer */
        KillTimer(handle_, drag_timer_);
        free_timer_id(drag_timer_);
        drag_timer_ = 0;

        /* end drag mode */
        dragging_cur_line_ = FALSE;
    }
    else if (tracking_splitter_)
    {
        /* end tracking mode */
        tracking_splitter_ = FALSE;

        /* show the final layout */
        split_layout();
    }

    /* inherit the base handling */
    return CTadsWin::do_capture_changed(hwnd);
}


/*
 *   set the 'dirty' (modified) status of the document
 */
void CHtmlSys_dbgsrcwin::isp_set_dirty(int dirty)
{
    /* if the status isn't changing, ignore this */
    if ((int)dirty_ == dirty)
        return;

    /* set the dirty flag */
    dirty_ = dirty;

    /* update our window title */
    update_win_title();
}

/*
 *   receive notification of a focus change in the Scintilla control
 */
void CHtmlSys_dbgsrcwin::isp_on_focus_change(ScintillaWin *sci,
                                             int gaining_focus)
{
    /* notify myself of an artificial activation */
    do_parent_activate(gaining_focus);

    /* remember the active scintilla panel */
    asci_ = sci;
}

/*
 *   receive notification of a change in cursor position
 */
void CHtmlSys_dbgsrcwin::isp_update_cursorpos(int lin, int col)
{
    if (dbgmain_->get_active_mdi_child() == handle_)
        dbgmain_->update_cursorpos(lin, col);
}

/*
 *   update our window title with the current name
 */
void CHtmlSys_dbgsrcwin::update_win_title()
{
    char buf[MAX_PATH + 25];

    /* show a "*" after the name in the window title if it's modified */
    strcpy(buf, title_.get());
    if (dirty_)
        strcat(buf, "*");

    /* if it's read-only, say so */
    if (readonly_)
        strcat(buf, " [RO]");

    /* set the window title */
    SetWindowText(handle_, buf);

    /* set the MDI tab bar title to the root name */
    dbgmain_->update_win_title(this);
}

/*
 *   gain focus
 */
void CHtmlSys_dbgsrcwin::do_setfocus(HWND prv)
{
    /* inherit default */
    CTadsWin::do_setfocus(prv);

    /* set focus on the last active Scintilla control */
    SetFocus(asci_->get_handle());
}

/*
 *   handle non-client activation
 */
int CHtmlSys_dbgsrcwin::do_ncactivate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent
 */
void CHtmlSys_dbgsrcwin::do_parent_activate(int flag)
{
    /* set active/inactive with the main window */
    dbgmain_->notify_active_dbg_win(this, flag);

    /* if I'm activating, update the status line with my cursor position */
    if (flag)
        isp_update_cursorpos(asci_->get_cursor_linenum(),
                             asci_->get_cursor_column());

    /* inherit default handling */
    CTadsWin::do_parent_activate(flag);
}

/*
 *   resize
 */
void CHtmlSys_dbgsrcwin::do_resize(int mode, int x, int y)
{
    /* figure the new split layout */
    split_layout();

    /* remember the change in the preferences after the move has completed */
    PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
}

/*
 *   figure the new splitter layout
 */
void CHtmlSys_dbgsrcwin::split_layout()
{
    RECT rc;
    int ht;
    int y;

    /* figure the size of the top window */
    GetClientRect(handle_, &rc);
    ht = (int)(rc.bottom * split_);

    /*
     *   If the height is below the scrollbar height, and we're not in
     *   tracking mode, give the bottom window the full client area, without
     *   even showing the splitter bar across the top.  We never let the
     *   height get to zero in tracking mode because we want to keep the
     *   splitter shown continuously in that case.
     */
    if (ht < GetSystemMetrics(SM_CYHSCROLL) && !tracking_splitter_)
    {
        /* hide top window entirely, and minimize the splitter */
        ht = 0;
        y = 0;
        split_ = 0.0;
    }
    else
    {
        /* figure the bottom window position below the splitter */
        y = ht + 5;
    }

    /* move the windows */
    MoveWindow(sci_->get_handle(), 0, 0, rc.right, ht, TRUE);

    /* set the bottom window position */
    MoveWindow(sci2_->get_handle(), 0, y, rc.right, rc.bottom - y, TRUE);

    /*
     *   if the bottom window gets the whole area, tell it to show its
     *   internal splitter in its scrollbar
     */
    sci2_->show_splitter(y == 0);

    /*
     *   if the focus was in the top window, and the top window is now gone,
     *   and we're done tracking, move focus to the bottom window
     */
    if (!tracking_splitter_ && asci_ == sci_ && y == 0)
    {
        asci_ = sci2_;
        SetFocus(sci2_->get_handle());
    }
}

/*
 *   move
 */
void CHtmlSys_dbgsrcwin::do_move(int x, int y)
{
    /* remember the change in the preferences after the move has completed */
    PostMessage(handle_, HTMLM_SAVE_WIN_POS, 0, 0);
}

/*
 *   handle a user message
 */
int CHtmlSys_dbgsrcwin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see what we have */
    switch (msg)
    {
    case HTMLM_SAVE_WIN_POS:
        /* do the deferred window position saving */
        store_winpos_prefs();
        return TRUE;

    case SRCM_AFTER_CREATE:
        /* customize the system menu */
        {
            /* add some custom commands to the system window menu */
            HMENU m = GetSystemMenu(handle_, FALSE);

            /* set up the basic MENUITEMINFO entries */
            MENUITEMINFO info;
            info.cbSize = menuiteminfo_size_;
            info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_ID;
            info.fType = MFT_STRING;

            /* add our items */
            int i = 0;
            info.wID = ID_FILE_SAVE;
            info.dwTypeData = "&Save";
            InsertMenuItem(m, i++, TRUE, &info);

            info.wID = ID_PROJ_ADDCURFILE;
            info.dwTypeData = "Add to Pro&ject";
            InsertMenuItem(m, i++, TRUE, &info);

            info.fType = MFT_SEPARATOR;
            InsertMenuItem(m, i++, TRUE, &info);
        }
        return TRUE;
    }

    /* inherit the default handling */
    return CTadsWin::do_user_message(msg, wpar, lpar);
}

/*
 *   Store my position in the preferences.  This is invoked from our
 *   self-posted follow-up message, which we post during a size/move event
 *   for deferred processing after the triggering event has finished.
 */
void CHtmlSys_dbgsrcwin::store_winpos_prefs()
{
    RECT rc;
    CHtmlRect pos;
    HWND hwnd, mdihwnd;

    /* get my frame position */
    hwnd = get_frame_pos(&rc);

    /* ignore this if we're not the active MDI window, or we're maximized */
    mdihwnd = dbgmain_->get_active_mdi_child();
    if ((mdihwnd != hwnd && !IsChild(mdihwnd, hwnd)) || is_win_maximized())
        return;

    /* convert to our local position structure */
    pos.set(rc.left, rc.top, rc.right, rc.bottom);

    /*
     *   Store the position in the project (local) configuration.  Store
     *   under the key "src win pos:path", where "path" is our filename.
     */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval(
            "src win pos", path_.get(), &pos);
}


/*
 *   handle a notification from a child control
 */
int CHtmlSys_dbgsrcwin::do_notify(int control_id, int notify_code, LPNMHDR nm)
{
    /* try running it by the scintilla control */
    if (sci_->handle_notify(notify_code, nm)
        || sci2_->handle_notify(notify_code, nm))
        return TRUE;

    /* if we got here without handling it, inherit the default handling */
    return CTadsWin::do_notify(control_id, notify_code, nm);
}

/*
 *   customize a tooltip message
 */
int CHtmlSys_dbgsrcwin::active_get_tooltip(TOOLTIPTEXT *ttt)
{
    char str[50];
    static textchar_t msgbuf[MAX_PATH + 30];
    int ids;

    /* customize the strings for a few commands */
    switch (ttt->hdr.idFrom)
    {
    case ID_FILE_SAVE:
        /* use "Save <filename>" */
        ids = IDS_TPL_SAVE_FNAME;
        goto add_fname;

    case ID_SEARCH_GO:
        /* if the search mode is "Find in file", use our custom string */
        if (dbgmain_->get_searchbar_cmd() == ID_SEARCH_FILE)
        {
            ids = IDS_TPL_FIND_FNAME;
            goto add_fname;
        }
        break;

    case ID_EDIT_FIND:
    case ID_SEARCH_FILE:
        /* use "Find in <filename>" */
        ids = IDS_TPL_FIND_FNAME;

    add_fname:
        /* load the template string */
        if (LoadString(CTadsApp::get_app()->get_instance(),
                       ids, str, sizeof(str)))
        {
            /* format the message with our root filename */
            _snprintf(msgbuf, sizeof(msgbuf) - 1, str,
                      os_get_root_name(path_.get()));
            msgbuf[sizeof(msgbuf)-1] = '\0';

            /* use the formatted string */
            ttt->lpszText = msgbuf;
            return TRUE;
        }
        break;
    }

    /* we don't need to customize it */
    return FALSE;
}

/*
 *   get the caret position
 */
int CHtmlSys_dbgsrcwin::active_get_caretpos(int *line, int *col)
{
    *line = asci_->get_cursor_linenum();
    *col = asci_->get_cursor_column();
    return TRUE;
}

/*
 *   get the caret position
 */
int CHtmlSys_dbgsrcwin::active_get_caretpos(unsigned long *pos)
{
    *pos = asci_->get_cursor_pos();
    return TRUE;
}

/*
 *   begin/end an undo group
 */
void CHtmlSys_dbgsrcwin::active_undo_group(int begin)
{
    asci_->undo_group(begin);
}

/*
 *   find text
 */
int CHtmlSys_dbgsrcwin::active_find_text(
    const textchar_t *txt,
    int exact_case, int wrap, int direction, int regex, int whole_word,
    unsigned long first_match_a, unsigned long first_match_b)
{
    /* ask the active Scintilla window to do the work */
    return asci_->search(txt, exact_case, FALSE, wrap, direction,
                         regex, whole_word,
                         (int)first_match_a, (int)first_match_b);
}

/*
 *   replace the current Find selection
 */
void CHtmlSys_dbgsrcwin::active_replace_sel(
    const textchar_t *txt, int direction, int regex)
{
    /* ask the active Scintilla window to do the work */
    asci_->find_replace_sel(txt, direction, regex);
}

/*
 *   replace all remaining instances of a given string
 */
void CHtmlSys_dbgsrcwin::active_replace_all(
    const textchar_t *txt, const textchar_t *repl,
    int exact_case, int wrap, int direction, int regex, int whole_word)
{
    /* ask the active Scintilla window to do the work */
    asci_->find_replace_all(txt, repl, exact_case, wrap, direction,
                            regex, whole_word);
}


/*
 *   Check the file's current modification timestamp against the value when
 *   we last loaded the file.
 */
int CHtmlSys_dbgsrcwin::file_changed_on_disk() const
{
    WIN32_FIND_DATA ff;
    HANDLE hfind;
    int ret;

    /* if there's no file, there's no timestamp to have changed */
    if (path_.get() == 0)
        return FALSE;

    /*
     *   presume we won't find a change; if we don't find the file at all, we
     *   won't find a change
     */
    ret = FALSE;

    /* look up the file's current timestamp information */
    hfind = FindFirstFile(path_.get(), &ff);
    if (hfind != INVALID_HANDLE_VALUE)
    {
        /* return true if the timestamps are different */
        ret = (CompareFileTime(&ff.ftLastWriteTime, &file_time_) != 0);

        /* we're done with the find handle */
        FindClose(hfind);
    }

    /* return the result */
    return ret;
}

/*
 *   set the read-only flag
 */
void CHtmlSys_dbgsrcwin::set_readonly(int f)
{
    /* note the flag internally */
    readonly_ = f;

    /* set the mode in Scintilla */
    sci_->set_readonly(f);
    sci2_->set_readonly(f);

    /* update the window title, since we show the readonly status there */
    update_win_title();
}

/*
 *   Save the file under the current name.  If the file doesn't yet have a
 *   name, prompt for a name.
 */
int CHtmlSys_dbgsrcwin::save_file()
{
    /*
     *   if we don't yet have a name, prompt for a name and save under the
     *   result; otherwise, save under the current name
     */
    if (untitled_)
        return save_file_as_prompt();
    else
        return save_file_as(path_.get());
}

/*
 *   Save the file, prompting for a name
 */
int CHtmlSys_dbgsrcwin::save_file_as_prompt()
{
    /* ask for the name */
    OPENFILENAME5 info;
    char fname[OSFNMAX];
    CStringBuf filter;
    int i;
    const textchar_t *p;

    /* use the existing name as the default name */
    strcpy(fname, path_.get());

    /* build the filter list */
    dbgmain_->build_text_file_filter(&filter, TRUE);

    /* find our extension in the list */
    info.nFilterIndex = 0;
    for (i = 1, p = filter.get() ; *p != '\0' ; ++i, ++p)
    {
        /* skip the display name and go to the extension list */
        p += get_strlen(p) + 1;

        /* check each extension */
        for (;;)
        {
            /* if it's a "*.xxx", process it */
            if (*p == '*' && *++p == '.')
            {
                const textchar_t *start;

                /* find the end of the extension */
                for (start = ++p ; *p != ';' && *p != '\0' ; ++p) ;

                /* compare it */
                if ((size_t)(p - start) == get_strlen(defext_.get())
                    && memicmp(defext_.get(), start, p - start) == 0)
                {
                    /* this is the one we want - set it and stop looking */
                    info.nFilterIndex = i;
                    goto found_ext;
                }
            }

            /* skip to the next pattern */
            for ( ; *p != ';' && *p != '\0' ; ++p) ;

            /* if we found the '\0', we're done */
            if (*p == '\0')
                break;

            /* skip the semicolon */
            ++p;
        }
    }

found_ext:
    /* ask for the new name */
    info.hwndOwner = handle_;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = filter.get();
    info.lpstrCustomFilter = 0;
    info.lpstrFile = fname;
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.lpstrTitle = "Save File As";
    info.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT
                 | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = (defext_.get()[0] != '\0' ? defext_.get() : 0);
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
    if (!GetSaveFileName((OPENFILENAME *)&info))
    {
        /* canceled - return failure */
        return FALSE;
    }

    /* save the file */
    return save_file_as(fname);
}

/*
 *   Save the contents of the editor window to the given file
 */
int CHtmlSys_dbgsrcwin::save_file_as(const textchar_t *fname)
{
    int was_untitled = untitled_;

    /* have Scintilla do the work */
    if (!sci_->save_file(fname))
    {
        MessageBox(dbgmain_->get_handle(),
                   "An error occurred saving the file. Check "
                   "that the disk and folder you selected were valid, "
                   "and that there's enough disk space.", "TADS Workbench",
                   MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);

        return FALSE;
    }

    /* we have a title now */
    untitled_ = FALSE;

    /* if the name doesn't match our current path, update the name */
    if (stricmp(fname, path_.get()) != 0)
        file_renamed(fname);

    /*
     *   update our recollection of the disk file time - we explicitly wrote
     *   the file to disk, so we're inherently synchronized with what's on
     *   disk as of right now
     */
    update_file_time();

    /*
     *   If we were formerly untitled, and a project is open, and there's a
     *   project window, consider adding the file to the project.  Don't add
     *   script files, since they're handled specially.
     */
    if (was_untitled
        && dbgmain_->get_local_config_filename() != 0
        && dbgmain_->get_proj_win() != 0
        && !is_script_file())
    {
        int add;

        /* assume we're going to add the file */
        add = TRUE;

        /* if we still want to add the file, confirm it if desired */
        if (dbgmain_->get_global_config()->getval_int(
            "ask autoadd new file", 0, TRUE))
        {
            char buf[MAX_PATH + 128];

            sprintf(buf, "Would you like to add %s to the Project "
                    "file list?", os_get_root_name((char *)fname));
            add = (MessageBox(dbgmain_->get_handle(), buf, "TADS",
                              MB_YESNO | MB_ICONQUESTION) == IDYES);
        }

        /* if we're still adding the file, add it */
        if (add)
            dbgmain_->get_proj_win()->auto_add_file(fname);
    }

    /*
     *   if this is a script, resync the script window with the changes to
     *   the files in the script directory
     */
    if (is_script_file())
        dbgmain_->refresh_script_win(FALSE);

    /* note that we've saved a file */
    dbgmain_->note_file_save(fname);

    /* success */
    return TRUE;
}

/*
 *   handle a change to my underlying file's name
 */
void CHtmlSys_dbgsrcwin::file_renamed(const textchar_t *new_name)
{
    /* close any existing window on the new filename */
    helper_->enum_source_windows(
        &dbgmain_->enum_win_close_path_cb, (void *)new_name);

    /* change our file association with the helper */
    helper_->change_file_link(dbgmain_, this, new_name);

    /* save the new name internally */
    set_path(new_name);

    /* use the filename as our new window title */
    title_.set(new_name);
    update_win_title();

    /*
     *   save my window configuration - we save these settings under a key
     *   associated with our file name, so changing the filename changes the
     *   key
     */
    store_winpos_prefs();
}


/*
 *   remember the file's current modification timestamp for future reference
 */
void CHtmlSys_dbgsrcwin::update_file_time()
{
    /* presume we won't get valid date information for the file */
    memset(&file_time_, 0, sizeof(file_time_));

    /* if the path is provided, remember it and get the original timestamp */
    if (path_.get() != 0)
    {
        WIN32_FIND_DATA ff;
        HANDLE hfind;

        /*
         *   store the modification time for the file so that we can check it
         *   against the file's modification time later on
         */
        hfind = FindFirstFile(path_.get(), &ff);
        if (hfind != INVALID_HANDLE_VALUE)
        {
            /* remember the modification time on the file */
            file_time_ = ff.ftLastWriteTime;

            /* we're done with the find handle */
            FindClose(hfind);
        }
    }
}

/* route command notifications to the main debug window */
int CHtmlSys_dbgsrcwin::do_command(int notify_code, int command_id, HWND ctl)
{
    /*
     *   if it's from one of our own child controls, try handling it directly
     *   ourselves first if we're not the active window
     */
    if (ctl != 0
        && IsChild(handle_, ctl)
        && dbgmain_->get_active_dbg_win() != this
        && active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* route it to the main window */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/* route command notifications to the main debug window */
TadsCmdStat_t CHtmlSys_dbgsrcwin::check_command(const check_cmd_info *info)
{
    return dbgmain_->check_command(info);
}

/*
 *   check command status
 */
TadsCmdStat_t CHtmlSys_dbgsrcwin::active_check_command(
    const check_cmd_info *info)
{
    /* note the project window - some commands rely on it */
    CHtmlDbgProjWin *projwin = dbgmain_->get_proj_win();

    /* run it by the active Scintilla control's extension */
    if (asci_->get_extension() != 0)
    {
        UINT status;

        /* run it by the extension */
        if (asci_->get_extension()->GetCommandStatus(
            info->command_id, &status, info->menu, info->item_idx))
        {
            /*
             *   The extension handled it - return its status.  The extension
             *   status codes are the same as our own, so we can just cast
             *   'status' to our status type.
             */
            return (TadsCmdStat_t)status;
        }
    }

    /* check the command */
    switch (info->command_id)
    {
    case ID_SCRIPT_REPLAY_TO_CURSOR:
        /* always enable this; we'll figure it out at execution time */
        return TADSCMD_ENABLED;

    case ID_PROJ_ADDCURFILE:
        /* show the filename in the menu item name */
        check_command_change_menu(info, IDS_TPL_PROJADD_FNAME,
                                  os_get_root_name(path_.get()));

        /*
         *   this is enabled if (a) we have a valid filename (i.e., we're not
         *   untitled), (b) there's a project window at all, and (c) we're
         *   not already in the project
         */
        return (!untitled_ && projwin != 0
                && !projwin->is_file_in_project(path_.get())
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROJ_REMOVE:
        /* show the filename in the menu item name */
        check_command_change_menu(info, IDS_TPL_PROJREM_FNAME,
                                  os_get_root_name(path_.get()));

        /* this is enabled if we're actually in the project */
        return (!untitled_ && projwin != 0
                && projwin->is_file_in_project(path_.get())
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_FIND:
        /* set the menu title to indicate the filename */
        check_command_change_menu(info, IDS_TPL_FIND_FNAME,
                                  os_get_root_name(path_.get()));

        /* this is always allowed */
        return TADSCMD_ENABLED;

    case ID_EDIT_WRAPNONE:
        return (wrap_mode_ == 'N' ? TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_EDIT_WRAPCHAR:
        return (wrap_mode_ == 'C' ? TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_EDIT_WRAPWORD:
        return (wrap_mode_ == 'W' ? TADSCMD_CHECKED : TADSCMD_ENABLED);

    case ID_EDIT_INCSEARCH:
    case ID_EDIT_INCSEARCHREV:
    case ID_EDIT_INCSEARCHREGEX:
    case ID_EDIT_INCSEARCHREGEXREV:
    case ID_EDIT_FINDNEXT:
    case ID_EDIT_GOTOLINE:
    case ID_EDIT_REPLACE:
    case ID_SEARCH_FILE:
    case ID_EDIT_SELECTALL:
    case ID_EDIT_SWAP_MARK:
    case ID_EDIT_SELECT_TO_MARK:
    case ID_EDIT_SELECTIONMODE:
    case ID_EDIT_RECTSELECTMODE:
    case ID_EDIT_LINESELECTMODE:
    case ID_EDIT_PAGELEFT:
    case ID_EDIT_OPENLINE:
    case ID_EDIT_CHARTRANSPOSE:
    case ID_EDIT_PAGERIGHT:
    case ID_EDIT_SCROLLLEFT:
    case ID_EDIT_SCROLLRIGHT:
    case ID_EDIT_VCENTERCARET:
    case ID_EDIT_WINHOME:
    case ID_EDIT_WINEND:
    case ID_DEBUG_EVALUATE:
    case ID_DEBUG_ADDTOWATCH:
    case ID_FILE_PRINT:
    case ID_EDIT_CHANGEMODIFIED:
    case ID_EDIT_CUTLINERIGHT:
    case ID_EDIT_DELETECHAR:
    case ID_EDIT_REPEATCOUNT:
    case ID_EDIT_QUOTEKEY:
    case ID_EDIT_FILL_PARA:
    case ID_EDIT_TOGGLEBOOKMARK:
    case ID_EDIT_SETNAMEDBOOKMARK:
    case ID_EDIT_CLEARFILEBOOKMARKS:
    case ID_EDIT_FINDSYMDEF:
        /* these are always allowed in a source window */
        return TADSCMD_ENABLED;

    case ID_EDIT_CHANGEREADONLY:
        return (readonly_ ? TADSCMD_CHECKED : TADSCMD_DISABLED);

    case ID_EDIT_SPLITWINDOW:
        return split_ != 0.5 ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_UNSPLITWINDOW:
    case ID_EDIT_UNSPLITSWITCHWINDOW:
        return split_ != 0.0 ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_SWITCHSPLITWINDOW:
        return split_ != 0.0 ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_PASTE:
        /* allow this if Scintilla does */
        return (asci_->can_paste()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_PASTEPOP:
        /* allow this if Scintilla does */
        return (asci_->can_paste_pop()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_COPY:
        return (asci_->can_copy()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_CUT:
    case ID_EDIT_DELETE:
        return (asci_->can_cut()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_UNDO:
        return (asci_->can_undo()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_EDIT_REDO:
        return (asci_->can_redo()
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_FILE_SAVE:
        /* set the menu title to indicate the filename */
        check_command_change_menu(info, IDS_TPL_SAVE_FNAME,
                                  os_get_root_name(path_.get()));

        /* always allow saving, whether or not modified */
        return TADSCMD_ENABLED;

    case ID_FILE_SAVE_AS:
        /* set the menu title to indicate the filename */
        check_command_change_menu(info, IDS_TPL_SAVE_FNAME_AS,
                                  os_get_root_name(path_.get()));

        /* always allow saving, whether or not modified */
        return TADSCMD_ENABLED;

    case ID_FILE_EDIT_TEXT:
        /* we can always open a source file in an external text editor */
        check_command_change_menu(info, IDS_TPL_EDITTEXT_FNAME,
                                  os_get_root_name(path_.get()));
        return TADSCMD_ENABLED;

    case ID_DEBUG_SETCLEARBREAKPOINT:
    case ID_DEBUG_DISABLEBREAKPOINT:
    case ID_DEBUG_RUNTOCURSOR:
        /* breakpoint/run commands are allowed in source windows */
        return TADSCMD_ENABLED;

    case ID_DEBUG_SETNEXTSTATEMENT:
        /* this is allowed if we're running and stopped in the debugger */
        return dbgmain_->get_in_debugger()
            ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_EDIT_SHOWLINENUMBERS:
        /* show it checked if line numbers are current enabled */
        return dbgmain_->get_global_config()->getval_int(
            "src win options", "show line numbers", FALSE)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    case ID_EDIT_SHOWFOLDING:
        /* show it checked if folding is current enabled */
        return dbgmain_->get_global_config()->getval_int(
            "src win options", "show folding", FALSE)
            ? TADSCMD_CHECKED : TADSCMD_ENABLED;

    default:
        /* if it's a Scintilla editor command mapping, allow it */
        if (info->command_id >= ID_SCI_FIRST
            && info->command_id <= ID_SCI_LAST)
            return TADSCMD_ENABLED;

        /* check to see if it's our active language mode */
        if (info->command_id == mode_cmd_id_)
            return TADSCMD_CHECKED;

        /* if it's in the language-mode range, allow it */
        if (info->command_id >= ID_EDIT_LANGMODE
            && info->command_id <= ID_EDIT_LANGLAST)
            return TADSCMD_ENABLED;

        /* for others, use the default handling */
        break;
    }

    /* inherit base class behavior for other commands */
    return CTadsWin::check_command(info);
}

int CHtmlSys_dbgsrcwin::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    CHtmlDbgProjWin *projwin = dbgmain_->get_proj_win();
    CHtmlDebugConfig *gc = dbgmain_->get_global_config();

    /* if it's from Scintilla, process it via our scintilla interface */
    if (ctl == sci_->get_handle())
    {
        sci_->handle_command_from_child(notify_code);
        return TRUE;
    }
    if (ctl == sci2_->get_handle())
    {
        sci2_->handle_command_from_child(notify_code);
        return TRUE;
    }

    /* try running it by the Scintilla window's extension first */
    if (asci_->get_extension() != 0
        && asci_->get_extension()->DoCommand(command_id))
        return TRUE;

    /* check what we have */
    switch (command_id)
    {
    case ID_PROJ_ADDCURFILE:
        /* if I'm not already in the project, add me */
        if (!untitled_ && projwin != 0
            && !projwin->is_file_in_project(path_.get()))
            projwin->auto_add_file(path_.get());
        return TRUE;

    case ID_PROJ_REMOVE:
        /* if I'm in the project, remove me */
        if (!untitled_ && projwin != 0
            && projwin->is_file_in_project(path_.get()))
            projwin->remove_proj_file(path_.get());
        return TRUE;

    case ID_EDIT_SELECTALL:
        asci_->select_all();
        return TRUE;

    case ID_EDIT_SELECTIONMODE:
        /*
         *   set 'stream' selection mode (mode 0), so that cursor movement
         *   moves just the caret and leaves anchor at its current position
         */
        asci_->selection_mode(0);

        /* set the mark to the starting position */
        asci_->set_mark_pos(asci_->get_cursor_pos());
        return TRUE;

    case ID_EDIT_RECTSELECTMODE:
        /* set rectangular selection mode (mode 1) */
        asci_->selection_mode(1);

        /* set the mark to the starting position */
        asci_->set_mark_pos(asci_->get_cursor_pos());
        return TRUE;

    case ID_EDIT_LINESELECTMODE:
        /* set line selection mode (mode 2) */
        asci_->selection_mode(2);

        /* set the mark to the starting position */
        asci_->set_mark_pos(asci_->get_cursor_pos());
        return TRUE;

    case ID_EDIT_SWAP_MARK:
        /* swap the current position and the "mark" position */
        {
            /* get the current mark position */
            long mark = asci_->get_mark_pos();

            /* set the mark to the current cursor position */
            asci_->set_mark_pos(asci_->get_cursor_pos());

            /*
             *   set just the cursor position if we're in selection range
             *   mode, so that we continue extending the selection; otherwise
             *   move the cursor and anchor to the mark
             */
            if (asci_->get_selection_mode() != -1)
                asci_->set_cursor_pos(mark);
            else
                asci_->set_sel_range(mark, mark);
        }
        return TRUE;

    case ID_EDIT_SELECT_TO_MARK:
        /* select the range from cursor to mark */
        asci_->set_anchor_pos(asci_->get_mark_pos());
        return TRUE;

    case ID_EDIT_PAGELEFT:
    case ID_EDIT_PAGERIGHT:
        {
            /* scroll by a little less than a page width */
            int pgwid = asci_->page_width();
            if (pgwid > 7)
                pgwid -= 5;
            if (command_id == ID_EDIT_PAGELEFT)
                pgwid = -pgwid;
            asci_->scroll_by(0, pgwid);
        }
        return TRUE;

    case ID_EDIT_SCROLLLEFT:
        asci_->scroll_by(0, -1);
        return TRUE;

    case ID_EDIT_SCROLLRIGHT:
        asci_->scroll_by(0, 1);
        return TRUE;

    case ID_EDIT_WINHOME:
        asci_->window_home();
        return TRUE;

    case ID_EDIT_WINEND:
        asci_->window_end();
        return TRUE;

    case ID_EDIT_VCENTERCARET:
        asci_->vcenter_caret();
        return TRUE;

    case ID_EDIT_SPLITWINDOW:
        /* split the window vertically down the middle */
        split_ = 0.5;
        split_layout();

        /*
         *   move the cursor in the other window to the position in the
         *   active window
         */
        {
            long a, b;
            asci_->get_sel_range(&a, &b);
            (asci_ == sci_ ? sci2_ : sci_)->set_sel_range(a, b);
        }
        return TRUE;

    case ID_EDIT_UNSPLITWINDOW:
        split_ = 0.0;
        split_layout();
        return TRUE;

    case ID_EDIT_UNSPLITSWITCHWINDOW:
        if (split_ != 0.0)
        {
            asci_ = (asci_ == sci_ ? sci2_ : sci_);
            SetFocus(asci_->get_handle());

            split_ = 0.0;
            split_layout();
        }
        return TRUE;

    case ID_EDIT_SWITCHSPLITWINDOW:
        if (split_ != 0.0)
        {
            asci_ = (asci_ == sci_ ? sci2_ : sci_);
            SetFocus(asci_->get_handle());
        }
        return TRUE;

    case ID_EDIT_OPENLINE:
        asci_->open_line();
        return TRUE;

    case ID_EDIT_CHARTRANSPOSE:
        asci_->transpose_chars();
        return TRUE;

    case ID_EDIT_FILL_PARA:
        asci_->fill_paragraph();
        return TRUE;

    case ID_EDIT_TOGGLEBOOKMARK:
        toggle_bookmark();
        return TRUE;

    case ID_EDIT_SETNAMEDBOOKMARK:
        add_named_bookmark();
        return TRUE;

    case ID_EDIT_JUMPNEXTBOOKMARK:
        jump_to_next_bookmark(1);
        return TRUE;

    case ID_EDIT_JUMPPREVBOOKMARK:
        jump_to_next_bookmark(-1);
        return TRUE;

    case ID_EDIT_CLEARFILEBOOKMARKS:
        delete_file_bookmarks();
        return TRUE;

    case ID_EDIT_CUT:
        asci_->cut();
        return TRUE;

    case ID_EDIT_CUTLINERIGHT:
        asci_->cut_eol();
        return TRUE;

    case ID_EDIT_DELETECHAR:
        asci_->del_char();
        return TRUE;

    case ID_EDIT_COPY:
        asci_->copy();
        return TRUE;

    case ID_EDIT_DELETE:
        asci_->clear_selection();
        return TRUE;

    case ID_EDIT_PASTE:
        asci_->paste();
        return TRUE;

    case ID_EDIT_PASTEPOP:
        asci_->paste_pop();
        return TRUE;

    case ID_EDIT_UNDO:
        asci_->undo();
        return TRUE;

    case ID_EDIT_REDO:
        asci_->redo();
        return TRUE;

    case ID_EDIT_GOTOLINE:
        do_goto_line();
        return TRUE;

    case ID_FILE_PRINT:
        do_print();
        return TRUE;

    case ID_FILE_SAVE:
        /* save the file under its current name */
        save_file();
        return TRUE;

    case ID_FILE_SAVE_AS:
        /* save the file under a new name */
        save_file_as_prompt();
        return TRUE;

    case ID_EDIT_WRAPNONE:
        wrap_mode_ = 'N';
        update_wrap_mode();
        return TRUE;

    case ID_EDIT_WRAPCHAR:
        wrap_mode_ = 'C';
        update_wrap_mode();
        return TRUE;

    case ID_EDIT_WRAPWORD:
        wrap_mode_ = 'W';
        update_wrap_mode();
        return TRUE;

    case ID_EDIT_FIND:
        /* show the Find dialog */
        dbgmain_->open_find_dlg();
        return TRUE;

    case ID_EDIT_FINDNEXT:
    case ID_SEARCH_FILE:
        /* do the Find using the last text or the search bar text */
        do_find(command_id);
        return TRUE;

    case ID_EDIT_INCSEARCH:
    case ID_EDIT_INCSEARCHREV:
    case ID_EDIT_INCSEARCHREGEX:
    case ID_EDIT_INCSEARCHREGEXREV:
        do_inc_search(command_id);
        return TRUE;

    case ID_EDIT_FINDSYMDEF:
        do_find_symdef();
        return TRUE;

    case ID_EDIT_CHANGEREADONLY:
        set_readonly(!readonly_);
        return TRUE;

    case ID_EDIT_CHANGEMODIFIED:
        /* reverse our own notion of the status */
        dirty_ = !dirty_;

        /* if it's now clean, mark the Scintilla document as clean */
        if (!dirty_)
            asci_->set_clean();

        /* update our window title */
        update_win_title();
        return TRUE;

    case ID_EDIT_REPLACE:
        /* show the Replace dialog */
        dbgmain_->open_replace_dlg();
        return TRUE;

    case ID_DEBUG_SETCLEARBREAKPOINT:
        /* set/clear a breakpoint at the current selection */
        helper_->toggle_breakpoint(dbgmain_->get_dbg_ctx(), this);
        return TRUE;

    case ID_DEBUG_DISABLEBREAKPOINT:
        /* enable/disable breakpoint at the current selection */
        helper_->toggle_bp_disable(dbgmain_->get_dbg_ctx(), this);
        return TRUE;

    case ID_DEBUG_RUNTOCURSOR:
        /* try to set a temporary breakpoint here */
        if (helper_->set_temp_bp(dbgmain_->get_dbg_ctx(), this))
        {
            /* got it - tell the main window to resume execution */
            dbgmain_->exec_go();
        }
        else
        {
            /*
             *   couldn't set a temporary breakpoint - it's not worth a whole
             *   error dialog, but beep to indicate that it failed
             */
            Beep(1000, 100);
        }
        return TRUE;

    case ID_FILE_EDIT_TEXT:
        {
            long linenum;
            long colnum;

            /* get the current caret position */
            linenum = asci_->get_cursor_linenum() + 1;
            colnum = asci_->get_cursor_column() + 1;

            /* open the file in the text editor */
            if (dbgmain_->open_in_text_editor(
                path_.get(), linenum, colnum, FALSE))
            {
                /*
                 *   we were successful, so mark the file as opened for
                 *   editing - this will suppress prompts when we find that
                 *   the file has been modified externally; we'll simply
                 *   reload the file if the user explicitly told us they
                 *   wanted to edit it
                 */
                opened_for_editing_ = TRUE;
            }
        }

        /* handled */
        return TRUE;

    case ID_DEBUG_SETNEXTSTATEMENT:
        /*
         *   make sure the game is running, and the new address is within the
         *   current function
         */
        if (!dbgmain_->get_game_loaded())
        {
            Beep(1000, 100);
            return TRUE;
        }
        else if (!helper_->is_in_same_fn(dbgmain_->get_dbg_ctx(), this))
        {
            MessageBox(dbgmain_->get_handle(),
                       "This is not a valid location - execution "
                       "cannot be transferred outside of the current "
                       "function or method.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
            return TRUE;
        }

        /* change the execution position */
        {
            int need_single_step;

            /* set the next statement here */
            if (helper_->set_next_statement(dbgmain_->get_dbg_ctx(),
                                            dbgmain_->get_exec_ofs_ptr(),
                                            this, &need_single_step))
            {
                /* something went wrong - beep an error */
                Beep(1000, 100);
            }
            else
            {
                /*
                 *   success - if the engine requires it, execute for a
                 *   single step, so that we enter into the current line
                 */
                if (need_single_step)
                    dbgmain_->exec_step_into();
            }
        }

        /* handled */
        return TRUE;

    case ID_SCRIPT_REPLAY_TO_CURSOR:
        replay_to_cursor();
        return TRUE;

    case ID_EDIT_REPEATCOUNT:
        read_repeat_count();
        return TRUE;

    case ID_EDIT_QUOTEKEY:
        read_quoted_key();
        return TRUE;

    case ID_EDIT_SHOWLINENUMBERS:
        /* invert the setting */
        gc->setval("src win options", "show line numbers",
                   !gc->getval_int("src win options", "show line numbers",
                                   FALSE));

        /* update any open editor windows */
        dbgmain_->on_update_srcwin_options();

        /* handled */
        return TRUE;

    case ID_EDIT_SHOWFOLDING:
        /* invert the setting */
        gc->setval("src win options", "show folding",
                   !gc->getval_int("src win options", "show folding", FALSE));

        /* update any open editor windows */
        dbgmain_->on_update_srcwin_options();

        /* handled */
        return TRUE;

    default:
        /*
         *   if it's a Scintilla editor command mapping, map it to the
         *   corresponding SCI_xxx command and forward it to the Scintilla
         *   window
         */
        if (asci_->handle_workbench_cmd(command_id))
            return TRUE;

        /* if it's a language mode command, set the language mode */
        if (command_id >= ID_EDIT_LANGMODE
            && command_id <= ID_EDIT_LANGLAST)
        {
            /* set the new mode */
            set_editor_mode(command_id);

            /* update styles for the new mode */
            dbgmain_->note_formatting_changes(this);

            /* handled */
            return TRUE;
        }

        /* not one of ours */
        break;
    }

    /* inherit base class behavior for other commands */
    return CTadsWin::do_command(notify_code, command_id, ctl);
}

/*
 *   Resolve all bookmarks associated with this file.  This gets the current
 *   line number for each bookmark handle.
 */
void CHtmlSys_dbgsrcwin::resolve_bookmarks(int closing)
{
    tdb_bookmark_file *f;

    /*
     *   look up our bookmark file object - if there isn't a bookmark file
     *   object for this file, then we must not have any bookmarks, so
     *   there's nothing to do in that case
     */
    if ((f = tdb_bookmark_file::find(dbgmain_, path_.get(), FALSE)) != 0)
    {
        tdb_bookmark *b;

        /* translate handles for each bookmark in this file */
        for (b = dbgmain_->bookmarks_ ; b != 0 ; b = b->nxt)
        {
            /*
             *   if this is one of our bookmarks, translate the handle if it
             *   has one
             */
            if (b->file == f && b->handle != -1)
            {
                /* translate the bookmark to a line number */
                b->line = asci_->get_bookmark_line(b->handle);

                /* the handle is no longer valid after closing the window */
                if (closing)
                    b->handle = -1;
            }
        }
    }
}

/*
 *   find a bookmark at the given line
 */
tdb_bookmark *CHtmlSys_dbgsrcwin::find_bookmark(int line)
{
    tdb_bookmark *b;
    tdb_bookmark_file *f;

    /*
     *   find our file in the bookmark file list; if we can't, there are no
     *   bookmarks for this file, so there's no way we'll find one at the
     *   given line
     */
    if ((f = tdb_bookmark_file::find(dbgmain_, path_.get(), FALSE)) == 0)
        return 0;

    /* scan the global list of bookmarks */
    for (b = dbgmain_->bookmarks_ ; b != 0 ; b = b->nxt)
    {
        /* if this bookmark matches the file and location, it's the one */
        if (b->file == f && asci_->get_bookmark_line(b->handle) == line)
            return b;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Toggle a bookmark at the current line
 */
void CHtmlSys_dbgsrcwin::toggle_bookmark()
{
    tdb_bookmark *b;
    int cnt;
    int curline = asci_->get_cursor_linenum();

    /* remove any existing bookmarks at the line */
    for (cnt = 0 ; (b = find_bookmark(curline)) != 0 ; ++cnt)
        dbgmain_->delete_bookmark(b);

    /* if there weren't any bookmarks there already, add an unnamed one */
    if (cnt == 0)
        add_bookmark(0);
}

/*
 *   prompt for a bookmark name, then add a bookmark with the name
 */
void CHtmlSys_dbgsrcwin::add_named_bookmark()
{
    int name;
    tdb_bookmark *b;
    int curline = asci_->get_cursor_linenum();

    /* ask for a name; if they cancel, we're done */
    if ((name = dbgmain_->ask_bookmark_name()) == 0)
        return;

    /* if there's already a bookmark at this line, delete it */
    while ((b = find_bookmark(curline)) != 0)
        dbgmain_->delete_bookmark(b);

    /* add a new named bookmark at the line */
    add_bookmark(name);
}

/*
 *   Add a bookmark
 */
void CHtmlSys_dbgsrcwin::add_bookmark(int name)
{
    int handle;
    int l = asci_->get_cursor_linenum();

    /* create a Scintilla marker for the new bookmark */
    handle = asci_->add_bookmark(l);

    /* add it to the master list */
    dbgmain_->add_bookmark(path_.get(), handle, l, name);
}

/*
 *   Delete all bookmarks in this file
 */
void CHtmlSys_dbgsrcwin::delete_file_bookmarks()
{
    tdb_bookmark *cur, *nxt;
    tdb_bookmark_file *f;

    /* get my bookmark file object */
    if ((f = tdb_bookmark_file::find(dbgmain_, path_.get(), FALSE)) == 0)
        return;

    /*
     *   run through the bookmark list, and delete each one associated with
     *   this file
     */
    for (cur = dbgmain_->bookmarks_ ; cur != 0 ; cur = nxt)
    {
        /* remember the next one (in case we delete this one */
        nxt = cur->nxt;

        /* if this bookmark is associated with our file, delete it */
        if (cur->file == f)
            dbgmain_->delete_bookmark(cur);
    }
}

/*
 *   Delete a bookmark.  This removes the visual marker for the bookmark from
 *   the editor window.  Note that this doesn't delete the bookmark in the
 *   tracker list - call dbgmain to do that.
 */
void CHtmlSys_dbgsrcwin::delete_bookmark(tdb_bookmark *b)
{
    /* remove the Scintilla marker */
    asci_->delete_bookmark(b->handle);
}

/*
 *   Jump to the next bookmark
 */
void CHtmlSys_dbgsrcwin::jump_to_next_bookmark(int dir)
{
    /*
     *   Try finding a line before/after the current line in the current file
     *   containing a bookmark.  If we find one, that's the next bookmark, so
     *   we can stop there.
     */
    if (asci_->jump_to_next_bookmark(dir))
        return;

    /* go to the next/previous bookmark in the next file */
    dbgmain_->jump_to_next_bookmark(path_.get(), dir);
}

/*
 *   Read a repeat count from the keyboard and apply it to the next command
 */
void CHtmlSys_dbgsrcwin::read_repeat_count()
{
    const int init_acc = 4;
    int acc = init_acc;
    int got_key = FALSE;

    /* set up the status message */
    dbgmain_->show_statusline(TRUE);
    dbgmain_->set_lrp_status("Repeat Count: 4");

    /* enter a nested event loop to read the quoted key */
    for (int done = FALSE ; !done ; )
    {
        MSG msg;
        int upd = FALSE;
        int handled = FALSE;

        switch (GetMessage(&msg, 0, 0, 0))
        {
        case -1:
            /* failed - abort */
            done = TRUE;
            break;

        case 0:
            /* quitting - repost the WM_QUIT and exit the loop */
            PostQuitMessage(msg.wParam);
            done = TRUE;
            break;

        default:
            /* check the message */
            switch (msg.message)
            {
            case WM_KEYDOWN:
                /*
                 *   if it's a number key, add it to the number under
                 *   construction; otherwise just treat it the same as any
                 *   other event
                 */
                if (msg.wParam >= '0' && msg.wParam <= '9')
                {
                    /*
                     *   if we haven't started entering keys yet, clear the
                     *   accumulator
                     */
                    if (!got_key)
                        acc = 0;

                    /* we have now received a key */
                    got_key = TRUE;

                    /* add this to the accumulator */
                    acc *= 10;
                    acc += msg.wParam - '0';

                    /* we need to update the display */
                    upd = TRUE;

                    /* we've handled the key - do not dispatch it further */
                    handled = TRUE;
                }
                else
                    goto key_common;
                break;

            case WM_SYSKEYDOWN:
            key_common:
                /*
                 *   Look for an accelerator translation.  If it translates
                 *   to a WM_COMMAND, intercept the command and handle it
                 *   ourselves so that we can apply the repeat count.
                 *   Otherwise just let the command proceed normally.
                 */
                if (CTadsApp::get_app()->get_accel_translation(&msg)
                    && msg.message == WM_COMMAND)
                    goto command_common;
                break;

            case WM_CHAR:
            case WM_COMMAND:
            command_common:
                /*
                 *   It's an ordinary character or a command.  These all
                 *   consume the repeat count - many commands simply ignore
                 *   it, but even ignoring it counts as consuming it, since a
                 *   repeat count always applies to the very next command
                 *   entered, and to nothing further.  Set the repeat count
                 *   in the editor and dispatch the message, then we're done.
                 *
                 *   There's one exception.  If the command is another
                 *   RepeatCount command, it simply multiplies the
                 *   accumulator by the initial accumulator value.
                 */
                if (msg.message == WM_COMMAND
                    && LOWORD(msg.wParam) == ID_EDIT_REPEATCOUNT)
                {
                    /* multiply the accumulator by the initial repeat */
                    acc *= init_acc;

                    /* update the display */
                    upd = TRUE;

                    /* any subsequent number entry starts over */
                    got_key = FALSE;

                    /* we've fully handled the command */
                    handled = TRUE;
                }
                else if (msg.message == WM_COMMAND
                         && LOWORD(msg.wParam) == ID_EDIT_QUOTEKEY)
                {
                    /*
                     *   repeating a quoted - set the repeat count and go
                     *   read the quoted key
                     */
                    asci_->set_repeat_count(acc);
                    read_quoted_key();

                    /* reset the repeat count, and we're done */
                    asci_->set_repeat_count(1);
                    handled = TRUE;
                    done = TRUE;
                }
                else
                {
                    /* set the repeat count in the editor */
                    asci_->set_repeat_count(acc);

                    /* dispatch the command */
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);

                    /* reset the editor's repeat count */
                    asci_->set_repeat_count(1);

                    /*
                     *   the message has been handled, and we're done with
                     *   the nested loop
                     */
                    handled = TRUE;
                    done = TRUE;
                }
                break;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
                /* on any mouse click, abort */
                done = TRUE;
                break;
            }
            break;
        }

        /* update the status message if applicable */
        if (upd)
        {
            char msg[50];

            sprintf(msg, "Repeat Count: %d", acc);
            dbgmain_->set_lrp_status(msg);
        }

        /* if we didn't handle it here, dispatch it normally */
        if (!handled)
            CTadsApp::get_app()->process_message(&msg);
    }

    /* clear our status message */
    dbgmain_->set_lrp_status(0);
}

/*
 *   Read and insert a quoted keystroke
 */
void CHtmlSys_dbgsrcwin::read_quoted_key()
{
    /* set up the status message */
    dbgmain_->show_statusline(TRUE);
    dbgmain_->set_lrp_status("Quote:");

    /* enter a nested event loop to read the quoted key */
    for (int done = FALSE ; !done ; )
    {
        MSG msg;
        int upd = FALSE;
        int handled = FALSE;
        char key[20];

        switch (GetMessage(&msg, 0, 0, 0))
        {
        case -1:
            /* failed - abort */
            done = TRUE;
            break;

        case 0:
            /* quitting - repost the WM_QUIT and exit the loop */
            PostQuitMessage(msg.wParam);
            done = TRUE;
            break;

        default:
            /* check the message */
            switch (msg.message)
            {
            case WM_KEYDOWN:
                /*
                 *   If it's any alphabetic, numeric, or punctuation key, or
                 *   one of the ordinary ASCII function keys (tab, enter,
                 *   escape, backspace), bypass the normal accelerator
                 *   translation: simply translate it into a character code
                 *   and carry on.  We can tell it's a punctuation key by
                 *   checking for a key name translation that's one character
                 *   long.
                 */
                switch (msg.wParam)
                {
                case VK_RETURN:
                case VK_ESCAPE:
                case VK_TAB:
                case VK_BACK:
                case VK_SPACE:
                quotable_key:
                    /*
                     *   it's an ordinary key - translate it, but don't
                     *   dispatch it; we'll follow up by handling the
                     *   ordinary keystroke
                     */
                    TranslateMessage(&msg);
                    handled = TRUE;
                    break;

                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                    /* process shift keys normally */
                    break;

                default:
                    /* check for letters, numbers, and punctuation */
                    if ((msg.wParam >= 'A' && msg.wParam <= 'Z')
                        || (msg.wParam >= '0' && msg.wParam <= '9')
                        || (CTadsApp::kb_->get_key_name(
                            key, sizeof(key), msg.wParam, 0)
                            && key[0] != '\0' && key[1] == '\0'))
                    {
                        /* it's one of those - handle it as a quoted key */
                        goto quotable_key;
                    }
                    else
                    {
                        /* for anything else, just cancel the quoting */
                        done = TRUE;
                    }
                    break;
                }
                break;

            case WM_CHAR:
                /* ordinary character - insert it */
                asci_->insert_char(msg.wParam);

                /*
                 *   we've now handled the event and are done reading the
                 *   quoted keystroke
                 */
                done = TRUE;
                handled = TRUE;
                break;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
            case WM_COMMAND:
                /* on any mouse click or command, abort */
                done = TRUE;
                break;
            }
            break;
        }

        /* if we didn't handle it here, dispatch it normally */
        if (!handled)
            CTadsApp::get_app()->process_message(&msg);
    }

    /* clear our status message */
    dbgmain_->set_lrp_status(0);
}

/*
 *   replay a script file to the current cursor position
 */
void CHtmlSys_dbgsrcwin::replay_to_cursor()
{
    long lcur, l, lmax;
    CStringBuf buf;

    /* get the line containing the cursor */
    lcur = asci_->get_cursor_linenum();

    /* find the next non-empty line after the cursor line */
    for (l = lcur + 1, lmax = asci_->get_line_count() ; l <= lmax ; ++l)
    {
        /* stop if we've reached a non-blank line */
        asci_->get_line_text(&buf, l);
        if (strlen(buf.get()) != 0)
            break;
    }

    /*
     *   if there are no non-empty lines after the cursor, simply re-run the
     *   entire script
     */
    if (l > lmax)
    {
        dbgmain_->run_script(path_.get(), FALSE);
        return;
    }

    /*
     *   We need to create a temporary file containing only the text of this
     *   script up to and including the cursor line, then run the temporary
     *   script.  First, create the temp file.
     */
    char *fname = _tempnam(0, "tmpscript");
    if (fname != 0)
    {
        /* open the file */
        FILE *fp = fopen(fname, "w");
        if (fp != 0)
        {
            /* copy lines to the file */
            for (l = 0 ; l <= lcur ; ++l)
            {
                /* read the next line */
                asci_->get_line_text(&buf, l);

                /* write it out */
                fputs(buf.get(), fp);
                fputs("\n", fp);
            }

            /* done with the file */
            fclose(fp);
        }

        /* run the script, noting that it's a temporary file */
        dbgmain_->run_script(fname, TRUE);

        /* done with the filename - free it */
        free(fname);
    }
}


/*
 *   print the file
 */
void CHtmlSys_dbgsrcwin::do_print()
{
    PRINTDLG pd;
    long sel1, sel2;

    /* get the current Scintilla selection */
    asci_->get_sel_range(&sel1, &sel2);

    /* run the print dialog */
    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = dbgmain_->get_handle();
    pd.hDevMode = dbgmain_->print_devmode_;
    pd.hDevNames = dbgmain_->print_devnames_;
    pd.hDC = 0;
    pd.Flags = PD_ALLPAGES | PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    if (sel1 == sel2)
        pd.Flags |= PD_NOSELECTION;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nMinPage = 1;
    pd.nMaxPage = 0xFFFF;
    pd.nCopies = 1;
    if (PrintDlg(&pd))
    {
        RECT margins;
        RECT phys;
        POINT pg;
        POINT dpi;

        /* get the dc */
        HDC dc = pd.hDC;

        /*
         *   Now the hard part: figure the margins.  We have to take into
         *   account the physically printable size of the page, plus the
         *   user's Page Setup margins, if these were ever specified.
         */

        /* get the printer resolution */
        dpi.x = GetDeviceCaps(dc, LOGPIXELSX);
        dpi.y = GetDeviceCaps(dc, LOGPIXELSY);

        /* get the physical size of the printer page, in device units */
        pg.x = GetDeviceCaps(dc, PHYSICALWIDTH);
        pg.y = GetDeviceCaps(dc, PHYSICALHEIGHT);

        /* get the dimensions of the physically printable part of the page */
        phys.left = GetDeviceCaps(dc, PHYSICALOFFSETX);
        phys.top = GetDeviceCaps(dc, PHYSICALOFFSETY);
        phys.right = pg.x - GetDeviceCaps(dc, HORZRES) - phys.left;
        phys.bottom = pg.y - GetDeviceCaps(dc, VERTRES) - phys.top;

        /* apply the user's Page Setup margins */
        if (dbgmain_->print_margins_set_)
        {
            RECT rc;
            int metric;

            /*
             *   get the page margin settings from the Page Setup, in the
             *   dialog units
             */
            rc = dbgmain_->print_margins_;

            /*
             *   Convert from the units the Page Setup dialog gave us to
             *   device units.  The dialog gave us either hundredths of
             *   millimeters or thousandths of inches.  (Could the good
             *   people at Microsoft possibly have found a less convenient
             *   way to do this?)  Check the print setup flags to see what
             *   the dialog tols us.
             */
            if (dbgmain_->print_psd_flags_ & PSD_INHUNDREDTHSOFMILLIMETERS)
                metric = TRUE;
            else if (dbgmain_->print_psd_flags_ & PSD_INTHOUSANDTHSOFINCHES)
                metric = FALSE;
            else
            {
                /* no flags from the dialog; use the locale settings */
                char li[3];
                GetLocaleInfo(
                    LOCALE_USER_DEFAULT, LOCALE_IMEASURE, li, sizeof(li));
                metric = (li[0] == '0');
            }

            if (metric)
            {
                /* metric system - 2540 hundredths of millimeters per inch */
                rc.left = MulDiv(rc.left, dpi.x, 2540);
                rc.top = MulDiv(rc.top, dpi.y, 2540);
                rc.right = MulDiv(rc.right, dpi.x, 2540);
                rc.bottom = MulDiv(rc.bottom, dpi.y, 2540);
            }
            else
            {
                /* English system - 1000 thousandths of inches per inch */
                rc.left = MulDiv(rc.left, dpi.x, 1000);
                rc.top = MulDiv(rc.top, dpi.y, 1000);
                rc.right = MulDiv(rc.right, dpi.x, 1000);
                rc.bottom = MulDiv(rc.bottom, dpi.y, 1000);
            }

            /* use the higher of the given margins or the physical margins */
            margins.left = max(rc.left, phys.left);
            margins.right = max(rc.right, phys.right);
            margins.top = max(rc.top, phys.top);
            margins.bottom = max(rc.bottom, phys.bottom);
        }
        else
        {
            /* no Page Setup margins - just use the physical margins */
            margins = phys;
        }

        /* convert everything from device coordinates to logical coordinates */
        DPtoLP(dc, (LPPOINT)&margins, 2);
        DPtoLP(dc, (LPPOINT)&phys, 2);
        DPtoLP(dc, &pg, 1);

        /* start the print job */
        DOCINFO di;
        di.cbSize = sizeof(di);
        di.lpszDocName = path_.get();
        di.lpszOutput = 0;
        di.lpszDatatype = 0;
        di.fwType = 0;
        if (StartDoc(dc, &di) > 0)
        {
            /* tell Scintilla to print the document */
            asci_->print_doc(dc, &pg, &margins, &phys,
                             pd.Flags & PD_SELECTION,
                             pd.Flags & PD_PAGENUMS ? pd.nFromPage : 1,
                             pd.Flags & PD_PAGENUMS ? pd.nToPage : 0xFFFFU);

            /* end the document */
            EndDoc(dc);
        }
        else
        {
            /* couldn't start the print job */
            MessageBox(dbgmain_->get_handle(),
                       "An error occurred starting the print job.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
        }

        /* delete the print DC */
        DeleteDC(dc);

        /* remember the new page setup settings */
        dbgmain_->print_devmode_ = pd.hDevMode;
        dbgmain_->print_devnames_ = pd.hDevNames;

        /* if we haven't set the margins, set default 1" margins */
        if (!dbgmain_->print_margins_set_)
            SetRect(&dbgmain_->print_margins_, 1000, 1000, 1000, 1000);
    }
}

/*
 *   carry out an incremental search
 */
void CHtmlSys_dbgsrcwin::do_inc_search(int command_id)
{
    CStringBuf query;
    CStringBuf statmsg;
    HWND focus;
    int dir;
    int regex;
    int newstat = TRUE;
    int found = TRUE;
    int exact = FALSE;
    int whole_word = FALSE;
    SciIncSearchCtx ctx;
    int cancel = FALSE;
    int started = FALSE;
    HtmlKeyTable *istab = dbgmain_->get_key_table("Incremental Search");
    CTadsAccelerator *accel = (istab != 0 ? istab->accel : 0);
    app_accel_info old_accel;

    /*
     *   note the selection mode, and cancel it - Scintilla will internally
     *   cancel selection mode in its search operation, so to keep things in
     *   sync we need to cancel our own mode
     */
    int old_anchor = asci_->get_anchor_pos();
    int old_sel_mode = asci_->get_selection_mode();
    asci_->cancel_sel_mode(FALSE);

    /* set the mark to the cursor position before the search */
    asci_->set_mark_pos(asci_->get_cursor_pos());

    /* if there's an Inc Search accelerator, install it temporarily */
    if (accel != 0)
        CTadsApp::get_app()->push_accel(&old_accel, accel, dbgmain_);

    /* note the modes */
    dir = (command_id == ID_EDIT_INCSEARCH
           || command_id == ID_EDIT_INCSEARCHREGEX ? 1 : -1);
    regex = (command_id == ID_EDIT_INCSEARCHREGEX
             || command_id == ID_EDIT_INCSEARCHREGEXREV);

    /* show the status line, since that's where we do our text entry */
    dbgmain_->show_statusline(TRUE);

    /* start with an empty query string */
    query.set("");

    /* begin the i-search in scintilla */
    asci_->isearch_begin(&ctx);

    /* enter a nested event loop for processing the search */
    for (int done = FALSE ; !done ; )
    {
        MSG msg, origmsg;
        int handled = FALSE;
        textchar_t ch;

        /* update the status if needed */
        if (newstat)
        {
            /* build the new status message */
            statmsg.set("");
            if (ctx.wrapcnt == 1)
                statmsg.append("Wrapped ");
            if (ctx.wrapcnt > 1)
                statmsg.append("Overwrapped ");
            if (!ctx.found)
                statmsg.append("Failing ");
            if (exact)
                statmsg.append("Exact ");
            if (dir < 0)
                statmsg.append("Reverse ");
            if (regex)
                statmsg.append("RegEx ");
            if (whole_word)
                statmsg.append("Word ");
            statmsg.append("I-Search: ");
            statmsg.append(query.get());

            /* set it */
            dbgmain_->set_lrp_status(statmsg.get());

            /* we're up to date now */
            newstat = FALSE;
        }

        /* get the next message */
        switch (GetMessage(&msg, 0, 0, 0))
        {
        case -1:
            /* failed - abort */
            done = TRUE;
            break;

        case 0:
            /* quitting - repost the WM_QUIT and exit the loop */
            PostQuitMessage(msg.wParam);
            done = TRUE;
            break;

        default:
            /* check the message */
            switch (msg.message)
            {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                /* check for an accelerator translation */
                origmsg = msg;
                if (CTadsApp::get_app()->get_accel_translation(&msg))
                {
                    /*
                     *   There's a translation.  First, if it's a WM_NULL, it
                     *   means that this is the first key of a multi-key
                     *   chord, so simply translate it, thereby setting the
                     *   key state, and move on.
                     */
                    if (msg.message == WM_NULL)
                    {
                        /* translate it */
                        CTadsApp::get_app()->accel_translate(&origmsg);

                        /* done with this message */
                        break;
                    }

                    /* if it's not a WM_COMMAND, cancel the search */
                    if (msg.message != WM_COMMAND)
                    {
                        /* the search is done */
                        done = TRUE;
                        break;
                    }

                    /* check for an I-Search command */
                    switch (LOWORD(msg.wParam))
                    {
                    case ID_ISEARCH_FINDNEXT:
                    case ID_EDIT_INCSEARCH:
                        dir = 1;
                        goto continue_isearch;

                    case ID_EDIT_INCSEARCHREGEX:
                        dir = 1;
                        regex = TRUE;
                        goto continue_isearch;

                    case ID_ISEARCH_FINDPREV:
                    case ID_EDIT_INCSEARCHREV:
                        dir = -1;
                        goto continue_isearch;

                    case ID_EDIT_INCSEARCHREGEXREV:
                        dir = -1;
                        regex = TRUE;
                        goto continue_isearch;

                    continue_isearch:
                        /*
                         *   continue forward/backward; or, if the search
                         *   hasn't started yet, copy the previous search
                         */

                        /* check to see if we've started the search yet */
                        if (!started
                            && last_isearch_query_.get() != 0
                            && last_isearch_query_.get()[0] != '\0')
                        {
                            query.set(&last_isearch_query_);
                            regex = last_isearch_regex_;
                            exact = last_isearch_exact_;
                            whole_word = last_isearch_word_;

                            /* run the search */
                            asci_->isearch_update(
                                &ctx, query.get(), dir, regex, exact,
                                whole_word);

                            /* we've now started the search */
                            started = TRUE;
                        }
                        else if (query.get()[0] != '\0')
                        {
                            /* continue the search */
                            asci_->isearch_next(&ctx, query.get(), dir);
                        }

                        /* update the status */
                        newstat = TRUE;
                        break;

                    case ID_ISEARCH_EXIT:
                        /* end the search, staying where we are */
                        done = TRUE;
                        break;

                    case ID_ISEARCH_CANCEL:
                        /*
                         *   cancel the search and return to the starting
                         *   position
                         */
                        asci_->isearch_cancel(&ctx);
                        done = TRUE;
                        cancel = TRUE;
                        break;

                    case ID_ISEARCH_BACKSPACE:
                        /*
                         *   backspace - if the buffer's not already, empty,
                         *   remove the last character
                         */
                        if (query.get()[0] != '\0')
                        {
                            /* back out one character */
                            query.get()[get_strlen(query.get()) - 1] = '\0';

                            /* update the search and status message */
                            asci_->isearch_update(
                                &ctx, query.get(), dir, regex, exact,
                                whole_word);
                            newstat = TRUE;

                            /* the search is now started */
                            started = TRUE;
                        }
                        break;

                    case ID_ISEARCH_CASE:
                        /* exact case on/off */
                        exact = !exact;

                        /* update the search and status message */
                        asci_->isearch_update(
                            &ctx, query.get(), dir, regex, exact,
                            whole_word);
                        newstat = TRUE;
                        break;

                    case ID_ISEARCH_WORD:
                        /* whole word mode on/off */
                        whole_word = !whole_word;

                        /* update the search and status message */
                        asci_->isearch_update(
                            &ctx, query.get(), dir, regex, exact,
                            whole_word);
                        newstat = TRUE;
                        break;

                    case ID_ISEARCH_REGEX:
                        /* toggle regex mode */
                        regex = !regex;

                        /* update the search and status */
                        asci_->isearch_update(
                            &ctx, query.get(), dir, regex, exact,
                            whole_word);
                        newstat = TRUE;
                        break;

                    default:
                        /*
                         *   For any other command, terminate the search and
                         *   process the command normally.
                         */
                        done = TRUE;
                        CTadsApp::get_app()->accel_translate(&origmsg);

                        /* we've now fully handled this message */
                        handled = TRUE;
                        break;
                    }
                }
                else
                {
                    /*
                     *   There's no accelerator translation.  Translate the
                     *   message into a WM_CHAR but don't dispatch it - we
                     *   want to handle the WM_CHAR ourselves, but we don't
                     *   want the editor window to hear about it.
                     */
                    TranslateMessage(&msg);
                    handled = TRUE;
                    break;
                }
                break;

            case WM_CHAR:
                /* assume we'll handle the character fully */
                handled = TRUE;

                /* process the keystroke as a search command */
                ch = (textchar_t)msg.wParam;
                if (ch >= 32)
                {
                    char buf[2];

                    /* regular character - add it to the query */
                    buf[0] = (char)ch;
                    buf[1] = '\0';
                    query.append(buf);

                    /* update the search */
                    asci_->isearch_update(
                        &ctx, query.get(), dir, regex, exact, whole_word);

                    /* need a status message update */
                    newstat = TRUE;

                    /* the search is now started */
                    started = TRUE;
                }

                /* done */
                break;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_NCLBUTTONDOWN:
            case WM_NCRBUTTONDOWN:
            case WM_NCMBUTTONDOWN:
            case WM_SYSCOMMAND:
            case WM_SETFOCUS:
            case WM_KILLFOCUS:
            case WM_ACTIVATE:
            case WM_NCACTIVATE:
                /*
                 *   terminate the search on any mouse click, system cmd, or
                 *   change in focus or window activation
                 */
                done = TRUE;
                break;
            }

            /* if we didn't handle it here, dispatch it normally */
            if (!handled)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            /* if we no longer have focus, abort */
            focus = GetFocus();
            if (focus != handle_ && !IsChild(handle_, focus))
                break;
        }
    }

    /* clear our status message */
    dbgmain_->set_lrp_status(0);

    /* restore accelerators */
    if (accel != 0)
        CTadsApp::get_app()->pop_accel(&old_accel);

    /*
     *   if we didn't cancel the search, and the query is non-empty, remember
     *   this for next time
     */
    if (!cancel && query.get()[0] != '\0')
    {
        last_isearch_query_.set(&query);
        last_isearch_regex_ = regex;
        last_isearch_exact_ = exact;
        last_isearch_word_ = whole_word;
    }

    /*
     *   if we didn't cancel, and we were in a selection mode, restore the
     *   original anchor position
     */
    if (!cancel && old_sel_mode != -1)
    {
        asci_->set_sel_range(old_anchor, asci_->get_cursor_pos());
        asci_->selection_mode(old_sel_mode);
    }
}

/*
 *   carry out a Find command
 */
void CHtmlSys_dbgsrcwin::do_find(int command_id)
{
    const textchar_t *txt;
    int exact, top, wrap, dir, regex, whole_word;
    itfdo_scope scope;
    IDebugSysWin *new_win;

    /* mark the starting position */
    asci_->set_mark_pos(asci_->get_cursor_pos());

    /* prompt for the text to find */
    txt = dbgmain_->get_find_text_ex(
        command_id, &exact, &top, &wrap, &dir, &regex, &whole_word, &scope);

    /* if they canceled, there's nothing to do */
    if (txt == 0)
        return;

    /* run the search */
    if (dbgmain_->find_and_replace(
        txt, 0, FALSE, exact, wrap, dir, regex, whole_word, scope, &new_win))
#if 0
    /* do the search */
    if (asci_->search(txt, exact, top, wrap, dir, regex, whole_word, 0, 0))
#endif
    {
        /* success - if focus is in the search box, move it here */
        HWND f = GetFocus();
        HWND s = dbgmain_->get_searchbox();
        if (f == s || IsChild(s, f) && new_win == this)
            SetFocus(handle_);
    }
    else
    {
        char buf[256];

        /* display a message box telling the user that we failed */
        LoadString(CTadsApp::get_app()->get_instance(), IDS_FIND_NO_MORE,
                   buf, sizeof(buf));
        MessageBox(dbgmain_->get_handle(), buf, "TADS Workbench",
                   MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
    }
}

/*
 *   find the definition of the symbol at the cursor
 */
void CHtmlSys_dbgsrcwin::do_find_symdef()
{
    long pos, a, b;
    IDebugSysWin *new_win = 0;
    const int MAX_SYM = 64;
    const int MAX_EXPSYM = MAX_SYM + 20;
    char sym[MAX_EXPSYM];
    char pat[MAX_EXPSYM + 256];
    char *p;
    size_t len;
    static const char *pat_tpl =
        "^("

        "("
        "((replace|modify)<space>+)?((function|class|grammar)<space>+)?"
        "|<plus><space|plus>*"
        ")"
        "%s"
        "<space>*"
        "(:<space>*<alpha|_>|<lparen>|$)"

        "|"

        "Define<alpha>*Action<space>*<lparen><space>*%s<space>*"
        "<rparen|,>"

        ")";

    /* get the word at the cursor */
    pos = asci_->get_cursor_pos();
    asci_->get_word_boundaries(&a, &b, pos);

    /* limit the length to our buffer size */
    if (b - a > MAX_SYM)
        b = a + MAX_SYM;

    /* pull out the word */
    asci_->get_text(sym, a, b);

    /* if it's not a word, ignore it */
    if (!isalpha(sym[0]) && !isdigit(sym[0]) && sym[0] != '_')
        return;

    /* end the symbol at the first non-symbol character */
    for (p = sym ; isalpha(*p) || isdigit(*p) || *p == '_' ; ++p) ;
    *p = '\0';

    /*
     *   if it looks like "xxxAction", look for either the base symbol alone
     *   or the full symbol - do this by converting "xxxAction" into
     *   "(xxx(Action)?)" in the regular expression pattern
     */
    if ((len = strlen(sym)) > 6 && strcmp(sym + len - 6, "Action") == 0)
    {
        sprintf(pat, "(%.*s(Action)?)", (int)(len - 6), sym);
        strcpy(sym, pat);
    }

    /* build the search pattern */
    sprintf(pat, pat_tpl, sym, sym);

    /* set the busy cursor while working */
    HCURSOR oldcsr = SetCursor(wait_cursor_);

    /* run a search for the string */
    if (dbgmain_->find_and_replace(
        pat, 0, FALSE, TRUE, TRUE, 1, TRUE, FALSE, ITFDO_SCOPE_GLOBAL,
        &new_win))
    {
        ScintillaWin *sci;

        /* if we switched to a new window, get its scintilla object */
        if (new_win != 0 && new_win->idsw_as_srcwin() != 0)
            sci = new_win->idsw_as_srcwin()->get_scintilla();
        else
            sci = asci_;

        /*
         *   start over at the beginning of the current line and search for
         *   just the symbol - this will ensure that if they press the Find
         *   Symbol Definition command key again, they'll repeat the search
         *   for the same symbol
         */
        sci->go_to_line(sci->get_cursor_linenum());
        sci->search(sym, TRUE, FALSE, FALSE, 1, TRUE, TRUE, 0, 0);
    }
    else
    {
        /* we didn't find it */
        MessageBox(dbgmain_->get_handle(),
                   "No definition was found.",
                   "TADS Workbench",
                   MB_OK | MB_ICONINFORMATION);
    }

    /* restore the normal cursor */
    SetCursor(oldcsr);
}

/*
 *   get the selection range
 */
int CHtmlSys_dbgsrcwin::get_sel_range(class CHtmlFormatter **formatter,
                                      unsigned long *sel_start,
                                      unsigned long *sel_end)
{
    long a, b;

    /* get the selection range from Scintilla */
    asci_->get_sel_range(&a, &b);

    /* set the results */
    *formatter = 0;
    *sel_start = a;
    *sel_end = b;

    /* got a range */
    return TRUE;
}

/*
 *   get the selection range as spots
 */
int CHtmlSys_dbgsrcwin::get_sel_range_spots(void **spota, void **spotb)
{
    long a, b;

    /* get the selection range from Scintilla */
    asci_->get_sel_range(&a, &b);

    /* create spots for the endpoints */
    *spota = asci_->create_spot(a);
    *spotb = asci_->create_spot(b);

    /* got a range */
    return TRUE;
}

/*
 *   get the current document position from a spot handle
 */
unsigned long CHtmlSys_dbgsrcwin::get_spot_pos(void *spot)
{
    /* ask Scintilla to dereference the spot */
    return asci_->get_spot_pos(spot);
}

/*
 *   delete a spot
 */
void CHtmlSys_dbgsrcwin::delete_spot(void *spot)
{
    /* ask Scintilla to delete the spot */
    asci_->delete_spot(spot);
}

/*
 *   get the selected text
 */
int CHtmlSys_dbgsrcwin::get_sel_text(CStringBuf *buf, unsigned int flags,
                                     size_t maxlen)
{
    long start, end;

    /* get the selection range */
    asci_->get_sel_range(&start, &end);

    /* if it's empty, apply the requested range extension */
    if (start == end)
    {
        if ((flags & DSW_EXPR_EXTEND) != 0)
            asci_->get_expr_boundaries(&start, &end, start, TRUE);
        else if ((flags & DSW_WORD_EXTEND) != 0)
            asci_->get_word_boundaries(&start, &end, start);
    }

    /* if the range is empty, return an empty buffer */
    if (start == end)
    {
        buf->clear();
        return TRUE;
    }

    /* if the selection is too long, return failure */
    if (end - start > (long)maxlen)
        return FALSE;

    /* make sure the buffer is big enough */
    buf->ensure_size(end - start);

    /* extract the text */
    asci_->get_text(buf->get(), start, end);

    /* success */
    return TRUE;
}

/*
 *   set the selection range
 */
int CHtmlSys_dbgsrcwin::set_sel_range(unsigned long sel_start,
                                      unsigned long sel_end)
{
    /* set it in Scintilla */
    asci_->set_sel_range(sel_start, sel_end);

    /* success */
    return TRUE;
}


/*
 *   run the "go to line" dialog
 */
void CHtmlSys_dbgsrcwin::do_goto_line()
{
    CTadsDlgGoToLine *dlg;
    long linenum;

    /* get my current line */
    linenum = asci_->get_cursor_linenum() + 1;

    /* create the dialog */
    dlg = new CTadsDlgGoToLine(linenum);

    /* display it */
    dlg->run_modal(DLG_GOTOLINE, handle_,
                   CTadsApp::get_app()->get_instance());

    /* if we got a valid line number, go there */
    if ((linenum = dlg->get_linenum()) != 0)
        asci_->go_to_line(linenum - 1);

    /* done with it */
    delete dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   Base class for debugger HTML panel windows
 */

CHtmlSysWin_win32_tdb::CHtmlSysWin_win32_tdb(
    class CHtmlFormatter *formatter,
    class CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll,
    class CHtmlPreferences *prefs,
    class CHtmlDebugHelper *helper,
    class CHtmlSys_dbgmain *dbgmain)
    : CDebugSysWin(formatter, owner, has_vscroll, has_hscroll, prefs)
{
    /* remember the debugger helper object, and add our reference to it */
    helper_ = helper;
    helper_->add_ref();

    /* remember the debugger main window, and add our reference */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();
}

CHtmlSysWin_win32_tdb::~CHtmlSysWin_win32_tdb()
{

    /* release our helper object reference */
    helper_->remove_ref();

    /* release the debugger main window */
    dbgmain_->Release();
}

/*
 *   create the window
 */
void CHtmlSysWin_win32_tdb::do_create()
{
    /* inherit default handling */
    CHtmlSysWin_win32::do_create();

    /*
     *   set focus on myself (this seems to be necessary on NT for some
     *   reason - after creating an MDI child window, it doesn't receive
     *   focus by default, as it does on 95/98; this is not necessary on
     *   95/98 but seems harmless enough)
     */
    SetFocus(handle_);
}

/*
 *   handle a command
 */
int CHtmlSysWin_win32_tdb::do_command(int notify_code,
                                      int command_id, HWND ctl)
{
    /* route it to the main window for processing */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Check a command - route to the main window for processing
 */
TadsCmdStat_t CHtmlSysWin_win32_tdb::check_command(const check_cmd_info *info)
{
    /* route it to the main window for processing */
    return dbgmain_->check_command(info);
}

/*
 *   Determine command status, routed from the main window
 */
TadsCmdStat_t CHtmlSysWin_win32_tdb::active_check_command(
    const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_EDIT_FIND:
        /* set the blank Find... menu name and enable it */
        check_command_change_menu(info, IDS_TPL_FIND_BLANK);
        return TADSCMD_ENABLED;

    case ID_EDIT_FINDNEXT:
        /* by default, this is allowed in debugger windows */
        return TADSCMD_ENABLED;

    default:
        break;
    }

    /* route others to the regular system window handler */
    return CHtmlSysWin_win32::check_command(info);
}


/*
 *   Handle a command routed from the main window
 */
int CHtmlSysWin_win32_tdb::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    switch (command_id)
    {
    case ID_EDIT_FIND:
        /* show the Find dialog */
        dbgmain_->open_find_dlg();
        return TRUE;

    case ID_EDIT_FINDNEXT:
    case ID_SEARCH_FILE:
        /* do the find using the last text or Search bar text */
        do_find(command_id);
        return TRUE;

    default:
        break;
    }

    /* route others to the regular system window handler */
    return CHtmlSysWin_win32::do_command(notify_code, command_id, ctl);
}

/*
 *   show the text-not-found message for a 'find' command
 */
void CHtmlSysWin_win32_tdb::find_not_found()
{
    /* display a message box telling the user that we failed */
    MessageBox(dbgmain_->get_handle(),
               "No more matches found.", "TADS Workbench",
               MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
}

/*
 *   Execute a find command
 */
void CHtmlSysWin_win32_tdb::do_find(int command_id)
{
    /* if focus is in the search toolbar combo box, move focus to me */
    HWND f = GetFocus();
    HWND s = dbgmain_->get_searchbox();
    if (f == s || IsChild(s, f))
        SetFocus(handle_);

    /* inherit the base handling */
    CDebugSysWin::do_find(command_id);
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger source HTML panel window implementation
 */

CHtmlSysWin_win32_dbginfo::CHtmlSysWin_win32_dbginfo(
    CHtmlFormatter *formatter, CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll, CHtmlPreferences *prefs,
    CHtmlDebugHelper *helper, CHtmlSys_dbgmain *dbgmain)
    : CHtmlSysWin_win32_tdb(formatter, owner, has_vscroll, has_hscroll,
                            prefs, helper, dbgmain)
{
    /* load the normal size (16x16) debugger icons */
    ico_dbg_curline_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                        MAKEINTRESOURCE(IDI_DBG_CURLINE),
                                        IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxline_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                        MAKEINTRESOURCE(IDI_DBG_CTXLINE),
                                        IMAGE_ICON, 16, 16, 0);
    ico_dbg_bp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                   MAKEINTRESOURCE(IDI_DBG_BP),
                                   IMAGE_ICON, 16, 16, 0);
    ico_dbg_bpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_BPDIS),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_curbp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_CURBP),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_curbpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                         MAKEINTRESOURCE(IDI_DBG_CURBPDIS),
                                         IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxbp_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                      MAKEINTRESOURCE(IDI_DBG_CTXBP),
                                      IMAGE_ICON, 16, 16, 0);
    ico_dbg_ctxbpdis_ = (HICON)LoadImage(CTadsApp::get_app()->get_instance(),
                                         MAKEINTRESOURCE(IDI_DBG_CTXBPDIS),
                                         IMAGE_ICON, 16, 16, 0);

    /* load the small (8x8) debugger icons */
    smico_dbg_curline_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURLINE), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxline_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXLINE), IMAGE_ICON, 8, 8, 0);
    smico_dbg_bp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_BP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_bpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_BPDIS), IMAGE_ICON, 8, 8, 0);
    smico_dbg_curbp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURBP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_curbpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CURBPDIS), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxbp_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXBP), IMAGE_ICON, 8, 8, 0);
    smico_dbg_ctxbpdis_ = (HICON)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDI_DBG_CTXBPDIS), IMAGE_ICON, 8, 8, 0);

    /* enable the caret */
    caret_enabled_ = TRUE;

    /* get rid of the default popup menu and load a new one instead */
    load_context_popup(IDR_DEBUGINFOWIN_POPUP_MENU);

    /* no target column for up/down arrow yet */
    target_col_valid_ = FALSE;

    /* the icon size hasn't been measured yet */
    icon_size_ = W32TDB_ICON_UNKNOWN;
}

CHtmlSysWin_win32_dbginfo::~CHtmlSysWin_win32_dbginfo()
{
    /* done with the debugger source window icons */
    DeleteObject(ico_dbg_curline_);
    DeleteObject(ico_dbg_ctxline_);
    DeleteObject(ico_dbg_bp_);
    DeleteObject(ico_dbg_bpdis_);
    DeleteObject(ico_dbg_curbp_);
    DeleteObject(ico_dbg_curbpdis_);
    DeleteObject(ico_dbg_ctxbp_);
    DeleteObject(ico_dbg_ctxbpdis_);
    DeleteObject(smico_dbg_curline_);
    DeleteObject(smico_dbg_ctxline_);
    DeleteObject(smico_dbg_bp_);
    DeleteObject(smico_dbg_bpdis_);
    DeleteObject(smico_dbg_curbp_);
    DeleteObject(smico_dbg_curbpdis_);
    DeleteObject(smico_dbg_ctxbp_);
    DeleteObject(smico_dbg_ctxbpdis_);
}

/*
 *   process a click with the left button
 */
int CHtmlSysWin_win32_dbginfo::do_leftbtn_down(int keys, int x, int y,
                                               int clicks)
{
    /* process a click normally */
    return common_btn_down(keys, x, y, clicks);
}

/*
 *   process a click with the right button
 */
int CHtmlSysWin_win32_dbginfo::do_rightbtn_down(int keys, int x, int y,
                                                int clicks)
{
    /* process a basic click */
    return common_btn_down(keys, x, y, clicks);
}

/*
 *   Process a left or right mouse click
 */
int CHtmlSysWin_win32_dbginfo::common_btn_down(int keys, int x, int y,
                                               int clicks)
{
    /*
     *   it's a click in the source area - forget any target column we
     *   had, since this click sets an explicit active column
     */
    target_col_valid_ = FALSE;

    /* inherit default left-button code */
    return CHtmlSysWin_win32_tdb::do_leftbtn_down(keys, x, y, clicks);
}

/*
 *   Handle keystrokes
 */
int CHtmlSysWin_win32_dbginfo::do_keydown(int vkey, long keydata)
{
    /* check the key */
    switch(vkey)
    {
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_END:
    case VK_HOME:
    case VK_NEXT:
    case VK_PRIOR:
        /* these keystrokes all move the caret */
        {
            unsigned long sel_start, sel_end;
            unsigned long caret_end, other_end;

            /* get current selection range */
            formatter_->get_sel_range(&sel_start, &sel_end);

            /*
             *   if the shift key is down, note the caret end of the range
             *   and the other end of the range; otherwise, treat them as
             *   equivalent
             */
            if (get_shift_key())
            {
                /* shift key is down - note both ends of the range */
                if (caret_at_right_)
                {
                    caret_end = sel_end;
                    other_end = sel_start;
                }
                else
                {
                    caret_end = sel_start;
                    other_end = sel_end;
                }
            }
            else
            {
                /* no shift key - we only care about the caret position */
                caret_end = other_end =
                    (caret_at_right_ ? sel_end : sel_start);
            }

            switch(vkey)
            {
            case VK_NEXT:
            case VK_PRIOR:
                {
                    RECT rc;
                    int lines;

                    /*
                     *   if the control key is down, scroll without moving
                     *   the cursor - simply treat this the same way the
                     *   base class does
                     */
                    if (get_ctl_key())
                        return CHtmlSysWin_win32_tdb::do_keydown(vkey, keydata);

                    /* get the window's height */
                    GetClientRect(handle_, &rc);

                    /*
                     *   figure out how many lines that will hold
                     *   (assuming that all of the lines are of constant
                     *   height, which is the case for our type of window)
                     */
                    if (formatter_->get_first_disp() != 0)
                    {
                        CHtmlRect lrc;

                        /* get the height of a line */
                        lrc = formatter_->get_first_disp()->get_pos();

                        /* figure out how many fit in the window */
                        lines = (rc.bottom - rc.top) / (lrc.bottom - lrc.top);

                        /*
                         *   for context, move one line less than the
                         *   window will hold
                         */
                        if (lines > 1)
                            --lines;
                    }
                    else
                    {
                        /*
                         *   no lines - use a default window size (it
                         *   doesn't even matter, since if there are no
                         *   lines we can't do any real navigation)
                         */
                        lines = 25;
                    }

                    /* move the caret up or down by one screenfull */
                    caret_end =
                        do_keydown_arrow(vkey == VK_PRIOR ? VK_UP : VK_DOWN,
                                         caret_end, lines);
                }
                break;

            case VK_UP:
            case VK_DOWN:
                caret_end = do_keydown_arrow(vkey, caret_end, 1);
                break;

            case VK_LEFT:
                /* move the caret left a character */
                caret_end = formatter_->dec_text_ofs(caret_end);
                break;

            case VK_RIGHT:
                /* move the caret right a character */
                caret_end = formatter_->inc_text_ofs(caret_end);
                break;

            case VK_END:
                if (get_ctl_key())
                {
                    /* move to the end of the file */
                    caret_end = formatter_->get_text_ofs_max();
                }
                else
                {
                    unsigned long lstart, lend;

                    /* move to the end of the current line */
                    formatter_->get_line_limits(&lstart, &lend, caret_end);
                    caret_end = formatter_->dec_text_ofs(lend);
                }
                break;

            case VK_HOME:
                if (get_ctl_key())
                {
                    /* move to the start of the file */
                    caret_end = 0;
                }
                else
                {
                    unsigned long lstart, lend;

                    /* move to the start of the current line */
                    formatter_->get_line_limits(&lstart, &lend, caret_end);
                    caret_end = lstart;
                }
                break;
            }

            /*
             *   if the keystroke wasn't an up/down arrow, forget any
             *   target column we had
             */
            if (vkey != VK_UP && vkey != VK_DOWN
                && vkey != VK_NEXT && vkey != VK_PRIOR)
                target_col_valid_ = FALSE;

            /*
             *   If we reached the very end, because of the way the final
             *   newlines are laid out, we may find ourselves past the end
             *   of the last item.  If this is the case, decrement our
             *   position so that we're at the start of the last item.
             */
            if (caret_end == formatter_->get_text_ofs_max()
                && formatter_->find_by_txtofs(caret_end, FALSE, FALSE) == 0)
            {
                CHtmlDisp *disp;

                /* get the previous item */
                disp = formatter_->find_by_txtofs(caret_end, TRUE, FALSE);

                /* move to the start of that item */
                if (disp != 0)
                    caret_end = disp->get_text_ofs();
            }

            /* figure out the new selection range */
            if (get_shift_key())
            {
                /* extended range - figure out what direction to go */
                if (caret_end < other_end)
                {
                    /* the caret is at the low (left) end of the range */
                    sel_start = caret_end;
                    sel_end = other_end;
                    caret_at_right_ = FALSE;
                }
                else
                {
                    /* the caret is at the high (right) end */
                    sel_start = other_end;
                    sel_end = caret_end;
                    caret_at_right_ = TRUE;
                }
            }
            else
            {
                /* no shift key - set zero-length range at caret */
                sel_start = sel_end = caret_end;
            }

            /* set the new selection range */
            formatter_->set_sel_range(sel_start, sel_end);

            /* make sure the selection is on the screen */
            update_caret_pos(TRUE, FALSE);
        }

        /* handled */
        return TRUE;

    default:
        /* inherit default for other keys */
        return CHtmlSysWin_win32_tdb::do_keydown(vkey, keydata);
    }
}

/*
 *   process an arrow key; returns new text offset after movement
 */
unsigned long CHtmlSysWin_win32_dbginfo::
   do_keydown_arrow(int vkey, unsigned long caret_pos, int cnt)
{
    /* keep going for the requested number of lines */
    for ( ; cnt > 0 ; --cnt)
    {
        unsigned long lstart, lend;
        unsigned long ofs;
        CHtmlDisp *nxt;

        /*
         *   find our current column by examining the limits of the
         *   current line
         */
        formatter_->get_line_limits(&lstart, &lend, caret_pos);

        /*
         *   if we don't have an existing valid target column, use the
         *   current column as the new target
         */
        if (!target_col_valid_)
        {
            target_col_ = caret_pos - lstart;
            target_col_valid_ = TRUE;
        }

        /* get the next/previous line's display item */
        if (vkey == VK_UP)
        {
            ofs = formatter_->dec_text_ofs(lstart);
            nxt = formatter_->find_by_txtofs(ofs, TRUE, FALSE);
        }
        else
        {
            ofs = lend;
            nxt = formatter_->find_by_txtofs(ofs, FALSE, TRUE);
        }

        /* make sure we got a target item */
        if (nxt != 0)
        {
            CHtmlDispTextDebugsrc *disp;
            unsigned int cur_target;

            /* cast it to my correct type */
            disp = (CHtmlDispTextDebugsrc *)nxt;

            /* see if it contains the target column */
            if (disp->get_text_len() > target_col_)
            {
                /* it's long enough - go to the taret */
                cur_target = target_col_;
            }
            else
            {
                /* it's too short - go to its end */
                cur_target = disp->get_text_len();
            }

            /* move to the correct spot */
            caret_pos = disp->get_text_ofs() + cur_target;
        }
    }

    /* return the new caret position */
    return caret_pos;
}

/*
 *   set the colors
 */
void CHtmlSysWin_win32_dbginfo::note_debug_format_changes(
    HTML_color_t bkg_color, HTML_color_t text_color,
    int use_windows_colors, int use_windows_bgcolor,
    HTML_color_t sel_bkg_color, HTML_color_t sel_text_color,
    int use_win_sel_colors, int use_win_sel_bg)
{
    /* inherit default handling to update the colors */
    CHtmlSysWin_win32_tdb::note_debug_format_changes(
        bkg_color, text_color, use_windows_colors, use_windows_bgcolor,
        sel_bkg_color, sel_text_color, use_win_sel_colors, use_win_sel_bg);

    /*
     *   forget the icon size, so we remeasure the next time we need it (wait
     *   until we actually need it, since that way we'll be sure the new font
     *   has been selected)
     */
    icon_size_ = W32TDB_ICON_UNKNOWN;
}

/*
 *   Measure the debugger source icon
 */
CHtmlPoint CHtmlSysWin_win32_dbginfo::measure_dbgsrc_icon()
{
    CHtmlPoint retval;

    /* if we haven't decided what size icon to use, figure it out */
    if (icon_size_ == W32TDB_ICON_UNKNOWN)
        calc_icon_size();

    switch(icon_size_)
    {
    case W32TDB_ICON_SMALL:
        /* the small icons are 8x8 */
        retval.x = 10;
        retval.y = 8;
        return retval;

    case W32TDB_ICON_NORMAL:
    default:
        /*
         *   the normal icons are 16x16, but put in a couple of extra pixels
         *   horizontally to space things out
         */
        retval.x = 20;
        retval.y = 16;
        return retval;
    }
}

/*
 *   Calculate the icon size, if we haven't already
 */
void CHtmlSysWin_win32_dbginfo::calc_icon_size()
{
    HDC dc;
    TEXTMETRIC tm;

    /* if we already know the icon size, save the trouble */
    if (icon_size_ != W32TDB_ICON_UNKNOWN)
        return;

    /* get my device context */
    dc = GetDC(handle_);

    /*
     *   get my current text metrics - use small or normal icons, depending
     *   on the text size
     */
    GetTextMetrics(dc, &tm);

    /* done with our device context */
    ReleaseDC(handle_, dc);

    /* use small icons or large icons, depending on line spacing */
    icon_size_ = (tm.tmHeight + tm.tmExternalLeading < 14
                  ? W32TDB_ICON_SMALL
                  : W32TDB_ICON_NORMAL);

    /* recalculate the caret height while we're at it */
    set_caret_size(formatter_->get_font());
}

/*
 *   Draw a debugger source line icon
 */
void CHtmlSysWin_win32_dbginfo::draw_dbgsrc_icon(
    const CHtmlRect *pos, unsigned int stat)
{
    CHtmlRect scrpos;
    RECT rc;
    HICON ico;
    int siz;

    /* if we haven't decided what size icon to use, figure it out */
    if (icon_size_ == W32TDB_ICON_UNKNOWN)
        calc_icon_size();

    /* set up a windows RECT structure for the screen coordinates */
    scrpos = doc_to_screen(*pos);
    scrpos.right -= 2;
    SetRect(&rc, scrpos.left, scrpos.top, scrpos.right, scrpos.bottom);

    /* draw the margin background */
    draw_margin_bkg(&rc);

    /* if there's no breakpoint, lose any related status */
    if ((stat & HTMLTDB_STAT_BP) == 0)
        stat &= ~(HTMLTDB_STAT_BP_DIS | HTMLTDB_STAT_BP_COND);

    /* select the correct icon based on the status */
    switch(stat & (HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_CTXLINE
                   | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS))
    {
    case HTMLTDB_STAT_CURLINE:
    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_CTXLINE:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curline_ : smico_dbg_curline_);
        break;

    case HTMLTDB_STAT_CTXLINE:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxline_ : smico_dbg_ctxline_);
        break;

    case HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_bp_ : smico_dbg_bp_);
        break;

    case HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_bpdis_ : smico_dbg_bpdis_);
        break;

    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curbp_ : smico_dbg_curbp_);
        break;

    case HTMLTDB_STAT_CURLINE | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_curbpdis_ : smico_dbg_curbpdis_);
        break;

    case HTMLTDB_STAT_CTXLINE | HTMLTDB_STAT_BP:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxbp_ : smico_dbg_ctxbp_);
        break;

    case HTMLTDB_STAT_CTXLINE | HTMLTDB_STAT_BP | HTMLTDB_STAT_BP_DIS:
        ico = (icon_size_ == W32TDB_ICON_NORMAL
               ? ico_dbg_ctxbpdis_ : smico_dbg_ctxbpdis_);
        break;

    default:
        ico = 0;
        break;
    }

    /* note the size */
    switch (icon_size_)
    {
    case W32TDB_ICON_NORMAL:
        /* offset the area slightly to leave blank space in the margin */
        rc.left += 2;

        /* normal icons are 16x16 pixels */
        siz = 16;
        break;

    case W32TDB_ICON_SMALL:
        /* small icons are 8x8 */
        siz = 8;
        break;
    }

    /* vertically center */
    rc.top += (rc.bottom - rc.top - siz) / 2;

    /* draw the icon in the given area */
    if (ico != 0)
        DrawIconEx(hdc_, rc.left, rc.top, ico, siz, siz, 0, 0, DI_NORMAL);
}

/*
 *   draw the margin background
 */
void CHtmlSysWin_win32_dbginfo::draw_margin_bkg(const RECT *rc)
{
    /* fill in the area with a light gray background */
    FillRect(hdc_, rc, GetSysColorBrush(COLOR_3DFACE));
}

/*
 *   Set the background cursor.  We use the i-beam cursor when the mouse
 *   isn't over a display item.
 */
int CHtmlSysWin_win32_dbginfo::do_setcursor_bkg()
{
    /* use the I-beam cursor */
    SetCursor(ibeam_csr_);

    /* indicate that we set a cursor */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger doc search results window.
 */

/* create our HTML panel */
void CHtmlSys_docsearchwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_docsearch(
        formatter, this, TRUE, TRUE, prefs_, helper_, dbgmain_);
}

/*
 *   store our window position in the preferences
 */
void CHtmlSys_docsearchwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("docsearch win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("docsearch win pos", 0, pos);
}

/*
 *   Process hyperlink clicks in the search window
 */
void CHtmlSys_docsearchwin::process_command(
    const textchar_t *cmd, size_t len, int append, int enter, int os_cmd_id)
{
    /* check the prefix */
    if (len > 5 && memcmp(cmd, "more:", 5) == 0)
    {
        /* process it as a continuing search request */
        process_more_command(cmd + 5, len - 5);
    }
    else if (len > 5 && memcmp(cmd, "file:", 5) == 0)
    {
        /* process it as a file request */
        process_file_command(cmd + 5, len - 5);
    }
}

/* process a hyperlink click on a "more:" link */
void CHtmlSys_docsearchwin::process_more_command(
    const textchar_t *cmd, size_t len)
{
    /*
     *   the HREF is in the format "n,query", where 'n' is the starting index
     *   for the next set of results and 'query' is the URL-encoded query
     *   string
     */

    /* make sure the format looks right */
    const char *p = strchr(cmd, ',');
    if (p == 0)
        return;

    /* decode the starting index and query string */
    int start_idx = atoi(cmd);
    char *query = url_decode(p+1, len - (p+1 - cmd));

    /* run the query */
    ((CHtmlSysWin_win32_docsearch *)html_panel_)->execute_search(
        query, start_idx);

    /* done with the query string */
    th_free(query);
}

/* process a hyperlink click on a "file:" link */
void CHtmlSys_docsearchwin::process_file_command(
    const textchar_t *cmd, size_t len)
{
    char buf[1024];
    const doc_manifest_t *m;
    char rootdir[MAX_PATH];
    char *p;

    /* limit the copy to the buffer size */
    if (len > sizeof(buf) - 1)
        len = sizeof(buf) - 1;

    /* copy it and null-terminate it */
    memcpy(buf, cmd, len);
    buf[len] = '\0';

    /* get the root directory - this is simply the .exe path */
    GetModuleFileName(0, rootdir, sizeof(rootdir));
    p = os_get_root_name(rootdir);
    if (p > rootdir)
        *(p-1) = '\0';

    /*
     *   Check for a doc manifest entry, to see if we need to generate a
     *   temporary frame page: if we find a manifest, and it has a frame
     *   page, we do need to generate the dynamic frame for the content.
     */
    if ((m = find_doc_manifest(rootdir, buf)) != 0 && m->frame_page != 0)
    {
        FILE *fpin = 0, *fpout = 0;
        char tmppath[MAX_PATH];
        char tmpfile[MAX_PATH];
        char basepath[MAX_PATH];
        char framefile[MAX_PATH];
        char *contrel;

        /*
         *   We have to generate a dynamic frame page for this object,
         *   because it belongs inside a frame, but the normal frame page is
         *   for the "front page" of the book, not for this specific content
         *   page.  We need to generate a frame that selects this content
         *   page.  To do this, we need to create a temporary copy of the
         *   static frame page, but with the SRC for the container frame
         *   replaced with the content page we're trying to view.  Once we
         *   generate the temporary page, we can launch our browser and point
         *   it at that page.
         */

        /* build the full path to the frame file */
        _makepath(framefile, 0, rootdir, m->frame_page, 0);

        /*
         *   if that's actually the page they're trying to view, don't bother
         *   framing it further - this ensures that we don't try to frame the
         *   top level frame setup page itself
         */
        if (stricmp(framefile, buf) == 0)
            goto done;

        /* pull out the directory path - this is our URL base in the frame */
        strcpy(basepath, framefile);
        p = os_get_root_name(basepath);
        *p = '\0';

        /* open the book's ordinary static frame file */
        if ((fpin = fopen(framefile, "r")) == 0)
            goto done;

        /* create a temporary file for the dynamic frame page */
        GetTempPath(sizeof(tmppath), tmppath);
        for (int r = rand() ; ; r = rand())
        {
            char fname[30];

            /* set up a temp file name */
            sprintf(fname, "dsFrm%x.htm", r);
            os_build_full_path(tmpfile, sizeof(tmpfile), tmppath, fname);

            /* if this isn't an existing file, use it */
            if ((fpout = fopen(tmpfile, "r")) != 0)
                fclose(fpout);
            else
                break;
        }

        /* open it */
        if ((fpout = fopen(tmpfile, "w")) == 0)
        {
            fclose(fpin);
            goto done;
        }

        /* get the content page relative to the base path */
        if (strlen(buf) > strlen(basepath)
            && memicmp(buf, basepath, strlen(basepath)) == 0)
            contrel = buf + strlen(basepath);
        else
            contrel = buf;

        /* process the original frame file */
        for (;;)
        {
            char lbuf[4096];
            char *p;

            /* get the next line */
            if (fgets(lbuf, sizeof(lbuf), fpin) == 0)
                break;

            /* scan for "<head>" */
            if ((p = stristr(lbuf, "<head>")) != 0)
            {
                size_t len = strlen(basepath);

                /* insert our <base HREF=""> tag just after the <head> */
                p += 6;
                memmove(p + 14 + len, p, strlen(p) + 1);

                strcpy(p, "<base href=\"");
                memcpy(p + 12, basepath, len);
                memcpy(p + 12 + len, "\">", 2);
            }

            /* scan for our target <frame> */
            for (p = stristr(lbuf, "<frame ") ; p != 0 ;
                 p = stristr(p + 1, "<frame "))
            {
                char *q;
                char *nm;
                char qu = '\0';

                /* find the NAME */
                if ((q = stristr(p, "name=")) == 0)
                    break;

                /* skip the open quote */
                q += 5;
                if (*q == '"' || *q == '\'')
                {
                    qu = *q;
                    ++q;
                }

                /* scan the name */
                for (nm = q ; *q != '\0' ; ++q)
                {
                    /* stop at the close quote or the space */
                    if ((qu != '\0' && *q == qu)
                        || (qu == '\0' && (*q == ' ' || *q == '>')))
                        break;
                }

                /* check for our target name */
                if (q - nm == (int)strlen(m->frame_title)
                    && memicmp(nm, m->frame_title, q - nm) == 0)
                {
                    /* this is our frame - go back and find the SRC tag */
                    p = stristr(p, "src=\"");

                    /* if we found it, plug in our target page */
                    if (p != 0)
                    {
                        /* find the close quote */
                        p += 5;
                        q = strchr(p, '"');

                        /* if we found it, replace it */
                        memmove(p + strlen(contrel), q, strlen(q) + 1);
                        memcpy(p, contrel, strlen(contrel));
                    }

                    /* done scanning */
                    break;
                }
            }

            /* write out this line */
            fputs(lbuf, fpout);
        }

        /* close files */
        fclose(fpin);
        fclose(fpout);

        /* the temp file is what we actually want to display */
        strcpy(buf, tmpfile);

        /* create a cleanup entry, so that we delete this file in a bit */
        temp_page_cleaner *cl = (temp_page_cleaner *)th_malloc(
            sizeof(temp_page_cleaner) + get_strlen(tmpfile));

        /* set it up and link it into the list of cleanup records */
        strcpy(cl->fname, tmpfile);
        cl->t0 = GetTickCount();
        cl->nxt = S_temp_pages;
        S_temp_pages = cl;

    done: ;
    }


    /* open the document in the help window */
    dbgmain_->show_help(buf);
}

/* ------------------------------------------------------------------------ */
/*
 *   Doc Search html panel window
 */

/*
 *   Execute a search
 */
void CHtmlSysWin_win32_docsearch::execute_search(
    const textchar_t *query, int start_idx)
{
    char dir[MAX_PATH];
    char indexfile[MAX_PATH];
    char *p;
    docsearch_result_list lst;
    int cnt;
    int i;
    struct CHtmlDbg_win_link *link;
    class CHtmlTextBuffer txtbuf;
    int limit = 25;
    int end_idx;
    char buf[128];
    long end_scroll;

    /* the search index and the doc live in our executable directory */
    GetModuleFileName(CTadsApp::get_app()->get_instance(),
                      dir, sizeof(dir));
    if ((p = os_get_root_name(dir)) != 0 && p > dir)
        *(p-1) = '\0';

    /* count the documentation directory list size */
    for (i = 0 ; w32_doc_dirs[i] != 0 ; ++i) ;

    /* set the long-running process message */
    dbgmain_->set_lrp_status("Searching documentation index...");

    /* get the folder path for common (All Users) application data */
    build_special_path(indexfile, sizeof(indexfile), CSIDL_COMMON_APPDATA,
                       "SearchIndex.dat", TRUE, TRUE);

    /* local search is available - run the query */
    lst = docsearch_execute(0, query, &cnt, indexfile,
                            dir, w32_doc_dirs, i);

    /* if there are no results, there's no count */
    if (lst == 0)
        cnt = 0;

    /* get some information from the helper object */
    link = helper_->get_doc_search_link();

    /* if this isn't a continuing search, clear out the last search results */
    if (start_idx == 0)
    {
        /* clear out the old results */
        link->srcmgr_->clear_document();
        notify_clear_contents();

        /* scroll to the top when done */
        end_scroll = 0;
    }
    else
    {
        /*
         *   when done, scroll to the current bottom of the document, which
         *   will be the top of the new results
         */
        end_scroll = get_formatter()->get_max_y_pos() / get_vscroll_units();

        /* add a paragraph break after the previous material */
        txtbuf.append("<p>");
    }

    /* get the query in HTML notation */
    char *query_html = html_encode(query);

    /*
     *   figure the end of our results: stop when we run out of results, or
     *   when we reach the limit for one block of results
     */
    end_idx = cnt;
    if (end_idx > start_idx + limit)
        end_idx = start_idx + limit;

    /* set up the header */
    txtbuf.append("<body link=#0000ff vlink=#008080 alink=#ff00ff>"
                  "<p><basefont face='Verdana,Arial,TADS-Sans' size=2>"
                  "<table width=100% bgcolor=#e0e0e0><tr><td>"
                  "<font color=black>Search Results for <b>");
    txtbuf.append(query_html);
    txtbuf.append("</b></font>"
                  "<td align=right><font size=-1>Results ");
    sprintf(buf, "%d-%d of %d", start_idx + 1, end_idx, cnt);
    txtbuf.append(buf);
    txtbuf.append("</font></table><br><br>");

    /* if there are no results, just say so, and we're done */
    if (lst == 0 || cnt == 0)
    {
        txtbuf.append("<p>No matches found.");
    }
    else if (cnt <= start_idx)
    {
        txtbuf.append("<p>No more matches found.");
    }
    else
    {
        /* start the list */
        sprintf(buf, "<ol start=%d>", start_idx + 1);
        txtbuf.append(buf);

        /* list the results */
        for (i = start_idx ; i < end_idx ; ++i)
        {
            docsearch_details d;

            /* add a status message if this is taking a while */
            if (i % 25 == 24)
            {
                char buf[100];
                sprintf(buf, "Building results - %d of %d done...", i, cnt);
                dbgmain_->set_lrp_status(buf);
            }

            /* get this item's details */
            if (docsearch_get_details(lst, i, &d))
            {
                const doc_manifest_t *mf;

                /* show the line */
                txtbuf.append("<li><a href=\"file:");
                txtbuf.append(d.fname);
                txtbuf.append("\"><font size=3>");
                txtbuf.append(d.title[0] != '\0' ? d.title : d.fname);
                txtbuf.append("</font></a>");

                /* identify the book */
                if ((mf = find_doc_manifest(dir, d.fname)) != 0)
                {
                    txtbuf.append(" &nbsp;&nbsp; (<i><a href=\"file:");
                    txtbuf.append(dir);
                    txtbuf.append("\\");
                    txtbuf.append(mf->front_page);
                    txtbuf.append("\">");
                    txtbuf.append(mf->title);
                    txtbuf.append("</a></i>)");
                }

                /* add the summary and finish the item */
                txtbuf.append("<br>");
                txtbuf.append(d.summary);
                txtbuf.append("<br><br>");
            }
            else
            {
                txtbuf.append("<li><i>Unable to retrieve details on this "
                              "result.</i>");
            }
        }

        /* end the page */
        txtbuf.append("</ol>");
    }

    /* add a search continuation link if there are more results to show */
    if (end_idx < cnt)
    {
        char *query_link = url_encode(query);

        sprintf(buf, "<a href=\"more:%d,", end_idx);
        txtbuf.append(buf);
        txtbuf.append(query_link);
        txtbuf.append("\"><font size=-1>Show results ");
        sprintf(buf, "%d-%d for <b>", end_idx + 1, min(end_idx + limit, cnt));
        txtbuf.append(buf);
        txtbuf.append(query_html);
        txtbuf.append("</b></font></a><br><br>");

        /* done with the link-encoded query */
        th_free(query_link);
    }

    /* display it */
    link->srcmgr_->append_line(txtbuf.getbuf(), txtbuf.getlen());
    do_formatting(FALSE, FALSE, TRUE);
    fmt_adjust_vscroll();

    /* scroll to the ending position */
    do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, end_scroll, TRUE);

    /* free the HTML query */
    th_free(query_html);

    /* done with the result list */
    docsearch_free_list(lst);

    /* clear the status message */
    dbgmain_->set_lrp_status(0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger Project File Search Results window.
 */

/* create our HTML panel */
void CHtmlSys_filesearchwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_filesearch(
        formatter, this, TRUE, TRUE, prefs_, helper_, dbgmain_);
}

/*
 *   store our window position in the preferences
 */
void CHtmlSys_filesearchwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("filesearch win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("filesearch win pos", 0, pos);
}

/*
 *   Process hyperlink clicks in the search window
 */
void CHtmlSys_filesearchwin::process_command(
    const textchar_t *cmd, size_t len, int append, int enter, int os_cmd_id)
{
    char fname[MAX_PATH];
    IDebugWin *win = 0;

    /*
     *   Our commands are links to files - either "file:xxx" to link to a
     *   file generally, without a line number, or "line:nn:xxx" to go to a
     *   given line number in the file.
     */
    if (memcmp(cmd, "file:", 5) == 0)
    {
        /* open the file */
        safe_strcpy(fname, sizeof(fname), cmd + 5);
        win = dbgmain_->get_helper()->open_text_file(dbgmain_, fname, fname);
    }
    else if (memcmp(cmd, "line:", 5) == 0)
    {
        const textchar_t *p;

        /* find the end of the line number field */
        if ((p = strchr(cmd + 5, ':')) != 0)
        {
            /* parse the line number and retrieve the filename */
            int linenum = atoi(cmd + 5);
            safe_strcpy(fname, sizeof(fname), p + 1);

            /* open the file */
            win = dbgmain_->get_helper()->open_text_file_at_line(
                dbgmain_, fname, fname, linenum);
        }
    }

    /* if we opened the window, bring it to the front */
    if (win != 0)
        ((IDebugSysWin *)win)->idsw_bring_to_front();
}

CHtmlSysWin_win32_filesearch::CHtmlSysWin_win32_filesearch(
    class CHtmlFormatter *formatter,
    class CHtmlSysWin_win32_owner *owner,
    int has_vscroll, int has_hscroll,
    class CHtmlPreferences *prefs,
    class CHtmlDebugHelper *helper,
    class CHtmlSys_dbgmain *dbgmain)
    : CHtmlSysWin_win32_tdb(formatter, owner, has_vscroll,
                            has_hscroll, prefs, helper, dbgmain)
{
    /*
     *   do not automatically scroll - when we show new results, we want to
     *   keep the top results showing
     */
    auto_vscroll_ = FALSE;

    /* load my custom context menu */
    load_context_popup(IDR_FILESEARCH_POPUP_MENU);

    /* get our initial auto-clear status */
    if (dbgmain_->get_global_config()->getval(
        "filesearch autoclear", "", &auto_clear_))
        auto_clear_ = TRUE;
}

TadsCmdStat_t CHtmlSysWin_win32_filesearch::active_check_command(
    const check_cmd_info *info)
{
    switch (info->command_id)
    {
    case ID_EDIT_CLEARFILESEARCH:
        return TADSCMD_ENABLED;

    case ID_EDIT_AUTOCLEARFILESEARCH:
        return auto_clear_ ? TADSCMD_CHECKED : TADSCMD_ENABLED;
    }

    /* inherit default handling */
    return CHtmlSysWin_win32::check_command(info);
}

int CHtmlSysWin_win32_filesearch::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    switch (command_id)
    {
    case ID_EDIT_CLEARFILESEARCH:
        /* clear our window */
        clear_window();
        return TRUE;

    case ID_EDIT_AUTOCLEARFILESEARCH:
        /* set the new status */
        auto_clear_ = !auto_clear_;

        /* save it in the config */
        dbgmain_->get_global_config()->setval(
            "filesearch autoclear", "", auto_clear_);

        /* handled */
        return TRUE;
    }

    /* inherit default handling */
    return CHtmlSysWin_win32::do_command(notify_code, command_id, ctl);
}

/*
 *   begin a search
 */
void CHtmlSysWin_win32_filesearch::begin_search()
{
    /* if we're in auto-clear mode, clear our contents */
    if (auto_clear_)
    {
        /* clear it out */
        clear_window();

        /* scroll to the top when done */
        end_search_scroll_ = 0;
    }
    else
    {
        /*
         *   when done, scroll to the current bottom of the document, which
         *   will be the top of the new search
         */
        end_search_scroll_ = get_formatter()->get_max_y_pos()
                             / get_vscroll_units();
    }
}

/*
 *   end a search
 */
void CHtmlSysWin_win32_filesearch::end_search()
{
    /* scroll to the ending scroll position */
    do_scroll(TRUE, vscroll_, SB_THUMBPOSITION, end_search_scroll_, TRUE);
}

/*
 *   clear the windiow
 */
void CHtmlSysWin_win32_filesearch::clear_window()
{
    CHtmlDbg_win_link *link;

    /* get our window link */
    link = dbgmain_->get_helper()->get_file_search_link();

    /* tell it to clear our document */
    link->srcmgr_->clear_document();

    /* notify our renderer of the change, and reformat */
    notify_clear_contents();
    do_formatting(FALSE, FALSE, TRUE);
    fmt_adjust_vscroll();
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger stack window implementation.  This is a simple
 *   specialization of the debugger source window which provides a
 *   slightly different drawing style.
 */

/*
 *   create the HTML panel
 */
void CHtmlSys_dbgstkwin::create_html_panel(CHtmlFormatter *formatter)
{
    CHtmlRect margins;

    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_dbgstk(formatter, this, TRUE, TRUE,
                                               prefs_, helper_, dbgmain_);

    /* don't inset from the left margin at all */
    margins = formatter->get_phys_margins();
    margins.left = 0;
    formatter->set_phys_margins(&margins);
}

/*
 *   store my position in the preferences
 */
void CHtmlSys_dbgstkwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("stack win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("stack win pos", 0, pos);
}

/*
 *   process a mouse click
 */
int CHtmlSysWin_win32_dbgstk::do_leftbtn_down(int keys, int x, int y,
                                              int clicks)
{
    /* if we have a double-click, do some special processing */
    if (clicks == 2)
    {
        CHtmlPoint pos;
        CHtmlDisp *disp;

        /* ignore it if we're not in the debugger */
        if (!dbgmain_->get_in_debugger())
        {
            Beep(1000, 100);
            return TRUE;
        }

        /*
         *   Get the document coordinates.  Ignore the x position of the
         *   click; act as though they're clicking on the left edge of the
         *   text area.
         */
        pos = screen_to_doc(CHtmlPoint(x, y));
        pos.x = 0;

        /* find the item we're clicking on */
        disp = formatter_->find_by_pos(pos, TRUE);
        if (disp != 0)
        {
            IDebugSysWin *win;

            /* tell the helper to select this item */
            win = (IDebugSysWin *)helper_->activate_stack_win_item(
                disp, dbgmain_->get_dbg_ctx(), dbgmain_);

            /*
             *   tell the debugger to refresh its expression windows for
             *   the new context
             */
            dbgmain_->update_expr_windows();

            /* bring the source window to the front */
            if (win != 0)
                win->idsw_bring_to_front();

            /* don't track the mouse any further */
            end_mouse_tracking(HTML_TRACK_LEFT);

            /* handled */
            return TRUE;
        }
    }

    /* inherit the default */
    return CHtmlSysWin_win32_dbginfo::do_leftbtn_down(keys, x, y, clicks);
}

/*
 *   check the status of a command
 */
TadsCmdStat_t CHtmlSysWin_win32_dbgstk::active_check_command(
    const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_EDIT_FIND:
        /* set the blank menu name, and disable the command */
        check_command_change_menu(info, IDS_TPL_FIND_BLANK);
        return TADSCMD_DISABLED;

    case ID_EDIT_FINDNEXT:
        /* can't do these in a stack window */
        return TADSCMD_DISABLED;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbginfo::active_check_command(info);
    }
}

/*
 *   execute a command
 */
int CHtmlSysWin_win32_dbgstk::active_do_command(int notify_code,
                                                int command_id, HWND ctl)
{
    switch(command_id)
    {
    case ID_EDIT_FIND:
    case ID_EDIT_FINDNEXT:
        /* can't do this in a stack window */
        return FALSE;

    default:
        /* inherit default */
        return CHtmlSysWin_win32_dbginfo::
            active_do_command(notify_code, command_id, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   History window implementation
 */

/*
 *   create the HTML panel
 */
void CHtmlSys_dbghistwin::create_html_panel(CHtmlFormatter *formatter)
{
    /* create the debugger stack window panel */
    html_panel_ = new CHtmlSysWin_win32_dbghist(formatter, this, TRUE, TRUE,
                                                prefs_, helper_, dbgmain_);
}

/*
 *   store my position in the preferences
 */
void CHtmlSys_dbghistwin::store_winpos_prefs(const CHtmlRect *pos)
{
    /* store the position in the debugger configuration */
    if (dbgmain_->get_local_config() != 0)
        dbgmain_->get_local_config()->setval("hist win pos", 0, pos);

    /* store it in the global configuration (for the default) as well */
    if (dbgmain_->get_global_config() != 0)
        dbgmain_->get_global_config()->setval("hist win pos", 0, pos);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Wait dialog
 */

/*
 *   process a command
 */
int CTadsDlg_buildwait::do_command(WPARAM id, HWND ctl)
{
    switch(id)
    {
    case IDOK:
        /* do not dismiss the dialog on regular keystrokes */
        return TRUE;

    case IDCANCEL:
        /* on cancel/escape, ask about killing the compiler */
        if (MessageBox(handle_,
                       "Unless the build appears to be stuck, you "
                       "should wait for it to finish normally, because "
                       "forcibly ending a process can destabilize Windows. "
                       "Do you really want to force the build to terminate?",
                       "Warning",
                       MB_YESNO | MB_ICONEXCLAMATION | MB_TASKMODAL)
            == IDYES)
        {
            /* try killing the compiler process */
            TerminateProcess(comp_hproc_, (UINT)-1);
        }

        /*
         *   handled - don't dismiss, since we still want to wait until the
         *   compiler process ends
         */
        return TRUE;

    case IDC_BUILD_DONE:
        /*
         *   special message sent from main window when it hears that the
         *   build is done - dismiss the dialog
         */
        EndDialog(handle_, id);
        return TRUE;

    default:
        /* inherit default processing for anything else */
        return CTadsDialog::do_command(id, ctl);
    }
}
