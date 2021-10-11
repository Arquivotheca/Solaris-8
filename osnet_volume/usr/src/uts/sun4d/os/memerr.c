/*
 * Copyright (c) 1993,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memerr.c	1.48	97/09/22 SMI"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/mqh.h>
#include <sys/memerr.h>
#include <sys/strlog.h>
#include <sys/sysmacros.h>
#include <sys/obpdefs.h>
#include <sys/promif.h>
#include <sys/debug.h>
#include <sys/syserr.h>

/*
 * Memory Error handling for sun4d
 */

/*
 * TODO
 * what about UE's before memerr_init()?
 * retire pages on repeated CE's
 * recover from UE's if possible
 * If UE on scrubbing (MEMERR_FATAL) on a free page, recover from error
 * rather than returning that errors were found (which panics the system)
 * on init, check if there were any errors already outstanding (mem, or bbus)
 * kmem_alloc mem_unit
 */

/*
 * External Data
 */
extern void nvsimmlist_add(dev_info_t *dip);

/*
 * Static Routines
 */
static int probe_mem_unit(u_int unit);
static u_int addr_to_group(u_int m, u_int b, u_int epg);
static void disable_ECI(u_int unit, u_int bus);
static void enable_ECI(u_int unit, u_int bus);
static void memerr_log(u_int m, u_int b, u_int g, u_int c, u_int s, u_int f);
static u_int ce_where(u_int err_cnt);
/*
 * Static Data
 */

static int n_mem_unit;
#define	MODEL_SC2000	0x2000
#define	MODEL_SC1000	0x1000

/*
 * Criteria used to decide whether to log a CE to the console,
 * the messages file, or to the msgbuf.
 */
static u_int memerr_ce_cons_first = 256;
static u_int memerr_ce_cons_next = 2048;
#ifdef DEBUG
static u_int memerr_ce_cons_last = (16 * 2048);
#else /* DEBUG */
static u_int memerr_ce_cons_last = (256 * 2048);
#endif /* DEBUG */
static u_int memerr_ce_do_msgbuf = 1;

#define	CE_CONSOLE(cnt) ((cnt) && ((cnt) < memerr_ce_cons_last) && \
				(((cnt) == memerr_ce_cons_first) || \
				!((cnt) % memerr_ce_cons_next)))
#define	CE_LOGFILE(cnt)	((cnt) && ((cnt) < memerr_ce_cons_first))
#define	CE_MSGBUF(cnt)	((cnt) && (memerr_ce_do_msgbuf))

static char msg_ce[] = "Corrected ECC Errors:";
static char msg_replace_ce[] = "\tConsider replacing the SIMM";

static char msg_ue[] = "Uncorrectable ECC Errors:";
static char msg_replace_ue[] = "\tReplace the SIMM";

#define	FMT_UNUM "J%d"

#define	MEMERR_LOG_SIMM	(1 << 6)
#define	MEMERR_LOG_GRP	(1 << 7)

#define	LOG_NOT		0
#define	LOG_CONSOLE	1
#define	LOG_FILE	2
#define	LOG_MSGBUF	3

/*
 * on SC-2000, g0 and g1 are on bus0, g2 & g3 are on bus1.
 * on SC-1000, g0, g1, g2, g3 are all on bus0.
 */
#ifdef DEBUG
#define	DEBUG_INC(x) (++(x))
extern void prom_enter_mon(void);
#else
#define	DEBUG_INC(x)
#endif /* DEBUG */

#define	ECI_ON_XD0	(1 << 0)
#define	ECI_ON_XD1	(1 << 1)

struct mem_unit
{
	u_longlong_t mcsr[2];
	struct grp
	{
		u_int start, end;
		u_int simm_ce[MQH_SIMM_PER_GROUP];
		u_int simm_ue[MQH_SIMM_PER_GROUP];
	} group[MQH_GROUP_PER_BOARD];
	u_int base;
	u_int flags;
	u_int pad[2];
} mem_unit[MEM_UNIT_MAX];

#define	G_INDEX(bus, group) (2 * (bus) + (group))

/*
 * ECC Syndrome Table:
 *
 *	This table is used to determine which string to use for printing
 * U numbers of failed or missing SIMMs. The numbers in the table are
 * indexes into arrays of strings. There is an array for each group on
 * a SunDragon system board. A Scorpion board has its own set of string
 * arrays. The numbers 0-3 correspond to SIMMs 0-3. The number 4 corresponds
 * to multiple errors.
 */

