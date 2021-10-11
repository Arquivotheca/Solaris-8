/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_query.c	1.29	99/11/14 SMI"

/*
 * This file implements the Dynamic Reconfiguration Query Routines
 */

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/dr.h>
#include <sys/sfdr.h>
#include <stdlib.h>
#include <libdevinfo.h>
#include <search.h>

#include "dr_subr.h"

/* A struct used to pass around a list of unsafe major numbers */
typedef	struct dr_unsafe_devs {
	int	maj_len;
	int	nam_len;
	int	major[MAX_DEV_UNITS_PER_BOARD*MAX_BOARDS];
	name_t	*name;
} dr_unsafe_devs_t;

/* Forward references */
static int major_compare(const void *, const void *);
static void get_unsafe_majors(dr_unsafe_devs_t *);
static int get_unsafe_name(di_node_t, void *);
static int cpp_cmp(const void *, const void *);

/*
 * ------------------------------------------------------------------
 * dr_get_board_state
 * ------------------------------------------------------------------
 *
 * Called at the end of every RPC routine to translate the current DR
 * driver's state for a board into a state that a DR application can
 * understand
 *
 * Input:	board (the board whose state will be queried/translated)
 *
 * Description:	If the board's in range: query its status to figure out what
 *		the board's doing, and then return a translated state.
 *
 *		Translations are from the `sfdr_state_t' type to the
 *		`dr_board_state_t' type.
 *
 *		A note on error checking: the get_dr_status() only verifies
 *		its board argument as being in range, and proceeds with the
 *		ioctl() anyways.  A bad status structure from dr_get_status()
 *		is only returned when the board is out of range or if its
 *		DR_CMD_STATUS ioctl failed somehow.  By checking if the board's
 *		in range before calling dr_get_status(), we limit the weirdness
 *		of discerning good dr_get_status() returns from bad ones.
 *
 *		The expected behavior of the previous DR Daemon was to detect
 *		intermediate states and attempt error recovery in such cases.
 *		(One reason for the behavior is that the SSP applications know
 *		nothing of the intermediate states and would be confused by
 *		them.)  To keep this tradition, if a "detach" wasn't completed,
 *		a recovery is attempte.d
 *
 * Output:	The appropriate `dr_board_state_t' type for the board
 *		(DR_ERROR_STATE if the board is out of range)
 */
dr_board_state_t
dr_get_board_state(int board)
{
	sfdr_stat_t	*status;
	sfdr_state_t	state;
	sfdr_state_t	pstate;

	/*
	 * Verify that the board is in range
	 */
	if (!BOARD_IN_RANGE(board))
		return (DR_ERROR_STATE);

	/*
	 * Query the board's status
	 */
	status = get_dr_status(board);
	if (status == NULL)
		return (DR_ERROR_STATE);
	else {
		state = status->s_bstate;
		pstate = status->s_pbstate;
		free(status);
	}

	/*
	 * Perform the translation
	 */
	switch (state) {
		case SFDR_STATE_EMPTY:
		case SFDR_STATE_OCCUPIED:
			if (dr_get_mib_update_pending() == board)
				return (DR_OS_DETACHED);
			else
				return (DR_NO_BOARD);

		case SFDR_STATE_UNCONFIGURED:
			cant_abort_complete_instead(board);
			if (get_dr_state(board) == SFDR_STATE_UNCONFIGURED) {
				dr_logerr(DRV_FAIL, 0, "Complete detachment " \
					"of board failed.");
				return (DR_DRAIN);
			}
			else
				return (dr_get_board_state(board));

		case SFDR_STATE_CONNECTED:
			return (DR_ATTACH_INIT);

		case SFDR_STATE_PARTIAL:
			if (pstate == SFDR_STATE_UNREFERENCED)
				return (DR_DRAIN);
			else if (pstate == SFDR_STATE_CONNECTED)
				return (DR_IN_USE);
			else
				return (DR_ERROR_STATE);

		case SFDR_STATE_CONFIGURED:
			return (DR_IN_USE);

		case SFDR_STATE_RELEASE:
		case SFDR_STATE_UNREFERENCED:
			return (DR_DRAIN);

		case SFDR_STATE_FATAL:
		case SFDR_STATE_MAX:
		default:
			return (DR_ERROR_STATE);
	}
}

