/*
 * Copyright (c) 1992 Sun Microsystems, Inc.
 */

#pragma	ident  "@(#)display_sun4d.c 1.28     99/04/29 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <sys/clock.h>
#include <libintl.h>
#include "pdevinfo.h"
#include "display.h"
#include "anlyzerr.h"

/*
 * following are the defines for the various sizes of reset-state
 * properties that we expect to receive. We key off of these sizes
 * in order to process the error log data.
 */
#define	RST_CPU_XD1_SZ		24
#define	RST_CPU_XD2_SZ		40
#define	RST_MEM_XD1_SZ		16
#define	RST_MEM_XD2_SZ		32
#define	RST_IO_XD1_SZ		24
#define	RST_IO_XD2_SZ		40
#define	RST_BIF_XD1_SZ		68
#define	RST_BIF_XD2_SZ		136
#define	RST_BIF_NP_XD1_SZ	12
#define	RST_BIF_NP_XD2_SZ	24

/* value in uninitialized NVRAM */
#define	UN_INIT_NVRAM		0x55555555

/* should really get from sys headers but can't due to bug 1120641 */
#define	MQH_GROUP_PER_BOARD	4
#define	MX_SBUS_SLOTS		4

/* Routines private to sun4d. */
static void display_board(Board_node *);
static void display_cpu_mem_devices(Sys_tree *);
static void display_io_devices(Sys_tree *);
static void display_sbus_cards(Board_node *);
static Prom_node *find_cpu(Board_node *, int);
static void get_mem_total(Sys_tree *, struct mem_total *);
static int get_devid(Prom_node *);
static int get_cpu_freq(Prom_node *);
static int get_group_size(Prom_node *, int);
static Prom_node *find_card(Prom_node *, int);
static Prom_node *next_card(Prom_node *, int);
static char *get_card_name(Prom_node *);
static char *get_card_model(Prom_node *);
static int all_boards_fail(Sys_tree *, char *);
static int prom_has_cache_size(Sys_tree *);
static int get_cache_size(Prom_node *pnode);

static int disp_fail_parts(Sys_tree *, Prom_node *);
static int disp_err_log(Sys_tree *, Prom_node *);
static int analyze_bif(Prom_node *, int, int);
static int analyze_iounit(Prom_node *, int, int);
static int analyze_memunit(Prom_node *, int, int);
static int analyze_cpu(Prom_node *, int, int);

int
display(Sys_tree *tree, Prom_node *root, int syserrlog)
{
	int exit_code = 0;	/* init to all OK */
	void *value;		/* used for opaque PROM data */
	struct mem_total memory_total;	/* Total memory in system */

	/*
	 * silently check for any types of machine errors
	 */
	print_flag = 0;
	if (disp_fail_parts(tree, root) || disp_err_log(tree, root)) {
		/* set exit_code to show failures */
		exit_code = 1;
	}
	print_flag = 1;

	/*
	 * Now display the machine's configuration. We do this if we
	 * are not logging or exit_code is set (machine is broke).
	 */
	if (!logging || exit_code) {
		struct utsname uts_buf;

		/*
		 * Display system banner
		 */
		(void) uname(&uts_buf);
		log_printf(gettext("System Configuration:  Sun Microsystems"
			"  %s %s\n"), uts_buf.machine,
			    get_prop_val(find_prop(root, "banner-name")), 0);

		/* display system clock frequency */
		value = get_prop_val(find_prop(root, "clock-frequency"));
		if (value != NULL) {
			log_printf(gettext("System clock frequency: "
				"%d MHz\n"),
				    ((*((int *)value)) + 500000) / 1000000, 0);
		} else {
			log_printf(gettext("System clock frequency "
				"not found\n"), 0);
		}

		/* display total usable installed memory */
		get_mem_total(tree, &memory_total);
		log_printf(gettext("Memory size: %4dMb\n"),
			memory_total.dram, 0);

		/* We display the NVSIMM size totals separately. */
		if (memory_total.nvsimm != 0) {
			log_printf(gettext("NVSIMM size: %4dMb\n"),
				memory_total.nvsimm, 0);
		}

		/*
		 * Display the number of XDBus's. This will be a no-op
		 * for all architectures but sun4d.
		 */
		value = get_prop_val(find_prop(root, "n-xdbus"));
		log_printf(gettext("Number of XDBuses: %d\n"),
			*(int *)value, 0);

		/* Display the CPU/Memory devices */
		display_cpu_mem_devices(tree);

		/* Display the IO cards on all the boards. */
		display_io_devices(tree);

		/* Display the failed parts in the system */
		(void) disp_fail_parts(tree, root);
	}


	/*
	 * Now display the last powerfail time and the fatal hardware
	 * reset information. We do this under a couple of conditions.
	 * First if the user asks for it. The second is iof the user
	 * told us to do logging, and we found a system failure.
	 */
	if (syserrlog || (logging && exit_code)) {
		/*
		 * display time of latest powerfail. Not all systems
		 * have this capability. For those that do not, this
		 * is just a no-op.
		 */
		disp_powerfail(root);

		(void) disp_err_log(tree, root);
	}

	return (exit_code);
}

