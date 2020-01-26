#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2013 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32dbgpr3.cpp - Workbench dialogs specific to TADS 3
Function
  Contains dialogs for TADS Workbench specific to the TADS 3 version
Notes

Modified
  05/01/13 MJRoberts  - Creation (split off from w32dbgpr.cpp)
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <Windows.h>

#include <os.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef W32DBGPR_H
#include "w32dbgpr.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef W32MAIN_H
#include "w32main.h"
#endif
#ifndef TADSREG_H
#include "tadsreg.h"
#endif
#ifndef TADSCOM_H
#include "tadscom.h"
#endif
#ifndef TADSHTMLDLG_H
#include "tadshtmldlg.h"
#endif
#ifndef TADSKB_H
#include "tadskb.h"
#endif

#define countof(x) (sizeof(x)/sizeof((x)[0]))


/* ------------------------------------------------------------------------ */
/*
 *   New project wizard HTML dialog
 */

/* template file descriptor */
struct new_proj_tpl
{
    /* full filename of the project */
    CStringBuf fname;

    /*
     *   project path relative to the system library folder containing the
     *   template (this is either the 'extensions' folder or one of the
     *   library folders in the 'srcdir' list)
     */
    CStringBuf rel_path;

    /* list of name/value pairs parsed from the file */
    CNameValTab tab;

    /* next template in list */
    new_proj_tpl *nxt;
};

/* is c a space? */
#define u_isspace(c) ((unsigned char)(c) < 128 && isspace(c))

/* is c a horizontal space? */
#define u_ishspace(c) (u_isspace(c) && (c) != '\n' && (c) != '\r')

/* is-newline - matches \n, \r, and \u2028 */
static int u_isnl(const char *p, size_t len)
{
    return (*p == '\n'
            || *p == '\r'
            || (len >= 3
                && *(unsigned char *)p == 0xe2
                && *(unsigned char *)(p+1) == 0x80
                && *(unsigned char *)(p+2) == 0xa8));
}

/*
 *   Skip a newline sequence.  Skips all common conventions, including \n,
 *   \r, \n\r, \r\n, and \u2028.
 */
static void skip_newline(const char *&p, size_t &rem)
{
    /* make sure we have something to skip */
    if (rem == 0)
        return;

    /* check what we have */
    switch ((unsigned char)*p)
    {
    case '\n':
        /* skip \n or \n\r */
        ++p, --rem ;
        if (*p == '\r')
            ++p, --rem;
        break;

    case '\r':
        /* skip \r or \r\n */
        ++p, --rem;
        if (*p == '\n')
            ++p, --rem;
        break;

    case 0xe2:
        /* \u2028 (unicode line separator) - just skip the one character */
        if (rem >= 3)
            p += 3, rem -= 3;
        break;
    }
}

/*
 *   Skip to the next line
 */
static void skip_to_next_line(const char *&p, size_t &rem)
{
    /* look for the next newline */
    for ( ; rem != 0 ; ++p, --rem)
    {
        /* if this is a newline of some kind, we're at the end of the line */
        if (u_isnl(p, rem))
        {
            /* skip the newline, and we're done */
            skip_newline(p, rem);
            break;
        }
    }
}

/* HTML dialog arguments */
class NewProjectArgs: public TadsDispatch
{
public:
    NewProjectArgs(CHtmlSys_dbgmain *dbgmain)
    {
        /* add one reference for our caller */
        refcnt_ = 1;

        /* remember the main debug window */
        (dbgmain_ = dbgmain)->AddRef();

        /* scan for project starter templates */
        tpl_head_ = tpl_tail_ = 0;
        scan_for_templates();
    }

    ~NewProjectArgs()
    {
        /* release our debugger window object */
        if (dbgmain_ != 0)
            dbgmain_->Release();

        /* delete our template file list */
        while (tpl_head_ != 0)
        {
            new_proj_tpl *nxt = tpl_head_->nxt;
            delete tpl_head_;
            tpl_head_ = nxt;
        }
        tpl_tail_ = 0;
    }

    /* main debug window */
    CHtmlSys_dbgmain *dbgmain_;

