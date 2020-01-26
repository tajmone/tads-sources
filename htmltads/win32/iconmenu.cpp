/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  iconmenu.cpp - menu-with-icons add-in
Function
  This is a little add-in class that lets you add an icon to menu items.
Notes

Modified
  10/02/06 MJRoberts  - Creation
*/

#include <Windows.h>
#include <commctrl.h>

#include <stdlib.h>
#include <stddef.h>

#include "iconmenu.h"
#include "htmlhash.h"
#include "tadswin.h"


/* ------------------------------------------------------------------------ */
/*
 *   Hash function.  Our keys are integers rather than strings, but we pass
 *   them in as arrays of bytes anyway since that's the interface.
 */
class MenuHashFunc: public CHtmlHashFunc
{
public:
    virtual unsigned int compute_hash(const textchar_t *str, size_t len)
    {
        unsigned char *p = (unsigned char *)str;
        unsigned int a = 63689;
        const unsigned int b = 378551;
        unsigned int hash;

        for (hash = 0 ; len != 0 ; ++p, --len)
        {
            hash = hash * a + (*str);
            a *= b;
        }

        return (hash & 0x7FFFFFFF);
    }
};

/*
 *   Our hash table entry
 */
class MenuHashEntry: public CHtmlHashEntry
{
public:
    /*
     *   construct: force our integer key to be treated as a string for
     *   conformance to the base class; we'll just hash the bytes of the
     *   native integer representation as though they were a string
     */
    MenuHashEntry(int command_id, int icon_index)
        : CHtmlHashEntry((const textchar_t *)&command_id,
                         sizeof(command_id), FALSE)
    {
        /* remember the command and icon index values */
        command_id_ = command_id;
        icon_idx_ = icon_index;

        /* the command_id_ is the key, so point our string to it */
        str_ = (textchar_t *)&command_id_;
        len_ = sizeof(command_id_);
    }

    /* do we match the given entry? */
    virtual int matches(const textchar_t *str, size_t len)
    {
        return len == sizeof(command_id_) && *(int *)str == command_id_;
    }

    /* our command ID value */
    int command_id_;

    /* the icon index value */
    int icon_idx_;
};


/* ------------------------------------------------------------------------ */
/*
 *   create
 */
IconMenuHandler::IconMenuHandler(int icon_width, int icon_height)
{
    /* remember the parent window and icon dimensions */
    icon_width_ = icon_width;
    icon_height_ = icon_height;

    /* set up our hash table for the command-to-icon mapping */
    hash_ = new CHtmlHashTable(256, new MenuHashFunc());

    /* create our image list */
    himl_ = ImageList_Create(icon_width, icon_height,
                             ILC_COLORDDB | ILC_MASK, 32, 16);
}


/* ------------------------------------------------------------------------ */
/*
 *   destroy
 */
