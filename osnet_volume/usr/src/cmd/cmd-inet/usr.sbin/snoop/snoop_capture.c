/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)snoop_capture.c	1.27	99/12/01 SMI"	/* SunOS */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/pfmod.h>
#include <netinet/if_ether.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <sys/bufmod.h>
#include <sys/dlpi.h>

#include <unistd.h>
#include <stropts.h>
#include <stdlib.h>
#include <ctype.h>

#include "snoop.h"

void scan();
void convert_to_network();
void convert_from_network();
void convert_old();
extern int quitting;
extern sigjmp_buf jmp_env, ojmp_env;
int netfd;
char *bufp;	/* pointer to read buffer */

/*
 * Convert a device id to a ppa value
 * e.g. "le0" -> 0
 */
int
device_ppa(device)
	char *device;
{
	char *p;
	char *tp;

	p = strpbrk(device, "0123456789");
	if (p == NULL)
		return (0);
	/* ignore numbers within device names */
	for (tp = p; *tp != '\0'; tp++)
		if (!isdigit(*tp))
			return (device_ppa(tp));
	return (atoi(p));
}

/*
 * Convert a device id to a pathname
 * e.g. "le0" -> "/dev/le"
 */
char *
device_path(device)
	char *device;
{
	static char buff[IF_NAMESIZE];
	char *p;

	(void) strcpy(buff, "/dev/");
	(void) strcat(buff, device);
	for (p = buff + (strlen(buff) - 1); p > buff; p--) {
		if (isdigit(*p))
			*p = '\0';
		else
			break;
	}
	return (buff);
}

/*
 * Open up the device, and start finding out something about it,
 * especially stuff about the data link headers.  We need that information
 * to build the proper packet filters.
 */
int
check_device(devicep, ppap)
	char **devicep;
	int *ppap;
{
	union DL_primitives dl;
	char *devname;
	/*
	 * Determine which network device
	 * to use if none given.
	 * Should get back a value like "le0".
	 */

	if (*devicep == NULL) {
		char *cbuf;
		static struct ifconf ifc;
		static struct ifreq *ifr;
		int s;
		int n;
		int numifs;
		unsigned bufsize;

		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		    pr_err("socket");

		if (ioctl(s, SIOCGIFNUM, (char *)&numifs) < 0) {
			pr_err("check_device: ioctl SIOCGIFNUM");
			(void) close(s);
			s = -1;
			return;
		}

		bufsize = numifs * sizeof (struct ifreq);
		cbuf = (char *)malloc(bufsize);
		if (cbuf == NULL) {
			pr_err("out of memory\n");
			(void) close(s);
			s = -1;
			return;
		}
		ifc.ifc_len = bufsize;
		ifc.ifc_buf = cbuf;
		if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
			pr_err("check_device: ioctl SIOCGIFCONF");
			(void) close(s);
			s = -1;
			(void) free(cbuf);
			return;
		}
		n = ifc.ifc_len / sizeof (struct ifreq);
		ifr = ifc.ifc_req;
		for (; n > 0; n--, ifr++) {
			if (strchr(ifr->ifr_name, ':') != NULL)
				continue;
			if (ioctl(s, SIOCGIFFLAGS, (char *)ifr) < 0)
				pr_err("ioctl SIOCGIFFLAGS");
			if ((ifr->ifr_flags &
				(IFF_LOOPBACK|IFF_UP|IFF_RUNNING)) ==
				(IFF_UP|IFF_RUNNING))
				break;
		}

		if (n == 0)
			pr_err("No network interface devices found");

		*devicep = ifr->ifr_name;
		(void) close(s);
	}

	devname = device_path(*devicep);
	if ((netfd = open(devname, O_RDWR)) < 0)
		pr_err("%s: %m", devname);

	*ppap = device_ppa(*devicep);

	/*
	 * Check for DLPI Version 2.
	 */
	dlinforeq(netfd, &dl);
	if (dl.info_ack.dl_version != DL_VERSION_2)
		pr_err("DL_INFO_ACK:  incompatible version %d",
		dl.info_ack.dl_version);

	for (interface = &INTERFACES[0]; interface->mac_type != -1; interface++)
		if (interface->mac_type == dl.info_ack.dl_mac_type)
			break;

	/* allow limited functionality even is interface isn't known */
	if (interface->mac_type == -1)
		fprintf(stderr, "snoop: WARNING: Mac Type = %x not supported\n",
			dl.info_ack.dl_mac_type);
	else if (interface->mac_hdr_fixed_size == IF_HDR_FIXED)
		return (1);
	return (0);
}

