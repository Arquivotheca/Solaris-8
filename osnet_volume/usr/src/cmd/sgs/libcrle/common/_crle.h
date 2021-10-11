/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__CRLE_H
#define	__CRLE_H

#pragma ident	"@(#)_crle.h	1.1	99/08/13 SMI"

#include "machdep.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	CRLE_AUD_DEPENDS	0x1	/* Audit - collect dependencies */
#define	CRLE_AUD_DLDUMP		0x2	/* Audit - dldump(3x) objects */

extern int		pfd;
extern int		dlflag;

extern int		dumpconfig(void);
extern int		filladdr(void);
extern const char *	msg(int);

#ifdef	__cplusplus
}
#endif

#endif	/* __CRLE_H */
