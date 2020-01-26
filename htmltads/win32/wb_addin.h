/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  wb_addin.h - TADS Workbench Add-In helpers
Function
  This defines some helper classes that make it easier to write a
  TADS Workbench extension add-in.
Notes

Modified
  10/18/06 MJRoberts  - Creation
*/

#ifndef WB_ADDIN_H
#define WB_ADDIN_H

#include "itadsworkbench.h"
#include <scintilla.h>
#include <scilexer.h>


/* ------------------------------------------------------------------------ */
/*
 *   Auto-indent modes
 */
#define TWB_AUTOINDENT_NONE   0                        /* no auto-indenting */
#define TWB_AUTOINDENT_SIMPLE 1     /* simple: use same indent as prior line */
#define TWB_AUTOINDENT_SYNTAX 2              /* full syntax-based indenting */

/*
 *   Comment auto-formatting modes
 */
#define TWB_FMTCOMMENT_OFF    0              /* do not auto-format comments */
#define TWB_FMTCOMMENT_ON     1            /* automatically format comments */


/* ------------------------------------------------------------------------ */
/*
 *   Utility routines
 */

/* number of elements in an array */
#define countof(x) (sizeof(x)/sizeof(x[0]))

/* "safe" wide strcpy - don't overflow, always null-terminate */
static void safe_wcscpy(wchar_t *dst, size_t dstcnt, const wchar_t *src)
{
    size_t copycnt;

    /* if the destination buffer is zero-size, we can't do anything */
    if (dstcnt == 0)
        return;

    /* get the source length */
    copycnt = wcslen(src);

    /* limit to the available destination space less null termination */
    if (copycnt > dstcnt - 1)
        copycnt = dstcnt - 1;

    /* copy the characters */
    memcpy(dst, src, copycnt * sizeof(*dst));

    /* null-terminate */
    dst[copycnt] = L'\0';
}

/* ------------------------------------------------------------------------ */
/*
 *   Private style description structure.  We use this to initialize a static
 *   array of style data, for ITadsWorkbenchEditorMode::GetStyleInfo()
 *   implementations.
 */
struct style_info
{
    int sci_index;
    const OLECHAR *title;
    const OLECHAR *sample;
    COLORREF fgcolor;
    COLORREF bgcolor;
    UINT styles;
    int keyword_set;
};


/* ------------------------------------------------------------------------ */
/*
 *   Scintilla fast-path invoker
 */
class SciInvoker
{
public:
    SciInvoker(HWND hwnd) { init(hwnd); }
    SciInvoker(SciInvoker &sci) { init(sci); }

    void init(HWND hwnd)
    {
        /* remember the window handle */
        hwnd_ = hwnd;

        /* set up the direct-call interface pointers */
        call_sci_ = (int (__cdecl *)(void *, int, int, int))SendMessage(
            hwnd, SCI_GETDIRECTFUNCTION, 0, 0);
        call_sci_ctx_ = (void *)SendMessage(
            hwnd, SCI_GETDIRECTPOINTER, 0, 0);

        /* look up the custom message names */
        msg_create_spot = RegisterWindowMessage("w32tdb.sci.CreateSpot");
        msg_delete_spot = RegisterWindowMessage("w32tdb.sci.DeleteSpot");
        msg_get_spot_pos = RegisterWindowMessage("w32tdb.sci.GetSpotPos");
        msg_set_spot_pos = RegisterWindowMessage("w32tdb.sci.SetSpotPos");
    }

    void init(SciInvoker &sci)
    {
        call_sci_ = sci.call_sci_;
        call_sci_ctx_ = sci.call_sci_ctx_;
    }

    /* invoke Scintilla through the direct-call interface */
    int invoke(int msg, int a = 0, int b = 0)
        { return call_sci_(call_sci_ctx_, msg, a, b); }

    /* call the various Workbench custom message handlers */
    LPARAM create_spot(int pos)
        { return SendMessage(hwnd_, msg_create_spot, pos, 0); }
    void delete_spot(LPARAM spot)
        { SendMessage(hwnd_, msg_delete_spot, 0, spot); }
    int get_spot_pos(LPARAM spot)
        { return SendMessage(hwnd_, msg_get_spot_pos, 0, spot); }
    void set_spot_pos(LPARAM spot, int pos)
        { SendMessage(hwnd_, msg_set_spot_pos, pos, spot); }

private:
    /* the Scintilla window handle */
    HWND hwnd_;

    /* Scintilla direct function interface */
    int (__cdecl *call_sci_)(void *, int, int, int);
    void *call_sci_ctx_;

    /* custom Workbench extension messages in the Scintilla window */
    UINT msg_create_spot;
    UINT msg_delete_spot;
    UINT msg_get_spot_pos;
    UINT msg_set_spot_pos;
};


/* ------------------------------------------------------------------------ */
/*
 *   Scintilla line buffer.  This encapsulates a line of text from Scintilla.
 */