/*
 * Do whatever is necessary to initialize the interface
 * for packet capture. Open the device (usually /dev/le),
 * attach and bind the network interface, request raw ethernet
 * packets and set promiscuous mode, push the streams buffer
 * module and packet filter module, set various buffer
 * parameters.
 */
void
initdevice(device, snaplen, chunksize, timeout, fp, ppa)
	char *device;
	ulong_t snaplen, chunksize;
	struct timeval *timeout;
	struct Pf_ext_packetfilt *fp;
	int ppa;
{
	union DL_primitives dl;
	extern int Pflg;

	/*
	 * Attach and Bind.
	 * Bind to SAP 2 on token ring, 0 on other interface types.
	 * (SAP 0 has special significance on token ring)
	 */
	dlattachreq(netfd, ppa);
	if (interface->mac_type == DL_TPR)
		dlbindreq(netfd, 2, 0, DL_CLDLS, 0);
	else
		dlbindreq(netfd, 0, 0, DL_CLDLS, 0);

	(void) fprintf(stderr, "Using device %s ", device_path(device));

	/*
	 * If Pflg not set - use physical level
	 * promiscuous mode.  Otherwise - just SAP level.
	 */
	if (!Pflg) {
		if (geteuid() != 0)
			pr_err("Must be root to capture in promiscuous mode\n");
		(void) fprintf(stderr, "(promiscuous mode)\n");
		dlpromiscon(netfd, DL_PROMISC_PHYS);
	} else
		(void) fprintf(stderr, "(non promiscuous)\n");

	dlpromiscon(netfd, DL_PROMISC_SAP);

	if (ioctl(netfd, DLIOCRAW, 0) < 0) {
		close(netfd);
		pr_err("ioctl: DLIOCRAW: %s: %m", device_path(device));
	}

	if (fp) {
		/*
		 * push and configure the packet filtering module
		 */
		if (ioctl(netfd, I_PUSH, "pfmod") < 0) {
			close(netfd);
			pr_err("ioctl: I_PUSH pfmod: %s: %m",
			    device_path(device));
		}

		if (strioctl(netfd, PFIOCSETF, -1, sizeof (*fp), fp) < 0) {
			close(netfd);
			pr_err("PFIOCSETF: %s: %m", device_path(device));
		}
	}

	if (ioctl(netfd, I_PUSH, "bufmod") < 0) {
		close(netfd);
		pr_err("push bufmod: %s: %m", device_path(device));
	}

	if (strioctl(netfd, SBIOCSTIME, -1, sizeof (struct timeval),
		timeout) < 0) {
		close(netfd);
		pr_err("SBIOCSTIME: %s: %m", device_path(device));
	}

	if (strioctl(netfd, SBIOCSCHUNK, -1, sizeof (uint_t),
		&chunksize) < 0) {
		close(netfd);
		pr_err("SBIOCGCHUNK: %s: %m", device_path(device));
	}

	if (strioctl(netfd, SBIOCSSNAP, -1, sizeof (uint_t),
		&snaplen) < 0) {
		close(netfd);
		pr_err("SBIOCSSNAP: %s: %m", device_path(device));
	}

	/*
	 * Flush the read queue, to get rid of anything that
	 * accumulated before the device reached its final configuration.
	 */
	if (ioctl(netfd, I_FLUSH, FLUSHR) < 0) {
		close(netfd);
		pr_err("I_FLUSH: %s: %m", device_path(device));
	}
}

