#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tinyxml.cpp - a very simple XML parser
Function
  Small, lightweight XML parser
Notes

Modified
  09/12/09 MJRoberts  - Creation
*/

#include <Windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "tinyxml.h"

TinyXML::~TinyXML()
{
    /* if we have a source buffer, delete it */
    if (buf != 0)
        delete [] buf;

    /* delete the tag tree */
    if (doc != 0)
        delete doc;
}

/* skip spaces and comments */
void TinyXML::skipSpaces()
{
    /* keep going until we run out of spaces or comments */
    for (;;)
    {
        /* skip spaces */
        while (len != 0 && isspace(*p))
            inc();

        /* if we're at a comment, skip the comment */
        if (len >= 4 && memcmp(p, "<!--", 4) == 0)
        {
            /* comment - skip the open comment */
            inc(4);

            /* scan for the end marker */
            int found = FALSE;
            for ( ; len > 0 ; inc())
            {
                if (len > 3 && memcmp(p, "-->", 3) == 0)
                {
                    found = TRUE;
                    break;
                }
            }

            /* check to see if we found the end marker */
            if (found)
            {
                /* got it - skip the end marker and keep going */
                inc(3);
            }
            else
            {
                /* not found - set an error and give up */
                setError(ErrUnclosedComment);
                return;
            }
        }
        else
        {
            /* not a comment, so we're done */
            break;
        }
    }
}

/*
 *   check for regular token characters - a regular token character is
 *   anything other than <, >, &, or quotes
 */
int TinyXML::isTokChar(char c)
{
    return strchr("<>/&'\"? \t", c) == 0;
}

/* match and skip a character sequence */
int TinyXML::skipChars(const char *txt, size_t txtlen)
{
    /* skip spaces before the token */
    skipSpaces();

    /* check for a match to the token */
    if (err == OK
        && len >= txtlen
        && memcmp(p, txt, txtlen) == 0)
    {
        /* matched - skip the token and indicate we found it */
        inc(txtlen);
        return TRUE;
    }
    else
    {
        /* no match */
        return FALSE;
    }
}

/* match and skip a token */
int TinyXML::skipToken(const char *tok, size_t toklen)
{
    /* skip spaces before the token */
    skipSpaces();

    /* check for a match to the token */
    if (err == OK
        && len >= toklen
        && memcmp(p, tok, toklen) == 0
        && (len == toklen
            || (isTokChar(*(tok+toklen-1)) ^ isTokChar(*(p+toklen)))))
    {
        /* matched - skip the token and indicate we found it */
        inc(toklen);
        return TRUE;
    }
    else
    {
        /* no match */
        return FALSE;
    }
}

/* skip any token */
void TinyXML::skipToken()
{
    /* skip spaces up to the next token */
    skipSpaces();

    /* if we're out of text (or encountered an error), stop */
    if (err != OK || len == 0)
        return;

    /* skip the token */
    if (isTokChar())
    {
        /* regular token character - skip continguous token characters */
        for (inc(); isTokChar() ; inc()) ;
    }
    else if (*p == '"' || *p == '\'')
    {
        /* note the open quote */
        char qu = *p;

        /* skip to the matching quote */
        for (inc() ; len != 0 && *p != qu ; inc()) ;

        /* skip the close quote */
        if (len != 0)
            inc();
    }
    else
    {
        /* punctuation mark - just skip a single character */
        inc();
    }
}

