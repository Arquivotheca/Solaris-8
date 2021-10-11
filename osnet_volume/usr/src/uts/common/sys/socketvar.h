/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef _SYS_SOCKETVAR_H
#define	_SYS_SOCKETVAR_H

#pragma ident	"@(#)socketvar.h	1.42	99/03/23 SMI"	/* SVr4.0 1.3 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/t_lock.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif



/*
 * Internal representation used for addresses.
 */
struct soaddr {
	struct sockaddr	*soa_sa;	/* Actual address */
	t_uscalar_t	soa_len;	/* Length in bytes for kmem_free */
	t_uscalar_t	soa_maxlen;	/* Allocated length */
};
/* Maximum size address for transports that have ADDR_size == 1 */
#define	SOA_DEFSIZE	128

/*
 * Internal representation of the address used to represent addresses
 * in the loopback transport for AF_UNIX. While the sockaddr_un is used
 * as the sockfs layer address for AF_UNIX the pathnames contained in
 * these addresses are not unique (due to relative pathnames) thus can not
 * be used in the transport.
 *
 * The transport level address consists of a magic number (used to separate the
 * name space for specific and implicit binds). For a specific bind
 * this is followed by a "vnode *" which ensures that all specific binds
 * have a unique transport level address. For implicit binds the latter
 * part of the address is a byte string (of the same length as a pointer)
 * that is assigned by the loopback transport.
 *
 * The uniqueness assumes that the loopback transport has a separate namespace
 * for sockets in order to avoid name conflicts with e.g. TLI use of the
 * same transport.
 */
struct soaddr_ux {
	void		*sou_vp;	/* vnode pointer or assigned by tl */
	uint_t		sou_magic;	/* See below */
};

#define	SOU_MAGIC_EXPLICIT	0x75787670	/* "uxvp" */
#define	SOU_MAGIC_IMPLICIT	0x616e6f6e	/* "anon" */

/*
 * The sonode represents a socket. A sonode never exist in the file system
 * name space and can not be opened using open() - only the socket, socketpair
 * and accept calls create sonodes.
 *
 * When an AF_UNIX socket is bound to a pathname the sockfs
 * creates a VSOCK vnode in the underlying file system. However, the vnodeops
 * etc in this VNODE remain those of the underlying file system.
 * Sockfs uses the v_stream pointer in the underlying file system VSOCK node
 * to find the sonode bound to the pathname. The bound pathname vnode
 * is accessed through so_ux_vp.
 *
 * A socket always corresponds to a VCHR stream representing the transport
 * provider (e.g. /dev/tcp). This information is retrieved from the kernel
 * socket configuration table and entered into so_accessvp. sockfs uses
 * this to perform VOP_ACCESS checks before allowing an open of the transport
 * provider.
 *
 * The locking of sockfs uses the so_lock mutex plus the SOLOCKED
 * and SOREADLOCKED flags in so_flag. The mutex protects all the state
 * in the sonode. The SOLOCKED flag is used to single-thread operations from
 * sockfs users to prevent e.g. multiple bind() calls to operate on the
 * same sonode concurrently. The SOREADLOCKED flag is used to ensure that
 * only one thread sleeps in kstrgetmsg for a given sonode. This is needed
 * to ensure atomic operation for things like MSG_WAITALL.
 *
 * Note that so_lock is sometimes held across calls that might go to sleep
 * (kmem_alloc and soallocproto*). This implies that no other lock in
 * the system should be held when calling into sockfs; from the system call
 * side or from strrput. If locks are held while calling into sockfs
 * the system might hang when running low on memory.
 */
struct sonode {
	struct	vnode so_vnode;	/* vnode associated with this sonode */
	/*
	 * These fields are initialized once.
	 */
	dev_t	so_dev;			/* device the sonode represents */
	struct	vnode *so_accessvp;	/* vnode for the /dev entry */

	/* The locks themselves */
	kmutex_t	so_lock;	/* protects sonode fields */
	kcondvar_t	so_state_cv;	/* synchronize state changes */
	kcondvar_t	so_ack_cv;	/* wait for TPI acks */
	kcondvar_t	so_connind_cv;	/* wait for T_CONN_IND */
	kcondvar_t	so_want_cv;	/* wait due to SOLOCKED */

	/* These fields are protected by so_lock */
	uint_t	so_state;		/* internal state flags SS_*, below */
	uint_t	so_mode;		/* characteristics on socket. SM_* */

