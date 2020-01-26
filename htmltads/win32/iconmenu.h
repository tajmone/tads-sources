/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  iconmenu.h - menu-with-icons add-in
Function
  This is a little add-in class that lets you add an icon to menu items.
Notes

Modified
  10/02/06 MJRoberts  - Creation
*/

#ifndef ICONMENU_H
#define ICONMENU_H

#include "tadsapp.h"

#include <Windows.h>
#include <commctrl.h>


/*
 *   Icon menu add-in class
 */
class IconMenuHandler: public IMenuHandler
{
public:
    /* create */
    IconMenuHandler(int icon_width, int icon_height);

    /* destroy */
    ~IconMenuHandler();

    /*
     *   Add a image from a bitmap resource.  The bitmap is treated as an
     *   array of images of the size specified when the instance was created.
     *   Returns the index of the first added image.
     */
    int add_bitmap(HINSTANCE inst, int res_id);

    /*
     *   Associate a command ID with an icon index.  When the menu is drawn,
     *   any items with this command ID will be drawn with the bitmap at the
     *   given index displayed to the left of the menu title.  If there's an
     *   existing icon mapping for the command, we'll replace it.
     */
    void map_command(int command_id, int icon_index);

    /*
     *   Add command associations to match the given set of toolbar buttons.
     *   The 'base_icon_idx' value is added to each icon index from the
     *   toolbar buttons, to allow different toolbars to each add their own
     *   buttons to the master menu icon list.
     */
    void map_commands(const TBBUTTON *buttons, int num_buttons,
                      int base_icon_idx);

    /* unmap a command - remove the existing association */
    void unmap_command(int command_id);

    /*
     *   IMenuHandler implementation
     */

    /*
     *   Initialize a menu.  This should be called to handle a
     *   WM_INITMENUPOPUP message for one of our menus.  We'll convert the
     *   menu's items to owner-drawn if we haven't already, and we'll
     *   calculate the margin space we need for the menu items based on
     *   whether we need checkmarks, radio buttons, and icons with the menu
     *   items.
     */
    void imh_init_menu_popup(HMENU menu, unsigned int pos, int sysmenu);

    /*
     *   Handle the owner-draw messages, WM_MEASUREITEM and WM_DRAWITEM.
     *   These return TRUE if the message was for a menu, in which case we
     *   handled it; or FALSE if it was for some other type of owner-drawn
     *   control, in which case the caller must handle it.
     */
    void imh_measure_item(int ctl_id, MEASUREITEMSTRUCT *mi);
    void imh_draw_item(int ctl_id, DRAWITEMSTRUCT *di);

private:
    /* find the hash table entry for a given command ID */
    class MenuHashEntry *find_hash_entry(int command_id);

    /* icon dimensions */
    int icon_width_;
    int icon_height_;

    /* command-to-icon hash table */
    class CHtmlHashTable *hash_;

    /* our image list for our icons */
    HIMAGELIST himl_;
};


#endif /* ICONMENU_H */
