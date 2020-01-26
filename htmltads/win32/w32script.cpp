#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32script.cpp - script window implementation
Function

Notes

Modified
  12/06/06 MJRoberts  - Creation
*/

#include <Windows.h>
#include <commctrl.h>
#include <ctype.h>

#include "tadshtml.h"
#include "tadswin.h"
#include "w32tdb.h"
#include "w32script.h"
#include "w32tdb.h"
#include "htmldcfg.h"


/* ------------------------------------------------------------------------ */
/*
 *   List view item data structure.  We store one of these structures in the
 *   lparam of each list view item.
 */
struct lvdata
{
    /* the modification timestamp for this file */
    FILETIME modtime;

    /*
     *   filename (this differs from the title, because we remove the
     *   extension for display purposes)
     */
    CStringBuf fname;

    lvdata(const textchar_t *fname)
        : fname(fname)
    {
        /* assume we won't find any data */
        memset(&modtime, 0, sizeof(modtime));

        /* refresh my file data */
        read_file_data();
    }

    /* read the file time from the OS */
    void read_file_data()
    {
        WIN32_FIND_DATA info;
        HANDLE hf;

        /* look up the file's information */
        if ((hf = FindFirstFile(fname.get(), &info)) != INVALID_HANDLE_VALUE)
        {
            /* save the modification time */
            modtime = info.ftLastWriteTime;

            /* done with the 'find' handle */
            FindClose(hf);
        }
    }

    /* comparison callback for list view sorting */
    static int CALLBACK compare(LPARAM la, LPARAM lb, LPARAM lpar)
    {
        lvdata *a = (lvdata *)la;
        lvdata *b = (lvdata *)lb;

        /*
         *   compare based on the file timestamps; return the negative sense
         *   of the a-to-b comparison, since we want to sort in descending
         *   order (i.e., newest first)
         */
        return CompareFileTime(&b->modtime, &a->modtime);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   construction
 */
CHtmlDbgScriptWin::CHtmlDbgScriptWin(CHtmlSys_dbgmain *dbgmain)
{
    /* remember the main debugger window, and keep a reference */
    dbgmain_ = dbgmain;
    dbgmain->AddRef();
}

/*
 *   destruction
 */
CHtmlDbgScriptWin::~CHtmlDbgScriptWin()
{
}

/*
 *   process window creation
 */
void CHtmlDbgScriptWin::do_create()
{
    /* set up our list view to take up our whole window */
    RECT rc;
    GetClientRect(handle_, &rc);
    lv_ = CreateWindow(WC_LISTVIEW, "list",
                       WS_VISIBLE | WS_CHILD | LVS_LIST
                       | LVS_NOCOLUMNHEADER | LVS_EDITLABELS
                       | LVS_SINGLESEL,
                       0, 0, rc.right, rc.left, handle_, (HMENU)1,
                       CTadsApp::get_app()->get_instance(), 0);

    /* set up the image list */
    HIMAGELIST himl = ImageList_Create(16, 16, FALSE, 5, 0);
    ListView_SetImageList(lv_, himl, LVSIL_SMALL);

    /* load the script file icon */
    HBITMAP bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                             MAKEINTRESOURCE(IDB_PROJ_SCRIPT));
    ImageList_Add(himl, bmp, 0);

    /*
     *   register as a drop target simply to make it explicit that we don't
     *   accept drops
     */
    drop_target_register();
}

/*
 *   resize the window
 */
void CHtmlDbgScriptWin::do_resize(int mode, int x, int y)
{
    switch (mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing subwindows on these changes */
        break;

    default:
        /* resize our listview to use the whole window */
        MoveWindow(lv_, 0, 0, x, y, TRUE);
        break;
    }

    /* inherit default handling */
    CTadsWin::do_resize(mode, x, y);
}


/*
 *   process window deletion
 */
void CHtmlDbgScriptWin::do_destroy()
{
    /*
     *   explicitly delete all of our list items - we have to do this to get
     *   item deletion notifications, which we need to delete our lparam
     *   objects for the items
     */
    ListView_DeleteAllItems(lv_);

    /* tell the debugger to clear my window if I'm the active one */
    dbgmain_->on_close_dbg_win(this);

    /* release our reference on the main window */
    dbgmain_->Release();
    dbgmain_ = 0;

    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   close the window - never really close it; simply hide it instead
 */
int CHtmlDbgScriptWin::do_close()
{
    /* hide the window, rather than closing it */
    ShowWindow(handle_, SW_HIDE);

    /* tell the caller not to really close it */
    return FALSE;
}

/*
 *   Non-client activation/deactivation.  Notify the debugger main window of
 *   the coming or going of this debugger window as the active debugger
 *   window, which will allow us to receive certain command messages from the
 *   main window.
 */
int CHtmlDbgScriptWin::do_ncactivate(int flag)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0)
    {
        if (flag)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }

