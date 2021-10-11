/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)in.rarpd.c	1.22	98/04/24 SMI"

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1996-1998 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 *
 */


/*
 * rarpd.c  Reverse-ARP server.
 * Refer to RFC 903 "A Reverse Address Resolution Protocol".
 */

#define	_REENTRANT

#include	<stdlib.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<sys/sockio.h>
#include	<net/if.h>
#include	<sys/ethernet.h>
#include	<netinet/arp.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<stropts.h>
#include	<sys/dlpi.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	<string.h>
#include	<ctype.h>
#include	<fcntl.h>
#include	<syslog.h>
#include	<dirent.h>
#include	<signal.h>
#include	<netdb.h>
#include	<errno.h>
#include	<thread.h>
#include	<synch.h>

#define	BOOTDIR		"/tftpboot"	/* boot files directory */
#define	DEVDIR		"/dev"		/* devices directory */
#define	DEVIP		"/dev/ip"	/* path to ip driver */
#define	DEVARP		"/dev/arp"	/* path to arp driver */

#define	BUFSIZE		2048		/* max receive frame length */
#define	MAXPATHL	128		/* max path length */
#define	MAXHOSTL	128		/* max host name length */
#define	MAXIFS		256

/*
 * XXX
 * DLPI Provider Address Format assumed.
 */
struct	dladdr {
	struct	ether_addr	dl_phys;
	ushort_t	dl_sap;
};

/*
 * Logical network devices
 */
struct	ifdev {
	char		ldevice[IFNAMSIZ];
	int		lunit;
	uint32_t	ipaddr;			/* network order */
	uint32_t	if_netmask;		/* host order */
	uint32_t	if_ipaddr;		/* host order */
	uint32_t	if_netnum;		/* host order, with subnet */
	struct ifdev *next;
};

/*
 * Physical network device
 */
struct	rarpdev {
	char		device[IFNAMSIZ];
	int		unit;
	int		fd;
	struct ether_addr etheraddr;
	struct ifdev	*ifdev;
	struct rarpdev	*next;
};

struct	rarpreply {
	struct rarpdev		*rdev;
	struct timeval		tv;
	struct ether_addr	dest;
	struct ether_arp	earp;
	struct rarpreply	*next;
};

static struct rarpreply	*delay_list;
static sema_t		delay_sema;
static mutex_t		delay_mutex;
static mutex_t		debug_mutex;

static struct rarpdev	*rarpdev_head;

#define	IPADDRL		sizeof (struct in_addr)

/*
 * Missing from header files
 */
extern char	*ether_ntoa(struct ether_addr *);
extern int	ether_ntohost(char *, struct ether_addr *);

/*
 * Globals initialized before multi-threading
 */
static char	*cmdname;		/* command name from argv[0] */
static int	dflag = 0;		/* enable diagnostics */
static int	aflag = 0;		/* start rarpd on all interfaces */
static char	*alarmmsg;		/* alarm() error message */
static long	pc_name_max;		/* pathconf maximum path name */

static void	getintf(void);
static void	getdevice(char *, char *, char *);
static void	getunit(char *, int *, int *);
static struct rarpdev *find_device(char *);
static void	init_rarpdev(struct rarpdev *);
static void	do_rarp(void *);
static void	rarp_request(struct rarpdev *, struct ether_arp *,
							struct ether_addr *);
					/* RARP request handler */
static void	add_arp(char *, struct ether_addr *);
static void	arp_request(struct rarpdev *, struct ether_arp *,
							struct ether_addr *);
					/* ARP request handler */
static int	rarp_open(char *, int, ushort_t, struct ether_addr *);
static void	do_delay_write(void *);
static void	delay_write(struct rarpdev *, struct rarpreply *);
static int	rarp_write(int, struct rarpreply *);
static int	mightboot(uint32_t);
static void	get_ifdata(char *, int, uint32_t *, uint32_t *);
static int	get_ipaddr(struct rarpdev *, struct ether_addr, char *,
								uint32_t *);
static void	sigalarm(int);
static int	strioctl(int, int, int, int, char *);
static void	usage();
static void	syserr(char *);
static void	error(char *, ...);
static void	debug(char *, ...);

extern	int	optind;
extern	char	*optarg;

