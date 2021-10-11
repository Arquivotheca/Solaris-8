/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_config_cost.c	1.53	99/08/25 SMI"

/*
 * This file implements the Dynamic Reconfiguration Board info
 * functions.
 */

#include <malloc.h>
#include <errno.h>
#include <sys/cpuvar.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <kstat.h>		/* get_cpu_states() uses  this kstat API */
#include <sys/pset.h> 		/* dr_get_partn_cpus() uses the pset struff */
#include <dirent.h>		/* dr_get_cpu_bindings() uses the directory */
#include <sys/fcntl.h>		/* 	stuff, the file control stuff, and */
#include <procfs.h>		/*	the /proc filesystem stuff from here */

#include "dr_subr.h"

#define	MEGABYTE		0x100000
#define	KILOBYTE 		0x400
#define	MAX_COST_CPU_PIDS 	40
#define	EIGHTGIGS		0x200000000ULL
#define	SIXTYFOURK		0x10000U
#define	PROCDIR			"/proc"
#define	LWPDIR			"lwp"
#define	LWPSINFO		"lwpsinfo"
#define	cpustate		d_cpu.cs_dstate
#define	ntype			d_common.c_type
#define	basepfn			d_mem.ms_basepfn
#define	totpages		d_mem.ms_totpages
#define	detpages		d_mem.ms_detpages
#define	noreloc_pages		d_mem.ms_noreloc_pages
#define	cage_enabled		d_mem.ms_cage_enabled

/* Forward declarations */
static board_cpu_configp_t get_cpu_info(int board);
static int dr_page_to_mb(int pages);
static int dr_page_to_kb(int page);
static int percent_complete(int total, int done);
static board_mem_costp_t get_mem_cost(int board);
static board_mem_drainp_t get_mem_drain(int board);
static int dr_get_cpu_states(board_cpu_configp_t cp, int *cputabl);
static void dr_get_cpu_bindings(board_cpu_configp_t cp, int *cputabl);
static void add_binding(cpu_info_t *cpu, lwpsinfo_t *info, processorid_t pid);
static int dr_get_sys_pages(void);

int irint(double x);

/*
 * ----------------------------------------------------------
 * dr_get_cpu0
 * ----------------------------------------------------------
 *
 * Routine needed by the cpu config code below and also the
 * detach routines to determine which cpu is cpu0.
 *
 * Also, this is the implementation layer of the get_cpu0() RPC routine
 *
 * Input: 	none
 *
 * Description:	Iterate through each board, querying its state.  If its state
 *		is non-empty, then read its CPU status searching for CPU0.  If
 *		CPU0 is there, the search is done.
 *
 * Output: 	cpu0 number or -1 if an error
 */
int
dr_get_cpu0()
{
	sfdr_stat_t	stat;
	int		board;
	int		i;

	/* Search every board until cpu0 is found.  */
	for (board = 0; board < MAX_BOARDS; board++) {

		/* Query the board's status (DR_CMD_STATUS) */
		memset((caddr_t)&stat, 0, sizeof (sfdr_stat_t));
		if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
			(void *)&stat, NULL))
			continue;

		/* If the board's state is non-empty, */
		if (stat.s_bstate != SFDR_STATE_EMPTY) {

			/* Iterate through all of its dev_stat nodes: */
			for (i = 0; i < stat.s_nstat; i++) {

				/*
				 * If the dev_stat node is a cpu_stat node
				 * and it's cpu0, return its cpuid
				 */
				if (stat.s_stat[i].ntype != DR_NT_CPU)
					continue;

				if (stat.s_stat[i].d_cpu.cs_isbootproc == TRUE)
					return (stat.s_stat[i].d_cpu.cs_cpuid);
			}
		}
	}

	/*
	 * If the search doesn't return and we get here, the ioctl to the board
	 * with cpu0 failed for some reason or the DR driver is on crack.
	 * Return an error.
	 */
	return (-1);
}

#ifdef _XFIRE

/*
 * ---------------------------------------------------------------
 * dr_get_sysbrd_info
 * ---------------------------------------------------------------
 *
 * Implementation layer of the get_sysbrd_info() RPC routine.
 * Fills in a mapping of board addresses for this domain.
 *
 * Input: 	brd_info	(map to fill in)
 *
 * Description:	Each board in the system has its memory configuration
 *		checked, and its base page frame number loaded.  Any
 *		boards whose memory configuration can't be read are
 *		marked with a -1 in the map.  Boards whose memory
 *		configurations can be read have their board address
 *		put into the map.  The board address is calculated
 *		as follows:
 *					    base page frame number
 *			board address =  ---------------------------
 *					   8 gig / System Page Size
 *
 * Results:	The filled in map is returned.
 */
