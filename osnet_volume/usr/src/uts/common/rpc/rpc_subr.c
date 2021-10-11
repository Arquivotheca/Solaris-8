/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1999 Sun Microsystems, Inc.
 * All Rights Reserved.
 */
#pragma ident	"@(#)rpc_subr.c	1.26	99/05/03 SMI" /* SVr4.0 1.1 */

/*
 * Miscellaneous support routines for kernel implementation of RPC.
 */

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/socket.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/tiuser.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpcb_prot.h>
#include <rpc/pmap_prot.h>

static int strtoi(char *, char **);
static void grow_netbuf(struct netbuf *, size_t);
static void loopb_u2t(const char *, struct netbuf *);

#define	RPC_PMAP_TIMEOUT	15

/*
 * Kernel level debugging aid. The global variable "rpclog" is a bit
 * mask which allows various types of debugging messages to be printed
 * out.
 *
 *	rpclog & 1 	will cause actual failures to be printed.
 *	rpclog & 2	will cause informational messages to be
 *			printed on the client side of rpc.
 *	rpclog & 4	will cause informational messages to be
 *			printed on the server side of rpc.
 *	rpclog & 8	will cause informational messages for rare events to be
 *			printed on the client side of rpc.
 *	rpclog & 16	will cause informational messages for rare events to be
 *			printed on the server side of rpc.
 *	rpclog & 32	will cause informational messages for rare events to be
 *			printed on the common client/server code paths of rpc.
 *	rpclog & 64	will cause informational messages for manipulation
 *			client-side COTS dispatch list to be printed.
 */

uint_t rpclog = 0;


void
rpc_poptimod(vnode_t *vp)
{
	int error, isfound, ret;

	error = strioctl(vp, I_FIND, (intptr_t)"timod", 0, K_TO_K, CRED(),
	    &isfound);
	if (error) {
		RPCLOG(1, "rpc_poptimod: I_FIND strioctl error %d\n", error);
		return;
	}
	if (isfound) {
		/*
		 * Pop timod module
		 */
		error = strioctl(vp, I_POP, 0, 0, K_TO_K, CRED(), &ret);
		if (error) {
			RPCLOG(1, "rpc_poptimod: I_POP strioctl error %d\n",
			    error);
			return;
		}
	}
}

/*
 * Return a port number from a sockaddr_in expressed in universal address
 * format.  Note that this routine does not work for address families other
 * than INET.  Eventually, we should replace this routine with one that
 * contacts the rpcbind running locally.
 */
int
rpc_uaddr2port(int af, char *addr)
{
	int p1;
	int p2;
	char *next, *p;

	if (af == AF_INET) {
		/*
		 * A struct sockaddr_in expressed in universal address
		 * format looks like:
		 *
		 *	"IP.IP.IP.IP.PORT[top byte].PORT[bottom byte]"
		 *
		 * Where each component expresses as a character,
		 * the corresponding part of the IP address
		 * and port number.
		 * Thus 127.0.0.1, port 2345 looks like:
		 *
		 *	49 50 55 46 48 46 48 46 49 46 57 46 52 49
		 *	1  2  7  .  0  .  0  .  1  .  9  .  4  1
		 *
		 * 2345 = 929base16 = 9.32+9 = 9.41
		 */
		(void) strtoi(addr, &next);
		(void) strtoi(next, &next);
		(void) strtoi(next, &next);
		(void) strtoi(next, &next);
		p1 = strtoi(next, &next);
		p2 = strtoi(next, &next);

	} else if (af == AF_INET6) {
		/*
		 * An IPv6 address is expressed in following two formats
		 * fec0:A02::2:202:4FCD or
		 * ::10.9.2.1
		 * An universal address will have porthi.portlo appended to
		 * v6 address. So always look for the last two dots when
		 * extracting port number.
		 */
		next = addr;
		while (next = strchr(next, '.')) {
			p = ++next;
			next = strchr(next, '.');
			next++;
		}
		p1 = strtoi(p, &p);
		p2 = strtoi(p, &p);
		RPCLOG(1, "rpc_uaddr2port: IPv6 port %d\n", ((p1 << 8) + p2));
	}

	return ((p1 << 8) + p2);
}

