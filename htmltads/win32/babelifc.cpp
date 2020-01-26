/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  babelifc.cpp - workbench interface to babel
Function

Notes

Modified
  11/05/06 MJRoberts  - Creation
*/

#include "tadshtml.h"

extern "C" {
#include "babel.h"
#include "babel_handler.h"
}

/*
 *   read an IFID
 */
int babel_read_ifid(const char *fname, char *buf, size_t buflen)
{
    char *f;
    CStringBuf fnamebuf(fname);
    int ok = 0;

    /* initialize babel with the file */
    if ((f = babel_init(fnamebuf.get())) != 0)
    {
        char outbuf[TREATY_MINIMUM_EXTENT];

        /* get the IFID */
        babel_treaty(GET_STORY_FILE_IFID_SEL, outbuf, TREATY_MINIMUM_EXTENT);

        /* copy it to the output buffer */
        safe_strcpy(buf, buflen, outbuf);
    }

    /* release babel memory */
    babel_release();

    /* return the success indication */
    return ok;
}

/*
 *   synthesize an iFiction record from a game file
 */
int babel_synth_ifiction(const char *fname, CStringBuf *buf)
{
    char *f;
    CStringBuf fnamebuf(fname);
    int ok = 0;

    /* initialize babel with the file */
    if ((f = babel_init(fnamebuf.get())) != 0)
    {
        /* figure the size of the iFiction record */
        int32 len = babel_treaty(GET_STORY_FILE_METADATA_EXTENT_SEL, 0, 0);
        if (len > 0)
        {
            /* allocate space in the caller's buffer */
            buf->ensure_size(len);

            /* get the iFiction data */
            len = babel_treaty(GET_STORY_FILE_METADATA_SEL, buf->get(), len);

            /* if we got any data, it's a success */
            ok = (len > 0);
        }
    }

    /* release babel memory */
    babel_release();

    /* return the success indication */
    return ok;
}
