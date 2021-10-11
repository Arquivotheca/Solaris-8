/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strplumb.c	1.45	99/10/18 SMI"

#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/user.h>
#include	<sys/vfs.h>
#include	<sys/vnode.h>
#include	<sys/file.h>
#include	<sys/stream.h>
#include	<sys/stropts.h>
#include	<sys/strsubr.h>
#include	<sys/dlpi.h>
#include	<sys/vnode.h>
#include	<sys/socket.h>
#include	<sys/sockio.h>
#include	<net/if.h>

#include	<sys/cred.h>
#include	<sys/sysmacros.h>

#include	<sys/sad.h>
#include	<sys/kstr.h>
#include	<sys/bootconf.h>

#include	<sys/errno.h>
#include	<sys/modctl.h>
#include	<sys/sunddi.h>
#include	<sys/promif.h>

#include	<netinet/in.h>
#include	<netinet/ip6.h>
#include	<netinet/icmp6.h>
#include	<inet/common.h>
#include	<inet/ip.h>
#include	<inet/ip6.h>
#include	<inet/tcp.h>

#include	<sys/strlog.h>
#include	<sys/log.h>

static int setifmuxid(vnode_t *vp, const char *ifname, int arp_id, int ip_id);
static void getifname(vnode_t *vp, int unit, char *ifname);
static int setifname(vnode_t *vp, char *ifname, int unit, boolean_t getflags);
static int resolve_netdrv(int *unitp, major_t *ndev);

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops,
	"Configure STREAMS Plumbing."
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlmisc,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int	strplumbdebug = 0;

major_t	ARP_MAJ;
major_t	TCP_MAJ;
major_t	UDP_MAJ;
major_t	ICMP_MAJ;
major_t	CLONE_MAJ;
major_t	TCP6_MAJ;
major_t	UDP6_MAJ;
major_t	ICMP6_MAJ;
major_t	IP6_MAJ;
minor_t	IP6_MIN;

#define	ARP		"arp"
#define	TCP		"tcp"
#define	TCP6		"tcp6"
#define	UDP		"udp"
#define	UDP6		"udp6"
#define	ICMP		"icmp"
#define	ICMP6		"icmp6"
#define	IP		"ip"
#define	IP6		"ip6"
#define	CLONE		"clone"
#define	TIMOD		"timod"

#define	DRIVER		"drv"
#define	STRMOD		"strmod"

/*
 * There must be at least one NULL entry in this table...
 */
static struct strp_list {
	char *type;
	char *name;
} strp_list[] = {
	{DRIVER, CLONE},
	{DRIVER, IP},
	{DRIVER, IP6},
	{DRIVER, TCP},
	{DRIVER, TCP6},
	{DRIVER, UDP},
	{DRIVER, UDP6},
	{DRIVER, ICMP},
	{DRIVER, ICMP6},
	{DRIVER, ARP},
	{STRMOD, TIMOD},
	{NULL, NULL}
};

/*
 * Called from swapgeneric.c:loadrootmodules() in the network boot case to
 * get all the plumbing drivers and their .conf files properly loaded
 * and added to the defer list, so they are here when asked for later.
 */

int
strplumb_get_driver_list(int flag, char **type, char **name)
{
	static struct strp_list *p;
	if (flag)
		p = strp_list;
	else if (p->name)
		++p;

	if (p->name == NULL)
		return (0);

	*type = p->type;
	*name = p->name;
	return (1);
}

/*
 * Do streams plumbing for internet protocols.
 */
