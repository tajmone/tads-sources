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
  htmldb3.cpp - debugger helper class - T3 interface
Function
  This module contains the helper class interfaces to the T3 VM debugger
  internal engine API.

  The code in this module is separated from the main debugger helper class
  so that the main helper class is independent of the underlying VM engine
  and its debugger API.  This allows the main debugger helper to be shared
  by different engines; in particular, this allows the main debugger helper
  class to be used for TADS 2 and T3 debuggers.  Because the UI code uses
  the helper class to access the VM engine (the UI code never calls the
  engine directly - it always uses the helper class instead), the UI code
  is also independent of the underlying engine.
Notes

Modified
  11/24/99 MJRoberts  - Creation
*/

#include <string.h>

/* include the necessary T3 interfaces */
#include "t3std.h"
#include "os.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmdbg.h"
#include "vmrun.h"
#include "vmconsol.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmsrcf.h"
#include "vmerr.h"
#include "tcerr.h"


/* include some HTML TADS headers */
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLDBG_H
#include "htmldbg.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTMLPRS_H
#include "htmlprs.h"
#endif
#ifndef HTMLTAGS_H
#include "htmltags.h"
#endif
#ifndef HTMLDISP_H
#include "htmldisp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   dbgcxdef structure.  We define this structure for interface
 *   compatibility with debugger UI's designed for TADS 2.  This is NOT
 *   the same structure as used in the TADS 2 debugger engine, so the
 *   underlying UI code must not attempt to access the interior of this
 *   structure.
 */
struct dbgcxdef
{
    /* pointer to VM global variables */
    vm_globals *vmg;

    /* underlying user interface context pointer */
    void *ui_ctx;
};


/* ------------------------------------------------------------------------ */
/*
 *   T3 API to the UI.  This is the implementation for the HTML TADS
 *   Debugger for Windows.
 */

/*
 *   Initialize
 */
void CVmDebugUI::init(VMG_ const char *image_filename)
{
    dbgcxdef *ctx;

    /* create our private context */
    ctx = new dbgcxdef;

    /*
     *   stash the global variable structure pointer in our context
     *   structure for later use
     */
    ctx->vmg = VMGLOB_ADDR;

    /*
     *   tell the UI to initialize, and save the UI context object it
     *   returns
     */
    ctx->ui_ctx = html_dbgui_ini(ctx, image_filename);

    /* save our context in the VM debugger object */
    G_debugger->set_ui_ctx(ctx);

    /* clear the HALT flag in the VM, since we're just starting */
    G_interpreter->set_halt_vm(FALSE);
}

/*
 *   Initialize, phase 2 - after image file loading
 */
void CVmDebugUI::init_after_load(VMG0_)
{
    dbgcxdef *ctx;

    /* get my private context */
    ctx = (dbgcxdef *)G_debugger->get_ui_ctx();

    /* tell the UI to finish initialization */
    html_dbgui_ini2(ctx, ctx->ui_ctx);
}

/*
 *   terminate
 */
void CVmDebugUI::terminate(VMG0_)
{
    dbgcxdef *ctx;

    /* get my private context */
    ctx = (dbgcxdef *)G_debugger->get_ui_ctx();

    /* if there's a valid context, notify the UI */
    if (ctx != 0)
    {
        /* notify the UI that we're shutting down */
        html_dbgui_term(ctx, ctx->ui_ctx);

        /* delete my context */
        delete ctx;
    }

    /* clear the debugger object's memory of our context */
    G_debugger->set_ui_ctx(0);
}

/*
 *   main command loop entrypoint
 */
