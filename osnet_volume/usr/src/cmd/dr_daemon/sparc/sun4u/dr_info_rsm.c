/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_info_rsm.c	1.11	99/04/22 SMI"

/*
 * The dr_info_*.c files determine devices on a specified board and
 * their current usage by the system.
 *
 * This file deals with the stopping and restarting of the RSM 2000 support
 * daemons.  These daemons (actually its multiple instances of the same daemon)
 * block on cv_wait() which prevents the OS from suspending.  If the current
 * detach requires an OS suspension, then we'll stop, and restart these
 * daemons.
 */

#include "dr_info.h"

#ifdef	DR_NEW_SONOMA
#define	SONOMA_HOT_ADD	"/etc/raid/bin/hot_add"
#endif	/* DR_NEW_SONOMA */

#ifdef	SONOMA

/* Local functions */
static int kill_rsm_daemon(char *find_cmd, char *kill_cmd);

/*
 * Defines to find, kill off, and restart the RSM daemons
 */
#define	FIND_RSMD_CMD \
	"(/usr/bin/ps -e | /usr/bin/grep rdaemon) >/dev/null 2>/dev/null"
#define	KILL_RSMD_CMD \
	"/etc/rcS.d/S45rdac"	/* stop */
#define	START_RSMD_CMD \
	"/etc/rcS.d/S45rdac"	/* start */

/*
 * Defines to allow the RAID Manager software to see newly
 * added array devices.
 */
#define	PKG_LOADED "/usr/bin/pkginfo | /usr/bin/grep -i SUNWosa"
#define	SYMDISKS	"/etc/raid/bin/symdisks -r /dev/osa -d sd"
#define	SYMCONF		"/etc/raid/bin/symconf > /kernel/drv/rdriver.conf"
#define	DRVCONFIG	"/usr/sbin/drvconfig -i rdriver"
#define	DISKS		"/usr/sbin/disks"

static daemon_type_t rsmd_state = DAEMON_UNKNOWN;

/*
 * ----------------------------------------------------------------------
 * dr_stop_rsm_daemons
 * ----------------------------------------------------------------------
 *
 * Stop any RSM daemons that must be killed prior to suspending the OS.
 *
 * Input:	board	(board being detached)
 *
 * Description:	Determine which, if any, RSM daemons must be killed.  Then
 *		kill the daemons if necessary.
 *
 *		With Stafire, it's only necessary if non-pageable memory exists
 *		on the board being detached.  With Superdragon, it's always
 *		necessary for an attach or detach.
 *		only
 *
 * Results:	Any RSM daemons that must be killed are killed.
 *
 *		The return value is DRV_SUCCESS if successful, DRV_FAIL if not.
 */
int
dr_stop_rsm_daemons(int board)
{
#ifdef	_XFIRE
	board_mem_configp_t	config;

	/*
	 * Query the memory configuration info for this board
	 */
	config = get_mem_config(board, NULL);
	if (config == NULL) {
		dr_logerr(DRV_FAIL, 0, "memory configuration query failed");
		return (DRV_FAIL);
	}

	/*
	 * If this board contains non-pageable memory, a quiesce will be
	 * required... so, nuke the RSM daemons.
	 */
	if (config->perm_memory == 1) {
		if (verbose)
			dr_loginfo("non-pageable memory on detaching board\n");
		if (rsmd_state == DAEMON_UNKNOWN) {
			rsmd_state = kill_rsm_daemon(FIND_RSMD_CMD,
							KILL_RSMD_CMD);
		}
	}
	free_mem_config(config);
#endif	_XFIRE


	return (DRV_SUCCESS);
}

/*
 * ----------------------------------------------------------------------
 * dr_restart_rsm_daemons
 * ----------------------------------------------------------------------
 *
 * This routine is called after a board has been detached or the detach
 * operation is aborted.
 *
 * If necessary, we'll restart the RSM daemons if they were killed off during
 * the detach operation (i.e. we did an OS suspend).
 */
void
dr_restart_rsm_daemons(void)
{
	char *cmdline[MAX_CMD_LINE];

	if (verbose) dr_loginfo("Called dr_restart_rsm_daemons\n");

	if (rsmd_state == DAEMON_KILLED) {

		if (verbose)
			dr_loginfo("rsmd_state == DAEMON_KILLED\n");
		cmdline[0] = "S45rdac";
		cmdline[1] = "start";
		cmdline[2] = (char *)0;
		if (verbose)
			dr_loginfo("running %s\n", START_RSMD_CMD);

#ifndef NO_SU
		/*
		 * Really don't care about the return value since
		 * rsm seems to be arbitrary about it.  If the
		 * exec failed, this has already been reported and
		 * that's the only error we're interested in.
		 */
		(void) exec_command(START_RSMD_CMD, cmdline);
#endif NO_SU
	}
	rsmd_state = DAEMON_UNKNOWN;

}