void
dr_get_sysbrd_info(sysbrd_infop_t brd_info)
{
	board_mem_config_t	mcp;
	long			pagesize;
	long			boundary;
	register int		board;

	pagesize = sysconf(_SC_PAGESIZE);
	boundary = (long)(EIGHTGIGS / (u_longlong_t)pagesize);

	/* Sanity check */
	if (brd_info == NULL) {
		dr_loginfo("dr_get_sysbrd_info: NULL parameter.");
		return;
	}

	/* For each board in the system... */
	for (board = 0; board < MAX_BOARDS; board++) {

		/*
		 * Try to read the board's memory configuration.
		 * A failure to read it means it's not in the system.
		 */
		memset((caddr_t)&mcp, 0, sizeof (board_mem_config_t));
		if (get_mem_config(board, &mcp) == NULL) {
			brd_info->brd_addr[board] = -1;
			continue;
		}

		/* If the base pfn seems valid, calculate the address */
		if (mcp.mem_pages.h_pfn != 0) {
			brd_info->brd_addr[board] =
				mcp.mem_pages.l_pfn / boundary;
		}
		else
			brd_info->brd_addr[board] = -1;

		/* Free up the string allocated within 'mcp' */
		if (mcp.dr_mem_detach) {
			free(mcp.dr_mem_detach);
			mcp.dr_mem_detach = NULL;
		}
	}
}

#endif /* _XFIRE */

/*
 * ----------------------------------------------------------
 * dr_get_partn_cpus
 * ----------------------------------------------------------
 *
 * Routine needed by the cpu config code below and also the
 * detach routines to determine a cpu's partition info.
 *
 * Input: 	cpuid - the cpuid of to get partition information for
 *		partition - partition number to be retrieved
 *		numcpus - number of cpus in partition to be retrieved
 *
 * Description:	Whether or not the cpu is in a partition is determined with the
 *		pset_assign(PS_QUERY, ...) routine.  If the cpu is in a
 *		partition, then use pset_info() to retrieve all of its partition
 *		details and fill in the input variables as described below.
 *
 * Output: input variables filled in
 *	syscpu[]	cpus which are in the system partiion
 *			undefined if cpuid is not is a partition(i.e. it is
 *			in the kernel's "default" partition).
 *			undefined if error.  (If NULL, the argument is unused.)
 *
 *	numcpus		number of cpus in the partition
 *			0 if cpuid is not in a partition.
 *			undefined if error.
 *	partition	cpu partition id (or set id).
 *			PS_NONE if cpuid is not in a partition.
 *			undefined if error.
 *
 * Function return value: <0, error
 *			  otherwise, 0
 */
int
dr_get_partn_cpus(int cpuid, int *partition, int *numcpus, int *syscpu)
{
	int	type;

	/* first use pset_assign(PS_QUERY, ...) to get the cpu's partition */
	if (pset_assign(PS_QUERY, cpuid, (psetid_t *)partition) != 0) {
		dr_logerr(DRV_FAIL, errno,
			    "dr_get_partn_cpus: cannot get cpu's partition");
		return (-1);
	}

	/* check to see if the cpu is not in a partition */
	if (*partition == PS_NONE) {
		*numcpus = 0;
		return (0);
	}

	/* cpu is in a partition, let's get the info */
	*numcpus = (int)sysconf(_SC_NPROCESSORS_CONF);
	if (pset_info(*partition, &type, (u_int *)numcpus,
	    (processorid_t *)syscpu) != 0) {
		dr_logerr(DRV_FAIL, errno,
		    "dr_get_partn_cpus: failed to get cpu partition info");
		return (-1);
	}

	return (0);
}

/*
 * ----------------------------------------------------------
 * dr_get_attached_board_info
 * ----------------------------------------------------------
 *
 * Implementation layer of the get_attached_board_info() RPC routine
 *
 * Input:	bip->board_slot	(which board to query)
 *		bip->flag	(which board components to query)
 *
 * Description:	All of the flagged components are queried and all the resulting
 *		info is added into a structure with separate substructures for
 *		each component's information.
 *
 * Output:	*bcp [possibly] contains the following items (whichever items
 *		were flagged in bip->flag):
 *
 *			- CPU info (how many cpu's, PID's/Threads associated
 *					with each CPU, processor set
 *					information, etc)
 *			- Memory info (how much there is, if it's detachable)
 *			- Memory costs (how costly detaching memory would be)
 *			- Memory drain (statistics for an in-progress "drain")
 *			- IO info (what i/o devices are installed)
 */
void
dr_get_attached_board_info(brd_info_t *bip, attached_board_infop_t bcp)
{
	/* Sanity */
	if (bip == NULL || bcp == NULL)
		return;

	bcp->board_slot = bip->board_slot;
	bcp->flag = bip->flag;

	if (bip->flag & BRD_CPU) {
		bcp->cpu = get_cpu_info(bip->board_slot);
	}

	if (bip->flag & BRD_MEM_CONFIG) {
		bcp->mem_config = get_mem_config(bip->board_slot, NULL);
	}

	if (bip->flag & BRD_MEM_COST) {
		bcp->mem_cost = get_mem_cost(bip->board_slot);
	}

	if (bip->flag & BRD_MEM_DRAIN) {
		bcp->mem_drain = get_mem_drain(bip->board_slot);
	}

	if (bip->flag & BRD_IO) {
		get_io_info(bip->board_slot, bcp);
	}
}

