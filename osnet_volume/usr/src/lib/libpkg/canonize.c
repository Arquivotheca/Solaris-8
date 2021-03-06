/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
#ident	"@(#)canonize.c	1.7	99/06/28 SMI"	/* SVr4.0  1.2	*/
#include <string.h>

#define	isdot(x)	((x[0] == '.') && (!x[1] || (x[1] == '/')))
#define	isdotdot(x)	((x[0] == '.') && (x[1] == '.') && \
			    (!x[2] || (x[2] == '/')))

void
canonize(char *file)
{
	char *pt, *last;
	int level;

	/* remove references such as './' and '../' and '//' */
	for (pt = file; *pt; /* void */) {
		if (isdot(pt))
			(void) strcpy(pt, pt[1] ? pt+2 : pt+1);
		else if (isdotdot(pt)) {
			level = 0;
			last = pt;
			do {
				level++;
				last += 2;
				if (*last)
					last++;
			} while (isdotdot(last));
			--pt; /* point to previous '/' */
			while (level--) {
				if (pt <= file)
					return;
				while ((*--pt != '/') && (pt > file))
					;
			}
			if (*pt == '/')
				pt++;
			(void) strcpy(pt, last);
		} else {
			while (*pt && (*pt != '/'))
				pt++;
			if (*pt == '/') {
				while (pt[1] == '/')
					(void) strcpy(pt, pt+1);
				pt++;
			}
		}
	}
	if ((--pt > file) && (*pt == '/'))
		*pt = '\0';
}

canonize_slashes(char *file)
{
	char *pt, *last;
	int level;

	/* remove references such as '//' */
	for (pt = file; *pt; /* void */) {
		while (*pt && (*pt != '/'))
			pt++;
		if (*pt == '/') {
			while (pt[1] == '/')
				(void) strcpy(pt, pt+1);
			pt++;
		}
	}
	if ((--pt > file) && (*pt == '/'))
		*pt = '\0';
}
