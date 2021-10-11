/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cachemgr_getldap.c 1.2     99/11/11 SMI"

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/door.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <synch.h>
#include <pthread.h>
#include <unistd.h>
#include "cachemgr.h"

static rwlock_t	ldap_lock = DEFAULTRWLOCK;
static int	sighup_update = FALSE;
extern admin_t	current_admin;

/* variables used for SIGHUP wakeup on sleep */
static struct timespec		timeout;
static mutex_t			sighuplock;
static cond_t			cond;


#ifdef SLP
extern int	use_slp;
#endif SLP

/* nis domain information */
#define	_NIS_FILTER		"objectclass=nisDomainObject"
#define	_NIS_DOMAIN		"nisdomain"

static const char *nis_domain_attrs[] = {
	_NIS_DOMAIN,
	(char *)NULL
};

#define	WAKEUPAHEADTIME		120
#define	CACHESLEEPTIME		600

#define	NSLDAPSETPARAM(a, b, c, d, e, f)				\
	if (__ns_ldap_setParam((a), (b), (c), &(d))			\
		!= NS_LDAP_SUCCESS) {					\
		logit("Error: __ns_ldap_setParam failed, "		\
			"status: %d message: %s",			\
			(d)->status, (d)->message);			\
		(void) __ns_ldap_freeError(&(d));			\
		(void) __ns_ldap_freeResult(&(e));			\
		rw_unlock(&(f));					\
		return (-1);						\
	}								\
	continue;


static int
verify_nis_domain()
{
	ns_ldap_result_t *result = NULL;
	char		cfg_domain[BUFSIZ];
	void		**paramVal = NULL;
	ns_ldap_error_t	*error;
	int		rc = 0, found = 0;
	int		j;
	ns_ldap_attr_t	*attrptr;
	ns_ldap_entry_t	*entry;


	if (current_admin.debug_level >= DBG_ALL) {
		logit("verify_nis_domain....\n");
	}

	/* get currently configured domain name */
	if ((rc = __ns_ldap_getParam(NULL, NS_LDAP_DOMAIN_P,
		&paramVal, &error)) != NS_LDAP_SUCCESS) {
		if (error != NULL && error->message != NULL)
			logit("Error: Unable to get domain name: %s\n",
				error->message);
		else {
			char *tmp;

			__ns_ldap_err2str(rc, &tmp);
			logit("Error: Unable to get domain name: %s\n", tmp);
		}
		(void) __ns_ldap_freeParam(&paramVal);
		(void) __ns_ldap_freeError(&error);
		return (-1);
	}
	if (paramVal == NULL || (char *)*paramVal == NULL)
			return (0);
	strcpy(cfg_domain, (char *)*paramVal);
	(void) __ns_ldap_freeParam(&paramVal);

	if ((rc = __ns_ldap_list(NULL, (const char *) _NIS_FILTER,
		nis_domain_attrs, NULL, NULL, NS_LDAP_SCOPE_BASE,
		&result, &error, NULL, NULL)) != NS_LDAP_SUCCESS) {
		if (error != NULL && error->message != NULL)
			logit("Error: Unable to get nis_domain "
				"from server: %s\n", error->message);
		else
			logit("Error: Unable to get nis_domain\n");
		(void) __ns_ldap_freeError(&error);
		(void) __ns_ldap_freeResult(&result);
		return (-1);
	}

	found = 0;
	entry = result->entry;
	for (j = 0; j < entry->attr_count; j++) {
		attrptr = entry->attr_pair[j];
		if (attrptr == NULL) {
			continue;
		}

		if (attrptr != NULL && attrptr->attrname != NULL)
			if (current_admin.debug_level >= DBG_ALL) {
				logit("Found name:value pair [%s:%s]\n",
					attrptr->attrname,
					attrptr->attrvalue[0]);
			}
		else
			if (current_admin.debug_level >= DBG_ALL) {
				logit("Either name or value is NULL\n");
			}

		if (strcasecmp(attrptr->attrname,
			_NIS_DOMAIN) == 0) {
			if (current_admin.debug_level >= DBG_ALL) {
				logit("Getting %s\n", _NIS_DOMAIN);
			}
			if (strcasecmp(cfg_domain,
			    attrptr->attrvalue[0]) != 0) {
				logit("Error: detected serious "
					"misconfiguration\n");
				logit("Error: client/server domain "
					"name mismatch\n");
				logit("Error: client is member of "
					"the '%s' domain\n",
					cfg_domain);
				logit("Error: server profile indicates "
					"'%s' domain\n",
					attrptr->attrvalue[0]);
				logit("Error: this discrepancy must be "
					"resolved immediately\n");
				syslog(LOG_ERR, "Error: detected serious "
					"misconfiguration");
				syslog(LOG_ERR, "Error: client/server domain "
					"name mismatch\n");
				syslog(LOG_ERR, "Error: client is member of "
					"the '%s' domain\n",
					cfg_domain);
				syslog(LOG_ERR, "Error: server profile "
					"indicates '%s' domain\n",
					attrptr->attrvalue[0]);
				syslog(LOG_ERR, "Error: this discrepancy "
					"must be resolved immediately\n");
				rc = -1;
				break;
			}
			found = 1;
		}
	}

	if (found == 0 && rc != -1) {
		if (current_admin.debug_level >= DBG_ALL) {
			logit("Error: name/value not found for %s\n",
				_NIS_DOMAIN);
			logit("Error: in %s\n", _NIS_FILTER);
		}
		rc = -1;
	}

	(void) __ns_ldap_freeError(&error);
	(void) __ns_ldap_freeResult(&result);

	return (rc);
}


