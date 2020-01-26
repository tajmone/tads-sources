// [mjr] - whole file

/*
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *
 *   This is an adaptation of the TADS 3 regular expression subsystem for
 *   Scintilla.  This is licensed under the same license as the rest of TADS
 *   3 - see license.txt in the TADS 3 materials for details.
 */
/*
Name
  RESearch2.h - regular expression searcher, T3-for-Scintilla version
Function

Notes
  Adapted from the TADS 2 regular expression parser.  This version uses
  UTF-8 strings rather than simple single-byte character strings, and is
  organized into a C++ class.
Modified
  04/11/99 CNebel     - Fix warnings.
  10/07/98 MJRoberts  - Creation
*/

#ifndef VMREGEX_H
#define VMREGEX_H

#include <stdlib.h>

#include "CharClassify.h" // [MJR]

/*
 *   The following defines are not meant to be changeable.  They are for
 *   readability only.
 */
#define MAXCHR  256
#define CHRBIT  8
#define BITBLK  MAXCHR/CHRBIT

class CharacterIndexer {
public:
    virtual char CharAt(int index)=0;
    virtual ~CharacterIndexer() {
    }
};

class RESearch
{
public:
    RESearch(CharClassify *cc);
    ~RESearch();

    /*
     *   the Scintilla public interface
     */

    enum {MAXTAG=10};
    enum {MAXNFA=2048};
    enum {NOTFOUND=-1};

    int bopat[MAXTAG];
    int eopat[MAXTAG];
    char *pat[MAXTAG];

    bool GrabMatches(CharacterIndexer &ci);
    const char *Compile(const char *pat, int length,
                        bool caseSensitive, bool posix);
    int Execute(class CharacterIndexer &ci, int lp, int endp, bool word);


protected:
    /*
     *   Character classification.  The T3 version uses Unicode
     *   classifications, which aren't available here.  The Scintilla
     *   character classifier provides some limited classifications; for
     *   others, we rely on the standard C library classifier, which at least
     *   works for ASCII characters.
     */
    int t3_is_alpha(int c) { return isalpha(c); }
    int t3_is_digit(int c) { return isdigit(c); }
    int t3_is_upper(int c) { return isupper(c); }
    int t3_is_lower(int c) { return islower(c); }
    wchar_t t3_to_upper(wchar_t c) { return (wchar_t)toupper(c); }
    wchar_t t3_to_lower(wchar_t c) { return (wchar_t)tolower(c); }
    int t3_is_space(int c)
        { return cc_->GetClass((unsigned char)c) == CharClassify::ccSpace; }
    int t3_is_punct(int c)
        { return cc_->GetClass((unsigned char)c) == CharClassify::ccPunctuation; }
    int is_word_char(wchar_t c) { return cc_->IsWord((unsigned char)c); }

    /* match a string to a compiled expression */
    int match(struct sci_buf_ptr line_ptr, int ofs, size_t origlen,
              const struct re_compiled_pattern_base *pattern,
              const struct re_tuple *tuple_arr,
              const struct re_machine *machine,
              struct re_group_register *regs, short *loop_vars);

    /* clear a set of group registers */
    void clear_group_regs(struct re_group_register *regs);

    /* our compiled pattern */
    struct re_compiled_pattern *pattern_;

    /* number of group registers used in pattern */
    int group_cnt_;

    /* our group registers */
    struct re_group_register *regs_;

    /* our regex parser */
    class CRegexParser *parser_;

    /* match state stack */
    class CRegexStack *stack_;

    /* Scintilla character classifier */
    CharClassify *cc_;
};

#endif /* VMREGEX_H */