class SciLine
{
public:
    SciLine(SciInvoker &sci)
        : sci_(sci)
    {
        /* initialize */
        init();
    }
    SciLine(SciInvoker &sci, int l)
        : sci_(sci)
    {
        /* initialize */
        init();

        /* retrieve the initial line */
        retrieve_line(l);
    }

    ~SciLine()
    {
        /* delete our buffer, if we have one */
        if (txt != 0)
            delete [] txt;
    }

    /* get the column of the given position in our buffer */
    int getcol(char *p)
        { return sci_.invoke(SCI_GETCOLUMN, pos + (p - txt)); }

    /* get the text at the given column */
    char *getcoltxt(int col)
        { return txt + (sci_.invoke(SCI_FINDCOLUMN, lin, col) - pos); }

    /* get the text for a given buffer position */
    char *getpostxt(int p)
    {
        if (p < pos)
            return txt;
        else if (p > pos + len)
            return txt + len;
        else
            return txt + (p - pos);
    }

    /* get the buffer position for a given text pointer */
    int gettxtpos(const char *p)
    {
        if (p >= txt && p < txt + len)
            return pos + (p - txt);
        else if (p >= txt + len)
            return pos + len;
        else
            return pos;
    }

    /* get the text position for a given buffer offset */
    int getofspos(int ofs)
    {
        if (ofs < 0)
            return pos;
        else if (ofs > len)
            return pos + len;
        else
            return pos + ofs;
    }


    /*
     *   Retrieve the given line number.  This fills in 'txt' with the text
     *   of the line, stripped of newlines.  The text is null-terminated.
     */
    void retrieve_line(int l)
    {
        /* note the new line number */
        lin = l;

        /* delete any existing buffer */
        if (txt != 0)
            delete [] txt;

        /* if the line number is out of range, there's nothing to fetch */
        if (l < 0 || l >= sci_.invoke(SCI_GETLINECOUNT))
        {
            len = 0;
            txt = new char[1];
            txt[0] = '\0';
        }
        else
        {
            /* get the length */
            len = sci_.invoke(SCI_LINELENGTH, l);

            /* allocate space */
            txt = new char[len + 1];

            /* note the position of the line */
            pos = sci_.invoke(SCI_POSITIONFROMLINE, l);

            /* retrieve it */
            sci_.invoke(SCI_GETLINE, l, (int)txt);

            /* null-terminate it */
            txt[len] = '\0';
        }

        /* remove trailing nulls and newlines */
        for (char *p = txt + len ;
             p > txt && (*(p-1) == '\0' || *(p-1) == '\n' || *(p-1) == '\r') ;
             *--p = '\0', --len) ;
    }

    /* retrieve the next/previous line */
    void go_next() { retrieve_line(lin + 1); }
    void go_prev() { retrieve_line(lin - 1); }

    /* splice text */
    void splice(int ofs, int del_chars, const char *ins_chars)
    {
        /* set the target to the starting offset plus the deleted characters */
        sci_.invoke(SCI_SETTARGETSTART, getofspos(ofs));
        sci_.invoke(SCI_SETTARGETEND, getofspos(ofs + del_chars));

        /* replace the target with the inserted characters */
        sci_.invoke(SCI_REPLACETARGET, strlen(ins_chars), (LPARAM)ins_chars);

        /* retrieve the updated line into our buffer */
        retrieve_line(lin);
    }

    /* flush changes to our buffer to scintilla */
    void flush()
    {
        /* set the target to our entire line */
        sci_.invoke(SCI_SETTARGETSTART, getofspos(0));
        sci_.invoke(SCI_SETTARGETEND, getofspos(len));

        /* replace the target with the current buffer */
        sci_.invoke(SCI_REPLACETARGET, len, (LPARAM)txt);

        /* retrieve the updated line into our buffer */
        retrieve_line(lin);
    }

    /* our text buffer */
    char *txt;

    /* the length of the text (not counting the null terminator) */
    int len;

    /* the current line number */
    int lin;

    /* the text position of the line */
    int pos;

protected:
    void init()
    {
        /* we don't have a line of text yet */
        lin = -1;
        pos = -1;
        txt = 0;
    }

    /* our Scintilla invoker */
    SciInvoker &sci_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Common base class for ITadsWorkbenchEditorMode implementations.
 */
class CTadsWorkbenchEditorMode: public ITadsWorkbenchEditorMode
{
public:
    CTadsWorkbenchEditorMode(ITadsWorkbench *itwb)
    {
        /* remember my ITadsWorkbench object */
        itwb->AddRef();
        itwb_ = itwb;

        /* set an initial reference count of 1 on behalf of our caller */
        refcnt_ = 1;
    }

    virtual ~CTadsWorkbenchEditorMode()
    {
        /* done with our ITadsWorkbench object */
        itwb_->Release();
    }

    /* IUnknown reference-counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret;
        if ((ret = --refcnt_) == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchEditorMode)
            *ifc = (void *)(ITadsWorkbenchEditorMode *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

    /*
     *   ITadsWorkbenchEditorMode methods
     */

