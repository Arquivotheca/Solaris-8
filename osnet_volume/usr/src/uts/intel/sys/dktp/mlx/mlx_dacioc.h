/*
 * Copyright (c) 1997,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DKTP_MLX_DACIOC_H
#define	_SYS_DKTP_MLX_DACIOC_H

#pragma ident	"@(#)mlx_dacioc.h	1.7	99/05/04 SMI"

/*
 * Mylex DAC960 Host Adapter Driver Header File.  Non-SCSI public interfaces.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following ioctl commands are provided to support Mylex DAC960
 * non-SCSI commands targeted to System Drives (logical disks).  The
 * SCSI commands, however, can be sent ONLY to the non-System Drives
 * (e.g. tapes, CD-ROMs) through the user SCSI interface.  In other
 * words, the first argument of the ioctl() system call should be the
 * file descriptor of a System Drive node in the filesystem and not
 * that of a CD_ROM or tape.
 *
 * The third argument passed to ioctl() should be of type mlx_dacioc_t.
 * The user is expected to use these ioctl's in conjunction with the
 * Mylex DAC960 technical specifications.  However,
 *
 *	1.  The command opcodes will be mapped and assigned based on
 *	    the ioctl command and the user need not provide them, unless
 *	    for the DACIO_GENERIC command.
 *
 *	2.  The command identifiers are assigned and handled by the
 *	    Solaris driver and the user is not allowed to make any
 *	    assignments to REG[0] (zC91).
 *
 * There is no need to reboot the host after the following ioctl commands,
 * except after the ones which set and run in the diagnostic mode.  However,
 * there are ioctl commands in the following which are destructive in
 * nature, e.g.  Store and Program firmware image which are used to upgrade
 * the firmware.  For the latter class of the ioctl commands it is strongly
 * recommended that they should be performed when the system is in single
 * user mode or at least no critical I/O operation is being performed .
 */
#define	MLX_DACIOC	(('M' << 24) | ('L' << 16) | ('X' << 8))

/*
 * The MLX_DACIOC_GENERIC command can be used to send any non-SCSI ioctl
 * command to the Mylex DAC960 controller which
 *
 *	1. Does not involve scatter-gather type operations, and
 *
 *	2. Requires assignment(s) to hardware register(s).
 *
 * This generic interface is provided to support the future enhancements
 * to the DAC960 firmware which may involve non-SCSI DAC960 specific
 * commands yet unknown to the Solaris driver.
 *
 * WARNING:
 *	It is almost impossible to check the validity of the arguments
 *	passed via MLX_DACIOC_GENERIC.  Therefore, it is the responsibility
 *	of the user program to ensure the correctness of the arguments
 *	passed this way.
 */
#define	MLX_DACIOC_GENERIC	(MLX_DACIOC | 0) /* Generic wild card */

#define	MLX_DACIOC_FLUSH	(MLX_DACIOC | 1) /* Flush */
#define	MLX_DACIOC_SETDIAG	(MLX_DACIOC | 2) /* Set Diagnostic Mode */
#define	MLX_DACIOC_SIZE		(MLX_DACIOC | 3) /* Size of system drive */
#define	MLX_DACIOC_CHKCONS	(MLX_DACIOC | 4) /* Check Consistency */
#define	MLX_DACIOC_RBLD		(MLX_DACIOC | 5) /* Rebuild SCSI Disk */
#define	MLX_DACIOC_START	(MLX_DACIOC | 6) /* Start Device */
#define	MLX_DACIOC_STOPC	(MLX_DACIOC | 7) /* Stop Channel */
#define	MLX_DACIOC_STARTC	(MLX_DACIOC | 8) /* Start Channel */
#define	MLX_DACIOC_GSTAT	(MLX_DACIOC | 9) /* Get Device State */
#define	MLX_DACIOC_RBLDA	(MLX_DACIOC |10) /* Async Rebuild SCSI Disk */
#define	MLX_DACIOC_RESETC	(MLX_DACIOC |11) /* Reset Channel */
#define	MLX_DACIOC_RUNDIAG	(MLX_DACIOC |12) /* Run Diagnostic */
#define	MLX_DACIOC_OENQUIRY	(MLX_DACIOC |13) /* Enquire system config */
#define	MLX_DACIOC_WRCONFIG	(MLX_DACIOC |14) /* Write DAC960 Config */
#define	MLX_DACIOC_ORDCONFIG	(MLX_DACIOC |15) /* Read ROM Config */
#define	MLX_DACIOC_RBADBLK	(MLX_DACIOC |16) /* Read Bad Block Table */
#define	MLX_DACIOC_RBLDSTAT	(MLX_DACIOC |17) /* Rebuild Status */
#define	MLX_DACIOC_GREPTAB	(MLX_DACIOC |18) /* Get Replacement Table */
#define	MLX_DACIOC_GEROR	(MLX_DACIOC |19) /* Get history of errors */
#define	MLX_DACIOC_ADCONFIG	(MLX_DACIOC |20) /* Add Configuration */
#define	MLX_DACIOC_SINFO	(MLX_DACIOC |21) /* All system drives info */
#define	MLX_DACIOC_RDNVRAM	(MLX_DACIOC |23) /* Read NVRAM Config */
#define	MLX_DACIOC_LOADIMG	(MLX_DACIOC |24) /* Get firmware Image */
#define	MLX_DACIOC_STOREIMG	(MLX_DACIOC |25) /* Store firmware Image */
#define	MLX_DACIOC_PROGIMG	(MLX_DACIOC |26) /* Program Image */

