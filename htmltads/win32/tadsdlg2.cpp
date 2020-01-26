#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadsdlg2.cpp - further CTadsDialog implementation
Function
  This is separated out to keep the base dialog class as small and
  unentangled as possible.  This is particularly important so that our
  SETUP program can avoid dragging in other classes.
Notes

Modified
  11/20/06 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <malloc.h>
#include <windows.h>

#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef HTMLRES_H
#include "htmlres.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   our native notification format (ANSI or Unicode, depending on how we're
 *   compiled)
 */
#ifdef UNICODE
#define NFR_NATIVE  NFR_UNICODE
#else
#define NFR_NATIVE  NFR_ANSI
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Property sheet constants.  These are system constants determined with
 *   the Spy tool.
 */
#define IDC_PROPSHEET_TAB  0x3020


/* ------------------------------------------------------------------------ */
/*
 *   Property sheet hook, for wizard-style dialogs
 */
class PropSheetHook: public CTadsWinSubclassHook
{
public:
    PropSheetHook(PROPSHEETHEADER *psh)
    {
        /* remember the property sheet descriptor */
        psh_ = psh;
    }

    ~PropSheetHook()
    {
    }

protected:
    /* (subclassed) subclassed window procedure */
    virtual LRESULT winproc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
    {
        /* inherit the default handling */
        return CTadsWinSubclassHook::winproc(hwnd, msg, wpar, lpar);
    }