void CVmDebugUI::cmd_loop(VMG_ int bpnum, int err,
                          const uchar **pc)
{
    dbgcxdef *ctx;

    /* get my private context */
    ctx = (dbgcxdef *)G_debugger->get_ui_ctx();

    /* invoke the command loop in the UI */
    html_dbgui_cmd(ctx, ctx->ui_ctx, bpnum, err, pc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Helper class extension.  This class encapsulates VM-specific private
 *   functions.  These functions are called by the vm_xxx code and are
 *   private to the vm_xxx code - the generic debugger helper class
 *   doesn't call any of these functions.
 *
 *   This class isn't ever instantiated; all of the methods are static.
 *   The only real reason we need this class at all is so that we can make
 *   it a friend of CHtmlDebugHelper, so that our private vm_xxx
 *   implementation functions can call back into the CHtmlDebugHelper
 *   private parts; this is a valid need because CHtmlDebugHelperVM is
 *   effectively an extension of CHtmlDebugHelper, but couldn't be part of
 *   CHtmlDebugHelper because of the need to keep the VM-specific
 *   functions out of the public interface.  (In particular, each VM
 *   implementation will have its own set of functions in
 *   CHtmlDebugHelperVM, so we can't come up with a set in
 *   CHtmlDebugHelper that will serve everyone.)
 */
class CHtmlDebugHelperVM
{
public:
    /* find the code address of the current source window's selection */
    static int srcofs_to_code(CHtmlDebugHelper *helper, dbgcxdef *ctx,
                              IDebugWin *win, ulong *linenum,
                              CHtmlDbg_win_link **win_link,
                              void *code_addr);

    /* find the code address of a given source file location */
    static int srcofs_to_code(dbgcxdef *ctx,
                              int source_id, ulong *linenum,
                              void *code_addr);

    /* toggle a breakpoint */
    static int toggle_breakpoint(CHtmlDebugHelper *helper, dbgcxdef *ctx,
                                 int source_id, unsigned long linenum,
                                 void *code_addr,
                                 const char *cond, int change,
                                 int set_only, int set_internal_bp,
                                 int *bpnum, int *did_set);

    /* toggle a breakpoint's disabled status */
    static void toggle_bp_disable(CHtmlDebugHelper *helper,
                                  dbgcxdef *ctx,
                                  int source_id, unsigned long linenum,
                                  void *code_addr);
};

/*
 *   Find the code address of the current source window's selection
 */
int CHtmlDebugHelperVM::
   srcofs_to_code(CHtmlDebugHelper *helper, dbgcxdef *ctx,
                  IDebugWin *win, ulong *linenum,
                  CHtmlDbg_win_link **win_link, void *code_addr)
{
    CHtmlDbg_line_link *line_link;

    /* find information the current line in the window */
    *linenum = helper->find_line_info(win, win_link, &line_link);

    /* if we didn't find the window, return failure */
    if (*win_link == 0)
        return 1;

    /* get information on this position */
    return srcofs_to_code(ctx, (*win_link)->source_id_, linenum, code_addr);
}

/*
 *   Find the code address of a given source file location
 */
int CHtmlDebugHelperVM::srcofs_to_code(dbgcxdef *ctx,
                                       int source_id, ulong *linenum,
                                       void *code_addr)
{
    CVmSrcfEntry *entry;

    /* if there's no context, we can't do anything */
    if (ctx == 0)
    {
        *(const uchar **)code_addr = 0;
        return 0;
    }

    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* get the source file entry for the given source ID */
    entry = G_srcf_table->get_entry(source_id);

    /* if we didn't get an entry, return failure */
    if (entry == 0)
        return 1;

    /* find the line record, and get the code pool offset */
    ulong ofs = entry->find_src_addr(linenum, FALSE);

    /* if we didn't find the line, return failure */
    if (ofs == 0)
        return 1;

    /* translate the offset to a code pool address */
    *(const uchar **)code_addr = (const uchar *)G_code_pool->get_ptr(ofs);

    /* success */
    return 0;
}

/*
 *   Set or clear a breakpoint at a given location.  Returns zero on
 *   success, non-zero on error.
 */
int CHtmlDebugHelperVM::toggle_breakpoint(CHtmlDebugHelper *helper,
                                          dbgcxdef *ctx,
                                          int source_id,
                                          unsigned long linenum,
                                          void *code_addr,
                                          const char *cond, int change,
                                          int set_only,
                                          int set_internal_bp,
                                          int *bpnum, int *did_set)
{
    /*
     *   If we can only set a new breakpoint, check to see if there's
     *   already a breakpoint at this location.  If there is, disallow it.
     */
    if (set_only && helper->find_internal_bp(source_id, linenum) != 0)
        return 2;

    /* set the breakpoint in the engine, if we're running */
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* set the breakpoint in the VM */
        if (G_debugger->toggle_breakpoint(vmg_ (const uchar *)code_addr,
                                          cond, change,
                                          bpnum, did_set, 0, 0))
            return 3;
    }
    else
    {
        /* the engine isn't running; synthesize a breakpoint ID */
        *bpnum = helper->synthesize_bp_num();
        *did_set = TRUE;
    }

    /*
     *   If the caller wants us to, toggle the internal breakpoint record
     *   so we show the breakpoint visually
     */
    if (set_internal_bp)
        helper->toggle_internal_bp(ctx, source_id, linenum, cond, change,
                                   *bpnum, *did_set, FALSE);

    /* success */
    return 0;
}

/*
 *   enable/disable a breakpoint at the given location
 */
void CHtmlDebugHelperVM::toggle_bp_disable(CHtmlDebugHelper *helper,
                                           dbgcxdef *ctx,
                                           int source_id,
                                           unsigned long linenum,
                                           void *code_addr)
{
    /*
     *   if the engine is running, ask it if there's a breakpoint there -
     *   if not, there's nothing to do
     */
    if (ctx != 0)
    {
        CHtmlDbg_bp *bp;

        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* find the breakpoint */
        bp = helper->find_internal_bp(source_id, linenum);

        /* if we found it, toggle the VM breakpoint status */
        if (bp != 0)
            G_debugger->toggle_breakpoint_disable(vmg_ bp->bpnum_);
    }

    /* toggle the status of the internal breakpoint record */
    helper->toggle_internal_bp_disable(ctx, source_id, linenum);
}


/* ------------------------------------------------------------------------ */
/*
 *   Flush output to the main game text window
 */
void CHtmlDebugHelper::vm_flush_main_output(dbgcxdef *ctx)
{
    /* ignore this if we get called without a context */
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* flush output on the main console */
        G_console->flush(vmg_ VM_NL_NONE);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Perform engine-specific checks on a newly-loaded a game
 */
void CHtmlDebugHelper::vm_check_loaded_program(dbgcxdef *ctx)
{
    /* ignore this if we get called without a context */
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* make sure the program was compiled for debugging */
        if (!G_debugger->image_has_debug_info(vmg0_))
            err_throw(VMERR_NO_IMAGE_DBG_INFO);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get information on the current source line at the given stack level.
 *   Returns zero on success, non-zero on failure.
 */
int CHtmlDebugHelper::
   vm_get_source_info_at_level(dbgcxdef *ctx, CHtmlDbg_src **src,
                               unsigned long *linenum, int level,
                               CHtmlDebugSysIfc_win *syswinifc)
{
    const char *fname;

    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* get the file information for the source level */
    if (G_debugger->get_source_info(vmg_ &fname, linenum, level))
        return 1;

    /* get the line source for this filename */
    *src = find_internal_src(syswinifc, fname, 0, TRUE);

    /* if we didn't find the source file, return failure */
    if (*src == 0)
        return 2;

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Load line sources from a compiled game program
 */
void CHtmlDebugHelper::vm_load_sources_from_program(dbgcxdef *ctx)
{
    size_t i;

    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* add each line source */
    for (i = 0 ; i < G_srcf_table->get_count() ; ++i)
    {
        CVmSrcfEntry *entry;

        /* get this entry */
        entry = G_srcf_table->get_entry(i);

        /*
         *   If the entry is a master record, add it to the UI's file
         *   list.  We don't add non-master entries, since we want to show
         *   each unique source file in the UI only once.
         */
        if (entry->is_master())
        {
            /* add the entry */
            add_internal_line_source(entry->get_name(), entry->get_name(), i);
        }
    }

    /* note the highest ID */
    last_compiled_line_source_id_ = i;
}

/* ------------------------------------------------------------------------ */
/*
 *   Set engine execution state to GO
 */
void CHtmlDebugHelper::vm_set_exec_state_go(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* set the execution state in the debugger object */
    G_debugger->set_go();
}

/*
 *   Set the engine execution state to BREAK
 */
void CHtmlDebugHelper::vm_set_exec_state_break(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* set the execution state in the debugger object */
    G_debugger->set_break_stop();
}

/*
 *   Set engine execution state to STEP OVER
 */
void CHtmlDebugHelper::vm_set_exec_state_step_over(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* set the execution state in the debugger object */
    G_debugger->set_step_over(vmg0_);
}

/*
 *   Set engine execution state to STEP OUT
 */
void CHtmlDebugHelper::vm_set_exec_state_step_out(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* set the execution state in the debugger object */
    G_debugger->set_step_out(vmg0_);
}

/*
 *   Set engine execution state to STEP INTO
 */
void CHtmlDebugHelper::vm_set_exec_state_step_into(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* set the execution state in the debugger object */
    G_debugger->set_step_in();
}

/* ------------------------------------------------------------------------ */
/*
 *   Build a stack listing
 */
void CHtmlDebugHelper::vm_build_stack_listing(dbgcxdef *ctx, void *cbctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* have the debugger object build the stack listing */
    G_debugger->build_stack_listing(vmg_ &update_stack_disp_cb, cbctx, FALSE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Evaluate an expression
 */
int CHtmlDebugHelper::
   vm_eval_expr(struct dbgcxdef *ctx, textchar_t *buf, size_t buflen,
                const textchar_t *expr, int level,
                int *is_lval, int *is_openable,
                void (*aggcb)(void *, const char *, int,
                              const char *), void *aggctx, int speculative)
{
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* go evalute it */
        return G_debugger->eval_expr(vmg_ buf, buflen, expr, level, is_lval,
                                     is_openable, aggcb, aggctx, speculative);
    }
    else
    {
        /* engine not running - can't evaluate the expression */
        buf[0] = '\0';
        return 1;
    }
}

/*
 *   Execute an assignment
 */
int CHtmlDebugHelper::vm_eval_asi_expr(dbgcxdef *ctx, int level,
                                       const textchar_t *lvalue,
                                       const textchar_t *rvalue)
{
    textchar_t asi_expr[2048];
    textchar_t result[10];

    /* build the assignment expression */
    sprintf(asi_expr, "(%s)=%s", lvalue, rvalue);

    /* evaluate the expression */
    return vm_eval_expr(ctx, result, sizeof(result), asi_expr, level,
                        0, 0, 0, 0, FALSE);
}

/*
 *   Enumerate local variables in the current stack context.
 */
void CHtmlDebugHelper::
   vm_enum_locals(dbgcxdef *ctx, int level,
                  void (*cbfunc)(void *, const textchar_t *, size_t),
                  void *cbctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* have the VM debug object enumerate the locals */
    G_debugger->enum_locals(vmg_ cbfunc, cbctx, level);
}


/* ------------------------------------------------------------------------ */
/*
 *   Toggle a breakpoint at the current selection in the given window
 */
void CHtmlDebugHelper::vm_toggle_breakpoint(dbgcxdef *ctx, IDebugWin *win)
{
    ulong linenum;
    CHtmlDbg_win_link *win_link;
    void *code_addr;
    int bpnum;
    int did_set;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win,
                                           &linenum, &win_link, &code_addr))
        return;

    /* toggle a breakpoint at the location we found */
    CHtmlDebugHelperVM::toggle_breakpoint(this, ctx, win_link->source_id_,
                                          linenum, code_addr, 0, FALSE,
                                          FALSE, TRUE, &bpnum, &did_set);
}

/*
 *   Set a temporary breakpoint at the current selection in the given
 *   window.  Returns true if successful, false if we didn't set the
 *   breakpoint.
 */
int CHtmlDebugHelper::vm_set_temp_bp(dbgcxdef *ctx, IDebugWin *win)
{
    ulong linenum;
    CHtmlDbg_win_link *win_link;
    void *code_addr;
    int did_set;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win,
                                           &linenum, &win_link, &code_addr))
        return FALSE;

    /*
     *   if there's already a breakpoint there, we don't need to set
     *   another one
     */
    if (find_internal_bp(win_link->source_id_, linenum) != 0)
        return FALSE;

    /* toggle a breakpoint at the location we found */
    if (CHtmlDebugHelperVM::toggle_breakpoint(this, ctx, win_link->source_id_,
                                              linenum, code_addr, 0, FALSE,
                                              FALSE, FALSE,
                                              &tmp_bpnum_, &did_set))
        return FALSE;

    /* success */
    return TRUE;
}

/*
 *   Clear a temporary breakpoint
 */
void CHtmlDebugHelper::vm_clear_temp_bp(dbgcxdef *ctx)
{
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* toggle the temporary breakpoint */
        G_debugger->delete_breakpoint(vmg_ tmp_bpnum_);

        /* forget the temporary breakpoint */
        tmp_bpnum_ = 0;
    }
}

