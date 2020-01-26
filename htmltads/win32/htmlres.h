/* $Header: d:/cvsroot/tads/html/win32/htmlres.h,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $ */

/*
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  htmlres.h - HTML-TADS resource header
Function
  Defines symbolic names for integer resource IDs for icons, bitmaps,
  dialogs and dialog controls, menus, menu commands, and so on.

  Command IDs can have some additional information specified via specially
  formatted comments.  Just after the #define COMMAND_ID nnn line, a comment
  like this can appear:

  ///cmd Name Description
  /// description continuation starts with a space after the three slashes
  /// and more continuation lines can follow
  ///tads2

  The Name is a string giving a mnemonic name for the command.  This is a
  series of any non-space characters, but by convention it's alphanumeric,
  with Java-style inter-caps, and with a '.' to indicate a simple two-level
  hierarchy: "File.Open".  We use the hierarchy to organize the commands
  into groups, to make it easier for users to find a command they're looking
  for.  These names are meant to be presented to the user, such as via a
  keyboard, menu, or toolbar customizer dialog.

  The Description is simply a narrative description of the command; this can
  be displayed on the status line as "micro help" when a menu item for the
  command is selected, and can also be displayed in the command browser
  dialog for keyboard, menu, and toolbar customization.

  The last line is optional.  If present, it indicates that the command is
  used only in one version of the system.  The valid keywords are "tads2",
  meaning the command is valid only in TADS 2 Workbench, and "tads3",
  meaning it's valid only in TADS 3 Workbench.

  These comments are designed to be extracted via a simple tool that scans
  this source file looking for the comments.  The tool can build a command
  list for inclusion in the build.
Notes

Modified
  10/26/97 MJRoberts  - Creation
*/

#ifndef HTMLRES_H
#define HTMLRES_H


#define IDC_STATIC                      -1

#define IDI_EXE_ICON                    1
#define IDI_GAM_ICON                    2
#define IDI_SAV_ICON                    3
#define IDI_TDC_ICON                    4
#define IDI_GAM3_ICON                   5
#define IDI_SAV3_ICON                   6
#define IDI_T_ICON                      7
#define IDI_H_ICON                      8
#define IDI_TL_ICON                     9
#define IDI_RESFILE_ICON                10
#define IDI_SYSFILE_ICON                11
#define IDI_MAINWINICON                 12
#define IDI_DBGWIN                      13
#define IDI_TXTFILE_ICON                14

#define IDI_DBG_CURLINE                 200
#define IDI_DBG_BP                      201
#define IDI_DBG_BPDIS                   202
#define IDI_DBG_CURBP                   203
#define IDI_DBG_CURBPDIS                204
#define IDI_DBG_CTXLINE                 205
#define IDI_DBG_CTXBP                   206
#define IDI_DBG_CTXBPDIS                207
#define IDI_BOX_PLUS                    208
#define IDI_BOX_MINUS                   209
#define IDI_VDOTS                       210
#define IDI_HDOTS                       211
#define IDI_NO_SOUND                    212
#define IDB_BMP_PAT50                   213
#define IDB_BMP_SMALL_CLOSEBOX          214
#define IDB_BMP_START_NEW               215
#define IDB_BMP_START_OPEN              216
#define IDB_PROJ_ROOT                   217
#define IDB_PROJ_FOLDER                 218
#define IDB_PROJ_SOURCE                 219
#define IDB_PROJ_RESOURCE               220
#define IDB_PROJ_RES_FOLDER             221
#define IDB_PROJ_EXTRESFILE             222
#define IDB_PROJ_IN_INSTALL             226
#define IDB_PROJ_NOT_IN_INSTALL         227
#define IDB_TERP_TOOLBAR                228
#define IDI_MOVE_UP                     229
#define IDI_MOVE_DOWN                   230
#define IDB_CHEST_SEARCH                231
#define IDB_PROJ_LIB                    232
#define IDB_PROJ_CHECK                  233
#define IDB_PROJ_NOCHECK                234
#define IDB_MDI_TAB_TOOLBAR             235

#define IDX_DBG_CURLINE                 236
#define IDX_DBG_CTXLINE                 237
#define IDX_DBG_BP                      238
#define IDX_DBG_BPDIS                   239
#define IDX_DBG_BPCOND                  240
#define IDX_DBG_DRAGLINE                241

#define IDB_PROPSHEETTREE               242
#define IDI_RIGHTARROW                  243
#define IDB_BMP_START_OPENLAST          244
#define IDB_BMP_START_BKG               245
#define IDB_CARDCATALOG                 246
#define IDB_PROJ_WEBEXTRA               247
#define IDB_PROJ_SCRIPT                 248

#define IDX_DBG_BOOKMARK                249
#define IDB_BACKSPACE_BTN               250


#define IDS_MORE                        4
#define IDS_DEBUG_GO                    5
#define IDS_DEBUG_STEPINTO              6
#define IDS_DEBUG_STEPOVER              7
#define IDS_DEBUG_RUNTOCURSOR           8
#define IDS_DEBUG_BREAK                 9
#define IDS_DEBUG_SETCLEARBREAKPOINT    10
#define IDS_DEBUG_DISABLEBREAKPOINT     11
#define IDS_DEBUG_EVALUATE              12
#define IDS_VIEW_GAMEWIN                13
#define IDS_VIEW_WATCH                  14
#define IDS_VIEW_LOCALS                 15
#define IDS_VIEW_STACK                  16
#define IDS_DEBUG_SHOWNEXTSTATEMENT     17
#define IDS_DEBUG_SRCWIN                18
#define IDS_DEBUG_STEPOUT               19
#define IDS_DEBUG_SETNEXTSTATEMENT      20
#define IDS_FILE_SAFETY                 21
#define IDS_QUITTING                    22
#define IDS_MEM                         23
#define IDS_DEBUG_PROMPTS               24
#define IDS_DEBUG_EDITOR                25
#define IDS_BUILD_SRC                   26
#define IDS_BUILD_INC                   27
#define IDS_BUILD_OUTPUT                28
#define IDS_BUILD_DEFINES               29
#define IDS_BUILD_ADVANCED              30
#define IDS_NEWWIZ_WELCOME              31
#define IDS_NEWWIZ_SOURCE               32
#define IDS_NEWWIZ_GAMEFILE             33
#define IDS_NEWWIZ_TYPE                 34
#define IDS_NEWWIZ_SUCCESS              35
#define IDS_LDSRCWIZ_WELCOME            36
#define IDS_LDSRCWIZ_GAMEFILE           37
#define IDS_LDSRCWIZ_SUCCESS            38
#define IDS_BUILD_INSTALL               39
#define IDS_INFO_DISPNAME               40
#define IDS_INFO_SAVE_EXT               41
#define IDS_INFO_ICON                   42
#define IDS_INFO_LICENSE                43
#define IDS_INFO_PROGDIR                44
#define IDS_INFO_STARTFOLDER            45
#define IDS_INFO_NONE                   46
#define IDS_BUILD_COMPDBG               47
#define IDS_BUILD_COMP_AND_RUN          48
#define IDS_STARTING                    49
#define IDS_DEBUG_SRCPATH               50
#define IDS_INFO_README                 51
#define IDS_INFO_README_TITLE           52
#define IDS_FILE_LOADGAME               53
#define IDS_FILE_SAVEGAME               54
#define IDS_FILE_RESTOREGAME            55
#define IDS_EDIT_CUT                    56
#define IDS_EDIT_COPY                   57
#define IDS_EDIT_PASTE                  58
#define IDS_CMD_UNDO                    59
#define IDS_VIEW_FONTS_NEXT_SMALLER     60
#define IDS_VIEW_FONTS_NEXT_LARGER      61
#define IDS_VIEW_OPTIONS                62
#define IDS_GO_PREVIOUS                 63
#define IDS_GO_NEXT                     64
#define IDS_EDIT_FIND                   65
#define IDS_FILE_TERMINATEGAME          66
#define IDS_BTN_ADD_GAMES               67
#define IDS_GCH_SEARCH_FOLDER_TITLE     68
#define IDS_GCH_SEARCH_FOLDER_PROMPT    69
#define IDS_GAME_CHEST_SEARCH_ROOT      70
#define IDS_GAME_CHEST_SEARCH_RESULTS   71
#define IDS_GAME_CHEST_SEARCH_GROUP     72
#define IDS_BUILD_DIAGNOSTICS           73
#define IDS_DEBUG_STARTOPT              74
#define IDS_GAMECHEST_PREFS             75
#define IDS_MANAGE_PROFILES             76
#define IDS_CUSTOMIZE_THEME             77
#define IDS_BUILD_INTERMEDIATE          78
#define IDS_INFO_CHARLIB                79
#define IDS_APPEARANCE                  80
#define IDS_KEYS                        81
#define IDS_COLORS                      82
#define IDS_FONT                        83
#define IDS_MEDIA                       84
#define IDS_THEMEDESC_MULTIMEDIA        85
#define IDS_THEMEDESC_PLAIN_TEXT        86
#define IDS_THEMEDESC_WEB_STYLE         87
#define IDS_LINKS_ALWAYS                88
#define IDS_LINKS_CTRL                  89
#define IDS_LINKS_NEVER                 90
#define IDS_THEMES_DROPDOWN             91
#define IDS_MUTE_SOUND                  92
#define IDS_BUILD_STOP                  93
#define IDS_MORE_STATUS_MSG             94
#define IDS_WORKING_MSG                 95
#define IDS_PRESS_A_KEY_MSG             96
#define IDS_EXIT_PAUSE_MSG              97
#define IDS_MORE_PROMPT                 98
#define IDS_FIND_NO_MORE                99
#define IDS_NO_GAME_MSG                 100
#define IDS_GAME_OVER_MSG               101
#define IDS_CANNOT_OPEN_HREF            102
#define IDS_LINK_PREF_CHANGE            103
#define IDS_SHOW_DBG_WIN_MENU           104
#define IDS_ABOUT_GAME_WIN_TITLE        105
#define IDS_NO_RECENT_GAMES_MENU        106
#define IDS_REALLY_QUIT_MSG             107
#define IDS_REALLY_GO_GC_MSG            108
#define IDS_REALLY_NEW_GAME_MSG         109
#define IDS_CHOOSE_GAME                 110
#define IDS_CHOOSE_NEW_GAME             111
#define IDS_ABOUTBOX_1                  112
#define IDS_ABOUTBOX_2                  113
#define IDS_COMCTL32_WARNING            114
#define IDS_SET_DEF_PROFILE             115
#define IDS_NEWWIZ_BIBLIO               116
#define IDS_SEARCH_DOC                  117
#define IDS_SEARCH_PROJECT              118
#define IDS_SEARCH_FILE                 119
#define IDS_FILE_CREATEGAME             120
#define IDSDB_FILE_LOADGAME             121
#define IDS_FILE_OPEN                   122
#define IDS_FILE_SAVE                   123
#define IDS_FILE_SAVE_ALL               124
#define IDS_EDIT_UNDO                   128
#define IDS_EDIT_REDO                   129
#define IDS_TPL_SAVE_BLANK              130
#define IDS_TPL_SAVE_FNAME              131
#define IDS_TPL_SAVE_BLANK_AS           132
#define IDS_TPL_SAVE_FNAME_AS           133
#define IDS_FILE_NEW                    134
#define IDS_TPL_FIND_FNAME              135
#define IDS_TPL_FIND_BLANK              136
#define IDS_DEBUG_SYNTAX_COLORS         137
#define IDS_DEBUG_INDENT                138
#define IDS_CONFIRM_RESTORE_MODE_DEFAULTS  139
#define IDS_DEBUG_WINMODE               140
#define IDS_DEBUG_KEYBOARD              141
#define IDS_TPL_PROJOPEN_BLANK          143
#define IDS_TPL_PROJOPEN_FNAME          144
#define IDS_TPL_EDITTEXT_BLANK          145
#define IDS_TPL_EDITTEXT_FNAME          146
#define IDS_TPL_PROJREM_BLANK           147
#define IDS_TPL_PROJREM_FNAME           148
#define IDS_TPL_PROJADD_BLANK           149
#define IDS_TPL_PROJADD_FNAME           150
#define IDS_DEBUG_EXTERNTOOLS           151
#define IDS_BUILD_ZIP                   152
#define IDS_BUILD_WEBPAGE               153
#define IDS_BUILD_SRCZIP                154
#define IDS_BUILD_SCRIPTS               155
#define IDS_DEBUG_REPLAY_SESSION        156
#define IDS_DEBUG_AUTOBUILD             157
#define IDS_HELP_GOBACK                 158
#define IDS_HELP_GOFORWARD              159
#define IDS_HELP_REFRESH                160
#define IDS_HELP_TADSMAN                161
#define IDS_HELP_TUTORIAL               162
#define IDS_HELP_T3LIB                  163
#define IDS_HELP_TOPICS                 164
#define IDS_BUILD_SPECIAL_FILES         165
#define IDS_DEBUG_WRAP                  166
#define IDS_EDIT_TOGGLEBOOKMARK         167
#define IDS_EDIT_SETNAMEDBOOKMARK       168
#define IDS_EDIT_POPBOOKMARK            169
#define IDS_EDIT_JUMPNEXTBOOKMARK       170
#define IDS_EDIT_JUMPNAMEDBOOKMARK      171
#define IDS_EDIT_ADDCOMMENT             172
#define IDS_EDIT_JUMPPREVBOOKMARK       173
#define IDS_SUBZIPREADME1               174
#define IDS_SUBZIPREADME2               175
#define IDS_DEBUG_EXTS_PATH             176
#define IDS_NET_SAFETY                  177
#define IDS_NEWWIZ_UI                   188

