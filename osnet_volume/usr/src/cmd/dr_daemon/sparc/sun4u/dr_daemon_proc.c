/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_daemon_proc.c	1.61	99/09/19 SMI"

/*
 * Functions describing DR Daemon interface.
 */

#include <stdio.h>
#include <signal.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/pset.h>

#include "dr_subr.h"

/*
 * STUB_TEST is defined when making a dummy client to test the
 * NODAEMON code in this routine (testclnt in the daemon Makefile).
 * The other TEST_CLNT usage is libdr which can use this file to
 * dummy up a daemon for testing.
 */
#if defined(TEST_CLNT) && !defined(STUB_TEST)
#define	DRLIB
#endif

#ifdef DRLIB
#include "dr_lib.h" 	/* So we can use dr_error()  */
#include "dr_local.h" 	/* So we can use dr_debug  */
#include <sd_error.h>
#endif DRLIB

/*
 * verbose controls how much of the daemon's dr_loginfo messages
 * are displayed.
 *
 * A value of 0 will inhibit almost all info messages.  Basically,
 * only errors will be output to the syslog.
 *
 * A value of 1 will display messages prior to
 * each significant operation such as ioctl calls or exec's of
 * programs.
 *
 * A value of 10 or greater will display info on entry to
 * and exit from each RPC call.
 *
 * A value greater than 10 will add extra debugging messages displaying things
 * such as which board/target/cmd arguments are sent for each ioctl() or what
 * page size is used to calculate the memory size for a board.
 */
int	verbose = 10;

/*
 * This macro is at the end of every RPC call and defines the common
 * code which is executed on return.  Input args:
 *
 *	board	operation is for ( <0 if no board in RPC call)
 *	errp 	is the address of the dr_err_t structure in the result struct
 *	status 	is a 'good state' string to place in errp->msg if it is NULL
 */
#define	DO_WRAPUP(board, errp, status) \
	dr_err.state = dr_get_board_state(board); \
	*(errp) = dr_err; \
	if ((errp)->msg == NULL) \
		(errp)->msg = (drmsg_t)strdup(status); \
	if (verbose >= 10) \
		dr_loginfo("RPC done (state = %d)\n", (errp)->state);

/*
 * If TEST_CLNT is defined, this file is compiled suitable for
 * inclusion in the client build.  This allows the client application
 * to be debugged as a single process.  This file should replace
 * the dr_daemon_clnt.c module in the client build.  NODAEMON
 * should also be defined for this compile (see below).
 *
 * The main things that change with TEST_CLNT are:
 *	- we make the type of the second argument to CLIENT *
 *	  to make the client program's lint happy.
 *	- we don't do RPC authorization
 *	- we define dr_err, dr_logerr(), and dr_loginfo().  Note
 *	  that the caller is responsible for initialzing test_log_file;
 *
 * Routine needed to link this module: xdr routines
 */
#ifdef TEST_CLNT

#ifndef DRLIB
typedef CLIENT *reqp_t;
#endif DRLIB

#define	DO_AUTH(rqstp, resultp) 	/* empty */

static dr_err_t dr_err;
extern FILE *test_log_file;	/* caller must declare/open */

#include <stdarg.h>

void
dr_loginfo(char *format, ...)
{
	va_list	varg_ptr;
#ifdef DRLIB
	static char buffer[128];

	if (!dr_debug)
		return;
#endif DRLIB

	va_start(varg_ptr, format);
#ifdef DRLIB
	vsprintf(buffer, format, varg_ptr);
	dr_error(INFO, "dr_loginfo", "dr_loginfo: %s", buffer);
#else
	vfprintf(test_log_file, format, varg_ptr);
#endif DRLIB
	va_end(varg_ptr);
}

void
dr_logerr(int error, int errno, char *msg)
{
	dr_err.err = error;
	dr_err.errno = errno;
	dr_err.msg = (drmsg_t)strdup(msg);
#ifdef DRLIB
	if (!dr_debug)
		return;

	dr_error(INFO, "dr_logerr", "dr_logerr: %d %d %s\n", error, errno, msg);
#else
	fprintf(test_log_file, "logerr: %d %d %s\n",
		    error, errno, msg);
#endif DRLIB
}

#else TEST_CLNT

/* This is what is compiled for the regular daemon */

typedef struct svc_req *reqp_t;

#define	DO_AUTH(rqstp, resultp) \
	if (!valid_dr_auth(rqstp)) { \
		dr_logerr(DRV_FAIL, EPERM, \
			    "Unauthorized RPC call."); \
		svcerr_auth((rqstp)->rq_xprt, AUTH_TOOWEAK); \
		(resultp) = NULL; \
		return (&(resultp)); \
	}
#endif TEST_CLNT


/*
 * If TEST_SVC is defined, the daemon and a test driver are built
 * as a single process.  We must disable the RPC call authorization.
 */
#ifdef TEST_SVC

#undef DO_AUTH
#define	DO_AUTH(rqstp, resultp) 	/* empty */

#endif TEST_SVC


#ifdef NODAEMON
#include <sys/types.h>
#include <time.h>
#include <hswp/hswp.h>

/*
 * Our dummied version of this variable
 */
static time_t start_time;


/*
 * If NODAEMON is defined, this module will dummy up RPC return
 * values instead of actually doing the RPC work.  This allows
 * a daemon RPC program to be built and used for application
 * testing.  NODAEMON and TEST_CLNT can be used together.
 */

/*
 * routines which aren't needed in this mode are made nops.
 */
#define	dr_driver_close()

/*
 * If we've enabled our error test code, the TEST_CRASH_DAEMON
 * macro should return a NULL pointer to simulate RPC call failure
 */
#ifdef DR_TEST_CONFIG

#undef TEST_CRASH_DAEMON
#define	TEST_CRASH_DAEMON(mask) \
	if (dr_testconfig & (mask)) { \
		resultp = NULL; \
		return (&resultp) \
	}

#endif DR_TEST_CONFIG

/*
 * Dummy up board state table for application testing.
 */
#define	get_cpu0_state(board)	(SVC_board_state[board] == DR_CPU0_MOVED)
#define	set_cpu0_state(board, state) (SVC_board_state[board] = DR_OS_DETACHED)
#define	get_dr_state(board) SVC_board_state[board]
#define	dr_get_board_state(board) SVC_board_state[board]

static int debug_newcpu0;

static dr_board_state_t SVC_board_state[MAX_BOARDS] =
#ifdef DRLIB
	{DR_IN_USE, DR_IN_USE, DR_NO_BOARD, DR_NO_BOARD,
	    DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD,
	    DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD,
	    DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD};
#else
	{DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD,
	    DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD, DR_NO_BOARD,
	    DR_IN_USE, DR_IN_USE, DR_IN_USE, DR_IN_USE,
	    DR_IN_USE, DR_IN_USE, DR_IN_USE, DR_IN_USE};
#endif DRLIB

/*
 * The following defines are used to test error returns.  Every N times a
 * routine is called it will fail or cycle through alternate return
 */
