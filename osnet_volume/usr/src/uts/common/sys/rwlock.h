/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_RWLOCK_H
#define	_SYS_RWLOCK_H

#pragma ident	"@(#)rwlock.h	1.9	98/02/18 SMI"

/*
 * Public interface to readers/writer locks.  See rwlock(9F) for details.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

typedef enum {
	RW_DRIVER = 2,		/* driver (DDI) rwlock */
	RW_DEFAULT = 4		/* kernel default rwlock */
} krw_type_t;

typedef enum {
	RW_WRITER,
	RW_READER
} krw_t;

typedef struct _krwlock {
	void	*_opaque[1];
} krwlock_t;

#if defined(_KERNEL)

#define	RW_READ_HELD(x)		(rw_read_held((x)))
#define	RW_WRITE_HELD(x)	(rw_write_held((x)))
#define	RW_LOCK_HELD(x)		(rw_lock_held((x)))
#define	RW_ISWRITER(x)		(rw_iswriter(x))

extern	void	rw_init(krwlock_t *, char *, krw_type_t, void *);
extern	void	rw_destroy(krwlock_t *);
extern	void	rw_enter(krwlock_t *, krw_t);
extern	int	rw_tryenter(krwlock_t *, krw_t);
extern	void	rw_exit(krwlock_t *);
extern	void	rw_downgrade(krwlock_t *);
extern	int	rw_tryupgrade(krwlock_t *);
extern	int	rw_read_held(krwlock_t *);
extern	int	rw_write_held(krwlock_t *);
extern	int	rw_lock_held(krwlock_t *);
extern	int	rw_read_locked(krwlock_t *);
extern	int	rw_iswriter(krwlock_t *);
extern	struct _kthread *rw_owner(krwlock_t *);

#endif	/* defined(_KERNEL) */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RWLOCK_H */
