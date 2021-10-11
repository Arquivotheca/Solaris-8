/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)volstat.c	1.9	96/01/11 SMI"

/*
 * Program to look at statistics and attributes of a volume.
 */

#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<dirent.h>
#include	<fcntl.h>
#include	<string.h>
#include	<locale.h>
#include	<libintl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/dkio.h>
#include	<sys/param.h>
#include	<errno.h>
#include	<volmgt.h>
#include	<sys/vol.h>

#include	"volutil.h"


/*
 * ON-private routines from libvolmgt
 */
extern char		*_media_oldaliases(char *);

static char	*prog_name = NULL;

static char	*attr = NULL;
static char	*value = NULL;

static int	do_set = NULL;
static int	do_get = NULL;
static int	do_info = NULL;


int
main(int argc, char **argv)
{
	static void	usage(void);
	static void	work(char *);
	int		c;
	char		*s;


	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	prog_name = argv[0];

	/* process arguments */
	while ((c = getopt(argc, argv, "s:g:i")) != EOF) {
		switch (c) {
		case 's':
			do_set++;
			attr = optarg;
			break;
		case 'g':
			do_get++;
			attr = optarg;
			break;
		case 'i':
			do_info++;
			break;
		default:
			usage();
			return (-1);
		}
	}

	/*
	 * break apart attr=value for set operation.
	 */
	if (do_set) {
		if ((s = strchr(attr, '=')) == NULL) {
			(void) fprintf(stderr,
			    gettext("%s: %s, error; must be attr=value\n"),
			    prog_name, attr);
			return (-1);
		}
		*s++ = '\0';
		value = s;
	}

	for (; optind < argc; optind++) {
		work(argv[optind]);
	}
	return (0);
}


static void
usage()
{
	(void) fprintf(stderr,
	    gettext("usage: %s [name | nickname] \n"), prog_name);
}


static void
work(char *arg)
{
	int	fd;
	char	*name;
	char	*name1;



	if ((name = media_findname(arg)) == NULL) {
		if ((name1 = _media_oldaliases(arg)) != NULL) {
			name = media_findname(name1);
		}

		if (name == NULL) {
			(void) fprintf(stderr, gettext("No such volume: %s\n"),
			    arg);
			return;
		}
	}
	if (do_info) {
		struct vioc_info	info;
		char			path[MAXNAMELEN+1];

		if ((fd = open(name, O_RDONLY|O_NDELAY)) < 0) {
			perror(name);
			return;
		}
		memset(path, 0, MAXNAMELEN+1);
		info.vii_devpath = path;
		info.vii_pathlen = MAXNAMELEN;
		if (ioctl(fd, VOLIOCINFO, &info) < 0) {
			(void) fprintf(stderr, "%s; ", name);
			perror("info");
			(void) close(fd);
			return;
		}
		(void) printf("%s\t%llu\t%s\t%s\n", name, media_getid(name),
		    (path[0] == '\0') ? "no mapping" : path,
		    (info.vii_inuse > 1) ? "busy" : "not busy");
		(void) close(fd);
	}

	if (do_get) {
		char	*val = (char *)media_getattr(name, attr);

		(void) printf("%s: %s=%s\n", name, attr,
		    val == NULL ? "null": val);
	}

	if (do_set) {
		if (media_setattr(name, attr, value) == FALSE) {
			(void) printf(gettext(
			"setattr failed for attribute %s on %s (errno %d)\n"),
			    name, attr, errno);
		}
	}
}
