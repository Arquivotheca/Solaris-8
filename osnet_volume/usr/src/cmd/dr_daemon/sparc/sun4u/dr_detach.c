/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_detach.c	1.66	99/09/21 SMI"

/*
 * This file implements the Dynamic Reconfiguration Detach functions.
 */

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/processor.h>
#ifdef	_JCF
#include <sys/procset.h>
#endif	/* _JCF */
#include <sys/cpuvar.h>
#include <utmpx.h>
#include <time.h>
#include <sys/stat.h>

#include "dr_subr.h"

/* temporary file for backup copy of the copy-rename details */
#define	DR_PENDING_FILE	"/tmp/.dr_pending"

/* format of the of the temporary file's contents */
typedef struct dr_pending_t {
	struct val_t {
		int		pending_flag;
		sysbrd_info_t	sysmap;
	} val;
	ulong_t	checksum;
} dr_pending_t;

/* globals related to this module */
time_t			drain_start_time;
static sysbrd_info_t	dr_sysmap;
static int		dr_mib_update_pending = -1;

/* forward references */
void dr_init_sysmap(void);
static void pre_detect_copy_rename(int board);
static void detect_copy_rename(int board, detach_errp_t dep);
static int check_pending_file(char *pending_file);
static int detach_io_ready(int board);
static ulong_t cksum(void *data, size_t size);
static void log_unsafe_devs(void);
static void free_unsafe_devs(unsafe_devp_t udp);

/*
 * -----------------------------------------------------------------------
 * dr_detachable_board
 * -----------------------------------------------------------------------
 *
 * Implementation layer of the detachable_board() RPC routine.
 *
 * Input:	board		(system board to look at)
 *
 * Description:	Sometimes this routine is called simply because a MIB update
 *		is pending (ie, the SSP application crashed during a complete
 *		detach operation, and didn't get copy_rename info integrated.
 *		This protects against inconsistencies in the MIB mappings.
 *		A check is made to see if that's why a complete_detach was
 *		initiated.  If so, then an attempt to put in the correct
 *		copy rename info into the error structure is made.  Otherwise,
 *		all of the normal steps for this routine must be followed as
 *		described below.
 *
 *		First, how much memory would be left in the system after
 *		detaching this board is determined.  The amount left over
 *		can't be less than DR_MEM_FLOOR.
 *
 *		Second, query all of the other boards attached to the OS and
 *		see if they have CPUs (so as not to leave the system
 *		CPU-less after a detach).
 *
 * Output:	If this call fails (dr_logerr(ERROR)), means the board is not
 *		detachable.
 */
void
dr_detachable_board(int board)
{
	brd_info_t		bip;
	attached_board_info_t	bcp;
	int			mb_left;
	int			sys_cpus;
	int			brd_cpus = 0;
	int			i;
	char			msg[MAXMSGLEN];

	/*
	 * Query the memory and CPU configuration info for this board by:
	 *		1) initializing "bip" to get this board's memory and
	 *		   cpu information
	 *		2) call dr_get_attached_board_info() to get the info
	 */

	/* Step 1 */
	bip.board_slot = board;
	bip.flag = (BRD_MEM_CONFIG | BRD_CPU);


	/* Step 2 */
	dr_get_attached_board_info(&bip, &bcp);
	if (bcp.mem_config == NULL || bcp.cpu == NULL) {
		dr_logerr(DRV_FAIL, 0, "board configuration query failed");
		return;
	}

	/*
	 * With the memory configuration queried, use the following steps to see
	 * if the memory configuration after a detach is acceptable:
	 *
	 *		1) Check dr_max_mem to see if a detach is even allowed
	 *		2) Check dr_mem_detach if memory detach is allowed
	 *		3) Compute how many megabytes would be left over
	 *		4) Compare against the floor condition.
	 *
	 * Note: Bugid 4063645: if the board has no memory, don't bother
	 * checking for the floor condition in Step 3.
	 */

	/* Step 1 */
	if (bcp.mem_config->dr_max_mem == 0) {
		dr_logerr(DRV_FAIL, 0, \
			"Detach is not enabled.  Must reboot system with " \
			"dr-max-mem set to a non-zero value.");
		free_mem_config(bcp.mem_config);
		free_cpu_config(bcp.cpu);
		bcp.mem_config = NULL;
		bcp.cpu = NULL;
		return;
	}

	/* Step 2 */
	if (bcp.mem_config->dr_mem_detach[0] == '0') {
		dr_logerr(DRV_FAIL, 0, \
			"Detach is not enabled.  Must reboot system with " \
			"kernel_cage_enable set to a non-zero value.");
		free_mem_config(bcp.mem_config);
		free_cpu_config(bcp.cpu);
		bcp.mem_config = NULL;
		bcp.cpu = NULL;
		return;
	}

	/* Step 3 */
	mb_left = bcp.mem_config->sys_mem - bcp.mem_config->mem_size;

	/* Step 4 */
	if ((bcp.mem_config->mem_pages.h_pfn != 0) && \
			(mb_left < DR_MEM_FLOOR)) {
		sprintf(msg, "Remaining system memory (%d mb) below minimum " \
			"threshold (%d mb).", mb_left, DR_MEM_FLOOR);
		dr_logerr(DRV_FAIL, ENOMEM, msg);
		free_mem_config(bcp.mem_config);
		free_cpu_config(bcp.cpu);
		bcp.mem_config = NULL;
		bcp.cpu = NULL;
		return;
	}

	free_mem_config(bcp.mem_config);
	bcp.mem_config = NULL;

	/*
	 * The memory check passed, so now we check to see if detaching this
	 * board would leave us CPU-less.  sysconf() reports the number of
	 * configured CPUs, and our cpu value has a CPU count.  Check if the
	 * sysconf() value is larger than the cpu value; if not, we can't
	 * detach this board.
	 */
	sys_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	for (i = 0; i < bcp.cpu->cpu_cnt; i++)
		if (bcp.cpu->cpu[i].cpu_state != NULL && \
			strcmp("online", bcp.cpu->cpu[i].cpu_state) == 0)
			brd_cpus++;
	if (sys_cpus == -1)
		dr_logerr(DRV_FAIL, errno, \
			"sysconf failed (_SC_NPROCESSORS_ONLN)");
	else if (brd_cpus >= sys_cpus)
		dr_logerr(DRV_FAIL, 0, "detaching board would leave no " \
				"online CPUs.");

	/*
	 * Done, do some clean up
	 */
	free_cpu_config(bcp.cpu);
	bcp.cpu = NULL;
}

