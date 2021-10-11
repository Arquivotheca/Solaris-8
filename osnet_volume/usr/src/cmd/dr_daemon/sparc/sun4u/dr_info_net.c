/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_net.c	1.21	99/11/14 SMI"

/*
 * The dr_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with determing the configuration of network devices.
 * Additionally, support to reconfigure network devices/daemons prior
 * to detaching a board is handled here.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "dr_info.h"

/*
 * Use this flag to turn on local debugging code
#define	INFO_NET_DEBUG
 */

/*
 * These command line flags control which operations happen
 * during the unplumb operation.
 */
int	no_smtd_kill = 0;		/* don't kill off smtd daemon */
int	no_net_unplumb = 0;		/* don't unplumb nets for detach */

/*
 * These are flag values for the dv_leaf netflags field.
 */
#define	NET_UP			0x0001
#define	NET_DOWN		0x0002
#define	NET_PRIMARY		0x0004
#define	NET_SSPLINK		0x0008
#ifdef	DR_BF_PBF_FDDI
#define	NET_BF_FDDI		0x0010
#define	NET_PBF_FDDI		0x0020
#endif	/* DR_BF_PBF_FDDI */
#ifdef	DR_NF_FDDI
#define	NET_NF_FDDI		0x0040
#endif	/* DR_NF_FDDI */
#define	NET_AP_ALT		0x0080
#define	NET_AP_ACTIVE		0x0100
#define	NET_IFCONFIG_AP		0x0200
#ifdef	DR_BF_PBF_FDDI
#define	NET_IFCONFIG_PBF	0x0400
#endif	/* DR_BF_PBF_FDDI */
#ifdef	DR_PF_FDDI
#define	NET_PF_FDDI		0x0800
#endif	/* DR_PF_FDDI */
#define	NET_IPV4		0x1000
#define	NET_IPV6		0x2000

/*
 * If any of the NET_USAGE flags are set in a net leaf netflags field,
 * a usage record for the device will be reported.  Used in
 * add_net_usage_record below.  Note that some flags like NET_PRIMARY
 * and NET_SSPLINK can only be set if NET_UP or NET_DOWN are set so
 * there's no need to check them here.
 */
#define	NET_USAGE	(NET_UP|NET_DOWN)

#define	NODENAME	"/etc/nodename"
#define	SSPHOSTNAME	"/etc/ssphostname"
#define	IFCONFIG_PATH	"/usr/sbin/ifconfig"

#ifdef	DR_BF_PBF_FDDI
/*
 * two fddi devices may be managed by the pbf driver.  This is
 * the name the bf devices are ifconfig'd by in this case.
 */
#define	FDDI_PBF_NAME "pbf"

/*
 * Defines to find, kill off, and restart the smtd daemons
 */
#define	FIND_SMTD_CMD \
	"(/usr/bin/ps -e | /usr/bin/grep smtd) >/dev/null 2>/dev/null"
#define	KILL_SMTD_CMD \
	"/opt/SUNWconn/bin/smtd_kill"
#define	START_SMTD_CMD \
	"/usr/sbin/smtd"
#endif	/* DR_BF_PBF_FDDI */

/*
 * The nf fddi has a the nf_snmd which must be stopped/restarted
 * around detaches.
 */
#ifdef	DR_NF_FDDI
#define	FIND_NF_SNMD_CMD \
	"(/usr/bin/ps -e | /usr/bin/grep nf_snmd) >/dev/null 2>/dev/null"
#define	KILL_NF_SNMD_CMD \
	"/etc/opt/SUNWconn/bin/nf_snmd_kill"
#define	START_NF_SNMD_CMD \
	"/etc/opt/SUNWconn/bin/nf_snmd"
#endif	/* DR_NF_FDDI */

/*
 * The pf fddi has a the pf_snmd which must be stopped/restarted
 * around detaches.
 */
#ifdef	DR_PF_FDDI
#define	FIND_PF_SNMD_CMD \
	"(/usr/bin/ps -e | /usr/bin/grep pf_snmd) >/dev/null 2>/dev/null"
#define	KILL_PF_SNMD_CMD \
	"/etc/opt/SUNWconn/bin/pf_snmd_kill"
#define	START_PF_SNMD_CMD \
	"/etc/opt/SUNWconn/bin//pf_snmd"
#endif	/* DR_PF_FDDI */

/*
 * These maintain the states of the different network interface-related daemons
 */
#ifdef	DR_BF_PBF_FDDI
static daemon_type_t smtd_state = DAEMON_UNKNOWN;
#endif	/* DR_BF_PBF_FDDI */
#ifdef	DR_NF_FDDI
static daemon_type_t nf_snmd_state = DAEMON_UNKNOWN;
#endif	/* DR_NF_FDDI */
#ifdef	DR_PF_FDDI
static daemon_type_t pf_snmd_state = DAEMON_UNKNOWN;
#endif	/* DR_PF_FDDI */

/*
 * Pick a large size for our maximum hostname length.  Note
 * that this constant is hard coded in a fscanf string below.
 */
#define	HOSTNAMELEN 81

/*
 * We need to verify that we don't disconnect the interface associated
 * with our hostname or the ssp<->host interface.  These structures
 * will contain the network addresses of these hosts.  Note that these
 * hosts must have internet addresses.
 */
struct hostinfo {
	char 			hi_name[HOSTNAMELEN];
	sa_family_t		hi_family;
	struct sockaddr_in	hi_addr;
	struct sockaddr_in6	hi_addr6;
};

static struct hostinfo nodename, ssphostname;

static int board_netflags;

