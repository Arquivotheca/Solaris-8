/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)ns_config.c 1.4     99/11/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libintl.h>
#include <locale.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <crypt.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/systeminfo.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <limits.h>
#include "ns_sldap.h"
#include "ns_internal.h"
#include "ns_cache_door.h"

static int		config_loaded = 0;
static DomainParamType	*config = NULL;
static mutex_t		ns_parse_lock = DEFAULTMUTEX;
static	int		cache_server = FALSE;

#define	ALWAYS		1

/*
 * Get the fd out of the stdio 0.255 space (if 32 bits)
 * NOTE: 64 bits SPARCv9 does not have this stdio limit
 */
extern int _fcntl(int, int, ...);

static ns_ldap_error_t * __s_api_SetDoorInfo(char *domainname,
				char *configptr);
static ns_ldap_error_t * __s_api_LoadConfig(char * domainname);

static const int min_fd = 256;

time_t conv_time(char *s);

int
__ns_ldap_raise_fd(int fd)
{
	int nfd;
	char errstr[BUFSIZ];

	if (fd >= min_fd)
		return (fd);

	if ((nfd = _fcntl(fd, F_DUPFD, min_fd)) == -1)
		return (fd);

	if (-1 == close(fd)) {
		snprintf(errstr, BUFSIZ,
		"__ns_ldap_raise_fd(): could not close() fd %d; (errno = %d)",
								fd, errno);
		(void) syslog(LOG_ERR, errstr);
	}

	return (nfd);
}

static int
__getdomainname(char *name, int namelen)
{
	int sysinfostatus;

	sysinfostatus = sysinfo(SI_SRPC_DOMAIN, name, namelen);
	return ((sysinfostatus < 0) ? -1 : 0);
}

void
__ns_ldap_setServer(int set)
{
	cache_server = set;
}

static int
_print2buf(LineBuf *line, char *toprint)
{
	int	toprintlen = 0;

	if (line == NULL)
		return (-1);

	/* has print buffer line been exhausted */
	if ((toprintlen = strlen(toprint)) + line->len > (line->alloc - 1)) {
		do {
			if (line->alloc == 0) {
				line->alloc = BUFSIZ;
				line->str = (char *)malloc(line->alloc);
				if (line->str == NULL) {
					line->alloc = 0;
					return (-1);
				}
				line->str[0] = '\0';
			} else {
				line->alloc += BUFSIZ;
				line->str = (char *)realloc(line->str,
					line->alloc);
			}
			if (line->str == 0) {
				return (-1);
			}
		} while (toprintlen > line->alloc);
	}
	/* now add new 'toprint' data to buffer */
	strcat(line->str, toprint);
	line->len += toprintlen;
	return (0);
}

void
_print2buf_destroy(LineBuf *line)
{
	free(line->str);
	line->str = NULL;
	line->alloc = 0;
	line->len = 0;
}


static int
timetorefresh(void)
{
	struct timeval	tp;
	static time_t	expire = 0;

	if (config_loaded != 1 || gettimeofday(&tp, NULL) == -1)
		return (-1);

	mutex_lock(&ns_parse_lock);
	if (config != NULL &&
	    config->configParamList[NS_LDAP_EXP_P] != NULL) {
		errno = 0;
		expire =
		    atol((char *)config->configParamList[NS_LDAP_EXP_P]);
		if (errno != 0 || expire == 0) {
			mutex_unlock(&ns_parse_lock);
			return (-1);
		}
	} else {
		mutex_unlock(&ns_parse_lock);
		return (-1);
	}
	mutex_unlock(&ns_parse_lock);

	if (tp.tv_sec > expire)
		return (1);

	return (-1);
}


static int
get_auth_type(char *value, AuthType_t *at)
{

	if (strcasecmp(value, "NS_LDAP_AUTH_NONE") == 0) {
		*at = NS_LDAP_AUTH_NONE;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_AUTH_SIMPLE") == 0) {
		*at = NS_LDAP_AUTH_SIMPLE;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_AUTH_SASL_CRAM_MD5") == 0) {
		*at = NS_LDAP_AUTH_SASL_CRAM_MD5;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_AUTH_SASL_GSSAPI") == 0) {
		*at = NS_LDAP_AUTH_SASL_GSSAPI;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_AUTH_SASL_SPNEGO") == 0) {
		*at = NS_LDAP_AUTH_SASL_SPNEGO;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_AUTH_TLS") == 0) {
		*at = NS_LDAP_AUTH_TLS;
		return (0);
	}
	return (-1);
}

static int
get_security_type(char *value, SecType_t *st)
{
	if (strcasecmp(value, "NS_LDAP_SEC_NONE") == 0) {
		*st = NS_LDAP_SEC_NONE;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_SEC_SASL_INTEGRITY") == 0) {
		*st = NS_LDAP_SEC_SASL_INTEGRITY;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_SEC_SASL_PRIVACY") == 0) {
		*st = NS_LDAP_SEC_SASL_PRIVACY;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_SEC_TLS") == 0) {
		*st = NS_LDAP_SEC_TLS;
		return (0);
	}
	return (-1);
}


static int
get_scope_type(char *value, ScopeType_t *st)
{
	if (strcasecmp(value, "NS_LDAP_SCOPE_BASE") == 0) {
		*st = NS_LDAP_SCOPE_BASE;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_SCOPE_ONELEVEL") == 0) {
		*st = NS_LDAP_SCOPE_ONELEVEL;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_SCOPE_SUBTREE") == 0) {
		*st = NS_LDAP_SCOPE_SUBTREE;
		return (0);
	}
	return (-1);
}


static int
get_pref_type(char *value, PrefOnly_t *pt)
{
	if (strcasecmp(value, "NS_LDAP_TRUE") == 0) {
		*pt = NS_LDAP_PREF_TRUE;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_FALSE") == 0) {
		*pt = NS_LDAP_PREF_FALSE;
		return (0);
	}
	return (-1);
}


static int
get_searchref_type(char *value, SearchRef_t *sf)
{
	if (strcasecmp(value, "NS_LDAP_FOLLOWREF") == 0) {
		*sf = NS_LDAP_FOLLOWREF;
		return (0);
	} else if (strcasecmp(value, "NS_LDAP_NOREF") == 0) {
		*sf = NS_LDAP_NOREF;
		return (0);
	}
	return (-1);
}


static char *
get_auth_name(AuthType_t type)
{
	switch (type) {
		case	NS_LDAP_AUTH_NONE:
			return ("NS_LDAP_AUTH_NONE");
		case	NS_LDAP_AUTH_SIMPLE:
			return ("NS_LDAP_AUTH_SIMPLE");
		case	NS_LDAP_AUTH_SASL_CRAM_MD5:
			return ("NS_LDAP_AUTH_SASL_CRAM_MD5");
		case	NS_LDAP_AUTH_SASL_GSSAPI:
			return ("NS_LDAP_AUTH_SASL_GSSAPI");
		case	NS_LDAP_AUTH_SASL_SPNEGO:
			return ("NS_LDAP_AUTH_SASL_SPNEGO");
		case	NS_LDAP_AUTH_TLS:
			return ("NS_LDAP_AUTH_TLS");
		default:
			return ("Unknown AuthType_t type specified");
	}
}


static char *
get_security_name(SecType_t type)
{
	switch (type) {
		case	NS_LDAP_SEC_NONE:
			return ("NS_LDAP_SEC_NONE");
		case	NS_LDAP_SEC_SASL_INTEGRITY:
			return ("NS_LDAP_SEC_SASL_INTEGRITY");
		case	NS_LDAP_SEC_SASL_PRIVACY:
			return ("NS_LDAP_SEC_SASL_PRIVACY");
		case	NS_LDAP_SEC_TLS:
			return ("NS_LDAP_SEC_TLS");
		default:
			return ("Unknown SecType_t type specified");
	}
}


static char *
get_scope_name(ScopeType_t type)
{
	switch (type) {
		case	NS_LDAP_SCOPE_BASE:
			return ("NS_LDAP_SCOPE_BASE");
		case	NS_LDAP_SCOPE_ONELEVEL:
			return ("NS_LDAP_SCOPE_ONELEVEL");
		case	NS_LDAP_SCOPE_SUBTREE:
			return ("NS_LDAP_SCOPE_SUBTREE");
		default:
			return ("Unknown ScopeType_t type specified");
	}
}


static char *
get_pref_name(PrefOnly_t type)
{
	switch (type) {
		case	NS_LDAP_PREF_FALSE:
			return ("NS_LDAP_FALSE");
		case	NS_LDAP_PREF_TRUE:
			return ("NS_LDAP_TRUE");
		default:
			return ("Unknown PrefOnly_t type specified");
	}
}

static char *
get_searchref_name(SearchRef_t type)
{
	switch (type) {
		case	NS_LDAP_FOLLOWREF:
			return ("NS_LDAP_FOLLOWREF");
		case	NS_LDAP_NOREF:
			return ("NS_LDAP_NOREF");
		default:
			return ("Unknown SearchRef_t type specified");
	}
}

static void
destroy_param(void **data)
{
	char	*ptr;
	int	i;

	if ((char **)data == NULL)
		return;
	for (i = 0, ptr = (char *) data[i];
		ptr != NULL;
		i++, ptr = (char *) data[i]) {
		if (ptr != NULL)
			free((void *)ptr);
	}
	free(data);
}

static void
destroy_config(DomainParamType *ptr)
{
	ParamIndexType	i;
	DomainParamType	*tofree;

	if (ptr != NULL) {
		do {
			if (ptr->domainName != NULL)
				free(ptr->domainName);
			for (i = 0; i <= LAST_VALUE; i++) {
				if (ptr->configParamList[i] != NULL) {
					destroy_param(
						ptr->configParamList[i]);
					ptr->configParamList[i] = NULL;
				}
			}
			tofree = ptr;
			ptr = ptr->next;
			free(tofree);
		} while (ptr != NULL);
	}
}


void
__ns_ldap_cache_destroy()
{
	DomainParamType	*ptr = config;

	mutex_lock(&ns_parse_lock);
	destroy_config(ptr);
	config = NULL;
	mutex_unlock(&ns_parse_lock);
}

static DomainParamType *load_def_cfg(int);

