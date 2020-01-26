/* $Header: d:/cvsroot/tads/html/win32/tadsdlg.h,v 1.3 1999/07/11 00:46:47 MJRoberts Exp $ */

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadsdlg.h - dialogs
Function

Notes

Modified
  10/27/97 MJRoberts  - Creation
*/

#ifndef TADSDLG_H
#define TADSDLG_H

#include <stdlib.h>
#include <memory.h>
#include <windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Dialog control class.  This class can be instantiated to associate a
 *   C++ object with a control in a dialog.
 */
class CTadsDialogCtl
{
public:
    CTadsDialogCtl(HWND hdl)
    {
        /* remember my window handle */
        hdl_ = hdl;

        /* I have no control ID */
        id_ = 0;
    }
    CTadsDialogCtl(HWND dlg, int id)
    {
        /* get my control's window handle from the dialog */
        hdl_ = GetDlgItem(dlg, id);

        /* remember my ID */
        id_ = id;
    }
    CTadsDialogCtl(int id)
    {
        /* we don't have our handle yet */
        hdl_ = 0;

        /* remember my ID */
        id_  = id;
    }

    /* set up on initializing the dialog */
    void on_init_dlg(HWND dlg)
    {
        /* get my handle from the dialog */
        hdl_ = GetDlgItem(dlg, id_);
    }

    /* get the control's handle */
    HWND gethdl() { return hdl_; }

    /* get my control ID */
    int getid() { return id_; }

    /* draw the control - for non-owner-drawn controls, does nothing */
    virtual void draw(int, DRAWITEMSTRUCT *) { }

    /* measure the control for owner-drawing */
    virtual void measure(int, MEASUREITEMSTRUCT *) { }

    /* invoke the command on the control */
    virtual void do_command(int notify_code) = 0;

private:
    /* my control's window handle */
    HWND hdl_;

    /* my control ID */
    int id_;
};


/*
 *   Owner-drawn dialog control.  This is a subclass of the dialog control
 *   class; this subclass provides for owner drawing.
 */
class CTadsOwnerDrawnCtl: public CTadsDialogCtl
{
public:
    CTadsOwnerDrawnCtl(HWND hdl) : CTadsDialogCtl(hdl) { }
    CTadsOwnerDrawnCtl(HWND dlg, int id) : CTadsDialogCtl(dlg, id) { }
    CTadsOwnerDrawnCtl(int id) : CTadsDialogCtl(id) { }

    /* draw the control */
    virtual void draw(int id, DRAWITEMSTRUCT *dis) = 0;

    /* measure the item for drawing */
    virtual void measure(int id, MEASUREITEMSTRUCT *mis) { }
};

/*
 *   Owner-drawn button.  This class provides default drawing for the
 *   frame portion of a button; it must be overridden to provide drawing
 *   of the actual contents of the button face.
 */
class CTadsOwnerDrawnBtn: public CTadsOwnerDrawnCtl
{
public:
    CTadsOwnerDrawnBtn(HWND hdl) : CTadsOwnerDrawnCtl(hdl) { }
    CTadsOwnerDrawnBtn(int id) : CTadsOwnerDrawnCtl(id) { }
    CTadsOwnerDrawnBtn(HWND dlg, int id) : CTadsOwnerDrawnCtl(dlg, id) { }

    virtual void draw(int id, DRAWITEMSTRUCT *dis);
};

/*
 *   Owner-drawn simple combo.  This provides the default handling for a
 *   combo that does owner-drawing for simple strings.  This can be used, for
 *   example, to draw an icon next to each item, or to use different styles
 *   for different items.
 */
class CTadsOwnerDrawnCombo: public CTadsOwnerDrawnCtl
{
public:
    CTadsOwnerDrawnCombo(HWND hdl) : CTadsOwnerDrawnCtl(hdl) { }
    CTadsOwnerDrawnCombo(int id) : CTadsOwnerDrawnCtl(id) { }
    CTadsOwnerDrawnCombo(HWND dlg, int id) : CTadsOwnerDrawnCtl(dlg, id) { }

    /* measure and draw */
    virtual void measure(int id, MEASUREITEMSTRUCT *mis);
    virtual void draw(int id, DRAWITEMSTRUCT *dis);

    /* invoke the command on the control */
    virtual void do_command(int notify_code) { }

