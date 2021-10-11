/*
 * Copyright (c) 1990 Sun Microsystems, Inc.
 */

#pragma	ident  "@(#)anlyzerr.c 1.19     99/04/29 SMI"

/*
 * Analyze sun4d specific error data gathered by Open Boot PROM
 */

#include <stdio.h>
#include <string.h>
#include "anlyzerr.h"

#define	BD_PID 0x80


static int decode_bic_set(bic_set *, int, char *, int, int, int *);
static void check_bit(bic_set *, int, int, int);

extern void print_header(int);
extern void log_printf(char *, ...);

/*
 * Hardware error register meanings. The following strings are
 * indexed for the bit positions of the corresponding bits in the
 * hardware. The code checks bit x of the hardware error register
 * and prints out string[x] if the bit is turned on.
 */
static char *ioc_error_txt[] = {
	{" DOHU"},				/* 0 */
	{" DOHO"},				/* 1 */
	{" DOLU"},				/* 2 */
	{" DOLO"},				/* 3 */
	{" BDOHU"},				/* 4 */
	{" BDOHO"},				/* 5 */
	{" BDOLU"},				/* 6 */
	{" BDOLO"},				/* 7 */
	{""},					/* 8 */
	{""},					/* 9 */
	{""},					/* 10 */
	{""},					/* 11 */
	{""},					/* 12 */
	{""},					/* 13 */
	{""},					/* 14 */
	{""},					/* 15 */
	{" DIHU"},				/* 16 */
	{" DIHO"},				/* 17 */
	{" DILU"},				/* 18 */
	{" DILO"},				/* 19 */
	{" DEVIDFU"},				/* 20 */
	{" DEVIDFO"},				/* 21 */
	{""},					/* 22 */
	{""},					/* 23 */
	{""},					/* 24 */
	{""},					/* 25 */
	{""},					/* 26 */
	{""},					/* 27 */
	{""},					/* 28 */
	{""},					/* 29 */
	{""},					/* 30 */
	{""},					/* 31 */
	{" XINQU"},				/* 32 */
	{" XINQO"},				/* 33 */
	{" DSTRQU"},				/* 34 */
	{" DSTRQO"},				/* 35 */
	{""},					/* 36 */
	{""},					/* 37 */
	{""},					/* 38 */
	{""},					/* 39 */
	{" ILE"},				/* 40 */
	{" PSE"},				/* 41 */
	{" RBE"},				/* 42 */
	{""},					/* 43 */
	{""},					/* 44 */
	{""},					/* 45 */
	{""},					/* 46 */
	{""},					/* 47 */
	{""},					/* 48 */
	{""},					/* 49 */
	{""},					/* 50 */
	{""},					/* 51 */
	{""},					/* 52 */
	{""},					/* 53 */
	{" XIOERR"},				/* 54 */
	{" XPE"},				/* 55 */
};


static char *mqh_error_txt[] = {
	{""},					/* 0 */
	{""},					/* 1 */
	{"  OQFIFO"},				/* 2 */
	{"  CMDQUE"},				/* 3 */
	{"  MEM_MSTR"},				/* 4 */
	{"  MEM_SLV"},				/* 5 */
	{"  MCRAM_CTR"},			/* 6 */
	{""},					/* 7 */
	{"  CMDQUE Overflow"},			/* 8 */
	{"  XDBus Packet Length Error"},	/* 9 */
	{"  Unexpected Arbiter Grant"},		/* 10 */
	{""},					/* 11 */
	{""},					/* 12 */
	{""},					/* 13 */
	{""},					/* 14 */
	{""},					/* 15 */
};