/*
 * ----------------------------------------------------------
 * get_cpu_info
 * ----------------------------------------------------------
 *
 * Determine the cpu info by querying the kernel and getting the partition info
 * for the cpu.
 *
 * Input:	board number and structure to fill in.
 *
 * Description: This routine is responsible for querying the following info:
 *			- which cpu in the system is cpu0
 *			- how many cpu's are on the given system board
 *			- the id number of each cpu on the given board
 *			- the state of each cpu on the given board
 *			- the processor set number and size, if any, that
 *			  each cpu belongs to
 *			- how many threads (both user and system) are bound
 *			  to each cpu
 *			- a list of pid's corresponding to the bound threads
 *			  of each cpu
 *
 *		The dr_get_cpu0() function figures out which cpu is cpu0.
 *
 *		The DR_CMD_STATUS ioctl gives us how many cpu's are on the
 *		board, and their id numbers.  So, we issue that ioctl and
 *		use its results to complete our information.  First, we
 *		can't assume that there'll be a contiguous range of CPU's on
 *		the board.  So, we make a table large enough to hold all of
 *		the cpu id's in the entire system and initialize it to -1 (to
 *		indicate the absence of a CPU from the system).
 *
 *		Then, we iterate through the CPU nodes returned from the ioctl
 *		with a linear index into our final list of CPU (the final list
 *		that we'll pass back from this function).  When a CPU is found,
 *		the current index into the final list (cp) is stored in the
 *		correct slot of our table, and the node's information is put
 *		into the final list of CPU's.  During this iteration we also
 *		count the number of CPU's on the board.
 *
 *		The status ioctl doesn't gather all the information we need,
 *		and here's where the cpu table really comes in handy.  We pass
 *		our table and the result structure that we're filling in to
 *		the dr_get_cpu_states() and dr_get_cpu_bindings() functions.
 *		They then use their own means to look at all CPU's in the
 *		system and use the table to figure out if the CPU's are on the
 *		board as well as translate the CPU id's to an index into the
 *		final 'cp' structure that's being filled in.
 *
 * Output:	*bcp (see below)
 */
static board_cpu_configp_t
get_cpu_info(int board)
{
	static int		cputabl[MAX_BOARDS*MAX_CPU_UNITS_PER_BOARD];
	sfdr_stat_t		status;
	board_cpu_configp_t	cp;
	register int		index, cp_index;
	register int		cpuid, i;

	/* Verify that the given board is in range */
	if (!BOARD_IN_RANGE(board)) {
		dr_logerr(DRV_FAIL, 0, \
			"get_cpu_info: invalid board number.");
		return (NULL);
	}

	/* Query the status for this board */
	memset((caddr_t)&status, 0, sizeof (sfdr_stat_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
			(void *)&status, DR_IOCTL_CPUCONFIG)) {

		dr_logerr(DRV_FAIL, errno, "get_cpu_info: ioctl failed");
		return (NULL);
	}

	/* Allocate the return structure */
	cp = calloc(1, sizeof (board_cpu_config_t));
	if (cp == NULL) {
		dr_logerr(DRV_FAIL, errno, \
			"malloc failed (board_cpu_config_t)");
		return (NULL);
	}

	/* Get cpu0 since it's quick and easy to get */
	cp->cpu0 = dr_get_cpu0();

	/* Clear our cputabl to -1's (indicating the cpu's aren't present) */
	for (i = 0; i < MAX_BOARDS*MAX_CPU_UNITS_PER_BOARD; i++)
		cputabl[i] = -1;

	/*
	 * Fill in the return structure as much as possible from the retrieved
	 * CPU status information.
	 *
	 * Even though a redundant status query will be made, the dr_get_cpu0()
	 * function is used to get cpu0.  The scope of dr_get_cpu0() is system
	 * wide, which is what we need here.
	 */
	cp->cpu_cnt = 0;
	cp_index = 0;
	for (index = 0; index < status.s_nstat; index++) {
		/* If it's not a CPU node, skip it */
		if (status.s_stat[index].ntype != DR_NT_CPU)
			continue;

		/* Increment our count of CPUs, since we found a CPU node */
		cp->cpu_cnt++;

		/* Mark the cp->cpu index in the cputabl */
		cpuid = status.s_stat[index].d_cpu.cs_cpuid;
		cputabl[cpuid] = cp_index;

		/* Save the cpuid to the cp->cpu array */
		cp->cpu[cp_index].cpuid = cpuid;

		/*
		 * If the cpu is attached to the OS, get its partition info.
		 * Continue if dr_get_partn_cpus() fails.
		 */
		if ((status.s_stat[index].cpustate != SFDR_STATE_CONFIGURED) ||
			(dr_get_partn_cpus(cpuid, \
				&(cp->cpu[cp_index].partition),
				&(cp->cpu[cp_index].partn_size), NULL) < 0)) {

			/* in case of error, assign reasonable values */
			cp->cpu[cp_index].partition = PS_NONE;
			cp->cpu[cp_index].partn_size = 0;
		}

		/*
		 * Initialize the following to benign values, and then search
		 * for them later (the searches later on may fail, thus they're
		 * initialized here).
		 */
		cp->cpu[cp_index].cpu_state = (drmsg_t)strdup("offline");
		cp->cpu[cp_index].num_user_threads_bound = 0;
		cp->cpu[cp_index].num_sys_threads_bound = 0;

		/* We added another cpu, so increment our final list index */
		cp_index++;
	}

	/*
	 * The states of all cpu's in the cp->cpu[] array are "offline."  Try
	 * to get correct state information.  If dr_get_cpu_states() fails,
	 * the states will just stay "offline."
	 */
	if (dr_get_cpu_states(cp, cputabl) != cp->cpu_cnt)
		dr_loginfo("get_cpu_info: cpu state info is incomplete " \
			"[non-fatal].");

	/*
	 * The num_user_threads_bound, num_sys_threads_bound, and proclist
	 * fields of each cp->cpu[] array entry are undefined.  Fill them in.
	 */
	dr_get_cpu_bindings(cp, cputabl);

	/* When done, return the filled-in board_cpu_config_t structure */
	return (cp);
}

