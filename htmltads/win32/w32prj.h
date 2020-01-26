/* $Header$ */

/*
 *   Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32prj.h - TADS 3 debugger for 32-bit Windows - project window
Function

Notes

Modified
  01/14/00 MJRoberts  - Creation
*/

#ifndef W32PRJ_H
#define W32PRJ_H

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef W32TDB_H
#include "w32tdb.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   treeview item types
 */
enum proj_item_type_t
{
    /* none */
    PROJ_ITEM_TYPE_NONE,

    /* the main project */
    PROJ_ITEM_TYPE_PROJECT,

    /* folder */
    PROJ_ITEM_TYPE_FOLDER,

    /* source (.t) file */
    PROJ_ITEM_TYPE_SOURCE,

    /* library (.tl) file */
    PROJ_ITEM_TYPE_LIB,

    /* header (.h) file */
    PROJ_ITEM_TYPE_HEADER,

    /* resource file (sound, image) */
    PROJ_ITEM_TYPE_RESOURCE,

    /* external resource file */
    PROJ_ITEM_TYPE_EXTRES,

    /* "feelie" - miscellaneous file to bundle with the released game */
    PROJ_ITEM_TYPE_FEELIE,

    /* miscellenaous file - part of project source, but not in release */
    PROJ_ITEM_TYPE_MISC,

    /* web extra - miscellaneous file to include in generated web page */
    PROJ_ITEM_TYPE_WEBEXTRA,

    /*
     *   Special file - a file with a dedicated special purpose (cover art,
     *   license, readme, .EXE icon)
     */
    PROJ_ITEM_TYPE_SPECIAL,

    /* javascript file (.js) */
    PROJ_ITEM_TYPE_JS,

    /* HTML file (.htm, html) */
    PROJ_ITEM_TYPE_HTML,

    /* XML file (.xml) */
    PROJ_ITEM_TYPE_XML,

    /* CSS file (.css) */
    PROJ_ITEM_TYPE_CSS
};


/* ------------------------------------------------------------------------ */
/*
 *   Source file enumerator callback
 */
class IProjWinFileCallback
{
public:
    virtual ~IProjWinFileCallback() { }
    virtual void file_cb(const textchar_t *fname,
                         const textchar_t *relname,
                         proj_item_type_t typ) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Project window
 */
class CHtmlDbgProjWin: public CTadsWin, public CHtmlDbgSubWin
{
    friend class CTcLibParserProj;

public:
    CHtmlDbgProjWin(class CHtmlSys_dbgmain *dbgmain);
    ~CHtmlDbgProjWin();

    /*
     *   Find the full path to a file, given the file's name.  This can be
     *   used to find a file in the project list when only the file's root
     *   name is known, such as when a file must be found based on debugging
     *   records.
     *
     *   If we can find the file, we'll fill in the fullname buffer with the
     *   full path to the file and return true; otherwise, we'll return
     *   false.
     */
    int find_file_path(char *fullname, size_t fullname_len,
                       const char *name);

    /*
     *   Refresh the window after application activation.  This is used to
     *   check for libraries that need to be reloaded because they've been
     *   modified since the initial load.
     */
    void refresh_for_activation();

    /*
     *   Unconditionally refresh all libraries in the tree.  This should be
     *   called whenever a macro definition changes, to ensure that any
     *   libraries with inclusions based on macros are reparsed with the new
     *   macro values.
     */
    void refresh_all_libs();

    /* process system window creation */
    void do_create();

    /* process window destruction */
    void do_destroy();

    /* we're a child window */
    DWORD get_winstyle()
    {
        return (WS_CHILD | WS_CLIPSIBLINGS);
    }
    DWORD get_winstyle_ex() { return 0; }

    /* save my window configuration */
    void save_win_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname);

    /* load my window configuration */
    void load_win_config(class CHtmlDebugConfig *config,
                         const textchar_t *varname,
                         RECT *pos, int *vis);

    /* resize the window */
    void do_resize(int mode, int x, int y);

    /* activate/disactivate */
    int do_ncactivate(int flag);

    /* parent activate/disactivate */
    void do_parent_activate(int flag);