	mblk_t	*so_ack_mp;		/* TPI ack received from below */
	mblk_t	*so_conn_ind_head;	/* b_next list of T_CONN_IND */
	mblk_t	*so_conn_ind_tail;
	mblk_t	*so_unbind_mp;		/* Preallocated T_UNBIND_REQ message */

	ushort_t so_flag;		/* flags, see below */
	dev_t	so_fsid;		/* file system identifier */
	time_t  so_atime;		/* time of last access */
	time_t  so_mtime;		/* time of last modification */
	time_t  so_ctime;		/* time of last attributes change */
	int	so_count;		/* count of opened references */

	/* Needed to recreate the same socket for accept */
	short	so_family;
	short	so_type;
	short	so_protocol;
	short	so_version;		/* From so_socket call */
	short	so_pushcnt;		/* Number of modules above "sockmod" */

	/* Options */
	short	so_options;		/* From socket call, see socket.h */
	struct linger	so_linger;	/* SO_LINGER value */
	int	so_sndbuf;		/* SO_SNDBUF value */
	int	so_rcvbuf;		/* SO_RCVBUF value */
#ifdef notyet
	int	so_sndlowat;		/* Not yet implemented */
	int	so_rcvlowat;		/* Not yet implemented */
	int	so_sndtimeo;		/* Not yet implemented */
	int	so_rcvtimeo;		/* Not yet implemented */
#endif /* notyet */
	ushort_t so_error;		/* error affecting connection */
	ushort_t so_delayed_error;	/* From T_uderror_ind */
	int	so_backlog;		/* Listen backlog */

	/*
	 * The counts (so_oobcnt and so_oobsigcnt) track the number of
	 * urgent indicates that are (logically) queued on the stream head
	 * read queue. The urgent data is queued on the stream head
	 * as follows.
	 *
	 * In the normal case the SIGURG is not generated until
	 * the T_EXDATA_IND arrives at the stream head. However, transports
	 * that have an early indication that urgent data is pending
	 * (e.g. TCP receiving a "new" urgent pointer value) can send up
	 * an M_PCPROTO/SIGURG message to generate the signal early.
	 *
	 * The mark is indicated by either:
	 *  - a T_EXDATA_IND (with no M_DATA b_cont) with MSGMARK set.
	 *    When this message is consumed by sorecvmsg the socket layer
	 *    sets SS_RCVATMARK until data has been consumed past the mark.
	 *  - a message with MSGMARKNEXT set (indicating that the
	 *    first byte of the next message constitutes the mark). When
	 *    the last byte of the MSGMARKNEXT message is consumed in
	 *    the stream head the stream head sets STRATMARK. This flag
	 *    is cleared when at least one byte is read. (Note that
	 *    the MSGMARKNEXT messages can be of zero length when there
	 *    is no previous data to which the marknext can be attached.)
	 *
	 * While the T_EXDATA_IND method is the common case which is used
	 * with all TPI transports, the MSGMARKNEXT method is needed to
	 * indicate the mark when e.g. the TCP urgent byte has not been
	 * received yet but the TCP urgent pointer has made TCP generate
	 * the M_PCSIG/SIGURG.
	 *
	 * The signal (the M_PCSIG carrying the SIGURG) and the mark
	 * indication can not be delivered as a single message, since
	 * the signal should be delivered as high priority and any mark
	 * indication must flow with the data. This implies that immediately
	 * when the SIGURG has been delivered if the stream head queue is
	 * empty it is impossible to determine if this will be the position
	 * of the mark. This race condition is resolved by using MSGNOTMARKNEXT
	 * messages and the STRNOTATMARK flag in the stream head. The
	 * SIOCATMARK code calls the stream head to wait for either a
	 * non-empty queue or one of the STR*ATMARK flags being set.
	 * This implies that any transport that is sending M_PCSIG(SIGURG)
	 * should send the appropriate MSGNOTMARKNEXT message (which can be
	 * zero length) after sending an M_PCSIG to prevent SIOCATMARK
	 * from sleeping unnecessarily.
	 */
	mblk_t	*so_oobmsg;		/* outofline oob data */
	uint_t	so_oobsigcnt;		/* Number of SIGURG generated */
	uint_t	so_oobcnt;		/* Number of T_EXDATA_IND queued */
	pid_t	so_pgrp;		/* pgrp for signals */

