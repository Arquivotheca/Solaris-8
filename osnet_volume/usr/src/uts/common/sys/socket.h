/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_SOCKET_H
#define	_SYS_SOCKET_H

#pragma ident	"@(#)socket.h	1.53	99/11/07 SMI"	/* SVr4.0 1.10  */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/feature_tests.h>
#ifndef	_KERNEL
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/netconfig.h>
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
#endif	/* !_KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_SA_FAMILY_T
#define	_SA_FAMILY_T
typedef uint16_t	sa_family_t;
#endif

#ifndef	_SOCKLEN_T
#define	_SOCKLEN_T

#if defined(_XPG4_2) && !defined(_XPG5) && !defined(_LP64)
typedef	size_t		socklen_t;
#else
typedef	uint32_t	socklen_t;
#endif	/* defined(_XPG4_2) && !defined(_XPG5) && !defined(_LP64) */

#if defined(_XPG4_2)
typedef	socklen_t	*Psocklen_t;
#else
typedef	void		*Psocklen_t;
#endif	/* defined(_XPG4_2) */

#endif	/* _SOCKLEN_T */

/*
 * Definitions related to sockets: types, address families, options.
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#ifndef	NC_TPI_CLTS
#define	NC_TPI_CLTS	1		/* must agree with netconfig.h */
#define	NC_TPI_COTS	2		/* must agree with netconfig.h */
#define	NC_TPI_COTS_ORD	3		/* must agree with netconfig.h */
#define	NC_TPI_RAW	4		/* must agree with netconfig.h */
#endif	/* !NC_TPI_CLTS */
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Types
 */
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	SOCK_STREAM	NC_TPI_COTS	/* stream socket */
#define	SOCK_DGRAM	NC_TPI_CLTS	/* datagram socket */
#define	SOCK_RAW	NC_TPI_RAW	/* raw-protocol interface */
#else
#define	SOCK_STREAM	2		/* stream socket */
#define	SOCK_DGRAM	1		/* datagram socket */
#define	SOCK_RAW	4		/* raw-protocol interface */
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */
#define	SOCK_RDM	5		/* reliably-delivered message */
#define	SOCK_SEQPACKET	6		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_DGRAM_ERRIND	0x0200		/* Application wants delayed error */

/*
 * N.B.: The following definition is present only for compatibility
 * with release 3.0.  It will disappear in later releases.
 */
#define	SO_DONTLINGER	(~SO_LINGER)	/* ~SO_LINGER */

/*
 * Additional options, not kept in so_options.
 */
#define	SO_SNDBUF	0x1001		/* send buffer size */
#define	SO_RCVBUF	0x1002		/* receive buffer size */
#define	SO_SNDLOWAT	0x1003		/* send low-water mark */
#define	SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define	SO_SNDTIMEO	0x1005		/* send timeout */
#define	SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */
#define	SO_PROTOTYPE	0x1009		/* get/set protocol type */

/* "Socket"-level control message types: */
#define	SCM_RIGHTS	0x1010		/* access rights (array of int) */

#define	SO_STATE	0x2000		/* Internal: get so_state */
#ifdef	_KERNEL
#define	SO_SRCADDR	0x2001		/* Internal: AF_UNIX source address */
#define	SO_FILEP	0x2002		/* Internal: AF_UNIX file pointer */
#define	SO_UNIX_CLOSE	0x2003		/* Internal: AF_UNIX peer closed */
#endif	/* _KERNEL */

/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host (pipes, portals) */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_NBS		7		/* nbs protocols */
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define	AF_DECnet	12		/* DECnet */
#define	AF_DLI		13		/* Direct data link interface */
#define	AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */
#define	AF_NIT		17		/* Network Interface Tap */
#define	AF_802		18		/* IEEE 802.2, also ISO 8802 */
#define	AF_OSI		19		/* umbrella for all families used */
#define	AF_X25		20		/* CCITT X.25 in particular */
#define	AF_OSINET	21		/* AFI = 47, IDI = 4 */
#define	AF_GOSIP	22		/* U.S. Government OSI */
#define	AF_IPX		23		/* Novell Internet Protocol */
#define	AF_ROUTE	24		/* Internal Routing Protocol */
#define	AF_LINK		25		/* Link-layer interface */
#define	AF_INET6	26		/* Internet Protocol, Version 6 */
#define	AF_KEY		27		/* Security Association DB socket */

