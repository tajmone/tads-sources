#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/*
 *   Copyright (c) 2011 by Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  tadshtml2.cpp - tadshtml.cpp extensions specific to TADS 2 builds
Function
  In debugging builds, HTML TADS defines its own global 'operator new'
  and 'operator delete', which do extra book-keeping that lets us detect
  leaks, double frees, and out-of-bound writes.  The complication is that
  TADS 3 has its own equivalents, and since the operator new/delete
  overloads are global, this creates a link conflict.  When linking
  with TADS 3, we want to use the TADS 3 versions of these instead of
  the HTML TADS versions, because as of 3.1 the TADS versions are
  thread-safe, and TADS 3.1 is multi-threaded.  So we want to simply
  leave out the HTML TADS operator new/delete definitions when linking
  with TADS 3.  But TADS 2, being purely C, doesn't define these C++
  operators, so we still need to provide the HTML TADS versions when
  linking with TADS 2 only.  This file implements this link-time
  selection: include this file in debug builds that link TADS 2 only,
  and exclude this file in debug builds that link TADS 3.  This file
  isn't needed in release builds of any kind, since we only overload
  operators new/delete in debug builds.
Notes

Modified
  05/11/11 MJRoberts  - Creation
*/

#include <stdio.h>
#include <memory.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef HTML_OS_H
#include "html_os.h"
#endif

#ifdef TADSHTML_DEBUG
#include <stdlib.h>
#include <malloc.h>
#include <windows.h>

void *operator new(size_t siz) SYSTHROW(throw (std::bad_alloc))
{
    return th_malloc(siz);
}

void operator delete(void *ptr) SYSTHROW(throw ())
{
    th_free(ptr);
}

void *operator new[](size_t siz) SYSTHROW(throw (std::bad_alloc))
{
    return th_malloc(siz);
}

void operator delete[](void *ptr) SYSTHROW(throw ())
{
    th_free(ptr);
}

/* system specific memory prefix settings for Windows */
#ifdef T_WIN32
#define OS_MEM_PREFIX struct { DWORD return_addr; } stk[6];
#define OS_MEM_PREFIX_FMT ", return=(%lx, %lx, %lx, %lx, %lx, %lx)"
# define OS_MEM_PREFIX_FMT_VARS(mem) \
    , (mem)->stk[0].return_addr, \
    (mem)->stk[1].return_addr, \
    (mem)->stk[2].return_addr, \
    (mem)->stk[3].return_addr, \
    (mem)->stk[4].return_addr, \
    (mem)->stk[5].return_addr
static void os_mem_prefix_set(struct hmem_prefix_t *mem);
#endif /* T_WIN32 */

/* if we didn't define a system-specific memory prefix, there isn't one */
#ifndef OS_MEM_PREFIX
#define OS_MEM_PREFIX
# define OS_MEM_PREFIX_FMT
# define OS_MEM_PREFIX_FMT_VARS(mem)
#define os_mem_prefix_set(mem)
#endif

/*
 *   memory block prefix - each block we allocate has this prefix attached
 *   just before the pointer that we return to the program
 */
struct hmem_prefix_t
{
    long id;
    size_t siz;
    hmem_prefix_t *nxt;
    hmem_prefix_t *prv;
    OS_MEM_PREFIX
        char guard[10];
};
struct mem_postfix_t
{
    char guard[10];
};
static const unsigned char guard_bytes1[] = {
    0xA1, 0x2B, 0xC3, 0x4D, 0xE5, 0xF6, 0x7A, 0xB8, 0x9C, 0xD0
};
static const unsigned char guard_bytes2[] = {
    0xF1, 0x2E, 0xD3, 0x4C, 0xB5, 0x6A, 0xF7, 0x8E, 0xD9, 0x0C
};

/* head and tail of memory allocation linked list */
static hmem_prefix_t *mem_head = 0;
static hmem_prefix_t *mem_tail = 0;

/*
 *   Check the integrity of the heap: traverse the entire list, and make sure
 *   the forward and backward pointers match up.
 */
static void th_check_heap()
{
    hmem_prefix_t *p;

    /* scan from the front */
    for (p = mem_head ; p != 0 ; p = p->nxt)
    {
        /*
         *   If there's a backwards pointer, make sure it matches up.  If
         *   there's no backwards pointer, make sure we're at the head of the
         *   list.  If this is the end of the list, make sure it matches the
         *   tail pointer.
         */
        if ((p->prv != 0 && p->prv->nxt != p)
            || (p->prv == 0 && p != mem_head)
            || (p->nxt == 0 && p != mem_tail))
            oshtml_dbg_printf("\n--- heap corrupted ---\n");
    }
}

/*
 *   Allocate a block, storing it in a doubly-linked list of blocks and
 *   giving the block a unique ID.
 */
void *th_malloc(size_t siz)
{
    static long id;
    static int check = 0;

    hmem_prefix_t *mem = (hmem_prefix_t *)malloc(
        siz + sizeof(hmem_prefix_t) + sizeof(mem_postfix_t));
    mem->id = id++;
    mem->siz = siz;
    mem->prv = mem_tail;
    mem->nxt = 0;
    if (mem_tail != 0)
        mem_tail->nxt = mem;
    else
        mem_head = mem;
    mem_tail = mem;

    os_mem_prefix_set(mem);
    memcpy(mem->guard, guard_bytes1, sizeof(mem->guard));

    mem_postfix_t *postfix = (mem_postfix_t *)((char *)(mem + 1) + siz);
    memcpy(postfix->guard, guard_bytes2, sizeof(postfix->guard));

    if (check)
        th_check_heap();

    return (void *)(mem + 1);
}

