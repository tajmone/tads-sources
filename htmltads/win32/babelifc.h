/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  babelifc.h - workbench interface to Babel library
Function

Notes

Modified
  11/05/06 MJRoberts  - Creation
*/

#ifndef BABELIFC_H
#define BABELIFC_H

#include <stdlib.h>

/* read an IFID */
int babel_read_ifid(const char *fname, char *buf, size_t buflen);

/* synthesize an iFiction record from a story file */
int babel_synth_ifiction(const char *fname, CStringBuf *buf);


#endif /* BABELIFC_H */