IconMenuHandler::~IconMenuHandler()
{
    /* delete our resources */
    delete hash_;
    ImageList_Destroy(himl_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Add a image from a bitmap resource.  The bitmap is treated as an array
 *   of images of the size specified when the instance was created.
 */
int IconMenuHandler::add_bitmap(HINSTANCE inst, int res_id)
{
    HDC dc;
    HBITMAP bmp;
    HBITMAP old_bmp;
    COLORREF mask_color;
    int idx;

    /* load the bitmap */
    bmp = LoadBitmap(inst, MAKEINTRESOURCE(res_id));
    if (bmp == 0)
        return -1;

    /* get the top left pixel of the bitmap, to use as the mask color */
    dc = CreateCompatibleDC(0);
    old_bmp = (HBITMAP)SelectObject(dc, bmp);
    mask_color = GetPixel(dc, 0, 0);

    /* release our temporary DC */
    SelectObject(dc, old_bmp);
    DeleteDC(dc);

    /* add the images from the bitmap */
    idx = ImageList_AddMasked(himl_, bmp, mask_color);

    /*
     *   the image list makes its own copy of the bitmap data, so we can
     *   delete our copy of the bitmap resource now
     */
    DeleteObject(bmp);

    /* return the base index */
    return idx;
}


/* ------------------------------------------------------------------------ */
/*
 *   Associate a command ID with an icon index.  When the menu is drawn, any
 *   items with this command ID will be drawn with the bitmap at the given
 *   index displayed to the left of the menu title.
 */
void IconMenuHandler::map_command(int command_id, int icon_index)
{
    MenuHashEntry *entry;

    /* look for an existing entry first */
    entry = find_hash_entry(command_id);

    /*
     *   if we found an existing entry, replace its icon index with the new
     *   mapping; otherwise, create and add a new entry
     */
    if (entry != 0)
        entry->icon_idx_ = icon_index;
    else
        hash_->add(new MenuHashEntry(command_id, icon_index));
}

/*
 *   Map commands to match toolbar associations
 */
void IconMenuHandler::map_commands(const TBBUTTON *buttons, int num_buttons,
                                   int base_icon_idx)
{
    /* apply each mapping in the toolbar button list */
    for ( ; num_buttons != 0 ; ++buttons, --num_buttons)
    {
        /* if we have a bitmap and a command, apply the mapping */
        if ((buttons->fsStyle & TBSTYLE_SEP) == 0
            && buttons->idCommand != 0
            && buttons->iBitmap != -1)
        {
            /* this has a valid mapping - add it */
            map_command(buttons->idCommand, buttons->iBitmap + base_icon_idx);
        }
    }
}


/*
 *   Unmap a command
 */
void IconMenuHandler::unmap_command(int command_id)
{
    MenuHashEntry *entry;

    /* look for an existing entry first */
    entry = find_hash_entry(command_id);

    /* if it exists, remove it */
    if (entry != 0)
    {
        /* remove it from the table, and delete the entry */
        hash_->remove(entry);
        delete entry;
    }
}



/* ------------------------------------------------------------------------ */
/*
 *   Initialize a menu.  This should be called to handle a WM_INITMENUPOPUP
 *   message for one of our menus.  We'll convert the menu's items to have
 *   owner-drawn bitmaps.
 */
void IconMenuHandler::imh_init_menu_popup(HMENU menu, unsigned int pos,
                                          int sysmenu)
{
    UINT i, cnt;
    int updated = FALSE;
    int has_bmp = FALSE;
    int has_bmp_and_check = FALSE;
    MENUINFO mi;

    /*
     *   run through the menu and mark each item as having an owner-drawn
     *   bitmap
     */
    for (i = 0, cnt = GetMenuItemCount(menu) ; i < cnt ; ++i)
    {
        UINT cmd;
        MenuHashEntry *entry;

        /* get this menu item's command */
        cmd = GetMenuItemID(menu, i);

        /* if the ID is -1, it means this menu has submenus; skip these */
        if (cmd == -1)
            continue;

        /* if there's an icon for this menu, set this item's bitmap */
        if ((entry = find_hash_entry(cmd)) != 0)
        {
            MENUITEMINFO mii;

            /* note that we have an item with an icon */
            has_bmp = TRUE;

            /* set the image to use a callback */
            memset(&mii, 0, sizeof(mii));
            mii.cbSize = CTadsWin::menuiteminfo_size_;
            mii.fMask = MIIM_BITMAP;
            mii.hbmpItem = HBMMENU_CALLBACK;
            SetMenuItemInfo(menu, i, TRUE, &mii);

            /* note the update */
            updated = TRUE;

            /* note if we have a checkmark in addition to the bitmap */
            if ((GetMenuState(menu, i, MF_BYPOSITION) & MF_CHECKED) != 0)
                has_bmp_and_check = TRUE;
        }
    }

    /*
     *   if we updated any items, set the menu style according to how much
     *   space we need: if we have any items that have both a bitmap and a
     *   checkmark, we'll need to set those specially
     */
    if (updated)
    {
        /* get the current style information */
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_STYLE;
        GetMenuInfo(menu, &mi);

        /* set the new style according to what we found */
        if (has_bmp_and_check)
            mi.dwStyle &= ~MNS_CHECKORBMP;
        else if (has_bmp)
            mi.dwStyle |= MNS_CHECKORBMP;

        SetMenuInfo(menu, &mi);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   find the hash entry for a given command ID
 */
class MenuHashEntry *IconMenuHandler::find_hash_entry(int command_id)
{
    /*
     *   Look up the entry and return what we find.  The command ID is the
     *   key; we treat the native bytes of the integer as a string for the
     *   purposes of the hash lookup.
     */
    return (MenuHashEntry *)hash_->find((const textchar_t *)&command_id,
                                        sizeof(command_id));
}

/* ------------------------------------------------------------------------ */
/*
 *   handle owner-draw measurement
 */
void IconMenuHandler::imh_measure_item(int ctl_id, MEASUREITEMSTRUCT *mi)
{
    /* if this isn't a menu measurement, it's not ours */
    if (mi->CtlType != ODT_MENU)
        return;

    /* we need space for the icon, plus some margin space */
    mi->itemWidth = icon_width_ + 4;
    mi->itemHeight = icon_height_;
}


/* ------------------------------------------------------------------------ */
/*
 *   handle owner-drawing
 */
void IconMenuHandler::imh_draw_item(int ctl_id, DRAWITEMSTRUCT *di)
{
    MenuHashEntry *entry;
    int x, y;

    /* if this isn't a menu item, it's not ours */
    if (di->CtlType != ODT_MENU)
        return;

    /*
     *   Look up the command in the hash table.  If it's not mapped, there's
     *   nothing for us to draw - but this still counts as successful
     *   handling, because it just means that the custom drawing draws
     *   nothing.
     */
    if ((entry = find_hash_entry(di->itemID)) == 0)
        return;

    /*
     *   calculate the position of the icon: draw it at the left edge and
     *   vertically centered in the available space
     */
    x = di->rcItem.left;
    y = (di->rcItem.top + di->rcItem.bottom - icon_height_) / 2;

    /*
     *   if the menu item is grayed, draw the icon grayed; otherwise draw it
     *   in the normal full color
     */
    if ((di->itemState & ODS_GRAYED) != 0)
    {
        HDC win_dc = GetWindowDC(0);
        HDC color_dc, mono_dc;
        HBITMAP color_bmp, old_color_bmp;
        HBITMAP mono_bmp, old_mono_bmp;
        IMAGELISTDRAWPARAMS dp;
        COLORREF hilite = GetSysColor(COLOR_BTNHIGHLIGHT);

        /* create a color bitmap and select it into our color DC */
        color_dc = CreateCompatibleDC(0);
        color_bmp = CreateCompatibleBitmap(win_dc, icon_width_, icon_height_);
        old_color_bmp = (HBITMAP)SelectObject(color_dc, color_bmp);

        /* create a monochrome bitmap and select it into our mono DC */
        mono_dc = CreateCompatibleDC(0);
        mono_bmp = CreateCompatibleBitmap(mono_dc, icon_width_, icon_height_);
        old_mono_bmp = (HBITMAP)SelectObject(mono_dc, mono_bmp);

        /* copy the toolbar button to the color bitmap */
        dp.cbSize = CTadsWin::imagelistdrawparams_size_;
        dp.himl = himl_;
        dp.hdcDst = color_dc;
        dp.i = entry->icon_idx_;
        dp.x = 0;
        dp.y = 0;
        dp.cx = icon_width_;
        dp.cy = icon_height_;
        dp.xBitmap = 0;
        dp.yBitmap = 0;
        dp.rgbBk = hilite;
        dp.rgbFg = CLR_NONE;
        dp.fStyle = ILD_NORMAL;
        dp.dwRop = SRCCOPY;
        ImageList_DrawIndirect(&dp);

        /*
         *   copy the color bitmap to the monochrome bitmap, using the menu
         *   background color for the background and the button hilite color
         *   for the foreground
         */
        SetBkColor(color_dc, hilite);
        BitBlt(mono_dc, 0, 0, icon_width_, icon_height_,
               color_dc, 0, 0, SRCCOPY);

        /* draw the monochrome bitmap onto the menu */
        BitBlt(di->hDC, x, y, icon_width_, icon_height_,
               mono_dc, 0, 0, SRCCOPY );

        /* we're done with the temporary drawing objects - clean up */
        SelectObject(color_dc, old_color_bmp);
        DeleteDC(color_dc);
        DeleteObject(color_bmp);

        SelectObject(mono_dc, old_mono_bmp);
        DeleteDC(mono_dc);
        DeleteObject(mono_bmp);

        ReleaseDC(0, win_dc);
    }
    else
    {
        /* draw the icon normally */
        ImageList_Draw(himl_, entry->icon_idx_, di->hDC, x, y,
                       ILD_TRANSPARENT);
    }
}