#define DLG_COLORS                      107
#define DLG_KEYS                        108
#define DLG_FONT                        109
#define DLG_MORE                        110
#define DLG_FILE_SAFETY                 111
#define DLG_MEDIA                       112
#define IDB_BMPMORE1                    113
#define IDB_BMPMORE2                    114
#define IDR_MAIN_MENU                   115
#define IDR_EDIT_POPUP_MENU             116
#define IDR_DEBUGWIN_MENU               117
#define IDR_DEBUGMAIN_MENU              118
#define IDR_DEBUGINFOWIN_POPUP_MENU     119
#define DLG_MEM                         120
#define IDC_SPLIT_EW_CSR                128
#define IDC_SPLIT_NS_CSR                129
#define IDB_DEBUG_TOOLBAR               131
#define IDB_BMP_TABPIX                  135
#define IDR_DEBUG_EXPR_POPUP            137
#define IDR_PROJ_POPUP_MENU             138
#define IDR_DEBUGSRCWIN_POPUP_MENU      139
#define IDB_VDOTS                       140
#define IDB_HDOTS                       141
#define IDR_STATUSBAR_POPUP             142
#define DLG_APPEARANCE                  143
#define IDR_DOCSEARCH_POPUP_MENU        144
#define IDR_FILESEARCH_POPUP_MENU       145
#define IDR_DOCKWIN_POPUP               150
#define IDR_DOCKEDWIN_POPUP             151
#define IDR_DOCKABLEMDI_POPUP           152
#define IDR_MDICLIENT_POPUP             157
#define IDR_DEBUGLOG_POPUP              158
#define IDC_DRAG_MOVE_CSR               159
#define IDC_BMP_1                       160
#define IDC_BMP_2                       161
#define IDR_TOOLBAR_POPUP               162
#define IDR_SEARCH_MODE_POPUP           163
#define IDC_DRAG_TAB_CSR                164
#define IDR_DEBUGSRCWIN_CAPTION_MENU    165
#define IDR_CMDARGS_MENU                166
#define IDR_INITDIR_MENU                167
#define IDR_SPECIALFILE_MENU            168
#define IDR_SCRIPT_MENU                 169
#define IDR_DEBUGSCRIPTWIN_POPUP_MENU   170
#define IDR_HELP_POPUP_MENU             171
#define IDC_DRAG_LINK_CSR               172
#define IDR_DEBUGSRCMARGIN_POPUP_MENU   173
#define DLG_NET_SAFETY                  174
#define IDR_WEBUI_POPUP                 175
#define IDR_WEBUI_CHILD_POPUP           176


/*
 *   menu items for Command Args and Init Dir menus - these are popup menus,
 *   so we can simply make these IDs local to the menus rather than fitting
 *   them into the general command scheme
 */
#define ID_CA_ITEMPATH                  1
#define ID_CA_ITEMDIR                   2
#define ID_CA_ITEMFILE                  3
#define ID_CA_ITEMEXT                   4
#define ID_CA_PROJDIR                   5
#define ID_CA_PROJFILE                  6
#define ID_CA_CURLINE                   7
#define ID_CA_CURCOL                    8
#define ID_CA_CURTEXT                   9
#define ID_ID_ITEM                      10
#define ID_ID_PROJ                      11

#define IDR_ACCEL_WIN                   900
#define IDR_ACCEL_EMACS                 901
#define IDR_ACCEL_DEBUGMAIN             902
#define IDC_RB_CTLV_EMACS               1002
#define IDC_RB_CTLV_PASTE               1003
#define IDC_RB_ARROW_SCROLL             1004
#define IDC_RB_ARROW_HIST               1005
#define IDC_MAINFONTPOPUP               1006
#define IDC_MONOFONTPOPUP               1007
#define IDC_TXTCOLOR                    1008
#define IDC_LINKCOLOR                   1009
#define IDC_HLINKCOLOR                  1010
#define IDC_ALINKCOLOR                  1011
#define IDC_BKCOLOR                     1012
#define IDC_CK_USEWINCLR                1013
#define IDC_CK_OVERRIDECLR              1014
#define IDC_CK_UNDERLINE                1020
#define IDC_RB_MORE_NORMAL              1022
#define IDC_RB_MORE_ALT                 1023
#define IDC_CK_HOVER                    1024
#define IDC_RB_SAFETY0                  1025
#define IDC_RB_SAFETY1                  1026
#define IDC_RB_SAFETY2                  1027
#define IDC_RB_SAFETY3                  1028
#define IDC_RB_SAFETY4                  1029
#define IDC_RB_ALTV_EMACS               1030
#define IDC_RB_ALTV_WIN                 1031
#define IDC_STXTCOLOR                   1032
#define IDC_SBKCOLOR                    1033
#define IDC_FONTSERIF                   1034
#define IDC_FONTSANS                    1035
#define IDC_FONTSCRIPT                  1036
#define IDC_FONTTYPEWRITER              1037
#define IDC_POP_MAINFONTSIZE            1038
#define IDC_POP_MONOFONTSIZE            1039
#define IDC_POP_SERIFFONTSIZE           1040
#define IDC_POP_SANSFONTSIZE            1041
#define IDC_POP_SCRIPTFONTSIZE          1042
#define IDC_POP_TYPEWRITERFONTSIZE      1043
#define IDC_POP_INPUTFONTSIZE           1044
#define IDC_RB_SAFETY5                  1045
#define IDC_RB_SAFETY0W                 1046
#define IDC_RB_SAFETY1W                 1047
#define IDC_RB_SAFETY4W                 1048
#define IDC_RB_SAFETY0R                 1049
#define IDC_RB_SAFETY2R                 1050
#define IDC_RB_SAFETY4R                 1051
#define IDC_CK_INPFONT_NORMAL           1101
#define IDC_POP_INPUTFONT               1102
#define IDC_BTN_INPUTCOLOR              1103
#define IDC_CK_INPUTBOLD                1104
#define IDC_CK_INPUTITALIC              1105
#define IDC_CK_GRAPHICS                 1106
#define IDC_CK_SOUND_FX                 1107
#define IDC_CK_MUSIC                    1108
#define IDC_POP_SHOW_LINKS              1109
#define IDC_POP_MEM                     1150

#define DLG_BREAKPOINTS                 2000
#define IDC_LB_BP                       2001
#define IDC_BTN_NEWGLOBAL               2002
#define IDC_BTN_GOTOCODE                2003
#define IDC_BTN_CONDITION               2004
#define IDC_BTN_REMOVE                  2005
#define IDC_BTN_REMOVEALL               2006
#define IDC_BTN_DISABLEBP               2007

#define DLG_BPCOND                      2050
#define IDC_EDIT_COND                   2051
#define IDC_RB_TRUE                     2052
#define IDC_RB_CHANGED                  2053

