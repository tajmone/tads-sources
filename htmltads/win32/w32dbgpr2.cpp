/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32dbgpr2.cpp - win32 workbench - TADS 2-specified dialogs
Function
  Dialogs specific to TADS 2 Workbench
Notes

Modified
  05/01/13 MJRoberts  - moved the new dilaog wizard here, renamed from
                        w32ldsrc.cpp to reflect that this is now the general
                        repository of Workbench dialogs specific to TADS 2,
                        not just the "load source wizard"
  03/01/03 MJRoberts  - creation (split off from w32dbgpr.cpp; we split this
                        up into a separate file because we don't need this
                        functionality in the TADS 3 Workbench, so we want
                        this stuff separate so we can link the rest of the
                        preferences dialogs, which are common to T2 and T3,
                        without having to include this code)
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
#ifndef W32MAIN_H
#include "w32main.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Load Source Wizard Dialog - game file
 */

class CHtmlLdSrcWizGame: public CHtmlNewWizGame
{
public:
    CHtmlLdSrcWizGame(CHtmlNewWiz *wiz) : CHtmlNewWizGame(wiz) { }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch(cmd)
        {
        case IDC_BTN_BROWSE:
            /* browse for a game or configuration file */
            browse_file(w32_config_opendlg_filter,
                        &wiz_->gamefile_,
                        "Choose a name for your new compiled game file",
                        w32_game_ext, IDC_FLD_NEWGAMEFILE,
                        OFN_PATHMUSTEXIST | OFN_HIDEREADONLY);
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizGame::do_command(cmd, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Load Source Game Wizard Dialog - success
 */

class CHtmlLdSrcWizSuccess: public CHtmlNewWizSuccess
{
public:
    CHtmlLdSrcWizSuccess(CHtmlNewWiz *wiz) : CHtmlNewWizSuccess(wiz) { }

    int do_notify(NMHDR *nm, int ctl)
    {
        textchar_t fname[OSFNMAX];

        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /*
             *   Check for the existence of the project (.tdc/.t3c) file.  If
             *   a project file exists, show the message indicating that
             *   we'll simply be loading the existing configuration;
             *   otherwise, show the message that we'll be creating a new
             *   configuration.
             */
            strcpy(fname, wiz_->gamefile_.get());
            os_remext(fname);
            os_addext(fname, w32_tdb_config_ext);
            if (osfacc(fname))
            {
                /* the project (.tdc/.t3c) doesn't exist - we'll create it */
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_NEW), SW_SHOW);
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_OLD), SW_HIDE);
                wiz_->tdc_exists_ = FALSE;
            }
            else
            {
                /* the project (.tdc/.t3c) already exists - we'll load it */
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_NEW), SW_HIDE);
                ShowWindow(GetDlgItem(handle_, IDC_TXT_CONFIG_OLD), SW_SHOW);
                wiz_->tdc_exists_ = TRUE;
            }

            /* inherit the default as well */
            break;

        default:
            /* go inherit the default */
            break;
        }

        /* inherit default handling */
        return CHtmlNewWizSuccess::do_notify(nm, ctl);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Run the load-game-from-source wizard
 */
