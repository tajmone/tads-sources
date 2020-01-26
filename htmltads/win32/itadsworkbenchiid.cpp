/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  itadsworkbenchiid.c - TADS Workbench extension IID definer
Function
  This file defines statics for the IIDs for the TADS Workbench extension
  COM interfaces.  You can link this in to an executable or DLL that needs to
  reference the actual IID values.
Notes

Modified
  10/05/06 MJRoberts  - Creation
*/

#include <windows.h>
#include <ole2.h>
#include "itadsworkbench.h"

/* ITadsWorkbench IID */
static const GUID IID_ITadsWorkbench =
    { 0xd44bd5b0,0x160e,0x2cd6, { 0x83,0xd9,0x04,0x6c,0x80,0xe7,0x8a,0x4e } };

/* ITadsWorkbenchExtension IID */
static const GUID IID_ITadsWorkbenchExtension =
    { 0xbc339b42,0xa0ce,0x0072, { 0x40,0x5e,0xd0,0xd7,0x92,0x49,0xde,0xd6 } };

/* ITadsWorkbenchEditor IID */
static const GUID IID_ITadsWorkbenchEditor =
    { 0x5abbbff8,0x5598,0x86de, { 0x5c,0xd3,0xb6,0x09,0x94,0xf4,0x63,0x95 } };

/* ITadsWorkbenchEditorMode IID */
static const GUID IID_ITadsWorkbenchEditorMode =
    { 0x6cb6afbb,0x05b1,0x740f, { 0xc8,0xb9,0x95,0xee,0x1f,0x63,0x5a,0x40 } };

/* ITadsWorkbenchMsgHandler IID */
static const GUID IID_ITadsWorkbenchMsgHandler =
    { 0x77285dc4,0x8238,0xe5eb, { 0x6d,0x8e,0x0e,0xd1,0x86,0x7b,0x48,0x86 } };