#define DLG_FIND                        2100
#define IDC_CBO_FINDWHAT                2101
#define IDC_CK_MATCHCASE                2102
#define IDC_CK_WRAP                     2103
#define IDC_CK_STARTATTOP               2104
#define IDC_RB_UP                       2105
#define IDC_RB_DOWN                     2106
#define IDC_BTN_FINDNEXT                2107
#define IDC_RB_CURFILE                  2108
#define IDC_RB_ALLFILES                 2109
#define IDC_RB_PROJFILES                2110

#define DLG_REGEXFIND                   2110
#define IDC_CK_REGEX                    2111
#define IDC_CK_WORD                     2112

#define DLG_FILEFIND                    2120
#define IDC_CK_COLLAPSESP               2121

#define DLG_REPLACE                     2130
#define IDC_BTN_REPLACE                 2131
#define IDC_BTN_REPLACEALL              2132
#define IDC_CBO_REPLACE                 2134

#define DLG_LOADING                     2140
#define IDC_TXT_LOADSTAT                2141

#define DLG_DEBUG_SRCWIN                2200
#define IDC_BTN_DBGTXTCLR               2201
#define IDC_BTN_DBGBKGCLR               2202
#define IDC_CK_DBGWINCLR                2203
#define IDC_CBO_DBGFONT                 2204
#define IDC_CBO_DBGFONTSIZE             2205
#define IDC_CBO_DBGTABSIZE              2206
#define IDC_CBO_DBGINDENTSIZE           2207
#define IDC_CK_USE_TABS                 2208
#define IDC_CB_AUTOINDENT               2209
#define IDC_CK_FORMAT_COMMENTS          2210
#define DLG_DEBUG_INDENT                2220
#define DLG_DIRECTX_WARNING             2320
#define IDC_STATIC_DXVSN                2321
#define IDC_STATIC_DXINST               2322
#define IDC_CK_SUPPRESS                 2323
#define IDC_STATIC_DXVSN2               2324
#define IDC_STATIC_DXINST2              2325
#define IDC_STATIC_DXVSN3               2326
#define IDC_STATIC_DXINST3              2337
#define IDC_POP_THEME                   2338
#define IDC_STATIC_SAMPLE               2339
#define IDC_BTN_CUSTOMIZE               2340

#define DLG_QUITTING                    2350
#define IDC_RB_CLOSE_QUITCMD            2351
#define IDC_RB_CLOSE_PROMPT             2352
#define IDC_RB_CLOSE_IMMEDIATE          2353
#define IDC_RB_POSTQUIT_EXIT            2354
#define IDC_RB_POSTQUIT_KEEP            2355

#define DLG_STARTING                    2360
#define IDC_CK_ASKGAME                  2361
#define IDC_FLD_INITFOLDER              2362

#define DLG_GAMECHEST_PREFS             2370
#define IDC_FLD_GC_FILE                 2371
#define IDC_FLD_GC_BKG                  2372
#define IDC_BTN_DEFAULT                 2373
#define IDC_BTN_DEFAULT2                2374

#define DLG_PROFILES                    2380
#define IDC_CBO_PROFILE                 2381

#define DLG_NEW_PROFILE                 2390
#define IDC_FLD_PROFILE                 2391

#define IDD_FOLDER_SELECTOR2            2400
#define IDC_BTN_NETWORK                 2401
#define IDC_FLD_FOLDER_PATH             2402
#define IDC_LB_FOLDERS                  2403
#define IDC_CBO_DRIVES                  2404
#define IDC_TXT_PROMPT                  2405

#define DLG_BUILD_DIAGNOSTICS           2410
#define IDC_CK_VERBOSE                  2411
#define IDC_RB_WARN_NONE                2412
#define IDC_RB_WARN_STANDARD            2413
#define IDC_RB_WARN_PEDANTIC            2414
#define IDC_CK_WARN_AS_ERR              2415

#define DLG_DBGWIZ_START                2420
#define IDC_CK_SHOWAGAIN                2421
#define IDC_BTN_CREATE                  2422
#define IDC_BTN_OPEN                    2423
#define IDC_ST_CREATE                   2424
#define IDC_ST_OPEN                     2425
#define IDC_ST_WELCOME                  2426
#define IDC_ST_OPENLAST                 2427
#define IDC_ST_OPENLAST2                2428
#define IDC_BTN_OPENLAST                2429

#define DLG_DEBUG_PROMPTS               2430
#define IDC_CK_SHOW_WELCOME             2431
#define IDC_CK_SHOW_GAMEOVER            2432
#define IDC_CK_SHOW_BPMOVE              2433
#define IDC_CK_ASK_EXIT                 2434
#define IDC_CK_ASK_LOAD                 2435
#define IDC_CK_ASK_RLSRC                2436
#define IDC_CK_CLR_DBGLOG               2437
#define IDC_CK_ASK_SAVE_ON_BUILD        2439

#define DLG_DEBUG_EDITOR                2450
#define IDC_FLD_EDITOR                  2451
#define IDC_FLD_EDITOR_ARGS             2452
#define IDC_BTN_BROWSE                  2453
#define IDC_BTN_ADVANCED                2454
#define IDC_BTN_AUTOCONFIG              2455

#define DLG_DBG_EDIT_ADV                2460
#define IDC_FLD_DDESERVER               2461
#define IDC_FLD_DDETOPIC                2462
#define IDC_CK_USE_DDE                  2463
#define IDC_FLD_DDEPROG                 2464

#define DLG_BUILD_SRC                   2470
#define IDC_FLD_SOURCE                  2471
#define IDC_FLD_RSC                     2472
#define IDC_BTN_ADDFILE                 2473
#define IDC_BTN_ADDDIR                  2474
#define IDC_BTN_ADDRS                   2475

#define DLG_BUILD_OUTPUT                2480
#define IDC_FLD_GAM                     2481
#define IDC_BTN_BROWSE2                 2482
#define IDC_FLD_EXE                     2483
#define IDC_FLD_RLSGAM                  2484
#define IDC_BTN_BROWSE3                 2485
#define IDC_BTN_BROWSE4                 2486

#define DLG_BUILD_ADVANCED              2490
#define IDC_CK_CASE                     2491
#define IDC_CK_C_OPS                    2492
#define IDC_CK_CHARMAP                  2493
#define IDC_FLD_CHARMAP                 2494
#define IDC_FLD_OPTS                    2495
#define IDC_BTN_HELP                    2496
#define IDC_CK_PREINIT                  2497
#define IDC_CK_DEFMOD                   2498
#define IDC_CK_SRCTXTGRP                2499

#define DLG_BUILD_INC                   2500
#define IDC_FLD_INC                     2501

#define DLG_BUILD_DEFINES               2510
#define IDC_FLD_DEFINES                 2511
#define IDC_FLD_UNDEF                   2512
#define IDC_BTN_ADD                     2513

#define DLG_ADD_MACRO                   2520
#define IDC_FLD_MACRO                   2521
#define IDC_FLD_MACRO_EXP               2522

#define DLG_BUILD_WAIT                  2530
#define IDC_BUILD_DONE                  2531

#define DLG_BUILD_INSTALL               2540
#define IDC_FLD_INSTALL_EXE             2541
#define IDC_FLD_INSTALL_OPTS            2542
#define IDC_BTN_EDIT                    2543
#define IDC_CK_ALLPACKAGES              2544

#define DLG_INSTALL_OPTIONS             2550
#define IDC_FLD_DISPNAME                2551
#define IDC_FLD_SAVE_EXT                2552
#define IDC_FLD_LICENSE                 2553
#define IDC_FLD_PROGDIR                 2554
#define IDC_FLD_STARTFOLDER             2555
#define IDC_FLD_INFO                    2556
#define IDC_FLD_README                  2557
#define IDC_FLD_README_TITLE            2558
#define IDC_FLD_CHARLIB                 2559

#define DLG_DEBUG_SRCPATH               2570
#define IDC_FLD_SRCPATH                 2571
#define IDC_CK_READONLY                 2572

#define DLG_DEBUG_STARTOPT              2580
#define IDC_RB_START_WELCOME            2581
#define IDC_RB_START_RECENT             2582
#define IDC_RB_START_NONE               2583

#define DLG_NEWWIZ_WELCOME              2600
#define DLG_NEWWIZ_SOURCE               2601
#define DLG_NEWWIZ_GAMEFILE             2602
#define DLG_NEWWIZ_TYPE                 2603
#define DLG_NEWWIZ_SUCCESS              2604
#define DLG_LDSRCWIZ_WELCOME            2605
#define DLG_LDSRCWIZ_GAMEFILE           2606
#define DLG_LDSRCWIZ_SUCCESS            2607
#define DLG_NEWWIZ_UI                   2608

#define IDC_FLD_NEWSOURCE               2610
#define IDC_FLD_NEWGAMEFILE             2611
#define IDC_RB_INTRO                    2612
#define IDC_RB_ADV                      2613
#define IDB_DEBUG_NEWGAME               2614
#define IDC_TXT_CONFIG_OLD              2615
#define IDC_TXT_CONFIG_NEW              2616
#define IDC_RB_CUSTOM                   2617
#define IDC_RB_PLAIN                    2618

#define DLG_DEBUG_EXTENSIONS            2620
#define IDC_FLD_EXTPATH                 2621

#define IDC_RB_TRADUI                   2622
#define IDC_RB_WEBUI                    2623

#define DLG_DEBUG_SYNTAX_COLORS         2670
#define IDC_CBO_EDITMODE                2671
#define IDC_LB_SYNTAXITEM               2672
#define IDC_CB_FGCOLOR                  2673
#define IDC_CB_BGCOLOR                  2674
#define IDC_CB_FONT                     2675
#define IDC_ST_SAMPLE                   2676
#define IDC_CB_EDITMODE                 2677
#define IDC_CK_ITALIC                   2678
#define IDC_CK_BOLD                     2679
#define IDC_CB_FONTSIZE                 2680
#define IDC_CK_USESYNTAXCOLORS          2681
#define IDC_BTN_KEYWORDS                2682
#define IDC_CB_SELFGCOLOR               2683
#define IDC_CB_SELBGCOLOR               2684
#define IDC_CK_OVERRIDESCISEL           2685

#define DLG_PROGARGS                    2700
#define IDC_FLD_CMDFILE                 2701
#define IDC_FLD_LOGFILE                 2702
#define IDC_FLD_PROGARGS                2703
#define IDC_BTN_SEL_CMDFILE             2704
#define IDC_BTN_SEL_LOGFILE             2705
#define IDC_FLD_CMDID                   2706
#define IDC_CBO_FILESAFETY              2707

