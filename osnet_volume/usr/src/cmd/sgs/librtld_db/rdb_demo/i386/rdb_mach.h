
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _RDB_MACH_H
#define	_RDB_MACH_H

#pragma	ident	"@(#)rdb_mach.h	1.2	96/09/10 SMI"

#include <sys/regset.h>
#include <sys/psw.h>

#define	ERRBIT	PS_C
#define	R_PS	EFL


/*
 * Breakpoint instruction
 */
typedef	unsigned char	bptinstr_t;
#define	BPINSTR		0xcc		/* int	3 */


/*
 * PLT section type
 */
#define	PLTSECTT	SHT_PROGBITS

#endif
