#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32dbgpr.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32dbgpr.cpp - win32 debugger preferences dialog
Function

Notes

Modified
  03/14/98 MJRoberts  - Creation
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
 *   Owner-drawn font combo.  This combo shows proportional fonts in normal
 *   type and monospaced fonts in bold.
 */
class CFontCombo: public CTadsOwnerDrawnCombo
{
public:
    CFontCombo(int id, int has_auto) : CTadsOwnerDrawnCombo(id)
    {
        /* create a bold version of the base font */
        bold_font = CTadsApp::make_bold_font(
            (HFONT)GetStockObject(DEFAULT_GUI_FONT));

        /* remember whether we have "(Automatic)" as our first item */
        has_auto_ = has_auto;
    }

    /* draw our contents */
    virtual void draw_contents(HDC dc, RECT *item_rc, textchar_t *title,
                               DRAWITEMSTRUCT *dis)
    {
        RECT rc;

        /*
         *   The dialog box init_font_popup() routine will have stored the
         *   tmPitchAndFamily value for the font in the item data.  If that
         *   indicates a proportional font, draw in ordinary text; if it's
         *   monospaced, draw in bold.  (NB: The Windows constant
         *   TMPF_FIXED_PITCH is named the opposite of what it means - the
         *   font really is fixed-pitch if this bit is *zero*!)
         *
         *   Don't use bold if we're drawing the edit field portion; the
         *   boldface is just a key to the user to point out list items that
         *   correspond to fixed-pitch fonts, so we only want it in the
         *   listbox portion.
         *
         *   Also, don't use bold for the special "(Automatic)" pseudo-item,
         *   if we have one - if we do, it's always at index 0.
         */
        if (!(has_auto_ && dis->itemID == 0)
            && !(dis->itemState & ODS_COMBOBOXEDIT)
            && (SendMessage(dis->hwndItem, CB_GETITEMDATA, dis->itemID, 0)
                & TMPF_FIXED_PITCH) == 0)
            SelectObject(dc, bold_font);

        /* draw the title string in the selected font */
        rc = *item_rc;
        rc.left += 2;
        DrawText(dc, title, -1, &rc, DT_SINGLELINE);
    }

protected:
    /* our bold font */
    HFONT bold_font;

    /* do we have an "(Automatic)" item as our first element? */
    int has_auto_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Browse for a file to load or save.  Fills in the string buffer with the
 *   new filename and returns true if successful; returns false if the user
 *   cancels the dialog without selecting anything.  If 'load' is true,
 *   we'll show an "open file" dialog; otherwise, we'll show a "save file"
 *   dialog.
 */
static int browse_file(char *fname, size_t fname_size, HWND parent,
                       const char *filter, const char *prompt,
                       const char *defext, DWORD flags,
                       int load, const char *rel_base_path)
{
    OPENFILENAME5 info;
    char dir[256];
    int result;

    /* if there's a filename, start out in the file's directory */
    if (fname[0] != '\0')
    {
        /* there's a file - get its path */
        os_get_path_name(dir, sizeof(dir), fname);

        /* start out in the file's directory */
        info.lpstrInitialDir = dir;
    }
    else
    {
        /* no file yet - start off in the default open-file directory */
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    }

    /* set up the dialog definition structure */
    info.hwndOwner = parent;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    info.nMaxFile = fname_size;
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrTitle = prompt;
    info.Flags = flags | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = defext;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);

    /* ask for the file */
    if (load)
        result = GetOpenFileName((OPENFILENAME *)&info);
    else
        result = GetSaveFileName((OPENFILENAME *)&info);

    /* if that succeeded, do some follow-up work */
    if (result)
    {
        /* update the open-file directory */
        CTadsApp::get_app()->set_openfile_dir(fname);

        /*
         *   if they gave us a relative base path, return the result filename
         *   as a path relative to the base path, if possible
         */
        if (rel_base_path != 0)
        {
            char buf[OSFNMAX];

            /* make a safe copy of the original filename */
            strcpy(buf, fname);

            /* get the path relative to the base path */
            os_get_rel_path(fname, fname_size, rel_base_path, buf);
        }
    }

    /* return the result from the dialog */
    return result;
}

/*
 *   Browse for a file to load or save.  Fills in the given dialog edit
 *   control with the selected name.
 */
static int browse_file(HWND handle, CTadsDialogPropPage *pg,
                       const char *filter, const char *prompt,
                       const char *defext, int fld_id, DWORD flags,
                       int load, const char *rel_base_path)
{
    int result;
    char fname[MAX_PATH];

    /* get the original filename from the field */
    GetWindowText(GetDlgItem(handle, fld_id), fname, sizeof(fname));

    /* run the dialog */
    result = browse_file(fname, sizeof(fname), handle, filter, prompt,
                         defext, flags, load, rel_base_path);

    /* if they selected a file, copy it to the field */
    if (result != 0)
    {
        /* copy the new filename back into the text field */
        SetWindowText(GetDlgItem(handle, fld_id), fname);

        /* set the change flag */
        if (pg != 0)
            pg->set_changed(TRUE);
    }

    /* return the result from the dialog */
    return result;
}

/*
 *   Browse for a folder
 */
static int browse_folder(HWND parent, const char *prompt, const char *title,
                         char *folder_buf, size_t folder_buf_len,
                         const char *start_folder, int project_relative)
{
    /* get the project folder, in case we need it */
    char projdir[OSFNMAX];
    CHtmlSys_dbgmain::get_main_win()->get_project_dir(
        projdir, sizeof(projdir));

    /* presume we won't have a starting folder */
    char startbuf[OSFNMAX];

    /*
     *   if the starting folder is in relative notation, get the full name
     *   relative to the folder path
     */
    if (start_folder != 0 && !os_is_file_absolute(start_folder))
    {
        os_build_full_path(startbuf, sizeof(startbuf), projdir, start_folder);
        start_folder = startbuf;
    }

    /* run the select folder dialog */
    if (CTadsDialogFolderSel2::run_select_folder(
        parent, CTadsApp::get_app()->get_instance(),
        prompt, title, folder_buf, folder_buf_len, start_folder, 0, 0))
    {
        /* change the path to be relative to the project folder if desired */
        if (project_relative)
        {
            /* rewrite the path relative to the project folder */
            char abspath[OSFNMAX];
            safe_strcpy(abspath, sizeof(abspath), folder_buf);
            os_get_rel_path(folder_buf, folder_buf_len, projdir, abspath);
        }

        /* success */
        return TRUE;
    }

    /* canceled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Basic debugger preferences property page
 */
class CHtmlDialogDebugPref: public CTadsDialogPropPage
{
public:
    CHtmlDialogDebugPref(CHtmlSys_dbgmain *mainwin,
                         CHtmlDebugConfig *config,
                         int *inited)
    {
        mainwin_ = mainwin;
        config_ = config;
        inited_ = inited;
    }

protected:
    /* initialize */
    void init()
    {
        /* if we haven't yet done so, center the dialog */
        if (!*inited_)
        {
            /* center the parent dialog */
            center_dlg_win(GetParent(handle_));

            /* turn off the context-help style in the parent window */
            SetWindowLong(GetParent(handle_), GWL_EXSTYLE,
                          GetWindowLong(GetParent(handle_), GWL_EXSTYLE)
                          & ~WS_EX_CONTEXTHELP);

            /* note that we've done this so that other pages don't */
            *inited_ = TRUE;
        }

        /* inherit default */
        CTadsDialog::init();
    }

    /* load a text field from a configuration string */
    void load_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id, const textchar_t *defval = 0)
    {
        load_field(var, subvar, field_id, defval, FALSE);
    }

    /* load a field, converting to a file path relative to the project */
    void load_field_rel(const textchar_t *var, const textchar_t *subvar,
                        int field_id, const textchar_t *defval = 0)
    {
        load_field(var, subvar, field_id, defval, TRUE);
    }

    /* load a text field, optionally converting to relative path notation */
    void load_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id, const textchar_t *defval, int rel)
    {
        textchar_t buf[1024];
        const textchar_t *val;

        /* get the configuration value, applying the default if not found */
        if (!config_->getval(var, subvar, 0, buf, sizeof(buf)))
            val = buf;
        else if (defval != 0)
            val = defval;
        else
            val = "";

        /* convert to relative notation if desired */
        if (rel)
        {
            /* get the project directory */
            char proj_dir[OSFNMAX];
            mainwin_->get_project_dir(proj_dir, sizeof(proj_dir));

            /*
             *   if the value is in absolute path notation, and it's in the
             *   project folder, convert to relative notation
             */
            if (os_is_file_absolute(val)
                && os_is_file_in_dir(val, proj_dir, TRUE, TRUE))
            {
                /* make a copy of the original value */
                char buf2[1024];
                strcpy(buf2, val);

                /* convert to relative notation */
                os_get_rel_path(buf, sizeof(buf), proj_dir, buf2);
            }
        }

        /* set the field text */
        SetWindowText(GetDlgItem(handle_, field_id), val);
    }

    /* save a field's value into a configuration string */
    void save_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id, int is_filename)
    {
        save_field(var, subvar, field_id, is_filename, FALSE);
    }

    /* save a field's value, converting from relative path notation */
    void save_field_rel(const textchar_t *var, const textchar_t *subvar,
                        int field_id, int is_filename)
    {
        save_field(var, subvar, field_id, is_filename, TRUE);
    }

    /* save a field, optionally converting from relative path notation */
    void save_field(const textchar_t *var, const textchar_t *subvar,
                    int field_id, int is_filename, int rel)
    {
        /* get the field value */
        textchar_t buf[1024];
        GetWindowText(GetDlgItem(handle_, field_id), buf, sizeof(buf));

        /* if desired, convert to an absolute path if it's not already */
        if (rel && !os_is_file_absolute(buf))
        {
            /* get the project directory */
            char proj_dir[OSFNMAX];
            mainwin_->get_project_dir(proj_dir, sizeof(proj_dir));

            /* make a copy of the value */
            char buf2[1024];
            strcpy(buf2, buf);

            /* build the full name */
            os_build_full_path(buf, sizeof(buf), proj_dir, buf2);
        }

        /* if this is a filename, make sure we obey DOS-style naming rules */
        if (is_filename)
        {
            char *p;

            /* scan the filename */
            for (p = buf ; *p != '\0'; ++p)
            {
                /* convert forward slashes to backslashes */
                if (*p == '/')
                    *p = '\\';
            }
        }

        /* store the value in the configuration */
        config_->setval(var, subvar, 0, buf);
    }

    /* load a multi-line field from a list of configuration strings */
    void load_multiline_fld(const textchar_t *var, const textchar_t *subvar,
                            int field_id, int is_relative);

    /* save a multi-line field into a list of configuration strings */
    void save_multiline_fld(const textchar_t *var, const textchar_t *subvar,
                            int field_id, int is_filename, int is_relative);

    /* check for changes to a multiline field */
    int multiline_fld_changed(const textchar_t *var, const textchar_t *subvar,
                              int field_id);

    /* load a checkbox */
    void load_ckbox(const textchar_t *var, const textchar_t *subvar,
                    int ctl_id, int default_val)
    {
        int val;

        /* get the current value */
        if (config_->getval(var, subvar, &val))
            val = default_val;

        /* set the checkbox */
        CheckDlgButton(handle_, ctl_id, val ? BST_CHECKED : BST_UNCHECKED);
    }

    /* save a checkbox */
    void save_ckbox(const textchar_t *var, const textchar_t *subvar,
                    int ctl_id)
    {
        int val;

        /* get the checkbox value */
        val = (IsDlgButtonChecked(handle_, ctl_id) == BST_CHECKED);

        /* save it */
        config_->setval(var, subvar, val);
    }

    /* browse for a file */
    int browse_file(const char *filter, const char *prompt,
                    const char *defext, int fld_id, DWORD flags, int load,
                    int keep_rel)
    {
        /* in case we want a relative path, get the project directory */
        char rel_base[OSFNMAX];
        mainwin_->get_project_dir(rel_base, sizeof(rel_base));

        /* do the normal file browsing */
        return ::browse_file(handle_, this, filter, prompt, defext,
                             fld_id, flags, load, keep_rel ? rel_base : 0);
    }

    /* configuration object */
    CHtmlDebugConfig *config_;

    /* main window */
    CHtmlSys_dbgmain *mainwin_;

    /*
     *   flag: we've initialized the parent dialog's position; we keep a
     *   pointer to a single flag common to all pages, so that, whichever
     *   page comes up first, we only initialize the parent dialog a
     *   single time
     */
    int *inited_;
};

/*
 *   load a multi-line field from a list of configuration strings
 */
void CHtmlDialogDebugPref::load_multiline_fld(
    const textchar_t *var, const textchar_t *subvar,
    int field_id, int is_relative)
{
    /* get the field handle */
    HWND fld = GetDlgItem(handle_, field_id);

    /* get the number of strings to load */
    int cnt = config_->get_strlist_cnt(var, subvar);

    /* get the project directory in case we're doing a relative conversion */
    char proj_dir[OSFNMAX];
    mainwin_->get_project_dir(proj_dir, sizeof(proj_dir));

    /* read the strings from the configuration */
    for (int i = 0 ; i < cnt ; ++i)
    {
        textchar_t buf[OSFNMAX];

        /* get the next string */
        if (!config_->getval(var, subvar, i, buf, sizeof(buf)))
        {
            /* if we want it in relative notation, convert it */
            if (is_relative
                && os_is_file_absolute(buf)
                && os_is_file_in_dir(buf, proj_dir, TRUE, TRUE))
            {
                /* make a copy of the absolute path */
                textchar_t buf2[OSFNMAX];
                strcpy(buf2, buf);

                /* get the relative path */
                os_get_rel_path(buf, sizeof(buf), proj_dir, buf2);
            }

            /* append this string to the field */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

            /* append a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
        }
    }
}

/*
 *   save a multi-line field into a list of configuration strings
 */
void CHtmlDialogDebugPref::save_multiline_fld(
    const textchar_t *var, const textchar_t *subvar,
    int field_id, int is_filename, int is_relative)
{
    int i;
    int idx;
    int cnt;
    HWND fld;

    /* clear the old resource list */
    config_->clear_strlist(var, subvar);

    /* get the field handle */
    fld = GetDlgItem(handle_, field_id);

    /* get the number of items in the field */
    cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

    /* save the items in the field */
    for (i = 0, idx = 0 ; i < cnt ; ++i)
    {
        size_t len;
        textchar_t buf[OSFNMAX];

        /* get this line */
        *(WORD *)buf = (WORD)sizeof(buf) - 1;
        len = (size_t)SendMessage(fld, EM_GETLINE, (WPARAM)i, (LPARAM)buf);

        /* null-terminate the buffer */
        buf[len] = '\0';

        /* convert to absolute path notation if desired */
        if (is_relative && !os_is_file_absolute(buf))
        {
            /* get the project folder */
            char proj_dir[OSFNMAX];
            mainwin_->get_project_dir(proj_dir, sizeof(proj_dir));

            /* make a copy of the value */
            char buf2[OSFNMAX];
            strcpy(buf2, buf);

            /* build the full name */
            os_build_full_path(buf, sizeof(buf), proj_dir, buf2);
        }

        /* if this is a filename, convert to DOS naming rules */
        if (is_filename)
        {
            /* scan the filename */
            for (char *p = buf ; *p != '\0' ; ++p)
            {
                /* convert forward slashes to backslashes */
                if (*p == '/')
                    *p = '\\';
            }
        }

        /* if there's anything there, save it */
        if (len > 0)
            config_->setval(var, subvar, idx++, buf);
    }
}

/*
 *   check for changes (from the saved configuration) to a multi-line field
 *   into a list of configuration strings
 */
int CHtmlDialogDebugPref::multiline_fld_changed(const textchar_t *var,
                                                const textchar_t *subvar,
                                                int field_id)
{
    int i;
    int cfg_cnt;
    int fld_cnt;
    HWND fld;
    char *matched;
    int mismatch;

    /* get the number of strings saved in the configuration */
    cfg_cnt = config_->get_strlist_cnt(var, subvar);

    /* get the field handle */
    fld = GetDlgItem(handle_, field_id);

    /* get the number of items in the field */
    fld_cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

    /* allocate an array of integers: one match per config item */
    matched = (char *)th_malloc(cfg_cnt);

    /* initialize each config item match flag to "not matched" */
    memset(matched, 0, cfg_cnt);

    /*
     *   scan the lines in the field: check to see if we can match up each
     *   line of the field to a value in the configuration
     */
    for (mismatch = FALSE, i = 0 ; i < fld_cnt ; ++i)
    {
        size_t len;
        textchar_t fld_buf[OSFNMAX];
        int j;
        int fld_matched;

        /* get this line from the field */
        *(WORD *)fld_buf = (WORD)sizeof(fld_buf) - 1;
        len = (size_t)SendMessage(fld, EM_GETLINE, (WPARAM)i,
                                  (LPARAM)fld_buf);

        /* null-terminate the buffer */
        fld_buf[len] = '\0';

        /* if there's nothing there, skip it */
        if (len <= 0)
            continue;

        /* scan the config items we haven't matched so far for a match */
        for (fld_matched = FALSE, j = 0 ; j < cfg_cnt && !fld_matched ; ++j)
        {
            textchar_t cfg_buf[OSFNMAX];

            /*
             *   if this config item has been previously matched to a
             *   different field item, skip it - we only want to match each
             *   item once
             */
            if (matched[j])
                continue;

            /* get this config item */
            if (!config_->getval(var, subvar, i, cfg_buf, sizeof(cfg_buf)))
            {
                /* if this one matches, so note */
                if (strcmp(cfg_buf, fld_buf) == 0)
                {
                    /* note that we found a field match */
                    fld_matched = TRUE;

                    /* note that this config item matched */
                    matched[j] = TRUE;

                    /* no need to keep scanning config items */
                    break;
                }
            }
        }

        /* if we didn't match the field, we have a mismatch */
        if (!fld_matched)
        {
            /* note the mismatch */
            mismatch = TRUE;

            /* no need to keep scanning field items */
            break;
        }
    }

    /*
     *   if we didn't already find a mismatch among the field items, check
     *   for any unmatched config items
     */
    if (!mismatch)
    {
        /* scan each config item for a mismatch */
        for (i = 0 ; i < cfg_cnt ; ++i)
        {
            /* check for a mismatch */
            if (!matched[i])
            {
                /* note that we have a mismatch */
                mismatch = TRUE;

                /* no need to keep looking */
                break;
            }
        }
    }

    /* we're done with the match array */
    th_free(matched);

    /* return the change indication */
    return mismatch;
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for start-up options
 */
class CHtmlDialogDebugStart: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugStart(CHtmlSys_dbgmain *mainwin,
                          CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /*
     *   Note: We store the current start-up mode in the configuration
     *   variable "startup:action" as an integer value, as follows:
     *
     *   1 = show welcome dialog
     *.  2 = open most recent project
     *.  3 = start with no project
     */
};

/*
 *   initialize
 */
void CHtmlDialogDebugStart::init()
{
    int mode;

    /* inherit default processing */
    CHtmlDialogDebugPref::init();

    /*
     *   get the current mode from the configuration, defaulting to "show
     *   welcome dialog" if there's no setting in the configuration
     */
    if (config_->getval("startup", "action", &mode))
        mode = 1;

    /* set the appropriate radio button */
    CheckRadioButton(handle_, IDC_RB_START_WELCOME, IDC_RB_START_NONE,
                     IDC_RB_START_WELCOME + mode - 1);
}

/*
 *   process a command
 */
int CHtmlDialogDebugStart::do_command(WPARAM cmd, HWND ctl)
{
    /* check which control is involved */
    switch(LOWORD(cmd))
    {
    case IDC_RB_START_WELCOME:
    case IDC_RB_START_RECENT:
    case IDC_RB_START_NONE:
        /* note the change */
        set_changed(TRUE);
        return TRUE;
    }

    /* we didn't handle it - inherit the default handling */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

int CHtmlDialogDebugStart::do_notify(NMHDR *nm, int ctl)
{
    int mode;

    /* check what we're doing */
    switch(nm->code)
    {
    case PSN_APPLY:
        /* figure out which mode is selected in the radio buttons */
        if (IsDlgButtonChecked(handle_, IDC_RB_START_WELCOME))
            mode = 1;
        else if (IsDlgButtonChecked(handle_, IDC_RB_START_RECENT))
            mode = 2;
        else
            mode = 3;

        /* save the mode in the configuration data */
        config_->setval("startup", "action", mode);

        /* done */
        return TRUE;
    }

    /* inherit the default handling */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   Indent and tabs dialog
 */
class CHtmlDialogDebugIndent: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugIndent(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        static const textchar_t *tabsizes[] =
            { "1", "2", "3", "4", "5", "6", "7", "8", "10", "16", 0 };
        static const textchar_t *autostyles[] =
            { "Off", "Same as previous line", "Syntax-based", 0 };
        char buf[32];
        int idx;
        const textchar_t **p;
        int tab_size, indent_size, use_tabs, format_comments, autoindent;

        /* load up the suggested tab and indent sizes list */
        for (idx = 0, p = tabsizes ; *p != 0 ; ++p, ++idx)
        {
            SendMessage(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE),
                        CB_INSERTSTRING, idx, (LPARAM)*p);
            SendMessage(GetDlgItem(handle_, IDC_CBO_DBGINDENTSIZE),
                        CB_INSERTSTRING, idx, (LPARAM)*p);
        }

        /* get the tab spacing */
        if (config_->getval("src win options", "tab size", &tab_size))
            tab_size = 8;

        /* and the indent spacing */
        if (config_->getval("src win options", "indent size", &indent_size))
            indent_size = 4;

        /* get the indent-with-tabs setting */
        if (config_->getval("src win options", "use tabs", &use_tabs))
            use_tabs = FALSE;

        /* set the initial tab size field */
        sprintf(buf, "%d", tab_size);
        SetWindowText(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE), buf);

        /* set the indent-with-tabs checkbox */
        CheckDlgButton(handle_, IDC_CK_USE_TABS,
                       use_tabs ? BST_CHECKED : BST_UNCHECKED);

        /* ...and the indent size field */
        sprintf(buf, "%d", indent_size);
        SetWindowText(GetDlgItem(handle_, IDC_CBO_DBGINDENTSIZE), buf);

        /* load the auto-indent options */
        for (idx = 0, p = autostyles ; *p != 0 ; ++p, ++idx)
            SendMessage(GetDlgItem(handle_, IDC_CB_AUTOINDENT),
                        CB_INSERTSTRING, idx, (LPARAM)*p);

        /* set the current auto-indent setting */
        if (config_->getval("src win options", "autoindent", &autoindent))
            autoindent = 2;

        /* select the combo item */
        SendMessage(GetDlgItem(handle_, IDC_CB_AUTOINDENT),
                    CB_SETCURSEL, autoindent, 0);

        /* get the initial auto-format-comments checkbox setting */
        if (config_->getval("src win options", "autoformat comments",
                            &format_comments))
            format_comments = TRUE;

        /* check the button */
        CheckDlgButton(handle_, IDC_CK_FORMAT_COMMENTS,
                       format_comments ? BST_CHECKED : BST_UNCHECKED);

        /* inherit default */
        CHtmlDialogDebugPref::init();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch(LOWORD(cmd))
        {
        case IDC_CK_USE_TABS:
        case IDC_CK_FORMAT_COMMENTS:
            /* note the change */
            set_changed(TRUE);
            return TRUE;

        case IDC_CBO_DBGTABSIZE:
        case IDC_CBO_DBGINDENTSIZE:
        case IDC_CB_AUTOINDENT:
            switch(HIWORD(cmd))
            {
            case CBN_EDITCHANGE:
            case CBN_SELCHANGE:
                /* note the change to the value */
                set_changed(TRUE);
                break;

            default:
                /* ignore other notifications */
                break;
            }
            return TRUE;

        default:
            /* inherit default behavior */
            return CHtmlDialogDebugPref::do_command(cmd, ctl);
        }
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        char buf[256];
        int idx;

        switch(nm->code)
        {
        case PSN_APPLY:
            /* save the tab size */
            GetWindowText(GetDlgItem(handle_, IDC_CBO_DBGTABSIZE),
                          buf, sizeof(buf));
            config_->setval("src win options", "tab size",
                            (int)get_atol(buf));

            /* save the indent size */
            GetWindowText(GetDlgItem(handle_, IDC_CBO_DBGINDENTSIZE),
                          buf, sizeof(buf));
            config_->setval("src win options", "indent size",
                            (int)get_atol(buf));

            /* save the use-tabs setting */
            config_->setval("src win options", "use tabs",
                            IsDlgButtonChecked(handle_, IDC_CK_USE_TABS)
                            == BST_CHECKED);

            /* save the autoindent setting */
            idx = SendMessage(GetDlgItem(handle_, IDC_CB_AUTOINDENT),
                              CB_GETCURSEL, 0, 0);
            config_->setval("src win options", "autoindent", idx);

            /* save the auto-format-comments setting */
            config_->setval(
                "src win options", "autoformat comments",
                IsDlgButtonChecked(handle_, IDC_CK_FORMAT_COMMENTS)
                == BST_CHECKED);

            /* tell the main window that we've updated the preferences */
            mainwin_->on_update_srcwin_options();

            /* done */
            return TRUE;

        default:
            /* inherit default behavior */
            return CHtmlDialogDebugPref::do_notify(nm, ctl);
        }
    }

private:
    /*
     *   auto-indent setting IDs - these are the values stored in the config
     *   file under "src win options:autoindent"
     */
    static const textchar_t *autoindent_ids_[];
};

/* ------------------------------------------------------------------------ */
/*
 *   Line wrapping dialog
 */
class CHtmlDialogDebugWrap: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugWrap(CHtmlSys_dbgmain *mainwin,
                         CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        char buf[32];

        /* get the right-margin guide flag */
        CheckDlgButton(
            handle_, IDC_CK_MARGIN_GUIDE,
            config_->getval_int(
                "src win options", "right margin guide", FALSE));

        /* get the auto-wrap flag */
        CheckDlgButton(
            handle_, IDC_CK_AUTO_WRAP,
            config_->getval_int(
                "src win options", "auto wrap", FALSE));

        /* get the right margin column */
        sprintf(buf, "%d",
                config_->getval_int(
                    "src win options", "right margin column", 80));
        SetDlgItemText(handle_, IDC_FLD_MARGIN, buf);

        /* inherit default */
        CHtmlDialogDebugPref::init();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch(LOWORD(cmd))
        {
        case IDC_CK_AUTO_WRAP:
        case IDC_CK_MARGIN_GUIDE:
            /* note the change */
            set_changed(TRUE);
            return TRUE;

        case IDC_FLD_MARGIN:
            /* if this is a text change, note the update */
            if (HIWORD(cmd) == EN_CHANGE)
                set_changed(TRUE);
            return TRUE;
        }

        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        char buf[256];

        switch(nm->code)
        {
        case PSN_APPLY:
            /* save the right-margin guide setting */
            config_->setval("src win options", "right margin guide",
                            IsDlgButtonChecked(handle_, IDC_CK_MARGIN_GUIDE)
                            == BST_CHECKED);

            /* save the auto-wrap setting */
            config_->setval("src win options", "auto wrap",
                            IsDlgButtonChecked(handle_, IDC_CK_AUTO_WRAP)
                            == BST_CHECKED);

            /* save the right margin column setting */
            GetDlgItemText(handle_, IDC_FLD_MARGIN, buf, sizeof(buf));
            config_->setval("src win options", "right margin column",
                            atoi(buf));

            /* tell the main window that we've updated the preferences */
            mainwin_->on_update_srcwin_options();

            /* done */
            return TRUE;

        default:
            /* inherit default behavior */
            return CHtmlDialogDebugPref::do_notify(nm, ctl);
        }
    }

private:
    /*
     *   auto-indent setting IDs - these are the values stored in the config
     *   file under "src win options:autoindent"
     */
    static const textchar_t *autoindent_ids_[];
};

/* ------------------------------------------------------------------------ */
/*
 *   base class for a dialog with font settings
 */
class DialogWithFonts
{
protected:
    DialogWithFonts()
    {
        /* no font yet */
        font_ = 0;
    }

    /* owner-draw a sample box */
    void draw_sample(DRAWITEMSTRUCT *dis, const CHtmlFontDesc *style,
                     const CHtmlFontDesc *sel_style)
    {
        RECT rc;
        HWND hwnd = dis->hwndItem;
        HDC dc = dis->hDC;
        HBRUSH br = CreateSolidBrush(
            HTML_color_to_COLORREF(
                style->default_bgcolor ? bgdefault_ : style->bgcolor));

        /* get our area */
        GetClientRect(hwnd, &rc);

        /* draw the background and border */
        FillRect(dc, &rc, br);
        FrameRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        /* draw the text */
        if (font_ != 0)
        {
            char buf[256];

            /* select the font and color */
            HGDIOBJ oldfont = font_->select(dc);
            COLORREF oldfg = SetTextColor(
                dc, HTML_color_to_COLORREF(
                    style->default_color ? fgdefault_ : style->color));

            /* draw the sample text */
            GetWindowText(hwnd, buf, sizeof(buf));
            DrawText(dc, buf, -1, &rc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            /* unselect the sample font and color */
            font_->unselect(dc, oldfont);
            SetTextColor(dc, oldfg);
        }

        /* clean up */
        DeleteObject(br);
    }

    /* load the standard color list into a combo */
    void load_color_list(HWND ctl)
    {
        static const struct
        {
            const char *name;
            COLORREF color;
        }
        colors[] =
        {
            { "Automatic", 0xFFFFFFFF },
            { "Black", RGB(0x00,0x00,0x00) },
            { "White", RGB(0xff,0xff,0xff) },
            { "Red", RGB(0xFF,0x00,0x00) },
            { "Green", RGB(0x00,0x80,0x00) },
            { "Blue", RGB(0x00,0x00,0xFF) },
            { "Yellow", RGB(0xFF,0xFF,0x00) },
            { "Gray", RGB(0x80,0x80,0x80) },
            { "Silver", RGB(0xEF,0xEF,0xEF) },
            { "Light Gray", RGB(0xD3,0xD3,0xD3) },
            { "Medium Gray", RGB(0xA9,0xA9,0xA9) },
            { "Dark Gray", RGB(0x33,0x33,0x33) },
            { "Salmon", RGB(0xFF,0x66,0x66) },
            { "Maroon", RGB(0x80,0x00,0x00) },
            { "Brown", RGB(0x99,0x33,0x00) },
            { "Dark Green", RGB(0x00,0x64,0x00) },
            { "Olive", RGB(0x80,0x80,0x00) },
            { "Teal", RGB(0x00,0x80,0x80) },
            { "Magenta", RGB(0xFF,0x00,0xFF) },
            { "Pink", RGB(0xFF,0xEF,0xFF) },
            { "Dark Red", RGB(0xC0,0x00,0x00) },
            { "Plum", RGB(0x80,0x00,0x40) },
            { "Violet", RGB(0x80,0x00,0xFF) },
            { "Purple", RGB(0x80,0x00,0x80) },
            { "Aquamarine", RGB(0x7F,0xFF,0xD4) },
            { "Light Yellow", RGB(0xFF,0xFF,0xD0) },
            { "Gold", RGB(0xFF,0xD0,0x00) },
            { "Light Blue", RGB(0xEF,0xEF,0xFF) },
            { "Gray Blue", RGB(0xC0,0xC0,0xE0) },
            { "Dark Blue", RGB(0x00,0x00,0x80) },
            { "Cobalt", RGB(0x33,0x66,0xFF) },
            { "Cyan", RGB(0x00,0xFF,0xFF) },
            { "Other...", 0xFFFFFFFE }
        };

        /* add the colors */
        for (int i = 0 ; i < countof(colors) ; ++i)
        {
            /* add the item */
            int idx = SendMessage(ctl, CB_ADDSTRING,
                                  0, (LPARAM)colors[i].name);

            /* set its data item to the colorref */
            SendMessage(ctl, CB_SETITEMDATA, idx, (LPARAM)colors[i].color);
        }
    }

    /* our font for drawing the sample */
    CTadsFont *font_;

    /* current color settings */
    HTML_color_t fg_;
    HTML_color_t bg_;
    HTML_color_t selfg_;
    HTML_color_t selbg_;

    /* our default foreground and background colors */
    HTML_color_t fgdefault_;
    HTML_color_t bgdefault_;
    HTML_color_t selfgdefault_;
    HTML_color_t selbgdefault_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for source window options
 */
class CHtmlDialogDebugSrcwin: public CHtmlDialogDebugPref,
    public DialogWithFonts
{
public:
    CHtmlDialogDebugSrcwin(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited), DialogWithFonts()
    {
        /* set up the color combos */
        controls_.add(fgcombo_ = new CColorComboPropPage(
            0, IDC_CB_FGCOLOR, &fg_, &fgdefault_,
            cust_colors_, this));
        controls_.add(bgcombo_ = new CColorComboPropPage(
            0, IDC_CB_BGCOLOR, &bg_, &bgdefault_,
            cust_colors_, this));
        controls_.add(selfgcombo_ = new CColorComboPropPage(
            0, IDC_CB_SELFGCOLOR, &selfg_, &selfgdefault_,
            cust_colors_, this));
        controls_.add(selbgcombo_ = new CColorComboPropPage(
            0, IDC_CB_SELBGCOLOR, &selbg_, &selbgdefault_,
            cust_colors_, this));

        /* set up the font combo */
        controls_.add(new CFontCombo(IDC_CBO_DBGFONT, FALSE));

        /* initialize the global style settings */
        mainwin->get_global_style(&style_, &sel_style_);

        /* apply defaults */
        if (style_.face[0] == '\0')
            strcpy(style_.face, "Courier New");
        if (style_.pointsize == 0)
            style_.pointsize = 10;
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

    /* global style settings */
    CHtmlFontDesc style_;
    CHtmlFontDesc sel_style_;

    /* custom colors */
    COLORREF cust_colors_[16];

protected:
    /* handle owner drawing */
    int draw_item(int ctl, DRAWITEMSTRUCT *dis)
    {
        switch (ctl)
        {
        case IDC_ST_SAMPLE:
            /* draw the text sample box */
            draw_sample(dis, &style_, &sel_style_);
            return TRUE;
        }

        /* not handled */
        return CHtmlDialogDebugPref::draw_item(ctl, dis);
    }

    /* update the sample */
    void update_sample()
    {
        /* create a font from our current style */
        font_ = CTadsApp::get_app()->get_font(
            style_.face, style_.pointsize, FALSE, FALSE, FALSE,
            RGB(0,0,0), FALSE, RGB(0,0,0), FALSE, 0);

        /* invalidate the sample area */
        InvalidateRect(GetDlgItem(handle_, IDC_ST_SAMPLE), 0, TRUE);
    }

    /* our color combo boxes */
    CColorComboPropPage *fgcombo_, *bgcombo_, *selfgcombo_, *selbgcombo_;
};

/*
 *   Initialize
 */
void CHtmlDialogDebugSrcwin::init()
{
    textchar_t buf[256];
    const textchar_t **p;
    int idx;
    static const textchar_t *fontsizes[] =
    {
        "4", "5", "6", "8", "9", "10", "11", "12", "14", "16", "18",
        "20", "24", "28", "36", 0
    };

    /* get the custom colors from the preferences */
    if (config_->getval_bytes("src win options", "custom colors",
                              cust_colors_, sizeof(cust_colors_)))
        memset(cust_colors_, 0, sizeof(cust_colors_));

    /* get the default system colors */
    fgdefault_ = COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOWTEXT));
    bgdefault_ = COLORREF_to_HTML_color(GetSysColor(COLOR_WINDOW));
    selfgdefault_ = COLORREF_to_HTML_color(GetSysColor(COLOR_HIGHLIGHTTEXT));
    selbgdefault_ = COLORREF_to_HTML_color(GetSysColor(COLOR_HIGHLIGHT));

    /* set the current colors */
    fg_ = (style_.default_color ? 0xFFFFFFFF : style_.color);
    bg_ = (style_.default_bgcolor ? 0xFFFFFFFF : style_.bgcolor);
    selfg_ = (sel_style_.default_color ? 0xFFFFFFFF : sel_style_.color);
    selbg_ = (sel_style_.default_bgcolor ? 0xFFFFFFFF : sel_style_.bgcolor);

    /* load the color list */
    load_color_list(GetDlgItem(handle_, IDC_CB_FGCOLOR));
    load_color_list(GetDlgItem(handle_, IDC_CB_BGCOLOR));
    load_color_list(GetDlgItem(handle_, IDC_CB_SELFGCOLOR));
    load_color_list(GetDlgItem(handle_, IDC_CB_SELBGCOLOR));

    /* select the current colors in the combos */
    fgcombo_->select_by_color();
    bgcombo_->select_by_color();
    selfgcombo_->select_by_color();
    selbgcombo_->select_by_color();

    /* set up the font combo with a list of the available fonts */
    init_font_popup(IDC_CBO_DBGFONT, TRUE, TRUE, style_.face, TRUE,
                    oshtml_charset_id_t(ANSI_CHARSET, CP_ACP).charset);

    /* set the initial font size field */
    sprintf(buf, "%d", style_.pointsize);
    SetWindowText(GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE), buf);

    /* load up the suggested font sizes list */
    for (idx = 0, p = fontsizes ; *p != 0 ; ++p, ++idx)
        SendMessage(GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE),
                    CB_INSERTSTRING, idx, (LPARAM)*p);

    /* set the "override scintilla selection colors" checkbox */
    CheckDlgButton(handle_, IDC_CK_OVERRIDESCISEL,
                   config_->getval_int(
                       "src win options", "override sci sel colors", FALSE)
                   ? BST_CHECKED : BST_UNCHECKED);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

int CHtmlDialogDebugSrcwin::do_command(WPARAM cmd, HWND ctl)
{
    char buf[64];

    switch(LOWORD(cmd))
    {
    case IDC_CBO_DBGFONT:
        /* check for a change to the font name setting */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            /* get the new font */
            GetDlgItemText(handle_, IDC_CBO_DBGFONT,
                           style_.face, sizeof(style_.face));

            /* update the sample text */
            update_sample();

            /* note the change */
            set_changed(TRUE);
        }
        return TRUE;

    case IDC_CK_OVERRIDESCISEL:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    case IDC_CBO_DBGFONTSIZE:
        /* check for a change to the font size */
        if (HIWORD(cmd) != CBN_SELCHANGE && HIWORD(cmd) != CBN_EDITCHANGE)
            break;

        /* get the new setting */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            /* get the new selection */
            HWND cbo = GetDlgItem(handle_, IDC_CBO_DBGFONTSIZE);
            int i = SendMessage(cbo, CB_GETCURSEL, 0, 0);
            SendMessage(cbo, CB_GETLBTEXT, i, (LPARAM)buf);
        }
        else
        {
            /* get the new edit field contents */
            GetDlgItemText(handle_, IDC_CBO_DBGFONTSIZE, buf, sizeof(buf));
        }

        /* remember the new font size */
        style_.pointsize = atoi(buf);

        /* make sure we redraw the sample text */
        update_sample();

        /* note the change */
        set_changed(TRUE);

        /* handled */
        return TRUE;
    }

    /* inherit default behavior */
    int ret = CHtmlDialogDebugPref::do_command(cmd, ctl);

    /* check for changes after the default handler */
    switch (LOWORD(cmd))
    {
    case IDC_CB_FGCOLOR:
    case IDC_CB_BGCOLOR:
    case IDC_CB_SELFGCOLOR:
    case IDC_CB_SELBGCOLOR:
        /* check for a selection change in the color pickers */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            int i;

            /* list item 0 is always the "Automatic" (default) selection */
            i = SendMessage(GetDlgItem(handle_, IDC_CB_FGCOLOR),
                            CB_GETCURSEL, 0, 0);

            /* update the foreground color */
            style_.default_color = (i == 0);
            if (i != 0)
                style_.color = COLORREF_to_HTML_color(
                    SendMessage(GetDlgItem(handle_, IDC_CB_FGCOLOR),
                                CB_GETITEMDATA, i, 0));

            /* update the background color */
            i = SendMessage(GetDlgItem(handle_, IDC_CB_BGCOLOR),
                            CB_GETCURSEL, 0, 0);
            style_.default_bgcolor = (i == 0);
            if (i != 0)
                style_.bgcolor = COLORREF_to_HTML_color(
                    SendMessage(GetDlgItem(handle_, IDC_CB_BGCOLOR),
                                CB_GETITEMDATA, i, 0));

            /* update the selected text foreground color */
            i = SendMessage(GetDlgItem(handle_, IDC_CB_SELFGCOLOR),
                            CB_GETCURSEL, 0, 0);
            sel_style_.default_color = (i == 0);
            if (i != 0)
                sel_style_.color = COLORREF_to_HTML_color(
                    SendMessage(GetDlgItem(handle_, IDC_CB_SELFGCOLOR),
                                CB_GETITEMDATA, i, 0));

            /* update the selected text background color */
            i = SendMessage(GetDlgItem(handle_, IDC_CB_SELBGCOLOR),
                            CB_GETCURSEL, 0, 0);
            sel_style_.default_bgcolor = (i == 0);
            if (i != 0)
                sel_style_.bgcolor = COLORREF_to_HTML_color(
                    SendMessage(GetDlgItem(handle_, IDC_CB_SELBGCOLOR),
                                CB_GETITEMDATA, i, 0));

            /* note the changed */
            set_changed(TRUE);

            /* make sure we redraw the sample text */
            update_sample();

            /* handled */
            ret |= TRUE;
        }
        break;
    }

    /* return the result */
    return ret;
}

