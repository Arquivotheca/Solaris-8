/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dr_mach_obp_config.c	1.12	98/11/19 SMI"

/*
 * This file implements the Dynamic Reconfiguration OBP Configuration Routine.
 */

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/autoconf.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddipropdefs.h>
#include <sys/obpdefs.h>
#include <sys/openpromio.h>
#include <wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "dr_subr.h"
#include "dr_openprom.h"

/*
 * obp_config_fn
 *
 * This routine is called during the OBP walk when a node is
 * found.  Determine if it belongs to this board and save configuration
 * information found there.
 *
 * The tree is walked in depth first order. We make use of the tree
 * level numbers to know when we are no longer looking at a particular
 * subtree.  For example, all device information is in the "sbus"
 * subtree and there are certain things we want to look for in that
 * particular subtree.
 *
 * Note that the particulars of the obp tree structure and what the properties
 * mean was taken from usr/src/cmd/prtdiag/sun4u1/display.c
 *
 * Input:
 *	id	- of the obp node used only in debug printout
 *	level	- obp tree level
 * 	ap	- obp_config_fn_arg pointer where
 *			argp->board
 *			argp->brdcfgp is ptr to config structure to fill in
 *
 * Output:
 * *brdcfgp
 */
/* ARGSUSED */
void
obp_config_fn(id, level, ap)
	int	id;
	int	level;
	void	*ap;
{
	board_configp_t		brdcfgp;
	int				board;
	struct obp_config_fn_arg	*argp;
	struct obp_level_info		*lp;
	char				*val_char;
	int				*val_int;
	sbus_configp_t			sp, tp;
	int				slot, cpu;

	argp = (struct obp_config_fn_arg *)ap;
	brdcfgp = argp->brdcfgp;
	board = argp->board;
	lp = &(argp->level_info);

#ifdef TESTING
	dr_loginfo("node id %x level %d\n", id, level);

	if (getpropval("name", (void *)&val_char))
		return;
	dr_loginfo("name:%s\n", val_char);
#endif TESTING

	/*
	 * If we get back up to the unit_level, this means we're
	 * done with this unit's subtree.
	 */
	if (level <= lp->unit_level)
		CLEAR_LEVEL_INFO(lp);

	if (lp->unit_type == DR_NO_UNIT) {

		/*
		 * This means we're looking for a unit to gather
		 * information for.  First get the board number
		 */
		if (getpropval("board#", (void *)&val_int))
			return;

		/* Not a unit on the board we're interested in */
		if (board != *val_int)
			return;

		/*
		 * grab the device_type of the node to see
		 * if we are interested in it.
		 */
		if (getpropval("device_type", (void *)&val_char))
			return;

		/*
		 * Only interested in the IO and CPU nodes
		 */
		if (strcmp("sbus", val_char) == 0) {
			lp->unit_type = DR_IO_SBUS_UNIT;

		} else if (strncmp("pci", val_char, 3) == 0) {
			lp->unit_type = DR_IO_PCI_UNIT;

		} else if (strcmp("cpu", val_char) == 0) {
			lp->unit_type = DR_CPU_UNIT;

		} else
			return;

		lp->unit_level = level;

		switch (lp->unit_type) {

		case DR_CPU_UNIT:
			/* cpu node */
			if (brdcfgp->cpu_cnt >= MAX_CPU_UNITS_PER_BOARD) {
				dr_logerr(DRV_FAIL, 0,
					"OBP config: too many CPUs.");
				break;
			}

			/* get the OBP upa-portid which equals the cpu-id */
			if (getpropval("upa-portid", (void *)&val_int)) {
				if (verbose)
					dr_loginfo("cpu unit without " \
						"upa-portid [non-fatal]\n");
				break;
			}

			cpu = brdcfgp->cpu_cnt;
			brdcfgp->cpu[cpu].cpuid = (*val_int);
			brdcfgp->cpu_cnt++;
			break;

		case DR_IO_SBUS_UNIT:
			/*
			 * We'll examine the child nodes to figure
			 * out what sbus cards are present.
			 * The children of the sbus nodes are the
			 * controllers in the slots.  Below these controllers
			 * can be devices attached to the controllers.
			 *
			 * Future calls to this routine will gather this
			 * information.
			 */
			lp->io.slot_level = level + 1;

			/*
			 * we need to note which sysio this sbus
			 * node represents (i.e. 0 or 1).
			 */
			if (getpropval("upa-portid", (void *)&val_int)) {
				if (verbose)
					dr_loginfo("sbus node without " \
						"upa-portid [non-fatal]\n");
				break;
			}

			lp->io.sysio_num = DEVICEID_UNIT_NUM((*val_int));

			if (lp->io.sysio_num < 0 ||
				lp->io.sysio_num >= MAX_SBUS_SLOTS_PER_IOC) {
				if (verbose)
					dr_loginfo("sysio_num out of range " \
						"[non-fatal]\n");
				lp->io.sysio_num = 0;

				break;
			}
			break;

		case DR_IO_PCI_UNIT:
			/*
			 * We'll examine the child nodes to figure
			 * out what pci cards are present.
			 * The children of the pci nodes are the
			 * controllers in the slots.  Below these controllers
			 * can be devices attached to the controllers.
			 *
			 * Future calls to this routine will gather this
			 * information.
			 */
			lp->io.slot_level = level + 1;

			/*
			 * we need to note which sysio this pci
			 * node represents (i.e. 0 or 1).
			 */
			if (getpropval("upa-portid", (void *)&val_int)) {
				if (verbose)
					dr_loginfo("pci node without " \
						"upa-portid [non-fatal]\n");
				break;
			}

			lp->io.sysio_num = DEVICEID_UNIT_NUM((*val_int));

			if (lp->io.sysio_num < 0 ||
				lp->io.sysio_num >= MAX_PSYCHO) {
				if (verbose)
					dr_loginfo("sysio_num out of range " \
						"[non-fatal]\n");
				lp->io.sysio_num = 0;

				break;
			}
			break;
		}

		/*
		 * we must let the CPU nodes fall through since this is
		 * the node we want to get information from
		 */
		if (lp->unit_type != DR_CPU_UNIT) {
			return;
		}
	}

	/*
	 * If we get here it means we're looking at the subtree
	 * of a unit to gather further information.
	 */
	switch (lp->unit_type) {

	case DR_CPU_UNIT:
		/* make sure we are at the SUNW,UltraSPARC node */
		if (getpropval("board#", (void *)&val_int) ||
			getpropval("device_type", (void *)&val_char) ||
			(strcmp(val_char, "cpu") != 0)) {
			return;
		}
		cpu = brdcfgp->cpu_cnt - 1;

		/* Clock frequency in MHz - rounded to nearest integer value */
		if (!getpropval("clock-frequency", (void *)&val_int)) {
			brdcfgp->cpu[cpu].frequency =
				(((*val_int) + 500000) / 1000000);
		}

		/* Cache size in MBytes */
		if (!getpropval("ecache-size", (void *)&val_int)) {
			brdcfgp->cpu[cpu].ecache_size =
				(((float)(*val_int))/(1024 * 1024));
		}

		/* mask# */
		if (!getpropval("mask#", (void *)&val_int)) {
			brdcfgp->cpu[cpu].mask = (*val_int);
		}

		break;

	case DR_IO_SBUS_UNIT:
		if (level < lp->io.slot_level)
			/* Only care about slot nodes or lower */
			return;

		if (level == lp->io.slot_level) {
			/*
			 * Find the slot number.  This is
			 * in the "reg" property.
			 */
			if (getpropval("reg", (void *)&val_int)) {
				if (verbose)
					dr_loginfo("obp_info: missing slot " \
						"number [non-fatal]\n");
				return;
			}
			slot = *val_int;

			if (slot < 0 ||
				slot >= MAX_SBUS_SLOTS_PER_IOC) {
				if (verbose)
					dr_loginfo("obp_info: bad slot " \
						"number [non-fatal]\n");
				return;
			}

			/*
			 * Allocate structures for the controller.
			 * Since we don't know the name of the controller
			 * at this point in time, just grab the max we
			 * can send.
			 */
			sp = (sbus_configp_t)calloc(1, sizeof (sbus_config_t));
			val_char = (char *)malloc(MAXLEN);
			if (sp == NULL || val_char == NULL) {
				dr_logerr(DRV_FAIL, errno,
						"malloc failed (sbus_config)");
				return;
			}
			sp->name = val_char;
			sp->name[0] = 0;

			if (lp->io.sysio_num == 0) { /* the first IOC */
				if (brdcfgp->ioc0[slot] != NULL) {
					/*
					 * place at the end of the list.
					 */
					tp = brdcfgp->ioc0[slot];
					while (tp->next != NULL)
						tp = tp->next;
					tp->next = sp;
				} else
					brdcfgp->ioc0[slot] = sp;
			} else { /* lp->io.sysio_num better be 1 ! */
				if (brdcfgp->ioc1[slot] != NULL) {
					/*
					 * place at the end of the list.
					 */
					tp = brdcfgp->ioc1[slot];
					while (tp->next != NULL)
						tp = tp->next;
					tp->next = sp;
				} else
					brdcfgp->ioc1[slot] = sp;
			}

			lp->io.cur_sbus = sp;
			lp->io.last_child = level-1;
		}

		/*
		 * Only report the first leaf child, not all the
		 * siblings.
		 */
		if (level > lp->io.last_child) {

			lp->io.last_child = level;

			/* Now create the device name */
			if (getpropval("name", (void *)&val_char)) {
				if (verbose)
				dr_loginfo("obp_info: missing sbus name " \
					"[non-fatal]\n");
				return;
			}

			if (level != lp->io.slot_level)
				strncat(lp->io.cur_sbus->name, "/", MAXLEN-1);
			strncat(lp->io.cur_sbus->name, val_char, MAXLEN-1);
		}

		break;

	case DR_IO_PCI_UNIT:
		if (level < lp->io.slot_level)
			/* Only care about slot nodes or lower */
			return;

		if (level == lp->io.slot_level) {
			/*
			 * Find the slot number.  This is
			 * in the "reg" property.
			 */
			if (getpropval("reg", (void *)&val_int)) {
				if (verbose)
					dr_loginfo("obp_info: missing slot " \
						"number [non-fatal]\n");
				return;
			}

			slot = DEVICEID_UNIT_NUM((*val_int));

			if (slot < 0 ||
				slot >= MAX_PSYCHO) {
				if (verbose)
					dr_loginfo("obp_info: bad slot " \
						"number [non-fatal] %d\n",
						slot);
				return;
			}

			/*
			 * Allocate structures for the controller.
			 * Since we don't know the name of the controller
			 * at this point in time, just grab the max we
			 * can send.
			 */
			sp = (sbus_configp_t)calloc(1, sizeof (sbus_config_t));
			val_char = (char *)malloc(MAXLEN);
			if (sp == NULL || val_char == NULL) {
				dr_logerr(DRV_FAIL, errno,
						"malloc failed (sbus_config)");
				return;
			}
			sp->name = val_char;
			sp->name[0] = 0;

			if (lp->io.sysio_num == 0) { /* the first IOC */
				if (brdcfgp->ioc0[slot] != NULL) {
					/*
					 * place at the end of the list.
					 */
					tp = brdcfgp->ioc0[slot];
					while (tp->next != NULL)
						tp = tp->next;
					tp->next = sp;
				} else
					brdcfgp->ioc0[slot] = sp;
			} else { /* lp->io.sysio_num better be 1 ! */
				if (brdcfgp->ioc1[slot] != NULL) {
					/*
					 * place at the end of the list.
					 */
					tp = brdcfgp->ioc1[slot];
					while (tp->next != NULL)
						tp = tp->next;
					tp->next = sp;
				} else
					brdcfgp->ioc1[slot] = sp;
			}

			lp->io.cur_sbus = sp;
			lp->io.last_child = level-1;
		}

		/*
		 * Only report the first leaf child, not all the
		 * siblings.
		 */
		if (level > lp->io.last_child) {

			lp->io.last_child = level;

			/* Now create the device name */
			if (getpropval("name", (void *)&val_char)) {
				if (verbose)
				dr_loginfo("obp_info: missing sbus name " \
					"[non-fatal]\n");
				return;
			}

			if (level != lp->io.slot_level)
				strncat(lp->io.cur_sbus->name, "/", MAXLEN-1);
			strncat(lp->io.cur_sbus->name, val_char, MAXLEN-1);
		}

		break;

	default:
		dr_loginfo("OBP_info: bad child units\n");
		break;
	}
}
