/*
 * Copyright (c) 1991 Sun Microsystems, Inc.
 */

#ifndef	_ALIAS_BOOT_H
#define	_ALIAS_BOOT_H

#ident	"@(#)alias_boot.h	1.1	92/04/17 SMI"

/*
 * Offsets for string constants used in alias bootstrap.
 */
#define	LDSO_S		0		/* "/usr/lib/ld.so.n" */
#define	ZERO_S		1		/* "/dev/zero" */
#define	EMPTY_S		2		/* "(null)" */
#define	S_MAX		3		/* count of strings */

/*
 * Offsets for function pointers used in alias bootstrap.
 */
#define	PANIC_F		0		/* panic() */
#define	OPEN_F		1		/* open() */
#define	MMAP_F		2		/* mmap() */
#define	FSTAT_F		3		/* fstat() */
#define	SYSCONFIG_F	4		/* sysconfig() */
#define	CLOSE_F		5		/* close() */
#define	MUNMAP_F	6		/* munmap() */
#define	F_MAX		7		/* count of functions */

#endif	/* _ALIAS_BOOT_H */
