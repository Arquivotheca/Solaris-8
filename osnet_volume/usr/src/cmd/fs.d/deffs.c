/* Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990, 1991, 1997 SMI	*/
/* All Rights Reserved							*/

#ident	"@(#)deffs.c	1.5	97/07/10 SMI"

#include	<stdio.h>
#include        <deflt.h>
#include        <string.h>

#define	LOCAL		"/etc/default/fs"
#define	REMOTE		"/etc/dfs/fstypes"

/*
 * This is used to figure out the default file system type if "-F FStype"
 * is not specified with the file system command and no entry in the
 * /etc/vfstab matches the specified special.
 * If the first character of the "special" is a "/" (eg, "/dev/dsk/c0d1s2"),
 * returns the default local filesystem type.
 * Otherwise (eg, "server:/path/name" or "resource"), returns the default
 * remote filesystem type.
 */
char	*
default_fstype(char *special)
{
	char	*deffs;
	static	char	buf[BUFSIZ];
	FILE	*fp;

	if (*special == '/') {
		if (defopen(LOCAL) != 0)
			return ("ufs");
		else {
			if ((deffs = defread("LOCAL=")) == NULL) {
				defopen(NULL);	/* close default file */
				return ("ufs");
			} else {
				defopen(NULL);	/* close default file */
				return (deffs);
			}
		}
	} else {
		if ((fp = fopen(REMOTE, "r")) == NULL)
			return ("nfs");
		else {
			if (fgets(buf, sizeof (buf), fp) == NULL) {
				fclose(fp);
				return ("nfs");
			} else {
				deffs = strtok(buf, " \t\n");
				fclose(fp);
				return (deffs);
			}
		}
	}
}
