/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memctrl.c	1.6	98/05/31 SMI"

/*
 * Starfire Memory Controller specific routines.
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/dditypes.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi_impldefs.h>
#include <sys/promif.h>
#include <sys/machsystm.h>

#include <sys/starfire.h>

struct mc_dimm_table {
	int	mc_type;
	int	mc_module_size;		/* module size in MB */
};

static struct mc_dimm_table dimmsize_table[] = {
	{ 4,	8 },
	{ 6,	8 },
	{ 11,	32 },
	{ 15,	128 },
	{ 0,	0 }
};

/*
 * Alignment of memory between MC's.
 */
uint64_t
mc_get_mem_alignment()
{
	return (STARFIRE_MC_MEMBOARD_ALIGNMENT);
}

uint64_t
mc_get_asr_addr(dnode_t nodeid)
{
	int		rlen;
	uint64_t	psi_addr;
	struct sf_memunit_regspec	reg;

	rlen = prom_getproplen(nodeid, "reg");
	if (rlen != sizeof (struct sf_memunit_regspec))
		return ((uint64_t)-1);

	if (prom_getprop(nodeid, "reg", (caddr_t)&reg) < 0)
		return ((uint64_t)-1);

	psi_addr = ((uint64_t)reg.regspec_addr_hi) << 32;
	psi_addr |= (uint64_t)reg.regspec_addr_lo;

	return (STARFIRE_MC_ASR_ADDR(psi_addr));
}

uint64_t
mc_get_idle_addr(dnode_t nodeid)
{
	int		rlen;
	uint64_t	psi_addr;
	struct sf_memunit_regspec	reg;

	rlen = prom_getproplen(nodeid, "reg");
	if (rlen != sizeof (struct sf_memunit_regspec))
		return ((uint64_t)-1);

	if (prom_getprop(nodeid, "reg", (caddr_t)&reg) < 0)
		return ((uint64_t)-1);

	psi_addr = ((uint64_t)reg.regspec_addr_hi) << 32;
	psi_addr |= (uint64_t)reg.regspec_addr_lo;

	return (STARFIRE_MC_IDLE_ADDR(psi_addr));
}

int
mc_get_dimm_size(dnode_t nodeid)
{
	uint64_t	psi_addr;
	uint_t		dimmtype;
	int		i, rlen;
	struct sf_memunit_regspec	reg;

	rlen = prom_getproplen(nodeid, "reg");
	if (rlen != sizeof (struct sf_memunit_regspec))
		return (-1);

	if (prom_getprop(nodeid, "reg", (caddr_t)&reg) < 0)
		return (-1);

	psi_addr = ((uint64_t)reg.regspec_addr_hi) << 32;
	psi_addr |= (uint64_t)reg.regspec_addr_lo;
	psi_addr = STARFIRE_MC_DIMMTYPE_ADDR(psi_addr);

	if (psi_addr == (uint64_t)-1)
		return (-1);

	dimmtype = ldphysio(psi_addr);
	dimmtype &= STARFIRE_MC_DIMMSIZE_MASK;

	for (i = 0; dimmsize_table[i].mc_type != 0; i++)
		if (dimmsize_table[i].mc_type == dimmtype)
			break;

	return (dimmsize_table[i].mc_module_size);
}

int
mc_read_asr(dnode_t nodeid, uint_t *mcregp)
{
	uint64_t	psi_addr;

	*mcregp = 0;

	psi_addr = mc_get_asr_addr(nodeid);
	if (psi_addr == (uint64_t)-1)
		return (-1);

	*mcregp = ldphysio(psi_addr);

	return (0);
}

int
mc_write_asr(dnode_t nodeid, uint_t mcreg)
{
	uint_t		mcreg_rd;
	uint64_t	psi_addr;

	psi_addr = mc_get_asr_addr(nodeid);
	if (psi_addr == (uint64_t)-1)
		return (-1);

	stphysio(psi_addr, mcreg);

	mcreg_rd = ldphysio(psi_addr);
	ASSERT(mcreg_rd == mcreg);

	return ((mcreg_rd != mcreg) ? -1 : 0);
}

uint64_t
mc_asr_to_pa(uint_t mcreg)
{
	uint64_t	pa, masr, addrmask, lowbitmask;

	/*
	 * Remove memory present bit.
	 */
	masr = (uint64_t)(mcreg & ~STARFIRE_MC_MEM_PRESENT_MASK);
	/*
	 * Get mask for bits 32-26.
	 */
	lowbitmask = masr & (uint64_t)STARFIRE_MC_MASK_MASK;
	lowbitmask <<= STARFIRE_MC_MASK_SHIFT;
	addrmask = STARFIRE_MC_ADDR_HIBITS | lowbitmask;

	pa = (masr << STARFIRE_MC_BASE_SHIFT) & addrmask;

	return (pa);
}