#define DLG_DBG_EDITOR_AUTOCONFIG       2720
#define IDC_LB_EDITORS                  2721

#define DLG_GOTOLINE                    2740
#define IDC_EDIT_LINENUM                2741

#define DLG_DEBUG_WINMODE               2760
#define IDC_CB_STKDBG                   2761
#define IDC_CB_STKDSN                   2762
#define IDC_CB_EXPDBG                   2763
#define IDC_CB_EXPDSN                   2764
#define IDC_CB_LCLDBG                   2765
#define IDC_CB_LCLDSN                   2766
#define IDC_CB_PRJDBG                   2767
#define IDC_CB_PRJDSN                   2768
#define IDC_CB_LOGDBG                   2769
#define IDC_CB_LOGDSN                   2770
#define IDC_CB_SCHDBG                   2771
#define IDC_CB_SCHDSN                   2772
#define IDC_CB_TRCDBG                   2773
#define IDC_CB_TRCDSN                   2774
#define IDC_CB_SCRDBG                   2775
#define IDC_CB_SCRDSN                   2776

#define DLG_GAME_CHEST_URL              2800
#define IDC_FLD_KEY                     2801
#define IDC_FLD_NAME                    2802
#define IDC_FLD_DESC                    2803
#define IDC_FLD_BYLINE                  2804
#define IDC_CB_DESC_HTML                2805
#define IDC_CB_BYLINE_HTML              2806

#define DLG_GAME_CHEST_GAME             2820
#define IDC_POP_GROUP                   2821

#define DLG_DOWNLOAD                    2830
#define IDC_TXT_URL                     2831
#define IDC_TXT_FILE                    2832
#define IDC_TXT_BYTES                   2833

#define DLG_GAME_CHEST_GROUPS           2840
#define IDC_TREE_GROUPS                 2841
#define IDC_BTN_DELETE                  2842
#define IDC_BTN_RENAME                  2843
#define IDC_BTN_NEW                     2844
#define IDC_BTN_CLOSE                   2845
#define IDC_BTN_MOVE_UP                 2846
#define IDC_BTN_MOVE_DOWN               2847

#define DLG_GAME_CHEST_SEARCH           2860
#define IDC_FLD_FOLDER                  2861
#define IDC_LB_RESULTS                  2862
#define IDC_MSG_SEARCH_DONE             2863

#define DLG_MISSING_SOURCE              2870
#define IDC_BTN_FIND                    2871

#define DLG_ABS_SOURCE                  2880
#define IDC_POP_PARENT                  2881

#define DLG_GAME_CHEST_SEARCH_ROOT      2900
#define IDC_TXT_FOLDER                  2901

#define DLG_GAME_CHEST_SEARCH_RESULTS   2920
#define IDC_TV_GAMES                    2921

#define DLG_GAME_CHEST_SEARCH_GROUP     2940

#define DLG_GAME_CHEST_SEARCH_BUSY      2960

#define DLG_SCAN_INCLUDES               2980

#define DLG_BUILD_INTERMEDIATE          3000
#define IDC_FLD_SYMDIR                  3001
#define IDC_FLD_OBJDIR                  3002

#define DLG_NEWWIZ_BIBLIO               3100
#define IDC_BIB_TITLE                   3101
#define IDC_BIB_AUTHOR                  3102
#define IDC_BIB_EMAIL                   3103
#define IDC_BIB_DESC                    3104

#define DLG_ASK_SAVE                    3120
#define DLG_ASK_SAVE_MULTI              3121
#define DLG_ASK_SAVE_MULTI2             3122
#define IDS_PROMPT                      3123
#define IDYESALL                        3124
#define IDNOALL                         3125

#define DLG_DEBUG_KEYBOARD              3200
#define IDC_BTN_SAVEKEYS                3201
#define IDC_BTN_LOADKEYS                3202
#define IDC_FLD_PICKCMD                 3203
#define IDC_LB_COMMANDS                 3204
#define IDC_CB_OLDKEYS                  3205
#define IDC_FLD_NEWKEYS                 3206
#define IDC_CB_OLDCMD                   3207
#define IDC_BTN_ASSIGN                  3208
#define IDC_CB_KEYCTX                   3209
#define IDC_BTN_BKSP                    3210

#define DLG_LICENSE                     3240
#define IDX_LICENSE_TEXT                101

#define DLG_NEWFILETYPE                 3250
#define IDC_LV_TYPE                     3251

#define DLG_DEBUG_EXTERNTOOLS           3300
#define IDC_LB_TOOLS                    3301
#define IDC_FLD_TITLE                   3302
#define IDC_FLD_DIR                     3303
#define IDC_CK_CAPTURE                  3304
#define IDC_BTN_ARGSMENU                3305
#define IDC_BTN_DIRMENU                 3306

#define DLG_BUILD_ZIP                   3320
#define IDC_FLD_ZIP                     3321

#define DLG_BUILD_WEBPAGE               3330
#define IDC_CK_INCLUDE_SETUP            3331
#define IDC_CK_INCLUDE_SRC              3332
#define IDC_RB_GAME                     3333
#define IDC_RB_ZIP                      3334

#define DLG_BUILD_SRCZIP                3340

#define DLG_STARTMENUTITLE              3350
#define DLG_WEBTITLE                    3351
#define IDC_RB_NOSTARTMENU              3352
#define IDC_RB_STARTMENU                3353
#define IDC_FLD_STARTMENU               3354

#define DLG_BUILD_SCRIPTS               3360
#define IDC_CK_AUTOSCRIPT               3361
#define IDC_FLD_SCRIPTFOLDER            3362
#define IDC_CK_PRUNESCRIPTS             3363
#define IDC_FLD_MAXSCRIPTS              3364
#define IDC_FLD_TRIMQUIT                3365

#define DLG_DEBUG_AUTOBUILD             3370
#define IDC_CK_SAVE_ON_BUILD            3371
#define IDC_CK_ASK_AUTOADDSRC           3372
#define IDC_CK_AUTOBUILD                3373

#define DLG_BUILD_SPECIAL_FILES         3380
#define IDC_FLD_COVER_ART               3381
#define IDC_FLD_ICON                    3382

#define DLG_DEBUG_WRAP                  3390
#define IDC_CK_AUTO_WRAP                3391
#define IDC_CK_MARGIN_GUIDE             3392
#define IDC_FLD_MARGIN                  3393

#define DLG_DEBUG_USER_KW               3400
#define IDC_ST_KW                       3401
#define IDC_FLD_KW                      3402

#define IDHTML_PUB_WIZ                  7000