#define	AF_MAX		27

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	sa_family_t	sa_family;	/* address family */
	char		sa_data[14];	/* up to 14 bytes of direct address */
};

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <netinet/in.h>
#include <sys/un.h>
#include <net/if_dl.h>

/*
 * sockaddr_storage:
 * Common superset of at least AF_INET, AF_INET6 and AF_LINK sockaddr
 * structures. Has sufficient size and alignment for those sockaddrs.
 */

/*
 * Desired maximum size, alignment size and related types.
 */
#define	_SS_MAXSIZE	256	/* Implementation specific max size */

/*
 * To represent desired sockaddr max alignment for platform, a
 * type is chosen which may depend on implementation platform architecture.
 * Type chosen based on alignment size restrictions from <sys/isa_defs.h>.
 * We desire to force up to (but no more than) 64-bit (8 byte) alignment,
 * on platforms where it is possible to do so. (e.g not possible on ia32).
 * For all currently supported platforms by our implementation
 * in <sys/isa_defs.h>, (i.e. sparc, sparcv9, ia32, ia64)
 * type "double" is suitable for that intent.
 *
 * Note: Type "double" is chosen over the more obvious integer type int64_t.
 *   int64_t is not a valid type for strict ANSI/ISO C compilation on ILP32.
 */
typedef	double		sockaddr_maxalign_t;

#define	_SS_ALIGNSIZE	(sizeof (sockaddr_maxalign_t))

/*
 * Definitions used for sockaddr_storage structure paddings design.
 */
#define	_SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof (sa_family_t))
#define	_SS_PAD2SIZE	(_SS_MAXSIZE - (sizeof (sa_family_t)+ \
			_SS_PAD1SIZE + _SS_ALIGNSIZE))

struct sockaddr_storage {
	sa_family_t	ss_family;	/* Address family */
	/* Following fields are implementation specific */
	char		_ss_pad1[_SS_PAD1SIZE];
	sockaddr_maxalign_t _ss_align;
	char		_ss_pad2[_SS_PAD2SIZE];
};
#endif	/* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * Protocol families, same as address families for now.
 */
#define	PF_UNSPEC	AF_UNSPEC
#define	PF_UNIX		AF_UNIX
#define	PF_INET		AF_INET
#define	PF_IMPLINK	AF_IMPLINK
#define	PF_PUP		AF_PUP
#define	PF_CHAOS	AF_CHAOS
#define	PF_NS		AF_NS
#define	PF_NBS		AF_NBS
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define	PF_DECnet	AF_DECnet
#define	PF_DLI		AF_DLI
#define	PF_LAT		AF_LAT
#define	PF_HYLINK	AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_NIT		AF_NIT
#define	PF_802		AF_802
#define	PF_OSI		AF_OSI
#define	PF_X25		AF_X25
#define	PF_OSINET	AF_OSINET
#define	PF_GOSIP	AF_GOSIP
#define	PF_IPX		AF_IPX
#define	PF_ROUTE	AF_ROUTE
#define	PF_LINK		AF_LINK
#define	PF_INET6	AF_INET6
#define	PF_KEY		AF_KEY

#define	PF_MAX		AF_MAX

/*
 * Maximum queue length specifiable by listen.
 */
#define	SOMAXCONN	5

/*
 * Message header for recvmsg and sendmsg calls.
 */
