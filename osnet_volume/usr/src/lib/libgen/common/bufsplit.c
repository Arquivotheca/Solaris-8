/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bufsplit.c	1.12	97/03/28 SMI"	/* SVr4.0 1.2.3.2 */

/*LINTLIBRARY*/

/*
 *	Split buffer into fields delimited by tabs and newlines.
 *	Fill pointer array with pointers to fields.
 * 	Return the number of fields assigned to the array[].
 *	The remainder of the array elements point to the end of the buffer.
 *
 *      Note:
 *	The delimiters are changed to null-bytes in the buffer and array of
 *	pointers is only valid while the buffer is intact.
 */

#pragma weak bufsplit = _bufsplit

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>

#ifndef _REENTRANT
static char *bsplitchar = "\t\n";	/* characters that separate fields */
#endif

#ifdef _REENTRANT
static char **
_get_bsplitchar(thread_key_t *key)
{
	static char **strp = NULL;
	static char *init_bsplitchar = "\t\n";

	if (_thr_getspecific(*key, (void **)&strp) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (NULL);
		}
	}
	if (!strp) {
		if (_thr_setspecific(*key, (void *)(strp = malloc(
			sizeof (char *)))) != 0) {
			if (strp)
				(void) free(strp);
			return (NULL);
		}
		*strp = init_bsplitchar;
	}
	return (strp);
}
#endif /* _REENTRANT */

size_t
bufsplit(char *buf, size_t dim, char **array)
{
#ifdef _REENTRANT
	static thread_key_t key = 0;
	char  **bsplitchar = _get_bsplitchar(&key);
#define	bsplitchar (*bsplitchar)
#endif _REENTRANT

	unsigned numsplit;
	int	i;

	if (!buf)
		return (0);
	if (!dim ^ !array)
		return (0);
	if (buf && !dim && !array) {
		bsplitchar = buf;
		return (1);
	}
	numsplit = 0;
	while (numsplit < dim) {
		array[numsplit] = buf;
		numsplit++;
		buf = strpbrk(buf, bsplitchar);
		if (buf)
			*(buf++) = '\0';
		else
			break;
		if (*buf == '\0') {
			break;
		}
	}
	buf = strrchr(array[numsplit-1], '\0');
	for (i = numsplit; i < dim; i++)
		array[i] = buf;
	return (numsplit);
}
