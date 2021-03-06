/*
$Header: d:/cvsroot/tads/TADS2/BIF.H,v 1.3 1999/07/11 00:46:29 MJRoberts Exp $
*/

/*
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  bif.h - built-in functions interface
Function
  interface to run-time intrinsic function implementation
Notes
  None
Modified
  12/16/92 MJRoberts     - add TADS/Graphic functions
  12/26/91 MJRoberts     - creation
*/

#ifndef BIF_INCLUDED
#define BIF_INCLUDED

#ifndef OS_INCLUDED
# include "os.h"
#endif
#ifndef ERR_INCLUDED
# include "err.h"
#endif
#ifndef RUN_INCLUDED
# include "run.h"
#endif
#ifndef TIO_INCLUDED
# include "tio.h"
#endif
#ifndef REGEX_H
#include "regex.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* forward definitions */
struct vocidef;
struct voccxdef;

/* maximum number of file handles available */
#define BIFFILMAX  10


/* file contexts for the built-in file handling functions */
struct biffildef
{
    osfildef *fp;                          /* underyling system file handle */
    uint      flags;                                               /* flags */
#define BIFFIL_F_BINARY    0x01                           /* file is binary */
};
typedef struct biffildef biffildef;

/* built-in execution context */
struct bifcxdef
{
    errcxdef *bifcxerr;                           /* error-handling context */
    runcxdef *bifcxrun;                           /* code execution context */
    tiocxdef *bifcxtio;                                 /* text I/O context */
    long      bifcxrnd;                               /* random number seed */
    int       bifcxseed1;                   /* first seed for new generator */
    int       bifcxseed2;                  /* second seed for new generator */
    int       bifcxseed3;                   /* third seed for new generator */
    int       bifcxrndset;                   /* randomize() has been called */
    biffildef bifcxfile[BIFFILMAX];          /* file handles for fopen, etc */
    int       bifcxsafetyr;                 /* file I/O safety level - read */
    int       bifcxsafetyw;                /* file I/O safety level - write */
    char     *bifcxsavext;        /* saved game extension (null by default) */
    struct appctxdef *bifcxappctx;              /* host application context */
    re_context bifcxregex;          /* regular expression searching context */
};
typedef struct bifcxdef bifcxdef;

/*
 *   argument list checking routines - can be disabled for faster
 *   run-time
 */

/* check for proper number of arguments */
/* void bifcntargs(bifcxdef *ctx, int argcnt) */

/* check that next argument has proper type */
/* void bifchkarg(bifcxdef *ctx, dattyp typ); */

#ifdef RUNFAST
# define bifcntargs(ctx, parmcnt, argcnt)
# define bifchkarg(ctx, typ)
#else /* RUNFAST */
# define bifcntargs(ctx, parmcnt, argcnt) \
  (parmcnt == argcnt ? DISCARD 0 : \
   (runsig(ctx->bifcxrun, ERR_BIFARGC), DISCARD 0))
# define bifchkarg(ctx, typ) \
  (runtostyp(ctx->bifcxrun) == typ ? DISCARD 0 : \
   (runsig(ctx->bifcxrun, ERR_INVTBIF), DISCARD 0))
#endif /* RUNFAST */

/* determine if one object is a subclass of another */
int bifinh(struct voccxdef *voc, struct vocidef *v, objnum cls);


/* enumerate the built-in functions */
void bifyon(bifcxdef *ctx, int argc);                   /* yorn - yes or no */
void bifsfs(bifcxdef *ctx, int argc);                            /* setfuse */
void bifrfs(bifcxdef *ctx, int argc);                            /* remfuse */
void bifsdm(bifcxdef *ctx, int argc);                          /* setdaemon */
void bifrdm(bifcxdef *ctx, int argc);                          /* remdaemon */
void bifinc(bifcxdef *ctx, int argc);                            /* incturn */
void bifqui(bifcxdef *ctx, int argc);                               /* quit */
void bifsav(bifcxdef *ctx, int argc);                               /* save */
void bifrso(bifcxdef *ctx, int argc);                            /* restore */
void biflog(bifcxdef *ctx, int argc);                            /* logging */
void bifres(bifcxdef *ctx, int argc);                            /* restart */
void bifinp(bifcxdef *ctx, int argc);           /* input - get line from kb */
void bifnfy(bifcxdef *ctx, int argc);                             /* notify */
void bifunn(bifcxdef *ctx, int argc);                           /* unnotify */
void biftrc(bifcxdef *ctx, int argc);                       /* trace on/off */
void bifsay(bifcxdef *ctx, int argc);                                /* say */
void bifcar(bifcxdef *ctx, int argc);                                /* car */
void bifcdr(bifcxdef *ctx, int argc);                                /* cdr */
void bifcap(bifcxdef *ctx, int argc);                               /* caps */
void biflen(bifcxdef *ctx, int argc);                             /* length */
void biffnd(bifcxdef *ctx, int argc);                               /* find */
void bifsit(bifcxdef *ctx, int argc);           /* setit - set current 'it' */
void bifsrn(bifcxdef *ctx, int argc);               /* randomize: seed rand */
void bifrnd(bifcxdef *ctx, int argc);         /* rand - get a random number */
void bifask(bifcxdef *ctx, int argc);                            /* askfile */
void bifssc(bifcxdef *ctx, int argc);                           /* setscore */
void bifsub(bifcxdef *ctx, int argc);                             /* substr */
void bifcvs(bifcxdef *ctx, int argc);          /* cvtstr: convert to string */
void bifcvn(bifcxdef *ctx, int argc);          /* cvtnum: convert to number */
void bifupr(bifcxdef *ctx, int argc);                              /* upper */
void biflwr(bifcxdef *ctx, int argc);                              /* lower */
void biffob(bifcxdef *ctx, int argc);                           /* firstobj */
void bifnob(bifcxdef *ctx, int argc);                            /* nextobj */
void bifsvn(bifcxdef *ctx, int argc);                         /* setversion */
void bifarg(bifcxdef *ctx, int argc);                             /* getarg */
void biftyp(bifcxdef *ctx, int argc);                           /* datatype */
void bifisc(bifcxdef *ctx, int argc);                            /* isclass */
void bifund(bifcxdef *ctx, int argc);                               /* undo */
void bifdef(bifcxdef *ctx, int argc);                            /* defined */
void bifpty(bifcxdef *ctx, int argc);                           /* proptype */
void bifoph(bifcxdef *ctx, int argc);                            /* outhide */
void bifgfu(bifcxdef *ctx, int argc);                            /* getfuse */
void bifruf(bifcxdef *ctx, int argc);                           /* runfuses */
void bifrud(bifcxdef *ctx, int argc);                         /* rundaemons */
void biftim(bifcxdef *ctx, int argc);                            /* gettime */
void bifsct(bifcxdef *ctx, int argc);                          /* intersect */
void bifink(bifcxdef *ctx, int argc);                           /* inputkey */
void bifwrd(bifcxdef *ctx, int argc);                           /* objwords */
void bifadw(bifcxdef *ctx, int argc);                            /* addword */
void bifdlw(bifcxdef *ctx, int argc);                            /* delword */
void bifgtw(bifcxdef *ctx, int argc);                           /* getwords */
void bifnoc(bifcxdef *ctx, int argc);                             /* nocaps */
void bifskt(bifcxdef *ctx, int argc);                           /* skipturn */
void bifcls(bifcxdef *ctx, int argc);                        /* clearscreen */
void bif1sc(bifcxdef *ctx, int argc);                            /* firstsc */
void bifvin(bifcxdef *ctx, int argc);                           /* verbinfo */
void bifcapture(bifcxdef *ctx, int argc);                     /* outcapture */