int CHtmlDialogDebugSrcwin::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* update our sample */
        update_sample();
        break;

    case PSN_APPLY:
        /* save the custom colors */
        config_->setval_bytes("src win options", "custom colors",
                              cust_colors_, sizeof(cust_colors_));

        /* save the new "override scintilla selection colors" settings */
        config_->setval("src win options", "override sci sel colors",
                        IsDlgButtonChecked(handle_, IDC_CK_OVERRIDESCISEL)
                        == BST_CHECKED);

        /* save the text style */
        mainwin_->set_global_style(&style_, &sel_style_);

        /* tell the main window that we've updated the preferences */
        mainwin_->on_update_srcwin_options();

        /* done */
        return TRUE;
    }

    /* inherit default behavior */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for syntax coloring
 */

/* style/font structure */
struct style_info
{
    int mode_idx;
    wb_style_info s;
    CHtmlFontDesc f;
    CStringBuf keywords;
};

/*
 *   internal mode descriptor - for each mode in the list, we allocate one of
 *   these and store it in the combo box item data pointer
 */
class ModeInfo
{
public:
    ModeInfo(ITadsWorkbenchEditorMode *m, CHtmlDebugConfig *config)
    {
        /* remember the mode */
        mode = m;
        mode->AddRef();

        /* get our mode name */
        textchar_t *modename = ansi_from_bstr(mode->GetModeName(), TRUE);

        /* initialize our default style */
        wcscpy(default_style.s.sample, L"Sample Text");
        default_style.s.sci_index = -1;

        /* allocate the array of font descriptors and styles */
        style_cnt = mode->GetStyleCount();
        styles = new style_info[style_cnt];

        /* load our style descriptions */
        for (int i = 0 ; i < style_cnt ; ++i)
        {
            /* get the style information */
            m->GetStyleInfo(i, &styles[i].s);
            styles[i].mode_idx = i;

            /* if there are keywords for this style, load them */
            if (styles[i].s.keyword_set != 0)
            {
                /* generate the keys */
                char keya[128], keyb[128];
                sprintf(keya, "%s-editor-prefs", modename);
                sprintf(keyb, "custom-keywords-%d", styles[i].s.keyword_set);

                /* load the value */
                styles[i].keywords.set(config->getval_strptr(keya, keyb, 0));
            }
        }


        /* done with the mode name */
        th_free(modename);

        /* sort the items */
        if (style_cnt > 1)
            qsort(styles, style_cnt, sizeof(*styles), &style_sort_cb);
    }

    /* refresh the styles */
    void refresh_styles(CHtmlSys_dbgmain *mainwin,
                        CHtmlDialogDebugSrcwin *srcwindlg)
    {
        int i;

        /* set up the list of styles we want to fetch */
        CHtmlFontDesc *fonts = new CHtmlFontDesc[style_cnt];

        /* load the current style settings */
        mainwin->get_syntax_style_info(
            mode, &syntax_enabled, &default_style.f, fonts, style_cnt);

        /* copy these to our saved style information */
        for (i = 0 ; i < style_cnt ; ++i)
            styles[i].f = fonts[styles[i].mode_idx];

        /* done with the font array */
        delete [] fonts;
    }

    /* style sorter */
    static int style_sort_cb(const void *a0, const void *b0)
    {
        style_info *a = (style_info *)a0;
        style_info *b = (style_info *)b0;

        /* sort on the title */
        return (wcscmp(a->s.title, b->s.title));
    }

    ~ModeInfo()
    {
        /* forget our mode object */
        mode->Release();

        /* delete our font and style lists */
        delete [] styles;
    }

    /* our mode object */
    ITadsWorkbenchEditorMode *mode;

    /*
     *   our array of style descriptors - these contain the current style
     *   settings, with any unsaved changes the user has made via the dialog;
     *   when the user hits Apply or OK, we'll save these to the debugger
     *   configuration data
     */
    int style_cnt;
    style_info *styles;

    /* the current mode-specific default style */
    style_info default_style;

    /* is syntax coloring enabled for this mode? */
    int syntax_enabled;
};


/* the dialog class */
class CHtmlDialogDebugSyntax: public CHtmlDialogDebugPref,
    public DialogWithFonts
{
public:
    CHtmlDialogDebugSyntax(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited,
                           CHtmlDialogDebugSrcwin *srcwindlg)
        : CHtmlDialogDebugPref(mainwin, config, inited), DialogWithFonts()
    {
        /* remember the source window dialog */
        srcwindlg_ = srcwindlg;

        /* no combos yet */
        fgcombo_ = 0;
        bgcombo_ = 0;

        /* set up the color combos */
        controls_.add(fgcombo_ = new CColorComboPropPage(
            0, IDC_CB_FGCOLOR, &fg_, &fgdefault_,
            srcwindlg_->cust_colors_, this));
        controls_.add(bgcombo_ = new CColorComboPropPage(
            0, IDC_CB_BGCOLOR, &bg_, &bgdefault_,
            srcwindlg_->cust_colors_, this));

        /* set up the font combo */
        controls_.add(new CFontCombo(IDC_CB_FONT, TRUE));
    }

protected:
    void init();
    void destroy();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

    /* handle owner drawing */
    int draw_item(int ctl, DRAWITEMSTRUCT *dis)
    {
        switch (ctl)
        {
        case IDC_ST_SAMPLE:
            /* draw the sample text */
            draw_sample(dis, &font_desc_, 0);
            return TRUE;
        }

        return CHtmlDialogDebugPref::draw_item(ctl, dis);
    }

    /* get the current mode */
    ModeInfo *get_current_mode()
    {
        /* get the mode combo box */
        HWND cbo = GetDlgItem(handle_, IDC_CB_EDITMODE);

        /* ask it for its current selection */
        int idx = SendMessage(cbo, CB_GETCURSEL, 0, 0);

        /* if there's no selection, there's no mode */
        if (idx == -1)
            return 0;

        /* the ModeInfo is the data object in the combo list item */
        return (ModeInfo *)SendMessage(cbo, CB_GETITEMDATA, idx, 0);
    }

    /* select the mode for the current selection in the Edit Mode combo */
    void select_mode()
    {
        int idx;

        /* get the ModeInfo object from the current selection in the combo */
        ModeInfo *mi = get_current_mode();

        /* set the Use Syntax Coloring checkbox state */
        CheckDlgButton(handle_, IDC_CK_USESYNTAXCOLORS,
                       mi->syntax_enabled ? BST_CHECKED : BST_UNCHECKED);

        /* clear the old contents of the syntax-item list box */
        HWND lb = GetDlgItem(handle_, IDC_LB_SYNTAXITEM);
        SendMessage(lb, LB_RESETCONTENT, 0, 0);

        /*
         *   Add the special "Default" item first, but only if the mode has
         *   more than one style OR we're not using syntax coloring for the
         *   mode.  For a single-style mode, there's just no point in having
         *   a default item, and it's somewhat confusing for it to be there.
         *   If we're not using syntax coloring, there's only the Default
         *   setting for the mode at all.
         */
        if (mi->style_cnt > 1 || !mi->syntax_enabled)
        {
            idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)"Default");
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)&mi->default_style);
        }

        /*
         *   Load the list of syntax items from the mode into the list box.
         *   Do this only if syntax coloring is enabled; if not, the only
         *   setting is for the mode-wide default style.
         */
        if (mi->syntax_enabled)
        {
            int i;
            style_info *info;

            /* add each style to the listbox */
            for (i = 0, info = mi->styles ; i < mi->style_cnt ; ++i, ++info)
            {
                char title[countof(info->s.title)];

                /* add the style name to the list box */
                ansi_from_olestr(title, sizeof(title), info->s.title);
                idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)title);
                SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)info);
            }
        }

        /* select the first display item */
        SendMessage(lb, LB_SETCURSEL, 0, 0);
        select_dispitem();
    }

    /* get the style_info for the current syntax item selection */
    style_info *get_dispitem()
    {
        /* get the listbox selection */
        HWND lb = GetDlgItem(handle_, IDC_LB_SYNTAXITEM);
        int sel = SendMessage(lb, LB_GETCURSEL, 0, 0);
        if (sel == -1)
            return 0;

        /* return the style information for this item */
        return get_dispitem(sel);
    }

    /* get style_info for the given item */
    style_info *get_dispitem(int idx)
    {
        /* get the style_info from the listbox item data */
        HWND lb = GetDlgItem(handle_, IDC_LB_SYNTAXITEM);
        return (style_info *)SendMessage(lb, LB_GETITEMDATA, idx, 0);
    }

    /* update everything for a change to the syntax element selection */
    void select_dispitem()
    {
        style_info *info;
        ModeInfo *mi = get_current_mode();

        /* get the current selection; if there isn't one, do nothing */
        if ((info = get_dispitem()) == 0)
            return;

        /* set the colors for this item */
        fg_ = (info->f.default_color ? 0xFFFFFFFF : info->f.color);
        bg_ = (info->f.default_bgcolor ? 0xFFFFFFFF : info->f.bgcolor);

        /*
         *   Set the appropriate default colors.  For item #0, the defaults
         *   are the inherited defaults from the global settings.  For other
         *   items, the defaults are the item #0 settings.
         */
        CHtmlFontDesc *fglob = &srcwindlg_->style_;
        CHtmlFontDesc *f0 = &get_dispitem(0)->f;

        fgdefault_ = bgdefault_ = 0xFFFFFFFF;
        if (&info->f != f0)
        {
            if (fgdefault_ == 0xFFFFFFFF && !f0->default_color)
                fgdefault_ = f0->color;
            if (bgdefault_ == 0xFFFFFFFF && !f0->default_bgcolor)
                bgdefault_ = f0->bgcolor;
        }
        if (fgdefault_ == 0xFFFFFFFF && !fglob->default_color)
            fgdefault_ = fglob->color;
        if (bgdefault_ == 0xFFFFFFFF && !fglob->default_bgcolor)
            bgdefault_ = fglob->bgcolor;
        if (fgdefault_ == 0xFFFFFFFF)
            fgdefault_ = GetSysColor(COLOR_WINDOWTEXT);
        if (bgdefault_ == 0xFFFFFFFF)
            bgdefault_ = GetSysColor(COLOR_WINDOW);

        /* select the current colors in the combos */
        fgcombo_->select_by_color();
        bgcombo_->select_by_color();

        /* select the font */
        select_combo_font(info->f.face, info->f.pointsize);

        /*
         *   if this is the "default" item (indicated by an invalid Scintilla
         *   style index), gray out the attribute buttons - attributes can't
         *   be inherited, so it would be pointless to let the user select
         *   them in the default item
         */
        int ena = (info->s.sci_index != -1);
        EnableWindow(GetDlgItem(handle_, IDC_CK_BOLD), ena);
        EnableWindow(GetDlgItem(handle_, IDC_CK_ITALIC), ena);
        EnableWindow(GetDlgItem(handle_, IDC_CK_UNDERLINE), ena);

        /* set the attribute checkboxes */
        CheckDlgButton(handle_, IDC_CK_BOLD,
                       info->f.weight >= 700 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(handle_, IDC_CK_ITALIC,
                       info->f.italic ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(handle_, IDC_CK_UNDERLINE,
                       info->f.underline ? BST_CHECKED : BST_UNCHECKED);

        /* update the sample */
        update_sample();

        /*
         *   enable the Keywords button only if there's a keyword set
         *   associated with this style
         */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_KEYWORDS),
                     ena && info->s.keyword_set != 0);
    }

    /* update the sample display text to reflect the new settings */
    void update_sample()
    {
        char buf[256];
        style_info *info;
        const textchar_t *txt;

        /* get the current selection; if there isn't one, do nothing */
        if ((info = get_dispitem()) == 0)
            return;

        /* set the sample font */
        font_desc_ = info->f;

        /*
         *   get the sources of the defaults - the 0th syntax item in the
         *   current mode is the default for the rest of the mode, and the
         *   global settings are the defaults for the 0th item
         */
        CHtmlFontDesc *fglob = &srcwindlg_->style_;
        CHtmlFontDesc *f0 = &get_dispitem(0)->f;

        /* apply defaults */
        if (font_desc_.face[0] == '\0')
            strcpy(font_desc_.face, f0->face);
        if (font_desc_.face[0] == '\0')
            strcpy(font_desc_.face, fglob->face);

        if (font_desc_.pointsize == 0)
            font_desc_.pointsize = f0->pointsize;
        if (font_desc_.pointsize == 0)
            font_desc_.pointsize = fglob->pointsize;

        /* create a font object for it */
        font_ = CTadsApp::get_app()->get_font(
            font_desc_.face, font_desc_.pointsize, font_desc_.weight >= 700,
            font_desc_.italic, font_desc_.underline,
            RGB(0,0,0), FALSE, RGB(0,0,0), FALSE, 0);

        /*
         *   if there's a keyword list defined for this style, use it;
         *   otherwise use the sample text defined for the style
         */
        if (info->keywords.get() != 0 && info->keywords.get()[0] != '\0')
            txt = info->keywords.get();
        else
            txt = ansi_from_olestr(buf, sizeof(buf), info->s.sample);

        /* set the sample text and font */
        SetDlgItemText(handle_, IDC_ST_SAMPLE, txt);
    }

    /* set a combo to display a font selection */
    void select_combo_font(const textchar_t *face, int ptsize)
    {
        HWND ctl;
        int i;

        /* if the face is unset, use the special "Automatic" item */
        ctl = GetDlgItem(handle_, IDC_CB_FONT);
        if (face == 0 || face[0] == '\0')
        {
            /* "Automatic" is always the first list item */
            SendMessage(ctl, CB_SETCURSEL, 0, 0);
        }
        else
        {
            /* find the font name in the combo */
            i = SendMessage(ctl, CB_FINDSTRING, (WPARAM)-1, (LPARAM)face);

            /* if we found it, select the item */
            if (i != 0)
                SendMessage(ctl, CB_SETCURSEL, i, 0);
        }

        /* if the point size is unset, use the "Auto" item */
        ctl = GetDlgItem(handle_, IDC_CB_FONTSIZE);
        if (ptsize == 0)
        {
            /* "Auto" is the first item in the list */
            SendMessage(ctl, CB_SETCURSEL, 0, 0);
        }
        else
        {
            char buf[40];

            /* set the text to the point size */
            sprintf(buf, "%d", ptsize);
            SetWindowText(ctl, buf);
        }
    }

    /*
     *   restore the factory defaults for the current mode
     */
    void restore_mode_defaults()
    {
        ModeInfo *mi = get_current_mode();
        int i;
        style_info *s;

        /* reset each style to its factory defaults */
        for (i = 0, s = mi->styles ; i < mi->style_cnt ; ++i, ++s)
        {
            s->f.default_color = (s->s.fgcolor == WBSTYLE_DEFAULT_COLOR);
            s->f.color = COLORREF_to_HTML_color(s->s.fgcolor);

            s->f.default_bgcolor = (s->s.bgcolor == WBSTYLE_DEFAULT_COLOR);
            s->f.bgcolor = COLORREF_to_HTML_color(s->s.bgcolor);

            ansi_from_olestr(s->f.face, sizeof(s->f.face), s->s.font_name);
            s->f.pointsize = s->s.ptsize;

            s->f.weight = ((s->s.styles & WBSTYLE_BOLD) != 0 ? 700 : 400);
            s->f.italic = ((s->s.styles & WBSTYLE_ITALIC) != 0);
            s->f.underline = ((s->s.styles & WBSTYLE_UNDERLINE) != 0);
        }

        /* reset the "default" item to inheriting everything */
        CHtmlFontDesc *f = &mi->default_style.f;
        f->default_color = TRUE;
        f->default_bgcolor = TRUE;
        f->face[0] = '\0';
        f->pointsize = 0;
        f->weight = 400;
        f->italic = FALSE;
        f->underline = FALSE;

        /* reselect the current item */
        select_dispitem();
    }

    /*
     *   save our changes
     */
    void save()
    {
        HWND cbo = GetDlgItem(handle_, IDC_CB_EDITMODE);
        int cnt = SendMessage(cbo, CB_GETCOUNT, 0, 0);

        /* run through our list of modes and save each one */
        for (int i = 0 ; i < cnt ; ++i)
        {
            int j;

            /* get the mode object */
            ModeInfo *mi = (ModeInfo *)SendMessage(cbo, CB_GETITEMDATA, i, 0);

            /* get this mode name */
            textchar_t *modename = ansi_from_bstr(
                mi->mode->GetModeName(), TRUE);

            /* create an array of descriptors for its entries */
            CHtmlFontDesc *fonts = new CHtmlFontDesc[mi->style_cnt];

            /* copy the fonts to the descriptor array */
            for (j = 0 ; j < mi->style_cnt ; ++j)
                fonts[mi->styles[j].mode_idx] = mi->styles[j].f;

            /* save the changes */
            mainwin_->save_syntax_style_info(
                mi->mode, mi->syntax_enabled,
                &mi->default_style.f, fonts, mi->style_cnt);

            /* done with the font array */
            delete [] fonts;

            /* save any custom keywords */
            for (j = 0 ; j < mi->style_cnt ; ++j)
            {
                /* if this item has custom keywords, save the list */
                if (mi->styles[j].s.keyword_set != 0
                    && mi->styles[j].keywords.get() != 0)
                {
                    /* generate the keys */
                    char keya[128], keyb[128];
                    sprintf(keya, "%s-editor-prefs", modename);
                    sprintf(keyb, "custom-keywords-%d",
                            mi->styles[j].s.keyword_set);

                    /* save the keyword list */
                    config_->setval(keya, keyb, 0,
                                    mi->styles[j].keywords.get());
                }
            }

            /* done with the mode name */
            th_free(modename);
        }
    }

    /*
     *   source window formatting dialog - we use this as the source of our
     *   global font settings, since we want to reflect the unsaved changes
     *   made in that page
     */
    CHtmlDialogDebugSrcwin *srcwindlg_;

    /* sample text information */
    CHtmlFontDesc font_desc_;

    /* current item's colors */
    HTML_color_t fg_;
    HTML_color_t bg_;

    /* color combo boxes */
    CColorComboPropPage *fgcombo_;
    CColorComboPropPage *bgcombo_;
};

/*
 *   initialize
 */