struct msghdr {
	void		*msg_name;		/* optional address */
	socklen_t	msg_namelen;		/* size of address */
	struct iovec	*msg_iov;		/* scatter/gather array */
	int		msg_iovlen;		/* # elements in msg_iov */

#if defined(_XPG4_2) || defined(_KERNEL)
	void		*msg_control;		/* ancillary data */
	socklen_t	msg_controllen;		/* ancillary data buffer len */
	int		msg_flags;		/* flags on received message */
#else
	caddr_t		msg_accrights;	/* access rights sent/received */
	int		msg_accrightslen;
#endif	/* defined(_XPG4_2) || defined(_KERNEL) */
};

#if	defined(_KERNEL)

/*
 *	N.B.:  we assume that omsghdr and nmsghdr are isomorphic, with
 *	the sole exception that nmsghdr has the additional msg_flags
 *	field at the end.
 */
struct omsghdr {
	void		*msg_name;	/* optional address */
	socklen_t	msg_namelen;	/* size of address */
	struct	iovec	*msg_iov;	/* scatter/gather array */
	int		msg_iovlen;	/* # elements in msg_iov */
	caddr_t		msg_accrights;	/* access rights sent/received */
	int		msg_accrightslen;
};

#define	nmsghdr		msghdr

#if defined(_SYSCALL32)

struct omsghdr32 {
	caddr32_t	msg_name;	/* optional address */
	uint32_t	msg_namelen;	/* size of address */
	caddr32_t	msg_iov;	/* scatter/gather array */
	int32_t		msg_iovlen;	/* # elements in msg_iov */
	caddr32_t	msg_accrights;	/* access rights sent/received */
	uint32_t	msg_accrightslen;
};

struct msghdr32 {
	caddr32_t	msg_name;	/* optional address */
	uint32_t	msg_namelen;	/* size of address */
	caddr32_t	msg_iov;	/* scatter/gather array */
	int32_t		msg_iovlen;	/* # elements in msg_iov */
	caddr32_t	msg_control;	/* ancillary data */
	uint32_t	msg_controllen;	/* ancillary data buffer len */
	int32_t		msg_flags;	/* flags on received message */
};

#define	nmsghdr32	msghdr32

#endif	/* _SYSCALL32 */
#endif	/* _KERNEL */

#define	MSG_OOB		0x1		/* process out-of-band data */
#define	MSG_PEEK	0x2		/* peek at incoming message */
#define	MSG_DONTROUTE	0x4		/* send without using routing tables */
/* Added for XPGv2 compliance */
#define	MSG_EOR		0x8		/* Terminates a record */
#define	MSG_CTRUNC	0x10		/* Control data truncated */
#define	MSG_TRUNC	0x20		/* Normal data truncated */
#define	MSG_WAITALL	0x40		/* Wait for complete recv or error */
/* End of XPGv2 compliance */
#define	MSG_DONTWAIT	0x80		/* Don't block for this recv */
#define	MSG_XPG4_2	0x8000		/* Private: XPG4.2 flag */

#define	MSG_MAXIOVLEN	16

/* Added for XPGv2 compliance */
#define	SHUT_RD		0
#define	SHUT_WR		1
#define	SHUT_RDWR	2

struct cmsghdr {
	socklen_t	cmsg_len;	/* data byte count, including hdr */
	int		cmsg_level;	/* originating protocol */
	int		cmsg_type;	/* protocol-specific type */
};

#if defined(_XPG4_2) || defined(_KERNEL)
#if defined(__sparc)
/* To maintain backward compatibility, alignment needs to be 8 on sparc. */
#define	_CMSG_HDR_ALIGNMENT	8
#else
/* for __i386 (and other future architectures) */
#define	_CMSG_HDR_ALIGNMENT	4
#endif	/* defined(__sparc) */
#endif	/* defined(_XPG4_2) || defined(_KERNEL) */

#if defined(_XPG4_2)
/*
 * The cmsg headers (and macros dealing with them) were made available as
 * part of UNIX95 and hence need to be protected with a _XPG4_2 define.
 */
#define	_CMSG_DATA_ALIGNMENT	(sizeof (int))
#define	_CMSG_HDR_ALIGN(x)	(((uintptr_t)(x) + _CMSG_HDR_ALIGNMENT - 1) & \
				    ~(_CMSG_HDR_ALIGNMENT - 1))