void biffopen(bifcxdef *ctx, int argc);                            /* fopen */
void biffclose(bifcxdef *ctx, int argc);                          /* fclose */
void biffwrite(bifcxdef *ctx, int argc);                          /* fwrite */
void biffread(bifcxdef *ctx, int argc);                            /* fread */
void biffseek(bifcxdef *ctx, int argc);                            /* fseek */
void biffseekeof(bifcxdef *ctx, int argc);                      /* fseekeof */
void bifftell(bifcxdef *ctx, int argc);                            /* ftell */

void bifsysinfo(bifcxdef *ctx, int argc);                     /* systemInfo */
void bifmore(bifcxdef *ctx, int argc);                        /* morePrompt */
void bifsetme(bifcxdef *ctx, int argc);                      /* parserSetMe */
void bifgetme(bifcxdef *ctx, int argc);                      /* parserGetMe */

void bifresearch(bifcxdef *ctx, int argc);                      /* reSearch */
void bifregroup(bifcxdef *ctx, int argc);                     /* reGetGroup */

void bifinpevt(bifcxdef *ctx, int argc);                      /* inputevent */
void bifdelay(bifcxdef *ctx, int argc);                        /* timeDelay */

void bifsetoutfilter(bifcxdef *ctx, int argc);           /* setOutputFilter */
void bifexec(bifcxdef *ctx, int argc);                       /* execCommand */
void bifgetobj(bifcxdef *ctx, int argc);                    /* parserGetObj */
void bifparsenl(bifcxdef *ctx, int argc);            /* parserParseNounList */
void bifprstok(bifcxdef *ctx, int argc);                  /* parserTokenize */
void bifprstoktyp(bifcxdef *ctx, int argc);            /* parserGetTokTypes */
void bifprsdict(bifcxdef *ctx, int argc);               /* parserDictLookup */
void bifprsrslv(bifcxdef *ctx, int argc);           /* parserResolveObjects */
void bifprsrplcmd(bifcxdef *ctx, int argc);         /* parserReplaceCommand */
void bifexitobj(bifcxdef *ctx, int argc);                        /* exitobj */
void bifinpdlg(bifcxdef *ctx, int argc);                     /* inputdialog */
void bifresexists(bifcxdef *ctx, int argc);               /* resourceExists */

/*
 *   TADS/graphic functions - these are present in the text system, but
 *   don't do anything.
 */
void bifgrp(bifcxdef *ctx, int argc);            /* g_readpic: read picture */
void bifgsp(bifcxdef *ctx, int argc);            /* g_showpic: show picture */
void bifgsh(bifcxdef *ctx, int argc);             /* g_sethot: set hot list */
void bifgin(bifcxdef *ctx, int argc);                        /* g_inventory */
void bifgco(bifcxdef *ctx, int argc);                          /* g_compass */
void bifgov(bifcxdef *ctx, int argc);                          /* g_overlay */
void bifgmd(bifcxdef *ctx, int argc);                             /* g_mode */
void bifgmu(bifcxdef *ctx, int argc);                            /* g_music */
void bifgpa(bifcxdef *ctx, int argc);                            /* g_pause */
void bifgef(bifcxdef *ctx, int argc);                           /* g_effect */
void bifgsn(bifcxdef *ctx, int argc);                            /* g_sound */

#ifdef __cplusplus
}
#endif

#endif /* BIF_INCLUDED */