void run_load_src_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin,
                      const textchar_t *srcfile)
{
    PROPSHEETPAGE psp[10];
    PROPSHEETHEADER psh;
    CHtmlNewWizWelcome *welcome;
    CHtmlLdSrcWizGame *game;
    CHtmlLdSrcWizSuccess *success;
    int i;
    int inited = FALSE;
    void *ctx;
    CHtmlNewWiz wiz;
    textchar_t fname[OSFNMAX];

    /* create our pages */
    welcome = new CHtmlNewWizWelcome(&wiz);
    game = new CHtmlLdSrcWizGame(&wiz);
    success = new CHtmlLdSrcWizSuccess(&wiz);

    /* set the source file in the wizard */
    wiz.srcfile_.set(srcfile);

    /* build the default game file name */
    strcpy(fname, srcfile);
    os_remext(fname);
    os_addext(fname, w32_game_ext);
    wiz.gamefile_.set(fname);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_WIZARD;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Build Settings";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptors */
    i = 0;
    welcome->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_LDSRCWIZ_WELCOME,
                          IDS_LDSRCWIZ_WELCOME, 0, 0, &psh);
    game->init_ps_page(CTadsApp::get_app()->get_instance(),
                       &psp[i++], DLG_LDSRCWIZ_GAMEFILE,
                       IDS_LDSRCWIZ_GAMEFILE, 0, 0, &psh);
    success->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_LDSRCWIZ_SUCCESS,
                          IDS_LDSRCWIZ_SUCCESS, 0, 0, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* disable windows owned by the main window before showing the dialog */
    ctx = CTadsDialog::modal_dlg_pre(mainwin->get_handle(), FALSE);

    /* run the property sheet */
    PropertySheet(&psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);

    /* if they clicked "Finish", create the new game */
    if (wiz.finish_)
    {
        /*
         *   if the project (.tdc/.t3c) file for the game already exists,
         *   simply load it; otherwise, create the new game
         */
        if (wiz.tdc_exists_)
        {
            /* simply load the new project (.tdc/.t3c) file */
            mainwin->load_game(wiz.gamefile_.get());
        }
        else
        {
            /* ask the main window to do the work for us */
            mainwin->create_new_game(wiz.srcfile_.get(), wiz.gamefile_.get(),
                                     FALSE, W32TDB_TPL_INTRO, W32TDB_UI_TRAD,
                                     0, 0);
        }
    }

    /* delete the dialogs */
    delete welcome;
    delete game;
    delete success;
}



/* ------------------------------------------------------------------------ */
/*
 *   Base class for new game wizard dialogs - implementation
 */

int CHtmlNewWizPage::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* by default, turn on the 'back' and 'next' buttons */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK | PSWIZB_NEXT);
        return TRUE;

    default:
        return CTadsDialogPropPage::do_notify(nm, ctl);
    }
}

/* initialize */
void CHtmlNewWizPage::init()
{
    /* if we haven't yet done so, center the dialog */
    if (!wiz_->inited_)
    {
        /* center the parent dialog */
        center_dlg_win(GetParent(handle_));

        /* note that we've done this so that other pages don't */
        wiz_->inited_ = TRUE;
    }

    /* inherit default */
    CTadsDialog::init();
}

/*
 *   Browse for a file to save.  Fills in the string buffer with the new
 *   filename and returns true if successful; returns false if the user
 *   cancels the dialog without selecting anything.
 */
int CHtmlNewWizPage::browse_file(const char *filter, CStringBuf *filename,
                                 const char *prompt, const char *defext,
                                 int fld_id, DWORD flags)
{
    OPENFILENAME5 info;
    char fname[256];
    char dir[256];

    /* ask for a file to save */
    info.hwndOwner = handle_;
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrFilter = filter;
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    if (filename->get() != 0 && filename->get()[0] != '\0')
    {
        char *p;

        /* start off with the current filename */
        strcpy(dir, filename->get());

        /* get the root of the name */
        p = os_get_root_name(dir);

        /* copy the filename part */
        strcpy(fname, p);

        /* remove the file portion */
        if (p > dir && *(p-1) == '\\')
            *(p-1) = '\0';
        info.lpstrInitialDir = dir;
    }
    else
    {
        /* there's no filename yet */
        fname[0] = '\0';

        /* start off in the default open-file directory */
        info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    }
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrTitle = prompt;
    info.Flags = flags | OFN_NOCHANGEDIR| OFN_ENABLESIZING;
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lpstrDefExt = defext;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
    if (GetSaveFileName((OPENFILENAME *)&info))
    {
        /* save the new filename */
        filename->set(fname);

        /* copy the new filename into the editor field, if desired */
        if (fld_id != 0)
            SetWindowText(GetDlgItem(handle_, fld_id), fname);

        /* update the open-file directory */
        CTadsApp::get_app()->set_openfile_dir(fname);

        /* enable the 'next' button now that we have a file */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK | PSWIZB_NEXT);

        /* success */
        return TRUE;
    }
    else
    {
        /* they cancelled */
        return FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - welcome
 */

int CHtmlNewWizWelcome::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /* this is the first page - turn off the 'back' button */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_NEXT);
        return TRUE;

    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - source file
 */

