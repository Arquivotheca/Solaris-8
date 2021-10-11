/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)colltbl.h	1.6	92/07/14 SMI"	/* SVr4.0 1.1	*/

/* Diagnostic mnemonics */
#define	WARNING		0
#define	ERROR		1
#define	ABORT		2

#define	SZ_COLLATE	256
#define	SUBFLG		0x80
#define	ORD_LST		1
#define	PAR_LST		2
#define	BRK_LST		3

enum errtype {
	GEN_ERR,
	DUPLICATE,
	EXPECTED,
	ILLEGAL,
	TOO_LONG,
	INSERTED,
	NOT_FOUND,
	NOT_DEFINED,
	TOO_MANY,
	INVALID,
	BAD_OPEN,
	NO_SPACE,
	NEWLINE,
	REGERR,
	CWARN,
	YYERR,
	PRERR
};

/* Diagnostics Functions and Subroutines */
void	error();
void	regerr();
void	usage();




/* The following definitions should move back to collfcns.c
 * if utils.c is merged int collfcns.c
 */

/* entry in the 1_to_1 collation table */
typedef struct collnd {
	int		index;
	unsigned	char		pwt;	/* primary weight */
	unsigned	char		swt;	/* secondary weight */
	} collnd;

/* node type of 2_to_1 collation element */
typedef struct c2to1_nd {
	char		c[2];	/* two characters */
	unsigned char	pwt;
	unsigned char	swt;
	struct c2to1_nd *next;
	}	c2to1_nd;


/* node type of m_to_m substitution node */
typedef struct smtom_nd {
	char		*exp;	/* expression to be replaced */
	char		*repl;	/* replacement string */
			/* it also used as prim order string in setdb() */
	char		*sec;	/*  2nd order string */
	struct smtom_nd	*next;
	} smtom_nd;

/* entry in the 1_to_m substitution table */

typedef struct s1tom_nd {
	int	index;
	char	*repl;		/* see smtom_nd */
	char	*sec;
	}	s1tom_nd;