#define	FAIL_INITA	4
#define	FAIL_COMPLA	3
#define	FAIL_ABORTA	5
#define	FAIL_OBP_CONFIG	4
#define	FAIL_FINISHA	3
#define	FAIL_STATE	(10*MAX_BOARDS)
#define	FAIL_DETACHABLE	6
#define	FAIL_DRAIN	4
#define	FAIL_CONFIG	3
#define	FAIL_COST_INFO	5
#define	FAIL_LOCK	4
#define		ALT_LOCK 3
#define	FAIL_DETACH	3
#define		ALT_DETACH 5
#define	FAIL_ABORTD	4
#define	FAIL_FINISHD	3
#define	FAIL_FINISH_CPU0 5
#define	FAIL_UNSAFE	4

/*
 * A get state for these two boards will return the attach/detach
 * intermediate states to test the SSP appl's ability to recover.
 */
#ifdef DRLIB
#define	ATTACH_RECOVER	-1
#define	DETACH_RECOVER	-1
#else
#define	ATTACH_RECOVER	0
#define	DETACH_RECOVER	8
#endif

#endif NODAEMON

/*
 * Generic RPC routine start-up code
 *
 * 	1) Log entry into this routine
 * 	2) Free memory allocated during prior RPC calls
 * 	3) Authenticate the RPC caller
 * 	4) Allocate zero'ed-out memory for this RPC call
 * 	5) If testing, simulate a daemon crash
 *
 * The first version takes no arguments for the dr_loginfo call, the second
 * takes one, and the third takes two.
 */
#define	DO_STARTUP0(routine, xdr_t, errptrtype, errtype, logmessage) \
	/* Step 1 */	if (verbose >= 10) \
				dr_loginfo(logmessage); \
	/* Step 2 */	xdr_free(xdr_t, (char *)(&resultp)); \
	/* Step 3 */	DO_AUTH(rqstp, resultp); \
	/* Step 4 */	resultp = (errptrtype)malloc(sizeof (errtype)); \
			if (resultp == NULL) \
				return (&resultp); \
			memset((char *)resultp, 0, sizeof (errtype)); \
			memset((char *)&dr_err, 0, sizeof (dr_err)); \
	/* Step 5 */	TEST_CRASH_DAEMON(routine);

#define	DO_STARTUP1(routine, xdr_t, errptrtype, errtype, logmessage, arg1) \
	/* Step 1 */	if (verbose >= 10) \
				dr_loginfo(logmessage, arg1); \
	/* Step 2 */	xdr_free(xdr_t, (char *)(&resultp)); \
	/* Step 3 */	DO_AUTH(rqstp, resultp); \
	/* Step 4 */	resultp = (errptrtype)malloc(sizeof (errtype)); \
			if (resultp == NULL) \
				return (&resultp); \
			memset((char *)resultp, 0, sizeof (errtype)); \
			memset((char *)&dr_err, 0, sizeof (dr_err)); \
	/* Step 5 */	TEST_CRASH_DAEMON(routine);

#define	DO_STARTUP2(routine, xdr_t, errptrtype, errtype, logmessage, arg1, \
arg2) \
	/* Step 1 */	if (verbose >= 10) \
				dr_loginfo(logmessage, arg1, arg2); \
	/* Step 2 */	xdr_free(xdr_t, (char *)(&resultp)); \
	/* Step 3 */	DO_AUTH(rqstp, resultp); \
	/* Step 4 */	resultp = (errptrtype)malloc(sizeof (errtype)); \
			if (resultp == NULL) \
				return (&resultp); \
			memset((char *)resultp, 0, sizeof (errtype)); \
			memset((char *)&dr_err, 0, sizeof (dr_err)); \
	/* Step 5 */	TEST_CRASH_DAEMON(routine);

/*
 * -----------------------------------------------------------
 * get_board_state()
 * -----------------------------------------------------------
 *
 *	RPC routine that queries a system board's state
 *
 * Input:	boardp	(pointer to board number to query)
 *
 * Description:	Verify the argument is in range, and if so pass
 *		it on to the next layer to perform the query.
 *
 * Results:	The board's state will be returned, and it'll
 *		be a dr_board_state_t not an sfdr_state_t.
 */
dr_errp_t *
#ifdef	_XFIRE
get_board_state_4(int *boardp, reqp_t rqstp)
#endif
{
	static dr_errp_t	resultp;
	int			board = *boardp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_STATE, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"get_board_state(board = %d)\n", board);

	/*
	 * Verify that the system board argument is valid
	 */
	if (!BOARD_IN_RANGE(board)) {
	    dr_logerr(DRV_FAIL, 0, \
		"get_board_state: invalid board number.");
	}

#ifdef NODAEMON
	else {
		static int fail_state = 1;

		/*
		 * For *_RECOVER board, return intermediate state only
		 * at start of attach/detach options.
		 */
		if (board == ATTACH_RECOVER &&
		    get_dr_state(board) == DR_NO_BOARD) {

			static int attach_recover = 0;

			if (attach_recover == 0) {
				SVC_board_state[board] = DR_ATTACH_INIT;
				attach_recover = 1;
			} else {
				SVC_board_state[board] = DR_OS_ATTACHED;
				attach_recover = 0;
			}

		} else if (board == DETACH_RECOVER &&
			    get_dr_state(board) == DR_IN_USE) {

			static int detach_recover = 0;

			if (detach_recover == 0) {
				SVC_board_state[board] = DR_DRAIN;
				detach_recover = 1;
			} else if (detach_recover == 1) {
				SVC_board_state[board] = DR_CPU0_MOVED;
				detach_recover = 2;
			} else {
				SVC_board_state[board] = DR_OS_DETACHED;
				detach_recover = 0;
			}

		} else if ((fail_state++ % FAIL_STATE) == 0) {

			dr_logerr(DRV_FAIL, 0,
				    "get_board_state: Error getting state.");

			/*
			 * setting board to an invalid board number
			 * will cause DR_ERROR_STATE to be returned
			 */
			board = -1;
		}
	}
#endif NODAEMON

	/*
	 * Clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "board state returned");

	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * initiate_attach_board()
 * -----------------------------------------------------------
 *
 *	RPC routine that init-attach's a system board
 *
 * Input:	- System board to init-attach
 *		- ID of the board's CPU that has the post2obp
 *		  struct (or, a -1 in recovery situations)
 *
 * Description:	Verify the arguments are in range and that the
 *		operation is valid.  If so, then pass on the
 *		arguments to the dr_init_attach() routine that
 *		will merge in the OBP nodes for this system
 *		board.
 *
 * Results:	If the board's unconnected, an attempt is made
 *		to connect it.  The board's state could change.
 */
