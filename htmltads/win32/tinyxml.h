/* Copyright (c) 2009 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  tinyxml.h - tiny XML parser
Function
  A very simple XML parser
Notes

Modified
  09/12/09 MJRoberts  - Creation
*/

#ifndef TINYXML_H
#define TINYXML_H

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

class TinyXML
{
public:
    /*
     *   error codes
     */
    enum ErrorCode
    {
        /* success */
        OK = 0,

        /* out of memory */
        ErrOutOfMemory = 1,

        /* missing processing directive */
        ErrMissingProcDir = 2,

        /* unclosed processing directive */
        ErrUnclosedProcDir = 3,

        /* missing document element */
        ErrMissingDocEle = 4,

        /* extra text after end of document element */
        ErrExtraTextAtEnd = 5,

        /* unclosed comment */
        ErrUnclosedComment = 6,

        /* missing tag name after '<' */
        ErrMissingTagName = 7,

        /* mismatched end tag */
        ErrMismatchedEndTag = 8,

        /* missing '>' at end of tag */
        ErrMissingGt = 9,

        /* unclosed tag */
        ErrUnclosedTag = 10
    };

    TinyXML()
    {
        p = 0;
        len = 0;
        wrtp = 0;
        buf = 0;
        err = OK;
        errTag = 0;
        lin = col = 0;
        doc = 0;
        curCont = 0;
    }

    virtual ~TinyXML();

    /*
     *   Parse the given text as an XML file.  Returns true on success, false
     *   on error; use getError() to get the specific error information.
     */
    int parse(const char *txt, size_t len);

    /* look up an element by path */
    class TinyXMLElement *get(const char *path)
        { return get(path, 0); }

    /* get the root element */
    class TinyXMLElement *getDoc() { return doc; }

    /* look up by path, getting the nth child at the final path element */
    class TinyXMLElement *get(const char *path, int n);

    /* get the error code */
    ErrorCode getError() { return err; }

    /* get the error token */
    class TinyXMLElement *getErrorTag() { return errTag; }

protected:
    /*
     *   set an error code, if we don't already have one; return false so
     *   that this can be returned from parse() or other success/failure
     *   interfaces
     */
    int setError(ErrorCode e, class TinyXMLElement *tag = 0)
    {
        /* if we don't already have an error, set the new error */
        if (err == OK)
        {
            err = e;
            errTag = tag;
        }

        /* return failure */
        return FALSE;
    }


    /* skip spaces and comments */
    void skipSpaces();

    /*
     *   match and skip a token; returns true if the token matched, false if
     *   not, in which case we don't skip the token
     */
    int skipToken(const char *tok) { return skipToken(tok, strlen(tok)); }
    int skipToken(const char *tok, size_t len);

    /* match and skip a character sequence */
    int skipChars(const char *txt) { return skipChars(txt, strlen(txt)); }
    int skipChars(const char *txt, size_t len);

    /* skip any token */
    void skipToken();

    /* is the given character a token character? */
    int isTokChar(char c);

    /* is the current character a token character? */
    int isTokChar() { return len != 0 && isTokChar(*p); }

    /* parse a tag and its contents */
    void parseTag(int tagLine, int tagCol);

    /* advance by n characters */
    void inc(int n)
    {
        while (n-- > 0)
            inc();
    }

    /* advance to the next character */
    void inc()
    {
        /* only advance if we don't have an error and we have more text */
        if (err == OK && len > 0)
        {
            /* note the character we're skipping */
            char c = *p;

            /* advance to the next character */
            *wrtp++ = *p++, --len;

            /* this advances by a column as well */
            ++col;

            /* if this is a newline, count the next line */
            if (c == '\n')
                ++lin, col = 1;
        }
    }

    /* replace the next n characters with the given character */
    void xlat(int n, char c)
    {
        /* remember the current write pointer */
        char *w = wrtp;

        /* advance n characters */
        inc(n);

        /* go back to the old write pointer and add the translation */
        wrtp = w;
        *wrtp++ = c;
    }

    /* current parsing position */
    char *p;
    size_t len;

    /*
     *   write position - as we scan the text, we write it back into the
     *   buffer, so that we can overwrite "&x;" entity sequences with their
     *   translations
     */
    char *wrtp;

    /* private copy of the source text */
    char *buf;

    /* error */
    ErrorCode err;
    class TinyXMLElement *errTag;

    /* current line and column numbers */
    int lin, col;

    /* document root element */
    class TinyXMLElement *doc;

    /* current parsing container element */
    class TinyXMLElement *curCont;
};

/*
 *   XML Element
 */
class TinyXMLElement
{
    friend class TinyXML;

public:
    TinyXMLElement(int lin, int col,
                   const char *name, size_t len, TinyXMLElement *par)
    {
        this->lin = lin;
        this->col = col;
        this->name = name;
        this->nameLen = len;
        this->par = par;
        this->nxt = this->prv = 0;
        this->firstChi = this->lastChi = 0;
        this->contents = 0;
        this->contentLen = 0;
    }

    virtual ~TinyXMLElement();

    /* get a child by path */
    TinyXMLElement *get(const char *path) { return get(path, 0); }

    /* get a child by path, looking up the nth child at the final element */
    TinyXMLElement *get(const char *path, int n);

    /* line/column in source text */
    int lin, col;

    /* name pointer and length */
    const char *name;
    size_t nameLen;

    /* my text contents and length */
    const char *contents;
    size_t contentLen;

    /* parent */
    TinyXMLElement *par;

    /* next/previous sibling */
    TinyXMLElement *nxt, *prv;

    /* first child/last child */
    TinyXMLElement *firstChi, *lastChi;
};

#endif /* TINYXML_H */