static int
checkupdate(int sighup)
{
	int	value;

	rw_wrlock(&ldap_lock);
	value = sighup;
	rw_unlock(&ldap_lock);

	return (value == TRUE);
}


static int
update_from_profile()
{
	ns_ldap_result_t *result = NULL;
	char		searchfilter[BUFSIZ];
	ns_ldap_error_t	*error;
	int		j, k, rc;
	ns_ldap_attr_t	*attrptr;
	ns_ldap_entry_t	*entry;
	void		**paramVal = NULL;

	if (current_admin.debug_level >= DBG_ALL) {
		logit("update_from_profile....\n");
	}
	do {
		rw_rdlock(&ldap_lock);
		if (verify_nis_domain() != 0) {
			rw_unlock(&ldap_lock);
			logit("Error: Unable to update from server "
				"profile as\n");
			logit("Error: client domain name could "
				"not be verified\n");
			logit("Error: against server domain name\n");
			return (-1);
		}
		rw_unlock(&ldap_lock);
		rw_wrlock(&ldap_lock);
		sighup_update = FALSE;
		rw_unlock(&ldap_lock);
		if ((rc = __ns_ldap_getParam(NULL, NS_LDAP_PROFILE_P,
			&paramVal, &error)) != NS_LDAP_SUCCESS) {
			if (error != NULL && error->message != NULL)
				logit("Error: Unable to get profile name: %s\n",
					error->message);
			else {
				char *tmp;

				__ns_ldap_err2str(rc, &tmp);
				logit("Error: Unable to get profile name: %s\n",
					tmp);
			}
			(void) __ns_ldap_freeParam(&paramVal);
			(void) __ns_ldap_freeError(&error);
			return (-1);
		}

		if (paramVal == NULL || (char *)*paramVal == NULL)
			return (-1);

		if (snprintf(searchfilter, BUFSIZ, _PROFILE_FILTER,
		    _PROFILE_OBJECTCLASS, (char *)*paramVal) < 0) {
			(void) __ns_ldap_freeParam(&paramVal);
			return (-1);
		}

		if ((rc = __ns_ldap_list(_PROFILE_CONTAINER,
		    (const char *)searchfilter,
		    profile_attrs, NULL, NULL, 0,
		    &result, &error, NULL, NULL)) != NS_LDAP_SUCCESS) {

			/* syslog the error */
			if ((error != NULL) && (error->message != NULL)) {
				syslog(LOG_ERR,
				    "Error: Unable to refresh profile:"
				    " %s\n", error->message);
				logit("Error: Unable to refresh profile: %s\n",
						error->message);
			} else {
				syslog(LOG_ERR, "Error: Unable to refresh "
					"from profile. (error=%d)\n", rc);
				logit("Error: Unable to refresh from profile "
					"(error=%d)\n", rc);
			}

			(void) __ns_ldap_freeParam(&paramVal);
			(void) __ns_ldap_freeError(&error);
			(void) __ns_ldap_freeResult(&result);
			return (-1);
		}

		(void) __ns_ldap_freeParam(&paramVal);

	} while (checkupdate(sighup_update) == TRUE);

	rw_wrlock(&ldap_lock);
	for (entry = result->entry; entry != NULL; entry = entry->next) {
		for (j = 0; j < entry->attr_count; j++) {
			attrptr = entry->attr_pair[j];
			if (attrptr == NULL) {
				continue;
			}

#ifndef SLP
			if (strcasecmp(attrptr->attrname, _P_SERVERS) == 0) {
#else
			if (strcasecmp(attrptr->attrname, _P_SERVERS) == 0 &&
				use_slp != 1) {
#endif SLP
				char	val[BUFSIZ];
				int	firsttime = 1;

				/* Multiple Value */
				val[0] = NULL;
				for (k = 0; k < attrptr->value_count; k++) {
					if (firsttime == 1) {
						firsttime = 0;
						strcpy(val,
							attrptr->attrvalue[k]);
					} else {
						strcat(val, ", ");
						strcat(val,
							attrptr->attrvalue[k]);
					}
				}
				if (current_admin.debug_level >= DBG_ALL) {

					logit("Setting %s: %s\n", _P_SERVERS,
						val);
				}
				NSLDAPSETPARAM(NULL, NS_LDAP_SERVERS_P,
					val, error, result, ldap_lock);
			}


			if (strcasecmp(attrptr->attrname,
				_P_SEARCHBASEDN) == 0) {
				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n", _P_SEARCHBASEDN);
				}
				NSLDAPSETPARAM(NULL, NS_LDAP_SEARCH_BASEDN_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_CACHETTL) == 0) {
				/* Override the default ttl value */
				long	oldvalue = 0;

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_CACHETTL);
				}
				errno = 0;
				oldvalue = current_admin.ldap_stat.ldap_ttl;
				current_admin.ldap_stat.ldap_ttl =
					atol(attrptr->attrvalue[0]);
				if (errno != 0) {
					current_admin.ldap_stat.ldap_ttl =
						oldvalue;
				}
				continue;
			}

			if (strcasecmp(attrptr->attrname, _P_BINDDN) == 0) {
				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n", _P_BINDDN);
				}
				NSLDAPSETPARAM(NULL, NS_LDAP_BINDDN_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_BINDPASSWORD) == 0) {
				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n", _P_BINDPASSWORD);
				}
				NSLDAPSETPARAM(NULL, NS_LDAP_BINDPASSWD_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_AUTHMETHOD) == 0) {
				/* Multiple Value */
				char	val[BUFSIZ];
				int	firsttime = 1;

				/* Multiple Value */
				val[0] = NULL;
				for (k = 0; k < attrptr->value_count; k++) {
					if (firsttime == 1) {
						firsttime = 0;
						strcpy(val,
							attrptr->attrvalue[k]);
					} else {
						strcat(val, ", ");
						strcat(val,
							attrptr->attrvalue[k]);
					}
				}

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s: %s\n",
						_P_AUTHMETHOD, val);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_AUTH_P,
					val, error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_TRANSPORTSECURITY) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_TRANSPORTSECURITY);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_TRANSPORT_SEC_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_CERTIFICATEPATH) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_CERTIFICATEPATH);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_CERT_PATH_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_CERTIFICATEPASSWORD) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_CERTIFICATEPASSWORD);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_CERT_PATH_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_DATASEARCHDN) == 0) {
				/* Multiple Value */

				for (k = 0; k < attrptr->value_count; k++) {
					if (__ns_ldap_setParam(
						NULL,
						NS_LDAP_SEARCH_DN_P,
						attrptr->attrvalue[k], &error)
						!= NS_LDAP_SUCCESS) {

						logit("Error: "
						    "__ns_ldap_setParam "
						    "failed, status: %d "
						    "message: %s",
						    error->status,
						    error->message);
						(void)
						__ns_ldap_freeError(&error);
						(void)
						__ns_ldap_freeResult(&result);
						rw_unlock(&ldap_lock);
						return (-1);
					}
				}
				continue;
			}

			if (strcasecmp(attrptr->attrname,
				_P_SEARCHSCOPE) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n", _P_SEARCHSCOPE);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_SEARCH_SCOPE_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_SEARCHTIMELIMIT) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_SEARCHTIMELIMIT);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_SEARCH_TIME_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_PREFERREDSERVER) == 0) {
				/* Multiple Value */
				char	val[BUFSIZ];
				int	firsttime = 1;

				/* Multiple Value */
				val[0] = NULL;
				for (k = 0; k < attrptr->value_count; k++) {
					if (firsttime == 1) {
						firsttime = 0;
						strcpy(val,
							attrptr->attrvalue[k]);
					} else {
						strcat(val, ", ");
						strcat(val,
							attrptr->attrvalue[k]);
					}
				}

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s: %s\n",
						_P_PREFERREDSERVER, val);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_SERVER_PREF_P,
					val, error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_PREFERREDSERVERONLY) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_PREFERREDSERVERONLY);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_PREF_ONLY_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}

			if (strcasecmp(attrptr->attrname,
				_P_SEARCHREFERRAL) == 0) {

				if (current_admin.debug_level >= DBG_ALL) {
					logit("Setting %s\n",
						_P_SEARCHREFERRAL);
				}

				NSLDAPSETPARAM(NULL, NS_LDAP_SEARCH_REF_P,
					attrptr->attrvalue[0], error, result,
					ldap_lock);
			}
		}
	}

	rw_unlock(&ldap_lock);
	(void) __ns_ldap_freeResult(&result);
	return (0);
}


