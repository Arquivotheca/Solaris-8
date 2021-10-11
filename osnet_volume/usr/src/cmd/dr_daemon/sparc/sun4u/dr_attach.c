/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_attach.c	1.52	98/10/22 SMI"

/*
 * This file implements the Dynamic Reconfiguration Attach functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <utmpx.h>
#include <sys/processor.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "dr_subr.h"

/*
 * Forward references.
 */
static void update_cpu_info_attach(int board);
static void update_cpu_utmp_info(processorid_t cpuid, int cpustate);
#ifndef NODRKERN
static void exit_unlock(void);
#endif NODRKERN

/*
 * -----------------------------------------------------------------
 * dr_init_attach
 * -----------------------------------------------------------------
 *
 * Implementation layer of the initiate_attach_board RPC routine
 *
 * Input:	attachp->board_slot	(board to init-attach)
 *		attachp->cpu_on_board	(CPU with the OBP structures from POST)
 *
 * Description:	When the policy checks at the RPC entry point are passed,
 *		this routine is called to actually perform an init-attach DR
 *		operation.  This routine can trust what the RPC layer gives it.
 *
 *		The DR_CMD_CONNECT ioctl is used to perform the probe/merge
 *		of the OBP nodes for the board.
 *
 * Results:	The board's state could change -- it could get connected.
 */
void
dr_init_attach(brd_init_attach_t *attachp)
{
	sfdr_ioctl_arg_t	ioctl_arg;

	/* Log entry to this routine */
	if (verbose)
		dr_loginfo("init-attach: connecting board %d using cpu %d\n",
				attachp->board_slot, attachp->cpu_on_board);

	/* Issue the ioctl and process any errors */
	memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
	ioctl_arg.i_cpuid = attachp->cpu_on_board;
	if (dr_issue_ioctl(DR_IOCTARG_BRD, attachp->board_slot, DR_CMD_CONNECT,
			(void *)&ioctl_arg, DR_IOCTL_IATTACH)) {
		dr_report_errors(&ioctl_arg, errno, \
					"initiate_attach: ioctl failed");
	}
}

/*
 * -----------------------------------------------------------------
 * dr_complete_attach
 * -----------------------------------------------------------------
 *
 * Implementation layer of the complete_attach_board RPC routine
 *
 * Input:	board	(board to complete-attach)
 *
 * Description:	When the policy checks at the RPC entry point are passed,
 *		this routine is called to actually perform a complete-attach DR
 *		operation.  This routine can trust what the RPC layer gives it.
 *
 *		The DR_CMD_CONFIGURE ioctl is used to add the devices for
 *		the board to the OS.  (The SFDR_FLAG_AUTO_CPU flag is set for
 *		this ioctl to make sure CPU's are automatically onlined.)
 *
 *		Errors from the ioctl may not be horrible; an error might
 *		indicate that the board was partially attached (meaning some
 *		devices were not configured).
 *
 *		The utmpx/wtmpx files, the Sonoma daemons, and the AP library
 *		are all informed of a successful (or partially successful)
 *		board attachment.
 *
 * Results:	There could be an error and the board could remain in the
 *		CONNECTED state (if it's a non-fatal error and it didn't go
 *		to the FATAL state).  Or, there could be a partial attachment
 *		and the board could go to the PARTIAL state.  Or, this routine
 *		could be totally successful and the board could transition to
 *		the CONFIGURED state.  In the last two cases, the OS is
 *		updated with the news of the board's attachment.
 */
void
dr_complete_attach(int board)
{
	sfdr_ioctl_arg_t	ioctl_arg;
	int			iocerr;
	int			iocerrno;

	/* Log the call to this routine */
	if (verbose)
		dr_loginfo("complete-attach: configuring board %d\n", board);

	/* Issue the ioctl */
	memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
	ioctl_arg.i_flags = SFDR_FLAG_AUTO_CPU;
	iocerr = dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_CONFIGURE, \
			(void *)&ioctl_arg, DR_IOCTL_CATTACH);

	/* Process any errors */
	if (iocerr == DRV_FAIL) {

		/* Save the ioctl()'s errno, in case get_dr_state changes it */
		iocerrno = errno;

		/*
		 * The PARTIAL board state indicates that some devices weren't
		 * configured.  Notify the user.  This isn't fatal.
		 */
		if (get_dr_state(board) == SFDR_STATE_PARTIAL)
			dr_logerr(DRV_FAIL, iocerrno, "Some devices not " \
				"attached. Examine the host syslog for " \
				"details");

		/* Anything other than a partial attachment is a fatal error */
		else {
			dr_report_errors(&ioctl_arg, iocerrno, \
					"complete_attach: ioctl failed");
			return;
		}
	}

	/* Inform other programs that the board's significantly different now */
	dr_signal_operation_complete(board);

	/* Update the memory address map with the new board */
	dr_init_sysmap();

	/* Update the utmpx file for cpu's now attached */
	update_cpu_info_attach(board);

	/* Notify AP that the board is now available */
	dr_ap_notify(board, DR_OS_ATTACHED);

	/* Run the commands to allow sonoma devices to be recognized */
