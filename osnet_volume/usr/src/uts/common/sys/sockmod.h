/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

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
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _SYS_SOCKMOD_H
#define	_SYS_SOCKMOD_H

#pragma ident	"@(#)sockmod.h	1.40	96/04/04 SMI"   /* SVr4.0 1.13	*/

#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/un.h>	/* for sockaddr_un */

#ifdef	__cplusplus
extern "C" {
#endif

/* internal flags - in addition to the ones in timod.h */
#define		S_WINFO		0x01000	/* waiting for T_info to complete */
#define		S_WRDISABLE	0x02000	/* write service queue disabled */
/*
 * Waiting on T_OK_ACK for T_UNBIND_REQ
 */
#define		S_WUNBIND	0x04000
/*
 * Processing T_DISCON_IND draining all messages
 */
#define		S_IGNORE_FLOW	0x08000
/*
 * Sent T_UNBIND_REQ on receiving T_DISCON_IND
 */
#define		S_WUNBIND_DISCON	0x10000

/*
 * Waiting to free the so_so, but have pending esballoc'ed msgs.
 * Set in closeflag which is protected by a separate so_global_lock.
 */
#define		SC_WCLOSE	0x20000

/*
 * Processing PF_INET SI_LISTEN.
 */
#define		S_INET_LISTEN	0x40000

/* socket module ioctls */
#define		SIMOD 		('I'<<8)

/*
 * The following are ioctl handled specially by the socket
 * module which were not handled by timod.
 */
#define		O_SI_GETUDATA		(SIMOD|101)
#define		SI_SHUTDOWN		(SIMOD|102)
#define		SI_LISTEN		(SIMOD|103)
#define		SI_SETMYNAME		(SIMOD|104)
#define		SI_SETPEERNAME		(SIMOD|105)
#define		SI_GETINTRANSIT		(SIMOD|106)
#define		SI_TCL_LINK		(SIMOD|107)
#define		SI_TCL_UNLINK		(SIMOD|108)
#define		SI_SOCKPARAMS		(SIMOD|109)
#define		SI_GETUDATA		(SIMOD|110)


/*
 * used by O_SI_GETUDATA (for compatability to
 * old statically linked apps)
 */

struct o_si_udata {
	int	tidusize;	/* TIDU size		*/
	int	addrsize;	/* address size		*/
	int	optsize;	/* options size		*/
	int	etsdusize;	/* expedited size	*/
	int	servtype;	/* service type		*/
	int	so_state;	/* socket states	*/
	int	so_options;	/* socket options	*/
	int	tsdusize;	/* TSDU size		*/
};

/*
 * used by SI_SOCKPARAMS ioctl
 */

struct si_sockparams {
	int	sp_family;		/* socket addr family */
	int	sp_type;		/* socket type */
	int	sp_protocol;		/* socket proto family */
};

struct si_udata {
	int	tidusize;	/* TIDU size		*/
	int	addrsize;	/* address size		*/
	int	optsize;	/* options size		*/
	int	etsdusize;	/* expedited size	*/
	int	servtype;	/* service type		*/
	int	so_state;	/* socket states	*/
	int	so_options;	/* socket options	*/
	int	tsdusize;	/* TSDU size		*/
	struct	si_sockparams	sockparams; /* Socket parameters */
};

struct _si_user {
	struct	_si_user 	*next;		/* next one		*/
	struct	_si_user 	*prev;		/* previous one		*/
	int			fd;		/* file descripter	*/
	int			ctlsize;	/* ctl buffer size	*/
	char			*ctlbuf;	/* ctl buffer		*/
	struct	si_udata	udata;		/* socket info		*/
	int			flags;
	int			fflags;		/* fcntl(GETFL) flags	*/
#ifdef _REENTRANT
	mutex_t			lock;
#endif /* _REENTRANT */
};

/*
 * Flag bits.
 */
#define		S_SIGIO		0x1	/* If set, user has SIGIO enabled */
#define		S_SIGURG	0x2	/* If set, user has SIGURG enabled */

/*
 * Used for the tortuous UNIX domain
 * naming.
 */
struct ux_dev {
	dev_t	dev;
	ino_t	ino;
};

struct ux_extaddr {
	size_t	size;				/* Size of following address */
	union	{
		struct ux_dev	tu_addr;	/* User selected address */
		int		tp_addr;	/* TP selected address */
	} addr;
};
#define		extdev		ux_extaddr.addr.tu_addr.dev
#define		extino		ux_extaddr.addr.tu_addr.ino
#define		extsize		ux_extaddr.size
#define		extaddr		ux_extaddr.addr

struct bind_ux {
	struct	sockaddr_un	name;
	struct	ux_extaddr	ux_extaddr;
};

/*
 * Doubly linked list of so_so that
 * represent a UNIX domain socket.
 */
struct so_ux {
	struct so_so *next;
	struct so_so *prev;
};

/*
 * The following fiedls are protected by the global so_global_lock
 * for all instances. (note: so_global_lock protects more then this)
 *      closeflags
 *      so_cbacks_outstanding
 *      so_cbacks_inprogress
 *      so_isaccepting
 *	so_acceptor
 *      so_next
 *      *so_ptpn
 * so_isaccepting and so_acceptor forms a relationship between
 * the listener and the accepting instance.
 */

struct so_so {
	long 			flags;
	long			closeflags;
	queue_t			*rdq;
	mblk_t  		*iocsave;
	struct	t_info		tp_info;
	struct	netbuf		raddr;
	struct	netbuf		laddr;
	struct	ux_extaddr	lux_dev;
	struct	ux_extaddr	rux_dev;
	int			so_error;
	mblk_t			*oob;
	struct	so_so		*so_conn;	/* Only used by netstat */
	mblk_t 			*consave;
	struct	si_udata	udata;
	int			so_option;
	mblk_t  		*bigmsg;
	struct	so_ux		so_ux;
	int			hasoutofband;
	mblk_t			*urg_msg;
	int			sndbuf;
	int			rcvbuf;
	int			sndlowat;
	int			rcvlowat;
	int			linger;
	int			sndtimeo;
	int			rcvtimeo;
	int			prototype;
	int			so_cbacks_outstanding;
	int			so_cbacks_inprogress;
	struct so_so		*so_next;
	struct so_so		**so_ptpn;
	int			wbufcid;
	int			rbufcid;
	int			wtimoutid;
	int			rtimoutid;
	queue_t			*so_driverq;
	int			so_isaccepting;
	struct so_so		*so_acceptor;
	int			so_id;	/* For strlog only */
};

#ifdef __STDC__
extern struct _si_user 	*_s_checkfd(int);
extern struct _si_user 	*_s_socreate(int, int, int);
extern void 		_s_aligned_copy(char *, int, int, char *, int *);
extern struct netconfig	*_s_match(int, int, int, void **);
extern int		_s_sosend(struct _si_user *, struct msghdr *, int);
extern int		_s_soreceive(struct _si_user *, struct msghdr *, int);
extern int 		_s_synch(struct _si_user *);
extern int 		_s_is_ok(struct _si_user *, long, struct strbuf *);
extern int 		_s_do_ioctl(int, char *, int, int, int *);
extern int		_s_min(int, int);
extern int		_s_max(int, int);
extern void		_s_close(struct _si_user *);
extern int		_s_getfamily(struct _si_user *);
extern int		_s_uxpathlen(struct sockaddr_un *);
extern void		_s_blockallsignals(sigset_t *);
extern void		_s_restoresigmask(sigset_t *);
extern int		_s_cbuf_alloc(struct _si_user *, char **);


#else

extern struct _si_user	*_s_checkfd();
extern struct _si_user	*_s_open();
extern void		_s_aligned_copy();
extern struct netconfig	*_s_match();
extern int		_s_sosend();
extern int		_s_soreceive();
extern int		_s_synch();
extern int		_s_is_ok();
extern int		_s_do_ioctl();
extern int		_s_min();
extern int		_s_max();
extern void		_s_close();
extern int		_s_getfamily();
extern int		_s_uxpathlen();
extern void		_s_blockallsignals();
extern void		_s_restoresigmask();
extern int		_s_cbuf_alloc();

#endif	/* __STDC__ */

/*
 * Socket library debugging
 */
extern int		_s_sockdebug;
#define	SOCKDEBUG(S, A, B)	\
			if ((((S) && (S)->udata.so_options & SO_DEBUG)) || \
						_s_sockdebug) { \
				(void) _syslog(LOG_ERR, (A), (B)); \
			}

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOCKMOD_H */