int
getldap_init()
{
	ns_ldap_error_t	*error;


	if (current_admin.debug_level >= DBG_ALL) {
		logit("getldap_init()...\n");
	}

	(void) __ns_ldap_setServer(TRUE);

	rw_wrlock(&ldap_lock);
	if ((error = __ns_ldap_LoadConfiguration(NULL)) != NULL) {
		logit("Error: Unable to read '%s': %s\n",
			NSCONFIGFILE, error->message);
		fprintf(stderr, gettext("\nError: Unable to read '%s': %s\n"),
			NSCONFIGFILE, error->message);
		__ns_ldap_freeError(&error);
		rw_unlock(&ldap_lock);
		return (-1);
	}
	rw_unlock(&ldap_lock);

	return (0);
}

static void
perform_update(void)
{
	ns_ldap_error_t	*error;
	struct timeval	tp;
	long long	newexp = 0;
	char		buf[20];

	if (current_admin.debug_level >= DBG_ALL) {
		logit("perform_update()...\n");
	}

	(void) __ns_ldap_setServer(TRUE);

	if (gettimeofday(&tp, NULL) != 0)
		return;

	newexp = (long long) tp.tv_sec + current_admin.ldap_stat.ldap_ttl;
	if (current_admin.ldap_stat.ldap_ttl < WAKEUPAHEADTIME)
		newexp += WAKEUPAHEADTIME;
	memset(buf, 0, 20);
	if (__ns_ldap_setParam(NULL, NS_LDAP_EXP_P,
		lltostr(newexp, &buf[19]), &error) != NS_LDAP_SUCCESS) {
		logit("Error: __ns_ldap_setParam failed, status: %d "
			"message: %s", error->status, error->message);
		__ns_ldap_freeError(&error);
		return;
	}

	rw_wrlock(&ldap_lock);
	sighup_update = FALSE;
	rw_unlock(&ldap_lock);

	do {
		if (update_from_profile() != 0) {
			logit("Error: Unable to update from profile\n");
		}
	} while (checkupdate(sighup_update) == TRUE);

	rw_rdlock(&ldap_lock);
	if ((error = __ns_ldap_DumpConfiguration(NSCONFIGREFRESH)) != NULL) {
		logit("Error: __ns_ldap_DumpConfiguration(\"%s\") failed, "
		    "status: %d "
		    "message: %s\n", NSCONFIGREFRESH,
		    error->status, error->message);
		__ns_ldap_freeError(&error);
	}
	if ((error = __ns_ldap_DumpConfiguration(NSCREDREFRESH)) != NULL) {
		logit("Error: __ns_ldap_DumpConfiguration(\"%s\") failed, "
		    "status: %d "
		    "message: %s\n", NSCREDREFRESH,
		    error->status, error->message);
		__ns_ldap_freeError(&error);
	}
	if (rename(NSCONFIGREFRESH, NSCONFIGFILE) != 0)
		logit("Error: unlink failed - errno: %d\n", errno);
	if (rename(NSCREDREFRESH, NSCREDFILE) != 0)
		logit("Error: unlink failed - errno: %d\n", errno);
	rw_unlock(&ldap_lock);
}

