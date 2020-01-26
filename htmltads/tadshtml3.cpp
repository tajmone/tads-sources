#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2011 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadshtml3.cpp - tadshtml.cpp extensions specific to TADS 3 builds
Function
  In debugging builds, HTML TADS defines its own global 'operator new'
  and 'operator delete', which do extra book-keeping that lets us detect
  leaks, double frees, and out-of-bound writes.  The complication is that
  TADS 3 has its own equivalents, and since the operator new/delete
  overloads are global, this creates a link conflict.  When linking
  with TADS 3, we want to use the TADS 3 versions of these instead of
  the HTML TADS versions, because as of 3.1 the TADS versions are
  thread-safe, and TADS 3.1 is multi-threaded.  For TADS 3 builds,
  then, we simply want to leave out the operator new/delete overloads.

  In addition, to keep all of the memory management overloads in one
  subsystem, we define aliases for the HTML TADS memory functions
  (th_malloc, th_free, etc) that simply call the TADS 3 equivalents.
Notes

Modified
  05/12/11 MJRoberts  - Creation
*/

#include <stdio.h>
#include <memory.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif

#ifdef TADSHTML_DEBUG

#include <stdlib.h>
#include <malloc.h>
#include <windows.h>
#include <t3std.h>

void *th_malloc(size_t siz) { return t3malloc(siz); }
void *th_realloc(void *ptr, size_t siz) { return t3realloc(ptr, siz); }
void th_free(void *ptr) { t3free(ptr); }
void th_list_memory_blocks() { t3_list_memory_blocks(0); }

#endif /* TADSHTML_DEBUG */

