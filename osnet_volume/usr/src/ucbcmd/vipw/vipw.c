/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)vipw.c	1.19	99/01/15 SMI"	/* SVr4.0 1.1	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fcntl.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>

/*
 * Password file editor with locking.
 */
char	*ptemp = "/etc/ptmp";
char	*stemp = "/etc/stmp";
char	*passwd = "/etc/passwd";
char	*shadow = "/etc/shadow";
char	buf[BUFSIZ];
char	*getenv();
char	*index();
extern	int errno;

main()
{
	int fd;
	FILE *ft, *fp;
	char *editor;
	int ok = 0;
	time_t o_mtime, n_mtime;
	struct stat osbuf, sbuf, oshdbuf, shdbuf;
	char c;

	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	setbuf(stderr, (char *)NULL);

	editor = getenv("VISUAL");
	if (editor == 0)
		editor = getenv("EDITOR");
	if (editor == 0)
		editor = "vi";

	(void)umask(0077);
	if (stat(passwd, &osbuf) < 0) {
                (void)fprintf(stderr,"vipw: can't stat passwd file.\n");
                goto bad;
        } 

	if (copyfile(passwd, ptemp))
		goto bad;

	if (stat(ptemp, &sbuf) < 0) {
                (void)fprintf(stderr,
			"vipw: can't stat ptemp file, %s unchanged\n",
			passwd);
		goto bad;
	}

	o_mtime = sbuf.st_mtime;

	if (editfile(editor, ptemp, passwd, &n_mtime)) {
		if (sanity_check(ptemp, &n_mtime, passwd))
			goto bad;
		if (o_mtime >= n_mtime)
			goto bad;
	}

	ok++;
	if (o_mtime < n_mtime) {
		fprintf(stdout, "\nYou have modified the password file.\n");
		fprintf(stdout,
	"Press 'e' to edit the shadow file for consistency,\n 'q' to quit: ");
		if ((c = getchar()) == 'q') {
			if (chmod(ptemp, (osbuf.st_mode & 0644)) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", ptemp);
				perror("chmod");
				goto bad;
			}
			if (rename(ptemp, passwd) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", ptemp);
				perror("rename");
				goto bad;
			}
			if (((osbuf.st_gid != sbuf.st_gid) ||
					(osbuf.st_uid != sbuf.st_uid)) &&
			(chown(passwd, osbuf.st_uid, osbuf.st_gid) < 0)) {
				(void) fprintf(stderr, "vipw: %s ", ptemp);
				perror("chown");
			}
			goto bad;
		} else if (c == 'e') {
			if (stat(shadow, &oshdbuf) < 0) {
				(void) fprintf(stderr,
					"vipw: can't stat shadow file.\n");
				goto bad;
			}

			if (copyfile(shadow, stemp))
				goto bad;
			if (stat(stemp, &shdbuf) < 0) {
				(void) fprintf(stderr,
					"vipw: can't stat stmp file.\n");
				goto bad;
			}

			if (editfile(editor, stemp, shadow, &o_mtime))
				goto bad;
			ok++;
			if (chmod(ptemp, (osbuf.st_mode & 0644)) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", ptemp);
				perror("chmod");
				goto bad;
			}
			if (chmod(stemp, (oshdbuf.st_mode & 0400)) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", stemp);
				perror("chmod");
				goto bad;
			}
			if (rename(ptemp, passwd) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", ptemp);
				perror("rename");
				goto bad;
			}
			if (((osbuf.st_gid != sbuf.st_gid) ||
					(osbuf.st_uid != sbuf.st_uid)) &&
			(chown(passwd, osbuf.st_uid, osbuf.st_gid) < 0)) {
				(void) fprintf(stderr, "vipw: %s ", ptemp);
				perror("chown");
			}
			if (rename(stemp, shadow) < 0) {
				(void) fprintf(stderr, "vipw: %s: ", stemp);
				perror("rename");
				goto bad;
			} else if (((oshdbuf.st_gid != shdbuf.st_gid) ||
					(oshdbuf.st_uid != shdbuf.st_uid)) &&
			(chown(shadow, oshdbuf.st_uid, oshdbuf.st_gid) < 0)) {
				(void) fprintf(stderr, "vipw: %s ", stemp);
				perror("chown");
				}
		}
	}
bad:
	(void) unlink(ptemp);
	(void) unlink(stemp);
	exit(ok ? 0 : 1);
	/* NOTREACHED */
}


