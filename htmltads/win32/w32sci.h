/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  w32sci.h - TADS Workbench Scintilla interface
Function

Notes
  Markers #20-24 are reserved for use by extensions.
Modified
  09/23/06 MJRoberts  - Creation
*/

#ifndef W32SCI_H
#define W32SCI_H

#include "tadshtml.h"
#include "itadsworkbench.h"
#include "tadscom.h"
#include "w32fndlg.h"

/*
 *   clipboard stack entry
 */
struct sci_cb_ele
{
    /* set this text */
    void set(const textchar_t *txt, size_t len, int from_sys)
    {
        /* set the text and remember the length */
        this->txt.set(txt, len);
        this->len = len;

        /* remember whether or not it's from the system clipboard */
        this->from_sys = from_sys;
    }

    /* the text of the entry */
    CStringBuf txt;

    /*
     *   the length of the text in bytes - this might differ from
     *   strlen(txt.get()) because the text could contain null bytes
     */
    size_t len;

    /* is the data from the external system clipboard? */
    int from_sys;
};


/*
 *   scintilla window interface
 */
class ScintillaWin: public ITadsWorkbenchMsgHandler
{
public:
    /*
     *   load the Scintilla DLL - this must be called once during program
     *   initialization, before creating the first Scintilla window
     */
    static int load_dll();

    /* create a new Scintilla control */
    ScintillaWin(class IScintillaParent *parent);

    /* delete */
    virtual ~ScintillaWin();

    /* static termination */
    static void static_term();

    /* plug in as a new view on another window's document */
    void set_doc(ScintillaWin *win);

    /* get the editor extension object */
    ITadsWorkbenchEditor *get_extension() const { return ext_; }

    /* set the ITadsWorkbench extension object */
    void set_extension(ITadsWorkbenchEditor *ext)
    {
        /*
         *   update reference counts (add first, to avoid unwanted deletion
         *   in case ext == ext_), then remember the new extension
         */
        if (ext != 0)
            ext->AddRef();
        if (ext_ != 0)
            ext_->Release();
        ext_ = ext;
    }

    /* flag the start of a file load */
    void begin_file_load(void *&ctx);

    /* end a file load */
    void end_file_load(void *ctx);

    /* reset state to a freshly loaded file - no undo, not modified */
    void reset_state();

    /* set read-only mode */
    void set_readonly(int f);

    /* clear the 'dirty' status */
    void set_clean();

    /* handle a Workbench ID_SCI_xxx command */
    int handle_workbench_cmd(int id);

    /* set the repeat count for the next command */
    void set_repeat_count(int cnt) { repeat_count_ = cnt; }

    /* clipboard/selection operations */
    void select_all();
    void cut();
    void copy();
    void paste();
    void clear_selection();

    /*
     *   "paste-pop" pops the top element from the clipboard stack and
     *   re-inserts it at the bottom of the stack, removes the last inserted
     *   text, and pastes the new top-of-stack in its place; this only works
     *   immediately after a paste or paste-pop
     */
    void paste_pop();

    void cut_eol();

    int can_paste();
    int can_paste_pop();
    int can_copy();
    int can_cut();

    /* delete the character or selection; this obeys the repeat count */
    void del_char();

    /* insert a character; this obeys the repeat count */
    void insert_char(int c);

    /* selection mode (0=stream, 1=rectangle, 2=lines) */
    void selection_mode(int mode);

    /* get the current selection mode */
    int get_selection_mode();

    /* cancel selection mode */
    void cancel_sel_mode(int cancel_sel);

    /* undo/redo */
    int can_undo();
    int can_redo();
    void undo();
    void redo();

    /* begin/end an Undo group */
    void undo_group(int begin);

    /* show our splitter control */
    void show_splitter(int show)
    {
        /* note the new state */
        show_splitter_ = show;

        /* adjust the vertical scrollbar accordingly */
        adjust_vscroll();

        /* show or hide the splitter control window as needed */
        ShowWindow(splitter_, show ? SW_SHOW : SW_HIDE);
    }

    /*
     *   Save the contents of the window to the given filename.  Returns TRUE
     *   on success, FALSE on failure.
     */
    int save_file(const char *fname);

    /* get my window handle */
    HWND get_handle() const { return hwnd_; }

    /* handle a notification */
    int handle_notify(int notify_code, LPNMHDR nm);

