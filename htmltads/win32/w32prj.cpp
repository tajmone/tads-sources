#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/*
 *   Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32prj.cpp - TADS 3 debugger for Win32 - project window
Function

Notes

Modified
  01/14/00 MJRoberts  - Creation
*/

#include <Windows.h>
#include <shlobj.h>
#include <Ole2.h>
#include <string.h>

#include "os.h"

#include "t3std.h"
#include "tclibprs.h"

#ifndef W32PRJ_H
#include "w32prj.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif
#ifndef W32EXPR_H
#include "w32expr.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLDCFG_H
#include "htmldcfg.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSTAB_H
#include "tadstab.h"
#endif
#ifndef TADSOLE_H
#include "tadsole.h"
#endif
#ifndef FOLDSEL_H
#include "foldsel.h"
#endif
#ifndef ICONMENU_H
#include "iconmenu.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Add a directory to the library search list
 */
static void add_search_path(CHtmlSys_dbgmain *dbgmain, const char *path)
{
    CHtmlDebugConfig *gconfig;
    int cnt;

    /* get the debugger's global configuration object */
    gconfig = dbgmain->get_global_config();

    /* get the current number of search path entries */
    cnt = gconfig->get_strlist_cnt("srcdir", 0);

    /* add the new entry at the end of the current list */
    gconfig->setval("srcdir", 0, cnt, path);
}

/* ------------------------------------------------------------------------ */
/*
 *   "Absolute Path" dialog.
 */
class CTadsDlgAbsPath: public CTadsDialog, public CTadsFolderControl
{
public:
    CTadsDlgAbsPath(CHtmlSys_dbgmain *dbgmain, const char *fname)
    {
        /* save the debugger object and filename */
        dbgmain_ = dbgmain;
        fname_ = fname;
    }

    int do_command(WPARAM id, HWND ctl)
    {
        int idx;
        HWND pop;

        /* see what we have */
        switch(LOWORD(id))
        {
        case IDOK:
            /* get the item selected in the parent popup */
            pop = GetDlgItem(handle_, IDC_POP_PARENT);
            idx = SendMessage(pop, CB_GETCURSEL, 0, 0);
            if (idx != CB_ERR)
            {
                lb_folder_t *info;

                /* get our descriptor associated with the item */
                info = (lb_folder_t *)
                       SendMessage(pop, CB_GETITEMDATA, idx, 0);

                /* add the item's path to the seach list */
                add_search_path(dbgmain_, info->fullpath_.get());
            }

            /* continue with default handling */
            break;
        }

        /* inherit default handling */
        return CTadsDialog::do_command(id, ctl);
    }

    /* initialize */
    void init()
    {
        char path[OSFNMAX];

        /* inherit default handling */
        CTadsDialog::init();

        /* show the filename in the dialog */
        SetDlgItemText(handle_, IDC_TXT_FILE, fname_);

        /*
         *   populate the popup with the folder list - include only the file
         *   path, not the file itself
         */
        os_get_path_name(path, sizeof(path), fname_);
        populate_folder_tree(GetDlgItem(handle_, IDC_POP_PARENT),
                             FALSE, path);
    }

    /* destroy */
    void destroy()
    {
        /*
         *   manually clear out the parent popup so that our owner-drawn
         *   items are deleted properly - NT doesn't do this automatically on
         *   destroying the controls, so we must do it manually
         */
        SendMessage(GetDlgItem(handle_, IDC_POP_PARENT),
                    CB_RESETCONTENT, 0, 0);

        /* inherit default handling */
        CTadsDialog::destroy();
    }

    /* draw an owner-drawn item */
    int draw_item(int ctl_id, DRAWITEMSTRUCT *draw_info)
    {
        switch(ctl_id)
        {
        case IDC_POP_PARENT:
            /* draw the popup item */
            draw_folder_item(draw_info);

            /* handled */
            return TRUE;

        default:
            /* not handled */
            return FALSE;
        }
    }

    /* delete an owner-allocated control item  */
    int delete_ctl_item(int ctl_id, DELETEITEMSTRUCT *delete_info)
    {
        switch (ctl_id)
        {
        case IDC_POP_PARENT:
            /* delet ethe folder information */
            delete (lb_folder_t *)delete_info->itemData;
            return TRUE;

        default:
            /* not one of ours */
            return FALSE;
        }
    }

protected:
    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;

    /* the name of the file with the absolute path */
    const char *fname_;
};

/* ------------------------------------------------------------------------ */
/*
 *   "Missing Source File" dialog.
 */
class CTadsDlgMissingSrc: public CTadsDialog
{
public:
    CTadsDlgMissingSrc(CHtmlSys_dbgmain *dbgmain, const char *fname)
    {
        /* save the debugger object and filename */
        dbgmain_ = dbgmain;
        fname_ = fname;
    }

    /* initialize */
    void init()
    {
        /* inherit default handling */
        CTadsDialog::init();

        /* show the filename in the dialog */
        SetDlgItemText(handle_, IDC_TXT_FILE, fname_);
    }

    /* handle a command */
    int do_command(WPARAM id, HWND ctl)
    {
        OPENFILENAME5 info;
        char buf[OSFNMAX];
        char dir[OSFNMAX];
        char title[OSFNMAX + 50];

        /* see what we have */
        switch(LOWORD(id))
        {
        case IDC_BTN_FIND:
            /* "Find" button - locate the file */
            info.hwndOwner = handle_;
            info.hInstance = CTadsApp::get_app()->get_instance();
            info.lpstrFilter = "TADS Source Files\0*.t;*.tl\0";
            info.lpstrCustomFilter = 0;
            info.nFilterIndex = 0;
            info.lpstrFile = buf;
            strcpy(buf, os_get_root_name((char *)fname_));
            info.nMaxFile = sizeof(buf);
            info.lpstrFileTitle = 0;
            info.nMaxFileTitle = 0;
            info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
            sprintf(title, "%s - Please Locate File", fname_);
            info.lpstrTitle = title;
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
                return TRUE;

            /*
             *   Add the directory path of the file they selected to the
             *   search list (but just the directory path - leave off the
             *   filename portion).  Note that we don't check that the file
             *   they selected is the same file we asked for; all we care
             *   about is the path.
             */
            os_get_path_name(dir, sizeof(dir), buf);
            add_search_path(dbgmain_, dir);

            /* post an "OK" to myself so we dismiss and proceed */
            PostMessage(handle_, WM_COMMAND, IDOK, 0);

            /* handled */
            return TRUE;
        }

        /* return default handling */
        return CTadsDialog::do_command(id, ctl);
    }

protected:
    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;

    /* the name of the file with the absolute path */
    const char *fname_;
};


/* ------------------------------------------------------------------------ */
/*
 *   User-defined messages specific to this window
 */

/* open the selected item in the project tree */
#define W32PRJ_MSG_OPEN_TREE_SEL  (WM_USER + 1)

/* refresh a folder listing */
#define W32PRJ_MSG_REFRESH_FOLDER (WM_USER + 2)


/* ------------------------------------------------------------------------ */
/*
 *   template configuration group name for external resource files - we'll
 *   change the '0000' suffix to a serial number based on each external
 *   resource file's position in the project list
 */
#define EXTRES_CONFIG_TEMPLATE "extres_files_0000"


/* ------------------------------------------------------------------------ */
/*
 *   child "open" actions
 */
enum proj_item_open_t
{
    /* direct children cannot be opened */
    PROJ_ITEM_OPEN_NONE,

    /* open as a source file */
    PROJ_ITEM_OPEN_SRC,

    /* open an an external file */
    PROJ_ITEM_OPEN_EXTERN,

    /* auto-sense based on the file type */
    PROJ_ITEM_OPEN_SENSE
};

/* ------------------------------------------------------------------------ */
/*
 *   folder monitor thread
 */
class proj_item_folder_thread
{
public:
    proj_item_folder_thread(HWND hwnd, HTREEITEM item, const char *path)
        : path_(path)
    {
        /*
         *   set a reference on behalf of the caller, and another on behalf
         *   of our system thread
         */
        refcnt_ = 2;

        /* remember our project window and tree item */
        hwnd_ = hwnd;
        item_ = item;

        /*
         *   create the quit event - the main thread uses this to signal us
         *   when the tree item is being deleted
         */
        quit_ = CreateEvent(0, TRUE, FALSE, 0);

        /* create our system thread */
        DWORD tid;
        HANDLE th = CreateThread(0, 0, &_main, this, 0, &tid);
        if (th == 0)
        {
            /* couldn't create the thread - release its refrence */
            Release();
        }
    }

    /* reference management */
    void AddRef() { ++refcnt_; }
    void Release() { if (--refcnt_ == 0) delete this; }

    /* tell the thread to terminate */
    void terminate() { SetEvent(quit_); }

    /* main thread entrypoint */
    static DWORD WINAPI _main(void *ctx)
    {
        /* invoke our member function entrypoint */
        return ((proj_item_folder_thread *)ctx)->main();
    }

    /* main thread entrypoint member function */
    DWORD main()
    {
        /* set up the change notification handler */
        HANDLE cnh = FindFirstChangeNotification(
            path_.get(), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);

        /* keep going until we get the 'done' signal */
        for (int done = FALSE ; !done ; )
        {
            /* wait for one of our events */
            HANDLE hlst[2] = { quit_, cnh };
            switch (WaitForMultipleObjects(2, hlst, FALSE, INFINITE))
            {
            case WAIT_OBJECT_0:
                /* the 'quit' handle fired - we're done */
                done = TRUE;
                break;

            case WAIT_OBJECT_0 + 1:
                /* a change occurred - send the refresh event */
                SendMessage(hwnd_, W32PRJ_MSG_REFRESH_FOLDER,
                            0, (LPARAM)item_);

                /* set up the next event wait; abort if that fails */
                if (!FindNextChangeNotification(cnh))
                    done = TRUE;
                break;

            default:
                /*
                 *   anything else is an error; abort the thread, since if we
                 *   loop we'll probably just encounter the same error again
                 */
                done = TRUE;
                break;
            }
        }

        /* release our self-reference on the way out */
        Release();

        /* done */
        return 0;
    }

    /* our project window and parent tree item */
    HWND hwnd_;
    HTREEITEM item_;

    /* folder path name */
    CStringBuf path_;

    /* termination event */
    HANDLE quit_;

    /* reference count */
    int refcnt_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Item extra information structure.  We allocate one of these
 *   structures for each item in the list so that we can track extra data
 *   for the item.
 */
struct proj_item_info
{
    proj_item_info(const char *fname, proj_item_type_t item_type,
                   const char *config_group,
                   const char *child_path, int child_icon,
                   int can_delete, int can_add_children,
                   int openable, proj_item_open_t child_open_action,
                   proj_item_type_t child_type, int is_sys_file)
    {
        /* remember the filename */
        if (fname == 0 || fname[0] == '\0')
            fname_ = 0;
        else
        {
            fname_ = (char *)th_malloc(strlen(fname) + 1);
            strcpy(fname_, fname);
        }

        /* remember the configuration file group name */
        if (config_group == 0 || config_group[0] == '\0')
            config_group_ = 0;
        else
        {
            config_group_ = (char *)th_malloc(strlen(config_group) + 1);
            strcpy(config_group_, config_group);
        }

        /* remember my type */
        type_ = item_type;

        /* remember the child path */
        if (child_path == 0 || child_path[0] == '\0')
            child_path_ = 0;
        else
        {
            child_path_ = (char *)th_malloc(strlen(child_path) + 1);
            strcpy(child_path_, child_path);
        }

        /* remember the child icon */
        child_icon_ = child_icon;

        /* note whether we can delete the item */
        can_delete_ = can_delete;

        /* note whether we can add children */
        can_add_children_ = can_add_children;

        /* note whether this can be opened in the user interface */
        openable_ = openable;

        /* remember the child open action */
        child_open_action_ = child_open_action;

        /* remember the child type */
        child_type_ = child_type;

        /* presume it's included in the installer */
        in_install_ = TRUE;

        /* remember whether or not it's a system file */
        is_sys_file_ = is_sys_file;

        /* we don't keep timestamp information by default */
        memset(&file_time_, 0, sizeof(file_time_));

        /* no URL-style name yet */
        url_ = 0;

        /* no child system-file path yet */
        child_sys_path_ = 0;

        /* presume it's not excluded from its library */
        exclude_from_lib_ = FALSE;

        /* no parent-library or user-library path yet */
        lib_path_ = 0;
        in_user_lib_ = FALSE;

        /* make it recursive by default (if it's a resource directory) */
        recursive_ = TRUE;

        /*
         *   assume it has a start-menu title only if it's a feelie or web
         *   extra
         */
        has_start_menu_title_ = (type_ == PROJ_ITEM_TYPE_FEELIE
                                 || type_ == PROJ_ITEM_TYPE_WEBEXTRA);

        /* we don't have a folder monitor thread */
        folder_thread_ = 0;
    }

    ~proj_item_info()
    {
        /* delete the filename */
        if (fname_ != 0)
            th_free(fname_);

        /* delete the configuration group name */
        if (config_group_ != 0)
            th_free(config_group_);

        /* delete the child path */
        if (child_path_ != 0)
            th_free(child_path_);

        /* delete the URL-style name if present */
        if (url_ != 0)
            th_free(url_);

        /* delete the child system-file path if present */
        if (child_sys_path_ != 0)
            th_free(child_sys_path_);

        /* if we have a library path, free it */
        if (lib_path_ != 0)
            th_free(lib_path_);

        /* kill our folder thread */
        if (folder_thread_ != 0)
        {
            folder_thread_->terminate();
            folder_thread_->Release();
        }
    }

    /* set the item's URL-style name */
    void set_url(const char *url)
    {
        /* if we already have a url-style name, delete it */
        if (url_ != 0)
            th_free(url_);

        /* allocate space and copy the URL string */
        if (url != 0 && url[0] != '\0')
        {
            url_ = (char *)th_malloc(strlen(url) + 1);
            strcpy(url_, url);
        }
        else
            url_ = 0;
    }

    /* set the item's child system-file path */
    void set_child_sys_path(const char *path)
    {
        /* free any old path */
        if (child_sys_path_ != 0)
            th_free(child_sys_path_);

        /* allocate space and copy the string */
        if (path != 0 && path[0] != '\0')
        {
            child_sys_path_ = (char *)th_malloc(strlen(path) + 1);
            strcpy(child_sys_path_, path);
        }
        else
            child_sys_path_ = 0;
    }

    /* set the item's library path */
    void set_lib_path(const char *path)
    {
        /* free any old path */
        if (lib_path_ != 0)
            th_free(lib_path_);

        /* allocate space and copy the string */
        if (path != 0 && path[0] != '\0')
        {
            lib_path_ = (char *)th_malloc(strlen(path) + 1);
            strcpy(lib_path_, path);
        }
        else
            lib_path_ = 0;
    }

    /* set the item's user library path */
    void set_user_path(const char *path)
    {
        /* save the library path */
        set_lib_path(path);

        /* remember that we're in a user library */
        in_user_lib_ = TRUE;
    }

    /* set the filename to a new string */
    void set_filename(const char *fname)
    {
        if (fname_ != 0)
        {
            th_free(fname_);
            fname_ = 0;
        }
        if (fname != 0)
        {
            fname_ = (char *)th_malloc(strlen(fname) + 1);
            strcpy(fname_, fname);
        }
    }

    /*
     *   Filename string - if this item represents a file, this contains the
     *   full filename; this entry is null for artificial grouping items
     *   (such as the "Sources" folder).
     *
     *   For most items, the filename stored here is relative to the
     *   containing item's full path.  However, there are exceptions:
     *
     *   - If is_sys_file_ is true, this filename is relative to the
     *   containing item's system directory path.
     *
     *   - If lib_path_ is non-null, then this filename is relative to the
     *   path given by lib_path_.
     */
    char *fname_;

    /* our Special File title - applicable to special items only */
    CStringBuf special_title_;

    /* our Special File configuration subkey */
    CStringBuf special_subkey_;

    /* file dialog filter for this special file type */
    CStringBuf special_filter_;

    /*
     *   Start Menu title for Windows SETUP edition - applicable to special
     *   items and feelies
     */
    CStringBuf start_menu_title_;

    /*
     *   Parent library path.  If this is non-null, then the filename in
     *   fname_ is relative to this path rather than to the container path.
     *
     *   This path is either the path to some library file that is now or
     *   has in the past been part of the project, or is a directory in the
     *   global configuration's user library search path.
     */
    char *lib_path_;

    /*
     *   Flag: the lib_path_ value is from the user library search path.  If
     *   this is true, then lib_path_ is a path taken from the user library
     *   search list; otherwise, lib_path_ is the path to a directory
     *   containing a .tl file.
     */
    int in_user_lib_;

    /*
     *   the URL-style filename - we retain this for library members,
     *   because it allows us to build an exclusion (-x) option for this
     *   item for the compiler command line
     */
    char *url_;

    /*
     *   configuration group name - this is the name of the string list in
     *   the configuration file that stores children of this object;
     *   applies only to containers
     */
    char *config_group_;

    /* my item type */
    proj_item_type_t type_;

    /* child object path - this is relative to any containers */
    char *child_path_;

    /* child object path for system files */
    char *child_sys_path_;

    /* icon to use for children of this item (index in image list) */
    int child_icon_;

    /* flag indicating whether we can delete this item */
    unsigned int can_delete_ : 1;

    /* flag indicating whether we can add children */
    unsigned int can_add_children_ : 1;

    /* flag indicating whether we can open this object in the UI */
    unsigned int openable_ : 1;

    /* for external resources: is it included in the install? */
    unsigned int in_install_ : 1;

    /* for resource directory entries: is this recursive? */
    unsigned int recursive_ : 1;

    /* flag indicating whether it's a system file or a user file */
    unsigned int is_sys_file_ : 1;

    /* flag indicating whether or not it's excluded from its library */
    unsigned int exclude_from_lib_ : 1;

    /* is this item allowed to have a Start Menu title? */
    unsigned int has_start_menu_title_ : 1;

    /* action to take to open a child item */
    proj_item_open_t child_open_action_;

    /* type of child items */
    proj_item_type_t child_type_;

    /*
     *   file timestamp - we keep this information for certain types of
     *   files (libraries) so that we can tell if the file on disk has
     *   changed since we last loaded the file
     */
    FILETIME file_time_;

    /*
     *   folder thread - we start a thread for each resource folder to
     *   monitor changes in the file system, so that we keep our listing up
     *   to date with the file system
     */
    proj_item_folder_thread *folder_thread_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Library parser for the project window - this is a library parser
 *   specialized to parse a library file as we add it to the project window.
 */
class CTcLibParserProj: public CTcLibParser
{
public:
    /*
     *   Parse a library and add the files in the library to the command
     *   line under construction.
     */
    static void add_lib_to_proj(const char *lib_name,
                                CHtmlDbgProjWin *proj_win, HWND tree,
                                HTREEITEM lib_tree_item,
                                proj_item_info *lib_item_info)
    {
        /* set up our library parser object */
        CTcLibParserProj parser(lib_name, proj_win, tree,
                                lib_tree_item, lib_item_info);

        /* scan the library and add its files to the command line */
        parser.parse_lib();
    }

protected:
    /* instantiate */
    CTcLibParserProj(const char *lib_name, CHtmlDbgProjWin *proj_win,
                     HWND tree, HTREEITEM lib_tree_item,
                     proj_item_info *lib_item_info)
        : CTcLibParser(lib_name)
    {
        /* remember our project window information */
        proj_win_ = proj_win;
        tree_ = tree;

        /* remember the parent library item information */
        lib_tree_item_ = lib_tree_item;
        lib_item_info_ = lib_item_info;
    }

    /*
     *   process the "name" variable - we'll set the displayed name of the
     *   library to this name
     */
    void scan_name(const char *name)
    {
        char buf[OSFNMAX + 1024];
        TV_ITEM item;

        /*
         *   build the full display name by using the library's descriptive
         *   name plus the filename in parentheses
         */
        sprintf(buf, "%s (%s)", name, lib_item_info_->fname_);

        /* set the new name */
        item.mask = TVIF_HANDLE | TVIF_TEXT;
        item.hItem = lib_tree_item_;
        item.pszText = buf;
        TreeView_SetItem(tree_, &item);
    }

    /*
     *   Process a "source" entry in the library.  Note that we scan the raw
     *   source file name, not the full filename with the library source
     *   path, because we do our own figuring on including the library
     *   source path.
     */
    void scan_source(const char *fname)
    {
        /* add this with a default "t" extension and known type SOURCE */
        scan_add_to_project(fname, "t", PROJ_ITEM_TYPE_SOURCE);
    }


    /*
     *   Process a "library" entry.  As with scan_file(), we scan the raw
     *   filename, because we handle path considerations ourselves.
     */
    void scan_library(const char *fname)
    {
        /* add this with a default "tl" extension and known type LIBRARY */
        scan_add_to_project(fname, "tl", PROJ_ITEM_TYPE_LIB);
    }

    /*
     *   Process a "resource" entry.  A resource can be entered as a single
     *   file or as an entire directory.
     */
    void scan_resource(const char *name)
    {
        scan_add_to_project(name, 0, PROJ_ITEM_TYPE_RESOURCE);
    }

    /*
     *   Common handler for scan_source() and scan_library().  Adds the file
     *   to the project with the given default extension and known item
     *   type.
     */
    void scan_add_to_project(const char *url, const char *ext,
                             proj_item_type_t known_type)
    {
        /* convert the filename from URL notation to Windows notation */
        char rel_path[OSFNMAX];
        os_cvt_url_dir(rel_path, sizeof(rel_path), url);

        /* add the default extension if necessary */
        if (ext != 0)
            os_defext(rel_path, ext);

        /* get the parent directory */
        char parent_path[OSFNMAX];
        proj_win_->get_item_child_dir(parent_path, sizeof(parent_path),
                                      lib_tree_item_);

        /* build the full path */
        char full_path[OSFNMAX];
        os_build_full_path(full_path, sizeof(full_path),
                           parent_path, rel_path);

        /* if this is a resource folder, add it as a folder object */
        if (known_type == PROJ_ITEM_TYPE_RESOURCE)
        {
            /* check to see if it's a directory */
            DWORD attr = GetFileAttributes(full_path);
            if (attr != INVALID_FILE_ATTRIBUTES
                && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                known_type = PROJ_ITEM_TYPE_FOLDER;
        }

        /* add this item to the project */
        HTREEITEM item = proj_win_->add_file_to_project(
            lib_tree_item_, TVI_LAST, full_path, known_type, FALSE);

        /*
         *   save the original URL-style name, since this is the name we use
         *   to identify the file for exclusion options
         */
        proj_item_info *info;
        if (item != 0 && (info = proj_win_->get_item_info(item)) != 0)
            info->set_url(url);
    }

    /* scan a "nodef" flag in the library */
    void scan_nodef()
    {
        /*
         *   We don't need to do anything special here.  Note, in particular,
         *   that we do NOT need to set the "-nodef" flag for our own Build
         *   Settings configuration, because the presence of this "nodef"
         *   flag in the library is enough; adding our own -nodef flag would
         *   be redundant.  So we can simply ignore this.
         */
    }

    /* look up a preprocessor symbol */
    int get_pp_symbol(char *dst, size_t dst_len,
                      const char *sym_name, size_t sym_len)
    {
        CHtmlDebugConfig *config;
        size_t cnt;
        size_t i;

        /* get the configuration object (which has the build settings) */
        config = proj_win_->dbgmain_->get_local_config();

        /* get the number of -D symbols */
        cnt = config->get_strlist_cnt("build", "defines");

        /* scan the -D symbols for the one they're asking about */
        for (i = 0 ; i < cnt ; ++i)
        {
            char buf[1024];
            char *val;
            size_t var_len;

            /* get this variable from the build settings */
            config->getval("build", "defines", i, buf, sizeof(buf));

            /*
             *   scan for an equals sign - the build configuration stores the
             *   variables as NAME=VALUE strings
             */
            for (val = buf ; *val != '\0' && *val != '=' ; ++val) ;

            /*
             *   if we found an equals sign, the variable name is the part up
             *   to the '=', and the rest is the value; if we didn't find an
             *   equals sign, the whole thing is the name and the value is
             *   the implied value "1"
             */
            var_len = val - buf;
            val = (*val == '=' ? val + 1 : "1");

            /* if it matches, return the value */
            if (sym_len == var_len && memcmp(buf, sym_name, var_len) == 0)
            {
                size_t val_len;

                /* if there's room, copy the expansion */
                val_len = strlen(val);
                if (val_len < dst_len)
                    memcpy(dst, val, val_len);

                /* return the result length */
                return val_len;
            }
        }

        /* didn't find it - so indicate by returning -1 */
        return -1;
    }

    /* the project window */
    CHtmlDbgProjWin *proj_win_;

    /* the treeview control for the project window */
    HWND tree_;

    /*
     *   the treeview item for the library file, which is the parent of the
     *   files we add to the tree; and our extra information object for that
     *   item
     */
    HTREEITEM lib_tree_item_;
    proj_item_info *lib_item_info_;
};


/* ------------------------------------------------------------------------ */
/*
 *   control ID for the tree view
 */
const int TREEVIEW_ID = 1;

/* statics */
HCURSOR CHtmlDbgProjWin::no_csr_ = 0;
HCURSOR CHtmlDbgProjWin::move_csr_ = 0;
HCURSOR CHtmlDbgProjWin::link_csr_ = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Creation
 */
CHtmlDbgProjWin::CHtmlDbgProjWin(CHtmlSys_dbgmain *dbgmain)
{
    /* use off-screen rendering */
    off_screen_render_ = TRUE;

    /* remember the debugger main window object, and add our reference */
    dbgmain_ = dbgmain;
    dbgmain_->AddRef();

    /*
     *   note whether or not we have a project open - if there's no game
     *   filename in the main debugger object, no project is open, so we
     *   show only a minimal display indicating this
     */
    proj_open_ = (dbgmain_->get_gamefile_name() != 0
                  && dbgmain_->get_gamefile_name()[0] != '\0');

    /* not tracking a drag yet */
    tracking_drag_ = FALSE;
    last_drop_target_ = 0;
    last_drop_target_sel_ = FALSE;
    drag_item_ = 0;
    drag_info_ = 0;

    /* load our static cursors if we haven't already */
    if (no_csr_ == 0)
        no_csr_ = LoadCursor(0, IDC_NO);
    if (move_csr_ == 0)
        move_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDC_DRAG_MOVE_CSR));
    if (link_csr_ == 0)
        link_csr_ = LoadCursor(CTadsApp::get_app()->get_instance(),
                               MAKEINTRESOURCE(IDC_DRAG_LINK_CSR));