    /* get my default style settings */
    void STDMETHODCALLTYPE GetStyleInfo(UINT idx, wb_style_info *info)
    {
        /* if the index is out of range, just say so */
        if (idx >= get_style_count())
            return;

        /* get this style definer */
        const style_info *s = &get_style_array()[idx];

        /* if it's undefined, fail */
        if (s == 0)
            return;

        /*
         *   Fill in the information.  We use the default font for all of our
         *   styles, so we can just leave the name empty and set the point
         *   size to zero.
         */
        info->sci_index = s->sci_index;
        info->font_name[0] = '\0';
        info->ptsize = 0;
        info->fgcolor = s->fgcolor;
        info->bgcolor = s->bgcolor;
        info->styles = s->styles;
        info->keyword_set = s->keyword_set;
        safe_wcscpy(info->title, countof(info->title), s->title);
        safe_wcscpy(info->sample, countof(info->sample), s->sample);
    }

    /* get the number of styles we use */
    UINT STDMETHODCALLTYPE GetStyleCount()
    {
        /* return the number of array elements */
        return get_style_count();
    }

    /* create an instance of our extension */
    ITadsWorkbenchEditor * STDMETHODCALLTYPE CreateEditorExtension(HWND hwnd);

protected:
    /*
     *   Each subclass must override this to return a new object of the
     *   appropriate CTadsWorkbenchEditor subclass.  The implementation is
     *   usually just something like 'return new MySubclass(itwb_, hwnd)'.
     */
    virtual class CTadsWorkbenchEditor *create_ext(HWND hwnd) = 0;

    /*
     *   Get an entry from my array of style_info elements, and get the
     *   number of elements in the array.  Subclasses can use our default
     *   implementation of GetStyleInfo() simply by defining a static array
     *   of style_info elements, and returning that static array address
     *   here.  Alternatively, the subclass can provide its own completely
     *   custom implementation, without defining a style_info array at all,
     *   by overriding GetStyleInfo() and GetStyleCount(); the subclass can
     *   then provide dummy implementations of these methods (i.e., just
     *   return 0 from both of these).
     */
    virtual const style_info *get_style_array() = 0;
    virtual UINT get_style_count() = 0;

    /* reference counter */
    ULONG refcnt_;

    /* my Workbench interface */
    ITadsWorkbench *itwb_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Common base class for ITadsWorkbenchEditor implementations.
 */
class CTadsWorkbenchEditor: public ITadsWorkbenchEditor
{
public:
    CTadsWorkbenchEditor(ITadsWorkbench *itwb, HWND hwnd)
        : sci_(hwnd)
    {
        /* count an initial reference on behalf of the caller */
        refcnt_ = 1;

        /* remember the Workbench interface */
        itwb->AddRef();
        itwb_ = itwb;

        /* remember our Scintilla window */
        hwnd_ = hwnd;

        /* no indent starting position yet */
        last_indent_cmd_start_ = -1;

        /* register the standard editor formatting extensions */
        indent_cmd = itwb->DefineCommand(
            L"Ext.Edit.SyntaxIndent",
            L"Auto-indent each line in the selected region based on "
            L"the TADS code syntax.");
        commentfmt_cmd = itwb->DefineCommand(
            L"Ext.Edit.FormatComment",
            L"Reformat the block comment enclosing the caret position, "
            L"word-wrapping paragraphs and adding a left '*' border.");
        stringfmt_cmd = itwb->DefineCommand(
            L"Ext.Edit.FormatString",
            L"Reformat the string at the current caret position, "
            L"word-wrapping the text to fit the margins.");

        /* look up some standard commands we're interested in */
        fill_para_cmd = itwb->GetCommandID(L"Edit.FillParagraph");
        addcomment_cmd = itwb->GetCommandID(L"Edit.CommentRegion");
    }

    virtual ~CTadsWorkbenchEditor()
    {
        /* release our objects */
        itwb_->Release();
    }

    /*
     *   Initialize.  The editor-mode object's CreateEditorExtension must
     *   call this after creating the object.
     */
    virtual void init()
    {
        /* note our initial configuration settings */
        OnSettingsChange();
    }

    /* IUnknown reference-counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret;
        if ((ret = --refcnt_) == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchEditor)
            *ifc = (void *)(ITadsWorkbenchEditor *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

    /*
     *   ITadsWorkbenchEditor methods
     */

    /*
     *   Receive notification of a Workbench configuration settings change.
     *   Subclasses should override if they need to refresh any property
     *   settings beyond the styles and the other properties we refresh here.
     *   In particular, editor modes that use custom keyword lists must
     *   refresh those lists in an override.
     */
    void STDMETHODCALLTYPE OnSettingsChange()
    {
        /*
         *   Get the auto-indent and format-comments modes from Workbench.
         *   The default auto-indent mode is "full syntax indenting."  The
         *   default comment formatting mode is "on."  These are global
         *   options settings.
         */
        auto_indent_ = itwb_->GetPropValInt(
            TRUE, L"src win options", L"autoindent",
            TWB_AUTOINDENT_SYNTAX);
        format_comments_ = itwb_->GetPropValInt(
            TRUE, L"src win options", L"autoformat comments",
            TWB_FMTCOMMENT_ON);
    }