#define	_CMSG_DATA_ALIGN(x)	(((uintptr_t)(x) + _CMSG_DATA_ALIGNMENT - 1) & \
				    ~(_CMSG_DATA_ALIGNMENT - 1))
#define	CMSG_DATA(c)							\
	((unsigned char *)_CMSG_DATA_ALIGN((struct cmsghdr *)(c) + 1))
#define	CMSG_FIRSTHDR(m)	((struct cmsghdr *)((m)->msg_control))
#define	CMSG_NXTHDR(m, c)						\
	((((uintptr_t)_CMSG_HDR_ALIGN((char *)(c) +			\
	((struct cmsghdr *)(c))->cmsg_len) + sizeof (struct cmsghdr)) >	\
	(((uintptr_t)((struct msghdr *)(m))->msg_control) +		\
	((uintptr_t)((struct msghdr *)(m))->msg_controllen))) ?		\
	((struct cmsghdr *)0) :						\
	((struct cmsghdr *)_CMSG_HDR_ALIGN((char *)(c) +		\
	    ((struct cmsghdr *)(c))->cmsg_len)))
#endif	/* _XPG4_2 */

#ifdef	_XPG4_2
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname bind __xnet_bind
#pragma redefine_extname connect __xnet_connect
#pragma redefine_extname recvmsg __xnet_recvmsg
#pragma redefine_extname sendmsg __xnet_sendmsg
#pragma redefine_extname sendto __xnet_sendto
#pragma redefine_extname socket __xnet_socket
#pragma redefine_extname socketpair __xnet_socketpair
#pragma redefine_extname getsockopt __xnet_getsockopt
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	bind	__xnet_bind
#define	connect	__xnet_connect
#define	recvmsg	__xnet_recvmsg
#define	sendmsg	__xnet_sendmsg
#define	sendto	__xnet_sendto
#define	socket	__xnet_socket
#define	socketpair	__xnet_socketpair
#define	getsockopt	__xnet_getsockopt
#endif	/* __PRAGMA_REDEFINE_EXTNAME */

#endif	/* _XPG4_2 */

#if defined(_XPG4_2) && !defined(_XPG5)
#ifdef	__PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname listen __xnet_listen
#else	/* __PRAGMA_REDEFINE_EXTNAME */
#define	listen	__xnet_listen
#endif	/* __PRAGMA_REDEFINE_EXTNAME */
#endif /* (_XPG4_2) && !defined(_XPG5) */

#ifndef	_KERNEL
#ifdef	__STDC__
extern int accept(int, struct sockaddr *, Psocklen_t);
extern int bind(int, const struct sockaddr *, socklen_t);
extern int connect(int, const struct sockaddr *, socklen_t);
extern int getpeername(int, struct sockaddr *, Psocklen_t);
extern int getsockname(int, struct sockaddr *, Psocklen_t);
extern int getsockopt(int, int, int, void *, Psocklen_t);
extern int listen(int, int);	/* XXX - fixme???  where do I go */
extern int socketpair(int, int, int, int *);
extern ssize_t recv(int, void *, size_t, int);
extern ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *,
	Psocklen_t);
extern ssize_t recvmsg(int, struct msghdr *, int);
extern ssize_t send(int, const void *, size_t, int);
extern ssize_t sendmsg(int, const struct msghdr *, int);
extern ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *,
	socklen_t);
extern int setsockopt(int, int, int, const void *, socklen_t);
extern int shutdown(int, int);
extern int socket(int, int, int);
#else	/* __STDC__ */
extern int accept();
extern int bind();
extern int connect();
extern int getpeername();
extern int getsockname();
extern int getsockopt();
extern int listen();
extern int recv();
extern int recvfrom();
extern int send();
extern int sendto();
extern int setsockopt();
extern int socket();
extern int recvmsg();
extern int sendmsg();
extern int shutdown();
extern int socketpair();
#endif	/* __STDC__ */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOCKET_H */
