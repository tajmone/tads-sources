/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  itadsworkbench.h - defines TADS Workbench extension interfaces
Function
  TADS Workbench has a plug-in mechanism that lets you write an add-in DLL
  customize certain parts of Workbench.  We use ordinary COM interfaces for
  this.  Most compiled programming languages on Windows support COM, so you
  can write an extension in just about any language.

  The plug-in mechanism works like this.  When Workbench starts running,
  it scans its own folder (that is, the folder containing the Workbench
  application executable file, htmltdb3.exe) for all files with names ending
  in ".twbAddIn".  It tries to load each such file as a DLL; if that
  succeeds, it looks for an exported function in the DLL called
  "twb_connect".  This function must be defined as follows:

    __declspec(dllexport) IUnknown *twb_connect(ITadsWorkbench *)

  That's the C/C++ notation, so if you're using a different programming
  language, you'll have to adjust it for your language's notation.  If you
  can't read C, this defines a function named "twb_connect" that's "exported"
  from the DLL; the function takes one argument, which is a reference to an
  ITadsWorkbench interface object; and it returns a reference to an IUnknown
  COM interface object.

  If Workbench finds this function in your DLL, it calls it, passing in an
  ITadsWorkbench interface as the argument value.  You are expected to do two
  things with this argument: first, you should stash it somewhere for later
  use, such as in a static variable or a class member variable; second,
  because you're hanging onto it, call AddRef() on the interface to count
  your saved reference.  In C++, it's usually quite simple - just something
  like this:

    __declspec(dllexport) IUnknown *twb_connect(ITadsWorkbench *wb)
    {
      return new MyWorkbenchExtension(wb);
    }

  where MyWorkbenchExtension is a class that you define, which implements
  one or more of the interface extensions defined in this file.  The
  constructor to your class should stash away the ITadsWorkbench object in a
  member variable, and should add the reference:

    class MyWorkbenchExtension: public ITadsWorkbenchEditor
    {
    public:
      MyWorkbenchExtension(ITadsWorkbench *wb)
      {
        // add a self-reference on the caller's behalf
        this->AddRef();

        // count our reference, since we're saving this pointer
        wb->AddRef();

        // save it in our member variable
        this->wb = wb;
      }

      // implement the ITadsWorkbenchEditor methods here...

    private:
      ITadsWorkbench *wb;
    }


  BSTR conventions: in methods that take BSTR values as parameters, the
  caller is responsible for allocating and freeing the BSTR values.  If
  a method wants to keep a copy of any BSTR value passed in as a parameter,
  the method must make its own copy of the BSTR.  In methods that return
  BSTR values, the method is responsible for allocating the BSTR, and the
  caller is responsible for freeing it.


  MARKERS: Workbench explicitly reserves Scintilla "markers" #20-24 for
  exclusive use by editor extensions.  See the Scintilla documentation
  for information on what a marker is and how to use it; basically,
  markers serve as the visual icon indicators in the left margin, but
  they're also useful as a purely programmatic tool because they can
  track the location of a line as text is inserted and deleted in the
  file.

Notes

Modified
  10/05/06 MJRoberts  - Creation
*/

#ifndef ITADSWORKBENCH_H
#define ITADSWORKBENCH_H

#include <windows.h>
#include <ole2.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   The DLL "connect" function.  TADS Workbench calls this during start-up
 *   to initialize the DLL and exchange interface pointers.  The DLL must
 *   implement and export this function; if it doesn't, Workbench will ignore
 *   the DLL and assume it's not actually a Workbench extension plug-in.
 */
EXTERN_C __declspec(dllexport) IUnknown
   *twb_connect(struct ITadsWorkbench *itwb);



/* ------------------------------------------------------------------------ */
/*
 *   Regular expression match structure.  This describes the result of a
 *   regular expression search.
 */
struct twb_regex_match
{
    /* starting index of the match in the string */
    INT start;

    /* length in characters of the match */
    INT len;
};


/* ------------------------------------------------------------------------ */
/*
 *   Command status indicators.  These are used during menu and toolbar
 *   updates to control the visual status of a command item.
 */
#define TWB_CMD_UNKNOWN            0                /* unrecognized command */
#define TWB_CMD_ENABLED            1                     /* command enabled */
#define TWB_CMD_DISABLED           2                    /* command disabled */
#define TWB_CMD_CHECKED            3        /* command enabled and selected */
#define TWB_CMD_DISABLED_CHECKED   4       /* command disabled and selected */
#define TWB_CMD_MIXED              5  /* enabled, state indeterminate/mixed */
#define TWB_CMD_DISABLED_MIXED     6             /* disabled, indeterminate */
#define TWB_CMD_DEFAULT            7      /* default item - implies enabled */
#define TWB_CMD_DO_NOT_CHANGE      8     /* leave status as it is currently */


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbench.  This is the callback interface that TADS Workbench
 *   provides to extensions.
 *
 *   Workbench hands an interface of this type to the extension when it's
 *   first loaded.  Workbench does this by calling the extension's
 *   twb_connect() exported function; passing in an interface pointer.  The
 *   extension will probably want to stash the interface pointer for
 *   subsequent use, probably by storing it in a static variable in the DLL.
 *   Note that if it does this, the extension MUST add a reference to the
 *   interface by calling AddRef().
 */
