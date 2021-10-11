/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * $Id: pmodes.c,v 1.23 1999/03/22 14:51:16 casper Exp $
 *
 *
 * Program to list files from packages with modes that are to
 * permissive.  Usage:
 *
 *	pmodes [options] pkgdir ...
 *
 * Pmodes currently has 4 types of modes that are changed:
 *
 *	m	remove group/other write permissions of all files,
 *		except those in the exceptions list.
 *	w	remove user write permission for executables that
 *		are not root owned.
 *	s	remove g/o read permission for set-uid/set-gid executables
 *	o	change the owner of files/directories that can be safely
 *		chowned to root.
 *
 *	Any combination of changes can be switched of by specifying -X
 *
 *	The -n option will create a "FILE.new" file for all changed
 *	pkgmap/prototype files.
 *	The -D option will limit changes to directories only.
 *
 * output:
 *
 * d m oldmode -> newmode pathname
 * | ^ whether the file/dir is group writable or even world writable
 * > type of file.
 * d o owner -> newowner pathname [mode]
 *
 *
 * Casper Dik (Casper.Dik@Holland.Sun.COM)
 */

#pragma ident	"@(#)pmodes.c	1.1	99/03/22 SMI"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>

static char *exceptions [] = {
#include "exceptions.h"
};

#define	PROTO "prototype_"

#define	DEFAULT_SU 0
#define	DEFAULT_OWNER 1
#define	DEFAULT_MODES 1
#define	DEFAULT_USERWRITE 1
#define	DEFAULT_DIRSONLY 0
#define	DEFAULT_EDITABLE 1

static int nexceptions = sizeof (exceptions)/sizeof (char *);
static int dosu = DEFAULT_SU;
static int doowner = DEFAULT_OWNER;
static int domodes = DEFAULT_MODES;
static int douserwrite = DEFAULT_USERWRITE;
static int dirsonly = DEFAULT_DIRSONLY;
static int editable = DEFAULT_EDITABLE;
static int makenew = 0;
static int installnew = 0;
static int diffout = 0;

static void update_map(char *, char *, int);

static char *program;

static void
usage(void) {
	(void) fprintf(stderr, "Usage: %s [-DowsnNmde] pkgdir ...\n", program);
	exit(1);
}

