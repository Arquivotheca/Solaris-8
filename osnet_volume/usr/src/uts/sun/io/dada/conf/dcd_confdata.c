/*
 * COpyright (c) 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dcd_confdata.c 1.4     98/02/25 SMI"

#ifdef _KERNEL

#include <sys/dada/conf/autoconf.h>

/*
 * AutoConfiguration Dependent data
 */


/*
 * DCD options word - defines are kept in <dada/conf/autoconf.h>
 *
 * All this options word does is to enable such capabilities. Each
 * implementation may disable this worf or ignore it entirely.
 * Changing this word after system autoconfiguration is not guarenteed
 * to cause any change in the operation of the system.
 */

int dcd_options = (DCD_MULT_DMA_MODE2 << 3) | DCD_DMA_MODE;

#endif	/* _KERNEL */