void CHtmlDialogDebugSyntax::init()
{
    HWND modelist = GetDlgItem(handle_, IDC_CB_EDITMODE);
    static const textchar_t *fontsizes[] =
    {
        "Auto", "4", "5", "6", "8", "9", "10", "11", "12", "14", "16", "18",
        "20", "24", "28", "36", 0
    };
    int idx;
    const textchar_t **p;
    CHtmlDbgSubWin *lastwin;
    ITadsWorkbenchEditorMode *lastmode = 0;
    textchar_t lastmodename[128];

    /* inherit the default */
    CHtmlDialogDebugPref::init();

    /*
     *   look to see if the last active window had a mode associated with it;
     *   if so, we'll want to select that mode initially
     */
    if ((lastwin = mainwin_->get_last_active_dbg_win()) != 0
        && (lastmode = lastwin->get_editor_mode()))
    {
        /* get the name of the last mode */
        ansi_from_bstr(lastmodename, sizeof(lastmodename),
                       lastmode->GetModeName(), TRUE);
    }

    /* load the color lists */
    load_color_list(GetDlgItem(handle_, IDC_CB_FGCOLOR));
    load_color_list(GetDlgItem(handle_, IDC_CB_BGCOLOR));

    /* set up the font combo with a list of the available fonts */
    init_font_popup(IDC_CB_FONT, TRUE, TRUE, srcwindlg_->style_.face, TRUE,
                    oshtml_charset_id_t(ANSI_CHARSET, CP_ACP).charset);

    /* load up the suggested font sizes list */
    for (idx = 0, p = fontsizes ; *p != 0 ; ++p, ++idx)
        SendMessage(GetDlgItem(handle_, IDC_CB_FONTSIZE),
                    CB_INSERTSTRING, idx, (LPARAM)*p);

    /* add an "Automatic" item for defaults */
    SendMessage(GetDlgItem(handle_, IDC_CB_FONT),
                CB_ADDSTRING, 0, (LPARAM)"(Automatic)");

    /* load the list of modes */
    twb_editor_mode_iter iter(mainwin_);
    ITadsWorkbenchEditorMode *mode;
    for (mode = iter.first() ; mode != 0 ; mode = iter.next())
    {
        char name[128];
        int idx;
        style_info *si;

        /* get the mode name */
        ansi_from_bstr(name, sizeof(name), mode->GetModeName(), TRUE);

        /* add this to the list */
        idx = SendMessage(modelist, CB_ADDSTRING, 0, (LPARAM)name);

        /*
         *   if this mode matches (by name) the mode of the last active
         *   debugger window, select this as the initial mode
         */
        if (lastmode != 0 && strcmp(name, lastmodename) == 0)
            SendMessage(modelist, CB_SETCURSEL, idx, 0);

        /*
         *   create a ModeInfo object and store it as the combo item's extra
         *   data object
         */
        ModeInfo *mi = new ModeInfo(mode, config_);
        SendMessage(modelist, CB_SETITEMDATA, idx, (LPARAM)mi);

        /* refresh this mode's styles */
        mi->refresh_styles(mainwin_, srcwindlg_);

        /* add any needed custom colors to the combo lists */
        for (idx = 0, si = mi->styles ; idx < mi->style_cnt ; ++idx, ++si)
        {
            if (!si->f.default_color)
                fgcombo_->add_custom_color(
                    HTML_color_to_COLORREF(si->f.color), FALSE);

            if (!si->f.default_bgcolor)
                bgcombo_->add_custom_color(
                    HTML_color_to_COLORREF(si->f.bgcolor), FALSE);
        }
    }

    /*
     *   if we didn't select a mode based on the last active window, select
     *   the first mode in the list by default
     */
    if (SendMessage(modelist, CB_GETCURSEL, 0, 0) == -1)
        SendMessage(modelist, CB_SETCURSEL, 0, 0);

    /* activate the mode we selected */
    select_mode();
}

/*
 *   Keywords sub-dialog
 */
class UserKeywordsDialog: public CTadsDialog
{
public:
    /* run the dialog; return true if the keyword list changes */
    static int run_dlg(HWND parent, style_info *info)
    {
        /* set up the dialog, and run it */
        UserKeywordsDialog dlg(info);
        dlg.run_modal(DLG_DEBUG_USER_KW, parent,
                      CTadsApp::get_app()->get_instance());

        /* return the update indication */
        return dlg.changed;
    }

protected:
    UserKeywordsDialog(style_info *info)
    {
        /* remember the style info object */
        this->info = info;

        /* we're not changed yet */
        changed = FALSE;
    }

    /* dialog window initialization */
    void init()
    {
        /* set the keyword list field to our current keyword list */
        if (info->keywords.get() != 0)
            SetDlgItemText(handle_, IDC_FLD_KW, info->keywords.get());

        /* set the field label to include the style title */
        char label[256];
        sprintf(label, "&Keywords for style \"%ws\"", info->s.title);
        SetDlgItemText(handle_, IDC_ST_KW, label);

        /* do the default work */
        CTadsDialog::init();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        /* check for our commands */
        switch (LOWORD(cmd))
        {
        case IDC_BTN_CLOSE:
            /* get the new field contents */
            {
                CStringBuf buf;
                size_t len;
                HWND fld = GetDlgItem(handle_, IDC_FLD_KW);
                char *p;

                /* ensure we have space for the new text */
                buf.ensure_size(len = GetWindowTextLength(fld));

                /* get the text */
                GetWindowText(fld, buf.get(), len + 1);

                /* convert newlines and tabs to spaces */
                for (p = buf.get() ; *p != '\0' ; ++p)
                {
                    if (strchr("\r\n\t", *p) != 0)
                        *p = ' ';
                }

                /* get the original */
                textchar_t *orig = info->keywords.get();
                if (orig == 0)
                    orig = "";

                /* compare it to the original */
                if (strcmp(orig, buf.get()) != 0)
                {
                    /* copy the new list into the style */
                    info->keywords.set(buf.get());

                    /* note that the list has changed */
                    changed = TRUE;
                }
            }

            /* terminate the dialog */
            EndDialog(handle_, cmd);
            return TRUE;
        }

        /* not one of ours */
        return CTadsDialog::do_command(cmd, ctl);
    }

    /* our style info object */
    style_info *info;

    /* did the keyword list change? */
    int changed;
};

/*
 *   destroy the window
 */
void CHtmlDialogDebugSyntax::destroy()
{
    HWND modelist = GetDlgItem(handle_, IDC_CB_EDITMODE);
    int i, cnt;

    /* release our mode objects */
    for (i = 0, cnt = SendMessage(modelist, CB_GETCOUNT, 0, 0) ;
         i < cnt ; ++i)
    {
        /* delete the ModeInfo object stored in the combo item data */
        delete (ModeInfo *)SendMessage(modelist, CB_GETITEMDATA, i, 0);
    }

    /* inherit default */
    CHtmlDialogDebugPref::destroy();
}

/*
 *   handle a command
 */
int CHtmlDialogDebugSyntax::do_command(WPARAM cmd, HWND ctl)
{
    style_info *info;

    switch (LOWORD(cmd))
    {
    case IDC_BTN_DEFAULT:
        {
            char buf[400];
            char msg[450];
            ModeInfo *mi = get_current_mode();
            char modename[64];

            /* if there's no mode, we can't restore anything */
            if (mi == 0)
                return TRUE;

            /* get its name */
            ansi_from_bstr(modename, sizeof(modename),
                           mi->mode->GetModeName(), TRUE);

            /* build the confirmation string */
            LoadString(CTadsApp::get_app()->get_instance(),
                       IDS_CONFIRM_RESTORE_MODE_DEFAULTS, buf, sizeof(buf));
            sprintf(msg, buf, modename);

            /* ask for confirmation */
            if (MessageBox(0, msg, "TADS Workbench",
                           MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL)
                == IDYES)
            {
                /* they said to go ahead */
                restore_mode_defaults();

                /* note the change */
                set_changed(TRUE);
            }
        }

        /* handled */
        return TRUE;

    case IDC_CB_EDITMODE:
        /* check for a selection change */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            /* select the new mode */
            select_mode();
            return TRUE;
        }
        break;

    case IDC_LB_SYNTAXITEM:
        /* check for a selection change */
        if (HIWORD(cmd) == LBN_SELCHANGE)
        {
            /* select the new syntax item */
            select_dispitem();
            return TRUE;
        }

    case IDC_CB_FONT:
        /* set the new font */
        if (HIWORD(cmd) == CBN_SELCHANGE && (info = get_dispitem()) != 0)
        {
            /*
             *   if it's the first item, it means to use the default font
             *   inherited from the global settings; otherwise, use the name
             */
            if (SendMessage(GetDlgItem(handle_, IDC_CB_FONT),
                            CB_GETCURSEL, 0, 0) == 0)
            {
                /* it's the default item  */
                info->f.face[0] = '\0';
            }
            else
            {
                /* get the new font name */
                GetDlgItemText(handle_, IDC_CB_FONT,
                               info->f.face, sizeof(info->f.face));
            }

            /* note the update */
            set_changed(TRUE);

            /* update the sample for the change */
            update_sample();
        }

        /* handled */
        return TRUE;

    case IDC_CB_FONTSIZE:
        /* set the new point size */
        if ((HIWORD(cmd) == CBN_SELCHANGE || HIWORD(cmd) == CBN_EDITCHANGE)
            && (info = get_dispitem()) != 0)
        {
            char buf[40];

            /* get the new selection or text */
            if (HIWORD(cmd) == CBN_SELCHANGE)
            {
                /* selection change - get the new selection */
                HWND cbo = GetDlgItem(handle_, IDC_CB_FONTSIZE);
                int i = SendMessage(cbo, CB_GETCURSEL, 0, 0);
                SendMessage(cbo, CB_GETLBTEXT, i, (LPARAM)buf);
            }
            else
            {
                /* edit change - get the new field contents */
                GetDlgItemText(handle_, IDC_CB_FONTSIZE, buf, sizeof(buf));
            }

            /* check for an actual change */
            if (info->f.pointsize != atoi(buf))
            {
                /* set the new size */
                info->f.pointsize = atoi(buf);

                /* note the change */
                set_changed(TRUE);

                /* update the sample */
                update_sample();
            }
        }
        return TRUE;

    case IDC_CK_BOLD:
    case IDC_CK_ITALIC:
    case IDC_CK_UNDERLINE:
        /* attribute change - update the current item */
        if ((info = get_dispitem()) != 0)
        {
            /* get the new settings */
            info->f.weight = (IsDlgButtonChecked(handle_, IDC_CK_BOLD)
                              == BST_CHECKED ? 700 : 400);
            info->f.italic = (IsDlgButtonChecked(handle_, IDC_CK_ITALIC)
                              == BST_CHECKED);
            info->f.underline = (IsDlgButtonChecked(handle_, IDC_CK_UNDERLINE)
                                 == BST_CHECKED);

            /* note the update */
            set_changed(TRUE);

            /* update the sample for the change */
            update_sample();
        }

        /* handled */
        return TRUE;

    case IDC_CK_USESYNTAXCOLORS:
        {
            ModeInfo *mi = get_current_mode();

            /* if we have a mode selected, update its status */
            if (mi != 0)
            {
                /* set the new status */
                mi->syntax_enabled =
                    (IsDlgButtonChecked(handle_, IDC_CK_USESYNTAXCOLORS)
                     == BST_CHECKED);

                /* note the change */
                set_changed(TRUE);

                /* rebuild the syntax list for the change */
                select_mode();
            }
        }

        /* handled */
        return TRUE;

    case IDC_BTN_KEYWORDS:
        /*
         *   get the selected item, and run the keywords dialog; if it
         *   indicates that the keyword list changed, apply the updates
         */
        if ((info = get_dispitem()) != 0
            && UserKeywordsDialog::run_dlg(GetParent(handle_), info))
        {
            /* note the change */
            set_changed(TRUE);

            /* update the sample text */
            update_sample();
        }

        /* handled */
        return TRUE;
    }

    /* inherit the default */
    int ret = CHtmlDialogDebugPref::do_command(cmd, ctl);

    /*
     *   check for some changes after doing the default work - we wait to
     *   check these because we want the inherited handler to update our
     *   member variables first
     */
    switch (LOWORD(cmd))
    {
    case IDC_CB_FGCOLOR:
    case IDC_CB_BGCOLOR:
        /* check for a selection change */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            /*
             *   the inherited handler has already updated fg_ or bg_ for us,
             *   but we still need to propagate the changes to the current
             *   style item
             */
            if ((info = get_dispitem()) == 0)
                break;

            /* propagate the changes */
            info->f.default_color =
                (SendMessage(GetDlgItem(handle_, IDC_CB_FGCOLOR),
                             CB_GETCURSEL, 0, 0) == 0);
            info->f.color = fg_;

            info->f.default_bgcolor =
                (SendMessage(GetDlgItem(handle_, IDC_CB_BGCOLOR),
                             CB_GETCURSEL, 0, 0) == 0);
            info->f.bgcolor = bg_;

            /* note the update */
            set_changed(TRUE);

            /* update the sample for the change */
            update_sample();

            /* handled */
            ret |= TRUE;
        }
        break;
    }

    /* return the result */
    return ret;
}

/*
 *   handle a notification
 */
int CHtmlDialogDebugSyntax::do_notify(NMHDR *nm, int ctl)
{
    switch (nm->code)
    {
    case PSN_SETACTIVE:
        /*
         *   on becoming active, re-select the current item; this will make
         *   sure that we pick up any unsaved changed made in the global
         *   style page
         */
        select_dispitem();
        break;

    case PSN_APPLY:
        /* save the settings for each mode */
        save();

        /* tell the main window that we've updated the preferences */
        mainwin_->on_update_srcwin_options();
        break;
    }

    /* inherit the default */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   External tools dialog
 */
class CHtmlDialogDebugTools: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugTools(CHtmlSys_dbgmain *mainwin,
                          CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
        last_sel_ = -1;
        warned_cmdid_ = TRUE;
    }

    struct toolitem
    {
        toolitem(const char *title, const char *cmdid, const char *prog,
                 const char *args, const char *dir, int capture)
            : title(title), cmdid(cmdid), prog(prog), args(args), dir(dir)
        {
            this->capture = capture;
        }
        toolitem(const char *title, const char *cmdid)
            : title(title), cmdid(cmdid)
        {
            this->capture = FALSE;
        }
        CStringBuf title;
        CStringBuf cmdid;
        CStringBuf prog;
        CStringBuf args;
        CStringBuf dir;
        int capture;
    };

    struct menuinfo
    {
        int id;
        const char *txt;
    };

    void init()
    {
        /* do the default work */
        CHtmlDialogDebugPref::init();

        /* set the icons for the menu buttons */
        HANDLE hi = LoadImage(CTadsApp::get_app()->get_instance(),
                              MAKEINTRESOURCE(IDI_RIGHTARROW),
                              IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        SendMessage(GetDlgItem(handle_, IDC_BTN_ARGSMENU), BM_SETIMAGE,
                    IMAGE_ICON, (LPARAM)hi);
        SendMessage(GetDlgItem(handle_, IDC_BTN_DIRMENU), BM_SETIMAGE,
                    IMAGE_ICON, (LPARAM)hi);

        /* load the configuration data */
        HWND lb = GetDlgItem(handle_, IDC_LB_TOOLS);
        int cnt = config_->get_strlist_cnt("extern tools", "title");
        for (int i = 0 ; i < cnt ; ++i)
        {
            const textchar_t *capture =
                config_->getval_strptr("extern tools", "capture", i);

            /* create the toolitem object */
            toolitem *t = new toolitem(
                config_->getval_strptr("extern tools", "title", i),
                config_->getval_strptr("extern tools", "cmdid", i),
                config_->getval_strptr("extern tools", "prog", i),
                config_->getval_strptr("extern tools", "args", i),
                config_->getval_strptr("extern tools", "dir", i),
                capture != 0 && (capture[0] == 'y' || capture[0] == 'Y'));

            /* add the list item */
            int idx = SendMessage(lb, LB_ADDSTRING,
                                  0, (LPARAM)t->title.get());

            /* save the toolitem object as the list item data */
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)t);
        }

        /* select the first item and load the fields from it */
        SendMessage(GetDlgItem(handle_, IDC_LB_TOOLS), LB_SETCURSEL, 0, 0);
        load_fields();
    }

    void destroy()
    {
        HWND lb = GetDlgItem(handle_, IDC_LB_TOOLS);
        int cnt = SendMessage(lb, LB_GETCOUNT, 0, 0);

        /* delete each listbox item */
        for (int i = 0 ; i < cnt ; ++i)
        {
            delete (toolitem *)SendMessage(lb, LB_GETITEMDATA, i, 0);
            SendMessage(lb, LB_SETITEMDATA, i, 0);
        }

        /* do the default work */
        CHtmlDialogDebugPref::destroy();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        static const menuinfo argsmenu[] =
        {
            { ID_CA_ITEMPATH, "$(ItemPath)" },
            { ID_CA_ITEMDIR, "$(ItemDir)" },
            { ID_CA_ITEMFILE, "$(ItemFile)" },
            { ID_CA_ITEMEXT, "$(ItemExt)" },
            { ID_CA_PROJDIR, "$(ProjDir)" },
            { ID_CA_PROJFILE, "$(ProjFile)" },
            { ID_CA_CURLINE, "$(CurLine)" },
            { ID_CA_CURCOL, "$(CurCol)" },
            { ID_CA_CURTEXT, "$(SelText)" },
            { 0, 0 }
        };
        static const menuinfo dirmenu[] =
        {
            { ID_ID_PROJ, "$(ProjDir)" },
            { ID_ID_ITEM, "$(ItemDir)" },
            { 0, 0 }
        };
        toolitem *t;
        char cmdid[128];
        int idx;
        HWND lb;

        /* note changes */
        switch (LOWORD(cmd))
        {
        case IDC_FLD_TITLE:
        case IDC_FLD_CMDFILE:
        case IDC_FLD_PROGARGS:
        case IDC_FLD_DIR:
        case IDC_FLD_CMDID:
            /*
             *   if this is a change to the field, and we have a selection,
             *   note the update
             */
            if (HIWORD(cmd) == EN_CHANGE && last_sel_ != -1)
                set_changed(TRUE);
            break;

        case IDC_CK_CAPTURE:
            /* if there's a selection, note the update */
            if (last_sel_ != -1)
                set_changed(TRUE);
            break;
        }

        /* check the control source */
        switch (LOWORD(cmd))
        {
        case IDC_BTN_MOVE_UP:
        case IDC_BTN_MOVE_DOWN:
            /* if there's no selection, ignore it */
            if (last_sel_ == -1)
                return TRUE;

            /* save the fields */
            save_fields();

            /* delete the item from its current position */
            lb = GetDlgItem(handle_, IDC_LB_TOOLS);
            t = (toolitem *)SendMessage(lb, LB_GETITEMDATA, last_sel_, 0);
            SendMessage(lb, LB_DELETESTRING, last_sel_, 0);

            /* re-insert it at the new position */
            last_sel_ += (LOWORD(cmd) == IDC_BTN_MOVE_UP ? -1 : 1);
            idx = SendMessage(lb, LB_INSERTSTRING, last_sel_,
                              (LPARAM)t->title.get());
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)t);

            /* select it */
            SendMessage(lb, LB_SETCURSEL, idx, 0);

            /* adjust for the new selection */
            load_fields();

            /* handled */
            return TRUE;

        case IDC_BTN_ADD:
            /* create a new item */
            gen_cmd_id(cmdid);
            t = new toolitem("Untitled", cmdid);

            /* add it to the list */
            lb = GetDlgItem(handle_, IDC_LB_TOOLS);
            idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)t->title.get());

            /* set its item data to point to the toolitem structure */
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)t);

            /* select the new item */
            SendMessage(lb, LB_SETCURSEL, idx, 0);
            save_fields();
            load_fields();

            /* note the change */
            set_changed(TRUE);

            /* handled */
            return TRUE;

        case IDC_BTN_DELETE:
            /* if we have a selection, delete it */
            if (last_sel_ != -1)
            {
                /* delete the selection */
                SendMessage(GetDlgItem(handle_, IDC_LB_TOOLS),
                            LB_DELETESTRING, last_sel_, 0);

                /* load the new selected item */
                load_fields();

                /* note the change */
                set_changed(TRUE);
            }

            /* done */
            return TRUE;

        case IDC_FLD_TITLE:
            /* check for change notifications */
            if (HIWORD(cmd) == EN_CHANGE && last_sel_ != -1)
            {
                char title[256];
                HWND lb = GetDlgItem(handle_, IDC_LB_TOOLS);

                /* update the title in the list in real time */
                GetDlgItemText(handle_, IDC_FLD_TITLE, title, sizeof(title));

                /* delete the old item, but save its toolitem object */
                LPARAM d = SendMessage(lb, LB_GETITEMDATA, last_sel_, 0);
                SendMessage(lb, LB_SETITEMDATA, last_sel_, 0);
                SendMessage(lb, LB_DELETESTRING, last_sel_, 0);

                /* insert a new item at the same position */
                int idx = SendMessage(
                    lb, LB_INSERTSTRING, last_sel_, (LPARAM)title);
                SendMessage(lb, LB_SETITEMDATA, idx, d);

                /* select the new item */
                SendMessage(lb, LB_SETCURSEL, idx, 0);

                /* handled */
                break;
            }
            break;

        case IDC_FLD_CMDID:
            /* check for change notifications */
            if (HIWORD(cmd) == EN_CHANGE)
            {
                /* check the text */
                GetDlgItemText(handle_, IDC_FLD_CMDID, cmdid, sizeof(cmdid));

                /* if we've already warned for this item, skip the warning */
                if (warned_cmdid_)
                    return TRUE;

                /* ignore this if there's no list selection */
                if (SendMessage(GetDlgItem(handle_, IDC_LB_TOOLS),
                                LB_GETCURSEL, 0, 0) == -1)
                    return TRUE;

                /* validate it */
                if (memicmp(cmdid, "Ext.Tools.", 10) != 0)
                {
                    /* it's not valid - warn about it */
                    MessageBox(GetParent(handle_),
                               "The command ID must start "
                               "with \"Ext.Tools.\".  Changes to the "
                               "command ID won't be saved unless the ID "
                               "is valid.", "TADS Workbench",
                               MB_OK | MB_ICONEXCLAMATION);

                    /* don't warn again until they select a new list item */
                    warned_cmdid_ = TRUE;
                }

                /* handled */
                return TRUE;
            }
            break;

        case IDC_BTN_ARGSMENU:
            /* display the args menu */
            arg_menu(IDC_BTN_ARGSMENU, IDR_CMDARGS_MENU,
                     argsmenu, IDC_FLD_PROGARGS);
            return TRUE;

        case IDC_BTN_DIRMENU:
            /*
             *   select the whole current contents, so that we replace it
             *   with the menu selection
             */
            SendMessage(GetDlgItem(handle_, IDC_FLD_DIR), EM_SETSEL, 0, -1);

            /* display the directory menu */
            arg_menu(IDC_BTN_DIRMENU, IDR_INITDIR_MENU,
                     dirmenu, IDC_FLD_DIR);
            return TRUE;

        case IDC_BTN_BROWSE:
            /* browse for an executable */
            browse_file("Executables (*.exe,*.com,*.pif,*.bat,*.cmd)\0"
                        "*.exe;*.com;*.pif;*.bat;*.cmd\0"
                        "All Files (*.*)\0*.*\0", "Select Program File",
                        0, IDC_FLD_CMDFILE,
                        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                        | OFN_HIDEREADONLY, TRUE, 0);
            return TRUE;

        case IDC_LB_TOOLS:
            /* check for a selection change in the list */
            if (HIWORD(cmd) == LBN_SELCHANGE)
            {
                /* save the current fields to the current selection */
                save_fields();

                /* populate the fields with the new selection */
                load_fields();

                /* handled */
                return TRUE;
            }
            break;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    /* generate a new command ID */
    void gen_cmd_id(char *buf)
    {
        static const char *desc = "User-defined external tool command";

        /* find an undefined command */
        for (int i = 1 ; ; ++i)
        {
            sprintf(buf, "Ext.Tools.User%i", i);
            if (!mainwin_->find_command(buf, FALSE))
                break;
        }

        /* add the command */
        mainwin_->define_ext_command(buf, strlen(buf), desc, strlen(desc));
    }

    /* pop up an argument helper menu */
    void arg_menu(int btn_id, int menu_id, const menuinfo *info, int fld_id)
    {
        RECT rc;
        HMENU menu;

        /* get the button location - we'll pop up the menu next to it */
        GetWindowRect(GetDlgItem(handle_, btn_id), &rc);

        /* load the menu */
        menu = LoadMenu(CTadsApp::get_app()->get_instance(),
                        MAKEINTRESOURCE(menu_id));

        /* pop it up to the right of the button */
        int cmd = TrackPopupMenu(
            GetSubMenu(menu, 0),
            TPM_RETURNCMD | TPM_TOPALIGN | TPM_LEFTALIGN,
            rc.right, rc.top, 0, handle_, 0);

        /* if we got a command, map it into text to insert */
        if (cmd != 0)
        {
            /* scan the menuinfo list for the command ID */
            for (int i = 0 ; info[i].id != 0 ; ++i)
            {
                /* if this is a match, insert its text */
                if (info[i].id == cmd)
                {
                    /* insert the text into the field */
                    SendMessage(GetDlgItem(handle_, fld_id), EM_REPLACESEL,
                                TRUE, (LPARAM)info[i].txt);

                    /* done */
                    break;
                }
            }
        }
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        /* check what we have */
        switch (nm->code)
        {
        case PSN_APPLY:
            /* save our current fields */
            save_fields();

            /* clear the old tool settings */
            config_->clear_strlist("extern tools", "title");
            config_->clear_strlist("extern tools", "cmdid");
            config_->clear_strlist("extern tools", "prog");
            config_->clear_strlist("extern tools", "args");
            config_->clear_strlist("extern tools", "dir");
            config_->clear_strlist("extern tools", "capture");

            /* save the new tool list */
            HWND lb = GetDlgItem(handle_, IDC_LB_TOOLS);
            int cnt = SendMessage(lb, LB_GETCOUNT, 0, 0);
            for (int i = 0 ; i < cnt ; ++i)
            {
                /* get this item */
                toolitem *t =
                    (toolitem *)SendMessage(lb, LB_GETITEMDATA, i, 0);

                /* save it */
                config_->setval("extern tools", "title", i, t->title.get());
                config_->setval("extern tools", "cmdid", i, t->cmdid.get());
                config_->setval("extern tools", "prog", i, t->prog.get());
                config_->setval("extern tools", "args", i, t->args.get());
                config_->setval("extern tools", "dir", i, t->dir.get());
                config_->setval("extern tools", "capture", i,
                                t->capture ? "y" : "n");
            }

            /* rebuild the Tools menu */
            mainwin_->rebuild_tools_menu();

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

    /* save the current fields */
    void save_fields()
    {
        toolitem *t;
        char buf[512];

        /* if we don't have a previous selection, there's nothing to do */
        if (last_sel_ == -1)
            return;

        /* get the item data for the selection */
        t = (toolitem *)SendMessage(GetDlgItem(handle_, IDC_LB_TOOLS),
                                    LB_GETITEMDATA, last_sel_, 0);

        /* if there's no item data, there's nothing to do */
        if (t == 0)
            return;

        /* save the field values */
        GetDlgItemText(handle_, IDC_FLD_TITLE, buf, sizeof(buf));
        t->title.set(buf);

        GetDlgItemText(handle_, IDC_FLD_CMDFILE, buf, sizeof(buf));
        t->prog.set(buf);

        GetDlgItemText(handle_, IDC_FLD_PROGARGS, buf, sizeof(buf));
        t->args.set(buf);

        GetDlgItemText(handle_, IDC_FLD_DIR, buf, sizeof(buf));
        t->dir.set(buf);

        t->capture =
            (IsDlgButtonChecked(handle_, IDC_CK_CAPTURE) == BST_CHECKED);

        /* save the updated command ID only if it's valid */
        GetDlgItemText(handle_, IDC_FLD_CMDID, buf, sizeof(buf));
        if (memicmp(buf, "Ext.Tools.", 10) == 0)
        {
            static const char *desc = "User-defined external tool command";

            /* save it in our list */
            t->cmdid.set(buf);

            /* define the command in Workbench */
            mainwin_->define_ext_command(buf, strlen(buf),
                                         desc, strlen(desc));
        }
    }

    /* load fields from the current selection */
    void load_fields()
    {
        /* get the current selection */
        HWND lb = GetDlgItem(handle_, IDC_LB_TOOLS);
        last_sel_ = SendMessage(lb, LB_GETCURSEL, 0, 0);

        /* if there's a selection, get its toolitem */
        toolitem *t =
            (toolitem *)(last_sel_ != -1
                         ? SendMessage(lb, LB_GETITEMDATA, last_sel_, 0)
                         : 0);

        /* enable/disable the data fields according to the selection */
        EnableWindow(GetDlgItem(handle_, IDC_FLD_TITLE), t != 0);
        EnableWindow(GetDlgItem(handle_, IDC_FLD_CMDID), t != 0);
        EnableWindow(GetDlgItem(handle_, IDC_FLD_CMDFILE), t != 0);
        EnableWindow(GetDlgItem(handle_, IDC_FLD_PROGARGS), t != 0);
        EnableWindow(GetDlgItem(handle_, IDC_FLD_DIR), t != 0);
        EnableWindow(GetDlgItem(handle_, IDC_CK_CAPTURE), t != 0);

        /* set the values */
        SetDlgItemText(handle_, IDC_FLD_TITLE,
                       t != 0 ? t->title.get() : 0);
        SetDlgItemText(handle_, IDC_FLD_CMDID,
                       t != 0 ? t->cmdid.get() : 0);
        SetDlgItemText(handle_, IDC_FLD_CMDFILE,
                       t != 0 ? t->prog.get() : 0);
        SetDlgItemText(handle_, IDC_FLD_PROGARGS,
                       t != 0 ? t->args.get() : 0);
        SetDlgItemText(handle_, IDC_FLD_DIR,
                       t != 0 ? t->dir.get() : 0);
        CheckDlgButton(handle_, IDC_CK_CAPTURE,
                       (t != 0 ? t->capture : FALSE)
                       ? BST_CHECKED : BST_UNCHECKED);

        /* enable the Move Up/Down buttons as appropriate */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_UP), last_sel_ != 0);
        int cnt = SendMessage(lb, LB_GETCOUNT, 0, 0);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_MOVE_DOWN),
                     last_sel_ >= 0 && last_sel_ + 1 < cnt);

        /* we can warn about command ID if we have a valid field now */
        if (last_sel_ != -1)
            warned_cmdid_ = FALSE;
    }

    /* the last selected item - this is where we save on a selection change */
    int last_sel_;

    /* only warn about the command ID once per selection */
    int warned_cmdid_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for auto-hiding/showing of tool windows
 */