dr_errp_t *
#ifdef	_XFIRE
initiate_attach_board_4(brd_init_attach_t *argp, reqp_t rqstp)
#endif
{
	static dr_errp_t	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP2(DR_RPC_IATTACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"initiate attach of board %d using cpu %d\n", \
		argp->board_slot, argp->cpu_on_board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 * 	2) Verify that the CPU argument is in range (or a -1)
	 *	3) Verify that the board isn't already init-attached
	 *	4) Verify that the system board is in the OCCUPIED or EMPTY
	 *	   state, the only states in which it can be connected
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(argp->board_slot)) {
		dr_logerr(DRV_FAIL, 0,
			    "initiate_attach_board: invalid board number.");
	}

	/* Step 2 */
	else if (argp->cpu_on_board != -1 &&
		    (argp->cpu_on_board <
		    (argp->board_slot*MAX_CPU_UNITS_PER_BOARD)) ||
		    (argp->cpu_on_board >=
		    ((argp->board_slot+1)*MAX_CPU_UNITS_PER_BOARD))) {
		dr_logerr(DRV_FAIL, 0,
			    "initiate_attach_board: invalid cpu number.");
	}

	/* Step 3 */
	else if ((state = get_dr_state(argp->board_slot)) \
			== SFDR_STATE_CONNECTED) {
		dr_loginfo("initiate_attach_board: already init-attached.");
	}

	/* Step 4 */
	else if (state != SFDR_STATE_EMPTY && state != SFDR_STATE_OCCUPIED) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				"initiate_attach_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up daemon for testing, dummy up results
		 */
#ifdef NODAEMON
		static int fail_inita = 1;

		/* Mimic a failure? */
		if ((fail_inita++ % FAIL_INITA) == 0) {
			dr_logerr(DRV_FAIL, 0, "Unable to probe the board");
		} else {
			SVC_board_state[argp->board_slot] = DR_ATTACH_INIT;
		}

		/*
		 * Else, pass this call's arguments on to the next level, for
		 * real processing.
		 */
#else
		dr_init_attach(argp);
#endif
	}

	/*
	 * Clean up and return the results of the RPC call
	 */
	DO_WRAPUP(argp->board_slot, resultp, "attach initiated");
	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * abort_attach_board()
 * -----------------------------------------------------------
 *
 *	RPC routine that aborts attachment of a system board
 *
 * Input:	System board to abort-attach
 *
 * Description:	Verify that the given system board is in range
 *		and that it's connected	(meaning its OBP nodes
 *		have been merged in).  If so, pass the board
 *		number to the next level to actually abort its
 *		attachment.
 *
 * Results:	If the board had been connected, then an attempt
 *		will be made to disconnect it.  The board's state
 *		could change.
 */
dr_errp_t *
#ifdef	_XFIRE
abort_attach_board_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t  	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_AATTACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"abort attach of board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is in the CONNECTED state, the
	 *	   only state in which it can be abort-attached.
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "abort_attach_board: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_CONNECTED &&
			state != SFDR_STATE_UNREFERENCED) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "abort_attach_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up daemon for testing, dummy up results
		 */
#ifdef NODAEMON
		static int fail_aborta = 1;

		/* Mimic a good/bad return */
		if ((fail_aborta++ % FAIL_ABORTA) == 0) {

			/*
			 * In the case of failure, leave the board in
			 * whatever state it currently is.  The
			 * board is in an unknown state and our best
			 * guess as to what state it should be in is
			 * the last known state.
			 */
			dr_logerr(DRV_FAIL, 0, "Abort attach failed.");

		} else {
			/* Make the application reset the board */
#ifndef NOTYET_ABORT_ATTACH
			SVC_board_state[board] = DR_OS_DETACHED;
#else
			SVC_board_state[board] = DR_BOARD_WEDGED;
#endif NOTYET_ABORT_ATTACH
		}

		/*
		 * Else, pass this call's arguments on to the next level, for
		 * real processing.
		 */
#else
		dr_abort_attach_board(board);
#endif
	}

	/*
	 * Clean up and return the results of the RPC call.
	 *
	 * Note: a board in the EMPTY state (which usually is translated to
	 * DR_NO_BOARD) should be translated to DR_OS_DETACHED within the
	 * context of an abort_attach RPC call.  (STATE TRANSLATION)
	 */
	DO_WRAPUP(board, resultp, "abort attach done");
	if (resultp && ((dr_err_t *)resultp)->state == DR_NO_BOARD)
		((dr_err_t *)resultp)->state = DR_OS_DETACHED;
	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * complete_attach_board()
 * -----------------------------------------------------------
 *
 * 	RPC routine that completes attachment of a board
 *
 * Input:	System board to complete-attach
 *
 * Description:	Verify that the given system board is in
 *		range and connected (meaning its OBP nodes
 *		have been merged in).  If so, pass on the
 *		board number to the next level to complete
 *		its attachment.
 *
 * Results:	If a board has been connected, an attempt
 *		is made to configure it.  The board's state
 *		could change.
 */
dr_errp_t *
#ifdef	_XFIRE
complete_attach_board_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t  	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_CATTACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"complete attach of board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is in the CONNECTED state, which
	 *	   is the only state in which it can be complete-attached.
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "complete_attach_board: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_CONNECTED &&
			state != SFDR_STATE_UNCONFIGURED &&
			state != SFDR_STATE_PARTIAL) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				"complete_attach_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up daemon for testing, dummy up results
		 */
#ifdef NODAEMON
		static int fail_compla = 1;

		/* Mimic a good/bad return */
		if ((fail_compla++ % FAIL_COMPLA) == 0) {

			dr_logerr(DRV_FAIL, 0, "Complete attach failed.");
		} else
			SVC_board_state[board] = DR_OS_ATTACHED;

		/*
		 * Else, pass this call's arguments on to the next level, for
		 * real processing.
		 */
#else
		dr_complete_attach(board);
#endif
	}

	/*
	 * Clean up and return the results of the RPC call
	 */
	DO_WRAPUP(board, resultp, "complete attach done");
	if (resultp->state == DR_IN_USE)
		resultp->state = DR_OS_ATTACHED;
	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * attach_finished()
 * -----------------------------------------------------------
 *
 *	RPC routine used by SSP to acknowledge success of
 *	a board's attachment (both the board's connection
 *	and configuration).
 *
 * Input:	System board that has been fully attached
 *
 * Description:	This routine used to just change a board's
 *		state to indicate that it was both connected
 *		and configured into the system.  But, the
 *		new DR driver changes boards' states itself
 *		so, this routine really has no purpose.  It
 *		is included as a noop for completeness.
 *
 *		(It's not really a noop, since it does some
 *		validation of the arguments to verify that
 *		the SSP isn't mistaken.)
 *
 *		Note that DR_OS_ATTACHED is never returned
 *		from dr_get_board_state().  This is the
 *		only time that we should need to return it.
 *		So we sneak that into the state value for
 *		the board.
 *
 * Results:	None, really.
 */
dr_errp_t *
#ifdef	_XFIRE
attach_finished_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_FATTACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"attach finished for board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is in the CONFIGURED state;
	 *	   anything else would mean the SSP is confused
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0, "attach_finished: " \
			"invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_CONFIGURED &&
			state != SFDR_STATE_PARTIAL) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "attach_finished: invalid board state.");
	}

	/*
	 * The RPC call seems valid; perform the operation.
	 */