main(int argc, char *argv[])
{
	int		c;
	struct rarpdev	*rdev;
	int		i;

	cmdname = argv[0];

	while ((c = getopt(argc, argv, "ad")) != -1) {
		switch (c) {
		case 'a':
			aflag = 1;
			break;

		case 'd':
			dflag = 1;
			break;

		default:
			usage();
		}
	}

	if ((!aflag && (argc - optind) != 2) ||
					(aflag && (argc - optind) != 0)) {
		usage();
		/* NOTREACHED */
	}

	if (!dflag) {
		/*
		 * Background
		 */
		switch (fork()) {
			case -1:	/* error */
				syserr("fork");
				/*NOTREACHED*/

			case 0:		/* child */
				break;

			default:	/* parent */
				return (0);
		}
		for (i = 0; i < 3; i++) {
			(void) close(i);
		}
		(void) open("/", O_RDONLY, 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);
		/*
		 * Detach terminal
		 */
		if (setsid() < 0)
			syserr("setsid");
	}

	/*
	 * Look up the maximum name length of the BOOTDIR, it may not
	 * exist so use /, if that fails use a reasonable sized buffer.
	 */
	if ((pc_name_max = pathconf(BOOTDIR, _PC_NAME_MAX)) == -1) {
		if ((pc_name_max = pathconf("/", _PC_NAME_MAX)) == -1) {
			pc_name_max = 255;
		}
	}

	(void) openlog(cmdname, LOG_PID, LOG_DAEMON);

	if (aflag) {
		/*
		 * Get each interface name and load rarpdev list
		 */
		getintf();
	} else {
		char buf[IFNAMSIZ];
		struct ifdev *ifdev;

		/*
		 * Load specified device as only element of the list
		 */
		rarpdev_head = (struct rarpdev *)calloc(1,
						sizeof (struct rarpdev));
		if (rarpdev_head == NULL) {
			error("out of memory");
		}
		(void) strncpy(buf, argv[optind], IFNAMSIZ);
		(void) strncat(buf, argv[optind + 1], IFNAMSIZ - strlen(buf));

		if ((ifdev = calloc(1, sizeof (struct ifdev))) == NULL) {
			error("out of memory");
		}

		getdevice(buf, rarpdev_head->device, ifdev->ldevice);
		getunit(buf, &rarpdev_head->unit, &ifdev->lunit);
		ifdev->next = rarpdev_head->ifdev;
		rarpdev_head->ifdev = ifdev;
	}

	/*
	 * Initialize each rarpdev
	 */
	for (rdev = rarpdev_head; rdev != NULL; rdev = rdev->next) {
		init_rarpdev(rdev);
	}

	(void) sema_init(&delay_sema, 0, USYNC_THREAD, NULL);
	(void) mutex_init(&delay_mutex, USYNC_THREAD, NULL);
	(void) mutex_init(&debug_mutex, USYNC_THREAD, NULL);

	/*
	 * Start delayed processing thread
	 */
	thr_create(NULL, NULL, (void *(*)(void *))do_delay_write, NULL,
							THR_NEW_LWP, NULL);

	/*
	 * Start RARP processing for each device
	 */
	for (rdev = rarpdev_head; rdev != NULL; rdev = rdev->next) {
		if (rdev->fd != -1) {
			thr_create(NULL, NULL, (void *(*)(void *))do_rarp,
					(void *)rdev, THR_NEW_LWP, NULL);
		}
	}

	/*
	 * Exit main() thread
	 */
	thr_exit(NULL);

	return (0);
}