extern const GUID IID_ITadsWorkbench;
DECLARE_INTERFACE_(ITadsWorkbench, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(
        THIS_ REFIID riid, LPVOID FAR *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*
     *   ITadsWorkbench methods
     */

    /*
     *   REGULAR EXPRESSIONS
     *
     *   Workbench has a powerful regular expression parser built in, which
     *   it uses for various purposes of its own.  Since regular expressions
     *   are so useful in general, Workbench makes the parser services
     *   available to extensions.  This can be particularly useful to editor
     *   extensions that provide syntax-based customization features, such as
     *   auto-indenting or syntax coloring.
     */

    /*
     *   Match a regular expression to a string.  This uses the TADS regular
     *   expression matcher syntax.  We return the length of the match, or -1
     *   if there is no match.
     *
     *   Note that this isn't a "search" operation: this simply matches the
     *   expression to the string, starting at its first character.
     *
     *   For the convenience of different language interfaces, we offer this
     *   in single-byte and wide-character versions.
     */
    STDMETHOD_(INT,ReMatchA)(THIS_ const char *txt, UINT txtlen,
                             const char *pat, UINT patlen) PURE;
    STDMETHOD_(INT,ReMatchW)(THIS_ const OLECHAR *txt, UINT txtlen,
                             const OLECHAR *pat, UINT patlen) PURE;

    /*
     *   Search for a regular expression within a string.  This searches
     *   through the string for a match to the patern.  If a match is found,
     *   we fill in the result structure and return TRUE; otherwise we return
     *   FALSE.
     */
    STDMETHOD_(INT,ReSearchA)(THIS_ const char *txt, UINT txtlen,
                              const char *pat, UINT patlen,
                              twb_regex_match *match) PURE;
    STDMETHOD_(INT,ReSearchW)(THIS_ const OLECHAR *txt, UINT txtlen,
                              const OLECHAR *pat, UINT patlen,
                              twb_regex_match *match) PURE;


    /*
     *   COMMANDS
     *
     *   Workbench handles menu and keyboard commands using a "command ID"
     *   scheme.  A command ID is an integer value that Workbench assigns to
     *   a command; it then associates that ID value with any menu item or
     *   key sequence bound to the command.  When the user selects a menu or
     *   presses a bound key sequence, Workbench looks up the command ID,
     *   then sends it to various subroutines within Workbench, called
     *   "command handlers," until it finds one that executes the command.
     *
     *   Some of the extension interfaces here allow extensions to register
     *   as command handlers, to supplement or override the standard
     *   Workbench handling of commands.  The tricky part is that the command
     *   IDs that Workbench assigns are essentially just arbitrary integer
     *   values, and are subject to change, so we need some way for
     *   extensions and Workbench to agree on the meaning of an ID value.  We
     *   also want a way for extensions to define additional commands of
     *   their own, so that (for example) an editor extension can define its
     *   own special extended commands that users can invoke via the
     *   keyboard.
     *
     *   This is where the "Command Name" system comes in.  In addition to
     *   the internal numeric command ID, Workbench gives each command a
     *   human-readable name.  The command name is designed to be stable
     *   across versions, and also to be readily extensible - so, extensions
     *   can easily add a new command just by inventing a new name.
     *
     *   The next several functions let extensions look up the command ID
     *   associated with a command name, and also let extensions define their
     *   own new commands.
     */

    /*
     *   Look up a command by name.  If the command is defined, this returns
     *   a non-zero value giving the command's ID code.  If it isn't, it
     *   returns zero.
     */
    STDMETHOD_(UINT,GetCommandID)(THIS_ const OLECHAR *command_name) PURE;

    /*
     *   Define a new command.  This adds the given command name to the
     *   master list of commands in Workbench, and assigns the command an ID
     *   number.  'command_name' is the name of the command (see below for
     *   naming rules), and 'desc' is a description of the command, to
     *   display to the user in the UI.
     *
     *   The return value is the newly assigned ID number.  If the command is
     *   already defined, this simply returns the ID of the existing command.
     *
     *   Workbench has a limited range of command IDs it can assign to
     *   extensions, so it's possible for this request to fail.  If the
     *   command ID cannot be assigned, the return value is zero.  (Don't
     *   worry, though - command IDs aren't exactly scarce.  As of this
     *   writing, Workbench reserves a range of 5000 IDs for custom commands.
     *   It seems extremely unlikely that add-ins will ever bump up against
     *   this limit; if we actually do start seeing a lot of custom commands
     *   in extensions, usability implications in the UI will require
     *   revisiting this whole scheme long before we get anywhere near the
     *   limit.)
     *
     *   Once a command is defined, it's available to the user, such as to
     *   bind to a key.  The user will see the name of the command in
     *   suitable placesin the UI, such as in the key-binding dialog.
     *
     *   The new command only remains defined for the current session - it's
     *   not saved in the persistent configuration information.  So,
     *   extensions must define their custom commands at each start-up.
     *
     *   Command names can contain ASCII alphanumeric characters and periods
     *   (".").  There's no hard limit on the length, but you should keep it
     *   within reason, since these names are visible to the user.  Command
     *   names can't contain spaces, punctuation marks, control characters,
     *   or accented letters.
     *
     *   Custom commands defined by extensions must always start with "Ext."
     *   - for example, "Ext.Edit.MyCommand".  The significance of the "Ext."
     *   prefix is that Workbench is tolerant of undefined "Ext." commands in
     *   saved configuration information - it won't report errors, for
     *   example, for saved key bindings that refer to "Ext." commands that
     *   haven't been defined.  This is important because a user might take a
     *   configuration file from one computer to another computer that
     *   doesn't have the same mix of add-ins installed; the new machine's
     *   set of add-ins might not define all of the same commands as the old
     *   machine's.  Rather than rejecting the saved configuration
     *   information in this case, Workbench will simply ignore the missing
     *   "Ext." commands.
     */
    STDMETHOD_(UINT,DefineCommand)(
        THIS_ const OLECHAR *command_name, const OLECHAR *desc) PURE;


    /*
     *   CONFIGURATION SETTINGS
     *
     *   Workbench has a "configuration" mechanism that stores the user's
     *   settings for the user interface environment in a disk file, so that
     *   the settings can be restored automatically on the next session.
     *   Workbench stores practically everything about the environment - the
     *   window layout, option settings, key bindings, etc.
     *
     *   From the program's perspective, the settings are represented as a
     *   set of name/value pairs.  The settings are stored in a hash table in
     *   memory, so it's quite fast to read or write a value.  The in-memory
     *   hash table is read from disk when a project is opened, and changes
     *   are saved back to disk at certain points, such as when beginning a
     *   test run, closing the project, or quitting out of Workbench.
     *
     *   There are actually two parallel sets of name/value pairs.  One is
     *   the "global" configuration - this is stored in a file in the
     *   Workbench directory, and is used for settings that are common to all
     *   projects.  The global configuration is used for most of the
     *   "environment" settings, such as the key bindings and editor options.
     *   The other set is the "local" or "project" configuration.  This is
     *   stored within the project file: in TADS 2, this is a binary file
     *   that's in the game's source code directory, and in TADS 3, it's the
     *.  t3m file that also contains the list of source files and build
     *   options and so forth.  The local configuration has the settings
     *   related to the specific files in the project, such as the source
     *   file window layout; and of course also stores the compiler settings
     *   for the project.
     *
     *   The next several functions provide access to the configuration
     *   settings.  Note that the set of name/value pairs is readily
     *   extensible - you're free to invent new key names of your own if you
     *   want to store additional configuration information beyond what
     *   Workbench ordinarily stores.  It's also perfectly legal to ask for
     *   the value of a key that doesn't exist - the retrieval functions will
     *   simply return a suitable result to tell you so.
     */

    /*
     *   Look up a property with a string value.  This retrieves a property
     *   value from the Workbench local or global configuration file.  If
     *   'global' is TRUE, this retrieves the property from the global
     *   (Workbench) configuration; otherwise it retrieves it from the local
     *   (current project) configuration.  The 'key' is an arbitrary name for
     *   a collection of related properties; a given add-in will generally
     *   want to use a single key name for all of its properties, although
     *   this isn't required.  'subkey' is an arbitrary name identifying the
     *   individual property of the key.  The 'key' and 'subkey' names can
     *   contain any printable characters except colons.  The 'idx' is an
     *   index value; a given key:subkey can contain any number of string
     *   values.  Properties are indexed contiguously from zero.
     *
     *   If there's no such key, or there's no such index for the given key,
     *   the return value is null.
     *
     *   The caller is responsible for freeing the returned BSTR.
     */
    STDMETHOD_(BSTR,GetPropValStr)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey,
        UINT idx) PURE;

    /*
     *   Get the number of string values stored for the given property.  This
     *   gives the range of valid indices for GetPropStr().
     */
    STDMETHOD_(UINT,GetPropValStrCount)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey) PURE;

    /*
     *   Get an integer property value.  Unlike string properties, integer
     *   properties are single-valued, so there's no index.  This returns the
     *   value stored in the configuration; if the value isn't set, it
     *   returns default_val.
     */
    STDMETHOD_(INT,GetPropValInt)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey,
        INT default_val) PURE;

    /*
     *   Clear a property string list.  This deletes all of the existing
     *   indexed property strings stored under a given property key.
     */
    STDMETHOD_(void,ClearPropValStr)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey) PURE;

    /*
     *   Set a property string value.  This replaces or adds a property value
     *   to the given key:subkey key with the given index value.  Values need
     *   not be indexed contiguously; adding a value with an index more than
     *   one higher than the current last index will add empty values at the
     *   intermediate index values to fill in the gap.
     */
    STDMETHOD_(void,SetPropValStr)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey,
        UINT idx, const OLECHAR *val) PURE;

    /* Set a property integer value. */
    STDMETHOD_(void,SetPropValInt)(
        THIS_ BOOL global, const OLECHAR *key, const OLECHAR *subkey,
        INT val) PURE;
};


