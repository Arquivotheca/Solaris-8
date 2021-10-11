/*
 * Copyright (c) 1992 Sun Microsystems, Inc.
 */

#ifndef	_DISPLAY_H
#define	_DISPLAY_H

#pragma ident	"@(#)display.h	1.8	95/08/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int logging;
extern int print_flag;

#define	NOPRINT		0
#define	PRINT		1
#define	MX_SBUS_SLOTS	4

/*
 * Define a structure to contain both the DRAM SIMM and NVRAM
 * SIMM memory totals in MBytes.
 */
struct mem_total {
	int dram;
	int nvsimm;
};

/* Functions in common display.c module */
void disp_powerfail(Prom_node *);
void log_printf(char *, ...);
char *get_time(u_char *);
void print_header(int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DISPLAY_H */