static char *bw_error_txt[] = {
	{" URE"},				/* 0 */
	{" IOWSCE"},				/* 1 */
	{" WSKBCE"},				/* 2 */
	{" RM1CE"},				/* 3 */
	{" RM2CE"},				/* 4 */
	{" WMCE"},				/* 5 */
	{" IOWBCE"},				/* 6 */
	{" SRCE"},				/* 7 */
	{" IORCE"},				/* 8 */
	{" UXC"},				/* 9 */
	{" UMFT"},				/* 10 */
	{" UER"},				/* 11 */
	{" ICMFT"},				/* 12 */
	{" CCE"},				/* 13 */
	{" XPE"},				/* 14 */
	{" SGT"},				/* 15 */
	{" WSTO"},				/* 16 */
	{" INVFIFOO"},				/* 17 */
	{" DREFIFOU"},				/* 18 */
	{" DREFIFOO"},				/* 19 */
	{" DRPFIFOU"},				/* 20 */
	{" DRPFIFOO"},				/* 21 */
	{" IDFIFOU"},				/* 22 */
	{" IDFIFOO"},				/* 23 */
	{" CPFIFOU"},				/* 24 */
	{" CPFIFOO"},				/* 25 */
	{" FBRFIFOU"},				/* 26 */
	{" FBRFIFOO"},				/* 27 */
	{" RBRFIFOU"},				/* 28 */
	{" RBRFIFOO"},				/* 29 */
	{" XBFIFOU"},				/* 30 */
	{" XBFIFOO"},				/* 31 */
	{" MDMP"},				/* 32 */
	{" PFMAE"},				/* 33 */
	{" PFMBE0"},				/* 34 */
	{" PFMBE1"},				/* 35 */
	{" PFMCE"},				/* 36 */
	{" TPE"},				/* 37 */
	{" FPE"},				/* 38 */
	{" UVA"},				/* 39 */
	{" IOWS"},				/* 40 */
};
static char *dcsr_txt[] = {
	{"  Client Device Error, Internal Error(s) ="},	/* 0 */
	{""},						/* 1 */
	{""},						/* 2 */
	{""},						/* 3 */
	{"  XDBus Parity Error, Data = %8.8X.%8.8X"
		" Parity = %2.2X\n"},			/* 4 */
	{"  Grant Parity Error, Arbiter Signals =\n"
	"      Grant Type = %1X Shared = %1X Owner = %1X Board Grant = "
		"%1X Parity = %1X\n"},			/* 5 */
	{"  Grant Timeout\n"},				/* 6 */
	{"  Multiple Errors\n"},			/* 7 */
	{""},						/* 8 */
	};

static int ae_dcsr(unsigned long long,
		unsigned long long,
		int,
		int,
		char *);

/*
 * Analyze the common parts of XDBus Control and Status Register (DCSR).
 * Append error messages onto the error log buffer if any errors found.
 * This register exists in the BW, MQH, and IOC's.
 */
static int
ae_dcsr(unsigned long long dcsr,
	unsigned long long ddr,
	int board,
	int hdr_printed,
	char *chip)
{
	int err = 0;

	if (dcsr & (1<<7))  {
		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}

		if (!err)
			log_printf("%s", chip, 0);
		err = 1;
		log_printf(dcsr_txt[7], 0); /* Multiple Errors */
	}

	if (dcsr & (1<<6)) {
		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}
		if (!err)
			log_printf("%s", chip, 0);
		err = 1;
		log_printf(dcsr_txt[6], 0); /* Grant Timeout */
	}

	if (dcsr & (1<<5)) { 			/* Grant Parity Error */
		int err_log;

		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}
		if (!err)
			log_printf("%s", chip, 0);
		err = 1;
		/* err_log has arbiter signals */
		err_log = (dcsr >> 19) & 0xff;
		log_printf(dcsr_txt[5], err_log&7, (err_log>>3)&1,
			(err_log>>4)&1, (err_log>>5)&1, (err_log>>6)&1, 0);
	}

	/* when parity error err log has parity bits */
	if (dcsr & (1<<4))  {
		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}
		if (!err)
			log_printf("%s", chip, 0);
		err = 1;
		log_printf(dcsr_txt[4], ddr, (int)((dcsr >> 19) & 0xff), 0);
	}
	return (hdr_printed);
}

/*
 * Analyze the MXCC Error register. Append messages onto the error log
 * buffer if any errors found.
 */
