/* $Header: d:/cvsroot/tads/html/win32/tadsapp.h,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $ */

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsapp.h - TADS Windows application object
Function
  This object provides a repository for data and methods associated
  with the application.
Notes

Modified
  09/16/97 MJRoberts  - Creation
*/

#ifndef TADSAPP_H
#define TADSAPP_H

#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   accelerator save/restore structure
 */
struct app_accel_info
{
    /* old accelerator */
    HACCEL waccel;
    class CTadsAccelerator *taccel;
    class CTadsWin *accel_win;

    /* new accelerator being set */
    HACCEL new_waccel;
    class CTadsAccelerator *new_taccel;
    class CTadsWin *new_accel_win;
};

/* ------------------------------------------------------------------------ */
/*
 *   Application object
 */
class CTadsApp
{
public:
    /*
     *   Run the main event loop.  Returns either when *flag becomes
     *   non-zero, or when the application is terminated by some external
     *   or user action.  If the application is terminated, we'll return
     *   false; if the application should keep running, we'll return true.
     */
    int event_loop(int *flag);

    /*
     *   Apply the standard processing to a message read from the Windows
     *   system event queue.  This looks for modeless dialogs that want the
     *   message, then looks for system MDI accelerator translations, then
     *   looks for our own accelerator translations; failing all of that, we
     *   dispatch the message to the target window normally.
     */
    void process_message(MSG *msg);

    /*
     *   Clear the current accelerator if it's associated with the given
     *   window.  This is used when the window is destroyed to ensure that we
     *   clear our memory of its accelerator.
     */
    void closing_accel_win(class CTadsWin *win);

    /*
     *   Set the current accelerator to a Windows native accelerator
     *   resource.  If accel is not null, we'll use the handle to translate
     *   accelerators and send resulting commands to the indicated window
     *   handle.
     */
    void set_accel(HACCEL accel, class CTadsWin *win);

    /* set the accelerator to a CTadsAccelerator object */
    void set_accel(class CTadsAccelerator *accel, class CTadsWin *win);

    /* set no accelerator */
    void clear_accel();

    /* enable or temporarily disable accelerators */
    int enable_accel(int ena)
    {
        int ret = accel_enabled_;
        accel_enabled_ = ena;
        return ret;
    }

    /* save the accelerator settings, and set up a new accelerator */
    void push_accel(app_accel_info *info, HACCEL accel, class CTadsWin *win)
    {
        /* save the old accelerator settings */
        get_accel(info);

        /* remember the new settings as well */
        info->new_waccel = accel;
        info->new_accel_win = win;

        /* establish the new settings */
        set_accel(accel, win);
    }
    void push_accel(app_accel_info *info,
                    class CTadsAccelerator *accel, class CTadsWin *win)
    {
        /* save the old accelerator settings */
        get_accel(info);

        /* remember the new settings as well */
        info->new_taccel = accel;
        info->new_accel_win = win;

        /* establish the new settings */
        set_accel(accel, win);
    }
    void push_accel_clear(app_accel_info *info)
    {
        /* save the old settings */
        get_accel(info);

        /* clear the accelerator */
        clear_accel();
    }

    /*
     *   Pop a saved accelerator.  If the current accelerators don't match
     *   the 'new' accelerator in the stack structure, we'll do nothing, as
     *   presumably a change in window activation has established a new
     *   accelerator before we got around to restoring the old one.
     */
    void pop_accel(app_accel_info *info)
    {
        /*
         *   if the current accelerator doesn't match the 'new' accelerator
         *   in the stack structure, don't restore - window activation must
         *   have changed and established a new current accelerator
         */
        if (info->new_waccel != waccel_
            || info->new_taccel != taccel_
            || info->new_accel_win != accel_win_)
            return;

        /* restore the old settings */
        set_accel(info);
    }

    /* get an empty accelerator table */
    HACCEL get_empty_accel() const { return empty_accel_; }

    /*
     *   Get the accelerator key translation for a message, if any.  Returns
     *   true if there's a translation, false if not.  We make all checks
     *   that we'd ordinarly make in translating an accelerator key,
     *   including checking for a disabled menu item; but rather than
     *   actually executing the command, we merely return its translation.
     *   If the message has an active translation but doesn't result in a
     *   command (for example, because it's the first key of a multi-key
     *   chord), we return true and fill in WM_NULL as the message.  Note
     *   that we won't adjust the accelerator state, though, since we're only
     *   looking up the message and not actually executing it.
     */
    int get_accel_translation(MSG *msg);

