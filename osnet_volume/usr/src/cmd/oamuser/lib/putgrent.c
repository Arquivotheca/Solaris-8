/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)putgrent.c	1.7	94/09/16	SMI"	/* SVr4.0 1.3 */

#include <stdio.h>
#include <grp.h>
#include <unistd.h>

/*
 * putgrent()	function to write a group structure to a file
 *		supports the use of group names that with + or -
 */
void
putgrent(grpstr, to)
struct group *grpstr;	/* group structure to write */
FILE *to;		/* file to write to */
{
	register char **memptr;		/* member vector pointer */

	if (grpstr->gr_name[0] == '+' || grpstr->gr_name[0] == '-') {
		/*
		 * if the groupname starts with either a '+' or '-' then
		 * write out what we can as best as we can
		 * we assume that fgetgrent() set gr_gid to 0 so
		 * write a null entry instead of 0
		 * This should not break /etc/nsswitch.conf for any of
		 * "group: compat", "group: files", "group: nis"
		 *
		 */
		(void) fprintf(to, "%s:%s:", grpstr->gr_name,
			grpstr->gr_passwd);

		if (grpstr->gr_gid == 0) {
			(void) fprintf(to, ":");
		} else {
			(void) fprintf(to, "%ld:", grpstr->gr_gid);
		}

		memptr = grpstr->gr_mem;

		while (memptr != NULL && *memptr != NULL) {
			(void) fprintf(to, "%s", *memptr);
			memptr++;
			if (memptr != NULL && *memptr != NULL)
				(void) fprintf(to, ",");
		}

		(void) fprintf(to, "\n");
	} else {
		/*
		 * otherwise write out all the fields in the group structure
		 *
		 */
		(void) fprintf(to, "%s:%s:%ld:", grpstr->gr_name,
			grpstr->gr_passwd, grpstr->gr_gid);

		memptr = grpstr->gr_mem;

		while (*memptr != NULL) {
			(void) fprintf(to, "%s", *memptr);
			memptr++;
			if (*memptr != NULL)
				(void) fprintf(to, ",");
		}

		(void) fprintf(to, "\n");
	}
}
