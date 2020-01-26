/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  keytool.cpp - keymap-to-initializer converter
Function
  This is a little program that takes a Workbench .keymap file and converts
  it to a list of static initializers that can be #included in a Workbench
  .cpp file, to build a default key table.

  We read the key table from stdin and write the static initializer table
  to stdout.
Notes

Modified
  10/27/06 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tadskb.h"

/* command name table */
struct cmd_t
{
    const char *sym;
    const char *id;
};
static const cmd_t cmds[] =
{
#define W32COMMAND(id, sym, desc, sys, sci_id) { sym, #id },
#include "w32tdbcmd.h"
#undef W32COMMAND
    { 0, 0 }
};

void main()
{
    CTadsKeyboard kb;
    char section[128] = "Global";

    /* write the header */
    printf("/*\n"
           " *   THIS IS A MECHANICALLY GENERATED FILE - DO NOT EDIT\n"
           " *   See the makefile (tads/html/win32/makefile.vc5) for "
           "information on how\n"
           " *   this file is generated and used.\n"
           " */\n\n");

    /* keep going until we run out of input */
    for (int linenum = 1 ; ; ++linenum)
    {
        char buf[256];
        char keybuf[256];
        char *p, *dst;
        const cmd_t *cp;

        /* read a line */
        if (fgets(buf, sizeof(buf), stdin) == 0)
            break;

        /* compress spaces and remove newlines */
        for (p = dst = buf ; *p != '\0' ; ++p)
        {
            /*
             *   copy ordinary characters unconditionally; don't copy
             *   newlines; copy spaces only following a non-space character
             */
            if (*p != '\n'
                && (!isspace(*p) || (dst != buf && !isspace(*(dst-1)))))
                *dst++ = *p;
        }

        /* remove any trailing spaces we left in place */
        for ( ; dst > buf && isspace(*(dst-1)) ; --dst) ;

        /* null-terminate the line */
        *dst = '\0';

        /* skip blank and comment lines */
        if (buf[0] == '\0' || buf[0] == '#')
            continue;

        /* check for a section line */
        if (buf[0] == '[' && *(dst-1) == ']')
        {
            memcpy(section, buf + 1, dst - buf - 2);
            section[dst - buf - 2] = '\0';
            continue;
        }

        /* make sure it looks like a valid line */
        if ((p = strstr(buf, " = ")) == 0)
        {
            fprintf(stderr, "line %d: ill-formed line: missing '='\n",
                    linenum);
            exit(2);
        }

        /* look up the command */
        for (cp = cmds ; cp->sym != 0 && stricmp(cp->sym, p+3) != 0 ; ++cp) ;

        /* canonicalize the key string and make sure it's valid */
        strcpy(keybuf, buf);
        keybuf[p - buf] = '\0';
        if (!kb.canonicalize_key_name(keybuf))
        {
            fprintf(stderr, "line %d: invalid key name \"%s\"\n",
                    linenum, keybuf);
            exit(2);
        }

        /* if we didn't find it, and it's not an Ext. command, it's an error */
        if (cp->sym == 0 && memicmp(p + 3, "Ext.", 4) != 0)
        {
            fprintf(stderr, "line %d: command \"%s\" not found\n",
                    linenum, p + 3);
            exit(2);
        }

        /* write it out */
        printf("W32KEYBINDING(\"%s\", \"%s\", %s, \"%s = %s\") /* %s */\n",
               section, keybuf, cp->id, keybuf, p + 3, p + 3);
    }
}
