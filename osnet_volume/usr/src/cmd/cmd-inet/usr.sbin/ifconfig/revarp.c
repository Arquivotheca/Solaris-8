/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)revarp.c	1.16	99/06/18 SMI"

#include "defs.h"
#include "ifconfig.h"
#include <sys/dlpi.h>

static struct ether_addr my_etheraddr;
static struct ether_addr etherbroadcast =
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#define	ETHERADDRL	sizeof (struct ether_addr)
#define	IPADDRL		sizeof (struct in_addr)
#define	DEVDIR		"/dev"
#define	RARPRETRIES	5
#define	RARPTIMEOUT	5
#define	DLPI_TIMEOUT	60

struct	etherdladdr {
	struct ether_addr	dl_phys;
	ushort_t		dl_sap;
};


static int rarp_timeout = RARPTIMEOUT;

/* Timeout for DLPI acks */
static int dlpi_timeout = DLPI_TIMEOUT;

static int	dlpi_info_req(int fd, dl_info_ack_t *info_ack);

static int	rarp_write(int, struct ether_arp *, struct ether_addr *);
static int	timed_getmsg(int, struct strbuf *, int *, int, char *, char *);
static int	rarp_open(char *, t_uscalar_t, struct ether_addr *);

/*
 * attempt to remove ppa from end of file name
 * return -1 if none found
 * return ppa if found and remove the ppa from the filename
 */
static int
ifrm_num(char *fname, unsigned int *ppa)
{
	int	i;
	uint_t	p = 0;
	unsigned int	m = 1;

	i = strlen(fname) - 1;
	while (i >= 0 && '0' <= fname[i] && fname[i] <= '9') {
		p += (fname[i] - '0')*m;
		m *= 10;
		i--;
	}
	if (m == 1) {
		return (-1);
	}
	fname[i + 1] = '\0';
	*ppa = p;
	return (0);
}

/*
 * Open the device defined in dev_att with the given mode starting with
 * the module indicated by mod_cnt (1 indexed).  If mod_cnt > 0, fd must
 * contain the file descriptor that modules are to be pushed on.  If
 * prerr (Print error) is set, Perror*exit() is called when an
 * error is encountered.  Otherwise,
 * returns -1 if device could not be opened, the index of
 * the module that could not be pushed or 0 on success.
 */
int
open_dev(dev_att_t *dev_att, int mode, boolean_t prerr, int *fd, int mod_cnt)
{
	int	cnt;
	int	local_fd;

	if (debug)
		(void) printf("open_dev: ifname: %s : dev %s fd %d "
		    " mod_cnt %d\n",
		    dev_att->ifname, dev_att->devname, *fd, mod_cnt);
	/*
	 * if no module count is given, try and open the device
	 */
	if (mod_cnt == 0) {
		if (debug)
			(void) printf("open_dev: opening %s\n",
			    dev_att->devname);
		if ((local_fd = open(dev_att->devname, mode)) < 0) {
			if (debug) {
				perror("open_dev: device");
				(void) printf("\n");
			}
			if (prerr) {
				Perror2_exit("Could not open",
				    dev_att->devname);
			}
			*fd = local_fd;
			return (-1);
		}
		*fd = local_fd;
		cnt = 1;
	} else {
		local_fd = *fd;
		assert(local_fd > 0);
		cnt = mod_cnt;
	}

	/*
	 * Try and push modules (if any) onto the device stream
	 */
	for (; cnt <= dev_att->mod_cnt; cnt++) {
		if (debug)
			(void) printf(" pushing: mod %s",
			    dev_att->modlist[cnt - 1]);
		if (ioctl(local_fd, I_PUSH, dev_att->modlist[cnt - 1]) == -1) {
			if (debug) {
				perror("open_dev: push");
				(void) printf("\n");
			}
			if (prerr) {
				Perror2_exit("Could not push module",
				    dev_att->modlist[cnt - 1]);
			}
			return (cnt);
		}
	}
	if (debug)
		(void) printf("\n");
	return (0);
}

/*
 * Debug routine to print out dev_att_t structure
 */
