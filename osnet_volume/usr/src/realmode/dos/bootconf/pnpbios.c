/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pnpbios.c -- handles pnp bios
 */

#ident "@(#)pnpbios.c   1.25   99/05/28 SMI"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <names.h>
#include "types.h"

#include "boards.h"
#include "boot.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "escd.h"
#include "pci.h"
#include "pnp.h"
#include "pnpbios.h"
#include "probe.h"
#include "resmgmt.h"
#include "tree.h"
#include "tty.h"
#include "ur.h"
#include "acpi_rm.h"

pnpbios_hdr_t *bhp;
int Pnpbios = 0;
int Parallel_ports_found = 0;
int Serial_ports_found = 0;

Board *extract_resources_pnpbios(dev_node_t *dnp);
Board *GetLargeResource(Board *bp, u_char *cp, u_char tag);
Board *GetSmallResource(Board *bp, u_char *cp, u_char tag);
void add_weak_attr_pnpbios(Board *bp, dev_node_t *dnp);

#define	IDE_PNPBIOS(dnp) (((dnp)->base_type == PCI_CLASS_MASS) && \
	((dnp)->sub_type == PCI_MASS_IDE))
#define	PIC_PNPBIOS(dnp) (((dnp)->base_type == PCI_CLASS_PERIPH) && \
	((dnp)->sub_type == PCI_PERIPH_PIC))

/*
 * Check for a plug and play bios according to the pnp bios spec:
 * Search from physical 0xf0000 to 0xffff0 at 16 bytes increments
 * looking for the "$PnP" string. If found doubly check by
 * computing the checksum, which should be 0.
 *
 * Saves the bios header structure pointer in the global bhp and sets
 * Pnpbios if found.
 */

void
init_pnpbios()
{
	char *fp;
	int i;
	u_char checksum;
	pnpbios_hdr_t *check = (pnpbios_hdr_t *)0;

	for (bhp = (pnpbios_hdr_t *)0xf0000000;
	    (u_long) bhp < 0xffff0000;
	    bhp = (pnpbios_hdr_t *)((u_long) bhp + 0x10000)) {
		if (strncmp(bhp->signature, "$PnP", 4) == 0) {
			for (i = 0, checksum = 0, fp = (char *)bhp;
			    i < bhp->length; i++, fp++) {
				checksum += *fp;
			}
			/*
			 * If the length is less than the offset to the
			 * protected mode data segment ignore this pnpbios.
			 * It just means we've found a machine that doesn't
			 * have a valid entry in the rm_data_seg member and
			 * if we use the callback (*rm_entry) we'll get bogus
			 * results.
			 * The same goes for the version if less the 1.0.
			 */
			if ((bhp->length <
			    (u_char)(&check->pm_data_seg)) ||
			    (bhp->version < 0x10)) {
				continue;
			}
			if (checksum == 0) {
				debug(D_FLOW, "pnpbios found\n");
				Pnpbios = 1;
				return;
			}
		}
	}
	debug(D_FLOW, "pnpbios not found\n");
}

int
get_pnp_info_pnpbios(u_char *num_csns, u_int *readport)
{
	struct {
		u_char revision;
		u_char num_csns;
		u_int readport;
		u_int reserved;
	} config;

	if (!Pnpbios) {
		return (1); /* error */
	}
	if ((*(bhp->rm_entry))(PNPBIOS_GET_PNP_ISA_INFO,
	    &config, bhp->rm_data_seg)) {
		debug(D_ERR, "bad get pnpbios conf\n");
		return (1); /* error */
	}
	*num_csns = config.num_csns;
	*readport = config.readport;
	debug(D_FLOW, "pnpbios: num PnP isa csns 0x%x, read port 0x%x\n",
		config.num_csns, config.readport);
	return (0); /* success */
}

void
enumerator_pnpbios(int phase)
{
	int devno = 0;
	u_char node_count;
	u_int max_node_size;
	dev_node_t *dnp;
	Board *bp;

	if (Pnpbios) {
		/*
		 * Get the maximum device node size, and malloc it.
		 */
		if ((*(bhp->rm_entry))(PNPBIOS_GET_NODE_COUNT,
		    &node_count, &max_node_size, bhp->rm_data_seg)) {
			debug(D_ERR, "bad pnpbios get node count\n");
			return;
		}

		if (!(dnp = (dev_node_t *)malloc(max_node_size))) {
			MemFailure();
		}

		/*
		 * Get each device, and check its resources
		 */
		do {
			if ((*(bhp->rm_entry))(PNPBIOS_GET_DEV_NODE,
			    &devno,
			    dnp,
			    PNPBIOS_GET_CURRENT_INFO,
			    bhp->rm_data_seg)) {
				debug(D_ERR, "bad pnpbios get dev node\n");
				return;
			}
			switch (phase) {
			case LPT_COM_PNPBIOS:
				if ((dnp->base_type == PCI_CLASS_COMM) &&
				    (bp = extract_resources_pnpbios(dnp))) {
					add_weak_attr_pnpbios(bp, dnp);
				}
				break;
			case REST_PNPBIOS:
				if (dnp->base_type != PCI_CLASS_COMM) {
					(void) extract_resources_pnpbios(dnp);
				}
				break;
			}
		} while (devno != 0xff);
		free(dnp);
	}
}

