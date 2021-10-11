/*
 * Copyright (c) 1996,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_STRICK_H
#define	_SYS_STRICK_H

#pragma ident	"@(#)strick.h	1.3	99/10/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * strick.h header for inetcksum STREAMS declarations.
 *
 *    XXX - this is a contract private interface for use by the Sun-ATM
 *    driver ONLY.
 */

#define	b_ick_flag	b_datap->db_struioflag
#define	b_ick_value	b_datap->db_struioun.u16
#define	b_ick_start	b_datap->db_struiobase
#define	b_ick_end	b_datap->db_struiolim
#define	b_ick_stuff	b_datap->db_struioptr

#define	ICK_REMAP	(STRUIO_ZC)
#define	ICK_VALID	(STRUIO_ICK)
#define	ICK_NONE	0

typedef struct {
	int	ick_magic;		/* M_CTL message magic */
	int	ick_split;		/* Byte offset */
	int	ick_split_align;	/* Byte count */
	int	ick_xmit;		/* Flag (0 = enable, -1 = disable) */
} inetcksum_t;

#define	ICK_M_CTL_MAGIC 0xD0DEC0DEu

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRICK_H */
