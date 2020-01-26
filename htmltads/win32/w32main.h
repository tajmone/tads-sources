/* $Header: d:/cvsroot/tads/html/win32/w32main.h,v 1.4 1999/07/11 00:46:51 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32main.h - tads html win32 main entrypoint
Function

Notes

Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32MAIN_H
#define W32MAIN_H

#include <setjmp.h>


/*
 *   TADS entrypoint selector.  These variables can be set up so that we
 *   call either the normal TADS runtime or the debugger.  By defining
 *   these as externals, we can configure the executable we build with the
 *   linker, by supplying an appropriate set of initializations for these
 *   variables.
 */

/* TADS entrypoint - usually trdmain() or tddmain() */
extern int (*w32_tadsmain)(int, char **, struct appctxdef *, char *);

/* "before options" - options to parse before config file options */
extern char *w32_beforeopts;

/* configuration file name - usually "config.trh" or "config.tdh" */
extern char *w32_configfile;

/*
 *   Flag indicating whether the debug log window is allowed.  If this is
 *   true, we'll show a debug window if "-debugwin" is specified on the
 *   command line; if it's false, we won't show a debug log window.
 *   Normally, the run-time will allow the debug log window, whereas the
 *   debugger will not (because it provides its own debug log window).
 */
extern int w32_allow_debugwin;

/*
 *   flag indicating that we always pause on exit
 */
extern int w32_always_pause_on_exit;

/*
 *   Name of the registry value (attached to our main registry key) that
 *   we use to detect whether we've completed (or permanently skipped)
 *   setup for this program.  The normal run-time and the debugger use
 *   different keys, because it's possible that a user will install the
 *   run-time, then later install the debugger; when only the run-time is
 *   present, we won't set up the debugger, so we need to be able to
 *   detect the need to run setup again when the debugger is first
 *   started.
 */
extern char *w32_setup_reg_val_name;

/*
 *   Name of the application to display in "usage" messages
 */
extern char *w32_usage_app_name;

/*
 *   Name of the application to display in the main window title bar
 */
extern char *w32_titlebar_name;

/*
 *   File selector filter.  This is the filter used for the open-file
 *   dialog that we present if there is no game specified on the command
 *   line.  This should be a string following the normal filter
 *   convention; for example, for TADS 2 .gam files, it might read "TADS
 *   Games\0*.gam\0All Files\0*.*\0\0".
 */
extern char *w32_opendlg_filter;

/*
 *   File selector filter for the debugger.  This is the filter used for
 *   opening a new program; it should include program files as well as
 *   configuration files and source files.
 */
extern char *w32_debug_opendlg_filter;

/*
 *   File selector for the debugger for opening a game configuration file.
 *   This filter is used when loading a program; it should include program
 *   files and configuration files.
 */
extern char *w32_config_opendlg_filter;

/*
 *   Game file default extension.  The debugger uses this extension to
 *   build the name of a program file given the corresponding
 *   configuration file, by substituting this extension for the
 *   configuration file's extension.
 */
extern char *w32_game_ext;

/*
 *   Debugger flag: true means that we're running under the debugger,
 *   false means we're running as a normal interpreter.
 */
extern int w32_in_debugger;

/* the Application Data subfolder for this version */
extern const char w32_appdata_dir[];

/* underlying VM engine version ID string */
extern const char *w32_version_string;

/* registry key name for our preference settings */
extern const char w32_pref_key_name[];

/* name of debugger global configuration file */
extern const char w32_tdb_global_prefs_file[];

/* filename extension for configuration/project files (.tdc, .t3c) */
extern const char w32_tdb_config_ext[];

/* name of main Author's Manual index html file */
extern const char w32_tadsman_fname[];

/* major version number of the underlying system (2 for TADS 2, 3 for T3) */
extern int w32_system_major_vsn;

/*
 *   tads.org doc search URL - this is a sprintf pattern for the search on
 *   tads.org, with a string (%s) parameter for the query text
 */
extern const char w32_doc_search_url[];

/* list of directories to include in doc searches */
extern const char *w32_doc_dirs[];

/*
 *   generated IFID file template: prefix for the generated IFID, and the
 *   detailed instructions on how to use it, as comment fill
 */
extern const char w32_ifid_prefix[], w32_ifid_instructions[];

/*
 *   documentation manifest, for locally installed documentation - this lets
 *   us figure out which book a search result is part of, and lets us know if
 *   we have to generate a dynamic frame page for a result
 */
struct doc_manifest_t
{
    /* the filename prefix that identifies the manual */
    const char *prefix;

    /* the title of the book */
    const char *title;

    /* front page for the book */
    const char *front_page;

    /* the frame page for the book */
    const char *frame_page;

    /* the name of the target frame in the frame page */
    const char *frame_title;
};
extern const doc_manifest_t w32_doc_manifest[];

/*
 *   Pre-show-window routine.  This is called to give each version variation
 *   a chance to do any special start-up work before the main game window is
 *   opened.  At the point when this is called, the main game window object
 *   (CHtmlSys_mainwin::get_main_win()) has been instantiated, but the system
 *   window hasn't yet been created, so there's no UI visible at all at this
 *   point.
 *
 *   Return true to tell the caller to proceed, false to terminate the
 *   program without ever showing the main window.
 */
int w32_pre_show_window(int argc, char **argv);

/*
 *   Pre-startup routine.  An appropriate implementation must be provided
 *   for each executable.
 *
 *   'iter' is the iteration count through the TADS invocation loop;
 *   before the first invocation of the TADS engine, iter == 0, and iter
 *   is incremented each time we invoke the engine again (in other words,
 *   each time we load and run a new game).
 *
 *   'argc' and 'argv' are the usual argument vector.  argc == 1 indicates
 *   that there are no arguments (argv[0] is always the executable name).
 *   This routine can change entries in the argument vector if it wants.
 *
 *   Return true to tell the caller to proceed, false to terminate the
 *   executable.
 */
int w32_pre_start(int iter, int argc, char **argv);

/*
 *   Post-quit processing routine.  An appropriate implementation of this
 *   routine must be provided for each executable; the stand-alone
 *   interpreter has a different implementation from the debugger, for
 *   example, because the two programs must handle quitting differently.
 *
 *   main_ret_code is the result of the main TADS invocation: OSEXSUCC for
 *   success, OSEXFAIL for failure.
 *
 *   Returns true if the caller should proceed to invoke the main TADS
 *   entrypoint again (normally to load a new game), false if the caller
 *   should terminate the program.
 */
int w32_post_quit(int main_ret_code);

/*
 *   System-specific application termination cleanup.  (The difference
 *   between this and w32_post_quit() is that the latter is called after each
 *   VM run.  We might invoke the VM multiple times in the course of a single
 *   application session, because we could go through many edit-compile-run
 *   cycles in the course of a Workbench session.  This function is called
 *   only once, when the host application terminates.)
 */
void w32_cleanup();

/*
 *   Ask the Web UI window, if any, to yield the foreground to us.
 */
void w32_webui_yield_foreground();

/*
 *   Bring the Web UI window into the foreground
 */
void w32_webui_to_foreground();

/*
 *   Display a message box dialog
 */
void w32_msgbox(const char *msg, const char *url);

/*
 *   Version-specific routine - run the interpreter "about" box
 */
void vsn_run_terp_about_dlg(HWND owner);

#endif /* W32MAIN_H */

