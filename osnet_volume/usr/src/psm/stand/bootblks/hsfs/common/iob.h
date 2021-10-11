/*
 * Copyright (c) 1990-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)iob.h	1.6	94/12/10 SMI"

/*
 * This struct keeps track of an open file in the standalone I/O system.
 *
 * It includes an IOB for device addess, a buffer for reading directory blocks
 * and a structure for volume.
 */
struct iob {
	void		*i_si;		/* I/O handle for this file */
	struct {
		off_t   si_offset;	/* byte offset */
		char	*si_ma;		/* memory address to r/w */
		int	si_cc;		/* character count to r/w */
		int	si_bn;		/* block number to r/w */
	} i_saio;
	struct inode	i_ino;		  /* Buffer for inode information */
	struct hs_volume ui_hsfs;	  /* Superblock for file system */
	char		i_buf[MAXBSIZE];  /* Buffer for reading dirs */
};

#define	i_offset	i_saio.si_offset
#define	i_bn		i_saio.si_bn
#define	i_ma		i_saio.si_ma
#define	i_cc		i_saio.si_cc
