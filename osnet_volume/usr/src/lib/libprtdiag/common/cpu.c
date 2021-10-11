/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)cpu.c	1.2	99/10/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <kstat.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/dkio.h>
#include "pdevinfo.h"
#include "display.h"
#include "pdevinfo_sun4u.h"
#include "display_sun4u.h"
#include "libprtdiag.h"

static	int get_cpu_freq(Prom_node *);
static	int get_ecache_size(Prom_node *);
static	void display_cpus(Board_node *);

void	display_cpu_devices(Sys_tree *);
void	display_mid(int mid);


/*
 * Return the operating frequency of a processor in Hertz. This function
 * requires as input a legal prom node pointer. If a NULL
 * is passed in or the clock-frequency property does not exist, the
 * function returns 0.
 */
static int
get_cpu_freq(Prom_node *pnode)
{
	Prop *prop;
	int *value;

	/* find the property */
	if ((prop = find_prop(pnode, "clock-frequency")) == NULL) {
		return (0);
	}

	if ((value = (int *)get_prop_val(prop)) == NULL) {
		return (0);
	}

	return (*value);
}

/*
 * returns the size of the given processors external cache in
 * bytes. If the properties required to determine this are not
 * present, then the function returns 0.
 */
static int
get_ecache_size(Prom_node *node)
{
	int *cache_size_p;	/* pointer to number of cache lines */

	/* find the properties */
	if ((cache_size_p = (int *)get_prop_val(find_prop(node,
		"ecache-size"))) == NULL) {
		return (0);
	}

	return (*cache_size_p);
}


/*
 * This routine is the generic link into displaying CPU and memory info.
 * It displays the table header, then calls the CPU and memory display
 * routine for all boards.
 */
void
display_cpu_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/*
	* Display the table header for CPUs . Then display the CPU
	* frequency, cache size, and processor revision of all cpus.
	*/
	log_printf("\n", 0);
	log_printf("=========================", 0);
	log_printf(" CPUs ", 0);
	log_printf("=========================", 0);
	log_printf("\n", 0);
	log_printf("\n", 0);
	log_printf("                    Run   Ecache  "
		" CPU    CPU\n", 0);
	log_printf("Brd  CPU   Module   MHz     MB    "
		"Impl.   Mask\n", 0);
	log_printf("---  ---  -------  -----  ------  "
		"------  ----\n", 0);

	/* Now display all of the cpus on each board */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_cpus(bnode);
	bnode = bnode->next;
	}

	log_printf("\n", 0);
}

/*
 * Display the CPUs present on this board.
 */
static void
display_cpus(Board_node *board)
{
	Prom_node *cpu;

	/*
	 * display the CPUs' operating frequency, cache size, impl. field
	 * and mask revision.
	 */
	for (cpu = dev_find_type(board->nodes, "cpu"); cpu != NULL;
	    cpu = dev_next_type(cpu, "cpu")) {
		int freq;	 /* CPU clock frequency */
		int ecache_size; /* External cache size */
		int *mid;
		int *impl;
		int *mask;

		mid = (int *)get_prop_val(find_prop(cpu, "upa-portid"));
		if (mid == NULL) {
			mid = (int *)get_prop_val(find_prop(cpu, "portid"));
		}

		freq = (get_cpu_freq(cpu) + 500000) / 1000000;
		ecache_size = get_ecache_size(cpu);
		impl = (int *)get_prop_val(find_prop(cpu, "implementation#"));
		mask = (int *)get_prop_val(find_prop(cpu, "mask#"));

		/* Do not display a failed CPU node */
		if ((freq != 0) && (node_failed(cpu) == 0)) {
			/* Board number */
			display_boardnum(board->board_num);

			/* CPU MID */
			log_printf(" %2d  ", *mid, 0);

			/* Module number */
			display_mid(*mid);

			/* Running frequency */
			log_printf(" %3d   ", freq, 0);

			/* Ecache size */
			if (ecache_size == 0)
				log_printf(" %3s    ", "N/A", 0);
			else
				log_printf(" %4.1f   ",
					(float)ecache_size / (float)(1<<20),
					0);

			/* Implementation */
			if (impl == NULL) {
				log_printf("%6s  ", "N/A", 0);
			} else {
				if (*impl == 0x10)
					log_printf("%-6s  ", "US-I", 0);
				else if (*impl == 0x11)
					log_printf("%-6s  ", "US-II", 0);
				else if (*impl == 0x14)
					log_printf("%-6s  ", "US-III", 0);
				else
					log_printf("%-6x  ", *impl, 0);
			}

			/* CPU Mask */
			if (mask == NULL) {
				log_printf(" %3s", "N/A", 0);
			} else {
				log_printf(" %d.%d", (*mask >> 4) & 0xf,
					*mask & 0xf, 0);
			}

			log_printf("\n", 0);
		}
	}
}

void
display_mid(int mid)
{
	log_printf("  %2d     ", mid, 0);
}
