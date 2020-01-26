/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadshtmldlg.h - simplified interface to Windows/IE HTML Dialogs
Function

Notes

Modified
  09/06/09 MJRoberts  - Creation
*/

#ifndef TADSHTMLDLG_H
#define TADSHTMLDLG_H

#include "tadscom.h"

/* error codes */
#define THD_ERR_LIB    1                           /* can't load MSHTML.DLL */
#define THD_ERR_FUNC   2  /* can't link to ShowHTMLDialog() function in DLL */


/*
 *   Run a modal HTML dialog.  The input arguments are passed in via our
 *   simplified IDispatch interface.  The width is in ex's and the height is
 *   in ems.
 */
int modal_html_dialog(HWND parent_hwnd, const char *dlg_name, int wid, int ht,
                      TadsDispatch *args);

/*
 *   Run a modal HTML dialog.  The height and width are given as CSS
 *   dimensions, with explicit units - e.g., "100px" or "40em".
 */
int modal_html_dialog(HWND parent_hwnd, const char *dlg_name,
                      const char *wid, const char *ht, int resizable,
                      TadsDispatch *args);

#endif /* TADSHTMLDLG_H */