/*
 * These two variables are used to create an array of
 * all network devices on the board.  Used by dr_unplumb_network()
 */
static int num_net_leaves;
static dr_leafp_t *net_leaf_array;		/* leaf table */

/* Local routines */
static int find_host_addr(const char *hostfile, struct hostinfo *hip);
static void recurs_find_net_entries(dr_iop_t dp, void (*func)(dr_leafp_t dp));
static void get_net_config_info(int s, uint_t iptype,
				dr_leafp_t dlp, struct lifreq *lifrp);
static void add_net_leaf(dr_leafp_t dp);
static void unplumb_net_devices(void);
static void stop_net_daemons(int flags);

/* Globals */
static int kill_daemon(char *find_cmd, char *kill_cmd);


#ifdef AP
/*
 * create_ap_net_leaf
 *
 * Given the name of an interface and alternate pathing information
 * for the interface, add a new leaf node to the network interface
 * list.
 *
 * dlp		interface leaf which has the alias
 * alias	alias dlp is known by
 * is_active	whether dlp is the active or inactive alternate
 */
void
create_ap_net_leaf(dr_leafp_t dlp, int is_active, char *alias)
{
	dr_leafp_t	new;
	int 		len;

	/* Allocate the leaf node */
	new = dr_leaf_malloc();
	if (new == NULL) {
		return;
	}

	/* remember what controller the ap meta-interface belongs to */
	dlp->ap_netname = new;

	/*
	 * Add a new leaf node to the end of the chain for this device
	 * This makes it the preferred name by which to report usage.
	 */
	while (dlp->next != NULL)
		dlp = dlp->next;
	dlp->next = new;

	/* Now fill in the new leaf node */
	new->major_dev = dlp->major_dev;

	/* Must break interface name into name-instance */
	len = strcspn(alias, "0123456789");
	if (len == strlen(alias)) {
	    dr_loginfo("create_ap_net_leaf: interface instance not found\n");
	    return;
	}

	strncpy(new->ifname, alias, len);
	new->ifname[len] = 0;
	new->ifinstance = atoi(&alias[len]);

	new->netflags = NET_AP_ALT;
	if (is_active)
		new->netflags |= NET_AP_ACTIVE;
}
#endif AP

/*
 * build_net_leaf
 *
 * This routine is called when building a network leaf node from
 * the dev-info node. This is called from the build_device_tree() flow.
 *
 * Fill in the leaf structure for a network device type.
 * This consists of finding all the names a network device
 * might be known by and linking new leaf structures onto the
 * one already created.
 *
 * This processing knows about the different types of network
 * devices we currently support and their naming idiosyncrasies.
 *
 * Note that if we later find the network is an AP alternate, we will add
 * a leaf structure for the AP name also.
 *
 * Input: dmpt	- leaf entry to fill in
 *	  name - string to use as the interface name (possibly overridden)
 */
int
build_net_leaf(dr_leafp_t dmpt, char *name)
{
	dr_iop_t	dp;

	dp = dmpt->major_dev;

	/*
	 * fill in the ifname and ifinstance number for the leaf node.
	 *
	 * HIPPI HACK: If this is a hippi controller, the name it is
	 * configured under is 'hi', otherwise it's the major device name.
	 */
	if (strcmp("CHIS,hs", dp->dv_name) == 0) {
		strncpy(dmpt->ifname, "hi", sizeof (dmpt->ifname));
	} else {
		strncpy(dmpt->ifname, name, sizeof (dmpt->ifname));
	}
	dmpt->ifinstance = dp->dv_instance;

	/*
	 * The nf fddi has special snmd daemon kill/restart
	 * processing which happens on a detach.  Save the
	 * fact that we have a nf device on this board.
	 */
#ifdef	DR_NF_FDDI
	if (strcmp("nf", dp->dv_name) == 0) {

		dmpt->netflags |= NET_NF_FDDI;
	}
#endif	/* DR_NF_FDDI */

	/*
	 * The pf fddi (PCI version of nf) has special snmd daemon
	 * kill/restart * processing which happens on a detach.  Save the
	 * fact that we have a pf device on this board.
	 */
#ifdef	DR_PF_FDDI
	if (strcmp("pf", dp->dv_name) == 0) {

		dmpt->netflags |= NET_PF_FDDI;
	}
#endif	/* DR_PF_FDDI */

	/*
	 * fddi kludge:  the bf devices also have special daemons
	 * which must be killed/restarted when a bf interface is
	 * detached.
	 *
	 * Additionally, the bf devices may also be
	 * known as pbfN where this pseudo device consists
	 * of two bf devices.  The instance number in
	 * this case is the bf instance number right shifted
	 * by one.
	 *
	 * Create another leaf node for the pbf device so we can
	 * check for usage of the interface by this name.  Place
	 * the pseudo name first in the linked list since we want
	 * the 'preferred' device name to be the last one on
	 * the list (see find_net_entries).
	 */
#ifdef	DR_BF_PBF_FDDI
	else if (strcmp("bf", dp->dv_name) == 0) {

		dr_leafp_t	pbf_leaf;

		/* Mark the bf node as being a bf fddi device */
		dmpt->netflags |= NET_BF_FDDI;

		/*
		 * Get a leaf entry and link it in
		 */
		pbf_leaf = dr_leaf_malloc();
		if (pbf_leaf == NULL) {
			return (1);
		}
		pbf_leaf->next = dp->dv_leaf;
		dp->dv_leaf = pbf_leaf;

		strncpy(pbf_leaf->ifname, FDDI_PBF_NAME,
			sizeof (pbf_leaf->ifname));
		pbf_leaf->ifinstance = dp->dv_instance>>1;
		pbf_leaf->major_dev = dp;
		pbf_leaf->netflags |= NET_PBF_FDDI;
	}
#endif	/* DR_BF_PBF_FDDI */

	/* All is well */
	return (0);
}