/*
 *   Set a global breakpoint
 */
int CHtmlDebugHelper::vm_set_global_breakpoint(dbgcxdef *ctx,
                                               const textchar_t *cond,
                                               int change,
                                               int *bpnum,
                                               char *errbuf, size_t errbuflen)
{
    if (ctx != 0)
    {
        int did_set;

        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* set the breakpoint in the VM */
        if (G_debugger->toggle_breakpoint(vmg_ 0, cond, change, bpnum,
                                          &did_set, errbuf, errbuflen))
            return 3;
    }
    else
    {
        /* the engine isn't running; synthesize a breakpoint ID */
        *bpnum = synthesize_bp_num();
    }

    /* set the internal breakpoint record */
    toggle_internal_bp(ctx, 0, 0, cond, change, *bpnum, TRUE, TRUE);

    /* success */
    return 0;
}

/*
 *   Enable or disable a breakpoint at the current selection in the given
 *   window
 */
void CHtmlDebugHelper::vm_toggle_bp_disable(dbgcxdef *ctx,
                                            IDebugWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long linenum;
    void *code_addr;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &linenum,
                                           &win_link, &code_addr))
        return;

    /* toggle the status */
    CHtmlDebugHelperVM::toggle_bp_disable(this, ctx, win_link->source_id_,
                                          linenum, code_addr);
}