/*
 * Read packets from the network.  Initdevice is called in
 * here to set up the network interface for reading of
 * raw ethernet packets in promiscuous mode into a buffer.
 * Packets are read and either written directly to a file
 * or interpreted for display on the fly.
 */
void
net_read(chunksize, filter, proc, flags)
	int chunksize, filter;
	void (*proc)();
	int flags;
{
	int r = 0;
	struct strbuf data;
	int flgs;
	extern int count;

	data.len = 0;
	count = 0;

	/* allocate a read buffer */

	bufp = malloc(chunksize);
	if (bufp == NULL)
		pr_err("no memory for %dk buffer", chunksize);

	/*
	 *	read frames
	 */
	for (;;) {
		data.maxlen = chunksize;
		data.len = 0;
		data.buf = bufp;
		flgs = 0;

		r = getmsg(netfd, NULL, &data, &flgs);

		if (r < 0 || quitting)
			break;

		if (data.len <= 0)
			continue;

		scan(bufp, data.len, filter, 0, 0, proc, 0, 0, flags);
	}

	free(bufp);
	close(netfd);

	if (!quitting) {
		if (r < 0)
			pr_err("network read failed: %m");
		else
			pr_err("network read returned %d", r);
	}
}

#ifdef DEBUG
/*
 * corrupt: simulate packet corruption for debugging interpreters
 */
void
corrupt(volatile char *pktp, volatile char *pstop, char *buf,
	volatile char *bufstop)
{
	int c;
	int i;
	int p;
	int li = rand() % (pstop - pktp - 1) + 1;
	volatile char *pp = pktp;
	volatile char *pe = bufstop < pstop ? bufstop : pstop;

	if (pktp < buf || pktp > bufstop)
		return;

	for (pp = pktp; pp < pe; pp += li) {
		c = ((pe - pp) < li ? pe - pp : li);
		i = (rand() % c)>>1;
		while (--i > 0) {
			p = (rand() % c);
			pp[p] = (unsigned char)(rand() & 0xFF);
		}
	}
}
#endif /* DEBUG */

