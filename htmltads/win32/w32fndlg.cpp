#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/w32fndlg.cpp,v 1.3 1999/07/11 00:46:51 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32fnddlg.cpp - TADS win32 debugger "Find" dialog
Function

Notes

Modified
  03/11/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef W32FNDLG_H
#include "w32fndlg.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


CTadsDialogFind::CTadsDialogFind()
{
    int i;

    /* we have no old strings yet - clear out our array */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        past_strings_[i] = 0;
        past_opts_[i][0] = '\0';
    }

    /*
     *   set the default options: forward, no wrapping, case insensitive,
     *   from current position
     */
    exact_case = FALSE;
    wrap_around = FALSE;
    direction = 1;
    start_at_top = FALSE;
    collapse_sp = FALSE;
}

CTadsDialogFind::~CTadsDialogFind()
{
    int i;

    /* delete any old strings we've allocated */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if we've allocated this one, delete it now */
        if (past_strings_[i] != 0)
            th_free(past_strings_[i]);
    }
}

/*
 *   Run the dialog
 */
int CTadsDialogFind::run_find_dlg(HWND parent_hdl, int dlg_id,
                                  textchar_t *buf, size_t buflen)
{
    /*
     *   set up our pointer to the caller's buffer, so that the dialog can
     *   copy the string into the buffer when the user hits "find"
     */
    buf_ = buf;
    buflen_ = buflen;

    /* run the dialog, returning true only if they hit "find" */
    int ret = (run_modal(dlg_id, parent_hdl,
                         CTadsApp::get_app()->get_instance()) == IDOK);

    /* forget the buffer */
    buf_ = 0;
    buflen_ = 0;

    /* return the result */
    return ret;
}

/*
 *   Initialize
 */