class CHtmlNewWizSource: public CHtmlNewWizPage
{
public:
    CHtmlNewWizSource(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch(nm->code)
        {
        case PSN_SETACTIVE:
            /*
             *   turn on the 'back' button, and turn on the 'next' button
             *   only if a source file has been selected
             */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                        PSWIZB_BACK
                        | (wiz_->srcfile_.get() != 0
                           && wiz_->srcfile_.get()[0] != 0
                           ? PSWIZB_NEXT : 0));
            return TRUE;

        default:
            return CHtmlNewWizPage::do_notify(nm, ctl);
        }
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        CStringBuf new_fname;

        switch(cmd)
        {
        case IDC_BTN_BROWSE:
        browse_again:
            /* browse for a source file */
            if (browse_file("TADS Source Files\0*.t\0"
                            "All Files\0*.*\0\0", &new_fname,
                            "Choose a name for your new source file", "t",
                            0, OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT
                            | OFN_HIDEREADONLY))
            {
                char fname[OSFNMAX];
                char *ext;
                int ret;

                /* check the suffix */
                ext = strrchr(new_fname.get(), '.');
                if (ext != 0
                    && (stricmp(ext, ".t3m") == 0
                        || stricmp(ext, ".tdc") == 0
                        || stricmp(ext, ".gam") == 0
                        || stricmp(ext, ".t3") == 0
                        || stricmp(ext, ".tl") == 0
                        || stricmp(ext, ".t3o") == 0
                        || stricmp(ext, ".t3s") == 0))
                {
                    /* point out the problem */
                    MessageBox(GetParent(handle_),
                               "The filename you chose ends in a suffix that "
                               "is reserved for other purposes.  You can't "
                               "use this suffix for a source file.  Please "
                               "change the name - a suffix of '.t' is "
                               "recommended.", "TADS Workbench",
                               MB_ICONEXCLAMATION | MB_OK);

                    /* do not accept this filename */
                    ret = IDNO;
                }
                else if (ext != 0 && stricmp(ext, ".t") != 0)
                {
                    /* ask if they really want to use this extension */
                    ret = MessageBox(GetParent(handle_),
                                     "The filename you chose ends in a "
                                     "suffix that isn't usually used for "
                                     "TADS source files - a suffix of '.t' "
                                     "is recommended.  Are you sure you "
                                     "want to use this non-standard "
                                     "filename pattern?", "TADS Workbench",
                                     MB_ICONQUESTION | MB_YESNO);
                }
                else
                {
                    /* proceed */
                    ret = IDYES;
                }

                /* if they wanted to change the name, try again */
                if (ret != IDYES)
                    goto browse_again;

                /* accept the new filename */
                wiz_->srcfile_.set(new_fname.get());
                SetWindowText(GetDlgItem(handle_, IDC_FLD_NEWSOURCE),
                              new_fname.get());

                /*
                 *   We might want to update the game file name to match
                 *   the new source file name.  If the game file hasn't
                 *   been set yet, change it without asking; otherwise,
                 *   ask the user what they want to do.
                 */
                strcpy(fname, wiz_->srcfile_.get());
                os_remext(fname);
                os_addext(fname, w32_game_ext);
                if (wiz_->gamefile_.get() == 0
                    || wiz_->gamefile_.get()[0] == '\0')
                {
                    /* there's no name defined; set it without asking */
                    wiz_->gamefile_.set(fname);
                }
                else
                {
                    int ret;

                    /* ask */
                    ret = MessageBox(GetParent(handle_),
                                     "Do you want to change the compiled "
                                     "game file name to match this new "
                                     "source file name?",
                                     "TADS Workbench",
                                     MB_ICONQUESTION | MB_YESNO);
                    if (ret == IDYES || ret == 0)
                        wiz_->gamefile_.set(fname);
                }
            }

            /* handled */
            return TRUE;

        default:
            /* proceed to the default handling */
            break;
        }

        /* inherit default */
        return CHtmlNewWizPage::do_command(cmd, ctl);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - game file
 */

int CHtmlNewWizGame::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /*
         *   the game file might have changed externally, so load it into our
         *   text field
         */
        if (wiz_->gamefile_.get() != 0
            && wiz_->gamefile_.get()[0] != '\0')
            SetWindowText(GetDlgItem(handle_, IDC_FLD_NEWGAMEFILE),
                          wiz_->gamefile_.get());

