/* $Header: d:/cvsroot/tads/html/win32/w32fndlg.h,v 1.2 1999/05/17 02:52:26 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32fndlg.h - TADS win32 debugger "Find" dialog
Function

Notes

Modified
  03/11/98 MJRoberts  - Creation
*/

#ifndef W32FNDLG_H
#define W32FNDLG_H

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif

/* search scope levels */
enum itfdo_scope
{
    ITFDO_SCOPE_FILE = 0,                              /* current file only */
    ITFDO_SCOPE_LOCAL = 1,            /* local project files - no libraries */
    ITFDO_SCOPE_GLOBAL = 2        /* all project files, including libraries */
};

/*
 *   the basic Find dialog
 */
class CTadsDialogFind: public CTadsDialog
{
public:
    CTadsDialogFind();
    ~CTadsDialogFind();

    /*
     *   Run the dialog and get the string to find.  Puts the string into the
     *   caller's buffer and returns true if we got a string; returns false
     *   and leaves the buffer and option flags unchanged if the user
     *   canceled the dialog.
     */
    int run_find_dlg(HWND parent_hdl, int dlg_id,
                     textchar_t *buf, size_t buflen);

    /* initialize */
    void init();

    /* handle a command */
    int do_command(WPARAM cmd, HWND ctl);

    /* forget all old search strings */
    void forget_old_search_strings();

    /* set an old search string */
    void set_old_search_string(int idx, const char *str, size_t len,
                               const char *opts);

    /* get an old search string - returns null if the string isn't set */
    const char *get_old_search_string(int idx, char *opt, size_t optlen) const
    {
        /* make sure the index is in range */
        if (idx >= get_max_old_search_strings())
            return 0;

        /* copy the options, and return the string pointer */
        safe_strcpy(opt, optlen, past_opts_[idx]);
        return past_strings_[idx];
    }

    /* get the maximum number of old search strings */
    int get_max_old_search_strings() const
        { return sizeof(past_strings_)/sizeof(past_strings_[0]); }

    /*
     *   The option flags.  The dialog updates these when the user dismisses
     *   the dialog with Find, and leaves them unchanged on dismissing with
     *   Cancel.
     */
    int exact_case;
    int start_at_top;
    int wrap_around;
    int direction;                      /* 1 for forwards, -1 for backwards */
    int collapse_sp;

protected:
    /*
     *   initialize our position - by default, we simply center the dialog on
     *   the desktop
     */
    virtual void init_dlg_pos() { center_dlg_win(handle_); }

    /* read the current control settings into our member variables */
    virtual int read_control_settings();

    /* save the current Find text in the history list */
    void save_find_text();

    /* enable or disable the Find button */
    virtual void enable_find_button(int ena)
        { EnableWindow(GetDlgItem(handle_, IDOK), ena); }

    /* build the options string */
    virtual void build_opts(char *buf, size_t len)
    {
        /* add the exact_case option */
        if (exact_case && len > 1)
            *buf++ = 'C', --len;

        /* add the ignore-spaces option */
        if (collapse_sp && len > 1)
            *buf++ = 'S', --len;

        /* null-terminate it */
        if (len != 0)
            *buf = '\0';
    }

    /* set the options from a saved search */
    virtual void set_opts(const char *p)
    {
        /* set defaults */
        CheckDlgButton(handle_, IDC_CK_MATCHCASE, BST_UNCHECKED);
        CheckDlgButton(handle_, IDC_CK_COLLAPSESP, BST_UNCHECKED);

        /* check each character */
        for ( ; *p != '\0' ; ++p)
        {
            switch (*p)
            {
            case 'C':
                CheckDlgButton(handle_, IDC_CK_MATCHCASE, BST_CHECKED);
                break;

            case 'S':
                CheckDlgButton(handle_, IDC_CK_COLLAPSESP, BST_CHECKED);
                break;
            }
        }
    }

    /*
     *   Caller's buffer for the result string.  We'll copy the result
     *   here if the user dismisses the dialog with "Find" (rather than
     *   "Cancel").
     */
    textchar_t *buf_;
    size_t buflen_;

    /*
     *   Stored strings from past invocations.  Each time the user enters
     *   a string and accepts it, we add the string to our list; we keep
     *   only the ten most recent strings if more than that have been
     *   entered.
     */
    textchar_t *past_strings_[10];

    /*
     *   Option settings for past strings.  This is a null-terminated string
     *   giving the options set with a particular search string: 'C' if
     *   exact_case is true.  We leave room for extra characters for use in
     *   subclasses with extra options.
     */
    textchar_t past_opts_[10][10];
};

/* ------------------------------------------------------------------------ */
/*
 *   Extended version of the Find dialog, offering a regular expression
 *   option and modeless operation.
 */
class CTadsDialogFindRegex: public CTadsDialogFind
{
public:
    CTadsDialogFindRegex(class ITadsFindDialogOwner *handler)
    {
        regex = FALSE;
        whole_word = FALSE;
        scope = ITFDO_SCOPE_FILE;
        handler_ = handler;
    }

    /* regular expression option setting */
    int regex;

    /* whole-word option setting */
    int whole_word;

    /* search scope - current file, local files, entire project */
    itfdo_scope scope;

    /* run the dialog modlessly */
    HWND run_find_modeless(HWND parent_hdl, int dlg_id,
                           textchar_t *findbuf, size_t findbuflen);

    /* get the resource ID last used to open the dialog */
    int get_dlg_id() { return dlg_id_; }

    /* adjust our buttons for what's allowed in the current context */
    void adjust_context_buttons();

protected:
    /* initialize our position */
    virtual void init_dlg_pos();

