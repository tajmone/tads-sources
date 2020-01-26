#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadshtmldlg.cpp - simplified interface to Windows/IE HTML Dialogs
Function

Notes

Modified
  09/06/09 MJRoberts  - Creation
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <Windows.h>
#include <oleauto.h>
#include <mshtmhst.h>

#include "tadshtmldlg.h"
#include "tadscom.h"
#include "tadsapp.h"


/*
 *   Run a modal HTML dialog.  The input arguments are passed in via our
 *   simplified IDispatch interface.  The height is in ems and the width is
 *   in ex's.
 */
int modal_html_dialog(HWND parent_hwnd, const char *dlg_name, int wid, int ht,
                      TadsDispatch *args)
{
    char widb[30], htb[30];
    sprintf(widb, "%dex", wid);
    sprintf(htb, "%dem", ht);
    return modal_html_dialog(parent_hwnd, dlg_name, widb, htb, TRUE, args);
}

/*
 *   Run a modal HTML dialog.  The input arguments are passed in via our
 *   simplified IDispatch interface.
 */
int modal_html_dialog(HWND parent_hwnd, const char *dlg_name,
                      const char *wid, const char *ht, int resizable,
                      TadsDispatch *args)
{
    /* load the HTML dialog for the publishing wizard */
    HINSTANCE hlib = LoadLibrary("MSHTML.DLL");
    if (hlib == 0)
        return THD_ERR_LIB;

    /* link to the ShowHTMLDialog function */
    SHOWHTMLDIALOGFN *func =
        (SHOWHTMLDIALOGFN *)GetProcAddress(hlib, "ShowHTMLDialog");
    if (func == 0)
    {
        FreeLibrary(hlib);
        return THD_ERR_FUNC;
    }

    /*
     *   Set up the URL string.  This is a res: string to our own exectuable,
     *   so start by getting the executable path.
     */
    char exe[256];
    GetModuleFileName(
        CTadsApp::get_app()->get_instance(), exe, sizeof(exe));

    /* count backslashes for expansion - \ -> %5c */
    int bscnt = 0;
    for (char *p = exe ; *p != '\0' ; *p++)
    {
        if (*p == '\\')
            ++bscnt;
    }

    /* translate '\' characters in the path to '%5C' sequences */
    char *uexe = new char[strlen(exe) + 2*bscnt + 1];
    char *src, *dst;
    for (src = exe, dst = uexe ; *src != '\0' ; ++src)
    {
        if (*src == '\\')
            *dst++ = '%', *dst++ = '5', *dst++ = 'C';
        else
            *dst++ = *src;
    }
    *dst = '\0';

    /* build the full resource path */
    int resl = strlen(uexe) + strlen(dlg_name) + 20;
    wchar_t *res = new wchar_t[resl];
    _snwprintf(res, resl, L"res://%hs/%hs", uexe, dlg_name);

    /* create a URL moniker for the path */
    IMoniker *urlmon = 0;
    BSTR url = SysAllocString(res);
    CreateURLMoniker(0, url, &urlmon);

    /* done with the intermediate strings */
    delete [] uexe;
    delete [] res;

    /* set up the input parameter */
    VARIANT inArg;
    inArg.vt = VT_DISPATCH;
    V_DISPATCH(&inArg) = args;

    /* set up a holder for the output parameter */
    VARIANT outArg;
    VariantInit(&outArg);

    /* build the dialog options string */
    wchar_t opts[256];
    swprintf(opts, L"dialogWidth:%hs;dialogHeight:%hs;center:yes;"
             L"status:no;resizable:%hs;scroll:no;",
             wid, ht, resizable ? "yes" : "no");

    /* run the dialog */
    (*func)(parent_hwnd, urlmon, &inArg, opts, &outArg);

    /* done with the URL moniker and source string */
    if (urlmon != 0)
        urlmon->Release();
    if (url != 0)
        SysFreeString(url);

    /* release the library */
    FreeLibrary(hlib);

    /* success */
    return 0;
}

