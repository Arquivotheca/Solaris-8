/*
 * Copyright (c) 1996, by Sun Microsystems Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DADA_IMPL_UDCD_H
#define	_SYS_DADA_IMPL_UDCD_H

#pragma ident	"@(#)udcd.h	1.9	98/02/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * definition for user dcd command  structure
 */

struct udcd_cmd {
	uchar_t	udcd_error_reg;	/* The error register value */
	uchar_t	udcd_status_reg; /* The status register */
	ushort_t	udcd_status;	/* The resulting status */
	ushort_t	udcd_timeout;	/* Timeout value for completion */
	int	udcd_flags;	/* Flags for specifying  read,write etc. */
	uint_t	udcd_resid;	/* This is the resid */
	uint_t	udcd_buflen;	/* Size of the io request */
	caddr_t	udcd_bufaddr;	/* Place to take the data or put the data in */
	struct	dcd_cmd *udcd_cmd; /* Command to be sent out */
	void	*udcd_reserved;	/* reserved for future use */
	uint_t	version_no;	/* Version number for this struct */
};

/*
 * Flags for the Udcd_flags field
 */
#define	UDCD_WRITE	0x00000 /* Send data to device */
#define	UDCD_SILENT	0x00001	/* no error messages */
#define	UDCD_DIAGNOSE	0x00002 /* Fail of any error occurs */
#define	UDCD_ISOLATE	0x00004	/* isolate from normal command */
#define	UDCD_READ	0x00008	/* Read data from device */
#define	UDCD_NOINTR	0x00040 /*  No interrupts */
#define	UDCD_RESET	0x04000 /* Reset the target */


/*
 * User ATA io control command
 */
#define	UDCDIOC	(0x05 << 8)
#define	UDCDCMD	(UDCDIOC|201) /* User dcd command */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DADA_IMPL_UDCD_H */
