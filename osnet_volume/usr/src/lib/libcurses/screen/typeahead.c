/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)typeahead.c	1.9	97/08/18 SMI"	/* SVr4.0 1.5	*/

/*LINTLIBRARY*/

#include	<unistd.h>
#include	<sys/types.h>
#include	"curses_inc.h"

/*
 * Set the file descriptor for typeahead checks to fd.  fd can be -1
 * to disable the checking.
 */

int
typeahead(int fd)
{
#ifdef	SYSV
	/*
	* Doing fcntls before and after each typeahead check
	* read is a serious problem on the 3b2. Profiling
	* results indicated that a simple program(edit.c from
	* "The New Curses and Terminfo Package") was spending
	* 9.2% of the time in fcntl().
	*/

#include	<fcntl.h>
	int	savefd = cur_term->_check_fd;

	/* Close the previous duped file descriptor. */
	if (savefd >= 0)
		(void) close(savefd);

	/*
	* Duplicate the file descriptor so we have one to play with.
	* We cannot use dup(2), unfortunately, so we do typeahead checking
	* on terminals only. Besides, if typeahead is done when input is
	* coming from a file, nothing would EVER get drawn on the screen!
	*/

	/* MODIFIED: DRS 3/26/89 */

	/*
	 * a couple of notes by DRS:  first, a new file descriptor is
	 * required, since O_NDELAY must be set.  calling fcntl() or dup()
	 * would provide a new file descriptor, but NOT a new file pointer
	 * for the open file (by file pointer, i mean a unix kernal file
	 * ptr).  if a new underlying unix file ptr. is not allocated,
	 * setting O_NDELAY will cause all normal terminal i/o to return
	 * prematurely without blocking.
	 *
	 * second, the call to ttyname is NOT necessary, /dev/tty can be
	 * used instead.  calling ttyname is quite expensive -- especially
	 * for large /dev directories.
	 *
	 * note also that the code for the '#else' clause will not work
	 * since the new file descriptor MUST have O_NDELAY set for the
	 * rest of libcurses code to function properly.
	 *
	 * 4/24/89:  modified to set the close on exec() flag of the newly
	 * 		opened file descriptor
	 */

	/*
		cur_term->_check_fd = (tty = ttyname(fd)) ?
		    open(tty, O_RDONLY | O_NDELAY) : -1;
	*/
	if (isatty(fd)) {
		if ((cur_term->_check_fd = open("/dev/tty", O_RDONLY |
		    O_NDELAY)) >= 0)
			(void) fcntl(cur_term->_check_fd, F_SETFD, 1);
	} else
		cur_term->_check_fd = -1;

#else	/* SYSV */
	int savefd = cur_term->_check_fd;
	/* Only do typeahead checking if the input is a tty. */
	if (isatty(fd))
		cur_term->_check_fd = fd;
	else
		cur_term->_check_fd = -1;
#endif	/* SYSV */
	return (savefd);
}
