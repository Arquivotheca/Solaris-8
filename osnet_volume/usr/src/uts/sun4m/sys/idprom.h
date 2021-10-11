/*
 * Copyright (c) 1986-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_IDPROM_H
#define	_SYS_IDPROM_H

#pragma ident	"@(#)idprom.h	1.2	96/02/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/*
 * Structure declaration for ID prom in CPU and Ethernet boards
 */
typedef struct idprom {
	unsigned char	id_format;	/* format identifier */
	/* The following fields are valid only in format IDFORM_1. */
	unsigned char	id_machine;	/* machine type */
	unsigned char	id_ether[6];	/* ethernet address */
	long		id_date;	/* date of manufacture */
	unsigned	id_serial:24;	/* serial number */
	unsigned char	id_xsum;	/* xor checksum */
	unsigned char	id_undef[16];	/* undefined */
} idprom_t;
#endif	/* _ASM */

#define	IDFORM_1	1	/* Format number for first ID proms */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IDPROM_H */
