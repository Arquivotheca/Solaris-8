/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)myfopen.c	1.4	94/01/07 SMI"	/* from SVr4.0 1.2.1.1 */

#include "rcv.h"

#undef	fopen
#undef	fclose

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Local version of fopen() and fclose(). These maintain a list of
 * file pointers which can be run down when we need to close
 * all files, such as before executing external commands.
 */

static NODE	*append();
static NODE	*del1();
static NODE	*getnode();
static NODE	*search();

static NODE *
getnode(FILE *fp)
{
	NODE *newnode;

	if ((newnode = (NODE *)malloc(sizeof(NODE))) == (NODE *)NULL) {
		(void) fputs("Cannot allocate node space\n", stderr);
		exit(3);
	}
	newnode->fp = fp;
	newnode->next = (NODE *)NULL;
	return(newnode);
}

static NODE *
search(FILE *fp)
{
	register NODE *tmp;
	
	for (tmp = fplist; tmp != (NODE *)NULL; tmp = tmp->next)
		if (tmp->fp == fp)
			break;
	return( tmp != (NODE *)NULL ? tmp : NOFP);
}
	
static NODE *
append(FILE *fp)
{
	register NODE *newnode;

	if ((newnode = getnode(fp)) == (NODE *)NULL)
		return(NOFP);
	if (fplist == NOFP) {
		fplist = newnode;
	} else {
		newnode->next = curptr->next;
		curptr->next = newnode;
	}
	return(newnode);
}

static NODE *
del1(NODE *oldcur)
{
	register NODE *cur, *prev;

	for (prev = cur = fplist; cur != (NODE *)NULL; cur = cur->next) {
		if (cur == oldcur) {
			if (cur == fplist) {
				cur = fplist = cur->next;
			} else {
				prev->next = cur->next;
				cur = prev->next ? prev->next : prev;
			}
			if (curptr == oldcur)
				curptr = prev;
			free(oldcur);
			break;
		}
		prev = cur;
	}
	return(cur);
}

FILE *
my_fopen(char *file, char *mode)
{
	FILE *fp;

	if ((fp = fopen(file, mode)) == (FILE *)NULL) {
		fplist = NOFP;
		return(fp);
	} else {
		curptr = append(fp);
	}
	return(fp);
}

int
my_fclose(register FILE *iop)
{
	register NODE *cur;

	int ret = fclose(iop);
	if (fplist != NOFP) {
		cur = search(iop);
		cur = del1(cur);
	}
	return ret;
}