static void
getintf(void)
{
	int		fd;
	int		numifs;
	unsigned	bufsize;
	struct ifreq	*reqbuf;
	struct ifconf	ifconf;
	struct ifreq	*ifr;
	struct rarpdev	*rdev;
	struct ifdev	*ifdev;

	/*
	 * Open the IP provider.
	 */
	if ((fd = open(DEVIP, 0)) < 0)
		syserr(DEVIP);

	/*
	 * Ask IP for the list of configured interfaces.
	 */
	if (ioctl(fd, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
	}
	bufsize = numifs * sizeof (struct ifreq);
	reqbuf = (struct ifreq *)malloc(bufsize);
	if (reqbuf == NULL) {
		error("out of memory");
	}

	ifconf.ifc_len = bufsize;
	ifconf.ifc_buf = (caddr_t)reqbuf;
	if (ioctl(fd, SIOCGIFCONF, (char *)&ifconf) < 0)
		syserr("SIOCGIFCONF");

	/*
	 * Initialize a rarpdev for each interface
	 */
	for (ifr = ifconf.ifc_req; ifconf.ifc_len > 0;
			ifr++, ifconf.ifc_len -= sizeof (struct ifreq)) {
		if (ioctl(fd, SIOCGIFFLAGS, (char *)ifr) < 0) {
			syserr("ioctl SIOCGIFFLAGS");
			exit(1);
		}
		if ((ifr->ifr_flags & IFF_LOOPBACK) ||
		    !(ifr->ifr_flags & IFF_UP) ||
		    !(ifr->ifr_flags & IFF_BROADCAST) ||
		    (ifr->ifr_flags & IFF_NOARP) ||
		    (ifr->ifr_flags & IFF_POINTOPOINT))
			continue;

		/*
		 * Look for an existing device for logical interfaces
		 */
		if ((rdev = find_device(ifr->ifr_name)) == NULL) {
			if ((rdev = calloc(1, sizeof (struct rarpdev))) ==
									NULL) {
				error("out of memory");
			}

			getdevice(ifr->ifr_name, rdev->device, NULL);
			getunit(ifr->ifr_name, &rdev->unit, NULL);
			rdev->next = rarpdev_head;
			rarpdev_head = rdev;
		}

		if ((ifdev = calloc(1, sizeof (struct ifdev))) == NULL) {
			error("out of memory");
		}

		getdevice(ifr->ifr_name, NULL, ifdev->ldevice);
		getunit(ifr->ifr_name, NULL, &ifdev->lunit);
		ifdev->next = rdev->ifdev;
		rdev->ifdev = ifdev;
	}
	(void) free((char *)reqbuf);
}

/*
 * Pick out leading alphabetic part of string 's'.
 * If this is a logical interface, split the device at the ':'
 * and leave it on the ldevice.  This allows the concatenation of
 * ldevice and lunit to produce the full name "le0:1".
 */
static void
getdevice(char *s, char *device, char *ldevice)
{
	char *cp;

	if (ldevice != NULL) {
		/*
		 * Handle logical interfaces
		 */
		(void) strcpy((char *)ldevice, s);
		if ((cp = strrchr((char *)ldevice, ':')) != NULL) {
			*++cp = '\0';
		} else {
			ldevice[0] = '\0';
		}
	}

	if (device != NULL) {
		cp = device;
		while (isalpha(*s)) {
			*cp++ = *s++;
		}
		*cp = '\0';
	}
}

/*
 * Pick out trailing numeric part of string 's' and return int.
 */
static void
getunit(char *s, int *unit, int *lunit)
{
	char	intbuf[IFNAMSIZ];
	char	*p = intbuf;
	char	*cp;

	if (lunit != NULL) {
		*lunit = -1;
		if ((cp = strrchr(s, ':')) != NULL) {
			(void) strcpy(intbuf, ++cp);
			*lunit = atoi(intbuf);
		}
	}

	if (unit != NULL) {
		while (isalpha(*s) || *s == ':')
			s++;
		while (isdigit(*s))
			*p++ = *s++;
		*p = '\0';

		*unit = atoi(intbuf);
	}
}

static struct rarpdev *
find_device(char *name)
{
	struct rarpdev *rdev;
	char	buf[IFNAMSIZ];
	int	unit;

	getdevice(name, buf, NULL);
	getunit(name, &unit, NULL);

	for (rdev = rarpdev_head; rdev != NULL; rdev = rdev->next) {
		if (unit == rdev->unit && strcmp(buf, rdev->device) == 0)
			return (rdev);
	}
	return (NULL);
}

static void
init_rarpdev(struct rarpdev *rdev)
{
	char *dev;
	int unit;
	struct ifdev *ifdev;

	/*
	 * Open datalink provider and get our ethernet address.
	 */
	rdev->fd = rarp_open(rdev->device, rdev->unit, ETHERTYPE_REVARP,
		&rdev->etheraddr);

	/*
	 * rarp_open may fail on certain types of interfaces
	 */
	if (rdev->fd < 0) {
		rdev->fd = -1;
		return;
	}

	/*
	 * Get the IP address and netmask from directory service for
	 * each logical interface.
	 */
	for (ifdev = rdev->ifdev; ifdev != NULL; ifdev = ifdev->next) {
		/*
		 * If lunit == -1 then this is the primary interface name
		 */
		if (ifdev->lunit == -1) {
			dev = rdev->device;
			unit = rdev->unit;
		} else {
			dev = ifdev->ldevice;
			unit = ifdev->lunit;
		}
		get_ifdata(dev, unit, &ifdev->if_ipaddr, &ifdev->if_netmask);

		/*
		 * Use IP address of the interface.
		 */
		ifdev->if_netnum = ifdev->if_ipaddr & ifdev->if_netmask;
		ifdev->ipaddr = (uint32_t)htonl(ifdev->if_ipaddr);
	}
}

static void
do_rarp(void *buf)
{
	struct rarpdev *rdev = (struct rarpdev *)buf;
	struct strbuf ctl;
	char	ctlbuf[BUFSIZE];
	struct strbuf data;
	char	databuf[BUFSIZE];
	char	*cause;
	struct	ether_arp	ans;
	struct ether_addr	shost;
	int	flags;
	union	DL_primitives	*dlp;
	struct	dladdr	*dladdrp;

	if (dflag) {
		debug("starting rarp service on device %s%d address %s",
				rdev->device, rdev->unit,
				ether_ntoa(&rdev->etheraddr));
	}

	/*
	 * read RARP packets and respond to them.
	 */
	/*CONSTCOND*/
	while (1) {
		ctl.len = 0;
		ctl.maxlen = BUFSIZE;
		ctl.buf = ctlbuf;
		data.len = 0;
		data.maxlen = BUFSIZ;
		data.buf = databuf;
		flags = 0;

		if (getmsg(rdev->fd, &ctl, &data, &flags) < 0)
			syserr("getmsg");

		/*
		 * Validate DL_UNITDATA_IND.
		 */
		/* LINTED pointer */
		dlp = (union DL_primitives *)ctlbuf;

		(void) memcpy(&ans, databuf, sizeof (struct ether_arp));

		cause = NULL;
		if (ctl.len == 0)
			cause = "missing control part of message";
		else if (ctl.len < 0)
			cause = "short control part of message";
		else if (dlp->dl_primitive != DL_UNITDATA_IND)
			cause = "not unitdata_ind";
		else if (flags & MORECTL)
			cause = "MORECTL flag";
		else if (flags & MOREDATA)
			cause = "MOREDATA flag";
		else if (ctl.len < DL_UNITDATA_IND_SIZE)
			cause = "short unitdata_ind";
		else if (data.len < sizeof (struct ether_arp))
			cause = "short ether_arp";
		else if (ans.arp_hrd != htons(ARPHRD_ETHER))
			cause = "hrd";
		else if (ans.arp_pro != htons(ETHERTYPE_IP))
			cause = "pro";
		else if (ans.arp_hln != ETHERADDRL)
			cause = "hln";
		else if (ans.arp_pln != IPADDRL)
			cause = "pln";
		if (cause) {
			if (dflag)
				debug("receive check failed: cause: %s",
					cause);
			continue;
		}

		/*
		 * Good request.
		 * Pick out the ethernet source address of this RARP request.
		 */
		/* LINTED pointer */
		dladdrp = (struct dladdr *)((char *)ctlbuf +
			dlp->unitdata_ind.dl_src_addr_offset);
		(void) memcpy(&shost, &dladdrp->dl_phys, ETHERADDRL);


		/*
		 * Handle the request.
		 */
		switch (ntohs(ans.arp_op)) {
		case REVARP_REQUEST:
			rarp_request(rdev, &ans, &shost);
			break;

		case ARPOP_REQUEST:
			arp_request(rdev, &ans, &shost);
			break;

		case REVARP_REPLY:
			if (dflag)
				debug("REVARP_REPLY ignored");
			break;

		case ARPOP_REPLY:
			if (dflag)
				debug("ARPOP_REPLY ignored");
			break;

		default:
			if (dflag)
				debug("unknown opcode 0x%x", ans.arp_op);
			break;
		}
	}
	/* NOTREACHED */
}

/*
 * Reverse address determination and allocation code.
 */
static void
rarp_request(struct rarpdev *rdev, struct ether_arp *rp,
						struct ether_addr *shostp)
{
	ulong_t			tpa;
	struct	rarpreply	*rrp;

	if (dflag) {
		debug("RARP_REQUEST for %s",
			ether_ntoa(&rp->arp_tha));
	}

	/*
	 * third party lookups are rare and wonderful
	 */
	if (memcmp((char *)&rp->arp_sha, (char *)&rp->arp_tha, ETHERADDRL) ||
	    memcmp((char *)&rp->arp_sha, (char *)shostp, ETHERADDRL)) {
		if (dflag)
			debug("weird (3rd party lookup)");
	}

	/*
	 * fill in given parts of reply packet
	 */
	(void) memcpy(&rp->arp_sha, &rdev->etheraddr, ETHERADDRL);

	/*
	 * If a good address is stored in our lookup tables, return it
	 * immediately or after a delay.  Store it our kernel's ARP cache.
	 */
	if (get_ipaddr(rdev, rp->arp_tha, (char *)rp->arp_tpa,
						(uint32_t *)rp->arp_spa)) {
		return;
	}

	add_arp((char *)rp->arp_tpa, &rp->arp_tha);

	rp->arp_op = htons(REVARP_REPLY);

	if (dflag) {
		struct in_addr addr;

		(void) memcpy(&addr, rp->arp_tpa, IPADDRL);
		debug("good lookup, maps to %s", inet_ntoa(addr));
	}

	rrp = (struct rarpreply *)calloc(1, sizeof (struct rarpreply));
	if (rrp == NULL) {
		error("out of memory");
	}

	/*
	 * Create rarpreply structure.
	 */
	(void) gettimeofday(&rrp->tv, NULL);
	rrp->tv.tv_sec += 3;	/* delay */
	rrp->rdev = rdev;
	(void) memcpy(&rrp->dest, shostp, ETHERADDRL);
	(void) memcpy(&rrp->earp, rp, sizeof (struct ether_arp));

	/*
	 * If this is diskless and we're not its bootserver, let the
	 * bootserver reply first by delaying a while.
	 */
	(void) memcpy(&tpa, rp->arp_tpa, IPADDRL);
	if (mightboot(ntohl(tpa))) {
		if (rarp_write(rdev->fd, rrp) < 0)
			syslog(LOG_ERR, "Bad rarp_write:  %m");
		if (dflag)
			debug("immediate reply sent");
		(void) free(rrp);
	} else {
		delay_write(rdev, rrp);
	}
}

/*
 * Download an ARP entry into our kernel.
 */
static void
add_arp(char *ip, struct ether_addr *eap)
{
	struct arpreq ar;
	struct sockaddr_in	*sin;
	int	fd;

	/*
	 * Common part of query or set
	 */
	(void) memset(&ar, sizeof (ar), '\0');
	ar.arp_pa.sa_family = AF_INET;
	/* LINTED pointer */
	sin = (struct sockaddr_in *)&ar.arp_pa;
	(void) memcpy(&sin->sin_addr, ip, IPADDRL);

	/*
	 * Open the IP provider.
	 */
	if ((fd = open(DEVARP, 0)) < 0)
		syserr(DEVARP);

	/*
	 * Set the entry
	 */
	(void) memcpy(ar.arp_ha.sa_data, eap, ETHERADDRL);
	ar.arp_flags = 0;
	(void) strioctl(fd, SIOCDARP, -1, sizeof (struct arpreq), (char *)&ar);
	if (strioctl(fd, SIOCSARP, -1, sizeof (struct arpreq),
							(char *)&ar) < 0)
		syserr("SIOCSARP");

	(void) close(fd);
}

/*
 * The RARP spec says we must be able to process ARP requests,
 * even through the packet type is RARP.  Let's hope this feature
 * is not heavily used.
 */
static void
arp_request(struct rarpdev *rdev, struct ether_arp *rp,
						struct ether_addr *shostp)
{
	struct	rarpreply	rr;
	struct ifdev		*ifdev;

	if (dflag)
		debug("ARPOP_REQUEST");

	for (ifdev = rdev->ifdev; ifdev != NULL; ifdev = ifdev->next) {
		if (memcmp((char *)&ifdev->ipaddr, (char *)rp->arp_tpa,
								IPADDRL) == 0)
			break;
	}
	if (ifdev == NULL)
		return;

	rp->arp_op = ARPOP_REPLY;
	(void) memcpy(&rp->arp_sha, &rdev->etheraddr, ETHERADDRL);
	(void) memcpy(rp->arp_spa, &ifdev->ipaddr, IPADDRL);
	(void) memcpy(&rp->arp_tha, &rdev->etheraddr, ETHERADDRL);

	add_arp((char *)rp->arp_tpa, &rp->arp_tha);

	/*
	 * Create rarp reply structure.
	 */
	(void) memcpy(&rr.dest, shostp, ETHERADDRL);
	(void) memcpy(&rr.earp, rp, sizeof (struct ether_arp));

	if (rarp_write(rdev->fd, &rr) < 0)
		error("rarp_write error");
}

/*
 * OPEN the datalink provider device, ATTACH to the unit,
 * and BIND to the revarp type.
 * Return the resulting descriptor.
 *
 * MT-UNSAFE
 */
static int
rarp_open(char *device, int unit, ushort_t type, struct ether_addr *etherp)
{
	register int fd;
	char	path[MAXPATHL];
	union DL_primitives *dlp;
	char	buf[BUFSIZE];
	struct	strbuf	ctl;
	int	flags;
	struct	ether_addr	*eap;

	/*
	 * Prefix the device name with "/dev/" if it doesn't
	 * start with a "/" .
	 */
	if (*device == '/')
		(void) sprintf(path, "%s", device);
	else
		(void) sprintf(path, "%s/%s", DEVDIR, device);

	/*
	 * Open the datalink provider.
	 */
	if ((fd = open(path, O_RDWR)) < 0)
		syserr(path);

	/*
	 * Issue DL_INFO_REQ and check DL_INFO_ACK for sanity.
	 */
	/* LINTED pointer */
	dlp = (union DL_primitives *)buf;
	dlp->info_req.dl_primitive = DL_INFO_REQ;

	ctl.buf = (char *)dlp;
	ctl.len = DL_INFO_REQ_SIZE;

	if (putmsg(fd, &ctl, NULL, 0) < 0)
		syserr("putmsg");

	(void) signal(SIGALRM, sigalarm);

	alarmmsg = "DL_INFO_REQ failed: timeout waiting for DL_INFO_ACK";
	(void) alarm(10);

	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;
	if (getmsg(fd, &ctl, NULL, &flags) < 0)
		syserr("getmsg");

	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	/*
	 * Validate DL_INFO_ACK reply.
	 */
	if (ctl.len < sizeof (ulong_t))
		error("DL_INFO_REQ failed:  short reply to DL_INFO_REQ");

	if (dlp->dl_primitive != DL_INFO_ACK)
		error("DL_INFO_REQ failed:  dl_primitive 0x%x received",
			dlp->dl_primitive);

	if (ctl.len < DL_INFO_ACK)
		error("DL_INFO_REQ failed:  short info_ack:  %d bytes",
			ctl.len);

	if (dlp->info_ack.dl_version != DL_VERSION_2)
		error("DL_INFO_ACK:  incompatible version:  %d",
			dlp->info_ack.dl_version);

	if (dlp->info_ack.dl_sap_length != -2) {
		if (dflag)
			debug(
"%s%d DL_INFO_ACK:  incompatible dl_sap_length:  %d",
				device, unit, dlp->info_ack.dl_sap_length);
		(void) close(fd);
		return (-1);
	}

	if ((dlp->info_ack.dl_service_mode & DL_CLDLS) == 0) {
		if (dflag)
			debug(
"%s%d DL_INFO_ACK:  incompatible dl_service_mode:  0x%x",
				device, unit, dlp->info_ack.dl_service_mode);
		(void) close(fd);
		return (-1);
	}

	/*
	 * Issue DL_ATTACH_REQ.
	 */
	/* LINTED pointer */
	dlp = (union DL_primitives *)buf;
	dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
	dlp->attach_req.dl_ppa = unit;

	ctl.buf = (char *)dlp;
	ctl.len = DL_ATTACH_REQ_SIZE;

	if (putmsg(fd, &ctl, NULL, 0) < 0)
		syserr("putmsg");

	(void) signal(SIGALRM, sigalarm);
	alarmmsg = "DL_ATTACH_REQ failed: timeout waiting for DL_OK_ACK";

	(void) alarm(10);

	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;
	if (getmsg(fd, &ctl, NULL, &flags) < 0)
		syserr("getmsg");

	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	/*
	 * Validate DL_OK_ACK reply.
	 */
	if (ctl.len < sizeof (ulong_t))
		error("DL_ATTACH_REQ failed:  short reply to attach request");

	if (dlp->dl_primitive == DL_ERROR_ACK)
		error("DL_ATTACH_REQ failed:  dl_errno %d unix_errno %d",
			dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);

	if (dlp->dl_primitive != DL_OK_ACK)
		error("DL_ATTACH_REQ failed:  dl_primitive 0x%x received",
			dlp->dl_primitive);

	if (ctl.len < DL_OK_ACK_SIZE)
		error("attach failed:  short ok_ack:  %d bytes",
			ctl.len);

	/*
	 * Issue DL_BIND_REQ.
	 */
	/* LINTED pointer */
	dlp = (union DL_primitives *)buf;
	dlp->bind_req.dl_primitive = DL_BIND_REQ;
	dlp->bind_req.dl_sap = type;
	dlp->bind_req.dl_max_conind = 0;
	dlp->bind_req.dl_service_mode = DL_CLDLS;
	dlp->bind_req.dl_conn_mgmt = 0;
	dlp->bind_req.dl_xidtest_flg = 0;

	ctl.buf = (char *)dlp;
	ctl.len = DL_BIND_REQ_SIZE;

	if (putmsg(fd, &ctl, NULL, 0) < 0)
		syserr("putmsg");

	(void) signal(SIGALRM, sigalarm);

	alarmmsg = "DL_BIND_REQ failed:  timeout waiting for DL_BIND_ACK";
	(void) alarm(10);

	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;
	if (getmsg(fd, &ctl, NULL, &flags) < 0)
		syserr("getmsg");

	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	/*
	 * Validate DL_BIND_ACK reply.
	 */
	if (ctl.len < sizeof (ulong_t))
		error("DL_BIND_REQ failed:  short reply");

	if (dlp->dl_primitive == DL_ERROR_ACK)
		error("DL_BIND_REQ failed:  dl_errno %d unix_errno %d",
			dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);

	if (dlp->dl_primitive != DL_BIND_ACK)
		error("DL_BIND_REQ failed:  dl_primitive 0x%x received",
			dlp->dl_primitive);

	if (ctl.len < DL_BIND_ACK_SIZE)
		error(
"DL_BIND_REQ failed:  short bind acknowledgement received");

	if (dlp->bind_ack.dl_sap != type)
		error(
"DL_BIND_REQ failed:  returned dl_sap %d != requested sap %d",
			dlp->bind_ack.dl_sap, type);

	/*
	 * Issue DL_PHYS_ADDR_REQ to get our local ethernet address.
	 */
	/* LINTED pointer */
	dlp = (union DL_primitives *)buf;
	dlp->physaddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	dlp->physaddr_req.dl_addr_type = DL_CURR_PHYS_ADDR;

	ctl.buf = (char *)dlp;
	ctl.len = DL_PHYS_ADDR_REQ_SIZE;

	if (putmsg(fd, &ctl, NULL, 0) < 0)
		syserr("putmsg");

	(void) signal(SIGALRM, sigalarm);

	alarmmsg =
	    "DL_PHYS_ADDR_REQ failed:  timeout waiting for DL_PHYS_ADDR_ACK";
	(void) alarm(10);

	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;
	if (getmsg(fd, &ctl, NULL, &flags) < 0)
		syserr("getmsg");

	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);

	/*
	 * Validate DL_PHYS_ADDR_ACK reply.
	 */
	if (ctl.len < sizeof (ulong_t))
		error("DL_PHYS_ADDR_REQ failed:  short reply");

	if (dlp->dl_primitive == DL_ERROR_ACK)
		error("DL_PHYS_ADDR_REQ failed:  dl_errno %d unix_errno %d",
			dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);

	if (dlp->dl_primitive != DL_PHYS_ADDR_ACK)
		error("DL_PHYS_ADDR_REQ failed:  dl_primitive 0x%x received",
			dlp->dl_primitive);

	if (ctl.len < DL_PHYS_ADDR_ACK_SIZE)
		error("DL_PHYS_ADDR_REQ failed:  short ack received");

	if (dlp->physaddr_ack.dl_addr_length != ETHERADDRL) {
		if (dflag)
			debug(
"%s%d DL_PHYS_ADDR_ACK failed:  incompatible dl_addr_length:  %d",
			device, unit, dlp->physaddr_ack.dl_addr_length);
		(void) close(fd);
		return (-1);
	}

	/*
	 * Save our ethernet address.
	 */
	eap = (struct ether_addr *)((char *)dlp +
		dlp->physaddr_ack.dl_addr_offset);
	(void) memcpy(etherp, eap, ETHERADDRL);

	if (dflag)
		debug("device %s%d ethernetaddress %s",
			device, unit,  ether_ntoa(etherp));

	return (fd);
}