/*
 * Display failed components from a sun4d system. Failures are indicated
 * by a PROM node having a status property. The value of this property
 * is a string. In some cases the string is parsed to yield more specific
 * information as to the type of the failure.
 */
static int
disp_fail_parts(Sys_tree *tree, Prom_node *root)
{
	int system_failed = 0;
	int system_degraded = 0;
	Board_node *bnode = tree->bd_list;
	Prom_node *pnode;
	Prop *prop;
	void *value;

	/* look in the root node for a disabled XDBus (SC2000 only) */
	if ((value = get_prop_val(find_prop(root, "disabled-xdbus"))) !=
		NULL) {
		system_degraded = 1;
		log_printf(
	gettext("\nSystem running in degraded mode, XDBus %d disabled\n"),
		*(int *)value, 0);
	}

	/* look in the /boards node for a failed system controller */
	if ((pnode = dev_find_node(root, "boards")) != NULL) {
		if ((value = get_prop_val(find_prop(pnode,
			"status"))) != NULL) {
			if (!system_failed) {
				system_failed = 1;
				log_printf(
	gettext("\nFailed Field Replaceable Units (FRU) in System:\n"), 0);
				log_printf("=========================="
					"====================\n", 0);
			}
			log_printf(
	gettext("System control board failed: %s\n"), (char *)value, 0);
			log_printf(
	gettext("Failed Field Replaceable Unit: System control board\n"), 0);
		}
	}

	/* go through all of the boards */
	while (bnode != NULL) {
		/* find failed chips */
		pnode = find_failed_node(bnode->nodes);
		if (pnode != NULL) {
			if (!system_failed) {
				system_failed = 1;
				log_printf(
	gettext("\nFailed Field Replaceable Units (FRU) in System:\n"), 0);
				log_printf("=========================="
					"====================\n", 0);
			}
		}

		while (pnode != NULL) {
			void *value;
			char *name;		/* name string for node */
			char *par_name;		/* name string for parent */

			value = get_prop_val(find_prop(pnode, "status"));

			/* sanity check of data retreived from PROM */
			if (value == NULL) {
				pnode = next_failed_node(pnode);
				continue;
			}
			name = get_node_name(pnode);
			par_name = get_node_name(pnode->parent);

			if ((par_name != NULL) && strstr(par_name, "sbi")) {
				log_printf(gettext("SBus Card "), 0);
			} else if ((name != NULL) && strstr(name, "bif") &&
			    value) {
				char *device;

				/* parse string to get rid of "fail-" */
				device = strchr((char *)value, '-');

				if (device != NULL)
					log_printf("%s", device+1, 0);
				else
					log_printf("%s ", (char *)value, 0);
			} else if (name != NULL) {
				log_printf("%s ", name, 0);
			}

			log_printf(
				gettext("unavailable on System Board #%d\n"),
				    bnode->board_num, 0);

			log_printf(
				gettext("Failed Field Replaceable Unit is "),
				    0);

			/*
			 * In this loop we want to print failures for FRU's
			 * This includes SUPER-SPARC modules, SBus cards,
			 * system boards, and system backplane.
			 */

			/* Look for failed SuperSPARC module */
			if ((name != NULL) && strstr(name, "cpu-unit") &&
			    (strstr((char *)value, "VikingModule") ||
			    strstr((char *)value, "CPUModule"))) {
				int devid = get_devid(pnode);
				char cpu;

				if ((devid >> 3) & 0x1)
					cpu = 'B';
				else
					cpu = 'A';

				log_printf(
					gettext("SuperSPARC Module %c\n\n"),
					    cpu, 0);

			} else if ((par_name != NULL) && strstr(name, "sbi")) {
				/* Look for failed SBUS card */
				int card;
				void *value;

				value = get_prop_val(find_prop(pnode, "reg"));

				if (value != NULL) {
					card = *(int *)value;
					log_printf(
						gettext("SBus card %d\n\n"),
						    card, 0);
				} else {
					log_printf(
						gettext("System Board #%d\n\n"),
						    bnode->board_num, 0);
				}
			} else if ((name != NULL) && strstr(name, "bif") &&
			    all_boards_fail(tree, value)) {
				log_printf(
					gettext("System Backplane\n\n"), 0);
			} else {
				log_printf(
					gettext("System Board #%d\n\n"),
					    bnode->board_num, 0);
			}

			pnode = next_failed_node(pnode);
		}

		/* find the memory node */
		pnode = dev_find_node(bnode->nodes, "mem-unit");

		/* look for failed memory SIMMs */
		value = NULL;
		prop = find_prop(pnode, "bad-parts");
		if ((value = get_prop_val(prop)) != NULL) {
			int size;

			if (!system_failed) {
				system_failed = 1;
				log_printf(
	gettext("\nFailed Field Replaceable Units (FRU) in System:\n"), 0);
				log_printf("=========================="
					"====================\n", 0);
			}

			log_printf(
	gettext("Memory SIMM group unavailable on System Board #%d\n"),
		bnode->board_num, 0);
			/*
			 * HACK for OBP bug in old PROMs. String
			 * was not NULL terminated.
			 */
			size = prop->value.opp.oprom_size;
			prop->value.opp.oprom_array[size-1] = 0;
			log_printf(
		gettext("Failed Field Replaceable Unit: SIMM(s) %s\n\n"),
			(char *)value, 0);
		}

		bnode = bnode->next;
	}

	if (!system_failed) {
		log_printf(gettext("\nNo failures found in System\n"),
			0);
		log_printf("===========================\n\n", 0);
	}

	if (system_degraded || system_failed)
		return (1);
	else
		return (0);
}	/* end of disp_fail_parts() */

