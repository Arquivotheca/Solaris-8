/*
 * Copyright (c) 1991-1994 Sun Microsystems, Inc.
 */

#ifndef	_SYS_DDIMAPREQ_H
#define	_SYS_DDIMAPREQ_H

#pragma ident	"@(#)ddimapreq.h	1.14	96/06/25 SMI"

#include <sys/mman.h>
#include <sys/dditypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * Mapping requests are for an rnumber or for a regspec.
 *
 * A regspec is a generic triple, usually representing
 * 	type, offset, length
 *
 * And is interpreted privately between the child and parent.
 * The triple should be sufficient for representing byte addressable devices.
 */

typedef union {
	int	rnumber;
	struct	regspec *rp;
} ddi_map_obj_t;

typedef enum {
	DDI_MT_RNUMBER = 0,
	DDI_MT_REGSPEC
} ddi_map_type_t;

/*
 * Mapping operators:
 */
typedef enum {
	DDI_MO_MAP_UNLOCKED = 0,	/* Create mapping, do not lock down */
	DDI_MO_MAP_LOCKED,		/* Create locked down mapping */
	DDI_MO_MAP_HANDLE,		/* Create handle, do not map */
	DDI_MO_UNMAP,			/* Unmap (implies unlock, if locked) */
	DDI_MO_UNLOCK			/* Unlock mapping, do *not* unmap */
} ddi_map_op_t;

/*
 * Mapping request structure...
 */

typedef struct {
	ddi_map_op_t map_op;
	ddi_map_type_t map_type;
	ddi_map_obj_t map_obj;
	int map_flags;	/* See below... */
	int map_prot;	/* Prot bits (see sys/mman.h) */
	ddi_acc_hdl_t *map_handlep;
	int map_vers;
} ddi_map_req_t;

/*
 * version number
 */
#define	DDI_MAP_VERSION		0x0001

/*
 * Mappings subject to the following flags:
 */

			/*
			 * Make mapping suitable for user program use.
			 */
#define	DDI_MF_USER_MAPPING	0x1

			/*
			 * Make mapping suitable for kernel mapping.
			 */
#define	DDI_MF_KERNEL_MAPPING	0x2
#define	DDI_MF_DEVICE_MAPPING	0x4

#endif	/* _KERNEL */

/*
 * Error (non-zero) return codes from DDI mapping functions...
 */

#define	DDI_ME_GENERIC		(-1)	/* Generic un-enumerated error */
#define	DDI_ME_UNIMPLEMENTED	(-2)	/* Unimplemented operator */
#define	DDI_ME_NORESOURCES	(-3)	/* No resources, try later? */
#define	DDI_ME_UNSUPPORTED	(-4)	/* Op is not supported in impl. */
#define	DDI_ME_REGSPEC_RANGE	(-5)	/* Addressing range error */
#define	DDI_ME_RNUMBER_RANGE	(-6)	/* Addressing range error */
#define	DDI_ME_INVAL		(-7)	/* Invalid input parameter */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDIMAPREQ_H */
