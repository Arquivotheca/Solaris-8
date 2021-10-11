/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pcihpres.c -- routines to retrieve available bus resources from
 *		 the MP Spec. Table
 */

#ident "@(#)mpspec.c   1.2   99/04/01 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <dostypes.h>
#include <string.h>
#include "types.h"
#include "ur.h"
#include "mps_table.h"
#include "mpspec.h"
#include "hrt.h"

int mps_init = 0;
u_char far *mps_extp = NULL;
u_char far *mps_ext_endp = NULL;

void mps_probe(void);
u_char far *find_sig(u_char far *cp, long len, char *sig);
int checksum(unsigned char far *cp, int len);

void
mps_probe()
{
	unsigned char far *extp;
	struct mps_fps_hdr far *fpp;
	struct mps_ct_hdr far *ctp;
	u_long ebda_start, base_end, seg, off;
	unsigned short ebda_seg, base_size, ext_len, base_len, base_end_seg;

	mps_init = 1;
	base_size = *((unsigned short far *) (0x413));
	ebda_seg = *((unsigned short far *) (0x40e));
	ebda_start = ((u_long) ebda_seg) << 16;
#ifdef	MPS_DEBUG
	printf("base memory size = 0x%x\n", base_size);
	printf("ebda segment = 0x%x\n", ebda_seg);
#endif
	fpp = NULL;
	if (ebda_seg != 0) {
		fpp = (struct mps_fps_hdr far *) find_sig(
			(u_char far *) ebda_start, 1024, "_MP_");
	}
	if (fpp == (struct mps_fps_hdr far *) NULL) {
		base_end_seg = (base_size > 512) ? 0x9FC0 : 0x7FC0;
		if (base_end_seg != ebda_seg) {
			base_end = ((u_long) base_end_seg) << 16;
			fpp = (struct mps_fps_hdr far *) find_sig(
				(u_char far *) base_end, 1024, "_MP_");
		}
	}
	if (fpp == (struct mps_fps_hdr far *) NULL) {
		if ((fpp = (struct mps_fps_hdr far *) find_sig(
			(u_char far *) 0xF0000000, 0x10000, "_MP_")) == NULL) {
#ifdef	MPS_DEBUG
			printf("MP Spec table doesn't exist");
#endif
			return;
		}
	}
#ifdef	MPS_DEBUG
	printf("Found MP Floating Pointer Structure at %lx\n", fpp);
#endif
	if (checksum((u_char far *) fpp, fpp->fps_len * 16) != 0) {
#ifdef	MPS_DEBUG
		printf("MP Floating Pointer Structure checksum error");
#endif
		return;
	}
	seg = fpp->fps_mpct_paddr & 0xF0000;
	off = fpp->fps_mpct_paddr & 0xFFFF;
	ctp = (struct mps_ct_hdr far *) ((seg << 12) | off);
	if (ctp->ct_sig != 0x504d4350) { /* check "PCMP" signature */
#ifdef	MPS_DEBUG
		printf("MP Configuration Table signature is wrong");
#endif
		return;
	}

	base_len = ctp->ct_len;
	if (checksum((u_char far *) ctp, base_len) != 0) {
#ifdef	MPS_DEBUG
		printf("MP Configuration Table checksum error");
#endif
		return;
	}
	if (ctp->ct_spec_rev != 4) { /* not MPSpec rev 1.4 */
#ifdef	MPS_DEBUG
		printf("MP Spec 1.1 found - extended table doesn't exist");
#endif
		return;
	}
	if ((ext_len = ctp->ct_ext_tbl_len) == 0) {
#ifdef	MPS_DEBUG
		printf("MP Spec 1.4 found - extended table doesn't exist");
#endif
		return;
	}
	extp = (u_char far *) ctp + base_len;
	if (((checksum(extp, ext_len) + ctp->ct_ext_cksum) & 0xFF) != 0) {
#ifdef	MPS_DEBUG
		printf("MP Extended Table checksum error");
#endif
		return;
	}
	mps_extp = extp;
	mps_ext_endp = mps_extp + ext_len;
}


int
mps_find_bus_res(int bus, int type, struct range **res)
{
	struct sasm far *sasmp;
	u_char far *extp;
	int res_cnt;

	if (mps_init == 0)
		mps_probe();
	if (mps_extp == NULL)
		return (0);
	extp = mps_extp;
	res_cnt = 0;
	while (extp < mps_ext_endp) {
		switch (*extp) {
		case SYS_AS_MAPPING:
			sasmp = (struct sasm far *) extp;
			if (((int) sasmp->sasm_as_type) == type &&
				((int) sasmp->sasm_bus_id) == bus) {
#ifdef	MPS_DEBUG
				int as_type;

				printf("System Address Space Mapping Entry:\n");
				printf("bus id = %d\n", sasmp->sasm_bus_id);
				as_type = (int) sasmp->sasm_as_type;
				printf("address type = %s\n", (as_type == 0) ?
					"I/O address" : ((as_type == 1) ?
					"Memory address" :
					"Prefetch address"));
				printf("address base = 0x%lx:%lx\n",
					sasmp->sasm_as_base_hi,
					sasmp->sasm_as_base);
				printf("lenght = 0x%lx:%lx\n",
					sasmp->sasm_as_len_hi,
					sasmp->sasm_as_len);
#endif
				if (sasmp->sasm_as_base_hi != 0 ||
					sasmp->sasm_as_len_hi != 0) {
					printf("64 bits address space\n");
					extp += SYS_AS_MAPPING_SIZE;
					break;
				}
				add_range_ur(sasmp->sasm_as_base,
					sasmp->sasm_as_len, res);
				res_cnt++;
			}
			extp += SYS_AS_MAPPING_SIZE;
			break;
		case BUS_HIERARCHY_DESC:
			extp += BUS_HIERARCHY_DESC_SIZE;
			break;
		case COMP_BUS_AS_MODIFIER:
			extp += COMP_BUS_AS_MODIFIER_SIZE;
			break;
		}
	}
	return (res_cnt);
}

u_char far *
find_sig(u_char far *cp, long len, char *sig)
{
	long i;

	/* Search for the "_MP_"  or "$HRT" signature */
	for (i = 0; i < len; i += 16) {
		if (memcmp(cp, sig, 4) == 0)
			return (cp);
		cp += 16;
	}
	return (NULL);
}

int
checksum(unsigned char far *cp, int len)
{
	int i;
	unsigned int cksum;

	for (i = cksum = 0; i < len; i++)
		cksum += (unsigned int) *cp++;

	return ((int) (cksum & 0xFF));
}