/*
 * add_net_usage_record
 *
 * Fill in the RPC usage record for the given network controller.
 * This is called from the get_io_info() flow to return the network
 * configuration information back to the user.  find_net_entries()
 * gathers the information which is packaged up here.
 *
 * Input: dp -- device to attach the usage record to.
 * dip -- record describing the network controller.
 */
void
add_net_usage_record(sbus_devicep_t dp, dr_iop_t dip)
{
	sbus_usagep_t	up;
	dr_leafp_t	dlp;
	char		addr[80], usage[80];
	char		tmpname[IFNAMSIZ];

	dlp = dip->dv_leaf;
	while (dlp) {

		/*
		 * Add a usage record only if this leaf record represents
		 * usage of the device or if it's the last leaf record
		 * and we've not reported usage as of yet.
		 */
		if ((dlp->netflags & NET_USAGE) == 0 &&
		    dlp->ap_netname == NULL &&
		    !(dlp->next == NULL && dp->usage == NULL)) {

			dlp = dlp->next;
			continue;
		}

		/* Get a usage record and link it in */
		up = calloc(1, sizeof (sbus_usage_t));
		if (up == NULL) {
			dr_logerr(DRV_FAIL, errno, \
				"malloc failed (sbus_usage_t)");
			return;
		}
		up->next = dp->usage;
		dp->usage = up;

		/* Fill in the name */
		sprintf(tmpname, "%s%d", drv_alias2nm(dlp->ifname),
			dlp->ifinstance);
		up->name = strdup(tmpname);

		/* Now save the status of the interface */
		usage[0] = 0;

		if (dlp->netflags & NET_DOWN) {
			strncpy(usage, "down", sizeof (usage));
		}

		if (dlp->netflags & NET_SSPLINK) {
			if (usage[0] != 0)
				strncat(usage, " ", sizeof (usage));
			strncpy(usage, "*SSP*", sizeof (usage));
		}

		/* Add the net name if this is known */
		if (dlp->canonical_name[0] != 0) {
			if (usage[0] != 0)
				strncat(usage, " ", sizeof (usage));
			strncat(usage, dlp->canonical_name, sizeof (usage));
		}

		if (dlp->netflags & NET_IPV4) {
			if (dlp->netaddr.sin_family == AF_INET) {
				strncat(usage, " (", sizeof (usage));
				strncat(usage, inet_ntoa(dlp->netaddr.sin_addr),
					sizeof (usage));
				strncat(usage, ")", sizeof (usage));

			} else {
				/*
				 * We don't know how to print out any other
				 * sort of net address nor its size.
				 */
				sprintf(addr, "af %d (IPv4)",
					dlp->netaddr.sin_family);

				strncat(usage, " (", sizeof (usage));
				strncat(usage, addr, sizeof (usage));
				strncat(usage, ")", sizeof (usage));
			}
		}

		/*
		 * For network devices, the AP info is on a per device basis
		 */
		if (dlp->ap_netname != NULL) {

			/* Add the metaname to the usage */
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));
			sprintf(tmpname, "%s%d", dlp->ap_netname->ifname,
				dlp->ap_netname->ifinstance);
			strncat(usage, tmpname, sizeof (usage));

			/* Mark it as AP active/inatice */
			if (usage[0] != 0)
				strncat(usage, ", ", sizeof (usage));
			if (dlp->ap_netname->netflags & NET_AP_ACTIVE)
				strncat(usage, "AP active", sizeof (usage));
			else
				strncat(usage, "AP alternate", sizeof (usage));
		}

		/*
		 * Tag on IPv6 usage at end to another usage record.
		 */
		if (dlp->netflags & NET_IPV6) {
			char		usage2[80];
			sbus_usagep_t	up2;

			up2 = calloc(1, sizeof (sbus_usage_t));
			up2->next = up->next;
			up->next = up2;

			/* Fill in the name */
			sprintf(tmpname, "%s%d", drv_alias2nm(dlp->ifname),
				dlp->ifinstance);
			up2->name = strdup(tmpname);

			/* Now save the status of the interface */
			strncpy(usage2, "IPv6 ", sizeof (usage2));
			/*
			 * NET_IPV6
			 */
			if (dlp->netaddr6.sin6_family == AF_INET6) {
				inet_ntop(AF_INET6,
					(void *)&dlp->netaddr6.sin6_addr,
					addr, sizeof (addr));
				strncat(usage2, " (", sizeof (usage2));
				strncat(usage2, addr, sizeof (usage2));
				strncat(usage2, ")", sizeof (usage2));
			} else {
				/*
				 * We don't know how to print out any other
				 * sort of net address nor its size.
				 */
				sprintf(addr, " af %d",
					dlp->netaddr.sin_family);

				strncat(usage2, " (", sizeof (usage2));
				strncat(usage2, addr, sizeof (usage2));
				strncat(usage2, ")", sizeof (usage2));
			}
			up2->opt_info = strdup(usage2);
			up2->usage_count = -1;
		}

		up->opt_info = strdup(usage);
		up->usage_count = -1;		/* usage count not available */

		dlp = dlp->next;
	}
}

/*
 * find_net_entries
 *
 * Called from the get_io_info() flow and dr_unplumb_network.
 *
 * Recursively descend the device tree looking for network devices.
 * Once one is found, determine it's status (unplumb, up, down).
 *
 * If the func argument is specified, call this routine once we've
 * determined the network status for the device.
 */