void
__ns_ldap_default_config()
{
	DomainParamType	*ptr = config;
	char		domainname[MAXHOSTNAMELEN];

	if (__getdomainname(domainname, sizeof (domainname)) < 0) {
		return;
	}

	mutex_lock(&ns_parse_lock);
	destroy_config(ptr);
	config = load_def_cfg(CLIENTCONFIG);
	if (NULL == config) {
		mutex_unlock(&ns_parse_lock);
		return;
	}
	if (NULL != domainname)
		config->domainName = strdup(domainname);
	config_loaded = 1;
	mutex_unlock(&ns_parse_lock);
}


static DomainParamType *
get_domainptr(char *domainname)
{
	DomainParamType	*ptr;

	if (NULL == config)
		return (NULL);

	for (ptr = config; ptr != NULL; ptr = ptr->next)
		if (ptr->domainName != NULL &&
			strcasecmp(ptr->domainName, domainname) == 0)
			return (ptr);
	return (NULL);
}


static void
insert_domain(DomainParamType *configptr)
{
	DomainParamType	*prev, *curr;
	int		found = 0;

	if (config != NULL) {
		curr = config;
		prev = curr;
		while (curr != NULL) {
			if (curr->domainName &&
			    configptr->domainName &&
				strcasecmp(curr->domainName,
					configptr->domainName) == 0) {
				found = 1;
				break;
			}
			prev = curr;
			curr = curr->next;
		}
		if (found == 0) {
			/* no duplicate found */
			if (config == NULL)
				config = configptr;
			else {
				configptr->next = config;
				config = configptr;
			}
		} else {
			if (prev == curr) {
				config = configptr;
				configptr->next = curr->next;
			} else {
				/* duplicate found */
				prev->next = curr->next;
			}
			curr->next = NULL;
			destroy_config(curr);
		}
	} else
		config = configptr;
}


static DomainParamType *
create_config(void)
{
	ParamIndexType	i;
	DomainParamType	*ptr;

	if ((ptr = (DomainParamType *)
		    malloc(sizeof (DomainParamType))) == NULL) {
		return (NULL);
	} else {
		ptr->domainName = NULL;
		ptr->next = NULL;
		for (i = 0; i <= LAST_VALUE; i++)
			ptr->configParamList[i] = NULL;
	}

	return (ptr);
}

static int
allocate_count(void **valueptr)
{
	int	i;
	char	*ptr;

	if (valueptr == NULL)
		return (0);
	for (i = 0, ptr = (char *)valueptr[i];
	    ptr != NULL;
	    i++, ptr = (char *)valueptr[i]);

	return (i);
}

static char *
stripdup(const char *instr)
{
	char	*pstart = (char *)instr;
	char	*pend, *ret;
	int	len;

	if (pstart == NULL)
		return (NULL);
	/* remove leading spaces */
	while (pstart != NULL && *pstart == SPACETOK)
		pstart++;
	/* remove trailing spaces */
	pend = pstart + strlen(pstart) - 1;
	for (; pend >= pstart && isspace(*pend); pend--);
	len = pend - pstart + 1;
	if ((ret = calloc(sizeof (char), len + 1)) == NULL)
		return (NULL);
	else if (len != 0)
		strncpy(ret, pstart, len);
	return (ret);
}


static void **
allocate_value(ParamIndexType i, char *value, DomainParamType	*configptr)
{
	char		*ptr, *strptr;
	void		**valueptr;
	int		j, *intptr, count = 0;
	AuthType_t	*authptr, auth;
	SearchRef_t	*searchptr, search;
	PrefOnly_t	*prefptr, pref;
	char		*rest;

	/* eat up beginning quote, if any */
	while (value != NULL && (*value == QUOTETOK || *value == SPACETOK))
		value++;

	/* eat up space/quote at end of value */
	if (strlen(value) > 0)
		ptr = value + strlen(value) - 1;
	else
		ptr = value;
	for (; ptr != value && (*ptr == SPACETOK || *ptr == QUOTETOK); ptr--) {
		*ptr = '\0';
	}

	switch (i) {
		case	NS_LDAP_SERVERS_P:
		case	NS_LDAP_SERVER_PREF_P:
			valueptr = configptr->configParamList[i];
			for (j = 0, ptr = value; *ptr != '\0'; ptr++)
				if (*ptr == COMMATOK)
					j++;
			j += 1;
			count = allocate_count(
				    configptr->configParamList[i]);
			if (count > 0) {
				valueptr = (void **)realloc(valueptr,
				    ((sizeof (void *)) * (j + count + 1)));
			} else {
				valueptr = (void **)malloc((sizeof (void *)) *
					(j + 1));
				if (valueptr == NULL)
					return (NULL);
			}
			strptr = strtok_r(value, ",", &rest);
			j = count;
			while (strptr != NULL) {
				valueptr[j] = (void *)stripdup(strptr);
				if (valueptr[j] == NULL)
					return (NULL);
				strptr = strtok_r(NULL, ",", &rest);
				j++;
			}
			valueptr[j] = NULL;
			break;
		case	NS_LDAP_SEARCH_DN_P:
			valueptr = configptr->configParamList[i];
			count = allocate_count(
				    configptr->configParamList[i]);
			if (count > 0) {
				valueptr = (void **)realloc(valueptr,
				    ((sizeof (void *)) * (count + 2)));
				count = count + 1;
			} else {
				valueptr = (void **)malloc(((sizeof (void *))
					    * 2));
				if (valueptr ==  NULL)
					return (NULL);
				count = 1;
			}
			valueptr[count - 1] = (void *)strdup(value);
			if (valueptr[count - 1] == NULL)
				return (NULL);
			valueptr[count] = NULL;
			break;
		case	NS_LDAP_AUTH_P:
			valueptr = configptr->configParamList[i];
			for (j = 0, ptr = value; *ptr != '\0'; ptr++)
				if (*ptr == COMMATOK)
					j++;
			j += 1;
			valueptr = (void **)malloc((sizeof (void *)*
					(j + 1)));
			if (valueptr == NULL)
				return (NULL);
			strptr = strtok_r(value, ",", &rest);
			j = 0;
			while (strptr != NULL) {
				char *tmp = NULL;

				authptr =
				    (AuthType_t *)malloc((sizeof (AuthType_t)));
				if (authptr == NULL)
					return (NULL);
				tmp = stripdup(strptr);
				(void) get_auth_type(tmp, &auth);
				*authptr = auth;
				valueptr[j] = (void *)authptr;
				strptr = strtok_r(NULL, ",", &rest);
				j++;
				free(tmp);
			}
			valueptr[j] = NULL;
			break;
		case	NS_LDAP_TRANSPORT_SEC_P:
		case	NS_LDAP_SEARCH_SCOPE_P:
			valueptr = (void **)malloc((sizeof (void *))*2);
			if (valueptr == NULL)
				return (NULL);
			if (i == NS_LDAP_TRANSPORT_SEC_P) {
				SecType_t	*secptr, sec;

				secptr = (SecType_t *)
					malloc((sizeof (SecType_t)));
				if (secptr == NULL)
					return (NULL);
				(void) get_security_type(value, &sec);
				*secptr = sec;
				valueptr[0] = (void *)secptr;
			} else {
				ScopeType_t	*scopeptr, scope;

				scopeptr = (ScopeType_t *)
					malloc((sizeof (ScopeType_t)));
				if (scopeptr == NULL)
					return (NULL);
				(void) get_scope_type(value, &scope);
				*scopeptr = scope;
				valueptr[0] = (void *)scopeptr;
			}
			valueptr[1] = NULL;
			break;
		case	NS_LDAP_SEARCH_REF_P:
			valueptr = (void **)malloc((sizeof (void *))*2);
			if (valueptr == NULL)
				return (NULL);
			searchptr = (SearchRef_t *)
					malloc((sizeof (SearchRef_t)));
			if (searchptr == NULL)
				return (NULL);
			(void) get_searchref_type(value, &search);
			*searchptr = search;
			valueptr[0] = (void *)searchptr;
			valueptr[1] = NULL;
			break;
		case	NS_LDAP_PREF_ONLY_P:
			valueptr = (void **)malloc((sizeof (void *))*2);
			if (valueptr == NULL)
				return (NULL);
			prefptr = (PrefOnly_t *) malloc(sizeof (PrefOnly_t));
			if (prefptr == NULL)
				return (NULL);
			(void) get_pref_type(value, &pref);
			*prefptr = pref;
			valueptr[0] = (void *)prefptr;
			valueptr[1] = NULL;
			break;
		case	NS_LDAP_SEARCH_TIME_P:
		case	NS_LDAP_CACHETTL_P:
			{
			    time_t	t;
			    char	buf[20];

			    valueptr = (void **)malloc((sizeof (void *))*2);
			    memset(buf, 0, sizeof (buf));
			    if (valueptr == NULL)
				return (NULL);
			    t = conv_time(value);
			    valueptr[0] = (void *)strdup(
				lltostr((long)t, &buf[sizeof (buf) - 1]));
			    if (valueptr[0] == NULL)
				return (NULL);
			    valueptr[1] = NULL;
			}
			break;
		case	NS_LDAP_SEARCH_BASEDN_P:
		case	NS_LDAP_BINDDN_P:
		case	NS_LDAP_BINDPASSWD_P:
		case	NS_LDAP_DOMAIN_P:
		case	NS_LDAP_EXP_P:
		case	NS_LDAP_FILE_VERSION_P:
		case	NS_LDAP_CERT_PATH_P:
		case	NS_LDAP_CERT_PASS_P:
		case	NS_LDAP_PROFILE_P:
			valueptr = (void **)malloc((sizeof (void *))*2);
			if (valueptr == NULL)
				return (NULL);
			valueptr[0] = (void *)strdup(value);
			if (valueptr[0] == NULL)
				return (NULL);
			valueptr[1] = NULL;
			break;
	}

	return (valueptr);
}


static int
configtype(int i)
{

	switch (i) {
		case	NS_LDAP_EXP_P:
		case	NS_LDAP_SERVERS_P:
		case	NS_LDAP_SEARCH_BASEDN_P:
			return (SERVERCONFIG);

		case	NS_LDAP_CACHETTL_P:
		case	NS_LDAP_AUTH_P:
		case	NS_LDAP_TRANSPORT_SEC_P:
		case	NS_LDAP_SEARCH_REF_P:
		case	NS_LDAP_SEARCH_SCOPE_P:
		case	NS_LDAP_DOMAIN_P:
		case	NS_LDAP_FILE_VERSION_P:
		case	NS_LDAP_SEARCH_DN_P:
		case	NS_LDAP_PREF_ONLY_P:
		case	NS_LDAP_SEARCH_TIME_P:
		case	NS_LDAP_SERVER_PREF_P:
		case	NS_LDAP_PROFILE_P:
			return (CLIENTCONFIG);

		case	NS_LDAP_BINDDN_P:
		case	NS_LDAP_BINDPASSWD_P:
		case	NS_LDAP_CERT_PATH_P:
		case	NS_LDAP_CERT_PASS_P:
			return (CREDCONFIG);

		default:
			return (-1);
	}
}


