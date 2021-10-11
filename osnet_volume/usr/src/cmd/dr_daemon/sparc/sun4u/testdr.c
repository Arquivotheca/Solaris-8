/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)testdr.c	1.17	98/08/19 SMI"

/*
 * Test driver for the dr daemon.  Command interface to issue
 * each RPC call.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "dr_subr.h"
#ifdef notdef
#include <hswp_ssp.h>		/* from the ssp libraries */
#endif notdef

#define	PRINT_LINE_LENGTH 78

#if defined(TEST_SVC) || defined(TEST_CLNT)
/*
 * there are times we'd like to toggle the daemon's verbose messages
 * off.
 */
extern int verbose;
#endif

/*
 * An external usually defined by dr_daemon_main.c which testdr.c is
 * replacing.
 */
int noapdaemon = 0;

/*
 * needed by test version of dr_daemon_proc.c
 */
FILE *test_log_file = stdout;

/*
 * echo is set when we're talking with a tty.  If driven by a
 * test script.  Don't output command prompts.
 */
static int echo;

#ifdef DR_TEST_CONFIG
jmp_buf test_setjmp_buf;
#endif DR_TEST_CONFIG

/*
 * Prototypes for the RPC call routines
 */
typedef void *reqp_t;	/* temp for single proc testing */
#define	DRCLIENT (void *)NULL

extern dr_errp_t *get_board_state_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *initiate_attach_board_3(brd_init_attach_t *argp,
					    reqp_t rqstp);
extern dr_errp_t *abort_attach_board_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *complete_attach_board_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *attach_finished_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *detachable_board_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *drain_board_resources_3(int *boardp, reqp_t rqstp);
extern detach_errp_t *detach_board_3(brd_detach_t *argp, reqp_t rqstp);
extern dr_errp_t *abort_detach_board_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *cpu0_move_finished_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *detach_finished_3(int *boardp, reqp_t rqstp);
extern dr_errp_t *run_autoconfig_3(void *null, reqp_t rqstp);

extern board_configp_t *get_obp_board_configuration_3(int *boardp,
						    reqp_t rqstp);
extern attached_board_infop_t *get_attached_board_info_3(brd_info_t *argp,
							    reqp_t rqstp);
extern unsafe_devp_t *unsafe_devices_3(void *argp, reqp_t rqstp);
extern cpu0_statusp_t *get_cpu0_3(void *argp, reqp_t rqstp);

/*
 * Forward declarations
 */
int get_cmd(const char *prompt);
char *get_cmd_str(const char *prompt);
void print_derr(char *rcp, dr_errp_t erp);
void print_obp_config(board_configp_t bcp);
void print_unsafe_devices(unsafe_devp_t udp);
char *state_string(dr_board_state_t state);
void print_board_info(attached_board_infop_t abp);
void print_controller_info(int slot, sbus_cntrlp_t scp);
void print_device_info(sbus_devicep_t sdp);
void print_proclist(int cnt, proclist_t procs[], char beg[]);
#ifdef notdef
void print_hswperr(struct hswperr_t *hsep);
#endif notdef