    /* inherit default handling */
    return CTadsWin::do_ncactivate(flag);
}

/*
 *   Receive notification of activation from parent
 */
void CHtmlDbgScriptWin::do_parent_activate(int active)
{
    /* register or deregister with the main window */
    if (dbgmain_ != 0)
    {
        if (active)
            dbgmain_->set_active_dbg_win(this);
        else
            dbgmain_->clear_active_dbg_win();
    }

    /* inherit default */
    CTadsWin::do_parent_activate(active);
}

/*
 *   check a command's status
 */
TadsCmdStat_t CHtmlDbgScriptWin::check_command(const check_cmd_info *info)
{
    TadsCmdStat_t stat;

    /* try handling it using the active-window mechanism */
    if ((stat = active_check_command(info)) != TADSCMD_UNKNOWN)
        return stat;

    /* let the debugger main window handle it */
    return dbgmain_->check_command(info);
}

/*
 *   process a command
 */
int CHtmlDbgScriptWin::do_command(int notify_code, int command_id, HWND ctl)
{
    /* try handling it using the active-window mechanism */
    if (active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* let the debugger main window handle it */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   check the status of a command routed from the main window
 */
TadsCmdStat_t CHtmlDbgScriptWin::active_check_command(
    const check_cmd_info *info)
{
    /* check the command */
    switch (info->command_id)
    {
    case ID_SCRIPT_OPEN:
    case ID_SCRIPT_REPLAY:
    case ID_SCRIPT_RENAME:
    case ID_SCRIPT_DELETE:
        /* these commands are valid as long as there's a selection */
        return (ListView_GetNextItem(lv_, -1, LVNI_ALL | LVNI_SELECTED) != -1
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);
    }

    /* inherit the base class handling */
    return CTadsWin::check_command(info);
}

/*
 *   execute a command routed from the main window
 */
int CHtmlDbgScriptWin::active_do_command(
    int notify_code, int command_id, HWND ctl)
{
    int idx;

    /* check for commands we recognize */
    switch (command_id)
    {
    case ID_SCRIPT_OPEN:
    case ID_SCRIPT_REPLAY:
    case ID_SCRIPT_RENAME:
    case ID_SCRIPT_DELETE:
        /* handle these commands on the selection */
        idx = ListView_GetNextItem(lv_, -1, LVNI_ALL | LVNI_SELECTED);
        if (idx != -1)
            do_item_command(command_id, idx);

        /* handled */
        return TRUE;
    }

    /* inherit the base class handling */
    return CTadsWin::do_command(notify_code, command_id, ctl);
}

/*
 *   Load my window configuration
 */
void CHtmlDbgScriptWin::load_win_config(CHtmlDebugConfig *config,
                                        const textchar_t *varname,
                                        RECT *pos, int *vis)
{
    CHtmlRect hrc;

    /* get my position from the saved configuration */
    if (!config->getval(varname, "pos", &hrc))
        SetRect(pos, hrc.left, hrc.top,
                hrc.right - hrc.left, hrc.bottom - hrc.top);

    /* get my visibility from the saved configuration */
    if (config->getval(varname, "vis", vis))
        *vis = TRUE;
}

/*
 *   Save my configuration
 */
void CHtmlDbgScriptWin::save_win_config(CHtmlDebugConfig *config,
                                        const textchar_t *varname)
{
    RECT rc;
    CHtmlRect hrc;

    /* save my visibility */
    config->setval(varname, "vis", IsWindowVisible(handle_));

    /* save my position */
    get_frame_pos(&rc);
    hrc.set(rc.left, rc.top, rc.right, rc.bottom);
    config->setval(varname, "pos", &hrc);
}

/*
 *   process a control notification
 */
int CHtmlDbgScriptWin::do_notify(
    int control_id, int notify_code, LPNMHDR nmhdr)
{
    NMLISTVIEW *nmlv;
    NMLVDISPINFO *nmdi;
    NMITEMACTIVATE *nmia;
    NMLVKEYDOWN *nmkd;
    LVITEM lvi;
    HWND fld;
    char buf[OSFNMAX];

    /* check the code */
    switch (notify_code)
    {
    case NM_DBLCLK:
        /* double click - open the file in the text editor */
        nmia = (NMITEMACTIVATE *)nmhdr;
        lvi.iItem = nmia->iItem;
        lvi.mask = LVIF_PARAM;
        if (ListView_GetItem(lv_, &lvi) && lvi.lParam != 0)
            dbgmain_->open_text_file(((lvdata *)lvi.lParam)->fname.get());

        /* handled */
        return TRUE;

    case NM_RCLICK:
        /* right click - bring up the item menu */
        nmia = (NMITEMACTIVATE *)nmhdr;
        right_click_item(nmia->iItem);

        /* allow the default processing to proceed */
        return 0;

    case LVN_DELETEITEM:
        /* deleting an item - delete its lvdata object */
        nmlv = (NMLISTVIEW *)nmhdr;
        lvi.iItem = nmlv->iItem;
        lvi.mask = LVIF_PARAM;
        if (ListView_GetItem(lv_, &lvi) && lvi.lParam != 0)
            delete (lvdata *)lvi.lParam;

        /* handled */
        return TRUE;

    case LVN_KEYDOWN:
        /* key press - check for special keys */
        nmkd = (NMLVKEYDOWN *)nmhdr;
        switch (nmkd->wVKey)
        {
        case VK_DELETE:
            /* handle this as an ID_SCRIPT_DELETE command */
            active_do_command(0, ID_SCRIPT_DELETE, 0);
            break;
        }

        /* allow the default handling to proceed */
        return 0;

    case LVN_BEGINLABELEDIT:
        /* disable accelerators while the label is being edited */
        CTadsApp::get_app()->push_accel_clear(&edit_accel_save_);

        /* only allow editing the filename portion */
        fld = ListView_GetEditControl(lv_);
        SendMessage(fld, WM_GETTEXT, (WPARAM)sizeof(buf), (LPARAM)buf);
        SendMessage(fld, WM_SETTEXT, 0, (LPARAM)os_get_root_name(buf));

        /* allow editing to proceed */
        return FALSE;

    case LVN_ENDLABELEDIT:
        /* restore accelerators */
        CTadsApp::get_app()->pop_accel(&edit_accel_save_);

        /* if we're updating the name, update the label and rename the file */
        nmdi = (NMLVDISPINFO *)nmhdr;
        if (nmdi->item.pszText != 0)
        {
            /* validate the text - it has to be a valid filename */
            for (char *p = nmdi->item.pszText ; *p != '\0' ; ++p)
            {
                if (strchr("\\/:*?\"<>", *p) != 0)
                {
                    /* don't allow the change */
                    MessageBox(dbgmain_->get_handle(),
                               "A script name cannot contain any of "
                               "these characters: \\ / : * ? \" < >",
                               "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }
            }

            /* it's valid - try renaming the file */
            LVITEM lvi;
            lvi.mask = LVIF_PARAM;
            lvi.iItem = nmdi->item.iItem;
            if (ListView_GetItem(lv_, &lvi))
            {
                char path[OSFNMAX];
                char buf[OSFNMAX];

                /* get the item */
                lvdata *d = (lvdata *)lvi.lParam;

                /* find the extension */
                char *fname = d->fname.get();
                char *p, *ext;
                for (ext = "", p = fname + strlen(fname) ; p > fname ; --p)
                {
                    if (*p == '.')
                    {
                        ext = p;
                        break;
                    }
                    else if (*p == '\\' || *p == ':' || *p == '/')
                        break;
                }

                /* build the new filename */
                os_get_path_name(path, sizeof(path), fname);
                os_build_full_path(buf, sizeof(buf), path,
                                   nmdi->item.pszText);
                if (strlen(buf) + strlen(ext) + 1 < sizeof(buf))
                    strcat(buf, ext);

                /* rename the file */
                if (!MoveFile(d->fname.get(), buf))
                {
                    MessageBox(dbgmain_->get_handle(),
                               "An error occurred renaming the script "
                               "file. Check that there isn't already "
                               "another script with the same name.",
                               "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
                    return FALSE;
                }

                /* update any open window on the file */
                dbgmain_->file_renamed(d->fname.get(), buf);

                /* remember the new name internally */
                d->fname.set(buf);

                /* set the new title - it's the name minus the extension */
                gen_item_title(buf, sizeof(buf), d->fname.get());

                /* set the new title */
                ListView_SetItemText(lv_, lvi.iItem, 0, buf);
            }
        }

        /* don't update the label - we did so explicitly */
        return FALSE;
    }

    /* inherit the default handling */
    return CTadsWin::do_notify(control_id, notify_code, nmhdr);
}

/*
 *   handle a right-click on an item
 */
void CHtmlDbgScriptWin::right_click_item(int item_idx)
{
    /* load the menu */
    HMENU m = LoadMenu(CTadsApp::get_app()->get_instance(),
                       MAKEINTRESOURCE(IDR_SCRIPT_MENU));

    /* get the cursor position */
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(handle_, &pt);

    /* run the context menu */
    int cmd = track_context_menu_ext(GetSubMenu(m, 0), pt.x, pt.y,
                                     TPM_TOPALIGN | TPM_LEFTALIGN
                                     | TPM_RETURNCMD);

    /* done with the menu for now */
    DestroyMenu(m);

    /* if we got a command, process it */
    if (cmd != 0)
        do_item_command(cmd, item_idx);
}

/*
 *   Carry out a command on an item
 */
void CHtmlDbgScriptWin::do_item_command(int command_id, int item_idx)
{
    LVITEM lvi;
    lvdata *item;

    /* get the data object from the item's lparam */
    lvi.iItem = item_idx;
    lvi.mask = LVIF_PARAM;
    if (!ListView_GetItem(lv_, &lvi) || (item = (lvdata *)lvi.lParam) == 0)
        return;

    /* handle the command on the item */
    switch (command_id)
    {
    case ID_SCRIPT_OPEN:
        /* open the script for editing */
        dbgmain_->open_text_file(item->fname.get());
        break;

    case ID_SCRIPT_RENAME:
        /* edit the item's label */
        ListView_EditLabel(lv_, item_idx);
        break;

    case ID_SCRIPT_DELETE:
        /* if this isn't an auto script, confirm the deletion */
        if (!is_auto_script_name(item->fname.get()))
        {
            char msgbuf[OSFNMAX + 256];

            sprintf(msgbuf, "%s\r\nDo you really want to permanently "
                    "delete this script file?", item->fname.get());
            if (MessageBox(0, msgbuf, "TADS Workbench",
                           MB_YESNO | MB_ICONQUESTION) != IDYES)
                break;
        }

        /* delete the underlying file */
        remove(item->fname.get());

        /* delete the list item */
        ListView_DeleteItem(lv_, item_idx);
        break;

    case ID_SCRIPT_REPLAY:
        /* replay the script */
        dbgmain_->run_script(item->fname.get(), FALSE);
        break;
    }
}

/*
 *   run the newest script in our list
 */
void CHtmlDbgScriptWin::run_latest_script()
{
    LVITEM lvi;

    /* if we don't have any scripts, there's nothing to do */
    if (ListView_GetItemCount(lv_) == 0)
        return;

    /*
     *   our list is sorted from newest to oldest, so the latest script is
     *   simply the first script
     */
    lvi.iItem = 0;
    lvi.mask = LVIF_PARAM;
    if (!ListView_GetItem(lv_, &lvi) || lvi.lParam == 0)
        return;

    /* the lparam is our data object */
    lvdata *d = (lvdata *)lvi.lParam;

    /*
     *   If this script is still active, and it's a duplicate of the previous
     *   run, we'll want to delete it as soon as we terminate this run (which
     *   we'll do as part of starting this new session).  However, we want
     *   the script to last long enough to play it back, so we potentially
     *   have a bit of a conflict.  The solution is to run our new session
     *   from a temporary copy of the old script - that way, it's okay if the
     *   new script is deleted before we start the new session, since we'll
     *   have our temporary copy already safely tucked away.
     */
    FILE *fpin = fopen(d->fname.get(), "r");
    if (fpin != 0)
    {
        char *fname = _tempnam(0, "tmpscript");
        if (fname != 0)
        {
            /* open the temp file */
            FILE *fpout = fopen(fname, "w");
            if (fpout != 0)
            {
                /* copy the file */
                for (;;)
                {
                    char buf[1024];

                    /* read a bufferfull */
                    size_t len = fread(buf, 1, sizeof(buf), fpin);
                    if (len == 0)
                        break;

                    /* write it back out */
                    fwrite(buf, 1, len, fpout);
                }
            }

            /* done with the output file */
            fclose(fpout);
        }

        /* done with the input file */
        fclose(fpin);

        /* run the script, noting that it's a temporary file */
        dbgmain_->run_script(fname, TRUE);

        /* done with the temp filename - free it */
        free(fname);
    }
}


/*
 *   get the title for a script file item
 */
void CHtmlDbgScriptWin::gen_item_title(textchar_t *title, size_t title_size,
                                       const textchar_t *fname)
{
    CHtmlDebugConfig *lc = dbgmain_->get_local_config();
    const textchar_t *dir;
    size_t len;

    /* start with the name minus the extension */
    safe_strcpy(title, title_size, fname);
    os_remext(title);

    /* get the script directory */
    if ((dir = lc->getval_strptr("auto-script", "dir", 0)) == 0)
        dir = "Scripts";

    /* remove the script directory prefix from the title, if present */
    len = strlen(dir);
    if (memcmp(title, dir, len) == 0 && title[len] == '\\')
        memmove(title, title + len + 1, strlen(title + len + 1) + 1);
}

/*
 *   get the number of scripts listed
 */
int CHtmlDbgScriptWin::get_script_count()
{
    return ListView_GetItemCount(lv_);
}

/*
 *   add a script to our list
 */
void CHtmlDbgScriptWin::add_script(const textchar_t *fname, int sort)
{
    LVITEM lvi;
    char title[OSFNMAX];
    int idx;

    /* if it's already in the list, don't add it again */
    if ((idx = find_file_item(fname)) != -1)
    {
        /* refresh the file time */
        lvi.iItem = idx;
        lvi.mask = LVIF_PARAM;
        if (ListView_GetItem(lv_, &lvi) && lvi.lParam != 0)
            ((lvdata *)lvi.lParam)->read_file_data();

        /* done */
        return;
    }

    /* get the title based on the filenaem */
    gen_item_title(title, sizeof(title), fname);

    /* add the new item */
    lvi.mask = LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = title;
    lvi.iImage = 0;
    lvi.lParam = (LPARAM)new lvdata(fname);
    ListView_InsertItem(lv_, &lvi);

    /* if desired, re-sort the list */
    if (sort)
        sort_list();
}

/*
 *   remove a script from our list
 */
void CHtmlDbgScriptWin::remove_script(const textchar_t *fname)
{
    int i;

    /* look for the item; if we find it, remove it from the list */
    if ((i = find_file_item(fname)) != -1)
        ListView_DeleteItem(lv_, i);
}

/*
 *   find an item by filename
 */
int CHtmlDbgScriptWin::find_file_item(const textchar_t *fname)
{
    int i, cnt;

    /* find the item with this filename */
    for (i = 0, cnt = ListView_GetItemCount(lv_) ; i < cnt ; ++i)
    {
        LVITEM lvi;

        /* get this item's lparam */
        lvi.iItem = i;
        lvi.mask = LVIF_PARAM;
        if (!ListView_GetItem(lv_, &lvi) || lvi.lParam == 0)
            continue;

        /* the lparam is our data object */
        lvdata *d = (lvdata *)lvi.lParam;

        /* if this filenames match, this is the one they want */
        if (stricmp(d->fname.get(), fname) == 0)
            return i;
    }

    /* didn't find it */
    return -1;
}


/*
 *   clear the list
 */
void CHtmlDbgScriptWin::clear_list()
{
    /* delete everything in the list view */
    ListView_DeleteAllItems(lv_);
}

/*
 *   prune the list to limit auto scripts to the given count
 */
void CHtmlDbgScriptWin::prune_auto(const textchar_t *dir, int cnt)
{
    int i, lstcnt;

    /* run through the list, looking for auto-scripts */
    for (i = 0, lstcnt = ListView_GetItemCount(lv_) ; i < lstcnt ; ++i)
    {
        LVITEM lvi;

        /* get this item's lparam */
        lvi.iItem = i;
        lvi.mask = LVIF_PARAM;
        if (!ListView_GetItem(lv_, &lvi) || lvi.lParam == 0)
            continue;

        /* the lparam is our data object */
        lvdata *d = (lvdata *)lvi.lParam;

        /* auto-scripts are always in the main script directory */
        textchar_t path[OSFNMAX];
        os_get_path_name(path, sizeof(path), d->fname.get());
        if (stricmp(dir, path) != 0)
            continue;

        /* skip it if it doesn't fit the "Auto" name pattern */
        if (!is_auto_script_name(d->fname.get()))
            continue;

        /*
         *   This is an auto-script.  We keep our whole list sorted in
         *   descending date order (newest first), so we always encounter
         *   auto-scripts from newest to oldest.  This means that we want to
         *   keep the first 'cnt' scripts we find in the list, and discard
         *   the rest.  So, each time we encounter an auto-script, we'll
         *   simply decrement 'cnt'; when 'cnt' reaches zero, we'll know that
         *   no more auto-scripts are wanted, so we'll discard this one.
         */
        if (cnt > 0)
        {
            /* we have room to keep this script - simply count it */
            --cnt;
        }
        else
        {
            /* We want to discard this script.  First, delete the file. */
            remove(d->fname.get());

            /* remove it from the list */
            ListView_DeleteItem(lv_, i);

            /* adjust our index and count for the removal of the item */
            --i;
            --lstcnt;
        }
    }
}

/*
 *   does the given filename fit the "Auto" script pattern?
 */
int CHtmlDbgScriptWin::is_auto_script_name(const textchar_t *fname)
{
    /* get the root name */
    const textchar_t *root = os_get_root_name((textchar_t *)fname);

    /* check that it starts with "Auto" */
    if (memicmp(root, "Auto ", 5) != 0 || !isdigit(*(root + 5)))
        return FALSE;

    /* check that only digits follow in the base filename */
    for (root += 6 ; isdigit(*root) ; ++root) ;

    /* make sure we're looking at the correct suffix */
    if (stricmp(root, ".cmd") != 0)
        return FALSE;

    /* it matches our pattern */
    return TRUE;
}

/*
 *   clean the list, by removing items that refer to non-existent files
 */
void CHtmlDbgScriptWin::clean_list()
{
    int i, cnt;

    /* run through the list */
    for (i = 0, cnt = ListView_GetItemCount(lv_) ; i < cnt ; ++i)
    {
        LVITEM lvi;

        /* get this item's lparam */
        lvi.iItem = i;
        lvi.mask = LVIF_PARAM;
        if (!ListView_GetItem(lv_, &lvi) || lvi.lParam == 0)
            continue;

        /* the lparam is our data object */
        lvdata *d = (lvdata *)lvi.lParam;

        /* if the file doesn't exist, delete this item */
        if (GetFileAttributes(d->fname.get()) == INVALID_FILE_ATTRIBUTES)
        {
            /* the file doesn't exist - remove the list item */
            ListView_DeleteItem(lv_, i);

            /* adjust our counters for the deletion */
            --i;
            --cnt;
        }
    }
}

/*
 *   synchronize the list with the current directory contents
 */
void CHtmlDbgScriptWin::sync_list(const textchar_t *dir)
{
    /* remove items that refer to deleted or renamed files */
    clean_list();

    /* re-scan the directory to add any new files */
    scan_dir(dir, TRUE);
}

/*
 *   scan a directory, adding each item to our list
 */
int CHtmlDbgScriptWin::scan_dir(const textchar_t *dir, int sort)
{
    WIN32_FIND_DATA info;
    HANDLE hf;
    char pat[OSFNMAX];
    int cnt = 0;

    /* set up the search pattern - "dir\*" */
    os_build_full_path(pat, sizeof(pat), dir, "*");

    /* run the search */
    if ((hf = FindFirstFile(pat, &info)) != INVALID_HANDLE_VALUE)
    {
        /* scan the files */
        do
        {
            char fname[OSFNMAX];
            size_t len;

            /* build the full filename */
            os_build_full_path(fname, sizeof(fname), dir, info.cFileName);

            /* if it's . or .., skip it */
            if (strcmp(info.cFileName, ".") == 0
                || strcmp(info.cFileName, "..") == 0)
                continue;

            /* check to see if it's a directory or a file */
            if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /* it's a directory - scan it recursively */
                cnt += scan_dir(fname, FALSE);
            }
            else if ((len = strlen(info.cFileName)) > 4
                     && stricmp(info.cFileName + len - 4, ".cmd") == 0)
            {
                /*
                 *   It's a regular file ending in ".cmd" - add it to our
                 *   list; don't sort now, as we'll do this when we're done.
                 */
                add_script(fname, FALSE);

                /* count it */
                ++cnt;
            }
        }
        while (FindNextFile(hf, &info));

        /* done - close the handle */
        FindClose(hf);
    }

    /* if we added anything, and the caller wants a sort, do the sort */
    if (sort && cnt != 0)
        sort_list();

    /* return the number of files we added */
    return cnt;
}

/*
 *   Sort the list
 */
void CHtmlDbgScriptWin::sort_list()
{
    /* sort via our item 'compare' function */
    ListView_SortItems(lv_, &lvdata::compare, 0);
}

