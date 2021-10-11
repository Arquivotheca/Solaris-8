/*
 * Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_MSCSI_BUS_H
#define	_SYS_DKTP_MSCSI_BUS_H

#pragma ident	"@(#)mscsi.h	1.2	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * mscsi_bus header file.  Driver private interface
 * between a multiple scsi bus hba scsa nexus driver
 * and the mscsi-bus nexus driver, which provides
 * per-bus support.
 */

/*
 * mbus_ops:     mbus nexus drivers only.
 *
 * This structure provides a wrapper for the generic bus_ops
 * structure, allowing mscsi drivers to transparently remap
 * bus_ops functions as needed.
 *
 * Only nexus drivers should use this structure.
 *
 *      m_ops         -  Replacement struct bus_ops
 *	m_dops        -  Saved struct dev_ops
 *      m_bops        -  Saved struct bus_ops
 *      m_private     -  Any other saved private data
 */

struct mbus_ops {
	struct bus_ops	 m_ops;		/* private struct bus_ops */
	struct dev_ops  *m_dops;	/* saved struct dev_ops* */
	struct bus_ops  *m_bops;	/* saved struct bus_ops* */
	void 		*m_private;	/* saved private data */
};

#define	MSCSI_FEATURE			/* mscsi feature define */
#define	MSCSI_NAME	"mscsi"		/* nodename of mscsi driver */
#define	MSCSI_BUSPROP	"mscsi-bus"	/* propertyname of mscsi-bus no. */
#define	MSCSI_CALLPROP	"mscsi-call"	/* propertyname of callback request */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKTP_MSCSI_BUS_H */