/*
 * Modified strtol(3).  Should we be using mi_strtol() instead?
 */
static int
strtoi(char *str, char **ptr)
{
	int c;
	int val;

	for (val = 0, c = *str++; c >= '0' && c <= '9'; c = *str++) {
		val *= 10;
		val += c - '0';
	}
	*ptr = str;
	return (val);
}

/*
 * Utilities for manipulating netbuf's.
 *
 * Note that loopback addresses are not null-terminated, so these utilities
 * typically use the strn* string routines.
 */

/*
 * Utilities to patch a port number (for NC_INET protocols) or a
 *	port name (for NC_LOOPBACK) into a network address.
 */


/*
 * PSARC 1999/553-01 Contract Private Interface
 * put_inet_port
 * Changes must be reviewed by Solaris File Sharing
 * Changes must be communicated to contract-1999-553-01@sun.com
 */
void
put_inet_port(struct netbuf *addr, ushort_t port)
{
	/*
	 * Easy - we always patch an unsigned short on top of an
	 * unsigned short.  No changes to addr's len or maxlen are
	 * necessary.
	 */
	((struct sockaddr_in *)(addr->buf))->sin_port = port;
}

void
put_inet6_port(struct netbuf *addr, ushort_t port)
{
	((struct sockaddr_in6 *)(addr->buf))->sin6_port = port;
}

void
put_loopback_port(struct netbuf *addr, char *port)
{
	char *dot;
	char *newbuf;
	int newlen;


	/*
	 * We must make sure the addr has enough space for us,
	 * patch in `port', and then adjust addr's len and maxlen
	 * to reflect the change.
	 */
	if ((dot = strnrchr(addr->buf, '.', addr->len)) == (char *)NULL)
		return;

	newlen = (int)((dot - addr->buf + 1) + strlen(port));
	if (newlen > addr->maxlen) {
		newbuf = kmem_zalloc(newlen, KM_SLEEP);
		bcopy(addr->buf, newbuf, addr->len);
		kmem_free(addr->buf, addr->maxlen);
		addr->buf = newbuf;
		addr->len = addr->maxlen = newlen;
		dot = strnrchr(addr->buf, '.', addr->len);
	} else {
		addr->len = newlen;
	}

	(void) strncpy(++dot, port, strlen(port));
}

/*
 * Convert a loopback universal address to a loopback transport address.
 */
static void
loopb_u2t(const char *ua, struct netbuf *addr)
{
	size_t stringlen = strlen(ua) + 1;
	const char *univp;		/* ptr into universal addr */
	char *transp;			/* ptr into transport addr */

	/* Make sure the netbuf will be big enough. */
	if (addr->maxlen < stringlen) {
		grow_netbuf(addr, stringlen);
	}

	univp = ua;
	transp = addr->buf;
	while (*univp != NULL) {
		if (*univp == '\\' && *(univp+1) == '\\') {
			*transp = '\\';
			univp += 2;
		} else if (*univp == '\\') {
			/* octal character */
			*transp = (((*(univp+1) - '0') & 3) << 6) +
			    (((*(univp+2) - '0') & 7) << 3) +
			    ((*(univp+3) - '0') & 7);
			univp += 4;
		} else {
			*transp = *univp;
			univp++;
		}
		transp++;
	}

	addr->len = (unsigned int)(transp - addr->buf);
	ASSERT(addr->len <= addr->maxlen);
}

/*
 * Make sure the given netbuf has a maxlen at least as big as the given
 * length.
 */
static void
grow_netbuf(struct netbuf *nb, size_t length)
{
	char *newbuf;

	if (nb->maxlen >= length)
		return;

	newbuf = kmem_zalloc(length, KM_SLEEP);
	bcopy(nb->buf, newbuf, nb->len);
	kmem_free(nb->buf, nb->maxlen);
	nb->buf = newbuf;
	nb->maxlen = (unsigned int)length;
}


/*
 * Try to get the address for the desired service by using the rpcbind
 * protocol.  Ignores signals.
 */