    /*
     *   Message filter.  Our default implementation simply passes the
     *   message directly to the default handler, and returns the result.
     */
    LRESULT STDMETHODCALLTYPE MsgFilter(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
        ITadsWorkbenchMsgHandler *def_handler)
    {
        /* pass the message directly to the default handler */
        return def_handler->CallMsgHandler(hwnd, msg, wparam, lparam);
    }

    /*
     *   Command status updater.  By default, we don't add any special
     *   commands of our own or override any Workbench commands, so we simply
     *   return FALSE here to tell Workbench to carry on with the default
     *   status check.  Subclasses should override this to indicate the
     *   status for extended commands they define.
     */
    BOOL STDMETHODCALLTYPE GetCommandStatus(
        UINT command_id, UINT *status, HMENU menu, UINT menu_index)
    {
        return FALSE;
    }

    /*
     *   Carry out a command.  By default, we don't add any commands of our
     *   own or override any Workbench commands, so we simply return FALSE to
     *   let Workbench carry out the default processing.
     */
    BOOL STDMETHODCALLTYPE DoCommand(UINT command_id)
    {
        /* check for our custom commands */
        if (command_id == indent_cmd)
        {
            /* get the current selection range, in terms of line numbers */
            int start = call_sci(SCI_GETSELECTIONSTART);
            int startcol = call_sci(SCI_GETCOLUMN, start);
            int end = call_sci(SCI_GETSELECTIONEND);
            int a = call_sci(SCI_LINEFROMPOSITION, start);
            int b = call_sci(SCI_LINEFROMPOSITION, end);

            /* make this whole operation a single undo block */
            call_sci(SCI_BEGINUNDOACTION);

            /* presume we won't have to save the indent position */
            int new_indent_cmd_start = -1;

            /* if there's no selection, handle specially */
            if (start == end)
            {
                /*
                 *   If we're already at the indent position, simply indent
                 *   more; otherwise, do syntax indenting.
                 */
                if (start == call_sci(SCI_GETLINEINDENTPOSITION, a))
                {
                    /*
                     *   Special case: if we're in the first column, it's
                     *   ambiguous because they could want to syntax-indent
                     *   to column 0 (if indeed this line syntax-indents to
                     *   column 0) or they might just want to insert a tab.
                     *   On the first try, do syntax indenting, and set a
                     *   flag.  If the flag is set for the same line, on the
                     *   second try, do a plain tab indent.
                     */
                    if (startcol == 0 && start != last_indent_cmd_start_)
                    {
                        /*
                         *   first try at this position - syntax indent, but
                         *   note the position so that we'll do a simple
                         *   indent if they repeat the command at the same
                         *   position
                         */
                        new_indent_cmd_start = start;
                        syntax_indent(a);
                    }
                    else
                    {
                        /* simply indent another level */
                        call_sci(SCI_SETLINEINDENTATION, a,
                                 call_sci(SCI_GETLINEINDENTATION, a)
                                 + call_sci(SCI_GETINDENT));
                    }
                }
                else
                {
                    /* re-indent the line */
                    syntax_indent(a);
                }

                /* go to the new indentation */
                start = call_sci(SCI_GETLINEINDENTPOSITION, a);
                call_sci(SCI_SETSEL, start, start);
            }
            else
            {
                /* syntax-indent the range */
                syntax_indent_range(start, a, end, b);
            }

            /* end the undo group */
            call_sci(SCI_ENDUNDOACTION);

            /* remember the indent position if applicable */
            last_indent_cmd_start_ = new_indent_cmd_start;

            /* handled */
            return TRUE;
        }
        else if (command_id == commentfmt_cmd)
        {
            /* do the comment formatting command */
            format_comment(call_sci(SCI_GETCURRENTPOS));

            /* handled */
            return TRUE;
        }
        else if (command_id == stringfmt_cmd)
        {
            /* reformat a string */
            format_string(call_sci(SCI_GETCURRENTPOS));

            /* handled */
            return TRUE;
        }
        else if (command_id == addcomment_cmd)
        {
            /* add or remove comments in the selected region */
            toggle_comments();

            /* handled */
            return TRUE;
        }

        /* we didn't handle it */
        return FALSE;
    }

