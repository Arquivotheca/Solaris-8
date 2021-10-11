/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * ur.c -- handles used resource maps and production of
 * 	"used-resources" device tree node
 */

#ident "@(#)ur.c   1.21   99/04/01 SMI"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "types.h"

#include "menu.h"
#include "boot.h"
#include "bop.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "err.h"
#include "ur.h"
#include "pci.h"
#include "pci1275.h"

static u_char irqs_ur[NUM_IRQ] = {0};
static u_char dmas_ur[NUM_DMA] = {0};
static struct range *mem_head_ur = NULL;
static struct range *io_head_ur = NULL;

void used_resources_ur(Board *bp);
void used_irq_ur(u_long irq);
void used_dma_ur(u_long dma);
void used_io_ur(u_long addr, u_long len);
void used_mem_ur(u_long addr, u_long len);
void link_range_ur(struct range *new, struct range **head);
void gather_used_resources_ur(void);
void merge_pci_bus_avail_ur(struct range **avail, struct range **headp);

u_long low_dev_memaddr = 0;

/*
 * Add resources used by the specified Board
 * to the individual resource maps
 * No check is made to ensure the resource is currently unused.
 */
void
used_resources_ur(Board *bp)
{
	u_int j, rc;
	Resource *rp;

	if (bp->bustype == RES_BUS_PCI) {
		if (bp->pci_ppb_io.start)
			used_io_ur(bp->pci_ppb_io.start, bp->pci_ppb_io.len);
		if (bp->pci_ppb_mem.start)
			used_mem_ur(bp->pci_ppb_mem.start,
					bp->pci_ppb_mem.len);
		if (bp->pci_ppb_pmem.start)
			used_mem_ur(bp->pci_ppb_pmem.start,
					bp->pci_ppb_pmem.len);
	}
	rc = resource_count(bp);
	for (j = 0, rp = resource_list(bp); j < rc; j++, rp++) {
		switch (RTYPE(rp)) {
		case RESF_Port:
			used_io_ur(rp->base, rp->length);
			break;
		case RESF_Irq:
			used_irq_ur(rp->base);
			break;
		case RESF_Dma:
			used_dma_ur(rp->base);
			break;
		case RESF_Mem:
			used_mem_ur(rp->base, rp->length);
			break;
		default:
			ASSERT(rp->flags & RESF_ALT);
			break;
		}
	}
}

void
used_irq_ur(u_long irq)
{
	if (irq >= NUM_IRQ) {
		fatal("used_irq() irq %ld out of range\n", irq);
	}
	irqs_ur[irq] = 1;
}

void
used_dma_ur(u_long dma)
{
	if (dma >= NUM_DMA) {
		fatal("used_dma() dma %ld out of range\n", dma);
	}
	dmas_ur[dma] = 1;
}

void
used_io_ur(u_long addr, u_long len)
{
	add_range_ur(addr, len, &io_head_ur);
}

void
used_mem_ur(u_long addr, u_long len)
{
	add_range_ur(addr, len, &mem_head_ur);
}

void
add_range_ur(u_long addr, u_long len, struct range **headp)
{
	struct range *new;

	/*
	 * 1st allocate a new list entry. In some rare
	 * coalescing cases we will free it without use
	 */
	new = (struct range *)malloc((size_t)sizeof (*new));
	if (!new) {
		printf("ERROR: malloc failed\n");
	}
	new->addr = addr;
	new->len = len;
	new->next = NULL;

	link_range_ur(new, headp);
}

void
link_range_ur(struct range *new, struct range **headp)
{
	u_long addr, len;
	struct range *r, *prev, *head;

	if (*headp == NULL) {
		*headp = new;
		return;
	}
	head = *headp;
	addr = new->addr;
	len = new->len;

	/*
	 * Check if at start of chain
	 */
	if (addr <= head->addr) {
		r = head;
		new->next = head;
		*headp = new;
	} else {

		/*
		 * Search for position to insert in ordered list
		 */
		for (prev = NULL, r = head; r != NULL; prev = r, r = r->next) {
			if (addr <= r->addr) {
				break;
			}
		}

		/*
		 * Now we check for coalescing with previous entry.
		 */
		if (addr <= (prev->addr + prev->len)) {
			prev->len = max(prev->len, (addr - prev->addr) + len);

			free(new);
			if ((r != NULL) &&
			    ((prev->addr + prev->len) < r->addr)) {
				return;
			}
			new = prev;
			addr = prev->addr;
			len = prev->len;
		} else {
			new->next = prev->next;
			prev->next = new;
		}
	}

	/*
	 * Finally abosrb all ranges that 'new' overlaps with forward
	 */
	while ((r != NULL) && ((new->addr + new->len) >= r->addr)) {
		new->len = max(new->len, (r->addr - addr) + r->len);
		new->next = r->next;
		free(r);
		r = new->next;
	}
}