void CTadsDialogFind::init()
{
    int i;
    HWND combo;

    /* initialize our position */
    init_dlg_pos();

    /*
     *   make sure that the current search is in the saved string list - this
     *   ensures that scrolling into the combo box list won't lose the
     *   initial string
     */
    save_find_text();

    /* load up the combo box with the old strings */
    combo = GetDlgItem(handle_, IDC_CBO_FINDWHAT);
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if we have a string in this slot, add it */
        if (past_strings_[i] != 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)past_strings_[i]);
    }

    /*
     *   set the initial enabling of the combo box according to whether we
     *   have any text for it
     */
    enable_find_button(get_strlen(buf_) != 0);

    /* put the initial text into the combo box */
    SetWindowText(GetDlgItem(handle_, IDC_CBO_FINDWHAT), buf_);

    /* set the initial checkbox and radio states */
    CheckDlgButton(handle_, IDC_CK_MATCHCASE,
                   (exact_case ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_WRAP,
                   (wrap_around ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_STARTATTOP,
                   (start_at_top ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_RB_UP,
                   (direction == -1 ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_RB_DOWN,
                   (direction == 1 ? BST_CHECKED : BST_UNCHECKED));
    CheckDlgButton(handle_, IDC_CK_COLLAPSESP,
                   collapse_sp ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   process a command
 */
int CTadsDialogFind::do_command(WPARAM cmd, HWND ctl)
{
    int idx;

    switch(LOWORD(cmd))
    {
    case IDC_CBO_FINDWHAT:
        switch(HIWORD(cmd))
        {
        case CBN_EDITCHANGE:
            /*
             *   The text in the combo has change.  Enable the "Find"
             *   button if the combo is non-empty, and disable the button
             *   if the combo is empty.
             */
            enable_find_button(GetWindowTextLength(ctl) != 0);
            return TRUE;

        case CBN_SELCHANGE:
            /* they selected something, so there must be some text now */
            enable_find_button(TRUE);

            /* set the options associated with the selection */
            idx = SendMessage(GetDlgItem(handle_, IDC_CBO_FINDWHAT),
                              CB_GETCURSEL, 0, 0);
            if (idx != -1)
                set_opts(past_opts_[idx]);

            /* handled */
            return TRUE;

        default:
            break;
        }
        return FALSE;

    case IDC_BTN_CLOSE:
        /* save settings */
        read_control_settings();
        save_find_text();

        /* close the dialog */
        goto close_dialog;

    case IDCANCEL:
    close_dialog:
        /*
         *   Cancel the dialog without updating any of the saved settings.
         *   If I'm running modally, end the dialog loop; otherwise, just
         *   destroy the dialog window.
         */
        if (is_modal_)
            EndDialog(handle_, IDCANCEL);
        else
            DestroyWindow(handle_);
        return TRUE;

    case IDOK:
        /* read the new control settings */
        read_control_settings();

        /* save the current Find text in the past search list */
        save_find_text();

        /* end the dialog */
        if (is_modal_)
            EndDialog(handle_, IDOK);
        else
            DestroyWindow(handle_);
        return TRUE;

    default:
        /* inherit default handling */
        return CTadsDialog::do_command(cmd, ctl);
    }
}

/*
 *   read the current control settings into our member variables
 */
int CTadsDialogFind::read_control_settings()
{
    int changed;
    CStringBuf oldbuf(buf_);

    /* get the checkbox options */
    int new_exact_case = (IsDlgButtonChecked(handle_, IDC_CK_MATCHCASE)
                          == BST_CHECKED);
    int new_wrap_around = (IsDlgButtonChecked(handle_, IDC_CK_WRAP)
                           == BST_CHECKED);
    int new_start_at_top = (IsDlgButtonChecked(handle_, IDC_CK_STARTATTOP)
                            == BST_CHECKED);
    int new_direction = ((IsDlgButtonChecked(handle_, IDC_RB_UP)
                          == BST_CHECKED) ? -1 : 1);
    int new_collapse_sp = (IsDlgButtonChecked(handle_, IDC_CK_COLLAPSESP)
                           == BST_CHECKED);

    /* check for a change to the options */
    changed = (exact_case != new_exact_case
               || wrap_around != new_wrap_around
               || start_at_top != new_start_at_top
               || direction != new_direction
               || collapse_sp != new_collapse_sp);

    /* save the new settings */
    exact_case = new_exact_case;
    wrap_around = new_wrap_around;
    start_at_top = new_start_at_top;
    direction = new_direction;
    collapse_sp = new_collapse_sp;

    /* copy the Find text into the caller's buffer */
    GetWindowText(GetDlgItem(handle_, IDC_CBO_FINDWHAT), buf_, buflen_);

    /* note changes to the Find text */
    changed |= (strcmp(oldbuf.get(), buf_) != 0);

    /* return the change indication */
    return changed;
}

/*
 *   save the current Find text in the past string list
 */
void CTadsDialogFind::save_find_text()
{
    size_t cnt;
    size_t i;
    char opts[10];

    /* build the option string */
    build_opts(opts, sizeof(opts));

    /* get the maximum number of strings to save */
    cnt = sizeof(past_strings_)/sizeof(past_strings_[0]);

    /*
     *   Scan for an existing copy of the current string.  If it's already in
     *   the list, move it to the head of the list (i.e., remove it from its
     *   current position and reinsert it at the first position).
     */
    for (i = 0 ; i < cnt && past_strings_[i] != 0 ; ++i)
    {
        /* if this one matches, stop looking */
        if (get_stricmp(past_strings_[i], buf_) == 0
            && get_strcmp(past_opts_[i], opts) == 0)
            break;
    }

    /* if we found a match, simply move it to the first position */
    if (i != cnt && past_strings_[i] != 0)
    {
        int j;
        textchar_t *str;

        /* remember the string we're moving for a moment */
        str = past_strings_[i];

        /*
         *   close the gap by moving all of the strings from the first
         *   position to the one before the one we're removing forward one
         *   position in the list
         */
        for (j = i ; j != 0 ; --j)
        {
            past_strings_[j] = past_strings_[j-1];
            do_strcpy(past_opts_[j], past_opts_[j-1]);
        }

        /* put this item back in as the first string */
        past_strings_[0] = str;
        strcpy(past_opts_[0], opts);
    }
    else
    {
        /*
         *   this search string is new, so we'll insert it; if we're pushing
         *   one off the end, free its memory
         */
        if (past_strings_[cnt-1] != 0)
            th_free(past_strings_[cnt-1]);

        /* move all of the strings down a slot */
        for (i = cnt - 1 ; i != 0 ; --i)
        {
            past_strings_[i] = past_strings_[i-1];
            do_strcpy(past_opts_[i], past_opts_[i-1]);
        }

        /* allocate space for the new string */
        past_strings_[0] = (textchar_t *)
                           th_malloc((get_strlen(buf_) + 1)
                                     * sizeof(textchar_t));

        /* copy it */
        do_strcpy(past_strings_[0], buf_);
        do_strcpy(past_opts_[0], opts);
    }
}

/*
 *   forget the old search strings
 */
void CTadsDialogFind::forget_old_search_strings()
{
    int i;

    /* loop through our strings and delete each one */
    for (i = 0 ; i < sizeof(past_strings_)/sizeof(past_strings_[0]) ; ++i)
    {
        /* if this one was allocated, free it and clear the slot */
        if (past_strings_[i] != 0)
        {
            th_free(past_strings_[i]);
            past_strings_[i] = 0;
        }
    }
}

/*
 *   set an old search string
 */
void CTadsDialogFind::set_old_search_string(
    int idx, const char *str, size_t len, const char *opts)
{
    /* make sure it's in range */
    if (idx < 0 || idx >= sizeof(past_strings_)/sizeof(past_strings_[0]))
        return;

    /* if it's already allocated, free the old one */
    if (past_strings_[idx] != 0)
        th_free(past_strings_[idx]);

    /* allocate space to save a copy of the string */
    past_strings_[idx] = (textchar_t *)
                         th_malloc((len + 1) * sizeof(textchar_t));

    /* make a copy of the string in our allocated buffer */
    memcpy(past_strings_[idx], str, len*sizeof(textchar_t));
    past_strings_[idx][len] = '\0';

    /* save the options */
    do_strcpy(past_opts_[idx], opts);
}

/* ------------------------------------------------------------------------ */
/*
 *   Regular Expression extension
 */

/*
 *   initialize
 */
void CTadsDialogFindRegex::init()
{
    /* inherit the default handling */
    CTadsDialogFind::init();

    /* add myself to the application modal dialog list */
    CTadsApp::get_app()->add_modeless(handle_);

    /* initialize the regex checkbox */
    CheckDlgButton(handle_, IDC_CK_REGEX,
                   regex ? BST_CHECKED : BST_UNCHECKED);

    /* initialize the whole-word checkbox */
    CheckDlgButton(handle_, IDC_CK_WORD,
                   whole_word ? BST_CHECKED : BST_UNCHECKED);

    /* initialize the single file/project wide radio buttons */
    CheckDlgButton(handle_, IDC_RB_CURFILE,
                   scope == ITFDO_SCOPE_FILE ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_RB_PROJFILES,
                   scope == ITFDO_SCOPE_LOCAL ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(handle_, IDC_RB_ALLFILES,
                   scope == ITFDO_SCOPE_GLOBAL ? BST_CHECKED : BST_UNCHECKED);
}

/*
 *   initialize our position
 */
void CTadsDialogFindRegex::init_dlg_pos()
{
    RECT rc;
    CHtmlRect hrc;

    /* try loading a saved position from the configuration */
    if (handler_ != 0 && handler_->itfdo_load_pos(&hrc))
    {
        RECT deskrc;

        /*
         *   got it - use the saved position, but note the current dialog
         *   position so that we can show it at the same size
         */
        GetWindowRect(handle_, &rc);

        /*
         *   make sure the restored position is within the desktop region, in
         *   case the saved position came from a machine with a bigger
         *   monitor
         */
        GetClientRect(GetDesktopWindow(), &deskrc);
        if (hrc.left < deskrc.left
            || hrc.left + (rc.right - rc.left) > deskrc.right
            || hrc.top < deskrc.top
            || hrc.top + (rc.bottom - rc.top) > deskrc.bottom)
        {
            /* it's outside the desktop bounds - use default positioning */
            CTadsDialogFind::init_dlg_pos();
        }
        else
        {
            /* set the saved position */
            MoveWindow(handle_, hrc.left, hrc.top,
                       rc.right - rc.left, rc.bottom - rc.top, TRUE);
        }
    }
    else
    {
        /* no saved position - use the default positioning */
        CTadsDialogFind::init_dlg_pos();
    }
}

/*
 *   read the control settings
 */
int CTadsDialogFindRegex::read_control_settings()
{
    /* inherit the default handling */
    int changed = CTadsDialogFind::read_control_settings();

    /* get the new Regular Expression setting */
    int new_re = (IsDlgButtonChecked(handle_, IDC_CK_REGEX) == BST_CHECKED);

    /* get the new Whole Word setting */
    int new_word = (IsDlgButtonChecked(handle_, IDC_CK_WORD) == BST_CHECKED);

    /* get the new single file/project wide setting */
    scope = (IsDlgButtonChecked(handle_, IDC_RB_CURFILE) == BST_CHECKED
             ? ITFDO_SCOPE_FILE
             : IsDlgButtonChecked(handle_, IDC_RB_ALLFILES) == BST_CHECKED
             ? ITFDO_SCOPE_GLOBAL
             : ITFDO_SCOPE_LOCAL);

    /* note changes */
    changed |= (new_re != regex
                || new_word != whole_word);


    /* save the new settings */
    regex = new_re;
    whole_word = new_word;

    /* return the change indication */
    return changed;
}

/*
 *   Run the dialog
 */
HWND CTadsDialogFindRegex::run_find_modeless(
    HWND parent_hdl, int dlg_id,
    textchar_t *findbuf, size_t findbuflen)
{
    /* remember the buffer */
    buf_ = findbuf;
    buflen_ = findbuflen;

    /* if I'm already open, just bring my window to the top */
    if (handle_ != 0)
    {
        BringWindowToTop(handle_);
        return handle_;
    }

    /* remember the dialog ID we used to display the dialog */
    dlg_id_ = dlg_id;

    /* opening the dialog explicitly starts a new search */
    if (handler_ != 0)
        handler_->itfdo_new_search();

    /* open the dialog modelessly */
    return open_modeless(dlg_id, parent_hdl,
                         CTadsApp::get_app()->get_instance());
}

/*
 *   handle a dialog message
 */
BOOL CTadsDialogFindRegex::do_dialog_msg(HWND dlg_hwnd, UINT message,
                                         WPARAM wpar, LPARAM lpar)
{
    switch (message)
    {
    case WM_ACTIVATE:
        /* on activating, tell my owner about it */
        if (wpar != WA_INACTIVE && handler_ != 0)
            handler_->itfdo_activate();

        /* adjust our buttons for the current context */
        adjust_context_buttons();

        /* continue to the default handling */
        break;

    case WM_DESTROY:
        /* before destroying the window, save the position */
        if (handler_ != 0)
        {
            RECT rc;
            CHtmlRect hrc;

            /* get the position, and convert to our rectangle type */
            GetWindowRect(handle_, &rc);
            hrc.set(rc.left, rc.top, rc.right, rc.bottom);

            /* save the position */
            handler_->itfdo_save_pos(&hrc);
        }

        /* continue to the default handling */
        break;
    }

    /* inherit the default handling */
    return CTadsDialogFind::do_dialog_msg(dlg_hwnd, message, wpar, lpar);
}

/*
 *   adjust our buttons for the current context
 */
void CTadsDialogFindRegex::adjust_context_buttons()
{
    /*
     *   if we're open, and we have a handler, ask it about the current
     *   context and adjust our buttons accordingly
     */
    if (handler_ != 0 && handler_ != 0)
    {
        /* check to see if Find (of any kind) is allowed here */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_FINDNEXT),
                     handler_->itfdo_allow_find(FALSE)
                     || handler_->itfdo_allow_find(TRUE));

        /* check to see if Replace is allowed */
        int repl = handler_->itfdo_allow_replace();
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REPLACE), repl);
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REPLACEALL), repl);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Find-and-Replace extension
 */

/* destroy */
CTadsDialogFindReplace::~CTadsDialogFindReplace()
{
    int i;

    /* delete our saved replacement strings */
    for (i = 0 ; i < sizeof(past_repl_)/sizeof(past_repl_[0]) ; ++i)
    {
        if (past_repl_[i] != 0)
            th_free(past_repl_[i]);
    }
}

/* initialize */
void CTadsDialogFindReplace::init()
{
    /* inherit the default handling */
    CTadsDialogFindRegex::init();

    /* we haven't found an occurrence yet */
    found_ = FALSE;

    /*
     *   save the replacement text in the history list - this ensures that
     *   scrolling into the combo box won't discard the initial text
     */
    save_repl_text();

    /* load up the combo box with the old replacement strings */
    HWND combo = GetDlgItem(handle_, IDC_CBO_REPLACE);
    for (int i = 0 ; i < sizeof(past_repl_)/sizeof(past_repl_[0]) ; ++i)
    {
        /* if we have a string in this slot, add it */
        if (past_repl_[i] != 0)
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)past_repl_[i]);
    }

    /* set the initial replacement text */
    if (rbuf_ != 0)
        SetDlgItemText(handle_, IDC_CBO_REPLACE, rbuf_);
}

