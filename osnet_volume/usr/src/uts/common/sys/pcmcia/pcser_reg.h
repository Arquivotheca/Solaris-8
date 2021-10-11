/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PCSER_REG_H
#define	_PCSER_REG_H

#pragma ident	"@(#)pcser_reg.h	1.9	96/01/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Register offset definitions
 */
#define	PCSER_REGS_RBRTHR	0x00	/* Rx/Tx buffer */
#define	PCSER_REGS_IER		0x01	/* interrupt enable register */
#define	PCSER_REGS_IIR		0x02	/* interrupt identification register */
#define	PCSER_REGS_LCR		0x03	/* line control register */
#define	PCSER_REGS_MCR		0x04	/* modem control register */
#define	PCSER_REGS_LSR		0x05	/* line status register */
#define	PCSER_REGS_MSR		0x06	/* modem status register */
#define	PCSER_REGS_SCR		0x07	/* scratch pad register */

#ifdef	__cplusplus
}
#endif

#endif	/* _PCSER_REG_H */