    /* no need to erase our background, as the tree covers the whole thing */
    virtual int do_erase_bkg(HDC) { return TRUE; }

    /* gain focus */
    void do_setfocus(HWND prev_win);

    /* process a notification message */
    int do_notify(int control_id, int notify_code, LPNMHDR nmhdr);

    /* process a user-defined message */
    int do_user_message(int msg, WPARAM, LPARAM);

    /* process a command */
    int do_command(int notify_code, int command_id, HWND ctl);

    /* check the status of a command */
    TadsCmdStat_t check_command(const check_cmd_info *);

    /* save the configuration */
    void save_config(class CHtmlDebugConfig *config);

    /*
     *   Auto-add a file.  This adds a file to a project section based on the
     *   file type.
     */
    void auto_add_file(const char *fname);

    /* remove a file by name */
    void remove_proj_file(const char *fname);

    /* is the given file in the project? */
    int is_file_in_project(const char *fname);

    /* note that a file has been saved */
    void note_file_save(const char *fname);

    /*
     *   Get the next text file for a multi-file search.  If we can find
     *   another file, we fill in 'fname' with the name of the next file in
     *   line and return TRUE.  If there are no more files, we return FALSE.
     *   'direction' is >0 for a forward search, <0 for a backward search.
     *
     *   'local_only' indicates whether we're looking at local project files
     *   (i.e., files within the project directory), or at the entire project
     *   (including extensions and libraries).
     */
    int get_next_search_file(char *fname, size_t fname_size,
                             const char *curfile, int direction, int wrap,
                             int local_only);

    /*
     *   Add a scanned #include file.  This is called by the compiler output
     *   processor when the compiler writes out a "#include" line in "-Pi"
     *   (scan for #include files) mode.  If the named file is not already
     *   listed in our "Include Files" section, we'll add the file to the
     *   end of the Include Files list.
     */
    void add_scanned_include(const char *fname);

    /* clear the include files */
    void clear_includes();

    /*
     *   enumerate all text files, including source files and resource files
     *   that are of a text type
     */
    void enum_text_files(IProjWinFileCallback *cb);

    /* enumerate all files in the project, including library contents */
    void enum_all_files(IProjWinFileCallback *cb);


    /* -------------------------------------------------------------------- */
    /*
     *   CHtmlDbgSubWin implementation
     */

    /* check the status of a command routed from the main window */
    virtual TadsCmdStat_t active_check_command(const check_cmd_info *);

    /* execute a command routed from the main window */
    virtual int active_do_command(int notify_code, int command_id,
                                  HWND ctl);

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

    /*
     *   generate the display and configuration group names for an
     *   external resource file
     */
    static void gen_extres_names(char *display_name,
                                 char *config_group_name, int fileno);

    /* clear project information from a configuration */
    static void clear_proj_config(class CHtmlDebugConfig *config);

    /* reset the project window - clear all sources, includes, etc */
    void reset_proj_win();

    /* handle timer messages */
    virtual int do_timer(int timer_id);

    /* process a mouse move event */
    virtual int do_mousemove(int keys, int x, int y);

    /* process a left mouse button up event */
    virtual int do_leftbtn_up(int keys, int x, int y);

    /* process a capture change */
    virtual int do_capture_changed(HWND new_capture_win);

    /*
     *   IDropTarget implementation
     */
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject __RPC_FAR *pDataObj,
                                        DWORD grfKeyState, POINTL pt,
                                        DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt,
                                       DWORD __RPC_FAR *pdwEffect);
    HRESULT STDMETHODCALLTYPE DragLeave();
    HRESULT STDMETHODCALLTYPE Drop(IDataObject __RPC_FAR *pDataObj,
                                   DWORD grfKeyState, POINTL pt,
                                   DWORD __RPC_FAR *pdwEffect);