int
ae_cc(int dev_id,
	unsigned long long cc_err,
	int board,
	int hdr_printed)
{
	/* if any error bits set, analyze them */
	if (cc_err & 0xfc00000000000000)  {
		char cpu;	/* which CPU has the error */

		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}

		if (dev_id & 0x8)
			cpu = 'B';
		else
			cpu = 'A';

		log_printf("MXCC (CPU %c)\n", cpu, 0);

		if ((cc_err >> 63) & 1)
			log_printf("  Multiple Errors\n", 0);

		if ((cc_err >> 62) & 1)
			log_printf("  XBus Parity Error\n", 0);

		if ((cc_err >> 61) & 1)
			log_printf("  Cache Consistency Error\n", 0);

		if ((cc_err >> 60) & 1)
			log_printf("  CPU Bus Parity Error\n", 0);

		if ((cc_err >> 59) & 1)
			log_printf("  Cache Parity Error\n", 0);

		if ((cc_err >> 58) & 1)
			log_printf("  Asynchronous Error\n", 0);

		if ((cc_err >> 57) & 1)  {
			log_printf("  Error Valid, CCOP=%3X "
				"ERR=%2X PA=%1X.%8.8X\n",
				    (int)((cc_err >>47) & 0x3ff),
				    (int)((cc_err >> 39) & 0xff),
				    (int)((cc_err >> 32) & 0xf),
				    (int)(cc_err & 0xffffffff), 0);
		}
	}
	return (hdr_printed);
}


/*
 * Analyze the Bus Watcher Error registers. Append messages onto the error
 * log buffer if any errors found in the data.
 */
int
ae_bw(int devid,
	int bus,
	unsigned long long dcsr,
	unsigned long long ddr,
	int board,
	int hdr_printed)
{
	char chip[32];
	char cpu;
	int i;

	/* if devid does not make sense then this chip was not inited */
	if (devid == ((dcsr >> 28) & 0xff)) {
		if (devid & 0x8)
			cpu = 'B';
		else
			cpu = 'A';

		(void) sprintf(chip, "BW%1d (CPU %c)", bus, cpu);

		/* go analyze the common bits of the dcsr */
		hdr_printed = ae_dcsr(dcsr, ddr, board, hdr_printed, chip);

		/* now analyze the chip specific error bits */
		if (dcsr & 0x1) {	/* Client Device Error */
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}
			log_printf("BW%1d (CPU %c)\n", bus, cpu, 0);
			log_printf(dcsr_txt[0], 0);
			for (i = 0; i <= 40; i++) {
				if (ddr & 1)
					log_printf(bw_error_txt[i], 0);
				ddr >>= 1;
			}
			log_printf("\n", 0);
		}
	}
	return (hdr_printed);
}

/*
 * Analyze the Memory Queue Handler Error registers. Append messages onto
 * the error log buffer if any errors found in the data.
 */
int
ae_mqh(int bus, unsigned long long dcsr,
	unsigned long long ddr,
	int board,
	int hdr_printed)
{
	int i;
	int devid;
	char chip[32];

	devid = (dcsr >> 28) & 0xff;
	if (devid == ((board << 4)|1)) {

		(void) sprintf(chip, "MQH%1d", bus);

		/* go analyze the common bits of the dcsr */
		hdr_printed = ae_dcsr(dcsr, ddr, board, hdr_printed, chip);

		/* now analyze the chip specific error bits */
		if ((i = (dcsr & 0xf)) != 0) {	/* Client Device Error */
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}
			log_printf("MQH%1d Client Device Error = %s\n",
				bus, mqh_error_txt[i], 0);
		}
	}
	return (hdr_printed);
}

/*
 * Analyze the IO Controller Error registers. Append messages onto
 * the error log buffer if any errors found in the data.
 */
