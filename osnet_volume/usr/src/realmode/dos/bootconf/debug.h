/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * debug.h -- public definitions for debug routines
 */

#ifndef	_DEBUG_H
#define	_DEBUG_H

#ident	"@(#)debug.h	1.30	99/10/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>

/* types of debugging messages (for "dtype" argument to debug routine) */
#define	D_BEF_PRINT	0x00000001UL	/* bef printf's */
#define	D_ERR		0x00000004UL	/* errors & warnings */
#define	D_FLOW		0x00000008UL	/* Verbose flow of control */
#define	D_DISP_DEV	0x00000010UL	/* Display the configured devices */
#define	D_PCI		0x00000080UL	/* PCI config space */
#define	D_EISA		0x00000100UL	/* eisa debug */
#define	D_BOOTOPS	0x00000200UL	/* print bootops */
#define	D_ITU		0x00000400UL	/* ITU debug */
#define	D_LBA		0x00000800UL	/* LBA debug */
#define	D_ACPI_VERBOSE	0x00001000UL	/* ACPI debug - Verbose */
#define	D_ACPI_CONFLICT	0x00002000UL	/* ACPI debug - display conflicts */
#define	D_MEMORY	0x00004000UL	/* Memory allocation info */
#define	D_NOFLUSH	0x40000000UL	/* don't flush each debug write */
#define	D_TTY		0x80000000UL	/* print to console & not debug.txt */

void init_debug();
void debug(long dtype, const char *fmt, ...);
void memfail_debug(char *file, int line);
void assfail_debug(char *expr, char *file, int line);

/* Blows up on memory failure (with file & line when debug is enabled) */
#define	MemFailure() memfail_debug(__FILE__, __LINE__)

#if DEBUG
/* ASSERT(ex) causes a program exit if expression ex is false */
#define	ASSERT(EX) if (!(EX)) assfail_debug(#EX, __FILE__, __LINE__)
#else
/* ASSERT is a no-op if we're not deugging */
#define	ASSERT(EX)
#endif

extern unsigned long Debug;	/* -d[mask] (turn on extra debugging info) */
extern FILE *Debug_file;

#ifdef	__cplusplus
}
#endif

#endif	/* _DEBUG_H */