void
find_net_entries(dr_iop_t dp, void (*func)(dr_leafp_t dp))
{
	/*
	 * first initialize the nodename and ssphostname structures
	 * so we can determine the relationship between the board
	 * interfaces and these hosts.
	 *
	 * Note that if there is a failure to find these host
	 * addresses, it is reported, but we don't allow this to
	 * stop the detach operation. In case of error, the nodename
	 * and ssphostname addresses will come back as zero.
	 *
	 * Worst case is that in the unlikely case where we can't get
	 * these host addresses, we'll allow a vital interface to be
	 * detached.
	 */
	if (find_host_addr(NODENAME, &nodename)) {
		dr_loginfo("WARNING: Cannot check for primary interface");
	}

	if (find_host_addr(SSPHOSTNAME, &ssphostname)) {
		dr_loginfo("WARNING: cannot check for cvc/ssp interface.");

	}

	/*
	 * Now go find all the entries.
	 */
	recurs_find_net_entries(dp, func);
}

/*
 * find_host_addr
 *
 * Input is a filename which contains a host name.  Find the network
 * address information for this host.
 *
 * Input: hostfile - name of file to find host name in.
 *
 * Function return: DRV_SUCCESS, address found ok.
 *		    DRV_FAIL - errors while finding addr
 */
static int
find_host_addr(const char *hostfile, struct hostinfo *hip)
{
	FILE		*fd;
	struct hostent	*hp;
	int		retval;

	/*
	 * Zero out the structure just in case we fail to init it.
	 */
	memset((void *)hip, 0, sizeof (struct hostinfo));

	/* Open the netname config file */
	if ((fd = fopen(hostfile, "r")) == NULL) {
		dr_loginfo("Unable to open %s (errno=%d)",
			    hostfile, errno);
		return (DRV_FAIL);
	}

	/*
	 * Now grab the host name -- should be the one and only string
	 * in this file (but we don't verify this -- just grab the first).
	 *
	 * Note the hard coded max string length so we don't overflow
	 * the hostname array.  This is HOSTNAMELEN-1
	 */
	retval = fscanf(fd, "%80s", hip->hi_name);
	fclose(fd);

	if (retval != 1) {
		dr_loginfo("Unable to read host name from %s",
			    hostfile);
		return (DRV_FAIL);
	}

#ifdef INFO_NET_DEBUG
	dr_loginfo("%s contains the host name '%s'.",
		    hostfile, hip->hi_name);
#endif INFO_NET_DEBUG

	hp = gethostbyname(hip->hi_name);
	if (hp == NULL) {
		dr_loginfo("Host addr for %s not found (h_errno=%d)",
			    hip->hi_name, h_errno);
		return (DRV_FAIL);
	}

	/*
	 * Save the first address for the host and ignore the
	 * possibility there may be more than one (currently does
	 * not occur as far as I can tell - other net code ignores
	 * this as well).
	 */
	if ((hp->h_addrtype != AF_INET) && (hp->h_addrtype != AF_INET6)) {
		dr_loginfo("Host address for %s must be internet address.",
			    hip->hi_name);
		return (DRV_FAIL);
	}

	if (hp->h_addr_list[0] == NULL) {
		dr_loginfo("Host address field for %s is null!!",
			    hip->hi_name);
		return (DRV_FAIL);
	}

	hip->hi_family = hp->h_addrtype;
	if (hp->h_addrtype == AF_INET) {
		hip->hi_addr.sin_family = hp->h_addrtype;
		memcpy(&hip->hi_addr.sin_addr, hp->h_addr_list[0],
			hp->h_length);

#ifdef INFO_NET_DEBUG
		dr_loginfo("%s (IPv4) addr=(%d.%d.%d.%d)\n",
			hip->hi_name,
			hip->hi_addr.sin_addr.s_net,
			hip->hi_addr.sin_addr.s_host,
			hip->hi_addr.sin_addr.s_lh,
			hip->hi_addr.sin_addr.s_impno);
#endif INFO_NET_DEBUG
	} else {
#ifdef INFO_NET_DEBUG
		int8_t	*ip;
#endif INFO_NET_DEBUG
		/*
		 * AF_INET6
		 */
		hip->hi_addr6.sin6_family = hp->h_addrtype;
		memcpy(&hip->hi_addr6.sin6_addr, hp->h_addr_list[0],
			hp->h_length);

#ifdef INFO_NET_DEBUG
		ip = hip->hi_addr6.sin6_addr.s6_addr;
		dr_loginfo("%s (IPv6) "
			"addr=(0x%x%x:%x%x:%x%x;%x%x:%x%x:%x%x:%x%x:%x%x)\n",
			hip->hi_name,
			ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6],
			ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13],
			ip[14], ip[15]);
#endif INFO_NET_DEBUG
	}

	return (DRV_SUCCESS);
}

/*
 * recurs_find_net_entries
 *
 * Find all the net leaves and determine their status.
 *
 */
