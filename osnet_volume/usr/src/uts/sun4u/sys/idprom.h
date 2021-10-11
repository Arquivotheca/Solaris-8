/*
 * Copyright (c) 1986-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_IDPROM_H
#define	_SYS_IDPROM_H

#pragma ident	"@(#)idprom.h	1.11	97/05/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/*
 * Structure declaration for ID prom in CPU and Ethernet boards
 */
typedef struct idprom {
	uint8_t	id_format;	/* format identifier */
	/*
	 * The following fields are valid only in format IDFORM_1.
	 */
	uint8_t	id_machine;	/* machine type */
	uint8_t	id_ether[6];	/* ethernet address */
	int32_t	id_date;	/* date of manufacture */
	uint32_t id_serial:24;	/* serial number */
	uint8_t	id_xsum;	/* xor checksum */
	uint8_t	id_undef[16];	/* undefined */
} idprom_t;
#endif	/* _ASM */

#define	IDFORM_1	1	/* Format number for first ID proms */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IDPROM_H */