void
getldap_refresh()
{
	int		sleeptime;
	struct timeval	tp;
	long		expire = 0;
	void		**paramVal = NULL;
	ns_ldap_error_t	*errorp;
	int		always = 1;

	if (current_admin.debug_level >= DBG_ALL) {
		logit("getldap_refresh()...\n");
	}

	(void) __ns_ldap_setServer(TRUE);
	while (always) {
		rw_rdlock(&ldap_lock);
		if (current_admin.ldap_stat.ldap_ttl < WAKEUPAHEADTIME)
			current_admin.ldap_stat.ldap_ttl = 2 * WAKEUPAHEADTIME;
		sleeptime = current_admin.ldap_stat.ldap_ttl;
		if (gettimeofday(&tp, NULL) == 0) {
			if ((__ns_ldap_getParam(NULL, NS_LDAP_EXP_P,
			    &paramVal, &errorp) == NS_LDAP_SUCCESS) &&
			    paramVal != NULL &&
			    (char *)*paramVal != NULL) {
				errno = 0;
				expire = atol((char *)*paramVal);
				(void) __ns_ldap_freeParam(&paramVal);
				if (errno == 0) {
					if (expire == 0) {
						rw_unlock(&ldap_lock);
						cond_init(&cond, NULL, NULL);
						mutex_lock(&sighuplock);
						timeout.tv_sec = time(NULL) +
							CACHESLEEPTIME;
						timeout.tv_nsec = 0;
						cond_timedwait(&cond,
							&sighuplock, &timeout);
						cond_destroy(&cond);
						mutex_unlock(
							&sighuplock);
						continue;
					}
					sleeptime = expire - tp.tv_sec;
				}
			}
		}
		rw_unlock(&ldap_lock);
		/* sleep for at least 1 minute */
		if (sleeptime > WAKEUPAHEADTIME) {

			if (current_admin.debug_level >= DBG_ALL) {
				logit("about to sleep for %d seconds...\n",
					sleeptime);
			}
			cond_init(&cond, NULL, NULL);
			mutex_lock(&sighuplock);
			timeout.tv_sec = time(NULL) + sleeptime;
			timeout.tv_nsec = 0;
			cond_timedwait(&cond,
				&sighuplock, &timeout);
			cond_destroy(&cond);
			mutex_unlock(&sighuplock);
			continue;
		}
		perform_update();
	}
}