struct winmode_map
{
    int combo_id;
    const char *pref_key;
    int default_setting;
    HWND combo;
};

class CHtmlDialogDebugWinMode: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugWinMode(CHtmlSys_dbgmain *mainwin,
                            CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        winmode_map *m;

        /* inherit default handling */
        CHtmlDialogDebugPref::init();

        /* initialize our combo boxes */
        for (m = map_ ; m->pref_key != 0 ; ++m)
        {
            int val;

            /* get this combo */
            if ((m->combo = GetDlgItem(handle_, m->combo_id)) == 0)
                continue;

            /* load the strings */
            SendMessage(m->combo, CB_ADDSTRING, 0, (LPARAM)"Show");
            SendMessage(m->combo, CB_ADDSTRING, 0, (LPARAM)"Hide");
            SendMessage(m->combo, CB_ADDSTRING, 0, (LPARAM)"Unchanged");

            /* select the current or default setting */
            if (config_->getval("tool window mode switch", m->pref_key, &val))
                val = m->default_setting;

            /* map it to the combo index */
            val = (val == 's' ? 0 : val == 'h' ? 1 : 2);

            /* select the current setting */
            SendMessage(m->combo, CB_SETCURSEL, val, 0);
        }
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        /* check for a SELCHANGE in one of our combos */
        if (HIWORD(cmd) == CBN_SELCHANGE)
        {
            /* look for one of our combos */
            for (winmode_map *m = map_ ; m->pref_key != 0 ; ++m)
            {
                /* if it's a combo, treat this as a change */
                if (m->combo_id == LOWORD(cmd))
                {
                    /* note the update */
                    set_changed(TRUE);

                    /* done */
                    return TRUE;
                }
            }
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        winmode_map *m;

        /* check what we have */
        switch (nm->code)
        {
        case PSN_APPLY:
            /* save our updates */
            for (m = map_ ; m->pref_key != 0 ; ++m)
            {
                int idx;
                static int valmap[] = { 's', 'h', 'n' };

                /* skip combo boxes that aren't present */
                if (m->combo == 0)
                    continue;

                /* get the setting for this combo */
                idx = SendMessage(m->combo, CB_GETCURSEL, 0, 0);

                /* save it */
                config_->setval("tool window mode switch", m->pref_key,
                                valmap[idx]);
            }

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

protected:
    /* list mapping the combo boxes to the preference settings */
    static winmode_map map_[];
};

/* the map between our combo boxes and the config keys */
winmode_map CHtmlDialogDebugWinMode::map_[] =
{
    { IDC_CB_STKDBG, "stack.debug", 's' },
    { IDC_CB_STKDSN, "stack.design", 'h' },
    { IDC_CB_EXPDBG, "expr.debug", 's' },
    { IDC_CB_EXPDSN, "expr.design", 'h' },
    { IDC_CB_LCLDBG, "locals.debug", 's' },
    { IDC_CB_LCLDSN, "locals.design", 'h' },
    { IDC_CB_PRJDBG, "project.debug", 'n' },
    { IDC_CB_PRJDSN, "project.design", 'n' },
    { IDC_CB_LOGDBG, "log.debug", 'h' },
    { IDC_CB_LOGDSN, "log.design", 'h' },
    { IDC_CB_SCHDBG, "search.debug", 'h' },
    { IDC_CB_SCHDSN, "search.design", 'n' },
    { IDC_CB_TRCDBG, "trace.debug", 'n' },
    { IDC_CB_TRCDSN, "trace.design", 'h' },
    { IDC_CB_SCRDBG, "scripts.debug", 'n' },
    { IDC_CB_SCRDSN, "scripts.design", 'n' },
    { 0, 0 }
};


/* ------------------------------------------------------------------------ */
/*
 *   Key Map dialog
 */

/* current (unapplied) keymap linked list entry */
struct keycmd
{
    keycmd(const textchar_t *key, size_t keylen,
           const textchar_t *tab, const HtmlDbgCommandInfo *cmd, keycmd *nxt)
        : key(key, keylen), tab(tab)
    {
        this->cmd = cmd;
        this->nxt = nxt;
    }

    /* the next link in the list */
    keycmd *nxt;

    /* the key name */
    CStringBuf key;

    /* the table it's bound to */
    CStringBuf tab;

    /* the command info structure */
    const HtmlDbgCommandInfo *cmd;
};

/* the key map dialog class */
class CHtmlDialogDebugKeymap: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugKeymap(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
        /* load the current key list */
        key_head_ = key_tail_ = 0;
        load_from_config();

        /* load our backspace button */
        bmp_bksp_ = (HBITMAP)LoadImage(
            CTadsApp::get_app()->get_instance(),
            MAKEINTRESOURCE(IDB_BACKSPACE_BTN), IMAGE_BITMAP, 0, 0,
            LR_DEFAULTSIZE | LR_DEFAULTCOLOR | LR_LOADMAP3DCOLORS | LR_SHARED);
    }

    ~CHtmlDialogDebugKeymap()
    {
        /* delete our key list */
        empty_key_list();
    }

    void init()
    {
        /* load the initial list of commands into the command list */
        update_command_list();

        /* initialize the Assign and Remove buttons */
        update_assign_button();
        update_remove_button();

        /* set up the backspace button */
        SendMessage(GetDlgItem(handle_, IDC_BTN_BKSP), BM_SETIMAGE,
                    (WPARAM)IMAGE_BITMAP, (LPARAM)bmp_bksp_);

        /* load the key table list */
        HWND tabcb = GetDlgItem(handle_, IDC_CB_KEYCTX);
        for (HtmlKeyTable *kt = CHtmlSys_dbgmain::keytabs_ ;
             kt->name != 0 ; ++kt)
        {
            /* add the string */
            int idx = SendMessage(tabcb, CB_ADDSTRING, 0, (LPARAM)kt->name);

            /* if this is the first item we're adding, select it */
            if (kt == CHtmlSys_dbgmain::keytabs_)
                SendMessage(tabcb, CB_SETCURSEL, idx, 0);
        }

        /* subclass the new-key field for our special keystroke handling */
        HWND fld = GetDlgItem(handle_, IDC_FLD_NEWKEYS);
        oldfldproc_ = (WNDPROC)GetWindowLongPtr(fld, GWLP_WNDPROC);
        SetWindowLongPtr(fld, GWLP_WNDPROC, (LONG_PTR)&s_fldproc);
        SetProp(fld, self_prop_, this);

        /* subclass the listbox so that it works properly with the tooltip */
        HWND lb = GetDlgItem(handle_, IDC_LB_COMMANDS);
        oldlbproc_ = (WNDPROC)GetWindowLongPtr(lb, GWLP_WNDPROC);
        SetWindowLongPtr(lb, GWLP_WNDPROC, (LONG_PTR)&s_lbproc);
        SetProp(lb, self_prop_, this);

        /* set up the tooltip for the listbox */
        tt_ = CreateWindowEx(
            WS_EX_TOPMOST, TOOLTIPS_CLASS, 0,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            handle_, 0, CTadsApp::get_app()->get_instance(), 0);
        SetWindowPos(tt_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        /* our messages can be rather long, so use multiple lines as needed */
        SendMessage(tt_, TTM_SETMAXTIPWIDTH, 0, 320);

        /* add the listbox as a tool */
        TOOLINFO ti;
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = lb;
        ti.hinst = CTadsApp::get_app()->get_instance();
        ti.uId = 1;
        ti.lpszText = LPSTR_TEXTCALLBACK;
        GetClientRect(lb, &ti.rect);
        SendMessage(tt_, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        char fname[MAX_PATH];

        /* check for a SELCHANGE in one of our combos */
        switch (LOWORD(cmd))
        {
        case IDC_BTN_SAVEKEYS:
        case IDC_BTN_LOADKEYS:
            fname[0] = '\0';
            if (::browse_file(fname, sizeof(fname), handle_,
                              "Key map files\0*.keymap\0All Files\0*.*\0",
                              "Key Mapping File", "keymap",
                              OFN_HIDEREADONLY | OFN_PATHMUSTEXIST
                              | OFN_OVERWRITEPROMPT,
                              LOWORD(cmd) == IDC_BTN_LOADKEYS, 0))
            {
                if (LOWORD(cmd) == IDC_BTN_SAVEKEYS)
                {
                    /* save the working key list to the file */
                    if (!save_to_file(fname))
                        MessageBox(GetParent(handle_),
                                   "An error occurred saving the key map.",
                                   "TADS Workbench",
                                   MB_OK | MB_ICONEXCLAMATION);
                }
                else if (LOWORD(cmd) == IDC_BTN_LOADKEYS)
                {
                    /* load the keymap file into our working list */
                    if (load_from_file(fname))
                    {
                        /*
                         *   success - update the bound-key combo and
                         *   bound-command combo to reflect any changes in
                         *   the newly loaded map
                         */
                        update_key_combo();
                        update_cmd_combo();

                        /* mark the dialog as having unsaved changes */
                        set_changed(TRUE);
                    }
                }
            }

            /* handled */
            return TRUE;

        case IDC_FLD_PICKCMD:
            /*
             *   if this is a 'change' message, update the command list for
             *   the new filter string
             */
            if (HIWORD(cmd) == EN_CHANGE)
                update_command_list();

            /* handled */
            return TRUE;

        case IDC_LB_COMMANDS:
            /* check what kind of notification this is */
            if (HIWORD(cmd) == LBN_SELCHANGE)
            {
                /*
                 *   when the selected command changes, refresh the bound-key
                 *   combo to show the new command's keys
                 */
                update_key_combo();

                /*
                 *   update the "assign" button as well, since that's enabled
                 *   according to whether we have a command selected
                 */
                update_assign_button();
            }
            return TRUE;

        case IDC_CB_OLDKEYS:
            /* on a selection change, enable or disable the "remove" button */
            if (HIWORD(cmd) == CBN_SELCHANGE)
                update_remove_button();
            return TRUE;

        case IDC_BTN_ASSIGN:
            /* assign the current key to the current command */
            assign_key();

            /* update the key-list combo and the current-binding combo */
            update_key_combo();
            update_cmd_combo();
            return TRUE;

        case IDC_BTN_REMOVE:
            /* remove the currently selected key binding */
            remove_key();

            /* update the key-list combo and the current-binding combo */
            update_key_combo();
            update_cmd_combo();
            return TRUE;

        case IDC_BTN_BKSP:
            /* backspace button - delete the last key in the new key field */
            {
                char buf[128];
                char *p, *lasttok;
                HWND fld = GetDlgItem(handle_, IDC_FLD_NEWKEYS);

                /* get the current contents of the field */
                GetWindowText(fld, buf, sizeof(buf));

                /* find the last token in the buffer */
                for (p = lasttok = buf ; ; )
                {
                    /* find the next non-space character */
                    for ( ; isspace(*p) ; ++p) ;

                    /* if this is the end, we're done */
                    if (*p == '\0')
                        break;

                    /* this is the last token so far */
                    lasttok = p;

                    /* find the next token */
                    for ( ; *p != '\0' && !isspace(*p) ; ++p) ;
                }

                /* remove the last token */
                *lasttok = '\0';

                /* set the new text */
                SetWindowText(fld, buf);

                /* move the cursor to the end of the text */
                SendMessage(fld, EM_SETSEL, strlen(buf), strlen(buf));

                /* update the "assign" button and the current command combo */
                update_assign_button();
                update_cmd_combo();

                /* send focus back to the field */
                SetFocus(fld);
            }

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        /* check what we have */
        switch (nm->code)
        {
        case PSN_APPLY:
            /* save the keys */
            save_to_config();

            /* update the actual accelerator table in use */
            mainwin_->load_keys_from_config();

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

protected:
    /* assign the "new key" name to the selected command */
    void assign_key()
    {
        HtmlDbgCommandInfo *cmd;
        int idx;
        HWND lb = GetDlgItem(handle_, IDC_LB_COMMANDS);
        char key[256];
        char tab[256];
        char *p;

        /* the command info is the listbox item data */
        if ((idx = SendMessage(lb, LB_GETCURSEL, 0, 0)) == -1)
            return;
        cmd = (HtmlDbgCommandInfo *)SendMessage(lb, LB_GETITEMDATA, idx, 0);

        /* get the key name, and remove trailing spaces */
        GetDlgItemText(handle_, IDC_FLD_NEWKEYS, key, sizeof(key));
        for (p = key + strlen(key) ; p > key && isspace(*(p-1)) ; *--p = 0) ;

        /* get the table we're binding it to */
        GetDlgItemText(handle_, IDC_CB_KEYCTX, tab, sizeof(tab));

        /*
         *   since we're assigning a new meaning to this key, remove any old
         *   meaning from the key first
         */
        remove_key(key, tab);

        /*
         *   add the new key at the tail of the list - we want the most
         *   recent to take precedence for menus and for prefix/command
         *   clashes, and the later list item has precedence, so simply put
         *   it at the end of the list
         */
        keycmd *k = new keycmd(key, strlen(key), tab, cmd, 0);
        if (key_tail_ != 0)
            key_tail_->nxt = k;
        else
            key_head_ = k;
        key_tail_ = k;

        /* mark the dialog as having unsaved changes */
        set_changed(TRUE);

        /* clear the "new key" field to make way for the next key entry */
        SetDlgItemText(handle_, IDC_FLD_NEWKEYS, "");
    }

    /* remove the current "old key" selection's binding */
    void remove_key()
    {
        char key[256];
        char *tab;
        char *p;

        /* get the current "old key" selection, minus trailing spaces */
        GetDlgItemText(handle_, IDC_CB_OLDKEYS, key, sizeof(key));
        for (p = key + strlen(key) ; p > key && isspace(*(p-1)) ; *--p = 0) ;

        /* if the key name is empty, abort */
        if (key[0] == '\0')
            return;

        /* make sure there's a table name in parens after the key name */
        if (*(p-1) != ')' || (tab = strrchr(key, '(')) == 0)
            return;

        /* get the table name, and trim off the close paren */
        ++tab;
        *(p-1) = '\0';

        /* remove spaces between the key and table name */
        for (p = tab - 1, *p = '\0' ; p > key && isspace(*(p - 1)) ;
             *--p = '\0') ;

        /* if we still have a key name, remove it */
        if (key[0] != '\0')
            remove_key(key, tab);
    }

    /* remove the given key's binding */
    void remove_key(const textchar_t *key, const textchar_t *tab)
    {
        keycmd *cur, *prv;

        /* search the list for an entry with a matching name */
        for (cur = key_head_, prv = 0 ; cur != 0 ; prv = cur, cur = cur->nxt)
        {
            /* if this key and table names match, remove the key */
            if (stricmp(cur->key.get(), key) == 0
                && stricmp(cur->tab.get(), tab) == 0)
            {
                /* unlink it from the list */
                if (prv != 0)
                    prv->nxt = cur->nxt;
                else
                    key_head_ = cur->nxt;
                if (cur->nxt == 0)
                    key_tail_ = prv;

                /* delete it */
                delete cur;

                /* mark the dialog as having unsaved changes */
                set_changed(TRUE);

                /*
                 *   a key can only be mapped to one command, so there's no
                 *   need to look any further
                 */
                break;
            }
        }
    }

    /* get the listbox item under the mouse cursor */
    int get_lb_item_at_cursor(HWND lb)
    {
        /* get the cursor position */
        POINT pt;
        GetCursorPos(&pt);

        /* adjust to listbox client coordinates */
        ScreenToClient(lb, &pt);

        /* find the item at this location */
        return LOWORD(
            SendMessage(lb, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y)));
    }

    /* empty the current key list */
    void empty_key_list()
    {
        while (key_head_ != 0)
        {
            keycmd *nxt = key_head_->nxt;
            delete key_head_;
            key_head_ = nxt;
        }
        key_tail_ = 0;
    }

    /* load our working key list from the application's configuration */
    void load_from_config()
    {
        int i, cnt;
        HtmlKeyTable *tab;

        /* empty the current list */
        empty_key_list();

        /* run through each key table */
        for (tab = CHtmlSys_dbgmain::keytabs_ ; tab->name != 0 ; ++tab)
        {
            /* run through the mappings for this table */
            cnt = config_->get_strlist_cnt("key bindings", tab->name);
            for (i = 0 ; i < cnt ; ++i)
            {
                const textchar_t *p;
                const textchar_t *val;
                const HtmlDbgCommandInfo *cmd;

                /* load the next item */
                val = config_->getval_strptr("key bindings", tab->name, i);

                /* find the end of the key name */
                p = get_strrstr(val, " = ");

                /* if we didn't find it, ignore this entry */
                if (p == 0)
                    continue;

                /* look up the command name; if there's no match, skip it */
                if ((cmd = mainwin_->find_command(p + 3, TRUE)) == 0)
                    continue;

                /* create the new entry */
                keycmd *cur = new keycmd(val, p - val, tab->name, cmd, 0);

                /* link it at the end of our list */
                if (key_tail_ != 0)
                    key_tail_->nxt = cur;
                else
                    key_head_ = cur;
                key_tail_ = cur;
            }
        }
    }

    /* load our working key list from a file */
    int load_from_file(const textchar_t *fname)
    {
        keycmd *head = 0, *tail = 0;
        FILE *fp;
        const char *errmsg = 0;
        int linenum;
        char tab[256] = "Global";

        /* open the file */
        if ((fp = fopen(fname, "r")) == 0)
        {
            MessageBox(GetParent(handle_), "Unable to open mapping file",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }

        /* read the file */
        for (linenum = 1 ; ; ++linenum)
        {
            char buf[1024];
            char *p;
            char *dst;
            const HtmlDbgCommandInfo *cmd;

            /* read the next line */
            if (fgets(buf, sizeof(buf), fp) == 0)
                break;

            /* remove trailing newlines */
            for (p = buf + strlen(buf) ; p > buf && *(p-1) == '\n' ; --p) ;
            *p = '\0';

            /* trim leading spaces, and compress embedded spaces */
            for (p = dst = buf ; *p != '\0' ; ++p)
            {
                /* check for spaces */
                if (isspace(*p))
                {
                    /*
                     *   copy it only if we have a non-space character
                     *   preceding this one
                     */
                    if (dst != buf && !isspace(*(dst-1)))
                        *dst++ = *p;
                }
                else
                {
                    /* not a space - copy it as-is */
                    *dst++ = *p;
                }
            }

            /* remove trailing spaces */
            for ( ; dst != buf && isspace(*(dst-1)) ; --dst) ;

            /* null-terminate the line */
            *dst = '\0';

            /* skip empty and comment lines */
            if (buf[0] == '\0' || buf[0] == '#')
                continue;

            /* check for a table section */
            if (buf[0] == '[' && *(dst-1) == ']')
            {
                /* note the new section */
                *--dst = '\0';
                safe_strcpy(tab, sizeof(tab), buf+1);

                /* we're done with this line */
                continue;
            }

            /* find the " = "; if it's not there, it's an error */
            if ((p = get_strrstr(buf, " = ")) == 0)
            {
                errmsg = "Incorrect syntax - the format of each line "
                         "must be \"keylist = commandName\"";
                break;
            }

            /*
             *   Note whether or not it's an extension command.  Extension
             *   commands start with "Ext.".
             */
            int is_ext_cmd = (get_strlen(p+3) > 4
                              && memicmp(p + 3, "Ext.", 4) == 0);

            /*
             *   Look up the command name; add it if it's an undefined
             *   extension command, since we might just not have this
             *   extension loaded at the moment.  If this fails, it's an
             *   error.
             */
            if ((cmd = mainwin_->find_command(p + 3, is_ext_cmd)) == 0)
            {
                errmsg = "The specified command name doesn't exist";
                break;
            }

            /* validate and canonicalize the key name */
            *p = '\0';
            if (!CTadsApp::kb_->canonicalize_key_name(buf))
            {
                errmsg = "The key name is not valid";
                break;
            }

            /* create the mapping */
            keycmd *cur = new keycmd(buf, get_strlen(buf), tab, cmd, 0);

            /* link it into our list */
            if (tail != 0)
                tail->nxt = cur;
            else
                head = cur;
            tail = cur;
        }

        /* close the file */
        fclose(fp);

        /* if we were successful, save the new list */
        if (errmsg == 0)
        {
            /* success - delete the old list and install the new list */
            empty_key_list();
            key_head_ = head;
            key_tail_ = tail;

            /* success */
            return TRUE;
        }
        else
        {
            char errbuf[256];

            /* error - show the message */
            sprintf(errbuf, "Line %d:\r\n%s", linenum, errmsg);
            MessageBox(GetParent(handle_), errbuf, "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* dump the new list */
            while (head != 0)
            {
                keycmd *nxt = head->nxt;
                delete head;
                head = nxt;
            }

            /* failure */
            return FALSE;
        }
    }

    /* save the working list to the configuration file */
    void save_to_config()
    {
        HtmlKeyTable *tab;
        keycmd *cur;

        /* clear out the old configuration data for each table */
        for (tab = CHtmlSys_dbgmain::keytabs_ ; tab->name != 0 ; ++tab)
            config_->clear_strlist("key bindings", tab->name);

        /* run through our mapping list */
        for (cur = key_head_ ; cur != 0 ; cur = cur->nxt)
        {
            char buf[256];

            /* format the item string */
            sprintf(buf, "%s = %s", cur->key.get(), cur->cmd->sym);

            /* add it to the list for its table */
            config_->appendval("key bindings", cur->tab.get(), buf);
        }
    }

    /* save the working list to a text file */
    int save_to_file(const textchar_t *fname)
    {
        FILE *fp;
        keycmd *cur;

        /* open the file */
        if ((fp = fopen(fname, "w")) == 0)
            return FALSE;

        /* write the data */
        for (cur = key_head_ ; cur != 0 ; cur = cur->nxt)
            fprintf(fp, "%s = %s\n", cur->key.get(), cur->cmd->sym);

        /* done with the file - close it and return the file status */
        return (fclose(fp) != EOF);
    }

    /* update the "assign" button */
    void update_assign_button()
    {
        char buf[256];
        int idx;

        /* get the current "new key" field */
        GetDlgItemText(handle_, IDC_FLD_NEWKEYS, buf, sizeof(buf));

        /* get the current command listbox selection */
        idx = SendMessage(GetDlgItem(handle_, IDC_LB_COMMANDS),
                          LB_GETCURSEL, 0, 0);

        /* enable "assign" if we have a key string and a command selection */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_ASSIGN),
                     idx >= 0 && buf[0] != '\0');
    }

    /* update the "remove" button */
    void update_remove_button()
    {
        /* get the current selection */
        HWND cb = GetDlgItem(handle_, IDC_CB_OLDKEYS);
        int idx = SendMessage(cb, CB_GETCURSEL, 0, 0);
        int cnt = SendMessage(cb, CB_GETCOUNT, 0, 0);

        /* enable "remove" if an item is selected */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE),
                     idx >= 0 && cnt != 0);
    }

    /* update the command listbox based on the filter condition */
    void update_command_list()
    {
        char filter[128];
        size_t filter_len;
        char *src, *dst;
        int idx;
        char *seltxt = 0;
        HWND lb = GetDlgItem(handle_, IDC_LB_COMMANDS);
        const HtmlDbgCommandInfo *cmd;

        /* get the filter */
        GetDlgItemText(handle_, IDC_FLD_PICKCMD, filter, sizeof(filter));

        /* remove spaces from the filter string */
        for (src = dst = filter ; *src != '\0' ; ++src)
        {
            if (!isspace(*src))
                *dst++ = *src;
        }
        *dst = '\0';
        filter_len = dst - filter;

        /* remember the current selection */
        idx = SendMessage(lb, LB_GETCURSEL, 0, 0);
        if (idx != -1)
        {
            seltxt = new char[SendMessage(lb, LB_GETTEXTLEN, idx, 0) + 1];
            SendMessage(lb, LB_GETTEXT, idx, (LPARAM)seltxt);
        }

        /* clear out the list */
        SendMessage(lb, LB_RESETCONTENT, 0, 0);

        /* build the list */
        twb_command_iter iter(mainwin_);
        for (cmd = iter.first() ; cmd != 0 ; cmd = iter.next())
        {
            size_t rem = strlen(cmd->sym);
            const textchar_t *p;

            /* if this item isn't for this system version, skip it */
            if (!(cmd->sysid & w32_sysid))
                continue;

            /* if this item contains the filter string, include it */
            for (p = cmd->sym ; rem >= filter_len ; ++p, --rem)
            {
                if (filter_len == 0 || memicmp(p, filter, filter_len) == 0)
                {
                    /* include this item */
                    idx = SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)cmd->sym);

                    /* set its lparam to the command info structure */
                    SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)cmd);

                    /* if this is the previously selected item, note it */
                    if (seltxt != 0 && strcmp(seltxt, cmd->sym) == 0)
                        SendMessage(lb, LB_SETCURSEL, idx, 0);

                    /* no need to search further for the filter */
                    break;
                }
            }
        }

        /* make sure the selection is in view */
        if ((idx = SendMessage(lb, LB_GETCURSEL, 0, 0)) != -1)
            SendMessage(lb, LB_SETCURSEL, idx, 0);
        else
            SendMessage(lb, LB_SETCURSEL, 0,  0);

        /* update dependent items */
        update_key_combo();
        update_remove_button();

        /* delete the selection text if we allocated it */
        if (seltxt != 0)
            delete seltxt;
    }

    /* update the bound-key combo */
    void update_key_combo()
    {
        HWND cb = GetDlgItem(handle_, IDC_CB_OLDKEYS);
        HWND lb = GetDlgItem(handle_, IDC_LB_COMMANDS);
        keycmd *cur;
        int idx;

        /* clear out the combo's existing list */
        SendMessage(cb, CB_RESETCONTENT, 0, 0);

        /* get the listbox selection - if there isn't one, we're done */
        idx = SendMessage(lb, LB_GETCURSEL, 0, 0);
        if (idx == -1)
            return;

        /* get the command info from the selected item's data object */
        const HtmlDbgCommandInfo *cmd =
            (const HtmlDbgCommandInfo *)
            SendMessage(lb, LB_GETITEMDATA, idx, 0);

        /* find each key mapped to this command */
        for (cur = key_head_ ; cur != 0 ; cur = cur->nxt)
        {
            /* if this command matches, add the key to the list */
            if (cur->cmd == cmd)
            {
                char keybuf[256];

                /* build the display string: "key (table)" */
                sprintf(keybuf, "%s (%s)", cur->key.get(), cur->tab.get());

                /* add the string */
                SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)keybuf);
            }
        }

        /* select the first item in the combo */
        SendMessage(cb, CB_SETCURSEL, 0, 0);

        /* enable the 'remove' button if the list is non-empty */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE),
                     SendMessage(cb, CB_GETCOUNT, 0, 0) != 0);
    }

    /* update the bound-command list */
    void update_cmd_combo()
    {
        char buf[256];
        size_t len;
        char *p;
        keycmd *cur;
        int prefixcnt = 0;
        HWND cmdcb = GetDlgItem(handle_, IDC_CB_OLDCMD);

        /* assume we won't find a match, so clear out the list */
        SendMessage(cmdcb, CB_RESETCONTENT, 0, 0);

        /* get the current key list */
        GetDlgItemText(handle_, IDC_FLD_NEWKEYS, buf, sizeof(buf));

        /* remove the trailing space that we always add */
        for (p = buf + strlen(buf) ; p > buf && isspace(*(p-1)) ; *--p = 0) ;
        len = p - buf;

        /* scan for a matching list entry */
        for (cur = key_head_ ; cur != 0 ; cur = cur->nxt)
        {
            /* get this key name */
            const textchar_t *curkey = cur->key.get();

            /* check for a full match or a prefix match */
            if (stricmp(curkey, buf) == 0
                || (strlen(curkey) > len
                    && curkey[len] == ' '
                    && memicmp(buf, curkey, len) == 0))
            {
                char cmdbuf[256];

                /* it's a match - add it to the combo */
                sprintf(cmdbuf, "%s (%s (%s))",
                        cur->cmd->sym, curkey, cur->tab);
                SendMessage(cmdcb, CB_ADDSTRING, 0, (LPARAM)cmdbuf);
            }
        }

        /* select the first item in the list */
        SendMessage(cmdcb, CB_SETCURSEL, 0, 0);
    }

    /* our subclassed new-key field window procedure */
    LRESULT fldproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        int shift;
        char buf[256];
        char *p;
        int cnt;
        size_t len, len2;
        LRESULT ret;
        static const textchar_t *bad_keys[] =
        {
            "Left", "Right", "Up", "Down", "Insert", "Home", "End",
            "Delete", "PageUp", "PageDown", "Clear",
            "Tab", "Enter",
            0
        };

        /* check what we have */
        switch (msg)
        {
        case WM_DESTROY:
            /* on destruction, undo our subclassing */
            RemoveProp(hwnd, self_prop_);
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldfldproc_);

            /* proceed to the default handling */
            break;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            /* ignore right-clicks */
            return 0;

        case EM_SETSEL:
            /* only allow zero-length selections */
            lparam = wparam;
            break;

        case WM_SETFOCUS:
            /*
             *   When we gain focus, enter our own nested event loop, so that
             *   we can prevent the dialog box's event loop from interpreting
             *   Alt+Letter keys as accelerators to dialog controls.  We want
             *   the user to be able to enter arbitrary keys into this field,
             *   without having the keys have their usual special meanings in
             *   the dialog.  Keep control in the event loop until we get a
             *   killfocus.
             */

            /* first, pass the event along to the native control */
            ret = CallWindowProc(oldfldproc_, hwnd, msg, wparam, lparam);

            /* now process events in our own event loop until we lose focus */
            for (;;)
            {
                MSG msg;

                /* get the next message */
                switch (GetMessage(&msg, 0, 0, 0))
                {
                case -1:
                    /* failed exit our nested event loop */
                    return ret;

                case 0:
                    /* quitting - re-post the WM_QUIT and exit the loop */
                    PostQuitMessage(msg.wParam);
                    return ret;

                default:
                    /* dispatch the message normally */
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);

                    /* if we no longer have focus, we're done */
                    if (GetFocus() != hwnd)
                        return ret;
                    break;
                }
            }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            /* get the shift settings */
            shift = (CTadsWin::get_shift_key() ? CTKB_SHIFT : 0)
                    | (CTadsWin::get_ctl_key() ? CTKB_CTRL : 0)
                    | ((lparam & (1 << 29)) != 0 ? CTKB_ALT : 0);

            /* get our current contents */
            GetWindowText(hwnd, buf, sizeof(buf));

            /* count keys in the current buffer */
            for (cnt = 0, p = buf ; ; )
            {
                /* find the next non-space character */
                for ( ; isspace(*p) ; ++p) ;

                /* if this is the end, we're done */
                if (*p == '\0')
                    break;

                /* count it */
                ++cnt;

                /* find the next token */
                for ( ; *p != '\0' && !isspace(*p) ; ++p) ;
            }

            /*
             *   if we already have the maximum number of keys, don't allow
             *   another - just reset the buffer to empty
             */
            if (cnt >= 2)
                buf[0] = '\0';

            /* add this key name to the buffer */
            len = strlen(buf);
            CTadsApp::kb_->get_key_name(buf + len, sizeof(buf) - len,
                                        (int)wparam, shift);

            /* if we didn't find a name, ignore the key entirely */
            if (buf[len] == '\0')
                return TRUE;

            /* don't allow plain ASCII characters, or Shift+ASCII */
            len2 = strlen(buf);
            if (len2 == 1
                || (len2 == 5 && memicmp(buf, "Space", 5) == 0)
                || (len2 == 7 && memicmp(buf, "Shift+", 6) == 0))
            {
                /*
                 *   it's an unshifted ASCII character, or Shift+ASCII -
                 *   ignore the key entry and keep what we currently have
                 */
                return TRUE;
            }

            /* add a space after the key, in case they want to add another */
            strcat(buf + len, " ");

            /*
             *   if the field was full (two keys already), and the key they
             *   pressed was Esc or Backspace, take it to mean they simply
             *   want to clear out the field for another key
             */
            if (cnt == 2 && shift == 0
                && (wparam == VK_BACK || wparam == VK_ESCAPE))
                buf[0] = '\0';

            /* set the updated text */
            SetWindowText(hwnd, buf);

            /* move the cursor to the end of the text */
            SendMessage(hwnd, EM_SETSEL, strlen(buf), strlen(buf));

            /* update the "assign" button and the current command combo */
            update_assign_button();
            update_cmd_combo();

            /* we've fully handled the key */
            return TRUE;

        case WM_CHAR:
        case WM_SYSCHAR:
            /* ignore ordinary character keys */
            return 0;
        }

        /* inherit the original procedure */
        return CallWindowProc(oldfldproc_, hwnd, msg, wparam, lparam);
    }

    /* our static subclassed-text-field window procedure entrypoint */
    static LRESULT CALLBACK s_fldproc(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        CHtmlDialogDebugKeymap *self;

        /* find our 'this' object; if we can't find it, we can't proceed */
        if ((self = (CHtmlDialogDebugKeymap *)GetProp(hwnd, self_prop_)) == 0)
            return 0;

        /* call our virtual window procedure */
        return self->fldproc(hwnd, msg, wparam, lparam);
    }

    /* our subclassed listbox window procedure */
    LRESULT lbproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        TOOLINFO ti;

        /* check what we have */
        switch (msg)
        {
        case WM_DESTROY:
            /* on destruction, undo our subclassing */
            RemoveProp(hwnd, self_prop_);
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldlbproc_);

            /* proceed to the default handling */
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            /*
             *   Reset the tooltip on any click - this will ensure that it
             *   comes back up again in due course.  If we didn't do this,
             *   the tooltip would normally wait until the user moves the
             *   mouse into a new control before displaying a tip in this
             *   control again, which in our case would mean waiting forever.
             */
            SendMessage(tt_, TTM_ACTIVATE, FALSE, 0);
            SendMessage(tt_, TTM_ACTIVATE, TRUE, 0);
            break;

        case WM_NOTIFY:
            {
                NMHDR *nm = (NMHDR *)lparam;

                switch (nm->code)
                {
                case TTN_GETDISPINFO:
                    /* get the tooltip message */
                    {
                        /* cast the notifier data */
                        NMTTDISPINFO *nmtt = (NMTTDISPINFO *)nm;

                        /* get the item at the cursor */
                        int idx = get_lb_item_at_cursor(hwnd);
                        if (idx != -1)
                        {
                            /*
                             *   the message is the description from the
                             *   command info structure, which is the listbox
                             *   item's data object
                             */
                            const HtmlDbgCommandInfo *cmd =
                                (const HtmlDbgCommandInfo *)
                                SendMessage(hwnd, LB_GETITEMDATA, idx, 0);

                            /* return the message */
                            nmtt->lpszText = (textchar_t *)cmd->tiptext;
                        }
                        else
                        {
                            /* not found - no message */
                            nmtt->lpszText = "Hover over a command to show "
                                             "the command's description.";
                        }
                    }
                    return TRUE;

                case TTN_POP:
                    /*
                     *   when the tooltip disappears, reset its active tool
                     *   rectangle to the whole listbox window
                     */
                    ti.cbSize = sizeof(ti);
                    ti.hwnd = hwnd;
                    ti.uId = 1;
                    GetClientRect(hwnd, &ti.rect);
                    SendMessage(tt_, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
                    return TRUE;

                case TTN_SHOW:
                    /*
                     *   when the tooltip appears, reset its active tool
                     *   rectangle to just the area of the item under the
                     *   cursor
                     */
                    {
                        TOOLINFO ti;
                        RECT rc;

                        /* find the item at the cursor */
                        int idx = get_lb_item_at_cursor(hwnd);
                        if (idx != -1)
                        {
                            /* use the item's rectangle as the bounding area */
                            SendMessage(hwnd, LB_GETITEMRECT, idx,
                                        (LPARAM)&rc);
                        }
                        else
                        {
                            POINT pt;

                            /*
                             *   remove the tooltip when if we stray much
                             *   above the cursor, since there could be an
                             *   item above if we're in the empty part at the
                             *   end of the list
                             */
                            GetCursorPos(&pt);
                            ScreenToClient(hwnd, &pt);
                            GetClientRect(hwnd, &rc);
                            rc.top = pt.y - 5;
                        }

                        /* set the new area */
                        ti.cbSize = sizeof(ti);
                        ti.hwnd = hwnd;
                        ti.uId = 1;
                        ti.rect = rc;
                        SendMessage(tt_, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
                    }
                    return TRUE;
                }
                break;
            }
            break;
        }

        /* inherit the original procedure */
        return CallWindowProc(oldlbproc_, hwnd, msg, wparam, lparam);
    }

    /* our static subclassed-listbox window procedure entrypoint */
    static LRESULT CALLBACK s_lbproc(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        CHtmlDialogDebugKeymap *self;

        /* find our 'this' object; if we can't find it, we can't proceed */
        if ((self = (CHtmlDialogDebugKeymap *)GetProp(hwnd, self_prop_)) == 0)
            return 0;

        /* call our virtual window procedure */
        return self->lbproc(hwnd, msg, wparam, lparam);
    }

    /*
     *   Our working keymap.  This is the temporary keymap with unapplied
     *   user changes.  We load this from the current app accelerator table
     *   when we initialize, and store it to the app accelerator on "Apply".
     *   This is the list we display and the list we update via the dialog
     *   box, and it's the list that we save in response to "Save Keymap".
     *   We load a file into this list on "Load Keymap".  We keep this as a
     *   simple linked list of key/command pairs.
     */
    keycmd *key_head_, *key_tail_;

    /* bitmap for the backspace button */
    HBITMAP bmp_bksp_;

    /* listbox tooltip */
    HWND tt_;

    /* listbox subclassing */
    WNDPROC oldlbproc_;

    /* new-key field subclassing */
    WNDPROC oldfldproc_;

    /* "self" window property for subclassed controls */
    static char *self_prop_;
};