enum clnt_stat
rpcbind_getaddr(struct knetconfig *config, rpcprog_t prog, rpcvers_t vers,
    struct netbuf *addr)
{
	char *ua = NULL;
	enum clnt_stat status;
	RPCB parms;
	struct timeval tmo;
	CLIENT *client = NULL;
	k_sigset_t oldmask;
	k_sigset_t newmask;
	ushort_t port;

	/*
	 * Call rpcbind (local or remote) to get an address we can use
	 * in an RPC client handle.
	 */
	tmo.tv_sec = RPC_PMAP_TIMEOUT;
	tmo.tv_usec = 0;
	parms.r_prog = prog;
	parms.r_vers = vers;
	parms.r_addr = parms.r_owner = "";

	if (strcmp(config->knc_protofmly, NC_INET) == 0) {
		if (strcmp(config->knc_proto, NC_TCP) == 0)
			parms.r_netid = "tcp";
		else
			parms.r_netid = "udp";
		put_inet_port(addr, htons(PMAPPORT));
	} else if (strcmp(config->knc_protofmly, NC_INET6) == 0) {
		if (strcmp(config->knc_proto, NC_TCP) == 0)
			parms.r_netid = "tcp6";
		else
			parms.r_netid = "udp6";
		put_inet6_port(addr, htons(PMAPPORT));
	} else if (strcmp(config->knc_protofmly, NC_LOOPBACK) == 0) {
		if (config->knc_semantics == NC_TPI_COTS_ORD)
			parms.r_netid = "ticotsord";
		else if (config->knc_semantics == NC_TPI_COTS)
			parms.r_netid = "ticots";
		else
			parms.r_netid = "ticlts";

		put_loopback_port(addr, "rpc");
	} else {
		status = RPC_UNKNOWNPROTO;
		goto out;
	}

	/*
	 * Mask signals for the duration of the handle creation and
	 * RPC calls.  This allows relatively normal operation with a
	 * signal already posted to our thread (e.g., when we are
	 * sending an NLM_CANCEL in response to catching a signal).
	 *
	 * Any further exit paths from this routine must restore
	 * the original signal mask.
	 */
	sigfillset(&newmask);
	sigreplace(&newmask, &oldmask);

	if (clnt_tli_kcreate(config, addr, RPCBPROG,
	    RPCBVERS, 0, 0, CRED(), &client)) {
		status = RPC_TLIERROR;
		sigreplace(&oldmask, (k_sigset_t *)NULL);
		goto out;
	}

	client->cl_nosignal = 1;
	if ((status = CLNT_CALL(client, RPCBPROC_GETADDR,
	    xdr_rpcb, (char *)&parms,
	    xdr_wrapstring, (char *)&ua,
	    tmo)) != RPC_SUCCESS) {
		sigreplace(&oldmask, (k_sigset_t *)NULL);
		goto out;
	}

	sigreplace(&oldmask, (k_sigset_t *)NULL);

	if (ua == NULL || *ua == NULL) {
		status = RPC_PROGNOTREGISTERED;
		goto out;
	}

	/*
	 * Convert the universal address to the transport address.
	 * Theoretically, we should call the local rpcbind to translate
	 * from the universal address to the transport address, but it gets
	 * complicated (e.g., there's no direct way to tell rpcbind that we
	 * want an IP address instead of a loopback address).  Note that
	 * the transport address is potentially host-specific, so we can't
	 * just ask the remote rpcbind, because it might give us the wrong
	 * answer.
	 */
	if (strcmp(config->knc_protofmly, NC_INET) == 0) {
		port = rpc_uaddr2port(AF_INET, ua);
		put_inet_port(addr, ntohs(port));
	} else if (strcmp(config->knc_protofmly, NC_INET6) == 0) {
		port = rpc_uaddr2port(AF_INET6, ua);
		put_inet6_port(addr, ntohs(port));
	} else if (strcmp(config->knc_protofmly, NC_LOOPBACK) == 0) {
		loopb_u2t(ua, addr);
	} else {
		/* "can't happen" - should have been checked for above */
		cmn_err(CE_PANIC, "rpcbind_getaddr: bad protocol family");
	}

out:
	if (client != NULL) {
		auth_destroy(client->cl_auth);
		clnt_destroy(client);
	}
	if (ua != NULL)
		xdr_free(xdr_wrapstring, (char *)&ua);
	return (status);
}

