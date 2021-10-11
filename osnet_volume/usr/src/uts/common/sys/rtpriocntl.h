/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_RTPRIOCNTL_H
#define	_SYS_RTPRIOCNTL_H

#pragma ident	"@(#)rtpriocntl.h	1.12	98/01/06 SMI"	/* SVr4.0 1.4 */

#include <sys/types.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Real-time class specific structures for the priocntl system call.
 */

typedef struct rtparms {
	pri_t	rt_pri;		/* real-time priority */
	uint_t	rt_tqsecs;	/* seconds in time quantum */
	int	rt_tqnsecs;	/* additional nanosecs in time quant */
} rtparms_t;


typedef struct rtinfo {
	pri_t	rt_maxpri;	/* maximum configured real-time priority */
} rtinfo_t;


#define	RT_NOCHANGE	-1
#define	RT_TQINF	-2
#define	RT_TQDEF	-3


/*
 * The following is used by the dispadmin(1M) command for
 * scheduler administration and is not for general use.
 */

#ifdef _SYSCALL32
/* Data structure for ILP32 clients */
typedef struct rtadmin32 {
	caddr32_t	rt_dpents;
	int16_t		rt_ndpents;
	int16_t		rt_cmd;
} rtadmin32_t;
#endif /* _SYSCALL32 */

typedef struct rtadmin {
	struct rtdpent	*rt_dpents;
	short		rt_ndpents;
	short		rt_cmd;
} rtadmin_t;

#define	RT_GETDPSIZE	1
#define	RT_GETDPTBL	2
#define	RT_SETDPTBL	3

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RTPRIOCNTL_H */
