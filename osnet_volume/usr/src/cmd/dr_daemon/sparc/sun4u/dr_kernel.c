/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_kernel.c	1.38	98/09/17 SMI"

/*
 * This file implements the Dynamic Reconfiguration interfaces to the
 * DR driver.
 */

#include "dr_kernel.h"

#ifdef NODRKERN

/* test stub ioctl for DR ioctls */
#define	ioctl(fd, code, argp) dr_ioctl(fd, code, argp)
static int dr_ioctl(int fd, int code, void *arg);

#ifdef DR_TEST_CONFIG
static int nodrkern_ioctl_fail;
#endif DR_TEST_CONFIG

#endif NODRKERN

/* Forward references */
static int dr_driver_open(int target, int board);

/*
 * --------------------------------------------------------------------
 * dr_driver_open
 * --------------------------------------------------------------------
 *
 * Open a DR driver
 *
 * Input:	target (the whole board, just its memory, or its cpu's)
 *		board (the board number to open)
 *
 * Description:	The DR devices now have the following names:
 *
 *			/dev/pseudo/dr@N:slotM
 *			/dev/pseudo/dr@N:slotM.allcpus
 *			/dev/pseudo/dr@N:slotM.allmem
 *
 *		For Starfire, N is always 0 (this indicates which "box", or
 *		physical machine, is being referred to).  The M value is the
 *		"board" argument (which system board number).  The "target"
 *		argument selects either the whole board (DR_IOCTL_TARGET_BRD),
 *		just its cpu's (DR_IOCTL_TARGET_CPUS), or just its memory
 *		(DR_IOCTL_TARGET_MEM).  This routine figures out which device
 *		is to be opened, and opens it up.  It returns a file descriptor
 *		to the opened device driver entry point.
 *
 *		This routine won't trust its callers, and it'll verify that its
 *		arguments are in range before attempting to open a device.
 *
 * Output:	function return value is a file descriptor or -1 for errors.
 *		errno is set accordingly when there is an error.
 */
static int
dr_driver_open(int target, int board)
{
	char	devname[MAXLEN];

	/* Validate arguments */
	if (!BOARD_IN_RANGE(board) || !TARGET_IN_RANGE(target)) {
		errno = EINVAL;
		return (-1);
	}

	/* Construct the device name to open */
	switch (target) {
		case DR_IOCTARG_MEM:
			(void) sprintf(devname, "%s%d.allmem", \
				DR_DRV_BASE, board);
			break;

		case DR_IOCTARG_CPUS:
			(void) sprintf(devname, "%s%d.allcpu", \
				DR_DRV_BASE, board);
			break;

		case DR_IOCTARG_IO:
			(void) sprintf(devname, "%s%d.allio", \
				DR_DRV_BASE, board);
			break;

		case DR_IOCTARG_BRD:
		default:
			(void) sprintf(devname, "%s%d", DR_DRV_BASE, board);
			break;
	}

	/* Open the device; open() will set errno and return -1 if failure */
	return (open(devname, O_RDONLY));

}

/*
 * --------------------------------------------------------------------
 * dr_issue_ioctl
 * --------------------------------------------------------------------
 *
 * Issue an IOCTL to one of the DR driver entry points.
 *
 * Input: 	targ	target (whole board, just cpus, or just memory)
 *    		brd	which system board is being controlled
 *    		cmd	ioctl command being issued
 *    		argp	address of ioctl argument structure
 *    		test_mask mask used if DR_TEST_CONFIG conditionally compiled
 *
 * Description:	The driver for the targetted board component is opened.  (And
 *		since the dr_driver_open call verifies the target/board,
 *		arguments, this routine relies on the "open" call failing for
 *		its argument validation.)  The cmd/argp are then issued (via
 *		ioctl()) to the driver, and the driver is closed.
 *
 *		If the given test_mask flag is set, then a dummied response is
 *		returned.
 *
 * Output:	On return, this function returns DRV_FAIL for error or
 *		DRV_SUCCESS for success.  errno is set to the ioctl()'s return
 *		value or to the ioctl()'s errno in case of failure.  errno is
 *		undefined in case of success.
 */
