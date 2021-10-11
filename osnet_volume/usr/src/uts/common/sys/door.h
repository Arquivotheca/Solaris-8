/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * The door lightweight RPC I/F.
 */

#ifndef	_SYS_DOOR_H
#define	_SYS_DOOR_H

#pragma ident	"@(#)door.h	1.21	98/10/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

#include <sys/types.h>

#if defined(_KERNEL)
#include <sys/mutex.h>
#include <sys/vnode.h>
#endif /* defined(_KERNEL) */

/* Basic door type information */
typedef unsigned long long door_ptr_t;	/* Handle 64 bit pointers */
typedef unsigned long long door_id_t;	/* Unique door identifier */
typedef	unsigned int	   door_attr_t;	/* Door attributes */

#ifdef _KERNEL
struct __door_handle;
typedef struct __door_handle *door_handle_t;	/* opaque kernel door handle */
#endif

#define	DOOR_INVAL -1			/* An invalid door descriptor */
#define	DOOR_UNREF_DATA ((void *)1)	/* Unreferenced invocation address */

/* Door descriptor passed to door_info to get current thread's binding */
#define	DOOR_QUERY -2

/*
 * Attributes associated with doors.
 */

/* Attributes originally obtained from door_create operation */
#define	DOOR_UNREF	0x01	/* Deliver an unref notification with door */
#define	DOOR_PRIVATE	0x02	/* Use a private pool of server threads */
#define	DOOR_UNREF_MULTI 0x10	/* Deliver unref notification more than once */

/* Attributes (additional) returned with door_info and door_desc_t data */
#define	DOOR_LOCAL	0x04	/* Descriptor is local to current process */
#define	DOOR_REVOKED	0x08	/* Door has been revoked */
#define	DOOR_IS_UNREF	0x20	/* Door is currently unreferenced */

/* Mask of above attributes */
#define	DOOR_ATTR_MASK	(DOOR_UNREF | DOOR_PRIVATE | DOOR_UNREF_MULTI | \
			DOOR_LOCAL | DOOR_REVOKED | DOOR_IS_UNREF)

/* Attributes used to describe door_desc_t data */
#define	DOOR_DESCRIPTOR	0x10000	/* A file descriptor is being passed */
#ifdef _KERNEL
#define	DOOR_HANDLE	0x20000 /* A kernel door handle is being passed */
#endif
#define	DOOR_RELEASE	0x40000	/* Passed references are also released */

/* Misc attributes used internally */
#define	DOOR_DELAY	0x80000	/* Delayed unref delivery */
#define	DOOR_UNREF_ACTIVE 0x100000	/* Unreferenced call is active */

/*
 * Structure used to pass descriptors/objects in door invocations
 */
typedef struct door_desc {
	door_attr_t	d_attributes;	/* Tag for union */
	union {
		/* File descriptor is passed */
		struct {
			int		d_descriptor;
			door_id_t	d_id;		/* unique id */
		} d_desc;
#ifdef _KERNEL
		/* Kernel passes handles referring to doors */
		door_handle_t d_handle;
#endif
		/* Reserved space */
		int		d_resv[5];
	} d_data;
} door_desc_t;

/*
 * Structure used to return info from door_info
 */
typedef struct door_info {
	pid_t		di_target;	/* Server process */
	door_ptr_t	di_proc;	/* Server procedure */
	door_ptr_t	di_data;	/* Data cookie */
	door_attr_t	di_attributes;	/* Attributes associated with door */
	door_id_t	di_uniquifier;	/* Unique number */
	int		di_resv[4];	/* Future use */
} door_info_t;

/*
 * Structure used to return info from door_cred
 */
typedef struct door_cred {
	uid_t	dc_euid;	/* Effective uid of client */
	gid_t	dc_egid;	/* Effective gid of client */
	uid_t	dc_ruid;	/* Real uid of client */
	gid_t	dc_rgid;	/* Real gid of client */
	pid_t	dc_pid;		/* pid of client */
	int	dc_resv[4];	/* Future use */
} door_cred_t;

/*
 * Structure used to pass/return data from door_call
 *
 * All fields are in/out paramters. Upon return these fields
 * are updated to reflect the true location and size of the results.
 */
