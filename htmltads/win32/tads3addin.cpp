/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tads3addin.cpp - Workbench extension: editor customizations for TADS 3
Function
  This is a TADS Workbench add-in that provides customized indenting and
  styling for TADS 3 source files opened in Workbench's integrated Scintilla
  editor.

  See itadsworkbench.h for details on how to create Workbench extensions.
Notes

Modified
  10/05/06 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include <windows.h>

#include <scintilla.h>
#include <scilexer.h>

#include "wb_addin.h"
#include "tads3addin.h"


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
 *   Globals
 */


/*
 *   T3 editor extension sub-languages.  Since TADS 3 is so C-like, we can
 *   reuse it for other C-like languages of interest in Workbench, such as
 *   Javascript.
 */
enum t3e_sublang_t
{
    T3E_SUBLANG_T3,                                          /* TADS 3 mode */
    T3E_SUBLANG_JS                                       /* Javascript mode */
};


/* ------------------------------------------------------------------------ */
/*
 *   TADS 3 Library editor mode - this is little more than a plain-text mode,
 *   but we do provide the custom filename filter.
 */
class Tads3LibEditorMode: public CTadsWorkbenchEditorMode
{
public:
    Tads3LibEditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"TADS 3 Library");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str[] = L"TADS 3 Libraries (*.tl)\0*.tl\0";

        *priority = 200;
        return SysAllocStringLen(str, sizeof(str)/sizeof(str[0]) - 1);
    }

    /* check for a filename affinity */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* we recognize .tl files */
        size_t len = wcslen(filename);
        if (len > 3 && (wcsicmp(filename + len - 3, L".tl") == 0))
        {
            /* we matched on filename extension */
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
            /* TADS library files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_TL_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"TADS 3 Library (.tl)");
            wcscpy(info->desc, L"A TADS 3 library file");
            wcscpy(info->defext, L"tl");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

protected:
    /*
     *   create our extension - we're a plain-text mode, so we don't use an
     *   extension
     */
    CTadsWorkbenchEditor *create_ext(HWND hwnd) { return 0; }

    /* get my style array */
    virtual const style_info *get_style_array() { return styles_; }
    virtual UINT get_style_count();

    /* default colors for the styles */
    static const style_info styles_[];
};

/* our style list - we just use the "palin text" style */
const style_info Tads3LibEditorMode::styles_[] =
{
    { 0, L"Text", L"Sample Text",
      WBSTYLE_DEFAULT_COLOR, WBSTYLE_DEFAULT_COLOR, 0, 0 }
};

/* get style count */
UINT Tads3LibEditorMode::get_style_count() { return countof(styles_); }


/* ------------------------------------------------------------------------ */
/*
 *   TADS 3 editor mode
 */
class Tads3EditorMode: public CTadsWorkbenchEditorMode
{
public:
    Tads3EditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"TADS 3");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str_open[] = L"TADS Source Files (*.t, *.h)\0*.t;*.h\0";
        wchar_t str_save[] = L"TADS Sources (*.t)\0*.t\0"
                             L"TADS Headers (*.h)\0*.h\0";

        /* use a high priority for TADS 3 files, for obvious reasons */
        *priority = 100;

        return (for_save
                ? SysAllocStringLen(str_save, countof(str_save))
                : SysAllocStringLen(str_open, countof(str_open)));
    }

    /*
     *   check to see if we want to provide editor customizations for a file
     */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* we recognize .t and .h files */
        size_t len = wcslen(filename);
        if (len > 2
            && (wcsicmp(filename + len - 2, L".t") == 0
                || wcsicmp(filename + len - 2, L".h") == 0))
        {
            /* we matched on filename extension */
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
            /* TADS Source files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_T_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"TADS Source File (.t)");
            wcscpy(info->desc, L"A TADS source code file");
            wcscpy(info->defext, L"t");
            return TRUE;

        case 1:
            /* TADS Header files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_H_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"TADS Header File (.h)");
            wcscpy(info->desc, L"A TADS source-code header (#include) file");
            wcscpy(info->defext, L"h");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

protected:
    /* create our extension */
    CTadsWorkbenchEditor *create_ext(HWND hwnd);

    /* get my style array */
    virtual const style_info *get_style_array() { return styles_; }
    virtual UINT get_style_count();

    /* default colors for the styles */
    static const style_info styles_[];
};

