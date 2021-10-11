/*
 *	gethostent6.c
 *
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)gethostent6.c	1.4	99/10/29 SMI"

/*
 * This is the DNS backend for IPv6 addresses.
 * getbyname() is a local routine, but getbyaddr() actually shares the
 * same codes as the one in gethostent.c.
 */

#define	endhostent	res_endhostent

#include <malloc.h>
#include <stddef.h>
#include <string.h>
#include "dns_common.h"

/*
 * If the DNS name service switch routines are used in a binary that depends
 * on an older libresolv (libresolv.so.1, say), then having nss_dns.so.1 or
 * libnss_dns.a depend on a newer libresolv (libresolv.so.2) will cause
 * relocation problems. In particular, copy relocation of the _res structure
 * (which changes in size from libresolv.so.1 to libresolv.so.2) could
 * cause corruption, and result in a number of strange problems, including
 * core dumps. Hence, we check if a libresolv is already loaded.
 */


#pragma weak	res_endhostent

extern struct hostent *_gethostbyname(int *, const char *);
extern struct hostent *_nss_dns_gethostbyname2(int *, const char *);

typedef union {
	long al;
	char ac;
} align;


static void
_endhostent(errp)
	nss_status_t	*errp;
{
	int	ret;

	ret = endhostent();
	if (ret == 0)
		*errp = NSS_SUCCESS;
	else
		*errp = NSS_UNAVAIL;
}


#ifdef	RNDUP
#undef	RNDUP
#endif
#define	RNDUP(x)	((1 + (((x)-1)/sizeof (void *))) * sizeof (void *))

#ifdef	PTROFF
#undef	PTROFF
#endif
#define	PTROFF(p, o)	(((o) == 0) ? 0 : (void *)((char *)(p) + (o)))


/*
 * Make a copy of h->h_name.
 */
static char *
cloneName(struct hostent *h, int *outerr) {

	char	*name;
	int	len;
	int	error, *errp;

	if (outerr)
		errp = outerr;
	else
		errp = &error;

	if (h == 0 || h->h_name == 0) {
		*errp = 0;
		return (0);
	}

	len = strlen(h->h_name);

	if ((name = malloc(len+1)) == 0) {
		*errp = 1;
		return (0);
	}

	memcpy(name, h->h_name, len+1);

	*errp = 0;
	return (name);
}


/*
 * Copy the h->h_addr_list[] array to a new array, and append the
 * moreAddrs[] list. If h->h_addr_list[] contains IPv4 addresses,
 * convert them to v4 mapped IPv6 addresses.
 *
 * Note: The pointers to the addresses in the moreAddrs[] array are copied,
 *       but not the IP addresses themselves.
 */
struct in6_addr **
cloneAddrList(struct hostent *h, struct in6_addr **moreAddrs, int *outerr) {

	struct in6_addr	**addrArray, *addrList;
	int		domap, addrlen, i, j, addrCount, moreAddrCount = 0;

	int	error, *errp;

	if (outerr)
		errp = outerr;
	else
		errp = &error;

	if (h == 0 || h->h_addr_list == 0) {
		*errp = 0;
		return (0);
	}

	/* Should we map v4 to IPv6 ? */
	domap = (h->h_length == sizeof (struct in_addr)) &&
		(h->h_addrtype == AF_INET);

	/* If mapping, make sure we allocate enough memory for addresses */
	addrlen = h->h_length;
	if (domap && addrlen < sizeof (struct in6_addr))
		addrlen = sizeof (struct in6_addr);

	for (addrCount = 0; h->h_addr_list[addrCount]; addrCount++);

	if (moreAddrs != 0) {
		for (moreAddrCount = 0; moreAddrs[moreAddrCount];
			moreAddrCount++);
	}

	if ((addrArray = malloc((addrCount+moreAddrCount+1)*sizeof (addrList) +
				addrCount*addrlen)) == 0) {
		*errp = 1;
		return (0);
	}

	addrList = PTROFF(addrArray, (addrCount+moreAddrCount+1) *
					sizeof (addrList));

	for (i = 0; i < addrCount; i++) {
		addrArray[i] = addrList;
		if (domap) {
			IN6_INADDR_TO_V4MAPPED(
			(struct in_addr *)h->h_addr_list[i], addrArray[i]);
		} else {
			memcpy(addrArray[i], h->h_addr_list[i], addrlen);
		}
		addrList = PTROFF(addrList, addrlen);
	}

	for (j = 0; j < moreAddrCount; j++, i++) {
		addrArray[i] = moreAddrs[j];
	}

	/* Last pointer should be NULL */
	addrArray[i] = 0;

	*errp = 0;
	return (addrArray);
}


