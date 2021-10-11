#ident	"@(#)iob.h	1.4	92/07/14 SMI" /* from SunOS 4.1 */ 

/* Copyright (c) 1990 by Sun Microsystems, Inc. */

/*
 * This struct keeps track of an open file in the standalone I/O system.
 *
 * It includes an IOB for device addess, an inode, a buffer for reading
 * indirect blocks and inodes, and a buffer for the superblock of the
 * file system (if any).
 */
typedef struct iob {
	struct saioreq	iob_si;		/* I/O request block for this file */
	struct inode	iob_ino;		/* Inode for this file */
	char		iob_buf[MAXBSIZE];/* Buffer for reading inodes & dirs */
	struct iob	*iob_forw;
	struct iob	*iob_back;
	char		*iob_desc;
	int		iob_fd;
} iob_t;

/*  We only require 1 of these since we only open 1 dev at a time */
struct device_desc {
	union {
		struct fs dd_fs;
		char dummy[SBSIZE];
	}	dd_un;
	char	*dd_desc;
};


#define iob_flgs	iob_si.si_flgs
#define iob_boottab	iob_si.si_boottab
#define iob_devdata	iob_si.si_devdata
#define iob_ctlr	iob_si.si_ctlr
#define iob_unit	iob_si.si_unit
#define iob_boff	iob_si.si_boff
#define iob_cyloff	iob_si.si_cyloff
#define iob_offset	iob_si.si_offset
#define iob_bn		iob_si.si_bn
#define iob_ma		iob_si.si_ma
#define iob_cc		iob_si.si_cc
#define	iob_devaddr	iob_si.si_devaddr
#define	iob_dmaaddr	iob_si.si_dmaaddr

#define dd_fs		dd_un.dd_fs
