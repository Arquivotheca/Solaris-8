/*
 * Copyright (c) 1999 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interface_id.c	1.2	99/10/19 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inet/common.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#define	IPIF_SEPARATOR_CHAR	":"

/*
 * Given an interface name, this function retrives the associated
 * index value. Returns index value if successful, zero otherwise.
 * The length of the supplied interface name must be at most
 * IF_NAMESIZE-1 bytes
 */
uint32_t
if_nametoindex(const char *ifname)
{
	int		s;
	struct lifreq	lifr;
	int		save_err;
	size_t		size;


	/* Make sure the given name is not NULL */
	if ((ifname == NULL)||(*ifname == '\0')) {
		errno = ENXIO;
		return (0);
	}

	/*
	 * Fill up the interface name in the ioctl
	 * request message. Make sure that the length of
	 * the given interface name <= (IF_NAMESIZE-1)
	 */
	size = strlen(ifname);
	if (size > (IF_NAMESIZE - 1)) {
		errno = EINVAL;
		return (0);
	}

	strncpy(lifr.lifr_name, ifname, size +1);

	/* Check the v4 interfaces first */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		if (ioctl(s, SIOCGLIFINDEX, (caddr_t)&lifr) >= 0) {
			(void) close(s);
			return (lifr.lifr_index);
		}
		(void) close(s);
	}

	/* Check the v6 interface list */
	s = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s < 0)
		return (0);

	if (ioctl(s, SIOCGLIFINDEX, (caddr_t)&lifr) < 0)
		lifr.lifr_index = 0;

	save_err = errno;
	(void) close(s);
	errno = save_err;
	return (lifr.lifr_index);
}

/*
 * Given an index, this function returns the associated interface
 * name in the supplied buffer ifname.
 * Returns physical interface name if successful, NULL otherwise.
 * The interface name returned will be at most IF_NAMESIZE-1 bytes.
 */
char *
if_indextoname(uint32_t ifindex, char *ifname)
{
	int		n;
	int		s;
	char		*buf;
	uint32_t	index;
	struct lifnum	lifn;
	struct lifconf	lifc;
	struct lifreq	*lifrp;
	int		numifs;
	size_t		bufsize;
	boolean_t 	found;

	/* A interface index of 0 is invalid */
	if (ifindex == 0) {
		errno = ENXIO;
		return (NULL);
	}

	s = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s < 0) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) {
			return (NULL);
		}
	}

	/* Prepare to send a SIOCGLIFNUM request message */
	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;
	if (ioctl(s, SIOCGLIFNUM, (char *)&lifn) < 0) {
		int save_err = errno;
		(void) close(s);
		errno = save_err;
		return (NULL);
	}
	numifs = lifn.lifn_count;

	/*
	 * Provide enough buffer to obtain the interface
	 * list from the kernel as response to a SIOCGLIFCONF
	 * request
	 */

	bufsize = numifs * sizeof (struct lifreq);
	buf = malloc(bufsize);
	if (buf == NULL) {
		int save_err = errno;
		(void) close(s);
		errno = save_err;
		return (NULL);
	}
	lifc.lifc_family = AF_UNSPEC;
	lifc.lifc_flags = 0;
	lifc.lifc_len = bufsize;
	lifc.lifc_buf = buf;
	if (ioctl(s, SIOCGLIFCONF, (char *)&lifc) < 0) {
		int save_err = errno;
		(void) close(s);
		errno = save_err;
		free(buf);
		return (NULL);
	}

	lifrp = lifc.lifc_req;
	found = B_FALSE;
	for (n = lifc.lifc_len / sizeof (struct lifreq); n > 0; n--, lifrp++) {
		/*
		 * Obtain the index value of each interface, and
		 * match to see if the retrived index value matches
		 * the given one. If so we have return the corresponding
		 * device name of that interface.
		 */
		size_t	size;

		index = if_nametoindex(lifrp->lifr_name);
		if (index == 0)
			/* Oops the interface just disappeared */
			continue;
		if (index == ifindex) {
			size = strcspn(lifrp->lifr_name,
			    (char *)IPIF_SEPARATOR_CHAR);
			lifrp->lifr_name[size] = '\0';
			found = B_TRUE;
			(void) strncpy(ifname, lifrp->lifr_name,
			    size + 1);
			break;
		}
	}
	(void) close(s);
	free(buf);
	if (!found) {
		errno = ENXIO;
		return (NULL);
	}
	return (ifname);
}

