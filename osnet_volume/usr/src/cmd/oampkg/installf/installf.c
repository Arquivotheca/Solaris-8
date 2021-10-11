/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)installf.c	1.20	99/06/28 SMI"	/* SVr4.0 1.10.1.1	*/

/*  5-20-92	added newroot function */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pkgstrct.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libinst.h"
#include "libadm.h"

#define	LSIZE	1024
#define	MALSIZ	164

#define	ERR_MEMORY	"memory allocation failure, errno=%d"
#define	ERR_MAJOR 	"invalid major number <%s> specified for <%s>"
#define	ERR_MINOR 	"invalid minor number <%s> specified for <%s>"
#define	ERR_MODE	"invalid mode <%s> specified for <%s>"
#define	ERR_RELPATH 	"relative pathname <%s> not permitted"
#define	ERR_NULLPATH 	"NULL or garbled pathname"
#define	ERR_LINK	"invalid link specification <%s>"
#define	ERR_LINKFTYPE	"ftype <%c> does not match link specification <%s>"
#define	ERR_LINKARGS	"extra arguments in link specification <%s>"
#define	ERR_LINKREL	"relative pathname in link specification <%s>"
#define	ERR_FTYPE	"invalid ftype <%c> for <%s>"
#define	ERR_ARGC 	"invalid number of arguments for <%s>"
#define	ERR_SPECALL	"ftype <%c> requires all fields to be specified"

extern char	*classname;
extern int	eptnum;
extern struct cfextra **extlist;

extern void	usage(void), quit(int exitval);
static int	validate(struct cfextra *ext, int argc, char *argv[]);
static void checkPaths(char *argv[]);

int		cfentcmp(const void *p1, const void *p2);

int
installf(int argc, char *argv[])
{
	struct cfextra *new;
	char	line[LSIZE];
	char	*largv[8];
	int	myerror;

	if (strcmp(argv[0], "-") != 0) {
		if (argc < 1)
			usage(); /* at least pathname is required */
		extlist = (struct cfextra **) calloc(2,
		    sizeof (struct cfextra *));
		extlist[0] = new = (struct cfextra *) calloc(1,
			sizeof (struct cfextra));
		eptnum = 1;

		/* There is only one filename on the command line. */

		/*
		 * This strips the install root from the path using
		 * a questionable algorithm. This should go away as
		 * we define more precisely the command line syntax
		 * with our '-R' option. - JST
		 *
		 * It has gone away since it didn't work when a link
		 * is provided that resembles the following:
		 *       /a/b/c/d/dest=../../src - TSK
		 *
		 */

		checkPaths(argv);

		if (validate(new, argc, argv))
			quit(1);
		return (0);
	}

	/* Read stdin to obtain entries, which need to be sorted. */
	eptnum = 0;
	myerror = 0;
	extlist = (struct cfextra **) calloc(MALSIZ, sizeof (struct cfextra *));
	while (fgets(line, LSIZE, stdin) != NULL) {
		argc = 0;
		argv = largv;
		argv[argc++] = strtok(line, " \t\n");
		while (argv[argc] = strtok(NULL, " \t\n"))
			argc++;

		if (argc < 1)
			usage(); /* at least pathname is required */

		new = (struct cfextra *) calloc(1, sizeof (struct cfextra));
		if (new == NULL) {
			progerr(gettext(ERR_MEMORY), errno);
			quit(99);
		}

		/*
		 * It is not clear why we call orig_path() above, but not
		 * here. I dispute its call above. Perhaps there is good
		 * reason to drop this function soon. - JST
		 */

		/*
		 * Fix for bug 419937. orig_path() doesn't work when a
		 * relative symlink is used. i.e. /a/b/c/d/dest=../../link
		 */

		checkPaths(argv);

		if (validate(new, argc, argv))
			myerror++;

		extlist[eptnum] = new;
		if ((++eptnum % MALSIZ) == 0) {
			extlist = (struct cfextra **) realloc((void *)extlist,
			    (unsigned) (sizeof (struct cfextra *) *
			    (eptnum+MALSIZ)));
			if (!extlist) {
				progerr(gettext(ERR_MEMORY), errno);
				quit(99);
			}
		}
	}
	extlist[eptnum] = (struct cfextra *)NULL;
	qsort((char *)extlist, (unsigned)eptnum, sizeof (struct cfextra *),
		cfentcmp);
	return (myerror);
}