    /*
     *   Receive notification of an added character
     */
    void STDMETHODCALLTYPE OnCharAdded(UINT ch)
    {
        /*
         *   If this is a CR (\r), and we're in CRLF mode, ignore it - we'll
         *   do the work on the LF (\n) instead.  We don't want to do the
         *   same work twice.
         */
        if (ch == '\r' && call_sci(SCI_GETEOLMODE) == SC_EOL_CRLF)
            return;

        /* auto-indent or auto-format comments on certain "hot" characters */
        if (strchr(hot_chars(), ch))
        {
            /* note the current line number */
            int curpos = call_sci(SCI_GETCURRENTPOS);
            int curline = call_sci(SCI_LINEFROMPOSITION, curpos);

            /* check the auto-indent mode */
            switch (auto_indent_)
            {
            case TWB_AUTOINDENT_NONE:
                /* auto-indenting is off - do nothing */
                break;

            case TWB_AUTOINDENT_SIMPLE:
                /*
                 *   simple indenting - just indent to same level as the
                 *   previous line; do this only on a newline insertion
                 */
                if (ch == '\n' || ch == '\r')
                    hanging_indent();
                break;

            case TWB_AUTOINDENT_SYNTAX:
                /* full syntax indenting */
                syntax_indent(curline);

                /*
                 *   if the key was a newline, position the cursor at the end
                 *   of the indentation; otherwise leave it where it was
                 */
                if (ch == '\n' || ch == '\r')
                    go_to_indent();
                break;
            }
        }
    }

protected:
    /*
     *   Hot characters for syntax indenting.  By default, we'll do
     *   auto-indenting on adding a newline.  For languages with block
     *   delimiters, it's often useful to add the delimiter characters to the
     *   list so that we auto-indent on those as well.
     */
    virtual const char *hot_chars() const { return "\n\r"; }

    /*
     *   Perform syntax indenting on the given line.  Language modes should
     *   override this to calculate the indenting.  We do nothing by default.
     */
    virtual void syntax_indent(int linenum) { }

    /* syntax-indent a range */
    virtual void syntax_indent_range(
        int startpos, int startline, int endpos, int endline) { }

    /* reformat a string */
    virtual int format_string(int pos) { return FALSE; }

    /* do comment fill */
    virtual int format_comment(int pos) { return FALSE; }

    /* toggle comments in the selection range */
    virtual void toggle_comments() { }

    /* match a regular expression */
    int re_match(char *txt, char *pat)
    {
        /* get the match */
        int len = itwb_->ReMatchA(txt, strlen(txt), pat, strlen(pat));

        /* if the search failed, return 0, otherwise the match length */
        return (len < 0 ? 0 : len);
    }

    /* search for a regular expression */
    int re_search(int dir, char **p, SciLine *lin, char *pat)
    {
        TextToFind ttf;
        int pos;

        /*
         *   set up to search from the current position to the start or end
         *   of the buffer, depending on the direction we're looking
         */
        ttf.chrg.cpMin = (p != 0
                          ? lin->gettxtpos(*p)
                          : call_sci(SCI_POSITIONFROMLINE, lin->lin));
        ttf.chrg.cpMax = (dir > 0 ? call_sci(SCI_GETTEXTLENGTH) : 0);
        ttf.lpstrText = pat;

        /* do the search */
        pos = call_sci(SCI_FINDTEXT, SCFIND_REGEXP, (LPARAM)&ttf);

        /* check the result */
        if (pos < 0)
        {
            /* failure */
            return FALSE;
        }
        else
        {
            /* found it - set up 'lin' and 'p' at the new position */
            lin->retrieve_line(call_sci(SCI_LINEFROMPOSITION, pos));
            if (p != 0)
            {
                *p = lin->getpostxt(
                    dir > 0 ? ttf.chrgText.cpMax : ttf.chrgText.cpMin);
            }

            /* indicate success */
            return TRUE;
        }
    }

    /* indent the current line to the given column */
    void indent_to_column(int lin, int indent)
    {
        /* apply the indentation to the current line */
        call_sci(SCI_SETLINEINDENTATION, lin, indent);
    }

    /* go to the indentation position on the current line */
    void go_to_indent()
    {
        /* get the current line */
        int lin = call_sci(SCI_LINEFROMPOSITION, call_sci(SCI_GETCURRENTPOS));

        /* go to the indentation position on the line */
        call_sci(SCI_GOTOPOS, call_sci(SCI_GETLINEINDENTPOSITION, lin));
        call_sci(SCI_CHOOSECARETX);
    }

    /* find the first non-whitespace character in a string */
    char *skip_spaces(char *p)
    {
        /* skip spaces */
        while (isspace(*p))
            ++p;

        /* return the pointer */
        return p;
    }

    /*
     *   Call Scintilla.  This uses the fast-path interface to the Scintilla
     *   window, which the Scintilla documentation describes as much faster
     *   than the SendMessage() interface.  It is otherwise similar: just
     *   pass in a SCI_xxx message ID for 'msg', along with any parameters
     *   the message requires.
     */
    int call_sci(int msg, int a = 0, int b = 0)
        { return sci_.invoke(msg, a, b); }

    /* call the various Workbench custom message handlers */
    LPARAM create_spot(int pos) { return sci_.create_spot(pos); }
    void delete_spot(LPARAM spot) { return sci_.delete_spot(spot); }
    int get_spot_pos(LPARAM spot) { return sci_.get_spot_pos(spot); }
    void set_spot_pos(LPARAM spot, int pos) { sci_.set_spot_pos(spot, pos); }