int
ae_ioc(int bus,
	unsigned long long dcsr,
	unsigned long long ddr,
	int board,
	int hdr_printed)
{
	int devid, i;
	char chip[32];
	unsigned long long tmp;

	devid = (dcsr >> 28) & 0xff;
	if (devid == ((board << 4)|2)) {
		(void) sprintf(chip, "IOC%1d\n", bus);

		/* go analyze the common bits of the dcsr */
		hdr_printed = ae_dcsr(dcsr, ddr, board, hdr_printed, chip);

		/* print chip header if any error present */
		if (dcsr & 0x7)
			log_printf("%s", chip, 0);

		/* Check  Client Device Error */
		if (dcsr & 4) {
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}
			log_printf("  Store Timeout\n", 0);
		}

		if (dcsr & 2)  {
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}

			log_printf("  XDBus Error, Header Cycle = "
				"%X.%X\n", (int)(ddr >> 32),
				    (int)(ddr & 0xFFFFFFFF), 0);
		}

		if (dcsr & 1) { /* Analyze Internal Error */
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}

			log_printf(dcsr_txt[0], 0);
			tmp = ddr;
			for (i = 0; i <= 55; i++) {
				if (tmp & 1) {
					log_printf(" %s ", ioc_error_txt[i], 0);
				}
				tmp >>= 1;
			}
			log_printf("\n", 0);
			/* some errors log additional data in the DDR */
			if ((ddr >> 40) & 7) { /* RBE|PSE|ILE */
				log_printf("Command from header = "
					"%2X Ow = %1X Err = %1X\n",
					    (int)(ddr>>58), (int)(ddr>>56) & 1,
					    (int)(ddr>>57) & 1, 0);
			}
		}
	}
	return (hdr_printed);
}

/*
 * Analyze the SBus Interface Error registers. Append messages onto
 * the error log buffer if any errors found in the data.
 */
int
ae_sbi(int ioc_devid, int sr, int board, int hdr_printed)
{
	/*
	 * To determine if the data logged is valid information
	 * make sure the IOC has been inited (check IOC devid)
	 * and that not every one of the SBI error bits are set
	 */
	if ((sr != 0xf) && (ioc_devid == ((board << 4)|2))) {
		if (sr & 0xf) {  /* any errors set ? */
			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}
		log_printf("SBI\n", 0);
			if ((sr & 1))
				log_printf("  PTE Parity Error\n", 0);

			if ((sr >> 1) & 1)
				log_printf("  XBus Parity Error\n", 0);

			if ((sr >> 2) & 1)
				log_printf("  XBus Protocol Error\n", 0);

			if ((sr >> 3) & 1)
				log_printf("  Finite State Machine Error\n", 0);
		}
	}
	return (hdr_printed);
}

/*
 * Analyze a JTAG scan dump of Bus Interface Chips(BICs). Append messages
 * to the error long buffer if any parity errors are found.
 */
int
analyze_bics(bus_interface_ring_status *bus_status,
		int bus,
		int board,
		int hdr_printed)
{
	bic_set bckpln;	/* 8 bytes from backplane */
	bic_set brd;	/* 8 bytes from board */
	int bic;	/* BIC under scrutiny */
	int result = 0;	/* result of BIC analysis */

	/* separate the BIC logs into incoming and outgoing for each */
	/* byte for each BIC */
	for (bic = 0; bic < NUM_BICS; bic++) {
		bckpln.word[bic][0] =
		    (bus_status->bus[bus].bic[bic].word[3] >> 26) |
		    ((bus_status->bus[bus].bic[bic].word[2] & 0xFFFFF) << 6);

		bckpln.word[bic][1] =
		    bus_status->bus[bus].bic[bic].word[3] & 0x03FFFFFF;

		brd.word[bic][0] =
		    (bus_status->bus[bus].bic[bic].word[1] >> 18) |
		    ((bus_status->bus[bus].bic[bic].word[0] & 0xFFF) << 14);

		brd.word[bic][1] =
		    (bus_status->bus[bus].bic[bic].word[2] >> 20) |
		    ((bus_status->bus[bus].bic[bic].word[1] & 0x3FFF) << 12);
	}

	/* check the current board first */
	/* if we caused the parity error, STOP checking */
	hdr_printed = decode_bic_set(&brd, bus, "On Board", board,
		hdr_printed, &result);

	/* look at the incoming bytes */
	if (result != -1)
		hdr_printed = decode_bic_set(&bckpln, bus, "Backplane",
			board, hdr_printed, &result);

	return (hdr_printed);
}	/* end of analyze_bics() */

