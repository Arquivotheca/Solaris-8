/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _MNTTAB_H
#define	_MNTTAB_H

#pragma ident	"@(#)mnttab.h	1.10	97/02/12 SMI"	/* SVr4.0 1.5.1.2 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	NMOUNT	100

/*
 * Format of the /etc/mnttab file which is set by the mount(1m)
 * command
 */
struct mnttab {
	char	mt_dev[32],
		mt_filsys[32];
		short	mt_ro_flg;
	time_t	mt_time;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _MNTTAB_H */