    /*
     *   get the standard newline sequence for the current file - this
     *   returns \r\n, \n, or \r, according to the newline mode that the
     *   underlying Scintilla document uses
     */
    const char *get_nl()
    {
        static const char *nl[] = { "\r\n", "\r", "\n" };

        /* ask Scintilla for its current newline mode */
        int mode = call_sci(SCI_GETEOLMODE);

        /* if we recognize the mode, return its newline sequence */
        if (mode >= 0 && mode < countof(nl))
            return nl[mode];
        else
            return "\n";
    }

    /*
     *   Do simple hanging indentation - indent the current line to the same
     *   level as the previous line, and leave the cursor at the indentation
     *   point in the current line.
     */
    void hanging_indent()
    {
        /* get the current line */
        int pos = call_sci(SCI_GETCURRENTPOS);
        int lin = call_sci(SCI_LINEFROMPOSITION, pos);

        /*
         *   if we're not at the first line, indent to the previous line's
         *   indentation position
         */
        if (lin > 0)
        {
            /* set the same indentation as the previous line */
            int ind = call_sci(SCI_GETLINEINDENTATION, lin - 1);
            call_sci(SCI_SETLINEINDENTATION, lin, ind);

            /* go to the indent position */
            pos = call_sci(SCI_GETLINEINDENTPOSITION, lin);
            call_sci(SCI_GOTOPOS, pos);
        }
    }

    /* format a C-style comment, grouping undo */
    void format_c_comment_undo(int l)
    {
        /* do the formatting in an undo group */
        call_sci(SCI_BEGINUNDOACTION);

        /* remember the current selection range as spots */
        LPARAM spot_a = create_spot(call_sci(SCI_GETSELECTIONSTART));
        LPARAM spot_b = create_spot(call_sci(SCI_GETSELECTIONEND));

        /* do the formatting */
        format_c_comment_noundo(l);

        /* restore the selection range */
        call_sci(SCI_SETSEL, get_spot_pos(spot_a), get_spot_pos(spot_b));
        delete_spot(spot_a);
        delete_spot(spot_b);

        /* done with the undo group */
        call_sci(SCI_ENDUNDOACTION);
    }

