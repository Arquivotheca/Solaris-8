/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nsparse.c	1.26	99/09/28 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <mtlib.h>
#include <synch.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <dlfcn.h>

#define	__NSS_PRIVATE_INTERFACE
#include "nsswitch_priv.h"
#undef	__NSS_PRIVATE_INTERFACE

#include <syslog.h>
#include <inet/common.h>	/* for MAX_INT */

#define	islabel(c) 	(isalnum(c) || (c) == '_')
/*
 * This file has all the routines that access the configuration
 * information.
 */

struct cons_cell_v1 { /* private to the parser */
	struct __nsw_switchconfig_v1 *sw;
	struct cons_cell_v1 *next;
};

struct cons_cell { /* private to the parser */
	struct __nsw_switchconfig *sw;
	struct cons_cell *next;
};

/*
 * Local routines
 */

static char *skip(char **, char);
static char *labelskip(char *);
static char *spaceskip(char *);
static struct __nsw_switchconfig_v1 *scrounge_cache_v1(const char *);
static struct __nsw_switchconfig *scrounge_cache(const char *);
static int add_concell_v1(struct __nsw_switchconfig_v1 *);
static int add_concell(struct __nsw_switchconfig *);
static void freeconf_v1(struct __nsw_switchconfig_v1 *);
static void freeconf(struct __nsw_switchconfig *);
static int alldigits(char *);

static struct cons_cell_v1 *concell_list_v1; /* stays with add_concell() */
static struct cons_cell *concell_list; /* stays with add_concell() */

/*
 *
 * With the "lookup control" feature, the default criteria for NIS, NIS+,
 * and any new services (e.g. ldap) will be:
 *     [SUCCESS=return  NOTFOUND=continue UNAVAIL=continue TRYAGAIN=forever]
 *
 * For backward compat, NIS via NIS server in DNS forwarding mode will be:
 *     [SUCCESS=return  NOTFOUND=continue UNAVAIL=continue TRYAGAIN=continue]
 *
 * And also for backward compat, the default criteria for DNS will be:
 *     [SUCCESS=return  NOTFOUND=continue UNAVAIL=continue TRYAGAIN=continue]
 */



/*
 * The BIND resolver normally will retry several times on server non-response.
 * But now with the "lookup control" feature, we don't want the resolver doing
 * many retries, rather we want it to return control (reasonably) quickly back
 * to the switch engine.  However, when TRYAGAIN=N or TRYAGAIN=forever is
 * not explicitly set by the admin in the conf file, we want the old "resolver
 * retry a few times" rather than no retries at all.
 */
static int 	dns_tryagain_retry = 3;

/*
 * For backward compat (pre "lookup control"), the dns default behavior is
 * soft lookup.
 */
static void
set_dns_default_lkp(struct __nsw_lookup_v1 *lkp)
{
	if (strcasecmp(lkp->service_name, "dns") == 0) {
		lkp->actions[__NSW_TRYAGAIN] =
			__NSW_TRYAGAIN_NTIMES;
		lkp->max_retries = dns_tryagain_retry;
	}
}

/*
 * Private interface used by nss_common.c, hence this function is not static
 */
