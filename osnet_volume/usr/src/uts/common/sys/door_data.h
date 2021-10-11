/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DOOR_DATA_H
#define	_SYS_DOOR_DATA_H

#pragma ident	"@(#)door_data.h	1.10	98/07/29 SMI"

#include <sys/types.h>
#include <sys/door.h>

#if defined(_KERNEL)
#include <sys/thread.h>
#include <sys/file.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_KERNEL)
/*
 * Data associated with a door invocation
 */

typedef struct door_data {
	door_arg_t	d_args;		/* Door arg/results */
	struct _kthread	*d_caller;	/* Door caller */
	struct _kthread *d_servers;	/* List of door servers */
	struct door_node *d_active;	/* Active door */
	struct door_node *d_pool;	/* Private pool of server threads */
	caddr_t		d_sp;		/* Saved thread stack base */
	caddr_t		d_buf;		/* Temp buffer for data transfer */
	int		d_bufsize;	/* Size of temp buffer */
	int		d_error;	/* Error (if any) */
	int		d_fpp_size;	/* Number of File ptrs */
	struct file	**d_fpp;	/* File ptrs  */
	uchar_t		d_upcall;	/* Kernel level upcall */
	uchar_t		d_noresults;	/* No results allowed */
	uchar_t		d_overflow;	/* Result overflow occured */
	uchar_t		d_flag;		/* State */
	uchar_t		d_kernel;	/* Kernel door server */
	kcondvar_t	d_cv;
} door_data_t;

/* flag values */
#define	DOOR_HOLD	0x01		/* Hold on to client/server */
#define	DOOR_WAITING	0x02		/* Client/server is waiting */
#define	DOOR_INVBOUND	0x04		/* Thread is bound to invalid door */

/*
 * Roundup buffer size when passing/returning data via kernel buffer.
 * This cuts down on the number of overflows that occur on return
 */
#define	DOOR_ROUND	128

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DOOR_DATA_H */