void
strplumb(void)
{
	vnode_t		*vp = NULL;				/* not open */
	vnode_t		*nvp = NULL;				/* not open */
	int		fd = -1;				/* not open */
	int		more;
	int		muxid, arp_muxid, ip_muxid;
	int		error;
	uint_t		anchor = 1;
	major_t		maj;
	minor_t		min;
	char		*mods[5];
	char		ifname[LIFNAMSIZ];
	major_t		ndev;
	char		*name, *type;
	int		unit;

	more = strplumb_get_driver_list(1, &type, &name);
	while (more)  {
		int err;

		if (strcmp(type, DRIVER) == 0) {
			err = ddi_install_driver(name);
		} else {
			err = modload(type, name);
		}

		if (err < 0)  {
			printf("strplumb: can't install module %s/%s, err %d\n",
			    type, name, err);
			return;
		}

		more = strplumb_get_driver_list(0, &type, &name);
	}

	TCP_MAJ = ddi_name_to_major(TCP);
	UDP_MAJ = ddi_name_to_major(UDP);
	ICMP_MAJ = ddi_name_to_major(ICMP);
	ARP_MAJ = ddi_name_to_major(ARP);

	IP6_MAJ = ddi_name_to_major(IP6);
	IP6_MIN = (minor_t)1;
	TCP6_MAJ = ddi_name_to_major(TCP6);
	UDP6_MAJ = ddi_name_to_major(UDP6);
	ICMP6_MAJ = ddi_name_to_major(ICMP6);

	CLONE_MAJ = ddi_name_to_major(CLONE);

	/* First set up the autopushes */

	maj = TCP_MAJ;
	min = (minor_t)-1;
	mods[0] = TCP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, &anchor, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/TCP) failed: %d\n", error);
		return;
	}

	maj = TCP6_MAJ;
	min = (dev_t)-1;
	mods[0] = TCP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, &anchor, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/TCP6) failed: %d\n", error);
		return;
	}

	maj = UDP_MAJ;
	min = (minor_t)-1;
	mods[0] = UDP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, &anchor, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/UDP) failed: %d\n", error);
		return;
	}

	maj = UDP6_MAJ;
	min = (dev_t)-1;
	mods[0] = UDP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, &anchor, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/UDP6) failed: %d\n", error);
		return;
	}

	maj = ICMP_MAJ;
	min = (minor_t)-1;
	mods[0] = ICMP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, NULL, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/ICMP) failed: %d\n", error);
		return;
	}

	maj = ICMP6_MAJ;
	min = (dev_t)-1;
	mods[0] = ICMP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, NULL, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/ICMP6) failed: %d\n", error);
		return;
	}

	maj = ARP_MAJ;
	min = (minor_t)-1;
	mods[0] = ARP;
	mods[1] = (char *)NULL;
	error = kstr_autopush(SET_AUTOPUSH, &maj, &min, NULL, &anchor, mods);
	if (error != 0) {
		printf("kstr_autopush(SET/ARP) failed: %d\n", error);
		return;
	}

	if (strplumbdebug)
		printf("strplumb: configure default tcp stream\n");
	if (error = kstr_open(IP6_MAJ, IP6_MIN, &nvp, NULL)) {
		printf("strplumb: kstr_open IP6_MAJ failed error %d\n", error);
		return;
	}

	/*
	 * We set the tcp default queue to IPv6 because IPv4 falls
	 * back to IPv6 when it can't find a client, but
	 * IPv6 does not fall back to IPv4.
	 */
	if (error = kstr_open(TCP6_MAJ, IP6_MIN, &vp, &fd)) {
		printf("strplumb: kstr_open TCP6_MAJ failed error %d\n",
		    error);
		goto out;
	}
	if (error = kstr_ioctl(vp, TCP_IOC_DEFAULT_Q, (intptr_t)0)) {
		printf(
		    "strplumb: kstr_ioctl TCP_IOC_DEFAULT_Q failed error %d\n",
		    error);
		goto out;
	}
	if (error = kstr_plink(nvp, fd, &muxid)) {
		printf("strplumb: kstr_plink failed, error %d\n", error);
		goto out;
	}
	(void) kstr_close(NULL, fd);
	fd = -1;

	ndev = (major_t)-1;

	if (error = resolve_netdrv(&unit, &ndev)) {
		printf("strplumb: resolve_netdrv: %d\n", error);
		goto out;
	}

	(void) kstr_close(nvp, -1);
	nvp = NULL;

	if (ndev == (major_t)-1) {
		/* No interface to plumb */
		if (strplumbdebug)
			printf("strplumb: no interface to plumb\n");
		return;
	}

	/*
	 * Now set up the links. Ultimately, we should have two
	 * streams permanently linked underneath UDP (which is
	 * actually IP with UDP autopushed). One stream consists of
	 * the ARP-[ifname] combination, while the other consists of
	 * ARP-IP-[ifname]. The second combination seems a little
	 * weird, but is linked underneath UDP just to keep it around.
	 *
	 * We pin underneath UDP here to match what is done in
	 * ifconfig(1M); otherwise, ifconfig will be unable to unplumb
	 * the stream (the major number and mux id must both match for
	 * a successful I_PUNLINK).
	 *
	 * There are subtleties in the plumbing which make it essential to
	 * follow the logic used in ifconfig(1m) very closely.  So we open
	 * the IP-device first, and then the ARP-device.  The final argument
	 * to setifname() says whether to prefetch the interface flags before
	 * we set the interface name.
	 */
	if (error = kstr_open(CLONE_MAJ, UDP_MAJ, &nvp, NULL)) {
		printf("strplumb: kstr_open of UDP failed, error %d\n",
		    error);
		return;
	}

	if (error = kstr_open(CLONE_MAJ, ndev, &vp, &fd)) {
		printf("strplumb: kstr_open CLONE/<net> failed error %d\n",
		    error);
		goto out;
	}

	getifname(vp, unit, ifname);	/* find the driver's ifname */

	if (error = kstr_push(vp, IP)) {
		printf("strplumb: kstr_push IP failed, error %d\n", error);
		goto out;
	}
	if (error = setifname(vp, ifname, unit, B_TRUE)) {
		printf("strplumb: setifname %s unit %d IP failed, error %d\n",
			ifname, unit, error);
		goto out;
	}
	if (error = kstr_push(vp, ARP)) {
		printf("strplumb: kstr_push ARP failed, error %d\n", error);
		goto out;
	}
	if (error = kstr_plink(nvp, fd, &ip_muxid)) {
		printf("strplumb: kstr_plink IP-ARP/<net> failed, error %d\n",
		    error);
		goto out;
	}

	(void) kstr_close(NULL, fd);
	fd = -1;

	if (error = kstr_open(CLONE_MAJ, ndev, &vp, &fd)) {
		printf("strplumb: kstr_open CLONE/<net> failed, error %d\n",
		    error);
		goto out;
	}

	if (error = kstr_push(vp, ARP)) {
		printf("strplumb: kstr_push ARP failed, error %d\n", error);
		goto out;
	}

	/*
	 * Don't get the interface flags, because ARP doesn't understand
	 * or need them.
	 */
	if (error = setifname(vp, ifname, unit, B_FALSE)) {
		printf("strplumb: setifname ARP failed, error %d\n", error);
		goto out;
	}
	if (error = kstr_plink(nvp, fd, &arp_muxid)) {
		printf("strplumb: kstr_plink IP/<net> failed, error %d\n",
		    error);
		goto out;
	}

	if (error = setifmuxid(nvp, ifname, arp_muxid, ip_muxid)) {
		printf("strplumb: setifmuxid failed, error %d\n", error);
		goto out;
	}

	if (strplumbdebug) {
		printf("strplumb: setifmuxid for %s, arp_muxid = %d, ip_muxid"
		    " = %d\n", ifname, arp_muxid, ip_muxid);
	}