/*
 * ----------------------------------------------------------
 * get_mem_config
 * ----------------------------------------------------------
 *
 * Query the memory configuration info for a board
 *
 * Input:	board	(system board number to query)
 *		mcp	(structure that will be filled in with memory info.
 *			 Note: this could be NULL in which case a structure
 *			 will need to be allocated.)
 *
 * Description:	Issuing a DR_CMD_STATUS on the MEM target for the board will
 *		retrieve all of the memory configuration info for the board.
 *
 *		If no mcp structure was provided to fill in, allocate one.
 *
 *		After retrieving the info, it's parsed and consolidated into
 *		the mcp argument.  (I say "consolidated" because there are
 *		multiple memory nodes in the retrieved status, and the returned
 *		info is a summary of all nodes combined.)
 *
 * Output:	ptr to allocated config structure
 */
board_mem_configp_t
get_mem_config(int board, board_mem_configp_t mcp)
{
	sfdr_stat_t		stat;
	sfdr_dev_stat_t		curstat;
	int			i;
	int			sys_pages;
	int			brd_pages;

	/* Retrieve all of the memory information for the board */
	memset((caddr_t)&stat, 0, sizeof (sfdr_stat_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, &stat, NULL)) {
		dr_logerr(DRV_FAIL, errno, "get_mem_config: ioctl failed");
		return (NULL);
	}

	/*
	 * Allocate an 'mcp' structure if necessary.  Note that it's calloc()'ed
	 * so that its values start out at zero.
	 */
	if (mcp == NULL) {
		mcp = calloc(1, sizeof (board_mem_config_t));
		if (mcp == NULL) {
			dr_logerr(DRV_FAIL, errno, \
				    "malloc failed (board_mem_config_t)");
			return (NULL);
		}
	}

	/*
	 * Initialize the board_mem_config_t structure that'll be returned.
	 *
	 * Only add non-zero stuff here, since it's alloc'ed to all zeros to
	 * start with.
	 *
	 * SMC:	dr_min_mem/dr_max_mem values are set to the minimum/maximum
	 * 	memory values for a Starfire.  That's what their meaning has
	 *	been reduced to.
	 */
	if (mcp->dr_mem_detach != NULL) {
		free(mcp->dr_mem_detach);
		mcp->dr_mem_detach = NULL;
	}
	mcp->dr_mem_detach = strdup("1 (enabled)");
	mcp->dr_min_mem = 0;
	mcp->dr_max_mem = SIXTYFOURK;
	mcp->mem_pages.l_pfn = ~((u_int)0);
	mcp->mem_pages.h_pfn = 0;

	/*
	 * Iterate through all of the memory nodes, and consolidate their info.
	 * Assuming there's only one memory node (but looking for multiple ones
	 * just to be safe), the basepfn/totpages consolidation is a little
	 * risky.  That is, if there *are* multiple memory nodes and they have
	 * non-contiguous page ranges, then this code breaks.  But then, how
	 * would old applications expect such memory configurations to be
	 * represented?
	 *
	 * We also look for the ms_cage_enabled flag along the way, and if it's
	 * not set we reset mcp->dr_mem_detach to "0 (disabled)."
	 */
	for (i = 0; i < stat.s_nstat; i++) {

		curstat = stat.s_stat[i];

		/*
		 * If we got a non-memory node in the status structure,
		 * then the driver goofed.  It's okay, though; carry on.
		 */
		if (curstat.ntype != DR_NT_MEM)
			continue;

		/* Calculate the pages for this board */
		brd_pages = curstat.totpages - curstat.detpages;
		if (brd_pages < 0)
			brd_pages = 0;

		/* If this memory node has any pages, add it into our sums */
		if (brd_pages > 0) {

			/* Contribute to high/low page frame numbers */
			if (curstat.basepfn < mcp->mem_pages.l_pfn)
				mcp->mem_pages.l_pfn = curstat.basepfn;
			if (curstat.basepfn + brd_pages > mcp->mem_pages.h_pfn)
				mcp->mem_pages.h_pfn = \
					(curstat.basepfn + brd_pages) - 1;
		}

		/* Add to the running total for memory amounts */
		mcp->mem_size += dr_page_to_mb(brd_pages);

		/* Set "perm_memory" flag if there are non-relocatable pages */
		if (curstat.noreloc_pages != 0)
			mcp->perm_memory = 1;

		/* Reset "dr_mem_detach" flag if the cage isn't enabled */
		if (curstat.cage_enabled == 0) {
			free(mcp->dr_mem_detach);
			mcp->dr_mem_detach = strdup("0 (disabled)");
		}
	}

	/*
	 * The low page frame number mark was set to a maximum value before the
	 * memory node(s) were looked at.  If no memory was found, then
	 * the low page frame number is still set at that maximum value.  It
	 * should be changed to zero to indicate that there is no memory in the
	 * system in this case.
	 */
	if (mcp->mem_pages.l_pfn == ~((u_int)0))
		mcp->mem_pages.l_pfn = 0;

	/*
	 * Use sysconf() to retrieve the total system memory.  If that fails,
	 * try the horrible approach of summing all of the pages from all
	 * memory boards.
	 */
	sys_pages = sysconf(_SC_PHYS_PAGES);
	if (sys_pages == -1) {
		sys_pages = dr_get_sys_pages();
		if (sys_pages == -1) {
			dr_loginfo("get_mem_config: couldn't determine total" \
		"system memory size; only 1 board counted [non-fatal].");
			sys_pages = mcp->mem_size;
		}
	}
	mcp->sys_mem = dr_page_to_mb(sys_pages);

	/* Done */
	return (mcp);
}