struct __nsw_switchconfig_v1 *
_nsw_getoneconfig_v1(const char *name, char *linep, enum __nsw_parse_err *errp)
	/* linep   Nota Bene: not const char *	*/
	/* errp  Meanings are abused a bit	*/
{
	struct __nsw_switchconfig_v1 *cfp;
	struct __nsw_lookup_v1 *lkp, **lkq;
	int end_crit;
	action_t act;
	char *p, *tokenp;

	*errp = __NSW_CONF_PARSE_SUCCESS;

	if ((cfp = (struct __nsw_switchconfig_v1 *)
	    calloc(1, sizeof (struct __nsw_switchconfig_v1))) == NULL) {
		*errp = __NSW_CONF_PARSE_SYSERR;
		return (NULL);
	}
	cfp->dbase = strdup(name);
	lkq = &cfp->lookups;

	/* linep points to a naming service name */
	while (1) {
		int i;

		/* white space following the last service */
		if (*linep == '\0' || *linep == '\n') {
			return (cfp);
		}
		if ((lkp = (struct __nsw_lookup_v1 *)
		    calloc(1, sizeof (struct __nsw_lookup_v1))) == NULL) {
			*errp = __NSW_CONF_PARSE_SYSERR;
			freeconf_v1(cfp);
			return (NULL);
		}

		*lkq = lkp;
		lkq = &lkp->next;

		for (i = 0; i < __NSW_STD_ERRS_V1; i++)
			if (i == __NSW_SUCCESS)
				lkp->actions[i] = __NSW_RETURN;
			else if (i == __NSW_TRYAGAIN)
				lkp->actions[i] = __NSW_TRYAGAIN_FOREVER;
			else
				lkp->actions[i] = __NSW_CONTINUE;

		/* get criteria for the naming service */
		if (tokenp = skip(&linep, '[')) { /* got criteria */

			/* premature end, illegal char following [ */
			if (!islabel(*linep))
				goto barf_line;
			lkp->service_name = strdup(tokenp);
			cfp->num_lookups++;

			set_dns_default_lkp(lkp);

			end_crit = 0;

			/* linep points to a switch_err */
			while (1) {
				int ntimes = 0; /* try again max N times */
				int dns_continue = 0;

				if ((tokenp = skip(&linep, '=')) == NULL) {
					goto barf_line;
				}

				/* premature end, ill char following = */
				if (!islabel(*linep))
					goto barf_line;

				/* linep points to the string following '=' */
				p = labelskip(linep);
				if (*p == ']')
					end_crit = 1;
				else if (*p != ' ' && *p != '\t')
					goto barf_line;
				*p++ = '\0'; /* null terminate linep */
				p = spaceskip(p);
				if (!end_crit) {
					if (*p == ']') {
					end_crit = 1;
					*p++ = '\0';
					} else if (*p == '\0' || *p == '\n') {
						return (cfp);
					} else if (!islabel(*p))
					/* p better be the next switch_err */
						goto barf_line;
				}
				if (strcasecmp(linep, __NSW_STR_RETURN) == 0)
					act = __NSW_RETURN;
				else if (strcasecmp(linep,
						    __NSW_STR_CONTINUE) == 0) {
					if (strcasecmp(lkp->service_name,
						    "dns") == 0) {
						dns_continue = 1;
						act = __NSW_TRYAGAIN_NTIMES;
					} else
						act = __NSW_CONTINUE;
				} else if (strcasecmp(linep,
					    __NSW_STR_FOREVER) == 0)
					act = __NSW_TRYAGAIN_FOREVER;
				else if (alldigits(linep)) {
					act = __NSW_TRYAGAIN_NTIMES;
					ntimes = atoi(linep);
					if (ntimes < 0 || ntimes > MAX_INT)
						ntimes = 0;
				}
				else
					goto barf_line;

				if (__NSW_SUCCESS_ACTION(act) &&
				    strcasecmp(tokenp,
					    __NSW_STR_SUCCESS) == 0) {
					lkp->actions[__NSW_SUCCESS] = act;
				} else if (__NSW_NOTFOUND_ACTION(act) &&
					strcasecmp(tokenp,
					    __NSW_STR_NOTFOUND) == 0) {
					lkp->actions[__NSW_NOTFOUND] = act;
				} else if (__NSW_UNAVAIL_ACTION(act) &&
					strcasecmp(tokenp,
					    __NSW_STR_UNAVAIL) == 0) {
					lkp->actions[__NSW_UNAVAIL] = act;
				} else if (__NSW_TRYAGAIN_ACTION(act) &&
					strcasecmp(tokenp,
					    __NSW_STR_TRYAGAIN) == 0) {
					lkp->actions[__NSW_TRYAGAIN] = act;
					if (strcasecmp(lkp->service_name,
						    "nis") == 0)
						lkp->actions[
						    __NSW_NISSERVDNS_TRYAGAIN]
						    = act;
					if (act == __NSW_TRYAGAIN_NTIMES)
						lkp->max_retries =
						dns_continue ?
						dns_tryagain_retry : ntimes;
				} else {
					/*EMPTY*/
					/*
					 * convert string tokenp to integer
					 * and put in long_errs
					 */
				}
				if (end_crit) {
					linep = spaceskip(p);
					if (*linep == '\0' || *linep == '\n')
						return (cfp);
					break; /* process next naming service */
				}
				linep = p;
			} /* end of while loop for a name service's criteria */
		} else {
			/*
			 * no criteria for this naming service.
			 * linep points to name service, but not null
			 * terminated.
			 */
			p = labelskip(linep);
			if (*p == '\0' || *p == '\n') {
				*p = '\0';
				lkp->service_name = strdup(linep);
				set_dns_default_lkp(lkp);
				cfp->num_lookups++;
				return (cfp);
			}
			if (*p != ' ' && *p != '\t')
				goto barf_line;
			*p++ = '\0';
			lkp->service_name = strdup(linep);
			set_dns_default_lkp(lkp);
			cfp->num_lookups++;
			linep = spaceskip(p);
		}
	} /* end of while(1) loop for a name service */

barf_line:
	freeconf_v1(cfp);
	*errp = __NSW_CONF_PARSE_NOPOLICY;
	return (NULL);
}