/*
 * -----------------------------------------------------------------------
 * dr_drain_board_resources
 * -----------------------------------------------------------------------
 *
 * Implementation layer of the drain_board_resources() RPC routine.
 *
 * Input:	board		(system board to drain)
 *
 * Description:	This routine notifies AP that the board is draining, and it
 *		issues a DR_CMD_RELEASE ioctl (because "drain" is now called
 *		"release").  Actually, it notifies AP as its last step because
 *		we wouldn't want AP thinking a "release" was in effect if the
 *		DR_CMD_RELEASE ioctl failed.
 *
 * Output: 	none
 */
void
dr_drain_board_resources(int board)
{
	sfdr_ioctl_arg_t	ioctl_arg;

	/* Log entry to this routine */
	if (verbose)
		dr_loginfo("dr_drain_board_resources: hold board\n");

	/* Issue the ioctl, and check for errors */
	memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_RELEASE, \
			(void *)&ioctl_arg, DR_IOCTL_DEVHOLD)) {

		/* Log the errors */
		dr_report_errors(&ioctl_arg, errno, "drain: ioctl failed, " \
			"error draining resources");

		/* To prevent board inconsistencies, CANCEL the RELEASE */
		memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
		dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_CANCEL, \
			(void *)&ioctl_arg, DR_IOCTL_DEVRESUME);

		/* Also re-configure the board, using the CONFIGURE ioctl */
		memset((caddr_t)(&ioctl_arg), 0, sizeof (sfdr_ioctl_arg_t));
		ioctl_arg.i_flags = SFDR_FLAG_AUTO_CPU;
		dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_CONFIGURE, \
			(void *)&ioctl_arg, DR_IOCTL_DEVRESUME);

		/* Don't process this command any more... just return */
		return;
	}

	/*
	 * Save the time which the drain started.  Used in the
	 * memory drain display (returned to user in dr_config_cost.c).
	 */
	(void) time(&drain_start_time);

	/*
	 * Notify AP daemon that controllers are in a drain state.
	 * We can notify AP after the drain has started since AP is
	 * not adversly affected by the drain operation.  Drain just
	 * lets AP know in advance which controllers may be detached
	 * soon and allows AP to not newly activate these departing
	 * controllers.
	 */
	dr_ap_notify(board, DR_DRAIN);

	/* We've successfully entered the DRAIN state */
	TEST_CRASH_DAEMON(DR_CRASH_DRAIN);
}


/*
 * -----------------------------------------------------------------------
 * dr_detach_board
 * -----------------------------------------------------------------------
 *
 * Implementation layer of the detach_board () RPC routine.  Also, this routine
 * is called from the implementation of dr_get_board_state() anytime an OCCUPIED
 * state is paired up with a CONNECTED previous state.  Such an occurence is
 * indicative of a daemon crash in the middle of a dr_detach_board() operation.
 *
 * Input:	board_slot	(system board to "detach" (disconnect))
 *		force_flag	(whether or not to force the OS quiesce)
 *
 * Description:	Sometimes this routine is called simply because a MIB update
 *		is pending.  This protects against inconsistencies in the
 *		MIB mappings when the SSP application crashes during a complete
 *		detach operation, and didn't get copy_rename info integrated.
 *		This protects against inconsistencies in the MIB mappings.
 *		A check is made to see if that's why a complete_detach was
 *		initiated, and if so all this routine does is put copy-rename
 *		details in the error structure and return.
 *
 *		If this isn't just a copy-rename detection thing, then
 *		the configuration of the board prior to a detach is saved,
 *		so that it's detachment from the system can be documented in
 *		the utmpx/wtmpx entries.  Then the I/O devices and AP are
 *		readied for the DR_CMD_UNCONFIGURE ioctl which will unload
 *		device drivers from this board's devices.  After that ioctl the
 *		DR_CMD_DISCONNECT ioctl is used to remove the board's devices'
 *		OBP nodes from the OS, fully detaching the board.  Any copy
 *		rename in the DR_CMD_UNCONFIGURE ioctl is detected and flagged.
 *
 * Results:	Hopefully, the entire board will be removed and any pending
 *		MIB map changes will be flagged as such.
 */