/*
 * Create a new alias array that is is a copy of h->h_aliases[] plus
 * the aliases in mergeAliases[] which aren't duplicates of any alias
 * in h->h_aliases[].
 *
 * Note 1: Only the string pointers (NOT the strings) in the mergeAliases[]
 *         array are copied.
 *
 * Note 2: The duplicate aliases in mergeAliases[] are replaced by NULL
 *         pointers.
 */
static char **
cloneAliasList(struct hostent *h, char **mergeAliases, int *outerr) {

	char	**aliasArray, *aliasList;
	int	i, j, k, aliasCount, mergeAliasCount = 0, realMac = 0;
	int	stringSize = 0;
	int	error, *errp;

	if (outerr)
		errp = outerr;
	else
		errp = &error;


	if (h == 0 || h->h_aliases == 0) {
		*errp = 0;
		return (0);
	}

	for (aliasCount = 0; h->h_aliases[aliasCount]; aliasCount++) {
		stringSize += RNDUP(strlen(h->h_aliases[aliasCount])+1);
	}

	if (mergeAliases != 0) {
		for (; mergeAliases[mergeAliasCount]; mergeAliasCount++) {
			int	countThis = 1;
			/* Skip duplicates */
			for (j = 0; j < aliasCount; j++) {
				if (strcmp(mergeAliases[mergeAliasCount],
						h->h_aliases[j]) == 0) {
					countThis = 0;
					break;
				}
			}
			if (countThis)
				realMac++;
			else
				mergeAliases[mergeAliasCount] = 0;
		}
	}

	if ((aliasArray = malloc((aliasCount+realMac+1)*sizeof (char **) +
				stringSize)) == 0) {
		*errp = 1;
		return (0);
	}

	aliasList = PTROFF(aliasArray,
				(aliasCount+realMac+1)*sizeof (char **));
	for (i = 0; i < aliasCount; i++) {
		int	len = strlen(h->h_aliases[i]);
		aliasArray[i] = aliasList;
		memcpy(aliasArray[i], h->h_aliases[i], len+1);
		aliasList = PTROFF(aliasList, RNDUP(len+1));
	}

	for (j = 0; j < mergeAliasCount; j++) {
		if (mergeAliases[j] != 0) {
			aliasArray[i++] = mergeAliases[j];
		}
	}

	aliasArray[i] = 0;

	*errp = 0;
	return (aliasArray);
}