/*
 * For each cpu-unit, mem-unit, io-unit, sbi, and bif in the system tree
 * dump any errors found in that unit. This is done in a board by board
 * manner.
 */
static int
disp_err_log(Sys_tree *tree, Prom_node *root)
{
	Board_node *bnode = tree->bd_list;
	Prom_node *pnode;
	u_char *mostek;
	int result = 0;

	pnode = dev_find_node(root, "boards");
	mostek = (u_char *) get_prop_val(find_prop(pnode, "reset-time"));

	/*
	 * Log date is stored in BCD. Convert to decimal, then load into
	 * time structure.
	 */
	/* LINTED */
	if ((mostek != NULL) && (*(int *)mostek != 0)) {
		log_printf(
			gettext("Analysis of most recent System Watchdog:\n"),
			    0);
		log_printf("========================================\n",
			0);

		log_printf("Log Date: %s\n", get_time(mostek), 0);
	} else {
		log_printf(gettext("No System Watchdog Log found\n"),
			0);
		log_printf("============================\n", 0);

		return (result);
	}


	/* go through all of the boards */
	while (bnode != NULL) {
		int hdr_printed = 0;

		/* find the cpu-units and analyze */
		pnode = dev_find_node(bnode->nodes, "cpu-unit");
		while (pnode != NULL) {
			/* analyze this CPU node */
			hdr_printed = analyze_cpu(pnode, bnode->board_num,
				hdr_printed);

			/* get the next CPU node */
			pnode = dev_next_node(pnode, "cpu-unit");
		}

		/* find the mem-units and analyze */
		pnode = dev_find_node(bnode->nodes, "mem-unit");
		hdr_printed = analyze_memunit(pnode, bnode->board_num,
			hdr_printed);

		/* find the io-units and analyze */
		pnode = dev_find_node(bnode->nodes, "io-unit");
		hdr_printed = analyze_iounit(pnode, bnode->board_num,
			hdr_printed);

		/* get the bif node for this board and analyze */
		pnode = dev_find_node(bnode->nodes, "bif");
		hdr_printed = analyze_bif(pnode, bnode->board_num,
			hdr_printed);

		/*
		 * transfer the hdr_printed value to the return code.
		 * The logic here is that if a header was printed, an
		 * error was found.
		 */
		if (hdr_printed != 0) {
			result = 1;
		}

		/* move to next board in list */
		bnode = bnode->next;
	}

	return (result);
}	/* end of disp_err_log() */

/*
 * display_cpu_mem_devices
 *
 * Display the header for the CPU and memory tables, then display
 * CPU and memory information for each board.
 */