/*
 * Private interface used by nss_common.c, hence this function is not static
 */
struct __nsw_switchconfig *
_nsw_getoneconfig(const char *name, char *linep, enum __nsw_parse_err *errp)
	/* linep   Nota Bene: not const char *	*/
	/* errp  Meanings are abused a bit	*/
{
	struct __nsw_switchconfig *cfp;
	struct __nsw_lookup *lkp, **lkq;
	int end_crit;
	action_t act;
	char *p, *tokenp;

	*errp = __NSW_CONF_PARSE_SUCCESS;

	if ((cfp = (struct __nsw_switchconfig *)
	    calloc(1, sizeof (struct __nsw_switchconfig))) == NULL) {
		*errp = __NSW_CONF_PARSE_SYSERR;
		return (NULL);
	}
	cfp->dbase = strdup(name);
	lkq = &cfp->lookups;

	/* linep points to a naming service name */
	while (1) {
		int i;

		/* white space following the last service */
		if (*linep == '\0' || *linep == '\n') {
			return (cfp);
		}
		if ((lkp = (struct __nsw_lookup *)
		    calloc(1, sizeof (struct __nsw_lookup))) == NULL) {
			*errp = __NSW_CONF_PARSE_SYSERR;
			freeconf(cfp);
			return (NULL);
		}

		*lkq = lkp;
		lkq = &lkp->next;

		for (i = 0; i < __NSW_STD_ERRS; i++)
			if (i == __NSW_SUCCESS)
				lkp->actions[i] = 1;
			else
				lkp->actions[i] = 0;

		/* get criteria for the naming service */
		if (tokenp = skip(&linep, '[')) { /* got criteria */

			/* premature end, illegal char following [ */
			if (!islabel(*linep))
				goto barf_line;
			lkp->service_name = strdup(tokenp);
			cfp->num_lookups++;
			end_crit = 0;

			/* linep points to a switch_err */
			while (1) {
				if ((tokenp = skip(&linep, '=')) == NULL) {
					goto barf_line;
				}

				/* premature end, ill char following = */
				if (!islabel(*linep))
					goto barf_line;

				/* linep points to the string following '=' */
				p = labelskip(linep);
				if (*p == ']')
					end_crit = 1;
				else if (*p != ' ' && *p != '\t')
					goto barf_line;
				*p++ = '\0'; /* null terminate linep */
				p = spaceskip(p);
				if (!end_crit) {
					if (*p == ']') {
					end_crit = 1;
					*p++ = '\0';
					} else if (*p == '\0' || *p == '\n')
						return (cfp);
					else if (!islabel(*p))
					/* p better be the next switch_err */
						goto barf_line;
				}
				if (strcasecmp(linep, __NSW_STR_RETURN) == 0)
					act = __NSW_RETURN;
				else if (strcasecmp(linep,
					    __NSW_STR_CONTINUE) == 0)
					act = __NSW_CONTINUE;
				else if (strcasecmp(linep,
					    __NSW_STR_FOREVER) == 0)
					/*
					 * =forever or =N might be in conf file
					 * but old progs won't expect it.
					 */
					act = __NSW_RETURN;
				else if (alldigits(linep))
					act = __NSW_CONTINUE;
				else
					goto barf_line;
				if (strcasecmp(tokenp,
					    __NSW_STR_SUCCESS) == 0) {
					lkp->actions[__NSW_SUCCESS] = act;
				} else if (strcasecmp(tokenp,
					    __NSW_STR_NOTFOUND) == 0) {
					lkp->actions[__NSW_NOTFOUND] = act;
				} else if (strcasecmp(tokenp,
					    __NSW_STR_UNAVAIL) == 0) {
					lkp->actions[__NSW_UNAVAIL] = act;
				} else if (strcasecmp(tokenp,
					    __NSW_STR_TRYAGAIN) == 0) {
					lkp->actions[__NSW_TRYAGAIN] = act;
				} else {
					/*EMPTY*/
					/*
					 * convert string tokenp to integer
					 * and put in long_errs
					 */
				}
				if (end_crit) {
					linep = spaceskip(p);
					if (*linep == '\0' || *linep == '\n')
						return (cfp);
					break; /* process next naming service */
				}
				linep = p;
			} /* end of while loop for a name service's criteria */
		} else {
			/*
			 * no criteria for this naming service.
			 * linep points to name service, but not null
			 * terminated.
			 */
			p = labelskip(linep);
			if (*p == '\0' || *p == '\n') {
				*p = '\0';
				lkp->service_name = strdup(linep);
				cfp->num_lookups++;
				return (cfp);
			}
			if (*p != ' ' && *p != '\t')
				goto barf_line;
			*p++ = '\0';
			lkp->service_name = strdup(linep);
			cfp->num_lookups++;
			linep = spaceskip(p);
		}
	} /* end of while(1) loop for a name service */

barf_line:
	freeconf(cfp);
	*errp = __NSW_CONF_PARSE_NOPOLICY;
	return (NULL);
}