static unsigned char	syndrome_table[] = {
	4, 0, 0, 4, 1, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4,
	2, 4, 4, 2, 4, 3, 4, 4, 4, 2, 3, 4, 2, 4, 4, 2,
	2, 4, 4, 2, 4, 3, 0, 4, 4, 4, 3, 4, 2, 4, 4, 2,
	4, 0, 0, 4, 0, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4,
	3, 4, 4, 2, 4, 3, 2, 4, 4, 4, 3, 4, 2, 4, 4, 2,
	4, 1, 1, 4, 1, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 2, 4, 0, 4, 4, 3, 4, 4, 4, 3, 4, 3, 3, 4,
	4, 4, 4, 4, 4, 4, 1, 4, 4, 1, 4, 4, 4, 4, 4, 4,
	3, 4, 4, 2, 4, 3, 4, 4, 4, 0, 3, 4, 2, 4, 4, 2,
	4, 0, 4, 4, 4, 4, 4, 3, 2, 4, 4, 3, 4, 3, 3, 4,
	4, 1, 1, 4, 1, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 1, 4, 4, 1, 4, 4, 4, 4, 4, 4,
	4, 0, 0, 4, 0, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 1, 4, 4, 1, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 1, 4, 4, 1, 4, 4, 4, 4, 4, 4,
	4, 0, 0, 4, 0, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4,
};

#define	U_INDEX(bus, group, simm) (((bus) << 3) + ((group) << 2) + (simm))

/*
 * Blank table to be filled in with data from the OBP property
 * 'unum-table'.
 */
static int obp_unum[] =
{
	0, 0, 0, 0, /* bus0, group0 */
	0, 0, 0, 0, /* bus0, group1 */
	0, 0, 0, 0, /* bus1, group0 */
	0, 0, 0, 0, /* bus1, group1 */
};

/*
 * Predefined SC2000 SIMM connector numbers.
 */
static int unum_sc2000[] =
{
	4300, 4200, 4100, 4000, /* bus0, group0 */
	3900, 3800, 3700, 3600,	/* bus0, group1 */
	5100, 5000, 4900, 4800, /* bus1, group0 */
	4700, 4600, 4500, 4400, /* bus1, group1 */
};

/*
 * Predefined SS1000 SIMM connector numbers.
 */
static int unum_sc1000[] =
{
	2800, 2900, 3000, 3100, /* bus0, group0 */
	3200, 3300, 3400, 3500, /* bus0, group1 */
	3600, 3700, 3800, 3900, /* bus0, group2 */
	4000, 4100, 4200, 4300, /* bus0, group3 */
};

static int *memerr_unums;

/*
 * This table has the strings for the bit positions. This is the
 * original table from LabPOST.
 */

char	*syndrome_strs[] = {
	"*", "C0", "C1", "D", "C2", "D", "D", "T",
	"C3", "D", "D", "T", "D", "T", "T", "Q",
	"C4", "D", "D", "32", "D", "57", "MU", "D",
	"D", "37", "49", "D", "40", "D", "D", "44",
	"C5", "D", "D", "33", "D", "61", "4", "D",
	"D", "MU", "53", "D", "45", "D", "D", "41",
	"D", "0", "1", "D", "10", "D", "D", "MU",
	"15", "D", "D", "MU", "D", "T", "T", "D",
	"C6", "D", "D", "42", "D", "59", "39", "D",
	"D", "MU", "51", "D", "34", "D", "D", "46",
	"D", "25", "29", "D", "27", "Q", "D", "MU",
	"31", "D", "Q", "MU", "D", "MU", "MU", "D",
	"D", "MU", "36", "D", "7", "D", "D", "54",
	"MU", "D", "D", "62", "D", "48", "56", "D",
	"T", "D", "D", "MU", "D", "MU", "22", "D",
	"D", "18", "MU", "D", "T", "D", "D", "MU",
	"C7", "D", "D", "47", "D", "63", "MU", "D",
	"D", "6", "55", "D", "35", "D", "D", "43",
	"D", "5", "MU", "D", "MU", "D", "D", "50",
	"38", "D", "D", "58", "D", "52", "60", "D",
	"D", "17", "21", "D", "19", "Q", "D", "MU",
	"23", "D", "Q", "MU", "D", "MU", "MU", "D",
	"T", "D", "D", "MU", "D", "MU", "30", "D",
	"D", "26", "MU", "D", "T", "D", "D", "MU",
	"D", "8", "13", "D", "2", "D", "D", "T",
	"3", "D", "D", "T", "D", "MU", "MU", "D",
	"T", "D", "D", "T", "D", "MU", "16", "D",
	"D", "20", "MU", "D", "MU", "D", "D", "MU",
	"T", "D", "D", "T", "D", "MU", "24", "D",
	"D", "28", "MU", "D", "MU", "D", "D", "MU",
	"Q", "12", "9", "D", "14", "D", "D", "MU",
	"11", "D", "D", "MU", "D", "MU", "MU", "Q"
};