/* ARGSUSED */
static void
do_delay_write(void *buf)
{
	struct	timeval		tv;
	struct	rarpreply	*rrp;
	int			err;

	/*CONSTCOND*/
	while (1) {
		if ((err = sema_wait(&delay_sema)) != 0) {
			if (err == EINTR)
				continue;
			error("do_delay_write: sema_wait failed");
		}

		mutex_lock(&delay_mutex);
		rrp = delay_list;
		delay_list = delay_list->next;
		mutex_unlock(&delay_mutex);

		(void) gettimeofday(&tv, NULL);
		if (tv.tv_sec < rrp->tv.tv_sec)
			(void) sleep(rrp->tv.tv_sec - tv.tv_sec);

		if (rarp_write(rrp->rdev->fd, rrp) < 0)
			error("rarp_write error");

		(void) free(rrp);
	}
	/* NOTREACHED */
}

/* ARGSUSED */
static void
delay_write(struct rarpdev *rdev, struct rarpreply *rrp)
{
	struct	rarpreply	*trp;

	mutex_lock(&delay_mutex);
	if (delay_list == NULL) {
		delay_list = rrp;
	} else {
		trp = delay_list;
		while (trp->next != NULL)
			trp = trp->next;
		trp->next = rrp;
	}
	mutex_unlock(&delay_mutex);

	(void) sema_post(&delay_sema);
}

