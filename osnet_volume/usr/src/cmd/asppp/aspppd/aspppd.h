#ident	"@(#)aspppd.h	1.21	96/10/30 SMI"

/* Copyright (c) 1993 by Sun Microsystems, Inc. */

#ifndef _ASPPPD_H
#define	_ASPPPD_H

#include <setjmp.h>

extern int	debug;
extern jmp_buf	restart;
extern int	conn_cntr;

#endif	/* _ASPPPD_H */