#ifndef DR_TEST_CONFIG
/* ARGSUSED */
#endif
int
dr_issue_ioctl(int targ, int brd, int cmd, void *argp, int test_mask)
{
	int			fd;
	int			iocerr;
	int			temperrno;
	sfdr_stat_t		*status;
	sfdr_ioctl_arg_t	error_ioctl;

	if (verbose > 10)
		dr_loginfo("dr_issue_ioctl: board=%d, target=%d, command=%d", \
			brd, targ, cmd);

	/*
	 * Sanity check
	 */
	if (argp == NULL)
		return (EINVAL);

#ifndef NODRKERN
	if ((fd = dr_driver_open(targ, brd)) < 0)
		/* error from the DR driver open */
		return (DRV_FAIL);
#else
	/* make purify happy */
	fd = 1;
#endif NODRKERN

#ifdef DR_TEST_CONFIG

#ifdef NODRKERN
	/* We want to call dr_ioctl to do the failed errno's for us */
	if (dr_testioctl & test_mask) {
		nodrkern_ioctl_fail = 1;
	} else {
		nodrkern_ioctl_fail = 0;
	}
#else NODRKERN
	/* don't call the ioctl, just make this routine return an error */
	if (dr_testioctl & test_mask) {
		iocerr = -1;
		errno = dr_testerrno;
	} else
#endif NODRKERN

#endif DR_TEST_CONFIG
		iocerr = ioctl(fd, cmd, argp);

	temperrno = errno;
	if (close(fd) == -1)
		dr_logerr(DRV_FAIL, errno, "dr_issue_ioctl: failed closing " \
			"driver.");
	errno = temperrno;

	/*
	 * When a STATUS ioctl is issued, it could possibly have an error
	 * from a previous RELEASE operation embedded within it.  We better
	 * check for that.
	 */
	if (cmd == DR_CMD_STATUS) {
		if (argp != (void *)NULL) {
			status = (sfdr_stat_t *)argp;
			if (status->s_lastop.l_pimerr.ierr_num ||
					status->s_lastop.l_psmerr.ierr_num) {
				error_ioctl.i_pim = status->s_lastop.l_pimerr;
				error_ioctl.i_psm = status->s_lastop.l_psmerr;
				dr_report_errors(&error_ioctl, 0, \
					"drain: ioctl failed");
			}
		}
	}

	/* If iocerr is -1, then the ioctl() failed and set errno for us */
	if (iocerr == -1) {
		return (DRV_FAIL);
	}

	/* Else we need to look at iocerr and see if the driver failed */
	if (iocerr > 0) {
		errno = iocerr;
		return (DRV_FAIL);
	}

	/* If neither ioctl() nor the driver failed, we succeeded */
	return (DRV_SUCCESS);
}

/*
 * --------------------------------------------------------------------
 * get_dr_state
 * --------------------------------------------------------------------
 *
 * Query a system board's state
 *
 * Input: 	board	(system board to query)
 *
 * Description:	A DR_CMD_STATUS ioctl is issued to the system board and the
 *		state portion is plucked out of the status.
 *
 * Output:	The current state of the board queried.
 */
sfdr_state_t
get_dr_state(int board)
{
	sfdr_stat_t 		status;

	if (verbose >= 10)
		dr_loginfo("get_dr_state: querying board %d", board);

	/*
	 * Be careful and double-check the board argument
	 */
	if (!BOARD_IN_RANGE(board))
		return (SFDR_STATE_FATAL);

	/*
	 * Issue a DR_CMD_STATUS ioctl to the system board to retrieve its
	 * status
	 */
	memset((caddr_t)(&status), 0, sizeof (sfdr_stat_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
		(void *)&status, DR_IOCTL_GETSTATE)) {

		dr_logerr(DRV_FAIL, errno, "get_dr_state: ioctl failed");
		return (SFDR_STATE_FATAL);
	}

	/*
	 * If there was no failure/return above, return the state portion of the
	 * status structure
	 */
	return (status.s_bstate);
}

/*
 * --------------------------------------------------------------------
 * get_dr_status
 * --------------------------------------------------------------------
 *
 * Query a system board's status
 *
 * Input: 	board	(system board to query)
 *
 * Description:	A DR_CMD_STATUS ioctl is issued to the system board and the
 *		whole status is struct is returned.
 *
 * Output:	The current status of the board queried.
 */