/*
 * ----------------------------------------------------------
 * dr_get_cpu_states
 * ----------------------------------------------------------
 *
 * Use the KSTAT library to query the states of the tabulated CPUs
 *
 * Input:	cp		(structure to fill in with state information)
 *		cputabl		(table of which cpu's are installed and what
 *				 their indices in the cp->cpu[] array are if so)
 *
 * Description:	The /dev/kstat (actually, the library that provides an API for
 *		accessing /dev/kstat) is opened, and each of its nodes is
 *		looked at in order.  Each cpu_info node discovered that refers
 *		to a cpuid in the "cputabl" tabulation is read in and its state
 *		information is used to fill out the cpu_state field of the
 *		corresponding cp->cpu[] entry.
 *
 * Output:	Each cpu whose state was discovered as being "online" has its
 *		cp->cpu[].cpu_state field updated to "online."
 *
 *		The return value indicates the number of successfully queried
 *		cpu states.
 */
static int
dr_get_cpu_states(board_cpu_configp_t cp, int *cputabl)
{
	kstat_ctl_t		*kc;
	kstat_t			*ksp;
	kstat_named_t		*name;
	int			cpuid;
	int			qcount = 0;

	/* Open the KSTAT library */
	if ((kc = kstat_open()) == NULL)
		return (0);

	/* Walk through the kstat linked list */
	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
		/* If this is a "cpu_info" node, then query the state */
		if (strcmp(ksp->ks_module, "cpu_info") == 0) {
			/* Don't even bother unless the cpu is in the table */
			if (cputabl[ksp->ks_instance] == -1)
				continue;

			/* Just ignore this node if kstat_read fails */
			if (kstat_read(kc, ksp, NULL) == -1)
				continue;

			/*
			 * Try loading the state name.  A failure to load the
			 * data just results in ignoring the node and moving on.
			 */
			name = (kstat_named_t *)kstat_data_lookup(ksp, "state");
			if (name != NULL) {
				/*
				 * A successful query means updating the cp->cpu
				 * entry's cpu_state.  The kstat string for an
				 * online processor is "on-line", but the daemon
				 * has traditionally used the message "online"
				 * (no dash).  In case applications are picky
				 * about the actual string, we translate.
				 */
				cpuid = ksp->ks_instance;
				if (strcmp(name->value.c, "on-line") == 0) {
					free(cp->cpu[cputabl[cpuid]].cpu_state);
					cp->cpu[cputabl[cpuid]].cpu_state = \
						(drmsg_t)strdup("online");
				} else if (strncmp(name->value.c, "off", 3) \
						!= 0) {
					free(cp->cpu[cputabl[cpuid]].cpu_state);
					cp->cpu[cputabl[cpuid]].cpu_state = \
						(drmsg_t)strdup(name->value.c);
				}

				/*
				 * Since we successfully queried this cpu's
				 * state, increment our return value.
				 */
				qcount++;
			}
		}

	/* Close the KSTAT library */
	kstat_close(kc);

	/* Return our count of successfully-queried cpu states */
	return (qcount);
}

