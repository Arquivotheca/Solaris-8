/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)collfcns.c	1.7	92/07/14 SMI"	/* SVr4.0 1.1.1.2	*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include "colltbl.h"


#ifdef REGEXP
#define	INIT		register char *sp = instring;
#define	GETC()		(*sp++)
#define	PEEKC()		(*sp)
#define	UNGETC(c)	(--sp)
#define	RETURN(c)	return (c)
#define	ERROR(c)	{ regerr (c); return ((char *) NULL); }
#include <regexp.h>
#endif

extern int	regexp_flag;

/* prim and secondary weights start from 02 */
int	curprim = 1;		/* magic number for primary ordering */
int	cursec = 1;		/* magic number for secondary ordering */

collnd	colltbl[SZ_COLLATE];	/* 1_to_1 collation table */
c2to1_nd	*ptr2to1 = NULL;	/* pointer of 2_to_1 list */
s1tom_nd	s1tomtab[SZ_COLLATE];	/* substitution table (1 to m) */
smtom_nd	*ptrsmtom = NULL;	/* pointer to m_to_m subst. list */

int	smtomflag = 0;
int	s1tomflag = 0;
int	c2to1flag = 0;
int	c1to1flag = 0;
int	secflag   = 0;


void
init()
{
	int	i;

	for (i=0; i<SZ_COLLATE; i++) {
		colltbl[i].index = colltbl[i].pwt = 0;
		colltbl[i].swt = 1;
		s1tomtab[i].index = 0;
	}
}


void
mkord(sym, type)
unsigned char *sym;
int type;
{
	int		prim, sec;
	c2to1_nd	*cptr1, *cptr2, *cptr3;
	int		i;
	unsigned char	uc;

	switch (type) {
		case ORD_LST:
			prim = ++curprim;
			sec = 1;
			break;
		case PAR_LST:
			prim = curprim;
			sec = cursec++;
			break;
		case BRK_LST:
			prim = curprim;
			sec = 1;
			break;
	}
	if (sec > 1 && sec > secflag)
		secflag = sec;
	if (sym[1] == '\0') { /* 1_to_1 collation */
		if (colltbl[*sym].pwt != 0)
			error(DUPLICATE, "symbol", sym);
		colltbl[*sym].pwt = prim;
		colltbl[*sym].swt = sec;
		c1to1flag = 1;
	} else {
		for (cptr1=ptr2to1; cptr1 != NULL; cptr1= cptr1->next) {
			uc = (unsigned char) cptr1->c[0];
			i = sym[0] - uc;
			if (i < 0)
				break;
			else if (i == 0) {
				if (cptr1->c[1] == sym[1]) {
					error(DUPLICATE, "symbol", sym);
					return;
				}
			}
			cptr2=cptr1;
		}

		cptr3 = (c2to1_nd *) malloc(sizeof (c2to1_nd));
		cptr3->c[0] = sym[0];
		cptr3->c[1] = sym[1];
		cptr3->pwt = prim;
		cptr3->swt = sec;

		if (cptr1)
			cptr3->next = cptr1;
		else
			cptr3->next = NULL;
		if (cptr1 == ptr2to1)
			ptr2to1 = cptr3;
		else
			cptr2->next = cptr3;
		c2to1flag = 1;
	}
}


void
substitute(exp, repl)
char *exp;
char *repl;
{
	smtom_nd	*psubnd, *ptr1, *ptr2, *ptr3;
	int		i;
	unsigned int	ui;

	if (*exp == '\0') {
		error(ILLEGAL, "substitution statement",
			"try to substitute a null string with another string");
		return;
	}

	if (exp[1] == '\0') { /* one to many substi */
		ui = (unsigned char) *exp;
		if (s1tomtab[ui].repl) {
			error(ILLEGAL, "substitution statement",
			"substitute a string with two different strings");
			return;
		}
		s1tomtab[ui].repl = repl;
		s1tomflag = 1;
		return;
	}
	if ((psubnd = (smtom_nd *) malloc(sizeof (smtom_nd))) == NULL) {
		fprintf(stderr, "Out of space\n");
		exit(-1);
	}
	psubnd->exp  = exp;
	psubnd->repl = repl;

	if (ptrsmtom == NULL) {
		psubnd->next = NULL;
		ptrsmtom = psubnd;
	} else {
		for (ptr1=ptrsmtom; ptr1!= NULL; ptr1=ptr1->next) {
			i=strcmp(exp, ptr1->exp);
			if (i > 0)
				break;
			else if (i==0) {
				error(ILLEGAL, "substitution statement",
			"substitute a string with two different strings");
				free(psubnd->exp);
				free(psubnd->repl);
				free(psubnd);
				return;
			}
		ptr2 = ptr1;
		}
		if (ptr1 == NULL) {
			ptr2->next = psubnd;
			psubnd->next = NULL;
		} else {
			psubnd->next=ptr1;
			if (ptr1 == ptrsmtom)
				ptrsmtom = psubnd;
			else
				ptr2->next = psubnd;
		}
	}
	smtomflag = 1;
}