static void
recurs_find_net_entries(dr_iop_t dp, void (*func)(dr_leafp_t dp))
{
	int		s, s6;
	struct lifreq 	lifr;
	dr_leafp_t	dlp;

	if (dp->dv_sibling) recurs_find_net_entries(dp->dv_sibling, func);
	if (dp->dv_child) {
		recurs_find_net_entries(dp->dv_child, func);
		return;
	}

	/*
	 * Node types only saved for leaf (childless) entries.
	 * Only look at network entries.
	 */
	if (dp->dv_node_type != DEVICE_NET)
		return;

	/* Now check the status of network devices */

	/*
	 * Check for both IPv4 and IPv6 interfaces.
	 */
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		dr_loginfo("Cannot open PF_INET socket (errno=%d)\n",
			errno);
		return;
	}
	s6 = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s6 < 0) {
		dr_loginfo("Cannot open PF_INET6 socket (errno=%d)\n",
			errno);
		close(s);
		return;
	}

	/*
	 * For each name the interface if known by, see if there
	 * is usage for the device.
	 */
	dlp = dp->dv_leaf;
	while (dlp) {

		/*
		 * Don't check for usage of an ap meta-interface
		 * unless the controller is the active alternate.
		 */
		if ((dlp->netflags & (NET_AP_ALT | NET_AP_ACTIVE)) ==
		    NET_AP_ALT) {
			dlp = dlp->next;
			continue;
		}

		memset((void *) &lifr, 0, sizeof (lifr));
		sprintf(lifr.lifr_name, "%s%d", drv_alias2nm(dlp->ifname),
			dlp->ifinstance);
		/*
		 * IPv4
		 */
		if (ioctl(s, SIOCGLIFFLAGS, (caddr_t)&lifr) >= 0) {
			/*
			 * The interface is plumbed.  Get more
			 * information if possible.
			 */
			if (lifr.lifr_flags & IFF_UP)
				dlp->netflags |= NET_UP;
			else
				dlp->netflags |= NET_DOWN;

			dlp->netflags |= NET_IPV4;
			get_net_config_info(s, NET_IPV4, dlp, &lifr);
		}

		memset((void *) &lifr, 0, sizeof (lifr));
		sprintf(lifr.lifr_name, "%s%d", drv_alias2nm(dlp->ifname),
			dlp->ifinstance);
		/*
		 * IPv6
		 */
		if (ioctl(s6, SIOCGLIFFLAGS, (caddr_t)&lifr) >= 0) {
			/*
			 * The interface is plumbed.  Get more
			 * information if possible.
			 */
			if (lifr.lifr_flags & IFF_UP)
				dlp->netflags |= NET_UP;
			else
				dlp->netflags |= NET_DOWN;

			dlp->netflags |= NET_IPV6;
			get_net_config_info(s6, NET_IPV6, dlp, &lifr);
		}

		/* Call the function if one is provided */
		if (func)
			(*func)(dlp);

		dlp = dlp->next;
	}

	close(s6);
	close(s);
}

/*
 * get_net_config_info
 *
 * Try to get additional information on the interface if it is
 * config'd.  Note that the dv_leaf struct is initially zero'd out so
 * all uninitialized fields remain 0.
 *
 * Input: s - socket fd we can do ioctls on
 *        dlp - pointer to the leaf structure to fill in
 *        ifrp - pointer to ifreq struct we can do ioctls with
 */
