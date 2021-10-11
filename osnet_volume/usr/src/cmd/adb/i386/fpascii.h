/*
 * Copyright (c) 1986, 1987, 1988, 1989, 1990, 1991, 1992 by
 * Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)fpascii.h	1.3     92/12/23 SMI"

#ifndef _FPASCII_H_
#define _FPASCII_H_

/*
 * Possible values for _fp_hw, based on iABI.
 *
 *	0	no floating point support present
 *	1	80387 software emulator is present
 *	2	80287 chip is present
 *	3	80387 chip is present
 */
extern _fp_hw;
extern fpa_disasm, fpa_avail, mc68881;

void fprtos(/* fpr, s */);

#endif /* !_FPASCII_H_ */