    /* handle a WM_COMMAND sent from the Scintilla control */
    int handle_command_from_child(int notify_code);

    /*
     *   Set the font.  This sets the font for the 'default' style, and
     *   resets all other styles to match.
     */
    void set_base_font(const char *font_name, int point_size,
                       COLORREF fg_color, COLORREF bkg_color,
                       int override_selection_colors,
                       COLORREF sel_fg_color, COLORREF sel_bg_color);
    void set_base_font(const class CHtmlFontDesc *font,
                       const class CHtmlFontDesc *sel_font,
                       int override_selection_colors);

    /*
     *   Set the base custom style.  This is similar to set_base_font(), but
     *   it inherits the current base font settings.  So, this can be used to
     *   set the defaults within a particular mode as a second level of
     *   inheritance, where set_base_font() sets the global defaults that
     *   every mode inherits from.
     */
    void set_base_custom_style(const char *font_name, int point_size,
                               COLORREF fgcolor, COLORREF bgcolor,
                               int bold, int italic, int ul);
    void set_base_custom_style(const class CHtmlFontDesc *font);

    /* set a custom style */
    void set_custom_style(int stylenum, const char *font_name, int point_size,
                          COLORREF fgcolor, COLORREF bgcolor,
                          int bold, int italic, int ul);
    void set_custom_style(int stylenum, const class CHtmlFontDesc *font);

    /* set indent and tab options */
    void set_indent_options(int indent_size, int tab_size, int use_tabs);

    /* set wrapping options */
    void set_wrapping_options(int right_margin, int auto_wrap,
                              int show_guide);

    /* show/hide line numbers */
    void show_line_numbers(int show);

    /* show/hide the folding controls */
    void show_folding(int show);

    /* clear the document */
    void clear_document();

    /* append text */
    void append_text(const char *txt, size_t len);

    /* open a line at the current position */
    void open_line();

    /* transpose the character at the caret and character after the caret */
    void transpose_chars();

    /* print the document */
    void print_doc(HDC hdc, const POINT *pgsize, const RECT *margins,
                   const RECT *phys_margins, int selection,
                   int first_page, int last_page);

    /* get the line number at the cursor */
    long get_cursor_linenum();

    /* get the number of lines in the document */
    long get_line_count();

    /* get the cursor column */
    long get_cursor_column();

    /* get/set the document offset of the cursor position */
    long get_cursor_pos();
    void set_cursor_pos(long pos);

    /* get/set the current "anchor" position */
    long get_anchor_pos();
    void set_anchor_pos(long pos);

    /* get/set the 'mark' position */
    long get_mark_pos() { return get_spot_pos(mark_); }
    void set_mark_pos(long pos) { set_spot_pos(mark_, pos); }

    /* get the current selection range */
    void get_sel_range(long *a, long *b);

    /* set the selection range */
    void set_sel_range(long a, long b);

    /* select the current line */
    void select_current_line();

    /*
     *   Create a "spot."  A spot is a marked position in the document buffer
     *   that we keep up to date as text is inserted and deleted.
     */
    void *create_spot(long pos);

    /* change the position of a spot */
    void set_spot_pos(void *spot, long pos);

    /* get the position from a spot */
    long get_spot_pos(void *spot);

    /* delete a spot */
    void delete_spot(void *spot);

    /* get the boundaries of the word at the given position */
    void get_word_boundaries(long *start, long *end, long pos);

    /*
     *   Get the boundaries of the expression at the current position.
     *
     *   Step 1: if the cursor is at a square bracket ('[ ]') or parenthesis
     *   ('( )'), scan backward or forward to find the matching delimiter,
     *   taking into account nested delimiters; but only seek for a limited
     *   number of lines.
     *
     *   Step 2: repeat the following sub-steps until the range doesn't
     *   change:
     *
     *   Step 2a: if first character of the expression is '[', and the first
     *   non-blank character before the '[' is not an operator, find the
     *   limits of the expression starting at the first non-blank character
     *   before the first character of the expression, and extend the result
     *   boundaries backwards to include this preceding expression.
     *
     *   Step 2b: if the first non-blank character BEFORE the expression is
     *   '.', find the limits of the expression preceding the '.', and extend
     *   the result boundaries backwards to include the preceding expression.
     *
     *   If 'func' is true, we'll work forward into suffixing '( )' argument
     *   lists as well.
     */
    void get_expr_boundaries(long *start, long *end, long pos, int func);