/*
 *   Enable or disable a breakpoint
 */
int CHtmlDebugHelper::vm_enable_breakpoint(dbgcxdef *ctx, int bpnum,
                                           int enable)
{
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* ask the VM to the do work */
        G_debugger->set_breakpoint_disable(vmg_ bpnum, !enable);

        /* success */
        return 0;
    }
    else
    {
        /* engine not active - return failure */
        return 1;
    }
}

/*
 *   Determine if a breakpoint is enabled
 */
int CHtmlDebugHelper::vm_is_bp_enabled(dbgcxdef *ctx, int bpnum)
{
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* ask the VM about the breakpoint */
        return !G_debugger->is_breakpoint_disabled(vmg_ bpnum);
    }
    else
    {
        /* engine not active - return enabled by default */
        return TRUE;
    }
}

/*
 *   Set a breakpoint's condition text
 */
int CHtmlDebugHelper::vm_set_bp_condition(dbgcxdef *ctx, int bpnum,
                                          const textchar_t *cond,
                                          int change,
                                          char *errbuf, size_t errbuflen)
{
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* ask the VM to the do work */
        return G_debugger
            ->set_breakpoint_condition(vmg_ bpnum, cond, change,
                                       errbuf, errbuflen);
    }
    else
    {
        /* engine isn't running - fail */
        strncpy(errbuf, "The program is not running.", errbuflen - 1);
        errbuf[errbuflen - 1] = '\0';
        return 1;
    }
}