void
pf_dev_att(dev_att_t *dev_att)
{
	int cnt;

	(void) printf("\tifname: %s\n", dev_att->ifname);
	(void) printf("\t  style: %d\n", dev_att->style);
	(void) printf("\t  ppa: %d\n", dev_att->ppa);
	(void) printf("\t  mod_cnt: %d\n", dev_att->mod_cnt);
	(void) printf("\t  devname: %s\n", dev_att->devname);
	for (cnt = 0; cnt < dev_att->mod_cnt; cnt++) {
		(void) printf("\t      module: %s\n", dev_att->modlist[cnt]);
	}
}

/*
 * This function parses a '.' delimited interface name of the form
 * dev[.module[.module...]][:lun]
 * and places the device and module name into dev_att
 */
/* ARGSUSED */
static void
parse_ifname(dev_att_t *dev_att)
{
	char		*lunstr;
	char		*modlist = NULL; /* list of modules to push */
	int		cnt = 0; /* number of modules to push */
	char		modbuf[LIFNAMSIZ];
	char		*nxtmod;

	/*
	 * check for specified lun at end of interface and
	 * strip it off.
	 */
	lunstr = strchr(dev_att->ifname, ':');

	if (lunstr) {
		char *endptr;

		*lunstr = '\0';
		lunstr++;
		endptr = lunstr;
		dev_att->lun = strtoul(lunstr, &endptr, 10);

		if (endptr == lunstr || *endptr != '\0')
			Perror2_exit("Invalid logical unit number", lunstr);
	} else {
		dev_att->lun = 0;
	}

	(void) strncpy(modbuf, dev_att->ifname, LIFNAMSIZ);

	/* parse '.' delmited module list */
	modlist = strchr(modbuf, '.');
	if (modlist) {
		/* null-terminate interface name (device) */
		*modlist = '\0';
		modlist++;
		if (strlen(modlist) == 0)
			modlist = NULL;
		while (modlist && cnt < MAX_MODS) {
			nxtmod = strchr(modlist, '.');
			if (nxtmod) {
				*nxtmod = '\0';
				nxtmod++;
			}
			(void) strncpy(dev_att->modlist[cnt], modlist,
			    LIFNAMSIZ);
			cnt++;
			modlist = nxtmod;
		}
	}
	(void) snprintf(dev_att->devname, LIFNAMSIZ, "%s/%s", DEVDIR, modbuf);
	dev_att->mod_cnt = cnt;
}

/*
 * given a interface name (with possible modules to push)
 * interface name must have the format of
 * dev[ppa][.module[.module...][ppa]][:lun]
 * where only one ppa may be specified e.g. ip0.foo.tun or ip.foo.tun0
 */
