/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  scriptaddin.cpp - Workbench extension: editor customizations for
  TADS command scripts
Function
  This is a TADS Workbench add-in that provides customized styling for
  command script files.

  See itadsworkbench.h for details on how to create Workbench extensions.
Notes

Modified
  12/24/06 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include <scintilla.h>

#include "wb_addin.h"
#include "scriptaddin.h"

/* ------------------------------------------------------------------------ */
/*
 *   Module management
 */

/* our DLL's instance handle - we need this to load our resources */
HINSTANCE G_instance = 0;

/* main DLL entrypoint */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, VOID *reserved)
{
    /* remember our instance handle */
    if (reason == DLL_PROCESS_ATTACH)
        G_instance = hinst;

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   script editor mode
 */
class ScriptEditorMode: public CTadsWorkbenchEditorMode
{
public:
    ScriptEditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /*
     *   ITadsWorkbenchEditorMode methods
     */

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"Command Script");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"Command Scripts (*.cmd)\0*.cmd\0";

        *priority = 9000;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* check to see if we want to provide editor customizations for a file */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* we recognize .cmd files */
        size_t len = wcslen(filename);
        if (len > 4 && wcsicmp(filename + len - 4, L".cmd") == 0)
        {
            /* we matched on extension */
            return 100;
        }

        /* not one of ours */
        return 0;
    }

    /* get file type information */
    BOOL STDMETHODCALLTYPE GetFileTypeInfo(UINT index, wb_ftype_info *info)
    {
        switch (index)
        {
        case 0:
            /* script files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_SCRIPT_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"Command Script");
            wcscpy(info->desc, L"A file containing a list of commands "
                  L"for a game to read as input");
            wcscpy(info->defext, L"cmd");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

protected:
    /* create our extension */
    CTadsWorkbenchEditor *create_ext(HWND hwnd);

    /* get the number of styles we use */
    virtual const style_info *get_style_array() { return styles_; }
    virtual UINT get_style_count() { return style_cnt_; }

protected:
    /* default colors for the styles */
    static const style_info styles_[];
    static size_t style_cnt_;
};

/* our style list */
const style_info ScriptEditorMode::styles_[] =
{
    { 0, L"Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 1, L"Comments", L"# comment text",
    RGB(0x80,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 2, L"Tags", L"<line>",
    RGB(0xFF,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 3, L"Parameters", L"parameter text",
    RGB(0x00,0x00,0xff), WBSTYLE_DEFAULT_COLOR, 0, 0 },
};
size_t ScriptEditorMode::style_cnt_ = countof(styles_);


/* ------------------------------------------------------------------------ */
/*
 *   The script editor customizer
 */
class ScriptEditor: public CTadsWorkbenchEditor
{
public:
    ScriptEditor(ITadsWorkbench *itwb, HWND hwnd)
        : CTadsWorkbenchEditor(itwb, hwnd)
    {
    }

    /*
     *   ITadsWorkbenchEditor implementation
     */

    /* initialize */
    void init()
    {
        /* inherit the default handling */
        CTadsWorkbenchEditor::init();

        /* tell Scintilla to use the command-script ("tadscmd") lexer */
        call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"tadscmd");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
    }

    /*
     *   Receive notification of an added character
     */
    void STDMETHODCALLTYPE OnCharAdded(UINT ch)
    {
    }

protected:
};


/* ------------------------------------------------------------------------ */
/*
 *   The extension object.
 */
class ScriptAddIn: public CTadsWorkbenchExtension
{
public:
    ScriptAddIn(ITadsWorkbench *itwb)
        : CTadsWorkbenchExtension(itwb)
    {
    }

    /*
     *   ITadsWorkbenchExtension implementation
     */

    /* get my nth editor mode */
    ITadsWorkbenchEditorMode * STDMETHODCALLTYPE GetMode(UINT index)
    {
        switch (index)
        {
        case 0:
            return new ScriptEditorMode(itwb_);

        default:
            /* no other modes */
            return 0;
        }
    }

protected:
};


/* ------------------------------------------------------------------------ */
/*
 *   Additional Mode object implementation
 */

/* create the extension */
CTadsWorkbenchEditor *ScriptEditorMode::create_ext(HWND hwnd)
{
    return new ScriptEditor(itwb_, hwnd);
}

/* ------------------------------------------------------------------------ */
/*
 *   Connect.  Workbench calls this at start-up to initialize the DLL and
 *   exchange interface pointers.
 */
__declspec(dllexport) IUnknown *twb_connect(ITadsWorkbench *itwb)
{
    /* create and return our extension object */
    return new ScriptAddIn(itwb);
}