    /* we don't have any of our special tree items yet */
    src_item_ = 0;
    inc_item_ = 0;
    res_item_ = 0;
    feelies_item_ = 0;
}

/*
 *   Deletion
 */
CHtmlDbgProjWin::~CHtmlDbgProjWin()
{
}

/*
 *   Find the full path to a file.
 */
int CHtmlDbgProjWin::find_file_path(char *fullname, size_t fullname_len,
                                    const char *name)
{
    /* recursively scan the tree starting at the root level */
    return find_file_path_in_tree(TVI_ROOT, fullname, fullname_len,
                                  os_get_root_name((char *)name));
}

/*
 *   Find the full path to a file among the children of the given parent
 *   item in the treeview control, recursively searching children of the the
 *   children of parent.
 */
int CHtmlDbgProjWin::find_file_path_in_tree(
    HTREEITEM parent, char *fullname, size_t fullname_len,
    const char *rootname)
{
    /* scan each child of this parent */
    for (HTREEITEM cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        proj_item_info *info = get_item_info(cur);
        if (info != 0 && info->fname_ != 0)
        {
            /*
             *   if this item's root name matches the name we're looking
             *   for, return this item's full path
             */
            if (stricmp(os_get_root_name(info->fname_), rootname) == 0)
            {
                /* get the full path to this item */
                get_item_path(fullname, fullname_len, cur);

                /* this is the one we're looking for */
                return TRUE;
            }
        }

        /* search this item's children */
        if (find_file_path_in_tree(cur, fullname, fullname_len, rootname))
            return TRUE;
    }

    /* we didn't find it; so indicate */
    return FALSE;
}

/*
 *   Find a file by path name
 */
HTREEITEM CHtmlDbgProjWin::find_item_for_file(
    HTREEITEM parent, const textchar_t *fname)
{
    /* scan each child */
    for (HTREEITEM cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's path name */
        textchar_t curpath[OSFNMAX];
        get_item_path(curpath, sizeof(curpath), cur);

        /* compare it */
        if (stricmp(curpath, fname) == 0)
            return cur;

        /* search this item's children */
        HTREEITEM chi = find_item_for_file(cur, fname);
        if (chi != 0)
            return chi;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Refresh after application activation.
 */
void CHtmlDbgProjWin::refresh_for_activation()
{
    /*
     *   Recursively refresh everything in the tree starting at the root.
     *   Only refresh libraries whose files have been updated externally.
     */
    refresh_lib_tree(TVI_ROOT, FALSE);
}

/*
 *   Unconditionally reload all libraries.  This should be called whenever a
 *   macro variable definition changes, to ensure that any library inclusions
 *   that depend on macro variables are reloaded to reflect the new variable
 *   values.
 */
void CHtmlDbgProjWin::refresh_all_libs()
{
    /* unconditionally refresh all libraries in the tree */
    refresh_lib_tree(TVI_ROOT, TRUE);
}

/*
 *   Refresh children of the given parent for application activation, and
 *   recursively refresh their children.  If 'always_reload' is true, we'll
 *   unconditionally reload all libraries, regardless of file timestamps;
 *   this can be used to ensure that all libraries are reloaded due to a
 *   change in macro variable values, for example.
 */
void CHtmlDbgProjWin::refresh_lib_tree(HTREEITEM parent, int always_reload)
{
    CHtmlDebugConfig config;

    /* save the project configuration into our temporary config object */
    save_config(&config);

    /* refresh the tree */
    refresh_lib_tree(&config, parent, always_reload);
}

void CHtmlDbgProjWin::refresh_lib_tree(CHtmlDebugConfig *config,
                                       HTREEITEM parent, int always_reload)
{
    /* visit each child of the given parent */
    for (HTREEITEM cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        proj_item_info *info = get_item_info(cur);

        /* assume we won't need to reload this file */
        int reload_cur = FALSE;

        /* if this is a library, check to see if we need to reload it */
        if (info != 0 && info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* determine if we need to reload */
            if (always_reload)
            {
                /* we are reloading all libraries */
                reload_cur = TRUE;
            }
            else
            {
                FILETIME ft;

                /*
                 *   we're only reloading libraries that have changed on
                 *   disk, so check this library's timestamp: get the current
                 *   timestamp for the item
                 */
                get_item_timestamp(cur, &ft);

                /*
                 *   if the timestamp is different than it was when we first
                 *   loaded the file, reload the file
                 */
                reload_cur = (CompareFileTime(&ft, &info->file_time_) != 0);
            }

            /* if we need to reload the library, do so */
            if (reload_cur)
                refresh_library(config, cur);
        }

        /*
         *   If we didn't reload this item, recursively visit this child's
         *   children.  We don't need to traverse into an item that we just
         *   explicitly reloaded, as all of their children will necessarily
         *   be reloaded when we reload the parent.
         */
        if (!reload_cur)
            refresh_lib_tree(config, cur, always_reload);
    }
}

/*
 *   Reload a library into the tree.  This is used when we detect that a
 *   library file's timestamp has changed since we originally loaded the
 *   file.
 */
void CHtmlDbgProjWin::refresh_library(CHtmlDebugConfig *config,
                                      HTREEITEM lib_item)
{
    /*
     *   delete all of the item's immediate children (this will
     *   automatically delete the children of the children, and so on)
     */
    for (HTREEITEM cur = TreeView_GetChild(tree_, lib_item) ; cur != 0 ;
         cur = TreeView_GetChild(tree_, lib_item))
    {
        /* delete the current child */
        TreeView_DeleteItem(tree_, cur);
    }

    /* get the library's filename */
    char path[OSFNMAX];
    get_item_path(path, sizeof(path), lib_item);

    /*
     *   remember the item's current expanded state, so we can restore it
     *   after loading (the loading process will open it even it's closed to
     *   start with)
     */
    TVITEM tvi;
    tvi.mask = TVIF_HANDLE | TVIF_STATE;
    tvi.stateMask = 0xff;
    tvi.hItem = lib_item;
    TreeView_GetItem(tree_, &tvi);
    int was_expanded = (tvi.state & TVIS_EXPANDED) != 0;

    /* reload the library */
    proj_item_info *lib_info = get_item_info(lib_item);
    CTcLibParserProj::add_lib_to_proj(path, this, tree_, lib_item, lib_info);

    /* restore the original expanded state of the item */
    TreeView_Expand(tree_, lib_item,
                    was_expanded ? TVE_EXPAND : TVE_COLLAPSE);

    /* reload the file exclusion settings */
    load_config_lib_exclude(config, lib_item, lib_info);

    /* the new time is now the loaded time */
    get_item_timestamp(lib_item, &lib_info->file_time_);
}

/*
 *   Process window creation
 */
void CHtmlDbgProjWin::do_create()
{
    /* do default creation work */
    CTadsWin::do_create();

    /*
     *   create our drop target helper object (we need to do this explicitly
     *   because we do our own drop target registration management)
     */
    create_drop_target_helper();

    /* create the tree control */
    create_tree();
}

/*
 *   create the tree control
 */
void CHtmlDbgProjWin::create_tree()
{
    HTREEITEM proj_item;
    HBITMAP bmp;
    const char *proj_file_name;
    const char *proj_name;
    DWORD ex_style;
    DWORD style;
    char proj_file_dir[OSFNMAX];

    /* get the local configuration object */
    CHtmlDebugConfig *config = dbgmain_->get_local_config();

    /* get our client area - the tree will fill the whole thing */
    RECT rc;
    GetClientRect(handle_, &rc);

    /* set the base styles for the tree control */
    style = WS_VISIBLE | WS_CHILD;
    ex_style = 0;

    /* add some style information if a project is open */
    if (proj_open_)
        style |= TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT;

    /* create a tree control filling our whole client area */
    tree_ = CreateWindowEx(ex_style, WC_TREEVIEW, "project tree", style,
                           0, 0, rc.right, rc.bottom, handle_,
                           (HMENU)TREEVIEW_ID,
                           CTadsApp::get_app()->get_instance(), 0);

    /* register ourselves as the drag-drop handler for the treeview */
    RegisterDragDrop(tree_, this);

    /*
     *   if there's a project, show the project's image file name as the
     *   root level object name; otherwise, simply show "No Project"
     */
    if (proj_open_)
    {
        /* there's a project - use the image file name */
        proj_file_name = dbgmain_->get_local_config_filename();

        /* the displayed project name is the root part of the name */
        proj_name = os_get_root_name((char *)proj_file_name);

        /* get the directory portion of the project path */
        os_get_path_name(proj_file_dir, sizeof(proj_file_dir),
                         proj_file_name);

        /*
         *   Build the NORMAL image list
         */

        /* create the image list */
        img_list_ = ImageList_Create(16, 16, FALSE, 6, 0);

        /* add the root bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_ROOT));
        img_idx_root_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the folder bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_FOLDER));
        img_idx_folder_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the source file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_SOURCE));
        img_idx_source_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the resource file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_RESOURCE));
        img_idx_resource_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the resource folder bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_RES_FOLDER));
        img_idx_res_folder_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the external resource file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_EXTRESFILE));
        img_idx_extres_ = ImageList_Add(img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the library file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_LIB));
        img_idx_lib_ = ImageList_Add(img_list_, bmp, 0);

        /* add the web extra file bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_WEBEXTRA));
        img_idx_webextra_ = ImageList_Add(img_list_, bmp, 0);

        /* set the image list in the tree control */
        TreeView_SetImageList(tree_, img_list_, TVSIL_NORMAL);

        /*
         *   Build the STATE image list
         */

        /* create the state image list */
        state_img_list_ = ImageList_Create(16, 16, FALSE, 2, 0);

        /*
         *   add the in-install state bitmap - note that we must add this
         *   item twice, since the zeroeth item in a state list is never
         *   used (this is an artifact of the windows treeview API - image
         *   zero means no image)
         */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_IN_INSTALL));
        ImageList_Add(state_img_list_, bmp, 0);
        img_idx_in_install_ = ImageList_Add(state_img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the not-in-install state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_NOT_IN_INSTALL));
        img_idx_not_in_install_ = ImageList_Add(state_img_list_, bmp, 0);
        DeleteObject(bmp);

        /* add the checkbox-unchecked state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_NOCHECK));
        img_idx_nocheck_ = ImageList_Add(state_img_list_, bmp, 0);

        /* add the checkbox-checked state bitmap */
        bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                         MAKEINTRESOURCE(IDB_PROJ_CHECK));
        img_idx_check_ = ImageList_Add(state_img_list_, bmp, 0);

        /* set the state image list in the tree control */
        TreeView_SetImageList(tree_, state_img_list_, TVSIL_STATE);
    }
    else
    {
        /* no project - display an indication to this effect */
        proj_name = "(No Project)";

        /* there's no project filename */
        proj_file_name = 0;
        proj_file_dir[0] = '\0';

        /* there's no image list */
        img_idx_root_ = 0;
        img_idx_folder_ = 0;
        img_idx_source_ = 0;
        img_idx_resource_ = 0;
        img_idx_res_folder_ = 0;
        img_idx_extres_ = 0;
    }

    /* fill in the top-level "project" item */
    proj_item = insert_tree(TVI_ROOT, TVI_LAST, proj_name,
                            TVIS_BOLD | TVIS_EXPANDED, img_idx_root_,
                            new proj_item_info(proj_file_name,
                                               PROJ_ITEM_TYPE_PROJECT, 0,
                                               proj_file_dir,
                                               0, FALSE, FALSE, FALSE,
                                               PROJ_ITEM_OPEN_NONE,
                                               PROJ_ITEM_TYPE_FOLDER, FALSE));

    /*
     *   add the project-level containers if a project is open - don't
     *   bother with this if there's no project, since we can't list any
     *   files unless we have a project
     */
    if (proj_open_)
    {
        HTREEITEM cur;
        int extres_cnt;
        int i;
        proj_item_info *info;
        char path[OSFNMAX];

        /* fill in the "sources" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "source_files",
                                  0, img_idx_source_, FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_SRC, PROJ_ITEM_TYPE_SOURCE,
                                  FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Source Files",
                          TVIS_BOLD | TVIS_EXPANDED, img_idx_folder_, info);

        /* remember the "sources" group */
        src_item_ = cur;

        /* set the system path for library source files */
        os_get_special_path(path, sizeof(path), _pgmptr, OS_GSP_T3_LIB);
        info->set_child_sys_path(path);

        /* load the source files from the configuration */
        load_config_files(config, cur);

        /* fill in the "includes" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "include_files",
                                  0, img_idx_source_, FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_SRC, PROJ_ITEM_TYPE_HEADER,
                                  FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Include Files",
                          TVIS_BOLD | TVIS_EXPANDED, img_idx_folder_, info);

        /* set the system path for library header files */
        os_get_special_path(path, sizeof(path), _pgmptr, OS_GSP_T3_INC);
        info->set_child_sys_path(path);

        /* remember the "includes" group */
        inc_item_ = cur;

        /* load the include files from the configuration */
        load_config_files(config, cur);

        /*
         *   fill in the "resources" item (don't expand this one
         *   initially, since it could be large)
         */
        cur = insert_tree(proj_item, TVI_LAST, "Resource Files",
                          TVIS_BOLD, img_idx_folder_,
                          new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER,
                                             "image_resources",
                                             0, img_idx_resource_,
                                             FALSE, TRUE, FALSE,
                                             PROJ_ITEM_OPEN_EXTERN,
                                             PROJ_ITEM_TYPE_RESOURCE, FALSE));

        /* remember the "resources" group */
        res_item_ = cur;

        /* load the resource files from the configuration */
        load_config_files(config, cur);

        /* explicitly collapse this list */
        TreeView_Expand(tree_, cur, TVE_COLLAPSE);

        /* get the number of external resource files */
        if (config->getval("build", "ext resfile cnt", &extres_cnt))
            extres_cnt = 0;

        /* load the external resource file configuration */
        for (i = 0 ; i < extres_cnt ; ++i)
        {
            char dispname[50];
            char groupname[50];
            char in_inst[10];
            proj_item_info *info;

            /* build the names */
            gen_extres_names(dispname, groupname, i);

            /* create the information structure */
            info = new proj_item_info(0, PROJ_ITEM_TYPE_EXTRES, groupname,
                                      proj_file_dir, img_idx_resource_,
                                      TRUE, TRUE, FALSE,
                                      PROJ_ITEM_OPEN_EXTERN,
                                      PROJ_ITEM_TYPE_RESOURCE, FALSE);

            /* add this resource item */
            cur = insert_tree(TVI_ROOT, TVI_LAST, dispname, TVIS_BOLD,
                              img_idx_extres_, info);

            /* get the in-intsaller status (put it in by default) */
            if (config->getval("build", "ext_resfile_in_install", i,
                               in_inst, sizeof(in_inst)))
                in_inst[0] = 'Y';

            /* set the status */
            info->in_install_ = (in_inst[0] == 'Y');
            set_extres_in_install(cur, info->in_install_);

            /* load the contents of this external resource file */
            load_config_files(config, cur);

            /* explicitly collapse this list */
            TreeView_Expand(tree_, cur, TVE_COLLAPSE);
        }


        /*
         *   Special file items
         */

        /* fill in the "special files" item */
        info = new proj_item_info(
            0, PROJ_ITEM_TYPE_FOLDER, 0, 0, img_idx_resource_,
            FALSE, FALSE, FALSE,
            PROJ_ITEM_OPEN_EXTERN, PROJ_ITEM_TYPE_SPECIAL, FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Special Files",
                          TVIS_BOLD | TVIS_EXPANDED,
                          img_idx_folder_, info);

        /* create the special items */
        load_special_item(config, cur, "ReadMe File",
                          "All Documents\0*.txt;*.htm;*.html;*.pdf;*.doc\0"
                          "Text Files (*.txt)\0*.txt\0"
                          "Web Pages (*.htm)\0*.htm;*.html\0"
                          "Portable Document Format (*.pdf)\0*.pdf\0"
                          "MS Word Documents (*.doc)\0*.doc\0"
                          "All Files (*.*)\0*.*\0",
                          "readme", TRUE);
        load_special_item(config, cur, "License File",
                          "Text Files (*.txt)\0*.txt\0"
                          "All Files (*.*)\0*.*\0",
                          "license", FALSE);
        load_special_item(config, cur, "Cover Art",
                          "All Images\0*.jpg;*.png\0"
                          "JPEG Images (*.jpg)\0*.jpg\0"
                          "PNG Images (*.png)\0*.png\0"
                          "All Files (*.*)\0*.*\0",
                          "cover art", FALSE);
        load_special_item(config, cur, "EXE File Icon",
                          "Icon Files (*.ico)\0*.ico\0"
                          "All Files (*.*)\0*.*\0",
                          "exe icon", FALSE);
        load_special_item(config, cur, "Char Map Library",
                          "T3 Resource Libraries (*.t3r)\0*.t3r\0"
                          "All Files (*.*)\0*.*\0",
                          "cmaplib", FALSE);
        load_special_item(config, cur, "Custom Web Builder",
                          "T3 Projects (*.t3m)\0*.t3m\0"
                          "All Files (*.*)\0*.*\0",
                          "web builder", FALSE);

        /*
         *   Feelies
         */

        /* fill in the "feelies" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "feelies",
                                  0, img_idx_resource_,
                                  FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_EXTERN,
                                  PROJ_ITEM_TYPE_FEELIE, FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Feelies",
                          TVIS_BOLD | TVIS_EXPANDED,
                          img_idx_folder_, info);

        /* remember the feelies group */
        feelies_item_ = cur;

        /* load the feelies files */
        load_config_files(config, cur);




        /*
         *   Web Extras
         */

        /* fill in the "feelies" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "web extras",
                                  0, img_idx_webextra_,
                                  FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_EXTERN,
                                  PROJ_ITEM_TYPE_WEBEXTRA, FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Web Page Extras",
                          TVIS_BOLD | TVIS_EXPANDED,
                          img_idx_folder_, info);

        /* load the feelies files */
        load_config_files(config, cur);


        /*
         *   Notes and Miscellaneous
         */

        /* fill in the "notes & misc" item */
        info = new proj_item_info(0, PROJ_ITEM_TYPE_FOLDER, "miscellaneous",
                                  0, img_idx_resource_,
                                  FALSE, TRUE, FALSE,
                                  PROJ_ITEM_OPEN_EXTERN,
                                  PROJ_ITEM_TYPE_MISC, FALSE);
        cur = insert_tree(proj_item, TVI_LAST, "Notes & Miscellaneous",
                          TVIS_BOLD | TVIS_EXPANDED,
                          img_idx_folder_, info);

        /* load the feelies files */
        load_config_files(config, cur);


        /* select the root item */
        TreeView_Select(tree_, proj_item, TVGN_CARET);
    }

    /* load the context menu */
    ctx_menu_container_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                                   MAKEINTRESOURCE(IDR_PROJ_POPUP_MENU));

    /* extract the context submenu from the menu container */
    ctx_menu_ = GetSubMenu(ctx_menu_container_, 0);

    /* load the special item menu */
    special_menu_ = LoadMenu(CTadsApp::get_app()->get_instance(),
                             MAKEINTRESOURCE(IDR_SPECIALFILE_MENU));

    /* set the initial scroll position to top left */
    SetScrollPos(tree_, SB_HORZ, 0, FALSE);
    SetScrollPos(tree_, SB_VERT, 0, TRUE);
}

/*
 *   Reset the project window - delete all sources, includes, etc
 */
void CHtmlDbgProjWin::reset_proj_win()
{
    HTREEITEM par;
    HTREEITEM cur;
    HTREEITEM nxt;
    const char *new_proj_name;

    /* delete all of the external resource files */
    for (cur = TreeView_GetNextSibling(tree_, TreeView_GetRoot(tree_)) ;
         cur != 0 ; cur = nxt)
    {
        /* remember the next item */
        nxt = TreeView_GetNextSibling(tree_, cur);

        /* delete this item */
        TreeView_DeleteItem(tree_, cur);
    }

    /* delete all children from the main project subgroups */
    for (par = TreeView_GetChild(tree_, TreeView_GetRoot(tree_)) ;
         par != 0 ; par = TreeView_GetNextSibling(tree_, par))
    {
        /* delete all children of this group */
        for (cur = TreeView_GetChild(tree_, par) ; cur != 0 ; cur = nxt)
        {
            /* remember the next item */
            nxt = TreeView_GetNextSibling(tree_, cur);

            /* delete this item */
            TreeView_DeleteItem(tree_, cur);
        }
    }

    /* rename the root item */
    if (dbgmain_->get_gamefile_name() != 0
        && dbgmain_->get_gamefile_name()[0] != '\0')
    {
        /* update the root item's name to show the new game file name */
        new_proj_name =
            os_get_root_name((char *)dbgmain_->get_gamefile_name());

        /*
         *   if we didn't have a project before (in which case the root
         *   item won't have any children), set up the default project
         *   items
         */
        if (TreeView_GetChild(tree_, TreeView_GetRoot(tree_)) == 0)
        {
            /* a project is now open */
            proj_open_ = TRUE;

            /* delete the old tree control */
            RevokeDragDrop(tree_);
            DestroyWindow(tree_);
            tree_ = 0;

            /* re-create the tree control */
            create_tree();
        }
    }
    else
    {
        /* no project is now open */
        proj_open_ = FALSE;

        /* forget the special folders - we're going to delete them */
        inc_item_ = 0;
        src_item_ = 0;
        res_item_ = 0;

        /* delete the old tree control */
        RevokeDragDrop(tree_);
        DestroyWindow(tree_);
        tree_ = 0;

        /* re-create the tree */
        create_tree();
    }
}

/*
 *   Load a Special File item from the configuration
 */
void CHtmlDbgProjWin::load_special_item(
    CHtmlDebugConfig *config, HTREEITEM parent, const char *title,
    const char *fname_filter, const char *subkey, int has_start_menu_title)
{
    proj_item_info *info;
    HTREEITEM chi;
    const textchar_t *fname;
    const textchar_t *smt = 0;
    const char *p;

    /* look up the current filename setting, if any */
    fname = config->getval_strptr("build special file", subkey, 0);

    /* check for a start menu title */
    if (has_start_menu_title)
        smt = config->getval_strptr(
            "build special file title", subkey, 0);

    /* if it's empty, treat it as no setting */
    if (fname != 0 && fname[0] == '\0')
        fname = 0;

    /* create the item info structure - leave the filename empty for now */
    info = new proj_item_info(
        0, PROJ_ITEM_TYPE_SPECIAL, 0, 0, 0, TRUE, FALSE, TRUE,
        PROJ_ITEM_OPEN_EXTERN, PROJ_ITEM_TYPE_NONE, FALSE);

    /* set the special item title */
    info->special_title_.set(title);
    info->special_subkey_.set(subkey);

    /* set the start menu title */
    info->start_menu_title_.set(smt);

    /* note whether or not it has a start menu title */
    info->has_start_menu_title_ = has_start_menu_title;

    /* find the actual length of the filter - find the double null byte */
    for (p = fname_filter + 1 ; *p != '\0' || *(p-1) != '\0' ; ++p) ;
    info->special_filter_.set(fname_filter, p - fname_filter);

    /* add it to the tree */
    chi = insert_tree(parent, TVI_LAST, "", 0, img_idx_resource_, info);

    /* set the special file */
    set_special_file(chi, fname, FALSE);
}

/*
 *   set the Start Menu title for a special item or feelie
 */
void CHtmlDbgProjWin::set_file_title(
    HTREEITEM chi, const char *title, int save)
{
    proj_item_info *info = get_item_info(chi);

    /* remember the new title */
    info->start_menu_title_.set(title);

    /* update the item's display name */
    update_item_dispname(chi, info);

    /* save if requested */
    if (save)
    {
        CHtmlDebugConfig *config = dbgmain_->get_local_config();
        size_t titlelen = (title != 0 ? get_strlen(title) : 0);
        const char *key;
        const char *subkey = info->fname_;

        /* determine the appropriate config keys */
        switch (info->type_)
        {
        case PROJ_ITEM_TYPE_SPECIAL:
            key = "build special file title";
            subkey = info->special_subkey_.get();
            break;

        case PROJ_ITEM_TYPE_FEELIE:
            key = "build feelie title";
            break;

        case PROJ_ITEM_TYPE_WEBEXTRA:
            key = "build web title";
            break;

        default:
            return;
        }

        /* set the config key */
        config->setval(key, subkey, 0, title, titlelen);

        /* save the changes */
        save_config(config);
    }
}

/*
 *   update an item's display name
 */
