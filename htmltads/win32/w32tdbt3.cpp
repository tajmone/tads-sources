#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/*
 *   Copyright (c) 1999 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32tdbt3.cpp - HTML TADS Debugger - TADS 3 engine configuration
Function

Notes

Modified
  11/24/99 MJRoberts  - Creation
*/

#include <WinSock2.h>
#include <Windows.h>

#include <string.h>
#include <ctype.h>

#include "os.h"

#include "vmvsn.h"

#include "t3main.h"
#include "t3std.h"
#include "w32main.h"
#include "w32tdb.h"
#include "w32prj.h"
#include "w32script.h"
#include "tadsdock.h"
#include "htmldcfg.h"
#include "w32dbgpr.h"
#include "tadsdlg.h"
#include "tadsapp.h"
#include "htmldcfg.h"
#include "vmprofty.h"
#include "htmlver.h"
#include "w32ver.h"
#include "tclibprs.h"
#include "osifcnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   VM-specific application termination cleanup
 */
void w32_cleanup()
{
    /*
     *   Disconnect from the web UI, if applicable.  Workbench is a container
     *   app that logically owns the UI window, so explicitly close the UI
     *   window on our way out.
     */
    osnet_disconnect_webui(TRUE);

    /* shut down the network layer */
    os_net_cleanup();
}

/* ------------------------------------------------------------------------ */
/*
 *   Service routine: is a file in the build configuration a library (".tl")
 *   file?  It is if it's explicitly marked as such in the build config, or
 *   if the filename ends in ".tl".  'idx' is the index in the build config
 *   of the source file in question.
 */