/*
 * ----------------------------------------------------------------------
 * kill_rsm_daemon
 * ----------------------------------------------------------------------
 *
 * This routine is called when we're detaching a board which contains
 * non-pageable memory, such that a suspend is necessary
 *
 * If a daemon is running, kill it off and remember that we need
 * to restart it after the detach operation is complete.  The detach
 * code will call _restart_ functions once the
 * detach is successfully completed or if the detach operation is aborted.
 *
 * Input
 *	find_cmd	- command to execute to determine if the daemon is
 *			  is executing
 *	kill_cmd	- command to execute to kill the daemon
 *
 * Output
 *	the state of the network daemon (killed, not present, unknown)
 */
static int
kill_rsm_daemon(char *find_cmd, char *kill_cmd)
{
	int retval;
	char *cmdline[MAX_CMD_LINE];

	/*
	 * See if we have net daemons currently running
	 */
	retval = dosys(find_cmd);

	if (retval < 0)
		/* Error executing the command (already reported) */
		return (DAEMON_UNKNOWN);

	if (retval != 0) {
		/* No daemons executing */
		return (DAEMON_NOT_PRESENT);
	}

	/*
	 * daemons present, off with their heads...
	 */
	cmdline[0] = kill_cmd;
	cmdline[1] = "stop";
	cmdline[2] = (char *)0;
	if (verbose)
		dr_loginfo("running %s\n", kill_cmd);

#ifndef NO_SU
	if ((retval = exec_command(kill_cmd, cmdline)) != 0) {
		dr_loginfo("Warning: Error return from %s (%d)\n",
			    kill_cmd, retval);
	}
#endif NO_SU

	return (DAEMON_KILLED);
}

/*
 * ----------------------------------------------------------------------
 * dr_rsm_hot_add
 * ----------------------------------------------------------------------
 *
 * Run necessary commands to let the RAID manager software
 * be able to access the newly added devices.
 *
 * NOTE - this routine is based on the dr_hotadd.sh script
 * provided with the sonoma software. If dr_hostadd.sh is
 * updated so must this routine! We can't just call the dr_hostadd.sh
 * script since it is not installed on the system by the
 * sonoma packages. It is just a tool script provided for the
 * customers convienence which can be installed anywhere (or nowhere).
 * BUMMER!
 */
void
dr_rsm_hot_add(void)
{
	int retval;

	/* first check to see if the sonoma packages are installed */
	retval = dosys(PKG_LOADED);
	if (retval < 0) {
		/* Error executing the command (already reported) */
		return;
	}

	if (retval != 0) {
		/* packages not installed. */
		return;
	}

	/* If we get an error during one of these, just continue on... */
	retval = dosys(SYMDISKS);
	retval = dosys(SYMCONF);
	retval = dosys(DRVCONFIG);
	retval = dosys(DISKS);
}
#endif	/* SONOMA */

#ifdef	DR_NEW_SONOMA
/*
 * ----------------------------------------------------------------------
 * dr_new_sonoma
 * ----------------------------------------------------------------------
 *
 * Run necessary command to let the RAID manager software be able to
 * access the newly added devices.  This is a replacement to the old
 * SONOMA hot_add support.
 *
 * This routine attempts to execute the new Sonoma hot_add script.  If
 * the script simply doesn't exist, than it is assumed the RAID manager
 * software isn't installed.  Otherwise, it is executed and any errors
 * with the script are reported.
 */
void
dr_new_sonoma(void)
{
	struct stat	buf;

	/* Try stat'ing the script, to see if it's installed */
	if (stat(SONOMA_HOT_ADD, &buf) == -1) {

		/* If the script doesn't exist, then maybe it's not installed */
		if (errno == ENOENT)
			return;

		/* Otherwise, there's some access error.  Report it. */
		dr_loginfo("dr_attach: failure executing %s script...%s",
			SONOMA_HOT_ADD, strerror(errno));
		return;
	}

	/* Try executing the script, ignoring failure */
	(void) dosys(SONOMA_HOT_ADD);
}
#endif	/* DR_NEW_SONOMA */