static int
rarp_write(int fd, struct rarpreply *rrp)
{
	struct	strbuf	ctl, data;
	union	DL_primitives	*dlp;
	char	ctlbuf[BUFSIZE];
	struct	dladdr	*dladdrp;

	/*
	 * Construct DL_UNITDATA_REQ.
	 */
	/* LINTED pointer */
	dlp = (union DL_primitives *)ctlbuf;
	dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp->unitdata_req.dl_dest_addr_length = sizeof (struct dladdr);
	dlp->unitdata_req.dl_dest_addr_offset = DL_UNITDATA_REQ_SIZE;
	dlp->unitdata_req.dl_priority.dl_min = 0;
	dlp->unitdata_req.dl_priority.dl_max = 0;
	/* LINTED pointer */
	dladdrp = (struct dladdr *)(ctlbuf + DL_UNITDATA_REQ_SIZE);
	dladdrp->dl_sap = ETHERTYPE_REVARP;
	(void) memcpy(&dladdrp->dl_phys, &rrp->dest, ETHERADDRL);

	/*
	 * Send DL_UNITDATA_REQ.
	 */
	ctl.len = DL_UNITDATA_REQ_SIZE + sizeof (struct dladdr);
	ctl.buf = (char *)dlp;
	data.len = sizeof (struct ether_arp);
	data.buf = (char *)&rrp->earp;
	return (putmsg(fd, &ctl, &data, 0));
}