/*
 * ------------------------------------------------------------------
 * dr_unsafe_devices
 * ------------------------------------------------------------------
 *
 * Called in response to the RPC UNSAFE_DEVICES request.
 *
 * Input:	udp	(structure in which to place the results of this query)
 *
 * Description:	This RPC is supposed to return a list of device driver names
 *		that are currently in the system and that are DR unsafe.
 *
 *		To do this, first the get_unsafe_majors() function is used to
 *		acquire from the DR driver the major numbers of all device
 *		drivers in the system that are both unsafe and referenced.  Then
 *		this list is sorted.  With the sorted list of majors, the
 *		libdevinfo API is used to walk through the whole devfs tree and
 *		seach for each major number from the devfs tree in the sorted
 *		list of unsafe majors.
 *
 *		The reason for the sorting is so that binary search can be used
 *		to check if each major number is unsafe.  There could be N
 *		major numbers in the system, and each of them might be unsafe.
 *		Acquiring the major numbers from the driver takes an unknown
 *		amount of time.  The sorting of the array of unsafe majors
 *		takes O(N*log(N)).  Then, the walk through the devfs tree
 *		while doing binary searches for each discovered major number
 *		takes O(N*log(N)).  Accompanying the discovery/search of each
 *		major number that's unsafe are some costly system calls, such as
 *		malloc().  The time should be bearable.
 *
 *		The old daemon used the /etc/name_to_major file to acquire
 *		a list of devices and it then issued a DR_SAFE_DEVICE ioctl
 *		against each one, serially.  This hints at a possible
 *		alternative to this algorithm that could be used in the future
 *		if the current algorithm has problems.  It involves opening that
 *		file and using it with some code like this:
 *
 *			NAM2MAJR = open("/etc/name_to_major", O_RDONLY);
 *			if (NAM2MAJR == -1) {
 *				dr_logerr(DRV_FAIL, 0, "can't open NAM2MAJR");
 *				return;
 *			}
 *			while (fscanf(NAM2MAJR, "%s %d\n", name, &major) == 2) {
 *				if (bsearch((const void *)&(nodep->drv_major), \
 *					(const void *)(unsafe_devs->major), \
 *					unsafe_devs->maj_len, sizeof (int),
 *					major_compare) != NULL) {
 *
 *					unsafe_devs.name[unsafe_devs.nam_len] =
 *						(name_t)strdup(name);
 *					unsafe_devs.nam_len++;
 *				}
 *			}
 *			close(NAM2MAJR);
 *
 *							- scarter
 *
 * Results:	The udp (unsafe_dev_t pointer) will be filled in with the list
 *		of device names that are not DR safe.
 */
void
dr_unsafe_devices(unsafe_devp_t udp)
{
	dr_unsafe_devs_t	unsafe_devs;
	di_node_t		root_node;

	/*
	 * Acquire the unsafe major numbers for all boards, and sort them.
	 *
	 * Use the major_compare() function for the qsort() call, because it's
	 * aware of the sentinel major number value (-1).
	 */
	get_unsafe_majors(&unsafe_devs);
	if (unsafe_devs.maj_len == 0) {
		udp->unsafe_devs.unsafe_devs_len = 0;
		udp->unsafe_devs.unsafe_devs_val = (name_t *)NULL;
		return;
	}
	qsort((void *)(unsafe_devs.major), unsafe_devs.maj_len, \
			sizeof (int), major_compare);

	/*
	 * We now know how many unsafe devices there are, so allocate memory
	 * for all of the unsafe device names
	 */
	unsafe_devs.name = \
		(name_t *)malloc(unsafe_devs.maj_len * sizeof (name_t));
	if (unsafe_devs.name == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (unsafe_devs)");
		return;
	}
	unsafe_devs.nam_len = 0;

	/*
	 * Walk the devfs tree to retrieve the names for all the unsafe major
	 * numbers.
	 */
	if ((root_node = di_init("/", DINFOSUBTREE)) == DI_NODE_NIL) {
		dr_logerr(DRV_FAIL, errno, \
			"unsafe_devices: libdevinfo failed");
		return;
	}
	di_walk_node(root_node, DI_WALK_CLDFIRST, (void *)&unsafe_devs, \
		get_unsafe_name);
	di_fini(root_node);

	/*
	 * Fill in the given udp structure with the acquired unsafe devices
	 * data.
	 */
	udp->unsafe_devs.unsafe_devs_len = unsafe_devs.nam_len;
	udp->unsafe_devs.unsafe_devs_val = unsafe_devs.name;
}