/*
 *   Delete a breakpoint given a breakpoint number
 */
int CHtmlDebugHelper::vm_delete_breakpoint(dbgcxdef *ctx, int bpnum)
{
    /* delete the VM breakpoint if the engine is running */
    if (ctx != 0)
    {
        /* set up for VM global access */
        VMGLOB_PTR(ctx->vmg);

        /* ask the VM to delete the breakpoint */
        G_debugger->delete_breakpoint(vmg_ bpnum);
    }

    /* delete the internal breakpoint tracker and update the display */
    delete_internal_bp(ctx, bpnum);

    /* success */
    return 0;
}

/*
 *   Set a breakpoint loaded from a saved configuration.  Returns zero on
 *   success, non-zero on failure.  We'll fill in *actual_linenum with the
 *   line number at which we actually set the breakpoint; this might
 *   differ from the proposed line number because the source might have
 *   been modified and recompiled since the configuration was saved, hence
 *   the lines at which breakpoints are valid might have changed.
 */
int CHtmlDebugHelper::vm_set_loaded_bp(dbgcxdef *dbgctx, int source_id,
                                       unsigned long orig_linenum,
                                       unsigned long *actual_linenum,
                                       const char *cond, int change,
                                       int disabled)
{
    void *code_addr;
    int bpnum;
    int did_set;

    /* try putting the breakpoint at the requested location */
    *actual_linenum = orig_linenum;

    /* get the exact location of the breakpoint */
    if (CHtmlDebugHelperVM::srcofs_to_code(dbgctx, source_id, actual_linenum,
                                           &code_addr))
        return 1;

    /* set the breakpoint */
    if (CHtmlDebugHelperVM::toggle_breakpoint(this, dbgctx, source_id,
                                              *actual_linenum, code_addr,
                                              cond, change, TRUE, TRUE,
                                              &bpnum, &did_set))
        return 2;

    /* if it's disabled, disable it */
    if (disabled)
        CHtmlDebugHelperVM::toggle_bp_disable(this, dbgctx, source_id,
                                              *actual_linenum, code_addr);

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Move the execution position in the engine to the current selection in
 *   the given window.  Returns zero on success, non-zero on failure.
 *   This only works for setting the line location within the currently
 *   executing function.
 */
int CHtmlDebugHelper::vm_set_next_statement(dbgcxdef *ctx,
                                            void *exec_ofs,
                                            IDebugWin *win,
                                            unsigned long *linenum,
                                            int *need_single_step)
{
    CHtmlDbg_win_link *win_link;
    const uchar *code_addr;

    /* we can't do anything if the engine isn't running */
    if (ctx == 0)
        return 1;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, linenum,
                                           &win_link, &code_addr))
        return 2;

    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* ask the VM to make the change */
    if (G_debugger->set_exec_ofs((const uchar **)exec_ofs, code_addr))
        return 3;

    /* T3 doesn't require any stepping after a location change */
    *need_single_step = FALSE;

    /* success */
    return 0;
}