/* handle a command */
int CTadsDialogFindReplace::do_command(WPARAM cmd, HWND ctl)
{
    int cnt;
    char msgbuf[256];

    switch (LOWORD(cmd))
    {
    case IDC_BTN_FINDNEXT:
        /*
         *   if there's no replace handler, use the default handling for the
         *   standard Find dialog
         */
        if (handler_ == 0)
            break;

        /* save the control settings, noting if it's a new search */
        if (read_control_settings() && handler_ != 0)
            handler_->itfdo_new_search();

        /* update the search history list */
        save_find_text();
        save_repl_text();

        /*
         *   if this is a regular expression Find, and plain searches but NOT
         *   regular expressions are allowed in this window, display an error
         */
        if (regex
            && handler_->itfdo_allow_find(FALSE)
            && !handler_->itfdo_allow_find(TRUE))
        {
            MessageBox(handle_, "This type of window doesn't support "
                       "Regular Expression searches.", "TADS",
                       MB_OK | MB_ICONINFORMATION);
            return TRUE;
        }

        /* if the Find isn't allowed right now, do nothing */
        if (!handler_->itfdo_allow_find(regex))
            return TRUE;

        /* find the next text */
        found_ = handler_->itfdo_find_text(
            buf_, exact_case, wrap_around, direction, regex, whole_word,
            scope);

    on_find_next:
        /* if we didn't find it, say so */
        if (!found_)
        {
            /* mention that there are no matches */
            MessageBox(handle_, "No more matches found.", "TADS",
                       MB_OK | MB_ICONINFORMATION);
        }

        /* enable Replace if we found something or there's text to find */
        EnableWindow(GetDlgItem(handle_, IDC_BTN_REPLACE),
                     found_ || buf_[0] != '\0');

        /* handled - this is not a dismiss button in this dialog */
        return TRUE;

    case IDC_BTN_REPLACE:
        /* make sure the control settings are up to date */
        if (read_control_settings() && handler_ != 0)
            handler_->itfdo_new_search();

        /* update the history lists */
        save_find_text();
        save_repl_text();

        /* replace the selection and find the next match */
        found_ = handler_->itfdo_replace_sel(
            buf_, rbuf_, exact_case, wrap_around, direction,
            regex, whole_word, scope);

        /* continue to the next occurrence */
        goto on_find_next;

    case IDC_BTN_REPLACEALL:
        /* update the control settings */
        if (read_control_settings() && handler_ != 0)
            handler_->itfdo_new_search();

        /* update the history lists */
        save_find_text();
        save_repl_text();

        /* run the replace-all operation */
        cnt = handler_->itfdo_replace_all(
            buf_, rbuf_, exact_case, wrap_around, direction,
            regex, whole_word, scope);

        /* indicate the number of replacements we made */
        if (cnt != 0)
            sprintf(msgbuf, "%d occurrence%s replaced.",
                    cnt, cnt == 1 ? "" : "s");
        else
            strcpy(msgbuf, "The specified text was not found.");
        MessageBox(handle_, msgbuf, "TADS", MB_OK | MB_ICONINFORMATION);

        /* this dismisses the dialog if we're running modally */
        if (is_modal_)
            EndDialog(handle_, IDC_BTN_REPLACEALL);
        return TRUE;

    case IDC_BTN_CLOSE:
        /* save settings */
        read_control_settings();
        save_find_text();
        save_repl_text();

        /* inherit the default behavior to close the dialog */
        break;
    }

    /* inherit the default handling */
    return CTadsDialogFindRegex::do_command(cmd, ctl);
}