/*
 * This function returns all the interface names and indexes
 */
struct if_nameindex *
if_nameindex(void)
{
	int		n;
	int		s;
	boolean_t	found;
	char		*buf;
	struct lifnum	lifn;
	struct lifconf	lifc;
	struct lifreq	*lifrp;
	int		numifs;
	int		index;
	int		i;
	int 		physinterf_num;
	size_t		bufsize;
	struct if_nameindex	 *interface_list;
	struct if_nameindex	 *interface_entry;

	s = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s < 0) {
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0)
			return (NULL);
	}

	lifn.lifn_family = AF_UNSPEC;
	lifn.lifn_flags = 0;
	if (ioctl(s, SIOCGLIFNUM, (char *)&lifn) < 0)
		return (NULL);
	numifs = lifn.lifn_count;

	bufsize = numifs * sizeof (struct lifreq);
	buf = malloc(bufsize);
	if (buf == NULL) {
		int save_err = errno;
		(void) close(s);
		errno = save_err;
		return (NULL);
	}
	lifc.lifc_family = AF_UNSPEC;
	lifc.lifc_flags = 0;
	lifc.lifc_len = bufsize;
	lifc.lifc_buf = buf;
	if (ioctl(s, SIOCGLIFCONF, (char *)&lifc) < 0) {
		int save_err = errno;
		(void) close(s);
		errno = save_err;
		free(buf);
		return (NULL);
	}

	lifrp = lifc.lifc_req;
	(void) close(s);

	/* Allocate the array of if_nameindex structure */
	interface_list = malloc((numifs + 1) * sizeof (struct if_nameindex));
	if (!interface_list) {
		int save_err = errno;
		free(buf);
		errno = save_err;
		return (NULL);
	}
	/*
	 * Make sure that terminator structure automatically
	 * happens to be all zeroes.
	 */
	bzero(interface_list, ((numifs + 1) * sizeof (struct if_nameindex)));
	interface_entry = interface_list;
	physinterf_num = 0;
	for (n = numifs; n > 0; n--, lifrp++) {
		size_t	size;

		size = strcspn(lifrp->lifr_name, (char *)IPIF_SEPARATOR_CHAR);
		lifrp->lifr_name[size] = '\0';
		found = B_FALSE;
		/*
		 * Search the current array to see if this interface
		 * already exists
		 */

		for (i = 0; i < physinterf_num; i++) {
			if (strcmp(interface_entry[i].if_name,
			    lifrp->lifr_name) == 0) {
				found = B_TRUE;
				break;
			}
		}

allocate_new:
		/* New one. Allocate an array element and fill it */
		if (!found) {
			if ((interface_entry[physinterf_num].if_name =
			    strdup(lifrp->lifr_name)) == NULL) {
				int save_err;

				if_freenameindex(interface_list);
				save_err = errno;
				free(buf);
				errno = save_err;
				return (NULL);
			}

			/*
			 * Obtain the index value for the interface
			 */
				interface_entry[physinterf_num].if_index =
				    if_nametoindex(lifrp->lifr_name);
				physinterf_num++;
		}
	}

	/* Create the last one of the array */
	interface_entry[physinterf_num].if_name = NULL;
	interface_entry[physinterf_num].if_index = 0;

	/* Free up the excess array space */
	free(buf);
	interface_list = realloc(interface_list, ((physinterf_num + 1) *
	    sizeof (struct if_nameindex)));

	return (interface_list);
}

/*
 * This function frees the the array that is created while
 * the if_nameindex function.
 */
void
if_freenameindex(struct if_nameindex *ptr)
{

	if (ptr == NULL)
		return;


	/* First free the if_name member in each array element */
	while (ptr->if_name != NULL) {
		free(ptr->if_name);
		ptr++;
	}

	/* Now free up the array space */
	free(ptr);
}
