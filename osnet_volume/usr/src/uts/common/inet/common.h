/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_COMMON_H
#define	_INET_COMMON_H

#pragma ident	"@(#)common.h	1.22	98/07/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/inttypes.h>

#define	A_CNT(arr)	(sizeof (arr)/sizeof (arr[0]))
#define	A_END(arr)	(&arr[A_CNT(arr)])
#define	A_LAST(arr)	(&arr[A_CNT(arr)-1])

#define	fallthru	/*FALLTHRU*/
#define	getarg(ac, av)	(optind < ac ? av[optind++] : nilp(char))
#ifndef	MAX
#define	MAX(x1, x2)	((x1) >= (x2) ? (x1) : (x2))
#endif
#ifndef	MIN
#define	MIN(x1, x2)	((x1) <= (x2) ? (x1) : (x2))
#endif

/*
 * The MAX_XXX and MIN_XXX defines assume a two's complement architecture.
 * They should be overriden in led.h if this assumption is incorrect.
 */
#define	MAX_INT		((int)(MAX_UINT >> 1))
#define	MAX_LONG	((long)(MAX_ULONG >> 1))
#define	MAX_SHORT	((short)(MAX_USHORT >> 1))
#define	MAX_UINT	((unsigned int)~0)
#define	MAX_ULONG	((unsigned long)~0)
#define	MAX_USHORT	((unsigned short)~0)
#define	MIN_INT		(~MAX_INT)
#define	MIN_LONG	(~MAX_LONG)
#define	MIN_SHORT	(~MAX_SHORT)

#define	newa(t, cnt)	((t *)calloc(cnt, sizeof (t)))
#define	nilp(t)		((t *)0)
#define	nil(t)		((t)0)
#define	noop

typedef	int	(*pfi_t)();
typedef	void	(*pfv_t)();
typedef	boolean_t	(*pfb_t)();
typedef	pfi_t	(*pfpfi_t)();

#define	BE32_EQL(a, b)	(((uint8_t *)a)[0] == ((uint8_t *)b)[0] && \
	((uint8_t *)a)[1] == ((uint8_t *)b)[1] && \
	((uint8_t *)a)[2] == ((uint8_t *)b)[2] && \
	((uint8_t *)a)[3] == ((uint8_t *)b)[3])
#define	BE16_EQL(a, b)	(((uint8_t *)a)[0] == ((uint8_t *)b)[0] && \
	((uint8_t *)a)[1] == ((uint8_t *)b)[1])
#define	BE16_TO_U16(a)	((((uint16_t)((uint8_t *)a)[0] << (uint16_t)8) | \
	((uint16_t)((uint8_t *)a)[1] & 0xFF)) & (uint16_t)0xFFFF)
#define	BE32_TO_U32(a)	((((uint32_t)((uint8_t *)a)[0] & 0xFF) << \
	(uint32_t)24) | \
	(((uint32_t)((uint8_t *)a)[1] & 0xFF) << (uint32_t)16) | \
	(((uint32_t)((uint8_t *)a)[2] & 0xFF) << (uint32_t)8)  | \
	((uint32_t)((uint8_t *)a)[3] & 0xFF))
#define	U16_TO_BE16(u, a) ((((uint8_t *)a)[0] = (uint8_t)((u) >> 8)), \
	(((uint8_t *)a)[1] = (uint8_t)(u)))
#define	U32_TO_BE32(u, a) ((((uint8_t *)a)[0] = (uint8_t)((u) >> 24)), \
	(((uint8_t *)a)[1] = (uint8_t)((u) >> 16)), \
	(((uint8_t *)a)[2] = (uint8_t)((u) >> 8)), \
	(((uint8_t *)a)[3] = (uint8_t)(u)))

/*
 * Local Environment Definition, this may and should override the
 * the default definitions above where the local environment differs.
 */
#include <inet/led.h>
#include <sys/isa_defs.h>

#ifdef	_BIG_ENDIAN

#ifndef	ABE32_TO_U32
#define	ABE32_TO_U32(p)		(*((uint32_t *)p))
#endif

#ifndef	ABE16_TO_U16
#define	ABE16_TO_U16(p)		(*((uint16_t *)p))
#endif

#ifndef	U16_TO_ABE16
#define	U16_TO_ABE16(u, p)	(*((uint16_t *)p) = (u))
#endif

#ifndef	U32_TO_ABE16
#define	U32_TO_ABE16(u, p)	U16_TO_ABE16(u, p)
#endif

#ifndef	UA32_TO_U32
#define	UA32_TO_U32(p, u)	((u) = (((uint32_t)((uint8_t *)p)[0] << 24) | \
				    ((uint32_t)((uint8_t *)p)[1] << 16) | \
				    ((uint32_t)((uint8_t *)p)[2] << 8) | \
				    (uint32_t)((uint8_t *)p)[3]))
#endif

#ifndef	U32_TO_ABE32
#define	U32_TO_ABE32(u, p)	(*((uint32_t *)p) = (u))
#endif

#else

#ifndef	ABE16_TO_U16
#define	ABE16_TO_U16(p)		BE16_TO_U16(p)
#endif

#ifndef	ABE32_TO_U32
#define	ABE32_TO_U32(p)		BE32_TO_U32(p)
#endif

#ifndef	U16_TO_ABE16
#define	U16_TO_ABE16(u, p)	U16_TO_BE16(u, p)
#endif

#ifndef	U32_TO_ABE16
#define	U32_TO_ABE16(u, p)	U16_TO_ABE16(u, p)
#endif

#ifndef	U32_TO_ABE32
#define	U32_TO_ABE32(u, p)	U32_TO_BE32(u, p)
#endif

#ifndef	UA32_TO_U32
#define	UA32_TO_U32(p, u)	((u) = (((uint32_t)((uint8_t *)p)[3] << 24) | \
				    ((uint32_t)((uint8_t *)p)[2] << 16) | \
				    ((uint32_t)((uint8_t *)p)[1] << 8) | \
				    (uint32_t)((uint8_t *)p)[0]))
#endif

#endif

#ifdef	_KERNEL

/* Extra MPS mblk type */
#define	M_MI		64
/* Subfields for M_MI messages */
#define	M_MI_READ_RESET	1
#define	M_MI_READ_SEEK	2
#define	M_MI_READ_END	4

#ifndef EINVAL
#include <errno.h>
#endif

#ifdef MPS
#define	mi_adjmsg	adjmsg
#endif

#ifndef	CANPUTNEXT
#define	CANPUTNEXT(q)	canput((q)->q_next)
#endif

#endif /* _KERNEL */

#ifndef UNIX5_3
#define	EBASE		127

#ifndef EBADMSG
#define	EBADMSG		(EBASE-0)
#endif

#ifndef	ETIME
#define	ETIME		(EBASE-1)
#endif

#ifndef EPROTO
#define	EPROTO		(EBASE-2)
#endif

#endif /* UNIX5_3 */

#ifndef	GOOD_EXIT_STATUS
#define	GOOD_EXIT_STATUS	0
#endif

#ifndef	BAD_EXIT_STATUS
#define	BAD_EXIT_STATUS		1
#endif

#ifndef	is_ok_exit_status
#define	is_ok_exit_status(status)	(status == GOOD_EXIT_STATUS)
#endif


#ifdef	__cplusplus
}
#endif

#endif	/* _INET_COMMON_H */