/*
 * See if we have a TFTP boot file for this guy. Filenames in TFTP
 * boot requests are of the form <ipaddr> for Sun-3's and of the form
 * <ipaddr>.<arch> for all other architectures.  Since we don't know
 * the client's architecture, either format will do.
 */
static int
mightboot(uint32_t ipa)
{
	char path[MAXPATHL];
	DIR *dirp;
	struct dirent *dp;
	struct dirent *dentry;

	(void) sprintf(path, "%s/%08X", BOOTDIR, ipa);

	/*
	 * Try a quick access() first.
	 */
	if (access(path, 0) == 0)
		return (1);

	/*
	 * Not there, do it the slow way by
	 * reading through the directory.
	 */
	(void) sprintf(path, "%08X", ipa);

	if (!(dirp = opendir(BOOTDIR)))
		return (0);

	dentry = (struct dirent *)malloc(sizeof (struct dirent) +
							pc_name_max + 1);
	if (dentry == NULL) {
		error("out of memory");
	}
#ifdef _POSIX_PTHREAD_SEMANTICS
	while ((readdir_r(dirp, dentry, &dp)) != 0) {
		if (dp == NULL)
			break;
#else
	while ((dp = readdir_r(dirp, dentry)) != NULL) {
#endif
		if (strncmp(dp->d_name, path, 8) != 0)
			continue;
		if ((strlen(dp->d_name) != 8) && (dp->d_name[8] != '.'))
			continue;
		break;
	}

	(void) closedir(dirp);
	(void) free(dentry);

	return (dp? 1: 0);
}

/*
 * Get our IP address and local netmask.
 */
static void
get_ifdata(char *dev, int unit, uint32_t *ipp, uint32_t *maskp)
{
	int	fd;
	struct	ifreq	ifr;
	struct	sockaddr_in	*sin;

	/* LINTED pointer */
	sin = (struct sockaddr_in *)&ifr.ifr_addr;

	/*
	 * Open the IP provider.
	 */
	if ((fd = open(DEVIP, 0)) < 0)
		syserr(DEVIP);

	/*
	 * Ask IP for our IP address.
	 */
	(void) sprintf(ifr.ifr_name, "%s%d", dev, unit);
	if (strioctl(fd, SIOCGIFADDR, -1, sizeof (struct ifreq),
		(char *)&ifr) < 0)
		syserr("SIOCGIFADDR");
	*ipp = (uint32_t)ntohl(sin->sin_addr.s_addr);

	if (dflag)
		debug("device %s%d address %s",
			dev, unit, inet_ntoa(sin->sin_addr));

	/*
	 * Ask IP for our netmask.
	 */
	if (strioctl(fd, SIOCGIFNETMASK, -1, sizeof (struct ifreq),
		(char *)&ifr) < 0)
		syserr("SIOCGIFNETMASK");
	*maskp = (uint32_t)ntohl(sin->sin_addr.s_addr);

	if (dflag)
		debug("device %s%d subnet mask %s",
			dev, unit, inet_ntoa(sin->sin_addr));

	/*
	 * Thankyou ip.
	 */
	(void) close(fd);
}

/*
 * Translate ethernet address to IP address.
 * Return 0 on success, nonzero on failure.
 */
static int
get_ipaddr(struct rarpdev *rdev, struct ether_addr e, char *ipp,
							uint32_t *ipaddr)
{
	char host[MAXHOSTL];
	char hbuffer[BUFSIZE];
	struct hostent *hp, res;
	int herror;
	struct in_addr addr;
	char	**p;
	struct ifdev *ifdev;

	/*
	 * Translate ethernet address to hostname
	 * and IP address.
	 */
	if (ether_ntohost(host, &e) != 0 ||
			!(hp = gethostbyname_r(host, &res, hbuffer,
						sizeof (hbuffer), &herror)) ||
			hp->h_addrtype != AF_INET ||
			hp->h_length != IPADDRL) {
		if (dflag)
			debug("could not map hardware address to IP address");
		return (1);
	}

	/*
	 * Find the IP address on the right net.
	 */
	for (p = hp->h_addr_list; *p; p++) {
		(void) memcpy(&addr, *p, IPADDRL);
		for (ifdev = rdev->ifdev; ifdev != NULL; ifdev = ifdev->next) {
			if (dflag) {
				struct in_addr daddr;
				uint32_t netnum;

				netnum = htonl(ifdev->if_netnum);
				(void) memcpy(&daddr, &netnum, IPADDRL);
				if (ifdev->lunit == -1)
					debug(
"trying physical netnum %s mask %x",
					inet_ntoa(daddr),
					ifdev->if_netmask);
				else
					debug(
"trying logical %d netnum %s mask %x",
					ifdev->lunit,
					inet_ntoa(daddr),
					ifdev->if_netmask);
			}
			if ((ntohl(addr.s_addr) & ifdev->if_netmask) ==
							ifdev->if_netnum) {
				/*
				 * Return the correct IP address.
				 */
				(void) memcpy(ipp, &addr, IPADDRL);

				/*
				 * Return the interface's ipaddr
				 */
				(void) memcpy(ipaddr, &ifdev->ipaddr, IPADDRL);

				return (0);
			}
		}
	}

	if (dflag)
		debug("got host entry but no IP address on this net");
	return (1);
}

/*ARGSUSED*/
void
sigalarm(int i)
{
	error(alarmmsg);
}

static int
strioctl(int fd, int cmd, int timout, int len, char *dp)
{
	struct	strioctl	si;

	si.ic_cmd = cmd;
	si.ic_timout = timout;
	si.ic_len = len;
	si.ic_dp = dp;
	return (ioctl(fd, I_STR, &si));
}

static void
usage()
{
	error("Usage:  %s [ -ad ] device unit", cmdname);
}

static void
syserr(s)
char	*s;
{
	char buf[256];
	int status = 1;

	(void) sprintf(buf, "%s: %s", s, strerror(errno));
	(void) fprintf(stderr, "%s:  %s\n", cmdname, buf);
	syslog(LOG_ERR, buf);
	thr_exit(&status);
}

/* VARARGS1 */
static void
error(char *fmt, ...)
{
	char buf[256];
	va_list ap;
	int status = 1;

	va_start(ap, fmt);
	(void) vsprintf(buf, fmt, ap);
	va_end(ap);
	(void) fprintf(stderr, "%s:  %s\n", cmdname, buf);
	syslog(LOG_ERR, buf);
	thr_exit(&status);
}

/* VARARGS1 */
static void
debug(char *fmt, ...)
{
	va_list ap;

	mutex_lock(&debug_mutex);
	va_start(ap, fmt);
	(void) fprintf(stderr, "%s:[%u]  ", cmdname, thr_self());
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
	mutex_unlock(&debug_mutex);
}
