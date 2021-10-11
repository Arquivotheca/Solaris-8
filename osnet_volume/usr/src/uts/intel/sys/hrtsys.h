/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_HRTSYS_H
#define	_SYS_HRTSYS_H

#pragma ident	"@(#)hrtsys.h	1.5	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef notdef
/*
 *	Kernel structure for keeping track of sleep and alarm requests.
 */


typedef struct timer {
	struct proc	*hrt_proc;	/* Ptr to proc table entry.	*/
	int		hrt_clk;	/* Which clock to use		*/
	unsigned long	hrt_time;	/* Remaining time to alarm in	*/
					/* HZ.				*/
	unsigned long	hrt_int;	/* Base interval for repeating	*/
					/* alarms.			*/
	unsigned long	hrt_rem;	/* Value of remainder for	*/
					/* repeat alarm.		*/
	unsigned long	hrt_crem;	/* Cummulative remainder for	*/
					/* repeating alarm.		*/
	unsigned long	hrt_res;	/* User specified resolution.	*/
	ushort_t	hrt_cmd;	/* The user specified command.	*/
	struct vnode	*hrt_vp;	/* Ptr to vnode for event queue	*/
					/* to post to.			*/
	ecb_t	hrt_ecb;		/* User specified event control */
					/* block.			*/
	int	(*hrt_fn)();		/* function to call		*/
	struct timer	*hrt_next;	/* Next on list.		*/
	struct timer	*hrt_prev;	/* Previous on list.		*/
} timer_t;

extern timer_t	hrtimes[];
extern int	hrtimes_size;
extern timer_t	hrt_active;
extern timer_t	hrt_avail;

extern timer_t	*hrt_alloc();
extern void	hrt_free();

extern void	hrtinit();
extern void	itinit();

extern long	hrt_convert();

extern	void	hrt_timeout();
extern	void	hrt_enqueue();
extern	void	hrt_dequeue();

extern	void	itimer_timeout();
extern	void	itimer_enqueue();
extern	void	itimer_dequeue();

extern timer_t	itimes[];
extern int	itimes_size;
extern timer_t	it_avail;

#endif /* notdef */

#ifdef _KERNEL

extern int	hrt_cancel_proc();
extern void	hrt_gettofd();

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_HRTSYS_H */