sfdr_stat_t *
get_dr_status(int board)
{
	sfdr_stat_t 		*status;

	/*
	 * Allocate and clear the status
	 */
	status = (sfdr_stat_t *)calloc(1, sizeof (sfdr_stat_t));
	if (status == NULL)
		return (NULL);

	/*
	 * Be careful and double-check the board argument
	 */
	if (!BOARD_IN_RANGE(board)) {
		status->s_board = board;
		status->s_bstate = SFDR_STATE_FATAL;
		status->s_pbstate = SFDR_STATE_FATAL;
		status->s_nstat = 0;
		return (status);
	}

	/*
	 * Issue a DR_CMD_STATUS ioctl to the system board to retrieve its
	 * status
	 */
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
		(void *)status, DR_IOCTL_GETSTATE)) {

		dr_logerr(DRV_FAIL, errno, "get_dr_status: ioctl failed");
	}

	/*
	 * If there was no failure/return above, return the state portion of the
	 * status structure
	 */
	return (status);
}

/*
 * ---------------------------------------------------------------------------
 * dr_signal_operation_complete
 * ---------------------------------------------------------------------------
 *
 * Input:	board (system board number for which the operation just ended)
 *
 * Description:	As a way to tell other programs about significant changes
 *		to system boards (such as them becoming fully attached or
 *		detached), we update the modification times of the board's
 *		DR driver entry point.
 *
 *		This is how SyMon finds out a board's OBP nodes are different.
 */
void
dr_signal_operation_complete(int board)
{
	int	ret;
	char	devname[MAXLEN];

	(void) sprintf(devname, "%s%d", DR_DRV_BASE, board);
	ret = utime(devname, (const struct utimbuf *)NULL);

	if (ret < 0)
		dr_loginfo("WARNING: Failed to update board %d's modification" \
			" time [non-fatal].", board);
}

#ifdef DR_TEST_VECTOR
/*
 * Variables and routines associated with testing failed DR operations
 *
 * These routines control the setting and clearing of the DR kernel
 * test vector.  This test vector can fail DR attach and detach operations
 * as follows:
 *	- after successful attach/detach of all IO devices
 *	- after successful attach/detach of all memory devices
 *	- after successful attach/detach of a cpu.  Cpu is chosen pseudo
 *		randomly with the possibility that a cpu may _not_ be chosen
 *		at all.
 */
#include <time.h>

int dr_fail_op = 0;		/* set via the -f flag.  0 == disabled */
char dr_test_vector_msg[30];  /* msg for decode_ioctl_error to append */

/*
 * dr_test_vector_clear
 *
 * Clear the kernel test vector so that we do not artifically fail DR
 * operations.  Note that the dr fd is closed upon return from the RPC
 * call.
 */
void
dr_test_vector_clear(void)
{
	struct dr_test_vector	dtv;
	int fd;

	if (dr_fail_op == 0)
		return;

	dr_test_vector_msg[0] = 0;

	if ((fd = dr_driver_open()) < 0)
		return;

	dtv.x_vec = 0;
	if (ioctl(fd, DR_TESTVECTOR_SET, (caddr_t)&dtv) != 0) {
		dr_loginfo("FAILED: ioctl(DR_TESTVECTOR_SET) errno = %d",
			    errno);
	}
}

/*
 * dr_test_vector_set
 *
 * Choose which (if any) DR operation should fail.  Note that the dr
 * fd is closed upon return from the RPC call.
 *
 * Input: op is either DR_COMPLETE_ATTACH_IP or DR_DETACH_IP depending
 *	upon whether a detach or attach operation is about to be executed.
 */
void
dr_test_vector_set(dr_board_state_t op)
{
	static int dr_op_count = 0;
	int vector;
	time_t tm;
	struct dr_test_vector	dtv;
	int fd;

	if (dr_fail_op == 0)
		return;

	dr_op_count++;

	if ((dr_op_count % dr_fail_op) != 0) {
		dr_test_vector_clear();
		return;
	}

	vector = 0;

	switch (op) {
	case DR_COMPLETE_ATTACH_IP:
		vector |= DR_TESTVEC_ATTACH;
		(void) strcpy(dr_test_vector_msg, " ATTACH");
		break;
	case DR_DETACH_IP:
		vector |= DR_TESTVEC_DETACH;
		(void) strcpy(dr_test_vector_msg, " DETACH");
		break;
	default:
		dr_loginfo(
	    "WARNING: internal error.  Invalid arg to dr_test_vector_set().");
		return;
	}

	/*
	 * pick which sort of device to fail in a semi-random fashion.
	 */
	(void) time(&tm);

	switch ((unsigned int)tm % 3) {
	case 0:
		vector |= DR_TESTVEC_IO;
		(void) strcat(dr_test_vector_msg, " TESTVEC IO");
		break;
	case 1:
		vector |= DR_TESTVEC_CPU;
		(void) strcat(dr_test_vector_msg, " TESTVEC CPU");
		break;
	case 2:
		vector |= DR_TESTVEC_MEM;
		(void) strcat(dr_test_vector_msg, " TESTVEC MEM");
		break;
	}

	if ((fd = dr_driver_open()) < 0)
		return;

	dtv.x_vec = vector;
	if (ioctl(fd, DR_TESTVECTOR_SET, (caddr_t)&dtv) != 0) {
		dr_loginfo("FAILED: ioctl(DR_TESTVECTOR_SET) errno = %d",
			    errno);
	}
}

