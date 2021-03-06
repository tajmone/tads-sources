/* $Header: d:/cvsroot/tads/TADS2/regex.h,v 1.3 1999/07/11 00:46:34 MJRoberts Exp $ */

/*
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  regex.h - regular expression parser for TADS
Function

Notes

Modified
  04/11/99 CNebel     - Fix warnings.
  10/07/98 MJRoberts  - Creation
*/

#ifndef REGEX_H
#define REGEX_H

#include <stdlib.h>


/* state ID */
typedef int re_state_id;

/* invalid state ID - used to mark null machines */
#define RE_STATE_INVALID   ((re_state_id)-1)

/* first valid state ID */
#define RE_STATE_FIRST_VALID  ((re_state_id)0)


/* ------------------------------------------------------------------------ */
/*
 *   Group register structure.  Each register keeps track of the starting
 *   and ending offset of the group's text.
 */
typedef struct
{
    const char *start_ofs;
    const char *end_ofs;
} re_group_register;

/* number of group registers we keep */
#define RE_GROUP_REG_CNT  10


/* ------------------------------------------------------------------------ */
/*
 *   Denormalized state transition tuple.  Each tuple represents the
 *   complete set of transitions out of a particular state.  A particular
 *   state can have one character transition, or two epsilon transitions.
 *   Note that we don't need to store the state ID in the tuple, because
 *   the state ID is the index of the tuple in an array of state tuples.
 */
typedef struct
{
    /* the character we must match to transition to the target state */
    char ch;

    /* the target states */
    re_state_id next_state_1;
    re_state_id next_state_2;

    /* character range match table, if used */
    unsigned char *char_range;

    /* flags */
    unsigned char flags;
} re_tuple;


/*
 *   Tuple flags
 */

/* this state is the start of a group - the 'ch' value is the group ID */
#define RE_STATE_GROUP_BEGIN  0x02

/* this state is the end of a group - 'ch' is the group ID */
#define RE_STATE_GROUP_END    0x04


/* ------------------------------------------------------------------------ */
/*
 *   Regular expression compilation context structure.  This tracks the
 *   state of the compilation and stores the resources associated with the
 *   compiled expression.
 */
typedef struct
{
    /* error context */
    errcxdef *errctx;

    /* next available state ID */
    re_state_id next_state;

    /*
     *   The array of transition tuples.  We'll allocate this array and
     *   expand it as necessary.
     */
    re_tuple *tuple_arr;

    /* number of transition tuples allocated in the array */
    int tuples_alloc;

    /* current group ID */
    int cur_group;

    /* group registers */
    re_group_register regs[RE_GROUP_REG_CNT];

    /*
     *   Buffer for retaining a copy of the last string we scanned.  We
     *   retain our own copy of each string, and point the group registers
     *   into this copy rather than the caller's original string -- this
     *   ensures that the group registers remain valid even after the
     *   caller has deallocated the original string.
     */
    char *strbuf;

    /* length of the string currently in the buffer */
    size_t curlen;

    /* size of the buffer allocated to strbuf */
    size_t strbufsiz;
} re_context;


/* ------------------------------------------------------------------------ */
/*
 *   Status codes
 */
typedef enum
{
    /* success */
    RE_STATUS_SUCCESS = 0,

    /* compilation error - group nesting too deep */
    RE_STATUS_GROUP_NESTING_TOO_DEEP
} re_status_t;


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the context.  The memory for the context structure itself
 *   must be allocated and maintained by the caller.
 */
void re_init(re_context *ctx, errcxdef *errctx);

/*
 *   Delete the context - frees structures associated with the context.
 *   Does NOT free the memory used by the context structure itself.
 */
void re_delete(re_context *ctx);

/*
 *   Compile an expression and search for a match within the given string.
 *   Returns the offset of the match, or -1 if no match was found.
 */
int re_compile_and_search(re_context *ctx,
                          const char *pattern, size_t patlen,
                          const char *searchstr, size_t searchlen,
                          int *result_len);

/*
 *   Compile an expression and check for a match.  Returns the length of
 *   the match if we found a match, -1 if we found no match.  This is not
 *   a search function; we merely match the leading substring of the given
 *   string to the given pattern.
 */
int re_compile_and_match(re_context *ctx,
                         const char *pattern, size_t patlen,
                         const char *searchstr, size_t searchlen);

#endif /* REGEX_H */