typedef struct door_arg {
	char		*data_ptr;	/* Argument/result data */
	size_t		data_size;	/* Argument/result data size */
	door_desc_t	*desc_ptr;	/* Argument/result descriptors */
	uint_t		desc_num;	/* Argument/result num discriptors */
	char		*rbuf;		/* Result area */
	size_t		rsize;		/* Result size */
} door_arg_t;

#if defined(_SYSCALL32)
/*
 * Structure to pass/return data from 32-bit program's door_call.
 */
typedef struct door_arg32 {
	caddr32_t	data_ptr;	/* Argument/result data */
	size32_t	data_size;	/* Argument/result data size */
	caddr32_t	desc_ptr;	/* Argument/result descriptors */
	uint32_t	desc_num;	/* Argument/result num descriptors */
	caddr32_t	rbuf;		/* Result area */
	size32_t	rsize;		/* Result size */
} door_arg32_t;
#endif

#if defined(_KERNEL)

/*
 * Errors used for doors. Negative numbers to avoid conflicts with errnos
 */
#define	DOOR_WAIT	-1	/* Waiting for response */
#define	DOOR_EXIT	-2	/* Server thread has exited */

#define	VTOD(v)	((struct door_node *)(v))
#define	DTOV(d) ((struct vnode *)(d))

/*
 * Underlying 'filesystem' object definition
 */
typedef struct door_node {
	vnode_t		door_vnode;
	struct proc 	*door_target;	/* Proc handling this doors invoc's. */
	struct door_node *door_list;	/* List of active doors in proc */
	struct door_node *door_ulist;	/* Unref list */
	void		(*door_pc)();	/* Door server entry point */
	void		*door_data;	/* Cookie passed during invocations */
	door_id_t	door_index;	/* Used as a uniquifier */
	door_attr_t	door_flags;	/* State associated with door */
	uint_t		door_active;	/* Number of active invocations */
	struct _kthread	*door_servers;	/* Private pool of server threads */
} door_node_t;

/* Test if a door has been revoked */
#define	DOOR_INVALID(dp)	((dp)->door_flags & DOOR_REVOKED)

struct file;
int	door_create(void (*pc_cookie)(void *, char *, size_t, door_desc_t *,
    uint_t), void *data_cookie, uint_t);
int	door_revoke(int);
int	door_insert(struct file *, door_desc_t *);
int	door_info(int, struct door_info *);
int	door_cred(struct door_cred *);
int	door_call(int, void *);
int	door_return(caddr_t, size_t, door_desc_t *, uint_t, caddr_t);
int	door_server_dispatch(struct door_data *, door_node_t *);
int	door_bind(int);
int	door_unbind();
int	door_upcall(vnode_t *, door_arg_t *);
void	door_fd_close(door_desc_t *, uint_t);
void	door_fp_close(struct file **, uint_t);
void	door_slam(void);
void	door_exit(void);
void	door_deliver_unref(door_node_t *);
caddr_t	door_arg_addr(caddr_t, size_t, size_t);
#if defined(_SYSCALL32_IMPL)
caddr_t	door_arg_addr32(caddr_t, size_t, size_t);
#endif
kthread_t *door_get_activation(vnode_t *);
void	door_release_activation(vnode_t *, kthread_t *);
void	door_list_delete(door_node_t *);
void	door_fork(kthread_t *, kthread_t *);

extern kmutex_t door_knob;
extern kcondvar_t door_cv;
extern size_t door_max_arg;

/*
 * In-kernel doors interface.  These functions are considered Sun Private
 * and may change incompatibly in a minor release of Solaris.
 */
int	door_ki_upcall(door_handle_t, door_arg_t *);
int	door_ki_create(void (*)(void *, door_arg_t *,
    void (**)(void *, void *), void **, int *), void *, door_attr_t,
    door_handle_t *);
void	door_ki_hold(door_handle_t);
void	door_ki_rele(door_handle_t);
int	door_ki_open(char *, door_handle_t *);
int	door_ki_info(door_handle_t, door_info_t *);

#endif	/* defined(_KERNEL) */
#endif	/* !defined(_ASM) */

/*
 * System call subcodes
 */
#define	DOOR_CREATE	0
#define	DOOR_REVOKE	1
#define	DOOR_INFO	2
#define	DOOR_CALL	3
#define	DOOR_RETURN	4
#define	DOOR_CRED	5
#define	DOOR_BIND	6
#define	DOOR_UNBIND	7

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DOOR_H */
