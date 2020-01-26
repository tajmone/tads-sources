#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadscbtn.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/*
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadscbtn.cpp - color button implementation
Function

Notes

Modified
  03/14/98 MJRoberts  - Creation
*/

#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSDLG_H
#include "tadsdlg.h"
#endif
#ifndef TADSCBTN_H
#include "tadscbtn.h"
#endif
#ifndef TADSAPP_H
#include "tadsapp.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Color button implementation
 */

/*
 *   process a command in the color button: shows the color picker dialog,
 *   and changes the button's face color if the user selects a new color
 */
void CColorBtn::do_command(int code)
{
    CHOOSECOLOR cc;

    /* set up the dialog with our current color as the default */
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = GetParent(gethdl());
    cc.hInstance = 0;
    cc.rgbResult = get_color();
    cc.lpCustColors = cust_colors_;
    cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;
    cc.lCustData = 0;
    cc.lpfnHook = 0;
    cc.lpTemplateName = 0;

    /* show the color dialog */
    if (ChooseColor(&cc))
    {
        /* user selected OK - select the new color */
        set_color(cc.rgbResult);
    }
}

/*
 *   draw a color button
 */
void CColorBtn::draw(int id, DRAWITEMSTRUCT *dis)
{
    RECT rc;

    /* do the default button drawing first */
    CTadsOwnerDrawnBtn::draw(id, dis);

    /* if it's not disabled, draw the color */
    if (!(dis->itemState & ODS_DISABLED))
    {
        HBRUSH br = CreateSolidBrush(get_color());

        /* draw the color area; offset it slightly if clicked */
        rc = dis->rcItem;
        InflateRect(&rc, -6, -5);
        if (dis->itemState & ODS_SELECTED)
            OffsetRect(&rc, 1, 1);
        FillRect(dis->hDC, &rc, br);

        /* draw the focus rectangle if we have focus */
        if (dis->itemState & ODS_FOCUS)
        {
            InflateRect(&rc, 1, 1);
            DrawFocusRect(dis->hDC, &rc);
        }

        /* done with the brush */
        DeleteObject(br);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Property page color button implementation
 */

/*
 *   process the command
 */
void CColorBtnPropPage::do_command(int code)
{
    HTML_color_t old_color;

    /* note the old color */
    old_color = *color_;

    /* process the command normally */
    CColorBtn::do_command(code);

    /* if the color changed, notify the property sheet */
    if (*color_ != old_color)
        page_->set_changed(TRUE);
}

/* ------------------------------------------------------------------------ */
/*
 *   Color combo
 */

/*
 *   process a command in the color button
 */
void CColorCombo::do_command(int code)
{
    LPARAM l;
    int idx;

    switch (code)
    {
    case CBN_SELCHANGE:
        /* check the item data */
        idx = SendMessage(gethdl(), CB_GETCURSEL, 0, 0);
        l = (LPARAM)SendMessage(gethdl(), CB_GETITEMDATA, idx, 0);

        /* if it's 0xFFFFFFFE, it's the "other..." item */
        if (l == 0xFFFFFFFE)
        {
            CHOOSECOLOR cc;

            /* set up the dialog with our current color as the default */
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = GetParent(gethdl());
            cc.hInstance = 0;
            cc.rgbResult = get_color();
            cc.lpCustColors = cust_colors_;
            cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;
            cc.lCustData = 0;
            cc.lpfnHook = 0;
            cc.lpTemplateName = 0;

            /* show the color dialog */
            if (ChooseColor(&cc))
            {
                /* user selected OK - select the new color */
                set_color(cc.rgbResult);

                /* make sure it's in the list as a custom color */
                add_custom_color(cc.rgbResult, TRUE);
            }
            else
            {
                /* select the previously selected color */
                select_by_color();
            }
        }
        else
        {
            /* store the current color in our dialog variable */
            *color_ = COLORREF_to_HTML_color((COLORREF)l);
        }
        break;
    }
}

/*
 *   draw
 */
void CColorCombo::draw_contents(HDC dc, RECT *item_rc, textchar_t *title,
                                DRAWITEMSTRUCT *dis)
{
    RECT rc;

    /* get the color for this item - it's given by the item data */
    COLORREF clr = (COLORREF)SendMessage(
        dis->hwndItem, CB_GETITEMDATA, dis->itemID, 0);

    /* 0xFFFFFFFF means to use the default color */
    if (clr == 0xFFFFFFFF)
        clr = HTML_color_to_COLORREF(*default_color_);

    /* set up the color square area */
    rc = *item_rc;
    InflateRect(&rc, -3, -1);
    rc.right = rc.left + (rc.bottom - rc.top);

    /*
     *   draw the color square, UNLESS the color code is 0xFFFFFFFE - this is
     *   the special item to bring up the custom color dialog
     */
    if (clr != 0xFFFFFFFE)
    {
        /* create the brush */
        HBRUSH br = CreateSolidBrush(clr);

        /* draw the square */
        FillRect(dc, &rc, br);
        FrameRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        /* done with the brush */
        DeleteObject(br);
    }

    /* draw the title string */
    rc.left = rc.right + 4;
    rc.right = item_rc->right;
    DrawText(dc, title, -1, &rc, DT_SINGLELINE);
}

