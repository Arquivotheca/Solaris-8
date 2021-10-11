/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)share.c	1.7	98/10/22 SMI"	/* SVr4.0 1.10	*/

/*
 *	generic interface to share
 *
 *	usage:	share [-F fstype] [-o fs_options] [-d desc] [ args ]
 *
 *	exec's /usr/lib/fs/<fstype>/<cmd>
 *	<cmd> is the basename of the command.
 *
 *	if -F is missing, fstype is the first entry in /etc/dfs/fstypes
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utmpx.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#define	DFSTYPES	"/etc/dfs/fstypes"		/* dfs list */
#define	FSCMD		"/usr/lib/fs/%s/%s"
#define	SHAREFILE	"/etc/dfs/sharetab"
#define	MAXFIELD	5
#define	BUFSIZE		4096

#define	ARGVPAD		6	/* non-[arg...] elements in new argv list: */
				/* cmd name, -o, opts, -d, desc, (char *)0 */
				/*  terminator */

static char *fieldv[MAXFIELD];
static struct	stat	stbuf;
static char *cmd;		/* basename of this command */
static char *getfs();
static void list_res(char *fsname);
static void get_data(char *s);
void perror();

main(argc, argv)
int argc;
char **argv;
{
	static int invalid();
	extern char *optarg;
	extern int optind;
	FILE *dfp;		/* fp for dfs list */
	int c, err = 0;
	char subcmd[BUFSIZE];	/* fs specific command */
	char *fsname = NULL;	/* file system name */
	char *desc = NULL;	/* for -d */
	char *opts = NULL;	/* -o options */
	char **nargv;		/* new argv list */
	int nargc = 0;		/* new argc */
	int optnum;
	struct utmpx *utmpxp;
	struct stat st;
	int showall = (argc <= 1);	/* show all resources */
	static char usage[] =
		"usage: %s [-F fstype] [-o fs_options ]"
		" [-d description] [pathname [resourcename]]\n";

	cmd = strrchr(argv[0], '/');	/* find the basename */
	if (cmd)
		++cmd;
	else
		cmd = argv[0];

	while ((c = getopt(argc, argv, "F:d:o:")) != -1)
		switch (c) {
		case 'F':
			err |= (fsname != NULL);	/* at most one -F */
			fsname = optarg;
			break;
		case 'd':			/* description */
			err |= (desc != NULL);		/* at most one -d */
			desc = optarg;
			break;
		case 'o':			/* fs specific options */
			err |= (opts != NULL);		/* at most one -o */
			opts = optarg;
			break;
		case '?':
			err = 1;
			break;
		}
	if (err) {
		(void) fprintf(stderr, usage, cmd);
		exit(1);
	}

	if ((dfp = fopen(DFSTYPES, "r")) == NULL) {
		(void) fprintf(stderr, "%s: cannot open %s\n", cmd, DFSTYPES);
		exit(1);
	}

	/* allocate a block for the new argv list */
	if (!(nargv = (char **)malloc(sizeof (char *)*(argc-optind+ARGVPAD)))) {
		(void) fprintf(stderr, "%s: malloc failed.\n", cmd);
		exit(1);
	}
	optnum = optind;
	nargv[nargc++] = cmd;
	if (opts) {
		nargv[nargc++] = "-o";
		nargv[nargc++] = opts;
	}
	if (desc) {
		nargv[nargc++] = "-d";
		nargv[nargc++] = desc;
	}
	for (; optind <= argc; ++optind)	/* this copies the last NULL */
		nargv[nargc++] = argv[optind];

	if (showall) {		/* share with no args -- show all dfs's */
		while (fsname = getfs(dfp)) {
			list_res(fsname);
		}
		(void) fclose(dfp);
		exit(0);
	}

	if (fsname) {		/* generate fs specific command name */
		if (invalid(fsname, dfp)) {	/* valid ? */
			(void) fprintf(stderr,
				"%s: invalid file system name\n", cmd);
			(void) fprintf(stderr, usage, cmd);
			exit(1);
		}
		if (argc <= 3 && argc == optnum) {
			/* list shared resources for share -F fstype */
			list_res(fsname);
			exit(0);
		}
		else
			(void) sprintf(subcmd, FSCMD, fsname, cmd);
	} else if (fsname = getfs(dfp))		/* use 1st line in dfstypes */
		(void) sprintf(subcmd, FSCMD, fsname, cmd);
	else {
		(void) fprintf(stderr, "%s: no file systems in %s\n",
				cmd, DFSTYPES);
		(void) fprintf(stderr, usage, cmd);
		exit(1);
	}
	/*
	 * if sharetab is older than boot time then remove it.
	 * This will happen only for the first time share is
	 * called after boot.
	 */
	if ((stat(SHAREFILE, &st) == 0) && /* does sharetab exist? */
		(utmpxp = getutxent()) != NULL && /* does utmpx exist? */
			(utmpxp->ut_xtime > st.st_mtime)) /* sharetab older? */
		(void) truncate(SHAREFILE, 0);

	(void) execvp(subcmd, nargv);
	perror(subcmd);
	return (1);
	/*NOTREACHED*/
}


