#ident  "@(#)iflist.c	1.2    97/01/02 SMI"

/* Copyright (c) 1995 by Sun Microsystems, Inc. */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>

#include "iflist.h"
#include "ipd.h"
#include "ipd_ioctl.h"
#include "path.h"
#include "log.h"
#include "ifconfig.h"


struct iflist   *iflist = NULL;
static struct iflist	*ifpool = NULL;

void
add_interface(char *name)
{
	int		i = 0;
	struct iflist   *ifitem;
	struct path	p;

	if ((ifitem = (struct iflist *)malloc(sizeof (struct iflist))) == NULL)
		fail("add_interface: malloc failed\n");

	if ((ifitem->name = (char *)malloc(strlen(name)+1)) == NULL)
		fail("add_interface: malloc failed\n");

	for (i = 0; name[i] != '\0'; ++i)
	    ifitem->name[i] = tolower(name[i]);
	ifitem->name[i] = '\0';


	if (iflist)
		ifitem->next = iflist;
	else
		ifitem->next = NULL;

	iflist = ifitem;

	/*
	 * bug 4014892
	 * create a list of all down ptp interfaces
	 */
	if (strncmp(name, "ipdptp", 6) == 0) {
		p.inf.iftype = IPD_PTP;
		p.inf.ifunit = (u_int)atoi(name + 6);
		if (!is_if_up(&p)) {
			/* add this int. to the pool list */
			ifitem =
			    (struct iflist *)malloc(sizeof (struct iflist));
			if (ifitem == NULL)
				fail("add_interface: malloc failed\n");

			ifitem->name = (char *)malloc(strlen(name)+1);
			if (ifitem->name == NULL)
				fail("add_interface: malloc failed\n");

			for (i = 0; name[i] != '\0'; ++i)
				ifitem->name[i] = tolower(name[i]);
				ifitem->name[i] = '\0';


			if (ifpool)
				ifitem->next = ifpool;
			else
				ifitem->next = NULL;

			ifpool = ifitem;

			ifitem->dst_addr = get_if_dst_addr(&p);
			ifitem->used = B_FALSE;
		}
	}
}

void
register_interfaces(void)
{
	struct strioctl cmio;
	struct iflist	*ifitem;
	int		offset;
	ipd_register_t	req;

	req.msg = IPD_REGISTER;

	cmio.ic_cmd = IPD_REGISTER;
	cmio.ic_timout = 0;
	cmio.ic_dp = (char *)&req;

	ifitem = iflist;
	while (ifitem) {
		if (strncmp("ipdptp", ifitem->name, 6) == 0) {
			req.iftype = IPD_PTP;
			offset = 6;
		} else {
			req.iftype = IPD_MTP;
			offset = 3;
		}
		req.ifunit = atoi(ifitem->name + offset);

		cmio.ic_len = sizeof (ipd_register_t);
		if (ioctl(ipdcm, I_STR, &cmio) < 0)
		    /* perhaps we should fail, rather than just logging it */
		    log(0, "register_interfaces: IPD_REGISTER failed\n");

		ifitem = ifitem->next;
	}
}

void
mark_interface_used(char *name)
{
	struct iflist	*ifitem;

	ifitem = ifpool;

	while (ifitem) {
		if (strcmp(name, ifitem->name) == 0) {
			ifitem->used = B_TRUE;
			return;
		}
		ifitem = ifitem->next;
	}
}

void
mark_interface_free(char *name)
{
	struct iflist	*ifitem;

	ifitem = ifpool;

	while (ifitem) {
		if (strcmp(name, ifitem->name) == 0) {
			ifitem->used = B_FALSE;
			return;
		}
		ifitem = ifitem->next;
	}
}

int
find_interface(u_long dst)
{
	struct iflist	*ifitem;

	ifitem = ifpool;

	while (ifitem) {
		if (dst == ifitem->dst_addr) {
			return (atoi(ifitem->name + 6));
		}
		ifitem = ifitem->next;
	}
	return (-1);
}
