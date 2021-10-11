/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * acpi_rm.h -- acpi definitions
 */

#ifndef	_ACPI_RM_H
#define	_ACPI_RM_H

#ident	"@(#)acpi_rm.h	1.1	99/05/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * acpi_copy - copy ACPI board infos from boot.bin (protected memory) to
 * bootconf.exe (realmode).
 * It takes a acpi_bc structure as noted below.
 */
struct acpi_bc {
	unsigned long bc_buf;		/* 32-bit linear buf addr */
	unsigned long bc_this;		/* this board address to copy */
	unsigned long bc_next;		/* next board addres to copy */
	unsigned short bc_buflen;	/* length of buf in bytes */
	unsigned short bc_nextlen;	/* length of next board */
	unsigned long bc_flag;
};

int acpi_copy(struct acpi_bc far *bcp);

void enumerator_acpi(int phase);
Board *acpi_check(Board *bp);

/*
 * enumerator_acpi() phase definitions
 */
#define	ACPI_INIT	0	/* copy acpi boards from boot.bin */
#define	ACPI_COMPLETE	1	/* add the rest of acpi boards */

/* default return value for OK */
#define	ACPI_OK		0

#ifdef	__cplusplus
}
#endif

#endif	/* _ACPI_RM_H */