void TinyXML::parseTag(int tagLin, int tagCol)
{
    /* parse the tag name */
    const char *start = wrtp;
    skipToken();
    if (p == start || err != OK)
    {
        setError(ErrMissingTagName);
        return;
    }

    /* create the element */
    TinyXMLElement *ele = new TinyXMLElement(
        tagLin, tagCol, start, wrtp - start, curCont);

    /* link this into the current container, if any */
    if (curCont != 0)
    {
        /* link this into the current container's child list */
        if (curCont->lastChi != 0)
        {
            /* link this after the current last child */
            curCont->lastChi->nxt = ele;
            ele->prv = curCont->lastChi;

            /* it's now the last child */
            curCont->lastChi = ele;
        }
        else
        {
            /* this is the container's first and only child */
            curCont->firstChi = curCont->lastChi = ele;
        }
    }
    else
    {
        /* this is the document element */
        doc = ele;
    }

    /* skip to the '>' */
    while (err == OK && !skipChars(">"))
    {
        /* check for a self-closing tag */
        if (skipChars("/") && skipChars(">"))
        {
            /* self-closing tag - it has no contents */
            ele->contents = wrtp;
            ele->contentLen = 0;

            /* done with this tag */
            return;
        }

        /* skip the current token */
        skipToken();
    }

    /* this is the new current container */
    curCont = ele;

    /* this is where my contents start */
    ele->contents = wrtp;

    /* process the contents until we find the end tag */
    while (len != 0 && err == OK)
    {
        /* check what we have */
        if (*p == '<')
        {
            /* note where the '<' is */
            char *tWrtp = wrtp;
            tagLin = lin, tagCol = col;

            /* skip the '<' */
            inc();

            /* if it's a comment, delete it */
            if (len >= 3 && memcmp(p, "!--", 3) == 0)
            {
                /* skip the open comment */
                inc(3);

                /* find the end of the comment */
                int found = FALSE;
                for ( ; len > 0 ; inc())
                {
                    /* if this is the end of the comment, we're done */
                    if (len >= 3 && memcmp(p, "-->", 3) == 0)
                    {
                        found = TRUE;
                        break;
                    }
                }

                /* if we found the end of comment, we're skip it */
                if (found)
                {
                    /* skip the close of the comment */
                    inc(3);
                }
                else
                {
                    /* unclosed comment - set an error and give up */
                    setError(ErrUnclosedComment);
                    return;
                }

                /*
                 *   set the write pointer to where the comment started, so
                 *   that the final text eliminates the comment
                 */
                wrtp = tWrtp;
            }
            else
            {
                /*
                 *   This isn't a comment, so it must be a tag.  Check to see
                 *   if it's an open or close tag.
                 */
                skipSpaces();
                if (skipChars("/"))
                {
                    /*
                     *   Close tag.  This must be a match for the current
                     *   container, or it's an error.
                     */
                    if (!skipToken(curCont->name, curCont->nameLen))
                    {
                        /* mismatched end tag */
                        setError(ErrMismatchedEndTag, curCont);
                        return;
                    }

                    /* the '>' must be the next token */
                    if (!skipChars(">"))
                    {
                        setError(ErrMissingGt);
                        return;
                    }

                    /*
                     *   we've finished with this tag - its contents end at
                     *   the '<'
                     */
                    curCont->contentLen = tWrtp - curCont->contents;

                    /* pop the container */
                    curCont = curCont->par;

                    /* we're done parsing the tag */
                    return;
                }
                else
                {
                    /* it's an open tag - parse its contents recursively */
                    parseTag(tagLin, tagCol);

                    /* if an error occurred parsing the contents, abort */
                    if (err != OK)
                        return;
                }
            }
        }
        else if (*p == '&')
        {
            /* entity markup */
            if (len >= 5 && memicmp(p, "&amp;", 5) == 0)
                xlat(5, '&');
            else if (len >= 4 && memicmp(p, "&lt;", 4) == 0)
                xlat(4, '<');
            else if (len >= 4 && memicmp(p, "&gt;", 4) == 0)
                xlat(4, '>');
        }
        else
        {
            /* anything else is an ordinary character - just skip it */
            inc();
        }
    }

    /* if we got this far, we ran out of text without finding the end tag */
    setError(ErrUnclosedTag);
}

int TinyXML::parse(const char *txt, size_t txtlen)
{
    /* no error yet */
    err = OK;
    errTag = 0;

    /* no document or parsing container yet */
    doc = 0;
    curCont = 0;

    /* make a private copy of the source text, so we can modify it in place */
    buf = new char[txtlen];
    if (buf == 0)
        return setError(ErrOutOfMemory);

    /* copy it all, translating newline pairs into '\n's */
    const char *src = txt, *srcEnd = txt + txtlen;
    char *dst;
    for (dst = buf ; src < srcEnd ; )
    {
        /* translate CR-LF, LF-CR, and CR to '\n' */
        if (srcEnd - src >= 2
            && ((*src == '\n' && *(src+1) == '\r')
                || (*src == '\r' && *(src+1) == '\n')))
        {
            /* CR-LF or LF-CR - translate to '\n' */
            *dst++ = '\n';
            src += 2;
        }
        else if (*src == '\r')
        {
            /* CR - translate to '\n' */
            *dst++ = '\n';
            src += 1;
        }
        else
        {
            /* for anything else, copy directly */
            *dst++ = *src++;
        }
    }

    /* set up at the start of the text */
    p = wrtp = buf;
    len = dst - buf;

    /* we're at line 1 column 1 */
    lin = 1;
    col = 1;

    /* skip leading spaces */
    skipSpaces();

    /* check for the <?XML ... ?> directive */
    if (!skipChars("<?") || !skipToken("xml"))
        return setError(ErrMissingProcDir);

    /* skip anything up to the "?>" */
    while (err == OK)
    {
        /* if we're out of text, it's an error */
        if (len == 0)
            return setError(ErrUnclosedProcDir);

        /* if this is the ?> token, we're done */
        if (skipChars("?>"))
            break;

        /* skip whatever other token we're at */
        skipToken();
    }

    /* skip to the next real token */
    skipSpaces();
    int tagLin = lin, tagCol = col;

    /* make sure there's a document element */
    if (!skipChars("<") || (skipSpaces(), !isTokChar()) || err != OK)
        return setError(ErrMissingDocEle);

    /* parse the document element tag - end tag */
    parseTag(tagLin, tagCol);

    /* make sure we're at the end of the document */
    skipSpaces();
    if (len != 0)
        return setError(ErrExtraTextAtEnd);

    /* if we haven't encountered an error, we successfully parsed the xml */
    return err == OK;
}