void
display_cpu_mem_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/*
	 * Display the table header for CPUs and memory.
	 * Then display the CPU frequency and cache size, and memory group
	 * sizes on all the boards.
	 */

	/* only print cache size header line if new PROM */
	if (prom_has_cache_size(tree)) {
		log_printf(
			gettext("       CPU Units: Frequency Cache-Size"), 0);
		log_printf(gettext("        Memory Units: Group "
			"Size\n"), 0);
		log_printf("            ", 0);
		log_printf("A: MHz MB   B: MHz MB           "
			"0: MB   1: MB   2: MB   3: MB\n", 0);
		log_printf("            ", 0);
		log_printf("---------   ---------           "
			"-----   -----   -----   -----\n", 0);
	} else {
		log_printf(gettext("           CPU Units: Frequency"),
			0);
		log_printf(
			gettext("               Memory Units: Group Size\n"),
			    0);
		log_printf("            ", 0);
		log_printf("A: MHz      B: MHz              "
			"0: MB   1: MB   2: MB   3: MB\n", 0);
		log_printf("            ", 0);
		log_printf("------      ------              "
			"-----   -----   -----   -----\n", 0);
	}

	/* Now display all of the boards */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_board(bnode);
		bnode = bnode->next;
	}
}

/*
 * Display the CPU frequencies and cache sizes and memory group
 * sizes on a system board.
 */
static void
display_board(Board_node *board)
{
	int dev_id;		/* device ID of CPU unit */
	int group;		/* index of memory group */
	Prom_node *pnode;
	/* print the board number first */
	/*
	 * TRANSLATION_NOTE
	 *	Following string is used as a table header.
	 *	Please maintain the current alignment in
	 *	translation.
	 */
	log_printf(gettext("Board%d:     "), board->board_num, 0);

	/* display the CPUs and their operating frequencies */
	for (dev_id = 0; dev_id < 0x10; dev_id += 0x8) {
		int freq;	/* CPU clock frequency */
		int cachesize;	/* MXCC cache size */

		freq = (get_cpu_freq(find_cpu(board, dev_id)) + 500000) /
			1000000;

		cachesize = get_cache_size(find_cpu(board, dev_id));

		if (freq != 0) {
			log_printf("   %d ", freq, 0);
			if (cachesize == 0) {
				log_printf("      ", 0);
			} else {
				log_printf("%0.1f   ", (float)cachesize/
					(float)(1024*1024), 0);
			}
		} else {
			log_printf("            ", 0);
		}
	}

	log_printf("        ", 0);
	/* display the memory group sizes for this board */
	pnode = dev_find_node(board->nodes, "mem-unit");
	for (group = 0; group < MQH_GROUP_PER_BOARD; group++) {
		log_printf("  %3d   ",
			get_group_size(pnode, group)/(1024*1024), 0);
	}
	log_printf("\n", 0);
}	/* end of display_board() */

/*
 * display_io_devices
 *
 * This is the wrapper called by the common display code. It just calls
 * the correct IO functions.
 */
void
display_io_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/* Display the header text for this section. */

	/*
	 * TRANSLATION_NOTE
	 *	Following string is used as a table header.
	 *	Please maintain the current alignment in
	 *	translation.
	 */
	log_printf(gettext("======================SBus Cards"), 0);
	log_printf("=========================================\n", 0);

	/* Now display the IO cards on all the boards. */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_sbus_cards(bnode);
		bnode = bnode->next;
	}
}

/*
 * Display all of the SBus Cards present on a system board. The display
 * is oriented for one line per PROM node. This translates to one line
 * per logical device on the card. The name property of the device is
 * displayed, along with the child device name, if found. If the child
 * device has a device-type that is displayed as well.
 */
