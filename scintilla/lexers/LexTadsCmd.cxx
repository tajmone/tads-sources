// [mjr] - entire file
// Scintilla source code edit control
/**
 *   @file LexTadsCmd.cxx * Lexer for TADS command scripts
 */
// Copyright 2006 by Michael Roberts <mjr_@hotmail.com>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

static void ColouriseTadsCmdDoc(
    unsigned int startPos, int length, int initStyle,
    WordList *keywordlists[], Accessor &styler)
{
    styler.StartAt(startPos);

    StyleContext sc(startPos, length, initStyle, styler);

    for (; sc.More(); sc.Forward())
    {
        switch(sc.state)
        {
        case SCE_TADSCMD_DEFAULT:
            // at start of line - if the line starts with '<', it's a tag;
            // if it starts with '>', it's the start of a command line in
            // an old-style command script; otherwise it's a comment
            if (sc.ch == '<')
                sc.SetState(SCE_TADSCMD_TAG);
            else if (sc.ch == '>')
            {
                sc.SetState(SCE_TADSCMD_TAG);
                sc.Forward();
                sc.SetState(SCE_TADSCMD_PARAM);
            }
            else
                sc.SetState(SCE_TADSCMD_COMMENT);
            break;

        case SCE_TADSCMD_COMMENT:
        case SCE_TADSCMD_PARAM:
            // a comment or parameter ends at the end of the line
            if (sc.atLineEnd)
                sc.SetState(SCE_TADSCMD_DEFAULT);
            break;

        case SCE_TADSCMD_TAG:
            // a tag ends at the '>'
            if (sc.ch == '>')
                sc.ForwardSetState(SCE_TADSCMD_PARAM);
            break;
        }
    }

    sc.Complete();
}


LexerModule lmTadsCmd(SCLEX_TADSCMD, ColouriseTadsCmdDoc, "tadscmd");