#ifdef NODAEMON
	else {
		static int fail_finisha = 1;

		/*
		 * For testing, report an error and _also_ change the
		 * state so the board doesn't get wedged in an intermediate
		 * state.  In real life, a failure when setting board state
		 * is pretty bad and the DR operations will quickly come to
		 * a stop.
		 */
		if ((fail_finisha++ % FAIL_FINISHA) == 0) {
			dr_logerr(DRV_FAIL, 0, "Attach finished failed.");
		}
	}
#endif

	/*
	 * Clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "attach finish done");
	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * detachable_board()
 * -----------------------------------------------------------
 *
 * 	RPC routine that queries whether a board is detachable
 *
 * Input:	System board to query
 *
 * Description:	If the board is in range and in this domain,
 *		then it's passed to the next layer which will
 *		determine whether this board is detachable.
 *
 * Results:	Basically, the next layer will fail if the
 *		board isn't detachable.  If it *is* detachable,
 *		then there won't be any errors.
 */
dr_errp_t *
#ifdef	_XFIRE
detachable_board_4(int *boardp, reqp_t rqstp)
#endif
{
	int				board = *boardp;
	static dr_errp_t		resultp;
	sfdr_state_t			state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_DBOARD, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"detachable query for board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is installed on the system.
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "detachable_board: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) == SFDR_STATE_EMPTY) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "detachable_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this daemon is for testing purposes only, fudge the
		 * results.
		 */
#ifdef NODAEMON
		static int fail_detachable = 1;

		/* Mimic a good/bad return */
		if ((fail_detachable++ % FAIL_DETACHABLE) == 0) {

			dr_logerr(DRV_FAIL, 0,
			    "Devices attached to the board "
			    "do not support DR.");
		}

		/*
		 * Else, perform a real query on the detachability of this
		 * board.
		 */
#else
		dr_detachable_board(board);
#endif
	}

	/*
	 * Clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "detachable query done");
	return (&resultp);
}

/*
 * -----------------------------------------------------------
 * drain_board_resources()
 * -----------------------------------------------------------
 *
 *	RPC routine that begins off the drain operation.
 *
 * Input:	System board to drain
 *
 * Description:	If the board is in range and drainable, then
 *		pass it on to the next layer to be drained.
 *		(What used to be called "draining" a board is
 *		now called "releasing" it with DR 2.7 QU2.)
 *
 * Results:	The "drain" (or "release") operation begins if the argument is
 *		valid.
 */
dr_errp_t *
#ifdef	_XFIRE
drain_board_resources_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t  	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_DRAIN, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"drain resources of board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is in the CONFIGURED state
	 *	   or the PARTIAL state
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "drain_board_resources: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_CONFIGURED &&
			state != SFDR_STATE_PARTIAL) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				"drain_board_resources: invalid board state.");
	}

	/*
	 * The RPC call seems valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up test daemon, dummy up some results
		 */
#ifdef NODAEMON
		static int fail_drain = 1;

		/* Mimic a good/bad return */
		if ((fail_drain++ % FAIL_DRAIN) == 0) {
			dr_logerr(DRV_FAIL, 0, "Drain resources failed.");
		} else {
			SVC_board_state[board] = DR_DRAIN;
			(void) time(&start_time);
		}

		/*
		 * Else, pass the arguments on to the next layer for processing
		 */
#else
		dr_drain_board_resources(board);
#endif
	}

	/*
	 * The RPC call is done, so clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "board resources draining");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * detach_board()
 * ------------------------------------------------------
 *
 * RPC routine that completes a detach operation, after a board's been drained.
 *
 * Input:	System board that'll be detached
 *
 * Description:	If the board is in range and detachable, then pass it on to the
 *		next level to actually be detached.  (What used to be called
 *		"detaching" a board is now two separate steps: "unconfiguring"
 *		and "disconnecting.")
 *
 * Results:	The "unconfigure" and "detach" operations are performed in the
 *		next layer if the arguments are valid.
 */
detach_errp_t *
#ifdef	_XFIRE
detach_board_4(brd_detach_t *argp, reqp_t rqstp)
#endif
{
	static detach_errp_t	resultp;
	sfdr_stat_t  		*status;
	sfdr_state_t		state, pstate;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP2(DR_RPC_DETACH, xdr_detach_errp_t, detach_errp_t, \
		detach_err_t, "detach board %d (force=%d)\n", \
		argp->board_slot, argp->force_flag);

	/*
	 * Verify that the RPC call is valid
	 *
	 *	1) Gather the current and previous board state, if possible
	 * 	2) Verify that the system board argument is in range
	 *	3) Verify that the system board is either in the UNREFERENCED
	 *	   state or that it is partially detached (its state would be
	 *	   PARTIAL, and its previous state would be UNREFERENCED in
	 *	   this case).
	 */

	/* Step 1 */
	if (BOARD_IN_RANGE(argp->board_slot)) {
		status = get_dr_status(argp->board_slot);
		if (status == NULL) {
			state = SFDR_STATE_FATAL;
			pstate = SFDR_STATE_FATAL;
		} else {
			state = status->s_bstate;
			pstate = status->s_pbstate;
			free(status);
		}
	}

	/* Step 2  */
	if (!BOARD_IN_RANGE(argp->board_slot)) {
		dr_logerr(DRV_FAIL, 0, \
			"detach_board: invalid board number.");
	}

	/* Step 3  */
	else if (state != SFDR_STATE_UNREFERENCED && \
			(state != SFDR_STATE_PARTIAL || \
			pstate != SFDR_STATE_UNREFERENCED) && \
			dr_get_mib_update_pending() != argp->board_slot) {

		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "detach_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up test daemon, dummy up some results
		 */
#ifdef NODAEMON
		static int fail_detach = 1;
		hswperr_t *herr;

		if ((fail_detach++ % FAIL_DETACH) == 0) {
			dr_logerr(DRV_FAIL, 0, "Detach board failed.");

		} else if ((fail_detach % ALT_DETACH) == 0) {

			herr = malloc(sizeof (hswperr_t));

			if (herr == NULL) {
				dr_logerr(DRV_FAIL, 0,
				    "malloc failed (hswperr_t)");
			} else {
				resultp->dr_hswperr_buff.dr_hswperr_buff_val
					= (unsigned char *)herr;
				resultp->dr_hswperr_buff.dr_hswperr_buff_len
					= (uint_t)sizeof (hswperr_t);

				/* dummy up an error structure */
				herr->error = (short)HSWPERR_UNSAFE_DEV;
				herr->uu.unsafe_dev.count = (short)1;
				herr->uu.unsafe_dev.majnum[0] = (ushort_t)20;

				dr_logerr(DRV_FAIL, 0, "OS quiesce failed.");
			}
		} else {
			SVC_board_state[argp->board_slot] = DR_OS_DETACHED;
#ifdef _XFIRE
			/*
			 * here we need to dummy up the board "copy/rename"
			 * return information.
			 */
			resultp->dr_board = argp->board_slot;
			resultp->dr_rename = 0;

#endif /* _XFIRE */
		}

		/*
		 * Else, pass the arguments on to the next layer for processing
		 */
#else
		dr_detach_board(argp, resultp);
#endif
	}

	/*
	 * The RPC call is done, so clean up and return the results.
	 *
	 * Note: we have a little extra context with which to translate the
	 * state of the board:
	 *
	 *	If this routine succeeded, the board would be in the EMPTY
	 *	state and the right state to return is actually DR_OS_DETACHED
	 *	since that's what the SSP expects at the end of this RPC.
	 *
	 * 	If this routine failed, the board should still be in the
	 *	UNREFERENCED state.  So we return the state DR_DRAIN to
	 *	indicate that this DR operation didn't work.  (STATE
	 *	TRANSLATION)
	 */
	DO_WRAPUP(argp->board_slot, &(resultp->dr_err), "detach board done");
	if (resultp != NULL) {
		if (resultp->dr_err.state == DR_NO_BOARD)
			resultp->dr_err.state = DR_OS_DETACHED;
		else {
			state = get_dr_state(argp->board_slot);
			if (state == SFDR_STATE_UNREFERENCED)
				resultp->dr_err.state = DR_DRAIN;
		}
	}

	return (&resultp);
}