static void
display_sbus_cards(Board_node *board)
{
	Prom_node *pnode;
	Prom_node *sbi_node;
	int card;
	void *value;

	/* display the SBus cards present on this board */
	/* find the io-unit node */
	pnode = dev_find_node(board->nodes, "io-unit");
	/* now find the SBI node */
	sbi_node = dev_find_node(pnode, "sbi");

	/* get sbus clock frequency */
	value = get_prop_val(find_prop(sbi_node, "clock-frequency"));

	log_printf("\n", 0);
	if (value != NULL) {
		log_printf(gettext("Board%d:        "
			"SBus clock frequency: %d MHz\n"), board->board_num,
			    ((*((int *)value)) + 500000) / 1000000, 0);
	} else {
		log_printf(
			gettext("Board%d:        SBus clock frequency not "
			"found\n"), board->board_num, 0);
	}

	for (card = 0; card < MX_SBUS_SLOTS; card++) {
		Prom_node *card_node;
		int device = 0;		/* # of device on card */
		Prop *sprop;

		card_node = find_card(sbi_node, card);

		/* display nothing for no card */
		if (card_node == NULL)
			continue;

		/* display nothing for failed card */
		if ((sprop = find_prop(card_node, "status")) != NULL) {
			char *status = get_prop_val(sprop);

			if (strstr(status, "fail") != NULL)
				continue;
		}

		/* now display all of the node names for that card */
		while (card_node != NULL) {
			char *model;
			char *name;
			char fmt_str[(OPROMMAXPARAM*3)+1];
			char tmp_str[OPROMMAXPARAM+1];

			model = get_card_model(card_node);
			name = get_card_name(card_node);

			if (name == NULL) {
				card_node = next_card(card_node, card);
				continue;
			}

			/*
			 * TRANSLATION_NOTE
			 * Following string is used as a table header.
			 * Please maintain the current alignment in
			 * translation.
			 */
			if (device == 0) {
				log_printf("               %d: ",
					card, 0);
				(void) sprintf(fmt_str, "%s", name);
			} else {
				log_printf("                  ", 0);
				(void) sprintf(fmt_str, "%s", name);
			}

			if (card_node->child != NULL) {
				void *value;
				char *child_name;

				child_name = get_node_name(card_node->child);

				if (child_name != NULL) {
					(void) sprintf(tmp_str, "/%s",
						child_name);
					(void) strcat(fmt_str, tmp_str);
				}

				if ((value = get_prop_val(find_prop(
				    card_node->child,
				    "device_type"))) != NULL) {
					(void) sprintf(tmp_str, "(%s)",
						(char *)value);
					(void) strcat(fmt_str, tmp_str);
				}
			}

			log_printf("%-20s", fmt_str, 0);

			if (model != NULL) {
				log_printf("\t'%s'\n", model, 0);
			} else {
				log_printf("\n", 0);
			}
			card_node = next_card(card_node, card);
			device++;
		}
	}

}	/* end of display_sbus_cards() */

/*
 * This function returns the device ID of a Prom node for sun4d. It returns
 * -1 on an error condition. This was done because 0 is a legal device ID
 * in sun4d, whereas -1 is not.
 */
static int
get_devid(Prom_node *pnode)
{
	Prop *prop;
	void *val;

	if ((prop = find_prop(pnode, "device-id")) == NULL)
		return (-1);

	if ((val = get_prop_val(prop)) == NULL)
		return (-1);

	return (*(int *)val);
}	/* end of get_devid() */

/*
 * Find the CPU on the current board with the requested device ID. If this
 * rountine is passed a NULL pointer, it simply returns NULL.
 */
static Prom_node *
find_cpu(Board_node *board, int dev_id)
{
	Prom_node *pnode;

	/* find the first cpu node */
	pnode = dev_find_node(board->nodes, "cpu-unit");

	while (pnode != NULL) {
		if ((get_devid(pnode) & 0xF) == dev_id)
			return (pnode);

		pnode = dev_next_node(pnode, "cpu-unit");
	}
	return (NULL);
}	/* end of find_cpu() */

/*
 * Return the operating frequency of a processor in Hertz. This function
 * requires as input a legal "cpu-unit" node pointer. If a NULL pointer
 * is passed in or the clock-frequency property does not exist, the function
 * returns 0.
 */
static int
get_cpu_freq(Prom_node *pnode)
{
	Prop *prop;		/* property pointer for "clock-frequency" */
	Prom_node *node;	/* node of "cpu" device */
	void *val;		/* value of "clock-frequency" */

	/* first find the "TI,TMS390Z55" device under "cpu-unit" */
	if ((node = dev_find_node(pnode, "TI,TMS390Z55")) == NULL) {
		return (0);
	}

	/* now find the property */
	if ((prop = find_prop(node, "clock-frequency")) == NULL) {
		return (0);
	}

	if ((val = get_prop_val(prop)) == NULL) {
		return (0);
	}

	return (*(int *)val);
}	/* end of get_cpu_freq() */

/*
 * returns the size of the given processors external cache in
 * bytes. If the properties required to determine this are not
 * present, then the function returns 0.
 */
static int
get_cache_size(Prom_node *pnode)
{
	Prom_node *node;	/* node of "cpu" device */
	int *nlines_p;		/* pointer to number of cache lines */
	int *linesize_p;	/* pointer to data for linesize */

	/* first find the "TI,TMS390Z55" device under "cpu-unit" */
	if ((node = dev_find_node(pnode, "TI,TMS390Z55")) == NULL) {
		return (0);
	}

	/* now find the properties */
	if ((nlines_p = (int *)get_prop_val(find_prop(node,
		"ecache-nlines"))) == NULL) {
		return (0);
	}

	if ((linesize_p = (int *)get_prop_val(find_prop(node,
		"ecache-line-size"))) == NULL) {
		return (0);
	}

	return (*nlines_p * *linesize_p);
}