    /* internal comment formatter without undo grouping */
    void format_c_comment_noundo(int l)
    {
        SciLine lin(sci_, l);
        int startline;
        int found;
        int pos;
        char *p;

        /* seek back to the start of the comment */
        for (found = FALSE ; l > 0 && !found ; )
        {
            /* go back a line */
            lin.retrieve_line(--l);

            /* skip short lines */
            if (lin.len < 2)
                continue;

            /* if this is the start of the comment, we're set */
            for (p = lin.txt + lin.len - 1 ; p > lin.txt ; )
            {
                /* go back a character */
                --p;

                /* check for the comment-start sequence */
                if (*p == '/' && *(p+1) == '*')
                {
                    /* note that we found it, and stop looking */
                    found = TRUE;
                    startline = l;
                    break;
                }
            }
        }

        /* if we didn't find the start of the comment, do nothing */
        if (!found)
            return;

        /*
         *   Word-wrap the comment text within the bounds of the current
         *   line's indentation point and the right margin.  Add 5 characters
         *   of spacing for the column of '*'s at the left of the block.
         */
        int left = call_sci(SCI_GETLINEINDENTATION, l);
        int right = call_sci(SCI_GETEDGECOLUMN);
        int wid = (left + 5 > right - 20 ? 20 : right - left - 5);

        /*
         *   Insert a newline after the last space after the comment opener,
         *   but only if there's something more on the line after that.  This
         *   will leave things like JavaDoc-style flags (slash-star-star) and
         *   "border" lines intact, while ensuring that any ordinary comment
         *   text on the same line as the comment opener will be properly
         *   formatted with the body of the comment.
         */
        for (p += 2 ; *p != '\0' && isspace(*p) ; ++p) ;
        if (*p != '\0')
        {
            /* we found text on the opener line - add a newline here */
            call_sci(SCI_SETTARGETSTART, lin.gettxtpos(p));
            call_sci(SCI_SETTARGETEND, lin.gettxtpos(p));
            const char *nl = get_nl();
            call_sci(SCI_REPLACETARGET, strlen(nl), (LPARAM)nl);
        }

        /* start formatting on the line after the opener */
        ++l;

        /* keep going until we find the end of the comment */
        for (found = FALSE ; l < call_sci(SCI_GETLINECOUNT) && !found ; )
        {
            /* retrieve the current line */
            lin.retrieve_line(l);

            /* if this is the end line, we're done */
            if (re_match(lin.txt, ".*%*/"))
            {
                found = TRUE;
                break;
            }

            /* remove the leading '*' border */
            remove_c_comment_border(&lin);

            /*
             *   If the current line is empty (ignoring spaces), it's a
             *   paragraph break, so don't include it in the fill.
             *
             *   If the line starts with '.', it means that we want an
             *   explicit line break here, and we don't want to fill the
             *   line.
             */
            if (lin.len == 0 || *skip_spaces(lin.txt) == '\0'
                || lin.txt[0] == '.')
            {
                /* put in the '*' border */
                call_sci(SCI_SETTARGETSTART, lin.pos);
                call_sci(SCI_SETTARGETEND, lin.pos);
                call_sci(SCI_REPLACETARGET, 1, (int)"*");

                /* indent it to the proper point */
                call_sci(SCI_SETLINEINDENTATION, l, left + 1);

                /* go on to the next line */
                ++l;
                continue;
            }

            /*
             *   this is the start of the next paragraph - note it as the
             *   start position for our upcoming word-wrap
             */
            int startpos = lin.pos;

            /*
             *   Find the end of the paragraph.  The paragraph ends at (a)
             *   the next blank line, (b) the next line with a '.' in the
             *   first column (possibly after a '*'), or (c) the end of the
             *   comment.
             */
            for (++l ; l < call_sci(SCI_GETLINECOUNT) ; ++l)
            {
                int sp, nonsp;

                /* retrieve the current line */
                lin.retrieve_line(l);

                /* look for the comment ending */
                for (sp = nonsp = FALSE, p = lin.txt ; *p != '\0' ; ++p)
                {
                    /* if it's the comment closer, stop here */
                    if (*p == '*' && *(p+1) == '/')
                    {
                        found = TRUE;
                        break;
                    }

                    /* if it's a non-space, note it */
                    if (!isspace(*p))
                        nonsp = TRUE;

                    /* if it's a space after a non-space, note it */
                    if (isspace(*p) && nonsp)
                        sp = TRUE;
                }

                /* if we found the comment ender, we're done */
                if (found)
                {
                    /*
                     *   If there's anything else on the line before the
                     *   comment closer, AND separated from the comment
                     *   closer by a space, add a newline before the last
                     *   space before the closer, and format the current line
                     *   as part of the comment body.  If the line has a
                     *   solid block of non-space characters right up to the
                     *   comment closer, with only whitespace preceding it,
                     *   this must be a box-style border, so leave that
                     *   intact.
                     */
                    if (nonsp && sp)
                    {
                        /* back up to the last space */
                        for ( ; p > lin.txt && !isspace(*p) ; --p) ;

                        /* insert a newline here */
                        call_sci(SCI_SETTARGETSTART, lin.gettxtpos(p));
                        call_sci(SCI_SETTARGETEND, lin.gettxtpos(p));
                        const char *nl = get_nl();
                        call_sci(SCI_REPLACETARGET, strlen(nl), (LPARAM)nl);

                        /* retrieve the modified line */
                        lin.retrieve_line(lin.lin);

                        /*
                         *   forget that we found the closer after all - it's
                         *   on the next line now, so we'll find it again
                         *   when we get to the next line
                         */
                        found = FALSE;
                    }
                    else
                    {
                        /* the closer is alone on the line, so stop here */
                        break;
                    }
                }

                /* remove any border */
                remove_c_comment_border(&lin);

                /* if it's empty, or starts with a '.', we're done */
                if (lin.len == 0 || *skip_spaces(lin.txt) == '\0'
                    || lin.txt[0] == '.')
                    break;
            }

            /* end the paragraph at the end of the previous line */
            call_sci(SCI_SETTARGETSTART, startpos);
            call_sci(SCI_SETTARGETEND,
                     call_sci(SCI_POSITIONBEFORE, lin.pos));

            /* join the paragraph into one line, then word-wrap it */
            call_sci(SCI_LINESJOIN);
            word_wrap(wid, startpos, call_sci(SCI_GETTARGETEND));

            /* now go back and insert the '*' prefix in each line */
            l = call_sci(SCI_LINEFROMPOSITION,
                         call_sci(SCI_GETTARGETSTART));
            int endl = call_sci(SCI_LINEFROMPOSITION,
                                call_sci(SCI_GETTARGETEND));
            for (;;)
            {
                /* get this line */
                lin.retrieve_line(l);

                /* if we're past the end position, we're done */
                if (l > endl)
                    break;

                /* remove leading spaces */
                remove_c_comment_border(&lin);

                /* indent to the comment level */
                call_sci(SCI_SETLINEINDENTATION, l, left);

                /* insert the prefix */
                pos = call_sci(SCI_GETLINEINDENTPOSITION, l);
                call_sci(SCI_SETTARGETSTART, pos);
                call_sci(SCI_SETTARGETEND, pos);
                call_sci(SCI_REPLACETARGET, 5, (int)" *   ");

                /* on to the next line */
                ++l;
            }
        }

        /*
         *   indent the last line to the comment indentation level; go one
         *   level to keep the asterisks aligned, since the comment start
         *   sequence has the '/' before its '*'
         */
        if (found)
            call_sci(SCI_SETLINEINDENTATION, l, left + 1);
    }