#define	MLX_DACIOC_ENQUIRY	(MLX_DACIOC |27) /* Enquire system config */
#define	MLX_DACIOC_RDCONFIG	(MLX_DACIOC |28) /* Read ROM Config */

/*
 * The MLX_DACIOC_CARDINFO command is a Type 0 command which can be
 * used to get the values for a mlx_dacioc_cardinfo_t object.  This
 * command is not documented in the Mylex DAC960 technical
 * specifications and it is handled by the Solaris driver.  In other
 * words, it requires no assignments to the DAC960 hardware registers.
 */
#define	MLX_DACIOC_CARDINFO	(MLX_DACIOC |255) /* See below */

typedef struct mlx_dacioc_generic_args {
	/*
	 * A byte value is passed to be assigned to a known register
	 * (physical) address.  e.g. opcode.
	 */
	uchar_t val;
	ushort_t reg_addr;	/* absolute register address */
} mlx_dacioc_generic_args_t;

typedef union mlx_dacioc_args {
	/* Type 0 requires no args */

	struct {
		uchar_t drv;	/* REG7 */
		uint_t blk;	/* 26 bits of REG4 REG5 REG6 REG3 */
		uchar_t cnt;	/* REG2 */
	} type1;

	struct {
		uchar_t chn;		/* REG2 */
		uchar_t tgt;		/* REG3 */
		uchar_t dev_state;	/* REG4 */
	} type2;

	struct {
		uchar_t test;	/* REG2 */
		uchar_t pass;	/* REG3 */
		uchar_t chan;	/* REG4 */
	} type4;

	/* Type 5 */
	uchar_t param;		/* REG2 */

	struct {
		ushort_t count;	/* REG2 and REG3 */
		uint_t offset;	/* REG4 REG5 REG6 REG7 */
	} type6;

	/* Type generic */
	struct {
		mlx_dacioc_generic_args_t *gen_args;
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
} mlx_dacioc_args_t;

/*
 * The oldest supported mlx boards have a transfer length
 * of less than 64k. Newer boards tell us the optimal
 * length in the configuration information.
 */
#define	MLX_MAX_XFER	0xF800

typedef struct mlx_dacioc {
	mlx_dacioc_args_t args;
	caddr_t ubuf;		/* virtual addr of the user buffer */
	ushort_t ubuf_len; /* length of ubuf in bytes <=  MLX_DAC_MAX_XFER */
	uchar_t flags;		/* see below */
	ushort_t status;	/* returned after command completion */
} mlx_dacioc_t;		/* type of the 3rd arg to ioctl() */

/* Possible values for flags field of mlx_dacioc_t */
#define	MLX_DACIOC_UBUF_TO_DAC	1	/* data transfer from user to DAC960 */
#define	MLX_DACIOC_DAC_TO_UBUF	2	/* data transfer from DAC960 to user */

/*
 * Address of a mlx_dacioc_cardinfo_t object should be passed as the ubuf
 * field of a mlx_dacioc_t object in a MLX_DACIOC_CARDINFO command.  In the
 * mlx_dacioc_t object, ubuf_len should be the size of the
 * mlx_dacioc_cardinfo_t object and the args and flags fields are ignored.
 */
typedef struct  mlx_dacioc_cardinfo {
	ulong_t slot;		/* where the card is physically installed */
	uchar_t nchn;		/* number of channels on the card */
	uchar_t max_tgt;	/* max number of targets per channel */
	uchar_t irq;
}  mlx_dacioc_cardinfo_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_MLX_DACIOC_H */