    /* get the boundaries of a simple expression */
    void get_simple_expr_boundaries(long *start, long *end, long pos);

    /*
     *   get the boundaries of a () or [] delimited expression for the
     *   delimiter at 'pos'
     */
    void get_paren_boundaries(long *start, long *end, long pos);

    /*
     *   get the position of the first non-blank character before the given
     *   position
     */
    long get_prev_non_blank(long pos);

    /* get the text in the given range */
    void get_text(char *buf, long start, long end);

    /* get the text of the given line number */
    void get_line_text(CStringBuf *buf, long linenum);

    /* go to the given line */
    void go_to_line(long l);

    /* fill a paragraph */
    void fill_paragraph();

    /* search: returns true if we find a match, false if not */
    int search(const textchar_t *txt, int exact_case, int from_top,
               int wrap, int dir, int regex, int whole_word,
               int first_match_a, int first_match_b);

    /*
     *   Begin an incremental search.  Call this at the start of an i-search
     *   session to note the starting position.
     */
    void isearch_begin(struct SciIncSearchCtx *ctx);

    /*
     *   Update the search text for an incremental search.  Call this
     *   whenever the user enters another character (or deletes a character)
     *   of the query text.  We'll run the search again from the starting
     *   position; we'll return true if we can find a match, false if not.
     */
    int isearch_update(struct SciIncSearchCtx *ctx, const textchar_t *query,
                       int dir, int regex, int exact_case, int whole_word);

    /* go to the next/previous match */
    int isearch_next(struct SciIncSearchCtx *ctx, const textchar_t *query,
                     int dir);

    /* cancel the i-search and return to the starting position */
    void isearch_cancel(struct SciIncSearchCtx *ctx);

    /* set the line status to a combination of HTMLTDB_STAT_xxx codes */
    void set_line_status(long linenum, unsigned int stat);

    /* clear all line status indicators */
    void reset_line_status();

    /* get the next line number containing any of the given markers */
    int next_marker_line(int startline, unsigned int mask,
                         unsigned int *found_markers);

    /* add a bookmark at the given line number; returns the marker handle */
    int add_bookmark(int line);

    /* delete a bookmark */
    void delete_bookmark(int handle);

    /* get the line number for a given bookmark handle */
    int get_bookmark_line(int handle);

    /*
     *   jump to the next/previous bookmark after/before the current cursor
     *   position
     */
    int jump_to_next_bookmark(int dir);

    /* jump to the first/last bookmark in the file */
    void jump_to_first_bookmark();
    void jump_to_last_bookmark();

    /* jump to a bookmark by handle */
    void jump_to_bookmark(int handle);

    /* set the line wrapping mode: 'N' none, 'W' word, 'C' character */
    void set_wrap_mode(char mode);

    /* go the start/end of the window */
    void window_home();
    void window_end();

    /* center the caret vertically in the window */
    void vcenter_caret();

    /* scroll the window by the given number of lines and/or columns */
    void scroll_by(int lines, int columns);

    /* get the approximate page width in columns (for scroll_by) */
    int page_width();

    /* determine if the given client coordinates are in a margin */
    int pt_in_margin(int x, int y);

    /* move the cursor to the start of the line containing the mouse point */
    void cursor_to_line(int x, int y);

    /*
     *   Check a margin drag operation.  If the mouse is within the margin,
     *   we'll invisibly move the current position to the mouse position and
     *   return TRUE, filling in *oldpos with the previous cursor position.
     *   Otherwise we'll return FALSE.  The old position should be restored
     *   with end_margin_drag() when done.
     */
    int check_margin_drag(int x, int y, int *oldpos, int *linenum);

    /*
     *   Do a margin drag to the give line number.  This fills in *oldpos
     *   with the old source position and sets the source position to the
     *   given line.  The old position should be restored with
     *   end_margin_drag() when done.
     */
    void do_margin_drag(int linenum, int *oldpos);

    /* end a margin drag operation */
    void end_margin_drag(int oldpos);

    /*
     *   show or hide the current drag line indicator - this sets a margin
     *   status icon showing that this is the drop location for the current
     *   code location
     */
    void show_drag_line(int linenum, int show);

    /* the Scintilla DLL module handle */
    static HMODULE hmod_;

