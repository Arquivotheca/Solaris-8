/*
 * Copyright (c) 1993,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PRNT_H
#define	_PRNT_H

#pragma ident	"@(#)prnt.h	1.3	98/03/16 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	P_INFO	0
#define	P_ERR	1

extern int verbose_out;
extern int verbose;

extern void prnt(int info_or_err, char *format, ...);

#ifdef __cplusplus
}
#endif

#endif	/* _PRNT_H */