#ifdef	SONOMA
	dr_rsm_hot_add();
#endif	/* SONOMA */
#ifdef	DR_NEW_SONOMA
	dr_new_sonoma();
#endif	/* DR_NEW_SONOMA */
}

/*
 * ----------------------------------------------------------------------
 * dr_abort_attach_board
 * ----------------------------------------------------------------------
 *
 * Implementation layer of the abort_attach_board RPC routine
 *
 * Input:	board	(board to abort-attach)
 *
 * Description:	When the policy checks at the RPC entry point are passed,
 *		this routine is called to actually perform an abort-attach DR
 *		operation.  This routine can trust what the RPC layer gives it.
 *
 *		The DR_CMD_DISCONNECT ioctl is used to parse out and remove
 *		the OBP nodes for the board (which were merged in during the
 *		init-attach operation).
 *
 * Results:	If successful the board could go back to the EMPTY state.
 */
void
dr_abort_attach_board(int board)
{
	sfdr_ioctl_arg_t	ioctl_arg;
	int			iocerrno;

#ifdef DR_TEST_VECTOR
	/* Make sure that the abort DR operations will not fail artificially */
	dr_test_vector_clear();
#endif DR_TEST_VECTOR

	/* Log entry to this routine */
	if (verbose)
		dr_loginfo("abort-attach: disconnecting board %d\n", board);

	/* Issue the ioctl and process any errors */
	memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_DISCONNECT, \
			(void *)&ioctl_arg, DR_IOCTL_DEVDETACH)) {

		/* Save the errno, because get_dr_state() could change it */
		iocerrno = errno;

		/* A botched abort-attach could be an extreme error */
		if (get_dr_state(board) == SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0, "Cannot abort attach. Board " \
				"ineligible for further DR operations.");
		else
			dr_report_errors(&ioctl_arg, iocerrno, \
						"abort_attach: ioctl failed");
	}

	/* Inform other programs that the board's significantly different now */
	dr_signal_operation_complete(board);
}

/*
 * -----------------------------------------------------------------
 * update_cpu_info_attach
 * -----------------------------------------------------------------
 *
 * Utility routine that updates CPU information for the utmpx/wtmpx data.
 *
 * Input:	board (board just attached)
 *
 * Description:	When a board gets attached, any CPU's on the board need to have
 *		their information in the utmpx/wtmpx data updated.  This routine
 *		iterates through all of the CPU's on the newly attached board
 *		updating the utmpx/wtmpx data entries for each CPU.
 *
 *		The information discussed here is what the "prsinfo" command
 *		shows as the attachment time for the CPU's.
 */
static void
update_cpu_info_attach(int board)
{
	sfdr_stat_t		status;
	processorid_t		cpuid;
	processor_info_t	pi;
	int			i;

	/* Verify that the board argument is valid */
	if (!BOARD_IN_RANGE(board)) {
		dr_loginfo("update_cpu_info: bad board number\n");
		return;
	}

	/* Query the board's status and report any error */
	memset((caddr_t)(&status), 0, sizeof (sfdr_stat_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
			(void *)&status, NULL)) {
		dr_logerr(DRV_FAIL, errno, "update_attach: ioctl failed");
		return;
	}

	/*
	 * At this point the status for the board is loaded, so pluck out
	 * the CPU status information and update the utmpx/wtmpx data with the
	 * new CPU information
	 */
	for (i = 0; i < status.s_nstat; i++) {
		if (status.s_stat[i].d_common.c_type == DR_NT_CPU) {
			cpuid = status.s_stat[i].d_cpu.cs_cpuid;
			if (processor_info(cpuid, &pi) == 0)
				update_cpu_utmp_info(cpuid, pi.pi_state);
		}
	}
}

/*
 * ------------------------------------------------------------
 * update_cpu_info_detach
 * ------------------------------------------------------------
 *
 * Update the utmpx file to put a cpu into the OFFLINE state
 *
 * Input: 	cpuid		(which is detaching)
 *
 * Description:	Set the cpu's info to "offline"
 */
void
update_cpu_info_detach(processorid_t cpuid)
{
	update_cpu_utmp_info(cpuid, P_OFFLINE);
}

