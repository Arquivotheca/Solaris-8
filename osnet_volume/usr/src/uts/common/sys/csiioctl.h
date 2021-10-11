/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CSIIOCTL_H
#define	_SYS_CSIIOCTL_H

#pragma ident	"@(#)csiioctl.h	1.2	99/08/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * csiioctl.h
 *
 * CodeSet Independent codeset width communication between stty(1) and
 * ldterm(7M).
 *
 * CSDATA_SET	This call takes a pointer to a ldterm_cs_data_t data
 *		structure, and uses it to set the line discipline definition
 *		and also for a possible switch of the internal methods and
 *		data for the current locale's codeset.
 *
 *		When this message is reached, the ldterm(7M) will check
 *		the validity of the message and if the message contains
 *		a valid data, it will accumulate the data and switch
 *		the internal methods if necessary to support the requested
 *		codeset.
 *
 * CSDATA_GET	This call takes a pointer to a ldterm_cs_data_t structure
 *		and returns in it the codeset data info currently in use by
 *		the ldterm(7M) module.
 */

#define	CSI_IOC		(('C' | 128) << 8)
#define	CSDATA_SET	(CSI_IOC | 1)
#define	CSDATA_GET	(CSI_IOC | 2)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CSIIOCTL_H */