    /* list of project template files */
    new_proj_tpl *tpl_head_, *tpl_tail_;

    /* scan for templates */
    void scan_for_templates()
    {
        /* scan the "lib" subdirectory of the install folder */
        char buf[OSFNMAX], buf2[OSFNMAX];
        GetModuleFileName(
            CTadsApp::get_app()->get_instance(), buf, sizeof(buf));
        os_get_path_name(buf2, sizeof(buf2), buf);
        os_build_full_path(buf, sizeof(buf), buf2, "lib");
        scan_for_templates(buf, "");

        /* get the global configuration object */
        CHtmlDebugConfig *gc = dbgmain_->get_global_config();

        /* scan the extensions directory */
        if (gc->getval("extensions dir", 0, 0, buf, sizeof(buf)))
            scan_for_templates(buf, "");

        /* scan the library directories */
        int cnt = gc->get_strlist_cnt("srcdir", 0);
        for (int i = 0 ; i < cnt ; ++i)
        {
            if (!gc->getval("srcdir", 0, i, buf, sizeof(buf)))
                scan_for_templates(buf, "");
        }
    }

    /* scan a directory and all of its subdirectories for templates */
    void scan_for_templates(const char *dir, const char *rel_path)
    {
        /* scan this directory */
        scan_dir_for_templates(dir, rel_path);

        /* search for all files in this directory, and scan subdirectories */
        char pat[OSFNMAX];
        os_build_full_path(pat, sizeof(pat), dir, "*");
        WIN32_FIND_DATA inf;
        HANDLE hfind = FindFirstFile(pat, &inf);
        while (hfind != INVALID_HANDLE_VALUE)
        {
            /* if it's a directory (except . or ..), recursively scan it */
            if ((inf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                && (inf.dwFileAttributes
                    & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) == 0
                && strcmp(inf.cFileName, ".") != 0
                && strcmp(inf.cFileName, "..") != 0)
            {
                /* scan this subdirectory */
                char sub[OSFNMAX], sub_rel[OSFNMAX];
                os_build_full_path(sub, sizeof(sub), dir, inf.cFileName);
                os_build_full_path(
                    sub_rel, sizeof(sub_rel), rel_path, inf.cFileName);
                scan_for_templates(sub, sub_rel);
            }

            /* move on */
            if (!FindNextFile(hfind, &inf))
            {
                FindClose(hfind);
                break;
            }
        }
    }

    /* scan a single directory for template files */
    void scan_dir_for_templates(const char *dir, const char *rel_path)
    {
        /* search for *.tdb-project-starter */
        char pat[OSFNMAX];
        os_build_full_path(pat, sizeof(pat), dir, "*.tdb-project-starter");
        WIN32_FIND_DATA inf;
        HANDLE hfind = FindFirstFile(pat, &inf);
        while (hfind != INVALID_HANDLE_VALUE)
        {
            /* if it's an ordinary file, include it */
            if ((inf.dwFileAttributes
                 & (FILE_ATTRIBUTE_DIRECTORY
                    | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) == 0)
            {
                char fname[OSFNMAX];
                os_build_full_path(fname, sizeof(fname), dir, inf.cFileName);
                add_template(fname, rel_path);
            }

            /* move on */
            if (!FindNextFile(hfind, &inf))
            {
                FindClose(hfind);
                break;
            }
        }
    }

    /* add a template file to our list */
    void add_template(const char *fname, const char *rel_path)
    {
        /*
         *   Make sure the template isn't already in the list.  We can scan
         *   the same directory multiple times if there's overlap among the
         *   default library folders and the various paths configured in the
         *   options settings (Library Paths, Extensions).  So make sure that
         *   we haven't already added this file.
         */
        new_proj_tpl *tpl;
        for (tpl = tpl_head_ ; tpl != 0 ; tpl = tpl->nxt)
        {
            if (strcmp(tpl->fname.get(), fname) == 0)
                return;
        }

        /* open the file */
        FILE *fp = fopen(fname, "rb");
        if (fp == 0)
            return;

        /* allocate space to hold the file's contents */
        fseek(fp, 0, SEEK_END);
        size_t len = (size_t)ftell(fp);
        char *buf = (char *)th_malloc(len);
        if (buf == 0)
        {
            fclose(fp);
            return;
        }

        /* load the file */
        fseek(fp, 0, SEEK_SET);
        int ok = (fread(buf, 1, len, fp) == len);

        /* we've read the whole file, so we're done with the handle */
        fclose(fp);

        /* if the read failed, give up */
        if (!ok)
        {
            th_free(buf);
            return;
        }

        /* create a new template list entry */
        tpl = new new_proj_tpl;
        tpl->fname.set(fname);
        tpl->rel_path.set(rel_path);

        /* link it into our list */
        tpl->nxt = 0;
        if (tpl_tail_ != 0)
            tpl_tail_->nxt = tpl;
        else
            tpl_head_ = tpl;
        tpl_tail_ = tpl;

        /* scan the file */
        char *p;
        size_t rem;
        for (p = buf, rem = len ; rem != 0 ; )
        {
            /* skip any leading whitespace */
            while (rem != 0 && u_isspace(*p))
                ++p, --rem;

            /* if the line starts with '#', it's a comment, so skip it */
            if (rem != 0 && *p == '#')
            {
                skip_to_next_line(p, rem);
                continue;
            }

            /* we must have the start of a name - note it */
            char *name_start = p;

            /* skip ahead to a space or colon */
            while (rem != 0 && *p != ':' && !u_ishspace(*p))
                ++p, --rem;

            /* note the length of the name */
            size_t name_len = p - name_start;

            /* skip any whitespace before the presumed colon */
            while (rem != 0 && u_ishspace(*p))
                ++p, --rem;

            /* if we're not at a colon, the line is ill-formed, so skip it */
            if (rem == 0 || *p != ':')
            {
                /* skip the entire line, and go back for the next one */
                skip_to_next_line(p, rem);
                continue;
            }

            /* skip the colon and any whitespace immediately after it */
            for (++p, --rem ; rem != 0 && u_ishspace(*p) ; ++p, --rem) ;

            /* note where the value starts */
            char *val_start = p;

            /*
             *   Scan the value to get its length.  The value runs from here
             *   to the next newline that's not followed immediately by a
             *   space.  We'll also delete leading spaces on continuation
             *   lines while we're scanning.
             */
            char *dst = p;
            while (rem != 0)
            {
                /* advance to the next line */
                while (rem != 0 && !u_isnl(p, rem))
                {
                    *dst++ = *p++;
                    --rem;
                }

                /* skip the newline */
                skip_newline(p, rem);

                /* check for a continuation line, starting with a space */
                if (rem != 0 && u_ishspace(*p))
                {
                    /* skip leading spaces */
                    for ( ; rem != 0 && u_ishspace(*p) ; ++p, --rem) ;

                    /*
                     *   if we found a non-whitespace character, it's a
                     *   continuation line; otherwise it's just a blank line
                     *   with some whitespace
                     */
                    if (u_isnl(p, rem))
                    {
                        /*
                         *   end of line - this is a blank line, so we're at
                         *   the end of the value
                         */
                        break;
                    }
                    else
                    {
                        /*
                         *   non-whitespace - it's a continuation line;
                         *   replace the newline with a space, and continue
                         *   with the value text starting at this character
                         */
                        *dst++ = ' ';
                        continue;
                    }
                }
                else
                {
                    /*
                     *   the line doesn't start with whitespace, so it's not
                     *   a continuation line - we've reached the end of the
                     *   value
                     */
                    break;
                }
            }

            /* remove trailing spaces */
            while (dst > val_start && u_ishspace(*(dst-1)))
                --dst;

            /* save the name/value pair */
            tpl->tab.add_key(name_start, name_len, val_start, dst - val_start);
        }

        /* done with the buffer */
        th_free(buf);
    }

    /* remove the Close button from the active window */
    void unclose(DISPPARAMS *params, VARIANT *ret)
    {
        HWND hwnd = GetActiveWindow();
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style & ~WS_SYSMENU);
        SendMessage(hwnd, WM_NCPAINT, 1, 0);
    }

    struct browse_folder_ctx
    {
        browse_folder_ctx(const char *path) { this->path = path; }
        const char *path;
    };
    static int CALLBACK browse_folder_cb(
        HWND hwnd, UINT msg, LPARAM lpar, LPARAM data)
    {
        browse_folder_ctx *ctx = (browse_folder_ctx *)data;
        if (msg == BFFM_INITIALIZED && ctx != 0)
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)ctx->path);
        return 0;
    }

    /* browse for a path name */
    void choosePath(DISPPARAMS *params, VARIANT *ret)
    {
        /* presume failure (in which case we'll return null) */
        ret->vt = VT_NULL;

        /* run the folder browser dialog */
        LPMALLOC pmalloc;
        if (SUCCEEDED(SHGetMalloc(&pmalloc)))
        {
            /* get the current name setting, if any; otherwise use the pwd */
            char path[OSFNMAX] = "";
            char *oldpath = get_str_arg(params, 0);
            if (oldpath == 0 || oldpath[0] == '\0')
                GetCurrentDirectory(sizeof(path), path);
            else
                safe_strcpy(path, sizeof(path), oldpath);

            /* release the old path string */
            if (oldpath != 0)
                th_free(oldpath);

            /* show the dialog */
            char disp[MAX_PATH];
            browse_folder_ctx cbctx(path);
            BROWSEINFO bi;
            bi.hwndOwner = GetActiveWindow();
            bi.pidlRoot = 0;
            bi.pszDisplayName = disp;
            bi.lpszTitle = "Select Folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
            bi.lpfn = browse_folder_cb;
            bi.lParam = (LPARAM)&cbctx;
            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

            /* if they made a selection, return the result as a path string */
            if (pidl != 0)
            {
                /* convert the PIDL to a path */
                if (SHGetPathFromIDList(pidl, path))
                {
                    /* return the path name string */
                    ret->vt = VT_BSTR;
                    ret->bstrVal = bstr_from_ansi(path);
                }

                /* done with the PIDL */
                pmalloc->Free(pidl);
            }

            /* done with the shell allocator */
            pmalloc->Release();
        }

#if 0
        /* run the folder browser dialog */
        HWND hwnd = GetActiveWindow();
        if (CTadsDialogFolderSel::run_select_folder(
            hwnd, CTadsApp::get_app()->get_instance(),
            "Select folder", "Project Folder",
            path, sizeof(path), path))
        {
            /* return the path name string */
            ret->vt = VT_BSTR;
            ret->bstrVal = bstr_from_ansi(path);
        }
        else
        {
            ret->vt = VT_NULL;
        }
#endif
    }

    /* create the project */
    void createProject(DISPPARAMS *params, VARIANT *ret)
    {
        /* get the arguments */
        char *name = get_str_arg(params, 0);
        char *path = get_str_arg(params, 1);
        char *tplfile = get_str_arg(params, 2);
        char *title = get_str_arg(params, 3);
        char *author = get_str_arg(params, 4);
        char *email = get_str_arg(params, 5);
        char *desc = get_str_arg(params, 6);

        /* get up the bibliographic data object */
        CNameValTab biblio;
        biblio.add_key("$TITLE$", title);
        biblio.add_key("$AUTHOR$", author);
        biblio.add_key("$EMAIL$", email);
        biblio.add_key("$DESC$", desc);

        /* generate $HTMLDESC$ as the HTMLified $DESC$ */
        CStringBuf htmldesc;
        for (const char *src = desc ; *src != 0 ; )
        {
            /* get the next character of the source */
            char c = *src++;

            /* translate markup-significant characters */
            switch (c)
            {
            case '<':
                htmldesc.append("&lt;");
                break;

            case '>':
                htmldesc.append("&gt;");
                break;

            case '&':
                htmldesc.append("&amp;");
                break;

            default:
                htmldesc.append(&c, 1);
                break;
            }
        }

        /* add it */
        biblio.add_key("$HTMLDESC$", htmldesc.get());

        /* search for the template file */
        new_proj_tpl *tpl;
        for (tpl = tpl_head_ ; tpl != 0 ; tpl = tpl->nxt)
        {
            if (strcmp(tpl->fname.get(), tplfile) == 0)
                break;
        }

        /* if we found the template, create the project */
        if (tpl != 0)
        {
            /* pull out the directory path to the template file */
            char tpldir[OSFNMAX];
            os_get_path_name(tpldir, sizeof(tpldir), tpl->fname.get());

            /* create the game */
            dbgmain_->create_new_game(
                path, name, tpldir, tpl->rel_path.get(), &tpl->tab, &biblio);
        }

        /* free the argument strings */
        th_free(name);
        th_free(path);
        th_free(tplfile);
        th_free(title);
        th_free(author);
        th_free(email);
        th_free(desc);
    }

    /* get the number of project information entries */
    void getProjectInfoCount(DISPPARAMS *params, VARIANT *ret)
    {
        /* count the list entries */
        int cnt = 0;
        for (new_proj_tpl *tpl = tpl_head_ ; tpl != 0 ; tpl = tpl->nxt)
            ++cnt;

        /* return the count */
        ret->vt = VT_I4;
        ret->lVal = cnt;
    }

    /* get the number of project information entries */
    void getProjectInfo(DISPPARAMS *params, VARIANT *ret)
    {
        /* get the index */
        DWORD idx = get_long_arg(params, 0);

        /* find the entry */
        for (new_proj_tpl *tpl = tpl_head_ ; tpl != 0 && idx != 0 ;
             tpl = tpl->nxt, --idx) ;

        /* if we found the entry, build the result */
        if (tpl != 0)
        {
            /* build the json string */
            CStringBuf buf;
            buf.append("({");
            append_json_prop(buf, "id", tpl->fname.get());
            append_json_prop(buf, "name", tpl->tab.find_key("name"));
            append_json_prop(buf, "desc", tpl->tab.find_key("desc"));
            append_json_prop(buf, "sequence", tpl->tab.find_key("sequence"));
            buf.append("})");

            /* return the string */
            ret->vt = VT_BSTR;
            ret->bstrVal = bstr_from_ansi(buf.get());
        }
        else
        {
            /* no such index - return null */
            ret->vt = VT_NULL;
        }
    }

    void append_json_prop(CStringBuf &buf, const char *name,
                          const CNameValPair *val)
    {
        if (val != 0)
            append_json_prop(buf, name, val->val_.get());
    }

    void append_json_prop(CStringBuf &buf, const char *name, const char *val)
    {
        if (val != 0)
        {
            if (buf.getlen() != 0 && buf.get()[buf.getlen()-1] != '{')
                buf.append(",");
            buf.append(name);
            buf.append(":\"");
            for (;;)
            {
                const char *qu;
                for (qu = val ;
                     *qu != 0 && *qu != '"' && *qu != '\\' && *qu > 27 ;
                     ++qu) ;

                buf.append(val, qu - val);
                if (*qu == 0)
                {
                    break;
                }
                else if (*qu <= 27)
                {
                    char hex[8];
                    sprintf(hex, "\\%02x", *qu);
                    buf.append(hex);
                    val = qu + 1;
                }
                else
                {
                    buf.append("\\");
                    buf.append(qu, 1);
                    val = qu + 1;
                }
            }
            buf.append("\"");
        }
    }

    TADSDISP_DECL_MAP;
};

TADSDISP_MAP_BEGIN(NewProjectArgs)
TADSDISP_MAP(NewProjectArgs, unclose)
TADSDISP_MAP(NewProjectArgs, choosePath)
TADSDISP_MAP(NewProjectArgs, createProject)
TADSDISP_MAP(NewProjectArgs, getProjectInfoCount)
TADSDISP_MAP(NewProjectArgs, getProjectInfo)
TADSDISP_MAP_END;

/* run the New Game wizard */
void run_new_game_wiz(HWND owner, CHtmlSys_dbgmain *mainwin)
{
    /* create the arguments object */
    NewProjectArgs *args = new NewProjectArgs(mainwin);

    /* run the dialog */
    if (modal_html_dialog(owner, "newprojwiz.htm", 100, 32, args))
        MessageBox(owner, "Error loading the New Project dialog",
                   "TADS Workbench", MB_OK);

    /* done with the arguments */
    args->Release();
}