#endif DR_TEST_VECTOR

#if	0
/*
 * decode_ioctl_error
 *
 * report the cause of the error by decoding the returned dr_ioctl_arg_t
 * structure.
 *
 * Input: driop		returned ioctl arg struct
 *	  iocerr	errno returned from the ioctl call
 *	  intro 	first part of the error string
 */
void
decode_ioctl_error(dr_ioctl_arg_t *driop, int iocerr, char *intro)
{
	char msg[MAXMSGLEN];

	/*
	 * If we know what device we failed at, print the
	 * device string following the intro.
	 */
	if (driop->dr_fail_dev_instance >= 0) {
		sprintf(msg, "%s (%s%d):", intro, driop->dr_fail_dev_name,
			driop->dr_fail_dev_instance);
	} else {
		sprintf(msg, "%s:", intro);
	}

#ifdef DR_TEST_VECTOR
	/*
	 * append type of test vector error so we can determine
	 * which ops are artifical failures and which are not
	 */
	if (dr_test_vector_msg[0] != 0)
		strncat(msg, dr_test_vector_msg, MAXMSGLEN);
#endif DR_TEST_VECTOR

	if (driop->dr_reason) {

		/* If we've got a reason, don't care what errno is */
		iocerr = 0;

		if (driop->dr_reason & DR_REASON_OP_NOT_SUPPORTED) {
			strncat(msg, " DR unsupported by board device.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_DEVICE_BUSY) {
			strncat(msg, " device busy.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_MEM_CONFIG) {
			strncat(msg,
				" Memory configuration prevents board detach.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_MEM_ILEAVE) {
			strncat(msg, " Unsupported memory interleave.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_MEM_PERM) {
			strncat(msg,
" Target memory currently unavailable, please retry.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_HOTSWAP) {
			strncat(msg, " OS Quiesce failed.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_MINMEM) {
			strncat(msg,
" Memory reduction would violate system requirements.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_MINMEM_RETRY) {
			strncat(msg,
" Estimated memory reduction would violate system requirements, please retry.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_KERNEL_NOT_CAGED) {
#ifdef	_XFIRE
			strncat(msg,
			    " Memory detach is not enabled.  "
			    "Must reboot system with dr-max-mem non-zero.",
			    MAXMSGLEN);
#endif
		}
		if (driop->dr_reason & DR_REASON_PROFILING) {
			strncat(msg,
" Kernel profiling enabled. Deactivate with 'kgmon -d'.",
				MAXMSGLEN);
		}
		if (driop->dr_reason & DR_REASON_PROCSET_CONFIG) {
			strncat(msg, " Processor set configuration"
			    " will not allow detach. See psrset(1M).",
			    MAXMSGLEN);
	} else {
		strncat(msg, " ioctl failed.", MAXMSGLEN);
	}

	dr_logerr(DRV_FAIL, iocerr, msg);
}
#endif	0

/*
 * From here to the end of file we have dummy routines to override system calls
 * which need to be dummied up for unit testing on workstations or without
 * the 'real kernel'.  We have a number of different testing
 * configurations as follows:
 *
 * NO_DRAGON - defined if we're testing on a Sparc work station.
 *	in dr_subr.h, this also defines NODRKERN.
 *
 * NODRKERN - running on a superdragon without the required DR mods.
 *
 * NO_SU - not running as superuser.
 *
 * TEST_SVC - test driver and daemon linked as one program.  RPC calls
 *	are not used.  Instead, the *_2 routines are called directly
 * 	by the driver.
 */

#ifdef NOTDEF
We no longer need a dummy syslog routine when testing.
Instead, when compiling TEST_SVC, also compile with RPC_SVC_FG so the
code in dr_subr.c does not call syslog.

/*
 * syslog
 *
 * When we're a single process, don't write messages to the syslog.
 * Instead, define our own verssion of syslog which dumps the output
 * to the test_log_file.
 */

#include <syslog.h>
extern FILE *test_log_file;

void
syslog(int code, const char *format, ...)
{
	va_list	varg_ptr;
	char	buf[256];
	char *code_str;

	if (code == LOG_ERR)
		code_str = "err";
	else
		code_str = "inf";

	va_start(varg_ptr, format);
	vsprintf(buf, format, varg_ptr);
	va_end(varg_ptr);

	if (buf[strlen(buf)-1] != '\n')
		fprintf(test_log_file, "dr_daemon(%s): %s\n", code_str, buf);
	else
		fprintf(test_log_file, "dr_daemon(%s): %s", code_str, buf);
}
#endif NOTDEF

#ifdef NO_DRAGON
/*
 * processor_info
 *
 * On a workstation, there is only one processor so we must dummy this
 * routine up for multiple processors.
 *
 * This routine is used to determine if a processor is configured into
 * the system (dr_attach.c and board_in_use() above (interested in success
 * or failure of the call).  Also, in dr_detach.c
 * to determine if a processor is on/off line when moving cpu0.
 *
 * FRV: =0 is good status
 */

/* array to keep track of cpu state. P_BAD means not present */
static int cpu_state[MAX_CPU_PER_SYS] = {
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_OFFLINE,
	P_ONLINE, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_BAD, P_BAD, P_BAD, P_BAD,
	P_ONLINE, P_ONLINE, P_ONLINE, P_ONLINE,	P_OFFLINE, P_BAD, P_BAD, P_BAD,
};

int
processor_info(processorid_t i, processor_info_t *pip)
{
	if (i < 0 || i >= MAX_CPU_PER_SYS) {
		dr_loginfo("BAD cpuid to processor info (%d)\n", i);
		return (-1);
	}

	if (cpu_state[i] == P_BAD)
		/* no cpu */
		return (-1);

	pip->pi_state = cpu_state[i];
	return (0);
}
#endif NO_DRAGON

#ifdef NODRKERN
/*
 * dr_ioctl
 *
 * For this file and this file only, supply a dummy ioctl
 * routine to mimic the DR ioctls.
 *
 * FRV: < 0 is error
 */
static int board_state[MAX_BOARDS]; /* starts out as DR_NULL_STATE */

#include <sys/cpuvar.h>

static int
dr_ioctl(int fd, int code, void *arg)
{
	union {
		dr_ioctl_arg_t	*i;
		obp_probe_t	*p;
		drsafe_t	*s;
		drmemconfig_t	*mc;
		drmemcost_t	*mcost;
		drmemstate_t	*mstate;
		brd_cpu_config_t *cc;
		cpu_cost_info_t	*ci;
		void		*v;
	} a;
	int i;

#ifdef DR_TEST_CONFIG
	if (nodrkern_ioctl_fail) {
		errno = dr_testerrno;
	} else
#endif DR_TEST_CONFIG
		errno = 0;

	a.v = arg;

	switch (code) {

	case DR_SYS_OBP_PROBE:
		if (a.p->cpuid < 0 || a.p->cpuid >= MAX_CPU_PER_SYS) {
			errno = EINVAL;
		}
		break;

	case DR_INIT_ATTACH:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			errno = EINVAL;
		}
		break;

	case DR_COMPLETE_ATTACH:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			errno = EINVAL;
		}
#ifdef NO_DRAGON
		/* mark the processors as P_ONLINE */
		for (i = a.i->dr_board * MAX_CPU_PER_BRD;
		    i < (a.i->dr_board+1) * MAX_CPU_PER_BRD; i++) {
			cpu_state[i] = P_ONLINE;
		}
#endif NO_DRAGON
		break;

	case DR_CHECK_DETACH:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			a.i->dr_fail_dev_instance = -1;
			errno = EINVAL;
		} else if (errno == EIO) {
			/* just plug some reason fields */
			a.i->dr_reason = DR_REASON_OP_NOT_SUPPORTED |
				DR_REASON_MEM_CONFIG;
			strcpy(a.i->dr_fail_dev_name, "sd");
			a.i->dr_fail_dev_instance = 2;
		}
		break;

	case DR_DEVICE_HOLD:
		/* hold is done on a board or mem device so don't check arg */
		break;

	case DR_DEVICE_DETACH:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			a.i->dr_fail_dev_instance = -1;
			errno = EINVAL;
		} else if (errno == EIO) {
			a.i->dr_reason = DR_REASON_DEVICE_BUSY;
			strcpy(a.i->dr_fail_dev_name, "le");
			a.i->dr_fail_dev_instance = 2;
		}
#ifdef NO_DRAGON
		if (errno == 0) {
			/* mark the processors as P_OFFLINE */
			for (i = a.i->dr_board * MAX_CPU_PER_BRD;
			    i < (a.i->dr_board+1) * MAX_CPU_PER_BRD; i++) {
				cpu_state[i] = P_OFFLINE;
			}
		}
#endif NO_DRAGON
		break;

	case DR_DEVICE_RESUME:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			a.i->dr_fail_dev_instance = -1;
			errno = EINVAL;
		}
		break;


	case DR_GET_BOARD_STATE:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			errno = EINVAL;
		}
		if (errno == 0) {
			a.i->dr_bstate = board_state[a.i->dr_board];
		}
		break;

	case DR_SET_BOARD_STATE:
		if (!BOARD_IN_RANGE(a.i->dr_board)) {
			errno = EINVAL;
		}
		if (errno == 0) {
			board_state[a.i->dr_board] = a.i->dr_bstate;
		}
		break;

	case DR_SAFE_DEVICE:
		{
			static int safe_device = 0;

			a.s->is_safe = 1;
			if (safe_device++ == 0) {
				a.s->is_safe = 0;
				a.s->is_referenced = 1;
			} else if (safe_device > 15) {
				safe_device = 0;
			}
		}
		break;

	case DR_CPU_CONFIG:
		a.cc->cpu_cnt = MAX_CPU_PER_BRD;
		for (i = 0; i < MAX_CPU_PER_BRD; i++) {
			a.cc->cpu[i].cpuid = i +
				(a.cc->board_slot * MAX_CPU_PER_BRD);
			a.cc->cpu[i].cpu_obpid = 0x1230 + 1;
			a.cc->cpu[i].cpu_flags = 0;
			a.cc->cpu[i].partition = 0;

		}
		break;

	case DR_CPU_COSTINFO:
		a.ci->num_user_threads_bound = 0;
		if (a.ci->max_pids > 1) {
			a.ci->num_sys_threads_bound = 1;
			a.ci->num_pids = 1;
			a.ci->pid_list[0] = 1234;
		} else
			a.ci->num_sys_threads_bound = 0;
		break;

	case DR_CPU_MOVE_CPU0:
		dr_loginfo("dr_ioctl: code not implemented 0x%x\n", code);
		break;

	case DR_MEM_CONFIG:
		i = a.mc->board_slot;
		memset((char *)a.mc, 0, sizeof (drmemconfig_t));
		a.mc->board_slot = i;
		a.mc->mem_obpid = 123;
		a.mc->mem_devid = 456;
		a.mc->mem_pages = 256;	/* pages */
		a.mc->mem_if = 1;
		a.mc->mem_board_mask = 1 << i;
		a.mc->sys_mem = 512;	/* pages */
		a.mc->dr_min_mem = 128;
		a.mc->dr_max_mem = 1024;
		a.mc->dr_mem_detach = 1;
		break;

	case DR_MEM_COSTINFO:
		i = a.mcost->board_slot;
		memset((char *)a.mcost, 0, sizeof (drmemcost_t));
		a.mcost->board_slot = i;
		a.mcost->pshrink = 20;
		a.mcost->pdetach = 100;
		a.mcost->actualcost = 1;
		break;

	case DR_MEM_STATE:
		i = a.mstate->board_slot;
		memset((char *)a.mstate, 0, sizeof (drmemstate_t));
		a.mstate->board_slot = i;
		a.mstate->pflush = 100;
		a.mstate->nflushed = 40;
		break;

	default:
		dr_loginfo("dr_ioctl: bad code 0x%0x\n", code);
		errno = EINVAL;
		break;
	}

	if (errno != 0)
		return (-1);
	else
		return (0);
}
#endif NODRKERN