/*ARGSUSED*/
void
dr_detach_board(brd_detach_t *bdp, detach_errp_t dep)
{
	sfdr_ioctl_arg_t	ioctl_arg;
	attached_board_info_t	bcp;
	sfdr_stat_t		*status;
	sfdr_state_t		state;
	sfdr_state_t		pstate;
	brd_info_t		bip;
	int			board = bdp->board_slot;
	int			iocerr;
	int			i;

#ifdef DR_TEST_VECTOR
	/*
	 * Set the test vector each time we try the device detach operation.
	 */
	dr_test_vector_set(DR_DETACH_IP);
#endif DR_TEST_VECTOR

	/*
	 * Acquire the current and previous states for this board
	 */
	status = get_dr_status(board);
	if (status == NULL) {
		state = SFDR_STATE_FATAL;
		pstate = SFDR_STATE_FATAL;
	} else {
		state = status->s_bstate;
		pstate = status->s_pbstate;
		free(status);
		status = NULL;
	}

	/* If a MIB update's pending and it's already detached, this is easy */
	if (dr_mib_update_pending == board && state == SFDR_STATE_EMPTY) {
		detect_copy_rename(board, dep);
		return;
	}

	/*
	 * Query the CPU configuration info for this board by:
	 *	1) initializing "bip" to get this board's cpu information
	 *	2) calling dr_get_attached_board_info() to get the info
	 *
	 * This information will be used later to update utmpx/wtmpx
	 */

	/* Step 1 */
	bip.board_slot = board;
	bip.flag = (BRD_CPU);

	/* Step 2 */
	dr_get_attached_board_info(&bip, &bcp);
	if (bcp.cpu == NULL) {
		dr_logerr(DRV_FAIL, 0, "couldn't query cpu configuration");
		return;
	}

	/*
	 * If we haven't yet fully detached this board, see how far we got
	 * and do any missed steps.
	 */
	if (state != SFDR_STATE_EMPTY) {

		/* The board could be in error */
		if (state == SFDR_STATE_FATAL)
			return;

		/*
		 * If we didn't get to the UNCONFIGURED state, then we must
		 * be in the UNREFERENCED state.  We must UNCONFIGURE the board.
		 *
		 * This code is executed when a drained board is being
		 * complete_detached.
		 */
		if (state != SFDR_STATE_UNCONFIGURED) {

			/* Check our "must be UNREFERENCED state" assumption */
			if (state != SFDR_STATE_UNREFERENCED && \
					(state != SFDR_STATE_PARTIAL || \
					pstate != SFDR_STATE_UNREFERENCED)) {
				dr_logerr(DRV_FAIL, EPROTO, \
					"detach_board: invalid board state.");
				return;
			}

			/* notify AP that controllers are being detached */
			dr_ap_notify(board, DR_DETACH_IP);

			/*
			 * Do whatever we can to ready the IO subsystem for
			 * detach operation to be done.  This operation is
			 * done after notifying AP since it may bring down
			 * network connections and we'd like to inform AP
			 * about this before any destructive operations happen.
			 *
			 * If the IO is not ready for detaching, just return
			 * with no state changes.  This is the same as if
			 * the device_detach ioctl failed and the user can try
			 * again or abort.
			 */
			if (detach_io_ready(board) == DRV_FAIL) {
				/* reset the ap DRAIN flags */
				dr_ap_notify(board, DR_DRAIN);
				return;
			}

			/* Start out with a clear ioctl argument */
			memset((caddr_t)&ioctl_arg, 0, \
				sizeof (sfdr_ioctl_arg_t));

			/* Tell the driver to automatically offline CPUs */
			ioctl_arg.i_flags = SFDR_FLAG_AUTO_CPU;

			/* Need to supply the force quiesce flag? */
			if (bdp->force_flag)
				ioctl_arg.i_flags |= SFDR_FLAG_FORCE;

			/*
			 * If there's going to be a copy-rename, buy some
			 * insurance.
			 */
			pre_detect_copy_rename(board);

			/* Issue the ioctl to unconfigure the board */
			if (verbose)
				dr_loginfo("detach_board: issuing the " \
					"UNCONFIGURE ioctl()");
			iocerr = dr_issue_ioctl(DR_IOCTARG_BRD, board, \
				DR_CMD_UNCONFIGURE, (void *)&ioctl_arg, \
				DR_IOCTL_DEVDETACH);

			/* Process any errors */
			if (iocerr != DRV_SUCCESS) {

				if (ioctl_arg.i_psm.ierr_num
				    == SFDR_ERR_UNSAFE) {
					/* Provide additional device info */
					dr_report_errors(&ioctl_arg, errno, \
						"detach_board: UNCONFIGURE " \
						"ioctl failed. See host " \
						"syslog.");
					log_unsafe_devs();
				} else {
					/* Log the error */
					dr_report_errors(&ioctl_arg, errno, \
						"detach_board: UNCONFIGURE " \
						"ioctl failed");
				}

				/* Cancel that insurance policy */
				(void) unlink(DR_PENDING_FILE);

				/* Bail out */
				return;
			}

			/* Check for/report any copy-rename that occurred */
			detect_copy_rename(board, dep);
		}

		/* Verify the board state is appropriate for disconnect */
		state = get_dr_state(board);
		if (state != SFDR_STATE_CONNECTED && \
			state != SFDR_STATE_UNCONFIGURED) {
			dr_logerr(DRV_FAIL, EPROTO, \
				"detach_board: invalid board state");
			return;
		}

		/*
		 * After unconfiguring the board, it must be disconnected
		 */
		if (verbose)
			dr_loginfo("detach_board: issuing the DISCONNECT " \
					"ioctl()");
		memset((caddr_t)&ioctl_arg, 0, sizeof (sfdr_ioctl_arg_t));
		iocerr = dr_issue_ioctl(DR_IOCTARG_BRD, board, \
				DR_CMD_DISCONNECT, (void *)&ioctl_arg, NULL);

		/* Process any errors */
		if (iocerr != DRV_SUCCESS) {

			/* Log the error */
			dr_report_errors(&ioctl_arg, errno, "detach_board: " \
					"DISCONNECT ioctl failed");
			return;
		}
	}

	/*
	 * This final phase issues no ioctl()'s but completes OS changes
	 * associated with a full detach of a board.
	 */

	/* Tell other programs that the board's been recently changed */
	dr_signal_operation_complete(board);

	/*
	 * The board is completely detached, restart the network
	 * daemons if necessary.
	 */
	dr_restart_net_daemons();

	/*
	 * Restart the RSM 2000 daemons if necessary
	 */
#ifdef	SONOMA
	dr_restart_rsm_daemons();
#endif	/* SONOMA */

	/*
	 * Notify AP daemon controllers are completely detached Note
	 * that dr_ap_notify for DR_OS_DETACHED _must_ always be
	 * proceeded by a call for DR_DETACH_IP.  This is needed since
	 * the DR_DETACH_IP call saves the on board controller info
	 * for the DR_OS_DETACHED call.  Since once we've detached the
	 * board, the dev-info nodes are gone and we have no idea what
	 * sorts of devices were on the board.
	 */
	dr_ap_notify(board, DR_OS_DETACHED);

	/*
	 * Update the utmpx file for cpu's now detached
	 */
	for (i = 0; i < bcp.cpu->cpu_cnt; i++) {
		update_cpu_info_detach(bcp.cpu->cpu[i].cpuid);
	}

	free_cpu_config(bcp.cpu);
	bcp.cpu = NULL;

	/* Successfully detached.  Let the SSP finish up his work */
	TEST_CRASH_DAEMON(DR_CRASH_OS_DETACHED);
}