int
ifname_open(char *dev_name, dev_att_t *dev_att)
{
	int		fd;
	uint_t		ppa;
	int		res;
	int		style;
	dl_info_ack_t	dl_info;
	int		mod_id;

	if (debug)
		(void) printf("ifname_open: %s\n", dev_name);

	if (strlen(dev_name) > LIFNAMSIZ - 1) {
		errno = EINVAL;
		return (-1);
	}

	/* save copy of original device name */
	(void) strncpy(dev_att->ifname, dev_name, LIFNAMSIZ);

	/* parse modules */
	parse_ifname(dev_att);

	/* try DLPI style 1 device first */

	if (debug) {
		pf_dev_att(dev_att);
	}
	mod_id = open_dev(dev_att, O_RDWR, _B_FALSE, &fd, 0);
	if (mod_id != 0) {
		if (debug) {
			(void) printf("Error on open_dev style 1 mod_id: %d"
			    " attemping style 2\n", mod_id);
			pf_dev_att(dev_att);
		}
		if (mod_id == -1) {
			res = ifrm_num(dev_att->devname, &ppa);
			mod_id = 0;
			if (res < 0) {
				if (debug)
					(void) fprintf(stderr,
					    "%s: No such file or directory\n",
					    dev_att->devname);
				(void) close(fd);
				return (-1);
			}
		/*
		 * ensure that it's the last module in the list to extract
		 * ppa
		 */
		} else if ((mod_id != dev_att->mod_cnt) ||
		    (res = ifrm_num(dev_att->modlist[dev_att->mod_cnt - 1],
		    &ppa)) < 0) {
			if (debug) {
				(void) fprintf(stderr,
				    "Error on open_dev style 2 mod_id: %d \n",
				    mod_id);
			}
			if (mod_id == dev_att->mod_cnt)
				(void) fprintf(stderr, "ifconfig: could not "
				    "locate ppa in %s\n",
				    dev_att->ifname);
			(void) close(fd);
			return (-1);
		}
		goto style2;
	}
	dev_att->style = 1;
	dev_att->ppa = 0;
	style = DL_STYLE1;
	goto dl_info_chk;
style2:
	dev_att->ppa = ppa;
	mod_id = open_dev(dev_att, O_RDWR, _B_FALSE, &fd, mod_id);
	if (mod_id != 0) {
		if (debug) {
			(void) fprintf(stderr,
			    "Error on open_dev style 2 mod_id: %d \n",
			    mod_id);
			if (mod_id > 0) {
				(void) fprintf(stderr, "%s: No such module\n",
				    dev_att->modlist[mod_id - 2]);
			}
			pf_dev_att(dev_att);
		}
		(void) close(fd);
		return (-1);
	}
	dev_att->style = 2;
	style = DL_STYLE2;
dl_info_chk:
	if (dlpi_info_req(fd, &dl_info) < 0) {
		(void) close(fd);
		pf_dev_att(dev_att);
		return (-1);
	}
	if (dl_info.dl_provider_style != style) {
		if (debug) {
			(void) fprintf(stderr, "DLPI provider style mismatch: "
			    "expected style %s got style %s (0x%lx)\n",
			    style == DL_STYLE1 ? "1" : "2",
			    dl_info.dl_provider_style == DL_STYLE1 ? "1" : "2",
			    dl_info.dl_provider_style);
		}
		(void) close(fd);
		return (-1);
	}
	if (debug) {
		(void) printf("pars_dev_att() success\n");
		pf_dev_att(dev_att);
	}
	return (fd);
}

/* ARGSUSED */
int
doifrevarp(char *ifname, struct sockaddr_in *laddr)
{
	int			if_fd;
	struct pollfd		pfd;
	int			s, flags, ret;
	char			*ctlbuf, *databuf, *cause;
	struct strbuf		ctl, data;
	struct ether_arp	req;
	struct ether_arp	ans;
	struct in_addr		from;
	struct in_addr		answer;
	union DL_primitives	*dlp;
	struct lifreq		lifr;
	int rarp_retries;	/* Number of times rarp is sent. */

	if (ifname[0] == '\0') {
		(void) fprintf(stderr, "ifconfig: doifrevarp: name not set\n");
		exit(1);
	}

	if (debug)
		(void) printf("doifrevarp interface %s\n", ifname);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		Perror0_exit("socket");
	}
	(void) strncpy(lifr.lifr_name, ifname, sizeof (lifr.lifr_name));
	if (ioctl(s, SIOCGLIFFLAGS, (char *)&lifr) < 0)
		Perror0_exit("SIOCGLIFFLAGS");

	/* don't try to revarp if we know it won't work */
	if ((lifr.lifr_flags & IFF_LOOPBACK) ||
	    (lifr.lifr_flags & IFF_NOARP) ||
	    (lifr.lifr_flags & IFF_POINTOPOINT))
		return (0);

	/* open rarp interface */
	if_fd = rarp_open(ifname, ETHERTYPE_REVARP, &my_etheraddr);
	if (if_fd < 0)
		return (0);

	/* create rarp request */
	(void) memset(&req, 0, sizeof (req));
	req.arp_hrd = htons(ARPHRD_ETHER);
	req.arp_pro = htons(ETHERTYPE_IP);
	req.arp_hln = ETHERADDRL;
	req.arp_pln = IPADDRL;
	req.arp_op = htons(REVARP_REQUEST);

	(void) memcpy(&req.arp_sha, &my_etheraddr, ETHERADDRL);
	(void) memcpy(&req.arp_tha, &my_etheraddr, ETHERADDRL);

	rarp_retries = RARPRETRIES;