static struct __nsw_switchconfig_v1 *
do_getconfig_v1(const char *dbase, enum __nsw_parse_err *errp)
{
	struct __nsw_switchconfig_v1 *cfp, *retp = NULL;
	FILE *fp;
	char *linep, *lineq;

	if (cfp = scrounge_cache_v1(dbase)) {
		*errp = __NSW_CONF_PARSE_SUCCESS;
		return (cfp);
	}

	if ((fp = fopen(__NSW_CONFIG_FILE, "r")) == NULL) {
		*errp = __NSW_CONF_PARSE_NOFILE;
		return (NULL);
	}

	if ((lineq = (char *)malloc(BUFSIZ)) == NULL) {
		(void) fclose(fp);
		*errp = __NSW_CONF_PARSE_SYSERR;
		return (NULL);
	}

	*errp = __NSW_CONF_PARSE_NOPOLICY;
	while (linep = fgets(lineq, BUFSIZ, fp)) {
		enum __nsw_parse_err	line_err;
		char			*tokenp, *comment;

		/*
		 * Ignore portion of line following the comment character '#'.
		 */
		if ((comment = strchr(linep, '#')) != NULL) {
			*comment = '\0';
		}
		/*
		 * skip past blank lines.
		 * otherwise, cache as a struct switchconfig.
		 */
		if ((*linep == '\0') || isspace(*linep)) {
			continue;
		}
		if ((tokenp = skip(&linep, ':')) == NULL) {
			continue; /* ignore this line */
		}
		if (cfp = scrounge_cache_v1(tokenp)) {
			continue; /* ? somehow this database is in the cache */
		}
		if (cfp = _nsw_getoneconfig_v1(tokenp, linep, &line_err)) {
			(void) add_concell_v1(cfp);
			if (strcmp(cfp->dbase, dbase) == 0) {
				*errp = __NSW_CONF_PARSE_SUCCESS;
				retp = cfp;
			}
		} else {
			/*
			 * Got an error on this line, if it is a system
			 * error we might as well give right now. If it
			 * is a parse error on the second entry of the
			 * database we are looking for and the first one
			 * was a good entry we end up logging the following
			 * syslog message and using a default policy instead.
			 */
			if (line_err == __NSW_CONF_PARSE_SYSERR) {
				*errp = __NSW_CONF_PARSE_SYSERR;
				break;
			} else if (line_err == __NSW_CONF_PARSE_NOPOLICY &&
				    strcmp(tokenp, dbase) == 0) {
				syslog(LOG_WARNING,
		"libc: bad lookup policy for %s in %s, using defaults..\n",
					dbase, __NSW_CONFIG_FILE);
				*errp = __NSW_CONF_PARSE_NOPOLICY;
				break;
			}
			/*
			 * Else blithely ignore problems on this line and
			 *   go ahead with the next line.
			 */
		}
	}
	free(lineq);
	(void) fclose(fp);
	return (retp);
}