static void
get_net_config_info(int s, uint_t iptype, dr_leafp_t dlp, struct lifreq *lifrp)
{
	struct hostent	*hp;

	/*
	 * If this is device is configured via it's AP or pbf name
	 * note this.
	 */
	if (dlp->netflags & NET_AP_ACTIVE) {
		dlp->netflags |= NET_IFCONFIG_AP;
	}
#ifdef	DR_BF_PBF_FDDI
	else if (dlp->netflags & NET_PBF_FDDI) {
		dlp->netflags |= NET_IFCONFIG_PBF;
	}
#endif	/* DR_BF_PBF_FDDI */

	/*
	 * Find the interface address
	 */
	if (ioctl(s, SIOCGLIFADDR, (caddr_t)lifrp) < 0) {
#ifdef INFO_NET_DEBUG
		dr_loginfo("get_net_config_info: SIOCGLIFADDR for %s "
			"failed (errno = %d)\n", lifrp->lifr_name, errno);
#else
		/*
		 * code stolen from ifconfig.c.  I think that
		 * these errnos are not really something we want
		 * to complain about.
		 */
		if (errno != EADDRNOTAVAIL && errno != EAFNOSUPPORT) {
			dr_loginfo("get_net_config_info: %s no address "
				"(errno=%d)\n", lifrp->lifr_name, errno);
		}
#endif INFO_NET_DEBUG
		/*
		 * Can't do anything without an address.
		 */
		return;
	}

	/*
	 * Save a copy of the address.  However, we can
	 * only move forward if the family is an internet address
	 */
	if (iptype & NET_IPV4) {
		dlp->netaddr = *((struct sockaddr_in *)&lifrp->lifr_addr);
		if (dlp->netaddr.sin_family != AF_INET)
			return;

		if ((nodename.hi_family == AF_INET) &&
				(dlp->netaddr.sin_addr.s_addr ==
					nodename.hi_addr.sin_addr.s_addr)) {
			dlp->netflags |= NET_PRIMARY;
		}
		hp = gethostbyaddr((char *)&dlp->netaddr.sin_addr.s_addr,
					sizeof (dlp->netaddr.sin_addr.s_addr),
					AF_INET);
	} else {
		/*
		 * IPv6
		 */
		dlp->netaddr6 = *((struct sockaddr_in6 *)&lifrp->lifr_addr);
		if (dlp->netaddr6.sin6_family != AF_INET6)
			return;

		if ((nodename.hi_family == AF_INET6) &&
			!memcmp((caddr_t)&dlp->netaddr6.sin6_addr,
				(caddr_t)&nodename.hi_addr6.sin6_addr,
				sizeof (dlp->netaddr6.sin6_addr))) {
			dlp->netflags |= NET_PRIMARY;
		}
		hp = gethostbyaddr((char *)&dlp->netaddr6.sin6_addr,
					sizeof (dlp->netaddr6.sin6_addr),
					AF_INET6);
	}

	/*
	 * Get the interface name.
	 */
	if (hp != NULL) {
		if (hp->h_name != NULL) {

			/*
			 * copy size-1 to make sure the string is
			 * null terminated
			 */
			strncpy(dlp->canonical_name, hp->h_name,
				sizeof (dlp->canonical_name)-1);
		}
	}
#ifdef INFO_NET_DEBUG
	else {
		dr_loginfo("Hostent for %s%d not found (h_errno=%d)\n",
			    dlp->ifname, dlp->ifinstance, h_errno);
	}
#endif INFO_NET_DEBUG

	/*
	 * If the ssphostname is not an INET address, we can't do the
	 * following check.  Note we've already reported this condition
	 * in find_net_entries().
	 * Also, the IP version for the interface we're checking has
	 * to be the same version of the SSP to be worth checking.
	 */
	if (((ssphostname.hi_family != AF_INET) &&
			(ssphostname.hi_family != AF_INET6)) ||
		((ssphostname.hi_family == AF_INET) && (iptype == NET_IPV6)) ||
		((ssphostname.hi_family == AF_INET6) && (iptype == NET_IPV4)))
		return;

	/*
	 * In order to determine if this interface is on the ssp subnet,
	 * we need it's netmask.
	 */
	if (ioctl(s, SIOCGLIFNETMASK, (caddr_t)lifrp) < 0) {
		/*
		 * If this interface does not have a netmask, then
		 * we can't check to see if it's on ssp subnet.
		 */
		dr_loginfo("WARNING: %s %s%d %s errno=%d\n",
			"Cannot determine if",
			dlp->ifname, dlp->ifinstance,
			"is cvc/ssp interface.  SIOCGLIFNETMASK",
			errno);
		return;
	}

	if (iptype == NET_IPV4) {
		ulong_t	mask, local_subnet, ssp_subnet;

		mask = ((struct sockaddr_in *)
				&lifrp->lifr_addr)->sin_addr.s_addr;
#ifdef INFO_NET_DEBUG
		dr_loginfo("netmask for (IPv4) %s%d=0x%08lx\n",
				dlp->ifname, dlp->ifinstance, mask);
#endif INFO_NET_DEBUG
		local_subnet = dlp->netaddr.sin_addr.s_addr & mask;
		ssp_subnet = ssphostname.hi_addr.sin_addr.s_addr & mask;

		if (mask && (local_subnet == ssp_subnet))
			dlp->netflags |= NET_SSPLINK;
	} else {
		/*
		 * IPv6
		 */
		int		s, notequal;
		struct sockaddr_in6	*sinp;
		in6_addr_t	*mask;
		uint8_t		local_subnet_octet, ssp_subnet_octet;
		uint32_t	maskv;

		sinp = (struct sockaddr_in6 *)&lifrp->lifr_addr;
		mask = &sinp->sin6_addr;
#ifdef INFO_NET_DEBUG
		dr_loginfo("netmask for (IPv6) %s%d=0x%x%x:%x%x:%x%x:%x%x:"
				"%x%x:%x%x:%x%x:%x%x\n",
				dlp->ifname, dlp->ifinstance,
				mask->s6_addr[0], mask->s6_addr[1],
				mask->s6_addr[2], mask->s6_addr[3],
				mask->s6_addr[4], mask->s6_addr[5],
				mask->s6_addr[6], mask->s6_addr[7],
				mask->s6_addr[8], mask->s6_addr[9],
				mask->s6_addr[10], mask->s6_addr[11],
				mask->s6_addr[12], mask->s6_addr[13],
				mask->s6_addr[14], mask->s6_addr[15]);
#endif INFO_NET_DEBUG
		maskv = 0;
		notequal = 0;
		for (s = 0; s < sizeof (mask->s6_addr); s++) {
			maskv += mask->s6_addr[s];
			local_subnet_octet =
				dlp->netaddr6.sin6_addr.s6_addr[s] &
							mask->s6_addr[s];
			ssp_subnet_octet =
				ssphostname.hi_addr6.sin6_addr.s6_addr[s] &
							mask->s6_addr[s];
			if (local_subnet_octet != ssp_subnet_octet)
				notequal++;
		}

		if (maskv && !notequal)
			dlp->netflags |= NET_SSPLINK;
	}
}

/*
 * dr_unplumb_network
 *
 * This routine is called from the detach logic prior to doing a
 * complete detach operation.
 *
 * Find all the network devices on the specified board and whether
 * they are configured.  If so, unplumb them in preparation for
 * detaching the board.
 *
 * Additionally, this routine verfies that we're not going to
 * unplumb a vital network connection such as the nodename or
 * ssphostname.
 *
 * Input: board to check interfaces for
 *
 * function return value:
 *	DRV_SUCCESS - no show stoppers for the detach
 *	DRV_FAIL - vital network interfaces are on this board, don't detach.
 *
 * Note that DRV_FAIL is only returned if we find a vital network device
 * which we don't want to unplumb.  Any other errors while trying to find
 * the configuration are ignored since they aren't vital to the detach.
 * If we don't unplumb an interface, worst thing that can happen is that
 * the device detach will fail and the user must ifconfig the device down
 * manually or try the detach complete again.
 */