rarp_retry:
	/* send the request */
	if (rarp_write(if_fd, &req, &etherbroadcast) < 0)
		return (0);

	if (debug)
		(void) printf("rarp sent\n");


	/* read the answers */
	if ((databuf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (0);
	}
	if ((ctlbuf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (0);
	}
	for (;;) {
		ctl.len = 0;
		ctl.maxlen = BUFSIZ;
		ctl.buf = ctlbuf;
		data.len = 0;
		data.maxlen = BUFSIZ;
		data.buf = databuf;
		flags = 0;

		/* start RARP reply timeout */
		pfd.fd = if_fd;
		pfd.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
		if ((ret = poll(&pfd, 1, rarp_timeout * 1000)) == 0) {
			if (--rarp_retries > 0) {
				if (debug)
					(void) printf("rarp retry\n");
				goto rarp_retry;
			} else {
				if (debug)
					(void) printf("rarp timeout\n");
				return (0);
			}
		} else if (ret == -1) {
			perror("ifconfig:  RARP reply poll");
			return (0);
		}

		/* poll returned > 0 for this fd so getmsg should not block */
		if ((ret = getmsg(if_fd, &ctl, &data, &flags)) < 0) {
			perror("ifconfig: RARP reply getmsg");
			return (0);
		}

		if (debug) {
			(void) printf("rarp: ret[%d] ctl.len[%d] data.len[%d] "
			    "flags[%d]\n", ret, ctl.len, data.len, flags);
		}
		/* Validate DL_UNITDATA_IND.  */
		/* LINTED: malloc returns a pointer aligned for any use */
		dlp = (union DL_primitives *)ctlbuf;
		if (debug) {
			(void) printf("rarp: dl_primitive[%lu]\n",
				dlp->dl_primitive);
			if (dlp->dl_primitive == DL_ERROR_ACK) {
				(void) printf(
				    "rarp: err ak: dl_errno %lu errno %lu\n",
				    dlp->error_ack.dl_errno,
				    dlp->error_ack.dl_unix_errno);
			}
			if (dlp->dl_primitive == DL_UDERROR_IND) {
				(void) printf("rarp: ud err: err[%lu] len[%lu] "
				    "off[%lu]\n",
				    dlp->uderror_ind.dl_errno,
				    dlp->uderror_ind.dl_dest_addr_length,
				    dlp->uderror_ind.dl_dest_addr_offset);
			}
		}
		(void) memcpy(&ans, databuf, sizeof (struct ether_arp));
		cause = NULL;
		if (ret & MORECTL)
			cause = "MORECTL flag";
		else if (ret & MOREDATA)
			cause = "MOREDATA flag";
		else if (ctl.len == 0)
			cause = "missing control part of message";
		else if (ctl.len < 0)
			cause = "short control part of message";
		else if (dlp->dl_primitive != DL_UNITDATA_IND)
			cause = "not unitdata_ind";
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
			(void) fprintf(stderr,
				"sanity check failed; cause: %s\n", cause);
			continue;
		}

		switch (ntohs(ans.arp_op)) {
		case ARPOP_REQUEST:
			if (debug)
				(void) printf("Got an arp request\n");
			break;

		case ARPOP_REPLY:
			if (debug)
				(void) printf("Got an arp reply.\n");
			break;

		case REVARP_REQUEST:
			if (debug)
				(void) printf("Got a rarp request.\n");
			break;

		case REVARP_REPLY:

			(void) memcpy(&answer, ans.arp_tpa, sizeof (answer));
			(void) memcpy(&from, ans.arp_spa, sizeof (from));
			if (debug) {
				(void) printf("answer: %s", inet_ntoa(answer));
				(void) printf(" [from %s]\n", inet_ntoa(from));
			}
			laddr->sin_addr = answer;
			return (1);

		default:
			(void) fprintf(stderr,
			    "ifconfig: unknown opcode 0x%xd\n", ans.arp_op);
			break;
		}
	}
	/* NOTREACHED */
}

