/*
 * Copyright (c) 1992 Sun Microsystems, Inc.
 */

#ifndef	_ANLYZERR_H
#define	_ANLYZERR_H

#pragma ident	"@(#)anlyzerr.h	1.9	95/08/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* encoding for source field */
#define	NONE		0
#define	ON_BOARD	1
#define	BACKPLANE	2

/* encoding for chip field */
#define	CHIP_VALID	0x80
#define	CHIP_UNKNOWN	0x40
#define	BIC(data)	((int)((data) & 0x6) >> 1)
#define	BYTE(data)	((data) & 0x1)
#define	ENCODE_UNK_CHIP	(CHIP_VALID|CHIP_UNKNOWN)
#define	BIC_LOGSIZE	26
#define	NUM_BICS	4
#define	BYTES_PER_BIC	2

typedef struct {
	int word[4][2];
} bic_set;

typedef struct {
	struct {
		struct {
			unsigned int word[4];
		} bic[4];		/* 106 bits */
		unsigned int barb;	/*  11 bits */
	} bus[2];
} bus_interface_ring_status;

/* We must compress the BICs when storing them for the non-processor */
/* boards. Otherwise the information will not all fit into the NVRAM. */
typedef struct {
struct {
		int source;
		unsigned char chip0;
		unsigned char chip1;
		int barb_scan;  /* BARB shadow 0 scan chain (x2) */
	} bus[2];
} cmp_db_state;


int ae_cc(int, unsigned long long, int, int);
int ae_bw(int, int, unsigned long long, unsigned long long, int, int);
int ae_mqh(int, unsigned long long, unsigned long long, int, int);
int ae_ioc(int, unsigned long long, unsigned long long, int, int);
int ae_sbi(int, int, int, int);
int analyze_bics(bus_interface_ring_status *, int, int, int);
int dump_comp_bics(cmp_db_state *, int, int, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _ANLYZERR_H */