void
scan(buf, len, filter, cap, old, proc, first, last, flags)
	char *buf;
	int len, filter, cap, old;
	void (*proc)();
	int first, last;
	int flags;
{
	volatile char *bp, *bufstop;
	volatile struct sb_hdr *hdrp;
	volatile struct sb_hdr nhdr, *nhdrp;
	volatile char *pktp;
	volatile struct timeval last_timestamp;
	volatile int header_okay;
	extern int count, maxcount;
	extern int snoop_nrecover;
#ifdef	DEBUG
	extern int zflg;
#endif	/* DEBUG */

	proc(0, 0, 0);
	bufstop = buf + len;

	/*
	 *
	 * Loop through each packet in the buffer
	 */
	last_timestamp.tv_sec = 0;
	(void) memcpy((char *)ojmp_env, (char *)jmp_env, sizeof (jmp_env));
	for (bp = buf; bp < bufstop; bp += nhdrp->sbh_totlen) {
		/*
		 * Gracefully exit if user terminates
		 */
		if (quitting)
			break;
		/*
		 * Global error recocery: Prepare to continue when a corrupt
		 * packet or header is encountered.
		 */
		if (sigsetjmp(jmp_env, 1)) {
			goto err;
		}

		header_okay = 0;
		hdrp = (struct sb_hdr *)bp;
		nhdrp = hdrp;
		pktp = (char *)hdrp + sizeof (*hdrp);

		/*
		 * If reading a capture file
		 * convert the headers from network
		 * byte order (for little-endians like X86)
		 */
		if (cap) {
			/*
			 * If the packets come from an old
			 * capture file, convert the header.
			 */
			if (old) {
				convert_old(hdrp);
			}

			nhdrp = &nhdr;

			nhdrp->sbh_origlen = ntohl(hdrp->sbh_origlen);
			nhdrp->sbh_msglen = ntohl(hdrp->sbh_msglen);
			nhdrp->sbh_totlen = ntohl(hdrp->sbh_totlen);
			nhdrp->sbh_drops = ntohl(hdrp->sbh_drops);
			nhdrp->sbh_timestamp.tv_sec =
				ntohl(hdrp->sbh_timestamp.tv_sec);
			nhdrp->sbh_timestamp.tv_usec =
				ntohl(hdrp->sbh_timestamp.tv_usec);
		}

		/* Enhanced check for valid header */

		if ((nhdrp->sbh_totlen == 0) ||
		    (bp + nhdrp->sbh_totlen) < bp ||
		    (bp + nhdrp->sbh_totlen) > bufstop ||
		    (nhdrp->sbh_origlen == 0) ||
		    (bp + nhdrp->sbh_origlen) < bp ||
		    (nhdrp->sbh_msglen == 0) ||
		    (bp + nhdrp->sbh_msglen) < bp ||
		    (bp + nhdrp->sbh_msglen) > bufstop ||
		    (nhdrp->sbh_msglen > nhdrp->sbh_origlen) ||
		    (nhdrp->sbh_totlen < nhdrp->sbh_msglen) ||
		    (nhdrp->sbh_timestamp.tv_sec == 0)) {
			if (cap)
				fprintf(stderr, "(warning) bad packet header "
						"in capture file");
			else
				fprintf(stderr, "(warning) bad packet header "
						"in buffer");
			(void) fprintf(stderr, " offset %d: length=%d\n",
				bp - buf, nhdrp->sbh_totlen);
			goto err;
		}

		if (nhdrp->sbh_totlen > interface->mtu_size) {
			if (cap)
				fprintf(stderr, "(warning) packet length "
					"greater than MTU in capture file");
			else
				fprintf(stderr, "(warning) packet length "
					"greater than MTU in buffer");

			(void) fprintf(stderr, " offset %d: length=%d\n",
				bp - buf, nhdrp->sbh_totlen);
		}

		/*
		 * Check for incomplete packet.  We are conservative here,
		 * since we don't know how good the checking is in other
		 * parts of the code.  We pass a partial packet, with
		 * a warning.
		 */
		if (pktp + nhdrp->sbh_msglen > bufstop) {
			fprintf(stderr, "truncated packet buffer\n");
			nhdrp->sbh_msglen = bufstop - pktp;
		}

#ifdef DEBUG
		if (zflg)
			corrupt(pktp, pktp + nhdrp->sbh_msglen, buf, bufstop);
#endif /* DEBUG */

		header_okay = 1;
		if (!filter ||
			want_packet(pktp,
				nhdrp->sbh_msglen,
				nhdrp->sbh_origlen)) {
			count++;

			/*
			 * Start deadman timer for interpreter processing
			 */
			(void) snoop_alarm(SNOOP_ALARM_GRAN*SNOOP_MAXRECOVER,
				NULL);

			if (!cap || count >= first)
				proc(nhdrp, pktp, count, flags);

			if (cap && count >= last) {
				(void) snoop_alarm(0, NULL);
				break;
			}

			if (maxcount && count >= maxcount) {
				fprintf(stderr, "%d packets captured\n", count);
				exit(1);
			}

			snoop_nrecover = 0;			/* success */
			(void) snoop_alarm(0, NULL);
			last_timestamp = hdrp->sbh_timestamp;	/* save stamp */
		}
		continue;
err:
		/*
		 * Corruption has been detected. Reset errors.
		 */
		snoop_recover();

		/*
		 * packet header was apparently okay. Continue.
		 */
		if (header_okay)
			continue;

		/*
		 * Otherwise try to scan forward to the next packet, using
		 * the last known timestamp if it is available.
		*/
		nhdrp = &nhdr;
		nhdrp->sbh_totlen = 0;
		if (last_timestamp.tv_sec == 0) {
			bp += sizeof (int);
		} else {
			for (bp += sizeof (int); bp <= bufstop;
				bp += sizeof (int)) {
				hdrp = (struct sb_hdr *)bp;
				/* An approximate timestamp located */
				if ((hdrp->sbh_timestamp.tv_sec >> 8) ==
				    (last_timestamp.tv_sec >> 8))
					break;
			}
		}
	}
	/* reset jmp_env for program exit */
	(void) memcpy((char *)jmp_env, (char *)ojmp_env, sizeof (jmp_env));
	proc(0, -1, 0);
}