/*
 * we have a mem-unit with a status property indicating a failure.
 * if it is actually a good mem-unit, return 1, else 0.
 */
#define	OBP_FAIL_MQH01	"fail-MQH0 MQH1"
#define	OBP_FAIL_XD_01	"fail-XDBus0 XDBus1"
#define	OBP_FAIL_MQH1	"fail-MQH1"
#define	OBP_FAIL_XD_MQH1	"fail-XDBus1 MQH1"
#define	OBP_FAIL_MQH0	"fail-MQH0"
#define	OBP_FAIL_XD_MQH0	"fail-XDBus0 MQH0"
#define	OBP_STATUS	"status"

int
mem_unit_good(char *status)
{
	if ((sun4d_model != MODEL_SC2000) || (n_xdbus != 1) ||
		(good_xdbus == -1)) {
#ifdef DEBUG
	cmn_err(CE_CONT, "?mem_unit_good: n/a model: 0x%x, n_xdbus %d,"
		" good %d\n", sun4d_model, n_xdbus, good_xdbus);
#endif /* DEBUG */
		return (0);
	}

	/*
	 * if both MQH or both XDBus are bad, no good
	 */
	if ((strncmp(status, OBP_FAIL_MQH01, strlen(OBP_FAIL_MQH01)) == 0) ||
	    (strncmp(status, OBP_FAIL_XD_01, strlen(OBP_FAIL_XD_01)) == 0)) {
#ifdef DEBUG
		cmn_err(CE_CONT, "?mem_unit_good: BAD:" OBP_FAIL_MQH01 "\n");
#endif /* DEBUG */
		return (0);
	}

	/*
	 * bad mqh0
	 */
	if ((strncmp(status, OBP_FAIL_MQH0, strlen(OBP_FAIL_MQH0)) == 0) ||
	    (strncmp(status, OBP_FAIL_XD_MQH0, strlen(OBP_FAIL_XD_MQH0))
		== 0)) {
		if (good_xdbus == 1) {
#ifdef DEBUG
			cmn_err(CE_CONT, "?mem_unit_good1: GOOD:" OBP_FAIL_MQH0
				"\n");
#endif /* DEBUG */
			return (1);
		} else {
#ifdef DEBUG
			cmn_err(CE_CONT, "?mem_unit_good0: BAD:" OBP_FAIL_MQH0
				"\n");
#endif /* DEBUG */
			return (0);
		}
	}

	/*
	 * bad mqh1
	 */
	if ((strncmp(status, OBP_FAIL_MQH1, strlen(OBP_FAIL_MQH1)) == 0) ||
	    (strncmp(status, OBP_FAIL_XD_MQH1, strlen(OBP_FAIL_XD_MQH1))
		== 0)) {
		if (good_xdbus == 0) {
#ifdef DEBUG
			cmn_err(CE_CONT, "?mem_unit_good0: GOOD:" OBP_FAIL_MQH1
				"\n");
#endif /* DEBUG */
			return (1);
		} else {
#ifdef DEBUG
			cmn_err(CE_CONT, "?mem_unit_good1: BAD:" OBP_FAIL_MQH1
				"\n");
#endif /* DEBUG */
			return (0);
		}
	}
#ifdef DEBUG
	cmn_err(CE_CONT, "?mem_unit_good: BAD\n");
#endif /* DEBUG */
	return (0);
}

/*
 * memerr_init()
 * set up data structures used by memerr().
 *
 * find all the mem-units in the device tree
 * record their MQH base addresses and their group sizes
 * in the mem_unit[] array.
 *
 * returns 0 on success
 */