/*
 *   Run the dialog
 */
HWND CTadsDialogFindReplace::run_replace_modeless(
    HWND parent_hdl, int dlg_id,
    textchar_t *findbuf, size_t findbuflen,
    textchar_t *replbuf, size_t replbuflen)
{
    /* remember the handler and replacement buffer */
    rbuf_ = replbuf;
    rbuflen_ = replbuflen;

    /* run the find dialog and return the window handle */
    return run_find_modeless(parent_hdl, dlg_id, findbuf, findbuflen);
}

/*
 *   Read the control settings
 */
int CTadsDialogFindReplace::read_control_settings()
{
    CStringBuf oldrbuf(rbuf_ != 0 ? rbuf_ : "");

    /* do the normal work */
    int changed = CTadsDialogFindRegex::read_control_settings();

    /* get the replacement text */
    if (rbuf_ != 0)
        GetDlgItemText(handle_, IDC_CBO_REPLACE, rbuf_, rbuflen_);

    /* check for changes to the text */
    if (rbuf_ != 0)
        changed |= (strcmp(oldrbuf.get(), rbuf_) != 0);

    /* return the change indication */
    return changed;
}

/*
 *   save the current replacement text in the history list
 */
void CTadsDialogFindReplace::save_repl_text()
{
    size_t cnt;
    size_t i;

    /* if there's no replacement text buffer, skip this */
    if (rbuf_ == 0)
        return;

    /* get the maximum number of strings to save */
    cnt = sizeof(past_repl_)/sizeof(past_repl_[0]);

    /*
     *   Scan for an existing copy of the current string.  If it's already in
     *   the list, move it to the head of the list (i.e., remove it from its
     *   current position and reinsert it at the first position).
     */
    for (i = 0 ; i < cnt && past_repl_[i] != 0 ; ++i)
    {
        /* if this one matches, stop looking */
        if (get_stricmp(past_repl_[i], rbuf_) == 0)
            break;
    }

    /* if we found a match, simply move it to the first position */
    if (i != cnt && past_repl_[i] != 0)
    {
        int j;
        textchar_t *str;

        /* remember the string we're moving for a moment */
        str = past_repl_[i];

        /*
         *   close the gap by moving all of the strings from the first
         *   position to the one before the one we're removing forward one
         *   position in the list
         */
        for (j = i ; j != 0 ; --j)
            past_repl_[j] = past_repl_[j-1];

        /* put this item back in as the first string */
        past_repl_[0] = str;
    }
    else
    {
        /*
         *   this search string is new, so we'll insert it; if we're pushing
         *   one off the end, free its memory
         */
        if (past_repl_[cnt-1] != 0)
            th_free(past_repl_[cnt-1]);

        /* move all of the strings down a slot */
        for (i = cnt - 1 ; i != 0 ; --i)
            past_repl_[i] = past_repl_[i-1];

        /* allocate space for the new string */
        past_repl_[0] = (textchar_t *)th_malloc(
            (get_strlen(rbuf_) + 1) * sizeof(textchar_t));

        /* copy it */
        do_strcpy(past_repl_[0], rbuf_);
    }
}

/*
 *   set an old replacement string
 */
void CTadsDialogFindReplace::set_old_repl_string(
    int idx, const char *str, size_t len)
{
    /* make sure it's in range */
    if (idx < 0 || idx >= sizeof(past_repl_)/sizeof(past_repl_[0]))
        return;

    /* if it's already allocated, free the old one */
    if (past_repl_[idx] != 0)
        th_free(past_repl_[idx]);

    /* allocate space to save a copy of the string */
    past_repl_[idx] = (textchar_t *)
                      th_malloc((len + 1) * sizeof(textchar_t));

    /* make a copy of the string in our allocated buffer */
    memcpy(past_repl_[idx], str, len*sizeof(textchar_t));
    past_repl_[idx][len] = '\0';
}

