
/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gethostby_door.c	1.11	97/08/01 SMI"

#include <pwd.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/door.h>
#include <errno.h>
#include <fcntl.h>
#include <synch.h>
#include <getxby_door.h>

#ifdef PIC

extern struct hostent *
_switch_gethostbyname_r(const char *, struct hostent *, char *, int , int *);

extern struct hostent *
_switch_gethostbyaddr_r(const char *, int ,int ,struct hostent *, char *,
	int, int *);

static struct hostent *
process_gethost(struct hostent *result, char *buffer, int buflen,
	int *h_errnop, nsc_data_t *sptr);

struct hostent *
_door_gethostbyname_r(const char *name, struct hostent *result, char *buffer,
	int buflen, int *h_errnop)
{

	/*
	 * allocate space on the stack for the nscd to return
	 * host and host alias information
	 */
	union {
		nsc_data_t 	s_d;
		char		s_b[8192];
	} space;
	nsc_data_t	*sptr;
	int		ndata;
	int		adata;
	struct	hostent *resptr = NULL;

	if ((name == (const char *)NULL) ||
	    (strlen(name) >= (sizeof (space) - sizeof (nsc_data_t)))) {
		errno = ERANGE;
		return (NULL);
	}

	adata = (sizeof (nsc_call_t) + strlen(name) + 1);
	ndata = sizeof (space);
	space.s_d.nsc_call.nsc_callnumber = GETHOSTBYNAME;
	strcpy(space.s_d.nsc_call.nsc_u.name, name);
	sptr = &space.s_d;

	switch (_nsc_trydoorcall(&sptr, &ndata, &adata)) {
	    case SUCCESS:	/* positive cache hit */
		break;
	    case NOTFOUND:	/* negative cache hit */
		if (h_errnop)
		    *h_errnop = space.s_d.nsc_ret.nsc_errno;
		return (NULL);
	    default:
		return ((struct hostent *)_switch_gethostbyname_r(name,
		    result, buffer, buflen, h_errnop));
	}
	resptr = process_gethost(result, buffer, buflen, h_errnop, sptr);

	/*
	 * check if doors realloced buffer underneath of us....
	 * munmap or suffer a memory leak
	 */

	if (sptr != &space.s_d) {
		munmap((char *)sptr, ndata); /* return memory */
	}

	return (resptr);
}

struct hostent *
_door_gethostbyaddr_r(const char *addr, int length, int type,
	struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	/*
	 * allocate space on the stack for the nscd to return
	 * host and host alias information
	 */
	union {
		nsc_data_t 	s_d;
		char		s_b[8192];
	} space;
	nsc_data_t 	*sptr;
	int		ndata;
	int		adata;
	struct	hostent *resptr = NULL;

	if (addr == NULL)
		return (NULL);

	ndata = sizeof (space);
	adata = length + sizeof (nsc_call_t) + 1;
	sptr = &space.s_d;

	space.s_d.nsc_call.nsc_callnumber = GETHOSTBYADDR;
	space.s_d.nsc_call.nsc_u.addr.a_type = type;
	space.s_d.nsc_call.nsc_u.addr.a_length = length;
	memcpy(space.s_d.nsc_call.nsc_u.addr.a_data, addr, length);

	switch (_nsc_trydoorcall(&sptr, &ndata, &adata)) {
	    case SUCCESS:	/* positive cache hit */
		break;
	    case NOTFOUND:	/* negative cache hit */
		if (h_errnop)
		    *h_errnop = space.s_d.nsc_ret.nsc_errno;
		return (NULL);
	    default:
		return ((struct hostent *)_switch_gethostbyaddr_r(addr,
		    length, type, result, buffer, buflen, h_errnop));
	}

	resptr = process_gethost(result, buffer, buflen, h_errnop, sptr);

	/*
	 * check if doors realloced buffer underneath of us....
	 * munmap it or suffer a memory leak
	 */

	if (sptr != &space.s_d) {
		munmap((char *)sptr, ndata); /* return memory */
	}

	return (resptr);

}

#if !defined(_LP64)