out:
	/* Close vnodes: fd is associated with vp */
	if (fd != -1)
		(void) kstr_close(NULL, fd);
	if (nvp)
		(void) kstr_close(nvp, -1);
}

/*
 * Can be set thru /etc/system file in the
 * case of local booting. In the case of
 * diskless booting it is too late by the
 * time this function is called to get the
 * specified driver loaded in.
 */
char	*ndev_name = 0;
int	ndev_unit = 0;

static int
resolve_netdrv(int *unitp, major_t *ndev)
{
	/*
	 * If we booted diskless then strplumb() will
	 * have been called from rootconf(). All we
	 * can do in that case is plumb the network
	 * device that we booted from.
	 *
	 * If we booted from a local disk, we will have
	 * been called from main(), and normally we defer the
	 * plumbing of interfaces until /etc/rcS.d/S30network.
	 * This can be overridden by setting "ndev_name" in /etc/system.
	 */
	if ((strncmp(backfs.bo_fstype, "nfs", 3) == 0) ||
	    (strncmp(rootfs.bo_fstype, "nfs", 3) == 0)) {
		dev_t		dev;
		minor_t		unit;
		char		*devname;

		if (strncmp(rootfs.bo_fstype, "nfs", 3) == 0)
			devname = rootfs.bo_name;
		else
			devname = backfs.bo_name;
		dev = ddi_pathname_to_dev_t(devname);
		if (dev == (dev_t)-1) {
			cmn_err(CE_CONT, "Cannot assemble drivers for %s\n",
				devname);
			return (ENXIO);
		}
#if defined(i386)
		unit = 0;
#else
		unit = getminor(dev);
#endif
		*ndev = getmajor(dev);

		if (strplumbdebug)
			printf("strplumb: network device maj %d, unit %d\n",
			    (int)*ndev, (int)unit);

		*unitp = unit;
	} else {
		if (strplumbdebug) {
			if (ndev_name != (char *)NULL)
				printf("strplumb: ndev_name %s\n", ndev_name);
			printf("strplumb: ndev_unit %d\n", ndev_unit);
		}

		if (ndev_name != (char *)NULL) {
			if (ddi_install_driver(ndev_name) != DDI_SUCCESS) {
				printf("strplumb: Can't install drv/%s\n",
				    ndev_name);
				return (ENXIO);
			}
		}

		if (ndev_name == (char *)NULL) {
			return (0);	/* May be acceptable */
		}

		*ndev = ddi_name_to_major(ndev_name);
		if (strplumbdebug)
			printf("strplumb: non-nfs: maj %d, unit %d\n",
			    (int)*ndev, ndev_unit);

		*unitp = ndev_unit;
	}
	return (0);
}

