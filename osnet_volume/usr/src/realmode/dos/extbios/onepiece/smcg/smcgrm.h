/*
 * Copyright (c) 1995,1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident	"@(#)smcgrm.h 1.7	97/11/10 SMI\n"

/*
 * SMC Generic Upper MAC realmode driver
 */
#ifndef _SMCGRM_H
#define	_SMCGRM_H 1

#define	MAX_ADAPTERS		16

#define	ETHERADDRL		6
#define	ETHERMIN		60
#define	ETHERMAX		1514

/* Definitions for the field bus_type */
#define	SMCG_AT_BUS		0x00
#define	SMCG_MCA_BUS		0x01
#define	SMCG_EISA_BUS		0x02
#define	SMCG_PCI_BUS		0x03

#endif /* _SMCGRM_H */
