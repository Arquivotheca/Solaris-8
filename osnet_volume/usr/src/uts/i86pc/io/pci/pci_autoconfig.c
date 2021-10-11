/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_autoconfig.c	1.36	96/08/22 SMI"

/*
 * Determine the PCI configuration mechanism recommended by the BIOS.
 */

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/pci_impl.h>
#include <sys/ddi_subrdefs.h>
#include <sys/bootconf.h>
#include <sys/psw.h>
#include <sys/modctl.h>
#include <sys/errno.h>

/*
 * Internal structures and functions
 */
int pci_bios_cfg_type = PCI_MECHANISM_UNKNOWN;
static  int pci_nounload = 0;

/*
 * Internal routines
 */
static int pci_check_bios(void);

/*
 * Interface routines
 */
static int pci_check(void);

/*
 * In 2.5.x, this module supplied a whole bunch of interfaces to the kernel.
 * Most of them have been replaced by the 2.6 boot subsystem.  All that
 * remains is a call to the BIOS to determine what configuration mechanism
 * to use.
 */
static struct impl_bus_promops pci_promops = {
	"pci",
	0,
	0,
	0,
	0,
	pci_check,
	0,
	0,
	0,
	0,
	0,
	0,
	0,	/* next ptr */
};

static struct modlmisc modlmisc = {
	&mod_miscops, "PCI BIOS interface"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	impl_bus_add_promops(&pci_promops);
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	if (pci_nounload)
		return (EBUSY);
	impl_bus_delete_promops(&pci_promops);
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * This code determines if this system supports PCI and which
 * type of configuration access method is used
 */

static int
pci_check(void)
{
	/*
	 * Only do this once.  NB:  If this is not a PCI system, and we
	 * get called twice, we can't detect it and will probably die
	 * horribly when we try to ask the BIOS whether PCI is present.
	 * This code is safe *ONLY* during system startup when the
	 * bootstrap is still available.
	 */
	if (pci_bios_cfg_type != PCI_MECHANISM_UNKNOWN)
		return (DDI_SUCCESS);

	pci_bios_cfg_type = pci_check_bios();

	if (pci_bios_cfg_type == PCI_MECHANISM_NONE)
		return (DDI_FAILURE);

	pci_nounload = 1;

	return (DDI_SUCCESS);
}

#define	PCI_FUNCTION_ID		(0xb1)
#define	PCI_BIOS_PRESENT	(0x1)

/*
 * NOTE: DO NOT use the debugger to step thru BOP_DOINT(). The
 * debugger also uses the doint() function which is non-reenterant.
 */
static int	pci_carryflag;
static unsigned long	pci_edx;
static ushort	pci_ax;

static int
pci_check_bios(void)
{
	struct bop_regs regs;

	regs.eax.word.ax = (PCI_FUNCTION_ID << 8) | PCI_BIOS_PRESENT;

	BOP_DOINT(bootops, 0x1a, &regs);
	pci_carryflag = regs.eflags & PS_C;
	pci_ax = regs.eax.word.ax;
	pci_edx = regs.edx.edx;

	/* the carry flag must not be set */
	if (pci_carryflag != 0)
		return (PCI_MECHANISM_NONE);

	if (pci_edx != ('P' | 'C'<<8 | 'I'<<16 | ' '<<24))
		return (PCI_MECHANISM_NONE);

	/* ah (the high byte of ax) must be zero */
	if ((pci_ax & 0xff00) != 0)
		return (PCI_MECHANISM_NONE);

	switch (pci_ax & 0x3) {
	default:	/* ?!? */
	case 0:		/* supports neither? */
		return (PCI_MECHANISM_NONE);

	case 1:
	case 3:		/* supports both */
		return (PCI_MECHANISM_1);

	case 2:
		return (PCI_MECHANISM_2);
	}
}
