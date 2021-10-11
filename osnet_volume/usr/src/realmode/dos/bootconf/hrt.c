/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * hrt.c -- routines to retrieve available bus resources from
 *	    the PCI Hot-Plug Resource Table (HRT)
 */

#ident "@(#)hrt.c   1.2   99/04/01 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <dostypes.h>
#include "types.h"
#include "ur.h"
#include "mps_table.h"
#include "mpspec.h"
#include "pcihrt.h"
#include "hrt.h"

int hrt_init = 0;
int hrt_entry_cnt = 0;
struct php_entry far *hrt_hpep = NULL;

void hrt_probe(void);

void
hrt_probe()
{
	struct hrt_hdr far *hrtp;

	hrt_init = 1;
#ifdef	HRT_DEBUG
	printf("search PCI Hot-Plug Resource Table starting at 0xF0000\n");
#endif
	if ((hrtp = (struct hrt_hdr far *) find_sig((u_char far *) 0xF0000000,
		0x10000, "$HRT")) == NULL) {
#ifdef	HRT_DEBUG
		printf("NO PCI Hot-Plug Resource Table");
#endif
		return;
	}
#ifdef	HRT_DEBUG
	printf("Found PCI Hot-Plug Resource Table at %lx:-\n", hrtp);
#endif
	if (hrtp->hrt_ver != 1) {
#ifdef	HRT_DEBUG
		printf("PCI Hot-Plug Resource Table version no. <> 1\n");
#endif
		return;
	}
	hrt_entry_cnt = (int) hrtp->hrt_entry_cnt;
#ifdef	HRT_DEBUG
	printf("No. of PCI hot-plug slot entries = 0x%x\n", hrt_entry_cnt);
#endif
	hrt_hpep = (struct php_entry far *) (hrtp + 1);
}

int
hrt_find_bus_res(int bus, int type, struct range **res)
{
	int res_cnt, i;
	struct php_entry far *hpep;

	if (hrt_init == 0)
		hrt_probe();
	if (hrt_hpep == NULL || hrt_entry_cnt == 0)
		return (0);
	hpep = hrt_hpep;
	res_cnt = 0;
	for (i = 0; i < hrt_entry_cnt; i++, hpep++) {
		if (hpep->php_pri_bus != bus)
			continue;
		if (type == IO_TYPE) {
			if (hpep->php_io_start == 0 ||
				hpep->php_io_size == 0)
				continue;
			add_range_ur(hpep->php_io_start, hpep->php_io_size,
					res);
			res_cnt++;
		} else if (type == MEM_TYPE) {
			if (hpep->php_mem_start == 0 ||
				hpep->php_mem_size == 0)
				continue;
			add_range_ur(((u_long) hpep->php_mem_start) << 16,
				((u_long) hpep->php_mem_size) * 64 * 1024,
				res);
			res_cnt++;
		} else if (type == PREFETCH_TYPE) {
			if (hpep->php_pfmem_start == 0 ||
				hpep->php_pfmem_size == 0)
				continue;
			add_range_ur(((u_long) hpep->php_pfmem_start) << 16,
				((u_long) hpep->php_pfmem_size) * 64 * 1024,
				res);
			res_cnt++;
		}
	}
	return (res_cnt);
}

int
hrt_find_bus_range(int bus)
{
	int i, max_bus, sub_bus;
	struct php_entry far *hpep;

	if (hrt_init == 0)
		hrt_probe();
	if (hrt_hpep == NULL || hrt_entry_cnt == 0) {
		return (-1);
	}
	hpep = hrt_hpep;
	max_bus = -1;
	for (i = 0; i < hrt_entry_cnt; i++, hpep++) {
		if (hpep->php_pri_bus != bus)
			continue;
		sub_bus = (int) hpep->php_subord_bus;
		if (sub_bus > max_bus)
			max_bus = sub_bus;
	}
	return (max_bus);
}
