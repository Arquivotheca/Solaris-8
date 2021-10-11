/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getspent_r.c	1.7	98/04/10	SMI"
/*LINTLIBRARY*/

#pragma weak setspent	= _setspent
#pragma weak endspent	= _endspent
#pragma weak getspnam_r	= _getspnam_r
#pragma weak getspent_r	= _getspent_r
#pragma weak fgetspent_r = _fgetspent_r

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <sys/types.h>
#include <shadow.h>
#include <stdlib.h>
#include <string.h>
#include <nss_dbdefs.h>
#include <stdio.h>
#include <synch.h>

int str2spwd(const char *, int, void *,
	char *, int);

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

static void
_nss_initf_shadow(nss_db_params_t *p)
{
	p->name	= NSS_DBNAM_SHADOW;
	p->config_name    = NSS_DBNAM_PASSWD;	/* Use config for "passwd" */
	p->default_config = NSS_DEFCONF_PASSWD;
}

struct spwd *
getspnam_r(const char *name, struct spwd *result, char *buffer, int buflen)
{
	nss_XbyY_args_t arg;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2spwd);
	arg.key.name = name;
	(void) nss_search(&db_root, _nss_initf_shadow, \
		    NSS_DBOP_SHADOW_BYNAME, &arg);
	return (struct spwd *) NSS_XbyY_FINI(&arg);
}

void
setspent(void)
{
	nss_setent(&db_root, _nss_initf_shadow, &context);
}

void
endspent(void)
{
	nss_endent(&db_root, _nss_initf_shadow, &context);
	nss_delete(&db_root);
}

struct spwd *
getspent_r(struct spwd *result, char *buffer, int buflen)
{
	nss_XbyY_args_t arg;
	char		*nam;

	/* In getXXent_r(), protect the unsuspecting caller from +/- entries */

	do {
		NSS_XbyY_INIT(&arg, result, buffer, buflen, str2spwd);
		/* No key to fill in */
		(void) nss_getent(&db_root, _nss_initf_shadow, &context, &arg);
	} while (arg.returnval != 0 &&
		    (nam = ((struct spwd *)arg.returnval)->sp_namp) != 0 &&
		    (*nam == '+' || *nam == '-'));

	return (struct spwd *) NSS_XbyY_FINI(&arg);
}

struct spwd *
fgetspent_r(FILE *f, struct spwd *result, char *buffer, int buflen)
{
	extern void	_nss_XbyY_fgets(FILE *, nss_XbyY_args_t *);
	nss_XbyY_args_t	arg;

	/* ... but in fgetXXent_r, the caller deserves any +/- entry he gets */

	/* No key to fill in */
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2spwd);
	_nss_XbyY_fgets(f, &arg);
	return (struct spwd *) NSS_XbyY_FINI(&arg);
}

typedef const char *constp;

static int	/* 1 means success and more input, 0 means error or no more */
getfield(constp *nextp, constp limit, int uns, void *valp)
{
	constp		p = *nextp;
	char		*endfield;
	char		numbuf[12];  /* Holds -2^31 and trailing \0 */
	size_t		len;

	if (p >= limit) {
		return (0);
	}
	if (*p == ':') {
		p++;
		*nextp = p;
		return (p < limit);
	}
	if ((len = limit - p) > sizeof (numbuf) - 1) {
		len = sizeof (numbuf) - 1;
	}
	/*
	 * We want to use strtol() and we have a readonly non-zero-terminated
	 *   string, so first we copy and terminate the interesting bit.
	 *   Ugh.  (It's convenient to terminate with a colon rather than \0).
	 */
	if ((endfield = memccpy(numbuf, p, ':', len)) == 0) {
		if (len != limit - p) {
			/* Error -- field is too big to be a legit number */
			return (0);
		}
		numbuf[len] = ':';
		p = limit;
	} else {
		p += (endfield - numbuf);
	}
	if (uns) {
		*((unsigned int *)valp) = (unsigned int) strtoul(numbuf, &endfield, 10);
	} else {
		*((int *)valp) = (int) strtol(numbuf, &endfield, 10);
	}
	if (*endfield != ':') {
		/* Error -- expected <integer><colon>, got something else */
		return (0);
	}
	*nextp = p;
	return (p < limit);
}

/*
 *  str2spwd() -- convert a string to a shadow passwd entry.  The parser is
 *	more liberal than the passwd or group parsers;  since it's legitimate
 *	for almost all the fields here to be blank, the parser lets one omit
 *	any number of blank fields at the end of the entry.  The acceptable
 *	forms for '+' and '-' entries are the same as those for normal entries.
 *  === Is this likely to do more harm than good?
 *
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
str2spwd(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	struct spwd	*shadow	= (struct spwd *)ent;
	const char	*p = instr, *limit;
	char	*bufp;
	int	black_magic;
	size_t	lencopy;

	limit = p + lenstr;
	if ((p = memchr(instr, ':', lenstr)) == 0 ||
		++p >= limit ||
		(p = memchr(p, ':', limit - p)) == 0) {
		lencopy = (size_t)lenstr;
		p = 0;
	} else {
		lencopy = p - instr;
		p++;
	}
	if (lencopy + 1 > buflen) {
		return (NSS_STR_PARSE_ERANGE);
	}
	(void) memcpy(buffer, instr, lencopy);
	buffer[lencopy] = 0;

	black_magic = (*instr == '+' || *instr == '-');
	shadow->sp_namp = bufp = buffer;
	shadow->sp_pwdp	= 0;
	shadow->sp_lstchg = -1;
	shadow->sp_min	= -1;
	shadow->sp_max	= -1;
	shadow->sp_warn	= -1;
	shadow->sp_inact = -1;
	shadow->sp_expire = -1;
	shadow->sp_flag	= 0;

	if ((bufp = strchr(bufp, ':')) == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	*bufp++ = '\0';

	shadow->sp_pwdp = bufp;
	if (instr == 0) {
		if ((bufp = strchr(bufp, ':')) == 0) {
			if (black_magic)
				return (NSS_STR_PARSE_SUCCESS);
			else
				return (NSS_STR_PARSE_PARSE);
		}
		*bufp++ = '\0';
		p = bufp;
	} /* else p was set when we copied name and passwd into the buffer */

	if (!getfield(&p, limit, 0, &shadow->sp_lstchg))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_min))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_max))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_warn))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_inact))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 0, &shadow->sp_expire))
			return (NSS_STR_PARSE_SUCCESS);
	if (!getfield(&p, limit, 1, &shadow->sp_flag))
			return (NSS_STR_PARSE_SUCCESS);
	if (p != limit) {
		/* Syntax error -- garbage at end of line */
		return (NSS_STR_PARSE_PARSE);
	}
	return (NSS_STR_PARSE_SUCCESS);
}