    /* message handler - instance method */
    virtual BOOL do_dialog_msg(HWND dlg_hwnd, UINT message, WPARAM wpar,
                               LPARAM lpar);

    /* read the current control settings into our member variables */
    virtual int read_control_settings();

    /* initialize */
    void init();

    /* build the options string */
    virtual void build_opts(char *buf, size_t len)
    {
        /* add the regex option */
        if (regex && len > 1)
            *buf++ = 'R', --len;

        /* add the whole-word option */
        if (whole_word && len > 1)
            *buf++ = 'W', --len;

        /* inherit the base handling */
        CTadsDialogFind::build_opts(buf, len);
    }

    /* set the options from a saved search */
    virtual void set_opts(const char *opts)
    {
        const char *p;

        /* set defaults */
        CheckDlgButton(handle_, IDC_CK_REGEX, BST_UNCHECKED);
        CheckDlgButton(handle_, IDC_CK_WORD, BST_UNCHECKED);

        /* check each character */
        for (p = opts ; *p != '\0' ; ++p)
        {
            switch (*p)
            {
            case 'R':
                CheckDlgButton(handle_, IDC_CK_REGEX, BST_CHECKED);
                break;

            case 'W':
                CheckDlgButton(handle_, IDC_CK_WORD, BST_CHECKED);
                break;
            }
        }

        /* inherit the base handling */
        CTadsDialogFind::set_opts(opts);
    }

    /* the dialog template last used to open the dialog */
    int dlg_id_;

    /* our owner interface */
    class ITadsFindDialogOwner *handler_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Extended version for Find-and-Replace, with regular expressions
 */
class CTadsDialogFindReplace: public CTadsDialogFindRegex
{
public:
    CTadsDialogFindReplace(class ITadsFindDialogOwner *handler)
        : CTadsDialogFindRegex(handler)
    {
        /* no old search strings yet */
        memset(past_repl_, 0, sizeof(past_repl_));

        /* no replacement buffer yet */
        rbuf_ = 0;
    }

    ~CTadsDialogFindReplace();

    /* run the Replace dialog modelessly */
    HWND run_replace_modeless(HWND parent_hdl, int dlg_id,
                              textchar_t *findbuf, size_t findbuflen,
                              textchar_t *replbuf, size_t replbuflen);

    /* set a replacement history string */
    void set_old_repl_string(int idx, const char *str, size_t len);

    /* get a replacement history string */
    const char *get_old_repl_string(int idx) const
        { return idx < get_max_old_repl_strings() ? past_repl_[idx] : 0; }

    /* get the maximum number of replacement history strings */
    int get_max_old_repl_strings() const
        { return sizeof(past_repl_)/sizeof(past_repl_[0]); }

protected:
    /* initialize */
    void init();

    /* handled a command */
    int do_command(WPARAM cmd, HWND ctl);

    /* read the current control settings into our member variables */
    virtual int read_control_settings();

    /* save the replacement text */
    void save_repl_text();

    /*
     *   Enable or disable the Find button.  We'll take this opportunity to
     *   enable the Replace button as well, depending on conditions.
     */
    virtual void enable_find_button(int ena)
    {
        /* do the normal work */
        CTadsDialogFindRegex::enable_find_button(ena);

        /*
         *   enable the Replace button if we've found something from a past
         *   search, or any time the Find button is enabled
         */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REPLACE), found_ || ena);
    }

    /* current replacement text buffer */
    textchar_t *rbuf_;
    size_t rbuflen_;

    /* stored replacement strings from past invocations */
    textchar_t *past_repl_[10];

    /* have we found an occurrence yet? */
    int found_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Dialog owner callback object.  This is an interface that lets the dialog
 *   call back to the owner window to carry out find/replace operations.
 */
class ITadsFindDialogOwner
{
public:
    /* the Find dialog is becoming the active window */
    virtual void itfdo_activate() = 0;

    /* is Find allowed in the current context? */
    virtual int itfdo_allow_find(int regex) = 0;

    /* is Replace allowed in the current context? */
    virtual int itfdo_allow_replace() = 0;

    /*
     *   Note that a new search is starting.  We call this when we open the
     *   dialog, and whenever we're about to perform a search with a
     *   different search term from the last search.  (This includes changes
     *   to the options, such as regular expression or exact-case mode, since
     *   new options form a new search.)  The owner can use this to determine
     *   when a wrapped search has wrapped around back to the first match.
     */
    virtual void itfdo_new_search() = 0;

    /*
     *   Find the next occurrence of the given text with the given options.
     *   If found, select the text and return TRUE; otherwise return FALSE.
     */
    virtual int itfdo_find_text(
        const textchar_t *txt,
        int exact_case, int wrap, int direction, int regex,
        int whole_word, itfdo_scope scope) = 0;

    /*
     *   Replace the current selection with the given replacement string, and
     *   find the next match.  Returns TRUE if another match was found, FALSE
     *   if not.  If regex is true, the special symbols \1 through \9 in the
     *   replacement text are group substitutions: they're replaced with the
     *   corresponding tagged expressions from the pattern string.
     */
    virtual int itfdo_replace_sel(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex,
        int whole_word, itfdo_scope scope) = 0;

    /*
     *   Replace all occurrences with the given text.  Returns the number of
     *   replacements actually made.
     */
    virtual int itfdo_replace_all(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex,
        int whole_word, itfdo_scope scope) = 0;

    /* save the dialog position in the configuration file */
    virtual void itfdo_save_pos(const CHtmlRect *rc) = 0;

    /*
     *   load the dialog position from the configuration file; returns true
     *   if a position was successfully loaded
     */
    virtual int itfdo_load_pos(CHtmlRect *rc) = 0;
};

#endif /* W32FNDLG_H */