/*
 * Gather used resources info, and create the /used-resources node.
 *
 * Note, the flow of this has changed. We now caclculate the resource
 * usage entirely in this routine create the node and release the
 * temporary memory used before exiting.
 */
void
used_resources_node_ur(void)
{
	int i;
	int first;
	char pbuf[80];
	struct range *io;
	struct range *mem;

	debug(D_FLOW, "Building used_resources property\n");

	out_bop("mknod /used-resources\n");

	gather_used_resources_ur();

	/*
	 * Create the properties for the different resources.
	 */

	/*
	 * IRQ
	 */
	for (first = 1, i = 0; i < NUM_IRQ; i++) {
		if (irqs_ur[i]) {
			if (first) {
				(void) sprintf(pbuf,
					"setbinprop interrupts %d", i);
				out_bop(pbuf);
				first = 0;
			} else {
				(void) sprintf(pbuf, ",%d", i);
				out_bop(pbuf);
			}
		}
	}
	if (!first) {
		out_bop("\n");
	}

	/*
	 * DMA
	 */
	for (first = 1, i = 0; i < NUM_DMA; i++) {
		if (dmas_ur[i]) {
			if (first) {
				(void) sprintf(pbuf,
					"setbinprop dma-channels %d", i);
				out_bop(pbuf);
				first = 0;
			} else {
				(void) sprintf(pbuf, ",%d", i);
				out_bop(pbuf);
			}
		}
	}
	if (!first) {
		out_bop("\n");
	}

	/*
	 * IO space
	 */
	for (first = 1, io = io_head_ur; io != NULL; io = io->next) {
		if (first) {
			(void) sprintf(pbuf, "setbinprop io-space 0x%lx,0x%lx",
				io->addr, io->len);
			out_bop(pbuf);
			first = 0;
		} else {
			(void) sprintf(pbuf, ",0x%lx,0x%lx", io->addr, io->len);
			out_bop(pbuf);
		}
	}
	if (!first) {
		out_bop("\n");
	}

	/*
	 * memory space
	 */
	for (first = 1, mem = mem_head_ur; mem != NULL; mem = mem->next) {
		if (first) {
			(void) sprintf(pbuf,
				"setbinprop device-memory 0x%lx,0x%lx",
				mem->addr, mem->len);
			out_bop(pbuf);
			first = 0;
		} else {
			(void) sprintf(pbuf,
				",0x%lx,0x%lx", mem->addr, mem->len);
			out_bop(pbuf);
		}
	}
	if (!first) {
		out_bop("\n");
	}

	/*
	 * Create the property for the lowest device memory address
	 * Allow for 64 bits. No lowest address, will generate a 0,0 value.
	 */
	(void) sprintf(pbuf,
	    "setbinprop low-dev-memaddr 0,0x%lx\n", low_dev_memaddr);
	out_bop(pbuf);

	out_bop("#\n"); /* readability comment */
}

void
gather_used_resources_ur(void)
{
	Board *bp;

	if (pci_bus_io_avail != NULL) {
		merge_pci_bus_avail_ur(pci_bus_io_avail, &io_head_ur);
		free(pci_bus_io_avail);
	}
	if (pci_bus_mem_avail != NULL) {
		merge_pci_bus_avail_ur(pci_bus_mem_avail, &mem_head_ur);
		free(pci_bus_mem_avail);
	}
	if (pci_bus_pmem_avail != NULL) {
		merge_pci_bus_avail_ur(pci_bus_pmem_avail, &mem_head_ur);
		free(pci_bus_pmem_avail);
	}
	for (bp = Head_board; bp; bp = bp->link) {
		used_resources_ur(bp);
	}
}

void
merge_pci_bus_avail_ur(struct range **avail, struct range **headp)
{
	struct range *rp, *next;
	int i;

	for (i = 0; i <= Max_bus_pci; i++) {
		if (avail[i] != NULL) {
			if (*headp == NULL)
				*headp = avail[i];
			else {
				rp = avail[i];
				while (rp) {
					next = rp->next;
					link_range_ur(rp, headp);
					rp = next;
				}
			}
		}
	}
}
