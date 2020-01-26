#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2006 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  cmdtool.cpp - command tool: extract command identifiers from htmlres.h
Function
  This scans htmlres.h for the special command description comments,
  and builds a list of commands for the keyboard/menu/toolbar customization
  mechanism.

  Run with stdin redirected from the source file, and stdout redirected
  to the target .cpp file:

     cmdtool <htmlres.h >cmdlist.cpp
Notes

Modified
  10/20/06 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define TRUE 1
#define FALSE 0

/* read a line from the input, stripping newlines */
static int read_line(char *buf, size_t buflen)
{
    size_t len;

    /* read the next line */
    if (fgets(buf, buflen, stdin) == 0)
    {
        buf[0] = '\0';
        return FALSE;
    }

    /* remove trailing newlines */
    for (len = strlen(buf) ; len > 0 && buf[len-1] == '\n' ;
         buf[--len] = '\0') ;

    /* success */
    return TRUE;
}

/*
 *   print a string as a string, enclosed in quotes, escaping embedded quote
 *   marks
 */
static void print_as_string(const char *str)
{
    const char *p;

    /* write the open quote */
    printf("\"");

    /* write out the string, escaping any quotes we find */
    for (p = str ; ; ++p)
    {
        /* if it's a quote or end of string, write out the part up to here */
        if ((*p == '"' || *p == '\0') && p != str)
            printf("%.*s", p - str, str);

        /* if it's end of string, we're done */
        if (*p == '\0')
            break;

        /* if it's a quote, write an escaped quote */
        if (*p == '"')
        {
            /* write the quote */
            printf("\\\"");

            /* the next chunk starts after the quote */
            str = p + 1;
        }
    }

    /* write the close quote */
    printf("\"");
}

/*
 *   print a string as HTML, encoding markup-significant characters
 */
static void print_as_html(FILE *fpdoc, const char *str)
{
    const char *p;

    /* if there's no doc file, skip this */
    if (fpdoc == 0)
        return;

    /* write out the string, escaping any markups we find */
    for (p = str ; ; ++p)
    {
        int is_markup;

        /* note if it's a markup character */
        is_markup = (strchr("<>&", *p) != 0);

        /* if it's a markup or end of line, write the part to here */
        if (p != str && (*p == '\0' || is_markup))
            fprintf(fpdoc, "%.*s", p - str, str);

        /* if it's end of string, we're done */
        if (*p == '\0')
            break;

        /* if it's a markup, escape it */
        if (is_markup)
        {
            switch (*p)
            {
            case '>':
                fprintf(fpdoc, "&gt;");
                break;

            case '<':
                fprintf(fpdoc, "&lt;");
                break;

            case '&':
                fprintf(fpdoc, "&amp;");
                break;
            }

            /* the next chunk starts after the quote */
            str = p + 1;
        }
    }
}


/*
 *   command name/definition pair
 */
struct cmddef
{
    cmddef(const char *cmd, size_t len)
    {
        /* copy the command */
        if (len > sizeof(this->cmd) - 1)
            len = sizeof(this->cmd) - 1;
        memcpy(this->cmd, cmd, len);
        this->cmd[len] = '\0';

        /* not in a list yet */
        nxt = 0;

        /* no def yet */
        def[0] = '\0';

        /* no system ID yet */
        sysid = 0;
    }

    /* write it out */
    void write(FILE *fp)
    {
        /* write the ID */
        fprintf(fp, "<dt>%s\n<dd>", cmd);

        /* write the definition */
        print_as_html(fp, def);

        /* end the definition */
        fprintf(fp, ".\n");

        /* add the system ID */
        if (sysid != 0)
            fprintf(fp, "<br><span class=sysid>TADS %c only</span>\n", sysid);
    }

    /* add definition text */
    void adddef(const char *p)
    {
        size_t len = strlen(p);
        size_t deflen = strlen(def);
        size_t rem = sizeof(def) - deflen - 1;
        if (len > rem)
            len = rem;

        memcpy(def + deflen, p, len);
        def[deflen + len] = '\0';
    }

    /* compare for sorting */
    static int sortcb(const void *a0, const void *b0)
    {
        const cmddef **a = (const cmddef **)a0;
        const cmddef **b = (const cmddef **)b0;

        return stricmp((*a)->cmd, (*b)->cmd);
    }

    cmddef *nxt;
    char cmd[128];
    char def[512];
    char sysid;
};

/*
 *   main entrypoint
 */