/*
 * ------------------------------------------------------
 * abort_detach_board()
 * ------------------------------------------------------
 *
 * RPC routine that aborts a detach if the board's only being/been released
 *
 * Input:	System board that'll be abort-detached
 *
 * Description:	If the board argument is valid, and the board is in the
 *		RELEASE or UNREFERENCED state, pass the arguments on
 *		to the next layer to actually abort the detachment of this
 *		board.
 *
 * Results:	If the arguments are valid, the detachment of the board is
 *		aborted.
 */
dr_errp_t *
#ifdef	_XFIRE
abort_detach_board_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	sfdr_stat_t  		*status;
	sfdr_state_t		state, pstate;
	static dr_errp_t  	resultp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_ADETACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"abort detach of board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 *	1) Gather the current and previous board state, if possible
	 * 	2) Verify that the system board argument is in range
	 *	3) Verify that the system board is in the RELEASE, UNREFERENCED,
	 *	   or partially detached (current state == PARTIAL, previous
	 *	   state == UNREFERENCED) state.
	 */

	/* Step 1 */
	if (BOARD_IN_RANGE(board)) {
		status = get_dr_status(board);
		if (status == NULL) {
			state = SFDR_STATE_FATAL;
			pstate = SFDR_STATE_FATAL;
		} else {
			state = status->s_bstate;
			pstate = status->s_pbstate;
			free(status);
		}
	}

	/* Step 2 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "abort_detach_board: invalid board number.");
	}

	/* Step 3 */
	else if (state != SFDR_STATE_RELEASE && \
			state != SFDR_STATE_UNREFERENCED && \
			state != SFDR_STATE_UNCONFIGURED && \
			(state != SFDR_STATE_PARTIAL || \
			pstate != SFDR_STATE_UNREFERENCED)) {

		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "abort_detach_board: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
		/*
		 * If this is a dummied up test daemon, dummy up some results
		 */
#ifdef NODAEMON
		static int fail_abortd = 1;

		/* Mimic a good/bad return */
		if ((fail_abortd++ % FAIL_ABORTD) == 0) {
			/*
			 * In the event of a failure during the abort,
			 * our best guess as to what state the board
			 * is in is the last state it achieved.
			 */
			dr_logerr(DRV_FAIL, 0, "Abort Detach failed.");
		} else {
			/* success, board is reattached */
			SVC_board_state[argp->board_slot] = DR_IN_USE;
		}

		/*
		 * Else, pass the arguments on to the next layer for processing
		 */
#else
		dr_abort_detach_board(board);
#endif
	}

	/*
	 * The RPC call is done, so clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "abort detach done");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * cpu0_move_finished()
 * ------------------------------------------------------
 *
 * This is the SSP's way of saying that it's done moving CPU0.
 *
 * Description:	This really doesn't matter anymore.  It's not
 *		totally noop'ed because, for completeness, we'd
 *		like to at least let the SSP know if it's calling
 *		this routine at the wrong time.
 */
dr_errp_t *
#ifdef	_XFIRE
cpu0_move_finished_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_CPU0MV, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"cpu0 move finished on board %d\n", board);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the system board is in the UNREFERENCED
	 *	   state (this just means the RPC is appropriate)
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "cpu0_move_finished: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_UNREFERENCED) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "cpu0_move_finished: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
#ifdef NODAEMON
	else {
		static int fail_finish_cpu0 = 1;

		/*
		 * For testing, report an error and _also_ change the
		 * state so the board doesn't get wedged in an intermediate
		 * state.  In real life, a failure when setting board state
		 * is pretty bad and the DR operations will quickly come to
		 * a stop.
		 */
		if ((fail_finish_cpu0++ % FAIL_FINISH_CPU0) == 0) {
			dr_logerr(DRV_FAIL, 0,
				    "Finish of cpu0 move failed.");
		}
	}
#endif

	/*
	 * The RPC call is done, so clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "cpu0 move complete");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * detach_finished()
 * ------------------------------------------------------
 *
 * This is the SSP's way of saying that it's done detaching a board.
 *
 * Description:	This really doesn't matter anymore.  It's not
 *		totally noop'ed because, for completeness, we'd
 *		like to at least let the SSP know if it's calling
 *		this routine at the wrong time.
 */
dr_errp_t *
#ifdef	_XFIRE
detach_finished_4(int *boardp, reqp_t rqstp)
#endif
{
	int			board = *boardp;
	static dr_errp_t	resultp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_FDETACH, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"detach finished for board %d\n", board);

	/*
	 * Verify that the RPC is valid
	 *
	 *		1) Verify that the given board is in range
	 *		2) Verify that the board is, in fact, detached
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "detach_finished: invalid board number.");
	}

	/* Step 2 */
	else if ((state = get_dr_state(board)) != SFDR_STATE_EMPTY) {

		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
				    "detach_finished: invalid board state.");
	}

	/*
	 * The RPC call is valid, so proceed
	 */
	else {
#ifdef NODAEMON
		static int fail_finishd = 1;

		/*
		 * For testing, report an error and _also_ change the
		 * state so the board doesn't get wedged in an intermediate
		 * state.  In real life, a failure when setting board state
		 * is pretty bad and the DR operations will quickly come to
		 * a stop.
		 */
		if ((fail_finishd++ % FAIL_FINISHD) == 0) {
			dr_logerr(DRV_FAIL, 0, "Detach finish failed.");
		}
		/* SSP application has completed the detach */
		SVC_board_state[argp->board_slot] = DR_NO_BOARD;
#else	NODAEMON
		dr_finish_detach();
#endif
	}

	/*
	 * The RPC call is done, so clean up and return the results
	 */
	DO_WRAPUP(board, resultp, "detach finish done");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * get_obp_board_configuration()
 * ------------------------------------------------------
 */