/*
 *   look up an element by path
 */
TinyXMLElement *TinyXML::get(const char *path, int n)
{
    return (doc != 0 ? doc->get(path, n) : 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   XML Element
 */

TinyXMLElement::~TinyXMLElement()
{
    /* delete our children */
    TinyXMLElement *cur, *nxt;
    for (cur = firstChi ; cur != 0 ; cur = nxt)
    {
        /* remember the next child */
        nxt = cur->nxt;

        /* delete this child (which recursively deletes its children) */
        delete cur;
    }
}

/*
 *   Get a child element by path.
 */
TinyXMLElement *TinyXMLElement::get(const char *path, int n)
{
    const char *p;

    /* if we're out of path, it's us! */
    if (*path == '\0')
        return this;

    /* find the end of the first token */
    for (p = path ; *p != '\0' && *p != '/' && *p != '[' ; ++p) ;
    size_t nl = p - path;

    /* if it's an index, get the index value */
    int idx = -1;
    if (*p == '[')
    {
        /* get the index value */
        idx = atoi(++p);

        /* skip to the '[' */
        for ( ; *p != '\0' && *p != ']' ; ++p) ;
        if (*p == ']')
            ++p;
    }

    /* skip the '/' path separator, if present */
    if (*p == '/')
        ++p;

    /*
     *   if this is the last element, and there was no index value specified
     *   in the path, use the index passed in
     */
    if (*p == '\0' && idx == -1)
        idx = n;

    /* if there's no index, use index 0 by default */
    if (idx == -1)
        idx = 0;

    /* find a child with this name */
    TinyXMLElement *e;
    for (e = firstChi ; e != 0 ; e = e->nxt)
    {
        /* if this child has the given name, it's the one we want */
        if (e->nameLen == nl && memcmp(e->name, path, nl) == 0)
        {
            /*
             *   the name matches - if the index is zero, recursively resolve
             *   the rest of the path through the item, otherwise just
             *   decrement the index and keep searching oru children
             *   otherwise decrement the index and keep searching
             */
            if (idx-- == 0)
                return e->get(p, n);
        }
    }

    /* we didn't find a child matching the element */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Tests
 */
#ifdef TEST_TINYXML
void main()
{
    TinyXML x;
    static const char *txt =
        "<?xml version=\"1.0\"?>\r\n"
        "<!-- a little test -->\r\n"
        "<test>\r\n"
        "  <items>\r\n"
        "    <item>\r\n"
        "       <name>First item name<!-- with a comment! --></name>\r\n"
        "       <desc>This is the <b>first</b> item's description!</desc>\r\n"
        "       <flag notes=\"empty tag!\"/>\r\n"
        "    </item>\r\n"
        "    <item>\r\n"
        "       <name>Second item name</name>\r\n"
        "       <desc>Here's the description of the <b>second</b> item.</desc>\r\n"
        "    </item>\r\n"
        "    <item>\r\n"
        "       <name>Third name</name>\r\n"
        "       <desc>This item has &lt;, &gt;, and &amp; entities.</desc>\r\n"
        "       <flag notes=\"another empty tag!!!\" / >\r\n"
        "    </item>\r\n"
        "  </items>\r\n"
        "</test>\r\n";

    if (x.parse(txt, strlen(txt)))
    {
        TinyXMLElement *e;

        printf("Parsed OK!!!\n");

        for (int i = 0 ; ; ++i)
        {
            char path[128];
            sprintf(path, "items/item[%d]/name", i);
            e = x.get(path);
            if (e != 0)
                printf("Name #%d = [%.*s]\n", i, e->contentLen, e->contents);
            else
                break;

            sprintf(path, "items/item[%d]/desc", i);
            e = x.get(path);
            if (e != 0)
                printf("Desc #%d = [%.*s]\n", i, e->contentLen, e->contents);

            e = x.get("items/item", i);
            if (e != 0 && e->get("flag") != 0)
                printf("Item %d has <flag> element!!!\n", i);
            if (e != 0 && (e = e->get("name")) != 0)
                printf("By index: name #%d = [%.*s]\n",
                       i, e->contentLen, e->contents);
        }
    }
    else
    {
        printf("Error parsing: %d\n", (int)x.getError());
    }
}
#endif
