/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getipnodeby_door.c	1.1	99/03/21 SMI"

#include <pwd.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/door.h>
#include <errno.h>
#include <fcntl.h>
#include <synch.h>
#include <getxby_door.h>
#include <nss_dbdefs.h>

#ifdef PIC

static struct hostent *
__process_getipnode(struct hostent *, char *, int, int *, nsc_data_t *);

extern struct hostent *
_switch_getipnodebyname_r(const char *, struct hostent *, char *, int, int *);

extern struct hostent *
_switch_getipnodebyaddr_r(const char *, int, int, struct hostent *, char *,
	int, int *);

struct hostent *
_door_getipnodebyname_r(const char *name, struct hostent *result, char *buffer,
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
	space.s_d.nsc_call.nsc_callnumber = GETIPNODEBYNAME;
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
		return ((struct hostent *)_switch_getipnodebyname_r(name,
		    result, buffer, buflen, h_errnop));
	}
	resptr = __process_getipnode(result, buffer, buflen, h_errnop, sptr);

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
_door_getipnodebyaddr_r(const char *addr, int length, int type,
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

	space.s_d.nsc_call.nsc_callnumber = GETIPNODEBYADDR;
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
		return ((struct hostent *)_switch_getipnodebyaddr_r(addr,
		    length, type, result, buffer, buflen, h_errnop));
	}

	resptr = __process_getipnode(result, buffer, buflen, h_errnop, sptr);

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
__process_getipnode(struct hostent *result, char *buffer, int buflen,
	int *h_errnop, nsc_data_t *sptr)
{
	int i;

	char *fixed;

	fixed = (char *) ROUND_UP((int)buffer, sizeof(char *));
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

static struct hostent *
__process_getipnode(struct hostent *result, char *buffer, int buflen,
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

	fixed = (char *) ROUND_UP(buffer, sizeof (char *));
	buflen -= fixed - buffer;
	buffer = fixed;

	/*
	 * find out whether the user has provided sufficient space
	 */
	start = sptr->nsc_ret.nsc_u.buff + sizeof (struct hostent32);
	/*
	 * Length of hostname + null
	 */
	strs = 1 + strlen(sptr->nsc_ret.nsc_u.hst.h_name + start);
	/*
	 * length of all aliases + null
	 */
	alias = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_aliases);
	for (numaliases = 0; alias[numaliases]; numaliases++)
	    strs += 1 + strlen(start + alias[numaliases]);
	/*
	 * Realign on word boundary
	 */
	strs = ROUND_UP(strs, sizeof (char *));
	/*
	 * Count the array of pointers to all aliases + null pointer
	 */
	strs += sizeof (char *) * (numaliases + 1);
	/*
	 * length of all addresses + null. Also, account for word alignment.
	 */
	address = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_addr_list);
	for (numaddrs = 0; address[numaddrs]; numaddrs++) {
		strs += sptr->nsc_ret.nsc_u.hst.h_length;
		strs = ROUND_UP(strs, sizeof (char *));
	}
	/*
	 * Count the array of pointers to all addresses + null pointer
	 */
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
	/*
	 * fill out h_name
	 */
	start = sptr->nsc_ret.nsc_u.buff + sizeof (struct hostent32);
	strcpy(dest, sptr->nsc_ret.nsc_u.hst.h_name + start);
	strs = 1 + strlen(sptr->nsc_ret.nsc_u.hst.h_name + start);
	result->h_name = dest;
	dest += strs;
	/*
	 * fill out the h_aliases list
	 */
	alias = (int *)(start + sptr->nsc_ret.nsc_u.hst.h_aliases);
	for (i = 0; i < numaliases; i++) {
		strcpy(dest, start + alias[i]);
		aliaseslist[i] = dest;
		dest += 1 + strlen(start + alias[i]);
	}
	aliaseslist[i] = 0;	/* null term ptr chain */
	result->h_aliases = aliaseslist;

	/*
	 * fill out the h_addr list
	 */
	dest = (char *)ROUND_UP(dest, sizeof (char *));
	address = (int *) (start + sptr->nsc_ret.nsc_u.hst.h_addr_list);
	for (i = 0; i < numaddrs; i++) {
		memcpy(dest, start + address[i],
		    sptr->nsc_ret.nsc_u.hst.h_length);
		addrlist[i] = dest;
		dest += sptr->nsc_ret.nsc_u.hst.h_length;
		dest = (char *)ROUND_UP(dest, sizeof (char *));
	}

	addrlist[i] = 0;	/* null term ptr chain */

	result->h_addr_list = addrlist;

	result->h_length = sptr->nsc_ret.nsc_u.hst.h_length;
	result->h_addrtype = sptr->nsc_ret.nsc_u.hst.h_addrtype;

	return (result);
}
#endif /* _LP64 */

#endif PIC