int
dr_unplumb_network(int board)
{
	dr_iop_t	rootp;
	int		retval;

	/* read the dev-info tree */
	rootp = build_device_tree(board);
	if (rootp == NULL) {
		return (DRV_SUCCESS);
	}

	/* If there are no leaf nodes on this board, nothing left to do */
	if (num_leaves == 0) {
		free_dr_device_tree(rootp);
		return (DRV_SUCCESS);
	}

#ifdef AP
	/*
	 * We must add in the AP netnames now for only network devices
	 */
	add_ap_meta_devices(rootp, 1);
#endif AP

	/*
	 * In addition to finding the configuration of the net devices,
	 * find_net_entries (via add_net_leaf), will build a table of the
	 * network leaves for us to go through.  Num_leaves is the
	 * total number of leaves, including non-network devices.
	 */
	net_leaf_array = (dr_leafp_t *)calloc(num_leaves, sizeof (dr_leafp_t));
	if (net_leaf_array == NULL) {

		dr_logerr(DRV_FAIL, errno, "malloc failed (net_leaf_array)");

		free_dr_device_tree(rootp);
		return (DRV_SUCCESS);
	}
	num_net_leaves = 0;

	/*
	 * Now find all the net entries and their status. We add
	 * these entries to our leaf array as they are encountered
	 * and also check to see if vital interfaces are on the board.
	 */
	board_netflags = 0;
	find_net_entries(rootp, add_net_leaf);

	retval = DRV_SUCCESS;

	/*
	 * Complain if there are vital intefaces on the
	 * board and don't allow it to be detached.
	 */
	if (board_netflags &
#ifdef	DR_BF_PBF_FDDI
	    (NET_PRIMARY|NET_SSPLINK|NET_IFCONFIG_AP|NET_IFCONFIG_PBF)) {
#else	DR_BF_PBF_FDDI
	    (NET_PRIMARY|NET_SSPLINK|NET_IFCONFIG_AP)) {
#endif	/* DR_BF_PBF_FDDI */
		char msg[MAXMSGLEN];
		int found;

		sprintf(msg, "Cannot detach board %d.  It has ", board);
		found = 0;

		if (board_netflags & NET_PRIMARY) {
			found = 1;
			strncat(msg, "primary", sizeof (msg));
		}
		if (board_netflags & NET_SSPLINK) {
			if (found) {
				strncat(msg, "/", sizeof (msg));
			} else {
				found = 1;
			}
			strncat(msg, "ssp", sizeof (msg));
		}
		if (board_netflags & NET_IFCONFIG_AP) {
			if (found) {
				strncat(msg, "/", sizeof (msg));
			} else {
				found = 1;
			}
			strncat(msg, "AP", sizeof (msg));
		}
#ifdef	DR_BF_PBF_FDDI
		if (board_netflags & NET_IFCONFIG_PBF) {
			if (found) {
				strncat(msg, "/", sizeof (msg));
			} else {
				found = 1;
			}
			strncat(msg, "pbf", sizeof (msg));
		}
#endif	/* DR_BF_PBF_FDDI */

		strncat(msg, " interfaces configured.", sizeof (msg));
		dr_logerr(DRV_FAIL, 0, msg);

		retval = DRV_FAIL;

	} else {
		stop_net_daemons(board_netflags);
		unplumb_net_devices();
	}

	/* Free up the temporary structures we've created */
	free(net_leaf_array);
	free_dr_device_tree(rootp);
	return (retval);
}

/*
 * add_net_leaf
 *
 * If the interface has been configured, place a pointer to the
 * network leaf into the net_leaf_array
 *
 * Also check to see if this interface is the primary network interface
 * or the ssp<->cvc host.
 */
static void
add_net_leaf(dr_leafp_t dlp)
{
	if ((dlp->netflags & NET_USAGE) != 0)
		net_leaf_array[num_net_leaves++] = dlp;

	board_netflags |= dlp->netflags;
}

/*
 * unplumb_net_devices
 *
 * Go through the net_leaf_array and use ifconfig to down/unplumb the
 * networks.
 */
static void
unplumb_net_devices(void)
{
	int 		i;
	dr_leafp_t	dlp;
	char 		*cmdline[MAX_CMD_LINE];
	char		tmpname[IFNAMSIZ];

	/*
	 * Just in case we don't want this automatic network reconfig,
	 * allow a flag to disable it.
	 */
	if (no_net_unplumb)
		return;

	for (i = 0; i < num_net_leaves; i++) {

		dlp = net_leaf_array[i];

		sprintf(tmpname, "%s%d", drv_alias2nm(dlp->ifname),
			dlp->ifinstance);

		cmdline[0] = "ifconfig";
		cmdline[1] = tmpname;
		cmdline[3] = "down";
		cmdline[4] = "unplumb";
		cmdline[5] = (char *)0;

		if (dlp->netflags & NET_IPV4) {
			/*
			 * Try to take down IPv4 interface in case
			 * it's plumbed.
			 *
			 *	ifconfig IF inet down unplumb
			 */
			cmdline[2] = "inet";

			if (verbose)
				dr_loginfo("running %s %s %s %s %s\n",
					IFCONFIG_PATH, cmdline[1], cmdline[2],
					cmdline[3], cmdline[4]);

#ifndef NO_SU
			if (exec_command(IFCONFIG_PATH, cmdline)) {
				dr_loginfo("ifconfig %s inet down "
					"unplumb failed.\n", tmpname);
				return;
			}
#endif NO_SU
		}

		if (dlp->netflags & NET_IPV6) {
			/*
			 * Now try to take down IPv6 interface in case
			 * it's plumbed.
			 *
			 *	ifconfig IF inet6 down unplumb
			 */
			cmdline[2] = "inet6";

			if (verbose)
				dr_loginfo("running %s %s %s %s %s\n",
					IFCONFIG_PATH, cmdline[1], cmdline[2],
					cmdline[3], cmdline[4]);

#ifndef NO_SU
			if (exec_command(IFCONFIG_PATH, cmdline)) {
				dr_loginfo("ifconfig %s inet6 down "
					"unplumb failed.\n", tmpname);
				return;
			}
#endif NO_SU
		}


	}
}

