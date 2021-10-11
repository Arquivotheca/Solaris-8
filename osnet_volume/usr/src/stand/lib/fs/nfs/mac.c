/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)mac.c	1.1	99/02/22 SMI"

#include <sys/types.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/salib.h>
#include "socket_inet.h"
#include "mac.h"
#include "atm_inet.h"
#include "ethernet_inet.h"
#include "fddi_inet.h"
#include "token_inet.h"

/*
 * MAC layer interface
 */
int			initialized;	/* Boolean state */
struct mac_type		mac_state;
int			arp_index;	/* current arp table index */
static struct arptable	atable[ARP_TABLE_SIZE];

#if	!defined(__i386)
struct ofw_net_types {
	char	*n_name;	/* OFW network media name */
	int	n_type;		/* IFT */
} ofw_types[] = {
	{ "atm",	IFT_ATM	},
	{ "ethernet",	IFT_ETHER },
	{ "fddi",	IFT_FDDI },
	{ "token-ring",	IFT_ISO88025 }
};
#endif	/* !__i386 */

/*
 * given the mac type, initialize the mac interface state.
 */
void
mac_init(char *bootdevicename)
{
	int		type = 0;
#if !defined(__i386)
	static char	*mtu_name = "max-frame-size";
	static char	*chosen_net = "chosen-network-type";
	static char	*supported_net = "supported-network-types";
	dnode_t		node;
	char		*wp, *media_type;
	int		len = 0, i;
#endif	/* !__i386 */
	char		tmpbuf[MAXNAMELEN];
	char		devname[MAXNAMELEN];

	if (initialized)
		return;

	mac_state.mac_in_timeout = MAC_IN_TIMEOUT;

#ifdef	DEBUG
	printf("mac_init: device path: %s\n", bootdevicename);
#endif	/* DEBUG */

	if ((mac_state.mac_dev = prom_open(bootdevicename)) == 0) {
		(void) sprintf(tmpbuf, "Cannot prom_open network device %s.",
		    bootdevicename);
		prom_panic(tmpbuf);
	}

#if !defined(__i386)
	(void) prom_devname_from_pathname(bootdevicename, devname);
#else
	(void) strcpy(devname, bootdevicename);
#endif	/* !__i386 */

#ifdef	DEBUG
	printf("mac_init: Network device name: %s\n", devname);
#endif	/* DEBUG */

#if !defined(__i386)
	/*
	 * Ask the prom for our MTU and media type. "chosen-network-type"
	 * is of the form of "<network type>,<speed (Mbps)>,<connector type>,
	 * <duplex mode>: e.g.: "ethernet,100,rj45,full"
	 */
	node = prom_finddevice(devname);
	if (node != OBP_NONODE && node != OBP_BADNODE) {
		if (prom_getproplen(node, mtu_name) == sizeof (ihandle_t)) {
			(void) prom_getprop(node, mtu_name,
			    (caddr_t)&mac_state.mac_mtu);
		}
		bzero(tmpbuf, sizeof (tmpbuf));
		if ((len = prom_getproplen(node, chosen_net)) > 0 &&
		    len < sizeof (tmpbuf)) {
			(void) prom_getprop(node, chosen_net, tmpbuf);
		} else if ((len = prom_getproplen(node, supported_net)) > 0 &&
		    len < sizeof (tmpbuf)) {
			(void) prom_getprop(node, supported_net, tmpbuf);
		}
		media_type = NULL;
		if (len > 0) {
			if ((wp = strstr(tmpbuf, ",")) != NULL) {
				*wp = '\0';
				media_type = tmpbuf;
			}
		}
		if (media_type != NULL) {
#ifdef	DEBUG
			printf("mac_init: Media type: %s\n", media_type);
#endif	/* DEBUG */
			for (i = 0; i < sizeof (ofw_types) /
			    sizeof (struct ofw_net_types); i++) {
				if (strcmp(ofw_types[i].n_name,
				    media_type) == 0) {
					type = ofw_types[i].n_type;
					break;
				}
			}
		}
	}
#else
	type = IFT_ETHER;	/* default to ethernet */
#endif	/* !__i386 */

	/* All mac layers look like ethernet MAC address-wise */
	mac_state.mac_addr_len = sizeof (ether_addr_t);
	mac_state.mac_addr_buf = bkmem_alloc(mac_state.mac_addr_len);
	if (mac_state.mac_addr_buf == NULL) {
		prom_panic("mac_init: Cannot allocate memory.");
	}
	if (prom_getmacaddr(mac_state.mac_dev,
	    mac_state.mac_addr_buf) != 0) {
		prom_panic("mac_init: Cannot obtain MAC address.");
	}

	/*
	 * Most Mac types are currently treated as ethernet. Forgive the
	 * duplication below - it's necessary if we begin supporting a
	 * mac type that *doesn't* behave like ethernet.
	 */
	switch (type) {
	case IFT_ATM:
		/*
		 * ATM is currently treated mostly like ethernet,
		 * with the exception that the MTU is most likely
		 * different.
		 */
		mac_state.mac_type = IFT_ATM;
		mac_state.mac_arp_timeout = ATM_ARP_TIMEOUT;
		mac_state.mac_in_timeout = ATM_IN_TIMEOUT;
		if (mac_state.mac_mtu == 0)
			mac_state.mac_mtu = ATMSIZE;
		mac_state.mac_arp = ether_arp;
		mac_state.mac_rarp = ether_revarp;
		mac_state.mac_header_len = ether_header_len;
		mac_state.mac_input = ether_input;
		mac_state.mac_output = ether_output;
		break;

	case IFT_FDDI:
		/*
		 * FDDI is currently treated mostly like ethernet,
		 * with the exception that the MTU is most likely
		 * different.
		 */
		mac_state.mac_type = IFT_FDDI;
		mac_state.mac_arp_timeout = FDDI_ARP_TIMEOUT;
		mac_state.mac_in_timeout = FDDI_IN_TIMEOUT;
		if (mac_state.mac_mtu == 0)
			mac_state.mac_mtu = FDDISIZE;
		mac_state.mac_arp = ether_arp;
		mac_state.mac_rarp = ether_revarp;
		mac_state.mac_header_len = ether_header_len;
		mac_state.mac_input = ether_input;
		mac_state.mac_output = ether_output;
		break;

	case IFT_ISO88025:
		/*
		 * Token ring is currently treated mostly like ethernet,
		 * with the exception that the MTU is most likely different.
		 */
		mac_state.mac_type = IFT_ISO88025;
		mac_state.mac_arp_timeout = TOKEN_ARP_TIMEOUT;
		mac_state.mac_in_timeout = TOKEN_IN_TIMEOUT;
		if (mac_state.mac_mtu == 0)
			mac_state.mac_mtu = TOKENSIZE;
		mac_state.mac_arp = ether_arp;
		mac_state.mac_rarp = ether_revarp;
		mac_state.mac_header_len = ether_header_len;
		mac_state.mac_input = ether_input;
		mac_state.mac_output = ether_output;
		break;

	case IFT_ETHER:
		/* FALLTHRU - default to ethernet */
	default:
		mac_state.mac_type = IFT_ETHER;
		mac_state.mac_mtu = ETHERSIZE;
		mac_state.mac_arp_timeout = ETHER_ARP_TIMEOUT;
		mac_state.mac_in_timeout = ETHER_IN_TIMEOUT;
		if (mac_state.mac_mtu == 0)
			mac_state.mac_mtu = ETHERSIZE;
		mac_state.mac_arp = ether_arp;
		mac_state.mac_rarp = ether_revarp;
		mac_state.mac_header_len = ether_header_len;
		mac_state.mac_input = ether_input;
		mac_state.mac_output = ether_output;
		break;
	}
	mac_state.mac_buf = bkmem_alloc(mac_state.mac_mtu);
	if (mac_state.mac_buf == NULL)
		prom_panic("mac_init: Cannot allocate netbuf memory.");
	else
		initialized = TRUE;
}