static const char *tpiprims[] = {
	"T_CONN_REQ      0        connection request",
	"T_CONN_RES      1        connection response",
	"T_DISCON_REQ    2        disconnect request",
	"T_DATA_REQ      3        data request",
	"T_EXDATA_REQ    4        expedited data request",
	"T_INFO_REQ      5        information request",
	"T_BIND_REQ      6        bind request",
	"T_UNBIND_REQ    7        unbind request",
	"T_UNITDATA_REQ  8        unitdata request",
	"T_OPTMGMT_REQ   9        manage options req",
	"T_ORDREL_REQ    10       orderly release req",
	"T_CONN_IND      11       connection indication",
	"T_CONN_CON      12       connection confirmation",
	"T_DISCON_IND    13       disconnect indication",
	"T_DATA_IND      14       data indication",
	"T_EXDATA_IND    15       expeditied data indication",
	"T_INFO_ACK      16       information acknowledgment",
	"T_BIND_ACK      17       bind acknowledment",
	"T_ERROR_ACK     18       error acknowledgment",
	"T_OK_ACK        19       ok acknowledgment",
	"T_UNITDATA_IND  20       unitdata indication",
	"T_UDERROR_IND   21       unitdata error indication",
	"T_OPTMGMT_ACK   22       manage options ack",
	"T_ORDREL_IND    23       orderly release ind"
};


const char *
rpc_tpiprim2name(uint_t prim)
{
	if (prim > (sizeof (tpiprims) / sizeof (tpiprims[0]) - 1))
		return ("unknown primitive");

	return (tpiprims[prim]);
}

static const char *tpierrs[] = {
	"error zero      0",
	"TBADADDR        1        incorrect addr format",
	"TBADOPT         2        incorrect option format",
	"TACCES          3        incorrect permissions",
	"TBADF           4        illegal transport fd",
	"TNOADDR         5        couldn't allocate addr",
	"TOUTSTATE       6        out of state",
	"TBADSEQ         7        bad call sequnce number",
	"TSYSERR         8        system error",
	"TLOOK           9        event requires attention",
	"TBADDATA        10       illegal amount of data",
	"TBUFOVFLW       11       buffer not large enough",
	"TFLOW           12       flow control",
	"TNODATA         13       no data",
	"TNODIS          14       discon_ind not found on q",
	"TNOUDERR        15       unitdata error not found",
	"TBADFLAG        16       bad flags",
	"TNOREL          17       no ord rel found on q",
	"TNOTSUPPORT     18       primitive not supported",
	"TSTATECHNG      19       state is in process of changing"
};


const char *
rpc_tpierr2name(uint_t err)
{
	if (err > (sizeof (tpierrs) / sizeof (tpierrs[0]) - 1))
		return ("unknown error");

	return (tpierrs[err]);
}

/*
 * derive  the code from user land inet_top6
 * convert IPv6 binary address into presentation (printable) format
 */
#define	INADDRSZ	4
#define	IN6ADDRSZ	16
#define	INT16SZ	2
const char *
kinet_ntop6(src, dst, size)
	uchar_t *src;
	char *dst;
	size_t size;
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof ("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	char *tp;
	struct { int base, len; } best, cur;
	uint_t words[IN6ADDRSZ / INT16SZ];
	int i;
	size_t len; /* this is used to track the sprintf len */

	/*
	 * Preprocess:
	 * Copy the input (bytewise) array into a wordwise array.
	 * Find the longest run of 0x00's in src[] for :: shorthanding.
	 */

	bzero(words, sizeof (words));
	for (i = 0; i < IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	cur.base = -1;

	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}

	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		(void) sprintf(tp, "%x", words[i]);
		len = strlen(tp);
		tp += len;
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((int)(tp - tmp) > size) {
		return (NULL);
	}
	(void) strcpy(dst, tmp);
	return (dst);
}