Board *
extract_resources_pnpbios(dev_node_t *dnp)
{
	u_char *cp;
	u_char tag;
	Board *bp;
	Resource *rp;
	u_int cnt;
	int changed;

	/*
	 * If the device is IDE or a PIC then ignore it.
	 *
	 * Note, IDE controllers are ignored because they often seem
	 * to be bad. For instance, some have 3 sets of io ports, others
	 * have the ports in the wrong order (eg 3f6 then 1f0-1f7)
	 * Another failure is caused by some pnpbioses claiming 3f6-3f7
	 * which conflicts with the floppy.
	 * The legacy ata.bef will find the controller instead, and create
	 * the standardised node.
	 *
	 * We remove pics because we already have them covered (in the
	 * motherboard device) and the AddResource_devdb code translates
	 * an irq of 2 to 9 (erroneously in this case).
	 */
	if (IDE_PNPBIOS(dnp) || PIC_PNPBIOS(dnp)) {
		return (0);
	}

	/*
	 * Create the new device
	 */
	bp = new_board();
	bp->bustype = RES_BUS_ISA;
	bp->category = DCAT_UNKNWN;
	bp->devid = dnp->eisa_id;
	bp->flags = BRDF_PNPBIOS;
	bp->pnpbios_base_type = dnp->base_type;
	bp->pnpbios_sub_type = dnp->sub_type;
	bp->pnpbios_interface_type = dnp->interface_type;

	cp = dnp->resources;

	for (tag = *cp++; tag != END_TAG; tag = *cp++) {
		/*
		 *  Read thru the resource info, and extract model resource
		 *  information according to type.
		 */

		if (tag & 0x80) {
			bp = GetLargeResource(bp, cp, tag);
			cp += (*(int *)cp) + 2;
		} else {
			bp = GetSmallResource(bp, cp, tag);
			cp += tag & 7;
		}
	}

	/*
	 * We link a new board in the global device chain if all
	 * the resources are unclaimed. If all resources are claimed
	 * we throw away the Board. Finally if there are some resources
	 * claimed by other boards we create an unclaimed node.
	 * This is to ensure that any dynamically configured devices
	 * don't grab these resources.
	 */
	rp = resource_list(bp);
	for (changed = 0, cnt = resource_count(bp); cnt; cnt--) {
		int t = rp->flags & RESF_TYPE;
		u_long base = rp->base;
		u_long len = rp->length;

		if (Query_resmgmt(Head_board, t, base, len)) {
			(void) DelResource_devdb(bp, rp);
			changed = 1;
		} else {
			rp++;
		}
	}

	/*
	 * If there are no resources left then ignore it.
	 */
	if (resource_count(bp) == 0) {
		free_board(bp);
		return (0);
	}

	if (changed) {
		/*
		 * Ensure Board is not put in the tree.
		 * We can't guarantee uniqueness if the board has
		 * no io or memory resources, and secondly it could
		 * conflict with a legitimate node of the same name
		 */
		bp->flags |= BRDF_NOTREE;
	}

	/* check with ACPI's list */
	if ((bp = acpi_check(bp)) != NULL)
		add_board(bp); /* Tell rest of system about device */
	return (bp);
}