int
dlpi_open_attach(char *ifname)
{
	int			fd;
	dev_att_t		dev_att;

	if (debug)
		(void) printf("dlpi_open_attach %s\n", ifname);

	/* if lun is specified fail (backwards compat) */
	if (strchr(ifname, ':') != NULL) {
		return (-1);
	}
	if ((fd = ifname_open(ifname, &dev_att)) < 0) {
		/* Not found */
		errno = ENXIO;
		return (-1);
	}
	if (dlpi_attach(fd, dev_att.ppa, dev_att.style) < 0) {
		(void) close(fd);
		return (-1);
	}
	return (fd);
}

int
dlpi_attach(int fd, int ppa, int style)
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	if (style != 2)
		return (0);

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}

	/* Issue DL_ATTACH_REQ */
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)buf;
	dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
	dlp->attach_req.dl_ppa = ppa;
	ctl.buf = (char *)dlp;
	ctl.len = DL_ATTACH_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;

	/* start timeout for DL_OK_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_OK_ACK", "DL_ATTACH_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		(void) printf("ok_ack: ctl.len[%d] flags[%d]\n", ctl.len,
		    flags);
	}

	/* Validate DL_OK_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr,
		    "ifconfig: attach failed: short reply to attach request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		if (debug)
			(void) fprintf(stderr,
			    "attach failed:  dl_errno %lu errno %lu\n",
			    dlp->error_ack.dl_errno,
			    dlp->error_ack.dl_unix_errno);
		free(buf);
		errno = ENXIO;
		return (-1);
	}
	if (dlp->dl_primitive != DL_OK_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: attach failed: "
		    "unrecognizable dl_primitive %lu received",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_OK_ACK_SIZE) {
		(void) fprintf(stderr,
		    "ifconfig: attach failed: "
		    "short attach acknowledgement received\n");
		free(buf);
		return (-1);
	}
	if (dlp->ok_ack.dl_correct_primitive != DL_ATTACH_REQ) {
		(void) fprintf(stderr,
		    "ifconfig: attach failed: "
		    "returned prim %lu != requested prim %lu\n",
		    dlp->ok_ack.dl_correct_primitive,
		    (t_uscalar_t)DL_ATTACH_REQ);
		free(buf);
		return (-1);
	}
	if (debug)
		(void) printf("attach done\n");

	free(buf);
	return (0);
}

int
dlpi_detach(int fd, int style)
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	if (style == 1)
		return (0);

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}

	/* Issue DL_DETACH_REQ */
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)buf;
	dlp->detach_req.dl_primitive = DL_DETACH_REQ;
	ctl.buf = (char *)dlp;
	ctl.len = DL_DETACH_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;

	/* start timeout for DL_OK_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_OK_ACK", "DL_DETACH_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		(void) printf("ok_ack: ctl.len[%d] flags[%d]\n", ctl.len,
		    flags);
	}

	/* Validate DL_OK_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr,
		    "ifconfig: detach failed: short reply to detach request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		if (debug)
			(void) fprintf(stderr,
			    "detach failed:  dl_errno %lu errno %lu\n",
			    dlp->error_ack.dl_errno,
			    dlp->error_ack.dl_unix_errno);
		free(buf);
		errno = ENXIO;
		return (-1);
	}
	if (dlp->dl_primitive != DL_OK_ACK) {
		(void) fprintf(stderr, "ifconfig: detach failed: "
		    "unrecognizable dl_primitive %lu received",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_OK_ACK_SIZE) {
		(void) fprintf(stderr, "ifconfig: detach failed: "
		    "short detach acknowledgement received\n");
		free(buf);
		return (-1);
	}
	if (dlp->ok_ack.dl_correct_primitive != DL_DETACH_REQ) {
		(void) fprintf(stderr, "ifconfig: detach failed: "
		    "returned prim %lu != requested prim %lu\n",
		    dlp->ok_ack.dl_correct_primitive,
		    (t_uscalar_t)DL_DETACH_REQ);
		free(buf);
		return (-1);
	}
	if (debug)
		(void) printf("detach done\n");

	free(buf);
	return (0);
}

static int
dlpi_bind(int fd, t_uscalar_t sap, uchar_t *eaddr)
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}
	/* Issue DL_BIND_REQ */
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)buf;
	dlp->bind_req.dl_primitive = DL_BIND_REQ;
	dlp->bind_req.dl_sap = sap;
	dlp->bind_req.dl_max_conind = 0;
	dlp->bind_req.dl_service_mode = DL_CLDLS;
	dlp->bind_req.dl_conn_mgmt = 0;
	dlp->bind_req.dl_xidtest_flg = 0;
	ctl.buf = (char *)dlp;
	ctl.len = DL_BIND_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;
	/* start timeout for DL_BIND_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout, "DL_BIND_ACK",
	    "DL_BIND_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		(void) printf("bind_ack: ctl.len[%d] flags[%d]\n", ctl.len,
		    flags);
	}

	/* Validate DL_BIND_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr,
		    "ifconfig: bind failed: short reply to bind request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: bind failed:  dl_errno %lu errno %lu\n",
		    dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_BIND_ACK) {
		(void) fprintf(stderr, "ifconfig: bind failed: "
		    "unrecognizable dl_primitive %lu received\n",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_BIND_ACK_SIZE) {
		(void) fprintf(stderr, "ifconfig: bind failed: "
		    "short bind acknowledgement received\n");
		free(buf);
		return (-1);
	}
	if (dlp->bind_ack.dl_sap != sap) {
		(void) fprintf(stderr, "ifconfig: bind failed: "
		    "returned dl_sap %lu != requested sap %lu\n",
		    dlp->bind_ack.dl_sap, sap);
		free(buf);
		return (-1);
	}
	/* copy Ethernet address */
	(void) memcpy(eaddr, &buf[dlp->bind_ack.dl_addr_offset], ETHERADDRL);

	free(buf);
	return (0);
}