static struct hostent *
process_gethost(struct hostent *result, char *buffer, int buflen,
	int *h_errnop, nsc_data_t *sptr)
{
	int i;

	char *fixed;

	fixed = (char *) (((int)buffer +3) & ~3);
	buflen -= fixed - buffer;
	buffer = fixed;

	if (buflen + sizeof (struct hostent)
	    < sptr->nsc_ret.nsc_bufferbytesused) {
		/*
		 * no enough space allocated by user
		 */
		errno = ERANGE;
		return (NULL);
	}

	memcpy(buffer, sptr->nsc_ret.nsc_u.buff + sizeof (struct hostent),
	    sptr->nsc_ret.nsc_bufferbytesused - sizeof (struct hostent));

	sptr->nsc_ret.nsc_u.hst.h_name += (int) buffer;
	sptr->nsc_ret.nsc_u.hst.h_aliases =
	    (char **) ((char *)sptr->nsc_ret.nsc_u.hst.h_aliases + (int)buffer);
	sptr->nsc_ret.nsc_u.hst.h_addr_list =
	    (char **) ((char *)sptr->nsc_ret.nsc_u.hst.h_addr_list +
	    (int)buffer);
	for (i = 0; sptr->nsc_ret.nsc_u.hst.h_aliases[i]; i++) {
		sptr->nsc_ret.nsc_u.hst.h_aliases[i] += (int) buffer;
	}
	for (i = 0; sptr->nsc_ret.nsc_u.hst.h_addr_list[i]; i++) {
		sptr->nsc_ret.nsc_u.hst.h_addr_list[i] += (int) buffer;
	}

	*result = sptr->nsc_ret.nsc_u.hst;

	return (result);
}

#else /* _LP64 */

#define	RNDUP(buf, n) (((uintptr_t)buf + n - 1l) & ~(n - 1l))

static struct hostent *
process_gethost(struct hostent *result, char *buffer, int buflen,
	int *h_errnop, nsc_data_t *sptr)
{
	char *fixed;
	char *dest;
	char *start;
	char **aliaseslist;
	char **addrlist;
	int *alias;
	int *address;
	size_t strs;
	int numaliases;
	int numaddrs;
	int i;

	fixed = (char *) RNDUP(buffer, sizeof (char *));
	buflen -= fixed - buffer;
	buffer = fixed;

	/* find out whether the user has provided sufficient space */

	start = sptr->nsc_ret.nsc_u.buff + sizeof (struct hostent32);
	strs = 1 + strlen(sptr->nsc_ret.nsc_u.hst.h_name + start);
	alias = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_aliases);
	for (numaliases = 0; alias[numaliases]; numaliases++)
	    strs += 1 + strlen(start + alias[numaliases]);
	strs = RNDUP(strs, sizeof (int));
	strs += sizeof (char *) * (numaliases + 1);
	address = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_addr_list);
	for (numaddrs = 0; address[numaddrs]; numaddrs++)
	    strs += RNDUP(sptr->nsc_ret.nsc_u.hst.h_length, sizeof (int));
	strs += sizeof (char *) * (numaddrs + 1);

	if (buflen < strs) {

		/* no enough space allocated by user */

		errno = ERANGE;
		return (NULL);
	}


	/*
	 * allocat the h_aliases list and the h_addr_list first to align 'em.
	 */

	dest = buffer;
	aliaseslist = (char **)dest;
	dest += sizeof (char *) * (numaliases + 1);
	addrlist = (char **)dest;
	dest += sizeof (char *) * (numaddrs + 1);

	/* fill out h_name */

	start = sptr->nsc_ret.nsc_u.buff + sizeof (struct hostent32);
	strcpy(dest, sptr->nsc_ret.nsc_u.hst.h_name + start);
	strs = 1 + strlen(sptr->nsc_ret.nsc_u.hst.h_name + start);
	result->h_name = dest;
	dest += strs;

	/*
	 * fill out the h_aliases list
	 */
	for (i = 0; i < numaliases; i++) {
		alias = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_aliases);
		strcpy(dest, start + alias[i]);
		strs = 1 + strlen(start + alias[i]);
		aliaseslist[i] = dest;
		dest += strs;
	}
	aliaseslist[i] = 0;	/* null term ptr chain */

	result->h_aliases = aliaseslist;

	/*
	 * fill out the h_addr list
	 */

	dest = (char *)RNDUP(dest, sizeof (int));

	for (i = 0; i < numaddrs; i++) {
		address = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_addr_list);
		memcpy(dest, start + address[i],
		    sptr->nsc_ret.nsc_u.hst.h_length);
		strs = sptr->nsc_ret.nsc_u.hst.h_length;
		addrlist[i] = dest;
		dest += strs;
		dest = (char *)RNDUP(dest, sizeof (int));
	}

	addrlist[i] = 0;	/* null term ptr chain */

	result->h_addr_list = addrlist;

	result->h_length = sptr->nsc_ret.nsc_u.hst.h_length;
	result->h_addrtype = sptr->nsc_ret.nsc_u.hst.h_addrtype;

	return (result);
}
#endif /* _LP64 */
#endif PIC
