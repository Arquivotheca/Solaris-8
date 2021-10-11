/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef _SYS_DR_H
#define	_SYS_DR_H

#pragma ident	"@(#)dr.h	1.22	98/08/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/obpdefs.h>
#include <sys/memlist.h>
#ifdef _KERNEL
#include <sys/mem_config.h>
#endif /* _KERNEL */

/*
 * Contains "generic" definitions used/exported by the PIM-DR layer
 * to the PSM-DR layer.
 */

/*
 * Length of error string.  Used to hold pathname of failed
 * devices, etc.
 */
#define	MAXPATHLEN	1024

#ifdef _KERNEL
/*
 * generic board structure
 */
typedef struct board {
	kmutex_t	b_lock;		/* lock for this board struct */
} board_t;

#define	DR_BOARD_LOCK(b)	(mutex_enter(&(b)->b_lock))
#define	DR_BOARD_UNLOCK(b)	(mutex_exit(&(b)->b_lock))
#define	DR_BOARD_LOCK_HELD(b)	(MUTEX_HELD(&(b)->b_lock))

/*
 * generic flags managed at PIM-DR (dr_handle_t.h_flags)
 */
#define	DR_FLAG_DEVI_FORCE	0x00000001
#define	DR_FLAG_DEVI_REMOVE	0x00000002
#define	DR_USER_FLAG_MASK	0x0000ffff

#define	DR_KERN_FLAG_MASK	0xffff0000	/* no flags in use */

typedef uint32_t	dr_flags_t;
#endif /* _KERNEL */

typedef struct dr_error {
	int32_t		e_errno;
	char		*e_str;
} dr_error_t;

/*
 * generic device node type
 */
typedef enum {
	DR_NT_UNKNOWN,
	DR_NT_CPU,
	DR_NT_MEM,
	DR_NT_IO
} dr_nodetype_t;

#ifdef _KERNEL

typedef struct {
	dr_error_t	dv_error;
	dnode_t		dv_nodeid;
} dr_devlist_t;

/*
 * generic handle to communicate information between PIM-DR and PSM-DR.
 */
struct dr_ops;

typedef struct dr_handle {
	struct board	*h_bd;
	dr_error_t	*h_err;
	dev_t		h_dev;		/* dev_t of opened device */
	int		h_cmd;		/* PIM ioctl argument */
	int		h_mode;		/* device open mode */
	dr_flags_t	h_flags;
} dr_handle_t;

#define	DR_HD2ERR(hd)		((hd)->h_err)
#define	DR_GET_ERRNO(ep)	((ep)->e_errno)
#define	DR_SET_ERRNO(ep, en)	((ep)->e_errno = (en))
#define	DR_GET_ERRSTR(ep)	((ep)->e_str)
#define	DR_SET_ERRSTR(ep, es)	(strncpy((ep)->e_str, (es), MAXPATHLEN))
#define	DR_ERR_ISALLOC(ep)	((ep)->e_str != NULL)

#define	DR_ALLOC_ERR(ep) \
	((ep)->e_errno = 0, \
	(ep)->e_str = GETSTRUCT(char, MAXPATHLEN), \
	*((ep)->e_str) = 0)
#define	DR_FREE_ERR(ep) \
	(FREESTRUCT((ep)->e_str, char, MAXPATHLEN), (ep)->e_str = NULL)

/*
 * vector of operations provided by PSM-DR layer.
 */
