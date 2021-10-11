/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_CDTYPES_H
#define	_SYS_DKTP_CDTYPES_H

#pragma ident	"@(#)cdtypes.h	1.5	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct cd_data {
	opaque_t	cd_tgpt_objp;
	ulong_t		cd_options;
};

/* cd_options values */
#define	SCCD_OPT_CDB10			0x01
#define	SCCD_OPT_PLAYMSF_BCD		0x02
#define	SCCD_OPT_READ_SUBCHANNEL_BCD	0x04
#define	SCCD_OPT_READCD			0x08

#define	TGPTOBJP(X) ((X)->cd_tgpt_objp)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CDTYPES_H */
