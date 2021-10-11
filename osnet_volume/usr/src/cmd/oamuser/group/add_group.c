/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)add_group.c	1.5	96/09/11 SMI"       /* SVr4.0 1.3 */

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<userdefs.h>

#define GRPTMP		"/etc/gtmp"
#define GRPBUFSIZ	5120

int
add_group(group, gid)
char *group;	/* name of group to add */
gid_t gid;		/* gid of group to add */
{
	FILE *etcgrp;		/* /etc/group file */
	FILE *etctmp;		/* temp file */
	int o_mask;		/* old umask value */
	int newdone = 0;	/* set true when new entry done */
	struct stat sb;		/* stat buf to copy modes */
	char buf[GRPBUFSIZ];

	if( (etcgrp = fopen( GROUP, "r" )) == NULL) {
		return( EX_UPDATE );
	}

	if ( fstat(fileno(etcgrp), &sb) < 0) {
		/* If we can't get mode, take a default */
		sb.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
	}

	o_mask = umask(077);
	etctmp = fopen(GRPTMP, "w+");
	(void)umask(o_mask);

	if (etctmp == NULL) {
		fclose(etcgrp);
fprintf(stderr, "err ret 2\n");
		return( EX_UPDATE );
	}

	if (fchmod(fileno(etctmp), sb.st_mode) != 0 ||
	   fchown(fileno(etctmp),sb.st_uid, sb.st_gid) !=0 ||
	    lockf( fileno(etctmp), F_LOCK, 0 ) != 0) {
		fclose(etcgrp);
		fclose(etctmp);
		unlink(GRPTMP);
		return( EX_UPDATE );
	}

	while (fgets(buf, GRPBUFSIZ, etcgrp) != NULL) {
		/* Check for NameService reference */
		if (!newdone && (buf[0] == '+' || buf[0] == '-')) {
			(void) fprintf( etctmp, "%s::%ld:\n", group, gid );
			newdone = 1;
		}

		fputs(buf, etctmp);
	}


	(void) fclose(etcgrp);

	if (!newdone) {
		(void) fprintf( etctmp, "%s::%ld:\n", group, gid );
	}

	if (rename(GRPTMP, GROUP) < 0) {
		fclose(etctmp);
		unlink(GRPTMP);
		return( EX_UPDATE );
	}

	(void) fclose(etctmp);


	return( EX_SUCCESS );
}
