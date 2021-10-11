/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)conf_ultra.c	1.2	99/11/20 SMI"

#include <sys/types.h>
#include <string.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <libintl.h>

#include "libcpc.h"
#include "libcpc_impl.h"

/*
 * Configuration data for UltraSPARC performance counters.
 *
 * Definitions taken from [1] and [2].  See the references to
 * understand what any of these settings actually means.
 *
 * Note that in the current draft of [2], there is some re-use
 * of existing bit assignments in the various fields of the %pcr
 * register - this may change before FCS.
 *
 * [1] "UltraSPARC I & II User's Manual," January 1997.
 * [2] "UltraSPARC-III Programmer's Reference Manual," April 1999.
 */

#define	V_US12	(1u << 0)	/* specific to UltraSPARC 1 and 2 */
#define	V_US3	(1u << 1)	/* specific to UltraSPARC 3 */
#define	V_END	(1u << 31)

/*
 * map from "cpu version" to flag bits
 */
static const uint_t cpuvermap[] = {
	V_US12,			/* CPC_ULTRA1 */
	V_US12,			/* CPC_ULTRA2 */
	V_US3			/* CPC_ULTRA3 */
};

struct nametable {
	const uint_t	ver;
	const uint8_t	bits;
	const char	*name;
};

/*
 * Definitions for counter 0
 */

#define	USall_EVENTS_0(v)					\
	{v,		0x0,	"Cycle_cnt"},			\
	{v,		0x1,	"Instr_cnt"},			\
	{v,		0x2,	"Dispatch0_IC_miss"},		\
	{v,		0x8,	"IC_ref"},			\
	{v,		0x9,	"DC_rd"},			\
	{v,		0xa,	"DC_wr"},			\
	{v,		0xc,	"EC_ref"},			\
	{v,		0xe,	"EC_snoop_inv"}

static const struct nametable US12_names0[] = {
	USall_EVENTS_0(V_US12),
	{V_US12,	0x3,	"Dispatch0_storeBuf"},
	{V_US12,	0xb,	"Load_use"},
	{V_US12,	0xd,	"EC_write_hit_RDO"},
	{V_US12,	0xf,	"EC_rd_hit"},
	{V_END}
};

static const struct nametable US3_names0[] = {
	USall_EVENTS_0(V_US3),
	{V_US3,		0x3,	"Dispatch0_br_target"},
	{V_US3,		0x4,	"Dispatch0_2nd_br"},
	{V_US3,		0x5,	"Rstall_storeQ"},
	{V_US3,		0x6,	"Rstall_IU_use"},
	{V_US3,		0xd,	"EC_write_hit_RTO"},
	{V_US3,		0xf,	"EC_rd_miss"},
	{V_US3,		0x10,	"PC_port0_rd"},
	{V_US3,		0x11,	"SI_snoop"},
	{V_US3,		0x12,	"SI_ciq_flow"},
	{V_US3,		0x13,	"SI_owned"},
	{V_US3,		0x14,	"SW_count_0"},
	{V_US3,		0x15,	"IU_Stat_Br_miss_taken"},
	{V_US3,		0x16,	"IU_Stat_Br_count_taken"},
	{V_US3,		0x17,	"Dispatch_rs_mispred"},
	{V_US3,		0x18,	"FA_pipe_completion"},
	{V_US3,		0x20,	"MC_reads_0"},
	{V_US3,		0x21,	"MC_reads_1"},
	{V_US3,		0x22,	"MC_reads_2"},
	{V_US3,		0x23,	"MC_reads_3"},
	{V_US3,		0x24,	"MC_stalls_0"},
	{V_US3,		0x25,	"MC_stalls_2"},
	{V_END}
};

#undef	USall_EVENTS_0

#define	USall_EVENTS_1(v)					\
	{v,		0x0,	"Cycle_cnt"},			\
	{v,		0x1,	"Instr_cnt"},			\
	{v,		0x2,	"Dispatch0_mispred"},		\
	{v,		0xd,	"EC_wb"},			\
	{v,		0xe,	"EC_snoop_cb"}

