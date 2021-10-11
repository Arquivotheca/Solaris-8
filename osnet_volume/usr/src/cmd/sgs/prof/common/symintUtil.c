/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)symintUtil.c	6.4	93/06/07 SMI"

/*
*	file: symintUtil.c
*	desc: utilities for symint code
*	date: 11/08/88
*/
#include <stdio.h>
#include <sys/types.h>
#include "debug.h"

/*
*	_Malloc and _Realloc are used to monitor the allocation
*	of memory.  If failure occurs, we detect it and exit.
*/

void *
_Malloc(item_count, item_size)
uint item_count;
uint item_size;
{
	char *malloc();
	register void *p;

	if ((p = (void *) calloc(item_count, item_size)) == NULL) {
		DEBUG_EXP(printf("- size=%d, count=%d\n", item_size, item_count));
		_err_exit("calloc: Out of space");
	}
	return (p);
}

void *
_Realloc(pointer, size)
void *pointer;
uint size;
{
	char *realloc();
	register void *p;

	if ((p = (void *) realloc(pointer, size)) == NULL) {
		_err_exit("realloc: Out of space");
	}
	return (p);
}