void main(int argc, char **argv)
{
    char macro[256];
    FILE *fpdoc = 0;
    int i;
    cmddef *cmds = 0;
    int cmdcnt = 0;
    cmddef *curcmd = 0;

    /* scan arguments */
    for (i = 1 ; i < argc ; ++i)
    {
        if (strcmp(argv[i], "-doc") == 0 && i+1 < argc)
        {
            ++i;
            if ((fpdoc = fopen(argv[i], "w")) == 0)
            {
                fprintf(stderr, "error: unable to open -doc output file\n");
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "usage: cmdtool [-doc <html-output-file>]\n");
            exit(1);
        }
    }

    /* write the boilerplate at the start of the output file */
    printf("/*\n"
           " *   THIS IS A MECHANICALLY GENERATED FILE - DO NOT EDIT\n"
           " *   See the makefile (tads/html/win32/makefile.vc5) for "
           "information on how\n"
           " *   this file is generated and used.\n"
           " */\n\n");

    /* process the input file */
    for (macro[0] = '\0' ; ; )
    {
        char buf[4096];
        char *p, *dst, *start;

        /* get the next line */
        if (!read_line(buf, sizeof(buf)))
            break;

    process_line:
        /* scan for #defines */
        if (memcmp(buf, "#define ", 8) == 0)
        {
            /* it's a #define - find the start of the symbol name */
            for (p = buf + 8 ; isspace(*p) ; ++p) ;

            /* copy the symbol name */
            for (dst = macro ;
                 dst < macro + sizeof(macro) - 1 && *p != '\0'
                     && !isspace(*p) && *p != '(' ; *dst++ = *p++) ;

            /* null-terminate the macro name */
            *dst = '\0';

            /* if it ended with a '(', it's not one we're interested in */
            if (*p == '(')
                macro[0] = '\0';

            /* we're done with this line */
            continue;
        }

        /* if it's one of our special comment lines, process it */
        if (memcmp(buf, "///cmd ", 7) == 0 && macro[0] != '\0')
        {
            /* find the command name */
            for (p = buf + 7 ; isspace(*p) ; ++p) ;
            for (start = p ; *p != '\0' && !isspace(*p) ; ++p) ;

            /* write out the macro ID and the command name */
            printf("W32COMMAND( %s, \"%.*s\",\n", macro, p - start, start);

            /* add the command to our list */
            if (fpdoc != 0)
            {
                curcmd = new cmddef(start, p - start);
                curcmd->nxt = cmds;
                cmds = curcmd;
                ++cmdcnt;
            }

            /* find the start of the description */
            for ( ; isspace(*p) ; ++p) ;

            /* write out the description */
            printf("  ");
            print_as_string(p);

            /* add it to the cmddef */
            if (curcmd != 0)
                curcmd->adddef(p);

            /* process continuation lines */
            for (;;)
            {
                /* get the next line */
                if (!read_line(buf, sizeof(buf)))
                    break;

                /* if it's not a continuation line, we're done */
                if (memcmp(buf, "/// ", 4) != 0)
                    break;

                /* write out the continuation of the description */
                printf("\n    ");
                print_as_string(buf + 3);

                /* add it to the cmddef */
                if (curcmd != 0)
                    curcmd->adddef(buf + 3);
            }

            /* end the description */
            printf(",\n  ");

            /* check for a system-specifier line */
            if (memcmp(buf, "///tads", 7) == 0)
            {
                /* add the specifier for the one system */
                printf("HTML_SYSID_TADS%c", buf[7]);

                /* note it in the cmddef */
                if (curcmd != 0)
                    curcmd->sysid = buf[7];

                /* read the next line */
                if (!read_line(buf, sizeof(buf)))
                    break;
            }
            else
            {
                /* no specifier, so both systems support this command */
                printf("HTML_SYSID_TADS2 | HTML_SYSID_TADS3");
            }

            /* add the is-Scintilla flag */
            printf(", %s\n",
                   memcmp(macro, "ID_SCI_", 7) == 0 ? macro + 3 : "0");

            /* end the block */
            printf(" )\n\n");

            /*
             *   we've already read the next line after the command
             *   descriptor block, so go back and process it
             */
            goto process_line;
        }
    }

    /* if there's a doc file, write it out */
    if (fpdoc != 0)
    {
        cmddef **cmdarr;
        int i;

        /* write the opening boilerplate */
        fprintf(fpdoc,
                "<html>\n\n<head>\n"
                "<title>TADS Workbench Command List</title>\n"
                "<link rel=stylesheet href=\"wbdoc.css\">\n"
                "</head>\n\n"
                "<body>\n"
                "<font size=-1>Help Topics &gt; "
                "<a href=\"wbcont.htm\">Table of Contents</a></font>"
                "<br><br><br>\n"
                "<h1>TADS Workbench Command List</h1>\n"
                "<dl class=cmdlist>\n");

        /* build an array of the commands */
        cmdarr = new cmddef*[cmdcnt];
        for (curcmd = cmds, i = 0 ; i < cmdcnt && curcmd != 0 ;
             curcmd = curcmd->nxt, ++i)
            cmdarr[i] = curcmd;

        /* sort it */
        qsort(cmdarr, cmdcnt, sizeof(cmdarr[0]), &cmddef::sortcb);

        /* write them out */
        for (i = 0 ; i < cmdcnt ; ++i)
            cmdarr[i]->write(fpdoc);

        /* write the closing boilerplate */
        fprintf(fpdoc, "</dl>\n\n"
                "<br><br><br><font size=-1>Help Topics &gt; "
                "<a href=\"wbcont.htm\">Table of Contents</a></font>"
                "<br><br>\n"
                "</body>\n</html>\n");

        /* close the file */
        if (fclose(fpdoc) == EOF)
        {
            fprintf(stderr, "Error writing documentation output file\n");
            exit(2);
        }
    }
}