Board *
GetLargeResource(Board *bp, u_char *cp, u_char tag)
{
	u_long base, len;
#ifdef NOTDEF
	u_int resource_len = *(u_int *) cp;
#endif

	cp += 2; /* skip over resource length */

	switch (tag) {

#ifdef NOTDEF
	/*
	 * If we ever need to get the ansi name which only 1 out of 7
	 * pnpbioses I tested had then it can be extracted with this case
	 */
	case CARD_IDENTIFIER_ANSI:
		tmp = *(cp + resource_len);
		*(cp + resource_len) = 0;
		printf("Ansi name: %s\n", cp);
		*(cp + resource_len) = tmp;
		break;
#endif

	case MEMORY_RANGE_DESCRIPTOR:
		if (*(u_int *)(cp + 1) != *(u_int *)(cp + 3)) {
			debug(D_ERR, "ERROR: 24bit mem min and max differ\n");
			return (bp);
		}
		len = *(u_int *)(cp + 7);
		if (len) {
			base = *(u_int *)(cp + 1);
			base <<= 8; /* convert to 24 bit addr */
			len <<= 8; /* convert to 24 bit addr */
			bp = AddResource_devdb(bp, RESF_Mem, base, len);
		}

		break;

	case MEMORY32_RANGE_DESCRIPTOR:
		if (*(u_long *)(cp + 1) != *(u_long *)(cp + 5)) {
			debug(D_ERR, "ERROR: 32bit mem min and max differ\n");
			return (bp);
		}
		len = *(u_long *)(cp + 0xd);
		if (len) {
			base = *(u_long *)(cp + 1);
			bp = AddResource_devdb(bp, RESF_Mem, base, len);
		}
		break;

	case MEMORY32_FIXED_DESCRIPTOR:
		len = *(u_long *)(cp + 5);
		if (len) {
			base = *(u_long *)(cp + 1);
			bp = AddResource_devdb(bp, RESF_Mem, base, len);
		}
		break;
	}
	return (bp);
}

Board *
GetSmallResource(Board *bp, u_char *cp, u_char tag)
{
	u_char dmas;
	u_char dma;
	u_int irqs;
	u_int irq;
	u_long base, len;

	switch (tag & 0x78) {

	case FIXED_LOCATION_IO_DESCRIPTOR:
		len = *(cp + 2);
		if (len) {
			base = *(u_int *)cp;
			bp = AddResource_devdb(bp, RESF_Port, base, len);
		}
		break;

	case IO_PORT_DESCRIPTOR:
		/*
		 * This io port is in the configured resources
		 * So the start and end better be the same
		 */
		if (*(u_int *)(cp + 1) != *(u_int *)(cp + 3)) {
			debug(D_ERR, "ERROR: IO port min and max differ\n");
			return (bp);
		}
		len = *(cp + 6);
		if (len) {
			base = *(u_int *)(cp + 1);
			bp = AddResource_devdb(bp, RESF_Port, base, len);
		}
		break;

	case DMA_FORMAT:
		for (dmas = *cp; dmas; dmas ^= (1 << dma)) {
			dma = ffbs(dmas) - 1;
			bp = AddResource_devdb(bp, RESF_Dma, dma, 1);
		}
		break;

	case IRQ_FORMAT:
		for (irqs = *(u_int *)cp; irqs; irqs ^= (1 << irq)) {
			irq = ffbs(irqs) - 1;
			bp = AddResource_devdb(bp, RESF_Irq, irq, 1);
		}
		break;
	}
	return (bp);
}

/*
 * Traditionally serial and parallel ports had a fixed mapping
 * from io ports to irqs. With the advent of pnpbioses this became
 * programmable. The mapping was stored by the bios in the pnpbios nvram.
 * So now we have to check the nvram to check if the device
 * is recorded and use its resources. Otherwise, later we run the
 * com and lpt half befs to find these devices.
 *
 * Both these devices set the weak attribute on resources as follows:-
 *	serial: irq
 *	parallel: irq & io ports
 *
 * This routine is only called for serial and parallel ports.
 */
void
add_weak_attr_pnpbios(Board *bp, dev_node_t *dnp)
{
	Resource *rp = resource_list(bp);
	int j;

	for (j = resource_count(bp); j--; rp++) {
		if (RTYPE(rp) == RESF_Irq) {
			rp->flags |= RESF_WEAK;
		}
		if ((RTYPE(rp) == RESF_Port) &&
		    (dnp->sub_type == PCI_COMM_PARALLEL)) {
			rp->flags |= RESF_WEAK;
		}
	}

	/*
	 * change the devid for parallel and serial ports to
	 * that of a device found in our master file. there's
	 * at least on machine (John.Fong@Eng) who's serial
	 * devices are reported as PNP0510 and TOS7007 even
	 * though they're 16550's.
	 */
	switch (dnp->sub_type) {
	case PCI_COMM_PARALLEL:
		Parallel_ports_found = 1;
		bp->devid = CompressName("PNP0400");
		break;
	case PCI_COMM_GENERIC_XT:
		Serial_ports_found = 1;
		bp->devid = CompressName("PNP0501");
		break;
	}
}