    /*
     *   translate a message via keyboard accelerators and carry it out;
     *   returns true if we translated the message, false if not
     */
    int accel_translate(MSG *msg);

    /* do idle-time processing */
    void do_idle();

    /* create the application object */
    static void create_app(HINSTANCE inst, HINSTANCE pinst,
                           LPSTR cmdline, int cmdshow);

    /* get the main application object */
    static CTadsApp *get_app() { return app_; }

    /* delete the application object */
    static void delete_app();

    /* get the application instance handle */
    HINSTANCE get_instance() { return instance_; }

    /*
     *   Get a font matching a given description.  If we already have a
     *   font matching the description, we'll return it and set *created
     *   to false; otherwise, we'll invoke the callback function (if
     *   provided; if not, we'll use a default creation function) to
     *   create the font, and set *created to true.
     */
    class CTadsFont *get_font(const CTadsLOGFONT *font,
                              class CTadsFont *
                              (*create_func)(const CTadsLOGFONT *),
                              int *created);

    /*
     *   Get a font, given a brief description of the font.  The font will
     *   be a proportional roman font in normal window-text color.  This
     *   does the same thing as the version of get_font() based on a
     *   CTadsLOGFONT structure, but is somewhat simpler to call when the
     *   standard defaults can be used in the CTadsLOGFONT structure.
     */
    class CTadsFont *get_font(const char *font_name, int point_size)
        { return get_font(font_name, point_size, 0, 0,
                          GetSysColor(COLOR_WINDOWTEXT),
                          FF_ROMAN | VARIABLE_PITCH); }

    /*
     *   Get a font, given a brief description.  'attributes' contains the
     *   Windows attribute flags (a combination of FF_xxx and xxx_PITCH
     *   flags: FF_SWISS | VARIABLE_PITCH for a proportional sans serif
     *   font) to use if the given font name cannot be found.
     */
    class CTadsFont *get_font(const char *font_name, int point_size,
                              int bold, int italic, int underline,
                              COLORREF fgcolor, int use_fgcolor,
                              COLORREF bgcolor, int use_bgcolor,
                              int attributes);

    class CTadsFont *get_font(const char *font_name, int point_size,
                              int bold, int italic, COLORREF color,
                              int attributes)
    {
        return get_font(font_name, point_size, bold, italic, FALSE,
                        color, TRUE, 0, FALSE, attributes);
    }

    /* create a bold-face version of a given system font */
    static HFONT make_bold_font(HFONT font);

    /*
     *   Get/set the directory for the standard file dialogs.  If the
     *   application wants to keep track of the directory last used for
     *   these dialogs without actually changing the directory, it can use
     *   these operations.  Simply set the lpstrInitialDir member of the
     *   dialog control structure to get_openfile_dir() before calling the
     *   open file function, and then call set_openfile_dir() with the
     *   resulting file to save the directory for next time.
     */
    char *get_openfile_dir() const { return open_file_dir_; }
    void set_openfile_dir(const char *fname)
        { set_openfile_dir_or_fname(fname, TRUE); }

    /*
     *   set the openfile directory to a path - this assumes that the
     *   caller already has a directory path without an attached filename
     */
    void set_openfile_path(const char *fname)
      { set_openfile_dir_or_fname(fname, FALSE); }

    /*
     *   Set the message capture filter.  If this is set, all input will be
     *   sent here before it's processed through the normal Windows
     *   translation and dispatching routines (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).
     *
     *   We only maintain one capture filter; callers are responsible for
     *   properly chaining and restoring filters as needed.  For the caller's
     *   convenience in chaining and restoring the older filter, we return
     *   the old filter object.
     */
    class CTadsMsgFilter *set_msg_filter(class CTadsMsgFilter *filter)
    {
        class CTadsMsgFilter *old_filter;

        /* remember the outgoing filter for a moment */
        old_filter = msg_filter_;

        /* set the new filter */
        msg_filter_ = filter;

        /* return the old filter */
        return old_filter;
    }

    /*
     *   Add a modeless dialog to our list.  Each modeless dialog that's
     *   running must be added to this list in order for keyboard events
     *   to be properly distributed to it.
     */
    void add_modeless(HWND win);