/*
 *   Is the given line executable?
 */
int CHtmlDebugHelper::vm_is_line_executable(struct dbgcxdef *ctx,
                                            IDebugWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long src_linenum, code_linenum;
    CHtmlDbg_line_link *line_link;
    unsigned long code_addr;

    /* we can't do anything if the engine isn't running */
    if (ctx == 0)
        return FALSE;

    /* get information on the current source file position */
    src_linenum = find_line_info(win, &win_link, &line_link);

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &code_linenum,
                                           &win_link, &code_addr))
        return FALSE;

    /*
     *   it's executable if the line number of the code matches the line
     *   number in the source
     */
    return (src_linenum == code_linenum);
}


/*
 *   Check if the current selection in the given window is within the same
 *   function or method as the current execution point.  Returns true if
 *   so, false if not.  This is a condition of setting the next statement;
 *   this function is provided so that the UI code can check to make sure
 *   that this condition is met when the user attempts to move the
 *   execution point, and generate an appropriate error message if not.
 */
int CHtmlDebugHelper::vm_is_in_same_fn(dbgcxdef *ctx,
                                       IDebugWin *win)
{
    CHtmlDbg_win_link *win_link;
    unsigned long linenum;
    const uchar *code_addr;

    /* we can't do anything if the engine isn't running */
    if (ctx == 0)
        return FALSE;

    /* find information on the current line in the window */
    if (CHtmlDebugHelperVM::srcofs_to_code(this, ctx, win, &linenum,
                                           &win_link, &code_addr))
        return FALSE;

    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* let the engine make the determination */
    return G_debugger->is_in_current_method(vmg_ code_addr);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the text of an error message
 */
void CHtmlDebugHelper::vm_get_error_text(struct dbgcxdef *ctx, int err,
                                         textchar_t *buf, size_t buflen)
{
    const char *txt;
    char dflt[50];

    /* look up the message in the run-time messages */
    txt = err_get_msg(vm_messages, vm_message_count, err, FALSE);

    /* try the compiler messages if we didn't find anything */
    if (txt == 0)
        txt = err_get_msg(tc_messages, tc_message_count, err, FALSE);

    /* if we didn't find the message, provide a generic default */
    if (txt == 0)
    {
        /* build the generic message based on the error number */
        sprintf(dflt, "VM Error %d", err);

        /* use the default message as the result */
        txt = dflt;
    }

    /* copy the text to the caller's buffer */
    strncpy(buf, txt, buflen);

    /* null-terminate the result if there's any room */
    if (buflen != 0)
        buf[buflen - 1] = '\0';
}

/*
 *   Format an error message with arguments from the error stack
 */
void CHtmlDebugHelper::vm_format_error(struct dbgcxdef *ctx, int err,
                                       textchar_t *buf, size_t buflen)
{
    /* get the error message */
    const char *txt = err_get_msg(vm_messages, vm_message_count, err, FALSE);

    /* try the compiler messages if we didn't find anything */
    if (txt == 0)
        txt = err_get_msg(tc_messages, tc_message_count, err, FALSE);

    /* if there's no message, use the default */
    if (txt == 0)
    {
        /* get the default text instead */
        vm_get_error_text(ctx, err, buf, buflen);
    }
    else
    {
        /* format the message using the current exception being handled */
        err_format_msg(buf, buflen, txt, err_get_cur_exc());
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a source file into a given window
 */
int CHtmlDebugHelper::vm_load_file_into_win(CHtmlDbg_win_link *win_link,
                                            const textchar_t *fname)
{
    /* open the file */
    osfildef *fp = osfoprbv(fname, OSFTTEXT);

    /* if we couldn't open the file, return failure */
    if (fp == 0)
        return 1;

    /* begin the loading process */
    void *load_ctx = 0;
    win_link->srcmgr_->begin_file_load(load_ctx);

    /* load text from the source file into the window */
    for (;;)
    {
        textchar_t buf[60000];
        size_t len;

        /* load another buffer; stop if we've reached EOF */
        if ((len = osfrbc(fp, buf, sizeof(buf))) == 0)
            break;

        /* add the text to the source window */
        win_link->srcmgr_->load_text(buf, len);
    }

    /* tell the source manager we're done loading the file */
    win_link->srcmgr_->end_file_load(load_ctx);

    /* done with the source file - close it */
    osfcls(fp);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Toggle hidden output
 */
void CHtmlDebugHelper::vm_toggle_dbg_hid()
{
    /* this is not applicable to the T3 engine */
}

/*
 *   Get hidden output display status
 */
int CHtmlDebugHelper::vm_get_dbg_hid()
{
    /*
     *   this is not applicable to the T3 engine - indicate that hidden
     *   output is not displayed
     */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Signal a QUIT condition in the engine
 */
void CHtmlDebugHelper::vm_signal_quit(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* tell the console to stop logging input */
    G_console->close_command_log(vmg0_);

    /* set the HALT flag in the VM */
    G_interpreter->set_halt_vm(TRUE);
}

/*
 *   cancel script recording
 */
void CHtmlDebugHelper::vm_cancel_script_recording(struct dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* close the command log */
    G_console->close_command_log(vmg0_);
}

/*
 *   Signal a RESTART condition in the engine
 */
void CHtmlDebugHelper::vm_signal_restart(dbgcxdef *)
{
    err_throw(VMERR_DBG_RESTART);
}

/*
 *   Signal abort in the engine
 */
void CHtmlDebugHelper::vm_signal_abort(dbgcxdef *)
{
    err_throw(VMERR_DBG_ABORT);
}

/* ------------------------------------------------------------------------ */
/*
 *   Determine if call tracing is active in the engine
 */
int CHtmlDebugHelper::vm_is_call_trace_active(dbgcxdef *ctx) const
{
    // $$$
    return FALSE;
}

/*
 *   turn call trace on or off in the engine
 */
void CHtmlDebugHelper::vm_set_call_trace_active(dbgcxdef *ctx, int flag)
{
    // $$$
}

/*
 *   Clear the call trace log in the engine
 */
void CHtmlDebugHelper::vm_clear_call_trace_log(dbgcxdef *ctx)
{
    // $$$
}

/*
 *   Get a pointer to the history log buffer maintained by the engine
 */
const textchar_t *CHtmlDebugHelper::vm_get_call_trace_buf(dbgcxdef *ctx) const
{
    // $$$
    return "";
}

/*
 *   get the size of the data accumulated in the history log
 */
unsigned long CHtmlDebugHelper::vm_get_call_trace_len(dbgcxdef *ctx) const
{
    // $$$
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Turn profiling on
 */
void CHtmlDebugHelper::start_profiling(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* ask the VM to start profiling */
    G_interpreter->start_profiling();
}

/*
 *   Turn profiling off
 */
void CHtmlDebugHelper::end_profiling(dbgcxdef *ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* ask the VM to end profiling */
    G_interpreter->end_profiling();
}

/*
 *   Get profiling data
 */
void CHtmlDebugHelper::get_profiling_data(
    dbgcxdef *ctx,
    void (*cb)(void *ctx, const char *func_name,
               unsigned long time_direct,
               unsigned long time_in_children,
               unsigned long call_cnt),
    void *cb_ctx)
{
    /* set up for VM global access */
    VMGLOB_PTR(ctx->vmg);

    /* ask the VM to get the data */
    G_interpreter->get_profiling_data(vmg_ cb, cb_ctx);
}