	/* From T_info_ack */
	t_uscalar_t	so_tsdu_size;
	t_uscalar_t	so_etsdu_size;
	t_scalar_t	so_addr_size;
	t_uscalar_t	so_opt_size;
	t_uscalar_t	so_tidu_size;
	t_scalar_t	so_serv_type;

	/* From T_capability_ack */
	t_uscalar_t	so_acceptor_id;

	/* Internal provider information */
	struct tpi_provinfo	*so_provinfo;

	/*
	 * The local and remote addresses have multiple purposes
	 * but one of the key reasons for their existence and careful
	 * tracking in sockfs is to support getsockname and getpeername
	 * when the transport does not handle the TI_GET*NAME ioctls.
	 * When all transports support the new TPI (with T_ADDR_REQ)
	 * we can revisit this code.
	 * The other usage of so_faddr is to keep the "connected to"
	 * address for datagram sockets.
	 * Finally, for AF_UNIX both local and remote addresses are used
	 * to record the sockaddr_un since we use a separate namespace
	 * in the loopback transport.
	 */
	struct soaddr so_laddr;		/* Local address */
	struct soaddr so_faddr;		/* Peer address */
#define	so_laddr_sa	so_laddr.soa_sa
#define	so_faddr_sa	so_faddr.soa_sa
#define	so_laddr_len	so_laddr.soa_len
#define	so_faddr_len	so_faddr.soa_len
#define	so_laddr_maxlen	so_laddr.soa_maxlen
#define	so_faddr_maxlen	so_faddr.soa_maxlen
	mblk_t		*so_eaddr_mp;	/* for so_delayed_error */

	/*
	 * For AF_UNIX sockets:
	 * so_ux_laddr/faddr records the internal addresses used with the
	 * transport.
	 * so_ux_vp and v_stream->sd_vnode form the cross-
	 * linkage between the underlying fs vnode corresponding to
	 * the bound sockaddr_un and the socket node.
	 */
	struct soaddr_ux so_ux_laddr;	/* laddr bound with the transport */
	struct soaddr_ux so_ux_faddr;	/* temporary peer address */
	struct vnode	*so_ux_bound_vp; /* bound AF_UNIX file system vnode */
	struct sonode	*so_next;	/* next sonode on socklist	*/
	struct sonode	*so_prev;	/* previous sonode on socklist	*/
};

/* flags */
#define	SOMOD		0x01		/* update socket modification time */
#define	SOACC		0x02		/* update socket access time */

#define	SOLOCKED	0x10		/* use to serialize open/closes */
#define	SOREADLOCKED	0x20		/* serialize kstrgetmsg calls */
#define	SOWANT		0x40		/* some process waiting on lock */
#define	SOCLONE		0x80		/* child of clone driver */

/*
 * Socket state bits.
 */
#define	SS_ISCONNECTED		0x001	/* socket connected to a peer */
#define	SS_ISCONNECTING		0x002	/* in process of connecting to peer */
#define	SS_ISDISCONNECTING	0x004	/* in process of disconnecting */
#define	SS_CANTSENDMORE		0x008	/* can't send more data to peer */

#define	SS_CANTRCVMORE		0x010	/* can't receive more data from peer */
#define	SS_ISBOUND		0x020	/* socket is bound */
#define	SS_NDELAY		0x040	/* FNDELAY non-blocking */
#define	SS_NONBLOCK		0x080	/* O_NONBLOCK non-blocking */

#define	SS_ASYNC		0x100	/* async i/o notify */
#define	SS_ACCEPTCONN		0x200	/* listen done */
#define	SS_HASCONNIND		0x400	/* T_CONN_IND for poll */
#define	SS_SAVEDEOR		0x800	/* Saved MSG_EOR receive side state */

#define	SS_RCVATMARK		0x1000	/* at mark on input */
#define	SS_OOBPEND		0x2000	/* OOB pending or present - poll */
#define	SS_HAVEOOBDATA		0x4000	/* OOB data present */
#define	SS_HADOOBDATA		0x8000	/* OOB data consumed */

#define	SS_FADDR_NOXLATE	0x20000	/* No xlation of faddr for AF_UNIX */
#define	SS_WUNBIND		0x40000	/* Waiting for ack of unbind */

/* State bits that can be changed using SO_STATE setsockopt */
#define	SS_CANCHANGE		(SS_ASYNC)

/* Set of states when the socket can't be rebound */
#define	SS_CANTREBIND	(SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING|\
			    SS_CANTSENDMORE|SS_CANTRCVMORE|SS_ACCEPTCONN)