static struct __nsw_switchconfig *
do_getconfig(const char *dbase, enum __nsw_parse_err *errp)
{
	struct __nsw_switchconfig *cfp, *retp = NULL;
	FILE *fp;
	char *linep, *lineq;

	if (cfp = scrounge_cache(dbase)) {
		*errp = __NSW_CONF_PARSE_SUCCESS;
		return (cfp);
	}

	if ((fp = fopen(__NSW_CONFIG_FILE, "r")) == NULL) {
		*errp = __NSW_CONF_PARSE_NOFILE;
		return (NULL);
	}

	if ((lineq = (char *)malloc(BUFSIZ)) == NULL) {
		(void) fclose(fp);
		*errp = __NSW_CONF_PARSE_SYSERR;
		return (NULL);
	}

	*errp = __NSW_CONF_PARSE_NOPOLICY;
	while (linep = fgets(lineq, BUFSIZ, fp)) {
		enum __nsw_parse_err	line_err;
		char			*tokenp, *comment;

		/*
		 * Ignore portion of line following the comment character '#'.
		 */
		if ((comment = strchr(linep, '#')) != NULL) {
			*comment = '\0';
		}
		/*
		 * skip past blank lines.
		 * otherwise, cache as a struct switchconfig.
		 */
		if ((*linep == '\0') || isspace(*linep)) {
			continue;
		}
		if ((tokenp = skip(&linep, ':')) == NULL) {
			continue; /* ignore this line */
		}
		if (cfp = scrounge_cache(tokenp)) {
			continue; /* ? somehow this database is in the cache */
		}
		if (cfp = _nsw_getoneconfig(tokenp, linep, &line_err)) {
			(void) add_concell(cfp);
			if (strcmp(cfp->dbase, dbase) == 0) {
				*errp = __NSW_CONF_PARSE_SUCCESS;
				retp = cfp;
			}
		} else {
			/*
			 * Got an error on this line, if it is a system
			 * error we might as well give right now. If it
			 * is a parse error on the second entry of the
			 * database we are looking for and the first one
			 * was a good entry we end up logging the following
			 * syslog message and using a default policy instead.
			 */
			if (line_err == __NSW_CONF_PARSE_SYSERR) {
				*errp = __NSW_CONF_PARSE_SYSERR;
				break;
			} else if (line_err == __NSW_CONF_PARSE_NOPOLICY &&
				    strcmp(tokenp, dbase) == 0) {
				syslog(LOG_WARNING,
		"libc: bad lookup policy for %s in %s, using defaults..\n",
					dbase, __NSW_CONFIG_FILE);
				*errp = __NSW_CONF_PARSE_NOPOLICY;
				break;
			}
			/*
			 * Else blithely ignore problems on this line and
			 *   go ahead with the next line.
			 */
		}
	}
	free(lineq);
	(void) fclose(fp);
	return (retp);
}


