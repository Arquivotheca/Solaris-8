
/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's UNIX(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *      All rights reserved.
 */
/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)yp_stubs.c	1.10	96/11/26 SMI"
/*LINTLIBRARY*/

/*
 * yp_stubs.c
 *
 * This is the interface to NIS library calls in libnsl, that
 * are made through dlopen() and dlsym() to avoid linking
 * libnsl. The primary reason for this file is to offer access
 * to yp_*() calls from within various libc routines that
 * use NIS password and shadow databases. Preferably, a cleaner
 * way should be found to accomplish inter-library dependency.
 */


#include "synonyms.h"
#include "_libc_gettext.h"
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/file.h>
#include <rpcsvc/ypclnt.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define	YPSTUB_OK 0
#define	YPSTUB_NOMEM 1
#define	YPSTUB_NOSYM 2
#define	YPSTUB_OPEN 3
#define	YPSTUB_ACCESS 4
#define	YPSTUB_SYSTEM 5

static char lnsl[] = "/usr/lib/libnsl.so";
static int ypstub_err = YPSTUB_OK;
static char *yperrbuf;

typedef struct translator {
	int		(*ypdom)(char **domain); /* yp_get_default_domain */
	int		(*ypfirst)(char *domain, char *map, char **key,
			    int *keylen, char **val, int *vallen);
			    /* yp_first */
	int		(*ypnext)(char *domain, char *map, char *inkey,
			    int inkeylen, char **outkey, int *outkeylen,
			    char **val, int *vallen);	/* yp_next	*/
	int		(*ypmatch)(char *domain, char *map, char *key,
			    int keylen, char **val, int *vallen);
			    /* yp_match */
	void	*tr_fd;	/* library descriptor */
	char	tr_name[512];	/* Full path	*/
} translator_t;

static translator_t *t = NULL;
static translator_t *load_xlate(char *name);
static void ypstub_perror(char *s);

int libc_yp_get_default_domain(char **domain);
int libc_yp_first(char *domain, char *map, char **key, int *keylen,
		    char **val, int *vallen);
int libc_yp_next(char *domain, char *map, char *inkey, int inkeylen,
		    char **outkey, int *outkeylen, char **val, int *vallen);
int libc_yp_match(char *domain, char *map, char *key, int keylen,
		    char **val, int *vallen);

int
libc_yp_get_default_domain(char **domain)
{
	int retval;
	if (t == NULL) {
		if ((t = load_xlate(lnsl)) == NULL) {
		/*
		 * We don't print this to avoid annoyance to the user,
		 * other libc_yp routines will anyway do that job.
		 */
#ifdef DEBUG
			(void) ypstub_perror("NIS access from libc routines");
#endif
			return (YPERR_YPERR);
		}
	}
	retval = (*(t->ypdom))(domain);
	return (retval);
}

int
libc_yp_first(char *domain, char *map, char **key, int *keylen, char **val,
	    int *vallen)
{
	int retval;
	if (t == NULL) {
		if ((t = load_xlate(lnsl)) == NULL) {
			(void) ypstub_perror("NIS access from libc routines");
			return (YPERR_YPERR);
		}
	}
	retval = (*(t->ypfirst))(domain, map, key, keylen, val, vallen);
	return (retval);
}

int
libc_yp_next(char *domain, char *map, char *inkey, int inkeylen,
	    char **outkey, int *outkeylen, char **val, int *vallen)
{
	int retval;
	if (t == NULL) {
		if ((t = load_xlate(lnsl)) == NULL) {
			(void) ypstub_perror("NIS access from libc routines");
			return (YPERR_YPERR);
		}
	}
	retval = (*(t->ypnext))(domain, map, inkey, inkeylen,
			outkey, outkeylen, val, vallen);
	return (retval);
}

int
libc_yp_match(char *domain, char *map, char *key, int keylen, char **val,
	    int *vallen)
{
	int retval;
	if (t == NULL) {
		if ((t = load_xlate(lnsl)) == NULL) {
			(void) ypstub_perror("NIS access from libc routines");
			return (YPERR_YPERR);
		}
	}
	retval = (*(t->ypmatch))(domain, map, key, keylen, val, vallen);
	return (retval);
}