        /*
         *   turn on the 'back' button, and turn on the 'next' button only if
         *   a game file has been selected
         */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_BACK
                    | (wiz_->gamefile_.get() != 0
                       && wiz_->gamefile_.get()[0] != 0
                       ? PSWIZB_NEXT : 0));
        return TRUE;

    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

int CHtmlNewWizGame::do_command(WPARAM cmd, HWND ctl)
{
    switch(cmd)
    {
    case IDC_BTN_BROWSE:
        /* browse for a game file */
        browse_file(w32_opendlg_filter,
                    &wiz_->gamefile_,
                    "Choose a name for your new compiled game file",
                    w32_game_ext, IDC_FLD_NEWGAMEFILE,
                    OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT
                    | OFN_HIDEREADONLY);
        return TRUE;

    default:
        /* inherit default */
        return CHtmlNewWizPage::do_command(cmd, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - source type
 */

class CHtmlNewWizType: public CHtmlNewWizPage
{
public:
    CHtmlNewWizType(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    void init()
    {
        /* set the "introductory" button by default */
        CheckRadioButton(handle_, IDC_RB_INTRO, IDC_RB_CUSTOM, IDC_RB_INTRO);

        /* inherit default*/
        CHtmlNewWizPage::init();
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        textchar_t buf[OSFNMAX + 20];

        switch(cmd)
        {
        case IDC_RB_INTRO:
            /* set "intro" mode */
            wiz_->tpl_type_ = W32TDB_TPL_INTRO;

        enable_next:
            /* we can go on now */
            SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS,
                        0, PSWIZB_BACK | PSWIZB_NEXT);

            /* handled */
            return TRUE;

        case IDC_RB_ADV:
            /* turn on "advanced" mode */
            wiz_->tpl_type_ = W32TDB_TPL_ADVANCED;
            goto enable_next;

        case IDC_RB_PLAIN:
            /* turn on "plain" mode */
            wiz_->tpl_type_ = W32TDB_TPL_NOLIB;
            goto enable_next;

        case IDC_RB_CUSTOM:
            /* turn on "custom" mode */
            wiz_->tpl_type_ = W32TDB_TPL_CUSTOM;

            /*
             *   if they haven't selected a custom template file yet, run
             *   the "browse" dialog to do so
             */
            if (wiz_->custom_.get() == 0 || wiz_->custom_.get()[0] == '\0')
            {
                /*
                 *   turn off the "next" button, since we can't go on
                 *   until a file is selected - we'll turn it back on if
                 *   they select a file or switch to one of the standard
                 *   template options
                 */
                SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS,
                            0, PSWIZB_BACK);

                /* run the "browse" dialog */
                PostMessage(handle_, WM_COMMAND, IDC_BTN_BROWSE, 0);
            }

            /* handled */
            return TRUE;

        case IDC_BTN_BROWSE:
            /* browse for a custom template file */
            if (browse_file("TADS Source Files\0*.t\0"
                            "TADS Library Files\0*.tl\0"
                            "All Files\0*.*\0\0",
                            &wiz_->custom_,
                            "Choose a custom template file",
                            0, 0,
                            OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST
                            | OFN_HIDEREADONLY))
            {
                /* update the radio button string */
                sprintf(buf, "&Custom: %s", wiz_->custom_.get());
                SetWindowText(GetDlgItem(handle_, IDC_RB_CUSTOM), buf);

                /*
                 *   if we haven't already selected the "custom" radio
                 *   button, select it
                 */
                if (wiz_->tpl_type_ != W32TDB_TPL_CUSTOM)
                    CheckRadioButton(handle_, IDC_RB_INTRO, IDC_RB_CUSTOM,
                                     IDC_RB_CUSTOM);
            }

            /* handled */
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizPage::do_command(cmd, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - user interface type for Adv3 games.  This page
 *   is only shown if an Adv3-based template is chosen.
 */

class CHtmlNewWizUI: public CHtmlNewWizPage
{
public:
    CHtmlNewWizUI(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    void init()
    {
        /* set the "traditional" button by default */
        CheckRadioButton(handle_, IDC_RB_TRADUI, IDC_RB_WEBUI, IDC_RB_TRADUI);

        /* inherit default */
        CHtmlNewWizPage::init();
    }

    int do_notify(NMHDR *nm, int ctl)
    {
        switch (nm->code)
        {
        case PSN_SETACTIVE:
            /* only show the UI mode page if we're using Adv3 */
            switch (wiz_->tpl_type_)
            {
            case W32TDB_TPL_INTRO:
            case W32TDB_TPL_ADVANCED:
                /* run the page as normal */
                break;

            default:
                /* other types don't use Adv3, so skip the UI selection */
                SetWindowLong(handle_, DWL_MSGRESULT, -1);
                return TRUE;
            }
            break;
        }

        /* inherit the default handling */
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }

    int do_command(WPARAM cmd, HWND ctl)
    {
        switch(cmd)
        {
        case IDC_RB_TRADUI:
            /* set "traditional ui" mode */
            wiz_->ui_type_ = W32TDB_UI_TRAD;
            return TRUE;

        case IDC_RB_WEBUI:
            /* set "web ui" mode */
            wiz_->ui_type_ = W32TDB_UI_WEB;
            return TRUE;

        default:
            /* inherit default */
            return CHtmlNewWizPage::do_command(cmd, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - bibliographic data
 */

/* initialize */
void CHtmlNewWizBiblio::init()
{
    /* set up the initial values for the fields */
    SetDlgItemText(handle_, IDC_BIB_TITLE, "Your New Game Title");
    SetDlgItemText(handle_, IDC_BIB_AUTHOR, "Your Name");
    SetDlgItemText(handle_, IDC_BIB_EMAIL, "your-email@host.com");
    SetDlgItemText(handle_, IDC_BIB_DESC,
                   "Put a brief \"blurb\" about your game here.");

    /* load the card catalog bitmap */
    HANDLE hbmp = (HBITMAP)LoadImage(
        CTadsApp::get_app()->get_instance(),
        MAKEINTRESOURCE(IDB_CARDCATALOG),
        IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
    SendMessage(GetDlgItem(handle_, IDB_CARDCATALOG), BM_SETIMAGE,
                IMAGE_BITMAP, (LPARAM)hbmp);

    /* inherit default */
    CHtmlNewWizPage::init();
}

/* notifications */
int CHtmlNewWizBiblio::do_notify(NMHDR *nm, int ctl)
{
    char buf[512];
    char buf2[512];
    char *src, *dst;

    switch(nm->code)
    {
    case PSN_WIZNEXT:
        /* clear our old settings */
        biblio_.clear();

        /* store our updated settings */
        GetDlgItemText(handle_, IDC_BIB_TITLE, buf, sizeof(buf));
        biblio_.add_key("$TITLE$", buf);

        GetDlgItemText(handle_, IDC_BIB_AUTHOR, buf, sizeof(buf));
        biblio_.add_key("$AUTHOR$", buf);

        GetDlgItemText(handle_, IDC_BIB_EMAIL, buf, sizeof(buf));
        biblio_.add_key("$EMAIL$", buf);

        GetDlgItemText(handle_, IDC_BIB_DESC, buf, sizeof(buf));
        biblio_.add_key("$DESC$", buf);

        /* htmlify it, and add it as $HTMLDESC$ */
        for (src = buf, dst = buf2 ;
             *src != '\0' && dst < buf2 + sizeof(buf2) - 1 ; )
        {
            const char *markup;
            char c = *src++;

            /* translate markup-significant characters */
            switch (c)
            {
            case '<':
                markup = "&lt;";
                goto add_markup;

            case '>':
                markup = "&gt;";
                goto add_markup;

            case '&':
                markup = "&amp;";

            add_markup:
                if (dst < buf2 + sizeof(buf2) - strlen(markup) - 1)
                {
                    strcpy(dst, markup);
                    dst += strlen(markup);
                }
                break;

            default:
                /* copy it unchanged */
                *dst++ = c;
                break;
            }
        }

        /* null-terminate it */
        *dst = 0;

        /* add it */
        biblio_.add_key("$HTMLDESC$", buf2);
    }

    /* inherit the default handling */
    return CHtmlNewWizPage::do_notify(nm, ctl);
}


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - success
 */

int CHtmlNewWizSuccess::do_notify(NMHDR *nm, int ctl)
{
    switch(nm->code)
    {
    case PSN_SETACTIVE:
        /*
         *   turn on the 'finish' button, and turn off the 'next' button,
         *   since there's nowhere to go from here
         */
        SendMessage(GetParent(handle_), PSM_SETWIZBUTTONS, 0,
                    PSWIZB_FINISH | PSWIZB_BACK);
        return TRUE;

    case PSN_WIZFINISH:
        /*
         *   the user has selected the "finish" button - set the successful
         *   completion flag
         */
        wiz_->finish_ = TRUE;

        /* handled */
        return TRUE;

    default:
        return CHtmlNewWizPage::do_notify(nm, ctl);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Run the New Game Wizard (old version)
 */
void run_new_game_wiz(HWND owner, CHtmlSys_dbgmain *mainwin)
{
    PROPSHEETPAGE psp[10];
    PROPSHEETHEADER psh;
    CHtmlNewWizWelcome *welcome;
    CHtmlNewWizSource *source;
    CHtmlNewWizGame *game;
    CHtmlNewWizType *typ;
    CHtmlNewWizBiblio *biblio;
    CHtmlNewWizSuccess *success;
    CHtmlNewWizUI *ui;
    int i;
    int inited = FALSE;
    CHtmlNewWiz wiz;

    /* create our pages */
    welcome = new CHtmlNewWizWelcome(&wiz);
    source = new CHtmlNewWizSource(&wiz);
    game = new CHtmlNewWizGame(&wiz);
    typ = new CHtmlNewWizType(&wiz);
    biblio = new CHtmlNewWizBiblio(&wiz);
    success = new CHtmlNewWizSuccess(&wiz);
    ui = new CHtmlNewWizUI(&wiz);

    /* set up the main dialog descriptor */
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_WIZARD;
    psh.hwndParent = owner;
    psh.hInstance = CTadsApp::get_app()->get_instance();
    psh.pszIcon = 0;
    psh.pszCaption = (LPSTR)"Build Settings";
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = 0;

    /* set up the property page descriptors */
    i = 0;
    welcome->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_NEWWIZ_WELCOME, IDS_NEWWIZ_WELCOME,
                          0, 0, &psh);
    source->init_ps_page(CTadsApp::get_app()->get_instance(),
                         &psp[i++], DLG_NEWWIZ_SOURCE, IDS_NEWWIZ_SOURCE,
                         0, 0, &psh);
    game->init_ps_page(CTadsApp::get_app()->get_instance(),
                       &psp[i++], DLG_NEWWIZ_GAMEFILE, IDS_NEWWIZ_GAMEFILE,
                       0, 0, &psh);
    typ->init_ps_page(CTadsApp::get_app()->get_instance(),
                      &psp[i++], DLG_NEWWIZ_TYPE, IDS_NEWWIZ_TYPE,
                      0, 0, &psh);
    ui->init_ps_page(CTadsApp::get_app()->get_instance(),
                     &psp[i++], DLG_NEWWIZ_UI, IDS_NEWWIZ_UI,
                     0, 0, &psh);
    biblio->init_ps_page(CTadsApp::get_app()->get_instance(),
                         &psp[i++], DLG_NEWWIZ_BIBLIO, IDS_NEWWIZ_BIBLIO,
                         0, 0, &psh);
    success->init_ps_page(CTadsApp::get_app()->get_instance(),
                          &psp[i++], DLG_NEWWIZ_SUCCESS, IDS_NEWWIZ_SUCCESS,
                          0, 0, &psh);

    /* set the number of pages */
    psh.nPages = i;

    /* run the property sheet */
    CTadsDialog::run_propsheet(mainwin->get_handle(), &psh);

    /* if they clicked "Finish", create the new game */
    if (wiz.finish_)
    {
        /* ask the main window to do the work for us */
        mainwin->create_new_game(wiz.srcfile_.get(), wiz.gamefile_.get(),
                                 TRUE, wiz.tpl_type_, wiz.ui_type_,
                                 wiz.tpl_type_ == W32TDB_TPL_CUSTOM
                                 ? wiz.custom_.get() : 0,
                                 biblio->get_biblio());
    }

    /* delete the dialogs */
    delete welcome;
    delete source;
    delete game;
    delete typ;
    delete ui;
    delete biblio;
    delete success;
}