/*
 * -----------------------------------------------------------------------
 * dr_finish_detach
 * -----------------------------------------------------------------------
 *
 * Implementation layer of the detach_finished_4() RPC.
 *
 * Description:	When the SSP sends the RPC, it means the SSP has completed
 *		its MIB update.  So we clear the mib_update_pending flag and
 *		re-initialize the daemon's snapshot of the system memory map.
 *		We also have to remove our temporary file.
 *
 * Results:	The dr_daemon acts as if the SSP is currently in sync with its
 *		board-to-memory_address mappings.  The daemon's map and its
 *		update_pending flag are reset as side effects of this function.
 */
void
dr_finish_detach(void)
{
	/* Refresh our system address mappings */
	dr_init_sysmap();

	/* Clear the "update pending" flag */
	dr_mib_update_pending = -1;

	/* Remove the pending file */
	(void) unlink(DR_PENDING_FILE);
}

/*
 * -----------------------------------------------------------------------
 * detach_io_ready
 * -----------------------------------------------------------------------
 *
 * Currently, almost all preparation for detaching the board is the
 * responsibility of the user.  He must unmount any file systems which
 * are mounted, reconfigure swap space,  kill off any processes using
 * the board devices, etc.  Hopefully, the device info returned by RPC
 * GET_BOARD_INFO gives him enough help finding where the devices are
 * being used.
 *
 * We will stop the RSM 2000 rdaemon's if a suspension is necessary,
 * and we will determine which network devices are in use and
 * then shut down the connection for them.
 */