void
mac_fini()
{
	if (mac_state.mac_addr_buf != NULL) {
		bkmem_free(mac_state.mac_addr_buf, mac_state.mac_addr_len);
		mac_state.mac_addr_buf = NULL;
	}
	if (mac_state.mac_buf != NULL) {
		bkmem_free(mac_state.mac_buf, mac_state.mac_mtu);
		mac_state.mac_buf = NULL;
	}
	prom_close(mac_state.mac_dev);
	initialized = FALSE;
}

/* MAC layer specific socket initialization */
void
mac_socket_init(struct inetboot_socket *isp)
{
	isp->input[MEDIA_LVL] = mac_state.mac_input;
	isp->output[MEDIA_LVL] = mac_state.mac_output;
	isp->headerlen[MEDIA_LVL] = mac_state.mac_header_len;
	isp->in_timeout = mac_state.mac_in_timeout;
}

/*
 * Add an entry to the ARP table. All values in table are network order.
 * No checking is done to determine whether there's duplicates.
 */
void
mac_set_arp(struct in_addr *ip, void *hp, int hl)
{
	atable[arp_index].ia.s_addr = ip->s_addr;
	bcopy(hp, (char *)atable[arp_index].ha, hl);
	atable[arp_index].hl = hl;
	arp_index++;
	if (arp_index >= ARP_TABLE_SIZE)
		arp_index = 0;
}

/*
 * Retrieve an entry from the ARP table using network-order IP address as
 * search criteria. HW address buffer is filled in up to hl in len. (make
 * sure the buffer is big enough given the mac type)
 *
 * Returns TRUE if successful, FALSE otherwise. Will wait timeout milliseconds
 * for a response.
 */
int
mac_get_arp(struct in_addr *ip, void *hp, int hl, uint32_t timeout)
{
	int i, result;

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ip->s_addr == atable[i].ia.s_addr) {
			bcopy((char *)atable[i].ha, hp, hl);
			return (TRUE);
		}
	}

	/* Not found. ARP for it. */
	bzero(hp, hl);
	result = mac_state.mac_arp(ip, hp, timeout);

	if (result) {
		/* Cool - add it to the arp table */
		mac_set_arp(ip, hp, hl);
	}
	return (result);
}
