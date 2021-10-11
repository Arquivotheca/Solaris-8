/*
 * Copyright (c) 1990,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_STRSUN_H
#define	_SYS_STRSUN_H

#pragma ident	"@(#)strsun.h	1.16	99/10/14 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * strsun.h header for common Sun STREAMS declarations.
 */

#define	DB_BASE(mp)		((mp)->b_datap->db_base)
#define	DB_LIM(mp)		((mp)->b_datap->db_lim)
#define	DB_REF(mp)		((mp)->b_datap->db_ref)
#define	DB_TYPE(mp)		((mp)->b_datap->db_type)

#define	MBLKL(mp)		((mp)->b_wptr - (mp)->b_rptr)
#define	MBLKSIZE(mp)	((mp)->b_datap->db_lim - (mp)->b_datap->db_base)
#define	MBLKHEAD(mp)	((mp)->b_rptr - (mp)->b_datap->db_base)
#define	MBLKTAIL(mp)	((mp)->b_datap->db_lim - (mp)->b_wptr)

#define	MBLKIN(mp, off, len)	((off <= MBLKL(mp)) && \
	(((mp)->b_rptr + off + len) <= (mp)->b_wptr))

#define	OFFSET(base, p)	((caddr_t)(p) - (caddr_t)(base))

#ifdef	_KERNEL
extern void	merror(queue_t *, mblk_t *, int);
extern void	miocack(queue_t *, mblk_t *, int, int);
extern void	miocnak(queue_t *, mblk_t *, int, int);
extern mblk_t	*mexchange(queue_t *, mblk_t *, size_t, int, t_scalar_t);
extern mblk_t	*mexpandb(mblk_t  *mp, int, int);
extern size_t	msgsize(mblk_t *);
extern void	mcopymsg(mblk_t *, uchar_t *);
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRSUN_H */