    /*
     *   Remove a modeless dialog from our list.  A dialog that's been
     *   added to our list must be removed before its window is destroyed,
     *   so that we don't refer to it after it's gone.
     */
    void remove_modeless(HWND win);

    /*
     *   Set the MDI frame window.  This should be called when the MDI
     *   frame is first created to set the MDI window.  This should also
     *   be called with a null argument before the MDI window is
     *   destroyed.
     */
    void set_mdi_win(class CTadsSyswinMdiFrame *mdi_win)
        { mdi_win_ = mdi_win; }

    /* get the Windows OS version information */
    unsigned long get_win_sys_id() const { return win_sys_id_; }
    unsigned long get_win_ver_major() const { return win_ver_major_; }
    unsigned long get_win_ver_minor() const { return win_ver_minor_; }

    /* is this Vista or later? */
    int is_win_vista_plus() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 6);
    }

    /* determine if the OS is win95 or NT4 (and only those - not later) */
    int is_win95_or_nt4() const
    {
        /*
         *   win95 is major version 4, minor 0, platform WIN32_WINDOWS; NT4
         *   is major 4, minor 0, platform WIN32_NT
         */
        return ((win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                 && win_ver_major_ == 4 && win_ver_minor_ == 0)
                || (win_sys_id_ == VER_PLATFORM_WIN32_NT
                    && win_ver_major_ == 4 && win_ver_minor_ == 0));
    }

    /* determine if the OS is win98 (and only win98) */
    int is_win98() const
    {
        /* win98 is major version 4, minor version 10 */
        return (win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                && (win_ver_major_ == 4 && win_ver_minor_ == 10));
    }

    /* determine if the OS is win98 or later */
    int is_win98_or_later() const
    {
        /*
         *   win95 is major version 4, minor version 0; anything with a
         *   higher major version than 4, or a major version of 4 and a minor
         *   version higher than 0, is post-win95, which means win98 or later
         */
        return (win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                && (win_ver_major_ > 4
                    || (win_ver_major_ == 4 && win_ver_minor_ > 0)));
    }

    /* determine if the OS is NT4 or later (this includes 95, 98, and ME) */
    int is_nt4_or_later() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 4);
    }

    /* determine if the OS is Windows 2000 (and ONLY Win2k) */
    int is_win2k() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ == 5
                && win_ver_minor_ == 0);
    }

    /* determine if we're on Windows 2k or later */
    int is_win2k_or_later() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 5);
    }

    /* get the "My Documents" folder path */
    static int get_my_docs_path(char *fname, size_t fname_len);

    /* keyboard utilities singleton */
    static class CTadsKeyboard *kb_;

    /*
     *   System menu font, in normal and bold renditions
     */
    static HFONT menu_font;
    static HFONT bold_menu_font;

    /*
     *   Get this thread's TLS object.  If the thread doesn't have a TLS
     *   object yet, we'll allocate one.
     */
    static class CTadsAppTls *get_tls_obj();

    /*
     *   Prior to terminating, any thread that called get_tls_obj() must call
     *   this routine to free its TLS object.
     */
    static void on_thread_exit();

    /*
     *   Register/unregister a status line
     */
    void register_statusline(class CTadsStatusline *sl);
    void unregister_statusline(class CTadsStatusline *sl);

    /* process a WM_MENUSELECT through the statuslines */
    void send_menuselect_to_statuslines(
        HWND hwnd, WPARAM wparam, LPARAM lparam);

    /*
     *   Register/unregister a menu handler.  Menu handlers receive
     *   notifications for WM_INITMENUPOPUP, WM_MEASUREITEM, and WM_DRAWITEM
     *   when relevant to menus.
     */
    void register_menu_handler(class IMenuHandler *imh);
    void unregister_menu_handler(class IMenuHandler *imh);

    /* process a WM_INITMENUPOPUP through the menu handlers */
    void send_initmenupopup_to_menu_handlers(
        HWND hwnd, WPARAM wparam, LPARAM lparam);

    /* process a WM_MEASUREITEM through the menu handlers */
    void send_measureitem_to_menu_handlers(
        HWND hwnd, WPARAM wparam, LPARAM lparam);

    /* process a WM_DRAWITEM through the registered menu handlers */
    void send_drawitem_to_menu_handlers(
        HWND hwnd, WPARAM wparam, LPARAM lparam);