/*
 * Characteristics of sockets. Not changed after the socket is created.
 */
#define	SM_PRIV			0x001	/* privileged for broadcast, raw... */
#define	SM_ATOMIC		0x002	/* atomic data transmission */
#define	SM_ADDR			0x004	/* addresses given with messages */
#define	SM_CONNREQUIRED		0x008	/* connection required by protocol */

#define	SM_FDPASSING		0x010	/* passes file descriptors */
#define	SM_EXDATA		0x020	/* Can handle T_EXDATA_REQ */
#define	SM_OPTDATA		0x040	/* Can handle T_OPTDATA_REQ */
#define	SM_BYTESTREAM		0x080	/* Byte stream - can use M_DATA */

#define	SM_ACCEPTOR_ID		0x100	/* so_acceptor_id is valid */

/*
 * Socket versions. Used by the socket library when calling _so_socket().
 */
#define	SOV_STREAM	0	/* Not a socket - just a stream */
#define	SOV_DEFAULT	1	/* Select based on so_default_version */
#define	SOV_SOCKSTREAM	2	/* Socket plus streams operations */
#define	SOV_SOCKBSD	3	/* Socket with no streams operations */
#define	SOV_XPG4_2	4	/* Xnet socket */

#if defined(_KERNEL) || defined(_KMEMUSER)
/*
 * Used for mapping family/type/protocol to vnode.
 * Defined here so that crash can use it.
 */
struct sockparams {
	int	sp_domain;
	int	sp_type;
	int	sp_protocol;
	char	*sp_devpath;
	int	sp_devpathlen;	/* Is 0 if sp_devpath is a static string */
	vnode_t	*sp_vnode;
	struct sockparams *sp_next;
};

extern struct sockparams *sphead;

/*
 * Used to traverse the list of AF_UNIX sockets to construct the kstat
 * for netstat(1m).
 */
struct socklist {
	kmutex_t	sl_lock;
	struct sonode	*sl_list;
};

extern struct socklist socklist;

#endif /* defined(_KERNEL) || defined(_KMEMUSER) */

#ifdef _KERNEL

#define	ISALIGNED_cmsghdr(addr) \
		(((uintptr_t)(addr) & (_CMSG_HDR_ALIGNMENT - 1)) == 0)

#define	ROUNDUP_cmsglen(len) \
	(((len) + _CMSG_HDR_ALIGNMENT - 1) & ~(_CMSG_HDR_ALIGNMENT - 1))

/*
 * Maximum size of any argument that is copied in (addresses, options,
 * access rights). MUST be at least MAXPATHLEN + 3.
 * BSD and SunOS 4.X limited this to MLEN or MCLBYTES.
 */
#define	SO_MAXARGSIZE	8192

/*
 * Convert between vnode and sonode
 */
#define	VTOSO(vp)	((struct sonode *)((vp)->v_data))
#define	SOTOV(sp)	(&(sp)->so_vnode)

/*
 * Internal flags for sobind()
 */
#define	_SOBIND_REBIND		0x01	/* Bind to existing local address */
#define	_SOBIND_UNSPEC		0x02	/* Bind to unspecified address */
#define	_SOBIND_LOCK_HELD	0x04	/* so_excl_lock held by caller */
#define	_SOBIND_NOXLATE		0x08	/* No addr translation for AF_UNIX */
#define	_SOBIND_XPG4_2		0x10	/* xpg4.2 semantics */
#define	_SOBIND_SOCKBSD		0x20	/* BSD semantics */
#define	_SOBIND_LISTEN		0x40	/* Make into SS_ACCEPTCONN */

/*
 * Internal flags for sounbind()
 */
#define	_SOUNBIND_REBIND	0x01	/* Don't clear fields - will rebind */
#define	_SOUNBIND_LOCK_HELD	0x02	/* so_excl_lock held by caller */

/*
 * Internal flags for soconnect()
 */
#define	_SOCONNECT_NOXLATE	0x01	/* No addr translation for AF_UNIX */
#define	_SOCONNECT_DID_BIND	0x02	/* Unbind when connect fails */
#define	_SOCONNECT_XPG4_2	0x04	/* xpg4.2 semantics */

/*
 * Internal flags for sodisconnect()
 */
#define	_SODISCONNECT_LOCK_HELD	0x01	/* so_excl_lock held by caller */