/*
 * load_xlate is a routine that will attempt to dynamically link in
 * libnsl for network access from within libc.
 */
static translator_t *
load_xlate(char *name)
{
	/* do a sanity check on the file ... */
	if (access(name, F_OK) == 0) {
		t = (translator_t *) malloc(sizeof (translator_t));
		if (!t) {
			ypstub_err = YPSTUB_NOMEM;
			return (0);
		}

		(void) strcpy(t->tr_name, name);

		/* open for linking */
		t->tr_fd = dlopen(name, RTLD_LAZY);
		if (t->tr_fd == NULL) {
			ypstub_err = YPSTUB_OPEN;
			(void) free((char *)t);
			return (0);
		}

		/* resolve the yp_get_default_domain symbol */
		t->ypdom = (int (*)(char **domain))dlsym(t->tr_fd,
			    "yp_get_default_domain");
		if (!(t->ypdom)) {
			ypstub_err = YPSTUB_NOSYM;
			(void) free((char *)t);
			return (0);
		}

		/* resolve the yp_first symbol */
		t->ypfirst = (int (*)(char *domain, char *map, char **key,
			    int *keylen, char **val, int *vallen))
			    dlsym(t->tr_fd, "yp_first");
		if (!(t->ypfirst)) {
			ypstub_err = YPSTUB_NOSYM;
			(void) free((char *)t);
			return (0);
		}

		/* resolve the yp_next symbol */
		t->ypnext = (int (*)(char *domain, char *map, char *inkey,
			    int inkeylen, char **outkey, int *outkeylen,
			    char **val, int *vallen))dlsym(t->tr_fd, "yp_next");
		if (!(t->ypnext)) {
			ypstub_err = YPSTUB_NOSYM;
			(void) free((char *)t);
			return (0);
		}

		/* resolve the yp_match symbol */
		t->ypmatch = (int (*)(char *domain, char *map, char *key,
			    int keylen, char **val, int *vallen))
			    dlsym(t->tr_fd, "yp_match");
		if (!(t->ypmatch)) {
			ypstub_err = YPSTUB_NOSYM;
			(void) free((char *)t);
			return (0);
		}
		return (t);
	}
	ypstub_err = YPSTUB_ACCESS;
	return (0);
}

static char *
_buf(void)
{
	if (yperrbuf == NULL)
		yperrbuf = (char *)malloc(128);
	return (yperrbuf);
}

/*
 * This is a routine that returns a string related to the current
 * error in ypstub_err.
 */
static char *
ypstub_sperror(void)
{
	char	*str = _buf();

	if (str == NULL)
		return (NULL);
	switch (ypstub_err) {
	case YPSTUB_OK :
		(void) sprintf(str, _libc_gettext(
					"%s: successful completion"),
					lnsl);
		break;
	case YPSTUB_NOMEM :
		(void) sprintf(str, _libc_gettext(
					"%s: memory allocation failed"),
					lnsl);
		break;
	case YPSTUB_NOSYM :
		(void) sprintf(str, _libc_gettext(
					"%s: symbol missing in shared object"),
					lnsl);
		break;
	case YPSTUB_OPEN :
		(void) sprintf(str, _libc_gettext(
					"%s: couldn't open shared object"),
					lnsl);
		break;
	case YPSTUB_ACCESS :
		(void) sprintf(str, _libc_gettext(
					"%s: shared object does not exist"),
					lnsl);
		break;
	case YPSTUB_SYSTEM:
		(void) sprintf(str, _libc_gettext(
					"%s: system error: %s"),
					lnsl, strerror(errno));
	default :
		(void) sprintf(str, _libc_gettext(
					"%s: unknown error #%d"),
					lnsl, ypstub_err);
		break;
	}
	return (str);
}

/*
 * This is a routine that prints out strings related to the current
 * error in ypstub_err. Like perror() it takes a string to print with a
 * colon first.
 */
static void
ypstub_perror(char *s)
{
	char	*err;

	err = ypstub_sperror();
	(void) fprintf(stderr, _libc_gettext("%s: %s\n"),
		s, err ? err: _libc_gettext("error"));
} 