private:
    CTadsApp();
    CTadsApp(HINSTANCE inst, HINSTANCE pinst, LPSTR cmdline, int cmdshow);

    virtual ~CTadsApp();

    /*
     *   internal routine to set the openfile directory; takes a flag
     *   indicating whether we're setting the directory from a directory
     *   name or a filename (in the latter case, we'll extract the path
     *   from the filename)
     */
    void set_openfile_dir_or_fname(const char *fname, int is_filename);

    /* set the accelerator window, maintaining reference counts */
    void set_accel_win(class CTadsWin *win);

    /* get/restore the current accelerator */
    void get_accel(app_accel_info *info)
    {
        memset(info, 0, sizeof(*info));
        info->waccel = waccel_;
        info->taccel = taccel_;
        info->accel_win = accel_win_;
    }
    void set_accel(app_accel_info *info)
    {
        if (info->waccel != 0)
            set_accel(info->waccel, info->accel_win);
        else if (info->taccel != 0)
            set_accel(info->taccel, info->accel_win);
        else
            clear_accel();
    }

private:
    /* application instance handle */
    HINSTANCE  instance_;

    /* the global application object */
    static CTadsApp *app_;

    /*
     *   font cache - an array of fonts, the number allocated, and the
     *   number actually in use
     */
    class CTadsFont **fontlist_;
    int fonts_allocated_;
    int fontcount_;

    /* open-file directory */
    char *open_file_dir_;

    /* message filter */
    class CTadsMsgFilter *msg_filter_;

    /*
     *   our list of modeless dialogs: space for the window handles, the
     *   count of the number of slots allocated, and the number of slots
     *   actually in use
     */
    HWND *modeless_dlgs_;
    int modeless_dlg_alloc_;
    int modeless_dlg_cnt_;

    /* the list of registered menu status handlers */
    struct statline_listele *statusline_;

    /* the list of registered menu handlers */
    struct imh_listele *menu_handlers_;

    /* flag indicating that OLE was initialized successfully */
    int ole_inited_ : 1;

    /* MDI frame window, if this is an MDI application */
    class CTadsSyswinMdiFrame *mdi_win_;

    /*
     *   Current accelerator and accelerator target window.  At any given
     *   time, we can have *either* a native Windows accelerator or a custom
     *   accelerator object.
     */
    HACCEL waccel_;
    class CTadsAccelerator *taccel_;
    class CTadsWin *accel_win_;

    /* are accelerators enabled? */
    int accel_enabled_;

    /*
     *   an empty accelerator table - our custom accelerator class can use
     *   this to check a key against the standard system accelerators as a
     *   last resort
     */
    HACCEL empty_accel_;

    /* Windows OS system identifier (tells us if we're on 95, NT, etc) */
    unsigned long win_sys_id_;

    /* major and minor Windows OS version identifiers */
    unsigned long win_ver_major_;
    unsigned long win_ver_minor_;

    /* the thread local storage (TLS) index of our thread context structure */
    static DWORD tls_index_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Menu handler.  Different parts of the application can register to
 *   receive notifications for menu messages (WM_INITMENUPOPUP, WM_DRAWITEM,
 *   WM_MEASUREITEM).  The handler is an abstract interface, so it can be
 *   implemented wherever is most convenient.  We send these notifications to
 *   all registered handlers when we receive them.
 */
class IMenuHandler
{
public:
    virtual ~IMenuHandler()
    {
        /* make sure I'm unregistered, if I was registered globally */
        CTadsApp::get_app()->unregister_menu_handler(this);
    }

    /* handle a WM_INITMENUPOPUP notification  */
    virtual void imh_init_menu_popup(
        HMENU menu, unsigned int pos, int sysmenu) = 0;

    /* handle the owner-draw notifications */
    virtual void imh_measure_item(int ctl_id, MEASUREITEMSTRUCT *di) = 0;
    virtual void imh_draw_item(int ctl_id, DRAWITEMSTRUCT *di) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Thread-Local Storage object.  We allocate one of these per thread, to
 *   store various global variables for the thread.
 */
class CTadsAppTls
{
public:
    CTadsAppTls()
    {
        /* clear the object */
        memset(this, 0, sizeof(*this));
    }