    /*
     *   Draw our contents.  When this is called, we've filled in the item
     *   rectangle with the standard background color for the selection
     *   status, and we've set up the DC with the correct font and text color
     *   for the selection status.  By default, this simply draws the title
     *   string in the item rectangle.
     */
    virtual void draw_contents(HDC dc, RECT *item_rc, textchar_t *title,
                               DRAWITEMSTRUCT *dis)
    {
        RECT rc = *item_rc;
        rc.left += 2;
        DrawText(dc, title, -1, &rc, DT_SINGLELINE);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   List of dialog controls.  This class simplifies management of dialog
 *   controls by providing an association between the window handles and
 *   the C++ objects.
 */

class CTadsDialogCtlList
{
public:
    CTadsDialogCtlList(int init_count) { init(init_count); }
    CTadsDialogCtlList() { init(10); }
    ~CTadsDialogCtlList();

    /*
     *   Add a new control.  We take ownership of the control; in
     *   particular, the list deletes all of the controls when the list
     *   itself is deleted.
     */
    void add(class CTadsDialogCtl *ctl);

    /*
     *   find a control given a handle or ID: if the handle is non-zero,
     *   we'll match on that, otherwise we'll match on ID
     */
    class CTadsDialogCtl *find(int id, HWND hdl);

    /*
     *   Respond to a WM_DRAWITEM message -- find the control to be drawn,
     *   and call its draw() method.  Returns true if we found the
     *   control, false if not.
     */
    int draw_item(int id, DRAWITEMSTRUCT *dis);

    /*
     *   Respond to a WM_MEASUREITEM message - find the control to be drawn,
     *   and call its measure() method.
     */
    int measure_item(int id, HWND hwnd, MEASUREITEMSTRUCT *mis);

    /*
     *   Invoke the command on the given item.  Returns true if the item
     *   is found, false if not.
     */
    int do_command(HWND dlg, WPARAM wpar);

    /* initialize the dialog */
    void on_init_dlg(HWND dlg)
    {
        /* initialize each control in the list */
        for (int i = 0 ; i < cnt_ ; ++i)
            arr_[i]->on_init_dlg(dlg);
    }

private:
    void init(int init_count)
    {
        cnt_ = 0;
        arr_size_ = init_count;
        arr_ = (class CTadsDialogCtl **)
               th_malloc(init_count * sizeof(*arr_));
    }

    int cnt_;
    int arr_size_;
    class CTadsDialogCtl **arr_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dialog class.  Individual dialogs should subclass this abstract class
 *   to provide implementations of the dialog-specific methods.
 */

class CTadsDialog
{
public:
    CTadsDialog();
    virtual ~CTadsDialog();

    /* get my window handle */
    HWND get_handle() const { return handle_; }

    /*
     *   Load the dialog from a resource and run it as a modal dialog.
     *   Returns the ID of the control used to dismiss the dialog.  If
     *   taskmodal is true, we'll disable all windows in the task while
     *   the dialog is running.
     */
    int run_modal(unsigned int res_id, HWND owner, HINSTANCE app_instance);

    /*
     *   Run a property sheet dialog.
     */
    static void run_propsheet(HWND owner, PROPSHEETHEADER *psh);

    /*
     *   Run a tree-style property sheet dialog.  This replaces the tab
     *   control normally shown at the top of the dialog with a tree control
     *   on the left side.
     */
    static void run_tree_propsheet(HWND owner, PROPSHEETHEADER *psh);

    /*
     *   Before running a modal dialog, disable all windows owned by the
     *   given window.  The return value is a private context structure
     *   that must be passed to modal_dlg_post after the dialog is
     *   dismissed.
     */
    static void *modal_dlg_pre(HWND owner, int disable_owner);

    /*
     *   After running a modal dialog, re-enable windows previously
     *   disabled with modal_dlg_pre().  The context parameter is the
     *   return value from the corresponding modal_dlg_pre() call.
     */
    static void modal_dlg_post(void *ctx);

    /* create as a modeless dialog */
    HWND open_modeless(unsigned int res_id, HWND owner, HINSTANCE app_inst);

    /*
     *   Set up an OPENFILENAME structure so that a call to
     *   GetOpenFileName or GetSaveFileName will hook to a private hook
     *   procedure defined here that will center the dialog on the screen.
     */
    static void set_filedlg_center_hook(OPENFILENAME *info);

    /*
     *   Center a dialog window on the screen
     */
    static void center_dlg_win(HWND dialog_hwnd);

    /*
     *   Prepare to open.  The dialog box procedure calls this when it
     *   receives a WM_INITDIALOG message.  We remember our window handle and
     *   initialize our pre-built control list.
     */
    void prepare_to_open(HWND hwnd)
    {
        /* add myself to the active list if I'm not there already */
        add_to_active();

        /* remember my handle */
        handle_ = hwnd;

        /* initialize my control list */
        init_controls();
    }

    /*
     *   Initialize the dialog.  This is called in response to the
     *   initialization message from Windows.  If the dialog has any
     *   CTadsDialogCtl's, those objects must be created here and added to
     *   the control list.  The initial values, enabling, and other states
     *   of all of the controls in the dialog (whether covered by
     *   CTadsDialogCtl instances or not) should be set here as well.  The
     *   default implementation doesn't do anything.
     */
    virtual void init();

    /*
     *   Initialize the dialog's focus.  'def_id' is the ID of the control
     *   that will receive focus by default; to accept the default, this
     *   routine can merely return true.  To change focus to another
     *   control, use SetFocus, and return false.  This default
     *   implementation simply returns true to accept the default focus.
     */
    virtual int init_focus(int def_id) { return TRUE; }

    /*
     *   Destroy the Windows dialog.  This is called in response to the
     *   deletion message from Windows.  The default implementation
     *   removes the dialog from the active dialog list.  Note that this
     *   does NOT destroy the CTadsDialog object -- this is merely the
     *   Windows object that's being deleted.
     */
    virtual void destroy();

    /*
     *   Draw an owner-drawn control.  By default, this routine attempts
     *   to find the owner-drawn control in our internal list of
     *   CTadsDialogCtl objects, and has that object do the drawing.  If
     *   all owner-drawn controls are covered by CTadsDialogCtl objects,
     *   and those objects are added to the control list during init(),
     *   this routine need not be overridden.  Returns true if the message
     *   was processed, false if not.
     */
    virtual int draw_item(int ctl_id, DRAWITEMSTRUCT *dis);

    /*
     *   Measure an owner-drawn control in preparation for drawing.  By
     *   default, this routine attempts to find the owner-drawn control in
     *   our internal list of CTadsDialogCtl objects, and has that object do
     *   the measuring.  If all owner-drawn controls are covered by
     *   CTadsDialogCtl objects, and those objects are added to the control
     *   list during init(), this routine need not be overridden.  Returns
     *   true if the message was processed, false if not.
     */
    virtual int measure_item(int id, MEASUREITEMSTRUCT *mis);

    /*
     *   Respond to a "pre" WM_MEASUREITEM message - that is, a
     *   WM_MEASUREITEM sent before a WM_INITDIALOG.  We haven't set up our
     *   controls yet at this point, so we have to handle this differently.
     */
    int pre_measure_item(int ctl_id, HWND ctl_hwnd, MEASUREITEMSTRUCT *mis);

    /*
     *   Delete an item in an owner-drawn control.  This is called during
     *   control deletion by controls such as popups and list boxes that
     *   can have lists of owner-allocated items, so that the owner of the
     *   control can delete the associated memory.  Returns true if the
     *   message was processed.  The default implementation does nothing.
     */
    virtual int delete_ctl_item(int /*ctl_id*/, DELETEITEMSTRUCT *)
        { return FALSE; }

    /*
     *   Compare items in an owner-drawn control.  The system calls this
     *   when owner-drawn non-string items are added to a listbox, combo
     *   box, or other control with an item list with a "sorted" style, in
     *   order to determine the correct ordering of the user-defined
     *   items.  By default, this just returns 0 to indicate that the
     *   items are equivalent in the sorting order.  Dialogs that use
     *   owner-drawn lists with non-string data must override this to
     *   provide correct sorting information for any lists that must be
     *   sorted.
     */
    virtual int compare_ctl_item(int /*ctlid*/, COMPAREITEMSTRUCT *)
        { return 0; }

    /*
     *   Process a command.  The default implementation will automatically
     *   route the message to a CTadsDialogCtl object, if one exists for
     *   the control; otherwise, this routine can be overriden to process
     *   commands for which no CTadsDialogCtl exists.
     */
    virtual int do_command(WPARAM id, HWND ctl);

    /*
     *   Process a notification message.  Does nothing by default.
     */
    virtual int do_notify(NMHDR *, int) { return FALSE; }

    /* add myself to/remove myself from the dialog list */
    void add_to_active();
    void remove_from_active();

protected:
    /* hook procedure for centering a GetOpenFileName dialog */
    static UINT APIENTRY filedlg_center_hook(HWND dlg, UINT msg,
                                             WPARAM wpar, LPARAM lpar);

    /* initialize a font popup */
    void init_font_popup(int id, int proportional, int fixed,
                         const textchar_t *init_setting, int clear,
                         unsigned int charset_id);

    /* initialize a font popup, using an explicit selection callback */
    void init_font_popup(int id, const textchar_t *init_setting,
                         int (*selector_func)(void *, ENUMLOGFONTEX *,
                                              NEWTEXTMETRIC *),
                         void *selector_func_ctx, int clear,
                         unsigned int charset_id);

    /* message handler - main entrypoint from windows */
    static BOOL CALLBACK dialog_proc(HWND dlg_hwnd, UINT message,
                                     WPARAM wpar, LPARAM lpar);

    /* message handler - instance method */
    virtual BOOL do_dialog_msg(HWND dlg_hwnd, UINT message, WPARAM wpar,
                               LPARAM lpar);

    /* property sheet initialization callback */
    static int CALLBACK prop_page_cb(HWND hwnd, UINT id, PROPSHEETPAGE *page);

    /* font enumerator callback */
    static int CALLBACK init_font_popup_cb(ENUMLOGFONTEX *elf,
                                           NEWTEXTMETRIC *tm,
                                           DWORD font_type, LPARAM lpar);

    /*
     *   find a dialog object in the active dialog list, given a window
     *   handle
     */
    static CTadsDialog *find_dialog(HWND hwnd);

    /* initialize my control list */
    void init_controls() { controls_.on_init_dlg(handle_); }

    /*
     *   Array of dialogs.  When we enter the dialog procedure, we must
     *   find the CTadsDialog object that corresponds to the window
     *   handle, so that we can call its methods as needed.  Whenever a
     *   dialog is opened or closed, it's added to or removed from this
     *   list.
     */
    static CTadsDialog **dialogs_;

    /* size of the array allocated for dialogs_ */
    static size_t dialogs_size_;

    /* number of dialogs in the dialog list */
    static size_t dialog_cnt_;

    /* the dialog's window handle */
    HWND handle_;

    /* our dialog control list */
    CTadsDialogCtlList controls_;

    /* are running modally or modelessly? */
    int is_modal_;

private:
    /* initialize a font popup (internal support routine) */
    void init_font_popup(int id, struct ifp_cb_info *info,
                         const textchar_t *init_setting, int clear);
};

/* ------------------------------------------------------------------------ */
/*
 *   Special values of 'wpar' for PSM_QUERYSIBLINGS
 */

/* reinitialize all pages */
#define TADS_PP_QUERY_REINIT    1

/* does this dialog have any unapplied changes? */
#define TADS_PP_QUERY_CHANGED   2


/* ------------------------------------------------------------------------ */
/*
 *   Property sheet page dialog subclass
 */

class CTadsDialogPropPage: public CTadsDialog
{
public:
    CTadsDialogPropPage()
    {
        /* we don't know our containing property sheet yet */
        sheet_handle_ = 0;

        /* we don't have an initial control yet */
        init_ctl_id_ = 0;
    }

    /*
     *   Initialize a PROPSHEETPAGE structure for this dialog page object.
     *   This is used to set up the PROPSHEETHEADER in preparation to run
     *   the property sheet or wizard.
     */
    void init_ps_page(HINSTANCE app_inst,
                      PROPSHEETPAGE *psp, int dlg_res_id,
                      int caption_str_id,
                      int init_page_id, int init_ctl_id,
                      PROPSHEETHEADER *psh);

    /*
     *   Prepare the dialog for use as a property page.  This should be
     *   called with the property page descriptor prior to opening the
     *   property sheet; this sets up the property page descriptor so that
     *   the property sheet calls our callback to initialize, which we
     *   need to do for the dialog to make it into the active dialog list.
     */
    void prepare_prop_page(PROPSHEETPAGE *page)
    {
        /* set up my dialog procedure */
        page->pfnDlgProc = (DLGPROC)&dialog_proc;

        /*
         *   store a pointer to this object in the lParam, so we can
         *   figure out in the message loop what CTadsDialog object it's
         *   being used with
         */
        page->lParam = (LPARAM)this;
    }

    /*
     *   set the initial control - this is the control on which we'll set
     *   focus when the dialog is initialized
     */
    void set_init_ctl(int ctl_id) { init_ctl_id_ = ctl_id; }

    /*
     *   check for initial focus
     */
    virtual int init_focus(int def_id)
    {
        /* if we're to set an initial control, do so now */
        if (init_ctl_id_ != 0)
        {
            /* set focus on the control */
            SetFocus(GetDlgItem(handle_, init_ctl_id_));

            /* forget the initial focus item now that we've used it */
            init_ctl_id_ = 0;

            /* tell the caller we chose the focus */
            return FALSE;
        }

        /* inherit default */
        return CTadsDialog::init_focus(def_id);
    }

    /*
     *   Set the "changed" flag in the property sheet
     */
    void set_changed(int changed);

    /*
     *   Determine if we have changes.  If 'save' is true, we are to save the
     *   changes, otherwise we're simply to check for pending, unapplied
     *   changes.  In either case, we return true if we have any changes
     *   pending, false if not.  This can be used to determine the state of
     *   the "Apply" button.
     *
     *   This is meant to be overridden, and is meant for subclass use.  We
     *   simply return false (i.e., no changes pending) by default.
     *   Subclasses do not have to use this idiom for managing the change
     *   state, but doing so allows the "Apply" button state to be inferred
     *   by sending a PSM_QUERYSIBLINGS message to the parent property sheet
     *   dialog with subcode TADS_PP_QUERY_CHANGED.
     */
    virtual int has_changes(int save)
    {
        return FALSE;
    }

    /*
     *   Check the "Apply" button state.  This queries all pages in the
     *   property sheet, using has_changes(), to determine if there are any
     *   pending changes.
     */
    int is_apply_enabled()
    {
        /*
         *   Send a query-siblings message to the parent, with our special
         *   check-for-changes code as the sub-message.  Return the result.
         */
        return SendMessage(GetParent(handle_), PSM_QUERYSIBLINGS,
                           TADS_PP_QUERY_CHANGED, 0);
    }

    /*
     *   Re-initialize all pages.  We send a PSM_QUERYSIBLINGS message to our
     *   parent property sheet, with our subcode TADS_PP_QUERY_REINIT.  This
     *   in turn notifies all pages to re-load themselves.
     */
    void reinit_all_pages()
    {
        /*
         *   send a query-siblings message to the parent, with our special
         *   re-initializae code
         */
        SendMessage(GetParent(handle_), PSM_QUERYSIBLINGS,
                    TADS_PP_QUERY_REINIT, 0);
    }

protected:
    /* property page dialog procedure */
    static BOOL CALLBACK dialog_proc(HWND dlg_hwnd, UINT message,
                                     WPARAM wpar, LPARAM lpar);

    /* message handler - instance method */
    virtual BOOL do_dialog_msg(HWND dlg_hwnd, UINT message, WPARAM wpar,
                               LPARAM lpar);

    /*
     *   Handle a "query siblings" message.  This message (PSM_QUERYSIBLINGS)
     *   can be sent to the parent property sheet dialog, which in turn sends
     *   it to each property page dialog, which is to say: us.  This routine
     *   should return zero to allow the message to be propagated to the
     *   other siblings, or any non-zero value to stop the processing.
     *
     *   The 'wpar' and 'lpar' values are application-defined.  We use 'wpar'
     *   as a sub-message ID, defining the TADS_PP_QUERY_xxx values and
     *   leaving remaining values for subclass use.
     */
    virtual int query_siblings(WPARAM wpar, LPARAM lpar)
    {
        /* check for known messages */
        switch(wpar)
        {
        case TADS_PP_QUERY_REINIT:
            /* re-initialize the page */
            init();

            /* after initialization, we're not changed */
            set_changed(FALSE);

            /* propagate the message to remaining siblings */
            return 0;

        case TADS_PP_QUERY_CHANGED:
            /* return our has-changes indication (check only - do not save) */
            return has_changes(FALSE);

        default:
            /* we do not handle other messages - allow them to propagate */
            return 0;
        }
    }

    /* property sheet handle */
    HWND sheet_handle_;

    /* initial control ID, if this is the initial page */
    int init_ctl_id_;
};

#endif /* TADSDLG_H */