/*
 * Internal flags for sogetsockopt().
 */
#define	_SOGETSOCKOPT_XPG4_2	0x01	/* xpg4.2 semantics */

/*
 * Internal flags for soallocproto*()
 */
#define	_ALLOC_NOSLEEP		0	/* Don't sleep for memory */
#define	_ALLOC_INTR		1	/* Sleep until interrupt */
#define	_ALLOC_SLEEP		2	/* Sleep forever */

/*
 * Internal structure for handling AF_UNIX file descriptor passing
 */
struct fdbuf {
	int		fd_size;	/* In bytes, for kmem_free */
	int		fd_numfd;	/* Number of elements below */
	char		*fd_ebuf;	/* Extra buffer to free  */
	int		fd_ebuflen;
	frtn_t		fd_frtn;
	struct file	*fd_fds[1];	/* One or more */
};
#define	FDBUF_HDRSIZE	(sizeof (struct fdbuf) - sizeof (struct file *))

/*
 * Variable that can be patched to set what version of socket socket()
 * will create.
 */
extern int so_default_version;

#ifdef DEBUG
/* Turn on extra testing capabilities */
#define	SOCK_TEST
#endif /* DEBUG */

#ifdef DEBUG
char	*pr_state(uint_t, uint_t);
char	*pr_addr(int, struct sockaddr *, t_uscalar_t);
int	so_verify_oobstate(struct sonode *);
#endif /* DEBUG */

/*
 * DEBUG macros
 */
#if defined(DEBUG) && !defined(lint)
#define	SOCK_DEBUG

extern int sockdebug;
extern int sockprinterr;

#define	eprint(args)	printf args
#define	eprintso(so, args) \
{ if (sockprinterr && ((so)->so_options & SO_DEBUG)) printf args; }
#define	eprintline(errno)					\
{								\
	if (errno != EINTR && (sockprinterr || sockdebug > 0))	\
		printf("socket error %d: line %d file %s\n",	\
			(errno), __LINE__, __FILE__);		\
}

#define	eprintsoline(so, errno)					\
{ if (sockprinterr && ((so)->so_options & SO_DEBUG))		\
	printf("socket(%p) error %d: line %d file %s\n",	\
		(so), (errno), __LINE__, __FILE__);		\
}
#define	dprint(level, args)	{ if (sockdebug > (level)) printf args; }
#define	dprintso(so, level, args) \
{ if (sockdebug > (level) && ((so)->so_options & SO_DEBUG)) printf args; }

#else /* define(DEBUG) && !defined(lint) */

#define	eprint(args)		{}
#define	eprintso(so, args)	{}
#define	eprintline(error)	{}
#define	eprintsoline(so, errno)	{}
#define	dprint(level, args)	{}
#define	dprintso(so, level, args) {}
#ifdef DEBUG
#undef DEBUG
#endif

#endif /* defined(DEBUG) && !defined(lint) */

extern struct vfsops sock_vfsops;
extern struct kmem_cache *sock_cache;

/*
 * sockfs functions
 */
int		sock_getmsg(vnode_t *, struct strbuf *, struct strbuf *,
			uchar_t *, int *, int, rval_t *);
int		sock_putmsg(vnode_t *, struct strbuf *, struct strbuf *,
			uchar_t, int, int);
struct sonode	*socreate(vnode_t *, int, int, int, int, struct sonode *,
			int *);
int		sock_open(struct vnode **, int, struct cred *);
struct vnodeops	*sock_getvnodeops(void);
void		so_sock2stream(struct sonode *);
void		so_stream2sock(struct sonode *);
int		sockinit(struct vfssw *, int);
struct vnode	*makesockvp(struct vnode *, int, int, int);
void		sockfree(struct sonode *);
void		so_update_attrs(struct sonode *, int);
int		soconfig(int, int, int,	char *, int);
struct vnode	*solookup(int, int, int, char *, int *);
int		so_lock_single(struct sonode *, int, int);
int		so_lock_intr(struct sonode *, int, int);
void		so_unlock_single(struct sonode *, int);
void		*sogetoff(mblk_t *, t_uscalar_t, t_uscalar_t, uint_t);
void		so_getopt_srcaddr(void *, t_uscalar_t,
			void **, t_uscalar_t *);
int		so_getopt_unix_close(void *, t_uscalar_t);
int		so_addr_verify(struct sonode *, const struct sockaddr *,
			socklen_t);