/*
 * ----------------------------------------------------------
 * dr_get_sys_pages
 * ----------------------------------------------------------
 *
 * This routine is only called if sysconf() fails to return the
 * total number of system pages.  This is much more expensive than sysconf().
 *
 * Description:	The memory status of all boards attached to the OS is retrieved
 *		and the total number of pages for all memory status nodes is
 *		summed.
 *
 *		The phrase "attached to the OS" means that the only boards
 *		relevant to this search are those in either the PARTIAL or the
 *		CONFIGURED state, indicating that their memory is actually
 *		available to the OS.  The total sum of pages for all memory
 *		nodes on all such "attached to the OS" boards is gathered.
 *
 * Output:	An integer value indicating the total number of pages in the
 *		system, or a -1 in case of error.
 */
static int
dr_get_sys_pages(void)
{
	sfdr_stat_t	status;
	int		brd;
	int		node;
	int		sys_pages = -1;

	/* For each board, */
	for (brd = 0; brd < MAX_BOARDS; brd++) {

		/* Query the board's status; if ioctl fails, trudge on */
		memset((caddr_t)&status, 0, sizeof (sfdr_stat_t));
		if (dr_issue_ioctl(DR_IOCTARG_BRD, brd, DR_CMD_STATUS, \
				&status, NULL))
			continue;

		/* For each memory node returned, */
		for (node = 0; node < status.s_nstat; node++) {

			/* Iff it's a memory node, add in its total pages */
			if (status.s_stat[node].ntype == DR_NT_MEM) {
				if (sys_pages == -1)
				    sys_pages = status.s_stat[node].totpages \
						- status.s_stat[node].detpages;
				else
				    sys_pages += status.s_stat[node].totpages \
						- status.s_stat[node].detpages;
			}
		}
	}

	/* Return whatever we got */
	return (sys_pages);
}

/*
 * ----------------------------------------------------------
 * get_mem_cost
 * ----------------------------------------------------------
 *
 * Get the detach cost info for the board.
 *
 * Input: board
 * Output ptr to allocated cost structure.
 */
static board_mem_costp_t
get_mem_cost(int board)
{
	sfdr_stat_t		status;
	board_mem_costp_t	mcp;
	int			totalpages;
	int			detachpages;
	int			node;

	/* Retrieve the board's status, and check for errors */
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, \
			(void *)&status, NULL)) {
		dr_logerr(DRV_FAIL, errno, "get_mem_cost: ioctl failed");
		return (NULL);
	}

	/* Allocate storage for the return value */
	mcp = calloc(1, sizeof (board_mem_cost_t));
	if (mcp == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (board_mem_cost_t)");
		return (NULL);
	}

	/* Count up all of the pages for this board */
	totalpages = 0;
	detachpages = 0;
	for (node = 0; node < status.s_nstat; node++)
		if (status.s_stat[node].ntype == DR_NT_MEM) {
			totalpages += status.s_stat[node].totpages;
			detachpages += status.s_stat[node].detpages;
		}

	/* Convert the total pages values to cost info values */
	mcp->mem_pshrink = 0;
	mcp->mem_pdetach = dr_page_to_mb(totalpages - detachpages);
	mcp->actualcost = MEMCOST_ACTUAL;

	/* Return the calculated cost info */
	return (mcp);
}

/*
 * ----------------------------------------------------------
 * get_mem_drain
 * ----------------------------------------------------------
 *
 * Get the periodic memory drain information for the board.
 *
 * Input:	board
 *
 * Description:	The memory status for the board is queried.  Over each of its
 *		memory nodes, sums of total pages and pages already detached are
 *		taken.  These totals are then used to determine statistics for
 *		how much memory still must be drained, and what percentage has
 *		already been drained.  The current time is also reported.
 *
 * Output:	ptr to allocated drain structure.
 */
static board_mem_drainp_t
get_mem_drain(int board)
{
	sfdr_stat_t		stat;
	board_mem_drainp_t 	mdp;
	int			totalpages = 0;
	int			detachpages = 0;
	int			node;

	/* Query the board's status, and report any errors */
	memset((caddr_t)&stat, 0, sizeof (sfdr_stat_t));
	if (dr_issue_ioctl(DR_IOCTARG_BRD, board, DR_CMD_STATUS, &stat, NULL)) {
		dr_logerr(DRV_FAIL, errno, "get_mem_drain: ioctl failed");
		return (NULL);
	}

	/*
	 * Acquire sums of all pages on the board and how many pages have been
	 * drained for the board.
	 */
	for (node = 0; node < stat.s_nstat; node++) {
		if (stat.s_stat[node].ntype == DR_NT_MEM) {
			totalpages += stat.s_stat[node].totpages;
			detachpages += stat.s_stat[node].detpages;
		}
	}

	/* Allocate a return structure */
	mdp = calloc(1, sizeof (board_mem_drain_t));
	if (mdp == NULL) {
		dr_logerr(DRV_FAIL, errno, "malloc failed (board_mem_drain_t)");
		return (NULL);
	}

	if (verbose > 10)
		dr_loginfo("get_mem_drain: totpages = %d, detpages = %d\n",
			totalpages, detachpages);

	/* Fill in the structure */
	mdp->mem_drain_percent = percent_complete(totalpages, detachpages);
	mdp->mem_kb_left = dr_page_to_kb(totalpages-detachpages);
	mdp->mem_drain_start = drain_start_time;
	(void) time(&mdp->mem_current_time);

	/* We're done, so return the structure */
	return (mdp);
}

