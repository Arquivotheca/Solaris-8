/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)putpwent.c	1.13	96/11/20 SMI"  /* SVr4.0 1.10 */

/*LINTLIBRARY*/
/*
 * format a password file entry
 */
#pragma weak putpwent = _putpwent
#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>

int
putpwent(const struct passwd *p, FILE *f)
{
	int black_magic;

	(void) fprintf(f, "%s:%s", p->pw_name,
	    p->pw_passwd ? p->pw_passwd : "");
	if (((p->pw_age) != NULL) && ((*p->pw_age) != '\0'))
		(void) fprintf(f, ",%s", p->pw_age); /* fatal "," */
	black_magic = (*p->pw_name == '+' || *p->pw_name == '-');
	/* leading "+/-"  taken from getpwnam_r.c */
	if (black_magic) {
		(void) fprintf(f, ":::%s:%s:%s",
			p->pw_gecos ? p->pw_gecos : "",
			p->pw_dir ? p->pw_dir : "",
			p->pw_shell ? p->pw_shell : "");
	} else { /* "normal case" */
		(void) fprintf(f, ":%d:%d:%s:%s:%s",
			p->pw_uid,
			p->pw_gid,
			p->pw_gecos,
			p->pw_dir,
			p->pw_shell);
	}
	(void) putc('\n', f);
	(void) fflush(f);
	return (ferror(f));
}