void CHtmlDbgProjWin::update_item_dispname(
    HTREEITEM item, proj_item_info *info)
{
    char dispname[OSFNMAX + 50 + 256];
    const textchar_t *fn = info->fname_;
    const textchar_t *st = (info->has_start_menu_title_
                            ? info->start_menu_title_.get() : 0);

    /* treat an empty title or filename as entirely absent */
    if (fn != 0 && fn[0] == '\0')
        fn = 0;
    if (st != 0 && st[0] == '\0')
        st = 0;

    /* the display format varies by type */
    switch (info->type_)
    {
    case PROJ_ITEM_TYPE_SPECIAL:
        /* special item format: "Title: File \"Start Menu\"" */
        if (fn != 0 && st != 0)
            _snprintf(dispname, sizeof(dispname), "%s: %s \"%s\"",
                      info->special_title_.get(), fn, st);
        else if (fn != 0)
            _snprintf(dispname, sizeof(dispname), "%s: %s",
                      info->special_title_.get(), fn, st);
        else
            _snprintf(dispname, sizeof(dispname),
                      "%s: (Not set - double-click to select)",
                      info->special_title_.get());
        break;

    case PROJ_ITEM_TYPE_FEELIE:
    case PROJ_ITEM_TYPE_WEBEXTRA:
        /* feelie/webextra format: "File \"Title\"" */
        if (st != 0)
            _snprintf(dispname, sizeof(dispname), "%s \"%s\"", fn, st);
        else
            safe_strcpy(dispname, sizeof(dispname), fn);
        break;

    default:
        /* for any other type, just display the base filename */
        safe_strcpy(dispname, sizeof(dispname), fn);
        break;
    }

    /* update the item title in the tree */
    TVITEM tvi;
    tvi.mask = TVIF_TEXT | TVIF_HANDLE;
    tvi.hItem = item;
    tvi.pszText = dispname;
    TreeView_SetItem(tree_, &tvi);
}

/*
 *   Set the title of a Special File item
 */
void CHtmlDbgProjWin::set_special_file(HTREEITEM chi, const char *fname,
                                       int save)
{
    proj_item_info *info = get_item_info(chi);
    textchar_t rel[OSFNMAX];

    /* if there's a filename, get its path relative to its parent */
    if (fname != 0)
    {
        textchar_t parent_path[OSFNMAX];

        /* get the item's path releative to the project folder */
        get_item_child_dir(parent_path, sizeof(parent_path),
                           TreeView_GetParent(tree_, chi));
        os_get_rel_path(rel, sizeof(rel), parent_path, fname);

        /* set the file */
        info->set_filename(rel);
    }
    else
    {
        /* clear the filename */
        info->set_filename(0);
        rel[0] = '\0';
    }

    /* update the display name */
    update_item_dispname(chi, info);

    /* we can delete the item only if it has an associated file */
    info->can_delete_ = (fname != 0);

    /* if saving the change, update the configuration */
    if (save)
    {
        CHtmlDebugConfig *config = dbgmain_->get_local_config();

        /* update the config variable for this special item */
        config->setval(
            "build special file", info->special_subkey_.get(), 0, rel);

        /* save the changes */
        save_config(config);
    }
}


/*
 *   Load a group of files from the configuration
 */
void CHtmlDbgProjWin::load_config_files(CHtmlDebugConfig *config,
                                        HTREEITEM parent)
{
    /* get information on the parent item */
    proj_item_info *parent_info = get_item_info(parent);

    /* create the configuration variable name for the sys/user flag */
    char sys_flag_var[128];
    sprintf(sys_flag_var, "%s-sys", parent_info->config_group_);

    /* get the number of files */
    int cnt = config->get_strlist_cnt("build", parent_info->config_group_);

    /* start adding at the start of the parent list */
    HTREEITEM after = TVI_FIRST;

    /* load the files */
    for (int i = 0 ; i < cnt ; ++i)
    {
        char fname[OSFNMAX];
        char sys_flag[40];
        char type_flag[40];
        HTREEITEM new_item;
        proj_item_info *new_item_info;
        proj_item_type_t new_type;
        const textchar_t *smt;

        /* we don't have any idea of the item type yet */
        new_type = PROJ_ITEM_TYPE_NONE;

        /* get this file */
        config->getval("build", parent_info->config_group_,
                       i, fname, sizeof(fname));

        /* read the system/user flag for the file */
        config->getval("build", sys_flag_var, i, sys_flag, sizeof(sys_flag));

        /* if this is the source file list, read the type */
        if (parent == src_item_
            && !config->getval("build", "source_file_types", i,
                               type_flag, sizeof(type_flag)))
        {
            /* we have an explicit type, so note it */
            new_type = (type_flag[0] == 'l'
                        ? PROJ_ITEM_TYPE_LIB
                        : PROJ_ITEM_TYPE_SOURCE);
        }

        /*
         *   if it's a system file, expand the name, which is stored in the
         *   file relative to the system directory, into the full absolute
         *   path
         */
        if (sys_flag[0] == 's' && parent_info->child_sys_path_ != 0)
        {
            char buf[OSFNMAX];

            /* build the full path to the system folder */
            os_build_full_path(buf, sizeof(buf),
                               parent_info->child_sys_path_, fname);

            /* use the fully-expanded version of the filename */
            strcpy(fname, buf);
        }
        else if (sys_flag[0] == 'l')
        {
            /*
             *   It's an include file that's found in a library's directory.
             *   Scan the libraries, looking for a library that has a file
             *   with this name in its directory.
             */
            add_lib_header_path(fname, sizeof(fname));
        }

        /* add it to the list */
        new_item = add_file_to_project(parent, after, fname, new_type, FALSE);

        /*
         *   if the new item is a library, load the exclusion list for the
         *   library
         */
        new_item_info = get_item_info(new_item);
        if (new_item_info != 0 && new_item_info->type_ == PROJ_ITEM_TYPE_LIB)
            load_config_lib_exclude(config, new_item, new_item_info);

        /* determine the file title key, if applicable */
        const char *key;
        switch (new_item_info->type_)
        {
        case PROJ_ITEM_TYPE_FEELIE:
            key = "build feelie title";
            break;

        case PROJ_ITEM_TYPE_WEBEXTRA:
            key = "build web title";
            break;

        default:
            key = 0;
            break;
        }

        if (key != 0
            && new_item_info->has_start_menu_title_
            && (smt = config->getval_strptr(key, fname, 0)) != 0
            && smt[0] != '\0')
        {
            /* set the title */
            set_file_title(new_item, smt, FALSE);
        }

        /* if we succeeded, add the next item after this one */
        if (new_item != 0)
            after = new_item;

        /* read the recursion setting, if applicable */
        if (strcmp(parent_info->config_group_, "image_resources") == 0)
        {
            char rbuf[5];

            /* read the recursion status (the default is 'recursive') */
            if (config->getval("build", "image_resources_recurse",
                               i, rbuf, sizeof(rbuf)))
                rbuf[0] = 'Y';

            /* note the status in the new item */
            new_item_info->recursive_ = (rbuf[0] == 'Y');
        }
    }
}

/*
 *   Scan the libraries, looking for a library that has a file with the given
 *   name in its directory.  If we find a library path containing this
 *   header, we'll expand the header file name to include the full library
 *   directory path prefix.
 */
void CHtmlDbgProjWin::add_lib_header_path(char *fname, size_t fname_size)
{
    /*
     *   scan the project source tree recursively, starting with the children
     *   of the root source item
     */
    add_lib_header_path_scan(src_item_, fname, fname_size);
}

/*
 *   Service routine for add_lib_header_path: scan recursively starting with
 *   the children of the given tree item.  Returns true if we find an item,
 *   so recursive callers will know that there's no need to look any further.
 */
int CHtmlDbgProjWin::add_lib_header_path_scan(HTREEITEM parent_item,
                                              char *fname, size_t fname_size)
{
    HTREEITEM cur;

    /* scan the children of the given item */
    for (cur = TreeView_GetChild(tree_, parent_item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *cur_info;
        char cur_fname[OSFNMAX];
        char *root;

        /* get this item's associated information */
        cur_info = get_item_info(cur);

        /* if this isn't a library, skip it */
        if (cur_info == 0 || cur_info->type_ != PROJ_ITEM_TYPE_LIB)
            continue;

        /* get the item's path */
        get_item_path(cur_fname, sizeof(cur_fname), cur);

        /* get the root filename */
        root = os_get_root_name(cur_fname);

        /* replace the library name with the new item's filename */
        strcpy(root, fname);

        /* if the file exists, take this as its path */
        if (!osfacc(cur_fname))
        {
            /* use the fully-expanded path for the filename */
            strcpy(fname, cur_fname);

            /* indicate success */
            return TRUE;
        }

        /*
         *   we didn't find the file here, but a library can contain other
         *   libraries, so search the children of this item for nested
         *   libraries
         */
        if (add_lib_header_path_scan(cur, fname, fname_size))
            return TRUE;
    }

    /* didn't find a match - indicate failure */
    return FALSE;
}

/*
 *   Load configuration information determining if an item is excluded from
 *   its library.  Sets the exclude_from_lib_ field in the given object
 *   based on the configuration information.
 */
void CHtmlDbgProjWin::load_config_lib_exclude(CHtmlDebugConfig *config,
                                              HTREEITEM item,
                                              proj_item_info *info)
{
    char var_name[OSFNMAX + 50];
    int cnt;
    int i;

    /* build the exclusion variable name for this library */
    sprintf(var_name, "lib_exclude:%s", info->fname_);

    /* get the number of excluded files */
    cnt = config->get_strlist_cnt("build", var_name);

    /* scan the excluded files and set the checkmarks accordingly */
    for (i = 0 ; i < cnt ; ++i)
    {
        char url[1024];

        /* get this item */
        config->getval("build", var_name, i, url, sizeof(url));

        /*
         *   scan children of the library for the item with this URL, and
         *   set its checkbox state to 'excluded'
         */
        load_config_lib_exclude_url(item, url);
    }
}

/*
 *   Scan children of the given item, and their children, for the given URL,
 *   and mark the given item as excluded by setting its checkbox state.
 */
int CHtmlDbgProjWin::load_config_lib_exclude_url(
    HTREEITEM parent, const char *url)
{
    HTREEITEM cur;

    /* scan the item's children */
    for (cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        char cur_url[1024];

        /* get this item's URL */
        get_item_lib_url(cur_url, sizeof(cur_url), cur);

        /* check it against the one we're looking for */
        if (stricmp(url, cur_url) == 0)
        {
            /* it matches - mark it as excluded */
            set_item_checkmark(cur, FALSE);

            /* remember that it's excluded */
            get_item_info(cur)->exclude_from_lib_ = TRUE;

            /* no need to look any further */
            return TRUE;
        }

        /* check this item's children */
        if (load_config_lib_exclude_url(cur, url))
            return TRUE;
    }

    /* didn't find it */
    return FALSE;
}

/*
 *   Save a group of files to the configuration
 */
void CHtmlDbgProjWin::save_config_files(CHtmlDebugConfig *config,
                                        HTREEITEM parent)
{
    /* get information on the parent item */
    proj_item_info *parent_info = get_item_info(parent);

    /* if this item has no config group, we don't save it in the usual way */
    if (parent_info->config_group_ == 0)
        return;

    /* clear any existing string list for this configuration group */
    config->clear_strlist("build", parent_info->config_group_);

    /* create the configuration variable name for the sys/user flag */
    char sys_flag_var[128];
    sprintf(sys_flag_var, "%s-sys", parent_info->config_group_);

    /* if it's "image_resources", clear the recursive flag as well */
    if (strcmp(parent_info->config_group_, "image_resources") == 0)
        config->clear_strlist("build", "image_resources_recurse");

    /* load the files */
    int i;
    HTREEITEM cur;
    for (i = 0, cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur), ++i)
    {
        proj_item_info *info;

        /* get this item's information */
        info = get_item_info(cur);

        /* save this item's name */
        config->setval("build", parent_info->config_group_, i, info->fname_);

        /*
         *   if this is the "source_files" group, add the file type to
         *   indicate whether it's a library or a source file
         */
        if (parent == src_item_)
            config->setval("build", "source_file_types", i,
                           info->type_ == PROJ_ITEM_TYPE_LIB
                           ? "library" : "source");

        /* if it's a resource file, save its recursion status */
        if (strcmp(parent_info->config_group_, "image_resources") == 0)
            config->setval("build", "image_resources_recurse", i,
                           info->recursive_ ? "Y" : "N");

        /* save the item's system/user status */
        config->setval("build", sys_flag_var, i,
                       info->is_sys_file_
                       ? "system"
                       : info->lib_path_ != 0 && !info->in_user_lib_
                       ? "lib"
                       : "user");

        /* if this is a library, save its exclusion information */
        if (info->type_ == PROJ_ITEM_TYPE_LIB)
            save_config_lib_excl(config, cur);
    }
}

/*
 *   Update the saved configuration information after making a change to a
 *   library exclusion setting.  We'll run through the exclusion list for
 *   the top-level library including the given item and update the
 *   configuration information for the excluded items.
 */
void CHtmlDbgProjWin::save_config_lib_excl(CHtmlDebugConfig *config,
                                           HTREEITEM lib)
{
    char var_name[OSFNMAX + 50];
    proj_item_info *lib_info;
    int idx;

    /* get the item's information */
    lib_info = get_item_info(lib);

    /* build the exclusion variable name for this library */
    sprintf(var_name, "lib_exclude:%s", lib_info->fname_);

    /* clear the old exclusion list for this top-level library */
    config->clear_strlist("build", var_name);

    /*
     *   recursively visit the children of this object and add an entry for
     *   each excluded item
     */
    idx = 0;
    save_config_lib_excl_children(config, lib, var_name, &idx);
}

/*
 *   Save library exclusion configuration information for the children of
 *   the given library.
 */
void CHtmlDbgProjWin::save_config_lib_excl_children(
    CHtmlDebugConfig *config, HTREEITEM lib, const char *var_name, int *idx)
{
    HTREEITEM cur;

    /* run through this item's children */
    for (cur = TreeView_GetChild(tree_, lib) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get this item's information; skip it if there's none */
        info = get_item_info(cur);
        if (info == 0)
            continue;

        /*
         *   If the item is a library, visit it recursively; otherwise, if
         *   this item is marked as excluded, add an item for it.  Note that
         *   sublibraries can never be excluded from their containing
         *   libraries, so these tests are mutually exclusive.
         */
        if (info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* scan the children of this library */
            save_config_lib_excl_children(config, cur, var_name, idx);
        }
        else if (info->exclude_from_lib_)
        {
            char url[1024];

            /* get this item's URL */
            get_item_lib_url(url, sizeof(url), cur);

            /* add it to the exclusion list for the library */
            config->setval("build", var_name, *idx, url);

            /* advance the variable array counter */
            ++(*idx);
        }
    }
}

/*
 *   Save the project configuration
 */
