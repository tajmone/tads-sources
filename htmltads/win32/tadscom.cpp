/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tadscom.cpp - COM helpers
Function

Notes

Modified
  10/09/06 MJRoberts  - Creation
*/

#include <windows.h>

#include <stdlib.h>

#include "tadshtml.h"
#include "tadscom.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate a BSTR from an ANSI string.
 */
BSTR bstr_from_ansi(const textchar_t *str, size_t len)
{
    size_t wlen;
    BSTR bstr;

    /* return nothing for a null string */
    if (str == 0)
        return 0;

    /* figure the required number of wide characters */
    wlen = MultiByteToWideChar(CP_ACP, 0, str, len, 0, 0);

    /* allocate the BSTR */
    bstr = SysAllocStringLen(0, wlen);

    /* translate it */
    MultiByteToWideChar(CP_ACP, 0, str, len, bstr, wlen);

    /* null-terminate it */
    bstr[wlen] = 0;

    /* return it */
    return bstr;
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate a BSTR from a null-terminated ANSI string.
 */
BSTR bstr_from_ansi(const textchar_t *str)
{
    return (str == 0 ? 0 : bstr_from_ansi(str, get_strlen(str)));
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert an LPOLESTR into an ANSI string buffer, copying the ANSI
 *   characters into a buffer.
 */
textchar_t *ansi_from_olestr(
    textchar_t *dst, size_t dstsize, const OLECHAR *olestr, size_t olecnt)
{
    size_t len;

    /*
     *   if there's no BSTR, there's no ANSI string either; and if the buffer
     *   is of zero size, we can't do anything
     */
    if (olestr == 0 || dstsize == 0)
        return 0;

    /* map the character into the buffer */
    len = WideCharToMultiByte(CP_ACP, 0, olestr, olecnt, dst, dstsize, 0, 0);

    /* add a null terminator */
    if (len > dstsize - 1)
        len = dstsize - 1;
    dst[len] = '\0';

    /* return the buffer */
    return dst;
}

/* ------------------------------------------------------------------------ */
/*
 *   Allocate an ANSI string buffer from an LPOLESTR
 */
textchar_t *ansi_from_olestr(const OLECHAR *olestr, size_t olecnt)
{
    textchar_t *dst;
    size_t dstsize;

    /* if there's no BSTR, there's no ANSI string either */
    if (olestr == 0)
        return 0;

    /* figure out how much space we need */
    dstsize = WideCharToMultiByte(CP_ACP, 0, olestr, olecnt, 0, 0, 0, 0) + 1;

    /* allocate space */
    if ((dst = (textchar_t *)th_malloc(dstsize)) == 0)
        return 0;

    /* convert it */
    return ansi_from_olestr(dst, dstsize, olestr, olecnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert a BSTR into an ANSI string buffer, copying the ANSI characters
 *   into a buffer.
 */
textchar_t *ansi_from_bstr(
    textchar_t *dst, size_t dstsize, BSTR bstr, int free_bstr)
{
    /* convert the string */
    ansi_from_olestr(dst, dstsize, bstr, SysStringLen(bstr));

    /* free the BSTR if desired */
    if (free_bstr)
        SysFreeString(bstr);

    /* return the buffer */
    return dst;
}



/* ------------------------------------------------------------------------ */
/*
 *   Allocate an ANSI string buffer from a BSTR
 */
textchar_t *ansi_from_bstr(BSTR bstr, int free_bstr)
{
    /* convert the string */
    textchar_t *dst = ansi_from_olestr(bstr, SysStringLen(bstr));

    /* free the BSTR if desired */
    if (free_bstr)
        SysFreeString(bstr);

    /* return the buffer */
    return dst;
}