static nss_status_t
getbyname(be, a)
	dns_backend_ptr_t	be;
	void			*a;
{
	struct hostent	*he;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *) a;
	int		ret, mt_disabled = 1;
	sigset_t	oldmask, newmask;
	int		gotv4 = 0, converr = 0;
	struct hostent	v4he;
	char		*v4Name = 0;
	struct in6_addr	**v4Addrs = 0, **mergeAddrs = 0;
	char		**v4Aliases = 0, **mergeAliases = 0;
	int		v4_h_errno;

	/*
	 * Try to enable MT; if not, we have to single-thread libresolv
	 * access
	 */
	if (enable_mt == 0 || (mt_disabled = (*enable_mt)()) != 0) {
		(void) sigfillset(&newmask);
		_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
		_mutex_lock(&one_lane);
	}

	/* Set no hosts fallback after the attempt at enabling MT */
	if (set_no_hosts_fallback != 0)
		(*set_no_hosts_fallback)();

	/* Get the A records, and store the information */
	he = _gethostbyname(&argp->h_errno, argp->key.name);
	if (he != 0) {
		v4Name = cloneName(he, &converr);
		if (converr) {
			argp->h_errno = HOST_NOT_FOUND;
			argp->erange = 1;
			if (mt_disabled) {
				_mutex_unlock(&one_lane);
				_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
			} else {
				(void) (*disable_mt)();
			}
			return (_herrno2nss(argp->h_errno));
		}
		v4Addrs = cloneAddrList(he, 0, &converr);
		if (converr) {
			if (v4Name != 0)
				free(v4Name);
			argp->h_errno = HOST_NOT_FOUND;
			argp->erange = 1;
			if (mt_disabled) {
				_mutex_unlock(&one_lane);
				_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
			} else {
				(void) (*disable_mt)();
			}
			return (_herrno2nss(argp->h_errno));
		}
		v4Aliases = cloneAliasList(he, 0, &converr);
		if (converr) {
			if (v4Name != 0)
				free(v4Name);
			if (v4Addrs != 0)
				free(v4Addrs);
			argp->h_errno = HOST_NOT_FOUND;
			argp->erange = 1;
			if (mt_disabled) {
				_mutex_unlock(&one_lane);
				_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
			} else {
				(void) (*disable_mt)();
			}
			return (_herrno2nss(argp->h_errno));
		}
		v4_h_errno = argp->h_errno;
		gotv4 = 1;
	}

	/* Now get the AAAA records */
	he = _nss_dns_gethostbyname2(&argp->h_errno, argp->key.name);

	/* Merge the results */
	if (he != 0) {
		if (v4Addrs != 0) {
			mergeAddrs = cloneAddrList(he, v4Addrs,	&converr);
			if (converr) {
				if (v4Name != 0)
					free(v4Name);
				if (v4Addrs != 0)
					free(v4Addrs);
				if (v4Aliases != 0)
					free(v4Aliases);
				argp->h_errno = HOST_NOT_FOUND;
				argp->erange = 1;
				if (mt_disabled) {
					_mutex_unlock(&one_lane);
					_thr_sigsetmask(SIG_SETMASK, &oldmask,
							NULL);
				} else {
					(void) (*disable_mt)();
				}
				return (_herrno2nss(argp->h_errno));
			}
			he->h_addr_list = (char **)mergeAddrs;
		}
		if (v4Aliases != 0) {
			mergeAliases = cloneAliasList(he, v4Aliases, &converr);
			if (converr) {
				if (v4Name != 0)
					free(v4Name);
				if (v4Addrs != 0)
					free(v4Addrs);
				if (v4Aliases != 0)
					free(v4Aliases);
				if (mergeAddrs != 0)
					free(mergeAddrs);
				argp->h_errno = HOST_NOT_FOUND;
				argp->erange = 1;
				if (mt_disabled) {
					_mutex_unlock(&one_lane);
					_thr_sigsetmask(SIG_SETMASK, &oldmask,
							NULL);
				} else {
					(void) (*disable_mt)();
				}
				return (_herrno2nss(argp->h_errno));
			}
			he->h_aliases = mergeAliases;
		}
	} else if (gotv4) {
		/*
		 * Reconstruct the v4 hostent structure. Remember that the IPv4
		 * addresses were converted to mapped IPv6 ones.
		 */
		v4he.h_name = v4Name;
		v4he.h_length = sizeof (*(v4Addrs[0]));
		v4he.h_addrtype = AF_INET6;
		v4he.h_addr_list = (char **)v4Addrs;
		v4he.h_aliases = v4Aliases;
		he = &v4he;
		argp->h_errno = v4_h_errno;
	}

	if (he != NULL) {
		ret = ent2result(he, a, AF_INET6);
		if (ret == NSS_STR_PARSE_SUCCESS) {
			argp->returnval = argp->buf.result;
		} else {
			argp->h_errno = HOST_NOT_FOUND;
			if (ret == NSS_STR_PARSE_ERANGE) {
				argp->erange = 1;
			}
		}
	}

	if (v4Name != 0)
		free(v4Name);
	if (v4Addrs != 0)
		free(v4Addrs);
	if (v4Aliases != 0)
		free(v4Aliases);
	if (mergeAddrs != 0)
		free(mergeAddrs);
	if (mergeAliases != 0)
		free(mergeAliases);

	if (mt_disabled) {
		_mutex_unlock(&one_lane);
		_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
	} else {
		(void) (*disable_mt)();
	}

	return (_herrno2nss(argp->h_errno));
}


extern nss_status_t __nss_dns_getbyaddr(dns_backend_ptr_t, void *);

static nss_status_t
getbyaddr(be, a)
	dns_backend_ptr_t	be;
	void			*a;
{
	/* uses the same getbyaddr from IPv4 */
	return (__nss_dns_getbyaddr(be, a));
}


/*ARGSUSED*/
static nss_status_t
_nss_dns_getent(be, args)
	dns_backend_ptr_t	be;
	void			*args;
{
	return (NSS_UNAVAIL);
}


/*ARGSUSED*/
static nss_status_t
_nss_dns_setent(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	/* XXXX not implemented at this point */
	return (NSS_UNAVAIL);
}


/*ARGSUSED*/
static nss_status_t
_nss_dns_endent(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	/* XXXX not implemented at this point */
	return (NSS_UNAVAIL);
}


/*ARGSUSED*/
static nss_status_t
_nss_dns_destr(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	nss_status_t	errp;

	if (be != 0) {
		/* === Should change to invoke ops[ENDENT] ? */
		sigset_t	oldmask, newmask;
		int		mt_disabled = 1;

		if (enable_mt == 0 || (mt_disabled = (*enable_mt)()) != 0) {
			(void) sigfillset(&newmask);
			_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
			_mutex_lock(&one_lane);
		}

		_endhostent(&errp);

		if (mt_disabled) {
			_mutex_unlock(&one_lane);
			_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
		} else {
			(void) (*disable_mt)();
		}

		free(be);
	}
	return (NSS_SUCCESS);   /* In case anyone is dumb enough to check */
}



static dns_backend_op_t ipnodes_ops[] = {
	_nss_dns_destr,
	_nss_dns_endent,
	_nss_dns_setent,
	_nss_dns_getent,
	getbyname,
	getbyaddr,
};

/*ARGSUSED*/
nss_backend_t *
_nss_dns_ipnodes_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_dns_constr(ipnodes_ops,
		sizeof (ipnodes_ops) / sizeof (ipnodes_ops[0])));
}