char *CHtmlDialogDebugKeymap::self_prop_ =
    "CHtmlDialogDebugKeymap.listbox.self";


/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for prompts
 */
class CHtmlDialogDebugPrompt: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugPrompt(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* read a config file setting */
    void read_config(const char *varname, const char *varname2,
                     int ckbox_id, int *internal_var, int default_val);

    /* get a checkbox into our internal state */
    void get_ck_state(int id, int *var);

    /* update button dependencies */
    void update_buttons()
    {
    }

    /* prompt settings */
    int welcome_;
    int gameover_;
    int bpmove_;
    int ask_exit_;
    int ask_load_;
    int ask_rlsrc_;
    int clr_dbglog_;
    int save_on_build_;
    int ask_save_on_build_;
    int ask_autoadd_;
};

/*
 *   read a stored config setting and set our checkbox and internal state
 *   accordingly
 */
void CHtmlDialogDebugPrompt::read_config(const char *varname,
                                         const char *varname2,
                                         int ckbox_id, int *internal_var,
                                         int default_val)
{
    /* get the config setting; if it's not defined, use the default value */
    if (config_->getval(varname, varname2, internal_var))
        *internal_var = default_val;

    /* set the checkbox */
    CheckDlgButton(handle_, ckbox_id,
                   *internal_var ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   initialize
 */
void CHtmlDialogDebugPrompt::init()
{
    /* get the initial settings */
    read_config("ask autoadd new file", 0, IDC_CK_ASK_AUTOADDSRC,
                &ask_autoadd_, TRUE);
    read_config("gameover dlg", 0, IDC_CK_SHOW_GAMEOVER, &gameover_, TRUE);
    read_config("bpmove dlg", 0, IDC_CK_SHOW_BPMOVE, &bpmove_, TRUE);
    read_config("ask exit dlg", 0, IDC_CK_ASK_EXIT, &ask_exit_, TRUE);
    read_config("ask load dlg", 0, IDC_CK_ASK_LOAD, &ask_load_, TRUE);
    read_config("ask reload src", 0, IDC_CK_ASK_RLSRC, &ask_rlsrc_, TRUE);
    read_config("dbglog", "clear on build", IDC_CK_CLR_DBGLOG,
                &clr_dbglog_, TRUE);
    read_config("save on build", 0, IDC_CK_SAVE_ON_BUILD,
                &save_on_build_, TRUE);
    read_config("ask save on build", 0, IDC_CK_ASK_SAVE_ON_BUILD,
                &ask_save_on_build_, TRUE);

    /* initialize button dependencies */
    update_buttons();

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   get a checkbutton's state into our internal variable
 */
void CHtmlDialogDebugPrompt::get_ck_state(int id, int *var)
{
    /* get the button state and set the variable accordingly */
    *var = (IsDlgButtonChecked(handle_, id) == BST_CHECKED);
}

/*
 *   process a command
 */
int CHtmlDialogDebugPrompt::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDC_CK_SHOW_GAMEOVER:
    case IDC_CK_SHOW_BPMOVE:
    case IDC_CK_ASK_EXIT:
    case IDC_CK_ASK_LOAD:
    case IDC_CK_ASK_RLSRC:
    case IDC_CK_CLR_DBGLOG:
    case IDC_CK_SAVE_ON_BUILD:
    case IDC_CK_ASK_AUTOADDSRC:
    case IDC_CK_ASK_SAVE_ON_BUILD:
        /* update our internal state */
        get_ck_state(IDC_CK_SHOW_GAMEOVER, &gameover_);
        get_ck_state(IDC_CK_SHOW_BPMOVE, &bpmove_);
        get_ck_state(IDC_CK_ASK_EXIT, &ask_exit_);
        get_ck_state(IDC_CK_ASK_LOAD, &ask_load_);
        get_ck_state(IDC_CK_ASK_RLSRC, &ask_rlsrc_);
        get_ck_state(IDC_CK_CLR_DBGLOG, &clr_dbglog_);
        get_ck_state(IDC_CK_SAVE_ON_BUILD, &save_on_build_);
        get_ck_state(IDC_CK_ASK_SAVE_ON_BUILD, &ask_save_on_build_);
        get_ck_state(IDC_CK_ASK_AUTOADDSRC, &ask_autoadd_);

        /* update button dependencies */
        update_buttons();

        /* note the change */
        set_changed(TRUE);

        /* handled */
        return TRUE;

    default:
        /* inherit default */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }
}

/*
 *   process a notification
 */
int CHtmlDialogDebugPrompt::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new settings */
        config_->setval("gameover dlg", 0, gameover_);
        config_->setval("bpmove dlg", 0, bpmove_);
        config_->setval("ask exit dlg", 0, ask_exit_);
        config_->setval("ask load dlg", 0, ask_load_);
        config_->setval("ask reload src", 0, ask_rlsrc_);
        config_->setval("dbglog", "clear on build", clr_dbglog_);
        config_->setval("save on build", 0, save_on_build_);
        config_->setval("ask save on build", 0, ask_save_on_build_);
        config_->setval("ask autoadd new file", 0, ask_autoadd_);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for Auto-Build settings
 */
class CHtmlDialogDebugAutoBuild: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugAutoBuild(CHtmlSys_dbgmain *mainwin,
                              CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* update button dependencies */
    void update_buttons()
    {
        /*
         *   we can ask about saving on build only if we save on build in the
         *   first place
         */
        EnableWindow(GetDlgItem(handle_, IDC_CK_ASK_SAVE_ON_BUILD),
                     IsDlgButtonChecked(handle_, IDC_CK_SAVE_ON_BUILD));
    }
};

/*
 *   initialize
 */
void CHtmlDialogDebugAutoBuild::init()
{
    /* get the initial settings */
    CheckDlgButton(handle_, IDC_CK_SAVE_ON_BUILD,
                   config_->getval_int("save on build", 0, TRUE)
                   ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_ASK_SAVE_ON_BUILD,
                   config_->getval_int("ask save on build", 0, TRUE)
                   ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_CK_AUTOBUILD,
                   config_->getval_int("build on run", 0, TRUE)
                   ? BST_CHECKED : BST_UNCHECKED);

    /* initialize button dependencies */
    update_buttons();

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a command
 */
int CHtmlDialogDebugAutoBuild::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDC_CK_SAVE_ON_BUILD:
    case IDC_CK_ASK_SAVE_ON_BUILD:
    case IDC_CK_AUTOBUILD:
        /* note changes */
        set_changed(TRUE);

        /* update button dependencies */
        update_buttons();

        /* handled */
        return TRUE;

    default:
        /* inherit default */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }
}

/*
 *   process a notification
 */
int CHtmlDialogDebugAutoBuild::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new settings */
        config_->setval("save on build", 0,
                        IsDlgButtonChecked(handle_, IDC_CK_SAVE_ON_BUILD)
                        == BST_CHECKED);
        config_->setval("ask save on build", 0,
                        IsDlgButtonChecked(handle_, IDC_CK_ASK_SAVE_ON_BUILD)
                        == BST_CHECKED);
        config_->setval("build on run", 0,
                        IsDlgButtonChecked(handle_, IDC_CK_AUTOBUILD)
                        == BST_CHECKED);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for source file paths
 */
class CHtmlDialogDebugSrcpath: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugSrcpath(CHtmlSys_dbgmain *mainwin,
                            CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogDebugSrcpath::init()
{
    /*
     *   ask the debugger to make sure the config is up to date with its
     *   internal source file list
     */
    mainwin_->srcdir_list_to_config();

    /* load the source path */
    load_multiline_fld("srcdir", 0, IDC_FLD_SRCPATH, TRUE);

    /* set the read-only option */
    CheckDlgButton(
        handle_, IDC_CK_READONLY,
        config_->getval_int("editor", "open libs readonly", FALSE));

    /*
     *   For tads 2, if there's no workspace is open, disable the field,
     *   because a source path has no meaning outside of an active workspace.
     *   In tads 3, the field is always enabled because this is global
     *   configuration data.
     */
    if (!mainwin_->get_workspace_open() && w32_system_major_vsn < 3)
        EnableWindow(GetDlgItem(handle_, IDC_FLD_SRCPATH), FALSE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}


/*
 *   process a command
 */
int CHtmlDialogDebugSrcpath::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];

    /* check the control that was activated */
    switch(LOWORD(cmd))
    {
    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    case IDC_BTN_ADDDIR:
        /*
         *   for tads 2, if no workspace is open, simply tell them why they
         *   can't add a directory at this time
         */
        if (!mainwin_->get_workspace_open() && w32_system_major_vsn < 3)
        {
            /* show the message */
            MessageBox(GetParent(handle_),
                       "You must load a game before you can set the source "
                       "file search path, because the path needs to be "
                       "associated with a game.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* handled */
            return TRUE;
        }

        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (browse_folder(GetParent(handle_),
                          "Folder to add to source &path:",
                          "Add Folder to Source Search Path",
                          fname, sizeof(fname), 0, FALSE))
        {
            HWND fld;

            /* add the folder to our list */
            fld = GetDlgItem(handle_, IDC_FLD_SRCPATH);
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)fname);

            /* add a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* handled */
        return TRUE;

    case IDC_FLD_SRCPATH:
        if (HIWORD(cmd) == EN_CHANGE)
        {
            /* mark the data as updated */
            set_changed(TRUE);

            /* handled */
            return TRUE;
        }
        break;

    case IDC_CK_READONLY:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    default:
        /* no special handling */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/*
 *   process a notification
 */
int CHtmlDialogDebugSrcpath::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new source path */
        save_multiline_fld("srcdir", 0, IDC_FLD_SRCPATH, TRUE, TRUE);

        /* tell the debugger to rebuild its internal source path list */
        mainwin_->rebuild_srcdir_list();

        /* save the lib-files-readonly option */
        config_->setval("editor", "open libs readonly",
                        IsDlgButtonChecked(handle_, IDC_CK_READONLY)
                        == BST_CHECKED);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger preferences page dialog for the extensions path
 */
class CHtmlDialogDebugExts: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugExts(CHtmlSys_dbgmain *mainwin,
                         CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogDebugExts::init()
{
    /* load the extensions path */
    load_field("extensions dir", 0, IDC_FLD_EXTPATH);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}


/*
 *   process a command
 */
int CHtmlDialogDebugExts::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];

    /* check the control that was activated */
    switch(LOWORD(cmd))
    {
    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    case IDC_BTN_BROWSE:
        /* start in the folder of the current setting, if any */
        GetDlgItemText(handle_, IDC_FLD_EXTPATH, fname, sizeof(fname));
        if (fname[0] == '\0')
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (browse_folder(
            GetParent(handle_), "Extensions &Folder:",
            "Set Extensions Folder", fname, sizeof(fname), fname, FALSE))
        {
            /* update the field */
            SetDlgItemText(handle_, IDC_FLD_EXTPATH, fname);
        }

        /* handled */
        return TRUE;

    case IDC_FLD_EXTPATH:
        if (HIWORD(cmd) == EN_CHANGE)
        {
            /* mark the data as updated */
            set_changed(TRUE);

            /* handled */
            return TRUE;
        }
        break;

    default:
        /* no special handling */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/*
 *   process a notification
 */
int CHtmlDialogDebugExts::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new source path */
        save_field("extensions dir", 0, IDC_FLD_EXTPATH, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Editor Auto-Configuration sub-dialog
 */

/* editor information structure */
struct auto_config_info
{
    auto_config_info(const char *disp_name, const char *prog,
                     const char *cmdline)
    {
        /* allocate space for all three strings */
        this->disp_name = (char *)th_malloc(strlen(disp_name) + 1
                                            + strlen(prog) + 1
                                            + strlen(cmdline) + 1);

        /* copy the display name */
        strcpy(this->disp_name, disp_name);

        /* copy the program name after that */
        this->prog = this->disp_name + strlen(this->disp_name) + 1;
        strcpy(this->prog, prog);

        /* copy the command line after that */
        this->cmdline = this->prog + strlen(this->prog) + 1;
        strcpy(this->cmdline, cmdline);
    }

    ~auto_config_info()
    {
        /* delete our string buffer */
        th_free(disp_name);
    }

    /* display name */
    char *disp_name;

    /* the program executable name, or the DDE string, as applicable */
    char *prog;

    /* command line */
    char *cmdline;
};

class CTadsDialogEditAuto: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(CHtmlDialogDebugPref *editor_dlg)
    {
        CTadsDialogEditAuto *dlg;

        /* create the dialog */
        dlg = new CTadsDialogEditAuto(editor_dlg);

        /* run the dialog */
        dlg->run_modal(DLG_DBG_EDITOR_AUTOCONFIG,
                       GetParent(editor_dlg->get_handle()),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        size_t i;
        HWND lb;
        HKEY hkey;
        DWORD disposition;

        /* allocate space for our editor list */
        editor_cnt_ = 10;
        editors_ = (auto_config_info **)
                   th_malloc(editor_cnt_ * sizeof(editors_[0]));

        /* clear the list */
        for (i = 0 ; i < editor_cnt_ ; ++i)
            editors_[i] = 0;

        /* we don't have anything in the list yet */
        i = 0;

        /* add Notepad */
        editors_[i++] = new auto_config_info("Notepad",
                                             "NOTEPAD.EXE", "%f");

        /* add Epsilon - read the registry to get its install directory */
        hkey = CTadsRegistry::open_key(HKEY_CURRENT_USER,
                                       "Software\\Lugaru\\Epsilon",
                                       &disposition, FALSE);
        if (hkey != 0)
        {
            DWORD idx;

            /* scan for a value with a name like "BinPathNN" */
            for (idx = 0 ; ; ++idx)
            {
                char namebuf[128];
                DWORD namesiz = sizeof(namebuf);
                BYTE databuf[OSFNMAX + 1];
                DWORD datasiz = sizeof(databuf);
                DWORD valtype;

                /* get the new value - give up on failure */
                if (RegEnumValue(hkey, idx, namebuf, &namesiz, 0,
                                 &valtype, databuf, &datasiz)
                    != ERROR_SUCCESS)
                    break;

                /* if this value starts with "BinPath", it's the one */
                if (valtype == REG_SZ
                    && strlen(namebuf) >= 7
                    && memicmp(namebuf, "BinPath", 7) == 0)
                {
                    char path[OSFNMAX];

                    /*
                     *   the value is the path to the Epsilon binaries
                     *   directory - build the full path to the SendEps
                     *   filename
                     */
                    os_build_full_path(path, sizeof(path),
                                       (char *)databuf, "SendEps.exe");

                    /* add the configuration item */
                    editors_[i++] = new auto_config_info(
                        "Epsilon", path, "\"+%n:%0c\" \"%f\"");

                    /* we're done searching for keys */
                    break;
                }
            }

            /* done with the key */
            CTadsRegistry::close_key(hkey);
        }

        /* check to see if Imaginate is installed, and add it if so */
        hkey = CTadsRegistry::open_key(HKEY_LOCAL_MACHINE,
                                       "Software\\Imaginate",
                                       &disposition, FALSE);
        if (hkey != 0)
        {
            char dir[OSFNMAX];

            /* read the "Location" value */
            if (CTadsRegistry::query_key_str(hkey, "Location",
                                             dir, sizeof(dir)) != 0)
            {
                char path[OSFNMAX];

                /* that's the directory - build the full path */
                os_build_full_path(path, sizeof(path), dir, "Imaginate.exe");

                /* add the configuration item */
                editors_[i++] = new auto_config_info(
                    "Imaginate", path, "-f\"%f\"");
            }

            /* done with the key */
            CTadsRegistry::close_key(hkey);
        }

        /* populate the editors with file type associations */
        add_auto_config(".c");
        add_auto_config(".h");
        add_auto_config(".cpp");
        add_auto_config(".txt");

        /* get the listbox's handle */
        lb = GetDlgItem(handle_, IDC_LB_EDITORS);

        /* populate our list box */
        for (i = 0 ; editors_[i] != 0 ; ++i)
        {
            int idx;

            /* add this editor's name to the list box */
            idx = SendMessage(lb, LB_ADDSTRING,
                              0, (LPARAM)editors_[i]->disp_name);

            /* set the item data to our editor info structure */
            SendMessage(lb, LB_SETITEMDATA, idx, (LPARAM)editors_[i]);
        }

        /* we have no initial selection, so disable the OK button */
        EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
    }

    /*
     *   Add an auto-configuration item for a file association
     */
    void add_auto_config(const char *suffix)
    {
        HKEY key;
        HKEY shell_key;
        DWORD disposition;
        char filetype[256];
        DWORD idx;

        /* find the registry key for the file suffix in CLASSES_ROOT */
        if (!get_key_val_str(HKEY_CLASSES_ROOT, suffix, 0,
                             filetype, sizeof(filetype)))
            return;

        /*
         *   the value of the suffix key's default value is the type name
         *   for the suffix; the type name is in turn another key, under
         *   which we can find the launch information for the file type
         */
        key = CTadsRegistry::open_key(HKEY_CLASSES_ROOT, filetype,
                                      &disposition, FALSE);
        if (key == 0)
            return;

        /*
         *   we're interested in the "Shell" subkey of this key - under this
         *   key we'll find the various shell commands for the file type
         */
        shell_key = CTadsRegistry::open_key(key, "Shell", &disposition, FALSE);

        /* we're done with the file type key */
        CTadsRegistry::close_key(key);

        /* enumerate the subkeys */
        for (idx = 0 ; ; ++idx)
        {
            LONG stat;
            char namebuf[256];
            char namebuf2[256];
            DWORD namelen;
            FILETIME wtime;
            HKEY item_key;
            char *src;
            char *dst;

            /* get the next key */
            namelen = sizeof(namebuf);
            stat = RegEnumKeyEx(shell_key, idx, namebuf, &namelen,
                                0, 0, 0, &wtime);

            /* if that was the last key, we're done */
            if (stat == ERROR_NO_MORE_ITEMS)
                break;

            /* remove any '&' we find in the buffer */
            for (src = namebuf, dst = namebuf2 ; *src != '\0' ; ++src)
            {
                /* copy anything buf '&' to the output */
                if (*src != '&')
                    *dst++ = *src;
            }

            /* null-terminate the output */
            *dst = '\0';

            /* if the name string doesn't start with "Open", skip it */
            if (strlen(namebuf2) < 4 || memicmp(namebuf2, "open", 4) != 0)
                continue;

            /* open the subkey */
            item_key = CTadsRegistry::open_key(shell_key, namebuf,
                                               &disposition, FALSE);
            if (item_key != 0)
            {
                HKEY dde_key;
                char dispname[256];
                char cmdbuf[256];
                char appbuf[256];
                char topicbuf[256];
                char argbuf[256];
                char *cmdroot;
                char *cmdext;
                size_t i;
                char dde_string[1024];
                char *p;

                /* get the "command" subkey's contents */
                if (!get_key_val_str(item_key, "command", 0,
                                     cmdbuf, sizeof(cmdbuf)))
                    cmdbuf[0] = '\0';

                /* get the arguments, which are with the "ddeexec" key */
                if (!get_key_val_str(item_key, "ddeexec", 0,
                                     argbuf, sizeof(argbuf)))
                    argbuf[0] = '\0';

                /* get the "ddeexec" subkey */
                dde_key = CTadsRegistry::open_key(item_key, "ddeexec",
                    &disposition, FALSE);
                if (dde_key != 0)
                {
                    /* get the "application" subkey */
                    if (!get_key_val_str(dde_key, "application", 0,
                                         appbuf, sizeof(appbuf)))
                        appbuf[0] = '\0';

                    /* get the "topic" subkey */
                    if (!get_key_val_str(dde_key, "topic", 0,
                                         topicbuf, sizeof(topicbuf)))
                        topicbuf[0] = '\0';

                    /* done with the DDE key */
                    CTadsRegistry::close_key(dde_key);
                }

                /* done with the item key - close it */
                CTadsRegistry::close_key(item_key);

                /* if we didn't find one or more of the values, skip it */
                if (cmdbuf[0] == '\0' || argbuf[0] == '\0'
                    || appbuf[0] == '\0' || topicbuf[0] == '\0')
                    continue;

                /*
                 *   Pull out the program name from the command buffer.  If
                 *   the command starts with a double quote, pull out the
                 *   part up to the close quote.  Otherwise, pull out the
                 *   part before the first space.
                 */
                if (cmdbuf[0] == '"')
                {
                    /*
                     *   move everything down a byte, until we find the
                     *   close quote
                     */
                    for (dst = cmdbuf, src = cmdbuf + 1 ; *src != '\0' ;
                         ++src)
                    {
                        /*
                         *   If we're at a close quote, we're done.  If the
                         *   quote is stuttered, it's intended to indicate a
                         *   single double-quote - just copy one of them in
                         *   this case, and keep going, since it's not the
                         *   closing quote.
                         */
                        if (*src == '"')
                        {
                            /* check for stuttering */
                            if (*(src+1) == '"')
                            {
                                /* it's stuttered - just copy one */
                                *dst++ = '"';
                                ++src;
                            }
                            else
                            {
                                /* that's the end of the command token */
                                break;
                            }
                        }
                        else
                        {
                            /* copy the character as it is */
                            *dst++ = *src;
                        }
                    }

                    /* null-terminate the result */
                    *dst = '\0';
                }
                else
                {
                    /* find the first space, and terminate there */
                    p = strchr(cmdbuf, ' ');
                    if (p != 0)
                        *p = '\0';
                }

                /* get the root of the program name */
                cmdroot = os_get_root_name(cmdbuf);

                /* find the extension */
                cmdext = strrchr(cmdroot, '.');
                if (cmdext == 0)
                    cmdext = cmdroot + strlen(cmdroot);

                /*
                 *   pull out the command root name, sans path or extension
                 *   - this is the display name
                 */
                memcpy(dispname, cmdroot, cmdext - cmdroot);
                dispname[cmdext - cmdroot] = '\0';

                /* set up the DDE command for this item */
                sprintf(dde_string, "DDE:%s,%s,%s", appbuf, topicbuf, cmdbuf);

                /*
                 *   update the argument buffer - if we find a "%1", change
                 *   it to "%f", since that's our notation for the filename
                 */
                for (p = argbuf ; *p != '\0' ; ++p)
                {
                    /* if it's "%1", change it to "%f" */
                    if (*p == '%' && *(p+1) == '1')
                        *(p+1) = 'f';
                }

                /* scan for another entry with the same program name */
                for (i = 0 ; i < editor_cnt_ && editors_[i] != 0 ; ++i)
                {
                    /*
                     *   if the display names match (ignoring case), we
                     *   already have this command in our list
                     */
                    if (stricmp(dispname, editors_[i]->disp_name) == 0)
                    {
                        /*
                         *   This one matches.  Use the newer version in
                         *   preference to the older one, since the older
                         *   one could have been statically added and thus
                         *   mioght not have included the full program name.
                         *   If we find the same program name in the
                         *   registry that we added statically, the registry
                         *   version is better because it'll have the full
                         *   path.
                         */
                        delete editors_[i];
                        editors_[i] = 0;

                        /* no need to look any further */
                        break;
                    }
                }

                /* set up the new editor at the index we landed on */
                editors_[i] =
                    new auto_config_info(dispname, dde_string, argbuf);
            }
        }

        /* we're done with the shell key */
        CTadsRegistry::close_key(shell_key);
    }

    /*
     *   Get the string value of a key.  Returns true on success, false if
     *   there is no such key or no such value.
     */
    static int get_key_val_str(HKEY parent_key, const char *key_name,
                               const char *val_name, char *buf, size_t buflen)
    {
        HKEY key;
        DWORD disposition;
        int ret;

        /* presume failure */
        ret = FALSE;

        /* open the key */
        key = CTadsRegistry::open_key(parent_key, key_name,
                                      &disposition, FALSE);

        /* if there's no key, indicate failure */
        if (key == 0)
            return FALSE;

        /* if the value exists, retrieve it */
        if (CTadsRegistry::value_exists(key, val_name))
        {
            /* get the value */
            CTadsRegistry::query_key_str(key, val_name, buf, buflen);

            /* success */
            ret = TRUE;
        }

        /* we're done with the key - close it */
        CTadsRegistry::close_key(key);

        /* return our results */
        return ret;
    }

    /* destroy */
    void destroy()
    {
        size_t i;

        /* delete each editor configuration we allocated */
        for (i = 0 ; i < editor_cnt_ ; ++i)
        {
            /* if we allocated it, delete it */
            if (editors_[i] != 0)
                delete editors_[i];
        }

        /* delete the editor list itself */
        th_free(editors_);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        int sel;
        HWND lb;

        /* see what command we're processing */
        switch(LOWORD(id))
        {
        case IDC_LB_EDITORS:
            /* it's a listbox notification - get the listbox handle */
            lb = GetDlgItem(handle_, IDC_LB_EDITORS);

            /* see what kind of notification we have */
            switch(HIWORD(id))
            {
            case LBN_SELCHANGE:
                /* the selection is changing - get the new selection */
                sel = SendMessage(lb, LB_GETCURSEL, 0, 0);

                /*
                 *   enable the "OK" button if we have a valid selection,
                 *   disable it if not
                 */
                EnableWindow(GetDlgItem(handle_, IDOK), (sel != LB_ERR));
                break;

            case LBN_DBLCLK:
                /*
                 *   they double-clicked on a selection - if we have a valid
                 *   selection, treat this like an "OK" click
                 */
                sel = SendMessage(lb, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR)
                {
                    /* change the event to IDOK */
                    id = IDOK;

                    /* go process it like an "OK" button event */
                    goto do_ok_btn;
                }
            }

            /* proceed to inherited handling */
            break;

        case IDOK:
        do_ok_btn:
            /* get the listbox handle */
            lb = GetDlgItem(handle_, IDC_LB_EDITORS);

            /* get the currently selected item from the listbox */
            sel = SendMessage(lb, LB_GETCURSEL, 0, 0);

            /*
             *   if an item is selected, fill in its information in the
             *   parent dialog
             */
            if (sel != LB_ERR)
            {
                auto_config_info *info;

                /*
                 *   get the pointer to our information structure for the
                 *   item
                 */
                info = (auto_config_info *)
                       SendMessage(lb, LB_GETITEMDATA, sel, 0);

                /* set the program information in the parent dialog */
                SetWindowText(GetDlgItem(editor_dlg_->get_handle(),
                                         IDC_FLD_EDITOR), info->prog);

                /* set the command line information in the parent dialog */
                SetWindowText(GetDlgItem(editor_dlg_->get_handle(),
                                         IDC_FLD_EDITOR_ARGS), info->cmdline);

                /* set the modified flag in the parent dialog */
                editor_dlg_->set_changed(TRUE);
            }

            /* proceed to inherit the default processing */
            break;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogEditAuto(CHtmlDialogDebugPref *editor_dlg)
    {
        /* remember the editor dialog */
        editor_dlg_ = editor_dlg;
    }

    /* the list of pre-configured editors */
    struct auto_config_info **editors_;
    size_t editor_cnt_;

    /* parent editor dialog */
    CHtmlDialogDebugPref *editor_dlg_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debugger editor preferences page
 */
class CHtmlDialogDebugEditor: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogDebugEditor(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* editor settings */
    CStringBuf editor_;
    CStringBuf args_;
};

/*
 *   Advanced editor settings dialog
 */
class CTadsDialogEditAdv: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(CTadsDialogPropPage *editor_dlg)
    {
        CTadsDialogEditAdv *dlg;

        /* create the dialog */
        dlg = new CTadsDialogEditAdv(editor_dlg);

        /* run the dialog */
        dlg->run_modal(DLG_DBG_EDIT_ADV, GetParent(editor_dlg->get_handle()),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        char prog[1024];

        /* get the current "program" setting from the parent dialog */
        GetWindowText(GetDlgItem(editor_hwnd_, IDC_FLD_EDITOR),
                      prog, sizeof(prog));

        /*
         *   if it starts with "DDE:", it has the form "DDE:server,topic";
         *   otherwise, it's not a DDE string
         */
        if (strlen(prog) > 4 && memicmp(prog, "DDE:", 4) == 0)
        {
            char *p;
            char *server;
            char *topic;
            char *exename;

            /* check the DDE checkbox */
            CheckDlgButton(handle_, IDC_CK_USE_DDE, BST_CHECKED);

            /* get the server name string - it's everything up to the comma */
            for (p = server = prog + 4 ; *p != '\0' && *p != ',' ; ++p) ;
            if (*p == ',')
                *p++ = '\0';

            /* get the topic name string - it's up to the next comma */
            for (topic = p ; *p != '\0' && *p != ',' ; ++p) ;
            if (*p == ',')
                *p++ = '\0';

            /* everything else is the executable name */
            exename = p;

            /* set the server name string in the dialog */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDESERVER), server);

            /* set the topic name */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDETOPIC), topic);

            /* set the executable name */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_DDEPROG), exename);

            /* note that we originally had DDE selected */
            dde_was_on_ = TRUE;
        }
        else
        {
            /* uncheck the DDE checkbox */
            CheckDlgButton(handle_, IDC_CK_USE_DDE, BST_UNCHECKED);

            /* note that DDE was not originally selected */
            dde_was_on_ = FALSE;
        }
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        switch(id)
        {
        case IDOK:
            /*
             *   they're accepting the settings - if "use dde" is checked,
             *   copy the DDE settings back to the main dialog; otherwise,
             *   restore the original program name setting
             */
            if (IsDlgButtonChecked(handle_, IDC_CK_USE_DDE) == BST_CHECKED)
            {
                char ddestr[1024];
                size_t len;

                /* get the server part */
                strcpy(ddestr, "DDE:");
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDESERVER),
                              ddestr + 4, sizeof(ddestr) - 4);

                /* add the comma */
                len = strlen(ddestr);
                ddestr[len++] = ',';

                /* add the topic part */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDETOPIC),
                              ddestr + len, sizeof(ddestr) - len);

                /* add another comma */
                len = strlen(ddestr);
                ddestr[len++] = ',';

                /* add the program name */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_DDEPROG),
                              ddestr + len, sizeof(ddestr) - len);

                /* set the program string to the DDE string */
                SetWindowText(GetDlgItem(editor_hwnd_,
                                         IDC_FLD_EDITOR), ddestr);

                /* set the 'changed' flag in the parent */
                editor_dlg_->set_changed(TRUE);
            }
            else
            {
                /* if DDE was originally selected, clear the program string */
                if (dde_was_on_)
                    SetWindowText(GetDlgItem(editor_hwnd_,
                                             IDC_FLD_EDITOR), "");

                /* set the 'changed' flag in the parent */
                editor_dlg_->set_changed(TRUE);
            }

            /* proceed to inherit the default processing */
            break;

        case IDC_BTN_BROWSE:
            /* browse for the program file */
            browse_file(handle_, 0, "Applications\0*.exe\0All Files\0*.*\0\0",
                        "Select a Text Editor Application", 0,
                        IDC_FLD_DDEPROG, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST
                        | OFN_PATHMUSTEXIST, TRUE, FALSE);

            /* handled */
            return TRUE;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogEditAdv(CTadsDialogPropPage *editor_dlg)
    {
        /* remember the editor dialog */
        editor_dlg_ = editor_dlg;
        editor_hwnd_ = editor_dlg->get_handle();
    }

    /* parent editor dialog */
    CTadsDialogPropPage *editor_dlg_;

    /* parent editor dialog window */
    HWND editor_hwnd_;

    /* flag: DDE was initially selected when the dialog started */
    int dde_was_on_ : 1;
};

