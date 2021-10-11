/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * adb - input routines
 */

#pragma ident	"@(#)input.c	1.13	99/05/04 SMI"
/*
 * adb - input routines
 */

#ident "@(#)input.c	1.1	93/02/04 SMI"

#if !defined(KADB) || !defined(i386)
#include <stdio.h>

char	line[BUFSIZ];
#else
char	line[1024];
#endif
#include "adb.h"
extern int infile;
int	eof;
char	lastc = '\n';

eol(c)
	char c;
{

	return (c == '\n' || c == ';');
}

char
rdc()
{

	do
		(void) readchar();
	while (lastc == ' ' || lastc == '\t');
	return (lastc);
}

char
readchar()
{

	if (eof) {
		lastc = 0;
		return (0);
	}
	if (lp == 0) {
		lp = line;

		do {
#if defined(KADB) && defined(i386)
			register int rc;
			rc = _read(infile, lp, 1);
			eof = (*lp == 0x4 || rc <= 0) ? 1 : 0;
#else
			eof = read(infile, lp, 1) <= 0;
#endif
			if (interrupted)
				error((char *)0);
		} while (eof == 0 && *lp++ != '\n');
		*lp = 0;
		lp = line;
	}
	if (lastc = nextc)
		nextc = 0;
	else
		if (lastc = *lp)
			lp++;
	return (lastc);
}

char
nextchar()
{

	if (eol(rdc())) {
		lp--;
		return (0);
	}
	return (lastc);
}

char
quotchar()
{

	if (readchar() == '\\')
		return (readchar());
	if (lastc == '\'')
		return (0);
	return (lastc);
}

void
getformat(deformat)
	char *deformat;
{
	register char *fptr = deformat;
	register int quote = 0;

	while (quote ? readchar() != '\n' : !eol(readchar()))
		if ((*fptr++ = lastc) == '"')
			quote = ~quote;
	lp--;
	if (fptr != deformat)
		*fptr++ = '\0';
}

#define	MAXIFD	5
struct {
#ifdef _LP64
	unsigned long fd;
	unsigned long r9;
#else
	int	fd;
	int	r9;
#endif
} istack[MAXIFD];
int	ifiledepth;

void
iclose(stack, err)
	int stack, err;
{
	db_printf(7, "iclose: stack=%D, err=%D", stack, err);
	if (err) {
		if (infile) {
			(void) close(infile);
			infile = 0;
		}
		while (--ifiledepth >= 0)
			if (istack[ifiledepth].fd)
				(void) close(istack[ifiledepth].fd);
		ifiledepth = 0;
		return;
	}
	switch (stack) {

	case 0:
		if (infile) {
			(void) close(infile);
			infile = 0;
		}
		break;

	case 1:
		if (ifiledepth >= MAXIFD)
			error("$<< nesting too deep");
		istack[ifiledepth].fd = infile;
		istack[ifiledepth].r9 = var[9];
		ifiledepth++;
		infile = 0;
		break;

	case -1:
		if (infile) {
			(void) close(infile);
			infile = 0;
		}
		if (ifiledepth > 0) {
			infile = istack[--ifiledepth].fd;
			var[9] = istack[ifiledepth].r9;
		}
	}
}
