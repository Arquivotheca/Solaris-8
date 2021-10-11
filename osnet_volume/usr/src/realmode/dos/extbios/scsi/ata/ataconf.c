
/*
 * Copyright (c) 1997, by Sun Microsystems, Inc
 * All Rights Reserved.
 */


#ident "@(#)ataconf.c   1.1   97/10/01 SMI"

#include <dostypes.h>
#include "ataconf.h"

/*
 * bug-A: single-sector-only
 *
 * These drives can't handle transfers larger than 1 sector at a time. 
 * They have two types of problems, either:
 *
 *	they don't clear the DRQ bit at the end of the transfer, or
 *
 *      they break up a multiple sector request into N partial
 *	responses but there's no way to tell when the data for the
 *	2nd-Nth responses are valid and can be transfered.
 *
 *
 * bug-B: bogus-busy 
 *
 * These drives don't manage the BSY bit in the status register
 * correctly between bus phases. Therefore we can't reliably poll
 * for phase transitions. The work around is to add a long delay
 * between the problematic phase changes. Drives that have this
 * bug usually also have bug-A.
 *
 *
 * bug-C: nec-bad-status
 *
 * Some old NEC drives return an interrupt reason code of 0 instead
 * of (ATI_IO | ATI_COD) during the final status phase.
 *
 *
 * bug-D: bogus-drq
 *
 * Just before or during each data transfer phase the drive
 * deasserts DRQ bit instead of asserting the BUSY bit. The
 * busy bit in fact never asserts between phases so this drive
 * also has the bogus-busy bug.
 *
 * Note:
 *
 *	When the model names are compared multiple blanks are
 *	treated the same as a single blank (i.e., "foo bar" matches
 *	"foo       bar".
 *
 *	If the drive's actual model name is longer than the
 *	strings specified in this table, then the extra
 *	characters are ignored (i.e, an entry in this table
 *	that specifies "foo" actually matches "foo*")
 *
 */
bl_t	ata_blacklist[] = {
	/* model name             bug-A bug-B bug-C bug-D */
	{ "NEC CD-ROM DRIVE:260", TRUE, TRUE, TRUE, FALSE },
	{ "NEC CD-ROM DRIVE:272", TRUE, TRUE, FALSE, FALSE },
	{ "NEC CD-ROM DRIVE:273", TRUE, TRUE, FALSE, FALSE },
	{ /*Mitsumi*/ "FX001DE", TRUE, FALSE, FALSE, FALSE },
	{ "LION OPTICS CORPORATION XC-200AI CD-ROM", FALSE, TRUE, FALSE, TRUE },
	{ /*Mitsumi*/ "FX400_02", FALSE, TRUE, FALSE, FALSE },
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};