static DomainParamType *
load_def_cfg(int type)
{
	ParamIndexType	i;
	DomainParamType	*configptr;
	char		domainname[MAXHOSTNAMELEN];

	configptr = create_config();
	if (configptr == NULL) {
		return (NULL);
	}
	for (i = 0; i <= LAST_VALUE; i++) {
		if (i == NS_LDAP_DOMAIN_P &&
			configtype((int) i) == type) {
			if (__getdomainname(domainname,
					sizeof (domainname)) >= 0)
				configptr->configParamList[i] =
						allocate_value(i, domainname,
						    configptr);
			else
				configptr->configParamList[i] = NULL;
			continue;
		} else if (i == NS_LDAP_EXP_P) {
			struct timeval  tp;
			time_t	newexp = 0;
			char	buf[20];

			if (gettimeofday(&tp, NULL) == -1) {
				newexp = 0;
			} else {
				newexp = (u_int) tp.tv_sec +
					atol(EXP_DEFAULT_TTL);
			}
			memset((void *)buf, 0, sizeof (buf));
			configptr->configParamList[i] = allocate_value(i,
				lltostr(newexp, &buf[sizeof (buf) - 1]),
				configptr);
			continue;
		}
		if (defconfig[i].defval != NULL &&
			configtype((int) i) == type) {
			configptr->configParamList[i] =
				allocate_value(i, defconfig[i].defval,
				    configptr);
		}
	}

	return (configptr);
}


static int
is_setmodifiable(ParamIndexType index)
{
	/* Currently, only programs which has done a */
	/* setServer call, will be allowed to do a */
	/* setParam call.  Hence we will return */
	/* 0 == NOTALLOWED for is_setmodifiable */
	/* Programs which have done a setServer call */
	/* bypass the is_setmodifiable call */

	switch (index) {
		case	NS_LDAP_EXP_P:
		case	NS_LDAP_CACHETTL_P:
		case	NS_LDAP_SERVERS_P:
		case	NS_LDAP_SEARCH_BASEDN_P:
		case	NS_LDAP_AUTH_P:
		case	NS_LDAP_TRANSPORT_SEC_P:
		case	NS_LDAP_SEARCH_REF_P:
		case	NS_LDAP_SEARCH_SCOPE_P:
		case	NS_LDAP_DOMAIN_P:
		case	NS_LDAP_FILE_VERSION_P:
		case	NS_LDAP_SEARCH_DN_P:
		case	NS_LDAP_PREF_ONLY_P:
		case	NS_LDAP_SEARCH_TIME_P:
		case	NS_LDAP_SERVER_PREF_P:
		case	NS_LDAP_PROFILE_P:
		case	NS_LDAP_BINDDN_P:
		case	NS_LDAP_BINDPASSWD_P:
		case	NS_LDAP_CERT_PATH_P:
		case	NS_LDAP_CERT_PASS_P:
			return (0);
	}
	return (0);
}


static ns_parse_status
crosscheck(DomainParamType *domainptr, char *errstr)
{
	if (domainptr == NULL)
		return (NS_SUCCESS);

	/* check for no server specified */
	if (domainptr->configParamList[NS_LDAP_SERVERS_P] == NULL) {
		sprintf(errstr,
			gettext("Configuration Error: No entry for "
				"'%s' found"),
				defconfig[NS_LDAP_SERVERS_P].name);
		return (NS_PARSE_ERR);
	}
	if (domainptr->configParamList[NS_LDAP_CERT_PASS_P] != NULL &&
		domainptr->configParamList[NS_LDAP_CERT_PATH_P] == NULL) {
			sprintf(errstr,
			gettext("Configuration Error: %s specified "
				"but no value for '%s' found"),
				defconfig[NS_LDAP_CERT_PASS_P].name,
				defconfig[NS_LDAP_CERT_PATH_P].name);
		return (NS_PARSE_ERR);
	}
	if (domainptr->configParamList[NS_LDAP_CERT_PASS_P] == NULL &&
		domainptr->configParamList[NS_LDAP_CERT_PATH_P] != NULL) {
			sprintf(errstr,
			gettext("Configuration Error: %s specified "
				"but no value for '%s' found"),
				defconfig[NS_LDAP_CERT_PATH_P].name,
				defconfig[NS_LDAP_CERT_PASS_P].name);
		return (NS_PARSE_ERR);
	}
	/* check if search basedn has been specified */
	/* check for no server specified */
	if (domainptr->configParamList[NS_LDAP_SEARCH_BASEDN_P] == NULL) {
		sprintf(errstr,
			gettext("Configuration Error: No entry for "
				"'%s' found"),
				defconfig[NS_LDAP_SEARCH_BASEDN_P].name);
		return (NS_PARSE_ERR);
	}
	/* check that cache version has been set */
	if (domainptr->configParamList[NS_LDAP_FILE_VERSION_P] == NULL) {
			sprintf(errstr,
			gettext("Configuration Error: No entry for "
				"'%s' found"),
				defconfig[NS_LDAP_FILE_VERSION_P].name);
		return (NS_PARSE_ERR);
	}
	/* check that domain has been set */
	if (domainptr->configParamList[NS_LDAP_DOMAIN_P] == NULL) {
			sprintf(errstr,
			gettext("Configuration Error: No entry for "
				"'%s' found"),
				defconfig[NS_LDAP_DOMAIN_P].name);
		return (NS_PARSE_ERR);
	}

	/* check for auth value....passwd/bindn etc */
	/* check that auth has been set */
	if (domainptr->configParamList[NS_LDAP_AUTH_P] == NULL) {
			sprintf(errstr,
			gettext("Configuration Error: No entry for "
				"'%s' found"),
				defconfig[NS_LDAP_AUTH_P].name);
		return (NS_PARSE_ERR);
	} else {
	    int	*value, j;

	    for (j = 0; domainptr->configParamList[NS_LDAP_AUTH_P][j] != NULL;
		j++) {

		value = (int *) domainptr->configParamList[NS_LDAP_AUTH_P][j];
		switch (*value) {
			case NS_LDAP_AUTH_SIMPLE:
			case NS_LDAP_AUTH_SASL_CRAM_MD5:
			if (domainptr->configParamList[NS_LDAP_BINDDN_P]
				== NULL) {
				sprintf(errstr,
				gettext("Configuration Error: No entry for "
					"'%s' found"),
					defconfig[NS_LDAP_BINDDN_P].name);
				return (NS_PARSE_ERR);
			}
			if (domainptr->configParamList[NS_LDAP_BINDPASSWD_P]
				== NULL) {
				sprintf(errstr,
				gettext("Configuration Error: No entry for "
					"'%s' found"),
					defconfig[NS_LDAP_BINDPASSWD_P].name);
				return (NS_PARSE_ERR);
			}
			break;
		}
	    }
	}

	return (NS_SUCCESS);
}


static int
get_type(char *value, ParamIndexType *type)
{
	ParamIndexType	i;

	for (i = 0; i <= LAST_VALUE; i++) {
		if (strcasecmp(defconfig[i].name, value) == 0) {
			*type = i;
			return (0);
		}
	}
	return (-1);
}


static void
cleanup_default(void **listptr)
{
	int	i;
	void	*ptr;

	if (listptr == NULL)
		return;
	for (i = 0, ptr = listptr[i]; ptr != NULL; i++, ptr = listptr[i])
		free(ptr);

	free(listptr);
}


static DomainParamType *
set_default_value(DomainParamType *configptr, char *name, char *value)
{
	ParamIndexType	i;

	if (get_type(name, &i) != 0)
		return (NULL);

	if (i != NS_LDAP_SERVERS_P &&
		i != NS_LDAP_SERVER_PREF_P &&
		i != NS_LDAP_SEARCH_DN_P) {
		if (configptr->configParamList[i] != NULL) {
			cleanup_default(configptr->configParamList[i]);
			configptr->configParamList[i] = NULL;
		}
	}

	configptr->configParamList[i] =  (void **)allocate_value(i, value,
						configptr);

	return (configptr);
}