void
main(int argc, char **argv)
{
	int 		selection, board;
	char 		*cmd;

	dr_errp_t			*erp;
	board_configp_t			*bcp;
	attached_board_infop_t		*abp;
	unsafe_devp_t			*udp;
	cpu0_statusp_t			*c0p;
	detach_errp_t			*detach_erp;

	brd_init_attach_t		brd_init;
	brd_detach_t			brd_detach;
	brd_info_t			brd_info;

#if defined(DR_TEST_CONFIG) && !defined(TEST_SVC)
	/*
	 * Open and read the testconfig file if compiled in this
	 * mode.  If we're linked with a test driver, defer to
	 * interactive setting of the errors below.
	 */
	if (argc > 1) {
		dr_loginfo("processing config file [%s]", argv[1]);
		if (process_dr_config(argv[1]))
			dr_loginfo("config file [%s] error", argv[1]);
	} else {
		dr_loginfo("no config file");
	}
#endif defined(DR_TEST_CONFIG) && !defined(TEST_SVC)

	if (isatty(0))
		echo = 1;
	else
		echo = 0;

	/* output menu on screen */
	if (echo) printf("\n\t\t DAEMON TEST DRIVER\n");

#ifdef DR_TEST_CONFIG
	/* save for longjmp */
	if (selection = setjmp(test_setjmp_buf)) {
		dr_loginfo("Test crash of 0x%x", selection);
	}
#endif DR_TEST_CONFIG

menu:
	fflush(stdin);
	if (echo) {
		printf("%s\n%s\n%s\n%s\n%s\n",
"0) board state   1) init attach     2) complete attach  3) APPL attach",
"4) abort attach  5) detachable?     6) drain board      7) complete detach",
"8) APPL detach   9) APPL cpu0 move 10) abort detach    11) obp config",
"12) board info   13) unsafe devices 14) get cpu0        15) autoconfig",
"16) test errors");
	}
	putchar('\n');

	selection = get_cmd("ENTER 0-17 for command:");

	/*
	 * Don't need a board for board states, cpu0, unsafe devices,
	 * autoconfig, and setting error words
	*/
	if (selection != 0 && selection < 13) {
		board = get_cmd("ENTER board number:");
	}
	switch (selection) {

	/* DISPLAY BOARD STATE */
	case 0:
#if defined(TEST_SVC) || defined(TEST_CLNT)
		selection = verbose;
		verbose = 0;
#endif defined(TEST_SVC) || defined(TEST_CLNT)
		putchar('\n');
		for (board = 0; board < MAX_BRD_PER_SYS; board++) {

		    erp = get_board_state_3(&board, DRCLIENT);

		    if (erp != NULL) {
			printf("%2d: %-15s", board,
			    ((*erp)->state == DR_NO_BOARD)?
			    "" : state_string((*erp)->state));
			if (((board+1) % 4) == 0)
			    printf("\n");
			xdr_free(xdr_dr_errp_t, (char *)erp);
		    } else {
			printf("\n\tget_board_state: bad rpc call (board %d)\n",
				board);
		    }
		}
#if defined(TEST_SVC) || defined(TEST_CLNT)
		verbose = selection;
#endif defined(TEST_SVC) || defined(TEST_CLNT)
		break;

	/* INITIATE ATTACH */
	case 1:
		brd_init.board_slot = board;
		printf("\n\t**** HAVE YOU RUN POST ON THE BOARD?? ****\n\n");
		brd_init.cpu_on_board = get_cmd("ENTER debut proc for board:");
		printf("\tinitiate_attach_board: board %d refproc %d\n\n",
			brd_init.board_slot, brd_init.cpu_on_board);

		erp = initiate_attach_board_3(&brd_init, DRCLIENT);

		if (erp != NULL) {
			print_derr("initiate_attach_board", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tinitiate_attach_board: bad rpc call\n");
		}
		break;

	/* COMPLETE ATTACH */
	case 2:
		printf("\tcomplete_attach_board: board %d\n\n", board);

		erp = complete_attach_board_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("complete_attach_board", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tcomplete_attach_board: bad rpc call\n");
		}
		break;

	/* ATTACH FINISHED  */
	case 3:
		printf("\tattach_finished: board %d\n\n", board);

		erp = attach_finished_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("attach_finished", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tattach_finished: bad rpc call\n");
		}
		break;

	/* ABORT ATTACH */
	case 4:
		printf("\tabort_attach_board: board %d\n\n", board);

		erp = abort_attach_board_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("abort_attach_board", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tabort_attach_board: bad rpc call\n");
		}
		break;

	/* DETACHABLE BOARD */
	case 5:
		printf("\tdetachable_board: board %d\n\n", board);

		erp = detachable_board_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("detachable_board", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tdetachable_board: bad rpc call\n");
		}
		break;

	/* DRAIN BOARD */
	case 6:
		printf("\tdrain_board_resources: board %d\n\n", board);

		erp = drain_board_resources_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("drain_board_resources", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tdrain_board_resources: bad rpc call\n");
		}
		break;

	/* COMPLETE DETACH */
	case 7:
		brd_detach.board_slot = board;
		brd_detach.force_flag =
			get_cmd("ENTER quiesce force flag (0/1):");
		printf("\tdetach_board: board %d force flag %d\n\n",
			brd_detach.board_slot, brd_detach.force_flag);

		detach_erp = detach_board_3(&brd_detach, DRCLIENT);

		if (erp != NULL) {
			print_derr("detach_board", &(*detach_erp)->dr_err);
#ifdef notdef
			print_hswperr((*detach_erp)->dr_hswperr_buff);
#endif notdef
			xdr_free(xdr_dr_errp_t, (char *)detach_erp);
		} else {
			printf("\n\tdetach_board: bad rpc call\n");
		}
		break;

	/* DETACH FINISHED */
	case 8:
		printf("\tdetach_finished: board %d\n\n", board);

		erp = detach_finished_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("detach_finished", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tdetach_finished: bad rpc call\n");
		}
		break;

	/* CPU0 MOVE FINISHED */
	case 9:
		printf("\tcpu0_move_finished: board %d\n\n", board);

		erp = cpu0_move_finished_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("cpu0_move_finished", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tcpu0_move_finished: bad rpc call\n");
		}
		break;

	/* ABORT DETACH */
	case 10:
		printf("\tabort_detach_board: board %d\n\n", board);

		erp = abort_detach_board_3(&board, DRCLIENT);

		if (erp != NULL) {
			print_derr("abort_detach_board", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\tabort_detach_board: bad rpc call\n");
		}
		break;

	/* GET OBP CONFIG */
	case 11:
		printf("\tget_obp_board_configuration: board %d\n\n", board);

		bcp = get_obp_board_configuration_3(&board, DRCLIENT);

		if (bcp != NULL) {
		    print_derr("get_obp_board_configuration", &(*bcp)->dr_err);
		    print_obp_config(*bcp);
		    xdr_free(xdr_board_configp_t, (char *)bcp);
		} else {
		    printf("\n\tget_obp_board_configuration: bad rpc call\n");
		}
		break;

	/* GET BOARD INFO */
	case 12:
		brd_info.board_slot = board;
		brd_info.flag = get_cmd(
"ENTER flag mask(cpu=1,io=2,mem_config=4,mem_cost=8,mem_drain=16):");
		printf("\tget_attached_board_info: board %d flag %d\n\n",
			brd_info.board_slot, brd_info.flag);

		abp = get_attached_board_info_3(&brd_info, DRCLIENT);

		if (abp != NULL) {
			print_derr("get_attached_board_info", &(*abp)->dr_err);
			print_board_info(*abp);
			xdr_free(xdr_attached_board_infop_t, (char *)abp);
		} else {
			printf("\n\tget_attached_board_info: bad rpc call\n");
		}
		break;

	/* UNSAFE DEVICES */
	case 13:
		printf("\tunsafe_devices:\n\n");

		udp = unsafe_devices_3(NULL, DRCLIENT);

		if (udp != NULL) {
			print_derr("unsafe_devices", &(*udp)->dr_err);
			print_unsafe_devices(*udp);
			xdr_free(xdr_unsafe_devp_t, (char *)udp);
		} else {
			printf("\n\tunsafe_devices: bad rpc call\n");
		}
		break;

	case 14:
		printf("\tget_cpu0:\n\n");

		c0p = get_cpu0_3(NULL, DRCLIENT);

		if (c0p != NULL) {
			print_derr("get_cpu0", &(*c0p)->dr_err);
			printf("Cpu0 is %d\n", (*c0p)->cpu0);
			xdr_free(xdr_cpu0_statusp_t, (char *)c0p);
		} else {
			printf("\n\tget_cpu0: bad rpc call\n");
		}
		break;

	/* run autoconfig */
	case 15:
		erp = run_autoconfig_3(NULL, DRCLIENT);

		if (erp != NULL) {
			print_derr("run_autoconfig", *erp);
			xdr_free(xdr_dr_errp_t, (char *)erp);
		} else {
			printf("\n\trun_autoconfig: bad rpc call\n");
		}
		break;

#if defined(TEST_SVC) && defined(DR_TEST_CONFIG)
	/* Set debug Error words */
	case 16:
		cmd = get_cmd_str("ENTER error string: ");
		if (strcmp(cmd, "none") == 0) {
			dr_testioctl = dr_testconfig = 0;
		}
		else
			set_test_config(cmd);
		printf("test: ioctl = 0x%x config = 0x%x errno = %d\n",
			dr_testioctl, dr_testconfig, dr_testerrno);
		break;
#endif  defined(TEST_SVC) && defined(DR_TEST_CONFIG)

	default:
		printf("\tInvalid command input (%d)\n", selection);
	}

	goto menu;
}


int
get_cmd(const char *prompt)
{
	/* Get an integer from stdin */
	int 	n = -1;
	char    buf[80];

	while (1) {
		if (echo) printf(prompt);
		fflush(stdout);
		buf[0] = '\0';

		if (gets(buf) == (char *)NULL) {
			if (feof(stdin)) {
				printf("Exit due to EOF on input\n");
				exit(0);
			}
			printf("Try again ...\n");

		} else if (buf[0] == '#') {
			/* comment line */
			puts(buf);

		} else if (sscanf(buf, "%d", &n) <= 0) {
			printf("Try again ...\n");

		} else {
			break;
		}
	}
	return (n);
}

char *
get_cmd_str(const char *prompt)
{
	/* Get a string from stdin */
	static char    buf[80];

	while (1) {
		if (echo) printf(prompt);
		fflush(stdout);
		buf[0] = '\0';
		if (gets(buf) == (char *)NULL) {
			if (feof(stdin)) {
				printf("Exit due to EOF on input\n");
				exit(0);
			}
			printf("Try again ...\n");
			continue;

		} else if (buf[0] == '#') {
			/* comment line */
			puts(buf);
			continue;
		}

	break;
	}
	return (buf);
}

void
print_derr(char *rcp, dr_errp_t erp)
{
	char *state;

	state = state_string(erp->state);
	printf("\n\t%s: %s %d %d '%s'\n", rcp, state,
		erp->err, erp->errno, erp->msg);
}

char *
state_string(dr_board_state_t brd_state)
{
	static char undefstr[30];
	char *state;

	switch (brd_state) {
	case DR_ERROR_STATE:
		state = "DR_ERROR_STATE";
		break;
	case DR_NULL_STATE:
		state = "DR_NULL_STATE";
		break;
	case DR_NO_BOARD:
		state = "DR_NO_BOARD";
		break;
	case DR_PRESENT:
		state = "DR_PRESENT";
		break;
	case DR_ATTACH_INIT:
		state = "DR_ATTACH_INIT";
		break;
	case DR_OS_ATTACHED:
		state = "DR_OS_ATTACHED";
		break;
	case DR_IN_USE:
		state = "DR_IN_USE";
		break;
	case DR_DRAIN:
		state = "DR_DRAIN";
		break;
	case DR_CPU0_MOVED:
		state = "DR_CPU0_MOVED";
		break;
	case DR_OS_DETACHED:
		state = "DR_OS_DETACHED";
		break;
	case DR_BOARD_WEDGED:
		state = "DR_BOARD_WEDGED";
		break;
	case DR_INIT_ATTACH_IP:
		state = "DR_INIT_ATTACH_IP";
		break;
	case DR_COMPLETE_ATTACH_IP:
		state = "DR_COMPLETE_ATTACH_IP";
		break;
	case DR_DRAIN_IP:
		state = "DR_DRAIN_IP";
		break;
	case DR_DETACH_IP:
		state = "DR_DETACH_IP";
		break;
	case DR_DETACH_IP_2:
		state = "DR_DETACH_IP_2";
		break;
	default:
		(void) sprintf(undefstr, "<undef=%d>", brd_state);
		state = undefstr;
	}

	return (state);
}


/* ==================================== */

typedef struct drmem_range {
	unsigned int	l_pfn;
	unsigned int	h_pfn;
} drmem_range_t;

/*
 * Routines which determine physical memory ranges
 */
int
range_cmp(const void *r1, const void *r2)
{
	return (((drmem_range_t *)r1)->l_pfn - ((drmem_range_t *)r2)->l_pfn);
}

int
get_mem_ranges(mem_config_t mqh[], drmem_range_t memrange[], int nrange)
{
	int	g, m;
	int	count = 0;

	if (nrange <= 0)
		return (0);

	memset((caddr_t)memrange, 0, nrange * sizeof (drmem_range_t));

	for (m = 0; m < MAX_MQH_PER_BRD; m++) {
		for (g = 0; g < MAX_GRP_PER_MQH; g++) {
			if (merge_mem_range(memrange,
					    count,
					    mqh[m].group[g].l_pfn,
					    mqh[m].group[g].h_pfn))
				continue;
			/*
			 * Couldn't merge with an existing range.
			 */
			memrange[count].l_pfn = mqh[m].group[g].l_pfn;
			memrange[count].h_pfn = mqh[m].group[g].h_pfn;
			count++;
		}
	}

	if (count)
		qsort((caddr_t)memrange, count, sizeof (drmem_range_t),
			range_cmp);

	return (count);
}

int
merge_mem_range(drmem_range_t memrange[], int count, u_int l_pfn, u_int h_pfn)
{
	int	i;

	if ((count <= 0) || (h_pfn < l_pfn))
		return (0);

	for (i = 0; i < count; i++) {
		if ((memrange[i].l_pfn <= h_pfn) &&
		    (memrange[i].h_pfn >= l_pfn)) {
			if (l_pfn < memrange[i].l_pfn)
				memrange[i].l_pfn = l_pfn;
			if (h_pfn > memrange[i].h_pfn)
				memrange[i].h_pfn = h_pfn;
			return (1);
		}
	}

	return (0);
}
/* ==================================== */

void
print_memory(int board, board_mem_config_t *mp)
{
	int i, n;
	char *begstr;
	drmem_range_t	mr[MAX_GRP_PER_MQH * MAX_MQH_PER_BRD];

	/*
	 * Print system memory info first
	 */
	printf("\n\t\tSystem Memory Sizes in MBytes\n\n");

	printf("%s%-12d%s%d\n%s%-12d%s%s\n",
		"Current System     = ", mp->sys_mem,
		" Attach Capacity    = ", mp->dr_max_mem-mp->sys_mem,
		"dr-max-mem         = ", mp->dr_max_mem,
		" dr-mem-detach      = ", mp->dr_mem_detach);

	/*
	 * Now print information specific to the board configuration
	 */
	printf("\n\t\tMemory Configuration for Board %d\n\n", board);

	printf("Memory Size        = %d MBytes\n", mp->mem_size);

	/* mem_if is zero if we've gotten the mem config prior to attach */
	if (mp->mem_size != 0 && mp->mem_if != 0) {

		printf("Interleave Factor  = %d boards\n", mp->mem_if);
		printf("Interleave Boards  =");
		for (i = 0; i < MAX_BRD_PER_SYS; i++) {
			if (mp->mem_board_mask & 0x1)
				printf(" %d", i);
			mp->mem_board_mask >>= 1;
		}
		putchar('\n');

		/* Find the physical memory ranges */
		n = get_mem_ranges(mp->mem_mqh, mr,
					MAX_GRP_PER_MQH*MAX_MQH_PER_BRD);
		begstr = "Physical Pages     = ";

		for (i = 0; i < n; i++) {
			printf("%s[%d-%d]\n", begstr, mr[i].l_pfn, mr[i].h_pfn);
			begstr = "                     ";
		}
	}
}

/*
 * print_mem_detach
 *
 * Memory detach status may have a cost or drain or both structures to
 * display.  Displaying the drain structure only makes sense
 * while the drain operation is in progress.  This routine assumes that
 * the drain operation has already started and therefore the drain structure
 * contains valid values if the cost structure is not provided.
 *
 * Input:
 *	board - board which info is printed for
 *	sys_mem - if non zero, the current size of system memory
 *	mcp - pointer to the cost structure (may be null)
 *	mdp - pointer to the drain structure (may be null)
 */
void
print_mem_detach(int board, int sys_mem, board_mem_costp_t mcp,
		    board_mem_drainp_t mdp)
{
	char *status;
	char	buf[100];
	int	drain_started;
	int	mem_detach;
	struct tm *tp;

	/*
	 * Determine the status of the detach/drain operation.
	 * Note that the cost structure is not provided, we assume
	 * that the drain operation has already started.
	 */
	drain_started = 1;
	status = "IN PROGRESS";
	if (mcp) {
		if (!mcp->actualcost) {
			status = "ESTIMATED";
			drain_started = 0;
		}
	}

	/* Drain status only valid if drain operation is in progress */
	if (mdp && drain_started) {
		if (mdp->mem_kb_left == 0)
			status = "COMPLETE";
	}

	printf("\n\t\tMemory Drain for Board %d - %s\n\n", board,
		status);

	if (mcp) {
		mem_detach = mcp->mem_pshrink + mcp->mem_pdetach;
		printf("Reduction (Board + Lost) = %d (%d + %d) MBytes\n",
			mem_detach,  mcp->mem_pdetach, mcp->mem_pshrink);
		if (sys_mem > 0)
			printf("Remaining in System	 = %d MBytes\n",
				sys_mem - mem_detach);
	}

	if (!mdp || !drain_started)
		/* Nothing more to print */
		return;

	printf("Percent Complete         = %d%% (%d KBytes remaining)\n\n",
		mdp->mem_drain_percent, mdp->mem_kb_left);

	/*
	 * Now print the time the drain started and the current time
	 */
	tp = localtime(&mdp->mem_drain_start);
	strftime(buf, sizeof (buf), (char *)NULL, tp);
	printf("Drain operation started at %s\n", buf);

	tp = localtime(&mdp->mem_current_time);
	strftime(buf, sizeof (buf), (char *)NULL, tp);
	printf("Current time		   %s\n", buf);
}

void
print_obp_config(board_configp_t bcp)
{
	int i;
	sbus_configp_t scp;
	int header_printed;

	printf("\n\t\tDevices Present on Board %d\n\n", bcp->board_slot);

	if (bcp->cpu_cnt <= 0) {
		printf("No cpus present on the board.\n");
	} else {
		printf("%s\n%s\n",
			"Cpu  Frequency (MHz)  Cache Size (MBytes)"
			"---  --------------   -------------------");

		for (i = 0; i < bcp->cpu_cnt; i++) {
			printf("%3d       %3d              %3d\n",
				bcp->cpu[i].cpuid,
				bcp->cpu[i].frequency,
				bcp->cpu[i].ecache_size);
		}
	}

	print_memory(bcp->board_slot, &bcp->mem);

	header_printed = 0;
	for (i = 0; i < MAX_SBUS_SLOTS_PER_BRD; i++) {
		if (!header_printed) {
			printf("\nSlot\tController\n----\t----------\n");
			header_printed = 1;
		}
		if (bcp->sbus_ctrlr[i]) {
			printf("%3d", i);
			scp = bcp->sbus_ctrlr[i];
			while (scp) {
				printf("\t%s\n", scp->name);
				scp = scp->next;
			}
		}
	}
	if (!header_printed)
		printf("\nNo sbus devices on the board.\n");
}

void
print_board_info(attached_board_infop_t abp)
{
	board_cpu_configp_t	cpu;
	sbus_cntrlp_t scp;
	int i;

	putchar('\n');

	if ((abp->flag & BRD_CPU) && abp->cpu) {

		cpu = abp->cpu;

		printf("\n\t\tCpu Configuration for Board %d\n\n",
			abp->board_slot);
		printf("%s\n%s\n%s\n",
		    "	        P a r t n     B o u n d   T h r e a d s",
		    "cpu  status  |  id  count  |  user  sys  procs",
		    "---  ------  |  --  -----  |  ----  ---  -----");

		for (i = 0; i < cpu->cpu_cnt; i++) {

			printf("%3d  %-7s |  %2d    %2d   |  %3d   %3d ",
				cpu->cpu[i].cpuid, cpu->cpu[i].cpu_state,
				cpu->cpu[i].partition, cpu->cpu[i].partn_size,
				cpu->cpu[i].num_user_threads_bound,
				cpu->cpu[i].num_sys_threads_bound);

			print_proclist(cpu->cpu[i].proclist.proclist_len,
				cpu->cpu[i].proclist.proclist_val,
				"             |             |            ");
		}
		printf("\nCpu %d is the bootproc.\n", cpu->cpu0);
	}

	if ((abp->flag & BRD_MEM_CONFIG) && abp->mem_config) {
		print_memory(abp->board_slot, abp->mem_config);
	}

	if (abp->flag & (BRD_MEM_COST|BRD_MEM_DRAIN)) {
		print_mem_detach(abp->board_slot,
				(abp->mem_config)? abp->mem_config->sys_mem : 0,
				abp->mem_cost, abp->mem_drain);
	}

	if (abp->flag & BRD_IO) {
		int did_print;

		printf("\n\t\tSBUS Controllers and Devices for Board %d\n\n",
			abp->board_slot);

		did_print = 0;
		for (i = 0; i < MAX_SBUS_SLOTS_PER_BRD; i++) {

			scp = abp->sbus_slot[i];
			while (scp) {
				did_print = 1;
				print_controller_info(i, scp);
				scp = scp->next;
			}
		}
		if (did_print == 0) {
			printf("No devices present.\n");
		}
	}
}

/*
 * print_proclist
 *
 * Assume that first strlen(beg) chars have already been output.
 * add procs to line until PRINT_LINE_LENGTH is exceeded.  Do a new
 * line.  print beg and start adding procs again.
 *
 * beg is a zero terminated string, 1 char short of where the first
 * proc should be displayed.
 */

void
print_proclist(int cnt, proclist_t procs[], char beg[])
{
	int	len, i, n, did_print;
	char	line[PRINT_LINE_LENGTH], procstr[10];

	len = strlen(beg);
	line[0] = 0;
	did_print = 0;
	if (len > PRINT_LINE_LENGTH) {
		printf("Line not long enough to display procs");
		return;
	}

	for (i = 0; i < cnt; i++) {

		n = sprintf(procstr, " %d", procs[i].pid);

		if ((len + n) > PRINT_LINE_LENGTH) {
			printf("%s\n", line);
			did_print = 1;
			strcpy(line, beg);
			len = strlen(beg);
		}
		strcat(line, procstr);
		len += n;
	}

	if (strlen(line) > 0) {
		printf("%s\n", line);
	} else if (did_print == 0) {
		putchar('\n');
	}
}
/*
 * print_controller_info
 */
void
print_controller_info(int slot, sbus_cntrlp_t scp)
{
	int i, len;
	sbus_devicep_t sdp;
	char dash[PRINT_LINE_LENGTH+1];
	char title[PRINT_LINE_LENGTH];

	/* If null, nothing to do */
	if (scp == NULL) {
		return;
	}

	/* line of dashes for formatting for */
	for (i = 0; i < PRINT_LINE_LENGTH; i++) {
		dash[i] = '-';
	}
	dash[PRINT_LINE_LENGTH] = 0;

	if (scp->ap_info.is_alternate) {
		if (scp->ap_info.is_active)
			len = sprintf(title,
				" Slot %d : %s : active AP controller ",
				slot, scp->name);
		else
			len = sprintf(title,
				" Slot %d : %s : inactive AP controller ",
				slot, scp->name);
	} else {
		if (scp->name)
			len = sprintf(title, " Slot %d : %s ", slot, scp->name);
		else
			len = sprintf(title, " Slot %d ", slot);
	}

	/* Now center the title in a sea of dashes */
	i = (PRINT_LINE_LENGTH - len)/2;
	if ((2*i+len) != PRINT_LINE_LENGTH) {
		/* add a blank to the end of the title string */
		title[len] = ' ';
		title[len+1] = 0;
	}
	i = PRINT_LINE_LENGTH - i;
	printf("%s%s%s\n", &dash[i], title, &dash[i]);

	if (scp == NULL)
		return;

	printf("\n%s\n%s\n",
		"device    opens  name                  usage",
		"------    -----  ----                  -----");

	sdp = scp->devices;
	while (sdp) {
		print_device_info(sdp);
		sdp = sdp->next;
	}
	putchar('\n');
}

/*
 * print_device_info
 */
void
print_device_info(sbus_devicep_t sdp)
{
	sbus_usagep_t	up;
	char	devname[11];
	char *begstr;

	/* print devices on that controller  This is the beginning string */
	sprintf(devname, "%-10.10s", sdp->name);
	begstr = devname;

	/* now print usage of the device */
	up = sdp->usage;
	if (up != NULL) {
		while (up) {

			if (up->usage_count < 0)
				printf("%s       %-20s  %s\n", begstr,
				(up->name == NULL) ? "" : up->name,
				(up->opt_info == NULL) ? "" : up->opt_info);
			else
				printf("%s%4d   %-20s  %s\n",
					begstr, up->usage_count,
					(up->name == NULL) ? "" : up->name,
					(up->opt_info == NULL) ?
						"" : up->opt_info);
			begstr = "          ";
			up = up->next;
		}
	} else {
		/* device, no usage records */
		printf("%s%4d\n", devname, 0);
	}
}

void
print_unsafe_devices(unsafe_devp_t udp)
{
	int i;

	printf("\t\tUnsafe Devices\n\n");

	if (udp->unsafe_devs.unsafe_devs_len <= 0) {
		printf("No unsafe devices currently open.\n");
	} else {
		printf("Unsafe devices which are currently open:\n");

		for (i = 0; i < udp->unsafe_devs.unsafe_devs_len; i++) {
			printf("\t%s\n", udp->unsafe_devs.unsafe_devs_val[i]);
		}
	}
}

#ifdef notdef
void
print_hswperr(struct hswperr_t *hsep)
{
	char		buffer[256];
	int reason;

	prep_hswperr_msg(hsep, buffer);
	reason = check_hswperr(hsep, buffer);

	printf("%s", buffer);
}
#endif notdef