/*
 * Routines for opening, closing, reading and writing
 * a capture file of packets saved with the -o option.
 */
static int capfile_out;
static long long spaceavail;

/*
 * The snoop capture file has a header to identify
 * it as a capture file and record its version.
 * A file without this header is assumed to be an
 * old format snoop file.
 *
 * A version 1 header looks like this:
 *
 *   0   1   2   3   4   5   6   7   8   9  10  11
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | s | n | o | o | p | \0| \0| \0|    version    |  data
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |	 word 0	   |	 word 1	   |	 word 2	   |
 *
 *
 * A version 2 header adds a word that identifies the MAC type.
 * This allows for capture files from FDDI etc.
 *
 *   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * | s | n | o | o | p | \0| \0| \0|    version    |    MAC type   | data
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |	 word 0	   |	 word 1	   |	 word 2	   |	 word 3
 *
 */
const char *snoop_id = "snoop\0\0\0";
const int snoop_idlen = 8;
const int snoop_version = 2;

void
cap_open_write(name)
	char *name;
{
	struct statvfs fs;
	int vers;

	capfile_out = open(name, O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (capfile_out < 0)
		pr_err("%s: %m", name);

	if (fstatvfs(capfile_out, &fs) < 0)
		pr_err("statvfs: %m");

	vers = htonl(snoop_version);
	write(capfile_out, snoop_id, snoop_idlen);
	write(capfile_out, &vers, sizeof (int));

	spaceavail = fs.f_bavail * fs.f_frsize;
}

void
cap_close()
{
	close(capfile_out);
}

static char *cap_buffp = NULL;
static int cap_len = 0;
static int cap_new;

void
cap_open_read(name)
	char *name;
{
	struct stat st;
	int cap_vers;
	int *word, device_mac_type;
	int capfile_in;

	capfile_in = open(name, O_RDONLY);
	if (capfile_in < 0)
		pr_err("couldn't open %s: %m", name);

	if (fstat(capfile_in, &st) < 0)
		pr_err("couldn't stat %s: %m", name);
	cap_len = st.st_size;

	cap_buffp = mmap(0, cap_len, PROT_READ, MAP_PRIVATE, capfile_in, 0);
	close(capfile_in);
	if ((int)cap_buffp == -1)
		pr_err("couldn't mmap %s: %m", name);

	/* Check if new snoop capture file format */

	cap_new = strncmp(cap_buffp, snoop_id, snoop_idlen) == 0;

	/*
	 * If new file - check version and
	 * set buffer pointer to point at first packet
	 */
	if (cap_new) {
		cap_vers = ntohl(*(int *)(cap_buffp + snoop_idlen));
		cap_buffp += snoop_idlen + sizeof (int);
		cap_len   -= snoop_idlen + sizeof (int);

		switch (cap_vers) {
		case 1:
			device_mac_type = DL_ETHER;
			break;

		case 2:
			device_mac_type = ntohl(*((int *)cap_buffp));
			cap_buffp += sizeof (int);
			cap_len   -= sizeof (int);
			break;

		default:
			pr_err("capture file: %s: Version %d unrecognized\n",
				name, cap_vers);
		}

		for (interface = &INTERFACES[0]; interface->mac_type != -1;
				interface++)
			if (interface->mac_type == device_mac_type)
				break;

		if (interface->mac_type == -1)
			pr_err("Mac Type = %x is not supported\n",
				device_mac_type);
	} else {
		/* Use heuristic to check if it's an old-style file */

		device_mac_type = DL_ETHER;
		word = (int *)cap_buffp;

		if (!((word[0] < 1600 && word[1] < 1600) &&
		    (word[0] < word[1]) &&
		    (word[2] > 610000000 && word[2] < 770000000)))
			pr_err("not a capture file: %s", name);

		/* Change protection so's we can fix the headers */

		if (mprotect(cap_buffp, cap_len, PROT_READ | PROT_WRITE) < 0)
			pr_err("mprotect: %s: %m", name);
	}
}

void
cap_read(first, last, filter, proc, flags)
	int first, last;
	int filter;
	void (*proc)();
	int flags;
{
	extern int count;

	count = 0;

	scan(cap_buffp, cap_len, filter, 1, !cap_new, proc, first, last, flags);

	munmap(cap_buffp, cap_len);
}

void
cap_write(hdrp, pktp, num, flags)
	struct sb_hdr *hdrp;
	char *pktp;
	int num, flags;
{
	int pktlen, mac;
	static int first = 1;
	struct sb_hdr nhdr;
	extern boolean_t qflg;

	if (hdrp == NULL)
		return;

	if (first) {
		first = 0;
		mac = htonl(interface->mac_type);
		write(capfile_out, &mac,  sizeof (int));
		spaceavail -= sizeof (int);
	}

	if ((spaceavail -= hdrp->sbh_totlen) < 0)
		pr_err("out of disk space");

	pktlen = hdrp->sbh_totlen - sizeof (*hdrp);

	/*
	 * Convert sb_hdr to network byte order
	 */
	nhdr.sbh_origlen = htonl(hdrp->sbh_origlen);
	nhdr.sbh_msglen = htonl(hdrp->sbh_msglen);
	nhdr.sbh_totlen = htonl(hdrp->sbh_totlen);
	nhdr.sbh_drops = htonl(hdrp->sbh_drops);
	nhdr.sbh_timestamp.tv_sec = htonl(hdrp->sbh_timestamp.tv_sec);
	nhdr.sbh_timestamp.tv_usec = htonl(hdrp->sbh_timestamp.tv_usec);

	write(capfile_out, &nhdr, sizeof (nhdr));
	write(capfile_out, pktp, pktlen);

	if (! qflg)
		show_count();
}

/*
 * Old header format.
 * Actually two concatenated structs:  nit_bufhdr + nit_head
 */
struct ohdr {
	/* nit_bufhdr */
	int	o_msglen;
	int	o_totlen;
	/* nit_head */
	struct timeval o_time;
	int	o_drops;
	int	o_len;
};

/*
 * Convert a packet header from
 * old to new format.
 */
void
convert_old(ohdrp)
	struct ohdr *ohdrp;
{
	struct sb_hdr nhdr;

	nhdr.sbh_origlen = ohdrp->o_len;
	nhdr.sbh_msglen  = ohdrp->o_msglen;
	nhdr.sbh_totlen  = ohdrp->o_totlen;
	nhdr.sbh_drops   = ohdrp->o_drops;
	nhdr.sbh_timestamp = ohdrp->o_time;

	*(struct sb_hdr *)ohdrp = nhdr;
}

strioctl(fd, cmd, timout, len, dp)
int	fd;
int	cmd;
int	timout;
int	len;
char	*dp;
{
	struct	strioctl	sioc;
	int	rc;

	sioc.ic_cmd = cmd;
	sioc.ic_timout = timout;
	sioc.ic_len = len;
	sioc.ic_dp = dp;
	rc = ioctl(fd, I_STR, &sioc);

	if (rc < 0)
		return (rc);
	else
		return (sioc.ic_len);
}