board_configp_t *
#ifdef	_XFIRE
get_obp_board_configuration_4(int *boardp, reqp_t rqstp)
#endif
{
	static board_configp_t	resultp;
	int			board = *boardp;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP1(DR_RPC_OBPCONFIG, xdr_board_configp_t, board_configp_t, \
		board_config_t, "get obp configuration for board %d\n", board);

	/* Verify args and current state */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0,
			    "get_obp_board_config: invalid board number.");
	}

	else if ((state = get_dr_state(board)) == SFDR_STATE_EMPTY ||
			state == SFDR_STATE_OCCUPIED) {
		if (state != SFDR_STATE_FATAL)
			dr_logerr(DRV_FAIL, 0,
			    "get_obp_board_config: invalid board state.");

	} else {
#ifdef NODAEMON
		static int 	fail_obp_config = 1;
		sbus_configp_t	sp;

		if ((fail_obp_config++ % FAIL_OBP_CONFIG) == 0) {
			dr_logerr(DRV_FAIL, 0, "Unable to get configuration.");
		} else {
			resultp->board_slot = board;

			resultp->cpu_cnt = 2;
			resultp->cpu[0].cpuid = board*MAX_CPU_UNITS_PER_BOARD \
						+ 1;
			resultp->cpu[0].frequency = 50;
			resultp->cpu[1].cpuid = board*MAX_CPU_UNITS_PER_BOARD \
						+ 2;
			resultp->cpu[1].frequency = 50;

#ifdef _XFIRE
			resultp->cpu[0].ecache_size = 0.5;
			resultp->cpu[1].ecache_size = 2.0;
			resultp->cpu[0].mask = 21;
			resultp->cpu[1].mask = 21;
#endif /* _XFIRE */

			resultp->mem.sys_mem = 256;
			resultp->mem.dr_min_mem = 128;
			resultp->mem.dr_max_mem = 1024;
			resultp->mem.dr_mem_detach = strdup("1 (enabled)");
			resultp->mem.mem_size = 256;

			sp = calloc(1, sizeof (sbus_config_t));
			if (sp == NULL) goto done;
#ifdef _XFIRE
			resultp->ioc0[0] = sp;
#endif /* _XFIRE */
			sp->name = strdup("QLGC,isp/sd");

			sp = calloc(1, sizeof (sbus_config_t));
			if (sp == NULL) goto done;
#ifdef _XFIRE
			resultp->ioc0[1] = sp;
#endif /* _XFIRE */
			sp->name = strdup("bf");

			sp = calloc(1, sizeof (sbus_config_t));
			if (sp == NULL) goto done;
#ifdef _XFIRE
			resultp->ioc1[0] = sp;
#endif /* _XFIRE */
			sp->name = strdup("dma/esp");

			sp = calloc(1, sizeof (sbus_config_t));
			if (sp == NULL) goto done;
#ifdef _XFIRE
			resultp->ioc1[1] = sp;
#endif /* _XFIRE */
			sp->name = strdup("lebuffer/le");
		done: /* empty */;
		}
#else
		dr_get_obp_board_configuration(board, resultp);
#endif
	}

	DO_WRAPUP(board, &resultp->dr_err,
		    "get obp board configuration complete");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * get_attached_board_info()
 * ------------------------------------------------------
 *
 * RPC routine that queries the configuration of an attached board
 *
 * Input:	System board that'll be queried
 *
 * Description:	This routine is used to retrieve information about memory,
 *		CPU's, and I/O devices and configurations attached to a
 *		system board.  The SSP application uses this RPC routine to
 *		periodically determine the progress of a "drain" operation,
 *		to calculate the cost associated with detaching a board,
 *		as well as to acquire statistics on the usage of an attached
 *		board (what its CPU's are doing, how much of its memory is
 *		unrelocatable, and how many devices it has).
 *
 *		After verifying that the board is in fact connected, this
 *		routine passes on the information request to the next layer
 *		for processing.
 *
 * Results:	If the arguments are valid, the requested board information is
 *		returned.
 */
