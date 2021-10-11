/*
 * Copyright (c) 1991, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)decode_xdb.c	1.21	99/08/28 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/autoconf.h>
#include <sys/pte.h>
#include <sys/bt.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>

int debug_autoconf = 0;
#define	APRINTF	if (debug_autoconf) printf

#define	CSR_UNIT(pageval)	((pageval >> (20 - MMU_PAGESHIFT)) & 0xff)
#define	ECSR_UNIT(pageval)	((pageval >> (24 - MMU_PAGESHIFT)) & 0xfe)

/*
 * Supposedly, this routine is the only place that enumerates details
 * of the encoding of the "bustype" part of the physical address.
 * This routine depends on details of the physical address map, and
 * will probably be somewhat different for different machines.
 *
 * The return value is the length of the formatted string, not the
 * actual number printed into the buffer.
 */
int
decode_address(space, addr, buf, buflen)
	uint_t space;
	uint_t addr;
	char *buf;
	int buflen;
{
	uint_t residue = addr & MMU_PAGEOFFSET;
	uint_t nibble = space & 0xf;
	uint_t pageval = (addr >> MMU_PAGESHIFT)
		+ (nibble << (32 - MMU_PAGESHIFT));
	int f_len = 0;

	APRINTF("decode_address: space= %x, addr= %x\n", space, addr);

#if	defined(SPO_VIRTUAL)
	if (space == SPO_VIRTUAL) {
		f_len += snprintf(buf, buflen, "SPO_VIRTUAL: addr=0x%8x", addr);
		return (f_len);
	}
#endif	/* SPO_VIRTUAL */

	if (space != nibble) {
		f_len += snprintf(buf, buflen,
		    "unknown: space=0x%x, addr=0x%8x", space, addr);
		return (f_len);
	}

	if (nibble == 0xf) {
		int len;
		uint_t device_id;

		switch ((pageval >> (28 - MMU_PAGESHIFT)) & 0xf) {
		case 0xf: {
			f_len += snprintf(buf, buflen, "local(cpu ?x?) ");
			pageval &= 0x03ffff;	/* 18 bits */
			break;
		}

		case 0xe: {
			device_id = CSR_UNIT(pageval);
			f_len += snprintf(buf, buflen, "csr(unit=0x%2x) ",
			    device_id);
			pageval &= 0x0003ff;	/* 10 bits */
			break;
		}

		default:
			device_id = ECSR_UNIT(pageval);
			f_len += snprintf(buf, buflen, "ecsr(unit=0x%2x) ",
			    device_id);
			pageval &= 0x003fff;	/* 14 bits */
			break;
		}

		len = strlen(buf);
		f_len += snprintf(buf + len, buflen - len,
		    "addr=0x%6x.%3x", pageval, residue);
		return (f_len);
	}

	if (nibble & 8) {
		uint_t sbus = (pageval >> 18) & 0xf;
		uint_t slot = (pageval >> 16) & 0x3;
		f_len += snprintf(buf, buflen, "sbus%d, slot%d, addr=0x%6x.%3x",
		    sbus, slot, (pageval & 0xffff), residue);
		return (f_len);
	}

	f_len += snprintf(buf, buflen, "paddr=0x%6x.%3x", pageval, residue);
	return (f_len);
}


/*
 * Compute the address of an I/O device within standard address
 * ranges and return the result.  This is used by DKIOCINFO
 * ioctl to get the best guess possible for the actual address
 * the card is at.
 */
int
getdevaddr(addr)
	caddr_t addr;
{
	struct pte pte;
	uint_t pageno;
	uint_t high_byte;

	mmu_getkpte(addr, &pte);
	pageno = pte.PhysicalPageNumber;
	high_byte = pageno >> 16;

	APRINTF("getdevaddr: pageno= 0x%x\n", pageno);

	switch (high_byte) {
		case 0xf0: {	/* local */
			uint_t physaddr = mmu_ptob(pageno & 0x0ffff);
			return (physaddr);
		}
		case 0xfe: {	/* ecsr */
			uint_t physaddr = mmu_ptob(pageno & 0x0ffff);
			return (physaddr);
		}
		case 0xff: {	/* csr */
			uint_t physaddr = mmu_ptob(pageno & 0x00fff);
			return (physaddr);
		}
		default: {
			uint_t physaddr = mmu_ptob(pageno & 0xffff);
			return (physaddr);
		}
	}
	/*NOTREACHED*/
}

/*
 * figure out what type of bus a page frame number points at.
 * NOTE: returns a BT_ macro, not the top four bits. most
 * things called "bustype" are actually just the top four
 * bits of the pte, which are part of the physical address
 * space as defined in the architecture and which change between
 * various implementations.
 * FIXME: we need pageno + AC!
 */
int
impl_bustype(pageno)
	uint_t pageno;
{
	extern int pa_in_nvsimmlist(u_longlong_t);
	uint_t space = pageno >> (32 - MMU_PAGESHIFT);

	APRINTF("bustype: pageno= 0x%6x\n", pageno);

	if (space < 8) {	/* hack */
		if (pa_in_nvsimmlist(mmu_ptob((u_longlong_t)pageno)))
			return (BT_NVRAM);
		return (BT_DRAM);
	}

	if (space == 0xf) {
		return (BT_OBIO);
	}

	if (space & 8) {
		return (BT_SBUS);
	}

	return (BT_UNKNOWN);
}