/*
 * Total up all of the configured memory in the system and update the
 * mem_total structure passed in by the caller.
 */
void
get_mem_total(Sys_tree *tree, struct mem_total *mem_total)
{
	Board_node *bnode;

	mem_total->dram = 0;
	mem_total->nvsimm = 0;

	/* loop thru boards */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		Prom_node *pnode;
		int group;

		/* find the memory node */
		pnode = dev_find_node(bnode->nodes, "mem-unit");

		/* add in all groups on this board */
		for (group = 0; group < MQH_GROUP_PER_BOARD; group++) {
			int group_size = get_group_size(pnode, group)/0x100000;

			/*
			 * 32 MByte is the smallest DRAM group on sun4d.
			 * If the memory size is less than 32 Mbytes,
			 * then the only legal SIMM size is 4 MByte,
			 * and that is an NVSIMM.
			 */
			if (group_size >= 32) {
				mem_total->dram += group_size;
			} else if (group_size == 4) {
				mem_total->nvsimm += group_size;
			}
		}
		bnode = bnode->next;
	}
}	/* end of get_mem_total() */

/*
 * Return group size in bytes of a memory group on a sun4d.
 * If any errors occur during reading of properties or an
 * illegal group number is input, 0 is returned.
 */
static int
get_group_size(Prom_node *pnode, int group)
{
	Prop *prop;
	void *val;
	int *grp;

	if ((prop = find_prop(pnode, "size")) == NULL)
		return (0);

	if ((val = get_prop_val(prop)) == NULL)
		return (0);
	if ((group < 0) || (group > 3))
		return (0);

	grp = (int *)val;

	/*
	 * If we read a group size of 8 MByte, then we have run
	 * into the PROM bug 1148961. We must then divide the
	 * size by 2.
	 */
	if (grp[group] == 8*1024*1024) {
		return (grp[group]/2);
	} else {
		return (grp[group]);
	}
}	/* end of get_group_size() */

/*
 * This function finds the mode of the first device with reg number word 0
 * equal to the requested card number. A sbi-unit node is passed in
 * as the root node for this function.
 */
static Prom_node *
find_card(Prom_node *node, int card)
{
	Prom_node *pnode;

	if (node == NULL)
		return (NULL);

	pnode = node->child;

	while (pnode != NULL) {
		void *value;

		if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL)
			if (*(int *)value == card)
				return (pnode);
		pnode = pnode->sibling;
	}
	return (NULL);
}	/* end of find_card() */

/*
 * Find the next sibling node on the requested SBus card.
 */
static Prom_node *
next_card(Prom_node *node, int card)
{
	Prom_node *pnode;

	if (node == NULL)
		return (NULL);

	pnode = node->sibling;
	while (pnode != NULL) {
		void *value;

		if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL)
			if (*(int *)value == card)
				return (pnode);
		pnode = pnode->sibling;
	}
	return (NULL);
}	/* end of next_card() */

/*
 * returns the address of the value of the name property for this PROM
 * node.
 */
static char *
get_card_name(Prom_node *node)
{
	char *name;

	if (node == NULL)
		return (NULL);

	/* get the model number of this card */
	name = (char *)get_prop_val(find_prop(node, "name"));
	return (name);
}	/* end of get_card_name() */

/*
 * returns the address of the value of the model property for this PROM
 * node.
 */
static char *
get_card_model(Prom_node *node)
{
	char *model;

	if (node == NULL)
		return (NULL);

	/* get the model number of this card */
	model = (char *)get_prop_val(find_prop(node, "model"));

	return (model);
}	/* end of get_card_model() */

/*
 * Check the first CPU in the device tree and see if it has the
 * properties 'ecache-line-size' and 'ecache-nlines'. If so,
 * return 1, else return 0.
 */
static int
prom_has_cache_size(Sys_tree *tree)
{
	Board_node *bnode;

	if (tree == NULL) {
		return (0);
	}

	/* go through the boards until you find a CPU to check */
	for (bnode = tree->bd_list; bnode != NULL; bnode = bnode->next) {
		Prom_node *pnode;

		/* find the first cpu node */
		pnode = dev_find_node(bnode->nodes, "cpu-unit");

		/* if no CPUs on board, skip to next */
		if (pnode != NULL) {
			if (get_cache_size(pnode) == 0) {
				return (0);
			} else {
				return (1);
			}
		}
	}
	/* never found the props, so return 0 */
	return (0);
}

/*
 * Check and see if all boards in the system fail on one of the XDBuses
 * failing in the current string. If this is true and there is more
 * than one board in the system, then the backplane is most likely at
 * fault.
 */