static const struct nametable US12_names1[] = {
	USall_EVENTS_1(V_US12),
	{V_US12,	0x3,	"Dispatch0_FP_use"},
	{V_US12,	0x8,	"IC_hit"},
	{V_US12,	0x9,	"DC_rd_hit"},
	{V_US12,	0xa,	"DC_wr_hit"},
	{V_US12,	0xb,	"Load_use_RAW"},
	{V_US12,	0xc,	"EC_hit"},
	{V_US12,	0xf,	"EC_ic_hit"},
	{V_END}
};

static const struct nametable US3_names1[] = {
	USall_EVENTS_1(V_US3),
	{V_US3,		0x3,	"IC_miss_cancelled"},
	{V_US3,		0x4,	"Re_endian_miss"},
	{V_US3,		0x5,	"Re_FPU_bypass"},
	{V_US3,		0x6,	"Re_DC_miss"},
	{V_US3,		0x7,	"Re_EC_miss"},
	{V_US3,		0x8,	"IC_miss"},
	{V_US3,		0x9,	"DC_rd_miss"},
	{V_US3,		0xa,	"DC_wr_miss"},
	{V_US3,		0xb,	"Rstall_FP_use"},
	{V_US3,		0xc,	"EC_misses"},
	{V_US3,		0xf,	"EC_ic_miss"},
	{V_US3,		0x10,	"Re_PC_miss"},
	{V_US3,		0x11,	"ITLB_miss"},
	{V_US3,		0x12,	"DTLB_miss"},
	{V_US3,		0x13,	"WC_miss"},
	{V_US3,		0x14,	"WC_snoop_cb"},
	{V_US3,		0x15,	"WC_scrubbed"},
	{V_US3,		0x16,	"WC_wb_wo_read"},
	{V_US3,		0x18,	"PC_soft_hit"},
	{V_US3,		0x19,	"PC_snoop_inv"},
	{V_US3,		0x1a,	"PC_hard_hit"},
	{V_US3,		0x1b,	"PC_port1_rd"},
	{V_US3,		0x1c,	"SW_count_1"},
	{V_US3,		0x1d,	"IU_Stat_Br_miss_untaken"},
	{V_US3,		0x1e,	"IU_Stat_Br_count_untaken"},
	{V_US3,		0x1f,	"PC_MS_misses"},
	{V_US3,		0x20,	"MC_writes_0"},
	{V_US3,		0x21,	"MC_writes_1"},
	{V_US3,		0x22,	"MC_writes_2"},
	{V_US3,		0x23,	"MC_writes_3"},
	{V_US3,		0x24,	"MC_stalls_1"},
	{V_US3,		0x25,	"MC_stalls_3"},
	{V_US3,		0x26,	"Re_RAW_miss"},
	{V_US3,		0x27,	"FM_pipe_completion"},
	{V_END}
};

#undef	USall_EVENTS_1

static const struct nametable *US12_names[2] = {
	US12_names0,
	US12_names1
};

static const struct nametable *US3_names[2] = {
	US3_names0,
	US3_names1
};

#define	MAPCPUVER(cpuver)	(cpuvermap[(cpuver) - CPC_ULTRA1])

static int
validargs(int cpuver, int regno)
{
	if (regno < 0 || regno > 1)
		return (0);
	cpuver -= CPC_ULTRA1;
	if (cpuver < 0 ||
	    cpuver >= sizeof (cpuvermap) / sizeof (cpuvermap[0]))
		return (0);
	return (1);
}

/*ARGSUSED*/
static int
versionmatch(int cpuver, int regno, const struct nametable *n)
{
	if (!validargs(cpuver, regno) || n->ver != MAPCPUVER(cpuver))
		return (0);
	return (1);
}

static const struct nametable *
getnametable(int cpuver, int regno)
{
	const struct nametable *n;

	if (!validargs(cpuver, regno))
		return (NULL);

	switch (MAPCPUVER(cpuver)) {
	case V_US12:
		n = US12_names[regno];
		break;
	case V_US3:
		n = US3_names[regno];
		break;
	default:
		n = NULL;
		break;
	}
	return (n);
}