/*
 *	invalid(name, f)  -  return non-zero if name is not in
 *			     the list of fs names in file f
 */

static int
invalid(name, f)
char *name;		/* file system name */
FILE *f;		/* file of list of systems */
{
	char *s;

	while (s = getfs(f))	/* while there's still hope ... */
		if (strcmp(s, name) == 0)
			return (0);	/* we got it! */
	return (1);
}


/*
 *  getfs(fp) - get the next file system name from fp
 *       ignoring lines starting with a #.
 *       All leading whitespace is discarded.
 */

static char buf[BUFSIZE];

static char *
getfs(fp)
FILE *fp;
{
	register char *s;

	while (s = fgets(buf, BUFSIZE, fp)) {
		while (isspace(*s))	/* leading whitespace doesn't count */
			++s;
		if (*s != '#') {	/* not a comment */
			char *t = s;

			while (!isspace(*t))	/* get the token */
				++t;
			*t = '\0';		/* ignore rest of line */
			return (s);
		}
	}
	return (NULL);	/* that's all, folks! */
}

/* list data in /etc/dfs/sharetab in adv(1) format */

static void
list_res(fsname)
char *fsname;
{
	char	advbuf[BUFSIZE];
	FILE	*fp;


	if (stat(SHAREFILE, &stbuf) != -1) {
		if ((fp = fopen(SHAREFILE, "r")) == NULL) {
			(void) fprintf(stderr, "%s: cannot open <%s>\n",
				cmd, SHAREFILE);
			exit(1);
		}
		while (fgets(advbuf, BUFSIZE, fp)) {
			get_data(advbuf);
			if (strcmp(fieldv[2], fsname) == 0) {
				(void) printf("%-14.14s", fieldv[1]);
				(void) printf("  %s  ", fieldv[0]);
				(void) printf(" %s ", fieldv[3]);
				if (*fieldv[4])		/* description */
					(void) printf("  \"%s\" ", fieldv[4]);
				else
					(void) printf("  \"\"  ");
				(void) printf("\n");
			}
		}
		(void) fclose(fp);
	}
}

static char empty[] = "";

static void
get_data(s)
char	*s;
{
	register int fieldc = 0;

	/*
	 *	This function parses an advertise entry from
	 *	/etc/dfs/sharetab and sets the pointers appropriately.
	 *	fieldv[0] :  pathname
	 *	fieldv[1] :  resource
	 *	fieldv[2] :  fstype
	 *	fieldv[3] :  options
	 *	fieldv[4] :  description
	 */

	while ((*s != '\n') && (*s != '\0') && (fieldc < 5)) {
		while (isspace(*s))
			s++;
		fieldv[fieldc++] = s;

		if (fieldc == 5) {	/* get the description field */
			if (fieldv[4][strlen(fieldv[4])-1] == '\n')
				fieldv[4][strlen(fieldv[4])-1] = '\0';
			break;
		}
		while (*s && !isspace(*s)) ++s;
		if (*s)
			*s++ = '\0';
	}
	while (fieldc < 5)
		fieldv[fieldc++] = empty;
}