    /* IUnknown reference counting */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }
    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ret = --refcnt_;
        if (refcnt_ == 0)
            delete this;
        return ret;
    }

    /* IUnknown interface query */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ifc)
    {
        /* check for our known interfaces */
        if (iid == IID_IUnknown)
            *ifc = (void *)(IUnknown *)this;
        else if (iid == IID_ITadsWorkbenchMsgHandler)
            *ifc = (void *)(ITadsWorkbenchMsgHandler *)this;
        else
            return E_NOINTERFACE;

        /* add a reference on behalf of the caller, per COM conventions */
        AddRef();

        /* success */
        return S_OK;
    }

    /* ITadsWorkbenchMsgHandler message handler */
    LRESULT STDMETHODCALLTYPE CallMsgHandler(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /* replace the current Find selection */
    virtual void find_replace_sel(
        const textchar_t *txt, int dir, int regex);

    /* find-and-replace all */
    virtual void find_replace_all(
        const textchar_t *txt, const textchar_t *repl,
        int exact_case, int wrap, int direction, int regex, int whole_word);

private:
    /* call Scintilla */
    int call_sci(int msg, int a = 0, int b = 0)
        { return call_sci_(call_sci_ctx_, msg, a, b); }

    /* internal search handler */
    int search(const textchar_t *txt, int exact, int from_top,
               int wrap, int dir, int regex, int whole_word,
               struct TextToFind *ttf);

    /* internal incremental search handler */
    int isearch(struct SciIncSearchCtx *ctx, const textchar_t *query,
                int startpos);

    /* load a marker from an XPIXMAP resource */
    void load_marker(int id, int res);

    /* set our standard caret policy for interactive editing */
    void set_std_caret_policy();

    /* our subclassed window procedure */
    LRESULT winproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    /* subclasses WM_NOTIFY handler */
    int sc_notify(int control_id, NMHDR *nm, LRESULT *ret);

    /* adjust our vertical scrollbar position for the splitter button */
    void adjust_vscroll();

    /* try matching braces at the current position */
    void match_braces();

    /* cut or copy text to the system clipboard and our clipboard stack */
    void cut_or_copy(int cut);

    /*
     *   Add text to the clipboard.  This adds the text to the Scintilla
     *   system clipboard and to our internal clipboard stack.
     */
    void add_clipboard(const textchar_t *txt, size_t len,
                       int push, int from_sys);

    /*
     *   Read the system clipboard into our stack, if necessary.  We call
     *   this before any attempt to paste from the clipboard, to ensure that
     *   we take into account any clippings from another application or from
     *   another non-Scintilla window within our application.
     */
    void read_sys_clipboard();

    /*
     *   check for auto-wrapping - this can be called after inserting a
     *   character to determine if we should do word-wrapping on a line that
     *   exceeds the right margin
     */
    void check_auto_wrap();

    /* internal fill-paragraph processing */
    void fill_paragraph_i();

    /* is the given line a blank line (i.e., empty or all whitespace)? */
    int is_blank_line(int l);

    /* our static entrypoint for the subclassed Sci window procedure */
    static LRESULT CALLBACK s_winproc(
        HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        ScintillaWin *self;

        /* find our 'this' object; if we can't find it, we can't proceed */
        if ((self = (ScintillaWin *)GetProp(hwnd, self_prop_)) == 0)
            return 0;

        /* call our virtual window procedure */
        return self->winproc(hwnd, msg, wparam, lparam);
    }

    /* our reference count */
    int refcnt_;

    /* the window handle */
    HWND hwnd_;

    /* our original (pre-subclassing) window procedure */
    WNDPROC oldwinproc_;

    /* the parent interface */
    class IScintillaParent *parent_;

    /* the tooltip for the window */
    HWND tooltip_;

    /* Scintilla direct function interface */
    int (__cdecl *call_sci_)(void *, int, int, int);
    void *call_sci_ctx_;

    /* ITadsWorkbenchEditor extension object, if one has attached to us */
    ITadsWorkbenchEditor *ext_;

    /* our vertical scrollbar, if any */
    HWND vscroll_;

    /* our splitter button window */
    HWND splitter_;

    /* are we in auto-wrap mode? */
    int auto_wrap_;

    /* right margin for auto-wrapping */
    int right_margin_;

    /* location of last cut_eol */
    int last_cut_eol_pos_;

    /*
     *   Buffered text from past cut_eol operations.  When the user performs
     *   a series of cut_eol operations without any intervening cursor
     *   movement, the text from the series of cuts is combined for clipboard
     *   purposes, as though the whole series had been one big 'cut'
     *   operation.
     */
    CStringBuf cut_eol_buf_;
    size_t cut_eol_len_;

    /*
     *   The clipboard stack/ring.  Each time we cut or copy text to the
     *   clipboard, we push it onto the stack.  A 'paste' restores the top of
     *   the stack; 'paste-pop' rotates the stack (pops the top element and
     *   inserts it back at the bottom of the stack), removes the previously
     *   pasted text, and inserts the new top-of-stack instead.
     *
     *   Note that the clipboard stack is static, since it's shared among all
     *   Scintilla windows.
     */
    const static int CB_STACK_SIZE = 10;
    static sci_cb_ele cb_stack_[CB_STACK_SIZE];

    /* the current top element of the clipboard stack */
    static int cbs_top_;

    /* the number of elements in the cut stack */
    static int cbs_cnt_;

    /* range of last text inserted by paste or paste-pop */
    int paste_range_start_;
    int paste_range_end_;

    /* repeat count for next command or ordinary character */
    int repeat_count_;

    /*
     *   'mark' is a special spot we create; the user can set and refer to
     *   the position of 'mark' with a number of commands
     */
    void *mark_;

    /* head of 'spot' list */
    struct sci_spot *spots_;

    /* flag: are we in the course of loading a file? */
    unsigned int loading_ : 1;

    /* are we in selection mode? */
    unsigned int selmode_ : 1;

    /* are we showing our splitter control? */
    unsigned int show_splitter_ : 1;

    /* our splitter cursor */
    static HCURSOR split_csr_;

    /*
     *   array of mappings for our Workbench ID_xxx command identifiers to
     *   SCI_xxx messages
     */
    static UINT cmd_to_sci_[];

    /* have we initialized the command mapping array? */
    static int cmd_to_sci_inited_;

    /* initialize the command-mapping array */
    static void init_cmd_to_sci();

    /* our window property, for finding 'self' in our subclassed wndproc */
    static const char *self_prop_;

    /* custom message IDs */
    static UINT msg_create_spot;
    static UINT msg_delete_spot;
    static UINT msg_get_spot_pos;
    static UINT msg_set_spot_pos;
};

