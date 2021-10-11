/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MHD_H
#define	_SYS_MHD_H

#pragma ident	"@(#)mhd.h	1.6	99/05/20 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definitions for multi-host device I/O control commands
 */
#define	MHIOC				('M'<<8)
#define	MHIOCENFAILFAST			(MHIOC|1)
#define	MHIOCTKOWN			(MHIOC|2)
#define	MHIOCRELEASE			(MHIOC|3)
#define	MHIOCSTATUS			(MHIOC|4)
#define	MHIOCGRP_INKEYS 		(MHIOC|5)
#define	MHIOCGRP_INRESV 		(MHIOC|6)
#define	MHIOCGRP_REGISTER		(MHIOC|7)
#define	MHIOCGRP_RESERVE		(MHIOC|8)
#define	MHIOCGRP_PREEMPTANDABORT	(MHIOC|9)
#define	MHIOCGRP_PREEMPT		(MHIOC|10)
#define	MHIOCGRP_CLEAR			(MHIOC|11)
#define	MHIOCQRESERVE			(MHIOC|12)
#define	MHIOCREREGISTERDEVID		(MHIOC|13)

/*
 * Following is the structure to specify the delay parameters in
 * milliseconds, via the MHIOCTKOWN ioctl.
 */
struct mhioctkown {
	int reinstate_resv_delay;
	int min_ownership_delay;
	int max_ownership_delay;
};

#define	MHIOC_RESV_KEY_SIZE	8
typedef struct mhioc_resv_key {
	uchar_t	key[MHIOC_RESV_KEY_SIZE];
} mhioc_resv_key_t;

typedef struct mhioc_key_list {
	uint32_t		listsize;
	uint32_t		listlen;
	mhioc_resv_key_t	*list;
} mhioc_key_list_t;

typedef struct mhioc_inkeys {
	uint32_t		generation;
	mhioc_key_list_t	*li;
} mhioc_inkeys_t;

typedef struct mhioc_resv_desc {
	mhioc_resv_key_t	key;
	uint8_t			type;
	uint8_t			scope;
	uint32_t		scope_specific_addr;
} mhioc_resv_desc_t;

typedef struct mhioc_resv_desc_list {
	uint32_t		listsize;
	uint32_t		listlen;
	mhioc_resv_desc_t	*list;
} mhioc_resv_desc_list_t;

typedef struct mhioc_inresvs {
	uint32_t		generation;
	mhioc_resv_desc_list_t	*li;
} mhioc_inresvs_t;

typedef struct mhioc_register {
    mhioc_resv_key_t	oldkey;
    mhioc_resv_key_t	newkey;
    boolean_t		aptpl;  /* True if persist across power failures */
} mhioc_register_t;

typedef struct mhioc_preemptandabort {
    mhioc_resv_desc_t	resvdesc;
    mhioc_resv_key_t	victim_key;
} mhioc_preemptandabort_t;


/*
 * SCSI-3 PGR Reservation Type Codes.  Codes with the _OBSOLETE suffix
 * have been removed from the SCSI3 PGR standard.
 */
#define	SCSI3_RESV_READSHARED_OBSOLETE			0
#define	SCSI3_RESV_WRITEEXCLUSIVE			1
#define	SCSI3_RESV_READEXCLUSIVE_OBSOLETE		2
#define	SCSI3_RESV_EXCLUSIVEACCESS			3
#define	SCSI3_RESV_SHAREDACCESS_OBSOLETE		4
#define	SCSI3_RESV_WRITEEXCLUSIVEREGISTRANTSONLY	5
#define	SCSI3_RESV_EXCLUSIVEACCESSREGISTRANTSONLY	6

#define	SCSI3_SCOPE_LOGICALUNIT				0
#define	SCSI3_SCOPE_EXTENT_OBSOLETE			1
#define	SCSI3_SCOPE_ELEMENT				2

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MHD_H */
