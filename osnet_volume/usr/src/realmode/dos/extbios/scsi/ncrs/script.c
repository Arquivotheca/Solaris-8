/*
 * Copyright (c) 1997 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: NCR 710/810 EISA SCSI HBA       (script.c)
 *
#pragma ident	"@(#)script.c	1.1	97/07/21 SMI"
 *
 */

/*
 * Routines in this file are based on equivalent routines in the Solaris
 * NCR driver.  Some have been simplified for use in a single-threaded
 * environment.
 */
/* #define DEBUG */

#ifdef DEBUG
    #pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#endif


#include <types.h>
#include "ncr.h"


caddr_t	ncr_scriptp = 0;
paddr_t	ncr_script_physp = 0;
paddr_t	ncr_scripts[NSS_FUNCS] = { 0 };
paddr_t ncr_do_list = 0;
paddr_t ncr_di_list = 0;

/*
 * Include the output of the NASM program. NASM is a DOS program
 * which takes the script.ss file and turns it into a series of
 * C data arrays and initializers.
 */

/*
 * Array script_pad below is designed to be immediately before SCRIPT^M
 * in memory.  Do not change the order of the array script_pad and the 
 * #include of SCRIPT.
 */
static ulong script_pad[2] = { 0 };
#include "scr.out"

ulong *ncr_aligned_script = SCRIPT;
static	size_t	ncr_script_size = sizeof SCRIPT;

/*
 * Offsets of SCRIPT routines. These get turned into physical
 * addresses before they're written into the DSP register. Writing
 * the DSA register starts the program.
 */
static int
ncr_script_offset( int func )
{
	switch (func) {
	case NSS_STARTUP:	/* select a target and start a request */
		return (Ent_start_up);
	case NSS_CONTINUE:	/* continue with current target (no select) */
		return (Ent_continue);
	case NSS_WAIT4RESELECT:	/* wait for reselect */
		return (Ent_resel_m);
	case NSS_CLEAR_ACK:
		return (Ent_clear_ack);
	case NSS_SYNC_OUT:
		return (Ent_sync_out);
	case NSS_ERR_MSG:
		return (Ent_errmsg);
	case NSS_BUS_DEV_RESET:
		return (Ent_dev_reset);
	case NSS_ABORT:
		return (Ent_abort);
	default:
#ifdef SOLARIS
		cmn_err(CE_PANIC, "ncr_script_offset: func=%d\n", func);
#else
		return (Ent_start_up);
#endif
	}
}


/*
 * ncr_script_init()
 */
bool_t
ncr_script_init(ushort seg)
{

	/*
	 * Need to make sure that script array is aligned on 4-byte boundary.
	 * Array script_pad above is designed to be immediately before SCRIPT
	 * in memory.  Copy down into it, if necessary, to force alignment.
	 */

	STATIC int done_init = FALSE;
	unchar	*src;
	unchar	*dest;
	unchar	*end;
	int	func;

	NDBG1(("ncr_script_init\n"));

	if ((int)ncr_aligned_script & 3) {
		ncr_aligned_script = (ulong *)(((int) script_pad + 3) & ~3);
		end = (unchar *)SCRIPT + sizeof (SCRIPT);
		dest = (unchar *) ncr_aligned_script;
		for (src = (unchar *)SCRIPT; src < end;) {
			*dest++ = *src++;
		}
	}

	/* save the physical addresses */
	ncr_script_physp = longaddr((ushort)ncr_aligned_script, seg);
	for (func = 0; func < NSS_FUNCS; func++)
		ncr_scripts[func] = ncr_script_physp +
					ncr_script_offset(func);

	ncr_do_list = ncr_script_physp + Ent_do_list;
	ncr_di_list = ncr_script_physp + Ent_di_list;

	done_init = TRUE;
	
	NDBG1(("ncr_script_init: okay\n"));
	return (TRUE);
}

#ifdef SOLARIS
	if (done_init)
		return (TRUE);

	if (btopr(ncr_script_size) != 1) {
		cmn_err(CE_WARN, "ncr_script_init: Too big %d\n"
				, ncr_script_size);
		return (FALSE);
	}
	/* alloc twice as much as needed to be certain
	 * I can fit it into a single page
	 */
	memp = kmem_zalloc(ncr_script_size * 2, KM_NOSLEEP);
	if (memp == NULL) {

		cmn_err(CE_WARN
			, "ncr_script_init: Unable to allocate memory\n");
		return (FALSE);
	}

	ncr_scriptp = memp;

	/* shift the pointer if necessary */
	memp = PageAlignPtr(memp, ncr_script_size);

	/* copy the script into the buffer we just allocated */
	bcopy((caddr_t)SCRIPT, (caddr_t)memp, ncr_script_size);

	/* save the physical addresses */
	ncr_script_physp = NCR_KVTOP(memp);
	for (func = 0; func < NSS_FUNCS; func++)
		ncr_scripts[func] = ncr_script_physp
					+ ncr_script_offset(func);

	ncr_do_list = ncr_script_physp + Ent_do_list;
	ncr_di_list = ncr_script_physp + Ent_di_list;

	done_init = TRUE;
	NDBG1(("ncr_script_init: okay\n"));
	return (TRUE);
}


/*
 * Free the script buffer
 */
void
ncr_script_fini( void )
{
	if (ncr_scriptp) {
		NDBG1(("ncr_script_fini: free-ing buffer\n"));
		kmem_free(ncr_scriptp, 2 * ncr_script_size);
		ncr_scriptp = NULL;
	}
	return;
}
#endif
