/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_SESSION_H
#define	_SYS_SESSION_H

#pragma ident	"@(#)session.h	1.14	98/09/30 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct sess {
	short s_ref; 			/* reference count */
	short s_mode;			/* /sess current permissions */
	uid_t s_uid;			/* /sess current user ID */
	gid_t s_gid;			/* /sess current group ID */
	time_t s_ctime;			/* /sess change time */
	dev_t s_dev;			/* tty's device number */
	struct vnode *s_vp;		/* tty's vnode */
	struct pid *s_sidp;		/* session ID info */
	struct cred *s_cred;		/* allocation credentials */
	kmutex_t s_lock;		/* sync s_vp use with freectty */
	kcondvar_t s_wait_cv;		/* Condvar for sleeping */
	int s_cnt;			/* # of active users of this session */
	int s_flag;			/* session state flag see below */
} sess_t;

#define	SESS_CLOSE	1		/* session about to close */
#define	s_sid s_sidp->pid_id

/*
 * Enumeration of the types of access that can be requested for a
 * controlling terminal under job control.
 */

enum jcaccess {
	JCREAD,			/* read data on a ctty */
	JCWRITE,		/* write data to a ctty */
	JCSETP,			/* set ctty parameters */
	JCGETP			/* get ctty parameters */
};

#if defined(_KERNEL)

extern sess_t session0;

#define	SESS_HOLD(sp)	(++(sp)->s_ref)
#define	SESS_RELE(sp)	sess_rele(sp)

/*
 * Used to synchronizing sessions vnode users with freectty
 */

#define	TTY_HOLD(sp)	{ \
	mutex_enter(&(sp)->s_lock); \
	(++(sp)->s_cnt); \
	mutex_exit(&(sp)->s_lock); \
}

#define	TTY_RELE(sp)	{ \
	mutex_enter(&(sp)->s_lock); \
	if ((--(sp)->s_cnt) == 0) \
		cv_signal(&(sp)->s_wait_cv); \
	mutex_exit(&(sp)->s_lock); \
}

/* forward referenced structure tags */
struct vnode;
struct cred;
struct proc;

extern void sess_rele(sess_t *);
extern void sess_create(void);
extern void freectty(sess_t *);
extern void alloctty(struct proc *, struct vnode *);
extern int realloctty(struct proc *, pid_t);
extern dev_t cttydev(struct proc *);
extern int hascttyperm(sess_t *, struct cred *, mode_t);

#endif /* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SESSION_H */
