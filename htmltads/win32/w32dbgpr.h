/* $Header: d:/cvsroot/tads/html/win32/w32dbgpr.h,v 1.3 1999/07/11 00:46:50 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32dbgpr.h - debugger preferences dialog
Function

Notes

Modified
  03/14/98 MJRoberts  - Creation
*/

#ifndef W32DBGPR_H
#define W32DBGPR_H

#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif


/*
 *   Run the debugger preferences dialog
 */
void run_dbg_prefs_dlg(HWND owner, class CHtmlSys_dbgmain *mainwin,
                       int init_page_id = 0);

/*
 *   Run the build configuration dialog
 */
void run_dbg_build_dlg(HWND owner, class CHtmlSys_dbgmain *mainwin,
                       int init_page_id, int init_ctl_id);

/*
 *   Set default debugger values for a configuration
 */
void set_dbg_defaults(class CHtmlSys_dbgmain *mainwin);

/*
 *   Set default build values for a configuration
 */
void set_dbg_build_defaults(class CHtmlSys_dbgmain *mainwin,
                            const textchar_t *gamefile);

/* private - set version-specific build defaults */
void vsn_set_dbg_build_defaults(class CHtmlDebugConfig *config,
                                class CHtmlSys_dbgmain *mainwin,
                                const textchar_t *gamefile);

/*
 *   Clear all build information from a configuration
 */
void clear_dbg_build_info(class CHtmlDebugConfig *config);

/*
 *   Run the new game wizard
 */
void run_new_game_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin);

/*
 *   Run the load-game-from-source wizard
 */
void run_load_src_wiz(HWND owner, class CHtmlSys_dbgmain *mainwin,
                      const textchar_t *srcfile);


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard - main wizard control object
 */

class CHtmlNewWiz
{
public:
    CHtmlNewWiz()
    {
        /* clear the flags */
        inited_ = FALSE;
        finish_ = FALSE;
        tdc_exists_ = FALSE;
        ui_type_ = W32TDB_UI_TRAD;

        /* presume we'll create an introductory (not advanced) source file */
        tpl_type_ = W32TDB_TPL_INTRO;
    }

    /* name of the source file */
    CStringBuf srcfile_;

    /* name of the game file */
    CStringBuf gamefile_;

    /* name of the custom template file, if specified */
    CStringBuf custom_;

    /* type of template file to create */
    w32tdb_tpl_t tpl_type_;

    /* UI mode */
    w32tdb_ui_t ui_type_;

    /* flag: parent dialog position has been initialized */
    int inited_;

    /* flag: user clicked the "finish" button */
    int finish_;

    /* flag: the project (.tdc/.t3c) file exists */
    int tdc_exists_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Base class for new game wizard dialogs
 */
class CHtmlNewWizPage: public CTadsDialogPropPage
{
public:
    CHtmlNewWizPage(class CHtmlNewWiz *wiz)
    {
        /* remember the main wizard object */
        wiz_ = wiz;
    }

    /* handle a notification */
    int do_notify(NMHDR *nm, int ctl);

    /* initialize */
    void init();

    /*
     *   Browse for a file to save.  Fills in the string buffer with the new
     *   filename and returns true if successful; returns false if the user
     *   cancels the dialog without selecting anything.
     */
    int browse_file(const char *filter, CStringBuf *filename,
                    const char *prompt, const char *defext, int fld_id,
                    DWORD flags);

    /* main wizard object */
    CHtmlNewWiz *wiz_;
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard dialog - game file
 */
class CHtmlNewWizGame: public CHtmlNewWizPage
{
public:
    CHtmlNewWizGame(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
    int do_command(WPARAM cmd, HWND ctl);
};


/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - welcome
 */

class CHtmlNewWizWelcome: public CHtmlNewWizPage
{
public:
    CHtmlNewWizWelcome(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
};

/* ------------------------------------------------------------------------ */
/*
 *   New Game Wizard Dialog - success
 */
class CHtmlNewWizSuccess: public CHtmlNewWizPage
{
public:
    CHtmlNewWizSuccess(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    int do_notify(NMHDR *nm, int ctl);
};

/* ------------------------------------------------------------------------ */
/*
 *   Name/Value pair.  CNameValTab uses these to build its list.
 */
class CNameValPair
{
public:
    CNameValPair(const char *key, const char *val)
        : key_(key), val_(val)
    {
        nxt_ = 0;
    }
    CNameValPair(const char *key, size_t keylen,
                 const char *val, size_t vallen)
        : key_(key, keylen), val_(val, vallen)
    {
        nxt_ = 0;
    }

    CStringBuf key_;
    CStringBuf val_;

    CNameValPair *nxt_;
};

class CNameValTab
{
public:
    CNameValTab()
    {
        key_head_ = key_tail_ = 0;
    }

    ~CNameValTab()
    {
        /* clear our list */
        clear();
    }

    /* clear the list */
    void clear()
    {
        /* delete the key list */
        while (key_head_ != 0)
        {
            CNameValPair *nxt = key_head_->nxt_;
            delete key_head_;
            key_head_ = nxt;
        }
        key_tail_ = 0;
    }

    /* add a key */
    void add_key(const char *key, const char *val)
    {
        add_key(key, strlen(key), val, strlen(val));
    }
    void add_key(const char *key, size_t keylen,
                 const char *val, size_t vallen)
    {
        /* create the new element */
        CNameValPair *ele = new CNameValPair(key, keylen, val, vallen);

        /* link it into our list */
        if (key_tail_ != 0)
            key_tail_->nxt_ = ele;
        else
            key_head_ = ele;
        key_tail_ = ele;
    }

    /* find a value by key */
    const CNameValPair *find_key(const char *buf, size_t len) const
    {
        /* scan our pair list for a match */
        for (CNameValPair *cur = key_head_ ; cur != 0 ; cur = cur->nxt_)
        {
            /* check for a match */
            if (cur->key_.getlen() == len
                && memicmp(buf, cur->key_.get(), len) == 0)
                return cur;
        }

        /* didn't find it */
        return 0;
    }
    const CNameValPair *find_key(const char *buf) const
    {
        return find_key(buf, strlen(buf));
    }

    /* find the next value with the same key */
    const CNameValPair *find_next_key(const CNameValPair *p) const
    {
        /* scan for another match to the same name */
        for (const CNameValPair *cur = p->nxt_ ; cur != 0 ; cur = cur->nxt_)
        {
            if (stricmp(p->key_.get(), cur->key_.get()) == 0)
                return cur;
        }

        /* didn't find it */
        return 0;
    }

private:
    /* our linked list of key/value pairs */
    CNameValPair *key_head_, *key_tail_;
};

/*
 *   New Game Wizard Dialog - bibliograhpic settings
 */
class CHtmlNewWizBiblio: public CHtmlNewWizPage
{
public:
    CHtmlNewWizBiblio(CHtmlNewWiz *wiz) : CHtmlNewWizPage(wiz) { }

    /* initialize */
    void init();

    /* handle notifications */
    int do_notify(NMHDR *nm, int ctl);

    /* get my bibliographic data list object */
    const CNameValTab *get_biblio() const { return &biblio_; }

private:
    /* our bibliographic data list object */
    CNameValTab biblio_;
};


#endif /* W32DBGPR_H */