    /* current window creation hook object, if any */
    class CTadsWinCreateHook *chook;
};


/* ------------------------------------------------------------------------ */
/*
 *   Message filter interface - for use with CTadsApp::set_msg_filter().
 */
class CTadsMsgFilter
{
public:
    /*
     *   Process a system message.  The message filter object always gets
     *   first crack at a message, before we pass it to any modeless dialogs
     *   or to any of the standard system handlers (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).  Return true to indicate that
     *   the message was handled - we'll do nothing further with it.  Return
     *   false to indicate that the standard handling should proceed.
     */
    virtual int filter_system_message(MSG *msg) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Custom keyboard accelerator.
 *
 *   This class defines a custom keyboard accelerator.  We can use this
 *   instead of the system accelerator mechanism.  This class provides a
 *   couple of extra features over the system mechanism, including dynamic
 *   updating and multi-key sequences, a la Emacs.
 */

/*
 *   Accelerator table enumerator
 */
class CTadsAccEnum
{
public:
    virtual ~CTadsAccEnum() { }

    /*
     *   process a key sequence: gives the full key sequence name, and the
     *   command to which the key is mapped
     */
    virtual void do_key(const textchar_t *keyname, unsigned short cmd) = 0;
};

/*
 *   A key mapping row gives the mapping for each shift-key combination for a
 *   single virtual key (VK_xxx).  There are three shift keys (Shift, Ctrl,
 *   Alt), so there are eight possible shift combinations.
 *
 *   Each mapping can contain one of the following:
 *
 *   0 -> the key is not assigned
 *
 *   1..99 -> the key is part of a multi-key sequence; the value gives the
 *   index of the next KeyMapTable in the sequence.
 *
 *   100..65535 -> the key generates a command; the value is the WM_COMMAND
 *   WPARAM value for the command.
 *
 *   Note that WM_COMMAND values below 100 aren't allowed, since values below
 *   100 are reserved to indicate multi-key sequences.
 */
struct KeyMapRow
{
    /* the command assignment for each shift combination for this key */
    unsigned short cmd[8];

    /* get the command entry for a given combination of shift keys */
    unsigned short get_cmd(int shiftkeys)
    {
        return cmd[shiftkeys];
    }

    /* set the command entry for a given combination of shift keys */
    void set_cmd(int shiftkeys, unsigned short val)
    {
        cmd[shiftkeys] = val;
    }

    /* enumerate our keys */
    void enum_keys(class CTadsAccelerator *acc, CTadsAccEnum *e,
                   const textchar_t *prefix, int vkey);
};

/* a key mapping table is a set of key rows, one per virtual key (VK_xxx) */
struct KeyMapTable
{
    /* there are 256 virtual keys; give each one a row */
    KeyMapRow row[256];

    /* get a command for a key */
    unsigned short get_cmd(int vkey, int shiftkeys)
        { return row[vkey].get_cmd(shiftkeys); }

    /* set a command for a key */
    void set_cmd(int vkey, int shiftkeys, unsigned short val)
        { row[vkey].set_cmd(shiftkeys, val); }

    KeyMapTable()
    {
        /* our keys are initially unassigned */
        memset(row, 0, sizeof(row));
    }

    /* enumerate our keys */
    void enum_keys(class CTadsAccelerator *acc, CTadsAccEnum *e,
                   const textchar_t *prefix);
};

/*
 *   The accelerator class
 */
class CTadsAccelerator
{
public:
    CTadsAccelerator();
    ~CTadsAccelerator();

    /*
     *   Translate a message.  This looks for a WM_KEYDOWN message with a
     *   mapping in the accelerator table; if we find one, we send a
     *   WM_COMMAND to the target window with the key's command, and return
     *   TRUE; if there's no mapping for the key, or it's a non-key message,
     *   we return FALSE.
     */
    int translate(class CTadsWin *win, MSG *msg);

    /*
     *   Get the translation, but don't execute it.
     */
    int get_translation(class CTadsWin *win, MSG *msg);

    /*
     *   Reset our key state.  This returns us to the initial state, so that
     *   the next incoming key goes to the first-level table.
     */
    void reset_keystate();

    /* map a command (single or multi-key) based on the key name string */
    int map(const textchar_t *keyname, size_t keylen, unsigned short cmd);

    /* enumerate mapped keys */
    void enum_keys(CTadsAccEnum *e);

    /* get a sutable */
    KeyMapTable *get_subtable(int idx) const { return tab[idx]; }

