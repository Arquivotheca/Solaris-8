/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fhc_asm.s	1.4	97/05/24 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/asi.h>
#include <sys/privregs.h>
#include <sys/spitregs.h>

#if defined(lint)

#else	/* lint */
#include "assym.h"
#endif	/* lint */

/*
 * fhc_shutdown_asm(u_longlong_t base, int size)
 *
 * Flush cpu E$ then shutdown.
 * This function is special in that it really sets the D-tags to
 * a known state.  And this is the behavior we're looking for.
 *
 * The flush address is known to be a cpu-unique non-existent
 * cacheable address.  We write to non-existent memory, using
 * the side effect of d-tag invalidation.
 *
 * Also, note that this function is never run from main memory.
 * Rather it is copied to non-cacheable SRAM (hence the ..._end
 * label at the bottom of the function).  This implies that the
 * function must be position independent code that doesn't reference 
 * cacheable real memory.
 */
#if defined(lint)

/*ARGSUSED*/
void
fhc_shutdown_asm(u_longlong_t base, int size)
{}

#else	/* lint */

	ENTRY(fhc_shutdown_asm)
#if !defined(__sparcv9)
	! form 64 bit base address in %o0
	sllx	%o0, 32, %o0
	srl	%o1, 0, %o1
	or	%o0, %o1, %o0

	! and move size into %o1
	mov	%o2, %o1
#endif

	! turn off errors (we'll be writing to non-existent memory)
	stxa	%g0, [%g0]ASI_ESTATE_ERR
	membar	#Sync			! SYNC

	rdpr	%pstate, %o4
	andn	%o4, PSTATE_IE | PSTATE_AM, %o3
	wrpr	%o3, %g0, %pstate
1:
	brlez,pn %o1, 2f		! if (len <= 0) exit loop
	  dec	64, %o1			! size -= 64
        sta     %g0, [%o0]ASI_MEM	! store (unpopulated) word
	ba	1b
	  inc	64, %o0			! addr += 64
2:
	membar  #Sync			! SYNC
	shutdown			! SHUTDOWN
	/*NOTREACHED*/

	! if, for some reason, this cpu doesn't shutdown, just sit here
3:
	ba	3b
	  nop				! eventually the master will notice
	SET_SIZE(fhc_shutdown_asm)

	.global	fhc_shutdown_asm_end
fhc_shutdown_asm_end:

#endif	/* lint */