static int is_source_file_a_lib(CHtmlDebugConfig *config, int idx)
{
    char typebuf[64];

    /* check for an explicit type indicator */
    if (config->getval("build", "source_file_types", idx,
                       typebuf, sizeof(typebuf)))
        typebuf[0] = '\0';

    /*
     *   Note the type.  If we have an explicit type specifier, use the type
     *   from the specifier; otherwise, it's a library if it ends in ".tl", a
     *   source file otherwise.
     */
    if (typebuf[0] != '\0')
    {
        /*
         *   we have an explicit type specifier, so this is easy - if it's
         *   type 'l', it's a library; otherwise it isn't
         */
        return (typebuf[0] == 'l');
    }
    else
    {
        char buf[OSFNMAX];
        size_t len;

        /* no type specifier, so we need to check the filename - fetch it */
        config->getval("build", "source_files", idx, buf, sizeof(buf));

        /* it's a library if the filename ends in ".tl" */
        return ((len = strlen(buf)) > 3
                && stricmp(buf + len - 3, ".tl") == 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Read a line of text from a file.  Skip blank lines.  Returns true on
 *   success, false on end of file or other failure.
 */
static int read_text_file_line(textchar_t *buf, size_t bufsize, FILE *fp)
{
    /* keep going until we get a non-blank line */
    for (;;)
    {
        /* read the next line; on EOF return failure */
        if (fgets(buf, bufsize, fp) == 0)
            return FALSE;

        /* if the line isn't blank, return it */
        if (buf[0] != '\n')
            return TRUE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Diff two files.  Returns 1 if the files match, 0 if they differ, or -1
 *   if an error occurs opening one of the files.
 */
static int files_diff_equal(const textchar_t *fname1,
                            const textchar_t *fname2)
{
    /* assume we won't be able to open the files */
    int eq = -1;

    /* open the files */
    FILE *fp1 = fopen(fname1, "r");
    FILE *fp2 = fopen(fname2, "r");

    /* if we managed to open both files, compare their contents */
    if (fp1 != 0 && fp2 != 0)
    {
        /* read the files and look for differences */
        for (eq = 1 ;; )
        {
            char buf1[512], buf2[512];
            int ok1, ok2;

            /* read the next line of each file, ignoring blanks */
            ok1 = read_text_file_line(buf1, sizeof(buf1), fp1);
            ok2 = read_text_file_line(buf2, sizeof(buf2), fp2);

            /* if they both ran out at the same time, we're done */
            if (!ok1 && !ok2)
                break;

            /* if one or the other ran out first, they're different */
            if (!ok1 || !ok2)
            {
                eq = 0;
                break;
            }

            /* if the lines don't match, the files differ */
            if (stricmp(buf1, buf2) != 0)
            {
                eq = 0;
                break;
            }
        }
    }

    /* done with the files */
    if (fp1 != 0)
        fclose(fp1);
    if (fp2 != 0)
        fclose(fp2);

    /* return what we found */
    return eq;
}

/* ------------------------------------------------------------------------ */
/*
 *   Trim any QUIT sequences from the end of an auto-script file.
 */
static void trim_quit_sequences(CHtmlSys_dbgmain *dbgmain,
                                const textchar_t *scriptfile)
{
    CHtmlDebugConfig *lc = dbgmain->get_local_config();
    FILE *fp;
    char *fbuf;
    long fsiz;
    int i, j, lin, cnt;
    char *p;
    const textchar_t *val;

    /* open the file; if we can't, there's no more we can do here */
    if ((fp = fopen(scriptfile, "r")) == 0)
        return;

    /* figure the size of the file */
    fseek(fp, 0, SEEK_END);
    fsiz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* allocate space for it */
    if ((fbuf = (char *)th_malloc(fsiz + 1)) == 0)
    {
        fclose(fp);
        return;
    }

    /* load it */
    fsiz = fread(fbuf, 1, fsiz, fp);

    /* if it doesn't end in a newline, explicitly add a trailing newline */
    if (fsiz == 0 || fbuf[fsiz-1] != '\n')
        fbuf[fsiz++] = '\n';

    /* done with the file for now */
    fclose(fp);

    /* count the lines - each '\n' indicates the start of a new line */
    for (lin = 0, p = fbuf ; p < fbuf + fsiz ; ++p)
    {
        if (*p == '\n')
            ++lin;
    }

    /* allocate pointers to the lines */
    char **markers = new char *[lin + 1];

    /* note the line starts */
    for (p = fbuf, i = 0, markers[i++] = p ; p < fbuf + fsiz ; ++p)
    {
        if (*p == '\n' && i <= lin)
            markers[i++] = p + 1;
    }

    /* now look for matches to the ending sequences */
    cnt = lc->get_strlist_cnt("auto-script", "quit sequences");
    for (i = 0 ; i < cnt ; ++i)
    {
        int sec;

        /* skip blank lines */
        val = lc->getval_strptr("auto-script", "quit sequences", i);
        if (val == 0 || *val == '\0')
            continue;

        /* find the next blank line, which ends this sublist */
        for (j = i + 1 ; j < cnt ; ++j)
        {
            val = lc->getval_strptr("auto-script", "quit sequences", j);
            if (val == 0 || *val == '\0')
                break;
        }

        /* calculate the number of lines in this section */
        sec = j - i;

        /* if we have enough lines in the file, scan for a match */
        if (lin >= sec)
        {
            int l;
            int match;

            /* scan for a match to the last n lines of the file */
            for (match = TRUE, l = lin - sec ; i < j ; ++i, ++l)
            {
                int linelen;

                /* get this line regex */
                val = lc->getval_strptr("auto-script", "quit sequences", i);

                /*
                 *   calculate the length of this line - it's the start of
                 *   the next line minus the start of this line, minus one
                 *   for the newline
                 */
                linelen = markers[l+1] - markers[l] - 1;

                /* check for a match - the entire line has to match */
                if (dbgmain->ReMatchA(markers[l], linelen,
                                      val, get_strlen(val)) != linelen)
                {
                    match = FALSE;
                    break;
                }
            }

            /* if we matched, trim the sequences out of the file */
            if (match)
            {
                /* rewrite the file, minus the matching quit sequence */
                if ((fp = fopen(scriptfile, "w")) != 0)
                {
                    /* write it out, excluding the last n lines */
                    fwrite(fbuf, 1, markers[lin - sec] - fbuf, fp);

                    /* done writing the file */
                    fclose(fp);
                }

                /* done */
                break;
            }
        }

        /* jump ahead to the next section */
        i = j;
    }

    /* done with the file buffer and line markers */
    th_free(fbuf);
    delete [] markers;
}


/* ------------------------------------------------------------------------ */
/*
 *   Debugger cover function for beginning program execution.  This
 *   routine re-builds the argument vector from the arguments stored in
 *   the configuration settings.
 */
static int t3_dbg_main(int argc, char **argv, struct appctxdef *appctx,
                       char *config_file)
{
    CHtmlSys_dbgmain *dbgmain = CHtmlSys_dbgmain::get_main_win();
    char **my_argv;
    int my_argc;
    int i;
    char *src;
    char *dst;
    char scriptfile[OSFNMAX];
    char resdir[OSFNMAX];
    char args[1024];
    char fs[32] = "-s";
    CHtmlDebugConfig *config;
    char quote;
    int ret;
    CStringBuf rps;
    int rps_temp;

    /*
     *   if there's no main debugger window, we can't synthesize any
     *   arguments
     */
    if (dbgmain == 0)
        return t3main(argc, argv, appctx, config_file);

    /* get the local configuration object */
    config = dbgmain->get_local_config();

    /* get the replay script, if any */
    rps.set(dbgmain->get_replay_script());
    rps_temp = dbgmain->is_replay_script_temp();

    /*
     *   Clear the replay script in the main debugger object - we've made a
     *   private copy here, so we can reset the stored value so that we don't
     *   repeat the replay on the next run if the next run is a plain run.
     *   We need to do this *before* starting the run: if we waited until
     *   after the run terminates, we'd overwrite any new script the user
     *   selects for playback during the run.
     */
    dbgmain->clear_replay_script();

    /* get the file safety setting */
    if (config->getval("prog_args", "file_safety", 0, fs+2, sizeof(fs)-2))
        fs[2] = '\0';

    /*
     *   Ignore the command line passed in from the caller, and use our
     *   own command line instead.  First, retrieve the command line from
     *   the configuration settings.
     */
    if (config->getval("prog_args", "args", 0, args, sizeof(args)))
        args[0] = '\0';

    /* skip any leading spaces in the argument string */
    for (src = args ; isspace(*src) ; ++src) ;

    /*
     *   count the first argument, if it is present - we count the start
     *   of each new argument
     */
    my_argc = 0;
    if (*src != '\0')
        ++my_argc;

    /*
     *   Parse the arguments into individual strings, null-terminating
     *   each argument, but leaving them in the original buffer.  Note
     *   that the buffer can only shrink, as we remove quotes, but it can
     *   never grow; this allows us to process the buffer in-place,
     *   without making a separate copy.
     */
    for (quote = '\0', dst = args ; *src != '\0' ; )
    {
        /* if process according to whether we're in a quoted string or not */
        if (quote == '\0')
        {
            /*
             *   we're not in a quoted string - if this is a space, it
             *   starts the next argument; if it's a quote, note that
             *   we're in a quoted string, and remove the quote; otherwise
             *   just copy it out
             */
            switch(*src)
            {
            case '"':
                /* it's a quote - note it, but don't copy it to the output */
                quote = *src;
                ++src;
                break;

            case ' ':
                /*
                 *   it's the end of the current argument - null-terminate
                 *   the current argument string
                 */
                *dst++ = '\0';

                /* skip the space we just scanned */
                ++src;

                /* skip to the next non-space character */
                while (isspace(*src))
                    ++src;

                /*
                 *   if another argument follows, count the start of the
                 *   new argument
                 */
                if (*src != '\0')
                    ++my_argc;
                break;

            default:
                /* simply copy anything else to the output */
                *dst++ = *src++;
                break;
            }
        }
        else
        {
            /*
             *   if it's our quote, we're done with the quoted section -
             *   unless it's a stuttered quote, in which case we will
             *   simply copy a single quote to the output
             */
            if (*src == quote)
            {
                /* check for stuttering */
                if (*(src + 1) == quote)
                {
                    /* it's stuttered - make a single copy of the quote */
                    *dst++ = quote;

                    /* skip both copies of the quote in the input */
                    src += 2;
                }
                else
                {
                    /*
                     *   it's the end of our quoted section - skip it in
                     *   the input, but don't copy it to the output
                     */
                    ++src;

                    /* note that we're no longer in a quoted section */
                    quote = '\0';
                }
            }
            else
            {
                /*
                 *   it's just the next character of the quoted section -
                 *   copy it unchanged
                 */
                *dst++ = *src++;
            }
        }
    }

    /* null-terminate the last argument string */
    *dst = '\0';

    /* add space for the executable name argument (argv[0]) */
    ++my_argc;

    /* add space for the image file name argument */
    ++my_argc;

    /* add space for the script capture file */
    my_argc += 2;

    /* add space for file safety, if applicable */
    if (fs[2] != '\0')
        my_argc += 1;

    /* add space for the replay script arguments, if applicable */
    if (rps.get() != 0 && *rps.get() != '\0')
        my_argc += 2;

    /* add another two for the resource directory ("-R dir") */
    my_argc += 2;

    /* and another two for the file base directory path ("-d dir") */
    my_argc += 2;

    /* ...and the sandbox directory path ("-sd dir") */
    my_argc += 2;

    /* add space for the -norand option */
    ++my_argc;

    /* allocate space for the argument vector */
    my_argv = (char **)t3malloc(my_argc * sizeof(*my_argv));

    /* copy the original argv[0] */
    my_argv[0] = argv[0];

    /* start at slot 1 */
    i = 1;

    /* add the automatic script capture file, if desired */
    if (dbgmain->name_auto_script(scriptfile, sizeof(scriptfile)))
    {
        osfildef *fp;
        const textchar_t *dir;

        /* make sure the Scripts directory exists */
        if ((dir = config->getval_strptr("auto-script", "dir", 0)) == 0)
            dir = "Scripts";
        if (GetFileAttributes(dir) == INVALID_FILE_ATTRIBUTES)
            CreateDirectory(dir, 0);

        /* add the option */
        my_argv[i++] = "-o";
        my_argv[i++] = scriptfile;

        /* create the file, so that it has a timestamp */
        if ((fp = osfopwt(scriptfile, OSFTCMD)) != 0)
            osfcls(fp);

        /* add it to the script window */
        dbgmain->add_file_to_script_win(scriptfile);
    }

    /* add the script replay file, if one is set */
    if (rps.get() != 0 && *rps.get() != '\0')
    {
        my_argv[i++] = "-I";
        my_argv[i++] = rps.get();
    }

    /*
     *   Add the resource file path - this is the path to the *project* file,
     *   which might be different from the path to the game file.  As a
     *   matter of convention, we tell users to keep resources rooted in the
     *   project directory, so we want to look there regardless of where the
     *   game file goes.  (In particular, it's typical for the game file to
     *   be in a ./Debug subdirectory, since that's the way we configure new
     *   projects by default.)
     */
    os_get_path_name(resdir, sizeof(resdir),
                     dbgmain->get_local_config_filename());
    my_argv[i++] = "-R";
    my_argv[i++] = resdir;

    /*
     *   add the base file directory - for Workbench debugger runs, override
     *   the default (the .t3 file path) with the main project folder
     */
    my_argv[i++] = "-d";
    my_argv[i++] = resdir;

    /* likewise for the sandbox directory */
    my_argv[i++] = "-sd";
    my_argv[i++] = resdir;

    /* add -norand */
    my_argv[i++] = "-norand";

    /* add file safety */
    if (fs[2] != '\0')
        my_argv[i++] = fs;

    /* add the program name, which we can learn from the debugger */
    my_argv[i++] = (char *)dbgmain->get_gamefile_name();

    /* add the user arguments */
    for (src = args ; i < my_argc ; ++i, src += strlen(src) + 1)
    {
        /* set this argument */
        my_argv[i] = src;
    }

    /*
     *   If we have a host application context, and it accepts resource file
     *   paths, add all of the top-level library directories to the resource
     *   search list.  This lets the program find resources that come from
     *   library directories.
     */
    if (appctx != 0 && appctx->add_res_path != 0)
    {
        int i;
        int cnt;

        /* scan the source file list for libraries */
        cnt = config->get_strlist_cnt("build", "source_files");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* if this is a library, add its path to the search list */
            if (is_source_file_a_lib(config, i))
            {
                char fname[OSFNMAX];
                char path[OSFNMAX];
                char *root;

                /* get the library's name */
                config->getval("build", "source_files", i,
                               fname, sizeof(fname));

                /* get the root filename */
                root = os_get_root_name(fname);

                /* search the library directory list for this file */
                if (CLibSearchPathList::find_tl_file(
                    dbgmain->get_global_config(), fname, path, sizeof(path))
                    && (path[0] != '\0' || root != fname))
                {
                    char fpath[MAX_PATH];

                    /* pull out the path from the filename */
                    os_get_path_name(fpath, sizeof(fpath), fname);

                    /*
                     *   build the full path to the library by combining the
                     *   location where we found it and the relative path we
                     *   started with
                     */
                    os_build_full_path(fname, sizeof(fname), path, fpath);

                    /* we found a path - add it to the search list */
                    appctx->add_res_path(appctx->add_res_path_ctx,
                                         fname, strlen(fname));
                }
            }
        }
    }

    /* keep a reference on the main window while we're working */
    dbgmain->AddRef();

    /* run the program */
    ret = t3main(my_argc, my_argv, appctx, config_file);

    /* free the argument vector */
    t3free(my_argv);

    /* notify the web UI that the server has disconnected */
    osnet_disconnect_webui(FALSE);

    /* assume we'll keep the new auto-script file */
    int keep_auto = TRUE;

    /*
     *   If there was an input script, check to see if the new auto-script
     *   matches it.  If so, and the new auto-script isn't already open in
     *   the UI, discard the new script, since it doesn't contain any new
     *   information.  This avoids piling up redundant copies of the input
     *   script if the user is repeatedly running the same script in an
     *   edit-test cycle.
     *
     *   Note that we make this check once BEFORE we trim off any QUIT
     *   command sequences from the end of the script.  This will ensure that
     *   if the original command script has the QUIT sequences, we'll match
     *   the copies we find in the new script.  We'll test again AFTER
     *   trimming the QUIT commands as well, in a moment.
     */
    if (rps.get() != 0 && *rps.get() != '\0'
        && !dbgmain->is_file_open(scriptfile)
        && files_diff_equal(scriptfile, rps.get()) == 1)
        keep_auto = FALSE;

    /* trim off any QUIT sequence(s) at the end of the file */
    trim_quit_sequences(dbgmain, scriptfile);

    /*
     *   Now try again at matching the new auto-script to the original input
     *   script.  We do this again here, AFTER trimming off any quit sequence
     *   at the end of the new script, so that we won't redundantly store the
     *   new script if its trimmed version matches the original.  We won't
     *   have detected that earlier if the user manually quit this run by
     *   entering a QUIT command that we just trimmed.
     */
    if (rps.get() != 0 && *rps.get() != '\0'
        && !dbgmain->is_file_open(scriptfile)
        && files_diff_equal(scriptfile, rps.get()) == 1)
        keep_auto = FALSE;

    /* if we're not keeping the new script, delete it */
    if (!keep_auto)
    {
        /* delete the file */
        remove(scriptfile);

        /* remove it from the script window UI */
        dbgmain->remove_file_from_script_win(scriptfile);
    }

    /*
     *   if the input script was a temporary file created just for this run,
     *   we can delete it now
     */
    if (rps.get() != 0 && *rps.get() != '\0' && rps_temp)
        remove(rps.get());

    /* done with our reference on the debugger */
    dbgmain->Release();

    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Define startup configuration variables for the TADS debugger
 */
int (*w32_tadsmain)(int, char **, struct appctxdef *, char *) = t3_dbg_main;
char *w32_beforeopts = "";
char *w32_configfile = 0;
int w32_allow_debugwin = FALSE;
int w32_always_pause_on_exit = FALSE;
char *w32_setup_reg_val_name = "Debugger Setup Done";
char *w32_usage_app_name = "htmltdb3";
char *w32_titlebar_name = "HTML TADS 3";
int w32_in_debugger = TRUE;
char *w32_opendlg_filter = "TADS 3 Applications\0*.t3\0All Files\0*.*\0\0";
char *w32_debug_opendlg_filter =
    "TADS 3 Projects\0*.t3m\0"
    "All Files\0*.*\0\0";
char *w32_config_opendlg_filter =
    "TADS 3 Applications and Projects\0*.t3;*.t3m\0"
    "All Files\0*.*\0\0";
char *w32_game_ext = "t3";
int w32_sysid = HTML_SYSID_TADS3;
const char *w32_version_string =
    HTMLTADS_VERSION
    " (Build Win" HTMLTADS_WIN32_BUILD
    "; TADS " T3VM_VSN_STRING ")";
const char w32_pref_key_name[] =
    "Software\\TADS\\HTML TADS\\3.0\\Settings";
const char w32_tdb_global_prefs_file[] = "htmltdb3.t3c";
const char w32_tdb_config_ext[] = "t3m";
const char w32_tadsman_fname[] = "doc\\indexwb.htm";
int w32_system_major_vsn = 3;
const char w32_doc_search_url[] =
    "http://www.tads.org/t3doc/doSearch.php?p=%s";
const char *w32_doc_dirs[] = { "doc", "lib\\adv3", 0 };
const char w32_appdata_dir[] = "TADS 3 Workbench";
const char w32_ifid_prefix[] = "IFID =";
const char w32_ifid_instructions[] =
    " *   To use this in your game, find the \"versionInfo\" object in your\r\n"
    " *   source code, then copy the \"IFID=\" line below into the object\r\n"
    " *   body as a property definition.\r\n";

const doc_manifest_t w32_doc_manifest[] =
{
    { "doc\\gsg\\",       "Getting Started in TADS 3",
          "doc\\gsg\\index.html",
           0, 0 },
    { "doc\\libref\\*\\", "Library Reference Manual",
           "doc\\libref\\index.html",
           "doc\\libref\\index.html", "main" },
    { "doc\\libref\\",    "TADS 3 Library Reference Manual",
           "doc\\libref\\index.html",
           "doc\\libref\\index.html", "classes" },
    { "doc\\sysman\\",    "TADS 3 System Manual",
           "doc\\sysman\\cover.htm",
           0, 0 },
    { "doc\\techman\\",   "TADS 3 Technical Manual",
           "doc\\techman\\cover.htm",
           0, 0 },
    { "doc\\tourguide\\", "TADS 3 Tour Guide",
           "doc\\tourguide\\index.html",
           "doc\\tourguide\\index.html", "topicview" },

    { 0, 0, 0, 0 }
};

/* ------------------------------------------------------------------------ */
/*
 *   Debugger version factoring routines for TADS 3
 */

/*
 *   T3 Workbench has the "project" UI to maintain the source file list, so
 *   there's no need to maintain a separate source files list in the helper's
 *   project configuration.
 */
int CHtmlSys_dbgmain::vsn_need_srcfiles() const
{
    return FALSE;
}

/*
 *   Load the configuration file
 */
int CHtmlSys_dbgmain::vsn_load_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    int err;

    /*
     *   if the filename ends in .t3m, load it with the .t3m loader;
     *   otherwise, load it with our private binary file format loader
     */
    size_t len = strlen(fname);
    if (len >= 4 && stricmp(fname + len - 4, ".t3m") == 0)
    {
        /* get the system library path */
        char sys_lib_path[OSFNMAX];
        os_get_special_path(sys_lib_path, sizeof(sys_lib_path),
                            _pgmptr, OS_GSP_T3_LIB);

        /* get the install directory */
        char exe[OSFNMAX], exepath[OSFNMAX];
        GetModuleFileName(0, exe, sizeof(exe));
        os_get_path_name(exepath, sizeof(exepath), exe);

        /* it's a .t3m file - use the .t3m loader */
        err = config->load_t3m_file(fname, sys_lib_path, exepath);
    }
    else
    {
        /* use the private binary file format loader */
        err = config->load_file(fname);
    }

    /*
     *   if successful, upgrade any old Windows SETUP parameters to the
     *   corresponding Special Files settings
     */
    int cnt = config->get_strlist_cnt("build", "installer options");
    if (cnt != 0)
    {
        int i;

        /* make a list of the current settings */
        CStringBuf *opts = new CStringBuf[cnt];
        for (i = 0 ; i < cnt ; ++i)
            opts[i].set(config->getval_strptr(
                "build", "installer options", i));

        /* delete the old list */
        config->clear_strlist("build", "installer options");

        /*
         *   Run through the list.  Translate any options that we can, and
         *   add the others back to the list.
         */
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this option line, skipping leading spaces */
            textchar_t *kw = opts[i].get();
            for ( ; isspace(*kw) ; ++kw) ;

            /* find the '=', minus any spaces */
            textchar_t *eq = strchr(kw, '=');
            for ( ; eq != 0 && eq > kw && isspace(*(eq-1)) ; --eq) ;

            /* note the keyword length */
            size_t kwlen = (eq == 0 ? 0 : eq - kw);

            /* check which keyword we have */
            static const struct
            {
                const textchar_t *kw;
                int is_file;
                const textchar_t *subkey;
            }
            *xp, xlat[] =
            {
                { "icon", TRUE, "exe icon" },
                { "license", TRUE, "license" },
                { "readme", TRUE, "readme" },
                { "readme_title", FALSE, "readme" },
                { "charlib", TRUE, "cmaplib" },
                { 0, 0 }
            };
            for (xp = xlat ; xp->kw != 0 ; ++xp)
            {
                /* check for a match */
                if (strlen(xp->kw) == kwlen
                    && memicmp(kw, xp->kw, kwlen) == 0)
                    break;
            }

            /*
             *   if we found a match, translate it; otherwise keep the
             *   untranslated version
             */
            if (xp->kw != 0)
            {
                /* get the value - the part after the '=' */
                for ( ; *eq != '\0' && *eq != '=' ; ++eq) ;
                if (*eq == '=')
                    for (++eq ; isspace(*eq) ; ++eq) ;

                /* save it under the translated key */
                config->setval(xp->is_file
                               ? "build special file"
                               : "build special file title",
                               xp->subkey, 0, eq);
            }
            else
            {
                /* keep the untranslated option string */
                config->appendval(
                    "build", "installer options", opts[i].get());
            }
        }

        /* done with our copy of the options */
        delete [] opts;
    }

    /* return the result code */
    return err;
}

/*
 *   Save the configuration file
 */
int CHtmlSys_dbgmain::vsn_save_config_file(CHtmlDebugConfig *config,
                                           const textchar_t *fname)
{
    /*
     *   if the filename ends in .t3m, save it with the .t3m writer;
     *   otherwise, save it with our private binary file format writer.
     */
    size_t len = strlen(fname);
    if (len >= 4 && stricmp(fname + len - 4, ".t3m") == 0)
    {
        /* get the install directory */
        char exe[OSFNMAX], exepath[OSFNMAX];
        GetModuleFileName(0, exe, sizeof(exe));
        os_get_path_name(exepath, sizeof(exepath), exe);

        /* it's a .t3m file - use the .t3m writer */
        return config->save_t3m_file(fname, exepath);
    }
    else
    {
        /* use the private binary file format writer */
        return config->save_file(fname);
    }
}

/*
 *   Close windows in preparation for loading a new configuration
 */
void CHtmlSys_dbgmain::vsn_load_config_close_windows()
{
    /* close the project window */
    if (projwin_ != 0)
    {
        SendMessage(GetParent(projwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        projwin_ = 0;
    }
    if (scriptwin_ != 0)
    {
        SendMessage(GetParent(scriptwin_->get_handle()), DOCKM_DESTROY, 0, 0);
        scriptwin_ = 0;
    }
}

/*
 *   create the project window
 */
CHtmlDbgProjWin *CHtmlSys_dbgmain::create_proj_win(const RECT *deskrc)
{
    RECT rc;
    int vis;

    /* if we don't have a project window yet, create it */
    if (projwin_ == 0)
        projwin_ = new CHtmlDbgProjWin(this);

    /* load its configuration */
    SetRect(&rc, deskrc->left + 2, deskrc->bottom - 2 - 150, 300, 150);
    rc.right += rc.left;
    rc.bottom += rc.top;
    projwin_->load_win_config(local_config_, "project win", &rc, &vis);
    load_dockwin_config_win(local_config_, "project win", projwin_,
                            "Project", vis, TADS_DOCK_ALIGN_LEFT,
                            170, 270, &rc, TRUE, FALSE);

    /* return the project window */
    return projwin_;
}

/*
 *   create the script window
 */
CHtmlDbgScriptWin *CHtmlSys_dbgmain::create_script_win(const RECT *deskrc)
{
    CHtmlDebugConfig *lc = local_config_;
    RECT rc;
    int vis;

    /* if we don't have the script window yet, create it */
    if (scriptwin_ == 0)
        scriptwin_ = new CHtmlDbgScriptWin(this);

    /* load its configuration */
    SetRect(&rc, deskrc->left + 175, deskrc->bottom - 2 - 150, 170, 150);
    rc.right += rc.left;
    rc.bottom += rc.top;
    scriptwin_->load_win_config(lc, "script win", &rc, &vis);
    load_dockwin_config_win(
        lc, "script win", scriptwin_, "Scripts",
        vis, TADS_DOCK_ALIGN_LEFT, 170, 150, &rc, TRUE, FALSE);

    /* load the list with the current directory contents */
    refresh_script_win(TRUE);

    /* return the window */
    return scriptwin_;
}

/*
 *   refresh the script window for a change to the script folder
 */
void CHtmlSys_dbgmain::refresh_script_win(int clear_first)
{
    if (scriptwin_ != 0)
    {
        CHtmlDebugConfig *lc = local_config_;
        const textchar_t *dir;

        /* clear out the current list if desired */
        if (clear_first)
            scriptwin_->clear_list();

        /* rebuild the list from the current script directory */
        if ((dir = lc->getval_strptr("auto-script", "dir", 0)) == 0)
            dir = "Scripts";
        scriptwin_->scan_dir(dir);

        /* prune the directory to delete older scripts, if desired */
        prune_script_dir();
    }
}

/*
 *   prune the script directory
 */
void CHtmlSys_dbgmain::prune_script_dir()
{
    CHtmlDebugConfig *lc = local_config_;
    int cnt;
    const textchar_t *dir;

    /*
     *   if there's no script window, or script pruning is disabled, there's
     *   nothing to do
     */
    if (scriptwin_ == 0 || !lc->getval_int("auto-script", "prune", TRUE))
        return;

    /* get the script directory */
    if ((dir = lc->getval_strptr("auto-script", "dir", 0)) == 0)
        dir = "Scripts";

    /* get the prune-to count */
    cnt = lc->getval_int("auto-script", "prune to count", 25);

    /* ask the script window to do the pruning */
    scriptwin_->prune_auto(dir, cnt);
}


/*
 *   Open windows for a newly-loaded configuration
 */
void CHtmlSys_dbgmain::vsn_load_config_open_windows(const RECT *deskrc)
{
    /* create the project window */
    create_proj_win(deskrc);

    /* create the script window */
    create_script_win(deskrc);
}

/*
 *   Save version-specific configuration information
 */
void CHtmlSys_dbgmain::vsn_save_config()
{
    /* save the project window's layout information */
    if (projwin_ != 0)
    {
        /* save the project window's layout */
        projwin_->save_win_config(local_config_, "project win");
        projwin_->save_win_config(global_config_, "project win");

        /* save the project file information */
        projwin_->save_config(local_config_);
    }

    /* save the script window's layout information */
    if (scriptwin_ != 0)
    {
        scriptwin_->save_win_config(local_config_, "script win");
        scriptwin_->save_win_config(global_config_, "script win");
    }
}

/*
 *   check command status
 */
TadsCmdStat_t CHtmlSys_dbgmain::vsn_check_command(const check_cmd_info *info)
{
    switch(info->command_id)
    {
    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDDIR:
        /*
         *   always route these to the project window, even if it's not
         *   the active window
         */
        return (projwin_ != 0
                ? projwin_->check_command(info)
                : TADSCMD_UNKNOWN);

    default:
        /* we don't handle other cases */
        return TADSCMD_UNKNOWN;
    }
}

/*
 *   execute command
 */
int CHtmlSys_dbgmain::vsn_do_command(int nfy, int cmd, HWND ctl)
{
    switch(cmd)
    {
    case ID_PROJ_ADDFILE:
    case ID_PROJ_ADDEXTRES:
    case ID_PROJ_ADDDIR:
        /*
         *   always route these to the project window, even if it's not
         *   the active window
         */
        return (projwin_ != 0 ? projwin_->do_command(nfy, cmd, ctl) : FALSE);

    default:
        /* we don't handle other cases */
        return FALSE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a temporary directory.  Returns zero on success, non-zero on
 *   failure.
 */
static int create_temp_dir(char *dirbuf, HWND owner_hwnd)
{
    char systmp[OSFNMAX];

    /*
     *   seed the random number generator - we'll try random names until
     *   we find one that doesn't collide with another process's names
     */
    srand((unsigned int)GetTickCount());

    /*
     *   get the system temporary directory - we'll create our temporary
     *   directory as a subdirectory
     */
    GetTempPath(sizeof(systmp), systmp);

    /* keep going until we find a unique name */
    for (;;)
    {
        UINT id;

        /* generate a temporary directory name */
        id = (UINT)rand();
        GetTempFileName(systmp, "tads", id, dirbuf);

        /* try creating the directory */
        if (CreateDirectory(dirbuf, 0))
        {
            /* success */
            return 0;
        }

        /* check the error - if it's not "already exists", give up */
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            /* tell the user what's wrong */
            MessageBox(owner_hwnd,
                       "An error occurred creating a temporary "
                       "directory.  Please free up some space on "
                       "your hard disk and try again.", "TADS Workbench",
                       MB_OK | MB_ICONEXCLAMATION);

            /* tell the caller we failed */
            return 1;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Dialog for waiting for a #include scan to complete.  We show this
 *   dialog modally while running the #include scan, which ensures that the
 *   user interface is disabled until the scan completes.
 */
class CTadsDlg_scan_includes: public CTadsDialog
{
public:
    /* handle a command */
    virtual int do_command(WPARAM id, HWND ctl)
    {
        switch(id)
        {
        case IDOK:
        case IDCANCEL:
            /* do not dismiss the dialog on regular keystrokes */
            return TRUE;

        case IDC_BUILD_DONE:
            /*
             *   special message sent from main window when it hears that
             *   the build is done - dismiss the dialog
             */
            EndDialog(handle_, id);
            return TRUE;

        default:
            /* inherit default processing for anything else */
            return CTadsDialog::do_command(id, ctl);
        }
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Add the files in a directory to a command line under construction
 */
static void add_files_to_cmdline(CStringBufCmd *cmdline,
                                 const textchar_t *dir,
                                 int (*filter)(const char *fname))
{
    char pat[OSFNMAX];
    HANDLE hf;
    WIN32_FIND_DATA info;

    /* build the pattern */
    os_build_full_path(pat, sizeof(pat), dir, "*");

    /* scan the directory */
    if ((hf = FindFirstFile(pat, &info)) != INVALID_HANDLE_VALUE)
    {
        /* scan files until we run out */
        do
        {
            char fname[OSFNMAX];

            /* skip '.' and '..' */
            if (strcmp(info.cFileName, ".") == 0
                || strcmp(info.cFileName, "..") == 0)
                continue;

            /* if there's a filter, skip files it rejects */
            if (filter != 0 && !(*filter)(info.cFileName))
                continue;

            /* build the full name */
            os_build_full_path(fname, sizeof(fname), dir, info.cFileName);

            /* check to see if it's a file or directory */
            if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                /* directory - recursively add its contents */
                add_files_to_cmdline(cmdline, fname, 0);
            }
            else
            {
                /* ordinary file - add it to the list */
                cmdline->append_qu(fname, TRUE);
            }
        }
        while (FindNextFile(hf, &info));

        /* done with the search */
        FindClose(hf);
    }
}

/*
 *   filter for the Scripts directory - filter out the "Auto nn" files
 */
static int filter_auto_scripts(const char *fname)
{
    /* if it starts with Auto... */
    if (memcmp(fname, "Auto ", 5) == 0)
    {
        /* ...followed by a string of digits... */
        const char *p;
        for (p = fname + 5 ; isdigit(*p) ; ++p) ;

        /* ...followed by ".cmd"... */
        if (strcmp(p, ".cmd") == 0)
        {
            /* ...then it's an auto-script file - filter it out */
            return FALSE;
        }
    }

    /* it's not an auto-script file - keep it */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Run the compiler.  We build a list of commands, then we start a
 *   thread to execute the list.  We build the list so that the background
 *   build thread doesn't need to allocate memory, access the
 *   configuration, or otherwise look at any data that aren't thread-safe.
 */
int CHtmlSys_dbgmain::run_compiler(int command_id, const TdbBuildTargets &t)
{
    CStringBufCmd cmdline(2048);
    char extres_base_filename[OSFNMAX] = "";
    char exe[OSFNMAX] = "";
    char proj_dir[OSFNMAX];
    char gamefile[OSFNMAX];
    char optfile[OSFNMAX];
    char buf[1024];
    int i;
    int outcnt;
    int cnt;
    int intval;
    DWORD thread_id;
    int extres_cnt;
    int cur_extres;
    int force_full = FALSE;
    char tmpdir[OSFNMAX];
    CStringBuf tadslib_var;
    CStringBuf env_var;
    CHtmlDebugConfig *const lc = local_config_;
    const textchar_t *tc_desc = "Compiling game";
    int ok = FALSE;

    /*
     *   make one last check to make sure we don't already have a build in
     *   progress - we can only handle one at a time
     */
    if (build_running_)
        return FALSE;

    /* run all commands in the project directory */
    GetFullPathName(local_config_filename_.get(), sizeof(buf), buf, 0);
    os_get_path_name(proj_dir, sizeof(proj_dir), buf);

    /*
     *   Set up the TADSLIB environment variable for the compiler.  We set
     *   this to the list of library directories, with entries separated by
     *   semicolons.
     */
    outcnt = 0;

    /* add the extensions directory to the library path, if it's set */
    if (!global_config_->getval("extensions dir", 0, 0, buf, sizeof(buf))
        && buf[0] != '\0')
    {
        /* add it */
        tadslib_var.append_inc(buf, 512);

        /* count it */
        ++outcnt;
    }

    /* add the paths from the "srcdir" list */
    cnt = cnt = global_config_->get_strlist_cnt("srcdir", 0);
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get this element */
        global_config_->getval("srcdir", 0, i, buf, sizeof(buf));

        /*
         *   if this isn't the first element, add a semicolon to separate it
         *   from the previous element
         */
        if (outcnt != 0)
            tadslib_var.append_inc(";", 512);

        /* append this element to the path under construction */
        tadslib_var.append_inc(buf, 512);

        /* count it */
        ++outcnt;
    }

    /*
     *   If there's a version inherited from our environment, append that
     *   after the paths we've added from our own configuration.
     */
    if (orig_tadslib_var_.get() != 0 && orig_tadslib_var_.get()[0] != '\0')
    {
        /*
         *   if we've added any paths so far, add a semicolon to separate the
         *   earlier paths from the TADSLIB paths
         */
        if (outcnt != 0)
            tadslib_var.append_inc(";", 512);

        /* append the original TADSLIB path */
        tadslib_var.append(orig_tadslib_var_.get());
    }

    /*
     *   Now that we have the properly-formatted list of directory names, set
     *   TADSLIB in our environment, so the compiler child process will
     *   inherit it.  We have no use for the value ourselves, as we've
     *   stashed away a private copy in orig_tadslib_var_, so there's no
     *   reason not to just clobber whatever's in our own environment.
     */
    SetEnvironmentVariable("TADSLIB", tadslib_var.get());

    /* presume we won't need a temporary directory */
    tmpdir[0] = '\0';

    /* clear the build command list */
    clear_build_cmds();

    /*
     *   Build the name of the game file.  If we're building a debug game,
     *   use the current game name.  If we're building the release game,
     *   use the release game name.  If we're building an executable,
     *   build to a temporary file.
     */
    switch (t.build_type)
    {
    case TdbBuildTargets::BUILD_DEBUG:
    case TdbBuildTargets::BUILD_CLEAN:
        /* building/cleaning a debug version - build to the debug game file */
        strcpy(gamefile, get_gamefile_name());

        /* if this is a full build, add the 'force' flag */
        if (command_id == ID_BUILD_COMPDBG_FULL)
            force_full = TRUE;
        break;

    case TdbBuildTargets::BUILD_RELEASE:
        /* preparing a release - force a full build */
        force_full = TRUE;

        /* build to the release game file */
        lc->getval("build", "rlsgam", 0, gamefile, sizeof(gamefile));

        /* create a temporary directory for our intermediate files */
        if (create_temp_dir(tmpdir, handle_))
            goto cleanup;

        /* base the external resource filenames on the release filename */
        strcpy(extres_base_filename, gamefile);

        /* describe the build specifically as a release build */
        tc_desc = "Compiling release verison of game";
        break;

    default:
        break;
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Step one: if we're building the game, run the compiler.
     */
    if (t.build_type != TdbBuildTargets::BUILD_NONE)
    {
        /* generate the name of the compiler executable */
        GetModuleFileName(0, exe, sizeof(exe));
        strcpy(os_get_root_name(exe), "t3make.exe");

        /* start the command with the executable name */
        cmdline.set("");
        cmdline.append_qu(exe, FALSE);

        /* if we're in "clean" mode, add the "-clean" option */
        if (t.build_type == TdbBuildTargets::BUILD_CLEAN)
            cmdline.append_inc(" -clean", 512);

        /*
         *   If we're scanning for #include files, add the -Pi option.  If
         *   we're doing a build-plus-scan, do the scan now, since we want to
         *   get that out of the way before we start the full build.
         */
        if (command_id == ID_PROJ_SCAN_INCLUDES
            || command_id == ID_BUILD_COMPDBG_AND_SCAN)
            cmdline.append_inc(" -Pi", 512);

        /* write intermediate files to our temp dir if we have one */
        if (tmpdir[0] != '\0')
        {
            /* add the symbol file directory option */
            cmdline.append_opt("-Fy", tmpdir);

            /* add the object file directory option */
            cmdline.append_opt("-Fo", tmpdir);
        }
        else
        {
            /* add the symbol file path elements, if any */
            cnt = lc->get_strlist_cnt("build", "symfile path");
            for (i = 0 ; i < cnt ; ++i)
            {
                /* get this path */
                lc->getval("build", "symfile path", i, buf, sizeof(buf));

                /* add the -Fy option and the path */
                cmdline.append_opt("-Fy", buf);

                /* make sure the directory exists */
                if (osfacc(buf))
                    add_build_cmd("Creating symbol file directory",
                                  "mkdir", "mkdir", buf, proj_dir, FALSE);
            }

            /* add the object file path elements, if any */
            cnt = lc->get_strlist_cnt("build", "objfile path");
            for (i = 0 ; i < cnt ; ++i)
            {
                /* get this path */
                lc->getval("build", "objfile path", i, buf, sizeof(buf));

                /* add the -Fy option and the path */
                cmdline.append_opt("-Fo", buf);

                /* make sure the directory exists */
                if (osfacc(buf))
                    add_build_cmd("creating object file directory",
                                  "mkdir", "mkdir", buf, proj_dir, FALSE);
            }
        }

        /* add the include list */
        cnt = lc->get_strlist_cnt("build", "includes");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this include path */
            lc->getval("build", "includes", i, buf, sizeof(buf));

            /* add the -i option and the path */
            cmdline.append_opt("-I", buf);
        }

        /*
         *   add the output image file name option - enclose the filename in
         *   quotes in case it contains spaces
         */
        cmdline.append_opt("-o", gamefile);

        /* make sure the image file output directory exists */
        os_get_path_name(buf, sizeof(buf), gamefile);
        if (strlen(buf) != 0 && osfacc(buf))
            add_build_cmd("Creating output directory",
                          "mkdir", "mkdir", buf, proj_dir, FALSE);

        /* if we need to force a full build, add the -a option */
        if (force_full)
            cmdline.append_inc(" -a", 512);

        /* add the #defines, enclosing each string in quotes */
        cnt = lc->get_strlist_cnt("build", "defines");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this symbol definition */
            lc->getval("build", "defines", i, buf, sizeof(buf));

            /* add the -D option */
            cmdline.append_opt("-D", buf);
        }

        /*
         *   add the undefs; these don't need quotes, since they must be
         *   simple identifiers
         */
        cnt = lc->get_strlist_cnt("build", "undefs");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this symbol */
            lc->getval("build", "undefs", i, buf, sizeof(buf));

            /* add the -U option and the symbol to undefine */
            cmdline.append_opt("-U", buf);
        }

        /* add the character set option if required */
        if (lc->getval_int("build", "use charmap", FALSE)
            && !lc->getval("build", "charmap", 0, buf, sizeof(buf)))
        {
            /* add the option */
            cmdline.append_opt("-cs", buf);
        }

        /* add the "verbose" option if required */
        if (lc->getval_int("build", "verbose errors", TRUE))
            cmdline.append_inc(" -v", 512);

        /* add the "-nopre" option if appropriate */
        if (!lc->getval_int("build", "run preinit", TRUE))
            cmdline.append_inc(" -nopre", 512);

        /* add the "-nodef" option if desired */
        if (!lc->getval_int("build", "keep default modules", TRUE))
            cmdline.append_inc(" -nodef", 512);

        /* add the "-Gstg" option if desired */
        if (lc->getval_int("build", "gen sourceTextGroup", FALSE))
            cmdline.append_inc(" -Gstg", 512);

        /*
         *   add the warning level option if required - note that warning
         *   level 1 is the default, so we only need to add the warning level
         *   if it's something different
         */
        intval = lc->getval_int("build", "warning level", 1);
        if (intval != 1)
        {
            char buf[20];

            /* build the option and add it to the command line */
            sprintf(buf, " -w%d", intval);
            cmdline.append_inc(buf, 512);
        }

        /* add the "warnings as errors" option */
        if (lc->getval_int("build", "warnings as errors", TRUE))
            cmdline.append_inc(" -we", 512);

        /* add the source file search path elements, if any */
        cnt = lc->get_strlist_cnt("build", "srcfile path");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this source file path */
            lc->getval("build", "srcfile path", i, buf, sizeof(buf));

            /* add the -Fy option and the path */
            cmdline.append_opt("-Fs", buf);
        }

        /* add the extra options - just add them exactly as they're entered */
        cnt = lc->get_strlist_cnt("build", "extra options");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this option */
            lc->getval("build", "extra options", i, buf, sizeof(buf));

            /* add a space, then this line of options */
            cmdline.append_inc(" ", 512);
            cmdline.append_inc(buf, 512);
        }

        /* if we're compiling for debugging, add the debug option */
        if (t.build_type == TdbBuildTargets::BUILD_DEBUG)
            cmdline.append_inc(" -d", 512);

        /*
         *   add our special status-message prefix, so we can easily pick out
         *   the status messages from the output stream while we're compiling
         */
        cmdline.append_inc(" -statprefix <@> -statpct", 512);

        /* add the list of source files */
        cnt = lc->get_strlist_cnt("build", "source_files");
        for (i = 0 ; i < cnt ; ++i)
        {
            int is_lib;

            /* get this source file */
            lc->getval("build", "source_files", i, buf, sizeof(buf));

            /* check to see if it's a library file */
            is_lib = is_source_file_a_lib(lc, i);

            /*
             *   if the filename starts with a hyphen, we need to include a
             *   type specifier so that the compiler doesn't confuse the
             *   filename for an option
             */
            if (buf[0] == '-')
            {
                /*
                 *   add the appropriate specifier, depending on whether it's
                 *   a source file or a library file
                 */
                cmdline.append_inc(is_lib ? " -lib" : " -source", 512);
            }

            /* add the filename it to the command line in quotes */
            cmdline.append_qu(buf, TRUE);

            /* if it's a library, add "-x" options as needed */
            if (is_lib)
            {
                char var_name[1024];
                int xcnt;
                int xi;

                /* build the exclusion variable name */
                sprintf(var_name, "lib_exclude:%s", buf);

                /* run through the exclusion list */
                xcnt = lc->get_strlist_cnt("build", var_name);
                for (xi = 0 ; xi < xcnt ; ++xi)
                {
                    char xbuf[1024];

                    /* get the item */
                    lc->getval("build", var_name, xi, xbuf, sizeof(xbuf));

                    /* add a "-x" option for this item */
                    cmdline.append_inc(" -x", 512);
                    cmdline.append_qu(xbuf, TRUE);
                }
            }
        }

        /* add the list of resource files, if not compiling for debugging */
        /*
         *   note - as of 3.0.17, we do this in ALL build modes: in debug
         *   mode, the compiler automatically uses the new "resource link"
         *   feature, which builds the resource map as links to local files;
         *   this gives us essentially the same time and space savings as
         *   leaving out the resources entirely, but still gives us a proper
         *   resource naming map, even in debug mode
         */
        // if (t.build_type != TdbBuildTargets::BUILD_DEBUG)
        {
            int recurse = TRUE;
            const textchar_t *val;

            /* add the "-res" option */
            cmdline.append_inc(" -res", 512);

            /* scan the resource file list */
            cnt = lc->get_strlist_cnt("build", "image_resources");
            for (i = 0 ; i < cnt ; ++i)
            {
                char rbuf[5];
                int new_recurse;

                /* get this resource */
                lc->getval("build", "image_resources", i, buf, sizeof(buf));

                /* note its recursive status */
                lc->getval("build", "image_resources_recurse",
                           i, rbuf, sizeof(rbuf));
                new_recurse = (rbuf[0] == 'Y');

                /* add a -recurse/-norecurse option if we're changing modes */
                if (recurse != new_recurse)
                {
                    /* add the option */
                    cmdline.append_inc(
                        new_recurse ? " -recurse" : " -norecurse", 512);

                    /* note the new mode */
                    recurse = new_recurse;
                }

                /* add this resource file */
                cmdline.append_qu(buf, TRUE);
            }

            /* add the Cover Art image */
            if ((val = lc->getval_strptr(
                "build special file", "cover art", 0)) != 0
                && val[0] != '\0')
            {
                /*
                 *   check to see if it's PNG or JPEG - if the name doesn't
                 *   end in .PNG, assume it's a JPEG image
                 */
                const textchar_t *ext = strrchr(val, '.');
                if (ext == 0
                    || (stricmp(ext, ".png") != 0
                        && stricmp(ext, ".jpg") != 0))
                    ext = ".jpg";

                /*
                 *   add the file, giving it the required special resource
                 *   path and name
                 */
                sprintf(buf, "%s=.system/CoverArt%s", val, ext);
                cmdline.append_qu(buf, TRUE);
            }
        }

        /* add the command to the list */
        add_build_cmd(tc_desc, "t3make", exe, cmdline.get(), proj_dir, FALSE);
    }

    /* -------------------------------------------------------------------- */
    /*
     *   Step two: run the resource bundler to build the external resource
     *   files.  Bundle resources only for release/exe builds, not for
     *   debugging builds or non-builds.  Note that this isn't for resources
     *   bundled directly into the .t3 file, since we've already done that
     *   through the compiler.
     */
    if (t.build_type == TdbBuildTargets::BUILD_RELEASE)
    {
        /* get the number of external resource files */
        if (lc->getval("build", "ext resfile cnt", &extres_cnt))
            extres_cnt = 0;

        /* build the external resource files */
        for (cur_extres = 0 ; cur_extres < extres_cnt ; ++cur_extres)
        {
            char config_name[50];

            /*
             *   check to see if we should include this file, and get its
             *   name if so - if this returns false, it means the file
             *   shouldn't be included, so just skip it in this case
             */
            if (!build_get_extres_file(buf, cur_extres, command_id,
                                       extres_base_filename))
                continue;

            /* build the name of the bundler */
            strcpy(os_get_root_name(exe), "t3res.exe");

            /* start the command line */
            cmdline.set("");
            cmdline.append_qu(exe, FALSE);

            /* add the resource filename with a -create option */
            cmdline.append_inc(" -create", 512);
            cmdline.append_qu(buf, TRUE);

            /* add the action specifiers */
            cmdline.append_inc(" -add", 512);

            /* generate the name of this external resource file */
            CHtmlDbgProjWin::gen_extres_names(0, config_name, cur_extres);

            /* get the count of resource files to go into the image file */
            cnt = lc->get_strlist_cnt("build", config_name);

            /* add the image file resources */
            for (i = 0 ; i < cnt ; ++i)
            {
                /* get this source file */
                lc->getval("build", config_name, i, buf, sizeof(buf));

                /* add it to the command line in quotes */
                cmdline.append_qu(buf, TRUE);
            }

            /* add the build command */
            add_build_cmd("Building external resource file",
                          "t3res", exe, cmdline.get(), proj_dir, FALSE);
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   If we're building an executable, run the executable bundler
     */
    if (t.build_exe)
    {
        const textchar_t *val;

        /* build the name of the executable builder */
        strcpy(os_get_root_name(exe), "maketrx32.exe");

        /* start the command with the executable name */
        cmdline.set("");
        cmdline.append_qu(exe, FALSE);

        /* set the "-t3" option */
        cmdline.append_qu("-t3", TRUE);

        /* if there's an icon specified, add it */
        if ((val = lc->getval_strptr(
            "build special file", "exe icon", 0)) != 0
            && *val != '\0')
        {
            cmdline.append_qu("-icon", TRUE);
            cmdline.append_qu(val, TRUE);
        }

        /* likewise the character map file */
        if ((val = lc->getval_strptr(
            "build special file", "cmaplib", 0)) != 0
            && *val != '\0')
        {
            cmdline.append_qu("-clib", TRUE);
            cmdline.append_qu(val, TRUE);
        }

        /* specify the source executable (in quotes) */
        GetModuleFileName(0, buf, sizeof(buf));
        strcpy(os_get_root_name(buf), "htmlt3.exe");
        cmdline.append_qu(buf, TRUE);

        /* add the game name */
        cmdline.append_qu(gamefile, TRUE);

        /* add the executable filename in quotes */
        lc->getval("build", "exe", 0, buf, sizeof(buf));
        cmdline.append_qu(buf, TRUE);

        /* run the command */
        add_build_cmd("Building stand-alone game application",
                      "maketrx32", exe, cmdline.get(), proj_dir, FALSE);
    }


    /* -------------------------------------------------------------------- */
    /*
     *   If we're building the installer, run the install maker
     */
    if (t.build_setup)
    {
        /* build the name of the installer maker */
        strcpy(os_get_root_name(exe), "mksetup.exe");

        /* start the command with the executable name */
        cmdline.set("");
        cmdline.append_qu(exe, FALSE);

        /*
         *   add the -detached option - this causes the installer creator to
         *   run its own subprocesses as detached, so that we get the results
         *   piped back to us through the standard handles that we create
         */
        cmdline.append_inc(" -detached", 512);

        /* create a temporary options file name */
        GetTempPath(sizeof(buf), buf);
        GetTempFileName(buf, "tads", 0, optfile);

        /* add the options file - this is a temp file */
        cmdline.append_qu(optfile, TRUE);

        /* add the name of the resulting installation program */
        lc->getval("build", "installer prog", 0, buf, sizeof(buf));
        cmdline.append_qu(buf, TRUE);

        /* add the command */
        add_build_cmd("Building Windows SETUP program",
                      "mksetup", exe, cmdline.get(), proj_dir, FALSE);

        /* add a build command to delete the temporary options file */
        add_build_cmd("Cleaning up temporary files",
                      "del", "del", optfile, "", TRUE);

        /* create the options file */
        {
            FILE *fp;
            int cnt;
            int i;
            int found_name = FALSE;
            int found_savext = FALSE;
            int found_gam = FALSE;
            int found_exe = FALSE;
            CHtmlHashTable files(128, new CHtmlHashFuncCI());
            const textchar_t *val;

            /* open the file */
            fp = fopen(optfile, "w");

            /* always include the "T3" option */
            fprintf(fp, "T3=1\n");

            /* write the options lines */
            cnt = lc->get_strlist_cnt("build", "installer options");
            for (i = 0 ; i < cnt ; ++i)
            {
                char *p;

                /* get this string */
                lc->getval("build", "installer options", i, buf, sizeof(buf));

                /* find the keyword */
                if ((p = strchr(buf, '=')) != 0)
                {
                    size_t kwlen = p - buf;
                    p += 1;

                    if (kwlen == 4 && memicmp(buf, "name", 4) == 0)
                        found_name = TRUE;
                    else if (kwlen == 6 && memicmp(buf, "savext", 6) == 0)
                        found_savext = TRUE;
                    else if (kwlen == 3 && memicmp(buf, "gam", 3) == 0)
                        found_gam = TRUE;
                    else if (kwlen == 4 && memicmp(buf, "exe", 3) == 0)
                        found_exe = TRUE;

                    /* if it's a file item, add it to the file hash table */
                    if ((kwlen == 3 && memicmp(buf, "gam", 3) == 0)
                        || (kwlen == 3 && memicmp(buf, "exe", 3) == 0)
                        || (kwlen == 7 && memicmp(buf, "charlib", 7) == 0)
                        || (kwlen == 7 && memicmp(buf, "license", 7) == 0)
                        || (kwlen == 6 && memicmp(buf, "readme", 6) == 0))
                    {
                        /* this refers to a file - add it */
                        files.add(new CHtmlHashEntryCI(p, strlen(p), TRUE));
                    }
                }

                /* write it out */
                fprintf(fp, "%s\n", buf);
            }

            /* write out the "GAM" line if we didn't find an explicit one */
            if (!found_gam)
                fprintf(fp, "GAM=%s\n", gamefile);

            /* write out the "EXE" line if we didn't find an explicit one */
            if (!found_exe)
            {
                /* use the normal build .EXE filename here, sans path */
                lc->getval("build", "exe", 0, buf, sizeof(buf));
                fprintf(fp, "EXE=%s\n", os_get_root_name(buf));
            }

            /* add the special files */
            if ((val = lc->getval_strptr(
                "build special file", "readme", 0)) != 0
                && val[0] != '\0')
                fprintf(fp, "readme=%s\n", val);

            if ((val = lc->getval_strptr(
                "build special file", "license", 0)) != 0
                && val[0] != '\0')
                fprintf(fp, "license=%s\n", val);

            if ((val = lc->getval_strptr(
                "build special file", "exe icon", 0)) != 0
                && val[0] != '\0')
                fprintf(fp, "icon=%s\n", val);

            if ((val = lc->getval_strptr(
                "build special file", "cmaplib", 0)) != 0
                && val[0] != '\0')
                fprintf(fp, "charlib=%s\n", val);

            /* set the ReadMe title, if applicable */
            if ((val = lc->getval_strptr(
                "build special file title", "readme", 0)) != 0
                && val[0] != '\0')
                fprintf(fp, "readme_title=%s\n", val);

            /* add the external resources we're including */
            for (cur_extres = 0 ; cur_extres < extres_cnt ; ++cur_extres)
            {
                /*
                 *   check to see if we should include this file, and get its
                 *   name if so - if this returns false, it means the file
                 *   shouldn't be included, so just skip it in this case
                 */
                if (!build_get_extres_file(buf, cur_extres, command_id,
                                           extres_base_filename))
                    continue;

                /* add this file to the installer options */
                fprintf(fp, "FILE=%s\n", buf);
            }

            /* add the feelies */
            cnt = lc->get_strlist_cnt("build", "feelies");
            for (i = 0 ; i < cnt ; ++i)
            {
                /* get this file */
                lc->getval("build", "feelies", i, buf, sizeof(buf));

                /*
                 *   if this file has already been added as one of the
                 *   special files (README=, LICENSE=, etc), there's no need
                 *   to add it again
                 */
                if (files.find(buf, strlen(buf)) != 0)
                    continue;

                /* check for a start menu title */
                val = lc->getval_strptr("build feelie title", buf, 0);
                if (val != 0 && *val != '\0')
                {
                    /* add it as a start menu item */
                    fprintf(fp, "startmenu=\"%s\" %s\n", val, buf);
                }
                else
                {
                    /* add it as an ordinary FILE= entry */
                    fprintf(fp, "FILE=%s\n", buf);
                }
            }

            /* add the interpreter help file */
            strcpy(os_get_root_name(exe), "htmltads.chm");
            fprintf(fp, "file=%s=htmltads.chm\n", exe);

            /* we've finished writing the options file */
            fclose(fp);

            /* if we didn't find the NAME or SAVEXT settings, it's an error */
            if (!found_name || !found_savext)
            {
                /* show the message */
                MessageBox(handle_, "Before you can build an installer, you "
                           "must fill in the Name and Save Extension options "
                           "in the installer configuration.  Please fill in "
                           "these values, then use Compile Installer again "
                           "when you're done.", "TADS Workbench",
                           MB_OK | MB_ICONEXCLAMATION);

                /* take them to the dialog */
                run_dbg_build_dlg(handle_, this, DLG_BUILD_INSTALL, 0);

                /*
                 *   delete the temporary files (for the options file and for
                 *   the game), since we won't actually be carrying out the
                 *   build commands, which would ordinarily clean up the temp
                 *   files
                 */
                DeleteFile(optfile);
                DeleteFile(gamefile);

                /* delete our temporary directory */
                del_temp_dir(tmpdir);

                /* return without starting a build */
                goto cleanup;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   If we're building a ZIP file for the release, add the commands to
     *   create the file.
     */
    if (t.build_zip)
    {
        const textchar_t *val;

        /* start with a blank command line */
        cmdline.set("");

        /* add the zip output file */
        lc->getval("build", "zipfile", 0, buf, sizeof(buf));
        cmdline.append_qu(buf, FALSE);

        /* add the release game file */
        cmdline.append_qu(gamefile, TRUE);

        /* add the special files */
        if ((val = lc->getval_strptr(
            "build special file", "readme", 0)) != 0
            && val[0] != '\0')
            cmdline.append_qu(val, TRUE);

        if ((val = lc->getval_strptr(
            "build special file", "license", 0)) != 0
            && val[0] != '\0')
            cmdline.append_qu(val, TRUE);

        /* add the feelies */
        cnt = lc->get_strlist_cnt("build", "feelies");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get this file */
            lc->getval("build", "feelies", i, buf, sizeof(buf));

            /* add it to the command line */
            cmdline.append_qu(buf, TRUE);
        }

        /* add the command */
        add_build_cmd("Building Release ZIP file",
                      "zip", "zip", cmdline.get(), proj_dir, FALSE);
    }


    /* -------------------------------------------------------------------- */
    /*
     *   If we're building a ZIP file for the source, add the commands to
     *   create the file.
     */
    if (t.build_srczip)
    {
        const textchar_t *val;

        /*
         *   Structure for a sub-ZIP directory list entry.  For each external
         *   folder (i.e., not within the project directory) that we find to
         *   contain a file in the project list, we need to create a ZIP file
         *   containing all of the files in that folder.  This allows the
         *   receiver of the source ZIP package to expand each external
         *   third-party or custom library in a suitable directory on the
         *   target machine.
         */
        struct dir_link
        {
            dir_link(const textchar_t *dir, const textchar_t *parent)
                : dir(dir), parent(parent),
                  zipname(os_get_root_name((char *)dir))
            {
                /* not in a list yet */
                nxt = 0;
            }

            /* next item in our list */
            dir_link *nxt;

            /*
             *   The directory to include.  This is the full path to the
             *   directory; in the ZIP we build, we'll recursively include
             *   all files within this folder.
             */
            CStringBuf dir;

            /*
             *   The parent directory.  This is the base path for the ZIP
             *   file's contents: the path for each item stored in the ZIP
             *   file will be the relative path starting from this base path.
             *   For example, if 'parent' is C:\TEST, the file system file
             *   C:\TEST\SUB\MYFILE.TXT will be stored in the ZIP file as
             *   SUB\MYFILE.TXT.
             */
            CStringBuf parent;

            /*
             *   The name of this ZIP file.  We'll generally name each
             *   sub-ZIP file with the name of the final directory in the
             *   path; for example, for the external folder C:\TEST\SUB, we'd
             *   try to name the sub-ZIP file SUB.ZIP.  However, it's
             *   possible that the same leaf folder name will be used in
             *   multiple external folders - we could have C:\TEST\SUB and
             *   C:\TEST2\SUB in the same project, for instance.  In this
             *   case, we'll find a numerical suffix that makes the second
             *   file's name unique - SUB2.ZIP, for instance.
             *
             *   Everything within the main Extensions folder (as set in the
             *   preferences) goes in EXTENSIONS.ZIP.
             */
            CStringBuf zipname;

            /*
             *   the name of the actual temporary file we create to hold the
             *   ZIP file
             */
            CStringBuf tmpname;
        };

        /*
         *   A file link for a file in the main Extensions directory.  We
         *   store these files within a separate Extensions.zip file.
         */
        struct file_link
        {
            file_link(const textchar_t *fname, proj_item_type_t typ)
                : fname(fname), typ(typ)
            {
                nxt = 0;
            }

            /* next list entry */
            file_link *nxt;

            /* the filename */
            CStringBuf fname;

            /* project item type */
            proj_item_type_t typ;
        };

        /*
         *   First, build sub-ZIP files for any libraries included from
         *   outside of the project directory.  This will ensure that
         *   third-party and private extension libraries are included in the
         *   package, but it will still make it clear that these are meant to
         *   be separate packages.
         */
        class subzip_cb: public IProjWinFileCallback
        {
        public:
            subzip_cb(CHtmlSys_dbgmain *dbgmain, CHtmlDbgProjWin *projwin)
                : dbgmain(dbgmain)
            {
                char path[OSFNMAX];
                dir_link *cur, *prv, *nxt;
                file_link *fcur, *fprv, *fnxt;

                /* there are no directories in our list yet */
                dirs = 0;
                files = 0;

                /*
                 *   get the system library and header paths - we won't
                 *   include anything in these folders in the ZIP file, since
                 *   it should be safe to assume that receivers of the ZIP
                 *   file will already have copies of the system files
                 */
                os_get_special_path(
                    path, sizeof(path), _pgmptr, OS_GSP_T3_LIB);
                sys_lib_path.set(path);

                os_get_special_path(
                    path, sizeof(path), _pgmptr, OS_GSP_T3_INC);
                sys_inc_path.set(path);

                /* remember the extensions folder, if set */
                if (!dbgmain->get_global_config()->getval(
                    "extensions dir", 0, 0, path, sizeof(path))
                    && path[0] != '\0')
                    exts_path.set(path);

                /*
                 *   enumerate the files through our callback to build the
                 *   list of sub-ZIP files we'll need
                 */
                projwin->enum_all_files(this);

                /*
                 *   reverse the list - we built it up in reverse order from
                 *   the project file enumeration, so to make it match the
                 *   project list order, reverse it
                 */
                for (prv = 0, cur = dirs ; cur != 0 ; prv = cur, cur = nxt)
                {
                    /* reverse this link's direction */
                    nxt = cur->nxt;
                    cur->nxt = prv;
                }

                /* the last item we visited is now the head of the list */
                dirs = prv;

                /* reverse the file list to get it back in project order */
                for (fprv = 0, fcur = files ; fcur != 0 ;
                     fprv = fcur, fcur = fnxt)
                {
                    /* reverse this link's direction */
                    fnxt = fcur->nxt;
                    fcur->nxt = fprv;
                }
                files = fprv;

                /* make sure the ZIP file names are unique */
                uniquify_zips();
            }

            ~subzip_cb()
            {
                /* delete our directory list */
                while (dirs != 0)
                {
                    dir_link *nxt = dirs->nxt;
                    delete dirs;
                    dirs = nxt;
                }

                /* delete our file list */
                while (files != 0)
                {
                    file_link *nxt = files->nxt;
                    delete files;
                    files = nxt;
                }
            }

            /* shortcut to is-file-in-dir */
            int is_file_in_dir(const textchar_t *file, const textchar_t *dir)
            {
                return dbgmain->is_file_in_dir(file, dir);
            }

            /* project file iteration callback */
            void file_cb(const textchar_t *fname, const textchar_t *relname,
                         proj_item_type_t typ)
            {
                /*
                 *   If this isn't a local file, queue its directory for
                 *   inclusion as a sub-zip.  However, don't include files
                 *   within the system library and include folders - it
                 *   should be safe to assume that recipients of the source
                 *   ZIP package will already have copies of the system files
                 *   as part of their TADS install set.
                 */
                if (!dbgmain->is_local_project_file(fname)
                    && ((exts_path.get() != 0
                         && exts_path.get()[0] != '\0'
                         && is_file_in_dir(fname, exts_path.get()))
                        || (!is_file_in_dir(fname, sys_lib_path.get())
                            && !is_file_in_dir(fname, sys_inc_path.get()))))
                {
                    dir_link *d, *prv, *nxt;
                    textchar_t dir[OSFNMAX];
                    textchar_t parent[OSFNMAX];

                    /*
                     *   Check to see if this file is in any of the
                     *   directories we're already including.  If so, there's
                     *   no need to include it again.
                     */
                    for (d = dirs ; d != 0 ; d = d->nxt)
                    {
                        /* if it's in this directory, we can skip it */
                        if (dbgmain->is_file_in_dir(fname, d->dir.get()))
                            return;
                    }

                    /*
                     *   if this isn't a folder, get the directory containing
                     *   the file; if it's already a folder, we actually want
                     *   to include the whole folder, so leave it as-is
                     */
                    if (typ == PROJ_ITEM_TYPE_FOLDER)
                    {
                        /* it's a folder - simply include it directly */
                        safe_strcpy(dir, sizeof(dir), fname);
                    }
                    else
                    {
                        const textchar_t *exts = exts_path.get();

                        /* get the parent folder */
                        os_get_path_name(dir, sizeof(dir), fname);

                        /*
                         *   If this file is DIRECTLY within the Extensions
                         *   directory, include it in the file list for the
                         *   Extensions.zip file, rather than including the
                         *   whole separate directory.  We assume that
                         *   individual files directly in the Extensions
                         *   folder are self-contained .t files, since
                         *   they're not grouped in their own subdirectories.
                         *   And in any case, we don't want to include the
                         *   ENTIRE Extensions tree just because we have one
                         *   file in the root of that tree.
                         */
                        if (exts != 0
                            && exts[0] != '\0'
                            && dbgmain->dbgsys_fnames_eq(dir, exts))
                        {
                            /*
                             *   It's directly in the Extensions folder -
                             *   store only this file rather than the whole
                             *   directory.  Set up the link.
                             */
                            file_link *f = new file_link(fname, typ);

                            /* link it into our list */
                            f->nxt = files;
                            files = f;

                            /* we're done with this file */
                            return;
                        }
                    }

                    /*
                     *   get the parent path - this is the base directory
                     *   that we use to build the ZIP file, since that way
                     *   the ZIP file will use the same relative path that
                     *   the project uses to refer to the file
                     */
                    get_parent_path(parent, sizeof(parent), fname, relname);

                    /* set up the new list entry */
                    d = new dir_link(dir, parent);

                    /* link it in */
                    d->nxt = dirs;
                    dirs = d;

                    /*
                     *   Now go back through the list and delete any existing
                     *   items that turn out to be inside this new item.
                     */
                    for (prv = dirs, d = dirs->nxt ; d != 0 ; d = nxt)
                    {
                        /* save the next item, in case we delete this one */
                        nxt = d->nxt;

                        /* if this one is inside the new one, delete it */
                        if (dbgmain->is_file_in_dir(
                            d->dir.get(), dirs->dir.get()))
                        {
                            /* clip it out */
                            prv->nxt = d->nxt;
                            delete d;
                        }
                        else
                        {
                            /*
                             *   we're keeping this one, so it's the previous
                             *   item for the next round
                             */
                            prv = d;
                        }
                    }
                }
            }

            /*
             *   get the parent directory for a given file - this retrieves
             *   the part of the full path up to the relative name
             */
            void get_parent_path(char *buf, size_t buflen,
                                 const textchar_t *fullname,
                                 const textchar_t *relname)
            {
                /*
                 *   A relative directory should end with the relative name.
                 *   Make sure the last n characters of fullname match the n
                 *   characters of relname.  If not, it's not really a
                 *   relative path, so just use the entire directory path
                 *   from fullname.
                 */
                size_t flen = strlen(fullname);
                size_t rlen = strlen(relname);
                if (flen < rlen + 1
                    || strchr("/\\:", fullname[flen - rlen - 1]) == 0
                    || stricmp(fullname + (flen - rlen), relname) != 0)
                {
                    /* it's not really a relative name */
                    os_get_path_name(buf, buflen, fullname);
                }
                else
                {
                    /*
                     *   it's relative - pull out the part up to (but not
                     *   including) the path separator before the relative
                     *   part
                     */
                    safe_strcpy(buf, buflen, fullname, flen - rlen - 1);
                }
            }

            /* ensure that each dir list entry has a unique ZIP file name */
            void uniquify_zips()
            {
                dir_link *d;

                /* run through the list looking for duplicate names */
                for (d = dirs ; d != 0 ; d = d->nxt)
                {
                    dir_link *d2;

                    /*
                     *   look for duplicates later in the list - we know we
                     *   won't have any earlier, since we will already have
                     *   ensured that all later names are unique
                     */
                    for (d2 = d->nxt ; d2 != 0 ; d2 = d2->nxt)
                    {
                        /* if this one matches d's name, change it */
                        if (stricmp(d->zipname.get(), d2->zipname.get()) == 0
                            || (files != 0
                                && stricmp(d2->zipname.get(), "Extensions")
                                    == 0))
                        {
                            int i;

                            /* try numerical suffixes until it's unique */
                            for (i = 2 ; ; ++i)
                            {
                                int found;
                                char buf[OSFNMAX];
                                dir_link *d3;

                                /* build the new name */
                                sprintf(buf, "%s%d", d2->zipname.get(), i);

                                /* make sure the new name is unique */
                                for (found = FALSE, d3 = dirs ;
                                     d3 != 0 ; d3 = d3->nxt)
                                {
                                    if (d3 != d2
                                        && stricmp(d3->zipname.get(), buf)
                                           == 0)
                                    {
                                        found = TRUE;
                                        break;
                                    }
                                }

                                /* if it's unique, use it */
                                if (!found)
                                {
                                    /* apply the new name */
                                    d2->zipname.set(buf);

                                    /* no need to keep looking */
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            /* add commands to the build to create the sub-ZIP files */
            void add_zip_commands()
            {
                char buf[MAX_PATH];

                /* if there are no sub-ZIPs, skip this */
                if (dirs == 0 && files == 0)
                    return;

                /* build our README file */
                GetTempPath(sizeof(buf), buf);
                GetTempFileName(buf, "readme", 0, buf);
                readme_tmp.set(buf);

                /* build the Extensions.zip file */
                if (files != 0)
                {
                    CStringBufCmd cmdline(4096);

                    /* select a temporary filename for this file */
                    GetTempPath(sizeof(buf), buf);
                    GetTempFileName(buf, "extzip", 0, buf);
                    exts_zip_tmpname.set(buf);

                    /* add it to the command line */
                    cmdline.append_qu(buf, FALSE);

                    /* add the files to the command line */
                    for (file_link *f = files ; f != 0 ; f = f->nxt)
                    {
                        /* add the file, or files in the directory */
                        if (f->typ == PROJ_ITEM_TYPE_FOLDER)
                            add_files_to_cmdline(&cmdline, f->fname.get(), 0);
                        else
                            cmdline.append_qu(f->fname.get(), TRUE);
                    }

                    /* build the ZIP command for this file */
                    dbgmain->add_build_cmd(
                        "Building source-code extensions ZIP file",
                        "zip", "zip", cmdline.get(), exts_path.get(), FALSE);
                }

                /* run through the list and build each ZIP file */
                for (dir_link *d = dirs ; d != 0 ; d = d->nxt)
                {
                    CStringBufCmd cmdline(4096);

                    /* select a temporary filename for this file */
                    GetTempPath(sizeof(buf), buf);
                    GetTempFileName(buf, "subzip", 0, buf);
                    d->tmpname.set(buf);

                    /* add it to the command line */
                    cmdline.append_qu(buf, FALSE);

                    /* set up the command line with the files in the folder */
                    add_files_to_cmdline(&cmdline, d->dir.get(), 0);

                    /* build the ZIP command for this file */
                    dbgmain->add_build_cmd(
                        "Building source-code library ZIP file",
                        "zip", "zip", cmdline.get(), d->parent.get(), FALSE);
                }
            }

            /* add our sub-ZIP files to the main ZIP file list */
            void add_zip_file_list(CStringBufCmd *cmdline)
            {
                dir_link *d;

                /* if we have nothing to add, we can skip this */
                if (dirs == 0 && files == 0)
                    return;

                //$$$ make sure "extensions/" isn't taken - rename if so

                /* add the README file in the extensions directory */
                cmdline->append_qu(readme_tmp.get(), TRUE);
                cmdline->append(" ::");
                cmdline->append_qu("extensions/ReadMe.txt", TRUE);

                /* generate the README contents */
                FILE *fp = fopen(readme_tmp.get(), "w");
                if (fp != 0)
                {
                    char buf[4096];

                    /* show the introduction */
                    LoadString(CTadsApp::get_app()->get_instance(),
                               IDS_SUBZIPREADME1, buf, sizeof(buf));
                    fputs(buf, fp);

                    /* list the files */
                    if (files != 0)
                        fprintf(fp, "    Extensions.zip\n");
                    for (d = dirs ; d != 0 ; d = d->nxt)
                        fprintf(fp, "    %s.zip\n", d->zipname.get());

                    /* add the closing text */
                    LoadString(CTadsApp::get_app()->get_instance(),
                               IDS_SUBZIPREADME2, buf, sizeof(buf));
                    fputs(buf, fp);

                    /* done with the file */
                    fclose(fp);
                }

                /* add Extensions.zip, if present */
                if (files != 0)
                {
                    cmdline->append_qu(exts_zip_tmpname.get(), TRUE);
                    cmdline->append(" :: extensions/Extensions.zip");
                }

                /* add each of our files */
                for (d = dirs ; d != 0 ; d = d->nxt)
                {
                    char buf[OSFNMAX + 30];

                    /*
                     *   add this ZIP command - add the file, but name it as
                     *   "extensions/ZIPNAME"
                     */
                    cmdline->append_qu(d->tmpname.get(), TRUE);
                    cmdline->append(" ::");

                    sprintf(buf, "extensions/%s.zip", d->zipname.get());
                    cmdline->append_qu(buf, TRUE);
                }
            }

            /* add commands to delete the temporary sub-ZIP files */
            void add_cleanup_commands()
            {
                /* if we have nothing to add, we can skip this */
                if (dirs == 0 && files == 0)
                    return;

                /* add a delete command for each temporary file */
                for (dir_link *d = dirs ; d != 0 ; d = d->nxt)
                {
                    dbgmain->add_build_cmd(
                        "Cleaning up temporary files",
                        "del", "del", d->tmpname.get(), "", TRUE);
                }

                /* delete the Extensions.zip temp file */
                if (files != 0)
                    dbgmain->add_build_cmd(
                        "Cleaning up temporary files",
                        "del", "del", exts_zip_tmpname.get(), "", TRUE);

                /* delete the readme file */
                dbgmain->add_build_cmd(
                    "Cleaning up temporary files",
                    "del", "del", readme_tmp.get(), "", TRUE);
            }

            /* our list of included directories */
            dir_link *dirs;

            /* our list of extension files in the main extension directory */
            file_link *files;

            /* debugger main window */
            CHtmlSys_dbgmain *dbgmain;

            /* the system library and include paths */
            CStringBuf sys_lib_path, sys_inc_path;

            /* the root extensions folder */
            CStringBuf exts_path;

            /* the filename for the Extensions.zip temp file */
            CStringBuf exts_zip_tmpname;

            /* name of our temp file for extensions/ReadMe.txt contents */
            CStringBuf readme_tmp;
        };

        /* build the list of sub-ZIP files we need for external folders */
        subzip_cb subcb(this, projwin_);

        /* add ZIP commands for the sub-ZIP files */
        subcb.add_zip_commands();

        /* start the ZIP command */
        cmdline.set("");

        /* add the output file */
        lc->getval("build", "srczipfile", 0, buf, sizeof(buf));
        cmdline.append_qu(buf, FALSE);

        /* set up our enumerator callback */
        class srczip_cb: public IProjWinFileCallback
        {
        public:
            srczip_cb(CStringBufCmd *cmdline, CHtmlSys_dbgmain *dbgmain)
                : cmdline(cmdline), dbgmain(dbgmain)
            {
            }

            void file_cb(const textchar_t *fname, const textchar_t *relname,
                         proj_item_type_t typ)
            {
                /* include this file if it's in the project folder */
                if (dbgmain->is_local_project_file(fname))
                {
                    /* include it in the source list */
                    cmdline->append_qu(fname, TRUE);

                    /* if it's a folder, include its contents */
                    DWORD attr = GetFileAttributes(fname);
                    if (attr != INVALID_FILE_ATTRIBUTES
                        && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                        add_files_to_cmdline(cmdline, fname, 0);

                    /*
                     *   if it's a library, make sure we pick up any resource
                     *   directories listed in the library
                     */
                    if (typ == PROJ_ITEM_TYPE_LIB)
                    {
                        /* set up our custom library parser */
                        class MyParser: public CTcLibParser
                        {
                        public:
                            MyParser(const textchar_t *libname,
                                     CStringBufCmd *cmdline)
                                : CTcLibParser(libname), cmdline(cmdline)
                            {
                            }

                            void scan_full_resource(
                                const char *val, const char *fname)
                            {
                                /* look up its attributes */
                                DWORD attr = GetFileAttributes(fname);

                                /*
                                 *   if it's a directory, recursively add its
                                 *   contents; otherwise just add the file
                                 *   itself
                                 */
                                if (attr != INVALID_FILE_ATTRIBUTES
                                    && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
                                    add_files_to_cmdline(cmdline, fname, 0);
                                else
                                    cmdline->append_qu(fname, TRUE);
                            }

                            CStringBufCmd *cmdline;
                        };

                        /* parse the library */
                        MyParser prs(fname, cmdline);
                        prs.parse_lib();
                    }
                }
            }

            /* the command line under construction */
            CStringBufCmd *cmdline;

            /* the main debug window */
            CHtmlSys_dbgmain *dbgmain;
        };

        /* enumerate all of the files in the project */
        srczip_cb cb(&cmdline, this);
        projwin_->enum_all_files(&cb);

        /* include the special directories - objects, symbols */
        if ((val = lc->getval_strptr("build", "objfile path", 0)) != 0)
            cmdline.append_qu(val, TRUE);
        if ((val = lc->getval_strptr("build", "symfile path", 0)) != 0)
            cmdline.append_qu(val, TRUE);

        /* add the path to the debug image file, if in a separate dir */
        if ((val = lc->getval_strptr("build", "debug_image_file", 0)) != 0)
        {
            char path[OSFNMAX];

            os_get_path_name(path, sizeof(path), val);
            if (strlen(path) != 0)
                cmdline.append_qu(path, TRUE);
        }

        /* add the contents of the Scripts directory */
        const textchar_t *scrdir = lc->getval_strptr("auto-script", "dir", 0);
        add_files_to_cmdline(&cmdline, scrdir != 0 ? scrdir : "Scripts",
                             filter_auto_scripts);

        /* add the sub-ZIP files for external folders */
        subcb.add_zip_file_list(&cmdline);

        /* add the command to the work list */
        add_build_cmd("Building source code ZIP file",
                      "zip", "zip", cmdline.get(), proj_dir, FALSE);

        /* add the cleanup commands to delete the temporary sub-ZIP files */
        subcb.add_cleanup_commands();
    }


    /* -------------------------------------------------------------------- */
    /*
     *   If we're building a web page, add the commands to create the page.
     */
    if (t.build_html)
    {
        const textchar_t *val;
        char webproj[OSFNMAX + 30];

        /*
         *   first, build the web-page builder program itself
         */
        val = lc->getval_strptr("build special file", "web builder", 0);
        if (val != 0 && *val != '\0')
        {
            /* use the custom web builder program */
            safe_strcpy(webproj, sizeof(webproj), val);
        }
        else
        {
            /* use our default web-page builder */
            GetModuleFileName(0, webproj, sizeof(webproj));
            strcpy(os_get_root_name(webproj), "mkwebfiles\\mkweb.t3m");
        }

        /* first, generate the command to compile the web builder */
        cmdline.set("");

        GetModuleFileName(0, exe, sizeof(exe));
        strcpy(os_get_root_name(exe), "t3make.exe");
        cmdline.append_qu(exe, FALSE);

        cmdline.append_qu("-f", TRUE);
        cmdline.append_qu(webproj, TRUE);

        /* add the command */
        add_build_cmd("Compiling web-page builder program",
                      "t3make", exe, cmdline.get(), proj_dir, FALSE);


        /*
         *   create the web output directory if it doesn't already exist
         */
        val = lc->getval_strptr("build", "webpage dir", 0);
        DWORD attr = GetFileAttributes(val);
        if (attr == INVALID_FILE_ATTRIBUTES)
            add_build_cmd("Creating web page directory",
                          "mkdir", "mkdir", val, proj_dir, FALSE);


        /*
         *   We've built the builder, so go run it
         */

        cmdline.set("");
        strcpy(os_get_root_name(exe), "t3run.exe");
        cmdline.append_qu(exe, FALSE);
        cmdline.append_qu("-plain", TRUE);
        cmdline.append_qu("-s0", TRUE);
        cmdline.append_qu("-d", TRUE);
        cmdline.append_qu(proj_dir, TRUE);

        os_remext(webproj);
        cmdline.append_qu(webproj, TRUE);

        /* add the tads version */
        cmdline.append_qu("tadsver=3", TRUE);

        /* add the output directory name */
        val = lc->getval_strptr("build", "webpage dir", 0);
        sprintf(buf, "out=%s", val);
        cmdline.append_qu(buf, TRUE);

        /* add the gameinfo file */
        cmdline.append_qu("info=GameInfo.txt", TRUE);

        /* add the release ZIP or .t3 file, as desired */
        if (lc->getval_int("build", "zip in webpage", TRUE))
        {
            /* they want to include the release ZIP package */
            val = lc->getval_strptr("build", "zipfile", 0);
            sprintf(buf, "zipfile=%s", val);
            cmdline.append_qu(buf, TRUE);
        }
        else
        {
            /* they want to include the raw .t3 file only */
            sprintf(buf, "gamefile=%s", gamefile);
            cmdline.append_qu(buf, TRUE);
        }

        /* add the source zip if applicable */
        if (lc->getval_int("build", "src in webpage", FALSE))
        {
            val = lc->getval_strptr("build", "srczipfile", 0);
            sprintf(buf, "sourcefile=%s", val);
            cmdline.append_qu(buf, TRUE);
        }

        /* add the Windows SETUP package if applicable */
        if (lc->getval_int("build", "installer in webpage", FALSE))
        {
            val = lc->getval_strptr("build", "installer prog", 0);
            sprintf(buf, "setupfile=%s", val);
            cmdline.append_qu(buf, TRUE);
        }

        /* add the cover art if set */
        val = lc->getval_strptr("build special file", "cover art", 0);
        if (val != 0)
        {
            sprintf(buf, "coverart=%s", val);
            cmdline.append_qu(buf, TRUE);
        }

        /* add any web extras */
        cnt = lc->get_strlist_cnt("build", "web extras");
        for (i = 0 ; i < cnt ; ++i)
        {
            /* add the extra file */
            val = lc->getval_strptr("build", "web extras", i);
            sprintf(buf, "extra.file=%s", val);
            cmdline.append_qu(buf, TRUE);

            /* if there's a title, add it */
            val = lc->getval_strptr("build web title", val, 0);
            if (val != 0)
            {
                sprintf(buf, "extra.title=%s", val);
                cmdline.append_qu(buf, TRUE);
            }
            else
                cmdline.append_qu("extra.title=", TRUE);
        }

        /* add the command */
        add_build_cmd("Building web page",
                      "t3run", exe, cmdline.get(), proj_dir, FALSE);

        /*
         *   finally, if that was successful, we'll want to display the
         *   generated web page
         */

        val = lc->getval_strptr("build", "webpage dir", 0);
        os_build_full_path(buf, sizeof(buf), val, "index.htm");

        add_build_cmd("Displaying web page",
                      "open", "open", buf, proj_dir, FALSE);
    }


#ifdef NOFEATURE_PUBLISH
    /* -------------------------------------------------------------------- */
    /*
     *   If we're publishing, upload selected files to the IF Archive, and
     *   send the game listing data to IFDB.
     */
    if (command_id == ID_BUILD_PUBLISH)
    {
        char buf[MAX_PATH + 256 + 50];
        CStringBufCmd links(1024);
        CStringBufCmd notes(1024);
        CStringBuf xmlbuf;
        const textchar_t *fn, *rn;

        /*
         *   Add the command to generate the iFiction record.  This reads the
         *   GameInfo record from the compiled game file, synthesizes an
         *   iFiction record, and saves the iFiction record in an in-memory
         *   temp file.
         *
         *   This has to go after the main release build, since it reads from
         *   the game file; and it has to go before the "ifdbput" and "upload
         *   <notes>" commands, since those commands read from the generated
         *   ifiction temp file.
         */
        add_build_cmd("Generating the game's bibliographic information",
                      "ifiction", "ifiction", gamefile, proj_dir, FALSE);

        /*
         *   Upload the iFiction record to IFDB.  Do this before uploading
         *   the games to the Archive, since (a) there are more things that
         *   can go wrong in this step (bad credentials, data validation
         *   errors) than in the Archive uploads, and (b) this step is more
         *   repeatable if we have to redo the whole procedure.  The Archive
         *   uploads tend to be problematic if we have to repeat them,
         *   because /incoming is basically write-only, and it doesn't let us
         *   overwrite files we previously uploaded.  So save the Archive
         *   uploads for last, after we know everything else has succeeded.
         */
        CStringBufCmd putCmd(1024);
        putCmd.appendf("\"%s\" \"%s\" \"%s\"",
                       gamefile, t.ifdb_user, t.ifdb_psw);
        add_build_cmd("Creating/updating game listing page on IFDB",
                      "ifdbput", "ifdbput", putCmd.get(), proj_dir, FALSE);

        /* start the upload notes */
        notes.appendf(
            "Dear IF Archive Administrators,\n\n"

            "TADS Workbench for Windows uploaded the following files "
            "on behalf of\n"
            "%s, via the \"Publish Game\" command.  These files\n"
            "all relate to the same game, whose iFiction record is "
            "attached below.\n\n"

            "The if-archive/ paths and filenames listed are Workbench's "
            "assumptions for\n"
            "where these files will end up.  Workbench has created tentative "
            "download\n"
            "links to these paths on IFDB.  If you have to rename the files "
            "or decide\n"
            "they belong in other directories, please notify %s\n"
            "so that he/she can fix the IFDB links manually.\n\n"

            "Yours in Robotic Uploading,\n"
            "TADS Workbench\n\n"

            "P.S. If this Workbench feature is causing any technical "
            "problems for the\n"
            "Archive, please let me know (c/o Mike Roberts "
            "<mjr_@hotmail.com>).\n\n"

            "*** List of files included in this upload batch ***\n\n",

            t.email, t.email);

        /* start the links file */
        int link_cnt = 0;
        links.appendf(
            "<?xml version=\"1.0\"?>"
            "<downloads xmlns=\"http://ifdb.tads.org/api/xmlns\">"
            "<links>");

        /* upload the release ZIP if desired */
        if (t.build_zip)
        {
            /* count it */
            ++link_cnt;

            /* get the filename and root filename */
            fn = lc->getval_strptr("build", "zipfile", 0);
            rn = os_get_root_name((char *)fn);

            /* add the upload command */
            sprintf(buf, "\"%s\" \"%s\" ifarchive \"%.256s\" binary",
                    fn, rn, t.email);
            add_build_cmd("Uploading Release ZIP file to IF Archive",
                          "upload", "upload", buf, proj_dir, FALSE);

            /* add it to the link list for IFDB */
            links.appendf(
                "<link>"
                "<url>http://www.ifarchive.org/if-archive/games/tads/%s</url>"
                "<title>Story File</title>"
                "<isGame/>"
                "<pending/>"
                "<format>tads3</format>"
                "<compression>zip</compression>"
                "<compressedPrimary>%s</compressedPrimary>"
                "</link>",
                xmlbuf.xmlify(rn), gamefile);

            /* add it to the notes list */
            notes.appendf("  %s - TADS 3 story file "
                          "(might also contain feelies, etc)\n"
                          "       /if-archive/games/tads/%s\n\n",
                          rn, rn);
        }

        /* upload the SETUP if desired */
        if (t.build_setup)
        {
            /* count it */
            ++link_cnt;

            /* get the filename and root filename */
            fn = lc->getval_strptr("build", "installer prog", 0);
            rn = os_get_root_name((char *)fn);

            /* add the upload command */
            sprintf(buf, "\"%s\" \"%s\" ifarchive \"%.256s\" binary",
                    fn, rn, t.email);
            add_build_cmd("Uplaoding Windows SETUP program to IF Archive",
                          "upload", "upload", buf, proj_dir, FALSE);

            /* add it to the link list for IFDB */
            links.appendf(
                "<link>"
                "<url>http://www.ifarchive.org/if-archive/games/pc/%s</url>"
                "<title>Windows Setup</title>"
                "<desc>Installs the game on your Windows system. No other "
                "software is required to play.</desc>"
                "<isGame/>"
                "<pending/>"
                "<format>executable</format>"
                "<os>Windows.95</os>"
                "</link>",
                xmlbuf.xmlify(rn));

            /* add it to the notes list */
            notes.appendf("  %s - Windows installer (SETUP)\n"
                          "       /if-archive/games/pc/%s\n\n",
                          rn, rn);
        }

        /* upload the source ZIP if desired */
        if (t.build_srczip)
        {
            /* count it */
            ++link_cnt;

            /* get the filename and root filename */
            fn = lc->getval_strptr("build", "srczipfile", 0);
            rn = os_get_root_name((char *)fn);

            /* add the upload command */
            sprintf(buf, "\"%s\" \"%s\" ifarchive \"%.256s\" binary",
                    fn, rn, t.email);
            add_build_cmd("Uploading source code ZIP file to IF Archive",
                          "upload", "upload", buf, proj_dir, FALSE);

            /* add it to the link list for IFDB */
            links.appendf(
                "<link>"
                "<url>http://www.ifarchive.org/if-archive/"
                "games/source/tads/%s</url>"
                "<title>Source Code</title>"
                "<desc>The TADS source code for the game.</desc>"
                "<pending/>"
                "<format>text</format>"
                "<compression>zip</compression>"
                "</link>",
                xmlbuf.xmlify(rn));

            /* add it to the notes list */
            notes.appendf("  %s - source code for the game\n"
                          "       /if-archive/games/source/tads/%s\n\n",
                          rn, rn);
        }

        /* finish the links file */
        links.appendf("</links></downloads>");

        /* add the footer to the notes */
        notes.appendf("*** iFiction record for this game ***\n\n");

        /* if we have any links, save them as an in-memory temp file */
        if (link_cnt != 0)
        {
            char rootname[MAX_PATH];

            /* save the links */
            add_build_memfile("ifdbput/links", links.get());

            /* save the notes */
            add_build_memfile("upload/ifarchive/notes", notes.get());

            /* get the root name - the Archive file will be <root>-notes.txt */
            strcpy(rootname, os_get_root_name(gamefile));
            os_remext(rootname);

            /* add a command to upload the notes */
            sprintf(buf, ":upload/ifarchive/notes \"%s-notes.txt\" "
                    "ifarchive \"%.256s\" text", rootname, t.email);
            add_build_cmd("Sending file upload explanation to IF Archive",
                          "upload", "upload", buf, proj_dir, FALSE);
        }
    }
#endif /* NOFEATURE_PUBLISH */


    /* -------------------------------------------------------------------- */
    /*
     *   We've built the list of commands, so we can now start the build
     *   thread to carry out the command list
     */

    /* if we have a temporary directory, add a command to delete it */
    if (tmpdir[0] != '\0')
        add_build_cmd("Cleaning up temporary files",
                      "rmdir/s", "rmdir/s", tmpdir, "", TRUE);

    /* note that we have a compile running */
    build_running_ = TRUE;

    /* remember the build mode */
    build_cmd_ = command_id;

    /*
     *   run all build output through our special processing, but assume for
     *   now that we won't capture all of the output
     */
    filter_build_output_ = TRUE;
    capture_build_output_ = FALSE;

    /* if we're scanning for include files, set up capturing */
    if (command_id == ID_PROJ_SCAN_INCLUDES
        || command_id == ID_BUILD_COMPDBG_AND_SCAN)
    {
        /* capture the output from the build */
        capture_build_output_ = TRUE;
    }

    /* if we're meant to run after the build, so note */
    switch (command_id)
    {
    case ID_BUILD_COMP_AND_RUN:
    case ID_DEBUG_GO:
    case ID_DEBUG_STEPINTO:
    case ID_DEBUG_STEPOVER:
    case ID_DEBUG_STEPOUT:
    case ID_DEBUG_RUNTOCURSOR:
    case ID_DEBUG_REPLAY_SESSION:
        /*
         *   Implicit build on go - set up to re-execute the same command
         *   after the build.  (If the command is 'compile and run', the
         *   command to run after the compilation is complete is simply
         *   'run'.)
         */
        run_after_build_ = TRUE;
        run_after_build_cmd_ = (command_id == ID_BUILD_COMP_AND_RUN
                                ? ID_DEBUG_GO : command_id);

        /* from now on, treat this as a regular debug build */
        command_id = ID_BUILD_COMPDBG;
        break;

    default:
        /* we don't need to run after this build */
        run_after_build_ = FALSE;
        break;
    }

    /* if we're scanning for include files, clear out the existing files */
    if (command_id == ID_PROJ_SCAN_INCLUDES && projwin_ != 0)
        projwin_->clear_includes();

    /* start the compiler thread */
    build_thread_ = CreateThread(
        0, 0, (LPTHREAD_START_ROUTINE)&run_compiler_entry,
        this, 0, &thread_id);

    /* if we couldn't start the thread, show an error */
    if (build_thread_ != 0)
    {
        /* we successfully started the build */
        ok = TRUE;
    }
    else
    {
        /* note the problem */
        oshtml_dbg_printf("Build failure: unable to start build thread\n");

        /* we don't have a build running after all */
        build_running_ = FALSE;
    }

    /*
     *   if we started the build successfully, and we're doing a #include
     *   scan, show a modal dialog until the scan completes
     */
    if (build_running_
        && (command_id == ID_PROJ_SCAN_INCLUDES
            || command_id == ID_BUILD_COMPDBG_AND_SCAN))
    {
        /* create the dialog */
        build_wait_dlg_ = new CTadsDlg_scan_includes();

        /* display the dialog */
        build_wait_dlg_->run_modal(DLG_SCAN_INCLUDES, handle_,
                                   CTadsApp::get_app()->get_instance());

        /* done with the dialog */
        delete build_wait_dlg_;
        build_wait_dlg_ = 0;

        /*
         *   If we were doing a build-plus-scan operation, start the build
         *   now that we're done with the include scan.  We do the include
         *   scan first because it's the modal operation; once we kick off
         *   the build, the user interface allows most operations while the
         *   build proceeds in the background.
         */
        if (command_id == ID_BUILD_COMPDBG_AND_SCAN)
            PostMessage(handle_, WM_COMMAND, ID_BUILD_COMPDBG, 0);
    }

cleanup:

    /* return the successful start indication */
    return ok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to determine if it's okay to compile
 */
int CHtmlSys_dbgmain::vsn_ok_to_compile()
{
    /* make sure the project window has saved its configuration */
    if (projwin_ != 0)
        projwin_->save_config(local_config_);

    /* ensure we have at least one source file listed */
    if (local_config_->get_strlist_cnt("build", "source_files") == 0)
    {
        /*
         *   make sure the project window is visible before showing the
         *   error message, since the message refers to the project
         */
        SendMessage(get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /* tell them what's wrong */
        MessageBox(handle_, "This project has no source files.  Before "
                   "you can compile, you must add at least one "
                   "file to the project's \"Source Files\" group.",
                   "TADS Workbench", MB_OK | MB_ICONEXCLAMATION);

        /*
         *   make sure the project window is the active window now that
         *   we've removed the error message dialog
         */
        SendMessage(get_handle(), WM_COMMAND, ID_VIEW_PROJECT, 0);

        /*
         *   don't proceed with compilation right now - wait for the user
         *   to retry explicitly
         */
        return FALSE;
    }

    /* we can proceed with the compilation */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the name of an external resource file, and check to see if it
 *   should be included in the build
 */
int CHtmlSys_dbgmain::build_get_extres_file(char *buf, int cur_extres,
                                            int build_command,
                                            const char *extres_base_name)
{
    char ext[32];

    /*
     *   if we're building an installer, check to see if this file is
     *   included in the installer, and skip it if not
     */
    if (build_command == ID_BUILD_COMPINST)
    {
        char in_inst[10];

        /* get the in-install status - make it yes by default */
        if (local_config_->getval("build", "ext_resfile_in_install",
                                  cur_extres, in_inst, sizeof(in_inst)))
            in_inst[0] = 'Y';

        /* if this one isn't in the install, tell the caller to skip it */
        if (in_inst[0] != 'Y')
            return FALSE;
    }

    /* build the name of this external resource file */
    sprintf(ext, "3r%d", cur_extres);
    strcpy(buf, extres_base_name);
    os_remext(buf);
    os_addext(buf, ext);

    /* include the file */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a system library file to the build
 */
void CHtmlSys_dbgmain::add_file_to_build(
    int idx, const char *path, const char *domain)
{
    /* add the file to the source list */
    local_config_->setval("build", "source_files", idx, path);

    /* mark it as a library or source file, according to the suffix */
    int is_lib = (strlen(path) > 3
                  && strcmp(path + strlen(path) - 3, ".tl") == 0);
    local_config_->setval("build", "source_file_types", idx,
                          is_lib ? "library" : "source");

    /* mark it as a system file */
    local_config_->setval("build", "source_files-sys", idx, domain);

    /* add the line source for the new file (using the root name only) */
    const char *root = os_get_root_name(path);
    helper_->add_internal_line_source(root, root, idx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create and load a new game
 */
void CHtmlSys_dbgmain::create_new_game(
    const textchar_t *path, const char *name,
    const char *tpldir, const char *tplrel, const CNameValTab *tpl,
    const CNameValTab *biblio)
{
    textchar_t new_config[OSFNMAX];
    char msg[512];
    char fbuf[OSFNMAX];
    char proj_path[OSFNMAX];
    int file_idx;
    int defs_idx;
    char obj_path[OSFNMAX];
    char dbg_path[OSFNMAX];
    char web_path[OSFNMAX];
    char script_path[OSFNMAX];

    /* name the compiled game file */
    char gamefile[OSFNMAX];
    os_build_full_path(gamefile, sizeof(gamefile), path, name);
    os_addext(gamefile, "t3");

    /* first, save the current configuration */
    if (local_config_filename_.get() != 0)
    {
        /* save the current configuration into the configuration object */
        save_config();

        /* write the configuration object to its file */
        vsn_save_config_file(local_config_, local_config_filename_.get());
    }

    /*
     *   Build the name of the new configuration file.  Use a standard T3
     *   makefile (.t3m) to store the configuration, so that the file can be
     *   shared with the command-line compiler.  The full .t3m filename is
     *   the project path plus the project title plus the ".t3m" suffix.
     */
    os_build_full_path(new_config, sizeof(new_config), path, name);
    os_addext(new_config, "t3m");

    /* note the path name to the project file */
    os_get_path_name(proj_path, sizeof(proj_path), new_config);

    /*
     *   before we do anything we might regret, make sure we can save the
     *   configuration file
     */
    if (vsn_save_config_file(local_config_, new_config))
    {
        /* tell them what went wrong */
        sprintf(msg, "An error occurred creating the new game's "
                "configuration file (%s).  Please make sure the folder "
                "is correct and the disk isn't full.", new_config);
        MessageBox(handle_, msg, "TADS Workbench",
                   MB_ICONEXCLAMATION | MB_OK);

        /* stop now with no change to the loaded configuration */
        return;
    }

    /*
     *   Forget the old configuration file, since we're about to start
     *   modifying the configuration object for the new configuration.
     *   We've already saved the old file, so we don't need to know about
     *   it any more, and we want to be sure we don't try to save the new
     *   configuration to the old file.  For now, set an empty
     *   configuration; this will ensure that we don't save the
     *   configuration at any point (since it's empty), and that we load a
     *   new configuration (since the new config filename will be
     *   different when we load the game than the current config
     *   filename).
     */
    local_config_filename_.set(0, 0);

    /* no files in our list yet */
    file_idx = 0;

    /* no -D defines in our list yet */
    defs_idx = 0;

    /*
     *   Set up the new configuration, based on the existing
     *   configuration.  First, clear out all of the information that
     *   isn't suitable for a default configuration, so that we carry
     *   forward only the information from this configuration that we'd
     *   apply to the default config.
     */
    clear_non_default_config();

    /* set up the default build configuration */
    set_dbg_build_defaults(this, gamefile);

    /* clear out our internal source file list */
    helper_->delete_internal_sources();

    /*
     *   create the debug output directory - create it as a Debug
     *   subdirectory of the project directory
     */
    os_build_full_path(dbg_path, sizeof(dbg_path), proj_path, "debug");
    if (CreateDirectory(dbg_path, 0))
    {
        /* put the image file in this directory */
        os_build_full_path(fbuf, sizeof(fbuf), "debug",
                           os_get_root_name((textchar_t *)gamefile));
    }
    else
    {
        /* failed - use the release game name plus "_dbg" */
        strcpy(fbuf, os_get_root_name((textchar_t *)gamefile));

        /* remove the extension, add "_dbg", and put the extension back */
        os_remext(fbuf);
        strcat(fbuf, "_dbg");
        os_defext(fbuf, "t3");
    }

    /* set the debug image file according to what we just decided */
    local_config_->setval("build", "debug_image_file", 0, fbuf);

    /*
     *   save the base game filename as the release path; just use the base
     *   filename, since we want to put it by default in the project
     *   directory
     */
    local_config_->setval("build", "rlsgam", 0,
                          os_get_root_name((textchar_t *)gamefile));

    /*
     *   create the object file directory - create it as a .\OBJ subdirectory
     *   of the project directory
     */
    os_build_full_path(obj_path, sizeof(obj_path), proj_path, "obj");
    CreateDirectory(obj_path, 0);

    /* if the .\OBJ directory doesn't exist, use the base path */
    if (osfacc(obj_path))
        strcpy(obj_path, proj_path);

    /* set the symbol and object file output directories */
    local_config_->setval("build", "symfile path", 0, obj_path);
    local_config_->setval("build", "objfile path", 0, obj_path);

    /* create the web page directory - create it as .\WEB subdirectory */
    os_build_full_path(web_path, sizeof(web_path), proj_path, "web");
    CreateDirectory(web_path, 0);
    local_config_->setval("build", "webpage dir", 0, web_path);

    /* set up the Scripts directory */
    os_build_full_path(
        script_path, sizeof(script_path), proj_path, "Scripts");
    CreateDirectory(script_path, 0);
    local_config_->setval("auto-script", "dir", 0, "Scripts");

    /* add system.tl to all configurations */
    add_file_to_build(file_idx++, "system.tl", "system");

    /* add the system libraries */
    for (const CNameValPair *nv = tpl->find_key("sysfile") ; nv != 0 ;
         nv = tpl->find_next_key(nv))
        add_file_to_build(file_idx++, nv->val_.get(), "system");

    /* add the extension libraries */
    for (const CNameValPair *nv = tpl->find_key("lib") ; nv != 0 ;
         nv = tpl->find_next_key(nv))
    {
        os_build_full_path(fbuf, sizeof(fbuf), tplrel, nv->val_.get());
        add_file_to_build(file_idx++, fbuf, "user");
    }

    /* add the #define symbols */
    for (const CNameValPair *nv = tpl->find_key("define") ; nv != 0 ;
         nv = tpl->find_next_key(nv))
    {
        local_config_->setval("build", "defines", defs_idx++, nv->val_.get());
    }

    /* add the GameInfo.txt resource */
    local_config_->setval("build", "image_resources", 0, "GameInfo.txt");

    /* copy the starter source files */
    for (const CNameValPair *nv = tpl->find_key("source") ; nv != 0 ;
         nv = tpl->find_next_key(nv))
    {
        /*
         *   If the source name contains a space, it's of the form <source
         *   file> <target file>, where the source file is the original in
         *   the library folder, and the target file is the name of the file
         *   to create in the new project folder.  Any '$' in the target
         *   filename is replaced with the new project name.
         */
        const char *n = nv->val_.get();
        const char *sp = strchr(n, ' ');
        char src[OSFNMAX], targ[OSFNMAX];
        if (sp != 0)
        {
            /* there's a space - pull out the two parts */
            safe_strcpy(src, sizeof(src), n, sp - n);
            safe_strcpy(targ, sizeof(targ), sp + 1);

            /* replace '$' in the target with the project name */
            const char *dol = strchr(targ, '$');
            if (dol != 0)
            {
                char targ1[OSFNMAX];
                strcpy(targ1, targ);
                dol = targ1 + (dol - targ);
                _snprintf(targ, sizeof(targ), "%.*s%s%s",
                          (int)(dol - targ1), targ1, name, dol + 1);
            }
        }
        else
        {
            /* there's only one name, so it's the same for source and target */
            safe_strcpy(src, sizeof(src), n);
            safe_strcpy(targ, sizeof(targ), n);
        }

        /* build the path to the source template file */
        os_build_full_path(fbuf, sizeof(fbuf), tpldir, src);

        /* ...and to the project copy */
        char targpath[OSFNMAX];
        os_build_full_path(targpath, sizeof(targpath), path, targ);

        /* copy the file */
        if (!copy_insert_fields(fbuf, targpath, biblio))
        {
            sprintf(msg, "An error occurred creating a source file (%s) "
                    "for the new project.  Please make sure the folder "
                    "is correct and the disk has enough free space.",
                    targpath);
            MessageBox(handle_, msg, "TADS Workbench",
                       MB_ICONEXCLAMATION | MB_OK);

            /* abort */
            return;
        }

        /* add the source file to the project */
        add_file_to_build(file_idx++, targ, "user");
    }

    /* ask the helper to save the new source list into our config object */
    helper_->save_internal_source_config(local_config_);

    /* set some additional options */
    local_config_->setval("build", "verbose errors", TRUE);

    /* save the new configuration */
    vsn_save_config_file(local_config_, new_config);

    /*
     *   Set up the event loop flags so that we terminate the current
     *   event loop and load the new game
     */
    reloading_new_ = TRUE;
    restarting_ = TRUE;
    reloading_ = TRUE;
    reloading_go_ = FALSE;
    reloading_exec_ = FALSE;

    /* tell the main loop to give up control so that we load the new game */
    go_ = TRUE;

    /*
     *   Set the new game to load.  Since we're only loading the project, not
     *   actually running the game yet, use the project filename as the
     *   pending file to load.
     */
    if (CHtmlSys_mainwin::get_main_win() != 0)
        CHtmlSys_mainwin::get_main_win()->set_pending_new_game(new_config);

    /*
     *   Post a Compile-and-Scan-Includes command, so that we compile the
     *   game as soon as we finish loading it, and then populate the include
     *   file list with the files actually #include'd after we're done with
     *   the compilation.  (The game should be fully loaded by the time we
     *   get around to processing events again.)
     */
    set_pending_build_cmd(ID_BUILD_COMPDBG_AND_SCAN);

    /*
     *   add an Open File command to the pending command list, so that we
     *   open the first source file, which is the main game source file.
     */
    add_pending_build_cmd(ID_FILE_OPENLINESRC_FIRST);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear non-default configuration data
 */
void CHtmlSys_dbgmain::vsn_clear_non_default_config()
{
    /* clear custom build configuration variables */
    local_config_->clear_strlist("makefile", "image_file");
    local_config_->clear_strlist("build", "includes");
    local_config_->clear_strlist("build", "srcfile path");
    local_config_->clear_strlist("build", "objfile path");
    local_config_->clear_strlist("build", "symfile path");
    local_config_->clear_strlist("build", "asmfile");
    local_config_->clear_strlist("makefile", "foreign_config");
    local_config_->clear_strlist("build", "defines");
    local_config_->clear_strlist("build", "undefs");
    local_config_->clear_strlist("build", "extra_options");

    /* clear all of the source window configuration data */
    class CClearWin: public IHtmlDebugConfigEnum
    {
    public:
        CClearWin(CHtmlDebugConfig *c) { config = c; }
        CHtmlDebugConfig *config;
        void enum_var(const textchar_t *varname, const textchar_t *ele)
        {
            if (memcmp(varname, "src win", 7) == 0
                || strcmp(varname, "line wrap") == 0
                || memcmp(varname, "mdi win", 7) == 0
                || strcmp(varname, "mdi tab order") == 0
                || strcmp(varname, "breakpoints") == 0)
                config->delete_val(varname, ele);
        }
    } cbClearWin(local_config_);
    local_config_->enum_values(&cbClearWin);

    /* clear all project information */
    CHtmlDbgProjWin::clear_proj_config(local_config_);

    /* reset the project window */
    if (projwin_ != 0)
        projwin_->reset_proj_win();
}

/*
 *   Set version-specific build defaults.  In T3 workbench, this does
 *   nothing, since we can never load an empty configuration.
 */
void vsn_set_dbg_build_defaults(CHtmlDebugConfig *config,
                                CHtmlSys_dbgmain *dbgmain,
                                const textchar_t *gamefile)
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Program arguments dialog
 */

class CTadsDlg_args: public CTadsDialog
{
public:
    CTadsDlg_args(CHtmlSys_dbgmain *dbgmain)
    {
        /* remember the debugger object */
        dbgmain_ = dbgmain;
    }

    /* initialize */
    void init();

    /* execute a command */
    int do_command(WPARAM id, HWND ctl);

protected:
    /* save our configuration settings */
    void save_config();

    /* debugger main window */
    CHtmlSys_dbgmain *dbgmain_;

    /* file safety settings list */
    static const char *fs[];
};

const char *CTadsDlg_args::fs[] = {
    "Use interpreter settings",
    "00 - All access",
    "02 - All read/Local write",
    "04 - All read/no write",
    "22 - Local read/write",
    "24 - Local read/no write",
    "44 - No access"
};

/*
 *   initialize
 */
void CTadsDlg_args::init()
{
    CHtmlDebugConfig *config;
    char buf[1024];

    /* get the local configuration object */
    config = dbgmain_->get_local_config();

    /* retrieve our arguments setting and initialize the field */
    if (!config->getval("prog_args", "args", 0, buf, sizeof(buf)))
        SetWindowText(GetDlgItem(handle_, IDC_FLD_PROGARGS), buf);

    /* load the file safety options */
    HWND cbo = GetDlgItem(handle_, IDC_CBO_FILESAFETY);
    for (int i = 0 ; i < countof(fs) ; ++i)
        SendMessage(cbo, CB_ADDSTRING, i, (LPARAM)fs[i]);

    /* get the current setting */
    buf[0] = '\0';
    config->getval("prog_args", "file_safety", 0, buf, sizeof(buf));

    /* search the list; assume we'll use the default at index 0 */
    int idx = 0;
    for (int i = 0 ; i < countof(fs) ; ++i)
    {
        /* just check the first two characters (the numeric setting) */
        if (memcmp(buf, fs[i], 2) == 0)
        {
            idx = i;
            break;
        }
    }

    /* set the selection */
    SendMessage(cbo, CB_SETCURSEL, idx, 0);

    /* inherit default */
    CTadsDialog::init();
}

/*
 *   save our configuration settings
 */
void CTadsDlg_args::save_config()
{
    CHtmlDebugConfig *config;
    char buf[1024];

    /* get the local configuration object */
    config = dbgmain_->get_local_config();

    /* save the program arguments setting */
    GetWindowText(GetDlgItem(handle_, IDC_FLD_PROGARGS), buf, sizeof(buf));
    config->setval("prog_args", "args", 0, buf);

    /* get the current file safety setting */
    int idx = SendMessage(GetDlgItem(handle_, IDC_CBO_FILESAFETY),
                          CB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= countof(fs))
        idx = 0;

    /* save only the numeric prefix portion */
    strcpy(buf, fs[idx]);
    char *p;
    for (p = buf ; isdigit(*p) ; ++p) ;
    *p = '\0';

    /* save the value */
    config->setval("prog_args", "file_safety", 0, buf);
}

/*
 *   process a command
 */
int CTadsDlg_args::do_command(WPARAM id, HWND ctl)
{
    /* see what we have */
    switch(id)
    {
    case IDOK:
        /* save the settings */
        save_config();

        /* proceed to default processing */
        break;

    default:
        /* use default processing */
        break;
    }

    /* inherit default processing */
    return CTadsDialog::do_command(id, ctl);
}

/*
 *   run the program argument dialog
 */
void CHtmlSys_dbgmain::vsn_run_dbg_args_dlg()
{
    CTadsDlg_args *dlg;

    /* create the dialog */
    dlg = new CTadsDlg_args(this);

    /* run the dialog */
    dlg->run_modal(DLG_PROGARGS, get_handle(),
                   CTadsApp::get_app()->get_instance());

    /* delete the dialog */
    delete dlg;
}

/* ------------------------------------------------------------------------ */
/*
 *   Process captured build output.
 */
int CHtmlSys_dbgmain::vsn_process_build_output(CStringBuf *txt)
{
    /* check the build mode */
    switch (build_cmd_)
    {
    case ID_PROJ_SCAN_INCLUDES:
    case ID_BUILD_COMPDBG_AND_SCAN:
        /*
         *   We're scanning include files - if this is a #include line, send
         *   it to the project window for processing.  (Ignore anything that
         *   doesn't start with #include, since the include file lines all
         *   start with this sequence.)
         */
        if (strlen(txt->get()) > 9 && memcmp(txt->get(), "#include ", 9) == 0)
            projwin_->add_scanned_include(txt->get() + 9);

        /* suppress all output while scanning includes */
        return FALSE;

    default:
        /* check for status messages */
        if (strlen(txt->get()) > 3 && memcmp(txt->get(), "<@>", 3) == 0)
        {
            /* check for a %done update */
            if (memcmp(txt->get() + 3, "%PCT:", 5) == 0)
            {
                /* parse it */
                int fcur, ftot;
                sscanf(txt->get() + 8, "%d/%d", &fcur, &ftot);

                /* update the progress counter */
                if (WaitForSingleObject(stat_mutex_, 100) == WAIT_OBJECT_0)
                {
                    /* update the variables */
                    stat_step_len_ = ftot;
                    stat_step_progress_ = fcur;

                    /* update the build progress bar with the new settings */
                    update_build_progress_bar(TRUE);

                    /* done updating */
                    ReleaseMutex(stat_mutex_);
                }

                /* don't show this text in the log output */
                return FALSE;
            }

            /* build a status line message based on the status */
            compile_status_.set("Build in Progress: ");
            compile_status_.append(txt->get() + 3);

            /* update the status line */
            if (statusline_ != 0)
                statusline_->update();

            /* save the update in the dialog status area */
            if (WaitForSingleObject(stat_mutex_, 100) == WAIT_OBJECT_0)
            {
                /* update the sub-message */
                stat_subdesc_.set(txt->get() + 3);

                /* done updating */
                ReleaseMutex(stat_mutex_);
            }

            /*
             *   if we have a statusline, suppress the output from the main
             *   buffer; otherwise, show it
             */
            if (show_statusline_)
            {
                /* we have a statusline - no need to show the output */
                return FALSE;
            }
            else
            {
                CStringBuf newtxt;

                /*
                 *   there's no statusline - show the output in the log
                 *   window, with the "<@>" turned into a tab
                 */
                newtxt.set("\t");
                newtxt.append(txt->get() + 3);
                txt->set(newtxt.get());
                return TRUE;
            }
        }

        /* show all other text */
        return TRUE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   High-precision timer, for the profiler
 */

/* get the current high-precision timer time */
void os_prof_curtime(vm_prof_time *t)
{
    LARGE_INTEGER i;

    /* get the current time on the Windows high-precision system clock */
    QueryPerformanceCounter(&i);

    /* set the return value */
    t->hi = i.HighPart;
    t->lo = i.LowPart;
}

/* convert a high-precision timer value to microseconds */
unsigned long os_prof_time_to_ms(const vm_prof_time *t)
{
    LARGE_INTEGER freq;

    /* get the system high-precision timer frequency, in ticks per second */
    if (QueryPerformanceFrequency(&freq))
    {
        LARGE_INTEGER tl;

        /*
         *   We have the frequency in ticks per second.  We want the time in
         *   microseconds.  To start with, we note that since freq is the
         *   number of ticks per second, freq/1e6 is the number of ticks per
         *   microsecond, and that t is expressed in ticks; so, t divided by
         *   (freq/1e6) would give us ticks divided by ticks-per-microsecond,
         *   which gives us the number of microseconds.  This calculation is
         *   equivalent to t*1e6/freq - do the calculation in this order to
         *   retain the full precision.  (Note that even though t times a
         *   million could be quite large, it's not likely to overflow our
         *   64-bit type.)
         */

        /* get the input value as a long-long for easy arithmetic */
        tl.LowPart = t->lo;
        tl.HighPart = t->hi;

        /* compute the quotient and return the result */
        return (unsigned long)(tl.QuadPart * (ULONGLONG)1000000
                               / freq.QuadPart);
    }
    else
    {
        /* no high-precision clock is available; return zero */
        return 0;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Validate a project file
 */
int CHtmlSys_dbgmain::vsn_validate_project_file(const textchar_t *fname)
{
    const textchar_t *ext;

    /* scan for the filename extension - it starts at the last '.' */
    ext = strrchr(fname, '.');

    /* if the extension is ".t" or ".t3", reject it */
    if (ext != 0 && (stricmp(ext, ".t") == 0 || stricmp(ext, ".t3") == 0))
    {
        /* can't load source or game - must load project */
        MessageBox(handle_, "To open a new project, you must open the "
                   "project's makefile, which usually has a name ending "
                   "in \".t3m\".  If you don't have a makefile, use "
                   "the \"New Project\" command to create one, then "
                   "add your source files to the project.",
                   "TADS Workbench", MB_OK);

        /* fully handled */
        return TRUE;
    }

    /* we didn't handle it; let the main loader proceed */
    return FALSE;
}

/*
 *   Determine if a project file is valid for loading
 */
int CHtmlSys_dbgmain::vsn_is_valid_proj_file(const textchar_t *fname)
{
    const textchar_t *ext;

    /* in tads 3 workbench, we can't load .t or .t3 files */
    ext = strrchr(fname, '.');
    return !(ext != 0
             && (stricmp(ext, ".t") == 0 || stricmp(ext, ".t3") == 0));
}

/*
 *   Get the "load profile" for the given Load Game filename.  This fills in
 *   *game_filename with a pointer to the proper buffer for the name of the
 *   game file to load, and fills in config_filename with the name of the
 *   configuration file to load.  Returns true to proceed, false to abort the
 *   load.  All buffers are OSFNMAX bytes in size.
 */
int CHtmlSys_dbgmain::vsn_get_load_profile(char **game_filename,
                                           char *game_filename_buf,
                                           char *config_filename,
                                           const char *load_filename,
                                           int game_loaded)
{
    /*
     *   in t3 workbench, we always load a project before we can start
     *   running the game; so if 'game_loaded' is true, it means we're
     *   running a game from a configuration we've already loaded, so there's
     *   no need to load the configuration again
     */
    if (game_loaded)
        return FALSE;

    /*
     *   If it's not a .t3m file, don't load it - in t3 workbench, we only
     *   load configurations as t3 files.  Due to quirks in the old tads2
     *   workbench initialization and re-initialization event handling, we
     *   can sometimes try to load the current *game* before and after
     *   running it.  Simply ignore such cases, since we can safely assume
     *   that we're just transitioning in and out of an active debug session,
     *   but the loaded configuration won't be changing.
     */
    if (load_filename != 0)
    {
        const char *ext = strrchr(load_filename, '.');
        if (ext == 0 || stricmp(ext, ".t3m") != 0)
            return FALSE;
    }

    /*
     *   in t3 workbench, we won't know the game filename until after loading
     *   the configuration, since it's stored there
     */
    *game_filename = 0;

    /*
     *   if we already have the same project file loaded, there's no need to
     *   load the same game again - just tell the caller to do nothing
     */
    if (local_config_ != 0
        && local_config_filename_.get() != 0
        && load_filename != 0
        && get_strcmp(local_config_filename_.get(), load_filename) == 0)
        return FALSE;

    /*
     *   if the game hasn't been loaded yet, set the open-file dialog's
     *   default directory to the project directory, so that we start in the
     *   project directory when looking for files to open
     */
    if (!game_loaded && load_filename != 0)
        CTadsApp::get_app()->set_openfile_dir(load_filename);

    /* the project file *is* the config file */
    if (load_filename != 0)
        strcpy(config_filename, load_filename);

    /* tell the caller to proceed with the loading */
    return TRUE;
}

/*
 *   Get the game filename from the load profile.  In T3 workbench, we obtain
 *   the game filename from the project configuration file.
 */
int CHtmlSys_dbgmain::vsn_get_load_profile_gamefile(char *buf)
{
    /* try the configuration string "build:debug_image_file" first */
    if (!local_config_->getval("build", "debug_image_file",
                               0, buf, OSFNMAX))
        return TRUE;

    /*
     *   didn't find it; use the project name, with "_dbg" added to the root
     *   filename and the extension changed to ".t3"
     */
    strcpy(buf, local_config_filename_.get());
    os_remext(buf);
    strcat(buf, "_dbg");
    os_defext(buf, "t3");

    /* save the name */
    local_config_->setval("build", "debug_image_file", 0, buf);

    /* tell the caller we found a name */
    return TRUE;
}

/*
 *   Set the main window title.  Set the title to the loaded project file
 *   name.
 */
void CHtmlSys_dbgmain::vsn_set_win_title()
{
    char title[OSFNMAX + 100];

    /* build the text */
    if (workspace_open_)
        sprintf(title, "%s - TADS Workbench",
                os_get_root_name((char *)local_config_filename_.get()));
    else
        strcpy(title, "TADS Workbench");

    /* set the label */
    SetWindowText(handle_, title);
}

/*
 *   Update the game file name to reflect a change in the preferences.
 */
void CHtmlSys_dbgmain::vsn_change_pref_gamefile_name(textchar_t *fname)
{
    char pathbuf[OSFNMAX];
    char relbuf[OSFNMAX];

    /* store the new game file name in the UI */
    game_filename_.set(fname);

    /* in the preferences, store this path relative to the project path */
    os_get_path_name(pathbuf, sizeof(pathbuf), get_local_config_filename());
    os_get_rel_path(relbuf, sizeof(relbuf), pathbuf, fname);

    /* save the change in the configuration data as well */
    local_config_->setval("build", "debug_image_file", 0, relbuf);
}

/*
 *   Adjust an argv argument (a program startup argument) to the proper
 *   filename extension for initial loading.
 *
 *   For the tads 3 workbench, we always load from project (.t3m) files.
 *   However, the user can also load from Windows Explorer by right-clicking
 *   on a .t3 file and selecting "debug" from the context menu, or even by
 *   typing in a command line directly via the Start/Run dialog or a DOS
 *   prompt.  So, if we have a .t3 file as the argument, change it to use a
 *.  t3m extension, and hope for the best.
 */
void CHtmlSys_dbgmain::vsn_adjust_argv_ext(textchar_t *fname)
{
    size_t len;
    size_t ext_len;

    /* get the length of the name and of the game file suffix */
    len = strlen(fname);
    ext_len = strlen(w32_game_ext);

    /* check to see if we have the config file suffix */
    if (len > ext_len + 1
        && fname[len - ext_len - 1] == '.'
        && memicmp(fname + len - ext_len, w32_game_ext, ext_len) == 0)
    {
        /* replace the extension with the standard project file suffix */
        os_remext(fname);
        os_addext(fname, w32_tdb_config_ext);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add the documentation toolbar buttons
 */
int CHtmlSys_dbgmain::vsn_add_doc_buttons(TBBUTTON *buttons, int i)
{
    /* 'user manuals' button */
    buttons[i].iBitmap = 32;
    buttons[i].idCommand = ID_HELP_TADSMAN;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* 'tutorial' button */
    buttons[i].iBitmap = 56;
    buttons[i].idCommand = ID_HELP_TUTORIAL;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* 'library reference' button */
    buttons[i].iBitmap = 33;
    buttons[i].idCommand = ID_HELP_T3LIB;
    buttons[i].fsState = TBSTATE_ENABLED;
    buttons[i].fsStyle = TBSTYLE_BUTTON;
    buttons[i].dwData = 0;
    buttons[i].iString = 0;
    ++i;

    /* return the next available index */
    return i;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the next project file for a search
 */
int CHtmlSys_dbgmain::get_next_search_file(
    char *fname, size_t fname_size,
    const char *curfile, int direction, int wrap, int local_only)
{
    /* let the project window figure this out */
    return projwin_ != 0
        ? projwin_->get_next_search_file(
            fname, fname_size, curfile, direction, wrap, local_only)
        : FALSE;
}