    /*
     *   Handle a WM_INITMENUPOPUP notification.  This updates the menu's
     *   accelerator keys to match the current keyboard bindings.
     */
    void init_menu_popup(HMENU menu, unsigned int pos, int sysmenu);

    /*
     *   Set our status-line object.  If this is set, we'll notify the status
     *   line of an update any time we have a new partial-chord message to
     *   show.
     */
    void set_statusline(class CTadsStatusline *s)
        { statusline = s; }

    /*
     *   Get a statusline message.  This can be used to show the "chord"
     *   state on the status line when a prefix key is pressed.  We follow
     *   the same protocol as CTadsStatusSource::get_status_message(), so a
     *   status source can call this and return what we return.  We return
     *   null if we don't have any chord state to show, in which case the
     *   caller can display a lower-priority status message.
     */
    textchar_t *get_status_message(int *caller_deletes);

protected:
    /*
     *   Set the new key state.  This sets the active mapping table, which is
     *   the table that will be used to interpret the next key we read.
     */
    void set_keystate(int vkey, int shift, int tab_idx);

    /* translate a message to a command ID */
    int msg_to_command(const MSG *msg, unsigned int *cmd);

    /* test a command to see if we can send it */
    int test_command(class CTadsWin *win, unsigned int cmd, MSG *msg);

    /* send a translated key command */
    void send_command(class CTadsWin *win, unsigned int cmd);

    /* find a command in a menu, recursively searching submenus */
    HMENU find_menu_command(int *index, HMENU menu, unsigned int cmd);

    /* delete an existing mapping */
    void del_key(unsigned short table_index, int vkey, int shiftkeys);

    /* delete a subtable */
    void delete_subtab(unsigned short subtab);

    /* map a key into a subtable */
    unsigned short map_to_subtab(
        unsigned short tabidx, int vkey, int shiftkeys);

    /* map a key into a table */
    void map_into_table(unsigned short tabidx, int vkey, int shiftkeys,
                        unsigned short cmd);

    /* add a key string to our menu hash table */
    void add_to_menu_hash(
        const textchar_t *keyname, size_t keylen, unsigned short cmd);

    /*
     *   Command-to-accelerator key hash table.  We use this to find the
     *   accelerator key name to display for a given menu item.
     */
    class CHtmlHashTable *menu_hash;

    /*
     *   Our mapping tables.  The zeroeth table is the first-key table; this
     *   is used to map single-key commands and the first key of a multi-key
     *   sequence.  Subsequent tables are nth-key tables, for keys in
     *   multi-key sequences.  For any multi-key sequence, the first table's
     *   entry for the first key in the sequence contains the index of the
     *   table for the second key, and so on.
     */
    KeyMapTable *tab[100];

    /*
     *   Our current key state.  This is the index of the key table we'll use
     *   to interpret the next key.  Initially, this is zero, which is the
     *   first-level key table.  When we encounter a key that's mapped to a
     *   second-level table, this changes to the index of the mapped
     *   subtable, so that we interpret the next key as a command in that
     *   second table.  When we reach a command or an unmapped key, we revert
     *   to the initial state.
     */
    int keystate;

    /*
     *   The chord state message.  This is a string showing the list of keys
     *   pressed so far to reach the current chord state.
     */
    CStringBuf chord_so_far;

    /* our status line object */
    class CTadsStatusline *statusline;
};



/* ------------------------------------------------------------------------ */
/*
 *   OPENFILENAME structure with and without the Win 2k+ expansion.  We want
 *   our code to work on NT4/95/98/Me as well, so we need to use the larger
 *   structure on the newer systems and the smaller structure on the older
 *   systems.  The whole point of the lStructSize field is that it *should*
 *   take care of this for us automatically by letting the APIs know which
 *   version of the structure we're using, but alas, things never quite work
 *   as documented.
 */
#if _WIN32_WINNT < 0x0500
#define OPENFILENAME_NT4  OPENFILENAME
#endif
struct OPENFILENAME5: OPENFILENAME_NT4
{
    /* the following are the NT5 extensions */
    void        *pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;

    OPENFILENAME5()
    {
        memset(this, 0, sizeof(*this));
        lStructSize = CTadsApp::get_app()->is_win2k_or_later()
                      ? sizeof(OPENFILENAME5) : sizeof(OPENFILENAME_NT4);
    }
};

#endif /* TADSAPP_H */