static int
dlpi_get_phys(int fd, uchar_t *eaddr)
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}
	/* Issue DL_PHYS_ADDR_REQ */
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)buf;
	dlp->physaddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	dlp->physaddr_req.dl_addr_type = DL_CURR_PHYS_ADDR;
	ctl.buf = (char *)dlp;
	ctl.len = DL_PHYS_ADDR_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;

	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_PHYS_ADDR_ACK", "DL_PHYS_ADDR_REQ (DL_CURR_PHYS_ADDR)") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		(void) printf("phys_addr_ack: ctl.len[%d] flags[%d]\n", ctl.len,
		    flags);
	}

	/* Validate DL_PHYS_ADDR_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr, "ifconfig: phys_addr failed: "
		    "short reply to phys_addr request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		/*
		 * Do not print errors for DL_UNSUPPORTED and DL_NOTSUPPORTED
		 */
		if (dlp->error_ack.dl_errno != DL_UNSUPPORTED &&
		    dlp->error_ack.dl_errno != DL_NOTSUPPORTED) {
			(void) fprintf(stderr, "ifconfig: phys_addr failed: "
			    "dl_errno %lu errno %lu\n",
			    dlp->error_ack.dl_errno,
			    dlp->error_ack.dl_unix_errno);
		}
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_PHYS_ADDR_ACK) {
		(void) fprintf(stderr, "ifconfig: phys_addr failed: "
		    "unrecognizable dl_primitive %lu received\n",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_PHYS_ADDR_ACK_SIZE) {
		(void) fprintf(stderr, "ifconfig: phys_addr failed: "
		    "short phys_addr acknowledgement received\n");
		free(buf);
		return (-1);
	}
	/* Check length of address. */
	if (dlp->physaddr_ack.dl_addr_length != ETHERADDRL)
		return (-1);

	/* copy Ethernet address */
	(void) memcpy(eaddr, &buf[dlp->physaddr_ack.dl_addr_offset],
		ETHERADDRL);

	free(buf);
	return (0);
}