u_int
memerr_init(void)
{
	dev_info_t *dip;

	/*
	 * Check if a Unumber table exists in the PROM. This will
	 * supercede any data hard-coded in the kernel. If table
	 * is present, then load it into main memory.
	 */
	if (prom_getprop(prom_rootnode(), "unum-table", (caddr_t)&obp_unum) ==
		sizeof (obp_unum)) {
		memerr_unums = obp_unum;
	} else if (sun4d_model == MODEL_SC2000) {
		memerr_unums = unum_sc2000;
	} else {
		memerr_unums = unum_sc1000;
	}

	dip = ddi_root_node();

	/*
	 * search 1st level children in devinfo tree
	 */
	dip = ddi_get_child(dip);	/* 1st child of root */
	while (dip) {
		char *name;
		int devid;

		name = ddi_get_name(dip);
		/*
		 * if find one, calculate addresses of the mqh control reg and
		 * counters
		 */
		if (strcmp("mem-unit", name) == 0) {
			int unit = n_mem_unit;

			/*
			 * if (mem-unit is marked "bad")
			 * then continue;
			 */

			devid = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
			    DDI_PROP_DONTPASS, PROP_DEVICE_ID, -1);
			if (devid == -1) {
				cmn_err(CE_WARN,
				    "memerr_init(): no %s for %s\n",
				    PROP_DEVICE_ID, name);
				return (1);
			}
			mem_unit[unit].base = (u_int) (MQH_CSR_BASE |
			    (devid << MQH_DEVID_SHIFT));

#ifdef DEBUG
			cmn_err(CE_CONT, "?mem-unit "
				"board %d (0x%x)\n",
				mqh_board(devid), mem_unit[unit].base);
#endif /* DEBUG */

			if (probe_mem_unit(unit)) {
				n_mem_unit = unit + 1;		/* populated */
			} else {
				mem_unit[unit].base = 0;	/* vacant */
			}
		} else if ((strcmp("SUNW,nvtwo", name) == 0 ||
		    strcmp("nvtwo", name) == 0)) {
			nvsimmlist_add(dip);
		}
		dip = ddi_get_next_sibling(dip);
	}

	/* found nothing */
	if (n_mem_unit == 0) {
		cmn_err(CE_WARN, "memerr_init: no mem-unit found!");
		return (1);
	}

	return (0);
}

/*
 * POST has not configured this group
 * if there are simms present, tell the user.
 */
static void
check_bad_group(u_int mqh_base, u_int b, u_int g, u_longlong_t gtr)
{
	/*
	 * if no simms present, simply return
	 */
	if (gtr == (u_longlong_t)0 || gtr == (u_longlong_t)~0)
		return;

	/*
	 * there is at least 1 simm present in the group.
	 * we could be here because there are missing or
	 * mis-matched simms, ECC errors, or because the
	 * number of good groups on each bus does not match.
	 *
	 * we could decode the gtr to figure out if it is missing
	 * or mis-matched, but we don't bother.
	 *
	 * print all 4 u-numbers.
	 */
	cmn_err(CE_WARN, "Board %d bus %d memory group %d is unavailable ("
		FMT_UNUM " " FMT_UNUM " " FMT_UNUM " " FMT_UNUM ")",
		MQH_BASE_TO_BOARD(mqh_base), b, g,
		memerr_unums[U_INDEX(b, g, 0)],
		memerr_unums[U_INDEX(b, g, 1)],
		memerr_unums[U_INDEX(b, g, 2)],
		memerr_unums[U_INDEX(b, g, 3)]);
}

/*
 * for the Given memory unit, fill in the struct mem_unit with
 * the existing MCSR info (needed by memerr_ECI), and the group info
 * (needed to identify which group is responsible for an ECC error).
 *
 * called with mem_unit[unit].base filled in.
 * returns how many populated groups found;
 */