/*
 * dr_page_to_mb
 * Convert the given memory pages to the number of MBytes.
 *
 * Input: pages
 *
 * Function Return: number of Mbytes
 */
static int
dr_page_to_mb(int pages)
{
	int page_size, mb_size;
	unsigned int pg_per_mb;

	/* Determine the number of MB in the system (round up if needed) */
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0) {
		dr_logerr(DRV_FAIL, errno, "Bad page size from sysconf");
		return (0);
	}

	pg_per_mb = MEGABYTE/page_size;
	mb_size = (int)((unsigned)(pages + pg_per_mb-1)/(unsigned)pg_per_mb);

	return (mb_size);
}

/*
 * dr_page_to_kb
 * Convert the given memory pages to the number of KBytes
 *
 * Input: pages
 *
 * Function Return: number of Kbytes
 */
static int
dr_page_to_kb(int pages)
{
	int page_size, kb_size;
	unsigned int kb_per_page;

	/* Determine page size */
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 0) {
		dr_logerr(DRV_FAIL, errno, "Bad page size from sysconf");
		return (0);
	}


	kb_per_page = page_size/KILOBYTE;

	if (verbose > 10) {
		dr_loginfo("dr_page_to_kb: pagesize = %d\n", page_size);
		dr_loginfo("dr_page_to_kb: kb_per_page = %d\n", kb_per_page);
	}

	/*
	 * We should not see page sizes smaller than a KB, but
	 * make sure we don't get a divide by zero non-the-less
	 */
	if (kb_per_page == 0) {
		dr_logerr(DRV_FAIL, 0,
			    "dr_page_to_kb: page size smaller than a KB");
		return (0);
	}

	kb_size = (int)((unsigned)(pages*(unsigned)kb_per_page));

	if (verbose > 10)
		dr_loginfo("dr_page_to_kb: kb_left = %d\n", kb_size);

	return (kb_size);
}

/*
 * percent_complete
 *
 * Find the percent which is completed (done/total).  This
 * value is returned as in integer so that it may easily be
 * displayed.  Make sure we always round down, not up since we
 * never want to say we're 100% done if a little still remains.
 *
 * Input: total - total number of item to do
 *	  done - number of items done. (done <= total)
 *
 * Function return value - integer percentage (0-100)
 */
static int
percent_complete(int total, int done)
{
	double dbl;

	if (total == 0)
		/* If there is no memory to drain, we're 100% done */
		return (100);

	dbl = ((double)(done)/(double)total) * 100;
	dbl = floor(dbl);
	return (irint(dbl));
}

/*
 * ----------------------------------------------------------
 * dr_get_cpu_bindings
 * ----------------------------------------------------------
 *
 * Use the /proc filesystem to look for threads that are bound
 * to the tabulated CPU id's.
 *
 * Input:	cp		(structure to fill in with binding information)
 *		table		(table of which cpu's are installed and what
 *				 their indices in the cp->cpu[] array are if so)
 *
 * Description:	For each PID in the system, there is a directory named
 *		/proc/<PID>.  And for every LWP in the system there is a
 *		directory named /proc/<PID>/lwp/<LWP>.  In that per-LWP
 *		directory is a file named "lwpsinfo" that tells whether the
 *		lwp is a system or user lwp, and which CPU id (if any) it's
 *		bound to.
 *
 *		This routine walks through the entire /proc directory examining
 *		every LWP's "lwpsinfo" file to look for threads bound to CPU's.
 *		The "cputabl" table is then used to see if the CPU id that the
 *		thread is bound to is relevant to this search.  If so, then the
 *		thread is examined further to see whether or not it's a system
 *		thread, and the	appropriate cp->cpu[] element is updated with
 *		the information concerning the discovered binding.
 *
 *		The "cp" structure has a list of all CPU info for a board,
 *		and this routine fills out the information regarding how many
 *		user threads are bound to each CPU, how many system threads are
 *		bound to each CPU, and the PID's corresponding to each thread
 *		bound to each CPU.
 *
 * Output:	The "cp" struct is updated with the discovered binding info.
 */