#define ID_FILE_LOADGAME                40002
///cmd File.OpenProject Open a project (closes the current project)
#define ID_FILE_OPEN                    40008
///cmd File.Open Open a file
#define ID_FILE_OPEN_NONE               40009
#define ID_WINDOW_NONE                  40010
#define ID_FILE_EDIT_TEXT               40011
///cmd File.SendToExternalEditor Open the current file in the external
/// text editor application
#define ID_FILE_PRINT                   40012
///cmd File.Print Print the current document window
#define ID_EDIT_SELECTLINE              40013
#define ID_FILE_PAGE_SETUP              40014
///cmd File.PageSetup Select a printer and set up the page layout for printing
#define ID_VIEW_FONTS_SMALLEST          40015
#define ID_VIEW_FONTS_SMALLER           40016
#define ID_VIEW_FONTS_MEDIUM            40017
#define ID_VIEW_FONTS_LARGER            40018
#define ID_VIEW_FONTS_LARGEST           40019
#define ID_VIEW_OPTIONS                 40020
#define ID_VIEW_SOUNDS                  40021
#define ID_VIEW_MUSIC                   40022
#define ID_FILE_QUIT                    40023
///cmd File.Exit Exit the application
#define ID_FILE_SAVEGAME                40024
#define ID_FILE_RESTOREGAME             40025
#define ID_FILE_RESTARTGAME             40026
///cmd File.RestartProgram Terminate the running program and start it again
#define ID_VIEW_LINKS                   40027
#define ID_VIEW_LINKS_HIDE              40028
#define ID_VIEW_LINKS_CTRL              40029
#define ID_VIEW_LINKS_TOGGLE            40030
#define ID_VIEW_GRAPHICS                40031
#define ID_FILE_VIEW_SCRIPT             40032
#define ID_FILE_RELOADGAME              40033
///cmd File.ReloadProject Close the current project and reload it
#define ID_FILE_TERMINATEGAME           40034
///cmd File.StopProgram Terminate the running program
#define ID_FILE_CLOSE                   40035
///cmd File.Close Close the current window
#define ID_WINDOW_CLOSE_ALL             40036
///cmd Window.CloseAllDocuments Close all document windows
#define ID_WINDOW_NEXT                  40037
///cmd Window.Next Switch to the next window
#define ID_WINDOW_PREV                  40038
///cmd Window.Prev Switch to the previous window
#define ID_WINDOW_CASCADE               40039
///cmd Window.Cascade Reposition all open windows in a staggered formation
#define ID_WINDOW_TILE_HORIZ            40040
///cmd Window.TileHoriz Reposition all open windows in a horizontal tile pattern
#define ID_WINDOW_TILE_VERT             40041
///cmd Window.TileVert Reposition all open windows in a vertical tile pattern
#define ID_FILE_CREATEGAME              40042
///cmd File.NewProject Run the New Project Wizard to create a new project
#define ID_FLUSH_GAMEWIN                40044
///cmd View.FlushGameWin Flush any buffered output in the main game window
#define ID_FLUSH_GAMEWIN_AUTO           40045
///cmd View.AutoFlushGameWin Automatically flush buffered output in the main
/// game window every time the debugger gets control
#define ID_VIEW_FONTS_NEXT_SMALLER      40046
#define ID_VIEW_FONTS_NEXT_LARGER       40047
#define ID_CMD_UNDO                     40048
#define ID_VIEW_TOOLBAR                 40049
#define ID_EDIT_FIND                    20027
///cmd Edit.Find Search for text in the current source file
#define ID_EDIT_FINDNEXT                20028
///cmd Edit.FindNext Search for the next occurrence of the last search text
#define ID_EDIT_GOTOLINE                20029
///cmd Edit.GoToLine Go to a given line number in the current window
#define ID_SEARCH_DOC                   20030
///cmd Help.SearchDoc Search the documentation for keywords
#define ID_SEARCH_PROJECT               20031
#define ID_SEARCH_FILE                  20032
#define ID_SEARCH_GO                    20033
#define ID_MDITB_LEFT                   20034
#define ID_MDITB_RIGHT                  20035
#define ID_GO_PREVIOUS                  40050
#define ID_GO_NEXT                      40051
#define ID_VIEW_TIMER                   40052
#define ID_VIEW_TIMER_HHMMSS            40053
#define ID_VIEW_TIMER_HHMM              40054
#define ID_TIMER_PAUSE                  40055
#define ID_TIMER_RESET                  40056
#define ID_GO_TO_GAME_CHEST             40057
#define ID_FILE_EXIT                    40058
#define ID_MUTE_SOUND                   40059
#define ID_FILE_HIDE                    40061
#define ID_WINDOW_RESTORE               40063
///cmd Window.Restore Restore the current maximized window to its original size
#define ID_WINDOW_MINIMIZE              40064
///cmd Window.Minimize Minimize the window
#define ID_WINDOW_MAXIMIZE              40065
///cmd Window.Maximize Maximize the window
#define ID_SHOW_DEBUG_WIN               40070
///cmd View.ShowDebugWin Show the Debug Log tool window
#define ID_FILE_SAVE                    40071
///cmd File.Save Save the current file
#define ID_FILE_SAVE_ALL                40072
///cmd File.SaveAll Save all modified files
#define ID_FILE_SAVE_AS                 40073
///cmd File.SaveAs Save the current file under a new name
#define ID_FILE_NEW                     40074
///cmd File.New Create a new file
#define ID_DEBUG_BUILDGO                40099
#define ID_DEBUG_GO                     40100
///cmd Debug.Go Start or continue execution of the program
#define ID_DEBUG_STEPINTO               40101
///cmd Debug.StepInto Step through one line of code, entering any function
/// or method called
#define ID_DEBUG_STEPOVER               40102
///cmd Debug.StepOver Step through one line of code, but run any function
/// or methods called within the line to completion
#define ID_DEBUG_RUNTOCURSOR            40103
///cmd Debug.RunToCursor Start or continue execution, stopping upon reaching
/// the current cursor location
#define ID_DEBUG_EVALUATE               40104
///cmd Debug.Evaluate Compute and display the current value of an expression
#define ID_DEBUG_EDITBREAKPOINTS        40105
///cmd Debug.EditBreakpoints Display the Breakpoints dialog
#define ID_DEBUG_STEPOUT                40106
///cmd Debug.StepOut Continue execution until the current function or method
/// returns to its caller
#define ID_DEBUG_SHOWNEXTSTATEMENT      40108
///cmd Debug.ShowNextStatement Display the next source code line that will
/// be executed when execution resumes
#define ID_DEBUG_SETCLEARBREAKPOINT     40109
///cmd Debug.SetClearBreakpoint Toggle the breakpoint at the current source
/// code line
#define ID_DEBUG_DISABLEBREAKPOINT      40111
///cmd Debug.DisableBreakpoint Disable the breakpoint (if any) at the
/// current source code line
#define ID_DEBUG_SETNEXTSTATEMENT       40112
///cmd Debug.SetNextStatement Move the execution point to the current source
/// code line
#define ID_DEBUG_BREAK                  40113
///cmd Debug.Break Break into the program's execution, interrupting any
/// command-line editing under way
#define ID_DEBUG_SHOWHIDDENOUTPUT       40114
///cmd Debug.ToggleHiddenOutput Toggle the show/hide status for hidden
/// command output
///tads2
#define ID_DEBUG_HISTORYLOG             40115
///cmd Debug.ToggleCallTrace Start/stop collecting a log of all method and
/// function calls
///tads2
#define ID_DEBUG_CLEARHISTORYLOG        40116
///cmd Debug.ClearCallTrace Clear the call trace log
///tads2
#define ID_DEBUG_OPTIONS                40117
///cmd Tools.Options Display the Options dialog to customize the Workbench
/// environment and text editor
#define ID_DEBUG_ABORTCMD               40119
///cmd Debug.AbortCommand Send an "abort" signal to the running program
/// to terminate processing of the current game command
#define ID_DEBUG_ADDTOWATCH             40120
///cmd Debug.AddWatch Add the current text selection to the expression window
#define ID_BUILD_COMPDBG                40121
///cmd Build.CompileForDebug Compile the project in "debug" mode, for
/// execution in the integrated debugger
#define ID_BUILD_COMPRLS                40122
///cmd Build.CompileForRelease Compile the project in "release" mode,
/// for distribution to users
#define ID_BUILD_COMPEXE                40123
///cmd Build.CompileExe Compile the project into a stand-alone executable
/// that Windows users can run without installing the TADS Interpreter
#define ID_BUILD_SETTINGS               40124
///cmd Build.Settings Display the Build Settings dialog to make changes to
/// the project's build configuration
#define ID_BUILD_COMPINST               40125
///cmd Build.CompileInstaller Compile the project and package it into a
/// stand-alone SETUP program for single-file distribution to Windows users
#define ID_DEBUG_SHOWERRORLINE          40126
///cmd Build.GoToError Display the source code line referenced by the
/// error message at the current location in the Debug Log tool window
#define ID_BUILD_COMP_AND_RUN           40127
///cmd Build.CompileAndRun Compile the project in "debug" mode, and if
/// successful, begin execution as soon as the build completes
#define ID_FILE_RECENT_1                40128
#define ID_FILE_RECENT_2                40129
#define ID_FILE_RECENT_3                40130
#define ID_FILE_RECENT_4                40131
#define ID_FILE_RECENT_NONE             40132
#define ID_GAME_RECENT_1                40133
#define ID_GAME_RECENT_2                40134
#define ID_GAME_RECENT_3                40135
#define ID_GAME_RECENT_4                40136
#define ID_GAME_RECENT_NONE             40137
#define ID_BUILD_COMPDBG_FULL           40138
///cmd Build.CompileForDebugFull Compile the project in "debug" mode,
/// recompiling all files, whether changed or not since the last build
///tads3
#define ID_DEBUG_PROGARGS               40139
///cmd Debug.SetProgramArgs Display the Program Arguments dialog to enter
/// command-line arguments to pass to the program on each run
///tads3
#define ID_BUILD_CLEAN                  40140
///cmd Build.Clean Delete all of the project's "debug" compiler-output
/// files - object files, symbol files, and the executable file
///tads3
#define ID_PROJ_SCAN_INCLUDES           40141
///cmd Project.ScanIncludes Rebuild the project's list of header files
/// by scanning the source files for #include directives
///tads3
#define ID_BUILD_COMPDBG_AND_SCAN       40142
#define ID_THEMES_DROPDOWN              40143
#define ID_BUILD_STOP                   40144
///cmd Build.Abort Interrupt the current build process (this should be used
/// only if the build appears to be stuck)
///tads3
#define ID_VIEW_DEBUG_TOOLBAR           40145
///cmd View.DebugToolbar Show/Hide the Debug toolbar
#define ID_VIEW_SEARCH_TOOLBAR          40146
///cmd View.SearchToolbar Show/Hide the Search toolbar
#define ID_VIEW_MENU_TOOLBAR            40147
///cmd View.Menu Show/Hide the main menu
#define ID_VIEW_EDIT_TOOLBAR            40148
///cmd View.EditToolbar Show/Hide the Text Editing toolbar
#define ID_HELP_GOBACK                  40149
///cmd Help.GoBack Go back to the previous page in the help window
#define ID_HELP_GOFORWARD               40150
///cmd Help.GoForward Navigate forward to the next page in the help window
#define ID_HELP_REFRESH                 40151
///cmd Help.Refresh Reload the page displayed in the help window
#define ID_VIEW_DOC_TOOLBAR             40152
///cmd View.DocToolbar Show/Hide the Documentation toolbar
#define ID_VIEW_STACK                   40200
///cmd View.StackWindow Show the Stack tool window
#define ID_VIEW_TRACE                   40201
///cmd View.CallTraceWindow Show the Trace Log tool window
///tads2
#define ID_VIEW_GAMEWIN                 40202
///cmd View.GameWindow Show the main game window, and bring to the front
#define ID_VIEW_WATCH                   40203
///cmd View.ExprWindow Show the Expression tool window
#define ID_VIEW_LOCALS                  40204
///cmd View.LocalsWindow Show the Local Variables tool window
#define ID_VIEW_PROJECT                 40205
///cmd View.ProjectWindow Show the Project window
///tads3
#define ID_VIEW_STATUSLINE              40206
///cmd View.Statusline Show/Hide the status line
#define ID_APPEARANCE_OPTIONS           40207
#define ID_MANAGE_PROFILES              40208
#define ID_SET_DEF_PROFILE              40209
#define ID_VIEW_DOCSEARCH               40210
///cmd View.DocSearch Show the Documentation Search Results tool window
#define ID_VIEW_FILESEARCH              40211
///cmd View.FileSearch Show the Project File Search Results tool window
#define ID_EDIT_CLEARFILESEARCH         40212
#define ID_EDIT_AUTOCLEARFILESEARCH     40213
#define ID_HELP_TUTORIAL                40214
///cmd Help.Tutorial Open the on-line Tutorial manual
///tads3
#define ID_VIEW_HELPWIN                 40215
///cmd View.Help Show the Help/Documentation Viewer tool window
#define ID_HELP_TOPICS                  40250
///cmd Help.Topics Open the on-line Help and go to the main topic list
#define ID_HELP_ABOUT                   40251
///cmd Help.About Show TADS Workbench version and copyright information
#define ID_HELP_ABOUT_GAME              40252
#define ID_HELP_COMMAND                 40253
#define ID_HELP_TADSMAN                 40254
///cmd Help.Manuals Open the on-line User's Manual
#define ID_HELP_TADSPRSMAN              40255
///cmd Help.ParserManual Open the on-line Parser Manual
///tads2
#define ID_HELP_T3LIB                   40256
///cmd Help.LibraryManual Open the on-line Library Manual
///tads3
#define ID_HELP_CONTENTS                40257
#define ID_HELP_SEARCH_DOC              40258
///cmd Help.EnterDocSearch Move keyboard focus to the Search toolbar to
/// enter keywords for a documentation search
#define ID_EDIT_QUOTEKEY                40259
///cmd Edit.QuoteKey Treat the next keystroke as a literal character to
/// insert into the editor, not as a command key
#define ID_EDIT_REPEATCOUNT             40260
///cmd Edit.RepeatCount Enter a number from the keyboard as a repeat count
/// for the next editing command
#define ID_EDIT_PASTEPOP                40261
///cmd Edit.PastePop Replace the last Paste or PastePop text with the
/// next most recent cut or copied text
#define ID_EDIT_SWAP_MARK               40262
///cmd Edit.SwapMark Swap the current position with the "mark" position
#define ID_EDIT_SELECT_TO_MARK          40263
///cmd Edit.SelectToMark Select the text between the current position and
/// the "mark" position
#define ID_EDIT_FILL_PARA               40264
///cmd Edit.FillParagraph Word-wrap the current paragraph or all of the
/// paragraphs in the selection range
#define ID_EDIT_SHOWLINENUMBERS         40265
///cmd Edit.ShowLineNumbers Show line numbers in text editing windows
#define ID_EDIT_SHOWFOLDING             40266
///cmd Edit.ShowFolding Show the code-folding controls in text editing windows
#define ID_EDIT_TOGGLEBOOKMARK          40267
///cmd Edit.ToggleBookmark Set or clear a bookmark at the current line
#define ID_EDIT_SETNAMEDBOOKMARK        40268
///cmd Edit.SetNamedBookmark Set a named bookmark at the current line
#define ID_EDIT_POPBOOKMARK             40269
///cmd Edit.PopBookmark Go to the most recently set bookmark, and pop it
/// off the recent bookmark stack
#define ID_EDIT_JUMPNEXTBOOKMARK        40270
///cmd Edit.JumpToNextBookmark Go to the next bookmark in the current file,
/// or to the first bookmark in the next project file
#define ID_EDIT_JUMPNAMEDBOOKMARK       40271
///cmd Edit.JumpToNamedBookmark Go to a named bookmark
#define ID_EDIT_ADDCOMMENT              40272
///cmd Edit.CommentRegion Add or remove comment markers on each line in the
/// selected region
#define ID_EDIT_FINDSYMDEF              40273
///cmd Edit.FindDefinition Search the project for the definition of the
/// symbol at the cursor
#define ID_EDIT_JUMPPREVBOOKMARK        40274
///cmd Edit.JumpToPreviousBookmark Go to the previous bookmark in the
/// current file, or to the last bookmark in the prior project file
#define ID_EDIT_CLEARALLBOOKMARKS       40275
///cmd Edit.ClearAllBookmarks Remove all bookmarks throughout the project
#define ID_EDIT_CLEARFILEBOOKMARKS      40276
///cmd Edit.ClearFileBookmarks Remove all bookmarks in the current file