/* our style list */
const style_info Tads3EditorMode::styles_[] =
{
    { SCE_T3_DEFAULT, L"Miscellaneous", L"Sample Text",
      WBSTYLE_DEFAULT_COLOR, WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_X_DEFAULT, L"Embedded << >> expressions",
      L"<< isLit ? 'lit' : 'unlit'>>",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_PREPROCESSOR, L"Preprocessor directives", L"#define ABC 123",
      RGB(0x80,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_BLOCK_COMMENT, L"Comment - block style", L"/* comment */",
      RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_LINE_COMMENT, L"Comment - line style", L"// comment",
      RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_OPERATOR, L"Operator", L"+ - * / % ++ --",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_KEYWORD, L"Keyword", L"if else for while",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_NUMBER, L"Number", L"17 123.45 6.2e-17",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_IDENTIFIER, L"Identifier", L"Thing name desc",
      RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_S_STRING, L"String - single-quoted", L"'string'",
      RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_D_STRING, L"String - double-quoted", L"\"String.\"",
      RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_X_STRING, L"String in << >> expressions", L"<< \"String\" >>",
      RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_LIB_DIRECTIVE, L"Style tag", L"<.parser> <./parser>",
      RGB(0x00,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_MSG_PARAM, L"Message parameters", L"{the dobj/hi, 0m}",
      RGB(0x80,0x00,0x40), WBSTYLE_DEFAULT_COLOR, WBSTYLE_ITALIC, 0 },
    { SCE_T3_HTML_TAG, L"HTML tag", L"<p> <a>",
      RGB(0x80,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_HTML_DEFAULT, L"HMTL attribute", L"href",
      RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_HTML_STRING, L"HTML attribute string", L"'test.htm'",
      RGB(0x80,0x80,0x80), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { 34, L"Brace Highlight", L"{ ( ), 0 }",
      RGB(0x00,0x00,0x00), RGB(0x7F,0xFF,0xD4), 0, 0 },
    { SCE_T3_BRACE, L"Brace", L"{, 0 }",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_T3_USER1, L"User Keywords 1", L"user keyword",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 1 },
    { SCE_T3_USER2, L"User Keywords 2", L"user keyword",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 2 },
    { SCE_T3_USER3, L"User Keywords 3", L"user keyword",
      RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 3 }
};

/* get style count */
UINT Tads3EditorMode::get_style_count()
{
    return countof(styles_);
}

/* ------------------------------------------------------------------------ */
/*
 *   TADS 3 editor mode
 */
class JavascriptEditorMode: public CTadsWorkbenchEditorMode
{
public:
    JavascriptEditorMode(ITadsWorkbench *itwb)
        : CTadsWorkbenchEditorMode(itwb)
    {
    }

    /* get my mode name */
    BSTR STDMETHODCALLTYPE GetModeName()
    {
        return SysAllocString(L"Javascript");
    }

    /* get my filter string */
    BSTR STDMETHODCALLTYPE GetFileDialogFilter(BOOL for_save, UINT *priority)
    {
        wchar_t str_open[] = L"Javascript Files (*.js)\0*.js\0";
        wchar_t str_save[] = L"Javascript Files (*.js)\0*.js\0";

        /* use a high priority for JS files, for obvious reasons */
        *priority = 100;

        return (for_save
                ? SysAllocStringLen(str_save, countof(str_save))
                : SysAllocStringLen(str_open, countof(str_open)));
    }

    /*
     *   check to see if we want to provide editor customizations for a file
     */
    UINT STDMETHODCALLTYPE GetFileAffinity(const OLECHAR *filename)
    {
        /* we recognize .js files */
        size_t len = wcslen(filename);
        if (len > 3 && wcsicmp(filename + len - 3, L".js") == 0)
        {
            /* we matched on filename extension */
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
            /* Javascript .js files */
            info->icon = (HICON)LoadImage(
                G_instance, MAKEINTRESOURCE(ICON_JS_FILE),
                IMAGE_ICON, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_DEFAULTCOLOR);
            info->need_destroy_icon = FALSE;
            wcscpy(info->name, L"Javascript file (.js)");
            wcscpy(info->desc, L"A Javascript file");
            wcscpy(info->defext, L"js");
            return TRUE;

        default:
            /* no more modes */
            return FALSE;
        }
    }

protected:
    /* create our extension */
    CTadsWorkbenchEditor *create_ext(HWND hwnd);

    /* get my style array */
    virtual const style_info *get_style_array() { return styles_; }
    virtual UINT get_style_count();

    /* default colors for the styles */
    static const style_info styles_[];
};

/* our style list */
const style_info JavascriptEditorMode::styles_[] =
{
    { SCE_C_DEFAULT, L"Misellaneous", L"Sample Text",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_COMMENT, L"Comment - block style", L"/* comment */",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_COMMENTLINE, L"Comment - line style", L"// comment",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_COMMENTDOC, L"Documentation comment", L"/// comment",
    RGB(0x00,0x80,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_NUMBER, L"Number", L"17 123.45 6.2e-17",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_IDENTIFIER, L"Identifier", L"i j length",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_WORD, L"Keyword", L"if else for while",
    RGB(0x00,0x00,0xFF), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_WORD, L"Keyword", L"if else for while",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_STRING, L"String in double quotes", L"\"string\"",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_CHARACTER, L"String in single quotes", L"'string'",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_OPERATOR, L"Operator", L"+ - * / % ++ --",
    RGB(0x00,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_STRINGEOL, L"End-of-line in string", L"\"String...",
    RGB(0xC0,0x00,0x00), WBSTYLE_DEFAULT_COLOR, 0, 0 },
    { SCE_C_REGEX, L"Regular expression", L"/\\s+(\\w+)\\s+/g",
    RGB(0xff,0x00,0xff), WBSTYLE_DEFAULT_COLOR, 0, 0 }
};

/* get style count */
UINT JavascriptEditorMode::get_style_count()
{
    return countof(styles_);
}

/* ------------------------------------------------------------------------ */
/*
 *   tokenizer modes for auto-indent
 */
#define TOK_MODE_CODE      1                  /* code outside an expression */
#define TOK_MODE_EXPR      2                   /* code inside an expression */
#define TOK_MODE_COMMENT   3                                     /* comment */
#define TOK_MODE_SSTRING   4                        /* single-quoted string */
#define TOK_MODE_SSTRING3  5                       /* triple-quoted sstring */
#define TOK_MODE_DSTRING   6                        /* double-quoted string */
#define TOK_MODE_DSTRING3  7                       /* triple-quoted dstring */
#define TOK_MODE_PREPROC   8                        /* preprocess directive */

/* token context information for auto-indent */
struct tok_info
{
    /* indentation level for continuation lines of last string */
    int str_indent;

    /*
     *   indentation level of the last unclosed open paren; this is -1 if
     *   we're not inside a parenthesized expression
     */
    int paren_col;

    /*
     *   statement indentation - this is the indentation level we'll use in
     *   ordinary code outside any expressions
     */
    int stm_indent;

    /*
     *   statement start indentation - this is the indentation level for the
     *   start of each statement at the current level
     */
    int stm_start_indent;

    /* are we in an object's property list? */
    int in_prop_list;

    /* parsing mode at this level */
    int mode;

    /* previous mode, on exiting from current string or # directive */
    int prv_mode;

    /* are we inside an object definition? */
    int in_obj_def;

    /* are we in a function definition? */
    int in_func_def;

    /* are we in a continuation line? */
    int is_continuation;

    /* line number of start of most recent statement */
    int stm_start_line;

    /* are we in an embedded expression? */
    int in_embed;

    /* are we in an anonymous function? */
    int in_anon_fn;

    /* brace nesting level */
    int braces;
};

/* maximum depth of the delimiter stack for auto-indent parsing */
#define PAREN_STACK_MAX 100

/* information on a brace/paren level */
struct paren_info
{
};

/* delimiter stack */
struct paren_stack
{
    /* stack pointer */
    int sp;

    /* stack slots */
    struct tok_info stk[PAREN_STACK_MAX];
};

/* ------------------------------------------------------------------------ */
/*
 *   The TADS 3 editor customizer
 */
class Tads3Editor: public CTadsWorkbenchEditor
{
public:
    Tads3Editor(ITadsWorkbench *itwb, HWND hwnd, t3e_sublang_t lang)
        : CTadsWorkbenchEditor(itwb, hwnd)
    {
        /* remember the language mode */
        lang_ = lang;
    }

    /*
     *   ITadsWorkbenchEditor implementation
     */

    /* initialize */
    void init()
    {
        /* inherit the default handling */
        CTadsWorkbenchEditor::init();

        /* set up for the languag emode */
        switch (lang_)
        {
        case T3E_SUBLANG_T3:
            /* tell Scintilla to use the "tads3" lexer */
            call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"tads3");
            call_sci(SCI_COLOURISE, 0, -1);
            call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");

            /* send the lexer the list of TADS 3 keywords */
            call_sci(SCI_SETKEYWORDS, 0, (int)
                     "argcount break case catch class continue default "
                     "delegated definingobj dictionary do else enum "
                     "export extern external false finally for foreach "
                     "function goto grammar if import inherited intrinsic "
                     "invokee "
                     "local method modify new nil object operator property "
                     "propertyset "
                     "replace replaced return self static switch targetobj "
                     "targetprop template throw transient true try "
                     "while");

            /* send the list of "word" characters */
            call_sci(SCI_SETWORDCHARS, 0, (int)
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "0123456789_#");
            break;

        case T3E_SUBLANG_JS:
            /* tell Scintilla to use the "javascript" lexer */
            call_sci(SCI_SETLEXERLANGUAGE, 0, (int)"cpp");
            call_sci(SCI_COLOURISE, 0, -1);
            call_sci(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");

            /* send the lexer the list of TADS 3 keywords */
            call_sci(SCI_SETKEYWORDS, 0, (int)
                     "break case catch const continue default "
                     "delete do else export false finally for foreach "
                     "function goto if import in instanceOf label let "
                     "new return switch this throw true try typeof "
                     "var void while with yield");

            /* send the list of "word" characters */
            call_sci(SCI_SETWORDCHARS, 0, (int)
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "0123456789_#$");
            break;
        }
    }

    /*
     *   Receive notification of a Workbench settings change
     */
    void STDMETHODCALLTYPE OnSettingsChange()
    {
        int i;

        /* do the base-class work */
        CTadsWorkbenchEditor::OnSettingsChange();

        /* send the lexer the user-defined custom keywords */
        for (i = 1 ; i <= 3 ; ++i)
        {
            wchar_t key[50];
            BSTR bval;

            /* look up this custom keyword set */
            swprintf(key, L"custom-keywords-%d", i);
            if ((bval = itwb_->GetPropValStr(
                TRUE, L"TADS 3-editor-prefs", key, 0)) != 0)
            {
                /* translate it to ANSI */
                size_t len = WideCharToMultiByte(
                    CP_ACP, 0, bval, SysStringLen(bval), 0, 0, 0, 0);
                char *aval = new char[len + 1];
                WideCharToMultiByte(
                    CP_ACP, 0, bval, SysStringLen(bval), aval, len + 1, 0, 0);
                aval[len] = '\0';

                /* set the keyword list */
                call_sci(SCI_SETKEYWORDS, i, (int)aval);

                /* done with the strings */
                delete [] aval;
                SysFreeString(bval);
            }
        }

        /*
         *   we need to re-style the whole buffer, since the lexer needs to
         *   re-examine each token in light of the new keyword lists to
         *   determine the appropriate custom keyword styles
         */
        call_sci(SCI_COLOURISE, 0, -1);
    }


    /*
     *   Window message filter.
     */
    LRESULT STDMETHODCALLTYPE MsgFilter(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
        ITadsWorkbenchMsgHandler *def_handler)
    {
        switch (msg)
        {
        case WM_CHAR:
            /* check for auto-indent characters */
            if ((auto_indent_ == TWB_AUTOINDENT_SIMPLE
                 && strchr("\n\r", wparam) != 0)
                || (auto_indent_ == TWB_AUTOINDENT_SYNTAX
                    && strchr("\n\r{}[]:;", wparam) != 0)
                || (wparam == '/' && format_comments_ != TWB_FMTCOMMENT_OFF))
            {
            undo_group:
                /*
                 *   This character might trigger auto-indenting or comment
                 *   formatting.  Open an undo group, so that the insertion
                 *   plus the resulting auto-indent will be undone as a
                 *   group.  Note that we can't tell for sure at this point
                 *   that any reformatting will actually be triggered, but
                 *   even if it doesn't, the worst that happens is that we'll
                 *   have added an unnecessary undo checkpoint.  That's not
                 *   so bad, since the grouping of undo operations while
                 *   typing is a bit arbitrary anyway.
                 */
                call_sci(SCI_BEGINUNDOACTION);

                /* process the keystroke through the Scintilla window */
                LRESULT ret = CTadsWorkbenchEditor::MsgFilter(
                    hwnd, msg, wparam, lparam, def_handler);

                /* end the undo group */
                call_sci(SCI_ENDUNDOACTION);

                /* we've handled it, so just return the result */
                return ret;
            }
            break;

        case WM_KEYDOWN:
            /* check for auto-indent keys */
            if (auto_indent_ != TWB_AUTOINDENT_NONE && wparam == VK_RETURN)
                goto undo_group;
            break;
        }

        /* inherit the default handling to pass the message to Scintilla */
        return CTadsWorkbenchEditor::MsgFilter(
            hwnd, msg, wparam, lparam, def_handler);
    }


    /*
     *   Receive notification of an added character
     */
    void STDMETHODCALLTYPE OnCharAdded(UINT ch)
    {
        /* inherit the default handling */
        CTadsWorkbenchEditor::OnCharAdded(ch);

        /* check for auto comment indenting */
        if (ch == '/' && format_comments_ != TWB_FMTCOMMENT_OFF)
        {
            /* get the current line */
            int pos = call_sci(SCI_GETCURRENTPOS);
            int lin = call_sci(SCI_LINEFROMPOSITION, pos);
            SciLine txt(sci_, lin);

            /* check to see if the previous character was a '*' */
            char *p = txt.getpostxt(pos);
            if (p >= txt.txt + 2 && *(p-2) == '*' && *(p-1) == '/')
            {
                /* do the indenting */
                syntax_indent(lin);
            }
        }
    }

    /* check command status */
    BOOL STDMETHODCALLTYPE GetCommandStatus(
        UINT command_id, UINT *status, HMENU menu, UINT menu_index)
    {
        /* our custom commands are always enabled */
        if (command_id == indent_cmd
            || command_id == commentfmt_cmd
            || command_id == stringfmt_cmd
            || command_id == addcomment_cmd)
        {
            *status = TWB_CMD_ENABLED;
            return TRUE;
        }

        /* it's not ours - inherit the default handling */
        return CTadsWorkbenchEditor::GetCommandStatus(
            command_id, status, menu, menu_index);
    }

    /*
     *   Carry out a command.  By default, we don't add any commands of our
     *   own or override any Workbench commands, so we simply return FALSE to
     *   let Workbench carry out the default processing.
     */
    BOOL STDMETHODCALLTYPE DoCommand(UINT command_id)
    {
        /* remap the Fill Paragraph command according to context */
        if (command_id == fill_para_cmd)
        {
            /* get the style at our cursor position */
            int curpos = call_sci(SCI_GETCURRENTPOS);
            int anchor = call_sci(SCI_GETANCHOR);
            int style = call_sci(SCI_GETSTYLEAT, curpos);

            /* format based on the style at the cursor position */
            if (anchor != curpos)
            {
                /* there's a range selected, so reindent the whole region */
                command_id = indent_cmd;
            }
            else if (style == SCE_T3_BLOCK_COMMENT)
            {
                /* we're in a comment, so treat this as a comment reformat */
                command_id = commentfmt_cmd;
            }
            else if (format_string(call_sci(SCI_GETCURRENTPOS)))
            {
                /* we found a string to reformat, so consider it done */
                return TRUE;
            }
            else
            {
                /* for anything else, treat this as a reindent */
                command_id = indent_cmd;
            }
        }

        /* it's not ours - inherit the default handling */
        return CTadsWorkbenchEditor::DoCommand(command_id);
    }

protected:
    /* hot characters for syntax indenting */
    virtual const char *hot_chars() const { return "\n\r{}[]:;"; }

    /* do comment fill */
    virtual int format_comment(int pos)
    {
        /* if we're in a comment, do the comment reformatting */
        tok_info info;
        int curline = call_sci(SCI_LINEFROMPOSITION, pos);
        if (get_tok_mode(&info, curline) == TOK_MODE_COMMENT)
        {
            /* do the formatting in an undo group */
            format_c_comment_undo(curline);

            /* handled */
            return TRUE;
        }

        /* we didn't do any reformatting */
        return FALSE;
    }

    /* do syntax-based auto-indenting on the given line */
    virtual void syntax_indent(int curline)
    {
        /* find the token mode of the current code */
        tok_info info;
        paren_stack ps;
        int mode = get_tok_mode(&info, curline, ps);

        /* now indent the line according to the current mode */
        indent_line(mode, info, curline);
    }

    /* syntax-indent a range */
    void syntax_indent_range(int start, int a, int end, int b)
    {
        /* set a spot at the start and end of the range */
        LPARAM spot_start = create_spot(start);
        LPARAM spot_end = create_spot(end);

        /* get the token mode at the start of the region */
        tok_info info;
        paren_stack ps;
        int mode = get_tok_mode(&info, a, ps);

        /* indent each line until we reach the marker */
        for (;;)
        {
            /* set the start spot to the starting line */
            set_spot_pos(spot_start, call_sci(SCI_POSITIONFROMLINE, a));

            /* indent this line */
            indent_line(mode, info, a);

            /* go back to the starting line */
            a = call_sci(SCI_LINEFROMPOSITION, get_spot_pos(spot_start));

            /* parse forward to the next line */
            parse_forward(&info, a, ps);
            mode = info.mode;

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

    /* indent a line */
    void indent_line(int mode, tok_info &info, int curline)
    {
        int ind = call_sci(SCI_GETINDENT);
        SciLine lin(sci_, curline);
        int firstchar, lastchar;
        char *p, *q;
        char summary[10];
        int i;

        /* find the first and last non-space characters on the line */
        for (firstchar = lastchar = 0, p = lin.txt ; *p != '\0' ; ++p)
        {
            if (!isspace(*p))
            {
                lastchar = *p;
                if (firstchar == 0)
                    firstchar = *p;
            }
        }

        /*
         *   Make a "summary" of the text on the line: this is the text
         *   stripped of spaces, up to the size of the summary buffer.  We
         *   use this for simple pattern matching for certain delimiter
         *   sequences.  If the stripped text is too long for the buffer,
         *   it's too long to match any of our special delimiter sequences,
         *   so it doesn't matter that there's extra text.
         */
        for (p = lin.txt, q = summary ;
             *p != 0 && q < summary + countof(summary) - 1 ; ++p)
        {
            /* copy non-space characters only */
            if (!isspace(*p))
                *q++ = *p;
        }
        *q = '\0';

        /* indent according to our mode */
        switch(mode)
        {
        case TOK_MODE_COMMENT:
            /* indent a comment line */
            indent_comment(curline);
            break;

        case TOK_MODE_SSTRING:
        case TOK_MODE_DSTRING:
        case TOK_MODE_SSTRING3:
        case TOK_MODE_DSTRING3:
            /* we're in a string - use the continuing string indentation */
            indent_to_column(curline, info.str_indent);
            break;

        case TOK_MODE_EXPR:
            /*
             *   In an expression - indent to the paren indent level.
             *
             *   If we have a stand-alone close square bracket, or a close
             *   square bracket and a close brace, it's probably the end of a
             *   nested object definition using a list template.  Pull it
             *   back a level if so.  If it's a close bracket and a
             *   semicolon, it's probably the end of a local variable
             *   initializer list, so pull this in a level as well.
             *
             *   If we have a stand-alone open brace, indent to the statement
             *   level instead, since this must be the start of an anonymous
             *   function.
             */
            if (firstchar == ']' && info.stm_indent >= ind)
            {
                /*
                 *   If this is a lone close bracket, or a close bracket plus
                 *   a close brace, bring it in a level.
                 */
                if (strcmp(summary, "]") == 0
                    || strcmp(summary, "]}") == 0
                    || strcmp(summary, "];") == 0)
                {
                    /* pull it in a level */
                    indent_to_column(curline, info.paren_col - ind);
                }
                else
                {
                    /* use the default expression level */
                    indent_to_column(curline, info.paren_col);
                }
            }
            else if (strcmp(summary, "{") == 0)
            {
                /*
                 *   if we're entering an anonymous function, indent to
                 *   statement level; otherwise indent to expression level
                 */
                indent_to_column(curline,
                                 info.in_anon_fn
                                 ? info.stm_indent : info.paren_col);
            }
            else
            {
                /* indent to the expression indentation column */
                indent_to_column(curline, info.paren_col);
            }
            break;

        case TOK_MODE_PREPROC:
            /*
             *   We're in a preprocessor directive.  If this is the first
             *   line of the directive (i.e., the previous line doesn't end
             *   with '\'), simply indent to the left margin.  Otherwise, if
             *   we're in a parenthesized expression, indent like an
             *   expression.  Otherwise, indent to the previous line's
             *   indentation if it's indented, otherwise by one indent level.
             */

            /* presume this is a continuation line */
            i = (info.paren_col != 0 ? info.paren_col : ind);

            /* check to see if this is really a continuation line */
            if (curline != 0)
            {
                /* retrieve the prior line, and check for ending in '\' */
                SciLine lin2(sci_, curline - 1);
                if (lin2.len == 0 || lin2.txt[lin2.len - 1] != '\\')
                {
                    /*
                     *   the previous line doesn't end in '\', so this isn't
                     *   a continuation line after all - simply put the
                     *   directive in the first column
                     */
                    i = 0;
                }
            }
            else
            {
                /* this is the first line, so it can't be a continuation */
                i = 0;
            }

            /* indent to our chosen point */
            indent_to_column(curline, i);
            break;

        case TOK_MODE_CODE:
            /*
             *   We're in regular code.
             *
             *   If the current character is an open brace, indent to the
             *   statement start point.  If it's a close brace, indent back
             *   one level from the current statement indentation, if
             *   possible.
             *
             *   Otherwise, if the current line looks like a label, indent
             *   back one level from the current statement indentation.
             *
             *   Otherwise, if the current character is a semicolon, and it's
             *   the only thing on the line, and we're at the first
             *   indentation level, it's an object terminator, so put it at
             *   column 1.
             *
             *   If it's a stand-alone open bracket, pull it back a level.
             *
             *   If we're in a property list, and we're right after a line
             *   that looks like "propname =" - without any value - then we
             *   must be putting the value on a separate line.  Indent it one
             *   level from the property name.
             *
             *   Otherwise, use the current statement indentation.
             */
            if (is_preproc_line(lin.txt))
            {
                /* use preprocessor indenting */
                indent_to_column(curline, 0);
            }
            else if (firstchar == '{')
            {
                /*
                 *   if it's a one-liner method definition (it starts and
                 *   ends with braces), indent one level in from the
                 *   statement start indentation; otherwise, it's a normal
                 *   block start, so indent to the statement start level
                 */
                if (firstchar == '{' && lastchar == '}')
                {
                    /* it's a one-liner method - indent one level in */
                    indent_to_column(curline, info.stm_start_indent + ind);
                }
                else
                {
                    /* use the statement start level */
                    indent_to_column(curline, info.stm_start_indent);
                }
            }
            else if (firstchar == '}' && info.stm_indent >= ind)
            {
                /* move back one level */
                indent_to_column(curline, info.stm_start_indent - ind);
            }
            else if (is_label(lin.txt)
                     && info.stm_indent >= ind && !info.in_prop_list)
            {
                /* move back one level */
                indent_to_column(curline, info.stm_start_indent - ind);
            }
            else if (strcmp(summary, ";") == 0 && info.stm_indent == ind)
            {
                /* closing semicolon of an object definition - column 1 */
                indent_to_column(curline, 0);
            }
            else if (strcmp(summary, "[") == 0 && info.stm_indent > ind)
            {
                /*
                 *   pull it back one level, unless the previous line is
                 *   itself a bare delimiter
                 */
                int newind = info.stm_indent - ind;
                if (curline > 0)
                {
                    SciLine lin2(sci_, curline - 1);
                    if (re_match(lin2.txt, "<space>*[[{(]<space>*$"))
                        newind += ind;
                }

                /* indent to the level we decided upon */
                indent_to_column(curline, newind);
            }
            else if (info.in_prop_list
                     && firstchar != '[' && firstchar != '{'
                     && curline > 0)
            {
                /*
                 *   we're in a property list; check the previous line to see
                 *   if this is a property value following its "property=" on
                 *   a separate line
                 */
                SciLine lin2(sci_, curline - 1);
                if (re_match(lin2.txt,
                             "<space>+<alpha|_><alphanum|_>*"
                             "<space>*=<space>*$"))
                {
                    /*
                     *   this is a continuation of a "property=" line - this
                     *   is the value part on a separate line, so indent it
                     *   one more level
                     */
                    indent_to_column(curline, info.stm_indent + ind);
                }
                else
                {
                    /* otherwise, just use the current statement indent */
                    indent_to_column(curline, info.stm_indent);
                }
            }
            else
            {
                /* use the current statement indentation */
                indent_to_column(curline, info.stm_indent);
            }
            break;
        }
    }

    /* push a paren */
    void push_paren(paren_stack *s, tok_info *info)
    {
        /* save the information, if we're at a valid level */
        if (s->sp >= 0 && s->sp < PAREN_STACK_MAX)
            s->stk[s->sp] = *info;

        /* increment the stack pointer */
        s->sp++;
    }

    /* pop a paren */
    void pop_paren(struct paren_stack *s, tok_info *info)
    {
        /* decrement the stack pointer */
        s->sp--;

        /* restore the information, if we're at a valid level */
        if (s->sp < 0)
        {
            /*
             *   We're not at a valid level.  Either this means the code is
             *   ill-formed, or we started scanning from a method inside a
             *   nested object definition.  Assume the latter case.  This
             *   means we're returning to code mode.
             *
             *   We should already be in a property list, so we don't need to
             *   change that flag; and we should already be out of all
             *   embedded expressions, so that one doesn't need to change.
             *   The expression indent level doesn't matter, since we're not
             *   in an expression.
             */
            memset(info, 0, sizeof(*info));
            info->mode = TOK_MODE_CODE;
        }
        else if (s->sp < PAREN_STACK_MAX)
        {
            /* pop the level */
            *info = s->stk[s->sp];
        }
    }

    /*
     *   Get the token mode of the current location in the buffer
     *
     *   TADS mode is difficult because strings can go on for many lines.
     *   Our best strategy is to make a quick scan back for the start of an
     *   object, method, or function definition, and then scan forwards to
     *   figure out our context.  An object or function definition starts in
     *   the first column, which is almost unique to objects and functions,
     *   so we can make a very quick scan for this.  A method definition,
     *   almost uniquely, starts at the first indent position.
     */
    int get_tok_mode(tok_info *info, int curline)
    {
        /* get the mode using local parse state information */
        paren_stack ps;
        return get_tok_mode(info, curline, ps);
    }

    /* get the token mode, using the caller's parse state information */
    int get_tok_mode(tok_info *info, int curline, paren_stack &ps)
    {
        SciLine lin(sci_);
        int ind = call_sci(SCI_GETINDENT);

        /* initialize the paren stack */
        ps.sp = 0;

        /* clear out our tokenizer context structure */
        memset(info, 0, sizeof(*info));

        /* start at the current line */
        lin.lin = curline;

        /*
         *   if we're at the first line, there's nothing earlier to scan, so
         *   we're going to skip the loop over the previous material; use an
         *   empty prior line in this case
         */
        if (lin.lin == 0)
            lin.retrieve_line(0);

        /*
         *   Scan backwards so that we can decide whether we're in a function
         *   definition, object definition, or something else entirely.  We
         *   can stop when we find something in the first column, since this
         *   will always give us an outer-level definition.  We can also stop
         *   if we find anything that's distinctively in the realm of an
         *   object definition.
         */
        while (lin.lin > 0)
        {
            /* move to the start of the previous line */
            lin.retrieve_line(lin.lin - 1);

            /* check for text in the first column */
            if (lin.len != 0 && !isspace(lin.txt[0]))
            {
                /* skip labels, preprocessor directives, and comments */
                if (re_match(lin.txt,
                             "<alpha|_><alphanum|_>*<space>*:<space>*$"
                             "|#.*|/*|//"))
                {
                    /* it's not an object/function start - skip it */
                    continue;
                }

                /*
                 *   this must be an object/function start - we've found what
                 *   we're looking for
                 */
                break;
            }

            /*
             *   If we have something that looks like a method definition, we
             *   must be in an object definition.  Certain ordinary
             *   statements look like method definitions, so first check for
             *   those and skip them.
             */
            if (re_match(lin.txt, "<space>*(while|if|switch)<space>*%("))
                continue;

            /*
             *   Check for things that look like method definitions.  Only
             *   take a definition at the first indent position as a method
             *   definition; this won't find nested methods on this pass, but
             *   we'll still catch them properly when we scan forward.  It's
             *   safer to only stop on top-level methods.
             */
            if (re_match(lin.txt,
                         "<space>*<alpha|_><alphanum|_>*<space>*%(<space>*"
                         "<alpha|_><alphanum|_>*<space>*"
                         "(,<space>*<alpha|_><alphanum|_>*)*<space>*%)"
                         "<space>*(<lbrace><space>*)?$")
                && call_sci(SCI_GETLINEINDENTATION, lin.lin) == ind)
            {
                /*
                 *   make sure that either this line ends in an open brace,
                 *   or the next line is just a brace
                 */
                int brace = re_match(lin.txt, ".*<lbrace><space>*$");
                if (!brace)
                {
                    SciLine lin2(sci_, lin.lin + 1);
                    brace = re_match(lin2.txt, "<space>*<lbrace><space>*$");
                }

                /* if we got the brace, this looks like a method def */
                if (brace)
                {
                    /* we must be in an object definition */
                    info->in_obj_def = TRUE;

                    /*
                     *   set the statement start and continuation levels to
                     *   the method definition's indentation point
                     */
                    info->stm_start_indent = info->stm_indent =
                        call_sci(SCI_GETLINEINDENTATION, lin.lin);

                    /* no need to keep scanning */
                    break;
                }
            }
        }

        /*
         *   if we don't already know we're in an object definition, check
         *   what we found
         */
        if (!info->in_obj_def)
        {
            /*
             *   If we found a brace in the first column of the line where we
             *   stopped, then we're definitely in a function definition.
             *   Otherwise, a function definition will have an open paren
             *   after the first token.
             */
            if (lin.len != 0 && lin.txt[0] == '{')
            {
                /* definitely a function definition */
                info->in_func_def = TRUE;
            }
            else if (re_match(lin.txt, "Define<alpha>*Action<space>*%("
                              "|VerbRule<space>*%("))
            {
                /* action and VerbRule definitions are object definitions */
                info->in_func_def = FALSE;
                info->in_obj_def = TRUE;
            }
            else
            {
                /* check for function syntax */
                info->in_func_def = re_match(
                    lin.txt, "<space>*"
                    "((modify|replace)<space>+)?"
                    "((function|method)<space>+)?"
                    "<alphanum|_>+<space>*%([^:\n]+$");
            }

            /*
             *   If we're not in a function definition, assume we're in an
             *   object definition unless we saw a ';' or a '}', in which
             *   case we're at the top level.  If we stopped at a blank line
             *   or at a comment or preprocessor directive, we also failed to
             *   find an object definition.
             */
            info->in_obj_def =
                (!info->in_func_def
                 && lin.len != 0
                 && !re_match(lin.txt, "<;|}|#>|/<star>"));
        }

        /*
         *   if we're at a ';' or '}', we already know we're at the top
         *   level, so skip this line
         */
        if (re_match(lin.txt, "<;|}>"))
            lin.lin += 1;

        /* we're in a property list if and only if we're defining an object */
        info->in_prop_list = info->in_obj_def;

        /* now scan forward until we reach the current line */
        int l;
        for (l = lin.lin, info->mode = TOK_MODE_CODE ; l < curline ; ++l)
            parse_forward(info, l, ps);

        /* return our final mode */
        return info->mode;
    }

    /*
     *   Parse forward one line
     */
    void parse_forward(tok_info *info, int curline, paren_stack &ps)
    {
        int ind = call_sci(SCI_GETINDENT);
        SciLine lin(sci_, curline);
        char *p = lin.txt;
        int done;
        char qu;
        int triple;

        /*
         *   if we're not in a continuation line, this is the start of a
         *   statement
         */
        if (info->mode == TOK_MODE_CODE
            && !info->is_continuation
            && !info->in_embed)
            info->stm_start_line = curline;

        /* skip whitespace */
        p = skip_spaces(p);

        /* scan until we reach the next line */
        for (done = FALSE ; !done ; )
        {
            /* presume we'll have a one-character token to skip */
            int toklen = 1;

            /* check the current mode */
            switch (info->mode)
            {
            case TOK_MODE_PREPROC:
                /*
                 *   preprocessor - if this line ends in a '\', the
                 *   preprocessor directive continues onto the next line;
                 *   otherwise the next line is back in the mode before the
                 *   directive
                 */
                toklen = strlen(p);
                if (toklen == 0 || p[toklen-1] != '\\')
                {
                    /*
                     *   there's no trailing '\', so the next line is no
                     *   longer in preprocessor mode
                     */
                    info->mode = info->prv_mode;
                }

                /* we're done with this line */
                done = TRUE;
                break;

            case TOK_MODE_SSTRING:
                qu = '\'';
                triple = FALSE;
                goto doStr;

            case TOK_MODE_SSTRING3:
                qu = '\'';
                triple = TRUE;
                goto doStr;

            case TOK_MODE_DSTRING:
                qu = '"';
                triple = FALSE;
                goto doStr;

            case TOK_MODE_DSTRING3:
                qu = '"';
                triple = TRUE;
                goto doStr;

            doStr:
                /* scan for the end of the string */
                for ( ; ; ++p)
                {
                    /* check for '\' escapes */
                    if (*p == '\\' && *(p+1) == qu)
                    {
                        /* skip the backslash */
                        ++p;

                        /* in triple-quote mode, skip a run of quotes */
                        if (triple)
                        {
                            /* triple-quotes - skip the run of quotes */
                            for ( ; *(p+1) == qu ; ++p) ;
                        }
                    }
                    else if (*p == '\\')
                    {
                        /* skip the \ */
                        ++p;
                    }
                    else if (*p == '<' && *(p+1) == '<')
                    {
                        /* embedded expression - skip the '<<' */
                        p += 2;

                        /* push a paren level and enter embedded mode */
                        push_paren(&ps, info);
                        info->in_embed = TRUE;
                        info->mode = TOK_MODE_EXPR;

                        /* indent the expression to align with the string */
                        info->paren_col = info->str_indent + 2;

                        /* stop processing the string */
                        break;
                    }
                    else if (*p == qu)
                    {
                        /*
                         *   matching quote - assume it's ending, then check
                         *   for a triple quote if necessary
                         */
                        if (triple)
                        {
                            /* end only on a triple quote */
                            if ((*(p+1) == qu && *(p+2) == qu))
                            {
                                /*
                                 *   skip the whole run of consecutive
                                 *   quotes: the first N-3 are part of the
                                 *   string's contents, and the last 3 are
                                 *   the delimiter
                                 */
                                for (p += 3 ; *p == qu ; ++p) ;

                                /* switch to the enclosing mode */
                                info->mode = info->prv_mode;

                                /* done with the string */
                                break;
                            }
                        }
                        else
                        {
                            /* regular string - skip the close quote */
                            ++p;

                            /* switch to the enclosing mode */
                            info->mode = info->prv_mode;

                            /* done with the string */
                            break;
                        }
                    }
                    else if (*p == '\0')
                    {
                        /* end of line - stop here */
                        done = TRUE;
                        break;
                    }
                }

                /* no need to skip any more text */
                toklen = 0;
                break;

            case TOK_MODE_COMMENT:
                /* find the end of the comment */
                for ( ; *p != '\0' && (*p != '*' || *(p+1) != '/') ; ++p) ;

                /* check where we ended */
                if (*p == '*')
                {
                    /* found the end - return to the old mode */
                    info->mode = info->prv_mode;

                    /* skip the comment ending */
                    p += 2;
                }
                else
                {
                    /* end of line - we're done */
                    done = TRUE;
                }

                /* no need to skip any more */
                toklen = 0;
                break;

            case TOK_MODE_CODE:
                /*
                 *   If we're not already in an object or function
                 *   definition, check if we're entering one.
                 */
                if (!info->in_obj_def && !info->in_func_def)
                {
                    /* presume we won't find either an object or func entry */
                    int objdef = FALSE;
                    int fndef = FALSE;

                    /*
                     *   Check for an object definition.  Look for grammar,
                     *   class, modify, replace, and "+" statements
                     *   specifically.  Also, if we have an identifier
                     *   followed by a colon, take it to be an object
                     *   definition.
                     */
                    if (re_match(p, "(grammar|class|%+)"
                                 "<space>*<alpha|_><alphanum|_>*")
                        || re_match(p, "<alpha|_><alphanum|_>*<space>*:"))
                        objdef = TRUE;

                    /*
                     *   if we have an explicit 'function' or 'method'
                     *   keyword, it's a function definition
                     */
                    if (!objdef
                        && re_match(p, "((modify|replace)<space>+)?"
                                    "(function|method)%>"))
                        fndef = TRUE;

                    /*
                     *   If we're starting in column 1, and all we have is an
                     *   identifier, scan ahead to see if we have a function
                     *   or object definition.  We'll take this to be a
                     *   function definition if we have something that looks
                     *   like an argument list followed by a brace in the
                     *   first column.
                     */
                    if (!objdef && !fndef && p == lin.txt
                        && re_match(p, "<alpha|_><alphanum|_>*"))
                    {
                        /* set up a duplicate line reader */
                        SciLine lin2(sci_, curline);
                        char *p2 = lin2.txt + (p - lin.txt);

                        /* skip any 'replace' or 'modify' keyword */
                        p2 += re_match(p2, "(replace|modify)%><space>*");

                        /* skip past the identifier */
                        for ( ; *p2 != '\0' && (isalnum(*p2) || *p2 == '_')
                              ; ++p2) ;

                        /* skip spaces */
                        p2 = skip_spaces(p2);

                        /* make sure there's an open paren */
                        if (*p2 == '(')
                        {
                            /*
                             *   now search for something that doesn't belong
                             *   in an argument list (including another open
                             *   paren - an argument list can only have the
                             *   single open paren)
                             */
                            ++p2;
                            if (re_search(1, &p2, &lin2, "[][({};:]"))
                            {
                                /*
                                 *   if we found a brace in the first column
                                 *   or the last column, take this to be a
                                 *   function; otherwise take it to be an
                                 *   object definition
                                 */
                                if (*(p2-1) == '{'
                                    && (p2 == lin2.txt + 1
                                        || *p2 == '\0'
                                        || re_match(p2, "<space>*$")))
                                    fndef = TRUE;
                                else
                                    objdef = TRUE;
                            }
                            else
                            {
                                /*
                                 *   reached eof without finding it - must be
                                 *   an object definition, since otherwise
                                 *   we'd have an open brace
                                 */
                                objdef = TRUE;
                            }
                        }
                        else
                        {
                            /* no open paren - must be an object def */
                            objdef = TRUE;
                        }
                    }

                    /* if we found an object definition, process it */
                    if (objdef)
                    {
                        /* enter object definition mode */
                        info->in_obj_def = TRUE;
                        info->in_prop_list = TRUE;
                    }
                    else if (fndef)
                    {
                        /* enter function definition mode */
                        info->in_func_def = TRUE;
                    }
                }

                /* go to the common code/expression handling */
                goto code_or_expr;

            case TOK_MODE_EXPR:
                /*
                 *   check for the 'new function()' or just plain
                 *   'function()' syntax - this switches us back to code mode
                 */
                if (re_match(p, "(new<space>+)?(function|method)"
                             "(<space>|<lparen>|$)"))
                {
                    /* note that we're now in an anonymous function */
                    info->in_anon_fn = TRUE;

                    /* skip the whole 'new function' bit */
                    p = skip_spaces(p + 3);
                    p = skip_spaces(p + 8);

                    /* move on */
                    toklen = 0;
                    break;
                }

                /*
                 *   common code/expression handling
                 */
            code_or_expr:
                switch (*p)
                {
                case '\0':
                end_of_line_in_code:
                    /*
                     *   End of the line.  If we're in ordinary code and
                     *   outside of a parenthesized expression, check to see
                     *   if the line ends in a semicolon.  If it does, indent
                     *   the next line at the same level as the most recent
                     *   statement start.  Otherwise, indent the next line
                     *   one level in from the statement start.
                     */
                    if (info->mode == TOK_MODE_CODE)
                    {
                        /*
                         *   if this is the first line of an outer object
                         *   definition, indent subsequent line starts and
                         *   continuations one level
                         */
                        if (info->in_obj_def && info->stm_start_indent == 0)
                            info->stm_start_indent = info->stm_indent = ind;

                        /*
                         *   If we're in a property list, indent to the
                         *   statement start level, since each line of a
                         *   property list is a continuation line.  Likewise,
                         *   if we're not a continuation line, indent to the
                         *   statement start level.  Otherwise, indent to the
                         *   continuation line level.
                         */
                        if (info->in_prop_list || !info->is_continuation)
                            info->stm_indent = info->stm_start_indent;
                        else if (info->is_continuation)
                            info->stm_indent = info->stm_start_indent + ind;
                    }

                    /* we've reached the end of the line, so we're done */
                    done = TRUE;

                    /* no need to skip anything in the main loop */
                    toklen = 0;

                    /* done */
                    break;

                case '#':
                    /*
                     *   '#' at the start of the line is a preprocessor
                     *   directive
                     */
                    if (p = skip_spaces(lin.txt))
                    {
                        /* enter preprocessor mode, saving the old mode */
                        info->prv_mode = info->mode;
                        info->mode = TOK_MODE_PREPROC;
                    }
                    else
                    {
                        goto normal_token;
                    }
                    break;

                case ';':
                    /* semicolon - whatever follows is not a continuation */
                    if (info->mode == TOK_MODE_CODE)
                    {
                        /*
                         *   end of statement, so the next line isn't a
                         *   continuation
                         */
                        info->is_continuation = FALSE;

                        /*
                         *   If we're defining an object, and we're at the
                         *   first indent level (either aligned at the left
                         *   edge or one level in), then this is the end of
                         *   the object definition.  This doesn't apply if
                         *   we're in a property list enclosed in braces.
                         */
                        if (info->in_obj_def
                            && info->stm_start_indent <= ind
                            && info->braces == 0)
                        {
                            /* return to the top level */
                            info->in_obj_def = FALSE;
                            info->in_prop_list = FALSE;
                            info->stm_start_indent = 0;
                            info->stm_indent = ind;
                        }
                    }
                    break;

                case ':':
                    /*
                     *   Colon - if we're in code mode, whatever follows is
                     *   not continuation.  This doesn't apply to javascript,
                     *   where a colon is a delimiter in an object
                     *   definition.
                     */
                    if (info->mode == TOK_MODE_CODE && lang_ != T3E_SUBLANG_JS)
                        info->is_continuation = FALSE;
                    break;

                case ',':
                    /*
                     *   In an expression, a line ending in a comma means
                     *   that we're continuing the expression on the next
                     *   line.  In Javascript code, this is generally a
                     *   delimiter in a list of values in a structure, so
                     *   it's not a continuation line.
                     */
                    if (info->mode == TOK_MODE_CODE && lang_ == T3E_SUBLANG_JS)
                        info->is_continuation = FALSE;
                    break;

                case '{':
                    /*
                     *   if we're in code, or we're starting an anonymous
                     *   function definition, indent subsequent lines one
                     *   level in from the current line's indentation
                     */
                    if (info->mode == TOK_MODE_CODE || info->in_anon_fn)
                    {
                        /*
                         *   remember if we're entering an anonymous
                         *   function, then clear that flag - if we are, it
                         *   only affects the nested delimiter level, so
                         *   we'll want to clear it after we pop out of the
                         *   level
                         */
                        int anonfn = info->in_anon_fn;
                        info->in_anon_fn = FALSE;

                        /* push a delimiter level */
                        push_paren(&ps, info);

                        /*
                         *   if we're entering an anonymous function, switch
                         *   to code mode
                         */
                        if (anonfn)
                            info->mode = TOK_MODE_CODE;

                        /* count the brace level */
                        info->braces += 1;

                        /* increase the indentation level */
                        info->stm_start_indent += ind;
                        info->stm_indent = info->stm_start_indent;

                        /* the next line is not a continuation line */
                        info->is_continuation = FALSE;

                        /*
                         *   If this line or the previous line looks like a
                         *   nested object definition or a dobjFor/iobjFor
                         *   block, then indent the nested block as though it
                         *   were another object definition.  We only need to
                         *   do this if we're in an object definition at the
                         *   enclosing brace level.
                         */
                        if (info->in_obj_def && info->in_prop_list)
                        {
                            char *txt2;
                            SciLine lin2(sci_);

                            /*
                             *   presume we won't be in an property list
                             *   inside the brace
                             */
                            info->in_prop_list = FALSE;

                            /* if the brace is alone, check the last line */
                            if (re_match(lin.txt, "<space>*%{<space>*$")
                                && lin.lin > 0)
                            {
                                /* get the previous line */
                                lin2.retrieve_line(lin.lin - 1);
                                txt2 = lin2.txt;
                            }
                            else
                            {
                                /* check the current line */
                                txt2 = lin.txt;
                            }

                            /* check for a nested object or an xobjFor */
                            if (re_match(txt2,
                                         "<space>*"
                                         "(<alpha|_><alphanum|_>*"
                                         "<space>*:<space>*"
                                         "<alpha|_><alphanum|_>*"
                                         "(<space>*,"
                                         "<space>*<alpha|_><alphanum|_>*)*"
                                         "|"
                                         "[di]objFor<space>*%("
                                         ")"))
                            {
                                /*
                                 *   we're in a nested object definition or
                                 *   xobjFor - note that we're in a property
                                 *   list
                                 */
                                info->in_prop_list = TRUE;
                            }
                        }
                    }
                    else
                    {
                        /* push a delimiter level */
                        push_paren(&ps, info);
                    }
                    break;

                case '}':
                    /* pop a delimiter level */
                    pop_paren(&ps, info);

                    /* if we're in code, the next line isn't a continuation */
                    if (info->mode == TOK_MODE_CODE)
                        info->is_continuation = FALSE;

                    /*
                     *   if this is at column 0, take it to indicate the end
                     *   of an object or function definition
                     */
                    if (p == lin.txt)
                    {
                        info->in_obj_def = FALSE;
                        info->in_func_def = FALSE;
                    }

                    /* done */
                    break;

                case '(':
                case '[':
                    /* push a delimiter level */
                    push_paren(&ps, info);

                    /* we're now in an expression */
                    info->mode = TOK_MODE_EXPR;

                    /*
                     *   if the line ends with the paren, or the open paren
                     *   is in a column so far right that we can't fit a lot
                     *   more after that point, then indent the next line so
                     *   that it's one level in from this line; otherwise,
                     *   indent a continuation line so that it matches the
                     *   paren level
                     */
                    if (*(p+1) == '\0'
                        || re_match(p+1, "<space>*$")
                        || lin.getcol(p) > 50
                        || *p == '[')
                    {
                        /* indent the next line one level in from this line */
                        info->paren_col =
                            call_sci(SCI_GETLINEINDENTATION, lin.lin) + ind;
                    }
                    else
                    {
                        /* indent continuation lines inside the paren */
                        info->paren_col = lin.getcol(p) + 1;
                    }
                    break;

                case ')':
                case ']':
                    /* pop the paren stack */
                    pop_paren(&ps, info);
                    break;

                case '>':
                    /*
                     *   if we're in an embedded expression, a '>>' is like
                     *   an open quote for a new string
                     */
                    if (info->mode == TOK_MODE_EXPR
                        && info->in_embed
                        && *(p+1) == '>')
                    {
                        /* pop the delimiter level */
                        pop_paren(&ps, info);

                        /* skip the two-character open quote sequence */
                        toklen = 2;
                    }
                    else
                    {
                        /* treat it as an ordinary token */
                        goto normal_token;
                    }
                    break;

                case '"':
                    /* note the current string indentation point */
                    info->str_indent = find_str_indent(ind, p, &lin);

                    /* enter string mode */
                    info->prv_mode = info->mode;
                    info->mode = TOK_MODE_DSTRING;

                    /* check for triple quotes */
                    if (*(p+1) == '"' && *(p+2) == '"')
                    {
                        /* skip the two extra quotes and set triple mode */
                        p += 2;
                        info->mode = TOK_MODE_DSTRING3;
                    }
                    break;

                case '\'':
                    /* note the current string indentation point */
                    info->str_indent = find_str_indent(ind, p, &lin);

                    /* enter string mode, pushing the old mode */
                    info->prv_mode = info->mode;
                    info->mode = TOK_MODE_SSTRING;

                    /* check for triple quotes */
                    if (*(p+1) == '\'' && *(p+2) == '\'')
                    {
                        /* skip the two extra quotes and set triple mode */
                        p += 2;
                        info->mode = TOK_MODE_SSTRING3;
                    }
                    break;

                case '/':
                    /* check to see if we're starting a comment */
                    if (*(p+1) == '*')
                    {
                        /* starting a comment */
                        info->prv_mode = info->mode;
                        info->mode = TOK_MODE_COMMENT;

                        /* skip the open marker */
                        toklen = 2;
                    }
                    else if (*(p+1) == '/')
                    {
                        /* comment to eol - skip to end of line */
                        p += strlen(p);
                        goto end_of_line_in_code;
                    }
                    else
                    {
                        /* it's an ordinary character */
                        goto normal_token;
                    }
                    break;

                default:
                normal_token:
                    /*
                     *   if a line break immediately follows, the next line
                     *   will be a continuation of this line
                     */
                    info->is_continuation = TRUE;

                    /* for efficiency, skip alphanumeric tokens all at once */
                    toklen = re_match(p, "<alpha|_><alphanum|_>+");
                    if (toklen == 0)
                        toklen = 1;
                    break;
                }
            }

            /* skip the current token and any subsequent spaces */
            p = skip_spaces(p + toklen);
        }
    }

    /*
     *   Find the string indentation for subsequent lines of the string
     *   beginning at the current character.  This must be called with 'p'
     *   positioned to the open quote of the string.
     *
     *   If the string is the first non-space character on the current line
     *   we'll indent subsequent lines of the string so that they line up
     *   with the open quote.  Otherwise, we'll indent subsequent lines so
     *   that they're indented one level in from where the current line
     *   starts.
     */
    int find_str_indent(int ind, char *p, SciLine *lin)
    {
        char *q, *r;

        /* presume we'll indent to match the open quote */
        int ret = lin->getcol(p);

        /* search for a non-space character before the quote on the line */
        for (q = lin->txt ; q < p && isspace(*q) ; ++q) ;
        if (q < p)
        {
            /*
             *   there's something else on the line before the string -
             *   assume we'll indent one level in from the current line's
             *   indentation
             */
            ret = call_sci(SCI_GETLINEINDENTATION, lin->lin) + ind;

            /*
             *   if we're inside a list, or we're part of a conditional
             *   expression, and there's nothing else on the line apart from
             *   the list open bracket or ternary operator part, indent to
             *   the quote or the list bracket, as appropriate
             */
            if (*q == '[' || *q == '?' || *q == ':')
            {
                /* check for anything else before the quote */
                for (r = q + 1 ; r < p && isspace(*r) ; ++r) ;
                if (r == p)
                {
                    if (*q == '[')
                    {
                        /* indent to the list opener */
                        ret = lin->getcol(q);
                    }
                    else
                    {
                        /* indent to the quote */
                        ret = lin->getcol(p);
                    }
                }
            }
        }

        /* return the result */
        return ret;
    }

    /*
     *   Is the current line a label?
     */
    int is_label(char *txt)
    {
        /* javascript doesn't have C-style labels */
        if (lang_ == T3E_SUBLANG_JS)
            return FALSE;

        /*
         *   If the line has an alphanumeric identifier followed by a colon,
         *   and nothing else, it's a label.  The one exception to "nothing
         *   else" is that we're allowed to have a comment at the end of the
         *   line.
         */
        return re_match(txt,
                        "<space>*"
                        "(<alpha|_><alphanum|_>*<space>*|case<space>+.+)"
                        ":<space>*(/[/*].*)?$");
    }

    /*
     *   Is this a semicolon on a line alone by itself?
     */
    int is_lone_semi(char *txt)
    {
        /* if the line is just a semicolon, it's an ending semicolon */
        return re_match(txt, "<space>*;<space>*$");
    }

    /*
     *   is this a preprocessor line?
     */
    int is_preproc_line(char *txt)
    {
        /*
         *   if the line starts with a '#', maybe with leading whitespace,
         *   it's a preprocessor line
         */
        return re_match(txt, "<space>*#");
    }

    /* indent a comment line */
    void indent_comment(int curline)
    {
        /* get the starting line */
        SciLine lin(sci_, curline);

        /*
         *   Check to see if we're at the end of a block comment.  If so,
         *   apply our special comment formatting: we'll word-wrap the text
         *   within the comment to fit within the current right margin, and
         *   put a '*' border along the left edge of the comment.  If it's a
         *   single-line comment (i.e., the comment opener is on the same
         *   line), don't format it.
         */
        if (re_match(lin.txt, ".*%*/") && !re_match(lin.txt, ".*/%*.*%*/"))
        {
            /* do the comment formatting, grouping undo */
            format_c_comment_undo(curline);
            return;
        }

        /* line up under the previous line's comment start, if possible */
        for (int l = curline ; l > 0 ; )
        {
            /* get the previous line */
            lin.retrieve_line(--l);

            /* if it's blank, keep looking */
            if (lin.len == 0 || re_match(lin.txt, "<space>+$"))
                continue;

            /* get this line's indentation position */
            int ind = call_sci(SCI_GETLINEINDENTATION, l);

            /*
             *   if it's the start of comment, line up under the open comment
             *   plus a few spaces; otherwise line up under the same
             *   indentation as this line
             */
            if (re_match(lin.txt, "<space>*/%*"))
                indent_to_column(curline, ind + 5);
            else
                indent_to_column(curline, ind);

            /* done */
            break;
        }
    }

    /* internal literal string formatting */
    virtual int format_string(int pos)
    {
        tok_info info;
        paren_stack ps;
        int l;
        int in_expr;
        int qu;
        int cur_start = -1;
        int cur_end = -1;
        int done = FALSE;
        const char *p;

        /*
         *   Determine the boundaries of the string containing 'pos'.  First,
         *   find the line where the statement starts, then parse from that
         *   point forward until we reach 'pos', noting the start of the most
         *   recent string.  Once we reach 'pos', keep going until we get to
         *   the end of the containing string.
         */

        /* find where the statement containing the cursor starts */
        l = call_sci(SCI_LINEFROMPOSITION, pos);
        get_tok_mode(&info, l, ps);

        /* make sure we parse into the string */
        parse_forward(&info, l, ps);

        /* initialize the parsing context */
        in_expr = FALSE;
        qu = 0;

        /* parse forward from the beginning of the statement */
        for (l = info.stm_start_line ;
             !done && l < call_sci(SCI_GETLINECOUNT) ; ++l)
        {
            /* get this line */
            SciLine lin(sci_, l);

            /* parse the line */
            for (p = lin.txt ; *p != '\0' && !done ; ++p)
            {
                switch (*p)
                {
                case '"':
                case '\'':
                    /*
                     *   if we're not in a string, this starts one; if we're
                     *   in a string, and this is the matching close quote,
                     *   it ends the string
                     */
                    if (qu == 0)
                    {
                        /* open quote - note the type of string we're in */
                        qu = *p;

                        /*
                         *   if we're not in an embedded expression, and we
                         *   haven't yet passed the starting position, note
                         *   the start of the string - this is the last
                         *   string start so far before the starting position
                         */
                        if (!in_expr && lin.gettxtpos(p) <= pos)
                            cur_start = lin.gettxtpos(p);
                    }
                    else if (qu == *p)
                    {
                        /* this matches our open quote - end the string */
                        qu = 0;

                        /*
                         *   if at or past the starting position, and we're
                         *   not in an embedded expression, we're done
                         */
                        if (!in_expr && lin.gettxtpos(p) >= pos)
                            cur_end = lin.gettxtpos(p + 1);
                    }
                    break;

                case '\\':
                    /* if we're in a string, skip the next character */
                    if (qu != 0 && *(p+1) != '\0')
                        ++p;
                    break;

                case '<':
                    /*
                     *   if we're in a d-string, and this is a '<<' sequence,
                     *   enter an embedded expression
                     */
                    if (qu == '"' && *(p+1) == '<' && !in_expr)
                    {
                        qu = 0;
                        in_expr = TRUE;
                        ++p;
                    }
                    break;

                case '>':
                    /*
                     *   if we're in an embedded expression, and this is a
                     *   '>>' sequence, go back to d-string mode
                     */
                    if (qu == 0 && *(p+1) == '>' && in_expr)
                    {
                        qu = '"';
                        in_expr = FALSE;
                        ++p;
                    }
                    break;
                }

                /*
                 *   if we've found the end of the string containing 'pos',
                 *   we're done; if we're at 'pos' and we're not in either a
                 *   string or an embedded expression, 'pos' isn't in a
                 *   string at all, so we're also done
                 */
                done = (cur_end != -1
                        || (lin.gettxtpos(p) >= pos && qu == 0 && !in_expr));
            }
        }

        /* if 'pos' didn't turn out to be in a string, do nothing */
        if (cur_start == -1 || cur_end == -1)
            return FALSE;

        /* do the real work within an undo group */
        call_sci(SCI_BEGINUNDOACTION);

        /*
         *   set up a spot to mark the original and end-of-string positions
         *   (they'll move as we edit)
         */
        LPARAM spot_orig = create_spot(pos);
        LPARAM spot_end = create_spot(cur_end);

        /*
         *   We have the bounds of the string, so fill it in.  First, join
         *   together all of the continuation lines that we can join.  Leave
         *   newlines in place wherever there's an explicit line-break
         *   formatting code (\n, \b, <br>, <p>, <.p>) just before or after
         *   the newline.  Also leave line breaks exactly as-is in embedded
         *   expressions.
         */
        for (l = call_sci(SCI_LINEFROMPOSITION, cur_start) ; ; ++l)
        {
            SciLine lin(sci_, l);
            SciLine nxt(sci_, l + 1);

            /* if this is the last line, we're done */
            if (l >= call_sci(SCI_LINEFROMPOSITION, get_spot_pos(spot_end)))
                break;

            /*
             *   if we're within string text, check for explicit line-break
             *   formatting codes at the end of the current line or the start
             *   of the next line; if we don't find one, join the two lines
             */
            if (!pos_is_string(call_sci(SCI_POSITIONFROMLINE, l + 1))
                || (!match_line_breaks(l, -1)
                    && !match_line_breaks(l + 1, 1)))
            {
                /* remove indentation from the second line */
                call_sci(SCI_SETLINEINDENTATION, l + 1, 0);

                /* join the two lines */
                call_sci(SCI_SETTARGETSTART,
                         call_sci(SCI_POSITIONFROMLINE, l));
                call_sci(SCI_SETTARGETEND,
                         call_sci(SCI_POSITIONFROMLINE, l + 1));
                call_sci(SCI_LINESJOIN);

                /* reconsider the same line, as it's now longer */
                --l;
            }
        }

        /*
         *   We've combined the string as much as possible into a single
         *   line, so the next step is to break it up again so that it fits
         *   within the margins.  Walk through the string looking for
         *   breakable positions - we can break at any space within the
         *   string that's not within a {format code} sequence.
         */
        int lastbrk = -1;
        int margin = call_sci(SCI_GETEDGECOLUMN);
        for (pos = cur_start ; ; )
        {
            /* get the current character and style */
            int c = call_sci(SCI_GETCHARAT, pos);
            int s = call_sci(SCI_GETSTYLEAT, pos);

            /* note the column */
            int col = call_sci(SCI_GETCOLUMN, pos);

            /* if we're at the start of a line, skip leading spaces */
            if (col == 0)
            {
                /*
                 *   skip to the next non-space, or end of the line or string
                 *   or buffer, whichever comes first
                 */
                while (isspace(c)
                       && c != '\n'
                       && c != '\r'
                       && pos < get_spot_pos(spot_end)
                       && pos < call_sci(SCI_GETTEXTLENGTH))
                {
                    /* move on */
                    pos = call_sci(SCI_POSITIONAFTER, pos);

                    /* get the next character */
                    c = call_sci(SCI_GETCHARAT, pos);
                }

                /* refetch our position */
                s = call_sci(SCI_GETSTYLEAT, pos);
                col = call_sci(SCI_GETCOLUMN, pos);
            }

            /*
             *   If we've reached a space or newline past the end of the
             *   string, or reached the end of the buffer, stop here.  Note
             *   that we keep going past the end of the string so that we're
             *   sure to wrap lines where the close quote and subsequent
             *   punctuation (as in "string";) would just push past the end
             *   of the line.
             */
            if ((isspace(c) || c == '\n' || c == '\r')
                && pos >= get_spot_pos(spot_end))
                break;

            /* definitely stop at the end of the buffer */
            if (pos >= call_sci(SCI_GETTEXTLENGTH))
                break;

            /* move on to the next character */
            pos = call_sci(SCI_POSITIONAFTER, pos);

            /* skip newlines */
            if (c == '\n' || c == '\r')
                continue;

            /*
             *   if this is a non-space, and it pushes us past the current
             *   right margin, and we have a previous break point, break at
             *   the last break point
             */
            if (!isspace(c) && lastbrk != -1 && col + 1 > margin)
            {
                /* insert a newline at the last breakpoint */
                call_sci(SCI_SETSEL, lastbrk, lastbrk);
                call_sci(SCI_NEWLINE);

                /* remember the line number of the next line */
                l = call_sci(SCI_LINEFROMPOSITION, lastbrk) + 1;

                /* trim trailing whitespace at the end of the line */
                for (pos = lastbrk ; pos > 0 ; )
                {
                    /* move back a character */
                    int nxtpos = pos;
                    pos = call_sci(SCI_POSITIONBEFORE, pos);

                    /* get this character */
                    c = call_sci(SCI_GETCHARAT, pos);

                    /* stop at a newline or any non-space */
                    if (c == '\n' || c == '\r' || !isspace(c))
                        break;

                    /* it's a space - remove it and keep going */
                    call_sci(SCI_SETSEL, pos, nxtpos);
                    call_sci(SCI_CLEAR);
                }

                /* indent the next line as though we were typing it in */
                indent_line(get_tok_mode(&info, l, ps), info, l);

                /* we no longer have a recent breakpoint */
                lastbrk = -1;

                /* resume from the start of the new line */
                pos = call_sci(SCI_POSITIONFROMLINE, l);
                continue;
            }

            /*
             *   If this is a space, note the next position as a possible
             *   break point.  Don't allow breaks within {format codes},
             *   which are in style SCE_T3_MSG_PARAM.
             */
            if (isspace(c) && s != SCE_T3_MSG_PARAM)
            {
                int found_qu;
                int pos2;

                /*
                 *   Make sure that we don't have a close quote following.
                 *   Many strings end with a space then a close quote, and in
                 *   these cases we want to treat the final space as
                 *   non-breaking.
                 */
                for (found_qu = FALSE, pos2 = pos ;
                     pos2 < get_spot_pos(spot_end) ;
                     pos2 = call_sci(SCI_POSITIONAFTER, pos2))
                {
                    /* get this character */
                    c = call_sci(SCI_GETCHARAT, pos2);

                    /* skip escaped characters */
                    if (c == '\\')
                    {
                        pos2 = call_sci(SCI_POSITIONAFTER, pos2);
                        continue;
                    }

                    /* if it's a quote of the current string type, note it */
                    if ((c == '"' && (s == SCE_T3_D_STRING
                                      || s == SCE_T3_X_STRING))
                        || (c == '\'' && s == SCE_T3_S_STRING))
                    {
                        found_qu = TRUE;
                        break;
                    }

                    /* stop at the end of the whitespace */
                    if (!isspace(c))
                        break;
                }

                /*
                 *   if we didn't find a quote, remember this as the break
                 *   position
                 */
                if (!found_qu)
                    lastbrk = pos;
            }
        }

        /* move back to the starting point */
        pos = get_spot_pos(spot_orig);
        call_sci(SCI_SETSEL, pos, pos);

        /* done with the spots */
        delete_spot(spot_end);
        delete_spot(spot_orig);

        /* end the undo group */
        call_sci(SCI_ENDUNDOACTION);

        /* indicate that we did the reformat */
        return TRUE;
    }

    /* check a line for starting/ending with a line-break formatting code */
    int match_line_breaks(int lin, int dir)
    {
        /* check each code */
        return match_fmt_code(lin, dir, "\\n")
            || match_fmt_code(lin, dir, "\\b")
            || match_fmt_code(lin, dir, "<p>")
            || match_fmt_code(lin, dir, "<br>")
            || match_fmt_code(lin, dir, "<.p>");
    }

    /* check a line for starting/ending with a given formatting code */
    int match_fmt_code(int lin, int dir, const char *fmt)
    {
        int pos;
        int rem;

        /* go to the start or end of the line, based on the direction */
        if (dir > 0)
            pos = call_sci(SCI_POSITIONFROMLINE, lin);
        else
            pos = call_sci(SCI_POSITIONBEFORE,
                           call_sci(SCI_POSITIONBEFORE,
                                    call_sci(SCI_POSITIONFROMLINE, lin + 1)));

        /* skip whitespace */
        while (pos > 0 && pos < call_sci(SCI_GETTEXTLENGTH)
               && isspace(call_sci(SCI_GETCHARAT, pos)))
            pos = call_sci(
                dir > 0 ? SCI_POSITIONAFTER : SCI_POSITIONBEFORE, pos);

        /* start at the corresponding direction of the format code */
        rem = strlen(fmt);
        if (dir < 0)
            fmt += rem - 1;

        /* check each character for a match and for a string style */
        for ( ; rem != 0 ; --rem, fmt += dir)
        {
            /* check this character */
            if (call_sci(SCI_GETCHARAT, pos) != *fmt
                || !pos_is_string(pos))
                return FALSE;

            /* move 'pos' */
            pos = call_sci(
                dir > 0 ? SCI_POSITIONAFTER : SCI_POSITIONBEFORE, pos);
        }

        /* it's a match */
        return TRUE;
    }

    /* is the character at the given position in a string style? */
    int pos_is_string(int pos)
    {
        /* check the character at the given position for a string style */
        return style_is_string(call_sci(SCI_GETSTYLEAT, pos));
    }

    /* is the given style something within a string? */
    int style_is_string(int s)
    {
        /* check for a match to the string styles */
        return (s == SCE_T3_S_STRING
                || s == SCE_T3_D_STRING
                || s == SCE_T3_X_STRING
                || s == SCE_T3_LIB_DIRECTIVE
                || s == SCE_T3_MSG_PARAM
                || s == SCE_T3_HTML_TAG
                || s == SCE_T3_HTML_DEFAULT
                || s == SCE_T3_HTML_STRING);
    }

    /* toggle comments */
    virtual void toggle_comments()
    {
        /* do the work in an undo group */
        call_sci(SCI_BEGINUNDOACTION);

        /* get the selection range */
        int a = call_sci(SCI_LINEFROMPOSITION,
                         call_sci(SCI_GETSELECTIONSTART));
        int b = call_sci(SCI_LINEFROMPOSITION,
                     call_sci(SCI_GETSELECTIONEND));

        /*
         *   check the first line: if it already starts with a comment
         *   marker, we'll remove comment markers from the region; otherwise
         *   we'll add them
         */
        int pos = call_sci(SCI_POSITIONFROMLINE, a);
        int adding = !(call_sci(SCI_GETCHARAT, pos) == '/'
                       && call_sci(SCI_GETCHARAT,
                                   call_sci(SCI_POSITIONAFTER, pos)) == '/');

        /* process each line */
        for ( ; a <= b ; ++a)
        {
            /* get the position of the start of the line */
            int pos = call_sci(SCI_POSITIONFROMLINE, a);

            /* add or remove the comment */
            if (adding)
            {
                /* insert // at the start of the line */
                call_sci(SCI_SETTARGETSTART, pos);
                call_sci(SCI_SETTARGETEND, pos);
                call_sci(SCI_REPLACETARGET, 2, (LPARAM)"//");
            }
            else if (call_sci(SCI_GETCHARAT, pos) == '/'
                     && call_sci(SCI_GETCHARAT,
                                 call_sci(SCI_POSITIONAFTER, pos)) == '/')
            {
                /*
                 *   we're removing comment markers, and this line starts
                 *   with a comment marker, so remove it - set the target
                 *   region to the comment marker, then delete the target
                 *   region
                 */
                call_sci(SCI_SETTARGETSTART, pos);
                pos = call_sci(SCI_POSITIONAFTER, pos);
                pos = call_sci(SCI_POSITIONAFTER, pos);
                call_sci(SCI_SETTARGETEND, pos);
                call_sci(SCI_REPLACETARGET, 0, (int)"");
            }
        }

        /* done - end the undo group */
        call_sci(SCI_ENDUNDOACTION);
    }

    /* get the first character on a line */
    int first_char_on_line(int lin)
    {
        /* if the line is empty, there's nothing on the line */
        if (call_sci(SCI_LINELENGTH, lin) == 0)
            return 0;

        /* get the position at the start of the line */
        int pos = call_sci(SCI_POSITIONFROMLINE, lin);

        /* get the character there */
        return call_sci(SCI_GETCHARAT, pos);
    }

    /* get the last character on a line */
    int last_char_on_line(int lin)
    {
        /* if the line is empty, there's nothing on the line */
        if (call_sci(SCI_LINELENGTH, lin) == 0)
            return 0;

        /* get the position just before the end of the line */
        int pos = call_sci(SCI_GETLINEENDPOSITION, lin);
        pos = call_sci(SCI_POSITIONBEFORE, pos);

        /* get the character there */
        return call_sci(SCI_GETCHARAT, pos);
    }

    /* sub-language mode */
    t3e_sublang_t lang_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Additional Tads3EditorMode implementation
 */

/* create our extension */
CTadsWorkbenchEditor *Tads3EditorMode::create_ext(HWND hwnd)
{
    return new Tads3Editor(itwb_, hwnd, T3E_SUBLANG_T3);
}


/* ------------------------------------------------------------------------ */
/*
 *   Additional JavascriptEditorMode implementation
 */

/* create our extension */
CTadsWorkbenchEditor *JavascriptEditorMode::create_ext(HWND hwnd)
{
    return new Tads3Editor(itwb_, hwnd, T3E_SUBLANG_JS);
}


/* ------------------------------------------------------------------------ */
/*
 *   The extension object.
 */
class Tads3AddIn: public CTadsWorkbenchExtension
{
public:
    Tads3AddIn(ITadsWorkbench *itwb)
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
            return new Tads3EditorMode(itwb_);

        case 1:
            return new Tads3LibEditorMode(itwb_);

        case 2:
            return new JavascriptEditorMode(itwb_);

        default:
            /* no other modes */
            return 0;
        }
    }
};


/*
 *   Connect.  Workbench calls this at start-up to initialize the DLL and
 *   exchange interface pointers.
 */
__declspec(dllexport) IUnknown *twb_connect(ITadsWorkbench *itwb)
{
    /* create and return our extension object */
    return new Tads3AddIn(itwb);
}
