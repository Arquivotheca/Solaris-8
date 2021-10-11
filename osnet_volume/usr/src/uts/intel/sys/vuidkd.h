/*
 * Copyright (c) 1993,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_VUIDKD_H
#define	_SYS_VUIDKD_H

#pragma ident	"@(#)vuidkd.h	1.4	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	TSIOC	('t' << 16 | 's' << 8)
#define	TSON	(TSIOC|1)
#define	TSOFF	(TSIOC|2)

#if _KERNEL

struct vuidkd_state {
	char		vs_state;
};
typedef struct vuidkd_state vuidkd_state_t;

#define	VUIDKD_STATE(sp)		((sp)->vs_state)

/*
 * Timestamping state (vs_state)
 */
#define	VS_OFF		0x0	/* vuids disabled */
#define	VS_ON		0x1	/* vuids enabled */

#define	VUIDKDPSZ	INFPSZ	/* max packet size allowed on the */
				/* vuidkd stream */

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_VUIDKD_H */