/*
 *   Spot list entry.  A "spot" is a marked position in the document buffer,
 *   which we automatically adjust to keep accurate as text is inserted or
 *   deleted.
 */
struct sci_spot
{
    sci_spot(long pos)
    {
        this->pos = pos;
        this->nxt = 0;
    }

    /* the document position marked by the spot */
    long pos;

    /* next spot in list */
    sci_spot *nxt;
};


/*
 *   Parent window interface
 */
class IScintillaParent
{
public:
    /* get the window handle */
    virtual HWND isp_get_handle() = 0;

    /* toggle breakpoint at the currently selected line */
    virtual void isp_toggle_breakpoint(ScintillaWin *sci) = 0;

    /* set the 'dirty' status of the document */
    virtual void isp_set_dirty(int dirty) = 0;

    /* receive notification of a change of some kind to the document or UI */
    virtual void isp_note_change() = 0;

    /* drag the current line pointer */
    virtual void isp_drag_cur_line(ScintillaWin *sci) = 0;

    /* receive notification of a focus change in the control */
    virtual void isp_on_focus_change(
        ScintillaWin *win, int gaining_focus) = 0;

    /* receive notification of a change in the cursor position */
    virtual void isp_update_cursorpos(int lin, int col) = 0;

    /* evaluate an expression for a tooltip */
    virtual int isp_eval_tip(const textchar_t *expr,
                             textchar_t *valbuf, size_t vallen) = 0;

    /* set a status message for a long-running process */
    virtual void isp_lrp_status(const textchar_t *msg) = 0;

    /* receive notification of a Windows Explorer file drop */
    virtual void isp_drop_file(const char *fname) = 0;
};

/*
 *   Incremental search context
 */
struct SciIncSearchCtx
{
    /* starting position of the search */
    int startpos;

    /* direction (1 = forward, -1 = reverse) */
    int dir;

    /* regular expression? */
    int regex;

    /* exact case? */
    int exact;

    /* whole word? */
    int whole_word;

    /* number of times the search has wrapped */
    int wrapcnt;

    /* did the last search attempt succeed? */
    int found;
};


#endif /* W32SCI_H */
