/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _SYS_VMMETER_H
#define	_SYS_VMMETER_H

#pragma ident	"@(#)vmmeter.h	2.26	94/05/09 SMI"	/* SVr4.0 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Virtual Address Cache flush instrumentation.
 *
 * Everything from f_first to f_last must be unsigned [int].
 */
struct flushmeter {
#define	f_first f_ctx
	unsigned f_ctx;		/* No. of context flushes */
	unsigned f_segment;	/* No. of segment flushes */
	unsigned f_page;	/* No. of complete page flushes */
	unsigned f_partial;	/* No. of partial page flushes */
	unsigned f_usr;		/* No. of non-supervisor flushes */
	unsigned f_region;	/* No. of region flushes */
#define	f_last	f_region
};

#ifdef _KERNEL
#ifdef VAC
/* cnt is 1 sec accum; rate is 5 sec avg; sum is grand total */
struct flushmeter	flush_cnt;
#endif /* VAC */
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VMMETER_H */
