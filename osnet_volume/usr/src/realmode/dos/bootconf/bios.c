/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * bios.c -- handles enumeration of bios
 */

#ident	"@(#)bios.c	1.20	99/01/25 SMI"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <names.h>
#include <conio.h>
#include "types.h"

#include "bios.h"
#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "enum.h"
#include "escd.h"
#include "resmgmt.h"
#include "tty.h"
#include "pci.h"
#include "prop.h"
#include "rtc.h"

void found_bios(u_long paddr, u_long len);

/*
 * Search for bios'es and create Boards for them
 */
void
enumerator_bios()
{
	u_long faddr;
	u_long ep;
	u_long paddr;
	u_long incr;
	u_long blen;
	u_char *cfp;

	for (faddr = BIOS_MIN; faddr < BIOS_MAX; faddr += incr) {
		if (*(u_short *)faddr == BIOS_ID) {
			paddr = MK_PHYS(faddr);
			cfp = (u_char *) faddr;
			blen = (unsigned long)cfp[2] * 512L;
			found_bios(paddr, blen);
			incr = blen << 12; /* shift to far pointer format */
			/*
			 * We have the length the BIOS claims to be, now
			 * if there is another bios following, keep going.
			 * If this was the last bios, round up to the next
			 * 64k boundary.  This is because the System BIOS
			 * may have enabled some memory to appear to hold
			 * copied bioses on PCI machines.  The granularity
			 * of the hardware memory controller will determine
			 * how much physical memory appears, but we assume
			 * it is not more than 64k.  We create a fake "weak"
			 * BIOS for this rounded up region.
			 */
			ep = faddr + incr;
			if (*(u_short *)ep != BIOS_ID) {
				/*
				 * If PCI round up to next 64k boundary
				 */
				if (Pci && (ep & 0xfffffff)) {
					faddr += incr;
					paddr = MK_PHYS(faddr);
					ep = (faddr + 0x10000000) & 0xf0000000;
					blen = (ep - faddr) >> 12;
					found_bios(paddr, blen);
				}

			}
		} else {
			incr = BIOS_ALIGN;
		}
	}
}

void
found_bios(u_long paddr, u_long len)
{
	Board *bp;

	debug(D_FLOW, "Bios found at phys 0x%lx, len 0x%lx bytes\n",
	    paddr, len);

	if (Query_Mem(paddr, len)) {
		return;
	}

	/*
	 * Fill in the Board fields
	 */
	bp = new_board();
	bp->bustype = RES_BUS_ISA;
	bp->category = DCAT_UNKNWN;
	bp->devid = CompressName("SUNFFE1");

	bp = AddResource_devdb(bp, RESF_Mem | RESF_WEAK, paddr, len);

	add_board(bp); /* Tell rest of system about device */
}

char *rtc_hw_dst_enable = "rtc-hw-dst-enable";


/*
 * int
 * init_bioscmos(void)
 *
 * General cmos editor from bios properties.
 *
 * Currently only rtc (real time clock) editing enabled.
 *
 * rtc-hw-dst-enable
 *
 * Edit the RTC DSE bit. This is an emergency point patch to
 * allow customers with uncertain bios settings to properly
 * handle daylight savings times transitions, by explicitely
 * setting a the rtc-hw-dst-enable property to
 *
 *	true	positively enables hardware dst
 *	false	positively disables hardware dst
 *
 * If the property doesn't exist, there is no change (protecting
 * systems already in the field which may be setup one way or the
 * other).
 */
int
init_bioscmos(void)
{
	char *val;
	unsigned char rtcreg;

	if ((val = read_prop(rtc_hw_dst_enable, "options")) != 0) {
	    if (strncmp(val, "true", 4) == 0) {

		/*
		 * Verify the RTC DSE bit is off.  If off, turn it on
		 */
		_outp(RTC_ADDR, RTC_B);
		if (!((rtcreg = _inp(RTC_DATA)) & RTC_DSE)) {
			_outp(RTC_ADDR, RTC_B);
			_outp(RTC_DATA, (unsigned char) (rtcreg | RTC_DSE));
		}

	    } else if (strncmp(val, "false", 4) == 0) {

		/*
		 * Verify the RTC DSE bit is on.  If on, turn it off
		 */
		_outp(RTC_ADDR, RTC_B);
		if ((rtcreg = _inp(RTC_DATA)) & RTC_DSE) {
			_outp(RTC_ADDR, RTC_B);
			_outp(RTC_DATA, (unsigned char) (rtcreg & ~RTC_DSE));
		}
	    }
	}
	return (0);
}
