/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)volcancel.c	1.13	96/09/26 SMI"

/*
 * Program to cancel pending i/o
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<fcntl.h>
#include	<locale.h>
#include	<libintl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mkdev.h>
#include	<sys/vol.h>
#include	<sys/param.h>
#include	<errno.h>
#include	<volmgt.h>

#include	"volutil.h"

/*
 * ON-private libvolmgt routine(s)
 */
extern void	_media_printaliases(void);

/*
 * volcancel return codes:
 */

#define	SUCCESS			0
#define	USAGE_ERROR		1
#define	VOLMGT_NOT_RUNNING	2
#define	OPEN_ERROR		3
#define	IOCTL_ERROR		4


static char		*prog_name;
static void	usage(void);
static char	*pathify(char *);	/* add /vol/rdsk if needed */
static int	cancel(char *);


void
main(int argc, char **argv)
{
	extern int	optind;
	int		c;
	char		*name;
	int		excode = SUCCESS;


#ifdef DEBUG
	(void) fprintf(stderr, "VOLCANCEL: entering\n");
	(void) fflush(stderr);
#endif

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	prog_name = argv[0];

	/* process arguments */
	while ((c = getopt(argc, argv, "n")) != EOF) {
		switch (c) {
		case 'n':
			_media_printaliases();
			exit(SUCCESS);
		default:
			usage();
			exit(USAGE_ERROR);
		}
	}

	if (!volmgt_running()) {
		(void) fprintf(stderr,
		    gettext("%s: volume management is not running\n"),
		    prog_name);
#ifdef DEBUG
		(void) fprintf(stderr, "VOLCANCEL: exit value = %d\n",
		    VOLMGT_NOT_RUNNING);
		(void) fflush(stderr);
#endif

		exit(VOLMGT_NOT_RUNNING);
	}

	for (; optind < argc; optind++) {
		name = pathify(argv[optind]); /* pathify the arg */
#ifdef DEBUG
		(void) fprintf(stderr, "VOLCANCEL: calling cancel(%s)\n",
		    name);
		(void) fflush(stderr);
#endif
		if ((excode = cancel(name)) != 0) {
			break;
		}
	}

#ifdef DEBUG
	(void) fprintf(stderr, "VOLCANCEL: returning %d\n", excode);
	(void) fflush(stderr);
#endif

	exit(excode);
}


static void
usage()
{
	(void) fprintf(stderr,
	    gettext("usage: %s [name | nickname]\n"), prog_name);
}


static int
cancel(char *path)
{
	int	fd;


#ifdef DEBUG
	(void) fprintf(stderr, "VOLCANCEL: in cancel try open(%s)\n", path);
	(void) fflush(stderr);
#endif

	if ((fd = open(path, O_RDONLY|O_NDELAY)) < 0) {
#ifdef DEBUG
		perror(path);
#endif
		return (OPEN_ERROR);
	}

#ifdef DEBUG
	(void) fprintf(stderr, "VOLCANCEL: in cancel try ioctl\n");
	(void) fflush(stderr);
#endif

	if (ioctl(fd, VOLIOCCANCEL, 0) < 0) {
#ifdef DEBUG
		(void) fprintf(stderr,
	"volcancel error: ioctl(VOLIOCCANCEL) failed (errno %d; %s)\n",
		    errno, strerror(errno));
#endif
		return (IOCTL_ERROR);
	}

#ifdef DEBUG
	(void) fprintf(stderr, "VOLCANCEL: cancel() returning %d\n", SUCCESS);
	(void) fflush(stderr);
#endif
	return (SUCCESS);
}


static char *
pathify(char *path)
{
	/*
	 * ensure path exists -- if it doesn't, tack "/vol/rdsk" on front
	 * (oor alternate root if not "/vol")
	 */
	static char	vold_root[MAXPATHLEN+1] = "";
	static uint	vold_root_len;
	struct stat64	sb;		/* set but not used */
	static char	path_buf[MAXPATHLEN+1];
	char		*path_ptr = path;


	if (*vold_root == '\0') {
		(void) strcpy(vold_root, volmgt_root());
		(void) strcat(vold_root, "/");
		(void) strcat(vold_root, "rdsk");
		vold_root_len = strlen(vold_root);
	}

	if (stat64(path, &sb) < 0) {
		/* path doesn't already exist */
		if (strncmp(path, vold_root, vold_root_len) != 0) {
			/* found it in rdsk under vol root */
			(void) strcpy(path_buf, vold_root);
			(void) strcat(path_buf, "/");
			(void) strcat(path_buf, path);
			path_ptr = path_buf;
		}
	}

	return (path_ptr);
}