static ns_parse_status
verify_value(char *name, char *value, char *errstr)
{
	ParamIndexType	index = 0;
	int		found = 0, j;
	char		*ptr = NULL, *strptr = NULL, buffer[BUFSIZE];
	char		*port;
	char		*ldapvalue;
	char		*rest;

	if (get_type(name, &index) != 0) {
		sprintf(errstr,
			gettext("Unknown keyword encountered '%s'."), name);
		return (NS_PARSE_ERR);
	}

	/* eat up beginning quote, if any */
	while (value != NULL && (*value == QUOTETOK || *value == SPACETOK))
		value++;

	/* eat up space/quote at end of value */
	if (strlen(value) > 0)
		ptr = value + strlen(value) - 1;
	else
		ptr = value;
	for (; ptr != value && (*ptr == SPACETOK || *ptr == QUOTETOK); ptr--) {
		*ptr = '\0';
	}

	switch (index) {
		case	NS_LDAP_EXP_P:
		case	NS_LDAP_CACHETTL_P:
		case	NS_LDAP_CERT_PATH_P:
		case	NS_LDAP_CERT_PASS_P:
		case	NS_LDAP_BINDDN_P:
		case	NS_LDAP_BINDPASSWD_P:
		case	NS_LDAP_DOMAIN_P:
		case    NS_LDAP_SEARCH_BASEDN_P:
		case	NS_LDAP_SEARCH_TIME_P:
		case	NS_LDAP_PROFILE_P:
			break;
		case	NS_LDAP_SEARCH_DN_P:
			if ((ldapvalue = ldap_dn2ufn(value)) == NULL) {
				sprintf(errstr,
					gettext("Unable to verify "
					    "name: %s value: '%s'."),
					    "NS_LDAP_SEARCH_BASEDN", value);
					return (NS_PARSE_ERR);
			}
			free(ldapvalue);
			break;
		case	NS_LDAP_FILE_VERSION_P:
			if (value != NULL &&
				strcasecmp(value, NS_LDAP_VERSION) != 0) {
				sprintf(errstr,
					gettext("Version mismatch, expected "
					    "cache version '%s' but "
					    "encountered version '%s'."),
					    NS_LDAP_VERSION, value);
					return (NS_PARSE_ERR);
			}
			break;
		case    NS_LDAP_SERVERS_P:
		case	NS_LDAP_SERVER_PREF_P:
			strcpy(buffer, value);
			strptr = strtok_r(buffer, ",", &rest);
			while (strptr != NULL) {
				int	portnum = 0;
				char	*tmp = NULL;
				tmp = stripdup(strptr);
				if (strchr(tmp, ' ') != NULL) {
					sprintf(errstr,
					    gettext("Invalid parameter values "
					    "'%s' specified for keyword '%s'."),
					    tmp, name);
					free(tmp);
					return (NS_PARSE_ERR);
				}
				if ((port = strchr(strptr, ':')) != NULL) {
					*port = '\0';
					port++;
				}
				if (port != NULL &&
					(((portnum = atoi(port)) == 0) ||
					(portnum > MAXPORTNUMBER) ||
					(portnum < 0))) {
					sprintf(errstr,
					    gettext("Invalid port "
					    "number '%s' specified with "
					    "address %s for "
					    "keyword '%s'."),
					    port, strptr, name);
					free(tmp);
					return (NS_PARSE_ERR);
				}
				if ((strcasecmp(strptr, "localhost") != 0) &&
				    ((int)inet_addr(strptr) == -1)) {
					sprintf(errstr,
					    gettext("Invalid Internet "
					    "address value '%s' specified for "
					    "keyword '%s'."), strptr, name);
					free(tmp);
					return (NS_PARSE_ERR);
				}
				free(tmp);
				strptr = strtok_r(NULL, ",", &rest);
			}
			break;
		case	NS_LDAP_AUTH_P:
		case	NS_LDAP_TRANSPORT_SEC_P:
		case	NS_LDAP_SEARCH_SCOPE_P:
			strcpy(buffer, value);
			strptr = strtok_r(buffer, ",", &rest);
			while (strptr != NULL) {
				AuthType_t	auth;
				SecType_t	sec;
				ScopeType_t	scope;
				char	*tmp = NULL;

				tmp = stripdup(strptr);
				if (strchr(tmp, ' ') != NULL) {
					sprintf(errstr,
					    gettext("Invalid parameter values "
					    "'%s' specified for keyword '%s'."),
					    tmp, name);
					free(tmp);
					return (NS_PARSE_ERR);
				}

				if (index == NS_LDAP_AUTH_P) {
					if (get_auth_type(tmp,
						&auth) == -1) {
						sprintf(errstr,
						    gettext("Unknown value "
						    "'%s' encountered for "
						    "keyword '%s'."),
						    tmp, name);
						free(tmp);
						return (NS_PARSE_ERR);
					}
				} else if (index == NS_LDAP_TRANSPORT_SEC_P) {
					if (get_security_type(tmp,
						&sec) == -1) {
						sprintf(errstr,
						    gettext("Unknown value "
						    "'%s' encountered for "
						    "keyword '%s'."),
						    tmp, name);
						free(tmp);
						return (NS_PARSE_ERR);
					}
				} else {
					if (get_scope_type(tmp,
						&scope) == -1) {
						sprintf(errstr,
						    gettext("Unknown value "
						    "'%s' encountered for "
						    "keyword '%s'."),
						    tmp, name);
						free(tmp);
						return (NS_PARSE_ERR);
					}
				}
				free(tmp);
				strptr = strtok_r(NULL, ",", &rest);
			}
			break;
		default :
			found = 0; j = 0;
			while (defconfig[index].allowed[j] != NULL &&
			    j < DEFMAX) {
				if (strcmp(defconfig[index].allowed[j],
				    value) == 0) {
					found = 1;
					break;
				}
				j++;
			}
			if (!found) {
				sprintf(errstr,
				    gettext("Invalid option specified for "
				    "'%s' keyword. '%s' is not a recognized "
				    "keyword value."), name, value);
				return (NS_PARSE_ERR);
			}
	}

	return (NS_SUCCESS);
}


static int
read_line(int fd, char *buffer, int buflen, char *errstr)
{
	int	linelen;
	int	rc;
	char	c, buf[2];

	/*CONSTCOND*/
	while (1) {
		linelen = 0;
		while (linelen < buflen) {
			rc = read(fd, buf, 1);
			if (1 == rc) {
				c = buf[0];
				switch (c) {
					case	'\n':
					    if (linelen > 0 &&
						buffer[linelen - 1] == '\\') {
						--linelen;
					    } else {
						buffer[linelen    ] = '\0';
						return (linelen);
					    }
					    break;
					default:
					    buffer[linelen++] = c;
				}
			} else {
				if (linelen == 0 ||
				    buffer[linelen - 1] == '\\') {
					return (-1);
				} else {
					buffer[linelen    ] = '\0';
					return (linelen);
				}
			}
		}
		/* Buffer overflow -- eat rest of line and loop again */
		do {
			rc = read(fd, buf, 1);
			if (-1 == rc) {
				sprintf(errstr,
					gettext("Internal Error. Parsing "
					"buffer overflow."));
				return (-1);
			}
			c = buf[0];
		} while (c != '\n');
	}
	/*NOTREACHED*/
}


static void
split_key_value(char *buffer, int buflen, char **name, char **value)
{
	char	*ptr;

	if (buflen == 0)
		return;

	*name = buffer;
	/* split into name value pair */
	if ((ptr = strchr(buffer, TOKENSEPARATOR)) != NULL) {
		*value = ptr+1;
		*ptr = '\0';
	}
}


static void
print_value(int fd, ParamIndexType index, void **value, char *str, int ldif)
{
	int	first, i = 0, *intptr;
	char	*ptr, string[BUFSIZE], obuf[BUFSIZ];

	if (value == NULL || (str != NULL && ldif == 1))
		return;

	switch (index) {
	case NS_LDAP_AUTH_P:
	case NS_LDAP_TRANSPORT_SEC_P:
	case NS_LDAP_SEARCH_SCOPE_P:
		first = 1;
		for (i = 0, intptr = (int *)value[i];
		    intptr != NULL;
		    i++, intptr = (int *)value[i]) {
			if (first == 1) {
				first = 0;
				if (index == NS_LDAP_AUTH_P)
					strcpy(string,
					    get_auth_name(*intptr));
				else if (index == NS_LDAP_TRANSPORT_SEC_P)
					strcpy(string,
						get_security_name(*intptr));
				else
					strcpy(string,
						get_scope_name(*intptr));
			} else {
				strcat(string, ",");
				if (index == NS_LDAP_AUTH_P)
					strcat(string,
					    get_auth_name(*intptr));
				else if (index == NS_LDAP_TRANSPORT_SEC_P)
					strcat(string,
					    get_security_name(*intptr));
				else
					strcpy(string,
					    get_scope_name(*intptr));
			}
		}
		if (str == NULL) {
			if (ldif == 0)
				snprintf(obuf, BUFSIZ, "%s= %s\n",
					defconfig[index].name, string);
			else
				snprintf(obuf, BUFSIZ, "\t%s: %s\n",
					defconfig[index].profile_name, string);
			write(fd, obuf, strlen(obuf));
		} else
			snprintf(str, BUFSIZE, "%s=%s", defconfig[index].name,
								string);
		break;
	case NS_LDAP_SERVERS_P:
	case NS_LDAP_SEARCH_DN_P:
	case NS_LDAP_SERVER_PREF_P:
		first = 1;
		for (i = 0, ptr = (char *)value[i];
		ptr != NULL;
		    i++, ptr = (char *)value[i]) {
			if (first == 1) {
				first = 0;
				strcpy(string, ptr);
			} else {
				strcat(string, ", ");
				strcat(string, ptr);
			}
		}
		if (str == NULL) {
			if (ldif == 0)
				snprintf(obuf, BUFSIZ, "%s= %s\n",
					defconfig[index].name, string);
			else
				snprintf(obuf, BUFSIZ, "\t%s: %s\n",
					defconfig[index].profile_name, string);
			write(fd, obuf, strlen(obuf));
		} else
			snprintf(str, BUFSIZE, "%s=%s", defconfig[index].name,
								string);
		break;
	case NS_LDAP_PREF_ONLY_P:
		intptr = (int *)value[0];
		if (str == NULL) {
			if (ldif == 0)
				snprintf(obuf, BUFSIZ, "%s= %s\n",
						defconfig[index].name,
						get_pref_name(*intptr));
			else
				snprintf(obuf, BUFSIZ, "\t%s: %s\n",
					defconfig[index].profile_name,
					get_pref_name(*intptr));
			write(fd, obuf, strlen(obuf));
		} else
			snprintf(str, BUFSIZE, "%s=%s", defconfig[index].name,
				get_pref_name(*intptr));
		break;
	case NS_LDAP_SEARCH_REF_P:
		intptr = (int *)value[0];
		if (str == NULL) {
			if (ldif == 0)
				snprintf(obuf, BUFSIZ, "%s= %s\n",
					defconfig[index].name,
					get_searchref_name(*intptr));
			else
				snprintf(obuf, BUFSIZ, "\t%s: %s\n",
					defconfig[index].profile_name,
					get_searchref_name(*intptr));
			write(fd, obuf, strlen(obuf));
		} else
			snprintf(str, BUFSIZE, "%s=%s", defconfig[index].name,
				get_searchref_name(*intptr));
		break;
	case NS_LDAP_CACHETTL_P:
	case NS_LDAP_EXP_P:
		if (((ldif != 0) && (index == NS_LDAP_EXP_P)) ||
				((ldif == 0) && (index == NS_LDAP_CACHETTL_P)))
			/*
			 * don't print expiration time if printing in LDIF
			 * format (ie. called from ldap_gen_profile)
			 * don't print cache ttl time if not printing in LDIF
			 * format (ie. called from ldap_client)
			 */
			break;
	default:
		first = 1;
		for (i = 0, ptr = (char *)value[i];
		    ptr != NULL;
		    i++, ptr = (char *)value[i]) {
			if (first == 1) {
				first = 0;
				strcpy(string, ptr);
			} else {
				strcat(string, ", ");
				strcat(string, ptr);
			}
		}
		if (str == NULL) {
			if (ldif == 0)
				snprintf(obuf, BUFSIZ, "%s= %s\n",
						defconfig[index].name, string);
			else {
				snprintf(obuf, BUFSIZ, "\t%s: %s\n",
					defconfig[index].profile_name,
					string);
			}
			write(fd, obuf, strlen(obuf));
		} else
			snprintf(str, BUFSIZE, "%s=%s",
						defconfig[index].name, string);
	}
}