static int
dlpi_set_phys(int fd, uchar_t *eaddr)
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}
	/* Issue DL_SET_PHYS_ADDR_REQ */
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)buf;
	dlp->set_physaddr_req.dl_primitive = DL_SET_PHYS_ADDR_REQ;
	dlp->set_physaddr_req.dl_addr_length = ETHERADDRL;
	dlp->set_physaddr_req.dl_addr_offset = DL_SET_PHYS_ADDR_REQ_SIZE;

	/* copy Ethernet address */
	(void) memcpy(&buf[dlp->physaddr_ack.dl_addr_offset], eaddr,
		ETHERADDRL);
	ctl.buf = (char *)dlp;
	ctl.len = DL_SET_PHYS_ADDR_REQ_SIZE + ETHERADDRL;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;

	/* start timeout for DL_SET_PHYS_ADDR_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_SET_PHYS_ADDR_ACK", "DL_SET_PHYS_ADDR_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		(void) printf("set_phys_addr_ack: ctl.len[%d] flags[%d]\n",
		    ctl.len, flags);
	}

	/* Validate DL_OK_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr, "ifconfig: set_phys_addr failed: "
		    "short reply to set_phys_addr request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: set_phys_addr failed: dl_errno %lu errno %lu\n",
		    dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_OK_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: set_phys_addr failed: "
		    "unrecognizable dl_primitive %lu received\n",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_OK_ACK_SIZE) {
		(void) fprintf(stderr, "ifconfig: set_phys_addr failed: "
		    "short ok acknowledgement received\n");
		free(buf);
		return (-1);
	}

	free(buf);
	return (0);
}


/*
 * Open the datalink provider device and bind to the REVARP type.
 * Return the resulting descriptor.
 */
static int
rarp_open(char *ifname, t_uscalar_t type, struct ether_addr *e)
{
	int			fd;

	if (debug)
		(void) printf("rarp_open: dlpi_open_attach\t");
	fd = dlpi_open_attach(ifname);
	if (fd < 0) {
		(void) fprintf(stderr, "ifconfig: could not open device for "
		    "%s\n", ifname);
		return (-1);
	}

	if (dlpi_bind(fd, type, (uchar_t *)e) < 0) {
		(void) close(fd);
		return (-1);
	}

	if (debug)
		(void) printf("device %s ethernetaddress %s\n", ifname,
		    ether_ntoa(e));

	return (fd);
}

static int
rarp_write(int fd, struct ether_arp *r, struct ether_addr *dhost)
{
	struct strbuf		ctl, data;
	union DL_primitives	*dlp;
	struct etherdladdr	dlap;
	char			*ctlbuf;
	char			*databuf;
	int			ret;

	/* Construct DL_UNITDATA_REQ.  */
	if ((ctlbuf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}
	/* LINTED: malloc returns a pointer aligned for any use */
	dlp = (union DL_primitives *)ctlbuf;
	dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp->unitdata_req.dl_dest_addr_length = ETHERADDRL + sizeof (ushort_t);
	dlp->unitdata_req.dl_dest_addr_offset = DL_UNITDATA_REQ_SIZE;
	dlp->unitdata_req.dl_priority.dl_min = 0;
	dlp->unitdata_req.dl_priority.dl_max = 0;

	/*
	 * XXX FIXME Assumes a specific DLPI address format.
	 */
	dlap.dl_sap = (ushort_t)(ETHERTYPE_REVARP);
	(void) memcpy(&dlap.dl_phys, dhost, ETHERADDRL);
	(void) memcpy(ctlbuf + DL_UNITDATA_REQ_SIZE, &dlap, sizeof (dlap));

	/* Send DL_UNITDATA_REQ.  */
	if ((databuf = malloc(BUFSIZ)) == NULL) {
		(void) fprintf(stderr, "ifconfig: malloc() failed\n");
		return (-1);
	}
	ctl.len = DL_UNITDATA_REQ_SIZE + ETHERADDRL + sizeof (ushort_t);
	ctl.buf = (char *)dlp;
	ctl.maxlen = BUFSIZ;
	(void) memcpy(databuf, r, sizeof (struct ether_arp));
	data.len = sizeof (struct ether_arp);
	data.buf = databuf;
	data.maxlen = BUFSIZ;
	ret = putmsg(fd, &ctl, &data, 0);
	free(ctlbuf);
	free(databuf);
	return (ret);
}

static int
dlpi_info_req(int fd, dl_info_ack_t *info_ack)
{
	dl_info_req_t   info_req;
	int	buf[BUFSIZ/sizeof (int)];
	union DL_primitives	*dlp = (union DL_primitives *)buf;
	struct  strbuf  ctl;
	int	flags;

	info_req.dl_primitive = DL_INFO_REQ;

	ctl.maxlen = 0;
	ctl.len = DL_INFO_REQ_SIZE;
	ctl.buf = (char *)&info_req;

	flags = RS_HIPRI;

	if (putmsg(fd, &ctl, (struct strbuf *)NULL, flags) < 0) {
		perror("ifconfig: putmsg");
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *)dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZ;
	flags = 0;
	/* start timeout for DL_BIND_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout, "DL_INFO_ACK",
	    "DL_INFO_ACK") == 0) {
		return (-1);
	}

	if (debug) {
		(void) printf("info_ack: ctl.len[%d] flags[%d]\n", ctl.len,
		    flags);
	}

	/* Validate DL_BIND_ACK reply.  */
	if (ctl.len < sizeof (t_uscalar_t)) {
		(void) fprintf(stderr,
		    "ifconfig: info req failed: short reply to info request\n");
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: info req failed:  dl_errno %lu errno %lu\n",
		    dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);
		return (-1);
	}
	if (dlp->dl_primitive != DL_INFO_ACK) {
		(void) fprintf(stderr,
		    "ifconfig: info req failed: "
		    "unrecognizable dl_primitive %lu received\n",
		    dlp->dl_primitive);
		return (-1);
	}
	if (ctl.len < DL_INFO_ACK_SIZE) {
		(void) fprintf(stderr,
		    "ifconfig: info req failed: "
		    "short info acknowledgement received\n");
		return (-1);
	}
	*info_ack = *(dl_info_ack_t *)dlp;
	return (0);
}

