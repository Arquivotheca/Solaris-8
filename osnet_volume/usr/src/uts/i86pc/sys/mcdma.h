/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_MCDMA_H
#define	_SYS_MCDMA_H

#pragma ident	"@(#)mcdma.h	1.3	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for PS/2 DMA controllers.
 */

/*
 * PS/2 DMA extended mode definitions
 *
 */
#define	PS2DMA_CTL	0x18	/* function register		*/
#define	PS2DMA_DAT	0x1A	/* execute function register	*/

#define	PS2DMA_IOR	0x0	/* I/O address register		*/
#define	PS2DMA_MAR	0x20	/* memory address register	*/
#define	PS2DMA_RMA	0x30	/* read memory address register	*/
#define	PS2DMA_TCR	0x40	/* transfer count register	*/
#define	PS2DMA_RTC	0x50	/* read transfer count register	*/
#define	PS2DMA_STR	0x60	/* read status register		*/
#define	PS2DMA_WMR	0x70	/* write mode register		*/
#define	PS2DMA_ARB	0x80	/* arbus register		*/
#define	PS2DMA_SMK	0x90	/* set mask bit			*/
#define	PS2DMA_CMK	0xA0	/* clear mask bit		*/


#define	PS2DMA_M8	0x00	/* 8-bit mode			*/
#define	PS2DMA_M16	0x40	/* 16-bit mode			*/
#define	PS2DMA_MVF	0x00	/* mode for verify operation	*/
#define	PS2DMA_MRD	0x0C	/* mode for read operation	*/
#define	PS2DMA_MWT	0x04	/* mode for write operation	*/
#define	PS2DMA_MIO	0x01	/* use i/o address reg		*/

/* the following read & write are relative to memory, not the device */
#define	PS2DMA_RD	0x44	/* 16-bit read mode		*/
#define	PS2DMA_WR	0x4C	/* 16-bit write mode		*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MCDMA_H */