protected:
    /* is the given file a child (recursively defined) or parent? */
    int is_file_child_of(HTREEITEM parent, const char *fname);

    /* enumerate text files for 'cur' and its children */
    void enum_text_files(IProjWinFileCallback *cb, HTREEITEM cur);

    /* enumerate all project files for 'cur' and its children */
    void enum_all_files(IProjWinFileCallback *cb, HTREEITEM cur);

    /* enumerate files in a directory */
    void enum_files_in_dir(
        IProjWinFileCallback *cb,
        const textchar_t *dir, const textchar_t *reldir,
        proj_item_type_t child_type,
        int include_dirs, int recurse, int include_dotdirs);

    /* find a file among the children of the given tree item */
    int find_file_path_in_tree(
        HTREEITEM parent, char *fullname, size_t fullname_len,
        const char *root_name);

    /* find the tree item corresponding to the given file */
    HTREEITEM find_item_for_file(HTREEITEM parent, const textchar_t *fname);

    /* refresh children of the given parent for application activation */
    void refresh_lib_tree(HTREEITEM parent, int always_reload);
    void refresh_lib_tree(class CHtmlDebugConfig *config,
                          HTREEITEM parent, int always_reload);

    /*
     *   scan the source tree for a library whose path matches the given
     *   file's directory
     */
    int scan_for_lib_inc_path(const char *fname,
                              char *parent_path, size_t parent_path_size,
                              char *rel_fname, size_t rel_fname_size);

    /*
     *   service routine for scan_for_lib_inc_path - scan the tree
     *   recursively from the given parent
     */
    int scan_for_lib_inc_path_in(
        HTREEITEM tree_item, const char *path, const char *fname,
        char *parent_path, size_t parent_path_size,
        char *rel_fname, size_t rel_fname_size);

    /*
     *   scan the source tree for a library containing the given file; if we
     *   find one, expand the filename to include the full path prefix for
     *   the directory containing the library
     */
    void add_lib_header_path(char *fname, size_t fname_size);

    /*
     *   service routine for add_lib_header_path - scan recursively starting
     *   at the given parent
     */
    int add_lib_header_path_scan(HTREEITEM parent_item,
                                 char *fname, size_t fname_size);

    /* get an item's file modification timestamp */
    void get_item_timestamp(HTREEITEM item, FILETIME *ft);

    /*
     *   refresh a library - reload the library and rebuild the subtree for
     *   the library's files
     */
    void refresh_library(class CHtmlDebugConfig *config, HTREEITEM lib_item);

    /* create the tree control */
    void create_tree();

    /* insert an item into our tree view control after the given item */
    HTREEITEM insert_tree(HTREEITEM parent, HTREEITEM ins_after,
                          const textchar_t *txt, DWORD state,
                          int icon_index, struct proj_item_info *info);

    /* insert an item in our tree view control at the end of a list */
    HTREEITEM append_tree(HTREEITEM parent, const textchar_t *txt,
                          DWORD state, int icon_index,
                          struct proj_item_info *info)
    {
        /* simply insert the item after the last child of the parent */
        return insert_tree(parent, TVI_LAST, txt, state, icon_index, info);
    }

    /* prompt the user for a file, and add it to the project */
    void add_project_file();

    /* prompt the user for a folder, and add it to the project */
    void add_project_dir();

    /* refresh a resource folder */
    void refresh_folder(HTREEITEM item);

    /* add an external resource file */
    void add_ext_resfile();

    /* delete the current selection in the tree */
    void delete_tree_selection();

    /*
     *   Add a file to the project.  If 'manual' is true, it means that the
     *   user explicitly selected the file for addition to the project;
     *   otherwise, it means that we're adding the file as part of a loaded
     *   configuration or from a library or the like.
     */
    HTREEITEM add_file_to_project(HTREEITEM parent, HTREEITEM after,
                                  const char *fname,
                                  proj_item_type_t known_type, int manual);

    /* open an item */
    void open_item(HTREEITEM item);

    /* open an item in the text editor */
    void open_item_in_text_editor(HTREEITEM item);

    /* get the path to an item */
    void get_item_path(char *buf, size_t buflen, HTREEITEM item);

    /* get an item's URL-style path */
    void get_item_lib_url(char *url, size_t url_len, HTREEITEM item);

    /* get the directory that will contain children of an item */
    void get_item_child_dir(char *buf, size_t buflen, HTREEITEM item);

    /* get the proj_item_info for a tree item */
    struct proj_item_info *get_item_info(HTREEITEM hitem) const
    {
        /* if the item is null or the special root item, there's no info */
        if (hitem == 0 || hitem == TVI_ROOT)
            return 0;

        /* ask the tree to fill in the TV_ITEM structure for us */
        TV_ITEM item;
        item.hItem = hitem;
        item.mask = TVIF_PARAM;
        if (!TreeView_GetItem(tree_, &item))
            return 0;

        /* return the lParam from the TV_ITEM - this is our info pointer */
        return (proj_item_info *)item.lParam;
    }

    /* move a tree item to a new position */
    HTREEITEM move_tree_item(HTREEITEM item,
                             HTREEITEM new_parent,
                             HTREEITEM ins_after,
                             HTREEITEM caret,
                             int recurse_depth = 0);

    /* forget the proj_item_info for a tree item */
    void forget_item_info(HTREEITEM hitem)
    {
        TV_ITEM item;

        /* set the lParam in this item to null */
        item.hItem = hitem;
        item.mask = TVIF_PARAM;
        item.lParam = 0;

        /* set the lParam in the item */
        TreeView_SetItem(tree_, &item);
    }

    /* enter a drag/drop operation */
    void enter_drag_drop(HTREEITEM item);

    /* cancel a drag/drop operation */
    void cancel_drag_drop();

    /* execute a drop operation */
    void execute_drag_drop(int keys, int x, int y,
                           IDataObject __RPC_FAR *dataObj);

    /*
     *   track a mouse-move event during drag/drop; returns a suitable
     *   DROPEFFECT_xxx code if this is a valid drop location,
     *   DROPEFFECT_NONE if not
     */
    enum DROPEFFECT track_drag_drop(int keys, int x, int y);

    /* get the drop location for a drag-move operation */
    int get_dragmove_target(
        int x, int y,  HTREEITEM *parent, HTREEITEM *insafter,
        HTREEITEM *caretafter) const;

    /* draw the drag/drop insertion caret */
    void draw_dragmove_caret(HTREEITEM target, int sel);

    /*
     *   determine if a drag-move onto the given target is allowed
     */
    int dragmove_allowed(
        HTREEITEM parent, HTREEITEM insafter,
        HTREEITEM caret, int *sel) const;

    /* process a drop-files event in the tree control */
    void do_tree_dropfiles(HDROP hdrop, int x, int y);

    /*
     *   fix up the external resource file numbers - this should be called
     *   whenever we insert or delete an external resource file
     */
    void fix_up_extres_numbers();

    /* set an external resource item's in-install icon */
    void set_extres_in_install(HTREEITEM item, int in_install);

    /* set a tree item to show a checkmark state icon */
    void set_item_checkmark(HTREEITEM item, int checked);

    /* set the 'exclude' status for all children of the given item */
    void set_exclude_children(HTREEITEM item, int exclude);

    /* set the 'exclude' status to 'included' for parents of the given item */
    void set_include_parents(HTREEITEM item);

    /* load files from the configuration into a given container */
    void load_config_files(class CHtmlDebugConfig *config, HTREEITEM parent);

    /* load library exclusion information for a library */
    void load_config_lib_exclude(CHtmlDebugConfig *config, HTREEITEM lib,
                                 proj_item_info *lib_info);

    /* load a Special File item */
    void load_special_item(CHtmlDebugConfig *config, HTREEITEM parent,
                           const char *title, const char *fname_filter,
                           const char *subkey, int has_start_menu_title);

    /* ask for the Start Menu title for an item */
    void ask_file_title(HTREEITEM sel);

    /* update an item's display name */
    void update_item_dispname(HTREEITEM item, proj_item_info *info);

    /* set a Special File item's file */
    void set_special_file(HTREEITEM chi, const char *fname, int save);

    /* set the Start Menu title for a special item or feelie */
    void set_file_title(HTREEITEM chi, const char *title, int save);

    /* ask for a special file via a file-open dialog */
    void ask_special_file(HTREEITEM sel);

    /*
     *   search children of the given parent, and children of those
     *   children, for the given URL, and mark the given item as excluded;
     *   returns true if we find the item, false if not
     */
    int load_config_lib_exclude_url(HTREEITEM parent, const char *url);

    /* save files from the given section into the configuration */
    void save_config_files(class CHtmlDebugConfig *config, HTREEITEM parent);

    /* save file exclusion information for the given top-level library */
    void save_config_lib_excl(class CHtmlDebugConfig *config, HTREEITEM lib);

    /* save file exclusion information for children of the given library */
    void save_config_lib_excl_children(
        class CHtmlDebugConfig *config, HTREEITEM lib,
        const char *var_name, int *idx);

    /* debugger main window object */
    class CHtmlSys_dbgmain *dbgmain_;

    /*
     *   flag: a project is open in the debugger (if not, we offer only
     *   minimal user interface, since if there's no project open there's
     *   really nothing the project window is good for)
     */
    unsigned int proj_open_ : 1;

    /* my tree view control */
    HWND tree_;

    /* tree item for the 'source files' group */
    HTREEITEM src_item_;

    /* tree item for the 'include files' group */
    HTREEITEM inc_item_;

    /* tree item for the 'image file resource files' group */
    HTREEITEM res_item_;

    /* tree item for the 'feelies' group */
    HTREEITEM feelies_item_;

    /* context menu for project window */
    HMENU ctx_menu_;

    /* context menu container */
    HMENU ctx_menu_container_;

    /* context menu for special items */
    HMENU special_menu_;

    /* image list */
    HIMAGELIST img_list_;

    /* state image list */
    HIMAGELIST state_img_list_;

    /* indices of our images in the image list */
    int img_idx_root_;
    int img_idx_folder_;
    int img_idx_source_;
    int img_idx_resource_;
    int img_idx_extres_;
    int img_idx_res_folder_;
    int img_idx_lib_;
    int img_idx_webextra_;

    /* indices of images in our state image list */
    int img_idx_in_install_;
    int img_idx_not_in_install_;
    int img_idx_check_;
    int img_idx_nocheck_;

    /* flag: tracking a drag operation in the tree */
    int tracking_drag_;

    /* time and position of last drag mouse event */
    DWORD drag_time_;
    POINT drag_pos_;

    /* drag timer ID */
    int drag_timer_id_;

    /* last drag/drop target item; and, was it drawn as selected? */
    HTREEITEM last_drop_target_;
    int last_drop_target_sel_;

    /* item being dragged, and information on the item */
    HTREEITEM drag_item_;
    struct proj_item_info *drag_info_;

    /* cursors for drag operations */
    static HCURSOR no_csr_;
    static HCURSOR move_csr_;
    static HCURSOR link_csr_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Library path searcher.  We'll look through the "srcdir" settings in the
 *   global configuration, then through the entries in the TADSLIB
 *   environment variable, if there is one.
 */
class CLibSearchPathList
{
public:
    CLibSearchPathList(class CHtmlDebugConfig *config);
    ~CLibSearchPathList();

    /*
     *   Conduct a search for the given .tl file, returning the full path of
     *   the first place we find the file.  If we find the file, we'll return
     *   true and fill in 'path' with the directory containing the file; if
     *   not, we'll just return false.
     */
    static int find_tl_file(class CHtmlDebugConfig *config,
                            const char *fname, char *path, size_t pathlen);

    /* is there anything left in the search? */
    int has_more() const;

    /* get the next entry; returns true on success, false on failure */
    int get_next(char *buf, size_t buflen);

    /* reset to the start of the search */
    void reset();

private:
    /* our config object */
    class CHtmlDebugConfig *config_;

    /* the number of entries in the "srcdir" global configuration variable */
    int srcdir_cnt_;

    /* current "srcdir" entry */
    int srcdir_cur_;

    /* current "extensions dir" entry */
    int exts_cur_;

    /* the value of the TADSLIB environment variable */
    char *tadslib_;

    /* current read point in TADSLIB string */
    char *tadslib_p_;
};


#endif /* W32PRJ_H */

