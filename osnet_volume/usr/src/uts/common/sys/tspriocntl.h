/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TSPRIOCNTL_H
#define	_SYS_TSPRIOCNTL_H

#pragma ident	"@(#)tspriocntl.h	1.10	97/01/16 SMI"	/* SVr4.0 1.5 */

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Time-sharing class specific structures for the priocntl system call.
 */

typedef struct tsparms {
	pri_t	ts_uprilim;	/* user priority limit */
	pri_t	ts_upri;	/* user priority */
} tsparms_t;


typedef struct tsinfo {
	pri_t	ts_maxupri;	/* configured limits of user priority range */
} tsinfo_t;

#define	TS_NOCHANGE	-32768

/*
 * The following is used by the dispadmin(1M) command for
 * scheduler administration and is not for general use.
 */

#ifdef _SYSCALL32
/* Data structure for ILP32 clients */
typedef struct tsadmin32 {
	caddr32_t	ts_dpents;
	int16_t		ts_ndpents;
	int16_t		ts_cmd;
} tsadmin32_t;
#endif /* _SYSCALL32 */

typedef struct tsadmin {
	struct tsdpent	*ts_dpents;
	short		ts_ndpents;
	short		ts_cmd;
} tsadmin_t;

#define	TS_GETDPSIZE	1
#define	TS_GETDPTBL	2
#define	TS_SETDPTBL	3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TSPRIOCNTL_H */
