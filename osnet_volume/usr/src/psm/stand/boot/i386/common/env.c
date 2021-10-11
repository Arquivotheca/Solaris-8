/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)env.c	1.10	99/05/04 SMI"

#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/bootvfs.h>
#include <sys/salib.h>
#include "devtree.h"

extern void interpline(char *line, int len);

#define	ENVBUFSIZE	1024

static char *envcpy;
static int  envcpysiz;
static int  envoffset;
/* env_src_file is setup in bsh_init() before bsh is started */
char *env_src_file;

/*
 * gettoken
 *	Given buffer, give me pointer to next token.  Token
 *	defined to be any sequence of consecutive chars > ' '.
 *	Has memory between invocations.  Called with NULL srcbuf
 *	means continue in previously known buffer.
 *
 *	Also returns pointer to character following end of this
 *	token in the source string.
 */
static char *
gettoken(char *srcbuf, char **rob)
{
	static char *srccpy;
	static char *laststop;
	static char *nextstart;
	static char lastsave;
	static int  srclen;
	char *fb, *fe;

	if (srcbuf && srccpy) {
		bkmem_free(srccpy, srclen);
		srccpy = (char *)NULL;
	}

	if (srcbuf) {
		srclen = strlen(srcbuf) + 1;
		if ((srccpy = (char *)bkmem_alloc(srclen)) == (char *)NULL) {
			printf("ERROR: No memory for gettoken!");
			return (NULL);
		}
		(void) strcpy(srccpy, srcbuf);
		nextstart = srcbuf;
		laststop = srccpy;
	} else {
		if (laststop)
			*laststop = lastsave;
	}

	fb = laststop;
	while (fb && *fb && (*fb <= ' ')) fb++;
	fe = fb;
	while (fe && (*fe > ' ')) fe++;
	if (fe) {
		lastsave = *fe;
		*fe = '\0';
	}

	*rob = (nextstart += (fe - laststop));
	if (((laststop = fe) - srccpy) >= srclen - 1)
		laststop = (char *)NULL;

	return (fb);
}

char *
env_gets(char *buf, int maxchars)
{
	char *rp = buf;
	int oc = maxchars - 1;

	/* Slimy error cases */
	if (!rp || maxchars <= 0 || !envcpy || envcpysiz == 0)
		return ((char *)NULL);

	while (oc) {
		if (envoffset >= envcpysiz) {
			break;
		} else {
			*buf = *(envcpy + envoffset);
			envoffset++; buf++; oc--;
			if (*(buf-1) == '\n')
				break;
		}
	}
	*buf = '\0';

	if (oc == maxchars - 1)
		return ((char *)NULL);
	else
		return (rp);
}

int
env_puts(char *buf, int fd)
{
	return (write(fd, buf, strlen(buf)));
}

/*
 * setupenv
 *	Convert disk version of environment rc file to a RAMfile
 *	for further manipulation (if disk version exists).
 *	Also open an empty working file.
 */
static void
setupenv(void)
{
	struct stat sbuf;
	int envfd;

	envoffset = 0;
	if ((envfd = open(env_src_file, DOSFD_RAMFILE)) < 0) {
		/*
		 * No environment to copy.  Odd, but not completely
		 * impossible.
		 */
		printf("No environment variables set?\n");
		return;
	}

	if (fstat(envfd, &sbuf) == 0) {
		envcpysiz = sbuf.st_size;
		if ((envcpy = (char *)bkmem_alloc(envcpysiz)) == (char *)NULL) {
			printf("Could not allocate copy of environment\n");
			(void) close(envfd);
			envcpysiz = 0;
			return;
		}

		if (read(envfd, envcpy, envcpysiz) != envcpysiz) {
			printf("Could not read a copy of environment\n");
			(void) close(envfd);
			bkmem_free(envcpy, envcpysiz);
			envcpy = (char *)NULL;
			envcpysiz = 0;
			return;
		}

		(void) close(envfd);
		return;
	} else {
		printf("Could not get a copy of environment\n");
		(void) close(envfd);
	}
}

/*
 * teardownenv
 */
static void
teardownenv(void)
{
	if (envcpy)
		bkmem_free(envcpy, envcpysiz);
	envcpy = (char *)NULL;
	envcpysiz = 0;
}

/*ARGSUSED*/
static char *
build_envcmd(int argc, char **argv)
{
	char *cmdbuf;

	if ((cmdbuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: No space to build a setprop command!");
	} else
		(void) sprintf(cmdbuf, "setprop %s %s\n", argv[1], argv[2]);
	return (cmdbuf);
}

static void
demolish_envcmd(char *cmdbuf)
{
	if (cmdbuf)
		bkmem_free(cmdbuf, ENVBUFSIZE);
}

static void
append_env(int envfd, char *cmdbuf)
{
	if (!cmdbuf ||
	    write(envfd, cmdbuf, strlen(cmdbuf)) != strlen(cmdbuf)) {
		printf("ERROR: Unable to add new environment value!\n");
	}
	(void) close(envfd);
}

static int
copy_nonmatches(char *matchme)
{
	char *parsebuf;
	char *fw, *sw, *rl;
	int envfd;

	if ((parsebuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: Unable to retrieve current environment!\n");
		return (-1);
	}

	if ((envfd = open(env_src_file, DOSACCESS_RDWR | DOSFD_RAMFILE)) < 0) {
		printf("ERROR: Unable to get current environment!\n");
		bkmem_free(parsebuf, ENVBUFSIZE);
		return (-1);
	}

	(void) lseek(envfd, 0, SEEK_SET);
	while (env_gets(parsebuf, ENVBUFSIZE)) {
		fw = gettoken(parsebuf, &rl);
		if (fw && strcmp(fw, "setprop") == 0) {
			sw = gettoken((char *)NULL, &rl);
			if (sw && strcmp(matchme, sw) == 0)
				continue;
			else
				(void) env_puts(parsebuf, envfd);
		} else
			(void) env_puts(parsebuf, envfd);
	}

	bkmem_free(parsebuf, ENVBUFSIZE);
	return (envfd);
}

static void
display_matches(char *matchme)
{
	char *parsebuf;
	char *fw, *sw, *rl, *val;

	if ((parsebuf = (char *)bkmem_alloc(ENVBUFSIZE)) == (char *)NULL) {
		printf("ERROR: Unable to retrieve current environment!");
		return;
	}

	while (env_gets(parsebuf, ENVBUFSIZE)) {
		fw = gettoken(parsebuf, &rl);
		if (strcmp(fw, "setprop") == 0) {
			sw = gettoken((char *)NULL, &rl);
			if (!matchme || (strcmp(matchme, sw) == 0)) {
				printf("%s=", sw);
				val = gettoken((char *)NULL, &rl);
				printf("%s\n", val);
			}
		}
	}

	bkmem_free(parsebuf, ENVBUFSIZE);
}

void
printenv_cmd(int argc, char **argv)
{
	setupenv();
	display_matches((argc > 1) ? argv[1] : (char *)NULL);
	teardownenv();
}

void
setenv_cmd(int argc, char **argv)
{
	char *cmd;
	int efd;

	if (argc != 3) {
		printf("Usage: setenv property-name value\n");
		return;
	}

	setupenv();
	cmd = build_envcmd(argc, argv);
	interpline(cmd, strlen(cmd));
	if ((efd = copy_nonmatches(argv[1])) >= 0)
		append_env(efd, cmd);
	demolish_envcmd(cmd);
	teardownenv();
}
