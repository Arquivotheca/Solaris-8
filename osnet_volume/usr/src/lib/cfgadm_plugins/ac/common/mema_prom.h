/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MEMA_SF_PROM_H
#define	_MEMA_SF_PROM_H

#pragma ident	"@(#)mema_prom.h	1.3	98/06/07 SMI"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char mema_disabled_t;

extern int prom_read_disabled_list(mema_disabled_t *, int);
extern int prom_write_disabled_list(mema_disabled_t *, int);
extern int prom_viable_disabled_list(mema_disabled_t *);

#define	PROM_MEMORY_DISABLED	0x02
#define	PROM_MEMORY_PRESENT	0x04	/* for viable check */

#ifdef __cplusplus
}
#endif

#endif /* _MEMA_SF_PROM_H */
