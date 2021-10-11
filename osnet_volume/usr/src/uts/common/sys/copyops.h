/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_COPYOPS_H
#define	_SYS_COPYOPS_H

#pragma ident	"@(#)copyops.h	1.4	98/07/17 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/buf.h>
#include <sys/aio_req.h>
#include <sys/uio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

/*
 * Copy in/out vector operations.  This structure is used to interpose
 * on the kernel copyin/copyout/etc. operations for various purposes
 * such as handling watchpoints in the affected user memory.  Calls to
 * copyin, for example, will actually call the function pointed to
 * by the cp_copyin field of the copyops structure pointed to by the
 * t_copyops field in the caller's thread structure.
 *
 * Copy interpositioning does not compose; that is, there is presently
 * no way to associate multiple copyops structures with the same thread
 * in such a way that multiple copy operation functions are called
 * for each actual operation.
 *
 * The 64-bit operations in the structure fetch and store extended
 * words and are only (currently) used on platforms with 64 bit general
 * registers e.g. sun4u.
 */
typedef struct copyops {
	/*
	 * unstructured byte copyin/out functions
	 */
	int	(*cp_copyin)(const void *, void *, size_t);
	int	(*cp_xcopyin)(const void *, void *, size_t);
	int	(*cp_copyout)(const void *, void *, size_t);
	int	(*cp_xcopyout)(const void *, void *, size_t);
	int	(*cp_copyinstr)(const char *, char *, size_t, size_t *);
	int	(*cp_copyoutstr)(const char *, char *, size_t, size_t *);

	/*
	 * fetch/store byte/halfword/word/extended word
	 */
	int	(*cp_fuword8)(const void *, uint8_t *);
	int	(*cp_fuiword8)(const void *, uint8_t *);
	int	(*cp_fuword16)(const void *, uint16_t *);
	int	(*cp_fuword32)(const void *, uint32_t *);
	int	(*cp_fuiword32)(const void *, uint32_t *);
	int	(*cp_fuword64)(const void *, uint64_t *);

	int	(*cp_suword8)(void *, uint8_t);
	int	(*cp_suiword8)(void *, uint8_t);
	int	(*cp_suword16)(void *, uint16_t);
	int	(*cp_suword32)(void *, uint32_t);
	int	(*cp_suiword32)(void *, uint32_t);
	int	(*cp_suword64)(void *, uint64_t);
	int	(*cp_physio)(int (*)(struct buf *), struct buf *, dev_t,
	    int, void (*)(struct buf *), struct uio *);
} copyops_t;

#define	CP_COPYIN(uaddr, kaddr, count) \
	((*curthread->t_copyops->cp_copyin)(uaddr, kaddr, count))
#define	CP_XCOPYIN(uaddr, kaddr, count) \
	((*curthread->t_copyops->cp_xcopyin)(uaddr, kaddr, count))
#define	CP_COPYOUT(kaddr, uaddr, count) \
	((*curthread->t_copyops->cp_copyout)(kaddr, uaddr, count))
#define	CP_XCOPYOUT(kaddr, uaddr, count) \
	((*curthread->t_copyops->cp_xcopyout)(kaddr, uaddr, count))
#define	CP_COPYINSTR(uaddr, kaddr, max, lencopied) \
	((*curthread->t_copyops->cp_copyinstr)(uaddr, kaddr, max, lencopied))
#define	CP_COPYOUTSTR(kaddr, uaddr, max, lencopied) \
	((*curthread->t_copyops->cp_copyoutstr)(kaddr, uaddr, max, lencopied))

#define	CP_FUWORD8(addr, valuep)	\
	((*curthread->t_copyops->cp_fuword8)(addr, valuep))
#define	CP_FUIWORD8(addr, valuep)	\
	((*curthread->t_copyops->cp_fuiword8)(addr, valuep))
#define	CP_FUWORD16(addr, valuep)	\
	((*curthread->t_copyops->cp_fuword16)(addr, valuep))
#define	CP_FUWORD32(addr, valuep)	\
	((*curthread->t_copyops->cp_fuword32)(addr, valuep))
#define	CP_FUIWORD32(addr, valuep)	\
	((*curthread->t_copyops->cp_fuiword32)(addr, valuep))
#define	CP_FUWORD64(addr, valuep)	\
	((*curthread->t_copyops->cp_fuword64)(addr, valuep))

#define	CP_SUWORD8(addr, value)		\
	((*curthread->t_copyops->cp_suword8)(addr, value))
#define	CP_SUIWORD8(addr, value)	\
	((*curthread->t_copyops->cp_suiword8)(addr, value))
#define	CP_SUWORD16(addr, value)	\
	((*curthread->t_copyops->cp_suword16)(addr, value))
#define	CP_SUWORD32(addr, value)	\
	((*curthread->t_copyops->cp_suword32)(addr, value))
#define	CP_SUIWORD32(addr, value)	\
	((*curthread->t_copyops->cp_suiword32)(addr, value))
#define	CP_SUWORD64(addr, value)	\
	((*curthread->t_copyops->cp_suword64)(addr, value))

#define	CP_PHYSIO(strat, bp, dev, rw, mincnt, uio)		\
	((*curthread->t_copyops->cp_physio)(strat, bp, dev, rw, mincnt, uio))

/*
 * Default copy operations.  The default_copyops structure contains
 * pointers to each of the default functions that will be called if
 * there is no interpositioning.  New copyops structures can contain
 * pointers to the default functions for operations that do not need
 * special treatment.  Also, the default functions can be called
 * directly in cases where there is no need for interpositioning
 * (e.g., inside the interposing functions).
 */
extern	struct copyops default_copyops;

extern	int	default_copyin(const void *, void *, size_t);
extern	int	default_xcopyin(const void *, void *, size_t);
extern	int	default_copyout(const void *, void *, size_t);
extern	int	default_xcopyout(const void *, void *, size_t);
extern	int	default_copyinstr(const char *, char *, size_t, size_t *);
extern	int	default_copyoutstr(const char *, char *, size_t, size_t *);

extern	int	default_fuword8(const void *, uint8_t *);
extern	int	default_fuiword8(const void *, uint8_t *);
extern	int	default_fuword16(const void *, uint16_t *);
extern	int	default_fuword32(const void *, uint32_t *);
extern	int	default_fuiword32(const void *, uint32_t *);
extern	int	default_fuword64(const void *, uint64_t *);

extern	int	default_suword8(void *, uint8_t);
extern	int	default_suiword8(void *, uint8_t);
extern	int	default_suword16(void *, uint16_t);
extern	int	default_suword32(void *, uint32_t);
extern	int	default_suiword32(void *, uint32_t);
extern	int	default_suword64(void *, uint64_t);
extern int	default_physio(int (*)(struct buf *), struct buf *,
    dev_t, int, void (*)(struct buf *), struct uio *);

/*
 * Interfaces for installing and removing copyops.
 */
extern void install_copyops(kthread_id_t tp, copyops_t *cp);
extern void remove_copyops(kthread_id_t tp);
extern copyops_t *get_copyops(kthread_id_t tp);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_COPYOPS_H */