/*
 *   initialize
 */
void CHtmlDialogDebugEditor::init()
{
    char buf[1024];

    /*
     *   Get the initial editor setting.  If there's no editor, use
     *   NOTEPAD by default.
     */
    if (config_->getval("editor", "program", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        strcpy(buf, "Notepad.EXE");
    SetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR), buf);

    /* get the command line; use "%f" by default */
    if (config_->getval("editor", "cmdline", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        strcpy(buf, "%f");
    SetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR_ARGS), buf);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a command
 */
int CHtmlDialogDebugEditor::do_command(WPARAM cmd, HWND ctl)
{
    switch(LOWORD(cmd))
    {
    case IDC_FLD_EDITOR:
    case IDC_FLD_EDITOR_ARGS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADVANCED:
        /* run the "advanced settings" dialog */
        CTadsDialogEditAdv::run_dlg(this);
        return TRUE;

    case IDC_BTN_AUTOCONFIG:
        /* run the "editor auto configuration" dialog */
        CTadsDialogEditAuto::run_dlg(this);
        return TRUE;

    case IDC_BTN_BROWSE:
        /* browse for the application file */
        browse_file("Applications\0*.exe\0All Files\0*.*\0\0",
                    "Select your Text Editor Application", 0, IDC_FLD_EDITOR,
                    OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
                    TRUE, FALSE);

        /* handled */
        return TRUE;

    default:
        /* nothing special; inherit default */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/*
 *   process a notification
 */
int CHtmlDialogDebugEditor::do_notify(NMHDR *nm, int ctl)
{
    char buf[1024];

    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new editor setting */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR), buf, sizeof(buf));
        config_->setval("editor", "program", 0, buf);

        /* save the new command line setting */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_EDITOR_ARGS),
                      buf, sizeof(buf));
        config_->setval("editor", "cmdline", 0, buf);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   Run the debugger preferences dialog
 */
void run_dbg_prefs_dlg(HWND owner, CHtmlSys_dbgmain *mainwin,
                       int init_page_id)
{
    PROPSHEETPAGE psp[15];
    int i;
    PROPSHEETHEADER psh;
    CHtmlDialogDebugStart *start_dlg;
    CHtmlDialogDebugSrcwin *srcwin_dlg;
    CHtmlDialogDebugPrompt *prompt_dlg;
    CHtmlDialogDebugEditor *editor_dlg;
    CHtmlDialogDebugSrcpath *srcpath_dlg;
    CHtmlDialogDebugExts *exts_dlg = 0;
    CHtmlDialogDebugSyntax *syntax_dlg;
    CHtmlDialogDebugKeymap *keymap_dlg;
    CHtmlDialogDebugTools *tools_dlg;
    CHtmlDialogDebugWinMode *winmode_dlg;
    CHtmlDialogDebugIndent *indent_dlg;
    CHtmlDialogDebugWrap *wrap_dlg;
    CHtmlDialogDebugAutoBuild *autobuild_dlg = 0;
    int inited = FALSE;

    /* create our pages */
    start_dlg = new CHtmlDialogDebugStart(
        mainwin, mainwin->get_global_config(), &inited);
    srcwin_dlg = new CHtmlDialogDebugSrcwin(
        mainwin, mainwin->get_global_config(), &inited);
    syntax_dlg = new CHtmlDialogDebugSyntax(
        mainwin, mainwin->get_global_config(), &inited, srcwin_dlg);
    keymap_dlg = new CHtmlDialogDebugKeymap(
        mainwin, mainwin->get_global_config(), &inited);
    indent_dlg = new CHtmlDialogDebugIndent(
        mainwin, mainwin->get_global_config(), &inited);
    wrap_dlg = new CHtmlDialogDebugWrap(
        mainwin, mainwin->get_global_config(), &inited);
    prompt_dlg = new CHtmlDialogDebugPrompt(
        mainwin, mainwin->get_global_config(), &inited);
    editor_dlg = new CHtmlDialogDebugEditor(
        mainwin, mainwin->get_global_config(), &inited);
    winmode_dlg = new CHtmlDialogDebugWinMode(
        mainwin, mainwin->get_global_config(), &inited);
    tools_dlg = new CHtmlDialogDebugTools(
        mainwin, mainwin->get_global_config(), &inited);

    if (w32_system_major_vsn >= 3)
    {
        autobuild_dlg = new CHtmlDialogDebugAutoBuild(
            mainwin, mainwin->get_global_config(), &inited);
        exts_dlg = new CHtmlDialogDebugExts(
            mainwin, mainwin->get_global_config(), &inited);
    }

    /*
     *   Create the library path dialog.  Note that the source path is part
     *   of the local configuration for tads 2 workbench, but in T3 it
     *   becomes the library path and moves to the global configuration.
     */
    srcpath_dlg = new CHtmlDialogDebugSrcpath(mainwin,
                                              w32_system_major_vsn >= 3
                                              ? mainwin->get_global_config()
                                              : mainwin->get_local_config(),
                                              &inited);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Workbench Options";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* start at the beginning of the page array */
    i = 0;

    /* add the basic formatting page */
    srcwin_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_SRCWIN, IDS_DEBUG_SRCWIN,
                             init_page_id, 0, &psh);

    /* add the syntax-coloring page */
    syntax_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_SYNTAX_COLORS,
                             IDS_DEBUG_SYNTAX_COLORS, init_page_id, 0, &psh);

    /* add the indent dialog */
    indent_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_INDENT,
                             IDS_DEBUG_INDENT, init_page_id, 0, &psh);

    /* add the margins & wrapping dialog */
    wrap_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                           &psp[i++], DLG_DEBUG_WRAP,
                           IDS_DEBUG_WRAP, init_page_id, 0, &psh);

    /* add the key map dialog */
    keymap_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_KEYBOARD,
                             IDS_DEBUG_KEYBOARD, init_page_id, 0, &psh);

    /* set up the "window mode" page */
    winmode_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_DEBUG_WINMODE, IDS_DEBUG_WINMODE,
                              init_page_id, 0, &psh);

    /* set up the "messages" page */
    prompt_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_PROMPTS, IDS_DEBUG_PROMPTS,
                             init_page_id, 0, &psh);

    /* set up the "start-up options" page */
    start_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                            &psp[i++], DLG_DEBUG_STARTOPT,
                            IDS_DEBUG_STARTOPT, init_page_id, 0, &psh);

    /* set up the "auto build" page */
    if (autobuild_dlg != 0)
        autobuild_dlg->init_ps_page(
            CTadsApp::get_app()->get_instance(), &psp[i++],
            DLG_DEBUG_AUTOBUILD, IDS_DEBUG_AUTOBUILD, init_page_id, 0, &psh);

    /* set up the "extensions" page */
    if (exts_dlg != 0)
        exts_dlg->init_ps_page(
            CTadsApp::get_app()->get_instance(), &psp[i++],
            DLG_DEBUG_EXTENSIONS, IDS_DEBUG_EXTS_PATH, init_page_id, 0, &psh);

    /* set up the "library path" page */
    srcpath_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_DEBUG_SRCPATH, IDS_DEBUG_SRCPATH,
                              init_page_id, 0, &psh);

    /* set up the "editor" page */
    editor_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                             &psp[i++], DLG_DEBUG_EDITOR, IDS_DEBUG_EDITOR,
                             init_page_id, 0, &psh);

    /* add the tools dialog */
    tools_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                            &psp[i++], DLG_DEBUG_EXTERNTOOLS,
                            IDS_DEBUG_EXTERNTOOLS, init_page_id, 0, &psh);

    /* set the page count in the header */
    psh.nPages = i;

    /* run the tree-style property sheet */
    CTadsDialog::run_tree_propsheet(mainwin->get_handle(), &psh);

    /* delete the dialogs */
    delete start_dlg;
    delete srcwin_dlg;
    delete syntax_dlg;
    delete keymap_dlg;
    delete indent_dlg;
    delete wrap_dlg;
    delete prompt_dlg;
    delete editor_dlg;
    delete winmode_dlg;
    delete srcpath_dlg;
    delete tools_dlg;
    if (autobuild_dlg != 0)
        delete autobuild_dlg;
    if (exts_dlg != 0)
        delete exts_dlg;
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Source page
 */
class CHtmlDialogBuildSrc: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildSrc(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
        /* start at the first file filter */
        selector_filter_index_ = 1;
    }

    ~CHtmlDialogBuildSrc()
    {
        /* restore the tree control's original window procedure */
        SetWindowLong(GetDlgItem(handle_, IDC_FLD_RSC), GWL_WNDPROC,
                      (LONG)tree_defproc_);

        /* remove our property from the tree control */
        RemoveProp(GetDlgItem(handle_, IDC_FLD_RSC),
                   "CHtmlDialogBuildSrc.parent.this");
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
    /* insert an item into our tree view control after the given item */
    HTREEITEM insert_tree(HTREEITEM parent, HTREEITEM ins_after,
                          const textchar_t *txt, DWORD state)
    {
        TV_INSERTSTRUCT tvi;
        HTREEITEM item;
        HWND tree;

        /* get the tree control */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* build the insertion record */
        tvi.hParent = parent;
        tvi.hInsertAfter = ins_after;
        tvi.item.mask = TVIF_TEXT | TVIF_STATE;
        tvi.item.state = state;
        tvi.item.stateMask = 0xffffffff;
        tvi.item.pszText = (char *)txt;

        /* insert it into the tree */
        item = TreeView_InsertItem(tree, &tvi);

        /*
         *   make sure the parent is expanded (but don't expand the root,
         *   since this is not legal with some version of the tree
         *   control)
         */
        if (parent != TVI_ROOT)
            TreeView_Expand(tree, parent, TVE_EXPAND);

        /* select the newly-inserted item */
        TreeView_SelectItem(tree, item);

        /* return the new item handle */
        return item;
    }

    /* insert an item in our tree view control at the end of a list */
    HTREEITEM append_tree(HTREEITEM parent, const textchar_t *txt,
                          DWORD state)
        { return insert_tree(parent, TVI_LAST, txt, state); }

    /* get the selected top-level resource file (a .RSn or the .GAM file) */
    HTREEITEM get_selected_rsfile() const
    {
        HWND tree;
        HTREEITEM parent;
        HTREEITEM cur;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* get the current selected item */
        parent = TreeView_GetSelection(tree);

        /* if there's nothing selected, use the first item */
        if (parent == 0)
            return TreeView_GetRoot(tree);

        /* find the top-level control containing the selection */
        for (cur = parent ; cur != 0 ;
             parent = cur, cur = TreeView_GetParent(tree, cur)) ;

        /* return the result */
        return parent;
    }

    /*
     *   renumber the .RSn files in our list - every time we insert or
     *   delete a .RSn file, we renumber them to keep the numbering
     *   contiguous
     */
    void renum_rsn_files()
    {
        HWND tree = GetDlgItem(handle_, IDC_FLD_RSC);
        HTREEITEM cur;
        int i;

        /*
         *   get the root item - this is the .gam file, so it doesn't need
         *   a number
         */
        cur = TreeView_GetRoot(tree);

        /* start at number 0 */
        i = 0;

        /* renumber each item */
        for (cur = TreeView_GetNextSibling(tree, cur) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree, cur), ++i)
        {
            char buf[40];
            TV_ITEM tvi;

            /* build the new name for this item */
            sprintf(buf, "External Resource File (.rs%d)", i);

            /* relabel the item */
            tvi.hItem = cur;
            tvi.mask = TVIF_TEXT;
            tvi.pszText = buf;
            TreeView_SetItem(tree, &tvi);
        }
    }

    /* hook procedure for subclassed tree control */
    static LRESULT CALLBACK tree_proc(HWND hwnd, UINT msg, WPARAM wpar,
                                      LPARAM lpar)
    {
        CHtmlDialogBuildSrc *win;

        /* get the 'this' pointer from the window properties */
        win = (CHtmlDialogBuildSrc *)
              GetProp(hwnd, "CHtmlDialogBuildSrc.parent.this");

        /* there's nothing we can do if we couldn't get the window */
        if (win == 0)
            return 0;

        /* invoke the non-static version */
        return win->do_tree_proc(hwnd, msg, wpar, lpar);
    }

    /* hook procedure - member variable version */
    LRESULT do_tree_proc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
    {
        /* check the message */
        switch(msg)
        {
        case WM_DROPFILES:
            /* do our drop file procedure */
            do_tree_dropfiles((HDROP)wpar);

            /* handled */
            return 0;

        default:
            /* no special handling is required */
            break;
        }

        /* call the original handler */
        return CallWindowProc((WNDPROC)tree_defproc_, hwnd, msg, wpar, lpar);
    }

    /* process a drop-files event in the tree control */
    void do_tree_dropfiles(HDROP hdrop)
    {
        HTREEITEM cur;
        HTREEITEM parent;
        HWND tree;
        TV_HITTESTINFO ht;
        int cnt;
        int i;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* find the tree view list item nearest the drop point */
        DragQueryPoint(hdrop, &ht.pt);
        parent = TreeView_HitTest(tree, &ht);

        /* if they're below the last item, use the last item */
        if (parent == 0)
        {
            /* find the last item at the root level in the list */
            for (parent = cur = TreeView_GetRoot(tree) ; cur != 0 ;
                 parent = cur, cur = TreeView_GetNextSibling(tree, cur)) ;
        }
        else
        {
            /* find the topmost parent of the selected item */
            for (cur = parent ; cur != 0 ;
                 parent = cur, cur = TreeView_GetParent(tree, cur)) ;
        }

        /* insert the files */
        cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
        for (i = 0 ; i < cnt ; ++i)
        {
            char fname[OSFNMAX];

            /* get this file's name */
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* add it to the list */
            append_tree(parent, fname, 0);
        }

        /* end the drag operation */
        DragFinish(hdrop);

        /* note that we've changed the list */
        set_changed(TRUE);
    }

    /* delete the selected item */
    void delete_tree_selection()
    {
        HWND tree;
        HTREEITEM sel;
        int deleting_rsn;

        /* get the tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* get the selected item */
        sel = TreeView_GetSelection(tree);

        /* if there's no selection, there's nothing to do */
        if (sel == 0)
            return;

        /* if this is the first item, we can't delete it */
        if (sel == TreeView_GetRoot(tree))
        {
            /* ignore it */
            return;
        }

        /* note whether we're deleting an .RSn file */
        deleting_rsn = (TreeView_GetParent(tree, sel) == 0);

        /*
         *   if this is a top-level item, and it has any children, confirm
         *   the deletion
         */
        if (deleting_rsn && TreeView_GetChild(tree, sel) != 0)
        {
            int btn;

            /* ask if they really want to do this */
            btn = MessageBox(GetParent(handle_),
                             "Are you sure you want to remove this entire "
                             "external resource file and all of its "
                             "resources?", "Confirm Deletion",
                             MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

            /* if they didn't say yes, ignore it */
            if (btn != IDYES && btn != 0)
                return;
        }

        /* delete the item */
        TreeView_DeleteItem(tree, sel);

        /* if we deleted an .RSn file, renumber the remaining .RSn files */
        if (deleting_rsn)
            renum_rsn_files();

        /* mark the change */
        set_changed(TRUE);
    }

    /* original tree procedure, before we hooked it */
    WNDPROC tree_defproc_;

    /* index of file filter in selector dialog */
    int selector_filter_index_;
};

/*
 *   initialize
 */
void CHtmlDialogBuildSrc::init()
{
    HTREEITEM parent;
    int cnt;
    int i;
    int rs_file_cnt;
    int next_rs_idx;
    int cur_rs_file;
    textchar_t buf[20];
    HWND tree;

    /* initialize the source file field */
    load_field("build", "source", IDC_FLD_SOURCE);

    /* insert the top-level resource list item for the game */
    parent = append_tree(TVI_ROOT, "Compiled game file (.gam)",
                         TVIS_BOLD | TVIS_EXPANDED);

    /* get the number of .RSn files */
    rs_file_cnt = config_->get_strlist_cnt("build", "rs file cnt");

    /* start at the zeroeth resource file, which is the .gam file */
    cur_rs_file = 0;

    /*
     *   get the number of resources in the main game file - this is
     *   always the first resource file
     */
    if (rs_file_cnt == 0
        || config_->getval("build", "rs file cnt", 0, buf, sizeof(buf)))
    {
        /*
         *   there's no limit; use a negative number so that we never get
         *   there
         */
        next_rs_idx = -1;
    }
    else
    {
        /* use the count from the configuration */
        next_rs_idx = atoi(buf);
    }

    /* get the resource list tree control */
    tree = GetDlgItem(handle_, IDC_FLD_RSC);

    /* insert the game resources */
    cnt = config_->get_strlist_cnt("build", "resources");
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[OSFNMAX];

        /*
         *   If we've reached the start of the next .RSn file, insert the
         *   new parent item.  Note that we must make this check
         *   repeatedly in case we encounter a .RSn file with no entries.
         */
        while (i == next_rs_idx)
        {
            char buf[50];

            /*
             *   if the previous file wasn't the game file, show it
             *   initially collapsed
             */
            if (cur_rs_file != 0)
                TreeView_Expand(tree, parent, TVE_COLLAPSE);

            /* append the next item */
            sprintf(buf, "External Resource File (.rs%d)", cur_rs_file);
            parent = append_tree(TVI_ROOT, buf, TVIS_BOLD);

            /* on to the next file */
            ++cur_rs_file;

            /* read the number of resources in the next block */
            if (cur_rs_file >= rs_file_cnt
                || config_->getval("build", "rs file cnt", cur_rs_file,
                                   buf, sizeof(buf)))
            {
                /*
                 *   this is the last one, so all remaining resources are
                 *   in this file
                 */
                next_rs_idx = -1;
            }
            else
            {
                /*
                 *   calculate the index of the first resource in the next
                 *   file - it's the starting index of this file plus the
                 *   number of items in this file
                 */
                next_rs_idx += atoi(buf);
            }
        }

        /* get this item */
        if (!config_->getval("build", "resources", i, buf, sizeof(buf)))
        {
            /* add it to the tree */
            append_tree(parent, buf, 0);
        }
    }

    /* initially collapse the last .RSn file (unless it's the .gam file) */
    if (cur_rs_file != 0)
        TreeView_Expand(tree, parent, TVE_COLLAPSE);

    /* add any additional empty files at the end of the list */
    while (cur_rs_file + 1 < rs_file_cnt)
    {
        char buf[50];

        /* append the next item */
        sprintf(buf, "External Resource File (.rs%d)", cur_rs_file);
        parent = append_tree(TVI_ROOT, buf, TVIS_BOLD);

        /* up the count */
        ++cur_rs_file;
    }

    /* initially select the first item in the list */
    TreeView_SelectItem(tree, TreeView_GetRoot(tree));

    /*
     *   hook the window procedure for the list control so that we can
     *   intercept WM_DROPFILE messages
     */
    tree_defproc_ = (WNDPROC)SetWindowLong(tree, GWL_WNDPROC,
                                           (LONG)&tree_proc);

    /*
     *   set a property with my 'this' pointer, so we can find it in the
     *   hook procedure
     */
    SetProp(tree, "CHtmlDialogBuildSrc.parent.this", (HANDLE)this);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildSrc::do_notify(NMHDR *nm, int ctl)
{
    HWND tree;
    HTREEITEM parent;
    int resnum;
    int resnum_base;
    char label[OSFNMAX];
    int cur_rs_file;

    switch(nm->code)
    {
    case TVN_KEYDOWN:
        if (ctl == IDC_FLD_RSC)
        {
            TV_KEYDOWN *tvk = (TV_KEYDOWN *)nm;

            /* check for special keys that we recognize */
            switch(tvk->wVKey)
            {
            case VK_DELETE:
            case VK_BACK:
                /* delete/backspace - delete the selected item */
                delete_tree_selection();

                /* done */
                break;

            default:
                /* no special handling is required */
                break;
            }

            /* handled */
            return TRUE;
        }

        /* other cases aren't handled */
        break;

    case TVN_SELCHANGED:
        if (ctl == IDC_FLD_RSC)
        {
            NM_TREEVIEW *tvn = (NM_TREEVIEW *)nm;
            HWND tree = GetDlgItem(handle_, ctl);

            /*
             *   turn on the "remove" button if anything other than the
             *   root item (the .gam file) is selected; turn it off if the
             *   game file is selected or there's no selection
             */
            EnableWindow(GetDlgItem(handle_, IDC_BTN_REMOVE),
                         tvn->itemNew.hItem != 0
                         && tvn->itemNew.hItem != TreeView_GetRoot(tree));

            /* handled */
            return TRUE;
        }

        /* other cases aren't handled */
        break;

    case PSN_APPLY:
        /* save the new source file setting */
        save_field("build", "source", IDC_FLD_SOURCE, TRUE);

        /* get the resource list tree */
        tree = GetDlgItem(handle_, IDC_FLD_RSC);

        /* clear the old resource list */
        config_->clear_strlist("build", "resources");
        config_->clear_strlist("build", "rs file cnt");

        /* start at resource zero */
        resnum = 0;
        resnum_base = 0;

        /* start at the first resource file, which is the .gam file */
        cur_rs_file = 0;

        /* go through the top-level tree items */
        for (parent = TreeView_GetRoot(tree) ; parent != 0 ;
             parent = TreeView_GetNextSibling(tree, parent))
        {
            HTREEITEM cur;
            char buf[20];

            /* scan the children of this item */
            for (cur = TreeView_GetChild(tree, parent) ; cur != 0 ;
                 cur = TreeView_GetNextSibling(tree, cur))
            {
                TV_ITEM tvi;

                /* get the information on this item */
                tvi.mask = TVIF_TEXT;
                tvi.pszText = label;
                tvi.cchTextMax = sizeof(label);
                tvi.hItem = cur;
                TreeView_GetItem(tree, &tvi);

                /* save this item */
                config_->setval("build", "resources", resnum, label);

                /* on to the next resource */
                ++resnum;
            }

            /* set the resource count for this file */
            sprintf(buf, "%u", resnum - resnum_base);
            config_->setval("build", "rs file cnt", cur_rs_file, buf);

            /* on to the next file */
            ++cur_rs_file;

            /* note the index of the first resource in this file */
            resnum_base = resnum;
        }

        /* done */
        return TRUE;

    default:
        /* not handled */
        break;
    }

    /* inherit default behavior */
    return CHtmlDialogDebugPref::do_notify(nm, ctl);
}