int
main(int argc, char **argv)
{
	char buf[8192];
	int c;
	extern int optind, opterr;

	opterr = 0;

	program = argv[0];

	while ((c = getopt(argc, argv, "eDowsnNmd")) != EOF) {
		switch (c) {
		case 's': dosu = !DEFAULT_SU; break;
		case 'o': doowner = !DEFAULT_OWNER; break;
		case 'm': domodes = !DEFAULT_MODES; break;
		case 'w': douserwrite = !DEFAULT_USERWRITE; break;
		case 'D': dirsonly = !DEFAULT_DIRSONLY; break;
		case 'e': editable = !DEFAULT_EDITABLE; break;
		case 'N': installnew = 1; /* FALLTHROUGH */
		case 'n': makenew = 1; break;
		case 'd': diffout = 1; break;
		default:
		case '?': usage(); break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	for (; *argv; argv++) {
		FILE *info;
		char name[MAXPATHLEN];
		char basedir[MAXPATHLEN] = "/";
		int basedir_len;

		(void) sprintf(name, "%s/pkginfo", *argv);

		/* if there's no pkginfo file, it could be a prototype area */

		if (access(name, R_OK) != 0)
			(void) strcat(name, ".tmpl");

		info = fopen(name, "r");
		if (info == 0) {
			(void) fprintf(stderr, "Can't open pkginfo in %s\n",
				*argv);
			continue;
		}

		while (fgets(buf, sizeof (buf), info) != 0) {
			if (strncmp(buf, "BASEDIR=", 8) == 0) {
				(void) strcpy(basedir, buf+8);
				basedir[strlen(basedir)-1] = '\0';
				break;
			}
		}

		(void) fclose(info);

		basedir_len = strlen(basedir);
		if (basedir_len != 1)
			basedir[basedir_len++] = '/';

		(void) sprintf(name, "%s/pkgmap", *argv);
		if (access(name, R_OK) == 0)
			update_map(name, basedir, basedir_len);
		else {
			DIR *d = opendir(*argv);
			struct dirent *de;

			if (d == NULL) {
				(void) fprintf(stderr,
					"Can't read directory \"%s\"\n", *argv);
				continue;
			}
			while (de = readdir(d)) {
				/* Skip files with .old or .new suffix */
				if (strncmp(de->d_name, PROTO,
					    sizeof (PROTO) - 1) == 0 &&
					strstr(de->d_name, ".old") == NULL &&
					strstr(de->d_name, ".new") == NULL) {
					(void) sprintf(name, "%s/%s", *argv,
					    de->d_name);
					update_map(name, basedir, basedir_len);
				}
			}
			(void) closedir(d);
		}
	}
	return (0);
}

#define	NEXTWORD(tmp, end) \
	do { \
		tmp = strpbrk(tmp, "\t "); if (!tmp) fatal(name, lineno);\
		end = tmp++;\
		while (*tmp && isspace(*tmp)) tmp++;\
	} while (0)

static void
fatal(const char *file, int line)
{
	(void) fprintf(stderr, "pmodes: error in line format in %s at %d\n",
		file, line);
	exit(1);
}

struct parsed_line {
	char *start;		/* buffer start */
	char *rest;		/* buffer after owner */
	char *owner;		/* same size as ut_user */
	char *old_owner;	/* same size as ut_user */
	char group[16];		/* whatever */
	int  modelen;		/* number of mode bytes (3 or 4); */
	int mode;		/* the complete file mode */
	char path[MAXPATHLEN];	/* NUL terminated pathname */
	char type;		/* */
	char realtype;		/* */
};

#define	LINE_OK		0
#define	LINE_IGNORE	1
#define	LINE_ERROR	2

static void
put_line(FILE *f, struct parsed_line *line)
{
	if (f != NULL)
		if (line->rest)
			(void) fprintf(f, "%s%.*o %s %s", line->start,
			    line->modelen, line->mode, line->owner, line->rest);
		else
			(void) fputs(line->start, f);
}

/*
 * the first field is the path, the second the type, the
 * third the class, the fourth the mode, when appropriate.
 * We're interested in
 *		f (file)
 *		e (edited file)
 *		v (volatile file)
 *		d (directory)
 *		c (character devices)
 *		b (block devices)
 */

static int
parse_line(struct parsed_line *parse, char *buf, const char *name, int lineno)
{
	char *tmp;
	char *p = buf;
	char *end, *q;

	parse->start = buf;
	parse->rest = 0;		/* makes put_line work */

	/* Trim trailing spaces */
	end = buf + strlen(buf);
	while (end > buf+1 && isspace(end[-2])) {
		end -= 1;
		end[-1] = end[0];
		end[0] = '\0';
	}

	if (*p == '#' || *p == ':' || *p == '\n' || *p == '!')
		return (LINE_IGNORE);

	/*
	 * Parse the pkgmap line:
	 * [<number>] <type> <class> <path> [<major> <minor>]
	 * [ <mode> <owner> <group> .... ]
	 */

	/* Skip first column for non-prototype (i.e., pkgmap) files */
	if (isdigit(*p))
		NEXTWORD(p, end);

	parse->realtype = parse->type = *p;

	switch (parse->type) {
	case 'i': case 's': case 'l':
		return (LINE_IGNORE);
	}

	NEXTWORD(p, end);

	/* skip class */
	NEXTWORD(p, end);

	/* p now points to pathname */
	tmp = p;
	NEXTWORD(p, end);

	/* end points to space after name */
	(void) strncpy(parse->path, tmp, end - tmp);
	parse->path[end - tmp] = '\0';

	switch (parse->type) {
	case 'e':
	case 'v':
		/* type 'e' and 'v' are files, just like 'f', use 'f' in out */
		parse->type = 'f';
		/* FALLTHROUGH */
	case 'f':
	case 'd':
	case 'p': /* FIFO - assume mode is sensible, don't treat as file */
		break;

	case 'x': /* Exclusive directory */
		parse->type = 'd';
		break;

	/* device files have class major minor, skip */
	case 'c':
	case 'b':
		NEXTWORD(p, end); NEXTWORD(p, end);
		break;

	default:
		(void) fprintf(stderr, "Unknown type '%c', %s:%d\n",
			    parse->type, name, lineno);
		exit(1);
	}
	tmp = p;
	NEXTWORD(p, end);

	/*
	 * the mode is either a 4 digit number (file is sticky/set-uid or
	 * set-gid or the mode has a leading 0) or a three digit number
	 * mode has all the mode bits, mode points to the three least
	 * significant bit so fthe mode
	 */
	parse->mode = 0;
	for (q = tmp; q < end; q++) {
		if (!isdigit(*q) || *q > '7') {
			(void) fprintf(stderr,
			    "Warning: Unparseble mode \"%.*s\" at %s:%d\n",
			    end-tmp, tmp, name, lineno);
			return (LINE_IGNORE);
		}
		parse->mode <<= 3;
		parse->mode += *q - '0';
	}
	parse->modelen = end - tmp;
	tmp[0] = '\0';

	parse->old_owner = parse->owner = p;

	NEXTWORD(p, end);

	parse->rest = end+1;
	*end = '\0';

	(void) memset(parse->group, 0, sizeof (parse->group));
	(void) strncpy(parse->group, end+1, strcspn(end+1, " \t\n"));

	return (LINE_OK);
}

static void
update_map(char *name, char *basedir, int basedir_len)
{
	char buf[8192];
	int i;
	FILE *map, *newmap;
	char newname[MAXPATHLEN];
	int nchanges = 0;
	unsigned int lineno = 0;
	struct parsed_line line;

	map = fopen(name, "r");
	if (map == 0) {
		(void) fprintf(stderr, "Can't open \"%s\"\n", name);
		return;
	}
	(void) strcpy(newname, name);
	(void) strcat(newname, ".new");
	if (makenew) {
		newmap = fopen(newname, "w");
		if (newmap == 0)
			(void) fprintf(stderr, "Can't open %s for writing\n",
			    name);
	} else
		newmap = 0;

	nchanges = 0;

	for (; fgets(buf, sizeof (buf), map) != 0; put_line(newmap, &line)) {

		int root_owner, mode_diff = 0;
		int changed = 0;

		lineno ++;

		switch (parse_line(&line, buf, name, lineno)) {
		case LINE_IGNORE: continue;
		case LINE_ERROR: exit(1);
		}

		if (dirsonly && line.type != 'd')
			continue;

		root_owner = strcmp(line.owner, "root") == 0;
		if (dosu && line.type == 'f' && (line.mode & (S_ISUID|S_ISGID)))
			mode_diff = line.mode & (S_IRGRP|S_IROTH);

		/*
		 * The following heuristics are used to determine whether a file
		 * can be safely chown'ed to root:
		 *	- it's not set-uid.
		 *	and one of the following applies:
		 *	    - it's not writable by the current owner and is
		 *	    group/world readable
		 *	    - it's world executable and a file
		 *	    - owner, group and world permissions are identical
		 *	    - it's a bin owned directory or a "non-volatile"
		 *	    file (any owner) for which group and other r-x
		 *	    permissions are identical, or it's a bin owned
		 *	    executable or it's a /etc/security/dev/ device
		 */

		if (doowner && !(line.mode & S_ISUID) &&
		    !root_owner &&
		    ((!(line.mode & S_IWUSR) &&
			(line.mode&(S_IRGRP|S_IROTH)) == (S_IRGRP|S_IROTH)) ||
			(line.type == 'f' && (line.mode & S_IXOTH)) ||
			((line.mode & 07) == ((line.mode>>3) & 07) &&
			    (line.mode & 07) == ((line.mode>>6) & 07) &&
			    strcmp(line.owner, "uucp") != 0) ||
			((line.type == 'd' && strcmp(line.owner, "bin") == 0 ||
			    (editable && strcmp(line.owner, "bin") == 0  ?
				    line.type : line.realtype)  == 'f') &&
			    ((line.mode & 05) == ((line.mode>>3) & 05) ||
				(line.mode & 0100) &&
				strcmp(line.owner, "bin") == 0) &&
			    ((line.mode & 0105) != 0 ||
				basedir_len < 18 &&
				strncmp(basedir, "/etc/security/dev/",
				    basedir_len) == 0 &&
				strncmp(line.path, "/etc/security/dev/"
				    + basedir_len, 18 - basedir_len) == 0)))) {
			if (!diffout)
				(void) printf("%c o %s -> root %s%s [%.*o]\n",
					line.realtype, line.owner, basedir,
					line.path, line.modelen, line.mode);
			line.owner = "root";
			root_owner = 1;
			changed = 1;
		}
		/*
		 * Strip user write bit if owner != root and executable by user.
		 * root can write even if no write bits set
		 * Could prevent  executables from being overwritten.
		 */
		if (douserwrite && line.type == 'f' && !root_owner &&
		    (line.mode & (S_IWUSR|S_IXUSR)) == (S_IWUSR|S_IXUSR))
			mode_diff |= S_IWUSR;


		if (domodes && (line.mode & (S_IWGRP|S_IWOTH)) != 0 &&
		    (line.mode & S_ISVTX) == 0) {
			if (basedir_len <= 1) { /* root dir */
				for (i = 0; i < nexceptions; i++) {
					if (strcmp(line.path,
					    exceptions[i]+basedir_len) == 0)
						break;
				}
			} else {
				for (i = 0; i < nexceptions; i++) {
					if (strncmp(basedir, exceptions[i],
						basedir_len) == 0 &&
					    strcmp(line.path,
						exceptions[i]+basedir_len) == 0)
						break;
				}
			}
			if (i == nexceptions)
				mode_diff |= line.mode & (S_IWGRP|S_IWOTH);
		}

		if (mode_diff) {
			int oldmode = line.mode;

			line.mode &= ~mode_diff;

			if (line.mode != oldmode) {
				if (!diffout)
					printf("%c %c %04o -> %04o %s%s\n",
					    line.realtype,
					    (mode_diff & (S_IRGRP|S_IROTH)) ?
						's' : 'm',
						oldmode, line.mode, basedir,
						line.path);
				changed = 1;
			}
		}
		nchanges += changed;
		if (diffout && changed) {
			(void) printf("< %c %04o %s %s %s%s\n", line.realtype,
			    line.mode | mode_diff, line.old_owner, line.group,
			    basedir, line.path);
			(void) printf("> %c %04o %s %s %s%s\n", line.realtype,
			    line.mode, line.owner, line.group, basedir,
			    line.path);
		}
	}
	(void) fclose(map);

	if (newmap != NULL) {
		(void) fflush(newmap);
		if (ferror(newmap)) {
			(void) fprintf(stderr, "Error writing %s\n", name);
			return;
		}
		(void) fclose(newmap);
		if (nchanges == 0)
			(void) unlink(newname);
		else if (installnew) {
			char oldname[MAXPATHLEN];

			(void) strcpy(oldname, name);
			(void) strcat(oldname, ".old");
			if (rename(name, oldname) == -1 ||
			    rename(newname, name) == -1)
				(void) fprintf(stderr,
				    "Couldn't install %s: %s\n",
				    newname, strerror(errno));
		}
	}
}
