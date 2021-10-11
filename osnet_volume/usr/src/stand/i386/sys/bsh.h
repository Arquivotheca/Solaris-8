/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _I386_SYS_BSH_H
#define	_I386_SYS_BSH_H

#pragma ident	"@(#)bsh.h	1.4	96/01/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* boot interpreter (shell) include file */

#define	NULL	0

#define	ARGSIZ	64	/* maximum number of args to a command */

struct arg {	/* argument info */
	int	argc;
	char	*argv[ARGSIZ];
	int	lenv[ARGSIZ];
};


struct cmd {
	char	*name;
	void	(*func)();
};

extern struct cmd cmd[];	/* command table */

struct var {	/* variable list element */
	struct var *next;	/* pointer to next element */
	int	namsiz;		/* size of name string */
	int	valsiz;		/* size of value string */
	/* name and value strings follow */
};

/* var_ops operaration codes */
#define	FIND_VAR	0
#define	SET_VAR		1
#define	UNSET_VAR	2


/* command source limits */
#define	LINBUFSIZ	256	/* max size of a source line */
#define	WORDSIZ		256	/* max size of a source word */
#define	VARSIZ		256	/* max size of a variable name */
#define	SRCSIZ		32	/* max number of nested sources */
#define	FILSIZ		0x1000	/* max size of a source file in bytes */

struct src {	/* command source struct */
	int	type;			/* type of source */
	int	bufsiz;			/* size of buffer */
	unsigned char *buf;		/* address of buffer */
	unsigned char *nextchar;	/* current point in buffer */
	unsigned int pushedchars;	/* pushed back chars */
};

/* source types */
#define	SRC_CONSOLE	0
#define	SRC_VARIABLE	1
#define	SRC_FILE	2


struct if_tbl {		/* if command nesting structure */
	char	is_true;	/* current block is true */
	char	was_true;	/* a previous block was true */
	short	count;		/* count of non-executed embedded ifs */
};

#define	IFTBLSIZ	32	/* max number of nested ifs */

#ifdef	__cplusplus
}
#endif

#endif	/* _I386_SYS_BSH_H */
