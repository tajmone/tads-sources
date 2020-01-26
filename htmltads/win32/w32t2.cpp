#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/*
 *   Copyright (c) 2000 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  w32t2.cpp - TADS 2 version-specific implementation
Function

Notes

Modified
  01/20/00 MJRoberts  - Creation
*/


#include <Windows.h>

#include "w32main.h"
#include "tadsdlg.h"
#include "tadsapp.h"
#include "htmlres.h"


/* ------------------------------------------------------------------------ */
/*
 *   VM-specific application termination cleanup
 */
void w32_cleanup() { }

/* TADS 2 doesn't have a Web UI */
void w32_webui_yield_foreground() { }
void w32_webui_to_foreground() { }


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific post-load initialization - not used in t2-only builds
 */
void os_init_ui_after_load(class CVmBifTable *bif, class CVmMetaTable *)
{
}


/* ------------------------------------------------------------------------ */
/*
 *   list memory blocks in the TADS 2 subsystem
 */

#ifdef TADSHTML_DEBUG

void th_list_subsys_memory_blocks()
{
    /* there's no separate tads 2 memory subsystem lister */
}

#endif /* TADSHTML_DEBUG */