    /* do simple word-wrapping over a region */
    void word_wrap(int col_width, int wrap_start, int wrap_end)
    {
        /* set up a spot to mark the end point, as it'll move as we edit */
        LPARAM spot_end = create_spot(wrap_end);

        /*
         *   Walk through the text looking for breakable positions.  We can
         *   break at any word boundaries.
         */
        int lastbrk = -1;
        for (int pos = wrap_start ; ; )
        {
            /* get the current character and style */
            int c = call_sci(SCI_GETCHARAT, pos);
            int s = call_sci(SCI_GETSTYLEAT, pos);

            /* note the starting column */
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
             *   text, stop here.
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
            if (!isspace(c) && lastbrk != -1 && col + 1 > col_width)
            {
                /* insert a newline at the last breakpoint */
                call_sci(SCI_SETSEL, lastbrk, lastbrk);
                call_sci(SCI_INSERTTEXT, lastbrk, (int)get_nl());

                /* remember the line number of the next line */
                int l = call_sci(SCI_LINEFROMPOSITION, lastbrk) + 1;

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

                /* we no longer have a recent breakpoint */
                lastbrk = -1;

                /* resume from the start of the new line */
                pos = call_sci(SCI_POSITIONFROMLINE, l);
                continue;
            }

            /* if this is a space, note it as a break point */
            if (isspace(c))
                lastbrk = pos;
        }

        /* done with the end spot */
        delete_spot(spot_end);
    }

    /* remove a comment border from a line of text */
    void remove_c_comment_border(SciLine *lin)
    {
        int len;

        /* check for a comment border */
        if ((len = re_match(lin->txt, "<space>*(%*<space>*)?")) != 0)
        {
            /* delete the border text */
            call_sci(SCI_SETTARGETSTART, lin->pos);
            call_sci(SCI_SETTARGETEND, lin->pos + len);
            call_sci(SCI_REPLACETARGET, 0, (int)"");

            /* re-retrieve the line */
            lin->retrieve_line(lin->lin);
        }

        /* strip trailing spaces */
        if (lin->len != 0 && isspace(lin->txt[lin->len-1]))
        {
            /* count spaces */
            int sp;
            for (sp = 2 ; sp < lin->len && isspace(lin->txt[lin->len-sp]);
                 ++sp) ;

            /* we stopped at the first non-space, so back up */
            sp -= 1;

            /* trim off this many characters from the end of the line */
            call_sci(SCI_SETTARGETSTART, lin->pos + lin->len - sp);
            call_sci(SCI_SETTARGETEND, lin->pos + lin->len);
            call_sci(SCI_REPLACETARGET, 0, (int)"");

            /* re-retrieve the line */
            lin->retrieve_line(lin->lin);
        }
    }

    /* the Scintilla window handle */
    HWND hwnd_;

    /* the Scintilla invoker */
    SciInvoker sci_;

    /* my callback interface to Workbench */
    ITadsWorkbench *itwb_;

    /* COM reference count */
    ULONG refcnt_;


    /*
     *   Cached configuration settings.  We retrieve this at initialization,
     *   and refresh them whenever we're notified of a settings change.
     */

    /* auto-indent mode (TWB_AUTOINDENT_xxx) */
    int auto_indent_;

    /* auto-format comments mode (TWB_FMTCOMMENT_xxx) */
    int format_comments_;

    /*
     *   Last start position for an "indent" command in the first column.
     *   There's a special case for the manual "indent" command: if the
     *   cursor is in the first column, and the text of the line also starts
     *   in the first column, we can't tell whether they want to insert a tab
     *   or re-syntax-indent.  We resolve this by doing the syntax indent on
     *   the first such command, but if the syntax indent leaves the line
     *   starting in the first column, we then do a regular tab insertion on
     *   the second attempt.  We detect the second attempt by keeping track
     *   of the position of the first attempt; when it's repeated, we can
     *   tell by consulting this variable.
     */
    int last_indent_cmd_start_;

    /* custom command IDs */
    UINT indent_cmd;
    UINT commentfmt_cmd;
    UINT stringfmt_cmd;
    UINT fill_para_cmd;
    UINT addcomment_cmd;
};

/* ------------------------------------------------------------------------ */
/*
 *   Additional editor mode implementation
 */

ITadsWorkbenchEditor * STDMETHODCALLTYPE CTadsWorkbenchEditorMode::
   CreateEditorExtension(HWND hwnd)
{
    /* create the extension object */
    CTadsWorkbenchEditor *ext = create_ext(hwnd);

    /* initialize it */
    if (ext != 0)
        ext->init();

    /* return it */
    return ext;
}


/* ------------------------------------------------------------------------ */
/*
 *   Base class for extension objects
 */
class CTadsWorkbenchExtension: public ITadsWorkbenchExtension
{
public:
    CTadsWorkbenchExtension(ITadsWorkbench *itwb)
    {
        /* remember the Workbench interface */
        itwb->AddRef();
        itwb_ = itwb;

        /* count an initial reference on behalf of the caller */
        refcnt_ = 1;
    }

    virtual ~CTadsWorkbenchExtension()
    {
        /* release our Workbench interface */
        itwb_->Release();
    }

    /* IUnknown reference-counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret;
        if ((ret = --refcnt_) == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchExtension)
            *ifc = (void *)(ITadsWorkbenchExtension *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

protected:
    /* my callback interface to Workbench */
    ITadsWorkbench *itwb_;

    /* my reference count */
    ULONG refcnt_;
};

#endif /* WB_ADDIN_H */