/*
 * stop_net_daemons
 *
 * Determine which, if any, net daemons must be killed prior to
 * detaching this board.
 */
static void
stop_net_daemons(int flags)
{
	/* We allow this to be selectively disabled for testing purposes */
	if (no_smtd_kill)
		return;

#ifdef	DR_BF_PBF_FDDI
	if ((flags & NET_BF_FDDI) && smtd_state == DAEMON_UNKNOWN) {
		smtd_state = kill_daemon(FIND_SMTD_CMD, KILL_SMTD_CMD);
	}
#endif	/* DR_BF_PBF_FDDI */

#ifdef	DR_NF_FDDI
	if ((flags & NET_NF_FDDI) && nf_snmd_state == DAEMON_UNKNOWN) {
		nf_snmd_state = kill_daemon(FIND_NF_SNMD_CMD, KILL_NF_SNMD_CMD);
	}
#endif	/* DR_NF_FDDI */

#ifdef	DR_PF_FDDI
	if ((flags & NET_PF_FDDI) && pf_snmd_state == DAEMON_UNKNOWN) {
		pf_snmd_state = kill_daemon(FIND_PF_SNMD_CMD, KILL_PF_SNMD_CMD);
	}
#endif	/* DR_PF_FDDI */
}

/*
 * kill_daemon
 *
 * This routine is called when we're detaching a board which contains
 * a network controller which has a daemon which needs to be killed off
 * prior to detaching the board.
 *
 * If a daemon is running, kill it off and remember that we need
 * to restart it after the detach operation is complete.  The detach
 * code will call _restart_ functions once the
 * detach is successfully completed or if the detach operation is aborted.
 *
 * Input
 *	find_cmd	- command to execute to determine if the daemon is
 *			  is executing
 *	kill_cmd	- command to execute to kill the daemon
 *
 * Output
 *	the state of the network daemon (killed, not present, unknown)
 */
static int
kill_daemon(char *find_cmd, char *kill_cmd)
{
	int retval;
	char *cmdline[MAX_CMD_LINE];

	/*
	 * See if we have net daemons currently running
	 */
	retval = dosys(find_cmd);

	if (retval < 0)
		/* Error executing the command (already reported) */
		return (DAEMON_UNKNOWN);

	if (retval != 0) {
		/* No daemons executing */
		return (DAEMON_NOT_PRESENT);
	}

	/*
	 * daemons present, off with their heads...
	 */
	cmdline[0] = kill_cmd;
	cmdline[1] = 0;
	if (verbose)
		dr_loginfo("running %s\n", kill_cmd);

#ifndef NO_SU
	if ((retval = exec_command(kill_cmd, cmdline)) != 0) {
		dr_loginfo("Warning: Error return from %s (%d)\n",
			    kill_cmd, retval);
	}
#endif NO_SU

	return (DAEMON_KILLED);
}

/*
 * dr_restart_net_daemons
 *
 * This routine is called after a board has been detached or the detach
 * operation is aborted.
 *
 * If necessary, we'll restart the smtd daemon if it was killed off during
 * the detach operation.
 */
void
dr_restart_net_daemons(void)
{
	char *cmdline[MAX_CMD_LINE];

#ifdef	DR_BF_PBF_FDDI
	if (smtd_state == DAEMON_KILLED) {

		cmdline[0] = "smtd";
		cmdline[1] = 0;
		if (verbose)
			dr_loginfo("running %s\n", START_SMTD_CMD);

#ifndef NO_SU
		/*
		 * Really don't care about the return value since
		 * smtd seems to be arbitrary about it.  If the
		 * exec failed, this has already been reported and
		 * that's the only error we're interested in.
		 */
		(void) exec_command(START_SMTD_CMD, cmdline);
#endif NO_SU
	}
	smtd_state = DAEMON_UNKNOWN;
#endif	/* DR_BF_PBF_FDDI */

#ifdef	DR_NF_FDDI
	if (nf_snmd_state == DAEMON_KILLED) {

		cmdline[0] = "nf_snmd";
		cmdline[1] = 0;
		if (verbose)
			dr_loginfo("running %s\n", START_NF_SNMD_CMD);

#ifndef NO_SU
		/*
		 * Really don't care about the return value since
		 * nf_snmd seems to be arbitrary about it.  If the
		 * exec failed, this has already been reported and
		 * that's the only error we're interested in.
		 */
		(void) exec_command(START_NF_SNMD_CMD, cmdline);
#endif NO_SU
	}
	nf_snmd_state = DAEMON_UNKNOWN;
#endif	/* DR_NF_FDDI */

#ifdef	DR_PF_FDDI
	if (pf_snmd_state == DAEMON_KILLED) {

		cmdline[0] = "pf_snmd";
		cmdline[1] = 0;
		if (verbose)
			dr_loginfo("running %s\n", START_PF_SNMD_CMD);

#ifndef NO_SU
		/*
		 * Really don't care about the return value since
		 * nf_snmd seems to be arbitrary about it.  If the
		 * exec failed, this has already been reported and
		 * that's the only error we're interested in.
		 */
		(void) exec_command(START_PF_SNMD_CMD, cmdline);
#endif NO_SU
	}
	pf_snmd_state = DAEMON_UNKNOWN;
#endif	/* DR_PF_FDDI */
}
