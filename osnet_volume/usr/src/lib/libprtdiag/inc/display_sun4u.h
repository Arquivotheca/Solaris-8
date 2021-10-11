/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DISPLAY_SUN4U_H
#define	_DISPLAY_SUN4U_H

#pragma ident	"@(#)display_sun4u.h	1.2	99/10/19 SMI"

#include <pdevinfo_sun4u.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Define the memory decode bits for easier reading.  These are from
 * the Sunfire Programmer's Manual.
 */
#define	MEM_SIZE_64M   0x4
#define	MEM_SIZE_256M  0xb
#define	MEM_SIZE_1G    0xf
#define	MEM_SIZE_2G    0x2

#define	MEM_SPEED_50ns 0x0
#define	MEM_SPEED_60ns 0x3
#define	MEM_SPEED_70ns 0x2
#define	MEM_SPEED_80ns 0x1

/*
 * Define strings in this structure as arrays instead of pointers so
 * that copying is easier.
 */
struct io_card {
	int  display;		    /* Should we display this card? */
	int  board;		    /* Board number */
	char bus_type[MAXSTRLEN];   /* Type of bus this IO card is on */
	int  slot;		    /* Slot number */
	char slot_str[MAXSTRLEN];   /* Slot description string */
	int  freq;		    /* Frequency (in MHz) */
	char status[MAXSTRLEN];	    /* Card status */
	char name[MAXSTRLEN];	    /* Card name */
	char model[MAXSTRLEN];	    /* Card model */
	struct io_card *next;
};

int display(Sys_tree *, Prom_node *, struct system_kstat_data *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DISPLAY_SUN4U_H */