/* ------------------------------------------------------------------------ */
/*
 *   Scintilla extension command messages.  The Workbench version of
 *   Scintilla defines some additional command messages beyond the standard
 *   SCI_xxx set that the generic version of Scintilla defines.  These
 *   extended commands provide additional functionality that add-ins can call
 *   upon.  Send these as Windows messages to the Scintilla window handle.
 *
 *   For future compatibility and interoperability, these are defined as
 *   named messages, so you'll need to look up the message names via
 *   RegisterWindowMessage to get the message ID.  For example, to call
 *   "w32tdb.sci.CreateSpot", you'd do this:
 *
 *.     UINT create_spot_id = RegisterWindowMessage("w32tdb.sci.CreateSpot");
 *.     SendMessage(sci_hwnd, create_spot_id, pos, 0);
 *
 *   LPARAM "w32tdb.sci.CreateSpot" (int pos) - create a "spot," which is a
 *   marker in the buffer.  The spot initially has the given buffer position.
 *   The spot is automatically kept in sync with the buffer as changes are
 *   made, so it will always refer to the same text position.  Returns a spot
 *   handle that can be used for subsequent operations on the spot.
 *
 *   void "w32tdb.sci.DeleteSpot" (0, LPARAM spot) - delete a spot.  This
 *   should be called when the spot is no longer needed, as spots require
 *   some memory.  The window will automatically delete any remaining spots
 *   when the window itself is destroyed.
 *
 *   int "w32tdb.sci.GetSpotPos" (0, LPARAM spot) - returns the current
 *   buffer position for the given spot handle.
 *
 *   void "w32tdb.sci.SetSpotPos" (int pos, LPARAM spot) - changes the given
 *   spot handle to refer to the given buffer position.
 */