static int
detach_io_ready(int board)
{

#ifdef	SONOMA
	if (dr_stop_rsm_daemons(board) == DRV_FAIL) {
		dr_logerr(DRV_FAIL, errno,
			"detach_io_ready: unable to stop RSM 2000 daemons");
		return (DRV_FAIL);
	}
#endif	/* SONOMA */

	/*
	 * This can fail if we're try to unplumb a vital network resource.
	 * Cannot proceed with the detach.
	 */
	if (dr_unplumb_network(board)) {
		return (DRV_FAIL);
	}

	return (DRV_SUCCESS);
}

/*
 * -----------------------------------------------------------------------
 * dr_abort_detach_board
 * -----------------------------------------------------------------------
 *
 * Implementation layer of the abort_detach_board () RPC routine
 *
 * Input:	board		(system board to abort-detach)
 *
 * Description:	This routine aborts a detachment of a board.  The board will
 *		be in the RELEASE state at this point, due to the policy that
 *		is instilled at the RPC layer.  Issuing a DR_CMD_CANCEL ioctl
 *		from this state will stop the release (or "drain") of a board
 *		and reconfigure it, and then the network and sonoma daemons
 *		as well as the AP library need to be notified of the abort
 *		operation so they can go back to using the devices from this
 *		board.
 *
 *		This routine tries to issue a DR_CMD_CANCEL, but if that ioctl
 *		fails it might not truly be a fault.  That is, it might just be
 *		that the release operation completed before issuing the ioctl()
 *		and that the board is actually in the UNREFERENCED state.  In
 *		this case, the policy is to detach the board completely, calling
 *		the dr_detach_board() routine (the implementation layer for the
 *		detach_board() RPC routine).
 *
 * Results:	The board could be returned to the PARTIAL or CONFIGURED state
 *		if it was in the process of a RELEASE during the ioctl.  Or, if
 *		the RELEASE finished just before this call, the board could be
 *		moved on to the EMPTY state.  Any errors are reported.
 */
void
dr_abort_detach_board(int board)
{
	sfdr_ioctl_arg_t	ioctl_arg;
	sfdr_state_t		state;
	int			iocerr;

	/*
	 * Log entry into this routine
	 */
	if (verbose)
		dr_loginfo("resuming board %d devices\n", board);

#ifdef DR_TEST_VECTOR
	/*
	 * Make sure that the abort DR operations will not fail artifically.
	 */
	dr_test_vector_clear();
#endif DR_TEST_VECTOR

	/*
	 * Issue the DR_CMD_CANCEL ioctl to cancel the release, and process
	 * errors.
	 */
	memset((caddr_t)&ioctl_arg, 0, sizeof (sfdr_ioctl_arg_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_CANCEL, \
			(void *)&ioctl_arg, DR_IOCTL_DEVRESUME)) {

		/* Save errno in case state/status queries change it */
		iocerr = errno;

		if (iocerr == EIO) {
			/*
			 * This really isn't a hard error in that the board is
			 * unattached, but we want this message displayed to the
			 * user so that he/she is aware that not all devices
			 * were re-attached.  The application keys off of the
			 * board state to determine if the re-attach was
			 * successful or not, and would fail to report this
			 * "error".
			 */
			dr_logerr(DRV_FAIL, iocerr, "Some devices " \
				"not re-attached. Examine the host " \
				"syslog for details");
		} else {
			state = get_dr_state(board);

			/*
			 * If the board state is PARTIAL or CONFIGURED, then the
			 * abort-detach operation already succeeded and the SSP
			 * didn't realize it did (perhaps because the daemon
			 * crashed).
			 */
			if (state == SFDR_STATE_PARTIAL ||
					state == SFDR_STATE_CONFIGURED) {
				dr_report_errors(&ioctl_arg, iocerr, \
					"abort_detach: CANCEL ioctl failed");
			}

			/*
			 * If the board state is UNREFERENCED, then our
			 * DR_CMD_CANCEL was too late and the release already
			 * finished.  The correct behavior in this situation is
			 * to complete the detach of the board.
			 */
			else if (state == SFDR_STATE_UNREFERENCED) {
				dr_logerr(DRV_FAIL, 0, "abort_detach: board " \
					"already drained.");
				cant_abort_complete_instead(board);
			}

			/* Any other error should be logged */
			else {
				dr_report_errors(&ioctl_arg, iocerr, \
					"abort_detach: CANCEL ioctl failed");
			}

			/* We return for all of these last cases */
			return;
		}
	}

	/*
	 * Issue the DR_CMD_CONFIGURE ioctl to completely re-attach the board,
	 * and process errors.
	 */
	memset((caddr_t)&ioctl_arg, 0, sizeof (sfdr_ioctl_arg_t));
	ioctl_arg.i_flags = SFDR_FLAG_AUTO_CPU;
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_CONFIGURE, \
			(void *)&ioctl_arg, DR_IOCTL_DEVRESUME)) {

		/* Save the ioctl()'s errno, in case get_dr_state changes it */
		iocerr = errno;

		/*
		 * The PARTIAL board state indicates that some devices weren't
		 * configured.  Notify the user.  This isn't fatal.
		 */
		if (get_dr_state(board) == SFDR_STATE_PARTIAL)
			dr_logerr(DRV_FAIL, iocerr, "Some devices not " \
				"re-attached. Examine the host syslog for " \
				"details");

		/* Anything other than a partial attachment is a fatal error */
		else {
			dr_report_errors(&ioctl_arg, iocerr, \
				"abort_detach: CONFIGURE ioctl failed");
			return;
		}
	}

	/*
	 * The detach is completely aborted, restart the network
	 * daemons if necessary.
	 */
	dr_restart_net_daemons();

	/*
	 * Restart the RSM 2000 daemons if necessary
	 */
