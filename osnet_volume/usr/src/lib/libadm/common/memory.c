/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)memory.c	1.11	97/07/28 SMI"	/* SVr4.0 1.1 */

/*LINTLIBRARY*/

/*
 *  memory.c
 *	sysmem()	Get the amount of memory on the system
 *	asysmem()	Get the amount of available memory on the system
 */

/*
 *  G L O B A L   D E F I N I T I O N S
 *	- Header files referenced
 *	- Global functions referenced
 */


/*
 * Header files included:
 *	<sys/types.h>	    Data types known to the kernel
 *	<nlist.h>	    Definitions for Sun symbol table entries
 *	<sys/sysinfo.h>	    Internal Kernel definitions
 *	<sys/param.h>	    Internal Kernel Parameters
 *	<sys/sysmacros.h>   Internal Kernel Macros
 *	<fcntl.h>	    File control definitions
 *	<unistd.h>	    UNIX Standard definitions
 */

#include	<sys/types.h>
#include	<nlist.h>
#include	<sys/sysinfo.h>
#include	<sys/param.h>
#include	<sys/sysmacros.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<string.h>
#include	<values.h>
#include	"libadm.h"

/*
 *  L O C A L   D E F I N I T I O N S
 *	- Local constants
 *	- Local function definitions
 *	- Static data
 */

/*
 * Local Constants
 *	TRUE		Boolean value, true
 *	FALSE		Boolean value, false
 *	NULL		No such address (nil)
 */

#ifndef	TRUE
#define	TRUE	(1)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	NULL
#define	NULL	(0)
#endif

#define	PHYSMEM		0
#define	FREEMEM		1
#define	MEG4		0x400000

static struct nlist nl[] = {
#ifndef SUNOS41
	{"physmem", 0, 0, 0, 2, 0},
	{"freemem", 0, 0, 0, 2, 0},
#else
	{"physmem", 0, 0, 0, 0},
	{"freemem", 0, 0, 0, 0},
#endif
	{NULL}
};

static long	sunmem(int);


/*
 * Local Macros
 *	strend(p)	Find the end of the string pointed to by "p"
 */

#define	strend(p)	strrchr(p, '\0')

/*
 * sysmem()
 *
 *	Return the amount of memory configured on the system.
 *
 *	On a Sun, this is the value of "physmem" read from the
 *	kernel symbol table, converted to number of bytes.
 *
 *  Arguments: None
 *
 *  Returns:  long
 *	On a Sun, whatever is read from "physmem" symbol table entry,
 *	converted to number of bytes.
 */

long
sysmem(void)
{
	long	physmem;	/* total amount of physical memory, in bytes */

	if ((physmem = sunmem(PHYSMEM)) == -1)
		return (-1);

	/*
	 * The value read from kernel memory does not include pages
	 * hidden by the monitor and debugger, and so looks like there's
	 * a memory loss.  That loss is well within 4Mg, so we should
	 * be able to round up to the next multiple of 4Mg and assume
	 * it to be amount of physical memory in the system.  If that
	 * would overflow a long, return -1.
	 */
	if (physmem > (MAXLONG & ~(MEG4 - 1)))
		return (-1);

	physmem = (physmem + (MEG4 - 1)) & ~(MEG4 - 1);

	return (physmem);
}

/*
 * int asysmem()
 *
 *	This function returns the amount of available memory on the system.
 *	This is defined by
 *
 *  Arguments:  None
 *
 *  Returns:  long
 *	The amount of available memory or -1 with "errno" set if the value
 *	is not available.
 *	On a Sun, this is the value of "freemem" read from the
 *	kernel symbol table, converted to number of bytes.
 */

long
asysmem(void)
{
	return (sunmem(FREEMEM));
}

/*
 * The SUN way of accessing amounts of kernel memory
 */
static long
sunmem(int index)	/* index into global nl array for desired symbol */
{
	long	rtnval;	   /* amount of memory, in bytes */
	long	memsize;   /* amount of memory, in pages */
	long	pagesize;  /* size of page, in bytes */
	int	memfd;

	/* Check out namelist and memory files. */
	if (nlist("/dev/ksyms", nl) != 0)
		return (-1);

	if (nl[index].n_value == 0)
		return (-1);

	/* Open kernel memory */
	rtnval = -1;
	if ((memfd = open("/dev/kmem", O_RDONLY, 0)) > 0) {

	    if ((lseek(memfd, nl[index].n_value, SEEK_SET) != -1) &&
		(read(memfd, &memsize, sizeof (memsize)) ==
		(ssize_t)sizeof (memsize))) {

		pagesize = sysconf(_SC_PAGESIZE);
		/* check for sysconf error or overflow */
		if (pagesize != -1 && memsize <= (MAXLONG / pagesize))
		    rtnval = memsize * pagesize;
	    }

	    /* Close kernel memory */
	    (void) close(memfd);

	}  /* If successfully opened /dev/kmem */

	/* Fini */
	return (rtnval);
}