int		so_ux_addr_xlate(struct sonode *, struct sockaddr *,
			socklen_t, int, void **, socklen_t *);
void		fdbuf_free(struct fdbuf *);
mblk_t		*fdbuf_allocmsg(int, struct fdbuf *);
int		fdbuf_create(void *, int, struct fdbuf **);
void		so_closefds(void *, t_uscalar_t, int, int);
int		so_getfdopt(void *, t_uscalar_t, int, void **, int *);
t_uscalar_t	so_optlen(void *, t_uscalar_t, int);
void		so_cmsg2opt(void *, t_uscalar_t, int, mblk_t *);
t_uscalar_t	so_cmsglen(mblk_t *, void *, t_uscalar_t, int);
int		so_opt2cmsg(mblk_t *, void *, t_uscalar_t, int,
			void *, t_uscalar_t);
void		soisconnecting(struct sonode *);
void		soisconnected(struct sonode *);
void		soisdisconnected(struct sonode *, int);
void		socantsendmore(struct sonode *);
void		socantrcvmore(struct sonode *);
void		soseterror(struct sonode *, int);
int		sogeterr(struct sonode *, int);
int		sogetrderr(vnode_t *, int, int *);
int		sogetwrerr(vnode_t *, int, int *);
void		so_unix_close(struct sonode *);
mblk_t		*soallocproto(size_t, int);
mblk_t		*soallocproto1(const void *, ssize_t, ssize_t, int);
void		soappendmsg(mblk_t *, const void *, ssize_t);
mblk_t		*soallocproto2(const void *, ssize_t, const void *, ssize_t,
			ssize_t, int);
mblk_t		*soallocproto3(const void *, ssize_t, const void *, ssize_t,
			const void *, ssize_t, ssize_t, int);
int		sowaitprim(struct sonode *, t_scalar_t, t_scalar_t,
			t_uscalar_t, mblk_t **, clock_t);
int		sowaitokack(struct sonode *, t_scalar_t);
int		sowaitack(struct sonode *, mblk_t **, clock_t);
void		soqueueack(struct sonode *, mblk_t *);
int		sowaitconnind(struct sonode *, int, mblk_t **);
void		soqueueconnind(struct sonode *, mblk_t *);
int		soflushconnind(struct sonode *, t_scalar_t);
int		sowaitconnected(struct sonode *, int, int);

int		sosend_dgram(struct sonode *, struct sockaddr *,
			socklen_t, struct uio *, int);
int		sosend_svc(struct sonode *, struct uio *, t_scalar_t, int, int);
int		so_strinit(struct sonode *, struct sonode *);
int		sobind(struct sonode *, struct sockaddr *,
			socklen_t, int, int);
int		sounbind(struct sonode *, int);
void		sounbind_nonblock(struct sonode *);
int		solisten(struct sonode *, int);
int		soaccept(struct sonode *, int, struct sonode **);
int		soconnect(struct sonode *, const struct sockaddr *,
			socklen_t, int, int);
int		sodisconnect(struct sonode *, t_scalar_t, int);
int		soshutdown(struct sonode *, int);
int		sorecvmsg(struct sonode *, struct nmsghdr *, struct uio *);
int		sosendmsg(struct sonode *, struct nmsghdr *, struct uio *);
int		sogetpeername(struct sonode *);
int		sogetsockname(struct sonode *);
int		sogetsockopt(struct sonode *, int, int,
			void *, socklen_t *, int);
int		sosetsockopt(struct sonode *, int, int, const void *,
			t_uscalar_t);

#endif

/*
 * Internal structure for obtaining sonode information from the socklist.
 * These types match those corresponding in the sonode structure.
 * This is not a published interface, and may change at any time.
 */
struct sockinfo {
	uint_t		si_size;		/* real length of this struct */
	short		si_family;
	short		si_type;
	ushort_t	si_flag;
	uint_t		si_state;
	uint_t		si_ux_laddr_sou_magic;
	uint_t		si_ux_faddr_sou_magic;
	t_scalar_t	si_serv_type;
	t_uscalar_t	si_laddr_soa_len;
	t_uscalar_t	si_faddr_soa_len;
	uint16_t	si_laddr_family;
	uint16_t	si_faddr_family;
	char		si_laddr_sun_path[MAXPATHLEN + 1]; /* NULL terminated */
	char		si_faddr_sun_path[MAXPATHLEN + 1];
};


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOCKETVAR_H */