#ifdef	SONOMA
	dr_restart_rsm_daemons();
#endif	/* SONOMA */

	/* Notify AP daemon that controllers are resumed. */
	dr_ap_notify(board, DR_IN_USE);
}

/*
 * -----------------------------------------------------------------------
 * cant_abort_complete_instead
 * -----------------------------------------------------------------------
 *
 * Either we've already detached the board or the error
 * returned indicates the detach has already happened.
 *
 * Can't abort the operation, we must go onward.
 */
void
cant_abort_complete_instead(int board)
{
	brd_detach_t 	detach_brd;
	detach_err_t	detach_err;

	detach_brd.board_slot = board;
	detach_brd.force_flag = 0;
	memset(&detach_err, 0, sizeof (detach_err));

	/*
	 * Ignore the hswp error status returned in detach_err since
	 * we have no way of returning this to the user.
	 */
	dr_detach_board(&detach_brd, &detach_err);

	dr_logerr(DRV_FAIL, 0, "Drained board is partially detached. " \
		"Attempting a complete detachment for recovery.");
}

/*
 * -----------------------------------------------------------------------
 * detect_copy_rename
 * -----------------------------------------------------------------------
 *
 * Called by dr_detach_board after an UNCONFIGURE ioctl, to detect and report
 * a copy-rename.
 *
 * Input:	board	(system board just unconfigured)
 * 		dep	(struct to fill in with copy-rename details)
 *
 * Description:	A copy rename is detected by analyzing differences between
 *		snapshots of the system address map taken both before and
 *		after the unconfigure ioctl.  The board that was just
 *		unconfigured combined with the maps allows us to determine
 *		which addresses have been copy-renamed, and to where they
 *		were copied.  Figuring out which address got displaced is
 *		determined as being the one old address that's missing in the
 *		new map.
 *
 * Results:	dep's dr_rename is set to 0 if no copy-rename was found;
 *		dep's dr_rename is set to 1 and dep's target/source board
 *			numbers and addresses are set if a copy-rename occurred.
 */
static void
detect_copy_rename(int board, detach_errp_t dep)
{
	sysbrd_info_t		new_map;
	int			i;
	register int		old_addr;

	/* initialize dr_rename */
	dep->dr_rename = 0;

	/* cache the old mapping */
	old_addr = dr_sysmap.brd_addr[board];

	/* grab the current slot_address-to-board mappings */
	dr_get_sysbrd_info(&new_map);

	/* search for the board's previous mapping having been moved */
	for (i = 0; i < MAX_BOARDS; i++) {

		if (new_map.brd_addr[i] != old_addr)
			continue;

		if (board == i) {
			/*
			 * Same board as before, so obviously
			 * no copy-rename.
			 */
			break;
		}

		dr_mib_update_pending = board;
		dep->dr_rename = 0x1;
		dep->dr_board = board;
		dep->dr_saddr = old_addr;
		dep->dr_tboard = i;
		dep->dr_taddr = dr_sysmap.brd_addr[i];

		if (verbose > 10)
			dr_loginfo("copy-rename found (%d -> %d)", \
				dep->dr_board, dep->dr_tboard);
		break;
	}

	/* Clean up (libdr doesn't even use these values anyways, honestly) */
	if (dep->dr_taddr == (u_int)-1)
		dep->dr_taddr = 0;
	if (dep->dr_saddr == (u_int)-1)
		dep->dr_saddr = 0;

	if (dep->dr_rename == 0 && verbose > 10)
		dr_loginfo("copy-rename *not* found from board %d.", board);
}

/*
 * -----------------------------------------------------------------------
 * dr_get_mib_update_pending
 * -----------------------------------------------------------------------
 *
 * Description:	This is to export the statically-declared dr_mib_update_pending
 *		variable to other modules.
 * Result:	It simply returns a copy of the variable
 */
int
dr_get_mib_update_pending(void)
{
	return (dr_mib_update_pending);
}