/*
 * ------------------------------------------------------------
 * update_cpu_utmp_info
 * ------------------------------------------------------------
 *
 * Update the utmpx file to put a cpu into a specified state
 *
 * Input:	cpuid		(Processor ID for the CPU entry to update)
 *		cpustate	(New state for the CPU)
 *
 * Descrption:	The utmpx entry for 'cpuid' is updated with the current time and
 *		the new state.  This will be the info displayed by "psrinfo.".
 */
static void
update_cpu_utmp_info(processorid_t cpuid, int cpustate)
{
#ifdef NODRKERN
	/* dummy this out */
	return;
#else
	struct utmpx	ut;

	sprintf(ut.ut_line, PSRADM_MSG,
		cpuid, (cpustate == P_ONLINE) ? "on" : "off");
	strncpy(ut.ut_user, DR_USER, sizeof (ut.ut_user)-1);
	ut.ut_pid  = getpid();
	ut.ut_type = USER_PROCESS;
	ut.ut_xtime = time((time_t *)0);
	updwtmpx(WTMPX_FILE, &ut);

#endif NODRKERN
}

#define	ADD_REM_LOCK    "/tmp/AdDrEm.lck"

#define	DRVCONFIG_PATH  "/usr/sbin/drvconfig"
#define	DEVLINKS_PATH   "/usr/sbin/devlinks"
#define	DISKS_PATH	"/usr/sbin/disks"
#define	PORTS_PATH	"/usr/sbin/ports"
#define	TAPES_PATH	"/usr/sbin/tapes"
#define	SYNC_PATH	"/usr/sbin/sync"

/*
 * ------------------------------------------------------------
 * autoconfig
 * ------------------------------------------------------------
 *
 * Once a board has been attached, run all IO related configuration utilities.
 *
 * Description: Read the code below for further details; but, here's summary of
 *		what this routine does follows:
 *
 *			- A lock is set for this operation, so that others
 *			  won't run conflicting copies of these utilities
 *
 *			- The following configuration utilities are fork/exec'ed
 *				. drvconfig
 *				. devlinks
 *				. disks
 *				. ports
 *				. tapes
 *
 *			- The disks are sync'ed a few times for good measure
 *			- The lock is released.
 */
void
autoconfig()
{
#ifdef NODRKERN
	/* only test this on the real system */
	return;
#else
	struct stat	buf;
	FILE 		*fp;
	char 		*cmdline[MAX_CMD_LINE];
	char  		msg[MAXMSGLEN];

	/*
	 * Log entry into this routine
	 */
	if (verbose)
		dr_loginfo("beginning auto-configuration\n");

	/*
	 * Set the lock
	 */
	if (verbose)
		dr_loginfo("getting lock %s\n", ADD_REM_LOCK);

	if ((stat(ADD_REM_LOCK, &buf) == -1) && errno == ENOENT) {
		fp = fopen(ADD_REM_LOCK, "a");
		(void) fclose(fp);
	} else {
		sprintf(msg, "Could not get %s lock.", ADD_REM_LOCK);
		(void) dr_logerr(DRV_FAIL, errno, msg);
		return;
	}

	/*
	 * Run the configuration utilities
	 */
	errno = 0;
	cmdline[1] = (char *)0;

	/* drvconfig */
	cmdline[0] = "drvconfig";

	if (verbose)
		dr_loginfo("running %s\n", DRVCONFIG_PATH);

	if (exec_command(DRVCONFIG_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, errno, "drvconfig cmd failed.");
		(void) exit_unlock();
		return;
	}

	/* devlinks */
	cmdline[0] = "devlinks";

	if (verbose)
		dr_loginfo("running %s\n", DEVLINKS_PATH);

	if (exec_command(DEVLINKS_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, errno, "devlinks cmd failed.");
		(void) exit_unlock();
		return;
	}

	/* disks */
	cmdline[0] = "disks";

	if (verbose)
		dr_loginfo("running %s\n", DISKS_PATH);

	if (exec_command(DISKS_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, errno, "disks cmd failed.");
		(void) exit_unlock();
		return;
	}

	/* ports */
	cmdline[0] = "ports";

	if (verbose)
		dr_loginfo("running %s\n", PORTS_PATH);

	if (exec_command(PORTS_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, errno, "ports cmd failed.");
		(void) exit_unlock();
		return;
	}

	/* tapes */
	cmdline[0] = "tapes";

	if (verbose)
		dr_loginfo("running %s\n", TAPES_PATH);

	if (exec_command(TAPES_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, 0, "tapes cmd failed.");
		(void) exit_unlock();
		return;
	}

	/* sync the disks for good measure */
	cmdline[0] = "sync";

	if (verbose)
		dr_loginfo("running %s\n", SYNC_PATH);

	if (exec_command(SYNC_PATH, cmdline)) {
		(void) dr_logerr(DRV_FAIL, 0, "sync cmd failed.");
		(void) exit_unlock();
		return;
	}

	/*
	 * Unset the lock
	 */
	exit_unlock();
#endif NODRKERN
}

