/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IOB_H
#define	_IOB_H

#ident	"@(#)iob.h	1.6	99/01/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * This struct keeps track of an open file in the standalone I/O system.
 *
 * It includes an IOB for device address, an inode, a buffer for reading
 * indirect blocks and inodes, and a buffer for the superblock of the
 * file system (if any).
 *
 */

struct iob {
	struct saioreq	i_si;		/* I/O request block for this file */
	struct inode	i_ino;		/* Inode for this file */
	char		i_buf[MAXBSIZE];/* Buffer for reading inodes & dirs */
	union {
		struct fs ui_fs;	/* Superblock for file system */
		char dummy[SBSIZE];
	}		i_un;
};
#define i_flgs		i_si.si_flgs
#define i_boottab	i_si.si_boottab
#define i_devdata	i_si.si_devdata
#define i_ctlr		i_si.si_ctlr
#define i_unit		i_si.si_unit
#define i_boff		i_si.si_boff
#define i_cyloff	i_si.si_cyloff
#define i_offset	i_si.si_offset
#define i_bn		i_si.si_bn
#define i_ma		i_si.si_ma
#define i_cc		i_si.si_cc

/*
 * i_fs conflicts with a definition in ufs_inode.h...
 */
#define iob_fs		i_un.ui_fs

#ifdef	__cplusplus
}
#endif

#endif /* _IOB_H */