static struct __nsw_switchconfig_v1 *
scrounge_cache_v1(const char *dbase)
{
	struct cons_cell_v1 *cellp = concell_list_v1;

	for (; cellp; cellp = cellp->next)
		if (strcmp(dbase, cellp->sw->dbase) == 0)
			return (cellp->sw);
	return (NULL);
}

static struct __nsw_switchconfig *
scrounge_cache(const char *dbase)
{
	struct cons_cell *cellp = concell_list;

	for (; cellp; cellp = cellp->next)
		if (strcmp(dbase, cellp->sw->dbase) == 0)
			return (cellp->sw);
	return (NULL);
}

static void
freeconf_v1(struct __nsw_switchconfig_v1 *cfp)
{
	if (cfp) {
		if (cfp->dbase)
			free(cfp->dbase);
		if (cfp->lookups) {
			struct __nsw_lookup_v1 *nex, *cur;
			for (cur = cfp->lookups; cur; cur = nex) {
				free(cur->service_name);
				nex = cur->next;
				free(cur);
			}
		}
		free(cfp);
	}
}

static void
freeconf(struct __nsw_switchconfig *cfp)
{
	if (cfp) {
		if (cfp->dbase)
			free(cfp->dbase);
		if (cfp->lookups) {
			struct __nsw_lookup *nex, *cur;
			for (cur = cfp->lookups; cur; cur = nex) {
				free(cur->service_name);
				nex = cur->next;
				free(cur);
			}
		}
		free(cfp);
	}
}

action_t
__nsw_extended_action_v1(struct __nsw_lookup_v1 *lkp, int err)
{
	struct __nsw_long_err *lerrp;

	for (lerrp = lkp->long_errs; lerrp; lerrp = lerrp->next) {
		if (lerrp->nsw_errno == err)
			return (lerrp->action);
	}
	return (__NSW_CONTINUE);
}

action_t
__nsw_extended_action(struct __nsw_lookup *lkp, int err)
{
	struct __nsw_long_err *lerrp;

	for (lerrp = lkp->long_errs; lerrp; lerrp = lerrp->next) {
		if (lerrp->nsw_errno == err)
			return (lerrp->action);
	}
	return (__NSW_CONTINUE);
}


/* give the next non-alpha character */
static char *
labelskip(char *cur)
{
	char *p = cur;
	while (islabel(*p))
		++p;
	return (p);
}

/* give the next non-space character */
static char *
spaceskip(char *cur)
{
	char *p = cur;
	while (*p == ' ' || *p == '\t')
		++p;
	return (p);
}

/*
 * terminate the *cur pointed string by null only if it is
 * followed by "key" surrounded by zero or more spaces and
 * return value is the same as the original *cur pointer and
 * *cur pointer is advanced to the first non {space, key} char
 * followed by the key. Otherwise, return NULL and keep
 * *cur unchanged.
 */
static char *
skip(char **cur, char key)
{
	char *p, *tmp;
	char *q = *cur;
	int found, tmpfound;

	tmp = labelskip(*cur);
	p = tmp;
	found = (*p == key);
	if (found) {
		*p++ = '\0'; /* overwrite the key */
		p = spaceskip(p);
	} else {
		while (*p == ' ' || *p == '\t') {
			tmpfound = (*++p == key);
			if (tmpfound) {
				found = tmpfound;
					/* null terminate the return token */
				*tmp = '\0';
				p++; /* skip the key */
			}
		}
	}
	if (!found)
		return (NULL); /* *cur unchanged */
	*cur = p;
	return (q);
}

