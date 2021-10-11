/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_openprom.c	1.9	98/08/12 SMI"

/*
 * This routine does the basic openprom interfaces: opening, closing,
 * walking the tree, and retrieving properties from /dev/openprom.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/autoconf.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddipropdefs.h>
#include <sys/openpromio.h>

#include "dr_openprom.h"
#include "dr_subr.h"

static char  *promdev = "/dev/openprom";

/*
 *
 * 128 is the size of the largest (currently) property name
 * 4096 - MAXPROPSIZE - sizeof (int) is the size of the largest
 * (currently) property value that is allowed.
 * the sizeof (u_int) is from struct openpromio
 */

/* Forward declarations */
static int is_openprom(void);
static void promclose(void);
static void walk(int id, int level, void (*fn)(int id, int level, void *argp),
		    void *argp);
static int promopen(int oflag);
static int next(int id);
static int child(int id);

static int prom_fd;

static int
is_openprom()
{
	Oppbuf	oppbuf;
	register struct openpromio *opp = &(oppbuf.opp);
	register unsigned int i;

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETCONS, opp) < 0) {
		dr_logerr(DRV_FAIL, errno, "OPROMGETCONS");
		return (-1);
	}
	i = (unsigned int)((unsigned char)opp->oprom_array[0]);
	return ((i & OPROMCONS_OPENPROM) == OPROMCONS_OPENPROM);
}

static char *badarchmsg =
	"System architecture does not support this option of this command.";

/*
 * do_prominfo
 *
 * This routine opens/closes the openprom device and starts the
 * walk of the openprom tree.
 *
 * Input:
 *    fn - function to call for each obp node. Passed to this routine are:
 *		id - obp address of the node
 *		level - tree level of the node
 *		argp - user supplied argument
 *    argp - argument to pass to (*fn)
 *
 * Function Return: 0, success, <0, failure, >0 no OBP nodes
*/
int
do_prominfo(void (*fn)(int id, int level, void *argp), void *argp)
{
	int err;

	if (promopen(O_RDONLY))  {
		return (-1);
	}

	err = is_openprom();
	if (err == 0)  {
		promclose();
		dr_logerr(DRV_FAIL, 0, badarchmsg);
		return (-1);
	}

	if (err == -1) {
		promclose();
		return (-1);
	}

	if (next(0) == 0)
		return (1);

	walk(next(0), 0, fn, argp);

	promclose();
	return (0);
}

static void
walk(id, level, fn, argp)
    int		id;
    int		level;
    void 		(*fn)(int id, int level, void *argp);
    void		*argp;		/* ptr to argument to (*fn) */

{
	register int curnode;

	if (dr_err.err)
	    return;

	fn(id, level, argp);

	if (curnode = child(id))
		walk(curnode, level+1, fn, argp);
	if (curnode = next(id))
		walk(curnode, level, fn, argp);
}

static int
promopen(oflag)
    register int oflag;
{
	int	max_wait;

	/*
	 * Be paranoid and limit how long we'll wait for the
	 * device to free up.  Only a limited number of processes
	 * may have this device open at one time.  Wait no more than
	 * 30 seconds.  This is only an info routine and the
	 * user can always try again later.
	 */
	for (max_wait = 6; max_wait > 0; max_wait--)  {

		if ((prom_fd = open(promdev, oflag)) < 0)  {
			if (errno == EAGAIN)   {
				sleep(5);
				continue;
			}
			dr_logerr(DRV_FAIL, errno, "cannot open /dev/openprom");
			return (-1);
		} else
			return (0);
	}

	dr_logerr(DRV_FAIL, 0, "/dev/openprom busy.  Cannot open.");
	return (-1);
}

static void
promclose()
{
	if (close(prom_fd) < 0) {
		dr_logerr(DRV_FAIL, errno, "close error on /dev/openprom");
		return;
	}
}

/*
 * getpropval
 *
 * Grab the given property from the current obp node
 *
 * Input:
 *	propname - name of the obp property to return
 *	result - pointer to fill in with address of property
 *		value returned.  Data type of this result is unknown.
 *		User is responsible for examinging it in the data
 *		appropriate manner.
 * Output:
 *	*result is set to fetched property value.
 *
 * Function return value: 0, success, != failure
 */
int
getpropval(char *propname, void **result)
{
	static Oppbuf op;

	op.opp.oprom_size = MAXVALSIZE;
	strcpy(op.opp.oprom_array, propname);

	if (ioctl(prom_fd, OPROMGETPROP, &op.opp) < 0) {
		dr_logerr(DRV_FAIL, errno, "OPROMGETPROP");
		return (-1);
	}
	if (op.opp.oprom_size == 0)
		return (-1);

	*result = (void *)op.opp.oprom_array;
	return (0);
}

static int
next(id)
    int 	id;
{
	Oppbuf	oppbuf;
	register struct openpromio *opp;
	int *ip;

	if (dr_err.err)
		/* 0 indicates no more nodes */
		return (0);

	opp = &(oppbuf.opp);
	/* LINTED */
	ip = (int *)(opp->oprom_array);

	memset(oppbuf.buf, 0, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	*ip = id;

	if (ioctl(prom_fd, OPROMNEXT, opp) < 0) {
		dr_logerr(DRV_FAIL, errno, "OPROMNEXT");
		return (0);
	}

	/* LINTED */
	return (*(int *)opp->oprom_array);
}

static int
child(id)
    int id;
{
	Oppbuf	oppbuf;
	register struct openpromio *opp;
	int *ip;

	if (dr_err.err)
		/* 0 indicates no more nodes */
		return (0);

	opp  = &(oppbuf.opp);
	/* LINTED */
	ip = (int *)(opp->oprom_array);

	memset(oppbuf.buf, 0, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	*ip = id;

	if (ioctl(prom_fd, OPROMCHILD, opp) < 0) {
		dr_logerr(DRV_FAIL, errno, "OPROMCHILD");
		return (0);
	}

	/* LINTED */
	return (*(int *)opp->oprom_array);
}