int
dlpi_set_address(char *ifname, struct ether_addr *ea)
{
	int 	fd;

	if (debug)
		(void) printf("dlpi_set_address: dlpi_open_attach\t");
	fd = dlpi_open_attach(ifname);
	if (fd < 0) {
		(void) fprintf(stderr, "ifconfig: could not open device for "
		    "%s\n", ifname);
		return (-1);
	}
	if (dlpi_set_phys(fd, (uchar_t *)ea) < 0) {
		(void) close(fd);
		return (-1);
	}
	(void) close(fd);

	return (0);
}

int
dlpi_get_address(char *ifname, struct ether_addr *ea)
{
	int 	fd;

	if (debug)
		(void) printf("dlpi_get_address: dlpi_open_attach\t");
	fd = dlpi_open_attach(ifname);
	if (fd < 0) {
		/* Do not report an error */
		return (-1);
	}

	if (debug)
		(void) printf("dlpi_get_address: dlpi_get_phys %s\n", ifname);
	if (dlpi_get_phys(fd, (uchar_t *)ea) < 0) {
		(void) close(fd);
		return (-1);
	}
	(void) close(fd);
	return (0);
}

static int
timed_getmsg(int fd, struct strbuf *ctlp, int *flagsp, int timeout, char *kind,
    char *request)
{
	char		perrorbuf[BUFSIZ];
	struct pollfd	pfd;
	int		ret;

	pfd.fd = fd;

	pfd.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
	if ((ret = poll(&pfd, 1, timeout * 1000)) == 0) {
		(void) fprintf(stderr, "ifconfig: %s timed out\n", kind);
		return (0);
	} else if (ret == -1) {
		(void) snprintf(perrorbuf, sizeof (perrorbuf),
		    "ifconfig: poll for %s from %s", kind, request);
		perror(perrorbuf);
		return (0);
	}

	/* poll returned > 0 for this fd so getmsg should not block */
	if ((ret = getmsg(fd, ctlp, NULL, flagsp)) < 0) {
		(void) snprintf(perrorbuf, sizeof (perrorbuf),
		    "ifconfig: getmsg expecting %s for %s", kind, request);
		perror(perrorbuf);
		return (0);
	}

	return (1);
}