static int
validate(struct cfextra *ext, int argc, char *argv[])
{
	char	*ret, *pt;
	int	n, allspec, is_a_link;
	struct	cfent *ept;

	ept = &(ext->cf_ent);

	/* initialize cfent structure */
	ept->pinfo = (struct pinfo *) 0;
	(void) gpkgmap(ept, NULL);	/* This just clears stuff. */

	n = allspec = 0;
	if (classname)
		(void) strncpy(ept->pkg_class, classname, CLSSIZ);

	if (argv[n] == NULL || *(argv[n]) == '\000') {
		progerr(gettext(ERR_NULLPATH));
		return (1);
	}

	/*
	 * It would be a good idea to figure out how to get much of
	 * this done using facilities in procmap.c - JST
	 */
	if (pt = strchr(argv[n], '=')) {
		*pt = '\0';	/* cut off pathname at the = sign */
		is_a_link = 1;
	} else
		is_a_link = 0;

	if (RELATIVE(argv[n])) {
		progerr(gettext(ERR_RELPATH),
		    (argv[n] == NULL) ? "unknown" : argv[n]);
		return (1);
	}

	/* get the pathnames */
	if (eval_path(&(ext->server_path), &(ext->client_path),
	    &(ext->map_path), argv[n++]) == 0)
		return (1);

	ept->path = ext->client_path;

	/* This isn't likely to happen; but, better safe than sorry. */
	if (RELATIVE(ept->path)) {
		progerr(gettext(ERR_RELPATH), ept->path);
		return (1);
	}

	if (is_a_link) {
		/* links specifications should be handled right here */
		ept->ftype = ((n >= argc) ? 'l' : argv[n++][0]);

		/* If nothing follows the '=', it's invalid */
		if (!pt[1]) {
			progerr(gettext(ERR_LINK), ept->path);
			return (1);
		}

		/* Test for an argument after the link. */
		if (argc != n) {
			progerr(gettext(ERR_LINKARGS), ept->path);
			return (1);
		}

		/*
		 * If it's a link but it's neither hard nor symbolic then
		 * it's bad.
		 */
		if (!strchr("sl", ept->ftype)) {
			progerr(gettext(ERR_LINKFTYPE), ept->ftype, ept->path);
			return (1);
		}

		ext->server_local = pathdup(pt+1);
		ext->client_local = ext->server_local;

		ept->ainfo.local = ext->client_local;

		/* If it's a hard link, the argument must be absolute. */
		if (ept->ftype != 's' && ept->ainfo.local[0] != '/') {
			progerr(gettext(ERR_LINKREL), ept->path);
			return (1);
		}
		return (0);
	} else if (n >= argc) {
		/* we are expecting to change object's contents */
		return (0);
	}

	ept->ftype = argv[n++][0];
	if (strchr("sl", ept->ftype)) {
		progerr(gettext(ERR_LINK), ept->path);
		return (1);
	} else if (!strchr("?fvedxcbp", ept->ftype)) {
		progerr(gettext(ERR_FTYPE), ept->ftype, ept->path);
		return (1);
	}

	if (ept->ftype == 'b' || ept->ftype == 'c') {
		if (n < argc) {
#ifdef SUNOS41
			ept->ainfo.xmajor = strtol(argv[n++], &ret, 0);
#else
			ept->ainfo.major = strtol(argv[n++], &ret, 0);
#endif
			if (ret && *ret) {
				progerr(gettext(ERR_MAJOR), argv[n-1],
				    ept->path);
				return (1);
			}
		}
		if (n < argc) {
#ifdef SUNOS41
			ept->ainfo.xminor = strtol(argv[n++], &ret, 0);
#else
			ept->ainfo.minor = strtol(argv[n++], &ret, 0);
#endif
			if (ret && *ret) {
				progerr(gettext(ERR_MINOR), argv[n-1],
				    ept->path);
				return (1);
			}
			allspec++;
		}
	}

	allspec = 0;
	if (n < argc) {
		ept->ainfo.mode = strtol(argv[n++], &ret, 8);
		if (ret && *ret) {
			progerr(gettext(ERR_MODE), argv[n-1], ept->path);
			return (1);
		}
	}
	if (n < argc)
		(void) strncpy(ept->ainfo.owner, argv[n++], ATRSIZ);
	if (n < argc) {
		(void) strncpy(ept->ainfo.group, argv[n++], ATRSIZ);
		allspec++;
	}
	if (strchr("dxbcp", ept->ftype) && !allspec) {
		progerr(gettext(ERR_ARGC), ept->path);
		progerr(gettext(ERR_SPECALL), ept->ftype);
		return (1);
	}
	if (n < argc) {
		progerr(gettext(ERR_ARGC), ept->path);
		return (1);
	}
	return (0);
}

int
cfentcmp(const void *p1, const void *p2)
{
	struct cfextra *ext1 = *((struct cfextra **) p1);
	struct cfextra *ext2 = *((struct cfextra **) p2);

	return (strcmp(ext1->cf_ent.path, ext2->cf_ent.path));
}

/* Strip PKG_INSTALL_ROOT from the path if it exists. */
static void
checkPaths(char *argv[])
{
	char *root;
	int rootLen;

	/* Note:  No local copy of argv is needed since this
	 * function is guarnteed to replace argv with a subset of
	 * the original argv.
	 */ 

	/* We only want to canonize the path if it contains multiple '/'s */

	canonize_slashes(argv[0]);

	if ((root = get_inst_root()) == NULL)
		return;
	if (strcmp(root, "/") != 0) {
		rootLen = strlen(root);
		if (strncmp(argv[0], root, rootLen) == 0) {
			argv[0] += rootLen;
		}
	}
}

