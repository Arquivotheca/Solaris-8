/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)rd_mach.c	1.1	99/05/04 SMI"


#include	<libelf.h>
#include	<sys/reg.h>
#include	<rtld_db.h>
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * On x86, basically, a PLT entry looks like this:
 *	8048738:  ff 25 c8 45 05 08   jmp    *0x80545c8	 < OFFSET_INTO_GOT>
 *	804873e:  68 20 00 00 00      pushl  $0x20
 *	8048743:  e9 70 ff ff ff      jmp    0xffffff70 <80486b8> < &.plt >
 *
 *  The first time around OFFSET_INTO_GOT contains address of pushl; this forces
 *	first time resolution to go thru PLT's first entry (which is a call)
 *  The nth time around, the OFFSET_INTO_GOT actually contains the resolved
 *	address of the symbol(name), so the jmp is direct  [VT]
 *  The only complication is when going from a .so to an a.out or to another
 *	.so, GOT's address is in %ebx
 */
/* ARGSUSED 3 */
rd_err_e
rd_plt_resolution(rd_agent_t *rap, psaddr_t pc, lwpid_t lwpid,
	psaddr_t pltbase, rd_plt_info_t *rpi)
{
	/*
	 * IA64:  it ain't here yet.
	 */
	return (RD_ERR);
}