#ifdef NODAEMON
void
dr_get_attached_board_info(brd_info_t *bip, attached_board_infop_t bcp)
{
	sbus_cntrlp_t		cp;
	sbus_devicep_t		dp;
	sbus_usagep_t		up;
	board_cpu_configp_t	cpu;
	board_mem_configp_t	mem_config;
	board_mem_costp_t	mem_cost;
	board_mem_drainp_t	mem_drain;
	static int 		fail_config = 1;
	int 			i, len;

	if ((fail_config++ % FAIL_CONFIG) == 0) {
		dr_logerr(DRV_FAIL, 0, "Unable to get configuration.");
		return;
	}

	/* dummy up info values */
	bcp->board_slot = bip->board_slot;
	bcp->flag = bip->flag;

	/* CPU info */
	if (bcp->flag & BRD_CPU) {

		cpu = calloc(1, sizeof (board_cpu_config_t));
		if (cpu == NULL) {
			dr_logerr(DRV_FAIL, 0, "malloc failed");
			return;
		}
		bcp->cpu = cpu;
		cpu->cpu_cnt = 2;
		cpu->cpu0 = debug_newcpu0;

		cpu->cpu[0].cpuid = (bip->board_slot*MAX_CPU_UNITS_PER_BOARD)+1;
#ifdef _XFIRE
			cpu->cpu[0].mask = 21;
#endif /* _XFIRE */
		cpu->cpu[0].cpu_state = (drmsg_t)strdup("Online");
		cpu->cpu[0].partn_size = 5;
		cpu->cpu[0].num_user_threads_bound = 4;
		cpu->cpu[0].num_sys_threads_bound = 6;
		len = 10;
		cpu->cpu[0].proclist.proclist_val = (proclist_t *)
			calloc(len, sizeof (proclist_t));
		if (cpu->cpu[0].proclist.proclist_val != NULL) {
			cpu->cpu[0].proclist.proclist_len = len;
			for (i = 0; i < len; i++)
				cpu->cpu[0].proclist.proclist_val[i].pid =
					5640 + i;
		}

		cpu->cpu[1].cpuid = cpu->cpu[0].cpuid + 1;
#ifdef _XFIRE
			cpu->cpu[1].mask = 21;
#endif /* _XFIRE */
		cpu->cpu[1].cpu_state = (drmsg_t)strdup("Online");
		cpu->cpu[1].partition = PS_NONE;
		cpu->cpu[1].partn_size = 0;
		cpu->cpu[1].num_user_threads_bound = 0;
		cpu->cpu[1].num_sys_threads_bound = 0;
	}

	if (bcp->flag & BRD_MEM_CONFIG) {

		mem_config = calloc(1, sizeof (board_mem_config_t));
		if (mem_config == NULL) {
			dr_logerr(DRV_FAIL, 0, "malloc failed");
			return;
		}
		bcp->mem_config = mem_config;
		mem_config->sys_mem = 512;
		mem_config->dr_min_mem = 128;
		mem_config->dr_max_mem = 1024;
		mem_config->dr_mem_detach = strdup("1 (enabled)");
		mem_config->mem_size = 256;
		mem_config->mem_if = 1;
#ifdef _XFIRE
		mem_config->interleave_board = bcp->board_slot;
		mem_config->mem_pages.l_pfn = 0x1000;
		mem_config->mem_pages.h_pfn = 0x2000;
#endif /* _XFIRE */


	}

	if (bcp->flag & BRD_MEM_COST) {

		mem_cost = calloc(1, sizeof (board_mem_cost_t));
		if (mem_cost == NULL) {
			dr_logerr(DRV_FAIL, 0, "malloc failed");
			return;
		}
		bcp->mem_cost = mem_cost;
		mem_cost->actualcost = 1;
		mem_cost->mem_pshrink = 20;
		mem_cost->mem_pdetach = 256;
	}

	if (bcp->flag & BRD_MEM_DRAIN) {

		mem_drain = calloc(1, sizeof (board_mem_drain_t));
		if (mem_drain == NULL) {
			dr_logerr(DRV_FAIL, 0, "malloc failed");
			return;
		}
		bcp->mem_drain = mem_drain;
		mem_drain->mem_drain_percent = 100;
		mem_drain->mem_kb_left = 0;
		mem_drain->mem_drain_start = start_time;
		(void) time(&mem_drain->mem_current_time);
	}

	if (bcp->flag & BRD_IO) {

		/*
		 * slot 0: esp0/dma0 (AP is_alternate, is_active)
		 * sd30:
		 *    /dev/dsk/c0t3d0s0	/	pids
		 *    /dev/dsk/c0t3d0s6	/usr	pids
		 */

		/* controller info */
#ifdef _XFIRE
		cp = bcp->ioc0[0] = (sbus_cntrlp_t)
		    calloc(1, sizeof (sbus_cntrl_t));
#endif /* _XFIRE */
			calloc(1, sizeof (sbus_cntrl_t));
		if (cp == NULL)
			return;
		cp->name = (name_t)strdup("esp0/dma0");
		cp->ap_info.is_alternate = 1;
		cp->ap_info.is_active = 1;

		/* first device for controller (sd30) */
		dp = cp->devices = (sbus_devicep_t)
			calloc(1, sizeof (sbus_device_t));
		if (dp == NULL)
			return;
		dp->name = (name_t)strdup("sd30");

		/* first usage for sd30 */
		up = dp->usage = (sbus_usagep_t)
			calloc(1, sizeof (sbus_usage_t));
		if (up == NULL)
			return;
		up->name = (name_t)strdup("/dev/dsk/c0t3d0s0");
		up->opt_info = (name_t)strdup("/export");
		up->usage_count = 5;

		/* second usage for sd30 */
		up = up->next = (sbus_usagep_t)
			calloc(1, sizeof (sbus_usage_t));
		if (up == NULL)
			return;
		up->name = (name_t)strdup("/dev/dsk/c0t3d0s6");
		up->opt_info = (name_t)strdup("/test");
		up->usage_count = 20;

		/*
		 * slot 0: lebuffer0/le0
		 * inactive AP controller
		 *    le0	interface up
		 */

		/* controller lebuffer0 */
		cp = cp->next = (sbus_cntrlp_t)
			calloc(1, sizeof (sbus_cntrl_t));
		if (cp == NULL)
			return;
		cp->name = (name_t)strdup("lebuffer0");

		/* device le0 */
		dp = cp->devices = (sbus_devicep_t)
			calloc(1, sizeof (sbus_device_t));
		if (dp == NULL)
			return;
		dp->name = (name_t)strdup("le0");

		/* usage of le0 */
		up = dp->usage = (sbus_usagep_t)
			calloc(1, sizeof (sbus_usage_t));
		if (up == NULL)
			return;
		up->name = (name_t)strdup("le0");
		up->opt_info = (name_t)
		    strdup("up (136.162.18.26), AP alternate");
		up->usage_count = -1;

		/*
		 * slot 1: isp0/st3
		 */

		/* controller isp0 */
#ifdef _XFIRE
		cp = bcp->ioc0[1] = (sbus_cntrlp_t)
		    calloc(1, sizeof (sbus_cntrl_t));
#endif /* _XFIRE */
			calloc(1, sizeof (sbus_cntrl_t));
		if (cp == NULL)
			return;
		cp->name = (name_t)strdup("isp0");

		/* device st3 */
		dp = cp->devices = (sbus_devicep_t)
			calloc(1, sizeof (sbus_device_t));
		if (dp == NULL)
			return;
		dp->name = (name_t)strdup("st3");

		/* usage for st3 */
		up = dp->usage = (sbus_usagep_t)
			calloc(1, sizeof (sbus_usage_t));
		if (up == NULL)
			return;
		up->name = (name_t)strdup("/dev/rmt/0ln");
		up->usage_count = 1;
	}
}
#endif /* NODAEMON */