/* add to the front: LRU */
static int
add_concell_v1(struct __nsw_switchconfig_v1 *cfp)
{
	struct cons_cell_v1 *cp;

	if (cfp == NULL)
		return (1);
	if ((cp = (struct cons_cell_v1 *)malloc(sizeof (struct cons_cell_v1)))
			== NULL)
		return (1);
	cp->sw = cfp;
	cp->next = concell_list_v1;
	concell_list_v1 = cp;
	return (0);
}

/* add to the front: LRU */
static int
add_concell(struct __nsw_switchconfig *cfp)
{
	struct cons_cell *cp;

	if (cfp == NULL)
		return (1);
	if ((cp = (struct cons_cell *)malloc(sizeof (struct cons_cell)))
			== NULL)
		return (1);
	cp->sw = cfp;
	cp->next = concell_list;
	concell_list = cp;
	return (0);
}

static mutex_t serialize_config_v1 = DEFAULTMUTEX;
static mutex_t serialize_config = DEFAULTMUTEX;

struct __nsw_switchconfig_v1 *
__nsw_getconfig_v1(const char *dbase, enum __nsw_parse_err	*errp)
{
	struct __nsw_switchconfig_v1	*res;

	/*
	 * ==== I don't feel entirely comfortable disabling signals for the
	 *	duration of this, but maybe we have to.  Or maybe we should
	 *	use mutex_trylock to detect recursion?  (Not clear what's
	 *	the right thing to do when it happens, though).
	 */
	(void) mutex_lock(&serialize_config_v1);
	res = do_getconfig_v1(dbase, errp);
	(void) mutex_unlock(&serialize_config_v1);
	return (res);
}

struct __nsw_switchconfig *
__nsw_getconfig(const char *dbase, enum __nsw_parse_err	*errp)
{
	struct __nsw_switchconfig	*res;

	/*
	 * ==== I don't feel entirely comfortable disabling signals for the
	 *	duration of this, but maybe we have to.  Or maybe we should
	 *	use mutex_trylock to detect recursion?  (Not clear what's
	 *	the right thing to do when it happens, though).
	 */
	(void) mutex_lock(&serialize_config);
	res = do_getconfig(dbase, errp);
	(void) mutex_unlock(&serialize_config);
	return (res);
}

int
__nsw_freeconfig_v1(struct __nsw_switchconfig_v1 *conf)
{
	struct cons_cell_v1 *cellp;

	if (conf == NULL) {
		return (-1);
	}
	/*
	 * Hacked to make life easy for the code in nss_common.c.  Free conf
	 *   iff it was created by calling _nsw_getoneconfig() directly
	 *   rather than by calling nsw_getconfig.
	 */
	(void) mutex_lock(&serialize_config_v1);
	for (cellp = concell_list_v1;  cellp;  cellp = cellp->next) {
		if (cellp->sw == conf) {
			break;
		}
	}
	(void) mutex_unlock(&serialize_config_v1);
	if (cellp == NULL) {
		/* Not in the cache;  free it */
		freeconf_v1(conf);
		return (1);
	} else {
		/* In the cache;  don't free it */
		return (0);
	}
}

int
__nsw_freeconfig(struct __nsw_switchconfig *conf)
{
	struct cons_cell *cellp;

	if (conf == NULL) {
		return (-1);
	}
	/*
	 * Hacked to make life easy for the code in nss_common.c.  Free conf
	 *   iff it was created by calling _nsw_getoneconfig() directly
	 *   rather than by calling nsw_getconfig.
	 */
	(void) mutex_lock(&serialize_config);
	for (cellp = concell_list;  cellp;  cellp = cellp->next) {
		if (cellp->sw == conf) {
			break;
		}
	}
	(void) mutex_unlock(&serialize_config);
	if (cellp == NULL) {
		/* Not in the cache;  free it */
		freeconf(conf);
		return (1);
	} else {
		/* In the cache;  don't free it */
		return (0);
	}
}

/* Return 1 if the string contains all digits, else return 0. */
static int
alldigits(char *s)
{
	for (; *s; s++)
		if (!isdigit(*s))
			return (0);
	return (1);
}
