/*
 * Copyright (c) 1994-1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCI_IMPL_H
#define	_SYS_PCI_IMPL_H

#pragma ident	"@(#)pci_impl.h	1.9	99/07/26 SMI"

#include <sys/dditypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef i386

/*
 * There are two ways to access the PCI configuration space on X86
 * 	Access method 2 is the older method
 *	Access method 1 is the newer method and is preferred because
 *	  of the problems in trying to lock the configuration space
 *	  for MP machines using method 2.  See PCI Local BUS Specification
 *	  Revision 2.0 section 3.6.4.1 for more details.
 *
 * In addition, on IBM Sandalfoot and a few related machines there's
 * still another mechanism.  See PReP 1.1 section 6.1.7.
 */

#define	PCI_MECHANISM_UNKNOWN		-1
#define	PCI_MECHANISM_NONE		0
#if	defined(__i386) || defined(__ia64)	/* XXX Merced */
#define	PCI_MECHANISM_1 		1
#define	PCI_MECHANISM_2			2
#else
#error "Unknown processor type"
#endif


#ifndef FALSE
#define	FALSE   0
#endif

#ifndef TRUE
#define	TRUE    1
#endif

#define	PCI_FUNC_MASK			0x07

/* these macros apply to Configuration Mechanism #1 */
#define	PCI_CONFADD		0xcf8
#define	PCI_PMC			0xcfb
#define	PCI_CONFDATA		0xcfc
#define	PCI_CONE		0x80000000
#define	PCI_CADDR1(bus, device, function, reg) \
		(PCI_CONE | (((bus) & 0xff) << 16) | (((device & 0x1f)) << 11) \
			    | (((function) & 0x7) << 8) | ((reg) & 0xfc))

/* these macros apply to Configuration Mechanism #2 */
#define	PCI_CSE_PORT		0xcf8
#define	PCI_FORW_PORT		0xcfa
#define	PCI_CADDR2(device, indx) \
		(0xc000 | (((device) & 0xf) <<  8) | (indx))

typedef struct 	pci_acc_cfblk {
	uchar_t	c_busnum;		/* bus number */
	uchar_t c_devnum;		/* device number */
	uchar_t c_funcnum;		/* function number */
	uchar_t c_fill;			/* reserve field */
} pci_acc_cfblk_t;

#endif /* i386 */


int
pci_resource_setup(dev_info_t *);

void
pci_resource_destroy(dev_info_t *);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PCI_IMPL_H */
