/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)fcns.c	6.5	97/05/21 SMI"

#include	<stdio.h>

/*
*  error(file, string)
*  simply prints the error message string
*  simply returns
*/

void
error(char *file, char *string)
{
	extern	exitcode;
	extern int is_archive;
	extern char *archive;

	(void) fflush(stdout);
	if (is_archive) {
		(void) fprintf(stderr,
		"size: %s[%s]: %s\n", archive, file, string);
	} else {
		(void) fprintf(stderr,
		"size: %s: %s\n", file, string);
	}
	exitcode++;
}
