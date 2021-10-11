
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

#include	<sys/psw.h>
#include	<sys/procfs.h>
#define	ERRBIT	PSR_C

struct ps_prochandle;

/*
 * BreakPoint instruction
 */
typedef	unsigned	bptinstr_t;

#define	BPINSTR		0x91d02001	/* ta   ST_BREAKPOINT */

/*
 * PLT section type
 */
#define	PLTSECTT	SHT_PROGBITS

extern void		display_in_regs(struct ps_prochandle *,
				prstatus_t *);
extern void		display_local_regs(struct ps_prochandle *,
				prstatus_t *);
extern void		display_out_regs(struct ps_prochandle *,
				prstatus_t *);
extern void		display_special_regs(struct ps_prochandle *,
				prstatus_t *);
extern void		display_global_regs(struct ps_prochandle *,
				prstatus_t *);

#endif
