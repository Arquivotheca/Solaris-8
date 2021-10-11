#ident	"@(#)encode.c	1.11	96/04/22 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/dhcp.h>
#include "hash.h"
#include "dhcpd.h"
#include "per_network.h"

/*
 * This file contains the code which creates, manipulates, and frees encode
 * structures.
 */

/*
 * Free an individual encode structure, including data.
 */
void
free_encode(ENCODE *ecp)
{
	if (ecp != (ENCODE *)NULL) {
		if (ecp->data)
			free(ecp->data);
		free(ecp);
	}
}

/*
 * Dump an entire encode list, including data.
 */
void
free_encode_list(ENCODE *ecp)
{
	register ENCODE *ec = ecp, *tmp = (ENCODE *)NULL;

	while (ec != (ENCODE *)NULL) {
		tmp = ec;
		ec = ec->next;
		free_encode(tmp);
	}
}

/*
 * Allocate an ENCODE structure, and fill it in with the passed data.
 *
 * Doesn't copy data if copy_flag is not set.
 *
 * Returns: ptr for success. Doesn't return if a failure occurs.
 */
ENCODE *
make_encode(u_short code, u_char len, void *data, int copy_flag)
{
	register ENCODE *ecp;

	/* LINTED [smalloc returns lw aligned values] */
	ecp = (ENCODE *)smalloc(sizeof (ENCODE));

	ecp->code = code;
	ecp->len = len;

	if (data != (void *)NULL && len != 0) {
		if (copy_flag) {
			ecp->data = (u_char *)smalloc(len);
			(void) memcpy(ecp->data, data, len);
		} else
			ecp->data = data;
	}
	return (ecp);
}

/*
 * Find a specific code in the ENCODE list. Doesn't consider class.
 *
 * Returns: ptr if successful, NULL otherwise.
 */
ENCODE *
find_encode(ENCODE *eclp, u_short code)
{
	register ENCODE	*ep;

	for (ep = eclp; ep != (ENCODE *)NULL; ep = ep->next) {
		if (ep->code == code)
			return (ep);
	}
	return ((ENCODE *)NULL);
}

/*
 * Duplicate the passed encode structure.
 */
ENCODE *
dup_encode(ENCODE *ecp)
{
	if (ecp == (ENCODE *)NULL)
		return ((ENCODE *)NULL);

	return (make_encode(ecp->code, ecp->len, ecp->data, 1));
}

/*
 * Duplicate an encode list.
 */
ENCODE *
copy_encode_list(ENCODE *ecp)
{
	register ENCODE *pp, *ep, *np, *headp;

	if (ecp == NULL)
		return ((ENCODE *)NULL);

	pp = headp = NULL;
	for (ep = ecp; ep != NULL; ep = ep->next) {
		np = dup_encode(ep);
		if (pp == NULL) {
			headp = np;
			np->prev = NULL;
		} else {
			pp->next = np;
			np->prev = pp;
		}
		pp = np;
	}
	return (headp);
}

/*
 * Given two ENCODE lists,  produce NEW ENCODE list by "OR"ing the first
 * encode list with the second. Note that the settings in the second encode
 * list override any identical code settings in the first encode list.
 *
 * The primary list is copied if flags argument is ENC_COPY. Class is not
 * considered.
 *
 * Returns a ptr to the merged list for success, NULL ptr otherwise.
 */
ENCODE *
combine_encodes(ENCODE *first_ecp, ENCODE *second_ecp, int flags)
{
	register ENCODE *ep;

	if (first_ecp != NULL) {
		if (flags == ENC_COPY)
			first_ecp = copy_encode_list(first_ecp);

		if (second_ecp != NULL) {
			for (ep = second_ecp; ep != NULL; ep = ep->next)
				replace_encode(&first_ecp, ep, ENC_COPY);
		}
	} else {
		if (second_ecp != NULL)
			first_ecp = copy_encode_list(second_ecp);
	}
	return (first_ecp);
}

/*
 * Replace/add the encode matching the code value of the second ENCODE
 * parameter in the list represented by the first ENCODE parameter.
 */
void
replace_encode(ENCODE **elistpp, ENCODE *rp, int flags)
{
	register ENCODE *op, *wp;

	if (elistpp == NULL || rp == NULL)
		return;

	if (flags == ENC_COPY)
		rp = dup_encode(rp);

	if (*elistpp == NULL) {
		*elistpp = rp;
		return;
	}

	for (wp = op = *elistpp; wp != NULL; wp = wp->next) {
		if (rp->code == wp->code) {
			if (wp->prev == NULL) {
				rp->next = wp->next;
				*elistpp = rp;
				rp->prev = NULL;
			} else {
				rp->next = wp->next;
				rp->prev = wp->prev;
				wp->prev->next = rp;
			}
			if (wp->next != NULL)
				wp->next->prev = rp;
			free_encode(wp);
			break;
		}
		op = wp;
	}
	if (wp == NULL) {
		op->next = rp;
		rp->prev = op;
	}
}

/*
 * Given a MACRO and a class name, return the ENCODE list for
 * that class name, or null if a ENCODE list by that class doesn't exist.
 */
ENCODE *
vendor_encodes(MACRO *mp, char *class)
{
	register VNDLIST **tvpp;
	register int	i;

	if (mp == NULL || class == NULL)
		return ((ENCODE *)NULL);

	for (tvpp = mp->list, i = 0; tvpp != NULL && i < mp->classes; i++) {
		if (strcmp(tvpp[i]->class, class) == 0)
			return (tvpp[i]->head);
	}
	return ((ENCODE *)NULL);
}