/*
 * -----------------------------------------------------------------------
 * dr_init_sysmap
 * -----------------------------------------------------------------------
 *
 * Called during program initialization or after a board is attached, this
 * initializes the system memory map and MIB pending flag.
 *
 * Input:	none
 *
 * Description:	This lets other modules (dr_attach and dr_daemon_main) update
 *		the system memory address map.  This routine keeps the map
 *		current even after board attaches and daemon crashes during
 *		copy-rename operations.  The DR pending file is used if it's
 *		found and valid.  Invalid pending files are deleted.
 *
 * Results:	dr_sysmap will contain the current address map if there was  no
 *		valid pending file, or it will contain the address map prior to
 *		the last copy-rename if a MIB update is still pending.
 *
 *		dr_mib_update_pending will either be initialized, or it will be
 *		set to the board number whose copy-rename has not yet been
 *		acknowledged by the SSP.
 */
void
dr_init_sysmap(void)
{
	if (check_pending_file(DR_PENDING_FILE) == FALSE) {
		dr_get_sysbrd_info(&dr_sysmap);
		dr_mib_update_pending = -1;
	}
}

/*
 * -----------------------------------------------------------------------
 * check_pending_file
 * -----------------------------------------------------------------------
 *
 * Check for a valid DR pending file, and load its values during program
 * initialization.
 *
 * Input:	pending_file	(the name of the file to check)
 *
 *	Given the name of a file, it can be determined if the file is a valid
 * DR pending file.  To be valid, a file must:
 *		1) Exist
 *		2) Be a regular file
 *		3) Be owned by root
 *		4) Have permissions 0600
 *		5) Be large enough to contain our data
 *		6) Have been created since the last boot-up
 *		6) Contain a dr_pending_t with a valid checksum.
 *		8) The board for which the copy-rename occurred must be fully
 *		   detached by the OS.
 *
 * Output:	TRUE if the file is a valid DR pending file, FALSE if not.
 *
 * Results:	Invalid DR pending files are deleted, and valid ones have their
 *		contents loaded into the dr_mib_update_pending flag and
 *		dr_sysmap address map.
 */
static int
check_pending_file(char *pending_file)
{
	struct stat		st_buf;
	struct utmpx		*ut1, *ut2;
	time_t			boottime;
	dr_pending_t		pend_data;
	ulong_t			checksum;
	int			pend_fd;
	sfdr_state_t		state;

	/*
	 * Stat the file and make sure it exists, has the right ownership and
	 * permissions, and is large enough to be valid.
	 */
	if (stat(pending_file, &st_buf) ||
			!S_ISREG(st_buf.st_mode) ||
			st_buf.st_nlink != 1 ||
			st_buf.st_uid != 0 ||
			(st_buf.st_mode & 0777) != 0600 ||
			st_buf.st_size != sizeof (pend_data)) {
		(void) unlink(pending_file);
		if (verbose > 10)
			dr_loginfo("An invalid DR pending file was deleted.");
		return (FALSE);
	}

	/*
	 * Make sure the file is new enough to report a copy-rename during this
	 * boot of the domain.  If we can't determine the boot time, we assume
	 * the file is current.
	 */
	if ((ut1 = getutxent()) == NULL)
		boottime = (time_t)-1;
	else {
		if (ut1->ut_type == BOOT_TIME) {
		boottime = ut1->ut_xtime;
		} else {
			ut1->ut_type = BOOT_TIME;
			if ((ut2 = getutxid(ut1)) == NULL)
				boottime = (time_t)-1;
			else
				boottime = ut2->ut_xtime;
		}
	}
	if (boottime != (time_t)-1 &&
			difftime(st_buf.st_mtime, boottime) <= 0) {
		(void) unlink(pending_file);
		if (verbose > 10)
			dr_loginfo("An invalid DR pending file was deleted.");
		return (FALSE);
	}

	/* Try to load the file's contents */
	pend_fd = open(pending_file, O_RDONLY);
	if (pend_fd == -1 ||
		read(pend_fd, (void *)&pend_data, sizeof (pend_data)) == -1) {
		(void) close(pend_fd);
		(void) unlink(pending_file);
		if (verbose > 10)
			dr_loginfo("An invalid DR pending file was deleted.");
		return (FALSE);
	}
	(void) close(pend_fd);

	/* Test the file's checksum */
	checksum = cksum((void *)&(pend_data.val), sizeof (pend_data.val));
	if (verbose > 10)
		dr_loginfo("Pending file checksum: file: %x, computed: %x.",
			pend_data.checksum, checksum);
	if (pend_data.checksum != checksum) {
		(void) unlink(pending_file);
		if (verbose > 10)
			dr_loginfo("An invalid DR pending file was deleted.");
		return (FALSE);
	}

	/* Test whether or not the indicated board is actually detached */
	state = get_dr_state(pend_data.val.pending_flag);
	if (state != SFDR_STATE_EMPTY &&
			state != SFDR_STATE_OCCUPIED &&
			state != SFDR_STATE_UNCONFIGURED) {
		(void) unlink(pending_file);
		if (verbose > 10)
			dr_loginfo("An invalid DR pending file was deleted.");
		return (FALSE);
	}

	/* The file seems valid, so use its contents */
	dr_mib_update_pending = pend_data.val.pending_flag;
	dr_sysmap = pend_data.val.sysmap;
	if (verbose > 10)
		dr_loginfo("A valid DR pending file was used.");
	return (TRUE);
}

