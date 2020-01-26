/* $Header$ */

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32script.h - Script window
Function
  Defines the Script tool window
Notes

Modified
  12/06/06 MJRoberts  - Creation
*/

#ifndef W32SCRIPT_H
#define W32SCRIPT_H

#include "tadshtml.h"
#include "tadswin.h"
#include "tadsapp.h"
#include "w32tdb.h"


class CHtmlDbgScriptWin: public CTadsWin, public CHtmlDbgSubWin
{
public:
    CHtmlDbgScriptWin(class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlDbgScriptWin();

    /* get the number of scripts listed */
    int get_script_count();

    /* add a script file to our list */
    void add_script(const textchar_t *fname, int sort = TRUE);

    /* remove a script file from our list */
    void remove_script(const textchar_t *fname);

    /* run the newest script in our list */
    void run_latest_script();

    /* clear the list */
    void clear_list();

    /* prune the auto-scripts to the given maximum number */
    void prune_auto(const textchar_t *dir, int cnt);

    /* clean the list: remove items that refer to non-existent files */
    void clean_list();

    /* re-sync the list with the current directory contents */
    void sync_list(const textchar_t *dir);

    /* scan a directory; returns the number of files added */
    int scan_dir(const textchar_t *dir, int sort = TRUE);

    /* activate or deactivate */
    int do_ncactivate(int flag);

    /* parent activation */
    void do_parent_activate(int active);

    /* process system window creation */
    void do_create();

    /* destroy the system window */
    void do_destroy();

    /* close */
    int do_close();

    /* resize */
    void do_resize(int mode, int x, int y);

    /* process a notification message */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* save my window configuration */
    void save_win_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* load my window configuration */
    void load_win_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname,
                         RECT *pos, int *vis);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(const check_cmd_info *);

    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlDbgSubWin implementation
     */

    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(const check_cmd_info *);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id, HWND ctl);

    /* get my window handle */
    virtual HWND active_get_handle() { return handle_; }

    /* get the selection range - we don't have one */
    virtual int get_sel_range(class CHtmlFormatter **formatter,
                              unsigned long *sel_start,
                              unsigned long *sel_end)
        { return FALSE; }

    virtual int get_sel_text(CStringBuf *buf, unsigned int flags,
                             size_t maxlen)
        { return FALSE; }

    /* set the selection range - we don't have one */
    virtual int set_sel_range(unsigned long start, unsigned long end)
        { return FALSE; }

    /* bring the window to the front */
    virtual void active_to_front() { BringWindowToTop(GetParent(handle_)); }

protected:
    /* we're a child inside a docking or MDI frame */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }

    /* generate the title for an item based on its filename */
    void gen_item_title(textchar_t *title, size_t title_size,
                        const textchar_t *fname);

    /* does the given filename fit the "Auto" script name pattern? */
    int is_auto_script_name(const textchar_t *fname);

    /* handle a right-click on an item */
    void right_click_item(int item_idx);

    /* carry out a command on an item */
    void do_item_command(int command_id, int item_idx);

    /*
     *   find an item by filename: returns the index of the item, or -1 if
     *   not present in the list
     */
    int find_file_item(const textchar_t *fname);

    /* re-sort the list */
    void sort_list();

    /* debugger main window */
    class CHtmlSys_dbgmain *dbgmain_;

    /* the list view that occupies most of our window */
    HWND lv_;

    /* saved accelerator settings for label editing */
    app_accel_info edit_accel_save_;
};


#endif /* W32SCRIPT_H */
