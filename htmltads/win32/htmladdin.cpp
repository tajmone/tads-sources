/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  htmladdin.cpp - Workbench extension: editor customizations for HTML/XML
Function
  This is a TADS Workbench add-in that provides customized indenting and
  styling for HTML and XML source files opened in Workbench's integrated
  Scintilla editor.

  See itadsworkbench.h for details on how to create Workbench extensions.
Notes

Modified
  10/05/06 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include <windows.h>

#include <scintilla.h>
#include <scilexer.h>

#include "wb_addin.h"
#include "htmladdin.h"


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
 *   XML editor mode
 */
class XmlEditorMode: public CTadsWorkbenchEditorMode
{
public:
    XmlEditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /*
     *   ITadsWorkbenchEditorMode methods
     */

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"XML");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"XML Files (*.xml)\0*.xml\0";

        *priority = 1001;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* check to see if we want to provide editor customizations for a file */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* the XML mode recognizes the .xml extension */
        size_t len = wcslen(filename);
        if (len > 4 && wcsicmp(filename + len - 4, L".xml") == 0)
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
            /* XML files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_XML_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"XML File");
            wcscpy(info->desc, L"A file containing XML (eXtensible "
                   L"Markup Language) code");
            wcscpy(info->defext, L"xml");
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

/* our style list - XML and HTML modes share the style numbering system */
const style_info XmlEditorMode::styles_[] =
{
    { 0, L"Plain Text", L"Sample Text",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 1, L"HTML Tags", L"<TABLE>",
    RGB(0x80,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 2, L"HTML Unknown Tags", L"<UNDEFINED>",
    RGB(0xFF,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 3, L"HTML Attributes", L"href=",
    RGB(0x00,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 4, L"HTML Unknown Attributes", L"undefined=",
    RGB(0xFF,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 5, L"HTML Numbers", L"12345",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 6, L"HTML Double quoted strings", L"\"Sample Text\"",
    RGB(0xc0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 7, L"HTML Single quoted strings", L"'Sample Text'",
    RGB(0xc0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 8, L"HTML Other inside tag", L"Sample Text",
    RGB(0x80,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 9, L"HTML Comment", L"<!-- Comment -->",
    RGB(0x80,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 10, L"HTML Entities", L"&&amp;",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 11, L"XML style tag end", L"<LINK/>",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 12, L"XML identifier start", L"<?XML",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 13, L"XML identifier end", L"?>",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 14, L"SCRIPT", L"Sample Text",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 15, L"ASP1", L"<% Sample %>",
    RGB(0xFF,0xFF,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 16, L"ASP2", L"<% Sample %>",
    RGB(0xFF,0xD0,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 17, L"HTML CDATA", L"Sample Text",
    RGB(0xFF,0xD0,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 18, L"PHP", L"Sample Text",
    RGB(0x00,0x00,0xFF), RGB(0xFF,0xFF,0xD0), 0, 0 },

    { 19, L"PHP Unquoted values", L"Sample Text",
    RGB(0xFF,0x00,0xFF), RGB(0xFF,0xEF,0xFF), 0, 0 },

    { 20, L"JSP Comment", L"<%-- Comment --%>",
    RGB(0x00,0x00,0x00), RGB(0xFF,0xFF,0xD0), 0, 0 },

    { 21, L"SGML tags", L"<!Sample>",
    RGB(0x00,0x00,0x80), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 22, L"SGML command", L"Sample Text",
    RGB(0x00,0x00,0x80), RGB(0xEF,0xEF,0xFF), WBSTYLE_BOLD, 0 },

    { 23, L"SGML 1st param", L"Sample Text",
    RGB(0x00,0x64,0x00), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 24, L"SGML double string", L"\"Sample Text\"",
    RGB(0x80,0x00,0x00), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 25, L"SGML single string", L"'Sample Text'",
    RGB(0x99,0x33,0x00), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 26, L"SGML error", L"Sample Text",
    RGB(0x80,0x00,0x00), RGB(0xFF,0x66,0x66), 0, 0 },

    { 27, L"SGML special", L"#1234",
    RGB(0x33,0x66,0xFF), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 28, L"SGML entity", L"Sample Text",
    RGB(0x33,0x33,0x33), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 29, L"SGML comment", L"Sample Text",
    RGB(0x80,0x80,0x00), RGB(0xEF,0xEF,0xFF), 0, 0 },

    { 31, L"SGML block", L"Sample Text",
    RGB(0x00,0x00,0x80), RGB(0xC0,0xC0,0xE0), 0, 0 },

    { 34, L"Brace Highlight", L"< >",
    RGB(0x00,0x00,0x00), RGB(0x7F,0xFF,0xD4), 0, 0 },

    { 35, L"Matched Operators", L"",
    RGB(0xFF,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 40, L"JavaScript Start", L"Sample Text",
    RGB(0x80,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 41, L"JavaScript Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 42, L"JavaScript Comment", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 43, L"JavaScript Line Comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 44, L"JavaScript Doc comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 45, L"JavaScript Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 46, L"JavaScript Word", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 47, L"JavaScript Keyword", L"Sample Text",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 48, L"JavaScript Double quoted string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 49, L"JavaScript Single quoted string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 50, L"JavaScript Symbols", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 51, L"JavaScript End-of-line in string", L"\"Text...",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 52, L"JavaScript Regular expression", L"/\\s+(\\w+)\\s+/g",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 55, L"JavaScript Start", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 56, L"JavaScript Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 57, L"JavaScript Comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 58, L"JavaScript Line Comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 59, L"JavaScript Doc comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 60, L"JavaScript Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 61, L"JavaScript Word", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 62, L"JavaScript Keyword", L"Sample Text",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 63, L"JavaScript Double quoted string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 64, L"JavaScript Single quoted string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 65, L"JavaScript Symbols", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 66, L"JavaScript EOL", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 67, L"JavaScript RegEx", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 70, L"VBS Start", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 71, L"VBS Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 72, L"VBS Comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 73, L"VBS Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 74, L"VBS KeyWord", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 75, L"VBS String", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 76, L"VBS Identifier", L"Sample Text",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 77, L"VBS Unterminated string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 80, L"ASP VBS Start", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 81, L"ASP VBS Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 82, L"ASP VBS Comment", L"Sample Text",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 83, L"ASP VBS Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 84, L"ASP VBS KeyWord", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 85, L"ASP VBS String", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 86, L"ASP VBS Identifier", L"Sample Text",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 87, L"ASP VBS Unterminated string", L"Sample Text",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 90, L"Python Start", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 91, L"Python Misc", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 92, L"Python Comment", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 93, L"Python Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 94, L"Python String", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 95, L"Python Single quoted string", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 96, L"Python Keyword", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 97, L"Python Triple quotes", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 98, L"Python Triple double quotes", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 99, L"Python Class definition", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 100, L"Python Function/Method definition", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 101, L"Python Operators", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 102, L"Python Identifiers", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 104, L"PHP complex variable", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 105, L"ASP Python Start", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 106, L"ASP Python Misc", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 107, L"ASP Python Comment", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 108, L"ASP Python Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 109, L"ASP Python String", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 110, L"ASP Python Single quoted string", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 111, L"ASP Python Keyword", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 112, L"ASP Python Triple quotes", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 113, L"ASP Python Triple double quotes", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 114, L"ASP Python Class definition", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 115, L"ASP Python Function/method definition", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 116, L"ASP Python Operators", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 117, L"ASP Python Identifiers", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 118, L"PHP Default", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 119, L"PHP Double quoted String", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 120, L"PHP Single quoted string", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 121, L"PHP Keyword", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 122, L"PHP Number", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 123, L"PHP Variable", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 124, L"PHP Comment", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 125, L"PHP One line comment", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 126, L"PHP variable in double quoted string", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { 127, L"PHP operator", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 }
};
size_t XmlEditorMode::style_cnt_ = countof(styles_);


/* ------------------------------------------------------------------------ */
/*
 *   HTML editor mode.  This is a specialization of the XML editor mode.
 */
class HtmlEditorMode: public XmlEditorMode
{
public:
    HtmlEditorMode(ITadsWorkbench *itwb) : XmlEditorMode(itwb) { }

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"HTML");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"HTML Files (*.htm, *.html)\0*.htm;*.html\0";

        *priority = 1000;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* get my file affinity */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* the HTML mode recognizes .htm and .html extensions */
        size_t len = wcslen(filename);
        if ((len > 4 && wcsicmp(filename + len - 4, L".htm") == 0)
            || (len > 5 && wcsicmp(filename + len - 5, L".html") == 0))
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
            /* XML files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_HTML_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"HTML File");
            wcscpy(info->desc, L"An HTML Page (HyperText Markup Language)");
            wcscpy(info->defext, L"htm");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

protected:
    /* create our extension */
    CTadsWorkbenchEditor *create_ext(HWND hwnd);
};


/* ------------------------------------------------------------------------ */
/*
 *   The XML editor customizer
 */
class XmlEditor: public CTadsWorkbenchEditor
{
public:
    XmlEditor(ITadsWorkbench *itwb, HWND hwnd)
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

        /* tell Scintilla to use the "html" lexer */
        call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"xml");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold.html", (LPARAM)"1");

        /* the hypertext lexer needs tons of styles */
        call_sci(SCI_SETSTYLEBITS, 7);
        call_sci(SCI_COLOURISE, 0, -1);

        /* add the SGML/DTD keywords */
        call_sci(SCI_SETKEYWORDS, 5, (int)
                 "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");

        /* send the list of "word" characters */
        call_sci(SCI_SETWORDCHARS, 0, (int)
                 "abcdefghijklmnopqrstuvwxyz"
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "0123456789_-");
    }

    /*
     *   Receive notification of an added character
     */
    void STDMETHODCALLTYPE OnCharAdded(UINT ch)
    {
        /* inherit the default handling */
        CTadsWorkbenchEditor::OnCharAdded(ch);
    }

protected:
};

/* ------------------------------------------------------------------------ */
/*
 *   The HTML editor customizer
 */
class HtmlEditor: public XmlEditor
{
public:
    HtmlEditor(ITadsWorkbench *itwb, HWND hwnd)
        : XmlEditor(itwb, hwnd)
    {
    }

    /*
     *   ITadsWorkbenchEditor implementation
     */

    /* initialize */
    void init()
    {
        /*
         *   inherit the default handling from the base editor class, but
         *   bypass the XML initialization, as we supersede all of that
         */
        CTadsWorkbenchEditor::init();

        /* tell Scintilla to use the "html" lexer */
        call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"hypertext");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold.html", (LPARAM)"1");

        /* the hypertext lexer needs tons of styles */
        call_sci(SCI_SETSTYLEBITS, 7);
        call_sci(SCI_COLOURISE, 0, -1);

        /* send the lexer the standard list of HTML keywords */
        call_sci(SCI_SETKEYWORDS, 0, (int)
                 /* elements */
                 "a abbr acronym address applet area b base basefont "
                 "bdo big blockquote body br button caption center "
                 "cite code col colgroup dd del dfn dir div dl dt em "
                 "fieldset font form frame frameset h1 h2 h3 h4 h5 h6 "
                 "head hr html i iframe img input ins isindex kbd label "
                 "legend li link map menu meta noframes noscript "
                 "object ol optgroup option p param pre q s samp "
                 "script select small span strike strong style sub sup "
                 "table tbody td textarea tfoot th thead title tr tt u ul "
                 "var xml xmlns "
                 /* attributes */
                 "abbr accept-charset accept accesskey action align alink "
                 "alt archive axis background bgcolor border "
                 "cellpadding cellspacing char charoff charset checked cite "
                 "class classid clear codebase codetype color cols colspan "
                 "compact content coords "
                 "data datafld dataformatas datapagesize datasrc datetime "
                 "declare defer dir disabled enctype event "
                 "face for frame frameborder "
                 "headers height href hreflang hspace http-equiv "
                 "id ismap label lang language leftmargin link longdesc "
                 "marginwidth marginheight maxlength media method multiple "
                 "name nohref noresize noshade nowrap "
                 "object onblur onchange onclick ondblclick onfocus "
                 "onkeydown onkeypress onkeyup onload onmousedown "
                 "onmousemove onmouseover onmouseout onmouseup "
                 "onreset onselect onsubmit onunload "
                 "profile prompt readonly rel rev rows rowspan rules "
                 "scheme scope selected shape size span src standby start style "
                 "summary tabindex target text title topmargin type usemap "
                 "valign value valuetype version vlink vspace width "
                 "text password checkbox radio submit reset "
                 "file hidden image "
                 /* html 5 elements */
                 "article aside calendar canvas card command commandset "
                 "datagrid datatree footer gauge header m menubar menulabel "
                 "nav progress section switch tabbox "
                 /* html 5 attributes */
                 "active command contenteditable ping "
                 /* additional */
                 "public !doctype");

        /* add the javascript keywords */
        call_sci(SCI_SETKEYWORDS, 1, (int)
                 "abstract boolean break byte case catch char class "
                 "const continue debugger default delete do double else "
                 "enum export extends "
                 "final finally float for function goto if implements "
                 "import in instanceof "
                 "int interface long native new package private protected "
                 "public return short static super switch synchronized "
                 "this throw throws "
                 "transient try typeof var void volatile while with");

        /* add the VBScript keywords */
        call_sci(SCI_SETKEYWORDS, 2, (int)
                 "addressof alias and as attribute base begin binary boolean "
                 "byref byte byval call case compare "
                 "const currency date decimal declare defbool defbyte defint "
                 "deflng defcur defsng defdbl defdec "
                 "defdate defstr defobj defvar dim do double each else "
                 "elseif empty end enum eqv erase error "
                 "event exit explicit false for friend function get gosub "
                 "goto if imp implements in input integer "
                 "is len let lib like load lock long loop lset me mid midb "
                 "mod new next not nothing null object "
                 "on option optional or paramarray preserve print private "
                 "property public raiseevent randomize "
                 "redim rem resume return rset seek select set single "
                 "static step stop string sub then time to "
                 "true type typeof unload until variant wend while with "
                 "withevents xor");

        /* add the Python keywords */
        call_sci(SCI_SETKEYWORDS, 3, (int)
                 "and assert break class continue def del elif "
                 "else except exec finally for from global if import "
                 "in is lambda None not or pass print raise return try "
                 "while yield");

        /* add the PHP keywords */
        call_sci(SCI_SETKEYWORDS, 4, (int)
                 "and array as bool boolean break case cfunction class "
                 "const continue declare "
                 "default die directory do double echo else elseif empty "
                 "enddeclare endfor "
                 "endforeach endif endswitch endwhile eval exit extends "
                 "false float for "
                 "foreach function global if include include_once int "
                 "integer isset list new "
                 "null object old_function or parent print real require "
                 "require_once resource "
                 "return static stdclass string switch true unset use "
                 "var while xor __class__ "
                 "__file__ __function__ __line__ __sleep __wakeup");

        /* add the SGML/DTD keywords */
        call_sci(SCI_SETKEYWORDS, 5, (int)
                 "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");

        /* send the list of "word" characters */
        call_sci(SCI_SETWORDCHARS, 0, (int)
                 "abcdefghijklmnopqrstuvwxyz"
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                 "0123456789_-");
    }
};




/* ------------------------------------------------------------------------ */
/*
 *   CSS editor mode
 */
class CSSEditorMode: public CTadsWorkbenchEditorMode
{
public:
    CSSEditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /*
     *   ITadsWorkbenchEditorMode methods
     */

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"CSS");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"CSS Files (*.css)\0*.css\0";

        *priority = 1001;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* check to see if we want to provide editor customizations for a file */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* the CSS mode recognizes the .css extension */
        size_t len = wcslen(filename);
        if (len > 4 && wcsicmp(filename + len - 4, L".css") == 0)
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
            /* CSS files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_CSS_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"CSS File");
            wcscpy(info->desc, L"A file containing CSS (Cascading "
                   L"Style Sheet) code");
            wcscpy(info->defext, L"css");
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
const style_info CSSEditorMode::styles_[] =
{
    { SCE_CSS_DEFAULT, L"Plain Text", L"Sample Text",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_TAG, L"Tag name", L"BODY",
    RGB(0x80,0x00,0x40), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_CLASS, L"Class name", L"myclass",
    RGB(0x00,0x80,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_PSEUDOCLASS, L"Pseudoclass", L":link",
    RGB(0x00,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_UNKNOWN_PSEUDOCLASS, L"Unknown pseudoclass", L":undef",
    RGB(0xFF,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_OPERATOR, L"Operator", L": , { }",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_IDENTIFIER, L"Identifier", L"\"Sample Text\"",
    RGB(0x80,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_UNKNOWN_IDENTIFIER, L"Unknown identifier", L"undef",
    RGB(0xd0,0x40,0x40), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_VALUE, L"Value", L"#ffffff",
    RGB(0x00,0x00,0xff), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_COMMENT, L"Comment", L"/* Comment */",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_ID, L"ID", L"#main",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_IMPORTANT, L"Important", L"!important",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_DIRECTIVE, L"Directive", L"@import",
    RGB(0x80,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_DOUBLESTRING, L"String in double quotes", L"\"Sample\"",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_SINGLESTRING, L"String in single quotes", L"'Sample'",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_IDENTIFIER2, L"Other identifier", L"Sample",
    RGB(0x00,0x00,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },

    { SCE_CSS_ATTRIBUTE, L"Attribute", L"[title]",
    RGB(0xD0,0xB0,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
};
size_t CSSEditorMode::style_cnt_ = countof(styles_);


/*
 *   The CSS editor customizer
 */
class CSSEditor: public CTadsWorkbenchEditor
{
public:
    CSSEditor(ITadsWorkbench *itwb, HWND hwnd)
        : CTadsWorkbenchEditor(itwb, hwnd)
    {
    }

    /*
     *   ITadsWorkbenchEditor implementation
     */

    /* initialize */
    void init()
    {
        /*
         *   inherit the default handling from the base editor class, but
         *   bypass the CSS initialization, as we supersede all of that
         */
        CTadsWorkbenchEditor::init();

        /* tell Scintilla to use the "css" lexer */
        call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"css");
        call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
        call_sci(SCI_SETSTYLEBITS, 5);
        call_sci(SCI_COLOURISE, 0, -1);

        /* CSS1 properties */
        call_sci(SCI_SETKEYWORDS, 0, (int)
                 "font-family font-style font-variant font-weight font-size "
                 "font color background-color background-image "
                 "background-repeat background-attachment "
                 "background-position background word-spacing "
                 "letter-spacing text-decoration vertical-align "
                 "text-transform text-align text-indent line-height "
                 "margin-top margin-right margin-bottom margin-left "
                 "margin padding-top padding-right padding-bottom "
                 "padding-left padding border-top-width border-right-width "
                 "border-bottom-width border-left-width border-width "
                 "border-color border-style border-top border-right "
                 "border-bottom border-left border width height float "
                 "clear white-space list-style-image "
                 "list-style-position list-style "
                 "page-break-before page-break-after marks "
                 "cursor page width height ");

        /* CSS pseudo-classes */
        call_sci(SCI_SETKEYWORDS, 1, (int)
                 "link visited active hover focus lang");

        /* CSS2 properties */
        call_sci(SCI_SETKEYWORDS, 2, (int)
                 "font-size-adjust font-stretch text-shadow list-style-type "
                 "outline-width outline-style outline-color outline "
                 "overflow visibility content counter-reset "
                 "counter-increment marker-offset quotes direction "
                 "display max-height max-width min-height min-width "
                 "position left top bottom right z-index "
                 "border-collapse border-spacing caption-side "
                 "empty-cells speak-header table-layout volume "
                 "speak pause-before pause-after pause cue-before "
                 "cue-after cue play-during azimuth elevation "
                 "speech-rate voice-family pitch pitch-range "
                 "stress richness speak-punctuation speak-numeral "
                 "border-bottom-style border-left-style border-right-style "
                 "border-top-style border-left-color border-right-color "
                 "border-top-color border-bottom-color clip "
                 "widows orphans size unicode-bidi");

        /* CSS3 properties */
        call_sci(SCI_SETKEYWORDS, 3, (int)
                 "alignment-adjust alignment-baseline animation "
                 "animation-delay animation-direction animation-duration "
                 "animation-iteration-count animation-name "
                 "animation-play-state animation-timing-function "
                 "appearance backface-visibility background-clip "
                 "background-origin background-size baseline-shift "
                 "binding bleed bookmark-label bookmark-level "
                 "bookmark-state bookmark-target border-bottom-left-radius "
                 "border-bottom-right-radius border-top-left-radius "
                 "border-top-right-radius border-image border-image-outset "
                 "border-image-repeat border-image-slice border-image-source "
                 "border-image-width border-radius box-align "
                 "box-decoration-break box-flex box-flex-group box-lines "
                 "box-ordinal-group box-orient box-pack box-shadow "
                 "box-sizing break-after break-before break-inside "
                 "color-profile column-count column-fill column-gap "
                 "column-rule column-rule-color column-rule-style "
                 "column-rule-width column-span colunn-width columns "
                 "crop dominant-baseline drop-initial-after-adjust "
                 "drop-initial-after-align drop-initial-before-adjust "
                 "drop-initial-before-align drop-initial-size "
                 "drop-initial-value fit fit-position float-offset "
                 "grid-columns grid-rows hanging-punctuation "
                 "hyphenate-after hyphenate-before hyphenate-character "
                 "hyphenate-lines hyphenate-resource hyphens icon "
                 "image-orientation image-rendering image-resolution "
                 "inline-box-align line-stacking line-stacking-ruby "
                 "line-stacking-strategy mark marquee-direction "
                 "marquee-loop marquee-play-count marquee-speed "
                 "marquee-style move-to nav-down nav-index nav-left "
                 "nav-right nav-up opacity outline-offset "
                 "overflow-style overflow-x overflow-y page-policy "
                 "perspective perspective-origin phonemes "
                 "presentation-level punctuation-trim rendering-intent "
                 "resize rest rest-after rest-before rotation "
                 "rotation-point ruby-align ruby-overhang ruby-position "
                 "ruby-span string-set target-name target-new "
                 "target-position text-align-last text-emphasis "
                 "text-height text-justify text-outline text-wrap "
                 "transform transform-origin transform-style "
                 "transition transition-delay transition-duration "
                 "transition-property transition-timing-function "
                 "voice-balance voice-duration voice-pitch voice-pitch-range "
                 "voice-rate voice-stress voice-volume white-space-collapse "
                 "word-break ");

        /* CSS pseudo-elements */
        call_sci(SCI_SETKEYWORDS, 4, (int)
                 "first-line first-letter first-child before after");

    }

    /* check command status */
    BOOL STDMETHODCALLTYPE GetCommandStatus(
        UINT command_id, UINT *status, HMENU menu, UINT menu_index)
    {
        /* our custom commands are always enabled */
        if (command_id == indent_cmd
            || command_id == addcomment_cmd
            || command_id == commentfmt_cmd)
        {
            *status = TWB_CMD_ENABLED;
            return TRUE;
        }

        /* it's not ours - inherit the default handling */
        return CTadsWorkbenchEditor::GetCommandStatus(
            command_id, status, menu, menu_index);
    }

    BOOL STDMETHODCALLTYPE DoCommand(UINT command_id)
    {
        /* remap the Fill Paragraph command to do syntax indenting */
        if (command_id == fill_para_cmd)
            command_id = indent_cmd;

        /* it's not ours - inherit the default handling */
        return CTadsWorkbenchEditor::DoCommand(command_id);
    }

    /* hot characters for syntax indenting */
    virtual const char *hot_chars() const { return "\n\r{}/"; }

    /* do syntax indenting */
    void syntax_indent(int curline)
    {
        /*
         *   Check to see if we're at the end of a block comment.  If so,
         *   apply our special comment formatting: we'll word-wrap the text
         *   within the comment to fit within the current right margin, and
         *   put a '*' border along the left edge of the comment.  If it's a
         *   single-line comment (i.e., the comment opener is on the same
         *   line), don't format it.
         */
        SciLine lin(sci_, curline);
        if (re_match(lin.txt, ".*%*/") && !re_match(lin.txt, ".*/%*.*%*/"))
        {
            /* do the comment formatting, grouping undo */
            format_c_comment_undo(curline);
            return;
        }


        /* indent the line */
        indent_to_column(curline, calc_indent(curline));
    }

    /* do comment fill */
    virtual int format_comment(int curpos)
    {
        /*
         *   Check to see if we're in a comment.  Starting at the current
         *   position, search backwards for star-slash or slash-star.  If we
         *   find a star-slash, we're evidently not in a comment because we
         *   found the end of a comment first.  If we find a slash-star,
         *   we're in a comment.  If we don't find either, we're not in a
         *   comment.
         */
        int curline = call_sci(SCI_LINEFROMPOSITION, curpos);
        SciLine lin(sci_, curline);
        char *p = lin.getpostxt(curpos);
        if (re_search(-1, &p, &lin, "/%*|%*/")
            && memcmp(p, "/*", 2) == 0)
        {
            /* do the formatting, C-style */
            format_c_comment_undo(curline);

            /* handled */
            return TRUE;
        }

        /* we didn't do any reformatting */
        return FALSE;
    }

    /* syntax-indent a range */
    void syntax_indent_range(int start, int a, int end, int b)
    {
        /* set a spot at the start and end of the range */
        LPARAM spot_start = create_spot(start);
        LPARAM spot_end = create_spot(end);

        /* indent each line until we reach the marker */
        for (;;)
        {
            /* set the start spot to the starting line */
            set_spot_pos(spot_start, call_sci(SCI_POSITIONFROMLINE, a));

            /* indent this line */
            syntax_indent(a);

            /* go back to the starting line */
            a = call_sci(SCI_LINEFROMPOSITION, get_spot_pos(spot_start));

            /* on to the next line */
            ++a;

            /*
             *   if we've reached the end of the range or the end of the
             *   document, we're done
             */
            if (call_sci(SCI_POSITIONFROMLINE, a) > get_spot_pos(spot_end)
                || a >= call_sci(SCI_GETLINECOUNT))
                break;
        }

        /* delete our spots */
        delete_spot(spot_start);
        delete_spot(spot_end);
    }

    /* figure the indentation for the given line */
    int calc_indent(int curline)
    {
        /* get the current line information */
        int delta = call_sci(SCI_GETINDENT);
        SciLine lin(sci_, curline);

        /*
         *   Scan backwards for the nearest previous line with text in the
         *   first column.  This could conceivably be in a comment, but
         *   assume that it's not.
         */
        SciLine cur(sci_, curline - 1);
        for ( ; cur.lin >= 0 ; cur.go_prev())
        {
            /* stop if we have text in the first column */
            if (re_match(cur.txt, "^<^space>"))
                break;
        }

        /*
         *   proposed indentation - this is the indentation for the next line
         *   after the current indent feature
         */
        int indent = 0;

        /* brace indentation level */
        int brindent = 0;

        /* now scan forward to figure our indent position */
        char *p = cur.txt;
        for (;;)
        {
            /* if we've reached the current line, return the current indent */
            if (cur.lin >= curline)
            {
                /* indent open braces to the brace level */
                if (cur.lin == curline
                    && re_match(cur.txt, "<space>*%{"))
                    return brindent;

                /* indent close braces to the enclosing brace level */
                if (cur.lin == curline
                    && re_match(cur.txt, "<space>*%}")
                    && brindent >= delta)
                    return brindent - delta;

                /* return the calculated indentation */
                return indent;
            }

            /* skip spaces */
            int len = re_match(p, "<space>+");
            p += len;

            /* if we're at the end of the line, fetch the next one */
            if (*p == '\0')
            {
                /* fetch the next line */
                cur.go_next();

                /* start scanning at the new line */
                p = cur.txt;
                continue;
            }

            /* check the token */
            switch (*p++)
            {
            case '/':
                if (*p == '*')
                {
                    /* comment - note where the star starts */
                    int starcol = p - cur.txt;

                    /* skip the star and scan for the closing star-slash */
                    ++p;
                    if (!re_search(1, &p, &cur, "%*/") || cur.lin >= curline)
                    {
                        /*
                         *   the comment is unclosed, or it ends in or after
                         *   the current line, so this line starts in the
                         *   comment - indent to the star
                         */
                        return starcol;
                    }
                }
                break;

            case ';':
                /* return to the brace indent level for the next line */
                indent = brindent;
                break;

            case ',':
                /*
                 *   at root level, indent a list of alternative selectors at
                 *   the same level
                 */
                if (brindent == 0)
                    indent = 0;
                break;

            case '{':
                /* open brace - indent one more than the current line */
                indent = call_sci(SCI_GETLINEINDENTATION, cur.lin) + delta;
                brindent = indent;
                break;

            case '}':
                /* close brace - unindent a brace level */
                if ((brindent -= delta) < 0)
                    brindent = 0;
                indent = brindent;
                break;

            default:
                /*
                 *   if it's an identifier, indent subsequent lines as
                 *   continuation lines
                 */
                if (re_match(p, "<alpha|_>"))
                    indent = brindent + delta;
                break;
            }
        }
    }

    /* toggle comments */
    virtual void toggle_comments()
    {
        /* do the work in an undo group */
        call_sci(SCI_BEGINUNDOACTION);

        /* get the selection range */
        int sela = call_sci(SCI_GETSELECTIONSTART);
        int selb = call_sci(SCI_GETSELECTIONEND);
        int a = call_sci(SCI_LINEFROMPOSITION, sela);
        int b = call_sci(SCI_LINEFROMPOSITION, selb);


        /*
         *   if the endpoint is at the start of a line, end at the end of the
         *   previous line
         */
        if (b > 0 && call_sci(SCI_POSITIONFROMLINE, b) == selb)
            --b;

        /*
         *   If the first line already starts with a comment marker, and the
         *   last line ends with a comment marker, we'll remove comment
         *   markers from the region; otherwise we'll add them.
         */
        SciLine lin(sci_, a);
        int adding = !re_match(lin.txt, "<space>*/%*<space>");
        lin.retrieve_line(b);
        adding |= !re_match(lin.txt, ".*<space>%*/<space>*$");

        /*
         *   if there are any embedded comments within the region, also
         *   assume that we're adding comments - if this was a region we
         *   commented previously, it can't have any embedded comments since
         *   we'll have changed those to the /# ... #/ format.
         */
        lin.retrieve_line(a);
        char *p = lin.txt;
        if (!adding
            && (p += re_match(lin.txt, "<space>*/%*<space>")) != 0
            && re_search(1, &p, &lin, "/%*")
            && lin.lin <= b)
            adding = TRUE;

        /* back to the starting line */
        lin.retrieve_line(a);

        /* process each line */
        for (int depth = 0 ;; lin.go_next())
        {
            /* change star-slash to /# and slash-star to #/, and vice versa */
            if (adding)
            {
                for (p = lin.txt ; *p != '\0' ; ++p)
                {
                    if (p[0] == '/' && p[1] == '*')
                        p[1] = '#';
                    else if (p[0] == '*' && p[1] == '/')
                        p[0] = '#';
                }
            }
            else
            {
                for (p = lin.txt ; *p != '\0' ; ++p)
                {
                    if (p[0] == '/' && p[1] == '#')
                    {
                        ++depth;
                        if (depth == 1)
                            p[1] = '*';
                    }
                    else if (p[0] == '#' && p[1] == '/')
                    {
                        --depth;
                        if (depth == 0)
                            p[0] = '*';
                    }
                }
            }

            /* flush the line to scintilla */
            lin.flush();

            /* add or remove slash-star in the first line */
            if (lin.lin == a)
            {
                int indent = re_match(lin.txt, "<space>*");
                lin.splice(indent, adding ? 0 : 3, adding ? "/* " : "");
            }

            /* add or remove star-slash at the end of the last line */
            if (lin.lin == b)
            {
                if (adding)
                    lin.splice(lin.len, 0, " */");
                else if ((p = strrstr(lin.txt, " */")) != 0)
                    lin.splice(p - lin.txt, 3, "");
            }

            /* stop after we've processed the last line */
            if (lin.lin >= b)
                break;
        }

        /* done - end the undo group */
        call_sci(SCI_ENDUNDOACTION);
    }

    static char *strrstr(char *txt, const char *sub)
    {
        size_t sublen = strlen(sub);
        size_t txtlen = strlen(txt);
        if (sublen > txtlen)
            return 0;

        for (char *p = txt + txtlen - sublen ; p >= txt ; --p)
        {
            if (memcmp(p, sub, sublen) == 0)
                return p;
        }
        return 0;
    }
};



/* ------------------------------------------------------------------------ */
/*
 *   The HTML extension object.
 */
class HtmlAddIn: public CTadsWorkbenchExtension
{
public:
    HtmlAddIn(ITadsWorkbench *itwb)
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
            return new HtmlEditorMode(itwb_);

        case 1:
            return new XmlEditorMode(itwb_);

        case 2:
            return new CSSEditorMode(itwb_);

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

/* create the HTML extension */
CTadsWorkbenchEditor *HtmlEditorMode::create_ext(HWND hwnd)
{
    return new HtmlEditor(itwb_, hwnd);
}

/* create the XML extension */
CTadsWorkbenchEditor *XmlEditorMode::create_ext(HWND hwnd)
{
    return new XmlEditor(itwb_, hwnd);
}

/* create the CSS extension */
CTadsWorkbenchEditor *CSSEditorMode::create_ext(HWND hwnd)
{
    return new CSSEditor(itwb_, hwnd);
}


/* ------------------------------------------------------------------------ */
/*
 *   Connect.  Workbench calls this at start-up to initialize the DLL and
 *   exchange interface pointers.
 */
__declspec(dllexport) IUnknown *twb_connect(ITadsWorkbench *itwb)
{
    /* create and return our extension object */
    return new HtmlAddIn(itwb);
}