void
getldap_revalidate()
{
	ns_ldap_error_t	*error;

	if (current_admin.debug_level >= DBG_ALL) {
		logit("getldap_revalidate()...\n");
	}

	(void) __ns_ldap_setServer(TRUE);
	rw_wrlock(&ldap_lock);
	sighup_update = TRUE;
	if ((error = __ns_ldap_LoadConfiguration(NULL)) != NULL) {
		logit("Error: Unable to read '%s': %s\n",
			NSCONFIGFILE, error->message);
		__ns_ldap_freeError(&error);
	}
	rw_unlock(&ldap_lock);

	/* now awake the sleeping refresh thread */
	(void) cond_signal(&cond);
}

void
getldap_lookup(ldap_return_t *out, ldap_call_t * in)
{
	LineBuf		configinfo;
	ns_ldap_error_t	*error;

	if (current_admin.debug_level >= DBG_ALL) {
		logit("getldap_lookup()...\n");
	}

	rw_rdlock(&ldap_lock);
	if ((error = __ns_ldap_LoadDoorInfo(&configinfo, in->ldap_u.domainname))
		!= NULL) {
		if (error != NULL && error->message != NULL)
			logit("Error: getldap_lookup: %s\n", error->message);
		(void) __ns_ldap_freeError(&error);
		out->ldap_errno = -1;
		out->ldap_return_code = NOTFOUND;
		out->ldap_bufferbytesused = sizeof (*out);

	} else {
		out->ldap_bufferbytesused = sizeof (ldap_return_t);
		strncpy(out->ldap_u.config, configinfo.str, configinfo.len);
		out->ldap_return_code = SUCCESS;
		out->ldap_errno = 0;
	}

	if (configinfo.str != NULL) {
		free(configinfo.str);
		configinfo.str = NULL;
		configinfo.alloc = 0;
		configinfo.len = 0;
	}

	rw_unlock(&ldap_lock);
}
