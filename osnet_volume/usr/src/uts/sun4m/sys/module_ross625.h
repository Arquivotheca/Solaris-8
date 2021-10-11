/*
 * Copyright (c) 1987-1993 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_MODULE_ROSS625_H
#define	_SYS_MODULE_ROSS625_H

#pragma ident	"@(#)module_ross625.h	1.1	94/05/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the Ross Technology RT620/RT625 hyperSPARC processor
 * module.
 */

/*
 * RT620 Instruction Cache Control Register
 */
#define	RT620_ICCR		%asr31
#define	RT620_ICCR_FTD		0x02		/* iflush trap disable */
#define	RT620_ICCR_ICE		0x01		/* icache enable */

/*
 * RT625 Module Control Register
 */
#define	RT625_CTL_CE		0x00000100	/* cache enable */
#define	RT625_CTL_CM		0x00000400	/* copyback mode */
#define	RT625_CTL_CS		0x00001000	/* cache size */
#define	RT625_CTL_WBE		0x00080000	/* write buffer enable */
#define	RT625_CTL_SE		0x00100000	/* snoop enable */
#define	RT625_CTL_CWE		0x00200000	/* cache-wrap enable */
#define	RT625_CTL_IDMASK	0xff000000	/* mask for ID */
#define	RT625_CTL_ID		0x17000000	/* identifies RT625 */

/*
 * ASIs
 */
#define	RT620_ASI_IC		0x31		/* icache flash clear */
#define	RT625_ASI_CTAG		0x0e		/* cache tags */

/*
 * Miscellaneous
 */
#define	RT625_MAX_CTXS		4096		/* max hardware contexts */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MODULE_ROSS625_H */
