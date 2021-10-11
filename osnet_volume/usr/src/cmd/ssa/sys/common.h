/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Common definitions
 */

#ifndef	_P_COMMON
#define	_P_COMMON

#pragma ident	"@(#)common.h	1.3	97/03/19 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	S_DPRINTF	if (getenv("SSA_S_DEBUG") != NULL) (void) printf
#define	P_DPRINTF	if (getenv("SSA_P_DEBUG") != NULL) (void) printf
#define	O_DPRINTF	if (getenv("SSA_O_DEBUG") != NULL) (void) printf
#define	I_DPRINTF	if (getenv("SSA_I_DEBUG") != NULL) (void) printf

#ifdef SUNOS41
#define	MSG(msgid)	msgid
#else
#define	MSG(msgid)	gettext(msgid)
#endif

/*
 * Define for physical name of children of pln
 */
#define	DRV_NAME_SD	"sd@"
#define	DRV_NAME_SSD	"ssd@"
#define	SLSH_DRV_NAME_SD	"/sd@"
#define	SLSH_DRV_NAME_SSD	"/ssd@"


/*
 * format parameter to dump()
 */
#define	HEX_ONLY	0	/* print hex only */
#define	HEX_ASCII	1	/* hex and ascii */


/*
 * 	Status of each drive
 */
#define	NO_LABEL	6	/* The disk label is not a valid UNIX label */
#define	MEDUSA		8	/* Medusa mirrored disk */
#define	DUAL_PORT	9	/* Dual Ported Disk */

#define	P_NPORTS	6
#define	P_NTARGETS	15


/*
 * controller/nexus node postfix strings
 */
#define	CTLR_POSTFIX	":ctlr"
#define	DEVCTL_POSTFIX	":devctl"


#ifdef	__cplusplus
}
#endif

#endif	/* _P_COMMON */