/* ------------------------------------------------------------------------ */
/*
 *   Window message handler.  This is implemented by the Workbench Scintilla
 *   window, and is passed to the editor extension's message filter routine
 *   so that the message filter can invoke the next routine in line.
 */
extern const GUID IID_ITadsWorkbenchMsgHandler;
DECLARE_INTERFACE_(ITadsWorkbenchMsgHandler, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(
        THIS_ REFIID riid, LPVOID FAR *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /* ITadsWorkbenchMsgHandler methods */

    /*
     *   Call the window message handler.  This invokes the default window
     *   procedure for the filtered message.  A message filter method should
     *   call this for every message it doesn't completely override, to let
     *   the default window procedure perform its normal handling.
     */
    STDMETHOD_(LRESULT,CallMsgHandler)(
        THIS_ HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) PURE;
};


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbenchExtension.  This is the basic interface that extensions
 *   provide to Workbench.  After connecting to the DLL, Workbench will call
 *   QueryInterface() on the COM object handed back from the DLL to obtain
 *   this interface.  Note that we use this two-step procedure - first
 *   obtaining an IUnknown, then using QueryInterface to get the actual
 *   interface of interest - because it makes it cleaner to add new
 *   interfaces in future versions.
 */
extern const GUID IID_ITadsWorkbenchExtension;
DECLARE_INTERFACE_(ITadsWorkbenchExtension, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(
        THIS_ REFIID riid, LPVOID FAR *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*
     *   ITadsWorkbenchExtension methods
     */

    /*
     *   Each extension can define one or more editing "modes."  A mode is a
     *   collection of customizations for a particular type of file; for
     *   example, the "tads3" mode is for TADS 3 source files.  A mode is
     *   described by an ITadsWorkbenchEditorMode object.
     *
     *   This method lets Workbench determine which modes are supported by an
     *   extension.  Workbench calls this during startup to obtain the
     *   extension's mode list.  Workbench calls this for index 0, 1, 2,
     *   etc., incrementing the index until the method returns null.
     */
    STDMETHOD_(struct ITadsWorkbenchEditorMode *,GetMode)
        (THIS_ UINT index) PURE;
};


/* ------------------------------------------------------------------------ */
/*
 *   style information structure
 */
typedef struct wb_style_info sb_style_info;
struct wb_style_info
{
    /*
     *   The Scintilla style index of this style.  This corresponds to the
     *   style index values used by the Scintilla lexers.
     */
    int sci_index;

    /*
     *   The font foreground and background colors.  Use
     *   WBSTYLE_DEFAULT_COLOR to inherit the system-wide defaults.
     */
    COLORREF fgcolor;
    COLORREF bgcolor;
#define WBSTYLE_DEFAULT_COLOR 0xFFFFFFFF

    /*
     *   The font name.  Use an empty string to inherit the system-wide
     *   default typeface.
     */
    OLECHAR font_name[64];

    /*
     *   The font point size.  Use zero to inherit the system-wide default
     *   font size.
     */
    UINT ptsize;

    /*
     *   Style flags - this is a combination of the WBSTYLE_xxx flags below.
     *   The system-wide default font is plain text (no bold, italic, etc),
     *   so setting this to zero uses plain text.
     */
    UINT styles;
#define WBSTYLE_BOLD        0x0001
#define WBSTYLE_ITALIC      0x0002
#define WBSTYLE_UNDERLINE   0x0004

    /*
     *   The human-readable title for the style.  This is displayed in the
     *   Workbench user interface for editor preferences, to describe what
     *   this style is used for in this mode.  This way, the user never has
     *   to see the raw style index values - the user always sees a nice
     *   human-readable description of each style entry.
     *
     *   If this is left blank, the style is not displayed in the user
     *   interface.  This means that the user won't be able to manually
     *   change this style's default values.  This can be used for any styles
     *   that the lexer for the mode doesn't use.
     */
    OLECHAR title[64];

    /*
     *   Sample text for this style.  This is used in the preferences dialog
     *   box to show an example of the style.
     */
    OLECHAR sample[64];

    /*
     *   Custom keyword set associated with this style.  Scintilla allows the
     *   container application to define up to 9 sets of keywords.  These are
     *   passed along to the lexer, which can use them to mark keywords from
     *   each set with a distinct style code.  The TADS3 mode uses this, for
     *   example, to let the user give a custom appearance to up to three
     *   sets of user-defined keywords.
     *
     *   If this is 0, there are no custom keywords associated with this
     *   style.  If this is a positive value, it means that there are
     *   keywords associated with this style.  These keywords will be stored
     *   in the configuration file under this key:
     *
     *.     MODE-editor-prefs . custom-keywords-N
     *
     *   where MODE is the name of the editor mode, and N is the value given
     *   here.
     */
    int keyword_set;
};

/* ------------------------------------------------------------------------ */
/*
 *   File Type Information structure.  This is used to describe a file type
 *   to Workbench.
 */
typedef struct wb_ftype_info wb_ftype_info;
struct wb_ftype_info
{
    /*
     *   The type's desktop icon.  The icon should include at least the
     *   standard "large desktop icon" format - 48 by 48 pixels.
     */
    HICON icon;

    /*
     *   Should Workbench destroy the icon explicitly when done with it?
     *   This should be TRUE if the icon is created via CreateIconIndirect(),
     *   CopyIcon(), etc.; it must be FALSE if the icon is loaded with
     *   LoadIcon() is otherwise "shared" (see the Win32 API docs).
     *
     *   The typical case is to use LoadIcon() to load an icon from a
     *   resource file, in which case this should be FALSE.
     */
    BOOL need_destroy_icon;

    /*
     *   The name of the type.  This is a short name to display to the user
     *   to describe the type of file, such as via a label on the icon.
     */
    OLECHAR name[64];

    /*
     *   A description of the type.  This can be displayed to the user, such
     *   as via a tooltip, to provide a more detailed (than the name)
     *   description of the file type.
     */
    OLECHAR desc[256];

    /*
     *   The default extension for the type.  When the user creates a new
     *   file of this type, Workbench remembers this default extension, so
     *   that it can use it in the Save As dialog when the user saves the
     *   file for the first time.  This is the bare extension string - DO NOT
     *   include a leading dot.  For example, for a plain text file, this
     *   would be L"TXT".
     */
    OLECHAR defext[32];
};


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbenchEditorMode.  This is a description of an editor mode.
 *
 *   A "mode" is a set of customizations for a particular type of file.  For
 *   example, the "tads3" mode is for TADS 3 source and header files.  The
 *   customizations typically include syntax coloring and auto-indent rules.
 *
 *   (A file type generally implies a mode, but a mode isn't strictly
 *   identical to a file type, because some modes might handle multiple
 *   types.  The tads3 mode handles TADS 3 source and header files, for
 *   example, and a C mode could handle both C and C++, and possibly even
 *   Java.)
 */
extern const GUID IID_ITadsWorkbenchEditorMode;
DECLARE_INTERFACE_(ITadsWorkbenchEditorMode, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(
        THIS_ REFIID riid, LPVOID FAR *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*
     *   Get the "mode" name that this extension defines.  This returns a
     *   string giving a unique, human-readable identifier for the mode.  For
     *   example, the TADS 3 add-in uses "tads3" as its mode name.
     *
     *   The purpose of an extension is to provide a collection of special
     *   customizations for a particular kind of file.  For tads 3 files, we
     *   want a certain type of syntax coloring, custom indenting rules, and
     *   so on; we want slightly different rules for all of these things for
     *   tads 2 files, or for makefiles, and so on.  Taken together, the
     *   collection of special behaviors for a particular kind of file is
     *   called the "mode."
     */
    STDMETHOD_(BSTR,GetModeName)(THIS) PURE;

    /*
     *   Get information on the given mode-specific style.  Workbench calls
     *   this during initialization to get the Scintilla style settings for
     *   the mode's lexer.  The 'index' value indicates which style Workbench
     *   is requesting.  This is an index into the extension's style list,
     *   and doesn't need to match the Scintilla style index, since the
     *   Scintilla index is indicated via the structure.
     *
     *   Styles must be defined contiguously from index 0 to the N-1, where N
     *   is the number of styles indicated by GetStyleCount().
     */
    STDMETHOD_(void,GetStyleInfo)(THIS_ UINT index, wb_style_info *info);

    /*
     *   get the number of mode-specific styles - this returns the number of
     *   styles that can be retrieved through GetStyleInfo
     */
    STDMETHOD_(UINT,GetStyleCount)(THIS) PURE;

    /*
     *   Get this mode's list of types for creating new files.  When the user
     *   tells Workbench to create a new file from scratch, Workbench offers
     *   the user a list of the types of files it knows how to handle, as
     *   defined through the various editor modes.  The user then selects one
     *   of these types, which lets Workbench activate the correct editor
     *   mode extension for the new file's window.
     *
     *   'index' is the requested index into the mode's type list.  Workbench
     *   calls this with index values starting at 0 and increasing until this
     *   function returns FALSE to indicate that no more file types are
     *   available.  That's how Workbench determines how many types there are
     *   for this mode.
     *
     *   If the index is within range (0 .. n-1, where n is the total number
     *   of types this mode defines), the function should fill in the '*info'
     *   structure's fields (see the structure definition for details) and
     *   return TRUE.  Otherwise, simply return FALSE.
     */
    STDMETHOD_(BOOL,GetFileTypeInfo)(
        THIS_ UINT index, wb_ftype_info *info) PURE;

    /*
     *   Get the "affinity" of this mode for a given file.  'filename' is the
     *   name of the file in question, as a null-terminated Unicode string.
     *   The return value is the affinity, as detailed below.
     *
     *   When the user tells Workbench to open a file, Workbench first
     *   submits the name of the file to be opened to each registered mode to
     *   determine which mode has the highest affinity for the file.
     *   Workbench then uses that mode as the default editing extension for
     *   the file.  (The user can manually select a different extension
     *   later, but we'd like to avoid that manual step most of the time by
     *   automatically selecting the right mode to start with.)
     *
     *   If the mode can't handle the given file, the return value is zero.
     *
     *   If this mode is suitable for handling this file, the return value is
     *   a positive integer giving the affinity of the mode for the file.
     *   The higher the value, the stronger the affinity.  There might be
     *   more than one mode that can handle a particular file, because some
     *   modes are more specific than others in the types of files they
     *   handle.  For example, a "plain text" mode could handle things like
     *   HTML and XML files, but an HTML mode is more specifically designed
     *   for HTML files, so it would have a higher affinity for a file whose
     *   name ends in ".htm" or ".html" than the plain text mode would.
     *
     *   As a guideline, use a value of 100 if you match on the filename
     *   extension, 200 if you match on a filename wildcard pattern that's
     *   more specific than just an extension match, 300 if you match on a
     *   directory prefix, 400 if you match on the exact root filename with
     *   no wildcards (not including directory path), and 500 if you match on
     *   the exact, entire filename, including directory path.  You can
     *   ignore these guidelines and use any value you like, but the
     *   mechanism will work best if all extensions use the priority values
     *   consistently.  If you match on something not covered by these
     *   guidelines (such as the contents of the file), try to interpolate a
     *   priority within the guidelines based on the specificity of your
     *   match criteria.  As a slightly special case, the "plain text" mode
     *   built into Workbench uses affinity 1 for any file at all - it's the
     *   mode of last resort, so any other mode naturally has priority over
     *   it, hence its use of the lowest non-rejecting affinity.
     *
     *   In most cases, an extension will determine its affinity based on the
     *   filename extension.  For example, the TADS extension will take any
     *   file whose name ends in ".t" or ".h" to be a TADS source file, so it
     *   will want to handle those files.
     */
    STDMETHOD_(UINT,GetFileAffinity)(THIS_ const OLECHAR *filename) PURE;

    /*
     *   Create an editor extension for this mode for the given Scintilla
     *   window.  This returns a new ITadsWorkbenchEditor object to serve as
     *   the extension handler for the window.  Workbench calls this after
     *   the file has been loaded.
     *
     *   It's valid to return a null extension object.  This simply tells
     *   Scintilla to use its default behavior with no mode-specific
     *   customizations.  Virtually any mode other than "plain text" will
     *   want to make some kind of customizations, since otherwise there's
     *   not much point in defining a mode in the first place; but it's
     *   conceivable that a mode might want to *explicitly* recognize certain
     *   file types that would otherwise be claimed by other modes, and force
     *   them to plain-text mode.
     */
    STDMETHOD_(struct ITadsWorkbenchEditor *,CreateEditorExtension)(
        THIS_ HWND scintilla_hwnd) PURE;

    /*
     *   Get the file dialog filename filter pattern for this mode, for the
     *   purposes of the Windows Get(Open|Save)FileName API.  This returns a
     *   string in the lpstrFilter format, like this:
     *
     *   "Text Files (*.txt, *.text)\0*.txt;*.text\0"
     *
     *   You can include as many filter strings as you'd like, per the usual
     *   Windows conventions: each one should be null-terminated, and the
     *   entire filter string sequence should have an extra null character at
     *   the end.
     *
     *   'for_save' is TRUE if the filter is for a Save As dialog, FALSE if
     *   it's for an Open dialog.  This usually makes a difference only if
     *   the mode handles multiple file types.  For example, the TADS 3 mode
     *   handles ".t" source files and ".h" header files, which have the same
     *   editor handling but are in some sense different file types - at the
     *   very least, they use different default extensions.  The point of the
     *   'for_save' differentiation is as follows: for an Open dialog, your
     *   returned string can gather all of the different file types that this
     *   mode handles under a single filter name, as in "TADS
     *   Files\0*.t;*.h\0".  This is desirable because it makes the filter
     *   list more compact than it would be if each extension had its own
     *   separate filter.  For a Save As dialog, though, the returned string
     *   should instead use separate filters for the different types: "TADS
     *   Source Files\0*.t\0TADS Header Files\0*.h\0".  This is less compact
     *   than the Open format, but it's better for the Save As dialog,
     *   because it lets the user choose the exact file type - and thus the
     *   correct default extension - for the saved file.
     *
     *   '*priority' must be filled in with a number indicating the sorting
     *   order of this filter in the overall list.  The filter list is sorted
     *   in ascending order of priority, so the lower the number, the earlier
     *   in the list this filter goes.  The TADS 3 mode uses priority 100,
     *   and the "plain text" filter uses 10000; other add-in modes should
     *   use values between these unless for some reason they want to go
     *   first or last in the list.  Generally, users will want the TADS file
     *   types going first, since those will presumably be the most
     *   frequently used files in Workbench.
     */
    STDMETHOD_(BSTR,GetFileDialogFilter)(
        THIS_ BOOL for_save, UINT *priority) PURE;
};


/* ------------------------------------------------------------------------ */
/*
 *   ITadsWorkbenchEditor.  This is an interface that extensions can provide
 *   to customize the Scintilla text editor feature in Workbench.
 */
extern const GUID IID_ITadsWorkbenchEditor;
DECLARE_INTERFACE_(ITadsWorkbenchEditor, IUnknown)
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(
        THIS_ REFIID riid, LPVOID FAR *ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;

    /*
     *   ITadsWorkbenchEditor methods
     */

    /*
     *   Receive notification of a configuration settings change that affects
     *   editor windows.  Workbench will call this when any change is made to
     *   the configuration that's typically of interest to editor windows; in
     *   particular, for changes to the formatting or indenting options.  For
     *   efficiency, editor extensions can cache property values that affect
     *   their operation, and update them in response to this notification;
     *   this saves editors the trouble of calling back into Workbench for
     *   the property settings every time they're needed, which could be
     *   quite frequently if the user is actively typing into the editor
     *   window.
     */
    STDMETHOD_(void,OnSettingsChange)(THIS) PURE;

    /*
     *   Filter a Scintilla window message.  This is called for every message
     *   sent to the Scintilla window, and is called BEFORE the Scintilla
     *   window's own window procedure processes the message.  This lets the
     *   extension override Scintilla's normal handling of the message.
     *
     *   The routine must call 'def_handler->CallMsgHandler()' in order
     *   to pass the message to the normal Scintilla window procedure, UNLESS
     *   this routine wants to override the message completely.
     *
     *   The return value is the message result, which is returned to Windows
     *   as the result from handling the message.
     */
    STDMETHOD_(LRESULT,MsgFilter)(
        THIS_ HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
        ITadsWorkbenchMsgHandler *def_handler) PURE;

    /*
     *   Receive notification that the user has typed an ordinary character.
     *   This is called whenever Scintilla sends an SCN_CHARADDED
     *   notification to its container (i.e., Workbench).  This gives the
     *   extension a chance to perform auto-indenting.  'ch' is the character
     *   added, expressed in the buffer character set.
     */
    STDMETHOD_(void,OnCharAdded)(THIS_ UINT ch) PURE;

    /*
     *   Test the status of a menu/toolbar command.  When a text editor
     *   window is active, Workbench calls this when it needs to check the
     *   status of a menu item or toolbar button, to give the extension a
     *   chance to determine the visual state of the item.
     *
     *   'command_id' is the ID number of the command.  You can get the IDs
     *   of built-in commands via ITadsWorkbench::GetCommandID(), and you can
     *   define extended commands of your own via DefineCommand().
     *
     *   Generally, this routine is called in two situations.  First, when
     *   the user opens a menu, it's called for each command item in the menu
     *   to determine how the menu should appear.  Second, it's called
     *   periodically (once a second or so) for each button in the visible
     *   toolbars.  Obviously, then, this routine needs to be relatively fast
     *   - you shouldn't do any lengthy computations here.  If you need to do
     *   anything too complicated to determine the status of a command,
     *   you're better off skipping the text here, leaving the command
     *   visually enabled unconditionally, and making the more complex check
     *   in your command execution handler instead.
     *
     *   The exact order of command processing in Workbench depends on the
     *   command; some commands are handled globally by the debugger or IDE,
     *   and are handled before a text editor window ever sees them.  But
     *   most commands are routed to the active text editor window (if any)
     *   first, and the text editor window will always call this routine as
     *   its first step.  This means that you can override Workbench's
     *   default handling for most commands, and certainly for all commands
     *   that are specifically related to editing text.  Workbench doesn't
     *   handle any custom extension-defined commands itself, for reasons
     *   that should be obvious, so you'll always have exclusive control over
     *   those.
     *
     *   If you recognize the command and want to set its status, fill in
     *   '*status' with a TWB_CMD_xxx status code and return TRUE.  If you
     *   don't handle the command, return FALSE to let the Workbench editor
     *   window apply its default handling.
     *
     *   'menu' and 'menu_index' give the menu handle and item index within
     *   the menu when the command is being tested for displaying a menu.
     *   'menu' is null when the command is being tested for something other
     *   than a menu (such as for a toolbar).  In most cases, you can simply
     *   ignore these parameters, because Workbench will automatically update
     *   the menu item's visual status based on the '*status' code you
     *   provide - you don't need to fuss with the menu directly if all you
     *   want to do is gray it out, enable it, show a checkmark, or anything
     *   else that the status codes can represent.  However, in some cases
     *   it's desirable to actually change the menu item's name; this is why
     *   the information is provided.  You're free to use the appropriate
     *   Win32 APIs to update the menu item if you need to.  (However, you
     *   shouldn't make any changes to the overall menu structure - you
     *   shouldn't insert or delete items, for example, or rearrange them.
     *   Doing so could confuse Workbench by invalidating its internal notion
     *   of the arrangement of its menus.)
     */
    STDMETHOD_(BOOL,GetCommandStatus)(
        UINT command_id, UINT *status, HMENU menu, UINT menu_index) PURE;

    /*
     *   Execute a command.
     *
     *   If you recognize the command and wish to handle it, then you should
     *   simply carry out the command and return TRUE.  Returning TRUE
     *   indicates to Workbench that you've fully handled the command, so no
     *   further Workbench default handling is needed.  Workbench will thus
     *   skip its built-in handling (if any) for the command.  If you don't
     *   handle the command, simply return FALSE.  Note that you can perform
     *   processing of your own and still return FALSE if you also want the
     *   built-in Workbench handling for the command.
     *
     *   Command execution is generally routed to handlers the same way as
     *   described in GetCommandStatus().  This means that Workbench executes
     *   some "global" commands before giving editor windows a shot at the
     *   command; but for the most part, Workbench routes commands to editor
     *   windows first, which in turn route them to extensions first, so you
     *   can override the built-in handling for most commands.
     *
     *   Note that this routine might be called without GetCommandStatus()
     *   ever having been called for the same command.  It might also be
     *   called for a command even if GetCommandStatus() indicated that the
     *   command should be disabled.  Therefore, if there are conditions
     *   under which the command isn't allowed, you must check them here
     *   before carrying out the command - you can't count on Workbench
     *   having vetoed the command on the basis of its status.
     */
    STDMETHOD_(BOOL,DoCommand)(UINT command_id) PURE;
};


#endif /* ITADSWORKBENCH_H */
