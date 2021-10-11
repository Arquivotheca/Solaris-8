/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)ldefs.c	6.11	95/02/11 SMI"

# include <stdio.h>
#ifdef __STDC__
# include <stdlib.h>
#endif

# include <widec.h>
# include <wctype.h>
# include "sgs.h"
# define CHR wchar_t
# define BYTE char
# define Boolean char
# define LONG_WCHAR_T 1

# define PP 1
# ifdef u370
# define CWIDTH 8
# define CMASK 0377
# define ASCII 1
# else

# ifdef unix
# define CWIDTH 7
# define CMASK 0177
# define ASCII 1
# endif

# ifdef gcos
# define CWIDTH 9
# define CMASK 0777
# define ASCII 1
# endif

# ifdef ibm
# define CWIDTH 8
# define CMASK 0377
# define EBCDIC 1
# endif
# endif

# define NCH 256
# define TOKENSIZE 10000
# define DEFSIZE 1000
# define DEFCHAR 2000
# define BUF_SIZ 2000
# define STARTCHAR 2560
# define STARTSIZE 256
# define CCLSIZE 20000


# ifdef SMALL
# define TREESIZE 600
# define NTRANS 1500
# define NSTATES 300
# define MAXPOS 1500
# define MAXPOSSTATE 500
# define NOUTPUT 1500
# endif

# ifndef SMALL
# define TREESIZE 1000
# define NSTATES 500
# define MAXPOS 2500
# define MAXPOSSTATE 1000
# define NTRANS 2000
# define NOUTPUT 3000
# endif
# define NACTIONS 1000
# define ALITTLEEXTRA 300

# define RCCL		0x4000
# define RNCCL		0x4001
# define RSTR		0x4002
# define RSCON		0x4003
/* XCU4: add RXSCON */
# define RXSCON		0x4011
# define RNEWE		0x4004
# define FINAL		0x4005
# define RNULLS		0x4006
# define RCAT		0x4007
# define STAR		0x4008
# define PLUS		0x4009
# define QUEST		0x400a
# define DIV		0x400b
# define BAR		0x400c
# define CARAT		0x400d
# define S1FINAL	0x400e
# define S2FINAL	0x400f
# define DOT		0x4010
# define ISOPERATOR(n)	((n&0xc080)==0x4000)
# define RANGE		0x40ff /* New to JLE; this is not really a node tag.
				  This is used in a string pointed to by
				  the leaf of an RCCL or RNCCL node as a
				  special prefix code that substitutes
				  the infix '-' range operator.  For
				  example, a lex character class "[_0-9a-zA-Z]"
				  would be translated to the intermidiate
				  form:
				  		RCCL
						 |
						 |
						 v
					      "_<RANGE>09<RANGE>a-z<RANGE>A-Z"
			   */

# define MAXNCG 1000
  extern int     ncgidtbl;
  extern int     ncg; /* ncg == ncgidtbl * 2 */
  typedef unsigned long          lchar;
  extern lchar yycgidtbl[];
  extern int     yycgid(/* wchar_t */);
  extern Boolean handleeuc; /* TRUE iff -w or -e option is specified. */       
  extern Boolean widecio; /* TRUE iff -w option is specified. */

# define DEFSECTION 1
# define RULESECTION 2
# define ENDSECTION 5

# define PC 1
# define PS 1

# ifdef DEBUG
# define LINESIZE 110
extern int yydebug;
extern int debug;		/* 1 = on */
extern int charc;
# endif

# ifndef DEBUG
# define freturn(s) s
# endif


extern int optind;
extern int no_input;
extern int sargc;
extern char **sargv;
extern char *v_stmp;
extern char *release_string;
extern CHR buf[];
extern int ratfor;		/* 1 = ratfor, 0 = C */
extern int fatal;
extern int n_error;
extern int copy_line;
extern int yyline;		/* line number of file */
extern int sect;
extern int eof;
extern int lgatflg;
extern int divflg;
extern int funcflag;
extern int pflag;
extern int casecount;
extern int chset;	/* 1 = CHR set modified */
extern FILE *fin, *fout, *fother, *errorf;
extern int fptr;
extern char *ratname, *cname;
extern int prev;	/* previous input character */
extern int pres;	/* present input character */
extern int peek;	/* next input character */
extern int *name;
extern int *left;
extern int *right;
extern int *parent;
extern Boolean *nullstr;
extern int tptr;
extern CHR pushc[TOKENSIZE];
extern CHR *pushptr;
extern CHR slist[STARTSIZE];
extern CHR *slptr;
extern CHR **def, **subs, *dchar;
extern CHR **sname, *schar;
/* XCU4: %x exclusive start */
extern int *exclusive;
extern CHR *ccl;
extern CHR *ccptr;
extern CHR *dp, *sp;
extern int dptr, sptr;
extern CHR *bptr;		/* store input position */
extern CHR *tmpstat;
extern int count;
extern int **foll;
extern int *nxtpos;
extern int *positions;
extern int *gotof;
extern int *nexts;
extern CHR *nchar;
extern int **state;
extern int *sfall;		/* fallback state num */
extern Boolean *cpackflg;		/* true if state has been character packed */
extern int *atable, aptr;
extern int nptr;
extern Boolean symbol[MAXNCG];
extern CHR cindex[MAXNCG];
extern int xstate;
extern int stnum;
extern int ctable[];
extern int ZCH;
extern int ccount;
extern CHR match[MAXNCG];
extern BYTE extra[];
extern CHR *pcptr, *pchar;
extern int pchlen;
extern int nstates, maxpos;
extern int yytop;
extern int report;
extern int ntrans, treesize, outsize;
extern long rcount;
extern int optim;
extern int *verify, *advance, *stoff;
extern int scon;
extern CHR *psave;
extern CHR *getl();
#ifdef __STDC__
extern BYTE *myalloc();
#else
extern BYTE *calloc(), *myalloc();
extern void exit();
extern void cfree();
#endif
extern int buserr(), segviol();
extern int error_tail();

extern int isArray;		/* XCU4: for %array %pointer */