attached_board_infop_t *
#ifdef	_XFIRE
get_attached_board_info_4(brd_info_t *argp, reqp_t rqstp)
#endif
{
	static attached_board_infop_t	resultp;
	static int			terminate_drain = 0;
	sfdr_state_t			state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP2(DR_RPC_BRDINFO, xdr_attached_board_infop_t, \
		attached_board_infop_t, attached_board_info_t, \
		"get info for board %d (flag = %d)\n", \
		argp->board_slot, argp->flag);

	/*
	 * Verify that the RPC call is valid
	 *
	 * 	1) Verify that the system board argument is in range
	 *	2) Verify that the "information-to-query" flags are valid
	 *	3) Verify that the system board is at least connected to the
	 *	   system.
	 */

	/* Step 1 */
	if (!BOARD_IN_RANGE(argp->board_slot)) {
		dr_logerr(DRV_FAIL, 0, "get_board_config: "
		    "invalid board number.");
	}

	/* Step 2 */
	else if ((argp->flag & BRD_ALL) == 0 ||
		    (argp->flag & ~BRD_ALL) != 0) {
		dr_logerr(DRV_FAIL, 0, "get_board_config: invalid flag.");
	}

	/* Step 3 */
	else if ((state = get_dr_state(argp->board_slot)) == SFDR_STATE_EMPTY) {
		dr_logerr(DRV_FAIL, 0,
			    "get_board_config: invalid board state.");
	}

	/*
	 * It's still possible that the board exists but is in a fatal state.
	 * If it's not in a fatal state, pass the arguments on to the next
	 * layer for processing.
	 */
	else if (state != SFDR_STATE_FATAL) {
		dr_get_attached_board_info(argp, resultp);
	}

	/*
	 * If a drain failure is encountered during a query, then the
	 * terminate drain flag is set so that on the next query we can report
	 * zero kilobytes of memory left for the drain to terminate the drain
	 * on the SSP side after the previous RPC already reported the drain
	 * failure.
	 */
	if (argp->flag & BRD_MEM_DRAIN) {
		if (terminate_drain == 0 && dr_err.msg) {
			terminate_drain = 1;
		} else if (terminate_drain == 1 && resultp->mem_drain) {
			terminate_drain = 0;
			resultp->mem_drain->mem_kb_left = 0;
		}
	}

	/*
	 * Do some cleanup and return the results
	 */
	DO_WRAPUP(argp->board_slot, &resultp->dr_err,
		    "get_attached_board_info complete");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * unsafe_devices()
 * ------------------------------------------------------
 *
 * RPC routine that queries which devices in the domain are unsafe.
 *
 * Input: 	Note that no arguments are passed to this routine, but the RPC
 * 		dispatcher always passes two arguments.
 *
 * Description:	The routine has no arguments to verify and no policy checks to
 *		make, so it just performs the query at the next layer, returning
 *		the results of that query.
 *
 * Results:	An array of all the unsafe devices in the domain.
 */
/* ARGSUSED */
unsafe_devp_t *
#ifdef	_XFIRE
unsafe_devices_4(void *argp, reqp_t rqstp)
#endif
{
	static unsafe_devp_t	resultp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP0(DR_RPC_UNSAFED, xdr_unsafe_devp_t, unsafe_devp_t, \
		unsafe_dev_t, "query unsafe devices\n");

#ifdef NODAEMON
	{
		static int fail_unsafe = 1;

		if ((fail_unsafe++ % FAIL_UNSAFE) == 0) {
			dr_logerr(DRV_FAIL, 0, "Unsafe devices failed.");
		} else {
			/* dummy up some unsafe devices */
			resultp->unsafe_devs.unsafe_devs_val = (name_t *)
				calloc(1, sizeof (name_t));
			if (resultp->unsafe_devs.unsafe_devs_val != NULL) {
				resultp->unsafe_devs.unsafe_devs_len = 1;
				resultp->unsafe_devs.unsafe_devs_val[0] =
					(name_t)strdup("css");
			}
		}
	}
#else
	dr_unsafe_devices(resultp);
#endif

	/* CONSTCOND */
	DO_WRAPUP(-1, &resultp->dr_err, "unsafe devices complete");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * get_cpu0()
 * ------------------------------------------------------
 *
 * RPC routine that queries the processor ID of cpu0
 *
 * Input: 	Note that no arguments are passed to this routine, but the RPC
 * 		dispatcher always passes two arguments.
 *
 * Description:	The routine has no arguments to verify and no policy checks to
 *		make, so it just performs the query at the next layer, returning
 *		the results of that query.
 *
 * Results:	The cpu0 processor id, or -1 in case of error.
 */
/* ARGSUSED */
cpu0_statusp_t *
#ifdef	_XFIRE
get_cpu0_4(void *argp, reqp_t rqstp)
#endif
{
	static cpu0_statusp_t	resultp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP0(DR_RPC_GETCPU0, xdr_cpu0_statusp_t, cpu0_statusp_t, \
		cpu0_status_t, "get cpu0\n");

#ifdef NODAEMON
	resultp->cpu0 = debug_newcpu0;
#else
	resultp->cpu0 = dr_get_cpu0();
#endif

	/* CONSTCOND */
	DO_WRAPUP(-1, &resultp->dr_err, "get cpu0 complete");
	return (&resultp);
}

#ifdef _XFIRE
/*
 * ------------------------------------------------------
 * get_sysbrd_info()
 * ------------------------------------------------------
 *
 * RPC routine that configures the OS after a attaching or detaching boards.
 *
 * Input:	Note that no arguments are passed to this routine, but the RPC
 *		dispatcher always passes two arguments.
 *
 * Description:
 *
 * Results:
 *
 */
/* ARGSUSED */
sysbrd_infop_t *
#ifdef	_XFIRE
get_sysbrd_info_4(void *argp, reqp_t rqstp)
#endif
{
	static sysbrd_infop_t	resultp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP0(NULL, xdr_sysbrd_infop_t, sysbrd_infop_t, sysbrd_info_t, \
		"get sysbrd info\n");

#ifdef NODAEMON
	for (i = 0; i < MAX_BOARDS; i++) {
		resultp->brd_addr[i] = i;
	}
#else
	dr_get_sysbrd_info(resultp);
#endif

	/* CONSTCOND */
	DO_WRAPUP(-1, &resultp->dr_err, "get sysbrd info complete");

	return (&resultp);
}
#endif /* _XFIRE */

/*
 * ------------------------------------------------------
 * run_autoconfig()
 * ------------------------------------------------------
 *
 * RPC routine that configures the OS after a attaching or detaching boards.
 *
 * Description:	Verify that there are no pending DR operations, and if not
 *		pass the arguments to the next level to actually configure
 *		the system.
 *
 * Results:	If there are no pending DR operations, the system is
 *		reconfigured.
 */
/* ARGSUSED */
dr_errp_t *
#ifdef	_XFIRE
run_autoconfig_4(void *argp, reqp_t rqstp)
#endif
{
	static dr_errp_t	resultp;
	int			i;
	sfdr_state_t		state;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP0(NULL, xdr_dr_errp_t, dr_errp_t, dr_err_t, \
		"run autoconfig\n");

	/*
	 * Verify that this RPC routine is valid at this point in time (that
	 * is, make sure no DR operations are pending)
	 */
	for (i = 0; i < MAX_BOARDS; i++) {

		state = get_dr_state(i);

		if (state != SFDR_STATE_PARTIAL &&
		    state != SFDR_STATE_CONFIGURED &&
		    state != SFDR_STATE_EMPTY) {

			dr_logerr(DRV_FAIL, EINVAL,
			    "Complete pending DR operation prior "
			    "to running autoconfig.");
			break;
		}
	}

	/*
	 * The RPC call is valid if all boards were iterated through in the
	 * previous loop.  Pass the arguments to the next level and do the
	 * actual configuration if it's valid.
	 */
	if (i >= MAX_BOARDS) {
#ifndef NODAEMON
		autoconfig();
#endif NODAEMON
	}

	/*
	 * Clean up and return the results
	 */
	/* CONSTCOND */
	DO_WRAPUP(-1, resultp, "autoconfig complete");
	return (&resultp);
}

/*
 * ------------------------------------------------------
 * test_control()
 * ------------------------------------------------------
 */
/* ARGSUSED */
test_statusp_t *
#ifdef	_XFIRE
test_control_4(drmsg_t *stringp, reqp_t rqstp)
#endif
{
	static test_statusp_t	resultp;

	/*
	 * Generic RPC routine start-up code
	 */
	DO_STARTUP0(NULL, xdr_test_statusp_t, test_statusp_t, test_status_t, \
		"test control request\n");

#ifdef DR_TEST_CONFIG
	dr_test_control(*stringp, resultp);
#else
	dr_logerr(DRV_FAIL, ENOTSUP, "test control not supported");
#endif DR_TEST_CONFIG

	/* CONSTCOND */
	DO_WRAPUP(-1, &resultp->dr_err, "test_control");
	return (&resultp);
}