/*
 * ------------------------------------------------------------------
 * major_compare
 * ------------------------------------------------------------------
 *
 * Perform a compare of major_t's that's appropriate for the qsort() and
 * bsearch() functions.
 *
 * Input:	a, b	(two pointers to major_t's being compared)
 *
 * Description:	Note that the -1 value is an invalid major number.  In the
 *		case that it's being compared against, consider everything
 *		less than it.
 *
 *		This routine de-references and then compares the two major
 *		numbers that it's given.  Warning: it assumes that its
 *		arguments are valid.
 *
 * Return:	-1 if m1 <  m2
 *		 0 if m1 == m2
 *		 1 if m1 >  m2
 */
static int
major_compare(const void *a, const void *b)
{
	major_t		*m1, *m2;

	m1 = (major_t *)a;
	m2 = (major_t *)b;

	if (*m1 == (major_t)-1 || *m2 == (major_t)-1) {
		if (*m1 != (major_t)-1)
			return (-1);
		else if (*m2 != (major_t)-1)
			return (1);
	} else {
		if (*m1 < *m2)
			return (-1);
		else if (*m1 > *m2)
			return (1);
	}

	return (0);
}


/*
 * ------------------------------------------------------------------
 * get_unsafe_name
 * ------------------------------------------------------------------
 *
 * This routine is used to conditionally add a devfs node's device name
 * to the unsafe_devs.name array.  (On the condition that the device is
 * unsafe.)
 *
 * NOTE: Only the di_walk_node() routine calls this, to parse info out of
 * each devfs node.
 *
 * Input:	node	(devfs node to check)
 *		argp	(argument pointer; in this case, it is a pointer to
 *			 the unsafe_devs struct that contains the list of
 *			 major numbers for all unsafe devices in the domain)
 *
 * Description:	Note that the ultimate return value from the dr_unsafe_devs
 *		function is just a list of names of unsafe devices.  This
 *		routine gets the major number of a devfs node, uses bsearch()
 *		to see if the devfs node represents an unsafe device, and then
 *		if so it gets the devfs node's device name and adds it into the
 *		unsafe_devs.name array.  The device names are added in no
 *		particular order; just the order that the devfs-tree walk finds
 *		them.
 *
 * Results:	unsafe_devs.name will have a single name added to it if
 *		the given devfs node represents an unsafe device.
 */
static int
get_unsafe_name(di_node_t node, void *argp)
{
	struct di_node		*nodep;
	dr_unsafe_devs_t	*unsafe_devs;
	name_t			drv_name;
	char			unknown_name[MAXLEN];

	/*
	 * Dereference node and argp
	 */
	nodep = (struct di_node *)node;
	unsafe_devs = (dr_unsafe_devs_t *)argp;

	/*
	 * Verify that this device node is valid
	 */
	if (nodep->drv_major == -1)
		return (DI_WALK_CONTINUE);

	/*
	 * If the major number for this devfs node is in the unsafe_devs
	 * major list...
	 */
	if (bsearch((const void *)&(nodep->drv_major), \
			(const void *)(unsafe_devs->major), \
			unsafe_devs->maj_len, sizeof (int),
			major_compare) != NULL) {

		/*
		 * Try acquiring its name.  Put an "(unknown, #)" into the
		 * name list if the lookup fails, and warn the user.
		 */
		if ((drv_name = di_driver_name(node)) == NULL) {
			dr_loginfo("unsafe_devices: couldn't determine name " \
				"of unsafe device %d.", nodep->drv_major);
			(void) sprintf(unknown_name, "(unknown, %d)", \
				nodep->drv_major);

			drv_name = unknown_name;
		}

		/* Ignore dups */
		if (lfind((const void *)&drv_name,
			(const void *)unsafe_devs->name,
			(size_t *)&unsafe_devs->nam_len,
			(size_t)sizeof (name_t),
			cpp_cmp) == NULL) {

			unsafe_devs->name[unsafe_devs->nam_len] = \
			(name_t)strdup(drv_name);

			unsafe_devs->nam_len++;
		}
	}

	/*
	 * Done analyzing this node...continue the walk
	 */
	return (DI_WALK_CONTINUE);
}

