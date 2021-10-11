/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)genmsg.h	5.4 96/05/16"

/*
 * Boolean values.
 */
#define	TRUE	1
#define	FALSE	0

/*
 * Default quote character for a message file.
 */
#define	QUOTE	'"'

/*
 * Number to be updated by the auto message numbering.
 */
#define	NOMSGID		-1

/*
 * Just in case...
 */
#ifndef MAXPATHLEN
#define	MAXPATHLEN	1024
#endif
#ifndef LINE_MAX
#define	LINE_MAX	2048
#endif
#ifndef NL_MSGMAX
#define	NL_MSGMAX	32767
#endif
#ifndef NL_SETMAX
#define	NL_SETMAX	255
#endif
#ifndef NL_TEXTMAX
#define	NL_TEXTMAX	2048
#endif

/*
 * Genmsg action mode is for genmsg to identify its tasks.
 */
#define	IsActiveMode(mode)	(active_mode & (mode))
#define	SetActiveMode(mode)	(active_mode |= (mode))
#define	ResetActiveMode(mode)	(active_mode &= ~(mode))

typedef long Mode;

#define	NoMode		(0L)		/* internal-mode */
#define	ReplaceMode	(1L<<0)		/* internal-mode */
#define	MessageMode	(1L<<1)		/* -o */
#define	AppendMode	(1L<<2)		/* -a */
#define	AutoNumMode	(1L<<3)		/* -l projfile */
#define	ReverseMode	(1L<<4)		/* -r */
#define	OverwriteMode	(1L<<5)		/* -f */
#define	ProjectMode	(1L<<6)		/* -g new-projfile */
#define	MsgCommentMode	(1L<<7)		/* -c comment-tag */
#define	SetCommentMode	(1L<<8)		/* -c comment-tag */
#define	BackCommentMode (1L<<9)		/* -b */
#define	LineInfoMode	(1L<<10)	/* -n */
#define	PrefixMode	(1L<<11)	/* -m prefix */
#define	SuffixMode	(1L<<12)	/* -M suffix */
#define	TripleMode	(1L<<13)	/* -t */
#define	DoubleLineMode	(1L<<14)	/* -d */
#define	PreProcessMode	(1L<<15)	/* -p cpp-path */
#define	NoErrorMode	(1L<<16)	/* -x */

extern Mode active_mode;


extern char *srcfile;		/* from main.c */
extern FILE *newfp;		/* from main.c */

extern void prg_err(char *fmt, ...);
extern void src_err(char *file, int line, char *fmt, ...);