static void
dr_get_cpu_bindings(board_cpu_configp_t cp, int *table)
{
	DIR		*procdir, *lwpdir;
	struct dirent	*procentry, *lwpentry;
	lwpsinfo_t	info;
	int		pid;
	int		lwp;
	int		lwpfd;
	cpu_info_t	*cpu;
	processorid_t	cpuid;
	static char	filename[MAXLEN];

	/* Open the /proc directory */
	procdir = opendir(PROCDIR);
	if (procdir == NULL) {
		dr_loginfo("get_cpu_bindings: can't access /proc filesystem " \
			"[non-fatal].");
		return;
	}

	/* Search through each PID's /proc entry */
	while (procentry = readdir(procdir)) {
		/* Skip over the . and .. directories */
		if (strcmp(".", procentry->d_name) == 0 ||
				strcmp("..", procentry->d_name) == 0)
			continue;

		/*
		 * Open this PID's LWP directory.  A failure just means one
		 * PID's LWPs can't be examined.  Just skip it and continue.
		 */
		pid = atoi(procentry->d_name);
		(void) sprintf(filename, "%s/%d/%s", PROCDIR, pid, LWPDIR);
		lwpdir = opendir(filename);
		if (lwpdir == NULL)
			continue;

		/* Check each LWP for this PID to see if it's bound anywhere */
		while (lwpentry = readdir(lwpdir)) {

			/* Skip over the . and .. directories */
			if (strcmp(".", lwpentry->d_name) == 0 ||
					strcmp("..", lwpentry->d_name) == 0)
				continue;

			/* Open the status file.  Just continue if failure. */
			lwp = atoi(lwpentry->d_name);
			(void) sprintf(filename, "%s/%d/%s/%d/%s", PROCDIR, \
				pid, LWPDIR, lwp, LWPSINFO);
			lwpfd = open(filename, O_RDONLY);
			if (lwpfd == -1)
				continue;

			/* Read in the status info */
			if (read(lwpfd, (void *)&info, \
				sizeof (lwpsinfo_t)) == sizeof (lwpsinfo_t)) {

				/* If this thread is bound to a processor, */
				cpuid = info.pr_bindpro;
				if (cpuid != -1) {

					/* If the processor is in our table, */
					if (table[cpuid] != -1) {

						/* Add this binding to "cp" */
						cpu = &(cp->cpu[table[cpuid]]);
						add_binding(cpu, &info, pid);

						/*
						 * Stop searching this PID's
						 * LWPs.
						 */
						close(lwpfd);
						break;
					}
				}
			}

			/* Done examining this singular thread */
			close(lwpfd);
		}

		/* Done examining all of this PID's threads */
		closedir(lwpdir);
	}

	/* Done examing all PID's threads */
	closedir(procdir);
}

/*
 * ----------------------------------------------------------
 * add_binding
 * ----------------------------------------------------------
 *
 * Add a singular pid:thread binding to the proclist of a CPU.
 * (A utility procedure called by dr_get_cpu_bindings()).
 *
 * Input:	cpu		(pointer to the cpu_info_t for the CPU)
 *		info		(pointer to the bound thread's info)
 *		pid		(PID containing the bound thread)
 *
 * Description:	Check the flags for the bound thread to find out if it's a
 *		system or a user thread, and increment the correct binding
 *		count for the CPU.  Then re-allocate the proclist for the CPU
 *		and add the pid of the bound thread to the proclist.
 *
 * Results:	"cpu" is updated for the new binding.
 */
static void
add_binding(cpu_info_t *cpu, lwpsinfo_t *info, processorid_t pid)
{
	register int	bindings;

	/*
	 * Increment the binding count appropriate for what kind of thread this
	 * is
	 */
	if ((info->pr_flag) & PR_ISSYS)
		cpu->num_sys_threads_bound++;
	else
		cpu->num_user_threads_bound++;

	/*
	 * Reallocate the proclist of bound PID's for this CPU
	 */
	bindings = cpu->num_sys_threads_bound + cpu->num_user_threads_bound;
	cpu->proclist.proclist_val = \
		(proclist_t *)realloc((void *)cpu->proclist.proclist_val, \
		bindings * sizeof (int));
	if (cpu->proclist.proclist_val == NULL) {
		dr_loginfo("get_cpu_bindings: malloc failed (proclist_t) " \
			"[non-fatal].");
		return;
	}

	/*
	 * Add the new binding to the proclist of bound PID's
	 */
	cpu->proclist.proclist_len++;
	cpu->proclist.proclist_val[bindings - 1].pid = pid;
}

/*
 * free_cpu_config
 *
 *	Frees all allocated memory for a board_cpu_config_t structure.
 */
void
free_cpu_config(board_cpu_configp_t cp)
{
	int i;

	if (cp == NULL)
		return;

	for (i = 0; i < MAX_CPU_UNITS_PER_BOARD; i++) {
		if (cp->cpu[i].cpu_state)
			free(cp->cpu[i].cpu_state);
		if (cp->cpu[i].proclist.proclist_val)
			free(cp->cpu[i].proclist.proclist_val);
	}

	free(cp);
}

/*
 * free_mem_config
 *
 *	Frees all allocated memory for a board_mem_config_t structure.
 */
void
free_mem_config(board_mem_configp_t mp)
{
	if (mp == NULL)
		return;

	if (mp->dr_mem_detach)
		free(mp->dr_mem_detach);

	free(mp);
}
