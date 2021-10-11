/*
 * Copyright (c) 1988,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MEMERR_H
#define	_SYS_MEMERR_H

#pragma ident	"@(#)memerr.h	1.16	98/01/06 SMI"
/* SunOS-4.1 1.9	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * All sun4d implementations have ECC
 */

extern uint_t memerr_init(void);
extern void memerr_ECI(uint_t enable);
extern uint_t memerr(uint_t type);

/* parameters to memerr() */
#define	MEMERR_CE	(1 << 0)
#define	MEMERR_UE	(1 << 1)
#define	MEMERR_FATAL	(1 << 2)

extern int memscrub_init(void);
extern int memscrub_add_span(uint_t pfn, uint_t pages);
extern int memscrub_delete_span(uint_t pfn, uint_t bytes);

#define	PROP_DEVICE_ID "device-id"	/* move elsewhere */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEMERR_H */
