/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_MWSS_H
#define	_SYS_MWSS_H

#ident "@(#)mwss.h   1.4   96/05/16 SMI"

#define MWSS_IO_LEN		8

/*
 * MWSS and AD184x register definitions
 */
#define MWSS_IRQSTAT            0       /* Interrupt status */
#define AD184x_INDEX            4       /* Index into register bank */
#define AD184x_MODE_CHANGE      0x40    /* Index bit to allow mode change */
#define AD184x_DATA             5       /* Indexed register data */
#define AD184x_STATUS           6       /* More interrupt status */

#define LEFT_IN_REG             0x0     /* Left input select/gain */
#define RIGHT_IN_REG            0x1     /* Right input select/gain */
#define LEFT_OUT_REG            0x6     /* Left output volume */
#define RIGHT_OUT_REG           0x7     /* Right output volume */
#define FORMAT_REG              0x8     /* Sampling format/rate */
#define CFG_REG                 0x9     /* Record/Play enable */
#define PIN_REG                 0xa     /* Interrupt enable */
#define TEST_INIT_REG           0xb     /* Test and Init */
#define MON_LOOP_REG            0xd     /* Monitor Volume */
#define COUNT_HIGH_REG          0xe     /* Interrupt countdown */
#define COUNT_LOW_REG           0xf

#endif	/* _SYS_MWSS_H */