/*
 * -----------------------------------------------------------------------
 * pre_detect_copy_rename
 * -----------------------------------------------------------------------
 *
 * This is called before an UNCONFIGURE ioctl to detect any potential
 * copy-renames that might occur during the ioctl.
 *
 * Input:	board		(the system board being detached)
 *
 * Description:	Before unconfiguring a system board, we look at its memory
 *		information.  If it has any non-relocatable memory, then a
 *		copy-rename will occur.  So we'll need to create a DR pending
 *		file just in case there's some sort of a crash.
 *
 * Results:	A valid DR pending file will exist iff a copy-rename will occur.
 */
static void
pre_detect_copy_rename(int board)
{
	board_mem_config_t	mc;
	int			pend_fd;
	dr_pending_t		pend_data;

	/* Sanity check */
	if (!BOARD_IN_RANGE(board))
		return;

	/* Query the board's memory configuration */
	memset((caddr_t)&mc, 0, sizeof (board_mem_config_t));
	if (get_mem_config(board, &mc) == NULL)
		return;

	/* Free up the mem_detach string within 'mc'; we don't need it here */
	if (mc.dr_mem_detach) {
		free(mc.dr_mem_detach);
		mc.dr_mem_detach = NULL;
	}

	/* If no-reloc pages are found, make a temporary file */
	if (mc.perm_memory) {

		/* Make sure we own the file */
		while ((pend_fd = open(DR_PENDING_FILE,
				O_WRONLY | O_CREAT | O_EXCL,
				(mode_t)0600)) == -1 && errno == EEXIST) {
			if (unlink(DR_PENDING_FILE) == -1 && errno != ENOENT)
				break;
		}
		if (pend_fd == -1)
			return;

		/* Fill in the data for the file */
		pend_data.val.pending_flag = board;
		pend_data.val.sysmap = dr_sysmap;
		pend_data.checksum = cksum((void *)&pend_data.val,
						sizeof (pend_data.val));

		/* Write out the data to the file */
		if (write(pend_fd, (void *)&pend_data,
				sizeof (pend_data)) == -1) {
			(void) close(pend_fd);
			(void) unlink(DR_PENDING_FILE);
			return;
		}

		/* Save out the file */
		(void) close(pend_fd);
		if (verbose > 10)
			dr_loginfo("Created pending file, checksum: %x.",
				pend_data.checksum);
	}
}

/*
 * -----------------------------------------------------------------------
 * cksum
 * -----------------------------------------------------------------------
 *
 * Used to compute a simplistic checksum of a bunch of data.
 *
 * Input:	data		(opaque pointer to the chunk of data)
 *		size		(the size of the data chunk)
 *
 * Description:	An unsigned long integer is allowed to overflow as each byte
 *		of the chunk of data is added to it.  The resulting sum is
 *		returned as the computed checksum.
 *
 * Output:	The computed unsigned long integer checksum for the data.
 */
static ulong_t
cksum(void *data, size_t size)
{
	uchar_t	*byte = (uchar_t *)data;
	ulong_t	sum = 0x0UL;

	while (size--) {
		sum = ((sum & 0x7FFFFFFFUL) << ((ulong_t)(*byte) & 0x5))
			+ (ulong_t)*byte;
		byte++;
	}

	return (sum);
}

/*
 * -----------------------------------------------------------------------
 * log_unsafe_devs
 * -----------------------------------------------------------------------
 *
 * In the case of a failed complete_detach due to unsafe devices,
 * call dr_loginfo to syslog the device names.
 */
static void
log_unsafe_devs(void)
{
	unsafe_dev_t	ud;
	int		i, ndevs;

	dr_unsafe_devices(&ud);

	ndevs = ud.unsafe_devs.unsafe_devs_len;
	for (i = 0; i < ndevs; i++) {
		dr_loginfo("unsafe device: %s\n",
			ud.unsafe_devs.unsafe_devs_val[i]);
	}
	free_unsafe_devs(&ud);
}

/*
 * -----------------------------------------------------------------------
 * free_unsafe_devs
 * -----------------------------------------------------------------------
 *
 * Frees all allocated memory for an unsafe_dev_t structure.
 */
static void
free_unsafe_devs(unsafe_devp_t udp)
{
	int	i;
	name_t	np, *npp;

	if (udp == NULL)
		return;

	if ((npp = udp->unsafe_devs.unsafe_devs_val) != NULL) {
		for (i = 0; i < udp->unsafe_devs.unsafe_devs_len; i++) {
			if ((np = udp->unsafe_devs.unsafe_devs_val[i])
			    != NULL) {
				free(np);
			}
		}
		free(npp);
		udp->unsafe_devs.unsafe_devs_val = NULL;
		udp->unsafe_devs.unsafe_devs_len = 0;
	}
}