static int
all_boards_fail(Sys_tree *tree, char *value)
{
	char temp[OPROMMAXPARAM];
	char *token;

	if ((tree == NULL) || (value == NULL))
		return (0);

	if (tree->board_cnt == 1)
		return (0);

	(void) strcpy(temp, value);

	/* toss the first token in the string */
	(void) strtok(temp, "-");
	token = strtok(NULL, "- ");

	/* loop through all the tokens in this string */
	while (token != NULL) {
		Board_node *bnode = tree->bd_list;
		int match = 1;

		/* loop thru all the boards looking for a match */
		while (bnode != NULL) {
			Prom_node *bif = dev_find_node(bnode->nodes, "bif");
			char *bif_status = get_prop_val(find_prop(bif,
				"status"));

			if ((bif == NULL) || (bif_status == NULL)) {
				match = 0;
				break;
			}

			if (!strstr(bif_status, token)) {
				match = 0;
				break;
			}

			bnode = bnode->next;

			/*
			 * if all boards fail with this token, then
			 * return 1
			 */
			if (match)
				return (1);
		}
		token = strtok(NULL, " -");
	}

	return (0);
}	/* end of all_boards_fail() */

/*
 * The following functions are all for reading reset-state information
 * out of prom nodes and calling the appropriate error analysis code
 * that has been ported from sun4d POST.
 */
static int
analyze_cpu(Prom_node *node, int board, int hdr_printed)
{
	void *value;
	Prop *prop;
	unsigned long long cc_err;
	unsigned long long ddr;
	unsigned long long dcsr;
	int n_xdbus;
	int devid;
	int i;

	/* get the value pointer from the reset-state property */
	prop = find_prop(node, "reset-state");

	if (prop == NULL)
		return (hdr_printed);

	value = get_prop_val(prop);

	/* exit if error finding property */
	if (value == NULL)
		return (hdr_printed);

	/* copy the reset_state info out of the Prom node */

	/* check for uninitialized NVRAM/TOD data */
	if (*(int *)value == UN_INIT_NVRAM)
		return (hdr_printed);

	/* read the device ID from the node */
	if ((devid = get_devid(node)) == -1)
		return (hdr_printed);

	/* first 8 bytes are MXCC Error register */
	cc_err = *(unsigned long long *)value;
	hdr_printed = ae_cc(devid, cc_err, board, hdr_printed);

	/* if length == (MXCC + 1 BW) => 1 XDBus */
	if (prop->value.opp.oprom_size == RST_CPU_XD1_SZ) {
		n_xdbus = 1;
	}
	/* else if length == (MXCC + 2 BW) => 2 XDBus */
	else if (prop->value.opp.oprom_size == RST_CPU_XD2_SZ) {
		n_xdbus = 2;
	} else {
		char *name = get_node_name(node);
		if (name != NULL) {
			log_printf("Prom node %s has incorrect status"
				" property length : %d\n", name,
				    prop->value.opp.oprom_size, 0);
		}
		return (hdr_printed);
	}

	/* move Prom value pointer to first BW data */
	value = (void *)((char *)value + 8);

	/* now analyze the error logs */
	for (i = 0; i < n_xdbus; i++) {
		ddr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		dcsr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		hdr_printed = ae_bw(devid, i, dcsr, ddr, board,
			hdr_printed);
	}
	return (hdr_printed);
}	/* end of analyze_cpu() */

static int
analyze_memunit(Prom_node *node, int board, int hdr_printed)
{
	void *value;
	Prop *prop;
	unsigned long long ddr;
	unsigned long long dcsr;
	int n_xdbus;
	int i;

	/* get the value pointer from the reset-state property */
	prop = find_prop(node, "reset-state");

	if (prop == NULL)
		return (hdr_printed);

	value = get_prop_val(prop);

	/* exit if error finding property */
	if (value == NULL)
		return (hdr_printed);

	/* check for uninitialized NVRAM/TOD data */
	if (*(int *)value == UN_INIT_NVRAM)
		return (hdr_printed);

	/* if length == (1 MQH) => 1 XDBus */
	if (prop->value.opp.oprom_size == RST_MEM_XD1_SZ) {
		n_xdbus = 1;
	}
	/* else if length == (2 MQH) => 2 XDBus */
	else if (prop->value.opp.oprom_size == RST_MEM_XD2_SZ) {
		n_xdbus = 2;
	} else {
		char *name = get_node_name(node);

		if (name != NULL) {
			log_printf("Prom node %s has incorrect "
				"status property length : %d\n", name,
				    prop->value.opp.oprom_size, 0);
		}

		return (hdr_printed);
	}

	/* now analyze the error logs */
	for (i = 0; i < n_xdbus; i++) {
		ddr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		dcsr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		hdr_printed = ae_mqh(i, dcsr, ddr, board,
			hdr_printed);
	}

	return (hdr_printed);
}	/* end of analyze_memunit() */