    /* the property sheet descriptor */
    PROPSHEETHEADER *psh_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Run a property sheet dialog
 */
void CTadsDialog::run_propsheet(HWND owner, PROPSHEETHEADER *psh)
{
    /* disable the owner's subwindows before showing the dialog */
    void *ctx = CTadsDialog::modal_dlg_pre(owner, FALSE);

    /* set up the hook */
    PropSheetHook hook(psh);
    hook.set_hook();

    /* run the property sheet */
    PropertySheet(psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Window creation hook for the tree-based property sheet dialog
 */
class TreePropSheetHook: public CTadsWinSubclassHook
{
public:
    TreePropSheetHook(PROPSHEETHEADER *psh)
    {
        /* remember the property sheet descriptor */
        psh_ = psh;

        /* we haven't set up our dialog window yet */
        tv_ = 0;
        tabht_ = 0;
    }

    ~TreePropSheetHook()
    {
        /* destroy our image list */
        ImageList_Destroy(tvil_);
    }

protected:
    /* (subclassed) subclassed window procedure */
    virtual LRESULT winproc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
    {
        LRESULT ret;

        switch (msg)
        {
        case WM_INITDIALOG:
            /* handle the dialog initialization */
            do_initdialog1();

            /*
             *   inherit the system handling, to let the system position
             *   everything in the dialog
             */
            ret = CTadsWinSubclassHook::winproc(hwnd, msg, wpar, lpar);

            /* do the second part, after system initialization */
            do_initdialog2();

            /*
             *   create a timer for monitoring page changes made without our
             *   approval (the timer ID is just a random number, in the hope
             *   that the windows internal control won't use the same ID)
             */
            SetTimer(hwnd, 29850, 50, 0);

            /* return what the system code returned */
            return ret;

        case WM_TIMER:
            /* check for a page change */
            if (wpar == 29850)
            {
                sync_tree(FALSE);
                return 0;
            }
            break;

        case WM_NOTIFY:
            /* handle the notification */
            if (do_notify((int)wpar, (LPNMHDR)lpar))
                return 0;

            /* didn't handle it - proceed to system handling */
            break;

        case WM_NOTIFYFORMAT:
            /*
             *   for the tree control, ask for notifications in our native
             *   mode, rather than what the system dialog handler wants -
             *   we're going to handle these, not the system handler, so it's
             *   our format that matters here
             */
            if ((HWND)wpar == tv_ && lpar == NF_QUERY)
                return NFR_NATIVE;

            /* otherwise, let the system handle it */
            break;
        }

        /* inherit the default handling */
        return CTadsWinSubclassHook::winproc(hwnd, msg, wpar, lpar);
    }

    /* dialog initialization - early part, before the system code runs */
    void do_initdialog1()
    {
        RECT rc, trc;
        int treewid;
        int margin;

        /* get the dialog area */
        GetClientRect(hwnd_, &rc);

        /*
         *   Get the tab control.  Note that we have to ask for the control
         *   by ID - can't use PropSheet_GetTabControl(), since 'hwnd_' is
         *   the overall property frame handle rather than the handle to an
         *   individual sheet.
         */
        HWND tabctl = GetDlgItem(hwnd_, IDC_PROPSHEET_TAB);

        /* get the margin - its distance from the left edge of the dialog */
        GetWindowRect(tabctl, &trc);
        MapWindowPoints(HWND_DESKTOP, hwnd_, (POINT *)&trc, 2);
        margin = trc.left;

        /* set it to single-line style */
        LONG_PTR style = GetWindowLongPtr(tabctl, GWL_STYLE);
        style &= ~TCS_MULTILINE;
        style |= TCS_SINGLELINE;
        SetWindowLongPtr(tabctl, GWL_STYLE, style);

        /* hide it */
        ShowWindow(tabctl, SW_HIDE);
        EnableWindow(tabctl, FALSE);

        /*
         *   get the height of an ordinary single-line tab control
         */

        /* create the control */
        GetWindowRect(hwnd_, &rc);
        HWND newtab = CreateWindow(
            WC_TABCONTROL, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            0, 0, rc.right - rc.left, 1, hwnd_, 0,
            CTadsApp::get_app()->get_instance(), 0);

        /* add a dummy item, and measure its height */
        TCITEM tcitem;
        tcitem.mask = TCIF_TEXT;
        tcitem.pszText = "Test";
        TabCtrl_InsertItem(newtab, 0, &tcitem);
        TabCtrl_GetItemRect(newtab, 0, &trc);
        tabht_ = trc.bottom - trc.top;

        /* done with it - destroy it */
        DestroyWindow(newtab);

        /*
         *   set up the tree view
         */
        tv_ = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_TREEVIEW, "",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TVS_DISABLEDRAGDROP
            | TVS_SHOWSELALWAYS,
            0, 0, 1, rc.bottom - rc.top, hwnd_, 0,
            CTadsApp::get_app()->get_instance(), 0);

        /*
         *   make sure the tree gets the word on our format now that we know
         *   its handle (it probably asked us already from within
         *   CreateWindowEx, but since we can't stash away the treeview
         *   handle until CreateWindowEx returns, we won't have been able to
         *   answer that earlier query)
         */
        SendMessage(tv_, WM_NOTIFYFORMAT, (WPARAM)hwnd_, NF_REQUERY);

        /* add the image list for the selected item */
        tvil_ = ImageList_Create(16, 16, ILC_COLOR | ILC_MASK, 5, 5);
        HBITMAP bmp = LoadBitmap(CTadsApp::get_app()->get_instance(),
                                 MAKEINTRESOURCE(IDB_PROPSHEETTREE));
        ImageList_AddMasked(tvil_, bmp, RGB(0x00,0xff,0x00));
        TreeView_SetImageList(tv_, tvil_, TVSIL_STATE);

        /* don't indent children; the state image is enough indentation */
        TreeView_SetIndent(tv_, 0);

        /* start out at zero width */
        treewid = 0;

        /* we don't have a hierarchical parent yet */
        HTREEITEM hipar = 0;
        char parname[128] = "";

        /* populate it with the tabs */
        for (UINT i = 0 ; i < psh_->nPages ; ++i)
        {
            TVINSERTSTRUCT tvi;
            HTREEITEM hi;
            RECT irc;
            char buf[256];
            char *p;

            /* get the title - if it's a resource string, read it in */
            if ((DWORD)psh_->ppsp[i].pszTitle <= 0xffff)
            {
                /* get the string */
                LoadString(CTadsApp::get_app()->get_instance(),
                           (UINT)psh_->ppsp[i].pszTitle, buf, sizeof(buf));
            }
            else
            {
                /* it's a real string - copy it */
                strcpy(buf, psh_->ppsp[i].pszTitle);
            }

            /* check for a hierarchical name */
            if ((p = strchr(buf, '\\')) != 0)
            {
                /*
                 *   null-terminate at the separator, and point to the child
                 *   name - we'll use that as the actual display name
                 */
                *p++ = '\0';

                /*
                 *   if this isn't a child of the current parent, we need to
                 *   create a new parent
                 */
                if (hipar == 0 || stricmp(buf, parname) != 0)
                {
                    /* remember the new parent name */
                    strcpy(parname, buf);

                    /* insert the new parent */
                    tvi.hParent = 0;
                    tvi.hInsertAfter = TVI_LAST;
                    tvi.item.mask = TVIF_TEXT;
                    tvi.item.pszText = buf;
                    hipar = TreeView_InsertItem(tv_, &tvi);
                }
            }
            else
            {
                /* it's a non-hierarchical item, so forget any parent */
                hipar = 0;

                /* use the whole buffer as the item name */
                p = buf;
            }

            /*
             *   Add the item.  Use the property sheet's dialog ID as the
             *   item lParam - we'll use this to activate the corresponding
             *   page when the user selects this tree item.
             */
            tvi.hParent = hipar;
            tvi.hInsertAfter = TVI_LAST;
            tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
            tvi.item.lParam = (LPARAM)psh_->ppsp[i].pszTemplate;
            tvi.item.pszText = p;
            tvi.item.state = INDEXTOSTATEIMAGEMASK(2);
            tvi.item.stateMask = TVIS_STATEIMAGEMASK;
            hi = TreeView_InsertItem(tv_, &tvi);

            /* make sure the parent is expanded */
            if (hipar != 0)
                TreeView_Expand(tv_, hipar, TVE_EXPAND);

            /* get the item rectangle */
            TreeView_GetItemRect(tv_, hi, &irc, TRUE);

            /* make sure our tree width is wide enough for this item */
            if (irc.right > treewid)
                treewid = irc.right;
        }

        /*
         *   make sure there's room for a scrollbar, and a little extra space
         *   to prevent crowding
         */
        treewid += GetSystemMetrics(SM_CXVSCROLL) + 8;

        /*
         *   make horizontal room for our tree control and its margin, and
         *   remove the tab control's vertical space
         */
        GetWindowRect(hwnd_, &rc);
        SetWindowPos(hwnd_, 0, rc.left - (treewid + margin)/2, rc.top,
                     rc.right - rc.left + treewid + margin,
                     rc.bottom - rc.top - tabht_,
                     SWP_NOACTIVATE);

        /* resize the tree to the calculated width */
        MoveWindow(tv_, margin, 0, treewid, rc.bottom - rc.top, FALSE);
    }

    /* dialog initialization - second phase, after the system sets up */
    void do_initdialog2()
    {
        RECT rc;
        int treewid;
        int margin;

        /* get the tree control's size */
        GetWindowRect(tv_, &rc);
        MapWindowPoints(HWND_DESKTOP, hwnd_, (POINT *)&rc, 2);
        treewid = rc.right - rc.left;
        margin = rc.left;

        /* get our new client area in screen coordinates */
        GetClientRect(hwnd_, &rc);
        MapWindowPoints(hwnd_, HWND_DESKTOP, (POINT *)&rc, 2);

        /* move our children to adjust for the added tree and deleted tab */
        for (HWND chi = GetWindow(hwnd_, GW_CHILD) ; chi != 0 ;
             chi = GetWindow(chi, GW_HWNDNEXT))
        {
            RECT crc;

            /* skip the tree, for obvious reasons */
            if (chi == tv_)
                continue;

            /* move this window */
            GetWindowRect(chi, &crc);
            MoveWindow(chi,
                       crc.left - rc.left + treewid + margin,
                       crc.top - rc.top - tabht_,
                       crc.right - crc.left, crc.bottom - crc.top, TRUE);

            /* if this is the property page, position the tree alongside it */
            char clsname[64];
            GetClassName(chi, clsname, sizeof(clsname));
            if (strcmp(clsname, "#32770") == 0)
            {
                RECT pprc;

                /* get the page's relative location */
                GetWindowRect(chi, &pprc);
                MapWindowPoints(HWND_DESKTOP, hwnd_, (POINT *)&pprc, 2);

                /* align the tree with this vertically */
                MoveWindow(tv_, margin, pprc.top,
                           treewid, pprc.bottom - pprc.top, TRUE);
            }
        }

        /*
         *   sync the tree with the current page - note that we might have
         *   selected something other than the first page via nStartPage or
         *   pStartPage in the PROPSHEETHEADER structure
         */
        sync_tree(TRUE);
    }

    /* handle a notification message */
    int do_notify(int ctlid, LPNMHDR nm)
    {
        /* check the message */
        switch (nm->code)
        {
        case TVN_SELCHANGED:
            /* tree selection changed - handle it if it's our tree control */
            if (nm->hwndFrom == tv_)
            {
                /* handle the change */
                do_tree_select((LPNMTREEVIEW)nm);

                /* handled */
                return TRUE;
            }
            break;
        }

        /* not ours */
        return FALSE;
    }

    /* sync the tree with the currently selected page */
    void sync_tree(int force)
    {
        /* get the current tree selection */
        HTREEITEM ti = get_tv_leaf_item(TreeView_GetSelection(tv_));

        /* get its LPARAM - this is the page ID of the associated page */
        TVITEM tvi;
        tvi.mask = TVIF_PARAM;
        tvi.hItem = ti;
        TreeView_GetItem(tv_, &tvi);

        /* get the active page's ID */
        int page_id = PropSheet_IndexToId(
            hwnd_, TabCtrl_GetCurSel(PropSheet_GetTabControl(hwnd_)));

        /* if it doesn't match, select the tree item for this page */
        if (force || page_id != (int)tvi.lParam)
            TreeView_Select(tv_, find_tree_item_for_page_id(page_id),
                            TVGN_CARET);
    }

    /* search the tree for the item with the given page ID */
    HTREEITEM find_tree_item_for_page_id(int id)
        {  return find_tree_item_for_page_id(id, TreeView_GetRoot(tv_)); }

    /* search an item's siblings for the given page ID */
    HTREEITEM find_tree_item_for_page_id(int id, HTREEITEM item)
    {
        /* scan this item and its siblings */
        for ( ; item != 0 ; item = TreeView_GetNextSibling(tv_, item))
        {
            /* get the item's lparam - this is the page ID */
            TVITEM tvi;
            tvi.mask = TVIF_PARAM;
            tvi.hItem = item;
            TreeView_GetItem(tv_, &tvi);

            /* if the page ID matches, this is the item we're looking for */
            if (tvi.lParam == (LPARAM)id)
                return item;

            /* if this item has children, recursively search the children */
            HTREEITEM chi = TreeView_GetChild(tv_, item);
            if (chi != 0 && (chi = find_tree_item_for_page_id(id, chi)) != 0)
                return chi;
        }

        /* didn't find it */
        return 0;
    }

    /* carry out side effects of udpating the selection */
    void do_tree_select(LPNMTREEVIEW nm)
    {
        /* if there's a previous selection, remove its activated state */
        if (nm->itemOld.hItem != 0)
            activate_tv_item(get_tv_leaf_item(nm->itemOld.hItem), FALSE);

        /* if there's a new item, activate it and select its page */
        if (nm->itemNew.hItem != 0)
        {
            TVITEM tvi;

            /* get the leaf item */
            HTREEITEM item = get_tv_leaf_item(nm->itemNew.hItem);

            /* activate it */
            activate_tv_item(item, TRUE);

            /* select its page; the lParam is the page's resource ID */
            tvi.mask = TVIF_PARAM;
            tvi.hItem = item;
            TreeView_GetItem(tv_, &tvi);
            SendMessage(hwnd_, PSM_SETCURSELID, 0, tvi.lParam);
        }
    }

    /*
     *   Activate or deactivate an item.  This sets the item's state image to
     *   the appropriate activation state.
     */
    void activate_tv_item(HTREEITEM item, int active)
    {
        TVITEM tvi;

        /* set the item's state image to 2 (inactive) or 1 (active) */
        tvi.hItem = item;
        tvi.mask = TVIF_HANDLE | TVIF_STATE;
        tvi.state = INDEXTOSTATEIMAGEMASK(active ? 1 : 2);
        tvi.stateMask = TVIS_STATEIMAGEMASK;
        TreeView_SetItem(tv_, &tvi);
    }

    /*
     *   Get the leaf node for a given item.  This returns the first child or
     *   grandchild of the item that has no children of its own.  Leaf nodes
     *   are the only nodes associated with property pages, so these are the
     *   only nodes that we can actually activate.  However, we need to let
     *   the user select parent nodes through the treeview to control the
     *   expanded/collapsed states, so we have separate notions of activated
     *   and selected.  The activated item is always either the selected item
     *   (if it's a leaf node) or the first leaf-node child of the selected
     *   item (if the selected item is a parent).
     */
    HTREEITEM get_tv_leaf_item(HTREEITEM item)
    {
        HTREEITEM chi;

        /* look for the first child without children of its own */
        while ((chi = TreeView_GetChild(tv_, item)) != 0)
            item = chi;

        /* return the item */
        return item;
    }

    /* the property sheet descriptor */
    PROPSHEETHEADER *psh_;

    /* our tree-view control, which substitutes for the normal tab control */
    HWND tv_;
    HIMAGELIST tvil_;

    /* height of a single-line tab control */
    int tabht_;
};

/*
 *   Run a tree-style property sheet dialog
 */
void CTadsDialog::run_tree_propsheet(HWND owner, PROPSHEETHEADER *psh)
{
    /* disable the owner's subwindows before showing the dialog */
    void *ctx = CTadsDialog::modal_dlg_pre(owner, FALSE);

    /* hook the dialog creation */
    TreePropSheetHook hook(psh);
    hook.set_hook();

    /* run the property sheet */
    PropertySheet(psh);

    /* re-enable windows */
    CTadsDialog::modal_dlg_post(ctx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize a PROPSHEETPAGE structure for this page
 */
void CTadsDialogPropPage::init_ps_page(HINSTANCE app_inst,
                                       PROPSHEETPAGE *psp, int dlg_res_id,
                                       int caption_str_id,
                                       int init_page_id, int init_ctl_id,
                                       PROPSHEETHEADER *psh)
{
    /* set up the structure */
    psp->dwSize = sizeof(PROPSHEETPAGE);
    psp->dwFlags = PSP_USETITLE;
    psp->hInstance = app_inst;
    psp->pszTemplate = MAKEINTRESOURCE(dlg_res_id);
    psp->pszIcon = 0;
    psp->pszTitle = MAKEINTRESOURCE(caption_str_id);

    /* hook ourselves up to the PROPSHEETPAGE */
    prepare_prop_page(psp);

    /* if this is the initial page, set its initial control */
    if (init_page_id == dlg_res_id)
    {
        /* set the initial control in the dialog */
        if (init_ctl_id != 0)
            set_init_ctl(init_ctl_id);

        /* set this as the initial page in the header */
        psh->pStartPage = MAKEINTRESOURCE(caption_str_id);
        psh->dwFlags |= PSH_USEPSTARTPAGE;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Owner-drawn button implementation
 */

/*
 *   Draw the button.  This draws the frame structure of the button.
 *   Normally, a subclass's draw method should inherit the default to draw
 *   the frame, then fill in the contents of the face.
 */
void CTadsOwnerDrawnBtn::draw(int id, DRAWITEMSTRUCT *dis)
{
    HPEN shadow_pen =
        CreatePen(PS_SOLID, 2, GetSysColor(COLOR_3DSHADOW));
    HPEN oldpen;
    int ofs;

    FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_3DFACE));

    /*
     *   if focused, draw a white edge on the left and top inside the
     *   black edge; if clicked, draw the edge in shadow color instead
     */
    if (dis->itemState & ODS_FOCUS)
    {
        SelectObject(dis->hDC, (dis->itemState & ODS_SELECTED) == 0 ?
                     GetStockObject(WHITE_PEN) : shadow_pen);
        MoveToEx(dis->hDC, dis->rcItem.left + 1,
                 dis->rcItem.bottom - 1, 0);
        LineTo(dis->hDC, dis->rcItem.left + 1, dis->rcItem.top + 1);
        LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top + 1);
    }

    /*
     *   draw the left and top outside edges - white if not focused, black
     *   if focused
     */
    oldpen = (HPEN)SelectObject(dis->hDC, GetStockObject(
        (dis->itemState & ODS_FOCUS) == 0 ? WHITE_PEN : BLACK_PEN));
    MoveToEx(dis->hDC, dis->rcItem.left, dis->rcItem.bottom - 1, 0);
    LineTo(dis->hDC, dis->rcItem.left, dis->rcItem.top);
    LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top);

    /*
     *   draw the right and bottom inside shadows - move these down and
     *   right slightly if the button is clicked
     */
    ofs = (dis->itemState & ODS_SELECTED)
          ? 0
          : (dis->itemState & ODS_FOCUS) ? -1 : 0;
    SelectObject(dis->hDC, shadow_pen);
    MoveToEx(dis->hDC, dis->rcItem.right - 1 + ofs,
             dis->rcItem.top + 2, 0);
    LineTo(dis->hDC, dis->rcItem.right - 1 + ofs,
           dis->rcItem.bottom - 1 + ofs);
    LineTo(dis->hDC, dis->rcItem.left + 2,
           dis->rcItem.bottom - 1 + ofs);

    /*
     *   if we have focus and we're not selected, draw another black line
     *   inside the right and bottom edges
     */
    if (!(dis->itemState & ODS_SELECTED)
        && (dis->itemState & ODS_FOCUS))
    {
        SelectObject(dis->hDC, GetStockObject(BLACK_PEN));
        MoveToEx(dis->hDC, dis->rcItem.right - 2,
                 dis->rcItem.top, 0);
        LineTo(dis->hDC, dis->rcItem.right - 2,
               dis->rcItem.bottom - 2);
        LineTo(dis->hDC, dis->rcItem.left,
               dis->rcItem.bottom - 2);
    }

    /* draw a black edge on the right and bottom */
    SelectObject(dis->hDC, GetStockObject(BLACK_PEN));
    MoveToEx(dis->hDC, dis->rcItem.right - 1, dis->rcItem.top, 0);
    LineTo(dis->hDC, dis->rcItem.right - 1, dis->rcItem.bottom - 1);
    LineTo(dis->hDC, dis->rcItem.left, dis->rcItem.bottom - 1);

    SelectObject(dis->hDC, oldpen);
    DeleteObject(shadow_pen);
}

/* ------------------------------------------------------------------------ */
/*
 *   Owner-drawn simple combo
 */

/*
 *   measure for drawing
 */
void CTadsOwnerDrawnCombo::measure(int id, MEASUREITEMSTRUCT *mis)
{
    HDC dc;
    TEXTMETRIC tm;

    /* get a display DC */
    dc = GetWindowDC(0);

    /* the default font for combos is the standard GUI font */
    HFONT oldfont = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

    /* measure the height of a line of text */
    GetTextMetrics(dc, &tm);
    mis->itemHeight = tm.tmHeight + 2;

    /* clean up */
    SelectObject(dc, oldfont);
    ReleaseDC(0, dc);
}

/*
 *   draw
 */
void CTadsOwnerDrawnCombo::draw(int id, DRAWITEMSTRUCT *dis)
{
    HDC dc = dis->hDC;
    HWND ctl = dis->hwndItem;
    int idx = dis->itemID;
    char *buf;
    int oldbkmode = SetBkMode(dc, TRANSPARENT);
    int oldtxtclr = SetTextColor(
        dc, GetSysColor((dis->itemState & ODS_SELECTED)
                        ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    HFONT oldfont = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

    /* fill the background */
    FillRect(dc, &dis->rcItem,
             GetSysColorBrush(dis->itemState & ODS_SELECTED
                              ? COLOR_HIGHLIGHT : COLOR_WINDOW));

    /* retrieve the item title string */
    buf = new char[SendMessage(ctl, CB_GETLBTEXTLEN, idx, 0) + 1];
    SendMessage(ctl, CB_GETLBTEXT, idx, (LPARAM)buf);

    /* do the custom drawing of the contents */
    draw_contents(dc, &dis->rcItem, buf, dis);

    /* if we have focus, add a focus rectangle */
    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dis->hDC, &dis->rcItem);

    /* clean up */
    SelectObject(dc, oldfont);
    SetTextColor(dc, oldtxtclr);
    SetBkMode(dc, oldbkmode);
    delete [] buf;
}


