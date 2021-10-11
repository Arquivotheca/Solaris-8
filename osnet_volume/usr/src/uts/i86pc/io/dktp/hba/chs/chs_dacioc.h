/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_DKTP_CHS_DACIOC_H
#define	_SYS_DKTP_CHS_DACIOC_H

#pragma	ident	"@(#)chs_dacioc.h	1.4	99/03/16 SMI"

/*
 * The are no public ioctls. These are left overs from mlx driver's
 * code.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CHS_DACIOC	(('C' << 24) | ('H' << 16) | ('S' << 8))

#define	CHS_DACIOC_GENERIC	(CHS_DACIOC | 0) /* Generic wild card */

#define	CHS_DACIOC_FLUSH	(CHS_DACIOC | 1) /* Flush */
#define	CHS_DACIOC_SETDIAG	(CHS_DACIOC | 2) /* Set Diagnostic Mode */
#define	CHS_DACIOC_SIZE		(CHS_DACIOC | 3) /* Size of system drive */
#define	CHS_DACIOC_CHKCONS	(CHS_DACIOC | 4) /* Check Consistency */
#define	CHS_DACIOC_RBLD		(CHS_DACIOC | 5) /* Rebuild SCSI Disk */
#define	CHS_DACIOC_START	(CHS_DACIOC | 6) /* Start Device */
#define	CHS_DACIOC_STOPC	(CHS_DACIOC | 7) /* Stop Channel */
#define	CHS_DACIOC_STARTC	(CHS_DACIOC | 8) /* Start Channel */
#define	CHS_DACIOC_GSTAT	(CHS_DACIOC | 9) /* Get Device State */
#define	CHS_DACIOC_RBLDA	(CHS_DACIOC |10) /* Async Rebuild SCSI Disk */
#define	CHS_DACIOC_RESETC	(CHS_DACIOC |11) /* Reset Channel */
#define	CHS_DACIOC_RUNDIAG	(CHS_DACIOC |12) /* Run Diagnostic */
#define	CHS_DACIOC_ENQUIRY	(CHS_DACIOC |13) /* Enquire system config */
#define	CHS_DACIOC_WRCONFIG	(CHS_DACIOC |14) /* Write DAC960 Config */
#define	CHS_DACIOC_RDCONFIG	(CHS_DACIOC |15) /* Read ROM Config */
#define	CHS_DACIOC_RBADBLK	(CHS_DACIOC |16) /* Read Bad Block Table */
#define	CHS_DACIOC_RBLDSTAT	(CHS_DACIOC |17) /* Rebuild Status */
#define	CHS_DACIOC_GREPTAB	(CHS_DACIOC |18) /* Get Replacement Table */
#define	CHS_DACIOC_GEROR	(CHS_DACIOC |19) /* Get history of errors */
#define	CHS_DACIOC_ADCONFIG	(CHS_DACIOC |20) /* Add Configuration */
#define	CHS_DACIOC_SINFO	(CHS_DACIOC |21) /* All system drives info */
#define	CHS_DACIOC_RDNVRAM	(CHS_DACIOC |23) /* Read NVRAM Config */
#define	CHS_DACIOC_LOADIMG	(CHS_DACIOC |24) /* Get firmware Image */
#define	CHS_DACIOC_STOREIMG	(CHS_DACIOC |25) /* Store firmware Image */
#define	CHS_DACIOC_PROGIMG	(CHS_DACIOC |26) /* Program Image */

/*
 * The CHS_DACIOC_CARDINFO command is a Type 0 command which can be
 * used to get the values for a chs_dacioc_cardinfo_t object.  This
 * command is not documented in the Mylex DAC960 technical
 * specifications and it is handled by the Solaris driver.  In other
 * words, it requires no assignments to the DAC960 hardware registers.
 */
#define	CHS_DACIOC_CARDINFO	(CHS_DACIOC |255) /* See below */
/* no data xfer during dacioc op */
#define	CHS_CCB_DACIOC_NO_DATA_XFER	0x4

typedef struct chs_dacioc_generic_args {
	/*
	 * A byte value is passed to be assigned to a known register
	 * (physical) address.  e.g. opcode.
	 */
	uchar_t val;
	ushort_t reg_addr;	/* absolute register address */
} chs_dacioc_generic_args_t;

typedef union chs_dacioc_args {
	/* Type 0 requires no args */

	struct {
		uchar_t drv;		/* REG7 */
		uint_t  blk;		/* 26 bits of REG4 REG5 REG6 REG3 */
		uchar_t cnt;		/* REG2 */
	} type1;

	struct {
		uchar_t chn;		/* REG2 */
		uchar_t tgt;		/* REG3 */
		uchar_t dev_state;	/* REG4 */
	} type2;

	struct {
		uchar_t test;		/* REG2 */
		uchar_t pass;		/* REG3 */
		uchar_t chan;		/* REG4 */
	} type4;

	/* Type 5 */
	uchar_t param;			/* REG2 */

	struct {
		ushort_t count;		/* REG2 and REG3 */
		uint_t   offset;	/* REG4 REG5 REG6 REG7 */
	} type6;

	/* Type generic */
	struct {
		chs_dacioc_generic_args_t *gen_args;
		ulong_t gen_args_len;	/* total length of gen_args in bytes */

		/*
		 * 1st of the 4 consecutive registers which will contain the
		 * physical address of transfer.  This register will contain
		 * the LSB of that physical address.
		 *
		 * In type1 to type6 this register is 0xzC98 (z is the slot
		 * number), but it may be assigned a different value in
		 * type_gen.
		 */
		ushort_t xferaddr_reg;
	} type_gen;
} chs_dacioc_args_t;

/*
 * XXX - IBM claims that CHS_DAC_MAX_XFER could be
 * 128 blocks (0x10000). Unfortunately the driver
 * isn't written to take advantage of this length.
 */
#define	CHS_DAC_MAX_XFER	0xF800

typedef struct chs_dacioc {
	chs_dacioc_args_t args;
	caddr_t  ubuf;	   /* virtual addr of the user buffer */
	ushort_t ubuf_len; /* length of ubuf in bytes <=  CHS_DAC_MAX_XFER */
	uchar_t flags;	   /* see below */
	ushort_t status;   /* returned after command completion */
} chs_dacioc_t;		   /* type of the 3rd arg to ioctl() */

/* Possible values for flags field of chs_dacioc_t */
#define	CHS_DACIOC_UBUF_TO_DAC	1	/* data transfer from user to hba */

#define	CHS_DACIOC_DAC_TO_UBUF	2	/* data transfer from hba to user */

/*
 * Address of a chs_dacioc_cardinfo_t object should be passed as the ubuf
 * field of a chs_dacioc_t object in a CHS_DACIOC_CARDINFO command.  In the
 * chs_dacioc_t object, ubuf_len should be the size of the
 * chs_dacioc_cardinfo_t object and the args and flags fields are ignored.
 */
typedef struct  chs_dacioc_cardinfo {
	ulong_t slot;		/* where the card is physically installed */
	uchar_t nchn;		/* number of channels on the card */
	uchar_t max_tgt;		/* max number of targets per channel */
	uchar_t irq;
}  chs_dacioc_cardinfo_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CHS_DACIOC_H */
