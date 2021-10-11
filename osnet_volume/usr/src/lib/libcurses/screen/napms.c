/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)napms.c	1.9	97/08/27 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/

#include	"curses_inc.h"
#include	<stdio.h>
#include	<sys/types.h>
#include	<poll.h>

/*
 * napms.  Sleep for ms milliseconds.  We don't expect a particularly good
 * resolution - 60ths of a second is normal, 10ths might even be good enough,
 * but the rest of the program thinks in ms because the unit of resolution
 * varies from system to system.  (In some countries, it's 50ths, for example.)
 * Vaxen running 4.2BSD and 3B's use 100ths.
 *
 * Here are some reasonable ways to get a good nap.
 *
 * (1) Use the poll() or select() system calls in SVr3 or Berkeley 4.2BSD.
 *
 * (2) Use the 1/10th second resolution wait in the System V tty driver.
 *     It turns out this is hard to do - you need a tty line that is
 *     always unused that you have read permission on to sleep on.
 *
 * (3) Install the ft (fast timer) device in your kernel.
 *     This is a psuedo-device to which an ioctl will wait n ticks
 *     and then send you an alarm.
 *
 * (4) Install the nap system call in your kernel.
 *     This system call does a timeout for the requested number of ticks.
 *
 * (5) Write a routine that busy waits checking the time with ftime.
 *     Ftime is not present on SYSV systems, and since this busy waits,
 *     it will drag down response on your system.  But it works.
 */

int
napms(int ms)
{
	struct pollfd pollfd;

	if (poll(&pollfd, 0L, ms) == -1)
		perror("poll");
	return (OK);
}