static int
probe_mem_unit(u_int unit)
{
	u_int b;
	u_char groups_found = 0;

	for (b = 0; b < n_xdbus; ++b) {
		u_int grp_cnt = 0;
		u_int g;
		u_int mqh_base;
		u_int bus_is_populated = 0;

		mqh_base = mem_unit[unit].base;
		mqh_base |= (b << MQH_CSR_BUS_SHIFT);

#define	POST_DEBUG
#ifdef POST_DEBUG
		{
			static u_int did_warning;
			u_longlong_t mcsr;
			mcsr = mqh_mcsr_get(mqh_base);
			mem_unit[unit].mcsr[b] = mcsr;

			if (!did_warning && (mcsr & MQH_MCSR_ECI_BIT)) {
				did_warning = 1;
				cmn_err(CE_CONT,
					"?NOTE: BETA1.3 PROM: MQH 0x%x "
					"ECI already enabled (0x%x.%x)",
					mqh_base,
					(u_int) (mcsr >> 32), (u_int) mcsr);
			}
		}
#endif /* POST_DEBUG */

		/*
		 * how many groups on this MQH?
		 */

		if (sun4d_model == MODEL_SC2000)
			grp_cnt = 2;
		else if (sun4d_model == MODEL_SC1000)
			grp_cnt = 4;

		for (g = 0; g < grp_cnt; g++) {
			u_int base, size;
			u_int adr;
			u_longlong_t gtr;
			struct grp *grp;

			grp = &(mem_unit[unit].group[G_INDEX(b, g)]);

			switch (g) {
			case 0:	adr = mqh_get_adr0(mqh_base);
				gtr = mqh_get_gtr0(mqh_base);
				break;
			case 1:	adr = mqh_get_adr1(mqh_base);
				gtr = mqh_get_gtr1(mqh_base);
				break;
			case 2:	adr = mqh_get_adr2(mqh_base);
				gtr = mqh_get_gtr2(mqh_base);
				break;
			case 3:	adr = mqh_get_adr3(mqh_base);
				gtr = mqh_get_gtr3(mqh_base);
				break;
			}

			if (ADR_SSIZE(adr) == 0) {
				/* nobody home */
				grp->start = 0;
				grp->end = 0;

				check_bad_group(mqh_base, b, g, gtr);

				continue;
			}
			base = base_8M_to_4K(ADR_BASE(adr));

			switch (ADR_SSIZE(adr)) {
			case 0:
				size = 0;
				break;

			case SSIZE_8MB:
				size = btop(0x00800000u);
				break;

			case SSIZE_32MB:
				size = btop(0x02000000u);
				break;

			case SSIZE_128MB:
				size = btop(0x08000000u);
				break;

			case SSIZE_512MB:
				size = btop(0x20000000u);
				break;

			case SSIZE_2GB:
				size = btop(0x80000000u);
				break;
			}

#ifdef DEBUG
			cmn_err(CE_CONT, "?\t%d MB: bus %d group %d"
				" IF 0x%x IV 0x%x base 0x%x\n",
				size/256, b, g, ADR_IF(adr),
				ADR_IV(adr), base);
#endif /* DEBUG */

			/*
			 * double size for 2 XDBus
			 */
			if (n_xdbus == 2)
				size = size << 1;

			/*
			 * multiply for interleaving factor
			 */
			size = size << ADR_IF(adr);


			grp->start = base;
			grp->end = base + size;

			grp->simm_ce[0] = 0;
			grp->simm_ce[1] = 0;
			grp->simm_ce[2] = 0;
			grp->simm_ce[3] = 0;

			grp->simm_ue[0] = 0;
			grp->simm_ue[1] = 0;
			grp->simm_ue[2] = 0;
			grp->simm_ue[3] = 0;

			if (size != 0) {
				bus_is_populated++;
				groups_found++;
			}
		}
		if (bus_is_populated) {
			enable_ECI(unit, b);
		}
	}
	return (groups_found);
}

/*
 * mem-units start life out with ECI disabled
 * when we probe a mem-unit and find it configured, we call enable_ECI()
 * to set the state such that memerr_ECI(1) will enable ECI on this MQH.
 */
static void
enable_ECI(u_int unit, u_int bus)
{
	if (bus == 1)
		mem_unit[unit].flags |= ECI_ON_XD1;
	else
		mem_unit[unit].flags |= ECI_ON_XD0;
}

/*
 * called when ECI is off for everybody
 * sets some state so that memerr_ECI(1) will not re-enable ECI
 * for the specified memory bank
 */
