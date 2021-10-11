/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MICIO_H
#define	_SYS_MICIO_H

#pragma ident	"@(#)micio.h	1.7	96/04/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Defines for mic specific ioctl calls
 */

#define	MIOC		('M'<<8)
#define	MIOCGETM_IR	(MIOC | 1)		/* Returns the IR mode */
#define	MIOCSETM_IR	(MIOC | 2)		/* Sets the IR mode */
#define	MIOCGETD_IR	(MIOC | 3)		/* Returns the IR Tx divisor */
#define	MIOCSETD_IR	(MIOC | 4)		/* Sets the IR Tx divisor */

#define	MIOCSLPBK_IR	(MIOC | 5)		/* Set IR loopback mode */
#define	MIOCCLPBK_IR	(MIOC | 6)		/* Unset IR loopback mode */

#define	MIOCSLPBK	(MIOC | 7)		/* Set SCC loopback mode */
#define	MIOCCLPBK	(MIOC | 8)		/* Unset SCC loopback mode */

/* Infra Red Modes */
#define	MIC_IR_PULSE		0x05		/* Pulse mode */
#define	MIC_IR_HI		0x0a		/* Hi frequency */
#define	MIC_IR_LO		0x0f		/* Lo frequency */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MICIO_H */