#define ID_DBGEXPR_NEW                  40300
#define ID_DBGEXPR_EDITEXPR             40301
#define ID_DBGEXPR_EDITVAL              40302
#define ID_PROJ_OPEN                    40320
///cmd Project.OpenSelection Open the file selected in the Project window
///tads3
#define ID_PROJ_REMOVE                  40321
///cmd Project.RemoveSelection Remove the file selected in the
/// Project window from the project
///tads3
#define ID_PROJ_ADDFILE                 40322
///cmd Project.AddFile Browse for a file or files to add to the project
///tads3
#define ID_PROJ_ADDEXTRES               40323
///cmd Project.AddExternalResource Add an external resource file to the project
///tads3
#define ID_PROJ_ADDDIR                  40324
///cmd Project.AddFolder Add all files in a specified folder to the
/// project in the selected section
///tads3
#define ID_PROJ_INCLUDE_IN_INSTALL      40325
///cmd Project.IncludeInInstall If an external resource file is selected
/// in the Project window, toggle its "included in install" status
///tads3
#define ID_PROJ_SEARCH                  40326
///cmd Project.SearchFiles Search for text in the project's source files
///tads3
#define ID_TOOLS_NEWIFID                40327
///cmd Tools.NewIFID Create an IFID (an Interative Fiction IDentifier,
/// for use in GameInfo metadata)
#define ID_TOOLS_READIFID               40328
///cmd Tools.ReadIFID Read the IFID from a previously released TADS game
#define ID_EDIT_SELECTIONMODE           40329
///cmd Edit.SelectionMode Set selection mode: sets "mark" to here;
/// subsequent ordinary cursor movement defines a selection
#define ID_EDIT_RECTSELECTMODE          40330
///cmd Edit.RectSelectMode Set rectangle selection mode: sets "mark" to
/// here; subsequent cursor movement defines a rectangular selection
#define ID_EDIT_SCROLLLEFT              40331
///cmd Edit.ScrollLeft Scroll one column left
#define ID_EDIT_SCROLLRIGHT             40332
///cmd Edit.ScrollRight Scroll one column right
#define ID_EDIT_PAGELEFT                40333
///cmd Edit.PageLeft Scroll one window-width left
#define ID_EDIT_PAGERIGHT               40334
///cmd Edit.PageRight Scroll one window-width right
#define ID_EDIT_LINESELECTMODE          40335
///cmd Edit.LineSelectMode Set line selection mode: sets "mark" to here;
/// subsequent ordinary cursor movement selects whole lines
#define ID_EDIT_OPENLINE                40336
///cmd Edit.OpenLine Opens a new line at the cursor
#define ID_EDIT_SPLITWINDOW             40337
///cmd Edit.SplitWindow Split the current window 50/50 horizontally
#define ID_EDIT_UNSPLITWINDOW           40338
///cmd Edit.UnsplitWindow Un-split the window to show as a single pane
#define ID_EDIT_UNSPLITSWITCHWINDOW     40339
///cmd Edit.UnsplitSwitchWindow Switch to the other split pane and un-split
/// the window
#define ID_EDIT_SWITCHSPLITWINDOW       40340
///cmd Edit.SwitchSplitWindow Move focus to the other pane of a split window
#define ID_EDIT_VCENTERCARET            40341
///cmd Edit.VCenterCaret Scroll the window to center the caret vertically
#define ID_EDIT_CHARTRANSPOSE           40342
///cmd Edit.CharTranspose Transpose the characters before and after the caret
#define ID_EDIT_WINHOME                 40343
///cmd Edit.WindowHome Move the caret to the top of the window
#define ID_EDIT_WINEND                  40344
///cmd Edit.WindowEnd Move the caret to the bottom of the window
#define ID_PROJ_ADDCURFILE              40345
///cmd Project.AddActiveFile Add the file in the active text editor window
/// to the project
///tads3
#define ID_EDIT_WRAPNONE                40346
///cmd Edit.WrapNone Turn off word-wrap and character-wrap mode for the
/// active window
#define ID_EDIT_WRAPCHAR                40347
///cmd Edit.WrapChar Wrap long lines in the active window at any character
/// boundary
#define ID_EDIT_WRAPWORD                40348
///cmd Edit.WrapWord Wrap long lines in the active window at word boundaries
#define ID_EDIT_REPLACE                 40349
///cmd Edit.Replace Search the active window for text and replace each
/// occurrence
#define ID_PROJ_SEARCHFILESRPL          40350
///cmd Project.SearchFilesReplace Search all project files for text and
/// replace each occurrence
#define ID_EDIT_CHANGEREADONLY          40351
///cmd Edit.ChangeReadOnly Set or clear read-only mode for the active window
#define ID_EDIT_CHANGEMODIFIED          40352
///cmd Edit.ChangeModified Set or clear the "modified" status for the
/// active window
#define ID_EDIT_CUTLINERIGHT            40353
///cmd Edit.CutLineRight Cut from the caret to the end of the line
#define ID_EDIT_INCSEARCH               40354
///cmd Edit.SearchIncremental Search incrementally for text entered
/// interactively
#define ID_EDIT_INCSEARCHREV            40355
///cmd Edit.SearchRevIncremental Incremental reverse search
#define ID_EDIT_INCSEARCHREGEX          40356
///cmd Edit.SearchRegexIncremental Incremental regular expression search
#define ID_EDIT_INCSEARCHREGEXREV       40357
///cmd Edit.SearchRevRegexIncremental Incremental reverse regular expression
/// search
#define ID_ISEARCH_FINDNEXT             40358
///cmd Edit.IncSearch.Next Find the next occurrence of the search text
#define ID_ISEARCH_FINDPREV             40359
///cmd Edit.IncSearch.Prev Find the previous occurrence of the search text
#define ID_ISEARCH_EXIT                 40360
///cmd Edit.IncSearch.Exit End the current incremental search, leaving
/// the caret at the current position
#define ID_ISEARCH_CANCEL               40361
///cmd Edit.IncSearch.Cancel Cancel the current incremental search, returning
/// to the position at the start of the search
#define ID_ISEARCH_BACKSPACE            40362
///cmd Edit.IncSearch.Backspace Delete the last character of the
/// search text
#define ID_ISEARCH_CASE                 40363
///cmd Edit.IncSearch.ToggleExactCase Toggle exact-case matching for the
/// current incremental search
#define ID_ISEARCH_REGEX                40364
///cmd Edit.IncSearch.ToggleRegEx Toggle regular-expression matching for
/// the current incremental search
#define ID_EDIT_DELETECHAR              40365
///cmd Edit.DeleteChar Delete the selection, or the character at the caret
/// if there's no selection
#define ID_TOOLS_EDITEXTERN             40366
///cmd Tools.EditExternal Edit the list of external commands displayed
/// on the Tools menu
#define ID_DOCKWIN_DOCK                 40367
///cmd Window.Dock Dock the active dockable window
#define ID_DOCKWIN_UNDOCK               40368
///cmd Window.Undock Undock the active docking window, making it a floating
/// tool window
#define ID_DOCKWIN_HIDE                 40369
///cmd Window.DockHide Hide the active docking window
#define ID_DOCKWIN_DOCKVIEW             40370
///cmd Window.DockingMode Make the current MDI tool window into a dockable
/// tool window, or vice versa
#define ID_PROFILER_START               40371
///cmd Profiler.Start Begin collecting "profiling" performance data, which
/// monitors the amount of time spent in each function and method called
/// while profiling is activated
///tads3
#define ID_PROFILER_STOP                40372
///cmd Profiler.Stop Stop collecting profiling data
///tads3
#define ID_BUILD_RELEASEZIP             40373
///cmd Build.ReleaseZIP Build a ZIP file for release, with the compiled
/// game and all "feelies"
///tads3
#define ID_BUILD_WEBPAGE                40374
///cmd Build.WebPage Generate a web page for distributing the game
///tads3
#define ID_BUILD_SRCZIP                 40375
///cmd Build.SourceZIP Build a ZIP file containing the complete source code
/// for the game, including the project file and all "feelies"
///tads3
#define ID_BUILD_ALLPACKAGES            40376
///cmd Build.AllPackages Build all packages (Release ZIP, SETUP, Web Page,
/// Source ZIP)
///tads3
#define ID_PROJ_SETSPECIAL              40377
///cmd Project.SetSpecial Select the file to use for the selected special
/// item in the Project list
///tads3
#define ID_PROJ_SETFILETITLE            40378
///cmd Project.SetFileTitle Set the title to display in the Start
/// Menu or Web Page for the selected file
///tads3
#define ID_VIEW_SCRIPT                  40379
///cmd View.ScriptWindow Show the Script tool window
///tads3
#define ID_SCRIPT_OPEN                  40380
#define ID_SCRIPT_REPLAY                40381
#define ID_SCRIPT_RENAME                40382
#define ID_SCRIPT_DELETE                40383
#define ID_SCRIPT_REPLAY_TO_CURSOR      40384
///cmd Script.ReplayToCursor Replay the current script up to (and including)
/// the line containing the cursor
///tads3
#define ID_DEBUG_REPLAY_SESSION         40385
///cmd Debug.ReplaySession Restart the game and replay the input script from
/// the current/last session
///tads3
#define ID_ISEARCH_WORD                 40386
///cmd Edit.IncSearch.ToggleWord Toggle whole-word matching for the current
/// incremental search
#define ID_BUILD_PUBLISH                40387
///cmd Build.Publish Publish the project to the Internet: upload to the
/// IF Archive and create a listing on IFDB.
///tads3
#define ID_HELP_WWWTADSORG              40388
//cmd Help.TadsWebSite Open the TADS Web site in a browser window