static ns_parse_status
readcfile(DomainParamType *ptr, char *errstr)
{
	char	*name, *value;
	int	fd, buflen = BUFSIZE, lineno;
	char	buffer[BUFSIZE];
	char	errbuf[MAXERROR];
	ParamIndexType	type;
	int	always = ALWAYS;

	if (ptr == NULL) {
		sprintf(errstr, gettext("No ParamType information passed\n"));
		return (NS_PARSE_ERR);
	}
	if (-1 == (fd = open(NSCREDFILE, O_RDONLY))) {
		/* default case for non-root users */
		return (NS_SUCCESS);
	}
	fd = __ns_ldap_raise_fd(fd);

	lineno = 0;
	do {
		int	linelen;

		if ((linelen = read_line(fd, buffer,
		    buflen, errstr)) < 0)
			/* End of file */
			break;
		/* get rid of comment lines */
		if (buflen > 2 &&
		    (buffer[0] == '#' && buffer[1] != '#'))
			continue;
		lineno++;
		if (linelen != 0) {
			split_key_value(buffer, buflen, &name, &value);
			if (verify_value(name, value, errbuf) !=
			    NS_SUCCESS) {
				sprintf(errstr,
				    gettext("%s (at or near line "
				    "%d).\n"), errbuf, lineno);
				close(fd);
				return (NS_PARSE_ERR);
			}
			/* add to current entry. */
			if (get_type(name, &type) != 0)
				return (NS_PARSE_ERR);
			if (configtype(type) == CREDCONFIG)
				ptr = set_default_value(ptr, name, value);
		}
	} while (always);
	close(fd);
	return (NS_SUCCESS);
}

ns_ldap_error_t *
__ns_ldap_LoadDoorInfo(LineBuf *configinfo, char *domainname)
{
	DomainParamType	*ptr;
	char		errstr[MAXERROR];
	char		string[BUFSIZE];
	ParamIndexType	i = 0;
	ns_ldap_error_t	*errorp;

	mutex_lock(&ns_parse_lock);
	if (config_loaded == 1) {
		if (config == NULL) {
			sprintf(errstr,
			    gettext("No configuration "
			    "information available for %s."),
			    domainname == NULL ? "<no domain specified>" :
			    domainname);
			MKERROR(errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
				NULL);
			mutex_unlock(&ns_parse_lock);
			return (errorp);
		}
		memset((char *) configinfo, 0, sizeof (LineBuf));
		for (ptr = config; ptr != NULL; ptr = ptr->next) {
			for (i = 0; i <= LAST_VALUE; i++) {
				if (ptr->configParamList[i] != NULL) {
					print_value(NULL, i,
					    ptr->configParamList[i], string, 0);
					if (_print2buf(configinfo,
						string) != 0) {
						sprintf(errstr,
						    gettext("_print2buf: Out "
						    "of memory."));
						MKERROR(errorp,
						    NS_CONFIG_NOTLOADED,
						    strdup(errstr), NULL);
						mutex_unlock(&ns_parse_lock);
						return (errorp);
					}
					if (_print2buf(configinfo,
						DOORLINESEP) != 0) {
						sprintf(errstr,
						    gettext("_print2buf: Out "
						    "of memory."));
						MKERROR(errorp,
						    NS_CONFIG_NOTLOADED,
						    strdup(errstr), NULL);
						mutex_unlock(&ns_parse_lock);
						return (errorp);
					}
				}
			}
		}
	}
	mutex_unlock(&ns_parse_lock);
	return (NULL);
}


ns_ldap_error_t *
__ns_ldap_DumpLdif(char *filename)
{
	DomainParamType	*ptr;
	char		errstr[MAXERROR];
	char		obuf[BUFSIZ];
	int		fd;
	ParamIndexType	i = 0;
	ns_ldap_error_t	*errorp;

	mutex_lock(&ns_parse_lock);
	if (config_loaded == 1) {
		if (config == NULL) {
			sprintf(errstr,
			    gettext("No configuration "
			    "information available."));
			MKERROR(errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
				NULL);
			mutex_unlock(&ns_parse_lock);
			return (errorp);
		}
		if (filename == NULL) {
			fd = 1; /* stdout */
		} else {
			if (-1 == (fd = open(filename, O_WRONLY | O_CREAT,
								0444))) {
				sprintf(errstr,
					gettext("Unable to open filename %s"
					" for ldif "
					"dump (errno=%d)."),
					filename, errno);
				MKERROR(errorp, NS_CONFIG_FILE,
					strdup(errstr), NULL);
				mutex_unlock(&ns_parse_lock);
				return (errorp);
			}
			fd = __ns_ldap_raise_fd(fd);
		}

		ptr = config;
		if (ptr->configParamList[NS_LDAP_SEARCH_BASEDN_P] == NULL ||
			ptr->configParamList[NS_LDAP_PROFILE_P] == NULL) {
			sprintf(errstr,
				gettext("Required BaseDN and/or Profile name "
					"ldif fields not present"));
			MKERROR(errorp, NS_CONFIG_FILE,
				strdup(errstr), NULL);
			mutex_unlock(&ns_parse_lock);
			return (errorp);
		}

		snprintf(obuf, BUFSIZ, "dn: cn=%s,ou=%s,%s\n",
			(char *)ptr->configParamList[NS_LDAP_PROFILE_P][0],
			_PROFILE_CONTAINER,
			(char *)
			ptr->configParamList[NS_LDAP_SEARCH_BASEDN_P][0]);
		write(fd, obuf, strlen(obuf));

		for (; ptr != NULL; ptr = ptr->next) {
			for (i = 0; i <= LAST_VALUE; i++) {
				if (defconfig[i].profile_name != NULL &&
					ptr->configParamList[i] != NULL &&
					i != NS_LDAP_FILE_VERSION_P) {
						print_value(fd, i,
							ptr->configParamList[i],
							NULL, 1);
					}
				}
		}
		/* dump objectclass names */
		snprintf(obuf, BUFSIZ,
			"\tObjectClass: top\n\tObjectClass: %s\n",
			_PROFILE_OBJECTCLASS);
		write(fd, obuf, strlen(obuf));
		if (filename != NULL)
			close(fd);
	}
	mutex_unlock(&ns_parse_lock);
	return (NULL);
}


ns_ldap_error_t *
__ns_ldap_DumpConfiguration(char * filename)
{
	DomainParamType	*ptr;
	char		obuf[BUFSIZ];
	char		errstr[MAXERROR];
	ParamIndexType	i = 0;
	int		fd, rc;
	ns_ldap_error_t	*errorp;
	struct stat	buf;
	int		cfgtype;

	if (filename != NULL)
		cfgtype = strcasecmp(filename, NSCONFIGFILE) == 0 ||
			strcasecmp(filename, NSCONFIGREFRESH) == 0
			? CONFIGFILE : CREDFILE;
	mutex_lock(&ns_parse_lock);
	if (config_loaded == 1) {
		if (config == NULL) {
			sprintf(errstr,
			    gettext("No configuration "
			    "information available."));
			MKERROR(errorp, NS_CONFIG_NOTLOADED, strdup(errstr),
				NULL);
			mutex_unlock(&ns_parse_lock);
			return (errorp);
		}
		if (filename == NULL) {
			fd = 2;	/* stderr */
		} else {
			if (cfgtype == CREDFILE) {
				rc = stat(NSCREDFILE, &buf);
				if (rc == 0)
					fd = open(filename,
						O_WRONLY | O_CREAT | O_TRUNC,
						buf.st_mode);
				else
					fd = open(filename, O_WRONLY | O_CREAT,
						0400);
				if (fd < 0) {
					sprintf(errstr,
					    gettext("Unable to open filename %s"
					    " for configuration "
					    "dump (errno=%d)."),
					    filename, errno);
					MKERROR(errorp, NS_CONFIG_FILE,
						strdup(errstr), NULL);
					mutex_unlock(&ns_parse_lock);
					return (errorp);
				}
			} else {
				if (-1 == (fd = open(filename,
					O_WRONLY | O_CREAT | O_TRUNC, 0444))) {
					sprintf(errstr,
					    gettext("Unable to open filename %s"
					    " for configuration "
					    "dump (errno=%d)."),
					    filename, errno);
					MKERROR(errorp, NS_CONFIG_FILE,
						strdup(errstr), NULL);
					mutex_unlock(&ns_parse_lock);
					return (errorp);
				}
			}
			fd = __ns_ldap_raise_fd(fd);
		}
		snprintf(obuf, BUFSIZ, "#\n# %s\n#\n", DONOTEDIT);
		write(fd, obuf, strlen(obuf));
		if (NULL == config->configParamList[NS_LDAP_FILE_VERSION_P]) {
			config = set_default_value(config,
						"NS_LDAP_FILE_VERSION",
							NS_LDAP_VERSION);
		}
		for (ptr = config; ptr != NULL; ptr = ptr->next) {
			for (i = 0; i <= LAST_VALUE; i++) {
				if (cfgtype == CREDFILE) {
					if ((configtype(i) == CREDCONFIG) &&
					    ptr->configParamList[i] != NULL) {
						print_value(fd, i,
							ptr->configParamList[i],
							NULL, 0);
					}
				} else {
					if ((configtype(i) != CREDCONFIG) &&
					    ptr->configParamList[i] != NULL) {
						print_value(fd, i,
							ptr->configParamList[i],
							NULL, 0);
					}
				}
			}
		}
		if (filename != NULL)
			close(fd);
	}
	mutex_unlock(&ns_parse_lock);
	return (NULL);
}