void
cpc_walk_names(int cpuver, int regno, void *arg,
    void (*action)(void *, int, const char *, uint8_t))
{
	const struct nametable *n;

	if ((n = getnametable(cpuver, regno)) == NULL)
		return;
	for (; n->ver != V_END; n++)
		if (versionmatch(cpuver, regno, n))
			action(arg, regno, n->name, n->bits);
}

const char *
__cpc_reg_to_name(int cpuver, int regno, uint8_t bits)
{
	const struct nametable *n;

	if ((n = getnametable(cpuver, regno)) == NULL)
		return (NULL);
	for (; n->ver != V_END; n++)
		if (bits == n->bits && versionmatch(cpuver, regno, n))
			return (n->name);
	return (NULL);
}

/*
 * Register names can be specified as strings or even as numbers
 */
int
__cpc_name_to_reg(int cpuver, int regno, const char *name, uint8_t *bits)
{
	const struct nametable *n;
	char *eptr = NULL;
	long value;

	if ((n = getnametable(cpuver, regno)) == NULL || name == NULL)
		return (-1);

	for (; n->ver != V_END; n++)
		if (strcmp(name, n->name) == 0 &&
		    versionmatch(cpuver, regno, n)) {
			*bits = n->bits;
			return (0);
		}

	value = strtol(name, &eptr, 0);
	if (name != eptr && value >= 0 && value <= UINT8_MAX) {
		*bits = (uint8_t)value;
		return (0);
	}

	return (-1);
}

const char *
cpc_getcciname(int cpuver)
{
	if (validargs(cpuver, 0))
		switch (MAPCPUVER(cpuver)) {
		case V_US12:
			return ("UltraSPARC I&II");
		case V_US3:
			return ("UltraSPARC III");
		default:
			break;
		}
	return (NULL);
}

const char *
cpc_getcpuref(int cpuver)
{
	if (validargs(cpuver, 0))
		switch (MAPCPUVER(cpuver)) {
		case V_US12:
			return (gettext(
			    "See Appendix B of the \"UltraSPARC I&II "
			    "User\'s Manual,\" STP1031A."));
		case V_US3:
			return (gettext(
			    "See Chapter 14 of the \"UltraSPARC III "
			    "Programmer\'s Reference Manual.\""));
		default:
			break;
		}
	return (NULL);
}

/*
 * This is a functional interface to allow CPUs with fewer %pic registers
 * to share the same data structure as those with more %pic registers
 * within the same instruction family.
 */
uint_t
cpc_getnpic(int cpuver)
{
	cpc_event_t *event;

	switch (cpuver) {
	case CPC_ULTRA1:
	case CPC_ULTRA2:
	case CPC_ULTRA3:
		return (sizeof (event->ce_pic) / sizeof	(event->ce_pic[0]));
	default:
		return (0);
	}
}

#include <sys/systeminfo.h>

/*
 * Return the version of the current processor.
 *
 * Version -1 is defined as 'not performance counter capable'
 *
 * XXX	Need a more accurate way to get this from the kernel.
 *	Perhaps libdevinfo might help?  Or perhaps we should
 *	offer a way to tunnel into the attribute-value pairs on
 *	a cpu devinfo node .. libcpuattr?
 */
int
cpc_getcpuver(void)
{
	static int ver = -1;

	if (ver == -1) {
		char tmp;
		size_t bufsize = sysinfo(SI_ISALIST, &tmp, 1);
		char *buf;

		if ((buf = malloc(bufsize)) != NULL) {
			if (sysinfo(SI_ISALIST, buf, bufsize) == bufsize) {
				if (strstr(buf, "sparcv9+vis2") != NULL)
					ver = CPC_ULTRA3;
				else if (strstr(buf, "sparcv8plus+vis") != NULL)
					ver = CPC_ULTRA1;
			}
			free(buf);
		}
	}
	return (ver);
}