copyfile(from, to)
char *from, *to;
{
	int fd;
	FILE *fp, *ft;

	fd = open(to, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0) {
		if (errno == EEXIST) {
			(void) fprintf(stderr, "vipw: %s file busy\n", from);
			exit(1);
		}
		(void) fprintf(stderr, "vipw: "); perror(to);
		exit(1);
	}
	ft = fdopen(fd, "w");
	if (ft == NULL) {
		(void) fprintf(stderr, "vipw: "); perror(to);
		return( 1 );
	}
	fp = fopen(from, "r");
	if (fp == NULL) {
		(void) fprintf(stderr, "vipw: "); perror(from);
		return( 1 );
	}
	while (fgets(buf, sizeof (buf) - 1, fp) != NULL)
		fputs(buf, ft);
	(void) fclose(ft);
	(void) fclose(fp);
	return( 0 );
}

editfile(editor, temp, orig, mtime)
char *editor, *temp, *orig;
time_t *mtime;
{

	(void)sprintf(buf, "%s %s", editor, temp);
	if (system(buf) == 0) {
		return (sanity_check(temp, mtime, orig));
	}
	return(1);
}


validsh(rootsh)
	char	*rootsh;
{

	char	*sh, *getusershell();
	int	ret = 0;

	setusershell();
	while((sh = getusershell()) != NULL ) {
		if( strcmp( rootsh, sh) == 0 ) {
			ret = 1;
			break;
		}
	}
	endusershell();
	return(ret);
}

/*
 * sanity checks
 * return 0 if ok, 1 otherwise
 */
sanity_check(temp, mtime, orig)
char *temp, *orig;
time_t *mtime;
{
	int i, ok = 0;
	FILE *ft;
	struct stat sbuf;
	int isshadow = 0;

	if (!strcmp(orig, shadow))
		isshadow = 1;

	/* sanity checks */
	if (stat(temp, &sbuf) < 0) {
		(void)fprintf(stderr,
		    "vipw: can't stat %s file, %s unchanged\n",
		    temp, orig);
		return(1);
	}
	*mtime = sbuf.st_mtime;
	if (sbuf.st_size == 0) {
		(void)fprintf(stderr, "vipw: bad %s file, %s unchanged\n",
		    temp, orig);
		return(1);
	}
	ft = fopen(temp, "r");
	if (ft == NULL) {
		(void)fprintf(stderr,
		    "vipw: can't reopen %s file, %s unchanged\n",
		    temp, orig);
		return(1);
	}

	while (fgets(buf, sizeof (buf) - 1, ft) != NULL) {
		register char *cp;

		cp = index(buf, '\n');
		if (cp == 0)
			continue;	/* ??? allow very long lines
					 * and passwd files that do
					 * not end in '\n' ???
					 */
		*cp = '\0';

		cp = index(buf, ':');
		if (cp == 0)		/* lines without colon
					 * separated fields
					 */
			continue;
		*cp = '\0';

		if (strcmp(buf, "root"))
			continue;

		/* root password */
		*cp = ':';
		cp = index(cp + 1, ':');
		if (cp == 0)
			goto bad_root;

		/* root uid for password */
		if (!isshadow)
			if (atoi(cp + 1) != 0) {

				(void)fprintf(stderr, "root UID != 0:\n%s\n",
				    buf);
				break;
			}
		/* root uid for passwd and sp_lstchg for shadow */
		cp = index(cp + 1, ':');
		if (cp == 0)
			goto bad_root;

		/* root's gid for passwd and sp_min for shadow*/
		cp = index(cp + 1, ':');
		if (cp == 0)
			goto bad_root;

		/* root's gecos for passwd and sp_max for shadow*/
		cp = index(cp + 1, ':');
		if (isshadow) {
			for (i=0; i<3; i++)
				if ((cp = index(cp + 1, ':')) == 0)
					goto bad_root;
		} else {
			if (cp == 0) {
bad_root:		(void)fprintf(stderr,
				    "Missing fields in root entry:\n%s\n", buf);
				break;
			}
		}
		if (!isshadow) {
			/* root's login directory */
			if (strncmp(++cp, "/:", 2)) {
				(void)fprintf(stderr,
				    "Root login directory != \"/\" or%s\n%s\n",
				    " default shell missing:", buf);
				break;
			}

			/* root's login shell */
			cp += 2;
			if (*cp && ! validsh(cp)) {
				(void)fprintf(stderr,
				    "Invalid root shell:\n%s\n", buf);
				break;
			}
		}

		ok++;
	}
	(void)fclose(ft);
	if (ok)
		return(0);
	else {
		(void)fprintf(stderr,
		    "vipw: you mangled the %s file, %s unchanged\n",
		    temp, orig);
		return(1);
	}			
}