int
__ns_ldap_setParam(const char *domain, const ParamIndexType type,
		const void *data, ns_ldap_error_t **error)
{
	char		errstr[2 * MAXERROR], *name;
	DomainParamType	*ptr;
	ns_ldap_error_t	*errorp;
	char		domainname[MAXHOSTNAMELEN];

	if (__getdomainname(domainname,
	    sizeof (domainname)) < 0) {
		sprintf(errstr, gettext("Unable to get current domain name"));
		MKERROR(*error, NS_CONFIG_NODOMAIN, strdup(errstr), NULL);
		return (NS_LDAP_CONFIG);
	}

	if (domain == NULL) {
		domain = domainname;
	} else {
		if (strcasecmp(domain, domainname) != 0) {
			sprintf(errstr,
			    gettext("No configuration information "
			    "available for specified domainname "
			    "'%s'."), domain);
			MKERROR(*error, NS_CONFIG_NODOMAIN,
				strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
	}

	if (cache_server == FALSE) {
		if (is_setmodifiable(type) == 0) {
			sprintf(errstr,
			    gettext("Unable to Set value.  Given parameter "
			    "'%s' is not modifiable through the "
			    " __ns_ldap_setParam() "
			    "function call"), defconfig[type].name);
			MKERROR(*error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
		if (configtype((int) type)  == SERVERCONFIG && domain == NULL) {
			sprintf(errstr,
			    gettext("Unable to Set value.  Given "
			    "parameter '%s' "
			    "must be set in a domain specific context."),
			    defconfig[type].name);
			MKERROR(*error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
		if (configtype((int) type) == CLIENTCONFIG && domain != NULL) {
			sprintf(errstr,
			    gettext("Unable to Set value.  Given "
			    "parameter '%s' "
			    "must not be set in a domain specific context."),
			    defconfig[type].name);
			MKERROR(*error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
	}

	if (config_loaded != 1 || timetorefresh() == 1) {
		if ((errorp = __ns_ldap_LoadConfiguration((char *)domain))
		    != NULL) {
			/* reload of new configuration failed */
			sprintf(errstr, gettext("Unable to load new "
				"information from configuration file "
				"'%s' ('%s')."),
				NSCONFIGFILE, errorp->message);
			syslog(LOG_ERR, errstr);
			__ns_ldap_freeError(&errorp);
			if (!config) {
				MKERROR(*error, NS_CONFIG_NOTLOADED,
					strdup(errstr), NULL);
				return (NS_LDAP_CONFIG);
			}
		}
	}

	mutex_lock(&ns_parse_lock);
	if (config_loaded == 1) {
		if ((ptr = get_domainptr((char *) domain)) == NULL) {
			sprintf(errstr,
			    gettext("Unable to Set value.  No "
			    "configuration information "
			    "available for specified domainname "
			    "'%s'."), domain);
			mutex_unlock(&ns_parse_lock);
			MKERROR(*error, NS_CONFIG_NODOMAIN,
				strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		} else {
			name = defconfig[type].name;
			if (verify_value(name, (char *) data, errstr) !=
			    NS_SUCCESS) {
				sprintf(errstr,
					gettext("%s"), errstr);
				MKERROR(*error, NS_CONFIG_SYNTAX,
					strdup(errstr), NULL);
				mutex_unlock(&ns_parse_lock);
				return (NS_LDAP_CONFIG);
			}
			destroy_param(ptr->configParamList[type]);
			ptr->configParamList[type] = NULL;

			/*
			 * if this is for setting password,
			 * encrypt the password before setting it.
			 * NOTE evalue() is smart and will just return
			 * the value passed if it is already encrypted.
			 */
			if (NS_LDAP_BINDPASSWD_P == type) {
				char *tp;

				tp = evalue((char *)data);
				ptr = set_default_value(ptr, name, tp);
				free(tp);
			} else {
				ptr = set_default_value(ptr, name,
								(char *) data);
				if (NS_LDAP_CACHETTL_P == type) {
					/*
					 * when setting the cachettl value
					 * we should automatically set the
					 * cache expiration time.
					 */
					time_t t;
					char tbuf[20];

					memset(tbuf, 0, sizeof (tbuf));
					t = conv_time((char *)
						*ptr->configParamList[type]);
					if (t == 0)
						ptr = set_default_value(ptr,
						defconfig[NS_LDAP_EXP_P].name,
						(char *) "0");
					else {
						t += time(NULL);
						ptr = set_default_value(ptr,
						defconfig[NS_LDAP_EXP_P].name,
						(char *) lltostr(t, &tbuf[19]));
					}
				}
			}
			goto exitpoint;
		}
	} else {
		sprintf(errstr,
		    gettext("Unable to set value.  Initial configuration "
		    "load has not been done (__ns_ldap_LoadConfiguration())."));
		mutex_unlock(&ns_parse_lock);
		MKERROR(*error, NS_CONFIG_NOTLOADED, strdup(errstr), NULL);
		return (NS_LDAP_CONFIG);
	}

exitpoint:
	mutex_unlock(&ns_parse_lock);
	return (NS_LDAP_SUCCESS);
}


static void **
make_duplicate(ParamIndexType Param, ConfigParamValType data)
{
	int	count, i;
	void	**tmp, **dupdata;
	char	*charptr;
	int	*intptr;
	int	*intptr2;

	if (data == NULL)
		return (NULL);

	for (count = 0, tmp = data; tmp[count] != NULL; count++);

	dupdata = (void **)calloc(count + 1, sizeof (void *));
	dupdata[count] = NULL;

	for (i = 0; i < count; i++) {
		switch (Param) {
			case	NS_LDAP_AUTH_P:
			case	NS_LDAP_TRANSPORT_SEC_P:
			case	NS_LDAP_SEARCH_SCOPE_P:
			case	NS_LDAP_PREF_ONLY_P:
			case	NS_LDAP_SEARCH_REF_P:
				intptr = (int *) data[i];
				intptr2 = (int *)malloc((sizeof (int)) + 1);
				if (intptr2 == NULL)
					return (NULL);
				*intptr2 = *intptr;
				dupdata[i] = (void *)intptr2;
				break;
			case	NS_LDAP_FILE_VERSION_P:
			case	NS_LDAP_BINDDN_P:
			case	NS_LDAP_BINDPASSWD_P:
			case	NS_LDAP_SERVERS_P:
			case	NS_LDAP_SEARCH_BASEDN_P:
			case	NS_LDAP_DOMAIN_P:
			case	NS_LDAP_EXP_P:
			case	NS_LDAP_CACHETTL_P:
			case	NS_LDAP_SEARCH_TIME_P:
			case	NS_LDAP_CERT_PATH_P:
			case	NS_LDAP_CERT_PASS_P:
			case	NS_LDAP_SEARCH_DN_P:
			case	NS_LDAP_SERVER_PREF_P:
			case	NS_LDAP_PROFILE_P:
				charptr = (char *) data[i];
				if (charptr == NULL)
					dupdata[i] = NULL;
				else {
					dupdata[i] = (void *)
						calloc(strlen(charptr) + 1,
							sizeof (char));
					memcpy((char *)dupdata[i], charptr,
						strlen(charptr));
				}
				break;
			default :
				return (NULL);
		}
	}

	return (dupdata);
}


int
__ns_ldap_freeParam(void ***data)
{
	void	**tmp;
	int	i = 0;

	if (*data == NULL)
		return (NS_LDAP_SUCCESS);

	for (i = 0, tmp = *data; tmp[i] != NULL; i++)
		free(tmp[i]);

	free(*data);

	*data = NULL;

	return (NS_LDAP_SUCCESS);
}


int
__ns_ldap_getParam(const char *domain, const ParamIndexType Param,
			void ***data, ns_ldap_error_t **error)
{
	char		errstr[2 * MAXERROR];
	DomainParamType	*ptr;
	ns_ldap_error_t	*errorp;
	char		domainname[MAXHOSTNAMELEN];

	if (__getdomainname(domainname,
	    sizeof (domainname)) < 0) {
		sprintf(errstr, gettext("Unable to get current domain name"));
		MKERROR(*error, NS_CONFIG_NODOMAIN, strdup(errstr), NULL);
		return (NS_LDAP_CONFIG);
	}

	if (domain == NULL) {
		domain = domainname;
	} else {
		if (strcasecmp(domain, domainname) != 0) {
			sprintf(errstr,
			    gettext("No configuration information "
			    "available for specified domainname "
			    "'%s'."), domain);
			MKERROR(*error, NS_CONFIG_NODOMAIN,
				strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
	}

	if (config_loaded != 1 || timetorefresh() == 1) {
		if ((errorp = __ns_ldap_LoadConfiguration((char *) domain))
		    != NULL) {
			/* reload of new configuration failed */
			sprintf(errstr, gettext("Unable to load new "
				"information from configuration file "
				"'%s' ('%s')."),
				NSCONFIGFILE, errorp->message);
			syslog(LOG_ERR, errstr);
			__ns_ldap_freeError(&errorp);
			if (!config) {
				MKERROR(*error, NS_CONFIG_NOTLOADED,
					strdup(errstr), NULL);
				return (NS_LDAP_CONFIG);
			}
		}
	}

	mutex_lock(&ns_parse_lock);
	if (config_loaded == 1) {
		if ((ptr = get_domainptr((char *)domain)) == NULL) {
			sprintf(errstr,
			    gettext("No configuration information "
			    "available for specified domainname "
			    "'%s'."), domain);
			mutex_unlock(&ns_parse_lock);
			MKERROR(*error, NS_CONFIG_NODOMAIN,
				strdup(errstr), NULL);
			return (NS_LDAP_CONFIG);
		}
	} else {
		sprintf(errstr,
		    gettext("No configuration information available."));
		mutex_unlock(&ns_parse_lock);
		MKERROR(*error, NS_CONFIG_NOTLOADED, strdup(errstr), NULL);
		return (NS_LDAP_CONFIG);
	}

exitpoint:
	*error = NULL;
	mutex_unlock(&ns_parse_lock);
	*data = make_duplicate(Param, ptr->configParamList[Param]);

	return (NS_LDAP_SUCCESS);
}


static int
__door_getldapconfig(char *domainname, char *buffer,
	int buflen, ns_ldap_error_t **error)
{
	union {
		ldap_data_t	s_d;
		char		s_b[DOORBUFFERSIZE];
	} space;
	ldap_data_t	*sptr;
	int		ndata;
	int		adata;
	char		errstr[MAXERROR];

	if ((strlen(domainname) >= (sizeof (space) - sizeof (ldap_data_t)))) {
		return (NS_LDAP_OP_FAILED);
	}

	memset(buffer, 0, buflen);
	memset(space.s_b, 0, DOORBUFFERSIZE);

	adata = (sizeof (ldap_call_t) + strlen(domainname) +1);
	ndata = sizeof (space);
	space.s_d.ldap_call.ldap_callnumber = GETLDAPCONFIG;
	strcpy(space.s_d.ldap_call.ldap_u.domainname, domainname);
	sptr = &space.s_d;

	switch (__ns_ldap_trydoorcall(&sptr, &ndata, &adata)) {
		case SUCCESS:
			break;
		case NOTFOUND:
			sprintf(errstr, gettext("Door call did "
				"not succeed - error: %d."),
				space.s_d.ldap_ret.ldap_errno);
			MKERROR(*error, NS_CONFIG_DOORERROR,
				strdup(errstr), NULL);
			return (NS_LDAP_OP_FAILED);
		default:
			return (NS_LDAP_OP_FAILED);
	}

	/* copy info from door call to buffer here */
	if (strlen(space.s_d.ldap_ret.ldap_u.config) > buflen) {
		sprintf(errstr, gettext("Not enough space "
			"allocated on door call."));
		MKERROR(*error, NS_CONFIG_DOORERROR,
			strdup(errstr), NULL);
		return (NS_LDAP_OP_FAILED);
	}
	strcpy(buffer, space.s_d.ldap_ret.ldap_u.config);

	if (sptr != &space.s_d) {
		munmap((char *)sptr, ndata); /* return memory */
	}
	*error = NULL;

	return (NS_LDAP_SUCCESS);
}


ns_ldap_error_t *
__ns_ldap_LoadConfiguration(char * domainname)
{
	ns_ldap_error_t	*error = NULL;
	char		buffer[DOORBUFFERSIZE];
	int		buflen = DOORBUFFERSIZE;
	char		domain[MAXHOSTNAMELEN];


	if (domainname == NULL) {
		if (__getdomainname(domain, sizeof (domain)) >= 0)
			domainname = domain;
	}
	if (cache_server == FALSE) {
		mutex_lock(&ns_parse_lock);
		if (__door_getldapconfig(domainname, buffer, buflen,
			&error) != NS_LDAP_SUCCESS) {
		/* try to get configuration from door */
			if (error != NULL && error->message != NULL) {
				syslog(LOG_WARNING, error->message);
				__ns_ldap_freeError(&error);
			}
		} else {
		/* now convert from door format */
			if ((error = __s_api_SetDoorInfo(domainname,
					buffer)) != NULL) {
				syslog(LOG_WARNING, error->message);
				__ns_ldap_freeError(&error);
			} else {
				/* Success */
				config_loaded = 1;
				mutex_unlock(&ns_parse_lock);
				return (NULL);
			}
		}
		mutex_unlock(&ns_parse_lock);
	}

	/* otherwise, load from file as usual */
	if ((error = __s_api_LoadConfig(domainname)) != NULL) {
		return (error);
	}

	return (NULL);
}


static ns_ldap_error_t *
__s_api_LoadConfig(char * domainname)
{
	DomainParamType	*ptr = NULL;
	ParamIndexType	i = 0;
	char		errstr[MAXERROR], errbuf[MAXERROR];
	ns_ldap_error_t	*error;
	char		buffer[BUFSIZE], *name, *value;
	int		buflen = BUFSIZE, lineno, emptyfile;
	DomainParamType	*oldconfig = NULL;
	char		domain[MAXHOSTNAMELEN];
	int		always = ALWAYS, fd;

	mutex_lock(&ns_parse_lock);
	if (domainname == NULL) {
		if (__getdomainname(domain, sizeof (domain)) >= 0)
			domainname = domain;
	}
	if (config_loaded == 1) {
		oldconfig = config;
		config = NULL;
		config_loaded = 0;
	}
	if (-1 == (fd = open(NSCONFIGFILE, O_RDONLY))) {
		sprintf(errstr,
		    gettext("Unable to open filename '%s' "
		    "for reading (errno=%d)."),
		    NSCONFIGFILE, errno);
		if (oldconfig)
			/* restore old configuration */
			config = oldconfig;
		mutex_unlock(&ns_parse_lock);
		MKERROR(error, NS_CONFIG_FILE, strdup(errstr), NULL);
		return (error);
	}
	fd = __ns_ldap_raise_fd(fd);
	emptyfile = 1;
	lineno = 0;
	do {
		int	linelen;

		if ((linelen = read_line(fd, buffer,
		    buflen, errstr)) < 0)
			/* End of file */
			break;
		/* get rid of comment lines */
		if (buflen > 2 &&
		    (buffer[0] == '#' && buffer[1] != '#'))
			continue;
		emptyfile = 0;
		lineno++;
		if (linelen != 0) {
			split_key_value(buffer, buflen, &name, &value);
			if (verify_value(name, value, errbuf) !=
			    NS_SUCCESS) {
				sprintf(errstr,
				    gettext("%s (at or near line "
				    "%d).\n"), errbuf, lineno);
				MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr),
					NULL);
				if (oldconfig)
					/* restore old configuration */
					config = oldconfig;
				close(fd);
				mutex_unlock(&ns_parse_lock);
				return (error);
			}
			if (ptr == NULL) {
				ptr = load_def_cfg(CLIENTCONFIG);
				ptr->domainName = strdup(domainname);
				if (ptr->domainName == NULL)
					return (NULL);
			}
			if (get_type(name, &i) != 0) {
				sprintf(errstr,
				    gettext("Unknown type on get_type call"));
				MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr),
					NULL);
				if (oldconfig)
					/* restore old configuration */
					config = oldconfig;
				close(fd);
				mutex_unlock(&ns_parse_lock);
				return (error);
			}
			switch (configtype(i)) {
				case SERVERCONFIG:
				case CLIENTCONFIG:
						ptr = set_default_value(
							ptr, name, value);
						break;
				case CREDCONFIG:
						break;
			}
		}
	} while (always);
	close(fd);

	if (ptr != NULL) {
		if (readcfile(ptr, errstr) != NS_SUCCESS) {
			destroy_config(config);
			config = oldconfig;
			mutex_unlock(&ns_parse_lock);
			MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (error);
		}
		if (crosscheck(ptr, errstr) != NS_SUCCESS) {
			destroy_config(config);
			config = oldconfig;
			mutex_unlock(&ns_parse_lock);
			MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (error);
		}
		insert_domain(ptr);
	}

	if (emptyfile == 1) {
		config = oldconfig;
		mutex_unlock(&ns_parse_lock);
		sprintf(errstr,
		    gettext("Configuration file %s is empty "
		    "or contains no valid key/value pairs."),
		    NSCONFIGFILE);
		MKERROR(error, NS_CONFIG_NOTLOADED, strdup(errstr), NULL);
		return (error);
	}
	config_loaded = 1;
	mutex_unlock(&ns_parse_lock);
	destroy_config(oldconfig);
	return (NULL);
}

static ns_ldap_error_t *
__s_api_SetDoorInfo(char *domainname, char *configptr)
{
	DomainParamType	*ptr = NULL;
	char		errstr[MAXERROR], errbuf[MAXERROR];
	ns_ldap_error_t	*error;
	char		buffer[BUFSIZE], *name, *value, valbuf[BUFSIZE];
	char		*strptr;
	int		strsize = BUFSIZE, lineno = 0;
	DomainParamType	*oldconfig = NULL;
	char		*rest;
	char		*bufptr = buffer;
	DomainParamType *set_default_value(DomainParamType *configptr,
					char *name, char *value);
	int		always = ALWAYS;

	if (config_loaded == 1) {
		oldconfig = config;
		config = NULL;
		config_loaded = 0;
	}

	strcpy(buffer, configptr);
	strptr = (char *)strtok_r(bufptr, DOORLINESEP, &rest);
	do {
		if (strptr == NULL)
			break;
		strcpy(valbuf, strptr);
		split_key_value(valbuf, strsize, &name, &value);
		if (verify_value(name, value, errbuf) != NS_SUCCESS) {
			sprintf(errstr, gettext("%s (at or near line "
				"%d).\n"), errbuf, lineno);
			MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			if (oldconfig)
				/* restore old configuration */
				config = oldconfig;
			return (error);
		}
		if (ptr == NULL) {
			ptr = load_def_cfg(CLIENTCONFIG);
			ptr->domainName = strdup(domainname);
			if (ptr->domainName == NULL)
				return (NULL);
		}
		ptr = set_default_value(ptr, name, value);
		strptr = (char *) strtok_r(NULL, DOORLINESEP, &rest);
	} while (always);

	if (ptr != NULL) {
		if (crosscheck(ptr, errstr) != NS_SUCCESS) {
			destroy_config(config);
			config = oldconfig;
			MKERROR(error, NS_CONFIG_SYNTAX, strdup(errstr), NULL);
			return (error);
		}
		insert_domain(ptr);
	}

	config_loaded = 1;
	destroy_config(oldconfig);
	return (NULL);
}

/* profile names do not match "real" names */
static int
__ns_ldap_profile_index(char *value, ParamIndexType *index)
{
	ParamIndexType	i;

	for (i = 0; i <= LAST_VALUE; i++) {
		if ((defconfig[i].profile_name == NULL) ||
			(*(defconfig[i].profile_name) == NULL))
			continue;
		if (strcasecmp(defconfig[i].profile_name, value) == 0) {
			*index = i;
			return (0);
		}
	}

	return (-1);
}

static DomainParamType *
__ns_ldap_make_config(ns_ldap_result_t *result)
{
	int		i, l, m;
	char		val[BUFSIZ];
	char    	*cp;
	ns_ldap_entry_t	*entry;
	ParamIndexType	index;
	DomainParamType	*ptr;
	char		*rest;

	if (result == NULL) {
		return (NULL);
	}
	ptr = create_config();
	entry = result->entry;
	for (i = 0; i < result->entries_count; i++) {
		for (l = 0; l < entry->attr_count; l++) {
			cp = entry->attr_pair[l]->attrname;
			if (NULL == cp) {
				continue;
			}
			if (__ns_ldap_profile_index(cp, &index) != 0)
				continue;
			if (NULL != entry->attr_pair[l]->attrvalue[0]) {
				val[0] = NULL;
				if ((0 < entry->attr_pair[l]->value_count) &&
				    (NULL !=  entry->attr_pair[l]->attrvalue))
					strcpy(val,
					    entry->attr_pair[l]->attrvalue[0]);
				m = 1;
				while (m < entry->attr_pair[l]->value_count) {
					strcat(val, ",");
					strcat(val,
					    entry->attr_pair[l]->attrvalue[m]);
					m++;
				}
				if (NS_LDAP_SERVERS_P == index) {
					char *strptr, *temp;

					temp = strdup(val);
					temp[0] = NULL;
					strptr = strtok_r(val, ", \t", &rest);
					while (NULL != strptr) {
					    strcat(temp, strptr);
					    strptr = strtok_r(NULL, ", \t",
							&rest);
					    if (NULL == strptr)
						break;
					    strcat(temp, ",");

					}
					strcpy(val, temp);
				}
				ptr = set_default_value(ptr,
					defconfig[index].name, val);
			}
		}
		entry = entry->next;
	}
	return (ptr);
}

int
__ns_ldap_dump_profile(DomainParamType *ptr)
{
	int rc;
	ns_ldap_error_t *errorp;
	struct stat buf;

	mutex_lock(&ns_parse_lock);
	config = ptr;
	config_loaded = 1;
	mutex_unlock(&ns_parse_lock);
	/*
	 * we assume the caller of __ns_ldap_download() has saved away
	 * a copy of NSCONFIGFILE & NSCREDFILE before calling us (as only
	 * ldapclient currently calls us).  This is probably a BAD
	 * assumption.
	 *
	 * Check if /var/ldap exists, if not mkdir() it mode 0755
	 * if it exists but it is NOT a directory exit with -2.
	 */
	rc = stat("/var/ldap", &buf);
	if (0 != rc)
		mkdir("/var/ldap", 0755);
	if ((0 == rc) && ((buf.st_mode & S_IFDIR) == 0))
		return (-2);
	if (NULL == (errorp = __ns_ldap_DumpConfiguration(NSCONFIGFILE))) {
		errorp = __ns_ldap_DumpConfiguration(NSCREDFILE);
		if (NULL == errorp)
			return (0);
	}
	rc = errorp->status;
	__ns_ldap_freeError(&errorp);
	return (rc);
}

int
__ns_ldap_download(const char *profile, char *addr, char *baseDN)
{
	char filter[BUFSIZ], domainname[MAXHOSTNAMELEN];
	int rc;
	ns_ldap_result_t *result;
	DomainParamType	*ptr = NULL;
	ns_ldap_error_t *errorp;

	__getdomainname(domainname, sizeof (domainname));

	/* first we should verify a valid address, but lets NOT right now */

	mutex_lock(&ns_parse_lock);
	ptr = load_def_cfg(CLIENTCONFIG);
	if (NULL == ptr)
		return (1);
	if (NULL == baseDN)
		return (1);
	ptr = set_default_value(ptr, "NS_LDAP_SEARCH_BASEDN", baseDN);
	if (NULL == ptr)
		return (1);
	ptr = set_default_value(ptr, "NS_LDAP_SERVERS", addr);
	if (NULL == ptr)
		return (1);

	ptr->domainName = strdup(domainname);
	config = ptr;
	config_loaded = 1;
	mutex_unlock(&ns_parse_lock);

	sprintf(filter, _PROFILE_FILTER, _PROFILE_OBJECTCLASS, profile);
	rc = __ns_ldap_list(_PROFILE_CONTAINER, (const char *)filter,
		NULL, NULL, NULL, 0, &result, &errorp, NULL, NULL);
	if (rc != NS_LDAP_SUCCESS) {
		if (errorp)
			__ns_ldap_freeError(&errorp);
		return (rc);
	}

	ptr = __ns_ldap_make_config(result);
	__ns_ldap_freeResult(&result);
	rc = __ns_ldap_dump_profile(ptr);
	return (rc);
}

/*
 * Yes the use of stdio is okay here because all we are doing is sending
 * output to stdout.  This would not be necessary if we could get to the
 * configuration pointer outside this file.
 */
ns_ldap_error_t *
__ns_ldap_print_config(int verbose)
{
	ns_ldap_error_t *errorp;
	ParamIndexType p;

	if (NULL == config) {
		if (NULL != (errorp = __ns_ldap_LoadConfiguration(NULL)))
			return (errorp);
	}

	if (verbose && (NULL != config->domainName)) {
		fputs("config->domainName ", stdout);
		fputs(config->domainName, stdout);
		putchar('\n');
	}
	for (p = NS_LDAP_FILE_VERSION_P; p <= NS_LDAP_PROFILE_P; p++) {
		if (NULL != config->configParamList[p]) {
		    int i, *intptr;
		    time_t exp_time;
		    char *cp;

		    if (p == NS_LDAP_CACHETTL_P)
			/*
			 * don't print default cache TTL for now since
			 * we don't store it in the ldap_client_file.
			 */
			continue;
		    if (verbose)
			putchar('\t');
		    fputs(defconfig[p].name, stdout);
		    fputs(" = ", stdout);
		    for (i = 0; NULL != config->configParamList[p][i]; i++) {
			if (i > 0) fputs(", ", stdout);
			switch (p) {
				case NS_LDAP_PROFILE_P:
				case NS_LDAP_CERT_PATH_P:
				case NS_LDAP_CERT_PASS_P:
				case NS_LDAP_DOMAIN_P:
				case NS_LDAP_BINDDN_P:
				case NS_LDAP_BINDPASSWD_P:
				case NS_LDAP_SEARCH_BASEDN_P:
				case NS_LDAP_FILE_VERSION_P:
				case NS_LDAP_SERVERS_P:
				case NS_LDAP_SERVER_PREF_P:
				case NS_LDAP_SEARCH_DN_P:
				case NS_LDAP_SEARCH_TIME_P:
					fputs((char *)
						config->configParamList[p][i],
									stdout);
					break;
				case NS_LDAP_EXP_P:
					exp_time = (time_t)atol((char *)
						config->configParamList[p][i]);
					if (exp_time == 0)
						fputs("0", stdout);
					else {
						cp = ctime(&exp_time);
						cp[24] = '\0';
						fputs(cp, stdout);
					}
					break;
				case NS_LDAP_SEARCH_REF_P:
					intptr = (int *)
						config->configParamList[p][i];
					switch (*intptr) {
					    case NS_LDAP_FOLLOWREF:
						fputs("NS_LDAP_FOLLOWREF",
								stdout);
						break;
					    case NS_LDAP_NOREF:
						fputs("NS_LDAP_NOREF",
								stdout);
						break;
					}
					break;
				case NS_LDAP_TRANSPORT_SEC_P:
					intptr = (int *)
						config->configParamList[p][i];
					switch (*intptr) {
					    case NS_LDAP_SEC_NONE:
						fputs("NS_LDAP_SEC_NONE",
								stdout);
						break;
					    case NS_LDAP_SEC_TLS:
						fputs("NS_LDAP_SEC_TLS",
								stdout);
						break;
					    case NS_LDAP_SEC_SASL_INTEGRITY:
						fputs(
						"NS_LDAP_SEC_SASL_INTEGRITY",
								stdout);
						break;
					    case NS_LDAP_SEC_SASL_PRIVACY:
						fputs(
						"NS_LDAP_SEC_SASL_PRIVACY",
								stdout);
						break;
					}
					break;
				case NS_LDAP_AUTH_P:
					intptr = (int *)
						config->configParamList[p][i];
					switch (*intptr) {
					    case NS_LDAP_AUTH_NONE:
						fputs("NS_LDAP_AUTH_NONE",
								stdout);
						break;
					    case NS_LDAP_AUTH_SIMPLE:
						fputs("NS_LDAP_AUTH_SIMPLE",
								stdout);
						break;
					    case NS_LDAP_AUTH_SASL_CRAM_MD5:
						fputs(
						"NS_LDAP_AUTH_SASL_CRAM_MD5",
								stdout);
						break;
					}
					break;
				case NS_LDAP_SEARCH_SCOPE_P:
					intptr = (int *)
						config->configParamList[p][i];
					switch (*intptr) {
					    case NS_LDAP_SCOPE_BASE:
						fputs("NS_LDAP_SCOPE_BASE",
								stdout);
						break;
					    case NS_LDAP_SCOPE_ONELEVEL:
						fputs("NS_LDAP_SCOPE_ONELEVEL",
								stdout);
						break;
					    case NS_LDAP_SCOPE_SUBTREE:
						fputs("NS_LDAP_SCOPE_SUBTREE",
								stdout);
						break;
					}
					break;
				case NS_LDAP_PREF_ONLY_P:
					intptr = (int *)
						config->configParamList[p][i];
					switch (*intptr) {
					    case NS_LDAP_PREF_TRUE:
						fputs("NS_LDAP_PREF_TRUE",
								stdout);
						break;
					    case NS_LDAP_PREF_FALSE:
						fputs("NS_LDAP_PREF_FALSE",
								stdout);
						break;
					}
					break;
				case NS_LDAP_CACHETTL_P:
					/* No action for now */
					break;
				default:
					fputs(
					"Unexpected configParamIndexType\n",
									stdout);
					break;
			}
		    }
		    putchar('\n');
		}
	}
	return (NULL);
}


/*
 * converts the time string into seconds.  The time string can be specified
 * using one of the following time units:
 * 	#s (# of seconds)
 *	#m (# of minutes)
 *	#h (# of hours)
 *	#d (# of days)
 *	#w (# of weeks)
 * NOTE: you can only specify one the above.  No combination of the above
 * units is allowed.  If no unit specified, it will default to "seconds".
 */
time_t
conv_time(char *s)
{
	time_t t;
	char c;
	int l, m;
	long tot;

	l = strlen(s);
	if (0 == l)
		return (0);
	c = s[--l];
	m = 0;
	switch (c) {
		case 'w': /* weeks */
			m = 604800;
			break;
		case 'd': /* days */
			m = 86400;
			break;
		case 'h': /* hours */
			m = 3600;
			break;
		case 'm': /* minutes */
			m = 60;
			break;
		case 's': /* seconds */
			m = 1;
			break;
		/* the default case is set to "second" */
	}
	if (0 != m)
		s[l] = '\0';
	else
		m = 1;
	errno = 0;
	tot = atol(s);
	if ((0 == tot) && (EINVAL == errno))
		return (0);
	if (((LONG_MAX == tot) || (LONG_MIN == tot)) && (EINVAL == errno))
		return (0);

	tot = tot * m;
	t = (time_t)tot;
	return (t);
}
