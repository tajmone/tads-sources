/* $Header: d:/cvsroot/tads/html/win32/tadscbtn.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscbtn.h - color selector buttons
Function

Notes

Modified
  03/14/98 MJRoberts  - Creation
*/

#ifndef TADSCBTN_H
#define TADSCBTN_H

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Color selector button.  This is a specialization of the owner-drawn
 *   button class that displays a color on its face, and lets the user
 *   select a color (via the standard color picker dialog) when the button
 *   is clicked.
 */

class CColorBtn: public CTadsOwnerDrawnBtn
{
public:
    CColorBtn(HWND hdl, HTML_color_t *color, COLORREF *cust_colors)
        : CTadsOwnerDrawnBtn(hdl)
        { color_ = color; cust_colors_  = cust_colors; }
    CColorBtn(HWND dlg, int id, HTML_color_t *color, COLORREF *cust_colors)
        : CTadsOwnerDrawnBtn(dlg, id)
        { color_ = color; cust_colors_ = cust_colors;}

    virtual void draw(int, DRAWITEMSTRUCT *dis);
    virtual void do_command(int code);

    COLORREF get_color() { return HTML_color_to_COLORREF(*color_); }
    void set_color(COLORREF color)
    {
        *color_ = COLORREF_to_HTML_color(color);
        InvalidateRect(gethdl(), 0, TRUE);
    }

protected:
    HTML_color_t *color_;
    COLORREF *cust_colors_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Specialized version of color button for property sheet page use
 */

class CColorBtnPropPage: public CColorBtn
{
public:
    CColorBtnPropPage(HWND dlg, int id, HTML_color_t *color,
                      COLORREF *cust_colors, class CTadsDialogPropPage *page)
        : CColorBtn(dlg, id, color, cust_colors)
    { page_ = page; }

    void do_command(int code);

private:
    /* property sheet page containing the button */
    class CTadsDialogPropPage *page_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Color selector combo.  This is a special owner-drawn combo box that
 *   shows a list of colors.
 */
class CColorCombo: public CTadsOwnerDrawnCombo
{
public:
    CColorCombo(HWND hdl, HTML_color_t *color, HTML_color_t *default_color,
                COLORREF *cust_colors)
        : CTadsOwnerDrawnCombo(hdl)
    {
        color_ = color;
        default_color_ = default_color;
        cust_colors_  = cust_colors;
    }
    CColorCombo(HWND dlg, int id, HTML_color_t *color,
                HTML_color_t *default_color, COLORREF *cust_colors)
        : CTadsOwnerDrawnCombo(dlg, id)
    {
        color_ = color;
        default_color_ = default_color;
        cust_colors_ = cust_colors;
    }

    virtual void draw_contents(HDC dc, RECT *rc, textchar_t *title,
                               DRAWITEMSTRUCT *dis);

    virtual void do_command(int code);

    COLORREF get_color() { return HTML_color_to_COLORREF(*color_); }
    void set_color(COLORREF color)
    {
        /* set the color */
        *color_ = COLORREF_to_HTML_color(color);

        /* make sure we redraw */
        InvalidateRect(gethdl(), 0, TRUE);
    }

    /* add a custom color if necessary */
    void add_custom_color(COLORREF color, int select)
    {
        int i, cnt;

        /* scan for a match to the color */
        cnt = SendMessage(gethdl(), CB_GETCOUNT, 0, 0);
        for (i = 1 ; i < cnt ; ++i)
        {
            /* if this is a match, we're set */
            if ((COLORREF)SendMessage(gethdl(), CB_GETITEMDATA, i, 0)
                == color)
            {
                /* if desired, select it */
                if (select)
                    SendMessage(gethdl(), CB_SETCURSEL, i, 0);

                /* done */
                return;
            }
        }

        /*
         *   didn't find it - add a new custom entry in second-to-last
         *   position (just before the "Others..." element)
         */
        i = SendMessage(gethdl(), CB_INSERTSTRING, cnt - 1, (LPARAM)"Custom");
        SendMessage(gethdl(), CB_SETITEMDATA, i, (LPARAM)color);

        /* if desired, select it */
        if (select)
            SendMessage(gethdl(), CB_SETCURSEL, i, 0);
    }

    /* select by color */
    void select_by_color()
    {
        int i, cnt;
        COLORREF cr = HTML_color_to_COLORREF(*color_);

        /* if it's 0xFFFFFFFF, it means 'automatic' */
        if (*color_ == 0xFFFFFFFF)
        {
            SendMessage(gethdl(), CB_SETCURSEL, 0, 0);
            return;
        }

        /* scan for a match to the color */
        cnt = SendMessage(gethdl(), CB_GETCOUNT, 0, 0);
        for (i = 1 ; i < cnt ; ++i)
        {
            /* if this is a match, we're set */
            if ((COLORREF)SendMessage(gethdl(), CB_GETITEMDATA, i, 0) == cr)
            {
                SendMessage(gethdl(), CB_SETCURSEL, i, 0);
                return;
            }
        }
    }

protected:
    HTML_color_t *color_;
    HTML_color_t *default_color_;
    COLORREF *cust_colors_;
};

/*
 *   Property-page version of color combo
 */
class CColorComboPropPage: public CColorCombo
{
public:
    CColorComboPropPage(
        HWND dlg, int id, HTML_color_t *color, HTML_color_t *default_color,
        COLORREF *cust_colors, class CTadsDialogPropPage *page)
        : CColorCombo(dlg, id, color, default_color, cust_colors)
    {
        /* remember my page */
        page_ = page;

        /*
         *   make sure it's in the active dialog list - we need it to be in
         *   the list in advance of the WM_INITDIALOG message, so that we can
         *   initialize our owner-drawn combo
         */
        page_->add_to_active();
    }

private:
    /* property sheet page containing the button */
    class CTadsDialogPropPage *page_;

};

#endif /* TADSCBTN_H */