static void check_block(void *p)
{
    /* check the prefix guard bytes */
    hmem_prefix_t *mem = ((hmem_prefix_t *)p) - 1;
    if (memcmp(mem->guard, guard_bytes1, sizeof(mem->guard)) != 0)
        oshtml_dbg_printf("\n--- corrupted memory prefix %lx ---\n", (long)p);

    /* check the postfix guard bytes */
    mem_postfix_t *post = (mem_postfix_t *)((char *)(mem + 1) + mem->siz);
    if (memcmp(post->guard, guard_bytes2, sizeof(post->guard)) != 0)
        oshtml_dbg_printf("\n--- corrupted memory postfix %lx ---\n", (long)p);
}

/*
 *   reallocate a block - to simplify, we'll allocate a new block, copy the
 *   old block up to the smaller of the two block sizes, and delete the old
 *   block
 */
void *th_realloc(void *oldptr, size_t newsiz)
{
    void *newptr;
    size_t oldsiz;

    /* check for writes outside the block's boundaries */
    check_block(oldptr);

    /* allocate a new block */
    newptr = th_malloc(newsiz);

    /* copy the old block into the new block */
    oldsiz = (((hmem_prefix_t *)oldptr) - 1)->siz;
    memcpy(newptr, oldptr, (oldsiz <= newsiz ? oldsiz : newsiz));

    /* free the old block */
    th_free(oldptr);

    /* return the new block */
    return newptr;
}


/* free a block, removing it from the allocation block list */
void th_free(void *ptr)
{
    /* statics for debugging */
    static int check = 0;
    static int double_check = 0;
    static int check_heap = 0;
    static long ckblk[] = { 0xD9D9D9D9, 0xD9D9D9D9, 0xD9D9D9D9 };

    /* check the integrity of the entire heap if desired */
    if (check_heap)
        th_check_heap();

    /* if freeing a null pointer, do nothing */
    if (ptr == 0)
        return;

    /* check the block */
    check_block(ptr);

    /* get the prefix pointer */
    hmem_prefix_t *mem = ((hmem_prefix_t *)ptr) - 1;

    /* check for a pre-freed block */
    if (memcmp(mem, ckblk, sizeof(ckblk)) == 0)
    {
        oshtml_dbg_printf("\n--- memory block freed twice ---\n");
        return;
    }

    /* if desired, check to make sure the block is in our list */
    if (check)
    {
        hmem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p == 0)
            oshtml_dbg_printf("\n--- memory block not found in th_free ---\n");
    }

    /* unlink the block from the list */
    if (mem->prv != 0)
        mem->prv->nxt = mem->nxt;
    else
        mem_head = mem->nxt;

    if (mem->nxt != 0)
        mem->nxt->prv = mem->prv;
    else
        mem_tail = mem->prv;

    /*
     *   if we're being really cautious, check to make sure the block is no
     *   longer in the list
     */
    if (double_check)
    {
        hmem_prefix_t *p;
        for (p = mem_head ; p != 0 ; p = p->nxt)
        {
            if (p == mem)
                break;
        }
        if (p != 0)
            oshtml_dbg_printf("\n--- memory block still in list after "
                              "th_free ---\n");
    }

    /* make it obvious that the memory is invalid */
    memset(mem, 0xD9, mem->siz + sizeof(hmem_prefix_t));

    /* free the memory with the system allocator */
    free((void *)mem);
}

/*
 *   Diagnostic routine to display the current state of the heap.  This can
 *   be called just before program exit to display any memory blocks that
 *   haven't been deleted yet; any block that is still in use just before
 *   program exit is a leaked block, so this function can be useful to help
 *   identify and remove memory leaks.
 */
void th_list_memory_blocks()
{
    hmem_prefix_t *mem;
    int cnt;
    char buf[128];

    /* display introductory message */
    os_dbg_sys_msg("\n(HTMLTADS) Memory blocks still in use:\n");

    /* display the list of undeleted memory blocks */
    for (mem = mem_head, cnt = 0 ; mem ; mem = mem->nxt, ++cnt)
    {
        sprintf(buf, "  addr=%lx, id=%ld, siz=%lu" OS_MEM_PREFIX_FMT "\n",
                (long)(mem + 1), mem->id, (unsigned long)mem->siz
                OS_MEM_PREFIX_FMT_VARS(mem));
        os_dbg_sys_msg(buf);
    }

    /* display totals */
    sprintf(buf, "\nTotal blocks in use: %d\n", cnt);
    os_dbg_sys_msg(buf);
}

/* windows-specific memory prefix header setup */
static void os_mem_prefix_set(struct hmem_prefix_t *mem)
{
    /*
     *   Trace back the call stack by following the BP chain, storing the
     *   return addresses in the memory prefix structure.  Skip our own
     *   return address since that's just the allocator routine itself, which
     *   is a given and thus isn't useful information to store.
     */
    DWORD bp_;
    __asm mov bp_, ebp;
    bp_ = *(DWORD *)bp_;
    for (size_t i = 0 ; i < countof(mem->stk) && bp_ < *(DWORD *)bp_ ;
         ++i, bp_ = *(DWORD *)bp_)
        mem->stk[i].return_addr = ((DWORD *)bp_)[1];
}

#endif /* TADSHTML_DEBUG */
