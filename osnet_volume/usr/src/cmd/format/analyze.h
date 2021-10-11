
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ANALYZE_H
#define	_ANALYZE_H

#pragma ident	"@(#)analyze.h	1.10	98/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains definitions related to surface analysis.
 */

/*
 * These are variables referenced by the analysis routines.  They
 * are declared in analyze.c.
 */
extern	int scan_entire;
extern	daddr_t scan_lower, scan_upper;
extern	int scan_correct, scan_stop, scan_loop, scan_passes;
extern	int scan_random, scan_size, scan_auto;
extern	int scan_restore_defects, scan_restore_label;
extern	unsigned int scan_patterns[], purge_patterns[], alpha_pattern;

/*
 * These variables hold summary info for the end of analysis.
 * They are declared in analyze.c.
 */
extern	daddr_t scan_cur_block;
extern	int scan_blocks_fixed;

/*
 * This variable is used to tell whether the most recent surface
 * analysis error was caused by a media defect or some other problem.
 * It is declared in analyze.c.
 */
extern	int media_error;
extern	int disk_error;

/*
 * These defines are flags for the surface analysis types.
 */
#define	SCAN_VALID		0x01		/* read data off disk */
#define	SCAN_PATTERN		0x02		/* write and read pattern */
#define	SCAN_COMPARE		0x04		/* manually check pattern */
#define	SCAN_WRITE		0x08		/* write data to disk */
#define	SCAN_PURGE		0x10		/* purge data on disk */
#define	SCAN_PURGE_READ_PASS	0x20		/* read/compare pass */
#define	SCAN_PURGE_ALPHA_PASS	0x40		/* alpha pattern pass */
#define	SCAN_VERIFY		0x80		/* verify data on disk */
#define	SCAN_VERIFY_READ_PASS	0x100		/* read/compare pass */


/*
 * Miscellaneous defines.
 */
#define	BUF_SECTS		126		/* size of the buffers */
/*
 * Number of passes for purge command.  It is kept here to allow
 * it to be used in menu_analyze.c also
 * This feature is added at the request of Sun Fed.
 */
#define	NPPATTERNS	4	/* number of purge patterns */
#define	READPATTERN	(NPPATTERNS - 1)


/*
 * defines for disk errors during surface analysis.
 */
#define	DISK_STAT_RESERVED		0x01	/* disk is reserved */
#define	DISK_STAT_NOTREADY		0x02	/* disk not ready */
#define	DISK_STAT_UNAVAILABLE		0x03	/* disk is being formatted */

/*
 *	Prototypes for ANSI C compilers
 */
int	do_scan(int flags, int mode);
int	scan_repair(daddr_t bn, int mode);
int	analyze_blocks(int flags, daddr_t blkno, int blkcnt,
		unsigned data, int init, int driver_flags, int *xfercntp);
int	handle_error_conditions(void);
int	verify_blocks(int flags, daddr_t blkno, int blkcnt,
		unsigned data, int driver_flags, int *xfercntp);

#ifdef	__cplusplus
}
#endif

#endif	/* _ANALYZE_H */