static void
disable_ECI(u_int unit, u_int bus)
{
	u_int flag;

	if (bus == 1)
		flag = ECI_ON_XD1;
	else
		flag = ECI_ON_XD0;

	if (mem_unit[unit].flags & flag) {
		cmn_err(CE_NOTE, "Board %d Bus %d:"
			" Disabling ECC interrupts.",
			MQH_BASE_TO_BOARD(mem_unit[unit].base), bus);
		mem_unit[unit].flags &= ~flag;
	}
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
/*
 * Turn on correctable error reporting.
 * parameter enable: 1 means enable, 0 means disable
 * if (enable), then also blast the MQH CE_ADDR register
 * to be sure interrupts aren't disabled due to a previous error.
 */
void
memerr_ECI(u_int enable)
{
	int unit, b;

	/* for each mem unit */
	for (unit = 0; unit < n_mem_unit; unit++) {
		if (mem_unit[unit].base == 0)	/* not populated */
			continue;
		/*
		 * for each bus running through the mem_unit
		 */
		for (b = 0; b < (int)n_xdbus; ++b) {
			u_int mqh_base;
			u_longlong_t new_mcsr;

			/*
			 * skip enabling ECI for requested banks.
			 */
			if (enable && (b == 0))
				if (!(mem_unit[unit].flags & ECI_ON_XD0))
					continue;

			if (enable && (b == 1))
				if (!(mem_unit[unit].flags & ECI_ON_XD1))
					continue;

			mqh_base = mem_unit[unit].base;

			mqh_base |= (b << MQH_CSR_BUS_SHIFT);

			new_mcsr = mem_unit[unit].mcsr[b];

			if (enable)
				new_mcsr = (new_mcsr | MQH_MCSR_ECI_BIT);
			else
				new_mcsr = (new_mcsr & ~MQH_MCSR_ECI_BIT);

			mqh_mcsr_set(new_mcsr, mqh_base);

			/*
			 * if we turn on ECI when there is already a CE
			 * in the CE_ERR_ADDR, then the MQH will not send
			 * any interrupt.  So if we're enabling ECI,
			 * clear CE_ERR_ADDR so it will work again.
			 */
			/*
			 * If the CE occurred before ECI = 1, then we'll
			 * never be notified.  If a CE occurred between
			 * setting ECI = 1 and clearing the CE_ERR_ADDR,
			 * then we'll get a level15, and depending on
			 * who runs the handler, it will race with this
			 * clear to either find data in the CE_ADDR,
			 * or find nothing there.
			 */

			if (enable) {
				mqh_set_ce_addr((u_longlong_t)0, mqh_base);
			}
		}
	}
}

#ifdef DEBUG
static u_int memerr_ce_found;
static u_int memerr_ce_me_found;
static u_int memerr_ce_called;
static u_int memerr_ue_found;
static u_int memerr_ue_me_found;
static u_int memerr_ue_called;
#endif /* DEBUG */

/*
 * poll all the MQH's for errors.
 * log any error's found, and
 * return number of errors found
 *
 * Does not know if this was a processor or DVMA access
 */

u_int
memerr(u_int type)
{
	int m;	/* mem-unit index */
	u_int errors_found = 0;
	int aflt_handled(u_longlong_t, int);

#ifdef DEBUG
	if (type & MEMERR_CE)
		DEBUG_INC(memerr_ce_called);
	else
		DEBUG_INC(memerr_ue_called);
#endif /* DEBUG */
	for (m = 0; m < n_mem_unit; ++m) {
	    /* for each mem-unit, m */
	    u_int b;	/* bus index */

	    if (mem_unit[m].base == 0)
		continue;

	    for (b = 0; b < n_xdbus; ++b)	/* for each bus, b */
	    {
		u_int mqh_base;
		u_int g;
		u_longlong_t err_addr;
		u_int s;	/* simm number */
		u_int err_cnt;

		mqh_base = mem_unit[m].base;
		mqh_base |= (b << MQH_CSR_BUS_SHIFT);

		/*
		 * MQH bug:  if we read an MQH's registers while
		 * it is transmitting an interrupt, we risk confusing
		 * the MQH and causing a sytem watchdog reset.
		 * So we get here only when ECI is off.
		 */

		ASSERT(!(MQH_MCSR_ECI_BIT & mqh_mcsr_get(mqh_base)));

		if (type & MEMERR_CE)
			err_addr = mqh_get_ce_addr(mqh_base);
		else
			err_addr = mqh_get_ue_addr(mqh_base);

		/*
		 * if no error recorded here, continue to next
		 */
		if (!(err_addr & MQH_ERR_ERR_BIT))
			continue;

		/*
		 * else we found an error.
		 */

		/*
		 * since we have no use for it, we don't call
		 * mqh_get_ce_data(mqh_base) to get the data or
		 * mqh_get_ue_data(mqh_base) to get the data.
		 */

		/*
		 * Clear the MQH [CE|UE]_ADDR.
		 * for CE, use a write, for UE, use a swap to gurarntee
		 * valididity of the  MQH_ERR_ME_BIT.
		 *
		 * if we're gonna panic (MEMERR_FATAL), leave the
		 * ERR.ADDR intact for POST to find it.
		 */
		if (type & MEMERR_CE)
			mqh_set_ce_addr(0, mqh_base);
		else if (!(type & MEMERR_FATAL))
			err_addr = err_addr | ((u_longlong_t)
				swapa_2f(0, mqh_base | MQH_OFF_UE_ADDR) << 32);

#ifdef DEBUG
		if (type == MEMERR_CE)
			DEBUG_INC(memerr_ce_found);
		else
			DEBUG_INC(memerr_ue_found);
#endif /* DEBUG */

		/*
		 * If a driver claims to have "handled" the error,
		 * act as if it never occurred.
		 */

		if (aflt_handled(err_addr & MQH_ERR_ADDR_MASK,
			type == MEMERR_UE ? 1 : 0)) {
			/*
			 * if survived a UE, clear MQH UE address register
			 * to prevent blocking future MQH interrupts
			 */
			if (type & MEMERR_FATAL)
				mqh_set_ue_addr(0, mqh_base);

			continue;
		}

		err_cnt = 1;

		/*
		 * if MQH_ERR_ME_BIT is set, then we had at least 2
		 * errors since the last time MQH [UE|CE]_ADDR was cleared.
		 * We call it 2 errors for the purposes of logging stats,
		 * and credit 2 errors to the SIMM(s) that
		 * we identify with the error address.
		 */
		if (err_addr & MQH_ERR_ME_BIT) {
			err_cnt = 2;
#ifdef DEBUG
			if (type == MEMERR_CE)
				DEBUG_INC(memerr_ce_me_found);
			else
				DEBUG_INC(memerr_ue_me_found);
#endif /* DEBUG */
		}

		errors_found += err_cnt;

		g = addr_to_group(m, b, MQH_ERR_TO_PAGE(err_addr));

		if (g == MQH_GROUP_PER_BOARD) {
			/*
			 * it is a (non-fatal) kernel bug if this happens,
			 * the consequence is we can't identify the group.
			 */
			cmn_err(CE_WARN, "memerr(): %s Board %d Bus %d",
				(type & MEMERR_UE) ? "UE": "CE",
				MQH_BASE_TO_BOARD(mqh_base), b);
			break;
		}

		s = syndrome_table[MQH_ERR_TO_SYN(err_addr)];

		if (s == MQH_SIMM_PER_GROUP) {
			/*
			 * syndrome couldn't identify which SIMM in group,
			 * log all four.
			 */
			memerr_log(m, b, g, err_cnt, s, type | MEMERR_LOG_GRP);

		} else if (sun4d_model == MODEL_SC2000 && n_xdbus == 1) {
			/*
			 * We're a single bus SC2000, and we can
			 * identify the physical index of the good bus.
			 */
			if (good_xdbus != -1) {
				memerr_log(m, good_xdbus, g, err_cnt, s,
					MEMERR_CE | MEMERR_LOG_SIMM);
			} else {
				/*
				 * (non-fatal) kernel bug
				 * don't know bus #.
				 */
				cmn_err(CE_WARN, "memerr(): %s Board %d Bus ?"
					" grp %d simm %d\n",
				(type & MEMERR_UE) ? "UE": "CE",
				MQH_BASE_TO_BOARD(mqh_base), g, s);
			}
		} else {
			/*
			 * syndrome identified a single simm, log it.
			 */
			memerr_log(m, b, g, err_cnt, s,
				MEMERR_CE | MEMERR_LOG_SIMM);
		}
	    }		/* end for bus b */
	}		/* end for mem unit m */

	return (errors_found);
}

static u_int
ce_where(u_int count)
{

	if (CE_CONSOLE(count))
		return (LOG_CONSOLE);

	if (CE_LOGFILE(count))
		return (LOG_FILE);

	if (CE_MSGBUF(count))
		return (LOG_MSGBUF);

	return (LOG_NOT);
}

static void
memerr_log(u_int m, u_int b, u_int g, u_int err_cnt, u_int s, u_int flags)
{
	int simm_num0, simm_num1, simm_num2, simm_num3;
	u_int mqh_base;
	char buf[100];
	u_int lw;
	u_int *counters;
	u_int count = 0;

	if (flags & MEMERR_UE)
		counters = (mem_unit[m].group[G_INDEX(b, g)]).simm_ue;
	else
		counters = (mem_unit[m].group[G_INDEX(b, g)]).simm_ce;

	mqh_base = mem_unit[m].base;
	mqh_base |= (b << MQH_CSR_BUS_SHIFT);

	if (flags & MEMERR_LOG_SIMM) {
		count = err_cnt + counters[s];
		counters[s] = count;

		simm_num0 = memerr_unums[U_INDEX(b, g, s)];

		(void) sprintf(buf, "%d %s Board %d, SIMM "
			FMT_UNUM "\n",
			count,
			flags & MEMERR_CE ? msg_ce : msg_ue,
			MQH_BASE_TO_BOARD(mqh_base),
			simm_num0);

	} else if (flags & MEMERR_LOG_GRP) {
		u_int tmp;

		tmp = err_cnt + counters[0];
		counters[0] = tmp;
		count = MAX(count, tmp);

		tmp = err_cnt + counters[1];
		counters[1] = tmp;
		count = MAX(count, tmp);

		tmp = err_cnt + counters[2];
		counters[2] = tmp;
		count = MAX(count, tmp);

		tmp = err_cnt + counters[3];
		counters[3] = tmp;
		count = MAX(count, tmp);

		simm_num0 = memerr_unums[U_INDEX(b, g, 0)];
		simm_num1 = memerr_unums[U_INDEX(b, g, 1)];
		simm_num2 = memerr_unums[U_INDEX(b, g, 2)];
		simm_num3 = memerr_unums[U_INDEX(b, g, 3)];

		(void) sprintf(buf, "%d %s Board %d, SIMM "
			FMT_UNUM " " FMT_UNUM " "
			FMT_UNUM " " FMT_UNUM "\n",
			count,
			flags & MEMERR_CE ? msg_ce : msg_ue,
			MQH_BASE_TO_BOARD(mqh_base),
			simm_num0, simm_num1,
			simm_num2, simm_num3);
	}

	if (flags & MEMERR_CE) {
		/*
		 * ce_where will be fooled if err_cnt makes us skip
		 * past a threshold -- so if ce_count is 2, round down.
		 */
		if (err_cnt == 2) {
			count &= ~1;
		}
		lw = ce_where(count);
	}
	else
		lw = LOG_CONSOLE;

	if (lw == LOG_CONSOLE)
		cmn_err(CE_WARN, "%s%s", buf, (flags & MEMERR_UE) ?
			msg_replace_ue : msg_replace_ce);
	else if (lw == LOG_FILE)
		(void) strlog(0, 0, 0, SL_CONSOLE | SL_NOTE, buf, 0);
	else if (lw == LOG_MSGBUF)
		cmn_err(CE_CONT, "!%s", buf);

	/*
	 * note that we disable ECI for the whole MQH
	 * based on the pestering from a single SIMM
	 */
	if ((flags & MEMERR_CE) && (count >= memerr_ce_cons_last))
		disable_ECI(m, b);
}

/*
 * Find which group contains this address.
 * by taking the physical page number and sifting through the groups
 * in mem_unit[] for the specfied unit and bus.
 * returns MQH_GROUP_PER_BOARD on failure;
 */
static u_int
addr_to_group(u_int m, u_int b, u_int epg)
{
	u_int g;

	for (g = 0; g < (MQH_GROUP_PER_BOARD/n_xdbus); ++g) {
		if ((epg >= mem_unit[m].group[G_INDEX(b, g)].start) &&
		    (epg < mem_unit[m].group[G_INDEX(b, g)].end))
			return (g);
	}

	return (MQH_GROUP_PER_BOARD);
}
