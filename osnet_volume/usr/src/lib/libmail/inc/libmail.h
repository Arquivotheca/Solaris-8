/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* This is a private header file.				*/

#ifndef _LIBMAIL_H
#define _LIBMAIL_H

#pragma ident	"@(#)libmail.h	1.4	99/03/09 SMI" 	/* SVr4.0 1.4	*/

#include <stdio.h>
#include <maillock.h>
#include "s_string.h"

#ifndef	MFMODE
#define	MFMODE		0660		/* create mode for `/var/mail' files */
#endif
#ifdef	FILENAME_MAX
#define	MAXFILENAME	FILENAME_MAX	/* max length of a filename */
#else
#define	MAXFILENAME	512		/* max length of a filename */
#endif

/* The following typedefs must be used in SVR4 */
#ifdef SVR3
typedef int mode_t;
#else
# include <sys/types.h>
#endif

#if defined(__STDC__) || defined(__cplusplus)
extern	string *abspath(char *path, char *dot, string *to);
extern	int casncmp(char *s1, char *s2, ssize_t n);
extern	int copystream(FILE *infp, FILE *outfp);
extern	int delempty(mode_t m, char *mailname);
extern	char *maildomain(void);
extern	void notify(char *user, char *msg, int check_mesg_y, char *etcdir);
extern	int pclosevp(FILE *fp);
extern	FILE *popenvp(char *file, char **argv, char *mode, int resetid);
extern	char **setup_exec(char *s);
extern	char *skipspace(char *p);
extern	int substr(char *string1, char *string2);
extern	void strmove(char *from, char *to);
extern	pid_t systemvp(char *file, char **argv, int resetid);
extern	void trimnl(char *s);
extern	char *Xgetenv(char *env);
extern	char *xgetenv(char *env);
extern	int xsetenv(char *file);
#else
extern	string *abspath();
extern	int casncmp();
extern	int copystream();
extern	int delempty();
extern	char *maildomain();
extern	void notify();
extern	int pclosevp();
extern	FILE *popenvp();
extern	char **setup_exec();
extern	char *skipspace();
extern	void strmove();
extern	int substr();
extern	pid_t systemvp();
extern	void trimnl();
extern	char *Xgetenv();
extern	char *xgetenv();
extern	int xsetenv();
#endif

#endif /* _LIBMAIL_H */
