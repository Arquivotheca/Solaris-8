/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Internal definitions for the myrcmd.c rcmd(3) replacement module.
 */

#ifndef _MYRCMD_H
#define	_MYRCMD_H

#pragma ident	"@(#)myrcmd.h	1.5	99/01/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Failure return values */
#define	MYRCMD_EBAD		-1
#define	MYRCMD_NOHOST		-2
#define	MYRCMD_ENOPORT		-3
#define	MYRCMD_ENOSOCK		-4
#define	MYRCMD_ENOCONNECT	-5

/*
 * On a failure, the output that would have normally gone to stderr is
 * now placed in the global string "myrcmd_stderr".  Callers should check
 * to see if there is anything in the string before trying to print it.
 */
extern char myrcmd_stderr[];

#ifdef __STDC__
extern int myrcmd(char **ahost, unsigned short rport, char *locuser,
	char *remuser, char *cmd);
#else
extern int myrcmd();
#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif /* _MYRCMD_H */
