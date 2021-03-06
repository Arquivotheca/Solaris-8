/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pt_chmod.c	1.8	97/01/22 SMI"	/* SVr4.0 1.1	*/

#include <grp.h>

#define	DEFAULT_TTY_GROUP	"tty"

/*
 * change the owner and mode of the pseudo terminal slave device.
 */
main(argc, argv)
int	argc;
char	**argv;
{
	int	fd;
	gid_t	gid;

	struct	group	*gr_name_ptr;

	char	*ptsname();

	if (argc < 2) {
		exit(-1);
	}
	if ((gr_name_ptr = getgrnam(DEFAULT_TTY_GROUP)) != NULL)
		gid = gr_name_ptr->gr_gid;
	else
		gid = getgid();

	fd = atoi(argv[1]);

	if (chown(ptsname(fd), getuid(), gid))
		exit(-1);

	if (chmod(ptsname(fd), 00620))
		exit(-1);

	exit(0);
}
