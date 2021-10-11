/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * enum.c - handle enumerators
 */

#ident	"@(#)enum.c	1.52	99/05/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <names.h>
#include "types.h"

#include "bios.h"
#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "eisa.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "main.h"
#include "menu.h"
#include "pci.h"
#include "pnp.h"
#include "pnpbios.h"
#include "probe.h"
#include "resmgmt.h"
#include "tty.h"
#include "vgaprobe.h"
#include "acpi_rm.h"

unsigned char Main_bus = 0;
Board *Head_board = NULL;
Board *Head_prog = NULL;

static int GetBusType_enum();

void
init_enum()
{
	Main_bus = GetBusType_enum();

	init_pnp();
	init_eisa();
	init_pnpbios();
	init_pci();
}

void
program_enum(Board *bp)
{
	/*
	 *  Program a DCD:
	 *
	 *  Calls bus-specific routine to set up resource usage based on
	 *  values recorded in the Board record at "bp".
	 */

	if (bp->flags & BRDF_PGM) {
		/*
		 *  Don't bother unless the device is marked programmable.  If
		 *  it is, the programming itself must be done by bus-specific
		 *  routines.
		 */

		switch (bp->bustype) {

			case RES_BUS_PNPISA: program_pnp(bp);  break;
			case RES_BUS_PCI:    program_pci(bp);  break;
		}
	}
}

void
unprogram_enum(Board *bp)
{
	/*
	 *  Unprogram a DCD:
	 *
	 *  Calls bus-specific routine to release resource usage.
	 */

	if (bp->flags & BRDF_PGM) {
		/*
		 *  Don't bother unless the device is marked programmable.  If
		 *  it is, the programming itself must be done by bus-specific
		 *  routines.
		 */

		switch (bp->bustype) {

			case RES_BUS_PNPISA: unprogram_pnp(bp);  break;
		}
	}
}

/*
 * Create the list of devices
 */
void
run_enum(int enum_option)
{
	if (enum_option == ENUM_ALL || enum_option == ENUM_TOP) {
		/*
		 * First cleanup up any existing devices
		 */
		free_chain_boards(&Head_board);
		free_chain_boards(&Head_prog);

		MotherBoard();

		enumerator_acpi(ACPI_INIT);

		/*
		 * Must run the pnp enumerator before the probe always
		 * befs because we need to take the pnp devices off the bus
		 * in case they are doubly found. For instance, the joyst
		 * would have found the game port at 0x200-207 programmed
		 * by the pnp bios, then the pnp enumerator would take it
		 * off the bus and later reasign different resources as
		 * 200-207 are taken!
		 */
		if (PnpCardCnt != 0)
			enumerator_pnp();

		/*
		 * Pick out the pnpbios parallel and serial port only.
		 * Later on we do the rest.
		 */
		if (Pnpbios)
			enumerator_pnpbios(LPT_COM_PNPBIOS);

		if (enum_option == ENUM_ALL) {
			/*
			 * add standard devices
			 */
			(void) device_probe((char **)0, DPROBE_ALWAYS);
		} else
			return;
	}
	if (enum_option == ENUM_ALL || enum_option == ENUM_BOT) {
		if (Pci)
			enumerator_pci();
		if (Eisa)
			enumerator_eisa();
		if (Pnpbios)
			enumerator_pnpbios(REST_PNPBIOS);

		/*
		 * This is a bit ugly, since the vga may be pci, eisa, isa or
		 * PnP isa we call this routine to ensure that if there is a vga
		 * in the system, a vga node exists somewhere in the board
		 * list. The enumerator_vgaprobe function will
		 * also attach a bunch of properties to the vga node to assist
		 * os level programs to identify the exact Manufacturer and
		 * Model of (s)vga.
		 */
		enumerator_vgaprobe();
		enumerator_bios();

		/*
		 * Read legacy isa info from config file
		 */
		read_escd();

		/*
		 * Add the rest of the ACPI boards to the list
		 */
		enumerator_acpi(ACPI_COMPLETE);

		/*
		 * And finally create some standard aliases.
		 */
		MakeAliases();

		/*
		 * print the device list
		 */
		if (Debug & D_DISP_DEV) {
			Board *bp;

			for (bp = Head_board; bp; bp = bp->link) {
				print_board_enum(bp);
			}
		}
	}
}

/*
 * print specified board configuration
 * - for debug (although permanently compiled in)
 */
void
print_board_enum(Board *bp)
{
	int j;

	debug(D_DISP_DEV, "Slot #%d: %s\n", bp->slot, GetDeviceId_devdb(bp, 0));

	for (j = 1; j < RESF_Max; j++) {

		int rc, k = 0;
		Resource *rp = resource_list(bp);

		for (rc = resource_count(bp); rc--; rp++) {

			if ((rp->flags & RESF_TYPE) == j) {

				if (k++) {
					debug(D_DISP_DEV, ", ");
				} else {
					debug(D_DISP_DEV, "\t%s: ",
					    ResTypes[j]);
				}
				debug(D_DISP_DEV, "0x%lX", rp->base);
				if (rp->length > 1) {
					debug(D_DISP_DEV, "-0x%lX",
					    rp->base + rp->length - 1);
				}
			}
		}

		if (k) {
			debug(D_DISP_DEV, "\n");
		}
	}
}

static int
GetBusType_enum()
{
	/*
	 * Get system bus type:
	 *
	 * Returns RES_BUS_ISA or RES_BUS_EISA based on what
	 * the BIOS claims the primary bus to be.  Note that we don't check
	 * for PCI; this is handled later by the PCI device enumerator.
	 */

	int btp = RES_BUS_ISA;

#ifndef __lint
	_asm {
		push  es
		push  bx
		mov   ax, 0F000h	; Check for EISA
		mov   es, ax
		xor   ax, ax
		mov   bx, 0FFD9h
		cmp   es:[bx], 4945h
		jne   short fin
		cmp   es:[bx+2], 4153h
		jne   short fin
		mov   ax, RES_BUS_EISA
		mov   btp, ax

	fin:	pop   bx
		pop   es
	}
#endif
	return (btp);
}