void CHtmlDbgProjWin::save_config(CHtmlDebugConfig *config)
{
    HTREEITEM cur;
    int extres_cnt;

    /*
     *   save the configuration information for each child of the first
     *   top-level item - the first item is the main project item, and its
     *   immediate children are the file groups (source, include,
     *   resource) within the main project
     */
    cur = TreeView_GetRoot(tree_);
    for (cur = TreeView_GetChild(tree_, cur) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* save this item's file list */
        save_config_files(config, cur);
    }

    /*
     *   now save each external resource file list - the external resource
     *   files are all at root level
     */
    for (extres_cnt = 0, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        proj_item_info *info;

        /* get the info on this item */
        info = get_item_info(cur);

        /* if this isn't an external resource file, skip it */
        if (info->type_ != PROJ_ITEM_TYPE_EXTRES)
            continue;

        /* save it */
        save_config_files(config, cur);

        /* save the in-installer status */
        config->setval("build", "ext_resfile_in_install", extres_cnt,
                       info->in_install_ ? "Y" : "N");

        /* count it */
        ++extres_cnt;
    }

    /* save the external resource file count */
    config->setval("build", "ext resfile cnt", extres_cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   enumerate all files in the project
 */
void CHtmlDbgProjWin::enum_all_files(IProjWinFileCallback *cb)
{
    /* start at the top of the tree, and recursively enumerate all files */
    enum_all_files(cb, TreeView_GetRoot(tree_));
}

void CHtmlDbgProjWin::enum_all_files(IProjWinFileCallback *cb,
                                     HTREEITEM cur)
{
    proj_item_info *info;
    HTREEITEM chi;

    /* get this item's information structure */
    if ((info = get_item_info(cur)) != 0)
    {
        char path[MAX_PATH];
        DWORD attr;

        /*
         *   enumerate it if it's any type of file - exclude only the project
         *   folders and special tree-structure (type 'none') items
         */
        switch (info->type_)
        {
        case PROJ_ITEM_TYPE_NONE:
        case PROJ_ITEM_TYPE_FOLDER:
            /* not a file - skip it */
            break;

        case PROJ_ITEM_TYPE_EXTRES:
            /*
             *   external resource files represent targets, not source files,
             *   so we don't need to include them
             */
            break;

        case PROJ_ITEM_TYPE_SPECIAL:
            /* include specials only when they're set */
            if (info->fname_ != 0)
                goto misc_item;
            break;

        case PROJ_ITEM_TYPE_PROJECT:
        case PROJ_ITEM_TYPE_SOURCE:
        case PROJ_ITEM_TYPE_LIB:
        case PROJ_ITEM_TYPE_HEADER:
        case PROJ_ITEM_TYPE_FEELIE:
        case PROJ_ITEM_TYPE_MISC:
        case PROJ_ITEM_TYPE_WEBEXTRA:
        case PROJ_ITEM_TYPE_RESOURCE:
        case PROJ_ITEM_TYPE_HTML:
        case PROJ_ITEM_TYPE_CSS:
        case PROJ_ITEM_TYPE_XML:
        case PROJ_ITEM_TYPE_JS:
        misc_item:
            /*
             *   These are all files within the project - include them.
             */

            /* get the full file path */
            get_item_path(path, sizeof(path), cur);

            /* send it to the callback */
            cb->file_cb(path, info->fname_, info->type_);

            /*
             *   If this is a directory, include the directory's contents,
             *   recursively descending into children if applicable.
             */
            attr = GetFileAttributes(path);
            if (attr != INVALID_FILE_ATTRIBUTES
                && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /*
                 *   Include the directory's contents.  Include directories
                 *   only if we're recursing into directories; skip the '.'
                 *   and '..' members in any case.
                 */
                enum_files_in_dir(cb, path, "", info->child_type_,
                                  info->recursive_, info->recursive_, FALSE);
            }

            /* done */
            break;
        }
    }

    /* recursively process this item's children */
    for (chi = TreeView_GetChild(tree_, cur) ; chi != 0 ;
         chi = TreeView_GetNextSibling(tree_, chi))
        enum_all_files(cb, chi);
}

/*
 *   Enumerate files in a directory, optionally recursing into subdirectories
 */
void CHtmlDbgProjWin::enum_files_in_dir(
    IProjWinFileCallback *cb,
    const textchar_t *dir, const textchar_t *reldir,
    proj_item_type_t child_type,
    int include_dirs, int recurse, int include_dotdirs)
{
    WIN32_FIND_DATA info;
    char path[MAX_PATH];
    char rel[MAX_PATH];
    HANDLE hf;

    /* build the query as "path\*" */
    os_build_full_path(path, sizeof(path), dir, "*");

    /* run the query */
    if ((hf = FindFirstFile(path, &info)) != INVALID_HANDLE_VALUE)
    {
        /* enumerate each file we find */
        do
        {
            /* build the full path to the file */
            os_build_full_path(path, sizeof(path), dir, info.cFileName);

            /* build the full *relative* path to the file */
            os_build_full_path(rel, sizeof(rel), reldir, info.cFileName);

            /* if it's a directory, and we're not recursing, skip it */
            if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /* check for the special directories '.' and '..' */
                int dotdir = (info.cFileName[0] == '.'
                              && (info.cFileName[1] == '\0'
                                  || (info.cFileName[1] == '.'
                                      && info.cFileName[2] == '\0')));

                /*
                 *   If we're including directories, enumerate it.  If it's a
                 *   'dot' directory (. or ..), include it only if the caller
                 *   explicitly wants those.
                 */
                if (include_dirs && (include_dotdirs || !dotdir))
                    cb->file_cb(path, rel, PROJ_ITEM_TYPE_FOLDER);

                /*
                 *   if we're recursing, recurse - unless it's one of the
                 *   'dot' directories, in which case we need to skip it
                 *   (otherwise we'd recurse forever in and out of self and
                 *   parent)
                 */
                if (recurse && !dotdir)
                    enum_files_in_dir(
                        cb, path, rel, child_type,
                        include_dirs, recurse, include_dotdirs);
            }
            else
            {
                /* it's an ordinary file - include it */
                cb->file_cb(path, rel, child_type);
            }
        }
        while (FindNextFile(hf, &info));

        /* done with the find */
        FindClose(hf);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Does the given filename have a text-file extension?
 */
static int is_text_ext(const char *fname)
{
    static const char *exts[] =
    {
        ".txt",
        ".htm",
        ".html",
        ".xml",
        ".log",
        ".cmd",
        ".t",
        ".tl",
        ".h",
        ".c",
        ".cpp",
        ".js",
        ".css",
        0
    };
    const char **extp;

    /* look for the last period in the filename */
    const char *ext = strrchr(fname, '.');

    /* if we didn't find one, there's no extensino to match */
    if (ext == 0)
        return FALSE;

    /* look for it in our extension list */
    for (extp = exts ; *extp != 0 ; ++extp)
    {
        /* if this one compares equal, we have a match */
        if (stricmp(ext, *extp) == 0)
            return TRUE;
    }

    /* no match */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   enumerate text files in the project
 */
void CHtmlDbgProjWin::enum_text_files(IProjWinFileCallback *cb)
{
    /* start at the top of the tree, and recursively enumerate all files */
    enum_text_files(cb, TreeView_GetRoot(tree_));
}

void CHtmlDbgProjWin::enum_text_files(IProjWinFileCallback *cb,
                                      HTREEITEM cur)
{
    proj_item_info *info;
    HTREEITEM chi;

    /* get this item's information structure */
    if ((info = get_item_info(cur)) != 0)
    {
        int is_text;

        /*
         *   If it's a source file, a header, or a .tl file, it's definitely
         *   a text file.  If it's a resource file, it could be text; guess
         *   based on the filename extension.  Otherwise, it's not a text
         *   file.
         */
        switch (info->type_)
        {
        case PROJ_ITEM_TYPE_SOURCE:
        case PROJ_ITEM_TYPE_HEADER:
        case PROJ_ITEM_TYPE_LIB:
            /* definitely text */
            is_text = TRUE;
            break;

        case PROJ_ITEM_TYPE_RESOURCE:
            /* it might be a text file, depending on the filename */
            is_text = is_text_ext(info->fname_);
            break;

        default:
            /* others aren't text files (or maybe even files at all) */
            is_text = FALSE;
            break;
        }

        /* if it's text, process it */
        if (is_text)
        {
            char path[MAX_PATH];

            /* get the full file path */
            get_item_path(path, sizeof(path), cur);

            /* send it to the callback */
            cb->file_cb(path, info->fname_, info->type_);
        }
    }

    /* recursively process this item's children */
    for (chi = TreeView_GetChild(tree_, cur) ; chi != 0 ;
         chi = TreeView_GetNextSibling(tree_, chi))
        enum_text_files(cb, chi);
}

/* ------------------------------------------------------------------------ */
/*
 *   drag-drop handler
 */

/*
 *   start dragging over this window
 */
HRESULT STDMETHODCALLTYPE CHtmlDbgProjWin::DragEnter(
    IDataObject __RPC_FAR *dataObj, DWORD grfKeyState, POINTL ptl,
    DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Enter(dataObj, grfKeyState, ptl, pdwEffect);

    /* assume we won't allow it */
    *pdwEffect = DROPEFFECT_NONE;

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
        *pdwEffect = DROPEFFECT_COPY;
    }

    /* begin our drag-drop tracking, if applicable */
    if (*pdwEffect != DROPEFFECT_NONE)
    {
        /* begin our drag/drop operation */
        enter_drag_drop(0);

        /* note where we're starting the operation */
        POINT pt;
        pt.x = ptl.x;
        pt.y = ptl.y;
        ScreenToClient(tree_, &pt);
        *pdwEffect = track_drag_drop(grfKeyState, pt.x, pt.y);
    }

    /*
     *   whether or not we handle the drop, we successfully responded to the
     *   drag-drop request
     */
    return S_OK;
}

/*
 *   continue dragging over this window
 */
HRESULT STDMETHODCALLTYPE CHtmlDbgProjWin::DragOver(
    DWORD grfKeyState, POINTL ptl, DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Over(grfKeyState, ptl, pdwEffect);

    /* by default, we won't allow dropping here */
    *pdwEffect = DROPEFFECT_NONE;

    /* if we're tracking the drag/drop, process the event */
    if (tracking_drag_)
    {
        /* track the drag */
        POINT pt;
        pt.x = ptl.x;
        pt.y = ptl.y;
        ScreenToClient(tree_, &pt);
        *pdwEffect = track_drag_drop(grfKeyState, pt.x, pt.y);
    }

    /* success */
    return S_OK;
}

/*
 *   leave this window with no drop having occurred
 */
HRESULT STDMETHODCALLTYPE CHtmlDbgProjWin::DragLeave()
{
    /* let the helper object do its work, if any */
    DropHelper_Leave();

    /* if we're tracking a drag/drop operation, cancel it */
    if (tracking_drag_)
        cancel_drag_drop();

    /* success */
    return S_OK;
}

/*
 *   drop in this window
 */
HRESULT STDMETHODCALLTYPE CHtmlDbgProjWin::Drop(
    IDataObject __RPC_FAR *dataObj, DWORD grfKeyState, POINTL ptl,
    DWORD __RPC_FAR *pdwEffect)
{
    /* let the helper object do its work, if any */
    DropHelper_Drop(dataObj, grfKeyState, ptl, pdwEffect);

    /* by default, don't allow dropping here */
    *pdwEffect = DROPEFFECT_NONE;

    /* if we're tracking the drag, execute the drop */
    if (tracking_drag_)
    {
        /* we're in 'copy' mode */
        *pdwEffect = DROPEFFECT_COPY;

        /* execute the drop */
        POINT pt;

        /* track the drag */
        pt.x = ptl.x;
        pt.y = ptl.y;
        ScreenToClient(tree_, &pt);
        execute_drag_drop(grfKeyState, pt.x, pt.y, dataObj);
    }

    /* handled */
    return S_OK;
}


/* ------------------------------------------------------------------------ */
/*
 *   Enter a drag/drop operation.  If 'item' is non-null, we're dragging the
 *   given tree item; otherwise, we're dragging something from an outside
 *   window over the tree.
 */
void CHtmlDbgProjWin::enter_drag_drop(HTREEITEM item)
{
    /* note that we're tracking a drag operation */
    tracking_drag_ = TRUE;

    /* note the event time, and set up a timer to monitor hovering */
    drag_time_ = GetTickCount();
    drag_pos_.x = drag_pos_.y = 0;
    drag_timer_id_ = alloc_timer_id();
    SetTimer(handle_, drag_timer_id_, 50, 0);

    /* remember the item being dragged */
    drag_item_ = item;
    drag_info_ = get_item_info(item);

    /* if we're dragging an item from the tree, set up for tracking it */
    if (item != 0)
    {
        /* capture input in the tree's parent window */
        SetCapture(handle_);

        /* select the item while it's being dragged */
        TreeView_SelectDropTarget(tree_, item);
    }
}

/*
 *   cancel a drag/drop operation
 */
void CHtmlDbgProjWin::cancel_drag_drop()
{
    /* remove any caret we've dragn */
    draw_dragmove_caret(0, FALSE);

    /* remove the drag item selection */
    TreeView_SelectDropTarget(tree_, 0);

    /*
     *   if we had capture, release it - we will have had capture if we were
     *   tracking a drag from one of our own items
     */
    if (drag_item_ != 0)
        ReleaseCapture();

    /* terminate our drag timer */
    KillTimer(handle_, drag_timer_id_);
    free_timer_id(drag_timer_id_);

    /* turn off tracking mode */
    tracking_drag_ = FALSE;

    /* forget the drag item */
    drag_item_ = 0;
    drag_info_ = 0;
}

/*
 *   execute a drop
 */
void CHtmlDbgProjWin::execute_drag_drop(
    int keys, int x, int y, IDataObject __RPC_FAR *dataObj)
{
    /* remove any caret we've dragn */
    draw_dragmove_caret(0, FALSE);

    /* remove the drag item selection */
    TreeView_SelectDropTarget(tree_, 0);

    /* get the target of the drop */
    HTREEITEM parent, insafter, caret;
    if (get_dragmove_target(x, y, &parent, &insafter, &caret)
        && dragmove_allowed(parent, insafter, caret, 0))
    {
        /* check to see if we're dragging a tree item or an external item */
        if (drag_item_ != 0)
        {
            /*
             *   dragging an item from our own tree - this simply moves the
             *   item to a new position in the tree
             */

            /* make sure nothing is selected before the drop */
            TreeView_SelectItem(tree_, 0);

            /* move the item */
            HTREEITEM new_item = move_tree_item(
                drag_item_, parent, insafter, caret);

            /* select the newly inserted item */
            TreeView_SelectItem(tree_, new_item);
        }
        else
        {
            /*
             *   dragging an external item
             */

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
                /* get the HDROP from the storage medium */
                HDROP hdrop = (HDROP)stg.hGlobal;

                /* do the drop */
                do_tree_dropfiles(hdrop, x, y);

                /* done with the data */
                ReleaseStgMedium(&stg);
            }
        }
    }

    /* cancel the drag tracking */
    cancel_drag_drop();
}

/*
 *   handle mouse movement during a drag-drop operation
 */
DROPEFFECT CHtmlDbgProjWin::track_drag_drop(int keys, int x, int y)
{
    /* determine which item we're pointing at */
    HTREEITEM parent, insafter, caret;
    get_dragmove_target(x, y, &parent, &insafter, &caret);

    /* determine if this is a valid drop destination */
    int sel;
    int ok = parent != 0 && dragmove_allowed(parent, insafter, caret, &sel);

    /* draw the drag-move caret at the target if the drop is okay */
    draw_dragmove_caret(ok ? caret : 0, sel);

    /* do some extra work if we're dragging our own tree item */
    if (drag_item_ != 0)
    {
        if (!sel)
        {
            /* select the item being dragged */
            TreeView_SelectDropTarget(tree_, drag_item_);

            /* show the appropriate cursor */
            SetCursor(ok ? move_csr_ : no_csr_);
        }
        else
        {
            /* show the link cursor */
            SetCursor(ok ? link_csr_ : no_csr_);
        }
    }

    /* note the event time */
    if (x != drag_pos_.x || y != drag_pos_.y)
    {
        drag_time_ = GetTickCount();
        drag_pos_.x = x;
        drag_pos_.y = y;
    }

    /* return the ok-to-drop-here indication */
    return (DROPEFFECT)
        (ok ? sel ? DROPEFFECT_LINK : DROPEFFECT_COPY : DROPEFFECT_NONE);
}

/*
 *   Draw a drag-move insertion caret.
 */
void CHtmlDbgProjWin::draw_dragmove_caret(HTREEITEM target, int sel)
{
    RECT rc;

    /* if this is the same caret as last time, there's nothing to do */
    if (target == last_drop_target_)
        return;

    /* remove the old caret by invalidating its area on the tree control */
    if (last_drop_target_ != 0)
    {
        /* get the preceding item's location */
        TreeView_GetItemRect(tree_, last_drop_target_, &rc, FALSE);

        /* adjust to the caret rectangle */
        SetRect(&rc, rc.left, rc.bottom - 2, rc.right, rc.bottom + 2);

        /* invalidate this area */
        InvalidateRect(tree_, &rc, TRUE);

        /* deselect it if applicable */
        if (last_drop_target_sel_)
            TreeView_SelectDropTarget(tree_, 0);
    }

    /* remember the new target item */
    last_drop_target_ = target;
    last_drop_target_sel_ = sel;

    /* add the new caret if we have a target and we're not selecting it */
    if (target != 0 && !sel)
    {
        HDC dc;
        RECT lrc;

        /*
         *   make sure the window is fully updated before we do any overwrite
         *   drawing
         */
        UpdateWindow(tree_);

        /* get the preceding item's location */
        TreeView_GetItemRect(tree_, target, &rc, FALSE);

        /* adjust to the caret rectangle */
        SetRect(&rc, rc.left, rc.bottom - 2, rc.right, rc.bottom + 2);

        /* get the device context for the tree control */
        dc = GetDC(tree_);

        /* draw a 2-pixel line across the width of the box */
        SetRect(&lrc, rc.left, rc.top + 1, rc.right, rc.top + 3);
        FillRect(dc, &lrc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        /* done with the device context */
        ReleaseDC(tree_, dc);
    }

    /* if we're selecting the new target, show it selected */
    if (target != 0 && sel)
        TreeView_SelectDropTarget(tree_, target);
}

/*
 *   Get the drag-move target.  This is the item immediately preceding the
 *   insertion point.
 */
int CHtmlDbgProjWin::get_dragmove_target(
    int x, int y, HTREEITEM *parent, HTREEITEM *insafter,
    HTREEITEM *caret) const
{
    TVHITTESTINFO hit;

    /* presume we won't find anything */
    *parent = *insafter = *caret = 0;

    /* determine which item we're pointing at */
    hit.pt.x = x;
    hit.pt.y = y;
    *caret = TreeView_HitTest(tree_, &hit);

    /* if we found a target item, figure the parent and insertion position */
    if (*caret != 0)
    {
        RECT rc;
        proj_item_info *info;

        /*
         *   if we're dropping onto an item that accepts children, insert at
         *   the start of the item's child list
         */
        if ((info = get_item_info(*caret)) != 0 && info->can_add_children_)
        {
            *parent = *caret;
            *insafter = TVI_FIRST;
            return TRUE;
        }

        /*
         *   if the target is a Special File item, allow dropping directly on
         *   it - we don't insert in this case, but rather drop onto the item
         */
        if (info != 0 && info->type_ == PROJ_ITEM_TYPE_SPECIAL)
        {
            *parent = TreeView_GetParent(tree_, *caret);
            *insafter = 0;
            return TRUE;
        }

        /*
         *   check to see if the parent accepts dropped files - if not, don't
         *   allow the drop here after all
         */
        *parent = TreeView_GetParent(tree_, *caret);
        if ((info = get_item_info(*parent)) == 0 || !info->can_add_children_)
        {
            *caret = *parent = 0;
            return FALSE;
        }

        /* get the target item's rectangle */
        TreeView_GetItemRect(tree_, *caret, &rc, FALSE);

        /*
         *   if the mouse y coordinate is less than the y coordinate of the
         *   vertical center of the item, we want to insert before this item;
         *   otherwise, we insert after it
         */
        if (y < rc.top + (rc.bottom - rc.top)/2)
        {
            /* insert before this item - get its previous sibling */
            *insafter = TreeView_GetPrevSibling(tree_, *caret);

            /*
             *   if the target item has no previous sibling, insert at the
             *   start of the parent list
             */
            if (*insafter == 0)
                *insafter = TVI_FIRST;

            /* draw the caret below the previous *visible* item */
            *caret = TreeView_GetPrevVisible(tree_, *caret);
        }
        else
        {
            /* insert after the target item */
            *insafter = *caret;
        }

        /*
         *   If the caret and the insert-after item are the same, and the
         *   caret item has visible children, don't allow the drop.
         *   Visually, it looks like we'd be dropping the dragged item into
         *   the child list of the caret item, when in fact we're dropping it
         *   as the next sibling of the caret item.  This is confusing, so
         *   just disallow it.
         */
        if (*caret != 0
            && *caret == *insafter
            && TreeView_GetNextVisible(tree_, *caret)
               == TreeView_GetChild(tree_, *caret))
        {
            *parent = *insafter = *caret = 0;
            return FALSE;
        }
    }

    /* return true if we found a target */
    return (*caret != 0);
}

/*
 *   Determine if the given drag-move target is allowed
 */
int CHtmlDbgProjWin::dragmove_allowed(
    HTREEITEM parent, HTREEITEM insafter, HTREEITEM caret, int *sel) const
{
    proj_item_info *info;

    /* presume we won't need to select the target item */
    if (sel != 0)
        *sel = 0;

    /* we can drop onto special items */
    if (caret != 0
        && (info = get_item_info(caret)) != 0
        && info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* we drop directly onto these items */
        if (sel != 0)
            *sel = TRUE;

        /* if we're dragging our own tree item, validate further */
        if (drag_item_ != 0)
        {
            /* only allow resource-like items with these drops */
            if ((info = get_item_info(drag_item_)) != 0)
            {
                /* check the type */
                switch (info->type_)
                {
                case PROJ_ITEM_TYPE_RESOURCE:
                case PROJ_ITEM_TYPE_FEELIE:
                case PROJ_ITEM_TYPE_MISC:
                case PROJ_ITEM_TYPE_WEBEXTRA:
                    /*
                     *   these types are allowed, as long as the underlying
                     *   file isn't a folder
                     */
                    if (info->fname_ != 0)
                    {
                        DWORD attr = GetFileAttributes(info->fname_);
                        if (attr != INVALID_FILE_ATTRIBUTES
                            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                            return FALSE;
                    }
                    break;

                default:
                    /* other types are not allowed */
                    return FALSE;
                }
            }
        }

        /* allow it */
        return TRUE;
    }

    /* check to see if we're dragging a tree item or an outside item */
    if (drag_item_ != 0)
    {
        /*
         *   Dragging an item from our own tree.
         *
         *   If we're attempting to insert the item where it started, don't
         *   allow the operation, since it would do nothing.  The item will
         *   end where it started if the insert-after item is the item
         *   immediately preceding the item, or it's the item itself.
         *
         *   Note that if the item being dragged has no previous sibling,
         *   then the effective drop target to insert just before the item is
         *   the item's parent.
         */
        if (parent != 0
            && (insafter == drag_item_
                || (insafter == TVI_FIRST
                    && TreeView_GetChild(tree_, parent) == drag_item_)
                || (insafter != TVI_FIRST
                    && TreeView_GetNextSibling(tree_, insafter)
                    == drag_item_)))
        {
            /* it's not a meaningful target */
            return FALSE;
        }

        /*
         *   If we're dragging an item from one parent to another, validate
         *   the group change.  Only certain group changes are allowed.
         */
        if (parent != 0 && parent != TreeView_GetParent(tree_, drag_item_))
        {
            /* get the item info and the new parent info */
            info = get_item_info(drag_item_);
            proj_item_info *parinfo = get_item_info(parent);

            /* check where we're coming from */
            if (info != 0)
            {
                switch (info->type_)
                {
                case PROJ_ITEM_TYPE_RESOURCE:
                case PROJ_ITEM_TYPE_FEELIE:
                case PROJ_ITEM_TYPE_MISC:
                case PROJ_ITEM_TYPE_WEBEXTRA:
                    /*
                     *   we can move these non-folder items freely among the
                     *   Resource, Feelies, Notes & Misc, and Web Extra
                     *   groups
                     */
                    if (info->fname_ != 0)
                    {
                        DWORD attr = GetFileAttributes(info->fname_);
                        if (attr != INVALID_FILE_ATTRIBUTES
                            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                            return FALSE;
                    }

                    /* check the destination type */
                    switch (parinfo->child_type_)
                    {
                    case PROJ_ITEM_TYPE_RESOURCE:
                    case PROJ_ITEM_TYPE_FEELIE:
                    case PROJ_ITEM_TYPE_MISC:
                    case PROJ_ITEM_TYPE_WEBEXTRA:
                        /* moving into these groups is allowed */
                        break;

                    default:
                        /* other types can't be exchanged */
                        return FALSE;
                    }
                    break;

                default:
                    /* other types can't be moved to other groups */
                    return FALSE;
                }
            }
        }

        /* otherwise, it's a valid target */
        return TRUE;
    }
    else
    {
        /*
         *   We're dragging an external item (i.e., an item from another
         *   window or another application).  We can only drop external items
         *   in certain places: either as children of parents that allow
         *   adding new items, or onto special items.
         */

        /* if the parent allows adding children, allow it */
        if (parent != 0
            && (info = get_item_info(parent)) != 0
            && info->can_add_children_)
            return TRUE;

        /* otherwise, it's not a valid drop target */
        return FALSE;
    }
}

/*
 *   process a drop-files event in the tree control
 */
void CHtmlDbgProjWin::do_tree_dropfiles(HDROP hdrop, int x, int y)
{
    int cnt;
    int i;
    proj_item_info *info;
    char fname[OSFNMAX];

    /* get the root item for easy reference */
    HTREEITEM root = TreeView_GetRoot(tree_);

    /* find the drop target */
    HTREEITEM parent, insafter, caret;
    if (!get_dragmove_target(x, y, &parent, &insafter, &caret))
        parent = insafter = caret = 0;

    /* check for a special-item target */
    if (caret != 0
        && (info = get_item_info(caret)) != 0
        && info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* make sure we're dropping one ordinary file */
        if (DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0) == 1)
        {
            /* get the first item */
            DragQueryFile(hdrop, 0, fname, sizeof(fname));

            /* get its attributes */
            DWORD attr = GetFileAttributes(fname);

            /* if it's a file, set it as the new file for this special item */
            if (attr != INVALID_FILE_ATTRIBUTES
                && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                /* set the special item to this file */
                set_special_file(caret, fname, TRUE);

                /* done */
                return;
            }
        }

        /* tell them that this drop isn't allowed */
        dbgmain_->bring_to_front();
        MessageBox(dbgmain_->get_handle(),
                   "You can't drop a folder, or more than one file, "
                   "onto this Special File item.", "TADS Workbench",
                   MB_OK | MB_ICONEXCLAMATION);

        /* done */
        return;
    }

    /* if we found a valid parent, insert the file */
    if (parent != 0)
    {
        /* iterate over each dropped file */
        cnt = DragQueryFile(hdrop, 0xFFFFFFFF, 0, 0);
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this file's name */
            DragQueryFile(hdrop, i, fname, sizeof(fname));

            /* add the file */
            HTREEITEM new_item = add_file_to_project(
                parent, insafter, fname, PROJ_ITEM_TYPE_NONE, TRUE);

            /*
             *   if we successfully added the item, add the next file (if
             *   any) after this item
             */
            if (new_item != 0)
                insafter = new_item;
        }

        /* save the configuration changes */
        save_config(dbgmain_->get_local_config());
    }
    else
    {
        /*
         *   bring the debugger to the foreground (the drag source window is
         *   probably the foreground window at the moment)
         */
        dbgmain_->bring_to_front();

        /* show a message explaining the problem */
        MessageBox(dbgmain_->get_handle(),
                   "You can't drop files into this part of the project "
                   "window.  If you want to add a source "
                   "file, drop it onto the \"Source Files\" section.  If "
                   "you want to add an image or sound file, drop it onto the "
                   "\"Resource Files\" section.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Process window destruction
 */
void CHtmlDbgProjWin::do_destroy()
{
    /* unregister our drag-drop handler for the tree view */
    RevokeDragDrop(tree_);

    /* destroy our context menus */
    if (ctx_menu_container_ != 0)
        DestroyMenu(ctx_menu_container_);
    if (special_menu_ != 0)
        DestroyMenu(special_menu_);

    /*
     *   delete each item in the tree explicitly, so that we free the
     *   associated memory
     */
    while (TreeView_GetRoot(tree_) != 0)
        TreeView_DeleteItem(tree_, TreeView_GetRoot(tree_));

    /* tell the debugger to clear my window if I'm the active one */
    dbgmain_->on_close_dbg_win(this);

    /* release our reference on the main window */
    dbgmain_->Release();

    /* delete our image lists */
    TreeView_SetImageList(tree_, 0, TVSIL_NORMAL);
    TreeView_SetImageList(tree_, 0, TVSIL_STATE);
    ImageList_Destroy(img_list_);
    ImageList_Destroy(state_img_list_);

    /* inherit default */
    CTadsWin::do_destroy();
}

/*
 *   Load my window configuration
 */
void CHtmlDbgProjWin::load_win_config(CHtmlDebugConfig *config,
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
void CHtmlDbgProjWin::save_win_config(CHtmlDebugConfig *config,
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
 *   Insert an item into the tree control
 */
HTREEITEM CHtmlDbgProjWin::insert_tree(HTREEITEM parent, HTREEITEM ins_after,
                                       const textchar_t *txt, DWORD state,
                                       int icon_index, proj_item_info *info)
{
    TV_INSERTSTRUCT tvi;
    HTREEITEM item;
    proj_item_info *parent_info;

    /* build the insertion record */
    tvi.hParent = parent;
    tvi.hInsertAfter = ins_after;
    tvi.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM
                    | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.item.state = state;
    tvi.item.stateMask = 0xffffffff;
    tvi.item.pszText = (char *)txt;
    tvi.item.iImage = tvi.item.iSelectedImage = icon_index;
    tvi.item.lParam = (LPARAM)info;

    /* insert it into the tree */
    item = TreeView_InsertItem(tree_, &tvi);

    /* get the parent information  */
    parent_info = get_item_info(parent);

    /*
     *   make sure the parent is expanded, unless the parent is a library
     *   (we leave libraries closed initially, because many users wish to
     *   treat them as opaque and thus don't wish to see their contents)
     */
    if (parent != TVI_ROOT
        && (parent_info == 0 || parent_info->type_ != PROJ_ITEM_TYPE_LIB))
    {
        /* expand the parent */
        TreeView_Expand(tree_, parent, TVE_EXPAND);
    }

    /* select the newly-inserted item */
    TreeView_SelectItem(tree_, item);

    /* return the new item handle */
    return item;
}

/*
 *   remove a project file
 */
void CHtmlDbgProjWin::remove_proj_file(const char *fname)
{
    /* find the item */
    HTREEITEM item = find_item_for_file(TreeView_GetRoot(tree_), fname);
    proj_item_info *info = get_item_info(item);

    /* if we didn't find it, there's nothing to do */
    if (item == 0 || info == 0)
        return;

    /* if it's a special item, simply unset the special item */
    if (info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* clear the special file */
        set_special_file(item, 0, TRUE);
        return;
    }

    /* delete the item */
    TreeView_DeleteItem(tree_, item);

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}

/*
 *   delete the current tree selection
 */
void CHtmlDbgProjWin::delete_tree_selection()
{
    HTREEITEM sel;
    proj_item_info *info;
    int is_extres;

    /* get the selected item in the tree control */
    sel = TreeView_GetSelection(tree_);

    /* get the associated information */
    info = get_item_info(sel);

    /* if the item is marked as non-deletable, ignore the request */
    if (info == 0 || !info->can_delete_)
        return;

    /*
     *   if it's a special item, we don't actually want to delete the tree
     *   item; we simply want to clear out its file
     */
    if (info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* clear the special file */
        set_special_file(sel, 0, TRUE);

        /* that's all */
        return;
    }

    /* note if it's an external resource file */
    is_extres = (info->type_ == PROJ_ITEM_TYPE_EXTRES);

    /*
     *   if this is an external resource item with children, confirm the
     *   deletion
     */
    if (is_extres && TreeView_GetChild(tree_, sel) != 0)
    {
        int resp;

        /* ask for confirmation */
        resp = MessageBox(dbgmain_->get_handle(),
                          "Are you sure you want to remove this external "
                          "file and all of the resources it contains "
                          "from the project?", "TADS Workbench",
                          MB_YESNO | MB_ICONQUESTION);

        /* if they didn't say yes, abort */
        if (resp != IDYES)
            return;
    }

    /* delete it */
    TreeView_DeleteItem(tree_, sel);

    /*
     *   if we just deleted an external resource file, renumber the
     *   remaining external resources
     */
    if (is_extres)
        fix_up_extres_numbers();

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}


/*
 *   gain focus
 */
void CHtmlDbgProjWin::do_setfocus(HWND /*prev_win*/)
{
    /* give focus to our tree control */
    SetFocus(tree_);
}

/*
 *   resize the window
 */
void CHtmlDbgProjWin::do_resize(int mode, int x, int y)
{
    RECT rc;

    switch(mode)
    {
    case SIZE_MAXHIDE:
    case SIZE_MAXSHOW:
    case SIZE_MINIMIZED:
        /* don't bother resizing the subwindows on these changes */
        break;

    case SIZE_MAXIMIZED:
    case SIZE_RESTORED:
    default:
        /* resize the tree control to fill our client area */
        GetClientRect(handle_, &rc);
        MoveWindow(tree_, 0, 0, rc.right, rc.bottom, TRUE);
        break;
    }

    /* inherit default handling */
    CTadsWin::do_resize(mode, x, y);
}

/*
 *   Activate/deactivate.  Notify the debugger main window of the coming
 *   or going of this debugger window as the active debugger window, which
 *   will allow us to receive certain command messages from the main
 *   window.
 */
int CHtmlDbgProjWin::do_ncactivate(int flag)
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
void CHtmlDbgProjWin::do_parent_activate(int flag)
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
    CTadsWin::do_parent_activate(flag);
}

/*
 *   check a command's status
 */
TadsCmdStat_t CHtmlDbgProjWin::check_command(const check_cmd_info *info)
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
int CHtmlDbgProjWin::do_command(int notify_code, int command_id, HWND ctl)
{
    /* try handling it using the active-window mechanism */
    if (active_do_command(notify_code, command_id, ctl))
        return TRUE;

    /* let the debugger main window handle it */
    return dbgmain_->do_command(notify_code, command_id, ctl);
}

/*
 *   Check the status of a command routed from the main window
 */
TadsCmdStat_t CHtmlDbgProjWin::active_check_command(
    const check_cmd_info *info)
{
    HTREEITEM sel;
    HTREEITEM parent;
    proj_item_info *sel_info;
    proj_item_info *par_info;

    /* get the current selection and its associated information */
    sel = TreeView_GetSelection(tree_);
    sel_info = get_item_info(sel);

    /* get the parent of the selection as well */
    if (sel != 0)
    {
        parent = TreeView_GetParent(tree_, sel);
        par_info = get_item_info(parent);
    }
    else
    {
        parent = 0;
        par_info = 0;
    }

    /* check the command */
    switch(info->command_id)
    {
    case ID_PROJ_OPEN:
        /* enable it if the selection is openable */
        if (sel_info != 0 && sel_info->openable_ && sel_info->fname_ != 0)
        {
            /* set the filename in the menu item and enable the menu */
            check_command_change_menu(info, IDS_TPL_PROJOPEN_FNAME,
                                      os_get_root_name(sel_info->fname_));
            return TADSCMD_ENABLED;
        }
        else
        {
            /* set the blank menu name and disable it */
            check_command_change_menu(info, IDS_TPL_PROJOPEN_BLANK);
            return TADSCMD_DISABLED;
        }

    case ID_FILE_EDIT_TEXT:
        /*
         *   enable it if the selection is openable, and its open action
         *   is "open as source"
         */
        if (sel_info != 0 && sel_info->openable_ && sel_info->fname_ != 0
            && par_info->child_open_action_ == PROJ_ITEM_OPEN_SRC)
        {
            /* set the filename in the menu item and enable the menu */
            check_command_change_menu(info, IDS_TPL_EDITTEXT_FNAME,
                                      os_get_root_name(sel_info->fname_));
            return TADSCMD_ENABLED;
        }
        else
        {
            /* set the blank menu name and disable it */
            check_command_change_menu(info, IDS_TPL_EDITTEXT_BLANK);
            return TADSCMD_DISABLED;
        }

    case ID_PROJ_ADDCURFILE:
        /* set the menu title to indicate the filename (or not) */
        if (sel_info != 0 && sel_info->fname_ != 0)
            check_command_change_menu(info, IDS_TPL_PROJADD_FNAME,
                                      os_get_root_name(sel_info->fname_));
        else
            check_command_change_menu(info, IDS_TPL_PROJADD_BLANK);

        /* the current file is always already in the project */
        return TADSCMD_DISABLED;

    case ID_PROJ_REMOVE:
        /* set the menu title to indicate the filename (or not) */
        if (sel_info != 0 && sel_info->fname_ != 0)
            check_command_change_menu(info, IDS_TPL_PROJREM_FNAME,
                                      os_get_root_name(sel_info->fname_));
        else
            check_command_change_menu(info, IDS_TPL_PROJREM_BLANK);

        /* enable it if the item is marked as deletable */
        return (sel_info != 0 && sel_info->can_delete_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROJ_SETSPECIAL:
        /* this is valid only when a special item is selected */
        return (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_SPECIAL
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROJ_SETFILETITLE:
        /* this is valid only if the item is so marked */
        return (sel_info != 0 && sel_info->has_start_menu_title_
                ? TADSCMD_ENABLED : TADSCMD_DISABLED);

    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDDIR:
        /* always enable these, as long as a project is open */
        return proj_open_ ? TADSCMD_ENABLED : TADSCMD_DISABLED;

    case ID_PROJ_INCLUDE_IN_INSTALL:
        /*
         *   if it's an external resource file, enable the command and set
         *   the appropriate checkmark state; otherwise disable the
         *   command
         */
        if (sel_info != 0)
        {
            switch (sel_info->type_)
            {
            case PROJ_ITEM_TYPE_EXTRES:
                /*
                 *   external resource file - these can be included or not,
                 *   as desired
                 */
                return (sel_info->in_install_
                        ? TADSCMD_CHECKED : TADSCMD_ENABLED);

            case PROJ_ITEM_TYPE_SPECIAL:
            case PROJ_ITEM_TYPE_FEELIE:
            case PROJ_ITEM_TYPE_RESOURCE:
                /*
                 *   specials, feelies, and resources are always included in
                 *   the normal release ZIP and install set
                 */
                return TADSCMD_DISABLED_CHECKED;

            default:
                break;
            }
        }

        /* for anything else, it's not part of the setup/zip */
        return TADSCMD_DISABLED;
    }

    /* we don't currently handle any commands */
    return TADSCMD_UNKNOWN;
}

/*
 *   execute a command routed from the main window
 */
int CHtmlDbgProjWin::active_do_command(int notify_code, int command_id,
                                       HWND ctl)
{
    HTREEITEM sel;

    /* get the current selection */
    sel = TreeView_GetSelection(tree_);

    /* check the command */
    switch(command_id)
    {
    case ID_PROJ_OPEN:
        /* open the selection */
        open_item(sel);
        return TRUE;

    case ID_FILE_EDIT_TEXT:
        /* open the selection in a text editor */
        open_item_in_text_editor(sel);
        return TRUE;

    case ID_PROJ_REMOVE:
        /* remove the selected item */
        delete_tree_selection();
        return TRUE;

    case ID_PROJ_SETSPECIAL:
        ask_special_file(sel);
        return TRUE;

    case ID_PROJ_SETFILETITLE:
        ask_file_title(sel);
        return TRUE;

    case ID_PROJ_ADDFILE:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add a file */
        add_project_file();

        /* handled */
        return TRUE;

    case ID_PROJ_ADDDIR:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add a diretory */
        add_project_dir();

        /* handled */
        return TRUE;

    case ID_PROJ_ADDEXTRES:
        /* if no project is open, ignore it */
        if (!proj_open_)
            return TRUE;

        /* go add an external resource file */
        add_ext_resfile();

        /* handled */
        return TRUE;

    case ID_PROJ_INCLUDE_IN_INSTALL:
        /* make sure a project is open and they clicked on some item */
        if (proj_open_ && sel != 0)
        {
            proj_item_info *sel_info;

            /* get the information for this item */
            sel_info = get_item_info(sel);

            /* make sure it's an external resource */
            if (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_EXTRES)
            {
                /* reverse the in-install state */
                sel_info->in_install_ = !sel_info->in_install_;

                /* update the visual state */
                set_extres_in_install(sel, sel_info->in_install_);

                /* save the configuration change */
                save_config(dbgmain_->get_local_config());
            }
        }

        /* handled */
        return TRUE;
    }

    /* not handled */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Start Menu Title dialog
 */
class StartMenuDlg: public CTadsDialog
{
public:
    StartMenuDlg(proj_item_info *info)
    {
        textchar_t *p;

        /*
         *   Remember the title.  If there isn't already a title, synthesize
         *   one from the filename.
         */
        if ((p = info->start_menu_title_.get()) != 0 && p[0] != '\0')
        {
            /* we already have a start menu title */
            safe_strcpy(title, sizeof(title), p);
            in_menu = TRUE;
        }
        else if ((p = info->fname_) != 0 && p[0] != '\0')
        {
            /* by default, use the base name, minus extension */
            safe_strcpy(title, sizeof(title), os_get_root_name(p));
            if ((p = strrchr(title, '.')) != 0)
                *p = '\0';

            /* uppercase the first letter of each word */
            for (p = title ; *p != '\0' ; ++p)
            {
                if ((p == title || isspace(*(p-1))) && islower(*p))
                    *p = (textchar_t)toupper(*p);
            }

            /* we're not in the start menu yet */
            in_menu = FALSE;
        }
        else if ((p = info->special_title_.get()) != 0 && p[0] != '\0')
        {
            size_t len;

            /* use the title, minus any "File" suffix */
            safe_strcpy(title, sizeof(title), p);
            if ((len = strlen(title)) > 5
                && strcmp(title + len - 5, " File") == 0)
                title[len - 5] = '\0';

            /* not in the menu yet */
            in_menu = FALSE;
        }
        else
        {
            /* there's no basis for an initial title */
            title[0] = '\0';
            in_menu = FALSE;
        }

        /*
         *   If the item is a Feelie, it's optional whether or not it appears
         *   in the Start menu.  There's no option for special files.
         */
        in_menu_is_optional = (info->type_ != PROJ_ITEM_TYPE_SPECIAL);
        if (!in_menu_is_optional)
            in_menu = TRUE;
    }

    void init()
    {
        /* inherit default handling */
        CTadsDialog::init();

        /* set the title */
        SetDlgItemText(handle_, IDC_FLD_STARTMENU, title);

        /* check the appropriate radio button for the Start Menu status */
        CheckRadioButton(handle_, IDC_RB_NOSTARTMENU, IDC_RB_STARTMENU,
                         in_menu ? IDC_RB_STARTMENU : IDC_RB_NOSTARTMENU);

        /*
         *   if Start Menu inclusion is mandatory, disable the "not in start
         *   menu" option
         */
        if (!in_menu_is_optional)
            EnableWindow(GetDlgItem(handle_, IDC_RB_NOSTARTMENU), FALSE);
    }

    int do_command(WPARAM id, HWND ctl)
    {
        switch (LOWORD(id))
        {
        case IDOK:
            /* save the title */
            GetDlgItemText(handle_, IDC_FLD_STARTMENU, title, sizeof(title));

            /* note the radio button status */
            if (GetDlgItem(handle_, IDC_RB_STARTMENU) != 0)
            {
                /* the button exists - get its status */
                in_menu = (IsDlgButtonChecked(handle_, IDC_RB_STARTMENU)
                           == BST_CHECKED);
            }
            else
            {
                /*
                 *   the button doesn't exist in this dialog, so it's always
                 *   activated by default
                 */
                in_menu = TRUE;
            }
            break;

        case IDC_FLD_STARTMENU:
            /* on any text change, select the in-start-menu radio button */
            if (HIWORD(id) == EN_CHANGE)
                CheckRadioButton(
                    handle_, IDC_RB_NOSTARTMENU, IDC_RB_STARTMENU,
                    IDC_RB_STARTMENU);
            break;
        }

        /* inherit the default */
        return CTadsDialog::do_command(id, ctl);
    }

    /* our current title setting */
    textchar_t title[256];

    /* are we in the start menu? */
    int in_menu;

    /* are we allowed *not* to be in the start menu? */
    int in_menu_is_optional;
};

/*
 *   Ask for the start menu or web page link title for an item
 */
void CHtmlDbgProjWin::ask_file_title(HTREEITEM sel)
{
    proj_item_info *info = get_item_info(sel);

    /* make sure the item has a start menu title */
    if (info == 0 || !info->has_start_menu_title_)
        return;

    /* find the appropriate dialog */
    int dlgid;
    switch (info->type_)
    {
    case PROJ_ITEM_TYPE_FEELIE:
    case PROJ_ITEM_TYPE_SPECIAL:
        /* for feelies and special files, it's the Start Menu title */
        dlgid = DLG_STARTMENUTITLE;
        break;

    case PROJ_ITEM_TYPE_WEBEXTRA:
        /* for web extras, its the web link title */
        dlgid = DLG_WEBTITLE;
        break;

    default:
        /* others shouldn't have titles */
        return;
    }

    /* run the dialog */
    StartMenuDlg dlg(info);
    if (dlg.run_modal(dlgid, dbgmain_->get_handle(),
                      CTadsApp::get_app()->get_instance()) == IDOK)
    {
        /* set the title */
        set_file_title(sel, dlg.in_menu ? dlg.title : 0, TRUE);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Ask for and set a special file
 */
void CHtmlDbgProjWin::ask_special_file(HTREEITEM sel)
{
    OPENFILENAME5 info;
    char fname[OSFNMAX];
    proj_item_info *selinfo = get_item_info(sel);

    /* set up the file dialog request */
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                 | OFN_NOCHANGEDIR | OFN_HIDEREADONLY
                 | OFN_EXPLORER | OFN_ENABLESIZING;
    info.lpstrTitle = selinfo->special_title_.get();
    info.lpstrFilter = selinfo->special_filter_.get();
    info.lpstrDefExt = 0;
    info.hwndOwner = dbgmain_->get_handle();
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    fname[0] = '\0';
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
    if (!GetOpenFileName((OPENFILENAME *)&info))
        return;

    /* update the file in the selected item */
    set_special_file(sel, fname, TRUE);

    /* set the path for future open-file dialog calls */
    CTadsApp::get_app()->set_openfile_path(fname);
}

/* message for attempting to change a library file */
static const char *no_lib_change_msg =
    "You cannot change the contents of a library "
    "in the Project window.  If you want to add a file "
    "to this library, you must edit the library's definition "
    "file using the \"Open File in Text Editor\" command "
    "on the \"Edit\" menu.";

/*
 *   Add a project file
 */
void CHtmlDbgProjWin::add_project_file()
{
    HTREEITEM cont;
    HTREEITEM sel;
    proj_item_info *cont_info;
    proj_item_info *sel_info;
    OPENFILENAME5 info;
    char fname[1024];
    char *p;

    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /* get the current selection */
    sel = TreeView_GetSelection(tree_);
    sel_info = get_item_info(sel);

    /* if the selected item is a special item, set it */
    if (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* treat this as "ask for special" */
        ask_special_file(sel);
        return;
    }

    /*
     *   First, determine which container is selected.  If a child is
     *   selected, insert into the child's container.
     */
    for (cont = sel ; cont != 0 ; cont = TreeView_GetParent(tree_, cont))
    {
        /* get this item's information */
        cont_info = get_item_info(cont);

        /*
         *   if this item allows children, or it's a library, it's the
         *   container
         */
        if (cont_info != 0
            && (cont_info->can_add_children_
                || cont_info->type_ == PROJ_ITEM_TYPE_LIB))
            break;
    }

    /* if we didn't find a valid container, complain and give up */
    if (cont == 0)
    {
        /* explain the problem */
        MessageBox(dbgmain_->get_handle(),
                   "Before adding a new file, please select the part "
                   "of the project that will contain the file.  Select "
                   "\"Source Files\" to add a source file, or "
                   "\"Resource Files\" to add an image or sound file.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /*
         *   make sure the project window is the active window (we need to
         *   wait until after the message box is dismissed, to ensure we
         *   leave the project window selected if it's docked; we also did
         *   this before the message box to ensure the project window is
         *   visible while the message box is being displayed)
         */
        SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* stop now - the user must select something and try again */
        return;
    }

    /* if the container is a library, complain and give up */
    if ((cont_info != 0 && cont_info->type_ == PROJ_ITEM_TYPE_LIB)
        || (sel_info != 0 && sel_info->type_ == PROJ_ITEM_TYPE_LIB))
    {
        /* explain the problem */
        MessageBox(dbgmain_->get_handle(), no_lib_change_msg,
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* select the library in the project window */
        TreeView_SelectItem(tree_, cont);

        /* activate the project window */
        SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* done */
        return;
    }

    /* set the default file selector options */
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
                 | OFN_NOCHANGEDIR | OFN_HIDEREADONLY
                 | OFN_ALLOWMULTISELECT | OFN_EXPLORER
                 | OFN_ENABLESIZING;

    /* set up the dialog to ask for the appropriate type of file */
    switch(cont_info->child_type_)
    {
    case PROJ_ITEM_TYPE_SOURCE:
        /* set up to ask for a source file */
        info.lpstrTitle = "Select source files to add";
        info.lpstrFilter = "Source files\0*.t\0"
                           "Library files\0*.tl\0"
                           "All Files\0*.*\0\0";
        info.lpstrDefExt = "t";
        break;

    case PROJ_ITEM_TYPE_HEADER:
        /* set up to ask for a header file */
        info.lpstrTitle = "Select #include files to add";
        info.lpstrFilter = "Include files\0*.t;*.h\0"
                           "All Files\0*.*\0\0";
        info.lpstrDefExt = "h";
        break;

    case PROJ_ITEM_TYPE_FEELIE:
        info.lpstrTitle = "Select \"feelie\" files to add";
        goto add_etc;

    case PROJ_ITEM_TYPE_MISC:
        info.lpstrTitle = "Select files to add to notes/miscellaneous";
        goto add_etc;

    case PROJ_ITEM_TYPE_WEBEXTRA:
        info.lpstrTitle = "Select web extras to add";
        goto add_etc;

    add_etc:
        /* set up to ask for feelies/web extras/miscellanous files */
        info.lpstrFilter = "Text, Document, and Image Files\0"
                           "*.txt;*.pdf;*.htm;*.html;*.doc;"
                           "*.jpg;*.jpeg;*.jpe;*.png\0"
                           "Text Files (*.txt)\0*.txt\0"
                           "Web Pages (*.htm)\0*.htm;*.html\0"
                           "Portable Document Format (*.pdf)\0*.pdf\0"
                           "MS Word Documents (*.doc)\0*.doc\0"
                           "JPEG Images (*.jpg)\0*.jpg;*.jpeg;*.jpe\0"
                           "PNG Images (*.png)\0*.png\0"
                           "GIF Images (*.gif)\0*.gif\0"
                           "All Files (*.*)\0*.*\0";
        info.lpstrDefExt = 0;

        /* allow adding files that haven't been created yet */
        info.Flags &= ~OFN_FILEMUSTEXIST;
        break;

    case PROJ_ITEM_TYPE_RESOURCE:
    default:
        /* set up to ask for a resource file */
        info.lpstrTitle = "Select resource files to add";
        info.lpstrFilter = "All Multimedia Files\0"
                           "*.jpg;*.jpeg;*.jpe;*.png;"
                           "*.wav;*.mp3;*.mid;*.midi\0"
                           "JPEG Images (*.jpg)\0*.jpg;*.jpeg;*.jpe\0"
                           "PNG Images (*.png)\0*.png\0"
                           "Wave Sounds (*.wav)\0*.wav\0"
                           "MPEG Sounds (*.mp3)\0*.mp3\0"
                           "MIDI Music (*.mid)\0*.mid;*.midi\0"
                           "All Files (*.*)\0*.*\0\0";

        /* there's no default filename extension for resource files */
        info.lpstrDefExt = 0;

        /* allow adding resource files that haven't been created yet */
        info.Flags &= ~OFN_FILEMUSTEXIST;
        break;
    }

    /* ask for a file or files to add to the project */
    info.hwndOwner = dbgmain_->get_handle();
    info.hInstance = CTadsApp::get_app()->get_instance();
    info.lpstrCustomFilter = 0;
    info.nFilterIndex = 0;
    info.lpstrFile = fname;
    fname[0] = '\0';
    info.nMaxFile = sizeof(fname);
    info.lpstrFileTitle = 0;
    info.nMaxFileTitle = 0;
    info.lpstrInitialDir = CTadsApp::get_app()->get_openfile_dir();
    info.nFileOffset = 0;
    info.nFileExtension = 0;
    info.lCustData = 0;
    info.lpfnHook = 0;
    info.lpTemplateName = 0;
    CTadsDialog::set_filedlg_center_hook((OPENFILENAME *)&info);
    if (!GetOpenFileName((OPENFILENAME *)&info))
        return;

    /*
     *   check for a single filename, and regularize the filename if
     *   that's what they selected
     */
    if (info.nFileOffset != 0)
        fname[info.nFileOffset - 1] = '\0';

    /* add each file in the list */
    for (p = fname + info.nFileOffset ; *p != '\0' ; p += strlen(p) + 1)
    {
        char fullname[OSFNMAX];
        HTREEITEM new_item;

        /* get the path part of the name */
        strcpy(fullname, fname);

        /* add a slash if we don't have one already */
        if (info.nFileOffset < 2 || fname[info.nFileOffset - 2] != '\\')
            strcat(fullname, "\\");

        /* append the filename */
        strcat(fullname, p);

        /* add this file */
        new_item = add_file_to_project(
            cont, sel, fullname, PROJ_ITEM_TYPE_NONE, TRUE);

        /*
         *   if we successfully added a file, add the next file (if any)
         *   after this new file, so that we keep them in order
         */
        if (new_item != 0)
            sel = new_item;
    }

    /* set the path for future open-file dialog calls */
    CTadsApp::get_app()->set_openfile_path(fname);

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}

/*
 *   Add a folder to the project (for resources only)
 */
void CHtmlDbgProjWin::add_project_dir()
{
    HTREEITEM cont;
    HTREEITEM sel;
    proj_item_info *cont_info;
    char fname[OSFNMAX];

    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /*
     *   First, determine which container is selected.  If a child is
     *   selected, insert into the child's container.
     */
    for (cont = sel = TreeView_GetSelection(tree_) ; cont != 0 ;
         cont = TreeView_GetParent(tree_, cont))
    {
        /* get this item's information */
        cont_info = get_item_info(cont);

        /*
         *   If this item allows children, and its children are either
         *   resource files or miscellanous files, it's the parent we're
         *   looking for.
         */
        if (cont_info != 0
            && cont_info->can_add_children_
            && (cont_info->child_type_ == PROJ_ITEM_TYPE_RESOURCE
                || cont_info->child_type_ == PROJ_ITEM_TYPE_MISC))
            break;

        /*
         *   if this is a library, don't allow it on the general principle
         *   that we can't add anything to a library through the project
         *   window
         */
        if (cont_info != 0 && cont_info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* explain that we can't change libraries */
            MessageBox(dbgmain_->get_handle(), no_lib_change_msg,
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

            /* make sure the message comes up immediately */
            SendMessage(dbgmain_->get_handle(), WM_COMMAND,
                        ID_VIEW_PROJECT, 0);

            /* stop now - the user must select something and try again */
            return;
        }
    }

    /* if we didn't find a valid container, complain and give up */
    if (cont == 0)
    {
#if 0 // there's always ambiguity now that we can add folders to Notes & Misc
        HTREEITEM cur;
        int found_extres;

        /*
         *   First, check to see if there are any external resource files.
         *   If there aren't, this request is unambiguous - simply add to
         *   the resources in the image file.
         */
        for (found_extres = FALSE, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree_, cur))
        {
            proj_item_info *info;

            /* get this item's information */
            info = get_item_info(cur);

            /* if it's an external resource file, note it */
            if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
            {
                found_extres = TRUE;
                break;
            }
        }

        /*
         *   if we found an external resource file, the request is
         *   ambiguous, so we can't go on
         */
        if (found_extres)
        {
            /* explain the problem */
            MessageBox(dbgmain_->get_handle(),
                       "Before adding a new directory, please select the "
                       "resource area to contain the folder.  Select "
                       "\"Resource Files\" to add to the image file, or "
                       "select an external resource file.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

            /*
             *   make sure the project window is the active window (we
             *   need to wait until after the message box is dismissed, to
             *   ensure we leave the project window selected if it's
             *   docked; we also did this before the message box to ensure
             *   the project window is visible while the message box is
             *   being displayed)
             */
            SendMessage(dbgmain_->get_handle(), WM_COMMAND,
                        ID_VIEW_PROJECT, 0);

            /* stop now - the user must select something and try again */
            return;
        }

        /* the container is the "resource files" section */
        cont = res_item_;
#endif

        /* explain that they need to select a valid section */
        MessageBox(dbgmain_->get_handle(),
                   sel == 0
                   ? "Before adding a new folder, you must first select "
                   "the section of the project where the folder will go."
                   : "The selected project section can't contain folders.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /* make sure the project window is active so they see what we mean */
        SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* done */
        return;
    }

    /* browse for a folder */
    if (!CTadsDialogFolderSel2::
        run_select_folder(dbgmain_->get_handle(),
                          CTadsApp::get_app()->get_instance(),
                          "Folder to add to the resources:",
                          "Select a Resource Folder",
                          fname, sizeof(fname),
                          CTadsApp::get_app()->get_openfile_dir(), 0, 0))
        return;

    /* add the folder */
    add_file_to_project(cont, sel, fname, PROJ_ITEM_TYPE_NONE, TRUE);

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}


/* ------------------------------------------------------------------------ */
/*
 *   Tree walker.  This does a recursive depth-first traversal of the tree
 *   control.
 */
class TreeWalker
{
public:
    TreeWalker(HWND tree) { tree_ = tree; }
    virtual ~TreeWalker() { }

    /*
     *   Visit a node - subclasses can override this to customize the tree
     *   walking behavior.  Return TRUE to continue the traversal, FALSE to
     *   abort the traversal.
     */
    virtual int visit(HTREEITEM cur) { return TRUE; }

    /* walk the tree */
    void walk_tree()
    {
        /* walk the root level, which will recurse into all sublevels */
        walk_level(TreeView_GetRoot(tree_));
    }

    /* walk a level in the tree */
    int walk_level(HTREEITEM par)
    {
        HTREEITEM chi;

        /* visit the parent */
        if (!visit(par))
            return FALSE;

        /* visit each child */
        for (chi = TreeView_GetChild(tree_, par) ; chi != 0 ;
             chi = TreeView_GetNextSibling(tree_, chi))
        {
            /* walk this child level; abort the traversal if desired */
            if (!walk_level(chi))
                return FALSE;
        }

        /* tell the caller to continue traversing */
        return TRUE;
    }

protected:
    /* the tree control */
    HWND tree_;
};

/*
 *   Get the next file for a multi-file search
 */
int CHtmlDbgProjWin::get_next_search_file(char *fname, size_t fname_size,
                                          const char *curfile,
                                          int direction, int wrap,
                                          int local_only)
{
    class TreeFinder: public TreeWalker
    {
    public:
        TreeFinder(CHtmlDbgProjWin *win, HWND tree, HTREEITEM start,
                   int local_only, CHtmlSys_dbgmain *dbgmain)
            : TreeWalker(tree)
        {
            /* remember the window and debugger objects */
            win_ = win;
            dbgmain_ = dbgmain;

            /* note the starting item */
            start_ = start;

            /* we haven't found our starting item yet */
            found_ = FALSE;

            /* we haven't found any searchable items yet */
            first_ = last_ = 0;

            /* we have no matching item yet */
            match_ = 0;

            /* note the local/global scope */
            local_only_ = local_only;
        }

        /* visit a searchable item */
        virtual int visit_searchable(HTREEITEM cur) = 0;

        int visit(HTREEITEM cur)
        {
            proj_item_info *info;
            HTREEITEM par;
            int ok = TRUE;

            /* if this item has no filename, skip it */
            if ((info = win_->get_item_info(cur)) == 0
                || info->fname_ == 0)
                return TRUE;

            /*
             *   check the open type through the item's parent: if it's not
             *   openable as a text file, it's not a searchable item
             */
            if ((par = TreeView_GetParent(tree_, cur)) == 0
                || (info = win_->get_item_info(par)) == 0
                || info->child_open_action_ != PROJ_ITEM_OPEN_SRC)
                return TRUE;

            /* if this the first one we've found, note it */
            if (first_ == 0)
                first_ = cur;

            /* if we only want local files, skip non-local files */
            if (local_only_)
            {
                char path[OSFNMAX];

                /* build the item's path */
                win_->get_item_path(path, sizeof(path), cur);

                /* if it's not a local file, skip it */
                if (!dbgmain_->is_local_project_file(path))
                    ok = FALSE;
            }

            /* if this one is acceptable, do subclass-specific work */
            if (ok && !visit_searchable(cur))
                return FALSE;

            /* this is the last one we've visited so far */
            if (ok)
                last_ = cur;

            /* if this is the one we're looking for, note it */
            if (cur == start_)
                found_ = TRUE;

            /* continue the traversal */
            return TRUE;
        }

        /* the project window */
        CHtmlDbgProjWin *win_;

        /* the debugger object */
        CHtmlSys_dbgmain *dbgmain_;

        /* do we want only local project files? */
        int local_only_;

        /* have we found our starting item yet? */
        int found_;

        /* the starting item */
        HTREEITEM start_;

        /* the first and last searchable items we encountered */
        HTREEITEM first_;
        HTREEITEM last_;

        /* the matching item */
        HTREEITEM match_;
    };

    class NextFinder: public TreeFinder
    {
    public:
        NextFinder(CHtmlDbgProjWin *win, HWND tree, HTREEITEM start,
                   int local_only, CHtmlSys_dbgmain *dbgmain)
            : TreeFinder(win, tree, start, local_only, dbgmain)
            { }

        int visit_searchable(HTREEITEM cur)
        {
            /*
             *   if we've already encountered the starting item, this is the
             *   next searchable item, so take it as the match
             */
            if (found_)
            {
                /* note the match */
                match_ = cur;

                /* there's no need to look any further */
                return FALSE;
            }

            /* this isn't our match; keep looking */
            return TRUE;
        }
    };

    class PrevFinder: public TreeFinder
    {
    public:
        PrevFinder(CHtmlDbgProjWin *win, HWND tree, HTREEITEM start,
                   int local_only, CHtmlSys_dbgmain *dbgmain)
            : TreeFinder(win, tree, start, local_only, dbgmain)
            { }

        int visit_searchable(HTREEITEM cur)
        {
            /*
             *   If this is our starting item, and we have a previous item,
             *   the last item we found is the previous searchable item.
             *   When we're starting at the first item, we won't have a
             *   previous item, so we'll need to keep scanning for the last
             *   item in case we want to wrap.
             */
            if (cur == start_ && last_ != 0)
            {
                /* the match is the previous item */
                match_ = last_;

                /* no need to continue looking */
                return FALSE;
            }

            /* keep looking */
            return TRUE;
        }
    };

    HTREEITEM cur;

    /*
     *   First, find the item for this file.  If the filename is null, get
     *   the first or last item in the tree, according to the direction.
     */
    if (curfile != 0)
    {
        /* we have a file - find the next item in the given direction */
        cur = find_item_for_file(TreeView_GetRoot(tree_), curfile);
    }
    else
    {
        /* there's no file - start with no initial item */
        cur = 0;
    }

    /* search for the next or previous item, according to 'direction' */
    if (direction > 0)
    {
        /* going forward - search the tree for the next item */
        NextFinder nf(this, tree_, cur, local_only, dbgmain_);
        nf.walk_tree();

        /*
         *   if we found a match, use it; otherwise, if this is a wrapped
         *   search, wrap to the first searchable item
         */
        if ((cur = nf.match_) == 0 && wrap)
            cur = nf.first_;
    }
    else
    {
        /* going backward - search for the previous item */
        PrevFinder pf(this, tree_, cur, local_only, dbgmain_);
        pf.walk_tree();

        /*
         *   if we found a match, use it; otherwise, if this is a wrapped
         *   search, wrap to the last searchable item
         */
        if ((cur = pf.match_) == 0 && wrap)
            cur = pf.last_;
    }

    /* if we found an item, fill in the filename information */
    if (cur != 0)
    {
        get_item_path(fname, fname_size, cur);
        return TRUE;
    }

    /* no more files */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Receive notification that a file has been saved.  If the file is a
 *   library that's part of the project, we'll reload it so that the project
 *   list reflects its latest contents.
 */
void CHtmlDbgProjWin::note_file_save(const char *fname)
{
    HTREEITEM item;
    proj_item_info *info;

    /* if we can find the item, and it's a library, reload it */
    if ((item = find_item_for_file(TreeView_GetRoot(tree_), fname)) != 0
        && (info = get_item_info(item)) != 0
        && info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        CHtmlDebugConfig config;

        /* save the project configuration into our temporary config object */
        save_config(&config);

        /* reload the library into the project list */
        refresh_library(&config, item);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Is the given file in the project?
 */
int CHtmlDbgProjWin::is_file_in_project(const char *fname)
{
    /* search recursively starting at the root */
    return is_file_child_of(TreeView_GetRoot(tree_), fname);
}

/*
 *   search recursively for a file
 */
int CHtmlDbgProjWin::is_file_child_of(HTREEITEM parent, const char *fname)
{
    HTREEITEM cur;

    /* search the children of this parent */
    for (cur = TreeView_GetChild(tree_, parent) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        char path[OSFNMAX];

        /* get this item's full path */
        get_item_path(path, sizeof(path), cur);

        /* if it matches, we've found it */
        if (stricmp(path, fname) == 0)
            return TRUE;

        /* scan this item's children, if anh */
        if (is_file_child_of(cur, fname))
            return TRUE;
    }

    /* not found */
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Auto-add a file: figure out what section the file should go in based on
 *   the file type, and add it there.
 */
void CHtmlDbgProjWin::auto_add_file(const char *fname)
{
    const char *p, *ext;
    struct
    {
        const char *ext;
        proj_item_type_t typ;
        HTREEITEM container;
    } typemap[] =
    {
        { "t", PROJ_ITEM_TYPE_SOURCE, src_item_ },
        { "tl", PROJ_ITEM_TYPE_LIB, src_item_ },
        { "h", PROJ_ITEM_TYPE_HEADER, inc_item_ },
        { "txt", PROJ_ITEM_TYPE_FEELIE, feelies_item_ },
        { "htm", PROJ_ITEM_TYPE_FEELIE, feelies_item_ },
        { "html", PROJ_ITEM_TYPE_FEELIE, feelies_item_ },
        { "pdf", PROJ_ITEM_TYPE_FEELIE, feelies_item_ },
        { "doc", PROJ_ITEM_TYPE_FEELIE, feelies_item_ },
        { 0, PROJ_ITEM_TYPE_RESOURCE, res_item_ }
    };
    int i;

    /* find the file extension - this determines where it goes */
    for (ext = "", p = fname + strlen(fname) ; p != fname ; )
    {
        --p;
        if (*p == '.')
        {
            ext = p + 1;
            break;
        }
        else if (*p == '\\' || *p == '/' || *p == ':')
            break;
    }

    /* figure the section based on the type */
    for (i = 0 ; typemap[i].ext != 0 ; ++i)
    {
        if (stricmp(typemap[i].ext, ext) == 0)
            break;
    }

    /* check to see if a special file is selected */
    HTREEITEM sel = TreeView_GetSelection(tree_);
    proj_item_info *selinfo = get_item_info(sel);
    if (selinfo != 0 && selinfo->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        char prompt[512];
        char sec[128];

        /* get the title of the selected section */
        TVITEM tvi;
        tvi.mask = TVIF_TEXT | TVIF_HANDLE;
        tvi.hItem = typemap[i].container;
        tvi.pszText = sec;
        tvi.cchTextMax = sizeof(sec);
        TreeView_GetItem(tree_, &tvi);

        /* ask if they want to add a special file */
        sprintf(prompt, "Do you wish to set this file as the \"%s\" in "
                "the Special Files section?\r\n\r\nIf you select No, "
                "the file will instead be added to the %s section.",
                selinfo->special_title_.get(), sec);
        switch (MessageBox(dbgmain_->get_handle(), prompt, "TADS Workbench",
                           MB_YESNOCANCEL | MB_ICONQUESTION))
        {
        case IDYES:
            /* they do want to add it as a special item */
            set_special_file(sel, fname, TRUE);

            /* our work here is done */
            return;

        case IDNO:
            /* they want to add it to the default section instead */
            break;

        case IDCANCEL:
            /* they don't want to add it at all - just abort now */
            return;
        }
    }

    /* add the file to this section */
    add_file_to_project(
        typemap[i].container, TVI_LAST, fname, typemap[i].typ, TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear out the list of include files
 */
void CHtmlDbgProjWin::clear_includes()
{
    HTREEITEM cur;
    HTREEITEM nxt;

    /* scan the children of the "Include Files" section */
    for (cur = TreeView_GetChild(tree_, inc_item_) ; cur != 0 ; cur = nxt)
    {
        /* remember the next sibling */
        nxt = TreeView_GetNextSibling(tree_, cur);

        /* delete this item */
        TreeView_DeleteItem(tree_, cur);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a scanned include file to the project
 */
void CHtmlDbgProjWin::add_scanned_include(const char *fname)
{
    char full_name[OSFNMAX];
    HTREEITEM cur;

    /* get the full path name of the file to be added */
    GetFullPathName(fname, sizeof(full_name), full_name, 0);

    /* scan the existing children of the "Include Files" section */
    for (cur = TreeView_GetChild(tree_, inc_item_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        char cur_name[OSFNMAX];

        /* get this item's full path */
        get_item_path(cur_name, sizeof(cur_name), cur);

        /*
         *   if this item's full path matches the full path of the item to
         *   be added, there's no need to add this item
         */
        if (stricmp(cur_name, full_name) == 0)
            return;
    }

    /*
     *   we didn't find this item among the current include files, so add it
     *   to the Include Files section
     */
    add_file_to_project(
        inc_item_, TVI_LAST, fname, PROJ_ITEM_TYPE_NONE, TRUE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Service routine: determine if the given filename is hierarchically below
 *   the given directory.  Both files are given as absolute paths.  Returns
 *   true if the file is within the given directory, or within one of its
 *   subdirectories.
 */
static int is_file_below_dir(const char *fname, const char *dir)
{
    char rel[OSFNMAX];
    char path[OSFNMAX];

    /* re-express fname relative to dir */
    os_get_rel_path(rel, sizeof(rel), dir, fname);

    /* extract the path from the relative name */
    os_get_path_name(path, sizeof(path), rel);

    /* if the path is empty, the file is directly within the directory */
    if (path[0] == '\0')
        return TRUE;

    /*
     *   if the relatively-expressed filename is absolute, then there is
     *   nothing in common between the two paths, so the file is definitely
     *   not within the directory
     */
    if (path[0] == '\\' || path[1] == ':')
        return FALSE;

    /*
     *   The path seems to be expressed relatively, and it's non-empty.
     *   There are two possibilities: the file is within the directory, or
     *   is within a parent of the directory or a subdirectory of a parent.
     *   In the latter case, the relatively-expressed path must start with
     *   ".."; so, if the path starts with "..", the file is not within the
     *   directory, otherwise it is.
     */
    if (memcmp(path, "..", 2) == 0)
    {
        /*
         *   it's relative to a parent of the directory, so it's not in the
         *   directory
         */
        return FALSE;
    }
    else
    {
        /* it's relative to the directory, so it's in the directory */
        return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Library path searcher.  We'll look in the "extensions dir" folder set in
 *   the global configuration, then through the "srcdir" folder list in the
 *   global config, then through the entries in the TADSLIB environment
 *   variable, if there is one.
 */
CLibSearchPathList::CLibSearchPathList(CHtmlDebugConfig *config)
{
    size_t len;

    /* remember the config object */
    config_ = config;

    /* reset to the start of the search */
    reset();

    /* read TADSLIB from the environment */
    if ((len = GetEnvironmentVariable("TADSLIB", 0, 0)) != 0)
    {
        /* allocate space */
        tadslib_ = (char *)th_malloc(len);

        /* read the variable */
        GetEnvironmentVariable("TADSLIB", tadslib_, len);

        /* start at the beginning of the variable */
        tadslib_p_ = tadslib_;
    }
    else
    {
        /* we have no TADSLIB, so clear the pointers to it */
        tadslib_ = tadslib_p_ = 0;
    }
}

CLibSearchPathList::~CLibSearchPathList()
{
    /* if we have a TADSLIB value stored, free the memory */
    if (tadslib_ != 0)
        th_free(tadslib_);
}

/* is there anything left in the search? */
int CLibSearchPathList::has_more() const
{
    /* if we haven't used the "extensions dir" yet, we have more entries */
    if (exts_cur_ == 0)
        return TRUE;

    /* if there's another "srcdir" entry, we have more entries */
    if (srcdir_cur_ < srcdir_cnt_)
        return TRUE;

    /* if there's anything left in the TADSLIB string, we have more */
    if (tadslib_p_ != 0 && *tadslib_p_ != '\0')
        return TRUE;

    /* we don't have anything left */
    return FALSE;
}

/* get the next entry; returns true on success, false on failure */
int CLibSearchPathList::get_next(char *buf, size_t buflen)
{
    int ret;

    /* presume failure */
    ret = FALSE;

    /* if we haven't used the "extensions dir" setting yet, read it */
    if (exts_cur_ < 1)
    {
        /* read it */
        ret = !config_->getval("extensions dir", 0, exts_cur_, buf, buflen);

        /* we've now consumed it */
        ++exts_cur_;

        /* if we were successful, and we got a non-empty result, return it */
        if (ret && buf[0] != '\0')
            return TRUE;
    }

    /* if we haven't exhausted the "srcdir" paths, read the next one */
    if (srcdir_cur_ < srcdir_cnt_)
    {
        /* read the next variable */
        ret = !config_->getval("srcdir", 0, srcdir_cur_, buf, buflen);

        /* skip to the next one */
        ++srcdir_cur_;

        /* return the success result */
        return ret;
    }

    /* check for a TADSLIB entry */
    if (tadslib_p_ != 0 && *tadslib_p_ != '\0')
    {
        char *p;
        size_t len;

        /* scan to the next semicolon */
        for (p = tadslib_p_ ; *p != '\0' && *p != ';' ; ++p) ;

        /* if we have space in the buffer, copy the result */
        if ((len = p - tadslib_p_) < buflen)
        {
            /* copy the data */
            memcpy(buf, tadslib_p_, len);

            /* null-terminate the result */
            buf[len] = '\0';

            /* success */
            ret = TRUE;
        }

        /* skip to the next non-semicolon */
        for ( ; *p == ';' ; ++p) ;

        /* this is the start of the next element in the list */
        tadslib_p_ = p;

        /* return the result */
        return ret;
    }

    /* we didn't find anything - return failure */
    return FALSE;
}

/* reset to the start of the search */
void CLibSearchPathList::reset()
{
    /* get the number of entries in the user library list */
    srcdir_cnt_ = config_->get_strlist_cnt("srcdir", 0);

    /* haven't read the "extensions dir" path yet */
    exts_cur_ = 0;

    /* start at the first entry */
    srcdir_cur_ = 0;

    /* start at the beginning of TADSLIB */
    tadslib_p_ = tadslib_;
}

/*
 *   Conduct a search.
 */
int CLibSearchPathList::find_tl_file(CHtmlDebugConfig *config,
                                     const char *fname,
                                     char *path, size_t pathlen)
{
    /*
     *   if we find the file in the working directory, or if the file has an
     *   absolute path and thus can be found without a path, there's no need
     *   for a path - just return 'true' with an empty path
     */
    if (!osfacc(fname))
    {
        /* we can find the filename as given, without an extra path prefix */
        path[0] = '\0';
        return TRUE;
    }

    /* set up a search context */
    CLibSearchPathList lst(config);

    /* scan the directories */
    while (lst.has_more())
    {
        char full_name[OSFNMAX];

        /* get the next path; if the fetch fails, skip this item */
        if (!lst.get_next(path, pathlen))
            continue;

        /* build the full filename */
        os_build_full_path(full_name, sizeof(full_name), path, fname);

        /* if this file exists, we found it */
        if (!osfacc(full_name))
            return TRUE;
    }

    /* return failure */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a file to the project under the given container
 */
HTREEITEM CHtmlDbgProjWin::add_file_to_project(
    HTREEITEM cont, HTREEITEM after, const char *fname,
    proj_item_type_t known_type, int manual)
{
    const char *orig_fname = fname;
    char new_fname[OSFNMAX];
    char parent_path[OSFNMAX];
    proj_item_info *cont_info;
    proj_item_info *info;
    char rel_fname[OSFNMAX];
    size_t len;
    HTREEITEM cur;
    int is_dir;
    char buf[OSFNMAX];
    char buf2[OSFNMAX];
    int icon_idx;
    HTREEITEM new_item;
    CHtmlDebugConfig *config = dbgmain_->get_local_config();
    const char *sys_path;
    int is_sys_file;
    proj_item_type_t new_item_type;
    proj_item_type_t new_child_type;
    proj_item_open_t new_child_open;
    int new_child_icon;
    char new_child_path[OSFNMAX];
    int can_delete;
    int is_lib_inc;
    int is_from_user_lib;

    /* if there's no container, do nothing */
    if (cont == 0)
        return 0;

    /* get the container's information */
    cont_info = get_item_info(cont);
    if (cont_info == 0)
        return 0;

    /*
     *   check to see if the file is a directory - if it is, we'll use a
     *   separate icon for it
     */
    DWORD attr = GetFileAttributes(fname);
    is_dir = (attr != INVALID_FILE_ATTRIBUTES
              && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0);

    /*
     *   If this file has an absolute path, and no such file exists, strip
     *   off the absolute path and keep just the root name, so that we can
     *   look for the root name in the usual places.  Chances are that the
     *   project was moved from another computer with a different directory
     *   layout; the easiest way to fix up the project for the new machine is
     *   to convert to a local path and use the usual search mechanism.
     */
    if (os_is_file_absolute(fname) && osfacc(fname))
    {
        /* it's an absolute path and doesn't exist - drop the path */
        safe_strcpy(new_fname, sizeof(new_fname),
                    os_get_root_name((char *)fname));
        fname = new_fname;
    }

    /* presume it's not a system file */
    is_sys_file = FALSE;

    /*
     *   If the container has a valid system path for its child files, check
     *   to see if this is a system file: it's a system file if it's in the
     *   system directory associated with this container.  (The system
     *   directory varies by container; for the "source" container, it's the
     *   system library directory, and for the "include" container, it's the
     *   system header directory.)
     */
    sys_path = cont_info->child_sys_path_;
    if (sys_path != 0)
    {
        /* get the full path name of the file we're adding */
        char full_fname[OSFNMAX];
        GetFullPathName(fname, sizeof(full_fname), full_fname, 0);

        /* get the length of the parent's system path, if it has one */
        size_t sys_path_len = strlen(sys_path);

        /*
         *   if the parent has a system path, and this name is relative to
         *   the system directory, mark it as a system file and store its
         *   name relative to the system directory; otherwise, store its name
         *   relative to the parent item's child path
         */
        if (strlen(full_fname) > sys_path_len
            && memicmp(full_fname, sys_path, sys_path_len) == 0
            && (full_fname[sys_path_len] == '\\'
                || full_fname[sys_path_len] == '/'))
        {
            /* note that it's a system file */
            is_sys_file = TRUE;

            /* the system path is the parent path */
            strcpy(parent_path, sys_path);
        }
    }

    /*
     *   if it's not a system file, get the parent item's path to its child
     *   items - we'll store the new item's filename relative to this path
     */
    if (!is_sys_file)
        get_item_child_dir(parent_path, sizeof(parent_path), cont);

    /*
     *   If we're adding a directory to something other than a resource or
     *   miscellanous container, don't allow it.  Libraries (.tl files) are
     *   valid as resoruce containers.
     */
    if (is_dir
        && cont_info->child_type_ != PROJ_ITEM_TYPE_RESOURCE
        && cont_info->child_type_ != PROJ_ITEM_TYPE_MISC
        && cont_info->type_ != PROJ_ITEM_TYPE_LIB)
    {
        /* if this was a manual operation, describe the problem */
        if (manual)
            MessageBox(dbgmain_->get_handle(),
                       "Folders cannot be added to the selected section "
                       "of the project.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

        /* indicate failure */
        return 0;
    }

    /*
     *   if the container is the target item, insert at the start of the
     *   container's list
     */
    if (after == cont)
        after = TVI_FIRST;
    else if (after == 0)
        after = TVI_LAST;

    /* generate a relative version of the filename path */
    os_get_rel_path(rel_fname, sizeof(rel_fname), parent_path, fname);

    /* presume it's not an include file from a library directory */
    is_lib_inc = FALSE;
    is_from_user_lib = FALSE;

    /*
     *   If it's a header file, and it's not a system file, check to see if
     *   it's relative to a library directory.  If so, mark it as a library
     *   include file.
     */
    if (cont == inc_item_
        && !is_sys_file
        && !is_file_below_dir(fname, parent_path))
    {
        /* scan the source tree for a library containing this file */
        is_lib_inc = scan_for_lib_inc_path(fname,
                                           parent_path, sizeof(parent_path),
                                           rel_fname, sizeof(rel_fname));
    }

    /*
     *   If it's a source file, and it's not a system file, check to see if
     *   it comes from somewhere on the library path.  Make this check if the
     *   filename as given doesn't exist, or it's not within the main project
     *   directory.
     */
    if (cont == src_item_
        && !is_sys_file
        && (!is_file_below_dir(fname, parent_path) || osfacc(fname)))
    {
        CHtmlDebugConfig *gconfig = dbgmain_->get_global_config();
        char buf[OSFNMAX];
        int found;
        int is_abs;
        CLibSearchPathList pathlist(gconfig);

    search_lib_path:
        /* note if the filename is absolute */
        is_abs = os_is_file_absolute(fname);

        /* reset to the start of the search path */
        pathlist.reset();

        /* scan the library directory list */
        for (found = FALSE ; pathlist.has_more() ; )
        {
            /* get this entry; if it's not valid, keep looping */
            if (!pathlist.get_next(buf, sizeof(buf)))
                continue;

            /*
             *   If the filename given was absolute, then we know exactly
             *   where the file is, so merely check to see if this absolute
             *   path happens to be the same as the current search item's
             *   directory - if so, then we can express the name relative to
             *   the search item, eliminating the need to refer to it by an
             *   absolute path in the project file.
             *
             *   If the name given was relative, then check to see if we can
             *   find this file in this search item's directory.  If so, then
             *   we can note that this search item is the parent directory of
             *   this file.
             */
            if (is_abs)
            {
                /* check to see if the file is in this search directory */
                if (is_file_below_dir(fname, buf))
                {
                    /* note that we found a match */
                    found = TRUE;

                    /* get the filename relative to the library path */
                    os_get_rel_path(rel_fname, sizeof(rel_fname), buf, fname);
                }
            }
            else
            {
                /* relative - check for an existing file in this folder */
                os_build_full_path(buf2, sizeof(buf2), buf, fname);

                /* if the file exists, use this location */
                if (!osfacc(buf2))
                {
                    /* note that we found a match */
                    found = TRUE;

                    /* the original name is the relative name */
                    safe_strcpy(rel_fname, sizeof(rel_fname), fname);

                    /* build the full absolute path for internal use */
                    os_build_full_path(new_fname, sizeof(new_fname),
                                       buf, rel_fname);
                    fname = new_fname;
                }
            }

            /* if we found a match, apply it */
            if (found)
            {
                /* mark it as a user library file */
                is_from_user_lib = TRUE;

                /* the parent path is the library path */
                strcpy(parent_path, buf);

                /* stop now, since we want to take the first match we find */
                break;
            }
        }

        /* try dropping any relative path prefix */
        if (!found && os_get_root_name((char *)fname) != fname)
        {
            /* remove the relative prefix */
            fname = os_get_root_name((char *)fname);

            /* try again */
            goto search_lib_path;
        }

        /*
         *   If we didn't find the file, run the appropriate dialog to tell
         *   the user about the problem and give them a chance to correct it.
         */
        if (!found)
        {
            int id;

            /*
             *   If the file was added manually by explicit user action, and
             *   'fname' tells us exactly where the file is, ask if they want
             *   to add its folder to the library path.
             *
             *   If the file isn't being added manually, they'll need to tell
             *   us where the file is located.  If the file was absolute, run
             *   the library path adder dialog; otherwise, run the file
             *   locator dialog.  If the dialog indicates that it added to
             *   the library search list, go back for another try at finding
             *   the file, since we might have added the file's location to
             *   the search list.
             */
            if (manual && !osfacc(orig_fname))
            {
                /*
                 *   The file was added manually, so just ask if they want to
                 *   add this directory to the library path.
                 */
                char buf[256 + OSFNMAX*2];
                char path[OSFNMAX];
                int id;

                /* get the full, absolute filename */
                GetFullPathName(orig_fname, sizeof(buf), buf, 0);
                os_get_path_name(path, sizeof(path), buf);

                /* ask what they want to do about it */
                sprintf(buf, "%s\r\n\r\n"
                        "This file isn't in the current project's folder, "
                        "and isn't in the Library Path list. Would you like "
                        "to add %s as a Library Path?\r\n\r\n"
                        "If you select No, "
                        "the file will be added to the project, but "
                        "Workbench might not be able to locate the file "
                        "in future sessions. Select Cancel if you don't "
                        "want to add this file to the project.",
                        orig_fname, path);
                id = MessageBox(dbgmain_->get_handle(), buf, "TADS Workbench",
                                MB_YESNOCANCEL | MB_ICONEXCLAMATION);
                switch (id)
                {
                case IDYES:
                    /* they want to add the path */
                    add_search_path(dbgmain_, path);

                    /* go back for another try */
                    goto search_lib_path;

                case IDNO:
                    /* add the file with the original filename */
                    fname = orig_fname;
                    break;

                case IDCANCEL:
                    /* canceled - don't add the file after all */
                    return 0;
                }
            }
            else if (is_abs)
            {
                CTadsDlgAbsPath *dlg;

                /*
                 *   The source has an absolute path, and the file exists on
                 *   the local system.  Run the dialog offering to add the
                 *   directory to the library search list and convert the
                 *   filename to relative notation.
                 */
                dlg = new CTadsDlgAbsPath(dbgmain_, fname);
                id = dlg->run_modal(DLG_ABS_SOURCE, dbgmain_->get_handle(),
                                    CTadsApp::get_app()->get_instance());
                delete dlg;
            }
            else
            {
                CTadsDlgMissingSrc *dlg;

                /*
                 *   The file has a relative name, and we couldn't find the
                 *   file in any of our search locations.  We probably just
                 *   need to add a new location to the library search list,
                 *   so run the missing-source dialog.
                 */
                dlg = new CTadsDlgMissingSrc(dbgmain_, fname);
                id = dlg->run_modal(DLG_MISSING_SOURCE,
                                    dbgmain_->get_handle(),
                                    CTadsApp::get_app()->get_instance());
                delete dlg;
            }

            /* if they updated the search list, go back for another try */
            if (id == IDOK)
                goto search_lib_path;
        }
    }

    /*
     *   scan the parent's contents list and make sure this same file
     *   isn't already in the list
     */
    for (cur = TreeView_GetChild(tree_, cont) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        proj_item_info *cur_info = get_item_info(cur);

        /* if the filenames match, ignore this addition */
        if (cur_info != 0 && cur_info->fname_ != 0
            && stricmp(rel_fname, cur_info->fname_) == 0)
            return 0;
    }

    /*
     *   assume the new item is of the default type for children of our
     *   container, and that the new item will not allow any children of its
     *   own
     */
    new_item_type = cont_info->child_type_;
    new_child_open = PROJ_ITEM_OPEN_NONE;
    new_child_type = PROJ_ITEM_TYPE_NONE;
    new_child_path[0] = '\0';
    new_child_icon = 0;
    can_delete = TRUE;

    /*
     *   If the type of the new item was specified by the caller, use that
     *   type.  Otherwise, we might need to infer the type from the filename
     *   (more specifically, from the filename suffix).
     */
    if (known_type != PROJ_ITEM_TYPE_NONE)
    {
        /*
         *   the caller knows exactly what type it wants to use for this
         *   file - use the item type that the caller specified, rather than
         *   the default child type for the container
         */
        new_item_type = known_type;
    }
    else
    {
        /*
         *   The caller did not specify the item type, so we must infer it.
         *   In most cases, the item type is fixed by the container, so the
         *   default child type we assumed above is probably the right type.
         *   However, in some cases, we'll use something other than the
         *   default child type.
         *
         *   If we're adding the file to the source file list, the new item
         *   could be either a source file or a library.  If the caller
         *   didn't specify which it is, infer the type from the filename:
         *   if the new item's filename ends in ".tl", it's a source
         *   library, not an ordinary source file.
         */
        if (new_item_type == PROJ_ITEM_TYPE_SOURCE
            && (len = strlen(rel_fname)) > 3
            && stricmp(rel_fname + len - 3, ".tl") == 0)
        {
            /* the new item is a library - note its type */
            new_item_type = PROJ_ITEM_TYPE_LIB;
        }
    }

    /* if we're adding a library, use appropriate settings for the item */
    if (new_item_type == PROJ_ITEM_TYPE_LIB)
    {
        /* children of a library are, by default, source files */
        new_child_open = PROJ_ITEM_OPEN_SRC;
        new_child_type = PROJ_ITEM_TYPE_SOURCE;
        new_child_icon = img_idx_source_;

        /*
         *   our children's filenames are all relative to our path - extract
         *   the path portion from our relative filename (we only want the
         *   relative part of the path, because we build paths by combining
         *   all of the paths from each of our parents hierarchically)
         */
        os_get_path_name(new_child_path, sizeof(new_child_path), rel_fname);
    }

    /* if this is a folder, set some additional attributes */
    if (is_dir)
    {
        /* set the defaults for child objects in a folder */
        new_child_open = PROJ_ITEM_OPEN_SENSE;
        new_child_type = PROJ_ITEM_TYPE_RESOURCE;
        new_child_icon = img_idx_resource_;

        /* if this is a resource object, mark it as a folder */
        if (new_item_type == PROJ_ITEM_TYPE_RESOURCE)
            new_item_type = PROJ_ITEM_TYPE_FOLDER;

        /* set the file path to the child items */
        safe_strcpy(new_child_path, sizeof(new_child_path), rel_fname);
    }

    /* we can't delete children from libraries */
    if (cont_info->type_ == PROJ_ITEM_TYPE_LIB)
        can_delete = FALSE;

    /* we can't delete children from resource folders */
    if (cont_info->type_ == PROJ_ITEM_TYPE_FOLDER
        && cont_info->fname_ != 0)
        can_delete = FALSE;

    /* set up the new item's descriptor */
    info = new proj_item_info(rel_fname, new_item_type, 0, new_child_path,
                              new_child_icon, can_delete, FALSE, TRUE,
                              new_child_open, new_child_type, is_sys_file);

    /* if it's a library include file, remember the library path */
    if (is_lib_inc)
        info->set_lib_path(parent_path);
    else if (is_from_user_lib)
        info->set_user_path(parent_path);

    /*
     *   figure out which icon to use, depending on whether this is a file
     *   or a directory
     */
    icon_idx = (is_dir
                ? img_idx_res_folder_
                : (new_item_type == PROJ_ITEM_TYPE_LIB
                   ? img_idx_lib_
                   : cont_info->child_icon_));

    /* add it to the container's list */
    new_item = insert_tree(cont, after, rel_fname, 0, icon_idx, info);

    /*
     *   if it's inside a library, give it a checkmark icon, checked by
     *   default; if we're loading this library from configuration
     *   information, the configuration loader will adjust the checkmarks
     *   for us later
     */
    if (new_item != 0 && cont_info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        /* give the item a checkbox, checked by default */
        set_item_checkmark(new_item, TRUE);
    }

    /*
     *   If we successfully added the item, and it's a source library, read
     *   the library's contents and add each file mentioned in the library
     *   to the list.
     */
    if (new_item != 0 && new_item_type == PROJ_ITEM_TYPE_LIB)
    {
        /* parse the library */
        CTcLibParserProj::add_lib_to_proj(fname, this, tree_,
                                          new_item, info);

        /*
         *   explicitly collapse the library list initially - many users
         *   want to treat libraries as opaque, so they don't want to see
         *   all of the source files involved
         */
        TreeView_Expand(tree_, new_item, TVE_COLLAPSE);

        /*
         *   store the library file's modification timestamp, so that we can
         *   detect if the library is ever changed on disk while we have it
         *   loaded
         */
        get_item_timestamp(new_item, &info->file_time_);
    }

    /*
     *   If we successfully added the item, and the parent is the "includes"
     *   group, add the path to this file to the include path in the
     *   configuration.
     *
     *   If this item is included from a library directory, don't include it
     *   in the explicit include path, since the compiler automatically adds
     *   library directories to the include path.
     */
    if (new_item != 0 && cont == inc_item_ && !is_lib_inc)
    {
        char full_path[OSFNMAX];
        char abs_path[OSFNMAX];
        char sys_path[OSFNMAX];
        char rel_path[OSFNMAX];
        int cnt;
        int i;
        int found;

        /* get this item's fully-qualified directory path */
        os_get_path_name(full_path, sizeof(full_path), fname);

        /*
         *   The include path entries are normally stored internally in
         *   absolute path format, so find the absolute path to this file.
         *   If it's already in absolute format, just use it as-is;
         *   otherwise, prepend the parent path.
         */
        if (os_is_file_absolute(fname))
            strcpy(abs_path, full_path);
        else
            os_build_full_path(abs_path, sizeof(abs_path),
                               parent_path, full_path);

        /* get the item's relative directory path */
        os_get_path_name(rel_path, sizeof(rel_path), rel_fname);

        /* presume we won't find the path */
        found = FALSE;

        /*
         *   if the relative path is empty, this file is in the same
         *   directory as the main source files; the compiler looks here to
         *   begin with, so we don't need to add it to the include path
         */
        if (strlen(rel_path) == 0)
            found = TRUE;

        /*
         *   if the path is the system include path, we don't need to add it
         *   to the explicit include path because the compiler automatically
         *   looks there
         */
        os_get_special_path(sys_path, sizeof(sys_path),
                            _pgmptr, OS_GSP_T3_INC);
        if (stricmp(sys_path, rel_path) == 0)
            found = TRUE;

        /*
         *   search the #include list in the configuration to see if this
         *   path appears, either in absolute or relative form
         */
        cnt = config->get_strlist_cnt("build", "includes");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this item */
            config->getval("build", "includes", i, buf, sizeof(buf));

            /*
             *   if it matches the relative, full, or absolute path, this
             *   file's path is already listed, so we don't need to add it
             *   again
             */
            if (stricmp(full_path, buf) == 0
                || stricmp(abs_path, buf) == 0
                || stricmp(rel_path, buf) == 0)
            {
                /* note that we found it, and stop searching */
                found = TRUE;
                break;
            }
        }

        /*
         *   If we didn't find it, add it - use the relative form of the
         *   path to make the configuration more easily movable to another
         *   location in the file system (or to another computer).
         */
        if (!found)
            config->setval("build", "includes", cnt, rel_path);
    }

    /*
     *   If we successfully added the item, and it's a folder within another
     *   folder, a library, or the Resource section, add the files within the
     *   folder to the list.
     */
    if (is_dir
        && new_item != 0
        && (cont == res_item_
            || (cont_info != 0
                && (cont_info->type_ == PROJ_ITEM_TYPE_LIB
                    || cont_info->type_ == PROJ_ITEM_TYPE_FOLDER))))
    {
        /* callback object for processing files within the directory */
        class mycb: public IProjWinFileCallback
        {
        public:
            mycb(CHtmlDbgProjWin *win, HTREEITEM item)
            {
                win_ = win;
                item_ = item;
            }

            /* process a file by adding it to the project */
            virtual void file_cb(const textchar_t *fname,
                                 const textchar_t *relname,
                                 proj_item_type_t typ)
            {
                /* add it to the project under the parent item */
                win_->add_file_to_project(item_, TVI_LAST, fname, typ, FALSE);
            }

            /* 'this' in the parent context */
            CHtmlDbgProjWin *win_;

            /* the parent folder item */
            HTREEITEM item_;
        };

        /* don't update the treeview while building the list */
        LockWindowUpdate(tree_);

        /* enumerate files in the directory, adding them to the project list */
        char full_path[OSFNMAX];
        get_item_path(full_path, sizeof(full_path), new_item);
        mycb cb(this, new_item);
        enum_files_in_dir(&cb, full_path, full_path,
                          PROJ_ITEM_TYPE_RESOURCE, TRUE, TRUE, FALSE);

        /* sort the child list */
        TreeView_SortChildren(tree_, new_item, FALSE);

        /* initially close the folder to reduce visual clutter */
        TreeView_Expand(tree_, new_item, TVE_COLLAPSE);

        /* done updating */
        LockWindowUpdate(0);

        /* create a thread to monitor the directory for changes */
        info->folder_thread_ = new proj_item_folder_thread(
            handle_, new_item, full_path);
    }

    /*
     *   Tell the debugger that we've just saved this file.  Even though we
     *   haven't actually changed the file's contents, we've changed the
     *   *project's* contents by adding the file to the project.
     */
    dbgmain_->note_file_save(fname);

    /* return the new item */
    return new_item;
}

/* ------------------------------------------------------------------------ */
/*
 *   Refresh a folder listing
 */
void CHtmlDbgProjWin::refresh_folder(HTREEITEM item)
{
    /* get the item info */
    proj_item_info *info = get_item_info(item);
    if (info == 0)
        return;

    /* lock the tree against redrawing while we're fiddling with the list */
    LockWindowUpdate(tree_);

    /* get the full path to the parent directory */
    char path[OSFNMAX];
    get_item_path(path, sizeof(path), item);

    /* add items for any new files - set up a file enumeration callback */
    class mycb: public IProjWinFileCallback
    {
    public:
        mycb(CHtmlDbgProjWin *win, HTREEITEM item)
        {
            win_ = win;
            par_ = item;
        }

        /* process a file by adding it if it's not already in the list */
        virtual void file_cb(const textchar_t *fname,
                             const textchar_t *relname,
                             proj_item_type_t typ)
        {
            /* if the file isn't already in the list, add it */
            if (win_->find_item_for_file(par_, fname) == 0)
                win_->add_file_to_project(par_, TVI_LAST, fname, typ, FALSE);
        }

        /* 'this' in the parent context */
        CHtmlDbgProjWin *win_;

        /* the parent folder item */
        HTREEITEM par_;
    };

    /* enumerate files through our adder callback */
    mycb cb(this, item);
    enum_files_in_dir(&cb, path, path, PROJ_ITEM_TYPE_RESOURCE,
                      TRUE, FALSE, FALSE);

    /* delete any items that refer to files that no longer exist */
    for (HTREEITEM chi = TreeView_GetChild(tree_, item) ; chi != 0 ; )
    {
        /* note the next item (do this first in case we delete 'cur') */
        HTREEITEM nxt = TreeView_GetNextSibling(tree_, chi);

        /* check to see if the item exists */
        char chipath[OSFNMAX];
        get_item_path(chipath, sizeof(chipath), chi);
        if (osfacc(chipath))
        {
            /*
             *   the file doesn't exist, so the item must refer to a file
             *   that has been deleted since the last refresh - delete the
             *   item
             */
            TreeView_DeleteItem(tree_, chi);
        }

        /* move on to the next item */
        chi = nxt;
    }

    /* sort the child list */
    TreeView_SortChildren(tree_, item, FALSE);

    /* done with the batch of updates - unlock the window */
    LockWindowUpdate(0);
}


/* ------------------------------------------------------------------------ */
/*
 *   Scan the source tree for a library whose location matches the given
 *   header file's path prefix.
 *
 *   If we find a library in the project source file list whose .tl file is
 *   in the same directory as the given .h file, we'll return true, and we'll
 *   fill in 'parent_path' with the path of the library, and 'rel_fname' with
 *   the given header file's name relative to the parent path.
 *
 *   If we don't find a matching library, we'll return false, and we won't
 *   change parent_path or rel_fname.
 */
int CHtmlDbgProjWin::scan_for_lib_inc_path(const char *fname,
                                           char *parent_path,
                                           size_t parent_path_size,
                                           char *rel_fname,
                                           size_t rel_fname_size)
{
    char path[OSFNMAX];

    /* get the full path to the file */
    os_get_path_name(path, sizeof(path), fname);

    /* scan the tree recursively, starting at the source item */
    return scan_for_lib_inc_path_in(src_item_, path, fname,
                                    parent_path, parent_path_size,
                                    rel_fname, rel_fname_size);
}

/*
 *   Service routine for scan_for_lib_inc_path: search the source list
 *   recursively, starting at the given parent item.  We'll search all
 *   children of the given item.
 */
int CHtmlDbgProjWin::scan_for_lib_inc_path_in(
    HTREEITEM parent_tree_item, const char *path, const char *fname,
    char *parent_path, size_t parent_path_size,
    char *rel_fname, size_t rel_fname_size)
{
    HTREEITEM cur;

    /* scan children of the given item in the tree */
    for (cur = TreeView_GetChild(tree_, parent_tree_item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        proj_item_info *cur_info = get_item_info(cur);

        /*
         *   if it's a library, check to see if the new file is in the same
         *   directory as the library
         */
        if (cur_info != 0 && cur_info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            char cur_fname[OSFNMAX];
            char cur_path[OSFNMAX];

            /* get the item's path */
            get_item_path(cur_fname, sizeof(cur_fname), cur);
            os_get_path_name(cur_path, sizeof(cur_path), cur_fname);

            /* check to see if it's in this directory */
            if (stricmp(path, cur_path) == 0)
            {
                /* it's a match - the parent path is the library path */
                strcpy(parent_path, path);

                /* re-figure the name relative to the new parent path */
                os_get_rel_path(rel_fname, rel_fname_size, parent_path, fname);

                /* no need to look further - indicate we found a match */
                return TRUE;
            }

            /*
             *   Libraries can contain other libraries, so scan children of
             *   this item.  If we find a match among the children, indicate
             *   success.
             */
            if (scan_for_lib_inc_path_in(cur, path, fname,
                                         parent_path, parent_path_size,
                                         rel_fname, rel_fname_size))
                return TRUE;
        }
    }

    /* we didn't find a match */
    return FALSE;
}

/*
 *   Get the timestamp for an item in the tree.
 */
void CHtmlDbgProjWin::get_item_timestamp(HTREEITEM item, FILETIME *ft)
{
    char path[OSFNMAX];
    WIN32_FIND_DATA fdata;
    HANDLE hfind;

    /* get the full path to the file */
    get_item_path(path, sizeof(path), item);

    /* begin a search for the file to get its directory data */
    hfind = FindFirstFile(path, &fdata);

    /* if we found the file, not its timestamp */
    if (hfind != INVALID_HANDLE_VALUE)
    {
        /* copy the timestamp information out of the find data */
        memcpy(ft, &fdata.ftLastWriteTime, sizeof(*ft));

        /* we're done with the search, so close the search handle */
        FindClose(hfind);
    }
    else
    {
        /* no timestamp information is available */
        memset(ft, 0, sizeof(*ft));
    }
}

/*
 *   Get the directory that will contain children of an item
 */
void CHtmlDbgProjWin::get_item_child_dir(char *buf, size_t buflen,
                                         HTREEITEM item)
{
    proj_item_info *info;
    proj_item_info *parent_info;
    HTREEITEM parent;
    char *p;
    size_t rem;

    /* get the information on this object */
    info = get_item_info(item);

    /*
     *   If this item is marked as a system file, then the parent is not
     *   relevant in determining our path, because our path is relative to
     *   the system directory rather than to a parent of ours.
     *
     *   Likewise, if this item was found on the user library search list,
     *   then the child location depends only on our own location.
     *
     *   Otherwise, if this isn't a top-level object, our path is relative
     *   to our parent's child path.  If we're a root object, though, our
     *   child path is absolute, since there's nothing to be relative to.
     */
    parent = TreeView_GetParent(tree_, item);
    parent_info = (parent != 0 ? get_item_info(parent) : 0);
    if (info->is_sys_file_
        && parent_info != 0
        && parent_info->child_sys_path_ != 0)
    {
        /* we're a system file, so we're relative to the system path */
        strncpy(buf, parent_info->child_sys_path_, buflen);
        buf[buflen-1] = '\0';
    }
    else if (info->in_user_lib_)
    {
        /*
         *   we're from the search list, so we're relative to the location
         *   where we found a match in the search list
         */
        strncpy(buf, info->lib_path_, buflen);
        buf[buflen-1] = '\0';
    }
    else if (parent != 0)
    {
        /* we're relative to our parent, so get the parent path */
        get_item_child_dir(buf, buflen, parent);
    }
    else
    {
        /* there's no parent - we're the root */
        buf[0] = '\0';
    }

    /* if we don't have our own child path, there's nothing to add */
    if (info->child_path_ == 0 || info->child_path_[0] == '\0')
        return;

    /* set up to add our path portion after the parent prefix */
    p = buf + strlen(buf);
    rem = buflen - (p - buf);

    /* add a path separator if necessary */
    if (p != buf && rem > 1
        && *(p - 1) != '\\' && *(p - 1) != '/' && *(p - 1) != ':')
    {
        *p++ = '\\';
        --rem;
    }

    /* add our path portion if there's room */
    strncpy(p, info->child_path_, rem);

    /* make sure the result is null-terminated */
    if (rem > 0)
        *(p + rem - 1) = '\0';
}

/*
 *   Get the path to an item
 */
void CHtmlDbgProjWin::get_item_path(char *buf, size_t buflen, HTREEITEM item)
{
    proj_item_info *info;
    proj_item_info *parent_info;
    HTREEITEM parent;

    /* get the information on this object */
    info = get_item_info(item);

    /* if there's no filename, there's no path */
    if (info->fname_ == 0)
    {
        buf[0] = '\0';
        return;
    }

    /*
     *   If it's a system file, it's in the system directory.  Otherwise, if
     *   it's not a top-level item, the path is relative to the parent
     *   object's common child path.  If it's a top-level item, its path is
     *   absolute and doesn't depend on any parent paths.
     */
    parent = TreeView_GetParent(tree_, item);
    parent_info = (parent != 0 ? get_item_info(parent) : 0);
    if (info->is_sys_file_
        && parent_info != 0
        && parent_info->child_sys_path_ != 0)
    {
        /* build the full path by adding our name to the system path */
        os_build_full_path(buf, buflen,
                           parent_info->child_sys_path_, info->fname_);
    }
    else if (info->lib_path_ != 0)
    {
        /*
         *   this item is explicitly in a library directory - use the
         *   library path stored with the item
         */
        os_build_full_path(buf, buflen, info->lib_path_, info->fname_);
    }
    else if (parent != 0)
    {
        char parent_path[OSFNMAX];

        /* get the parent path */
        get_item_child_dir(parent_path, sizeof(parent_path), parent);

        /* build the full path by adding our name to the parent path */
        os_build_full_path(buf, buflen, parent_path, info->fname_);
    }
    else
    {
        /* we're a root-level object - our name is absolute */
        strncpy(buf, info->fname_, buflen);

        /* ensure the result is null-terminated */
        if (buflen != 0)
            buf[buflen - 1] = '\0';
    }
}

/*
 *   Get the library URL-style name for an item.  For an item in a top-level
 *   library, this is simply the original name of the item as it appeared in
 *   the library file itself.  For an item in a sub-library, this is a path
 *   to the item formed by prefixing the name of the item as it appeared in
 *   its immediately enclosing library with the URL-style name for the
 *   enclosing library and a slash (this rule is applied recursively for
 *   each enclosing library).
 *
 *   For example, suppose we have a library structure that shows up in the
 *   treeview like so:
 *
 *.  mylib
 *.    abc
 *.    def
 *.    libdir/sublib
 *.      ghi
 *.      jkl
 *.      subdir/subsublib
 *.        files/mno
 *.        files/pqr
 *
 *   The URL for the first child of 'mylib' is simply 'abc'.  The URL for
 *   the last item in the list ('files/pqr') is
 *   'libdir/sublib/subdir/subsublib/files/pqr'.
 *
 *   Note that the top-level library makes no contribution to any URL path,
 *   because each library member's path is relative to its top-level library
 *   in the first place.
 */
void CHtmlDbgProjWin::get_item_lib_url(char *url, size_t url_len,
                                       HTREEITEM item)
{
    HTREEITEM parent;
    proj_item_info *info;
    proj_item_info *par_info;
    size_t len;

    /* if our parent is a library, get its path first */
    if ((parent = TreeView_GetParent(tree_, item)) != 0
        && (par_info = get_item_info(parent)) != 0
        && par_info->type_ == PROJ_ITEM_TYPE_LIB)
    {
        /* get the parent's URL as the prefix to our URL */
        get_item_lib_url(url, url_len, parent);

        /*
         *   If there's room, add a '/' after the parent URL.  Do not add a
         *   '/' if there's nothing in the buffer, since we don't want a
         *   slash at the root level; the root is implicitly our top-level
         *   library, and by convention we don't add a slash before the path
         *   to files within it.
         */
        len = strlen(url);
        if (len != 0 && len + 1 < url_len)
        {
            url[len++] = '/';
            url[len] = '\0';
        }
    }
    else
    {
        /*
         *   our parent is not a library, so it has no contribution to our
         *   URL - we have no prefix
         */
        url[0] = '\0';
        len = 0;
    }

    /* get our own information object */
    info = get_item_info(item);

    /* add our own URL-style name, if we have one and it fits */
    if (info != 0
        && info->url_ != 0
        && strlen(info->url_) + 1 < url_len - len)
    {
        /* copy it */
        strcpy(url + len, info->url_);
    }
}

/*
 *   Add an external resource file
 */
void CHtmlDbgProjWin::add_ext_resfile()
{
    HTREEITEM cur;
    HTREEITEM after;
    HTREEITEM new_item;
    proj_item_info *info;
    proj_item_info *root_info;

    /* make sure the project window is visible */
    SendMessage(dbgmain_->get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

    /*
     *   work our way up to the selected external resource file container,
     *   if we can find one
     */
    for (cur = TreeView_GetSelection(tree_) ; cur != 0 ;
         cur = TreeView_GetParent(tree_, cur))
    {
        /* get this item's information */
        info = get_item_info(cur);

        /* if this is an external resource file object, we've found it */
        if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
            break;
    }

    /*
     *   If we found a selected external resource file, insert the new one
     *   immediately after the selected one.  If we didn't find one,
     *   insert the new one as the first external resource.
     */
    if (cur != 0)
    {
        /* insert after the selected one */
        after = cur;
    }
    else
    {
        /*
         *   Find the top-level object immediately preceding the first
         *   external resource (or the last top-level object, if there are
         *   no external resource files yet).  Start with the first item
         *   in the list, which is the root "project" item, and go through
         *   root-level items until we find the first external resource
         *   file.
         */
        for (cur = after = TreeView_GetRoot(tree_) ; cur != 0 ;
             cur = TreeView_GetNextSibling(tree_, cur))
        {
            /* get this item's information */
            info = get_item_info(cur);

            /*
             *   if it's not an external resource item, presume it will be
             *   the item we're looking for (we might find another one
             *   later, but remember this one for now)
             */
            if (info->type_ != PROJ_ITEM_TYPE_EXTRES)
                after = cur;
        }

        /* if we didn't find any items, insert at the end of the list */
        if (after == 0)
            after = TVI_LAST;
    }

    /* get the root object's information */
    root_info = get_item_info(TreeView_GetRoot(tree_));

    /*
     *   Create and insert the new item.  Use the same child path that the
     *   project file itself uses.  For the config group name, just use
     *   the template - we'll overwrite it with the actual group when we
     *   fix up the groups below, so all that matters is that we get the
     *   length right, which we can do by using the template string.
     */
    info = new proj_item_info(0, PROJ_ITEM_TYPE_EXTRES,
                              EXTRES_CONFIG_TEMPLATE,
                              root_info->child_path_, img_idx_resource_,
                              TRUE, TRUE, FALSE, PROJ_ITEM_OPEN_EXTERN,
                              PROJ_ITEM_TYPE_RESOURCE, FALSE);

    /* add it */
    new_item = insert_tree(TVI_ROOT, after, "", TVIS_BOLD | TVIS_EXPANDED,
                           img_idx_extres_, info);

    /* assume this file will be in the installer */
    set_extres_in_install(new_item, TRUE);

    /* fix up the numbering of all of the external resource items */
    fix_up_extres_numbers();

    /* save the configuration */
    save_config(dbgmain_->get_local_config());
}

/*
 *   Visually mark a tree view item as being included in the install
 */
void CHtmlDbgProjWin::set_extres_in_install(HTREEITEM item, int in_install)
{
    TV_ITEM info;

    /* set up the information structure */
    info.mask = TVIF_STATE | TVIF_HANDLE;
    info.hItem = item;
    info.stateMask = TVIS_STATEIMAGEMASK;
    info.state = INDEXTOSTATEIMAGEMASK(in_install
                                       ? img_idx_in_install_
                                       : img_idx_not_in_install_);

    /* set the new state icon */
    TreeView_SetItem(tree_, &info);
}

/*
 *   Visually mark a tree view item with a checkbox in the checked or
 *   unchecked state
 */
void CHtmlDbgProjWin::set_item_checkmark(HTREEITEM item, int checked)
{
    TV_ITEM info;

    /* set up the information structure */
    info.mask = TVIF_STATE | TVIF_HANDLE;
    info.hItem = item;
    info.stateMask = TVIS_STATEIMAGEMASK;
    info.state = INDEXTOSTATEIMAGEMASK(checked
                                       ? img_idx_check_
                                       : img_idx_nocheck_);

    /* set the new state icon */
    TreeView_SetItem(tree_, &info);
}

/*
 *   Fix up the external resource item numbering
 */
void CHtmlDbgProjWin::fix_up_extres_numbers()
{
    HTREEITEM cur;
    proj_item_info *info;
    int fileno;

    /* go through the root items and fix each external resource file */
    for (fileno = 0, cur = TreeView_GetRoot(tree_) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        info = get_item_info(cur);

        /* if it's an external resource, renumber it */
        if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
        {
            char namebuf[OSFNMAX];
            char groupbuf[50];
            TV_ITEM item;

            /* build the new display name and config group name */
            gen_extres_names(namebuf, groupbuf, fileno);

            /* set the new name */
            item.mask = TVIF_HANDLE | TVIF_TEXT;
            item.hItem = cur;
            item.pszText = namebuf;
            TreeView_SetItem(tree_, &item);

            /* delete any config information under the old group name */
            dbgmain_->get_local_config()
                ->clear_strlist("build", info->config_group_);

            /*
             *   set the new configuration group name (they're always the
             *   same length, so just overwrite the old config group name)
             */
            strcpy(info->config_group_, groupbuf);

            /* consume this file number */
            ++fileno;
        }
    }
}

/*
 *   Generate the display name and configuration group name for an
 *   external resource file, given the file's index in the list
 */
void CHtmlDbgProjWin::gen_extres_names(char *display_name,
                                       char *config_group_name,
                                       int fileno)
{
    char *p;

    /* build the display name if desired */
    if (display_name != 0)
        sprintf(display_name, "External Resource File (.3r%d)", fileno);

    /* generate the configuration group name if desired */
    if (config_group_name != 0)
    {
        /* start with the template */
        strcpy(config_group_name, EXTRES_CONFIG_TEMPLATE);

        /* find the '0000' suffix */
        for (p = config_group_name ; *p != '\0' && *p != '0' ; ++p) ;

        /* add the serial number */
        sprintf(p, "%04x", fileno);
    }

}

/*
 *   Open an item
 */
void CHtmlDbgProjWin::open_item(HTREEITEM item)
{
    proj_item_info *info;
    HTREEITEM parent;
    proj_item_info *par_info;
    char fname[OSFNMAX];

    /* get the information on this item */
    if (item == 0
        || (info = get_item_info(item)) == 0
        || info->fname_ == 0
        || !info->openable_)
        return;

    /* find the parent to determine how to open the file */
    parent = TreeView_GetParent(tree_, item);
    if (parent == 0 || (par_info = get_item_info(parent)) == 0)
        return;

    /* take the appropriate action */
    switch(par_info->child_open_action_)
    {
    case PROJ_ITEM_OPEN_NONE:
        /* can't open this item - ignore the request */
        break;

    case PROJ_ITEM_OPEN_SENSE:
        /* sense based on the item's name */
        get_item_path(fname, sizeof(fname), item);
        if (is_text_ext(fname))
            goto open_as_text;
        else
            goto open_extern;

    case PROJ_ITEM_OPEN_SRC:
    open_as_text:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /* if the file doesn't exist, show an error */
        if (osfacc(fname))
        {
            MessageBox(dbgmain_->get_handle(),
                       "This file does not exist or is not accessible.  "
                       "Check the file's name and directory path, and "
                       "ensure that the file isn't being used by another "
                       "application.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);
            break;
        }

        /* open it as a source file */
        dbgmain_->open_text_file(fname);
        break;

    case PROJ_ITEM_OPEN_EXTERN:
    open_extern:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /*
         *   If it's a text file, open it locally even though we're supposed
         *   to open children of my parent externally.  Use the extension to
         *   determine if it's text.  (It's a bit cheesy to use a fixed list
         *   of extensions, but it's simple and should work fine most of the
         *   time.)
         */
        if (!osfacc(fname) && is_text_ext(fname))
        {
            /* open it internally */
            dbgmain_->open_text_file(fname);

            /* we've fully handled this */
            break;
        }

        /* if it's a resource file, check for and drop any alias */
        if (info->type_ == PROJ_ITEM_TYPE_RESOURCE)
        {
            char *p;

            /* scan for an '=' character, which introduces an alias */
            for (p = fname ; *p != '\0' && *p != '=' ; ++p) ;

            /* terminate the filename at the '=', if we found one */
            if (*p == '=')
                *p = '\0';
        }

        /* ask the OS to open the file */
        if ((unsigned long)ShellExecute(0, "open", fname,
                                        0, 0, SW_SHOWNORMAL) <= 32)
        {
            /* that failed - show an error */
            MessageBox(dbgmain_->get_handle(),
                       "Unable to open file.  In order to open this "
                       "file, you must install an application that "
                       "can view or edit this file type.",
                       "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);
        }

        /* done */
        break;
    }
}

/*
 *   Open an item in the text editor
 */
void CHtmlDbgProjWin::open_item_in_text_editor(HTREEITEM item)
{
    proj_item_info *info;
    HTREEITEM parent;
    proj_item_info *par_info;
    char fname[OSFNMAX];

    /* get the information on this item */
    if (item == 0
        || ((info = get_item_info(item)) == 0 || !info->openable_))
        return;

    /* find the parent to determine how to open the file */
    parent = TreeView_GetParent(tree_, item);
    if (parent == 0 || (par_info = get_item_info(parent)) == 0)
        return;

    /* take the appropriate action */
    switch(par_info->child_open_action_)
    {
    case PROJ_ITEM_OPEN_NONE:
    case PROJ_ITEM_OPEN_EXTERN:
        /* can't open this item in the text editor - ignore the request */
        break;

    case PROJ_ITEM_OPEN_SENSE:
        /*
         *   if it has an extension associated with text files, open as a
         *   source file; otherwise do nothing
         */
        get_item_path(fname, sizeof(fname), item);
        if (is_text_ext(fname))
            goto open_as_source;
        break;

    case PROJ_ITEM_OPEN_SRC:
    open_as_source:
        /* get this item's full name */
        get_item_path(fname, sizeof(fname), item);

        /* if the file doesn't exist, show an error */
        if (osfacc(fname))
        {
            MessageBox(dbgmain_->get_handle(),
                       "This file does not exist or is not accessible.  "
                       "Check the file's name and directory path, and "
                       "ensure that the file isn't being used by another "
                       "application.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);
            break;
        }

        /* it's a source file - open it in the text editor */
        dbgmain_->open_in_text_editor(fname, 1, 1, FALSE);
        break;
    }
}

/*
 *   Process a user-defined message
 */
int CHtmlDbgProjWin::do_user_message(int msg, WPARAM wpar, LPARAM lpar)
{
    /* see which user-defined message we have */
    switch (msg)
    {
    case W32PRJ_MSG_OPEN_TREE_SEL:
        /* open the item selected in the project tree */
        open_item(TreeView_GetSelection(tree_));
        return TRUE;

    case W32PRJ_MSG_REFRESH_FOLDER:
        /* refresh a folder - the lpar is the HTREEITEM */
        refresh_folder((HTREEITEM)lpar);
        return TRUE;

    default:
        /* not one of ours */
        break;
    }

    /* if we didn't handle the message, inherit the default implementation */
    return CTadsWin::do_user_message(msg, wpar, lpar);
}

/*
 *   Process a notification message
 */
int CHtmlDbgProjWin::do_notify(int control_id, int notify_code, NMHDR *nm)
{
    TV_KEYDOWN *tvk;
    NM_TREEVIEW *nmt;
    HTREEITEM target;
    TV_HITTESTINFO ht;
    proj_item_info *info;

    /* if it's the tree control, check the message */
    if (control_id == TREEVIEW_ID)
    {
        switch(nm->code)
        {
        case NM_RETURN:
            /*
             *   This has been fully handled already in the TVN_KEYDOWN
             *   handler; just tell the caller we've handled it.
             *
             *   (If we don't handle this notification explicitly, the tree
             *   view control will use the default window procedure handling
             *   on the event, which will beep.  The beep is annoying.
             *   Explicitly handling the notification stops the beep.)
             */
            return TRUE;

        case TVN_KEYDOWN:
            /* cast the notify structure to a keydown-specific notifier */
            tvk = (TV_KEYDOWN *)nm;

            /* see what key we have */
            switch(tvk->wVKey)
            {
            case VK_DELETE:
            case VK_BACK:
                /* delete/backspace - delete the selected item */
                delete_tree_selection();

                /* handled - don't pass through to the incremental search */
                return TRUE;

            case VK_RETURN:
                /* enter/return - open the selected item */
                open_item(TreeView_GetSelection(tree_));

                /* handled - don't pass through to the incremental search */
                return TRUE;
            }

            /* if we didn't return already, we didn't handle it */
            break;

        case TVN_DELETEITEM:
            /* cast the notify structure to a tre-view notifier */
            nmt = (NM_TREEVIEW *)nm;

            /*
             *   if we have an extra information structure for this item,
             *   delete it
             */
            if (get_item_info(nmt->itemOld.hItem) != 0)
                delete get_item_info(nmt->itemOld.hItem);

            /* handled */
            return TRUE;

        case TVN_BEGINDRAG:
            /* cast the notify structure to a tree-view notifier */
            nmt = (NM_TREEVIEW *)nm;

            /*
             *   don't allow dragging files around within a library, since we
             *   treat libraries as read-only
             */
            info = get_item_info(
                TreeView_GetParent(tree_, nmt->itemNew.hItem));
            if (info == 0 || info->type_ == PROJ_ITEM_TYPE_LIB)
                return TRUE;

            /*
             *   get the item being dragged - if we can't find an item, we
             *   can't do the drag
             */
            info = get_item_info(nmt->itemNew.hItem);
            if (info == 0)
                return TRUE;

            /* we can only drag around certain types of files */
            switch(info->type_)
            {
            case PROJ_ITEM_TYPE_SOURCE:
            case PROJ_ITEM_TYPE_LIB:
            case PROJ_ITEM_TYPE_HEADER:
            case PROJ_ITEM_TYPE_RESOURCE:
            case PROJ_ITEM_TYPE_FEELIE:
            case PROJ_ITEM_TYPE_MISC:
            case PROJ_ITEM_TYPE_WEBEXTRA:
                /* we can drag these types, so proceed */
                break;

            default:
                /*
                 *   we can't drag any other types - stop now, without
                 *   initiating any drag tracking
                 */
                return TRUE;
            }

            /* enter the drag/drop operation */
            enter_drag_drop(nmt->itemNew.hItem);

            /* handled */
            return TRUE;

        case NM_DBLCLK:
            /*
             *   Double-click - open the target item.  If it's a special file
             *   item that isn't set, bring up the file selector to set it.
             *   Otherwise, open the underlying file.
             */
            target = TreeView_GetSelection(tree_);
            info = get_item_info(target);
            if (info != 0
                && info->type_ == PROJ_ITEM_TYPE_SPECIAL
                && info->fname_ == 0)
            {
                ask_special_file(target);
                return TRUE;
            }

             /*
              *   Due to some quirk in the way the tree control processes
              *   double clicks, focus will be set to the tree after this
              *   message processor returns, so focus won't be in our
              *   newly-opened window if we open it here.  So, handle this
              *   event by posting ourselves our user message "open selected
              *   item"; that will open the item that should have been
              *   selected by tree control's own processing of the
              *   double-click, and by the time we get around to processing
              *   the enter-key notification, the tree should be done with
              *   its aggressive focus setting.
              */
            PostMessage(handle_, W32PRJ_MSG_OPEN_TREE_SEL, 0, 0);

            /* handled */
            return TRUE;

        case NM_CLICK:
            /* get the mouse location in tree-relative coordinates */
            GetCursorPos(&ht.pt);
            ScreenToClient(tree_, &ht.pt);

            /* get the item they clicked on */
            target = TreeView_HitTest(tree_, &ht);

            /* check for a click on a state icon */
            if ((ht.flags & TVHT_ONITEMSTATEICON) != 0)
            {
                proj_item_info *info;

                /*
                 *   get this item's extra information; give up if we can't
                 *   find the information object
                 */
                info = get_item_info(target);
                if (info == 0)
                    return TRUE;

                /*
                 *   if it's an external resource file, reverse the
                 *   installer state
                 */
                if (info->type_ == PROJ_ITEM_TYPE_EXTRES)
                {
                    /* reverse the state */
                    info->in_install_ = !info->in_install_;

                    /* set the new state */
                    set_extres_in_install(target, info->in_install_);

                    /* save the configuration changes */
                    save_config(dbgmain_->get_local_config());

                    /* handled */
                    return TRUE;
                }

                /*
                 *   if it's a source file or a nested library, change its
                 *   exclusion status
                 */
                if (info->type_ == PROJ_ITEM_TYPE_SOURCE
                    || info->type_ == PROJ_ITEM_TYPE_LIB)
                {
                    /* invert the library exclusion status */
                    info->exclude_from_lib_ = !info->exclude_from_lib_;

                    /*
                     *   invert the item's checkmarks (note that the
                     *   checkmark shows *inclusion*, so its value is the
                     *   opposite of our internal 'exclude' value)
                     */
                    set_item_checkmark(target, !info->exclude_from_lib_);

                    /* if it's a library, exclude/include all children */
                    if (info->type_ == PROJ_ITEM_TYPE_LIB)
                        set_exclude_children(target, info->exclude_from_lib_);

                    /*
                     *   if we're including the item, make sure all library
                     *   parents are included
                     */
                    if (!info->exclude_from_lib_)
                        set_include_parents(target);

                    /* update the saved configuration */
                    save_config(dbgmain_->get_local_config());
                }
            }

            /* not handled */
            break;

        case NM_RCLICK:
            /* get the mouse location */
            GetCursorPos(&ht.pt);
            ScreenToClient(handle_, &ht.pt);

            /* determine what they clicked on, and select that item */
            target = TreeView_HitTest(tree_, &ht);
            TreeView_Select(tree_, target, TVGN_CARET);

            /* check what we clicked on, if anything */
            if (target != 0)
            {
                /* get this item info */
                info = get_item_info(target);

                /* if it's a special item, track the special menu */
                if (info->type_ == PROJ_ITEM_TYPE_SPECIAL)
                {
                    track_context_menu(GetSubMenu(special_menu_, 0),
                                       ht.pt.x, ht.pt.y);
                    return TRUE;
                }
            }

            /* run the context menu */
            track_context_menu(ctx_menu_, ht.pt.x, ht.pt.y);

            /* handled */
            return TRUE;

        default:
            /* no special handling required */
            break;
        }
    }

    /* inherit default processing */
    return CTadsWin::do_notify(control_id, notify_code, nm);
}

/*
 *   Set the 'exclude' status for all parents of the given item to *include*
 *   the parents.  This should be used whenever we include an item, to make
 *   sure all of its parents are included as well.
 */
void CHtmlDbgProjWin::set_include_parents(HTREEITEM item)
{
    /* visit each parent */
    HTREEITEM cur, par;
    for (cur = TreeView_GetParent(tree_, item) ; cur != 0 ; cur = par)
    {
        proj_item_info *par_info;
        proj_item_info *cur_info;

        /* get this item's parent */
        par = TreeView_GetParent(tree_, cur);

        /* if there's no parent, we're done */
        if (par == 0)
            break;

        /* get our info and the parent item's info */
        cur_info = get_item_info(cur);
        par_info = get_item_info(par);

        /*
         *   if the parent isn't a library, then we're not a library inside a
         *   library, so we have no 'exclude' status at all; we're done in
         *   this case
         */
        if (par_info->type_ != PROJ_ITEM_TYPE_LIB)
            break;

        /* mark this item as included */
        cur_info->exclude_from_lib_ = FALSE;

        /* set the checkmark to 'checked' to show we're included */
        set_item_checkmark(cur, TRUE);
    }
}

/*
 *   Set the 'exclude' status for all children of the given item, recursively
 *   setting the same value for their children.
 */
void CHtmlDbgProjWin::set_exclude_children(HTREEITEM item, int exclude)
{
    /* visit the item's children */
    for (HTREEITEM cur = TreeView_GetChild(tree_, item) ; cur != 0 ;
         cur = TreeView_GetNextSibling(tree_, cur))
    {
        /* get this item's information */
        proj_item_info *info = get_item_info(cur);

        /* if it's a source or library file, mark it as excluded */
        if (info->type_ == PROJ_ITEM_TYPE_SOURCE
            || info->type_ == PROJ_ITEM_TYPE_LIB)
        {
            /* set its 'exclude' status */
            info->exclude_from_lib_ = exclude;

            /*
             *   set its checkmark (the checkmark shows the 'include' status,
             *   which is the inverse of our 'exclude' status)
             */
            set_item_checkmark(cur, !exclude);

            /* visit its children if needed */
            if (info->type_ == PROJ_ITEM_TYPE_LIB)
                set_exclude_children(cur, exclude);
        }
    }
}

/*
 *   process timer events
 */
int CHtmlDbgProjWin::do_timer(int id)
{
    /* get the mouse position in client coordinates */
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(tree_, &pt);

    /* if we're above or below the client area, scroll */
    RECT crc;
    GetClientRect(tree_, &crc);
    if (pt.y < 0 || pt.y > crc.bottom)
    {
        /*
         *   we need to auto-scroll - find the item at the edge at which
         *   we're out of bounds, and then find the next/previous item and
         *   scroll it into view
         */
        TVHITTESTINFO hti;
        hti.pt.x = pt.x;
        hti.pt.y = (pt.y < 0 ? 1 : crc.bottom - 1);
        if (TreeView_HitTest(tree_, &hti) != 0)
        {
            HTREEITEM cur =
                pt.y < 0
                ? TreeView_GetPrevVisible(tree_, hti.hItem)
                : TreeView_GetNextVisible(tree_, hti.hItem);

            if (cur != 0)
                TreeView_EnsureVisible(tree_, cur);
        }

        /* handled */
        return TRUE;
    }

    /* check for our drag timer */
    if (tracking_drag_ && id == drag_timer_id_)
    {
        /* check for hovering over a closed container */
        if (GetTickCount() - drag_time_ >= GetDoubleClickTime())
        {
            /* get the current drop target */
            HTREEITEM parent, insafter, caret;
            if (get_dragmove_target(pt.x, pt.y, &parent, &insafter, &caret)
                && parent != 0)
            {
                TVITEM tvi;
                tvi.mask = TVIF_HANDLE | TVIF_STATE;
                TreeView_GetItem(tree_, &tvi);
                if (!(tvi.state & TVIS_EXPANDED))
                {
                    /* hide the drag helper visual overlay, if any */
                    DropHelper_Show(FALSE);

                    /*
                     *   remember and remove the drag/drop caret - we need to
                     *   remove it during the expansion because the expansion
                     *   will trigger a redraw, which will invalidate our
                     *   superimposed caret drawing
                     */
                    HTREEITEM orig = last_drop_target_;
                    int sel = last_drop_target_sel_;
                    draw_dragmove_caret(0, FALSE);

                    /* expand it */
                    TreeView_Expand(tree_, caret, TVE_EXPAND);

                    /* restore the original drag/drop caret */
                    draw_dragmove_caret(orig, sel);

                    /* restore the drag helper visual overlay */
                    DropHelper_Show(TRUE);
                }
            }
        }

        /* handled */
        return TRUE;
    }

    /* not ours; inherit the default handling */
    return CTadsWin::do_timer(id);
}

/*
 *   process a mouse move
 */
int CHtmlDbgProjWin::do_mousemove(int keys, int x, int y)
{
    /* if we're tracking a drag event, process it */
    if (tracking_drag_)
        track_drag_drop(keys, x, y);

    /* inherit default handling */
    return CTadsWin::do_mousemove(keys, x, y);
}

/*
 *   mouse up event
 */
int CHtmlDbgProjWin::do_leftbtn_up(int keys, int x, int y)
{
    /* if we're tracking a drag event, process it */
    if (tracking_drag_)
        execute_drag_drop(keys, x, y, 0);

    /* inherit default */
    return CTadsWin::do_leftbtn_up(keys, x, y);
}

/*
 *   Move an item in the tree to a new location.  Returns the new item's
 *   handle (moving the item requires deleting the item and inserting a new
 *   one, so its handle changes in the process).  We recursively move all of
 *   the item's children with the item.
 */
HTREEITEM CHtmlDbgProjWin::move_tree_item(HTREEITEM item,
                                          HTREEITEM new_parent,
                                          HTREEITEM ins_after,
                                          HTREEITEM caret,
                                          int depth)
{
    TV_INSERTSTRUCT tvi;
    char txtbuf[_MAX_PATH + 512];
    HTREEITEM new_item;
    HTREEITEM chi;
    proj_item_info *info;

    /* special handling if we're dropping onto a Special File item */
    if (caret != 0
        && (info = get_item_info(caret)) != 0
        && info->type_ == PROJ_ITEM_TYPE_SPECIAL)
    {
        /* get the source item info */
        info = get_item_info(item);

        /* set the special item to this file */
        set_special_file(caret, info->fname_, TRUE);

        /* done - the affected target item is the drop target */
        return caret;
    }

    /*
     *   If we're moving the item to a new parent, insert a new item pointing
     *   to the same file, so that we set up all of the correct attributes
     *   for a child of the new parent; otherwise simply move the item, so
     *   that we retain its full set of attributes.
     *
     *   If this is a recursive call, we're simply moving an item from its
     *   original parent to its moved parent, so we can treat this as a
     *   same-parent move.
     */
    if (new_parent == TreeView_GetParent(tree_, item) || depth != 0)
    {
        /*
         *   we're simply reordering the parent's sibling list, so we can
         *   keep the same item; set up to insert a new item at the specified
         *   target position
         */
        tvi.hParent = new_parent;
        tvi.hInsertAfter = ins_after;

        /*
         *   get the item's current information, so we can replicate the
         *   information in the new item we'll insert
         */
        tvi.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM
                        | TVIF_IMAGE | TVIF_SELECTEDIMAGE
                        | TVIF_CHILDREN | TVIF_HANDLE;
        tvi.item.hItem = item;
        tvi.item.stateMask = (UINT)~0;
        tvi.item.pszText = (char *)txtbuf;
        tvi.item.cchTextMax = sizeof(txtbuf);
        TreeView_GetItem(tree_, &tvi.item);

        /* insert a copy of the item at the new location */
        new_item = TreeView_InsertItem(tree_, &tvi);

        /* move each child of this item to the new parent's child list */
        for (chi = TreeView_GetChild(tree_, item) ; chi != 0 ;
             chi = TreeView_GetChild(tree_, item))
        {
            /* move this child so it's a child of the new parent */
            move_tree_item(chi, new_item, TVI_LAST, 0, depth + 1);
        }

        /*
         *   unlink the old tree item from its info structure, so that the
         *   info structure isn't deleted when we delete the old item - we
         *   want to re-use the same info structure with the new item we're
         *   about to insert
         */
        forget_item_info(item);
    }
    else
    {
        proj_item_info *new_info;

        /*
         *   If the item has any children, don't allow moving it to a new
         *   parent.  This currently shouldn't be possible, since the only
         *   movable items with children are libraries (.tl files), and we
         *   don't allow moving these out of the Source Files list.  So, if
         *   we have any children, simply return the item unchanged without
         *   moving it.
         */
        if (TreeView_GetChild(tree_, item) != 0)
            return item;

        /*
         *   We're moving the item to a new parent, so we need to add a brand
         *   new item under the new parent, and delete the old item.
         */
        info = get_item_info(item);
        new_item = add_file_to_project(new_parent, ins_after, info->fname_,
                                       PROJ_ITEM_TYPE_NONE, TRUE);

        /* if that failed, do not proceed */
        if (new_item == 0)
            return item;

        /* get the new item's info */
        new_info = get_item_info(new_item);

        /* copy the Start Menu title */
        set_file_title(new_item, info->start_menu_title_.get(), TRUE);
    }

    /* delete the old item */
    TreeView_DeleteItem(tree_, item);

    /* return a handle to the new item */
    return new_item;
}

/*
 *   process a capture change
 */
int CHtmlDbgProjWin::do_capture_changed(HWND new_capture_win)
{
    /*
     *   if we're tracking a drag event, this must have been an involuntary
     *   capture change, so cancel the drag/drop tracking
     */
    if (tracking_drag_)
        cancel_drag_drop();

    /* inherit default */
    return CTadsWin::do_capture_changed(new_capture_win);
}


/*
 *   Clear project information from a configuration object
 */
void CHtmlDbgProjWin::clear_proj_config(CHtmlDebugConfig *config)
{
    int cnt;
    int i;
    static const char *specials[] = {
        "readme", "license", "cover art", "exe icon", "cmaplib",
        "web builder" };

    class CClearExclusions: public IHtmlDebugConfigEnum
    {
    public:
        CClearExclusions(CHtmlDebugConfig *c) { config = c; }
        CHtmlDebugConfig *config;
        void enum_var(const textchar_t *varname, const textchar_t *ele)
        {
            if (strcmp(varname, "build") == 0
                && memcmp(ele, "lib_exclude:", 12) == 0)
                config->clear_strlist(varname, ele);
        }
    } cbClearExclusions(config);

    /* clear the project file lists */
    config->clear_strlist("build", "source_files");
    config->clear_strlist("build", "source_file_types");
    config->clear_strlist("build", "include_files");
    config->clear_strlist("build", "image_resources");
    config->clear_strlist("build", "image_resources_recurse");
    config->clear_strlist("build", "ext_resfile_in_install");
    config->clear_strlist("build", "feelies");
    config->clear_strlist("build", "web extras");
    config->clear_strlist("build", "miscellaneous");
    config->enum_values(&cbClearExclusions);

    /* clear the special file list */
    for (i = 0 ; i < sizeof(specials)/sizeof(specials[0]) ; ++i)
    {
        config->clear_strlist("build special file", specials[i]);
        config->clear_strlist("build special file title", specials[i]);
    }

    /* clear each external resource file list */
    if (config->getval("build", "ext resfile cnt", &cnt))
        cnt = 0;
    for (i = 0 ; i < cnt ; ++i)
    {
        char groupname[50];

        /* build the config group name for this file */
        gen_extres_names(0, groupname, i);

        /* clear this list */
        config->clear_strlist("build", groupname);
    }

    /* clear the external resource counter */
    config->setval("build", "ext resfile cnt", 0);
}