/*
 * ------------------------------------------------------------
 * exit_unlock
 * ------------------------------------------------------------
 *
 * Unset the lock used by autoconfig()
 *
 */
#ifndef NODRKERN
static void
exit_unlock()
{
	struct stat	 buf;
	char  		msg[MAXMSGLEN];

	if (verbose)
		dr_loginfo("releasing lock %s\n", ADD_REM_LOCK);

	errno = 0;
	if (stat(ADD_REM_LOCK, &buf) == 0) {
		if (unlink(ADD_REM_LOCK) == -1) {
			sprintf(msg, "Could not unlock %s lock.",
				ADD_REM_LOCK);
			(void) dr_logerr(DRV_FAIL, errno, msg);
		}
	}
}
#endif NODRKERN

/*
 * ------------------------------------------------------------
 * check_wait_stat
 * ------------------------------------------------------------
 *
 * Check the wait stat from a child process and report any exceptions
 * on this status.
 *
 * Input:	cmd		(the executed command)
 *		stat_loc	(its returned status)
 *
 * Description:	The stat_loc is parsed, and the reason for the program's exit
 *		is described.
 *
 * Results:	Returns a -1 if process has terminated abnormally.  Error is
 *		reported already otherwise this is the exit status of the
 *		program (always positive).
 */
static int
check_wait_stat(const char *cmd, int stat_loc)
{
	char msg[MAXMSGLEN];

	if (WIFEXITED(stat_loc)) {
		return (WEXITSTATUS(stat_loc));
	}

	if (WIFSIGNALED(stat_loc)) {
		sprintf(msg, "%s terminated due to signal %d.",
			cmd, WTERMSIG(stat_loc));
		if (WCOREDUMP(stat_loc))
			strcat(msg, "  Core Dumped.");

	} else if (WIFSTOPPED(stat_loc)) {
		sprintf(msg, "%s stopped by signal %d\n",
			cmd, WSTOPSIG(stat_loc));

	} else if (WIFCONTINUED(stat_loc)) {
		sprintf(msg, "%s has continued\n", cmd);
	}

	dr_logerr(DRV_FAIL, 0, msg);
	return (-1);
}

/*
 * ------------------------------------------------------------
 * exec_command
 * ------------------------------------------------------------
 *
 * Execute the given command and return it's status.
 *
 * Input:	path		(file for the newly exec'ed process)
 *		cmdline		(command line to execute the new process)
 *
 * Description:	A new process is forked, and in it the path/cmdline combo is
 *		execv'ed.  This process then joins (with waitpid()) with the
 *		routine to get its exit status.  The exit status is interpreted
 *		and passed back up the line.
 */
int
exec_command(char *path, char *cmdline[MAX_CMD_LINE])
{

	pid_t pid;
	int stat_loc;
	int waitstat;

	errno = 0;

	/* Do the fork/exec of the command */

	if ((pid = fork()) == 0) {
		/* child */
		(void) execv(path, cmdline);
		dr_loginfo("Cannot exec %s (errno = %d).", path, errno);
		exit(-1);
	}

	/* parent */

	/* Process any errors with the fork/exec */
	else if (pid == -1) {
		dr_logerr(DRV_FAIL, errno, "Cannot fork() process.");
		return (-1);
	}

	/* If no errors, join with the child process */
	do {
		waitstat = waitpid(pid, (int *)&stat_loc, 0);

	} while ((!WIFEXITED(stat_loc) &&
		    !WIFSIGNALED(stat_loc)) || (waitstat == 0));

	/* Return an interpretation of the child's exit status */
	return (check_wait_stat(cmdline[0], stat_loc));
}

/*
 * ------------------------------------------------------------
 * dosys
 * ------------------------------------------------------------
 *
 * execute the given string via the system(2s) call.  Cmd format
 * should be correct sh(1) syntax.  system waits until the command
 * terminates.
 *
 * Input:
 *	cmd - command string to execute
 *
 * Function Return Value:
 *	< 0 - command failed - reason reported.
 *	otherwise, the value returned from the command.
 */
int
dosys(const char *cmd)
{
	int stat;

	if (verbose)
		dr_loginfo("running %s", cmd);

	stat = system(cmd);

	return (check_wait_stat(cmd, stat));
}
