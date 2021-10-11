/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)reach.c	1.6	97/07/28 SMI"

#include <stdio.h>
#include <unistd.h>
#include "symtab.h"
#include "kbd.h"

extern struct node *root;
extern int numnode;
extern int nerrors;
extern int optR;

extern struct node *findprev(struct node *, struct node *);
extern struct node *nalloc(void);

void	prinval(int, int);

/*
 * Just for kicks...Take a map, just before output, and see if
 * all 256 byte values can be generated from it.  This is the "-r"
 * option.
 */

int
reachout(
	char *nam,	/* map name */
	unsigned char *t,	/* text */
	int nt,	/* n bytes of text */
	unsigned char *one,	/* oneone table */
	struct cornode *cn,	/* cornodes */
	int ncn)	/* number of cornodes */
{
	int i;
	unsigned char *tp;	/* tmp for "t" */
	char tab[256];	/* check table */
	int pted0, pted1, pted2;	/* see if messages printed or not */

	pted0 = pted1 = pted2 = 0;
	/*
	 * If we have a oneone table, then only results that are in
	 * it can be input to sequences.  If we have no oneone table,
	 * then all 256 values can be used in sequences.
	 */
	if (one) {
		for (i = 0; i < 256; i++)
			tab[i] = 0;
	} else {
		for (i = 0; i < 256; i++)
			tab[i] = 1;
	}
	if (one) {	/* check results in oneone table */
		for (i = 0; i < 256; i++) {
			++tab[*one];
			++one;
		}
	}
	/*
	 * Check here for bytes NOT in the one-one table that
	 * ARE in the nodes: they can't be reached!
	 */
	if (one) {
		for (i = 0; i < ncn; i++) {
			if (! tab[cn[i].c_val]) {
				if (!pted0) {
					fprintf(stderr,	gettxt("kbdcomp:41",
						"Map %s:\n"), nam);
					++pted0;
				}
				if (!pted1)
					fprintf(stderr,	gettxt("kbdcomp:42",
"Cannot be generated by 'key', but used elsewhere:\n"));
				++pted1;
				prinval(cn[i].c_val, optR);
			}
		}
	}
	if (pted1)
		fprintf(stderr, "\n");
	/*
	 * Now, check for things in TEXT.  After that, if there is
	 * anything that cannot be generated in any way, tell the user.
	 */
	tp = t;
	for (i = 0; i < nt; i++) {	/* check text */
		++tab[*tp];
		++tp;
	}
	for (i = 0; i < 256; i++) {
		if (! tab[i]) {
			if (!pted0) {
				fprintf(stderr, gettxt("kbdcomp:43",
					"Map %s:\n"), nam);
				++pted0;
			}
			if (!pted2)
				fprintf(stderr, gettxt("kbdcomp:44",
					"Cannot be generated in any way:\n"));
			++pted2;
			prinval(i, optR);
		}
	}
	if (pted2)
		fprintf(stderr, "\n");
	if (pted1 || pted2)
		return (0);	/* can't generate some values */
	return (1);	/* all can be generated */
}

void
prinval(int i, int opt)
{
	if (opt) {
		switch (i) {
		case ' ':
			fprintf(stderr, gettxt("kbdcomp:45", "(SPACE) "), i);
			return;
		case '\177':
			fprintf(stderr, gettxt("kbdcomp:46", "(DEL) "), i);
			return;
		case '\r':
			fprintf(stderr, "\r ", i); return;
		case '\n':
			fprintf(stderr, "\n ", i); return;
		case '\t':
			fprintf(stderr, "\t ", i); return;
		default:
			break;
		}
		if ((i & 0x7f) >= ' ' && (i & 0x7f) <= '~')
			fprintf(stderr, "%c ", i);
		else if (i < ' ')
			fprintf(stderr, "^%c ", i);
		else
			fprintf(stderr, "\\%03o ", i);
		return;
	}
	fprintf(stderr, "\\%03o ", i);
}