/*
 *   File-open subcommands for the line sources.  The line sources are
 *   numbered consecutively starting at zero; for line source number n, we
 *   generate a command ID of (ID_FILE_OPENLINESRC_FIRST + n).  We reserve
 *   space for 500 line sources, which is probably ten or twenty times
 *   more than even the largest game will actually use.
 */
#define ID_FILE_OPENLINESRC_FIRST       41000
/* --------------- taken: 41001 through 41498 */
#define ID_FILE_OPENLINESRC_LAST        41499

/*
 *   Same idea for preference profiles.  Since these are used only in the
 *   game window, and the line source ID's are used only in the debugger
 *   window, we can re-use the same command ID's - we'll always know which we
 *   mean by virtue of which window is receiving them.
 */
#define ID_PROFILE_FIRST                41000
#define ID_PROFILE_LAST                 41499

/*
 *   Same idea for items in the Window menu.  The windows are numbered
 *   starting at zero; for window n, we generate a command ID of
 *   (ID_WINDOW_FIRST + n).
 */
#define ID_WINDOW_FIRST                 41500
/* --------------- taken: 41500 through 41999 */
#define ID_WINDOW_LAST                  41999

/*
 *   Main menu toolbar commands.  These identify the toolbar buttons in the
 *   main menu toolbar.
 */
#define ID_MENUBAR_FIRST                42000
#define ID_MENUBAR_LAST                 42050

/*
 *   Scintilla Text Editor commands.
 *
 *   Workbench handles all accelerators internally, so we don't use the
 *   Scintilla key-binding mechanism at all - when we start up a Scintilla
 *   window, we clear its key bindings, and handle all accelerator mappings
 *   from within the Workbench message loop.  This creates a small impedance-
 *   matching problem, though: Workbench and Scintilla each have their own
 *   independent systems of WM_COMMAND codes.  Workbench has to translate key
 *   accelerators into the Workbench WM_COMMAND numbering system, but
 *   Scintilla expects its own SCI_xxx command codes.
 *
 *   Here's how we deal with this: the Workbench message loop translates
 *   accelerator keys into Workbench ID_xxx command codes and generates
 *   WM_COMMAND messages with these codes.  It routes the WM_COMMAND messages
 *   to the active window using the usual CTadsWin routing scheme.  This will
 *   eventually reach our container window, which is the parent of the
 *   Scintilla control.  This is where the translation takes place: when the
 *   container window sees an ID_xxx command code in the ID_SCI_xxx range, it
 *   translates this to the corresponding SCI_xxx code and forwards it to its
 *   Scintilla child window.
 *
 *   To make it easy and efficient for our container window to identify
 *   ID_SCI_xxx codes, we simply dedicate a contiguous numerical range to
 *   those codes.  We can tell that an ID_xxx code is actually an ID_SCI_xxx
 *   code by checking to see if it's within this numerical range.
 *
 *   The relationship between ID_SCI_xxx and the corresponding SCI_xxx is
 *   arbitrary.  Once we know that we have an ID_SCI_xxx code, we simply
 *   subtract the base of the range from the code to get an array index, and
 *   we use that index to look up the SCI_xxx code in a mapping array that
 *   gives the SCI_xxx code for an ID_SCI_xxx value.  This way, we're
 *   insulated from any changes to Scintilla's numbering system, and we also
 *   make it easier to substitute a different editor control at some point.
 *   (Not that either of these eventualities seems likely.  Scintilla's
 *   numbering system really can't be changed, since other third-party
 *   containers rely upon the published SCI_xxx codes.  And Scintilla is very
 *   good; it seems unlikely that another free editor will come along that's
 *   much better.)
 */

/*
 *   The *dedicated* range of ID_SCI_xxx codes.  This is the range of codes
 *   reserved for ID_SCI_xxx codes, although we don't use all of them in the
 *   current implementation.  We dedicate a considerably larger range than we
 *   actually currently need, to allow for future expansion.
 */
#define ID_SCI_FIRST                    43000
#define ID_SCI_RANGE_LAST               44999

/*
 *   The Workbench commands corresponding to Scintilla commands.  Our
 *   numbering is arbitrary; it doesn't have to match the actual SCI_xxx
 *   command numbering in any way, not even in the order of the codes.  We
 *   simply use the ID_SCI_xxx code as an index into a lookup array giving
 *   the corresponding SCI_xxx values.  The only requirements for these code
 *   are (a) that they be within the ID_SCI_FIRST to ID_SCI_LAST range, and
 *   (b) they be contiguous, or at least nearly contiguous, so that we don't
 *   waste space in the mapping array with lots of unused entries.
 */