/*
 * ------------------------------------------------------------------
 * get_unsafe_majors
 * ------------------------------------------------------------------
 *
 * Acquire the major numbers of all unsafe devices present in the current
 * domain.
 *
 * Input:	unsafe_devs	(struct to fill in with unsafe major numbers)
 *
 * Description:	unsafe_devs is initialized, and then filled in with the
 *		major numbers of all unsafe devices in the current domain.
 *
 *		To find the unsafe devices, the DR_CMD_STATUS ioctl is used
 *		on the "allio" target for every board in the system.  Any
 *		board whose status indicates that it's CONFIGURED (or PARTIAL)
 *		has its IO nodes examined.  Any unsafe major numbers listed in
 *		the IO nodes are placed in "unsafe_majors."
 *
 * Results:	unsafe_devs->maj_len will indicate how many unsafe devices
 *		were found, and unsafe_devs->major[] contains all of the
 *		unsafe devices' major numbers.
 */
static void
get_unsafe_majors(dr_unsafe_devs_t *unsafe_devs)
{
	sfdr_stat_t		stat;
	int			brd;
	int			node;
	int			count;
	int			i;

	/*
	 * Initialize "unsafe_devs" to be empty
	 */
	if (unsafe_devs == NULL)
		return;
	unsafe_devs->maj_len = 0;

	/*
	 * Iterate through each board
	 */
	for (brd = 0; brd < MAX_BOARDS; brd++) {

		/*
		 * Issue a DR_CMD_STATUS against the board's "board" target.
		 * If this fails, just warn the user and skip this board.
		 */
		memset((caddr_t)&stat, 0, sizeof (sfdr_stat_t));
		if (dr_issue_ioctl(DR_IOCTARG_BRD, brd, DR_CMD_STATUS, \
				(void *)&stat, DR_IOCTL_SAFDEV)) {

			dr_loginfo("WARNING: board %d not checked for unsafe " \
					"devices", brd);
			continue;
		}

		/*
		 * Verify that the board is in the current domain
		 */
		if (stat.s_bstate != SFDR_STATE_PARTIAL &&
		    stat.s_bstate != SFDR_STATE_CONFIGURED)
			continue;

		/*
		 * Iterate through each of the board's status nodes
		 */
		for (node = 0; node < stat.s_nstat; node++) {

			/*
			 * Verify that it's an IO status node
			 */
			if (stat.s_stat[node].d_common.c_type != DR_NT_IO)
				continue;

			/*
			 * Add all of the node's unsafe majors to our struct
			 */
			count = stat.s_stat[node].d_io.is_unsafe_count;
			for (i = 0; i < count; i++) {
				unsafe_devs->major[unsafe_devs->maj_len] = \
			stat.s_stat[node].d_io.is_unsafe_list[i];
				unsafe_devs->maj_len++;
			}
		}
	}
}

/*
 * ------------------------------------------------------------------
 * cpp_cmp
 * ------------------------------------------------------------------
 *
 * Comparison routine required by lfind.
 *
 * Input:	cpp1, cpp2	(pointers to pointers to strings being compared)
 *
 * Description:	Dereferences char ** params and returns result of strcmp.
 *
 * Return:	-1 if *cpp1 < *cpp2
 *		 0 if *cpp1 == *cpp2
 *		 1 if *cpp1 > *cpp2
 */
static int
cpp_cmp(const void *cpp1, const void *cpp2)
{
	char *cp1, *cp2;

	cp1 = *(char **)cpp1;
	cp2 = *(char **)cpp2;

	return (strcmp(cp1, cp2));
}