/*
 * getifname() runs down the queues and fills in the interface name for
 * the device.  It writes the name in 'ifname' which the caller ensures
 * has enough space (LIFNAMSIZ).
 */
static void
getifname(vnode_t *vp, int unit, char *ifname)
{
	queue_t	*q;

	ASSERT(vp != NULL && vp->v_stream != NULL &&
	    vp->v_stream->sd_wrq != NULL);

	for (q = vp->v_stream->sd_wrq; q->q_next != NULL; q = q->q_next)
		;
	(void) sprintf(ifname, "%s%d", q->q_qinfo->qi_minfo->mi_idname, unit);

	if (strplumbdebug)
		printf("strplumb: got interface <%s>\n", ifname);
}

/*
 * Initialize and set the interface to IPv4.  This code needs follow the
 * logic of ifconfig(1m) closely.  IP has special logic to deal with an
 * SIOCGLIFFLAGS before the interface name is set (getflags), ARP will
 * just get confused (!getflags).
 */
static int
setifname(vnode_t *vp, char *ifname, int unit, boolean_t getflags)
{
	struct strioctl	iocb;
	struct lifreq	lifr;
	int		ret;

	bzero(&lifr, sizeof (struct lifreq));

	if (getflags) {
		/*
		 * Get the existing flags for this stream - note that
		 * lifr_name[0] is '\0' from the above.
		 */
		iocb.ic_cmd = SIOCGLIFFLAGS;
		iocb.ic_timout = 15;
		iocb.ic_len = sizeof (struct lifreq);
		iocb.ic_dp = (char *)&lifr;
		if ((ret = kstr_ioctl(vp, I_STR, (intptr_t)&iocb)) != 0)
			return (ret);

		/* strplumb is always plumbing a v4 interface */
		lifr.lifr_flags |= IFF_IPV4;
	}

	/* Set the name string and the ppa */
	lifr.lifr_ppa = unit;
	bcopy(ifname, lifr.lifr_name, strlen(ifname));

	iocb.ic_cmd = SIOCSLIFNAME;
	iocb.ic_timout = 15;
	iocb.ic_len = sizeof (struct lifreq);
	iocb.ic_dp = (char *)&lifr;

	return (kstr_ioctl(vp, I_STR, (intptr_t)&iocb));
}

static int
setifmuxid(vnode_t *vp, const char *ifname, int arp_muxid, int ip_muxid)
{
	struct ifreq	ifr;

	(void) bzero(&ifr, sizeof (ifr));

	ifr.ifr_arp_muxid = arp_muxid;
	ifr.ifr_ip_muxid = ip_muxid;
	(void) strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	return (kstr_ioctl(vp, SIOCSIFMUXID, (intptr_t)&ifr));
}
