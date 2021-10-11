/*
 * Copyright (c) 1991-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_IMMU_H
#define	_SYS_IMMU_H

#pragma ident	"@(#)immu.h	1.9	97/05/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * XXX - following stuff from 3b2 immu.h.  this really belongs elsewhere.
 */

/*
 * The following variables describe the memory managed by
 * the kernel.  This includes all memory above the kernel
 * itself.
 */

extern pgcnt_t	maxmem;		/* Maximum available free memory. */
extern pgcnt_t	freemem;	/* Current free memory. */
extern pgcnt_t	availrmem;	/* Available resident (not	*/
				/* swapable) memory in pages.	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IMMU_H */