typedef struct dr_ops {
	dr_handle_t	*(*dr_platform_get_handle)(dev_t, void *,
						intptr_t,
						int (*)(dr_handle_t *,
							void *,
							board_t *,
							dr_error_t *),
						void *);

	void	(*dr_platform_release_handle)(dr_handle_t *,
						void (*)(dr_handle_t *));

	int	(*dr_platform_pre_op)(dr_handle_t *);

	void	(*dr_platform_post_op)(dr_handle_t *);

	int	(*dr_platform_probe_board)(dr_handle_t *);

	int	(*dr_platform_deprobe_board)(dr_handle_t *);

	int	(*dr_platform_connect)(dr_handle_t *);

	int	(*dr_platform_disconnect)(dr_handle_t *);

	dr_devlist_t	*(*dr_platform_get_attach_devlist)(dr_handle_t *,
						int32_t *, int32_t);

	int	(*dr_platform_pre_attach_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	int	(*dr_platform_post_attach_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	dr_devlist_t	*(*dr_platform_get_release_devlist)(dr_handle_t *,
						int32_t *, int32_t);

	int	(*dr_platform_pre_release_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	int	(*dr_platform_post_release_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	boolean_t	(*dr_platform_release_done)(dr_handle_t *,
						dr_nodetype_t, dnode_t);

	dr_devlist_t	*(*dr_platform_get_detach_devlist)(dr_handle_t *,
						int32_t *, int32_t);

	int	(*dr_platform_pre_detach_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	int	(*dr_platform_post_detach_devlist)(dr_handle_t *,
						dr_devlist_t *, int32_t);

	int	(*dr_platform_cancel)(dr_handle_t *);

	int	(*dr_platform_status)(dr_handle_t *);

	int	(*dr_platform_ioctl)(dr_handle_t *);

	int	(*dr_platform_get_memhandle)(dr_handle_t *,
						dnode_t, memhandle_t *);

	struct memlist	*(*dr_platform_get_memlist)(dnode_t);

	int	(*dr_platform_detach_mem)(dr_handle_t *, dnode_t);

	dr_nodetype_t	(*dr_platform_get_devtype)(dr_handle_t *, dnode_t);

	processorid_t	(*dr_platform_get_cpuid)(dr_handle_t *, dnode_t);

	int	(*dr_platform_make_nodes)(dev_info_t *);
} dr_ops_t;

/*
 * dev_t is shared by PIM and PSM layers.
 *
 * Format = 31......16,15.......0
 *	    |   PIM   |   PSM   |
 */
#define	_DR_DEVPIM_SHIFT	16
#define	_DR_DEVPIM_MASK		0xffff
#define	_DR_DEVPSM_MASK		0xffff

#define	DR_GET_MINOR2INST(d)	(((d) >> _DR_DEVPIM_SHIFT) & _DR_DEVPIM_MASK)
#define	DR_MAKE_MINOR(i, m) \
			((((i) & _DR_DEVPIM_MASK) << _DR_DEVPIM_SHIFT) | \
			((m) & _DR_DEVPSM_MASK))

#define	GETSTRUCT(t, n) \
		((t *)kmem_zalloc((size_t)(n) * sizeof (t), KM_SLEEP))
#define	FREESTRUCT(p, t, n) \
		(kmem_free((caddr_t)(p), sizeof (t) * (size_t)(n)))

#endif /* _KERNEL */
/*
 * generic ioctl commands understood by PIM-DR layer driver.
 */
#define	_DR_IOC		(('D' << 16) | ('R' << 8))

#define	DR_CMD_IOCTL		(_DR_IOC | 0x01)
#define	DR_CMD_CONNECT		(_DR_IOC | 0x02)
#define	DR_CMD_DISCONNECT	(_DR_IOC | 0x03)
#define	DR_CMD_CONFIGURE	(_DR_IOC | 0x04)
#define	DR_CMD_UNCONFIGURE	(_DR_IOC | 0x05)
#define	DR_CMD_RELEASE		(_DR_IOC | 0x06)
#define	DR_CMD_CANCEL		(_DR_IOC | 0x07)
#define	DR_CMD_STATUS		(_DR_IOC | 0x08)

#ifdef _KERNEL
/*
 * Required PSM-DR entry points to be used directly by PIM-DR layer.
 */
extern int	dr_platform_init(void **, dev_info_t *, dr_ops_t **);
extern int	dr_platform_fini(void **);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DR_H */