/*
 *   process a command
 */
int CHtmlDialogBuildSrc::do_command(WPARAM cmd, HWND ctl)
{
    OPENFILENAME5 info;
    char fname[1024];
    HTREEITEM sel;

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_SOURCE:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the main source file */
        browse_file("TADS Source Files\0*.t\0"
                    "TADS Library Files\0*.tl\0"
                    "All Files\0*.*\0\0",
                    "Select your game's main source file", 0, IDC_FLD_SOURCE,
                    OFN_HIDEREADONLY, TRUE, TRUE);

        /* handled */
        return TRUE;

    case IDC_BTN_ADDRS:
        /* get the current selection */
        sel = get_selected_rsfile();

        /* add a new .RSn file after the selected .RSn file */
        insert_tree(TVI_ROOT, sel, "External Resource File",
                    TVIS_BOLD | TVIS_EXPANDED);

        /* renumber the .RSn files */
        renum_rsn_files();

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_REMOVE:
        /* delete the selected item */
        delete_tree_selection();

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_ADDFILE:
        /* ask for a file to open */
        info.hwndOwner = handle_;
        info.hInstance = CTadsApp::get_app()->get_instance();
        info.lpstrCustomFilter = 0;
        info.nFilterIndex = selector_filter_index_;
        info.lpstrFile = fname;
        fname[0] = '\0';
        info.nMaxFile = sizeof(fname);
        info.lpstrFileTitle = 0;
        info.nMaxFileTitle = 0;
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
        info.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_ENABLESIZING;
        info.nFileOffset = 0;
        info.nFileExtension = 0;
        info.lpstrDefExt = 0;
        info.lCustData = 0;
        info.lpfnHook = 0;
        info.lpTemplateName = 0;
        info.lpstrTitle = "Select resource file or files to add";
        info.lpstrFilter = "JPEG Images\0*.jpg;*.jpeg;*.jpe\0"
                           "PNG Images\0*.png\0"
                           "Wave Sounds\0*.wav\0"
                           "MPEG Sounds\0*.mp3\0"
                           "MIDI Music\0*.mid;*.midi\0"
                           "All Files\0*.*\0\0";
        CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);

        /* allow multiple files to be selected */
        info.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;

        /* ask for the file(s) */
        if (GetOpenFileName((OPENFILENAME *)&info))
        {
            char *p;
            HWND tree = GetDlgItem(handle_, IDC_FLD_RSC);
            HTREEITEM parent;

            /*
             *   get the project directory - this is the relative path for
             *   the files we're adding
             */
            char projdir[OSFNMAX];
            mainwin_->get_project_dir(projdir, sizeof(projdir));

            /* remember the updated filter the user selected */
            selector_filter_index_ = info.nFilterIndex;

            /* find the selected .RSn file in the tree */
            parent = get_selected_rsfile();

            /*
             *   if this is a lone filename, the dialog won't have
             *   inserted a null byte between the file and the path, so
             *   insert one of our own - this makes the two cases the same
             *   so that we won't need special handling in the loop
             */
            if (info.nFileOffset != 0)
                fname[info.nFileOffset - 1] = '\0';

            /*
             *   append each file (there may be many) to the resource
             *   list, each on a separate line
             */
            for (p = fname + info.nFileOffset ; *p != '\0' ;
                 p += strlen(p) + 1)
            {
                char fullname[OSFNMAX];
                char relname[OSFNMAX];

                /* build the full path */
                os_build_full_path(fullname, sizeof(fullname), fname, p);

                /* make it relative to the project folder */
                os_get_rel_path(relname, sizeof(relname), projdir, fullname);

                /* add it to the list */
                append_tree(parent, relname, 0);
            }

            /* note that we've changed the list */
            set_changed(TRUE);

            /* set the path for future open-file dialog calls */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDC_BTN_ADDDIR:
        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (browse_folder(
            GetParent(handle_), "&Resource Folder:", "Add Resource Folder",
            fname, sizeof(fname), fname, TRUE))
        {
            /* add the folder to the tree under the selected resource file */
            append_tree(get_selected_rsfile(), fname, 0);

            /* note that we've changed the list */
            set_changed(TRUE);

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* set focus on the tree */
        SetFocus(GetDlgItem(handle_, IDC_FLD_RSC));

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* not handled */
        break;
    }

    /* inherit default handling */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Includes page
 */
class CHtmlDialogBuildInc: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildInc(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogBuildInc::init()
{
    /* initialize the include path list */
    load_multiline_fld("build", "includes", IDC_FLD_INC, TRUE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildInc::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new include path setting */
        save_multiline_fld("build", "includes", IDC_FLD_INC, TRUE, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildInc::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_INC:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADDDIR:
        /* start in the current open-file folder */
        if (CTadsApp::get_app()->get_openfile_dir() != 0
            && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
            strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
        else
            GetCurrentDirectory(sizeof(fname), fname);

        /* run the folder selector dialog */
        if (browse_folder(
            GetParent(handle_), "Folder to add to #include &path:",
            "Add Folder to #include Search Path", fname, sizeof(fname),
            fname, FALSE))
        {
            /* add the folder to our list */
            HWND fld = GetDlgItem(handle_, IDC_FLD_INC);
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)fname);

            /* add a newline */
            SendMessage(fld, EM_SETSEL, 32767, 32767);
            SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");

            /* use this as the next open-file directory */
            CTadsApp::get_app()->set_openfile_path(fname);
        }

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Outputs page
 */
class CHtmlDialogBuildOut: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildOut(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogBuildOut::init()
{
    /*
     *   store the current game name - this field cannot be modified, since
     *   it's based on the name of the loaded project (.tdc/.t3c) file
     */
    SetWindowText(GetDlgItem(handle_, IDC_FLD_GAM),
                  mainwin_->get_gamefile_name());

    /* load the release .t3 and .exe filenames */
    load_field_rel("build", "rlsgam", IDC_FLD_RLSGAM);
    load_field_rel("build", "exe", IDC_FLD_EXE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildOut::do_notify(NMHDR *nm, int ctl)
{
    textchar_t buf[OSFNMAX];
    textchar_t buf2[OSFNMAX];

    switch(nm->code)
    {
    case PSN_APPLY:
        /*
         *   make sure the release game and debug game files aren't the
         *   same file
         */
        GetWindowText(GetDlgItem(handle_, IDC_FLD_RLSGAM), buf, sizeof(buf));
        GetWindowText(GetDlgItem(handle_, IDC_FLD_GAM), buf2, sizeof(buf2));
        if (stricmp(buf, buf2) == 0)
        {
            /* display an error message */
            MessageBox(GetParent(handle_),
                       "The release game and debug game file names "
                       "are identical; this is not allowed because the "
                       "files would overwrite each other.  "
                       "Please use different filenames.",
                       "TADS Debugger", MB_ICONEXCLAMATION | MB_OK);

            /* indicate that we cannot proceed */
            SetWindowLong(handle_, DWL_MSGRESULT,
                          PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }

        /* save the release .GAM and .EXE file settings */
        mainwin_->vsn_change_pref_gamefile_name(buf2);
        save_field_rel("build", "rlsgam", IDC_FLD_RLSGAM, TRUE);
        save_field_rel("build", "exe", IDC_FLD_EXE, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildOut::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_GAM:
    case IDC_FLD_EXE:
    case IDC_FLD_RLSGAM:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the release game file */
        browse_file(w32_opendlg_filter,
                    "Select the release compiled game file", w32_game_ext,
                    IDC_FLD_RLSGAM, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;

    case IDC_BTN_BROWSE2:
        /* browse for the .EXE file */
        browse_file("Applications\0*.exe\0All Files\0*.*\0\0",
                    "Select the application file name", "exe",
                    IDC_FLD_EXE, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;

    case IDC_BTN_BROWSE3:
        /*
         *   browse for debug game file - supported only in t3 workbench, but
         *   we don't have to worry about disabling the code for tads 2 as
         *   we'll simply never get this request from the dialog
         */
        browse_file(w32_opendlg_filter,
                    "Select the debug compiled game file", w32_game_ext,
                    IDC_FLD_GAM, OFN_HIDEREADONLY, FALSE, TRUE);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Defines page
 */
class CHtmlDialogBuildDef: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildDef(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   "Add #define symbol" dialog
 */
class CTadsDialogBuildDefAdd: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(HWND parent_hwnd)
    {
        CTadsDialogBuildDefAdd *dlg;

        /* create the dialog */
        dlg = new CTadsDialogBuildDefAdd(parent_hwnd);

        /* run the dialog */
        dlg->run_modal(DLG_ADD_MACRO, GetParent(parent_hwnd),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /* initialize */
    void init()
    {
        /*
         *   initially disable the "OK" button - it's not enabled until
         *   the macro name field contains some text
         */
        EnableWindow(GetDlgItem(handle_, IDOK), FALSE);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        switch(LOWORD(id))
        {
        case IDC_FLD_MACRO:
            if (HIWORD(id) == EN_CHANGE)
            {
                char buf[50];
                char *src;
                char *dst;
                int changed;

                /*
                 *   the macro field is being changed - turn the "OK"
                 *   button on if the field is not empty, off if it is
                 */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO),
                              buf, sizeof(buf));

                /* remove any invalid characters */
                for (changed = FALSE, src = dst = buf ; *src != '\0' ; ++src)
                {
                    /* copy this character if it's valid in a macro name */
                    if (isalpha(*src) || *src == '_'
                        || (src > buf && isdigit(*src)))
                    {
                        /* it's valid - copy it */
                        *dst++ = *src;
                    }
                    else
                    {
                        /* note the change */
                        changed = TRUE;
                    }
                }

                /* null-terminate the result */
                *dst = '\0';

                /* copy it back into the control if we changed it */
                if (changed)
                    SetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO), buf);

                /* enable the window if the name is not empty */
                EnableWindow(GetDlgItem(handle_, IDOK), dst > buf);

                /* handled */
                return TRUE;
            }

            /* not handled; pick up the default handling */
            break;

        case IDOK:
            /*
             *   they're accepting the settings - add the macro definition
             *   to the parent dialog's macro list
             */
            {
                HWND fld;
                char buf[256];

                /* get the parent field */
                fld = GetDlgItem(parent_hwnd_, IDC_FLD_DEFINES);

                /* add the macro name */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO),
                              buf, sizeof(buf));
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

                /* add the '=' */
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"=");

                /* add the expansion text */
                GetWindowText(GetDlgItem(handle_, IDC_FLD_MACRO_EXP),
                              buf, sizeof(buf));
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)buf);

                /* add the newline */
                SendMessage(fld, EM_SETSEL, 32767, 32767);
                SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
            }

            /* proceed to inherit the default processing */
            break;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogBuildDefAdd(HWND parent_hwnd)
    {
        /* remember the parent dialog window */
        parent_hwnd_ = parent_hwnd;
    }

    /* parent dialog window */
    HWND parent_hwnd_;
};


/*
 *   initialize
 */
void CHtmlDialogBuildDef::init()
{
    /* load the "defines" list */
    load_multiline_fld("build", "defines", IDC_FLD_DEFINES, FALSE);

    /* load the "undefines" list */
    load_multiline_fld("build", "undefs", IDC_FLD_UNDEF, FALSE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildDef::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /*
         *   If the list of defines is changing, reload the libraries.  This
         *   is necessary because libraries can refer to macro values; when
         *   macro values change, we need to reload the libraries to ensure
         *   that the loaded libraries reflect any changes that affect them.
         */
        if (multiline_fld_changed("build", "defines",
                                  IDC_FLD_DEFINES))
        {
            /* schedule the libraries for reloading when convenient */
            mainwin_->schedule_lib_reload();
        }

        /* save the "defines" and "undefs" lists */
        save_multiline_fld("build", "defines", IDC_FLD_DEFINES, FALSE, FALSE);
        save_multiline_fld("build", "undefs", IDC_FLD_UNDEF, FALSE, FALSE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildDef::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_DEFINES:
    case IDC_FLD_UNDEF:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_ADD:
        /* run the #define dialog */
        CTadsDialogBuildDefAdd::run_dlg(handle_);
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Diagnostics (errors) page
 */
class CHtmlDialogBuildErr: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildErr(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogBuildErr::init()
{
    int level;

    /* load the "verbose" checkbox */
    load_ckbox("build", "verbose errors", IDC_CK_VERBOSE, TRUE);

    /* load the "warnings as errors" checkbox */
    load_ckbox("build", "warnings as errors", IDC_CK_WARN_AS_ERR, FALSE);

    /* get the warning level setting */
    if (config_->getval("build", "warning level", &level))
        level = 1;

    /* check the appropriate warning level button */
    CheckRadioButton(handle_, IDC_RB_WARN_NONE, IDC_RB_WARN_PEDANTIC,
                     IDC_RB_WARN_NONE + level);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildErr::do_notify(NMHDR *nm, int ctl)
{
    int level;

    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the "verbose" setting */
        save_ckbox("build", "verbose errors", IDC_CK_VERBOSE);

        /* save the "warnings as errors" setting */
        save_ckbox("build", "warnings as errors", IDC_CK_WARN_AS_ERR);

        /* save the warning level setting */
        if (IsDlgButtonChecked(handle_, IDC_RB_WARN_NONE))
            level = 0;
        else if (IsDlgButtonChecked(handle_, IDC_RB_WARN_STANDARD))
            level = 1;
        else
            level = 2;
        config_->setval("build", "warning level", level);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildErr::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_CK_VERBOSE:
    case IDC_CK_WARN_AS_ERR:
    case IDC_RB_WARN_NONE:
    case IDC_RB_WARN_STANDARD:
    case IDC_RB_WARN_PEDANTIC:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}



/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Intermediate Files page
 */
class CHtmlDialogBuildInter: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildInter(CHtmlSys_dbgmain *mainwin,
                          CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogBuildInter::init()
{
    /* load the symbol and object file directory fields */
    load_field_rel("build", "symfile path", IDC_FLD_SYMDIR);
    load_field_rel("build", "objfile path", IDC_FLD_OBJDIR);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildInter::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the directory path fields */
        save_field_rel("build", "symfile path", IDC_FLD_SYMDIR, TRUE);
        save_field_rel("build", "objfile path", IDC_FLD_OBJDIR, TRUE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildInter::do_command(WPARAM cmd, HWND ctl)
{
    char fname[OSFNMAX];
    int fld;
    const char *prompt;
    const char *title;

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_SYMDIR:
    case IDC_FLD_OBJDIR:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_BTN_BROWSE:
        /* browse for the symbol file directory */
        fld = IDC_FLD_SYMDIR;
        prompt = "Folder for symbol files:";
        title = "Select Symbol File Folder";
        goto browse_for_folder;

    case IDC_BTN_BROWSE2:
        /* browse for the object file directory */
        fld = IDC_FLD_OBJDIR;
        prompt = "Folder for object files:";
        title = "Select Object File Folder";

    browse_for_folder:
        /*
         *   get the current setting; if there isn't a current setting, start
         *   browsing in the current open-file folder
         */
        GetDlgItemText(handle_, fld, fname, sizeof(fname));
        if (fname[0] == '\0')
        {
            /* start in the current open-file folder */
            if (CTadsApp::get_app()->get_openfile_dir() != 0
                && CTadsApp::get_app()->get_openfile_dir()[0] != '\0')
                strcpy(fname, CTadsApp::get_app()->get_openfile_dir());
            else
                GetCurrentDirectory(sizeof(fname), fname);
        }

        /* browse for a symbol file directory */
        if (browse_folder(
            GetParent(handle_), prompt, title, fname, sizeof(fname),
            fname, TRUE))
        {
            /* store the value */
            SetDlgItemText(handle_, fld, fname);
        }

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}



/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Advanced Options page
 */
class CHtmlDialogBuildAdv: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildAdv(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/*
 *   initialize
 */
void CHtmlDialogBuildAdv::init()
{
    /* load the checkboxes */
    load_ckbox("build", "ignore case", IDC_CK_CASE, FALSE);
    load_ckbox("build", "c ops", IDC_CK_C_OPS, FALSE);
    load_ckbox("build", "use charmap", IDC_CK_CHARMAP, FALSE);
    load_ckbox("build", "run preinit", IDC_CK_PREINIT, TRUE);
    load_ckbox("build", "keep default modules", IDC_CK_DEFMOD, TRUE);
    load_ckbox("build", "gen sourceTextGroup", IDC_CK_SRCTXTGRP, FALSE);

    /* load the character mapping file field */
    load_field("build", "charmap", IDC_FLD_CHARMAP);

    /* load the extra options field */
    load_multiline_fld("build", "extra options", IDC_FLD_OPTS, FALSE);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildAdv::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the checkboxes */
        save_ckbox("build", "ignore case", IDC_CK_CASE);
        save_ckbox("build", "c ops", IDC_CK_C_OPS);
        save_ckbox("build", "use charmap", IDC_CK_CHARMAP);
        save_ckbox("build", "run preinit", IDC_CK_PREINIT);
        save_ckbox("build", "keep default modules", IDC_CK_DEFMOD);
        save_ckbox("build", "gen sourceTextGroup", IDC_CK_SRCTXTGRP);

        /* load the character mapping file field */
        save_field("build", "charmap", IDC_FLD_CHARMAP, TRUE);

        /* load the extra options field */
        save_multiline_fld(
            "build", "extra options", IDC_FLD_OPTS, FALSE, FALSE);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildAdv::do_command(WPARAM cmd, HWND ctl)
{
    char buf[OSFNMAX*2];
    char *rootname;
    FILE *fp;

    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_CHARMAP:
    case IDC_FLD_OPTS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_CK_CASE:
    case IDC_CK_C_OPS:
    case IDC_CK_CHARMAP:
    case IDC_CK_PREINIT:
    case IDC_CK_SRCTXTGRP:
    case IDC_CK_DEFMOD:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    case IDC_BTN_HELP:
        /* look for the help file in the executable directory */
        GetModuleFileName(0, buf, sizeof(buf));
        rootname = os_get_root_name(buf);

        /* try helptc.htm; if it's not there, look in the doc/wb folder */
        strcpy(rootname, "helptc.htm");
        if (GetFileAttributes(buf) == 0xffffffff)
            strcpy(rootname, "doc\\wb\\helptc.htm");

        /* try the 'redirect' file if we can't find the htm file */
        if (GetFileAttributes(buf) == 0xffffffff)
        {
            strcpy(rootname, "helptc.redir");
            if ((fp = fopen(buf, "r")) != 0)
            {
                char *p;

                /* try each listed file */
                for (;;)
                {
                    if (fgets(rootname, sizeof(buf) - (rootname - buf),
                              fp) == 0)
                        break;

                    /* remove trailing newlines */
                    for (p = rootname + strlen(rootname) ;
                         p > rootname && *(p-1) == '\n' ; *--p = '\0') ;

                    /* if it looks like an http URL, use this name */
                    if (memicmp(rootname, "http://", 7) == 0)
                    {
                        memmove(buf, rootname, strlen(rootname)+1);
                        break;
                    }

                    /* if this file exists, stop here */
                    if (GetFileAttributes(buf) != 0xffffffff)
                        break;
                }
            }
        }

        /* display the help file for the compiler options */
        if ((unsigned long)ShellExecute(
            0, 0, buf, 0, 0, SW_SHOWNORMAL) <= 32)
            MessageBox(GetParent(handle_),
                       "Couldn't open help.  You must have a web "
                       "browser or HTML viewer installed to view help.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* handled */
        return TRUE;

    case IDC_BTN_BROWSE:
        /* browse for a character mapping file */
        browse_file("TADS Character Maps\0*.tcp\0All Files\0*.*\0\0",
                    "Select a TADS Character Mapping file", 0,
                    IDC_FLD_CHARMAP, OFN_HIDEREADONLY, TRUE, FALSE);

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Scripts dialog
 */
class CHtmlDialogBuildScripts: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildScripts(CHtmlSys_dbgmain *mainwin,
                            CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        char buf[50];
        int prune;

        /* do the default work */
        CHtmlDialogDebugPref::init();

        /* load the configuration data */
        load_field_rel("auto-script", "dir", IDC_FLD_SCRIPTFOLDER, "Scripts");

        CheckDlgButton(handle_, IDC_CK_AUTOSCRIPT,
                       config_->getval_int("auto-script", "enabled", TRUE)
                       ? BST_CHECKED : BST_UNCHECKED);

        prune = config_->getval_int("auto-script", "prune", TRUE);
        CheckDlgButton(handle_, IDC_CK_PRUNESCRIPTS,
                       prune ? BST_CHECKED : BST_UNCHECKED);

        sprintf(buf, "%d",
                config_->getval_int("auto-script", "prune to count", 25));
        SetDlgItemText(handle_, IDC_FLD_MAXSCRIPTS, buf);

        EnableWindow(GetDlgItem(handle_, IDC_FLD_MAXSCRIPTS), prune);

        load_multiline_fld("auto-script", "quit sequences",
                           IDC_FLD_TRIMQUIT, FALSE);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        char buf[MAX_PATH];

        /* note changes */
        switch (LOWORD(cmd))
        {
        case IDC_CK_PRUNESCRIPTS:
            /* enable or disable the max script field accordingly */
            EnableWindow(GetDlgItem(handle_, IDC_FLD_MAXSCRIPTS),
                         IsDlgButtonChecked(handle_, IDC_CK_PRUNESCRIPTS)
                         == BST_CHECKED);

            /* note the change */
            set_changed(TRUE);

            /* handled */
            return TRUE;

        case IDC_CK_AUTOSCRIPT:
            /* note the change */
            set_changed(TRUE);
            return TRUE;

        case IDC_FLD_SCRIPTFOLDER:
        case IDC_FLD_MAXSCRIPTS:
        case IDC_FLD_TRIMQUIT:
            /* on field edits, note that we have changes */
            if (HIWORD(cmd) == EN_CHANGE)
                set_changed(TRUE);
            return TRUE;

        case IDC_BTN_BROWSE:
            /* start with the current setting */
            GetDlgItemText(handle_, IDC_FLD_SCRIPTFOLDER, buf, sizeof(buf));
            if (buf[0] == '\0')
                GetCurrentDirectory(sizeof(buf), buf);

            /* browse for a script folder */
            if (browse_folder(
                GetParent(handle_), "Script &Folder:", "Select Script Folder",
                buf, sizeof(buf), buf, TRUE))
            {
                /* set the new folder */
                SetDlgItemText(handle_, IDC_FLD_SCRIPTFOLDER, buf);

                /* note changes */
                set_changed(TRUE);
            }

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        textchar_t buf[MAX_PATH];

        /* check what we have */
        switch (nm->code)
        {
        case PSN_APPLY:
            /* save our configuration values */
            save_field_rel("auto-script", "dir", IDC_FLD_SCRIPTFOLDER, TRUE);

            config_->setval("auto-script", "enabled",
                            IsDlgButtonChecked(handle_, IDC_CK_AUTOSCRIPT)
                            == BST_CHECKED);
            config_->setval("auto-script", "prune",
                            IsDlgButtonChecked(handle_, IDC_CK_PRUNESCRIPTS)
                            == BST_CHECKED);

            GetDlgItemText(handle_, IDC_FLD_MAXSCRIPTS, buf, sizeof(buf));
            config_->setval("auto-script", "prune to count", atoi(buf));

            save_multiline_fld("auto-script", "quit sequences",
                               IDC_FLD_TRIMQUIT, FALSE, FALSE);

            /* refresh the script window, in case the directory changed */
            mainwin_->refresh_script_win(TRUE);

            /* handled */
            return TRUE;
        }

        /* inherit default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Build Options - TADS 2 Special Files page.  This page applies only to
 *   TADS 2; in TADS 3, it's handled via the Special Files section of the
 *   Project window.
 */
class CHtmlDialogBuildSpecialFiles: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildSpecialFiles(CHtmlSys_dbgmain *mainwin,
                                 CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        const textchar_t *val;

        /*
         *   set the EXE icon value, if any - use our new-style setting
         *   first, then look for the old-style generic build settings if we
         *   don't find it
         */
        if ((val = config_->getval_strptr("build", "exe icon", 0)) != 0
            || (val = find_icon_line(0)) != 0)
            SetDlgItemText(handle_, IDC_FLD_ICON, val);

        /* set the Cover Art value, if any */
        if ((val = config_->getval_strptr("build", "cover art", 0)) != 0)
            SetDlgItemText(handle_, IDC_FLD_COVER_ART, val);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch (LOWORD(cmd))
        {
        case IDC_FLD_ICON:
        case IDC_FLD_COVER_ART:
            /* if we changed a field, note that we have unapplied changes */
            if (HIWORD(cmd) == EN_CHANGE)
            {
                set_changed(TRUE);
                return TRUE;
            }
            break;

        case IDC_BTN_BROWSE:
            /* browse for a Cover Art image */
            browse_file("All Supported Images (*.jpg, *.png)\0"
                        "*.png;*.jpg;*.jpe;*.jpeg\0"
                        "JPEG Images (*.jpg)\0*.jpg;*.jpe;*.jpeg\0"
                        "PNG Images (*.png)\0*.png\0"
                        "All Files (*.*)\0*.*\0",
                        "Select the Cover Art Image", "jpg",
                        IDC_FLD_COVER_ART, OFN_HIDEREADONLY, TRUE, TRUE);

            /* handled */
            return TRUE;

        case IDC_BTN_BROWSE2:
            /* browse for the EXE icon */
            browse_file("Icons (*.ico)\0*.ico\0All Files (*.*)\0*.*\0",
                        "Select the EXE file icon", "ico",
                        IDC_FLD_ICON, OFN_HIDEREADONLY, TRUE, TRUE);

            /* handled */
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        char buf[OSFNMAX];

        switch (nm->code)
        {
        case PSN_APPLY:
            /* save the EXE icon value, if set */
            GetDlgItemText(handle_, IDC_FLD_ICON, buf, sizeof(buf));
            config_->clear_strlist("build", "exe icon");
            if (buf[0] != '\0')
                config_->setval("build", "exe icon", 0, buf);

            /* save the Cover Art value, if set */
            GetDlgItemText(handle_, IDC_FLD_COVER_ART, buf, sizeof(buf));
            config_->clear_strlist("build", "cover art");
            if (buf[0] != '\0')
                config_->setval("build", "cover art", 0, buf);

            /* handled */
            return TRUE;
        }

        /* inherit the default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

private:
    /* find the "icon=" entry in the config */
    const textchar_t *find_icon_line(int *idx)
    {
        const textchar_t *val;
        int i, cnt;

        /* look for "icon=" among the install options */
        cnt = config_->get_strlist_cnt("build", "installer options");
        for (i = 0 ; i < cnt ; ++i)
        {
            val = config_->getval_strptr("build", "installer options", i);
            if (memicmp(val, "icon=", 5) == 0)
            {
                /* fill in the index, if requested */
                if (idx != 0)
                    *idx = i;

                /* return the name string */
                return val + 5;
            }
        }

        /*
         *   didn't find it; if we want to insert it, we'll append it after
         *   the existing entries
         */
        if (idx != 0)
            *idx = cnt;
        return 0;
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Installer page
 */
class CHtmlDialogBuildIns: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildIns(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init();
    int do_command(WPARAM cmd, HWND ctl);
    int do_notify(NMHDR *nm, int ctl);

private:
};

/* structure associating a field with an installer option */
struct insopt_fldinfo_t
{
    /* installer option name for the field */
    const textchar_t *nm;

    /* field control ID in the dialog */
    int id;

    /* information string ID */
    int strid;
};

/* list of field associations */
static const insopt_fldinfo_t insopt_flds[] =
{
    { "name", IDC_FLD_DISPNAME, IDS_INFO_DISPNAME },
    { "savext", IDC_FLD_SAVE_EXT, IDS_INFO_SAVE_EXT },
    { "license", IDC_FLD_LICENSE, IDS_INFO_LICENSE },
    { "progdir", IDC_FLD_PROGDIR, IDS_INFO_PROGDIR },
    { "startfolder", IDC_FLD_STARTFOLDER, IDS_INFO_STARTFOLDER },
    { "readme", IDC_FLD_README, IDS_INFO_README },
    { "readme_title", IDC_FLD_README_TITLE, IDS_INFO_README_TITLE },
    { "charlib", IDC_FLD_CHARLIB, IDS_INFO_CHARLIB },
    { 0, 0, IDS_INFO_NONE }
};

/*
 *   Installer Options dialog
 */
class CTadsDialogInsOpts: public CTadsDialog
{
public:
    /* run the dialog */
    static void run_dlg(HWND parent, CHtmlSys_dbgmain *mainwin)
    {
        CTadsDialogInsOpts *dlg;

        /* create the dialog */
        dlg = new CTadsDialogInsOpts(parent, mainwin);

        /* run the dialog */
        dlg->run_modal(DLG_INSTALL_OPTIONS, GetParent(parent),
                       CTadsApp::get_app()->get_instance());

        /* we're done with the dialog; delete it */
        delete dlg;
    }

    /*
     *   Parse the free-form text in the parent list, and either load it
     *   into our structured fields or store our structured fields back
     *   into the free-form text list.  If 'loading' is true, we'll load
     *   the free-form list into our fields; otherwise we'll store our
     *   fields back into the free-form list.
     */
    void parse_list(int loading)
    {
        HWND fld;
        const insopt_fldinfo_t *fldp;
        int i;
        int cnt;
        char field_found[countof(insopt_flds) - 1];

        /* get the parent field with our initial data */
        fld = GetDlgItem(parent_, IDC_FLD_INSTALL_OPTS);

        /* get the number of lines in the parent field */
        cnt = SendMessage(fld, EM_GETLINECOUNT, 0, 0);

        /* we haven't found any of the fields yet */
        memset(field_found, 0, sizeof(field_found));

        /*
         *   Load the lines.  For each line, find the installer option
         *   name; if we find it, load the value into the corresponding
         *   field in this dialog.
         */
        for (i = 0 ; i < cnt ; ++i)
        {
            char buf[512];
            size_t len;
            char *p;

            /* get this line */
            *(WORD *)buf = (WORD)sizeof(buf) - 1;
            len = (size_t)SendMessage(fld, EM_GETLINE,
                                      (WPARAM)i, (LPARAM)buf);

            /* null-terminate the buffer */
            buf[len] = '\0';

            /* find the '=', if any */
            for (p = buf ; *p != '=' && *p != '\0' ; ++p) ;
            len = p - buf;

            /* if we found the '=', skip it to get the value */
            if (*p == '=')
            {
                /* we got a variable - skip the delimiting '=' */
                ++p;
            }
            else
            {
                /* there's no '=' - ignore this line */
                continue;
            }

            /*
             *   if it's ICON=, skip it - that's obsolete; we use a new,
             *   separate variable (set via the Special Files page) for that
             *   setting now
             */
            if (len == 4 && memicmp(buf, "icon", 4) == 0)
                continue;

            /* find the variable name in our list */
            for (fldp = insopt_flds ; fldp->nm != 0 ; ++fldp)
            {
                /* check for a match */
                if (len == strlen(fldp->nm)
                    && memicmp(fldp->nm, buf, len) == 0)
                {
                    /* mark this field as found in our list */
                    field_found[fldp - insopt_flds] = TRUE;

                    /* we found it - stop looking */
                    break;
                }
            }

            /*
             *   If we found this string in our field list, load it or
             *   store it, as appropriate
             */
            if (fldp->nm != 0)
            {
                if (loading)
                {
                    /* set our field from the free-form list */
                    SetWindowText(GetDlgItem(handle_, fldp->id), p);
                }
                else
                {
                    int start;
                    int linelen;
                    char val[512];

                    /*
                     *   store our field value in the list - get the
                     *   limits of the current line
                     */
                    start = (int)SendMessage(fld, EM_LINEINDEX, (WPARAM)i, 0);
                    linelen = (int)SendMessage(fld, EM_LINELENGTH,
                                               (WPARAM)start, 0);

                    /* get the new value from the field */
                    GetWindowText(GetDlgItem(handle_, fldp->id),
                                  val, sizeof(val));

                    /*
                     *   if the field is empty, delete the line;
                     *   otherwise, replace the value
                     */
                    if (strlen(val) == 0)
                    {
                        /* select the entire line, plus the CR-LF */
                        SendMessage(fld, EM_SETSEL, (WPARAM)start,
                                    (LPARAM)(start + linelen + 2));

                        /* back up a line in our counting */
                        --i;
                        --cnt;
                    }
                    else
                    {
                        /* select the part of the line after the '=' */
                        SendMessage(fld, EM_SETSEL, (WPARAM)(start + len + 1),
                                    (LPARAM)(start + linelen));
                    }

                    /* replace the value part with the field's new value */
                    SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)val);
                }
            }
        }

        /*
         *   If we're saving, look through our fields for any that were
         *   not in the original list.  We need to add a new line for each
         *   field that has a value in our dialog but did not exist in the
         *   original free-form list.
         */
        if (!loading)
        {
            /* scan the defined fields */
            for (i = 0, fldp = insopt_flds ; i < countof(field_found) ;
                 ++i, ++fldp)
            {
                /*
                 *   if this field was not found, check for a value in the
                 *   text field
                 */
                if (!field_found[i])
                {
                    char val[512];
                    size_t len;

                    /* build the start of the "name=value" string */
                    len = strlen(fldp->nm);
                    memcpy(val, fldp->nm, len);
                    val[len++] = '=';

                    /* get the dialog field value */
                    GetWindowText(GetDlgItem(handle_, fldp->id),
                                  val + len, sizeof(val) - len);

                    /* if it's not empty, add a line for it */
                    if (strlen(val + len) != 0)
                    {
                        int has_newline;
                        int linecnt;

                        /* get the line count */
                        linecnt =
                            (int)SendMessage(fld, EM_GETLINECOUNT, 0, 0);

                        /* get the last line */
                        if (linecnt > 0)
                        {
                            size_t line_len;
                            int line_idx;

                            /* get the index of the last line */
                            line_idx = (int)SendMessage(fld, EM_LINEINDEX,
                                (WPARAM)(linecnt - 1), 0);

                            /* get the length of the last line */
                            line_len = (size_t)SendMessage(fld, EM_LINELENGTH,
                                (WPARAM)line_idx, 0);

                            /*
                             *   If there's a trailing newline, the last
                             *   line will be empty.  Note whether this is
                             *   the case.
                             */
                            has_newline = (line_len == 0);
                        }
                        else
                        {
                            /* there are no lines -> there's no newline */
                            has_newline = FALSE;
                        }

                        /* go to the end of the free-form field */
                        SendMessage(fld, EM_SETSEL, 32767, 32767);

                        /* if necessary, add a newline */
                        if (!has_newline)
                        {
                            SendMessage(fld, EM_REPLACESEL, FALSE,
                                        (LPARAM)"\r\n");
                            SendMessage(fld, EM_SETSEL, 32767, 32767);
                        }

                        /* add our new line's text */
                        SendMessage(fld, EM_REPLACESEL, FALSE, (LPARAM)val);
                    }
                }
            }
        }
    }

    /* initialize */
    void init()
    {
        /* load the free-form list into our fields */
        parse_list(TRUE);
    }

    /* process a command */
    int do_command(WPARAM id, HWND ctl)
    {
        char buf[512];
        int strid;
        const insopt_fldinfo_t *fldp;
        char projdir[OSFNMAX];

        /* check for special notification commands */
        switch(HIWORD(id))
        {
        case EN_SETFOCUS:
            /* find this field in our list */
            for (fldp = insopt_flds ; fldp->nm != 0 ; ++fldp)
            {
                /* if this is our field, stop looking */
                if (fldp->id == LOWORD(id))
                    break;
            }

            /* get this item's info string ID */
            strid = fldp->strid;

        set_info_msg:
            /* load the string */
            LoadString(CTadsApp::get_app()->get_instance(), strid,
                       buf, sizeof(buf));

            /* display the string in the info field */
            SetWindowText(GetDlgItem(handle_, IDC_FLD_INFO), buf);

            /* message handled */
            return TRUE;

        case EN_KILLFOCUS:
            /* leaving a text field - restore default message */
            strid = IDS_INFO_NONE;
            goto set_info_msg;
        }

        /* check which control is involved */
        switch(id)
        {
        case IDOK:
            /* save the updated values back into the parent dialog */
            parse_list(FALSE);

            /* proceed to inherit the default processing */
            break;

        case IDC_BTN_BROWSE:
            /* browsing for an icon */
            mainwin_->get_project_dir(projdir, sizeof(projdir));
            ::browse_file(handle_, 0,
                          "Icons\0*.ICO\0All Files\0*.*\0\0",
                          "Select an Icon File", 0, IDC_FLD_ICON,
                          OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                          | OFN_HIDEREADONLY, TRUE, projdir);
            return TRUE;

        case IDC_BTN_BROWSE2:
            /* browse for a license text file */
            mainwin_->get_project_dir(projdir, sizeof(projdir));
            ::browse_file(handle_, 0,
                          "Text Files\0*.TXT\0All Files\0*.*\0\0",
                          "Select a License Text File", 0, IDC_FLD_LICENSE,
                          OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                          | OFN_HIDEREADONLY, TRUE, projdir);
            return TRUE;

        case IDC_BTN_BROWSE3:
            /* browse for a read-me file */
            mainwin_->get_project_dir(projdir, sizeof(projdir));
            ::browse_file(handle_, 0,
                          "HTML Files\0*.htm;*.html\0"
                          "Text Files\0*.txt\0"
                          "All Files\0*.*\0\0",
                          "Select a ReadMe File", 0, IDC_FLD_README,
                          OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                          | OFN_HIDEREADONLY, TRUE, projdir);
            return TRUE;

        case IDC_BTN_BROWSE4:
            /* browse for a read-me file */
            mainwin_->get_project_dir(projdir, sizeof(projdir));
            ::browse_file(handle_, 0,
                          "Character Map Libraries\0*.t3r\0"
                          "All Files\0*.*\0\0",
                          "Select Character Map Library", 0, IDC_FLD_CHARLIB,
                          OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                          | OFN_HIDEREADONLY, TRUE, projdir);
            return TRUE;

        default:
            break;
        }

        /* inherit the default processing */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    CTadsDialogInsOpts(HWND parent, CHtmlSys_dbgmain *mainwin)
    {
        /* remember the parent dialog and main debugger window */
        parent_ = parent;
        mainwin_ = mainwin;
    }

    /* parent editor dialog window */
    HWND parent_;

    /* the main debugger window */
    CHtmlSys_dbgmain *mainwin_;
};


/*
 *   initialize
 */
void CHtmlDialogBuildIns::init()
{
    /* initialize the installer program name */
    load_field_rel("build", "installer prog", IDC_FLD_INSTALL_EXE);

    /* initialize the installer option list */
    load_multiline_fld("build", "installer options",
                       IDC_FLD_INSTALL_OPTS, FALSE);

    /* initialize the "include in all packages" checkbox */
    int allpk = config_->getval_int("build", "installer in all pk", TRUE);
    CheckDlgButton(handle_, IDC_CK_ALLPACKAGES,
                   allpk ? BST_CHECKED : BST_UNCHECKED);

    /* inherit default */
    CHtmlDialogDebugPref::init();
}

/*
 *   process a notification
 */
int CHtmlDialogBuildIns::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_APPLY:
        /* save the new installer executable */
        save_field_rel("build", "installer prog", IDC_FLD_INSTALL_EXE, TRUE);

        /* save the new installer option list */
        save_multiline_fld("build", "installer options",
                           IDC_FLD_INSTALL_OPTS, FALSE, FALSE);

        /* save the 'include in all packages' option */
        config_->setval("build", "installer in all pk",
                        IsDlgButtonChecked(handle_, IDC_CK_ALLPACKAGES)
                        == BST_CHECKED);

        /* done */
        return TRUE;

    default:
        /* inherit default behavior */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }
}

/*
 *   process a command
 */
int CHtmlDialogBuildIns::do_command(WPARAM cmd, HWND ctl)
{
    /* check the command */
    switch(LOWORD(cmd))
    {
    case IDC_FLD_INSTALL_EXE:
    case IDC_FLD_INSTALL_OPTS:
        /* if it's an EN_CHANGE notification, note the change */
        if (HIWORD(cmd) == EN_CHANGE)
        {
            set_changed(TRUE);
            return TRUE;
        }

        /* nothing special; inherit default */
        break;

    case IDC_CK_ALLPACKAGES:
        /* note the change */
        set_changed(TRUE);
        return TRUE;

    case IDC_BTN_BROWSE:
        /* browse for the setup executable */
        browse_file("Applications\0*.exe\0All Files\0*.*\0",
                    "Select the name of the installer executable to create",
                    "exe", IDC_FLD_INSTALL_EXE, OFN_HIDEREADONLY,
                    FALSE, TRUE);

        /* handled */
        return TRUE;

    case IDC_BTN_EDIT:
        /* run the edit-installer-options dialog */
        CTadsDialogInsOpts::run_dlg(handle_, mainwin_);

        /* handled */
        return TRUE;

    case IDCANCEL:
        /* pass cancel to our parent */
        SendMessage(GetParent(handle_), WM_COMMAND, IDCANCEL, (LPARAM)ctl);
        return TRUE;

    default:
        /* it's not one of ours */
        break;
    }

    /* inherit default */
    return CHtmlDialogDebugPref::do_command(cmd, ctl);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Release ZIP
 */
class CHtmlDialogBuildZip: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildZip(CHtmlSys_dbgmain *mainwin,
                        CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        /* inherit the default handling */
        CHtmlDialogDebugPref::init();

        /* load the zip file field */
        load_field_rel("build", "zipfile", IDC_FLD_ZIP);

        /* load the 'include in all packages' checkbox */
        CheckDlgButton(handle_, IDC_CK_ALLPACKAGES,
                       config_->getval_int("build", "zip in all pk", TRUE)
                       ? BST_CHECKED : BST_UNCHECKED);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch (LOWORD(cmd))
        {
        case IDC_FLD_ZIP:
            /* on a change notification, note the change */
            if (HIWORD(cmd) == EN_CHANGE)
            {
                set_changed(TRUE);
                return TRUE;
            }
            break;

        case IDC_CK_ALLPACKAGES:
            set_changed(TRUE);
            return TRUE;

        case IDC_BTN_BROWSE:
            browse_file("ZIP files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0",
                        "Select a name for the ZIP file",
                        "zip", IDC_FLD_ZIP,
                        OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT, FALSE, TRUE);
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_APPLY:
            /* save the fields */
            save_field_rel("build", "zipfile", IDC_FLD_ZIP, TRUE);

            /* save the 'include in all packages' checkbox */
            config_->setval("build", "zip in all pk",
                            IsDlgButtonChecked(handle_, IDC_CK_ALLPACKAGES)
                            == BST_CHECKED);

            /* handled */
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

private:
};

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Source
 */
class CHtmlDialogBuildSrcZip: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildSrcZip(CHtmlSys_dbgmain *mainwin,
                           CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        /* inherit the default handling */
        CHtmlDialogDebugPref::init();

        /* load the zip file field */
        load_field_rel("build", "srczipfile", IDC_FLD_ZIP);

        /* load the 'include in all packages' checkbox */
        CheckDlgButton(handle_, IDC_CK_ALLPACKAGES,
                       config_->getval_int("build", "src in all pk", TRUE)
                       ? BST_CHECKED : BST_UNCHECKED);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch (LOWORD(cmd))
        {
        case IDC_FLD_ZIP:
            /* on a change notification, note the change */
            if (HIWORD(cmd) == EN_CHANGE)
            {
                set_changed(TRUE);
                return TRUE;
            }
            break;

        case IDC_CK_ALLPACKAGES:
            set_changed(TRUE);
            return TRUE;

        case IDC_BTN_BROWSE:
            browse_file("ZIP files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0",
                        "Select a name for the ZIP file",
                        "zip", IDC_FLD_ZIP,
                        OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT, FALSE, TRUE);
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_APPLY:
            /* save the fields */
            save_field_rel("build", "srczipfile", IDC_FLD_ZIP, TRUE);

            /* save the 'include in all packages' checkbox */
            config_->setval("build", "src in all pk",
                            IsDlgButtonChecked(handle_, IDC_CK_ALLPACKAGES)
                            == BST_CHECKED);

            /* handled */
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

private:
};

/* ------------------------------------------------------------------------ */
/*
 *   Build Options - Web Page
 */
class CHtmlDialogBuildWebPage: public CHtmlDialogDebugPref
{
public:
    CHtmlDialogBuildWebPage(CHtmlSys_dbgmain *mainwin,
                            CHtmlDebugConfig *config, int *inited)
        : CHtmlDialogDebugPref(mainwin, config, inited)
    {
    }

    void init()
    {
        /* inherit the default handling */
        CHtmlDialogDebugPref::init();

        /* load the zip file field */
        load_field_rel("build", "webpage dir", IDC_FLD_DIR);

        /* load the checkboxes */
        CheckDlgButton(handle_, IDC_CK_ALLPACKAGES,
                       config_->getval_int("build", "webpage in all pk", TRUE)
                       ? BST_CHECKED : BST_UNCHECKED);

        CheckDlgButton(handle_, IDC_CK_INCLUDE_SETUP,
                       config_->getval_int(
                           "build", "installer in webpage", FALSE)
                       ? BST_CHECKED : BST_UNCHECKED);

        CheckDlgButton(handle_, IDC_CK_INCLUDE_SRC,
                       config_->getval_int("build", "src in webpage", FALSE)
                       ? BST_CHECKED : BST_UNCHECKED);

        CheckRadioButton(handle_, IDC_RB_GAME, IDC_RB_ZIP,
                         config_->getval_int("build", "zip in webpage", TRUE)
                         ? IDC_RB_ZIP : IDC_RB_GAME);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        char fname[OSFNMAX];

        switch (LOWORD(cmd))
        {
        case IDC_FLD_DIR:
            /* on a change notification, note the change */
            if (HIWORD(cmd) == EN_CHANGE)
            {
                set_changed(TRUE);
                return TRUE;
            }
            break;

        case IDC_CK_ALLPACKAGES:
        case IDC_CK_INCLUDE_SETUP:
        case IDC_CK_INCLUDE_SRC:
        case IDC_RB_GAME:
        case IDC_RB_ZIP:
            set_changed(TRUE);
            return TRUE;

        case IDC_BTN_BROWSE:
            /* load the current setting */
            GetDlgItemText(handle_, IDC_FLD_DIR, fname, sizeof(fname));
            if (fname[0] == '\0')
                GetCurrentDirectory(sizeof(fname), fname);

            /* browse for a folder */
            if (browse_folder(
                GetParent(handle_), "Web Page &Folder:",
                "Select Web Page Output Folder", fname, sizeof(fname),
                fname, TRUE))
            {
                /* set the new folder */
                SetDlgItemText(handle_, IDC_FLD_DIR, fname);
            }
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_command(cmd, ctl);
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_APPLY:
            /* save the fields */
            save_field_rel("build", "webpage dir", IDC_FLD_DIR, TRUE);

            /* save the checkboxes */
            config_->setval("build", "webpage in all pk",
                            IsDlgButtonChecked(handle_, IDC_CK_ALLPACKAGES)
                            == BST_CHECKED);
            config_->setval("build", "installer in webpage",
                            IsDlgButtonChecked(handle_, IDC_CK_INCLUDE_SETUP)
                            == BST_CHECKED);
            config_->setval("build", "src in webpage",
                            IsDlgButtonChecked(handle_, IDC_CK_INCLUDE_SRC)
                            == BST_CHECKED);
            config_->setval("build", "zip in webpage",
                            IsDlgButtonChecked(handle_, IDC_RB_ZIP));

            /* handled */
            return TRUE;
        }

        /* inherit the default handling */
        return CHtmlDialogDebugPref::do_notify(nm, ctl);
    }

private:
};

/* ------------------------------------------------------------------------ */
/*
 *   Run the build options dialog
 */
void run_dbg_build_dlg(HWND owner, CHtmlSys_dbgmain *mainwin,
                       int init_page_id, int init_ctl_id)
{
    PROPSHEETPAGE psp[16];
    PROPSHEETHEADER psh;
    CHtmlDialogBuildSrc *src_dlg = 0;
    CHtmlDialogBuildInc *inc_dlg;
    CHtmlDialogBuildOut *out_dlg;
    CHtmlDialogBuildDef *def_dlg;
    CHtmlDialogBuildErr *err_dlg = 0;
    CHtmlDialogBuildAdv *adv_dlg;
    CHtmlDialogBuildInter *int_dlg = 0;
    CHtmlDialogBuildIns *ins_dlg;
    CHtmlDialogBuildZip *zip_dlg = 0;
    CHtmlDialogBuildSrcZip *srczip_dlg = 0;
    CHtmlDialogBuildWebPage *webpage_dlg = 0;
    CHtmlDialogBuildScripts *scripts_dlg = 0;
    CHtmlDialogBuildSpecialFiles *special_dlg = 0;
    int i;
    int inited = FALSE;

    /*
     *   Create our pages.  Note that there is no need for a source page
     *   if the debugger uses the "project" window, since the project
     *   window supersedes the source page.
     */
    if (mainwin->get_proj_win() == 0)
        src_dlg = new CHtmlDialogBuildSrc(mainwin, mainwin->get_local_config(),
                                          &inited);
    inc_dlg = new CHtmlDialogBuildInc(mainwin, mainwin->get_local_config(),
                                      &inited);
    if (w32_system_major_vsn >= 3)
        int_dlg = new CHtmlDialogBuildInter(mainwin,
                                            mainwin->get_local_config(),
                                            &inited);
    out_dlg = new CHtmlDialogBuildOut(mainwin, mainwin->get_local_config(),
                                      &inited);
    def_dlg = new CHtmlDialogBuildDef(mainwin, mainwin->get_local_config(),
                                      &inited);

    if (w32_system_major_vsn >= 3)
        err_dlg = new CHtmlDialogBuildErr(mainwin, mainwin->get_local_config(),
                                          &inited);
    adv_dlg = new CHtmlDialogBuildAdv(mainwin, mainwin->get_local_config(),
                                      &inited);
    ins_dlg = new CHtmlDialogBuildIns(mainwin, mainwin->get_local_config(),
                                      &inited);

    if (w32_system_major_vsn >= 3)
    {
        scripts_dlg = new CHtmlDialogBuildScripts(
            mainwin, mainwin->get_local_config(), &inited);
        zip_dlg = new CHtmlDialogBuildZip(
            mainwin, mainwin->get_local_config(), &inited);
        srczip_dlg = new CHtmlDialogBuildSrcZip(
            mainwin, mainwin->get_local_config(), &inited);
        webpage_dlg = new CHtmlDialogBuildWebPage(
            mainwin, mainwin->get_local_config(), &inited);
    }
    else
    {
        special_dlg = new CHtmlDialogBuildSpecialFiles(
            mainwin, mainwin->get_local_config(), &inited);
    }

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Project & Build Settings";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptors */
    i = 0;
    if (src_dlg != 0)
        src_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_SRC, IDS_BUILD_SRC,
                              init_page_id, init_ctl_id, &psh);
    inc_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_INC, IDS_BUILD_INC,
                          init_page_id, init_ctl_id, &psh);
    if (int_dlg != 0)
        int_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_INTERMEDIATE,
                              IDS_BUILD_INTERMEDIATE,
                              init_page_id, init_ctl_id, &psh);
    if (special_dlg != 0)
        special_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                                  &psp[i++], DLG_BUILD_SPECIAL_FILES,
                                  IDS_BUILD_SPECIAL_FILES,
                                  init_page_id, init_ctl_id, &psh);
    out_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_OUTPUT, IDS_BUILD_OUTPUT,
                          init_page_id, init_ctl_id, &psh);
    def_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_DEFINES, IDS_BUILD_DEFINES,
                          init_page_id, init_ctl_id, &psh);
    if (err_dlg != 0)
        err_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_DIAGNOSTICS,
                              IDS_BUILD_DIAGNOSTICS,
                              init_page_id, init_ctl_id, &psh);
    adv_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_ADVANCED, IDS_BUILD_ADVANCED,
                          init_page_id, init_ctl_id, &psh);

    /* add the auto-scripting dialog */
    if (scripts_dlg != 0)
        scripts_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                                  &psp[i++], DLG_BUILD_SCRIPTS,
                                  IDS_BUILD_SCRIPTS, init_page_id, 0, &psh);

    ins_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_BUILD_INSTALL, IDS_BUILD_INSTALL,
                          init_page_id, init_ctl_id, &psh);

    if (zip_dlg != 0)
        zip_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                              &psp[i++], DLG_BUILD_ZIP, IDS_BUILD_ZIP,
                              init_page_id, init_ctl_id, &psh);

    if (webpage_dlg != 0)
        webpage_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                                  &psp[i++], DLG_BUILD_WEBPAGE,
                                  IDS_BUILD_WEBPAGE,
                                  init_page_id, init_ctl_id, &psh);

    if (srczip_dlg != 0)
        srczip_dlg->init_ps_page(CTadsApp::get_app()->get_instance(),
                                 &psp[i++], DLG_BUILD_SRCZIP,
                                 IDS_BUILD_SRCZIP,
                                 init_page_id, init_ctl_id, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* run the property sheet */
    CTadsDialog::run_tree_propsheet(mainwin->get_handle(), &psh);

    /* delete the dialogs */
    if (src_dlg != 0)
        delete src_dlg;
    delete inc_dlg;
    if (err_dlg != 0)
        delete err_dlg;
    if (int_dlg != 0)
        delete int_dlg;
    delete out_dlg;
    delete def_dlg;
    delete adv_dlg;
    delete ins_dlg;

    if (zip_dlg != 0)
        delete zip_dlg;
    if (webpage_dlg != 0)
        delete webpage_dlg;
    if (srczip_dlg != 0)
        delete srczip_dlg;
    if (scripts_dlg != 0)
        delete scripts_dlg;
    if (special_dlg != 0)
        delete special_dlg;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set default debugger values for a configuration
 */
void set_dbg_defaults(CHtmlSys_dbgmain *dbgmain)
{
    CHtmlDebugConfig *config = dbgmain->get_global_config();
    CHtmlDebugConfig *lc = dbgmain->get_local_config();
    textchar_t buf[OSFNMAX];

    /* set the default editor to "notepad */
    if (config->getval("editor", "program", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        config->setval("editor", "program", 0, "Notepad.EXE");

    /* set the default command line to "%f" */
    if (config->getval("editor", "cmdline", 0, buf, sizeof(buf))
        || strlen(buf) == 0)
        config->setval("editor", "cmdline", 0, "%f");

    /* set the default QUIT sequences for auto-scripts, if not already set */
    if (!lc->getval_int("auto-script", "default quit sequences set", FALSE))
    {
        /* add the default QUIT sequences */
        lc->setval("auto-script", "quit sequences", 0,
                   "<nocase>[<]line[>]<space>*q(uit)?<space>*");
        lc->setval("auto-script", "quit sequences", 1,
                   "<nocase>[<]line[>]<space>*y.*");

        /* note that the defaults are now set */
        lc->setval("auto-script", "default quit sequences set", TRUE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Set default build values for a configuration
 */
void set_dbg_build_defaults(CHtmlSys_dbgmain *dbgmain,
                            const textchar_t *gamefile)
{
    CHtmlDebugConfig *config = dbgmain->get_local_config();
    char buf[OSFNMAX + 50];
    char basename[OSFNMAX];
    int val;

    /*
     *   check to see if we've already applied the defaults; if so, don't
     *   apply them again, because the user might have deleted or modified
     *   values we previously stored as defaults
     */
    if (!config->getval("build", "defaults set", &val) && val)
        return;

    /* set version-specific defaults */
    vsn_set_dbg_build_defaults(config, dbgmain, gamefile);

    /*
     *   Get the base name for the various derived files - use the game
     *   file's name and the main project path.
     */
    if (gamefile != 0)
    {
        dbgmain->get_project_dir(buf, sizeof(buf));
        os_build_full_path(basename, sizeof(basename), buf,
                           os_get_root_name((textchar_t *)gamefile));
    }

    /*
     *   Set the default .EXE file to be the same as the game name, with
     *   the .GAM replaced by .EXE
     */
    if (gamefile != 0)
    {
        strcpy(buf, basename);
        os_remext(buf);
        os_addext(buf, "exe");
    }
    else
        buf[0] = '\0';

    /*
     *   set the name - use the root name, so that it ends up in the same
     *   directory as the project file
     */
    config->setval("build", "exe", 0, buf);

    /* set the default release game to the name of debug game plus "_rls" */
    if (gamefile != 0)
    {
        strcpy(buf, basename);
        os_remext(buf);
        strcat(buf, "_rls");
        os_addext(buf, w32_game_ext);
    }
    else
        buf[0] = '\0';

    /* set the root name */
    config->setval("build", "rlsgam", 0, os_get_root_name(buf));

    /* add the default installer options */
    if (gamefile != 0)
    {
        /* set the name of the installer */
        strcpy(buf, basename);
        os_remext(buf);
        strcat(buf, "_Setup");
        os_addext(buf, "exe");
        config->setval("build", "installer prog", 0, buf);

        /* set the game name to that of the game file, minus the extension */
        sprintf(buf, "name=%s", os_get_root_name((char *)gamefile));
        os_remext(buf + 5);
        config->setval("build", "installer options", 0, buf);

        /*
         *   set the default saved game extension to the name of the game,
         *   minus its extension
         */
        sprintf(buf, "savext=%s", os_get_root_name((char *)gamefile));
        os_remext(buf + 5);
        config->setval("build", "installer options", 1, buf);
    }

    /* set the default release ZIP file - use the game root name plus .zip */
    if (gamefile != 0)
    {
        strcpy(buf, basename);
        os_remext(buf);
        strcat(buf, ".zip");
        config->setval("build", "zipfile", 0, buf);
    }

    /* set the default source ZIP file */
    if (gamefile != 0)
    {
        strcpy(buf, basename);
        os_remext(buf);
        strcat(buf, "Source.zip");
        config->setval("build", "srczipfile", 0, buf);
    }

    /*
     *   make a note that we've set the default configuration, so that we
     *   don't try to set it again in the future
     */
    config->setval("build", "defaults set", TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear build information from a configuration
 */
void clear_dbg_build_info(CHtmlDebugConfig *config)
{
    config->clear_strlist("build", "source");
    config->clear_strlist("build", "resources");
    config->clear_strlist("build", "includes");
    config->clear_strlist("build", "exe");
    config->clear_strlist("build", "rlsgam");
    config->clear_strlist("build", "defines");
    config->clear_strlist("build", "undefs");
    config->setval("build", "ignore case", FALSE);
    config->setval("build", "c ops", FALSE);
    config->setval("build", "use charmap", FALSE);
    config->clear_strlist("build", "charmap");
    config->clear_strlist("build", "extra options");
    config->clear_strlist("build", "installer options");
    config->clear_strlist("build", "installer prog");
    config->setval("build", "defaults set", FALSE);
}