/*
 * Analyze a compressed JTAG scan dump of Bus Interface Chips (BICs).
 * Append messages to the error long buffer if any parity errors are found.
 */
int
dump_comp_bics(cmp_db_state *bic_info,
		int nbus,
		int board,
		int hdr_printed)
{
	int i;

	for (i = 0; i < nbus; i++) {
		if (bic_info->bus[i].source != NONE) {
			char *word;

			if (!hdr_printed) {
				print_header(board);
				hdr_printed = 1;
			}
			if (bic_info->bus[i].source == ON_BOARD)
				word = "Board";
			else

				word = "Backplane";

			log_printf("XDBus %d Parity error caused by %s, ",
				i, word, 0);

			if (bic_info->bus[i].chip0 != ENCODE_UNK_CHIP) {
				log_printf("detected by BIC %d byte %d\n",
				    BIC(bic_info->bus[i].chip0),
				    BYTE(bic_info->bus[i].chip0), 0);
			} else {
				log_printf("detected by unknown chip\n", 0);
			}
		}
	}
	return (hdr_printed);
}	/* end of dump_comp_bics() */

/*
 * This function is passed a BIC set, either the incoming or outgoing
 * bytes, and determines whether a parity error or detected. If so, it
 * attempts to determine which BIC and byte in that BIC caused the
 * problem. Up to two bits can be mismatched in the parity log and the
 * code can still identify the offending byte lanes. Otherwise the
 * offending byte lane is unknown and the bad parity log is printed.
 * If a bad parity log is found, FAIL is returned. Otherwise PASS is
 * returned. If any errors are found, the messages are asppended to the
 * error log buffer.
 */
static int
decode_bic_set(bic_set *set,
		int xpb,	/* XDBus index */
		char *source,
		int board,
		int hdr_printed,
		int *result)
{
	int i;

	/* go through the BIC logs starting at the oldest data and report */
	/* any mismatched parity bits */

	/* set the XDBus parity error source field to NONE */

	for (i = 0; i < BIC_LOGSIZE; i++) {
		int byte;
		int bic;
		int xor = 0;
		int sum = 0;

		/* look at the outgoing bytes first */
		for (bic = 0; bic < NUM_BICS; bic++)
			for (byte = 0; byte < BYTES_PER_BIC; byte++) {
				xor = xor ^ ((set->word[bic][byte] >> i) &
					0x1);
				sum += ((set->word[bic][byte] >> i) & 0x1);
			}

		/* if xor non-zero, parity error was detected */
		if (xor == 0)
			continue;

		if (!hdr_printed) {
			print_header(board);
			hdr_printed = 1;
		}

		log_printf("XDBus %d Parity error caused by %s, ",
			xpb, source, 0);

		if ((sum == 7) || (sum == 6)) {
			/* print all BIC numbers with even parity */
			for (bic = 0; bic < NUM_BICS; bic++)
				check_bit(set, bic, 1, i);
		} else if ((sum == 1) || (sum == 2)) {
			for (bic = 0; bic < NUM_BICS; bic++)
				check_bit(set, bic, 0, i);
		} else {
			/* print unknown BIC caused problem */
			log_printf("detected by unknown chip\n", 0);
		}

		*result = -1;
		return (hdr_printed);	/* don't keep checking if error found */
	}	/* end of for entire LOG */

	return (hdr_printed);
}	/* end of decode_bic_set() */

/*
 * Check a BIC history log bit for both bytes transmitted by a BIC. If
 * an error is found, it is appended to the error log buffer.
 */
static void
check_bit(bic_set *set,
		int bic,
		int expected,
		int shift)
{
	int byte;

	for (byte = 0; byte < BYTES_PER_BIC; byte++) {
		if (((set->word[bic][byte] >> shift) & 0x1) != expected) {
			log_printf("detected by BIC %d byte %d\n",
				bic, byte, 0);
		}
	}
}	/* end of check_bit() */
