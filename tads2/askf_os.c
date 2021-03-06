#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/*
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  askf_os.c - dialog-based implementation of tio_askfile
Function
  Implements tio_askfile() using the OS-layer os_askfile() routine.
Notes
  Only one of askf_tx.c or askf_os.c should be included in a given
  executable.  For a text-only version, include askf_tx.  For a version
  where os_askfile() provides a file dialog, use askf_os instead.

  We provide a choice of tio_askfile() implementations in the portable
  code (rather than only through the OS code) so that we can call
  the formatted text output routines in this version.  An OS-layer
  implementation could not call the formatted output routines (it would
  have to call os_printf directly), which would result in poor prompt
  formatting any time a prompt exceeded a single line of text.
Modified
  09/27/99 MJRoberts  - Creation
*/

#include "os.h"
#include "std.h"
#include "tio.h"

/*
 *   formatted text-only file prompt
 */
int tio_askfile(const char *prompt, char *reply, int replen,
                int prompt_type, os_filetype_t file_type)
{
    /* let the OS layer handle it */
    return os_askfile(prompt, reply, replen, prompt_type, file_type);
}