#define ID_SCI_LINEDOWN                 43000
///cmd Edit.LineDown Move the cursor to the next line
#define ID_SCI_LINEDOWNEXTEND           43001
///cmd Edit.LineDownExtend Move the cursor to the next line, extending
/// the selection
#define ID_SCI_LINEDOWNRECTEXTEND       43002
///cmd Edit.LineDownRectExtend Move the cursor to the next line, extending
/// the rectangular selection
#define ID_SCI_LINESCROLLDOWN           43003
///cmd Edit.LineScrollDown Scroll the window down one line
#define ID_SCI_LINEUP                   43004
///cmd Edit.LineUp Move the cursor to the previous line
#define ID_SCI_LINEUPEXTEND             43005
///cmd Edit.LineUpExtend Move the cursor to the previous line, extending
/// the selection
#define ID_SCI_LINEUPRECTEXTEND         43006
///cmd Edit.LineUpRectExtend Move the cursor to the previous line, extending
/// the rectangular selection
#define ID_SCI_LINESCROLLUP             43007
///cmd Edit.LineScrollUp Scroll the window up one line
#define ID_SCI_PARADOWN                 43008
///cmd Edit.ParaDown Move the curor to the start of the next paragraph
#define ID_SCI_PARADOWNEXTEND           43009
///cmd Edit.ParaDownExtend Move the curor to the start of the next paragraph,
/// extending the selection
#define ID_SCI_PARAUP                   43010
///cmd Edit.ParaUp Move the cursor up by one paragraph
#define ID_SCI_PARAUPEXTEND             43011
///cmd Edit.ParaUpExtend Move the cursor up by one paragraph, extending
/// the selection
#define ID_SCI_CHARLEFT                 43012
///cmd Edit.CharLeft Move the cursor left one character
#define ID_SCI_CHARLEFTEXTEND           43013
///cmd Edit.CharLeftExtend Move the cursor left one character, extending
/// the selection
#define ID_SCI_CHARLEFTRECTEXTEND       43014
///cmd Edit.CharLeftRectExtend Move the cursor left one character, extending
/// the rectangular selection
#define ID_SCI_CHARRIGHT                43015
///cmd Edit.CharRight Move the cursor right one character
#define ID_SCI_CHARRIGHTEXTEND          43016
///cmd Edit.CharRightExtend Move the cursor right one character, extending
/// the selection
#define ID_SCI_CHARRIGHTRECTEXTEND      43017
///cmd Edit.CharRightRectExtend Move the cursor right one character, extending
/// the rectangular selection
#define ID_SCI_WORDLEFT                 43018
///cmd Edit.WordLeft Move the cursor left by one word
#define ID_SCI_WORDLEFTEXTEND           43019
///cmd Edit.WordLeftExtend Move the cursor left by one word, extending
/// the selection
#define ID_SCI_WORDRIGHT                43020
///cmd Edit.WordRight Move the cursor right by one word
#define ID_SCI_WORDRIGHTEXTEND          43021
///cmd Edit.WordRightExtend Move the cursor right by one word, extending
/// the selection
#define ID_SCI_WORDLEFTEND              43022
///cmd Edit.WordLeftEnd Move the cursor to the end of the previous word
#define ID_SCI_WORDLEFTENDEXTEND        43023
///cmd Edit.WordLeftEndExtend Move the cursor to the end of the previous
/// word, extending the selection
#define ID_SCI_WORDRIGHTEND             43024
///cmd Edit.WordRightEnd Move the cursor to the end of the next word
#define ID_SCI_WORDRIGHTENDEXTEND       43025
///cmd Edit.WordRightEndExtend Move the cursor to the end of the next word,
/// extending the selection
#define ID_SCI_WORDPARTLEFT             43026
///cmd Edit.WordPartLeft Move the cursor left to the prior "word part"
/// (intercap, underscore, etc)
#define ID_SCI_WORDPARTLEFTEXTEND       43027
///cmd Edit.WordPartLeftExtend Move the cursor left to the prior "word part"
/// (intercap, underscore, etc), extending the selection
#define ID_SCI_WORDPARTRIGHT            43028
///cmd Edit.WordPartRight Move the cursor right to the next "word part"
/// (intercap, underscore, etc)
#define ID_SCI_WORDPARTRIGHTEXTEND      43029
///cmd Edit.WordPartRightExtend Move the cursor right to the next "word part"
/// (intercap, underscore, etc), extending the selection
#define ID_SCI_HOME                     43030
///cmd Edit.Home Move the cursor to the start of the line
#define ID_SCI_HOMEEXTEND               43031
///cmd Edit.HomeExtend Move the cursor to the start of the line, extending
/// the selection
#define ID_SCI_HOMERECTEXTEND           43032
///cmd Edit.HomeRectExtend Move the cursor to the start of the line,
/// extending the rectangular selection
#define ID_SCI_HOMEDISPLAY              43033
///cmd Edit.HomeDisplay Move the cursor to the start of the display line
#define ID_SCI_HOMEDISPLAYEXTEND        43034
///cmd Edit.HomeDisplayExtend Move the cursor to the start of the display
/// line, extending the selection
#define ID_SCI_HOMEWRAP                 43035
///cmd Edit.HomeWrap Move the cursor to the start of the wrapped line
#define ID_SCI_HOMEWRAPEXTEND           43036
///cmd Edit.HomeWrapExtend Move the cursor to the start of the wrapped line,
/// extending the selection
#define ID_SCI_VCHOME                   43037
///cmd Edit.IndentHome Move the cursor to the start of the current line's
/// indentation
#define ID_SCI_VCHOMEEXTEND             43038
///cmd Edit.IndentHomeExtend Move the cursor to the start of the current
/// line's indentation, extending the selection
#define ID_SCI_VCHOMERECTEXTEND         43039
///cmd Edit.IndentHomeRectExtend Move the cursor to the start of the current
/// line's indentation, extending the rectangular selection
#define ID_SCI_VCHOMEWRAP               43040
///cmd Edit.IndentHomeWrap Move the cursor to the start of the current
/// wrapped line's indentation
#define ID_SCI_VCHOMEWRAPEXTEND         43041
///cmd Edit.IndentHomeWrapExtend Move the cursor to the start of the current
/// wrapped line's indentation, extending the selection
#define ID_SCI_LINEEND                  43042
///cmd Edit.LineEnd Move the cursor to the end of the current line
#define ID_SCI_LINEENDEXTEND            43043
///cmd Edit.LineEndExtend Move the cursor to the end of the current line,
/// extending the selection
#define ID_SCI_LINEENDRECTEXTEND        43044
///cmd Edit.LineEndRectExtend Move the cursor to the end of the current
/// line, extending the rectangular selection
#define ID_SCI_LINEENDDISPLAY           43045
///cmd Edit.LineEndDisplay Move the cursor to the end of the current
/// display line
#define ID_SCI_LINEENDDISPLAYEXTEND     43046
///cmd Edit.LineEndDisplayExtend Move the cursor to the end of the current
/// display line, extending the selection
#define ID_SCI_LINEENDWRAP              43047
///cmd Edit.LineEndWrap Move the cursor to the end of the current
/// wrapped line
#define ID_SCI_LINEENDWRAPEXTEND        43048
///cmd Edit.LineEndWrapExtend Move the cursor to the end of the current
/// wrapped line, extending the selection
#define ID_SCI_DOCUMENTSTART            43049
///cmd Edit.DocumentStart Move the cursor to the start of the document
#define ID_SCI_DOCUMENTSTARTEXTEND      43050
///cmd Edit.DocumentStartExtend Move the cursor to the start of the document,
/// extending the selection
#define ID_SCI_DOCUMENTEND              43051
///cmd Edit.DocumentEnd Move the cursor to the end of the document
#define ID_SCI_DOCUMENTENDEXTEND        43052
///cmd Edit.DocumentEndExtend Move the cursor to the end of the document,
/// extending the selection
#define ID_SCI_PAGEUP                   43053
///cmd Edit.PageUp Move the cursor up one page
#define ID_SCI_PAGEUPEXTEND             43054
///cmd Edit.PageUpExtend Move the cursor up one page, extending
/// the selection
#define ID_SCI_PAGEUPRECTEXTEND         43055
///cmd Edit.PageUpRectExtend Move the cursor up one page, extending
/// the rectangular selection
#define ID_SCI_PAGEDOWN                 43056
///cmd Edit.PageDown Move the cursor down one page
#define ID_SCI_PAGEDOWNEXTEND           43057
///cmd Edit.PageDownExtend Move the cursor down one page, extending
/// the selection
#define ID_SCI_PAGEDOWNRECTEXTEND       43058
///cmd Edit.PageDownRectExtend Move the cursor down one page, extending
/// the rectangular selection
#define ID_SCI_STUTTEREDPAGEUP          43059
///cmd Edit.StutteredPageUp Move the cursor to the top of the current page,
/// or up one page if already at the top
#define ID_SCI_STUTTEREDPAGEUPEXTEND    43060
///cmd Edit.StutteredPageUpExtend Move the cursor to the top of the page
/// or up one page if already there, extending the selection
#define ID_SCI_STUTTEREDPAGEDOWN        43061
///cmd Edit.StutteredPageDown Move the cursor to the bottom of the current
/// page, or down a page if already at the bottom
#define ID_SCI_STUTTEREDPAGEDOWNEXTEND  43062
///cmd Edit.StutteredPageDownExtend Move the cursor to the bottom of the
/// page or down one page if already there, extending the selection
#define ID_SCI_DELETEBACK               43063
///cmd Edit.DeleteBack Delete the previous character
#define ID_SCI_DELETEBACKNOTLINE        43064
///cmd Edit.DeleteBackNotLine Delete the previous character, but do nothing
/// at the start of the line
#define ID_SCI_DELWORDLEFT              43065
///cmd Edit.DelWordLeft Delete the previous word
#define ID_SCI_DELWORDRIGHT             43066
///cmd Edit.DelWordRight Delete the word to the right of the cursor
#define ID_SCI_DELLINELEFT              43067
///cmd Edit.DelLineLeft Delete from the start of the line to the cursor
#define ID_SCI_DELLINERIGHT             43068
///cmd Edit.DelLineRight Delete from the cursor to the end of the line
#define ID_SCI_LINEDELETE               43069
///cmd Edit.LineDelete Delete the current line
#define ID_SCI_LINECUT                  43070
///cmd Edit.LineCut Cut the current line, placing it on the clipboard
#define ID_SCI_LINECOPY                 43071
///cmd Edit.LineCopy Copy the current line to the clipboard
#define ID_SCI_LINETRANSPOSE            43072
///cmd Edit.LineTranspose Transpose the current and previous lines
#define ID_SCI_LINEDUPLICATE            43073
///cmd Edit.LineDuplicate Insert a copy of the current line
#define ID_SCI_LOWERCASE                43074
///cmd Edit.Lowercase Convert the selection to lower-case
#define ID_SCI_UPPERCASE                43075
///cmd Edit.Uppercase Convert the selection to uppper-case
#define ID_SCI_CANCEL                   43076
///cmd Edit.Cancel Cancel any modes, and cancel the current selection
#define ID_SCI_EDITTOGGLEOVERTYPE       43077
///cmd Edit.ToggleOvertype Toggle between Insert and Overtype modes
#define ID_SCI_NEWLINE                  43078
///cmd Edit.Newline Insert a newline at the cursor
#define ID_SCI_FORMFEED                 43079
///cmd Edit.Formfeed Insert a page break ("form feed" character) at the cursor
#define ID_SCI_TAB                      43080
///cmd Edit.Tab Indent the current line by one position
#define ID_SCI_BACKTAB                  43081
///cmd Edit.BackTab Unindent the current line by one position
#define ID_SCI_SELECTIONDUPLICATE       43082
///cmd Edit.SelectionDuplicate Insert a copy of the selection
#define ID_EDIT_UNDO                    43083
#define ID_SCI_UNDO ID_EDIT_UNDO
///cmd Edit.Undo Undo the last editing action
#define ID_EDIT_CUT                     43084
#define ID_SCI_CUT ID_EDIT_CUT
///cmd Edit.Cut Cut the current text selection
#define ID_EDIT_COPY                    43085
#define ID_SCI_COPY ID_EDIT_COPY
///cmd Edit.Copy Copy the current text selection
#define ID_EDIT_PASTE                   43086
#define ID_SCI_PASTE ID_EDIT_PASTE
///cmd Edit.Paste Paste the contents of the clipboard
#define ID_EDIT_DELETE                  43087
#define ID_SCI_CLEAR ID_EDIT_DELETE
///cmd Edit.Delete Delete the current selection
#define ID_EDIT_REDO                    43088
#define ID_SCI_REDO ID_EDIT_REDO
///cmd Edit.Redo Redo the last editing action
#define ID_EDIT_SELECTALL               43089
#define ID_SCI_SELECTALL ID_EDIT_SELECTALL
///cmd Edit.SelectAll Select everything in the current window

/* the last *actual* ID_SCI_xxx currently in use */
#define ID_SCI_LAST                     43089


/*
 *   Language modes.  We allocate a range of command IDs for menu items that
 *   let the user select the language mode to use.  The number of modes is
 *   unpredictable because extension DLLs can add new modes on the fly, but
 *   in practice the number of modes that we're likely to find will be fairly
 *   small, since (a) it takes work to write a mode, and (b) there aren't
 *   that many file types that you'd want to edit with Workbench anyway.
 */
#define ID_EDIT_LANGMODE                45000
#define ID_EDIT_LANGLAST                45099

/*
 *   Custom extension commands.  We allocate a range of command IDs for use
 *   by extensions, via the features that let extensions add custom commands
 *   of their own.
 */
#define ID_CUSTOM_FIRST                 46000
#define ID_CUSTOM_LAST                  50999


#endif /* HTMLRES_H */