static int
analyze_iounit(Prom_node *node, int board, int hdr_printed)
{
	void *value;
	Prop *prop;
	unsigned long long ddr;
	unsigned long long dcsr;
	int n_xdbus;
	int devid;
	int i;
	int sr;

	/* get the value pointer from the reset-state property */
	prop = find_prop(node, "reset-state");

	if (prop == NULL)
		return (hdr_printed);

	value = get_prop_val(prop);

	/* exit if error finding property */
	if (value == NULL)
		return (hdr_printed);

	/* check for uninitialized NVRAM/TOD data */
	if (*(int *)value == UN_INIT_NVRAM)
		return (hdr_printed);

	/* if length == (SBI + 1 IOC) => 1 XDBus */
	if (prop->value.opp.oprom_size == RST_IO_XD1_SZ) {
		n_xdbus = 1;
	}
	/* else if length == (SBI + 2 IOC) => 2 XDBus */
	else if (prop->value.opp.oprom_size == RST_IO_XD2_SZ) {
		n_xdbus = 2;
	} else {
		char *name = get_node_name(node);

		if (name != NULL) {
			log_printf("Prom node %s has incorrect "
				"status property length : %d\n", name,
				    prop->value.opp.oprom_size, 0);
		}
		return (hdr_printed);
	}

	/* read the device ID from the node */
	if ((devid = get_devid(node)) == -1)
		return (hdr_printed);

	/* read out the SBI data first */
	sr = *(int *)value;
	value = (void *)((char *)value + 4);

	/* skip the SBI control register */
	value = (void *)((char *)value + 4);

	/* now analyze the error logs */
	for (i = 0; i < n_xdbus; i++) {
		ddr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		dcsr = *(unsigned long long *)value;
		value = (void *)((char *)value + 8);
		hdr_printed = ae_ioc(i, dcsr, ddr, board,
			hdr_printed);
	}
	hdr_printed = ae_sbi(devid, sr, board, hdr_printed);

	return (hdr_printed);
}	/* end of analyze_iounit() */

static int
analyze_bif(Prom_node *node, int board, int hdr_printed)
{
	void *value;
	Prop *prop;
	int n_xdbus;
	int i;
	int length;	/* length of Openprom property */

	/* get the value pointer from the reset-state property */
	prop = find_prop(node, "reset-state");

	if (prop == NULL)
		return (hdr_printed);

	value = get_prop_val(prop);

	/* exit if error finding property */
	if (value == NULL)
		return (hdr_printed);

	/* check for uninitialized NVRAM/TOD data */
	if (*(int *)value == UN_INIT_NVRAM)
		return (hdr_printed);

	length = prop->value.opp.oprom_size;

	/* is this a processor or non-processor board? */
	if ((length == RST_BIF_XD1_SZ) || (length == RST_BIF_XD2_SZ)) {
		bus_interface_ring_status bic_status;

		/* 1 or 2 XBbus? */
		if (length == RST_BIF_XD1_SZ)
			n_xdbus = 1;
		else
			n_xdbus = 2;

		/* copy the data out of the node */
		(void) memcpy((void *)&bic_status,
			(void *)prop->value.opp.oprom_array, length);

		/* now analyze the BIC errror logs */
		for (i = 0; i < n_xdbus; i++) {
			hdr_printed = analyze_bics(&bic_status, i, board,
				hdr_printed);
		}
	} else if ((length == RST_BIF_NP_XD1_SZ) ||
	    (length == RST_BIF_NP_XD2_SZ)) {
		/* analyze a non-processor bif node */
		cmp_db_state comp_bic_state;

		/* 1 or 2 XBbus? */
		if (length == RST_BIF_XD1_SZ)
			n_xdbus = 1;
		else
			n_xdbus = 2;

		/* copy the data out of the node */
		(void) memcpy((void *) &comp_bic_state,
			(void *) prop->value.opp.oprom_array, length);

		hdr_printed = dump_comp_bics(&comp_bic_state, n_xdbus, board,
			hdr_printed);
	} else {
		char *name = get_node_name(node);

		if (name != NULL) {
			log_printf("Node %s has bad reset-state prop "
				"length : %d\n", name,
				    prop->value.opp.oprom_size, 0);
		}
		return (hdr_printed);
	}
	return (hdr_printed);
}	/* end of analyze_bif() */
