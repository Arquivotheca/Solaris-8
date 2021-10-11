/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STRSTAT_H
#define	_SYS_STRSTAT_H

#pragma ident	"@(#)strstat.h	1.11	94/04/13 SMI"	/* SVr4.0 11.5 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Streams Statistics header file.  This file defines the counters
 * which are maintained for statistics gathering under Streams.
 */

/*
 * per-module statistics structure
 */
struct module_stat {
	long ms_pcnt;		/* count of calls to put proc */
	long ms_scnt;		/* count of calls to service proc */
	long ms_ocnt;		/* count of calls to open proc */
	long ms_ccnt;		/* count of calls to close proc */
	long ms_acnt;		/* count of calls to admin proc */
	char *ms_xptr;		/* pointer to private statistics */
	short ms_xsize;		/* length of private statistics buffer */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRSTAT_H */
