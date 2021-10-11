/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Defines for user SCSI commands					*
 */

#ifndef _SYS_SCSI_IMPL_USCSI_H
#define	_SYS_SCSI_IMPL_USCSI_H

#pragma ident	"@(#)uscsi.h	1.23	99/03/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * definition for user-scsi command structure
 */
struct uscsi_cmd {
	int		uscsi_flags;	/* read, write, etc. see below */
	short		uscsi_status;	/* resulting status  */
	short		uscsi_timeout;	/* Command Timeout */
	caddr_t		uscsi_cdb;	/* cdb to send to target */
	caddr_t		uscsi_bufaddr;	/* i/o source/destination */
	size_t		uscsi_buflen;	/* size of i/o to take place */
	size_t		uscsi_resid;	/* resid from i/o operation */
	uchar_t		uscsi_cdblen;	/* # of valid cdb bytes */
	uchar_t		uscsi_rqlen;	/* size of uscsi_rqbuf */
	uchar_t		uscsi_rqstatus;	/* status of request sense cmd */
	uchar_t		uscsi_rqresid;	/* resid of request sense cmd */
	caddr_t		uscsi_rqbuf;	/* request sense buffer */
	void		*uscsi_reserved_5;	/* Reserved for Future Use */
};

#if defined(_SYSCALL32)
struct uscsi_cmd32 {
	int		uscsi_flags;	/* read, write, etc. see below */
	short		uscsi_status;	/* resulting status  */
	short		uscsi_timeout;	/* Command Timeout */
	caddr32_t	uscsi_cdb;	/* cdb to send to target */
	caddr32_t	uscsi_bufaddr;	/* i/o source/destination */
	size32_t	uscsi_buflen;	/* size of i/o to take place */
	size32_t	uscsi_resid;	/* resid from i/o operation */
	uchar_t		uscsi_cdblen;	/* # of valid cdb bytes */
	uchar_t		uscsi_rqlen;	/* size of uscsi_rqbuf */
	uchar_t		uscsi_rqstatus;	/* status of request sense cmd */
	uchar_t		uscsi_rqresid;	/* resid of request sense cmd */
	caddr32_t	uscsi_rqbuf;	/* request sense buffer */
	caddr32_t	uscsi_reserved_5;	/* Reserved for Future Use */
};

#define	uscsi_cmd32touscsi_cmd(u32, ucmd)				\
	ucmd->uscsi_flags	= u32->uscsi_flags;			\
	ucmd->uscsi_status	= u32->uscsi_status;			\
	ucmd->uscsi_timeout	= u32->uscsi_timeout;			\
	ucmd->uscsi_cdb		= (caddr_t)u32->uscsi_cdb;		\
	ucmd->uscsi_bufaddr	= (caddr_t)u32->uscsi_bufaddr;		\
	ucmd->uscsi_buflen	= (size_t)u32->uscsi_buflen;		\
	ucmd->uscsi_resid	= (size_t)u32->uscsi_resid;		\
	ucmd->uscsi_cdblen	= u32->uscsi_cdblen;			\
	ucmd->uscsi_rqlen	= u32->uscsi_rqlen;			\
	ucmd->uscsi_rqstatus	= u32->uscsi_rqstatus;			\
	ucmd->uscsi_rqresid	= u32->uscsi_rqresid;			\
	ucmd->uscsi_rqbuf	= (caddr_t)u32->uscsi_rqbuf;		\
	ucmd->uscsi_reserved_5	= (void *)u32->uscsi_reserved_5;


#define	uscsi_cmdtouscsi_cmd32(ucmd, u32)				\
	u32->uscsi_flags	= ucmd->uscsi_flags;			\
	u32->uscsi_status	= ucmd->uscsi_status;			\
	u32->uscsi_timeout	= ucmd->uscsi_timeout;			\
	u32->uscsi_cdb		= (caddr32_t)ucmd->uscsi_cdb;		\
	u32->uscsi_bufaddr	= (caddr32_t)ucmd->uscsi_bufaddr;	\
	u32->uscsi_buflen	= (size32_t)ucmd->uscsi_buflen;		\
	u32->uscsi_resid	= (size32_t)ucmd->uscsi_resid;		\
	u32->uscsi_cdblen	= ucmd->uscsi_cdblen;			\
	u32->uscsi_rqlen	= ucmd->uscsi_rqlen;			\
	u32->uscsi_rqstatus	= ucmd->uscsi_rqstatus;			\
	u32->uscsi_rqresid	= ucmd->uscsi_rqresid;			\
	u32->uscsi_rqbuf	= (caddr32_t)ucmd->uscsi_rqbuf;		\
	u32->uscsi_reserved_5	= (caddr32_t)ucmd->uscsi_reserved_5;

#endif /* _SYSCALL32 */


/*
 * flags for uscsi_flags field
 */
/*
 * generic flags
 */
#define	USCSI_WRITE	0x00000	/* send data to device */
#define	USCSI_SILENT	0x00001	/* no error messages */
#define	USCSI_DIAGNOSE	0x00002	/* fail if any error occurs */
#define	USCSI_ISOLATE	0x00004	/* isolate from normal commands */
#define	USCSI_READ	0x00008	/* get data from device */
#define	USCSI_RESET	0x04000	/* Reset target */
#define	USCSI_RESET_ALL	0x08000	/* Reset all targets */
#define	USCSI_RQENABLE	0x10000	/* Enable Request Sense extensions */

/*
 * suitable for parallel SCSI bus only
 */
#define	USCSI_ASYNC	0x01000	/* Set bus to asynchronous mode */
#define	USCSI_SYNC	0x02000	/* Return bus to sync mode if possible */

/*
 * the following flags should not be used at user level but may
 * be used by a scsi target driver for internal commands
 */
/*
 * generic flags
 */
#define	USCSI_NOINTR	0x00040	/* No interrupts, NEVER to use this flag */
#define	USCSI_NOTAG	0x00100	/* Disable tagged queueing */
#define	USCSI_OTAG	0x00200	/* ORDERED QUEUE tagged cmd */
#define	USCSI_HTAG	0x00400	/* HEAD OF QUEUE tagged cmd */
#define	USCSI_HEAD	0x00800	/* Head of HA queue */

/*
 * suitable for parallel SCSI bus only
 */
#define	USCSI_NOPARITY	0x00010	/* run command without parity */
#define	USCSI_NODISCON	0x00020	/* run command without disconnects */


#define	USCSI_RESERVED	0xfffe0000	/* Reserved Bits, must be zero */

struct uscsi_rqs {
	int		rqs_flags;	/* see below */
	ushort_t	rqs_buflen;	/* maximum number or bytes to return */
	ushort_t	rqs_resid;	/* untransferred length of RQS data */
	caddr_t		rqs_bufaddr;	/* request sense buffer */
};

#if defined(_SYSCALL32)
struct uscsi_rqs32	{
	int		rqs_flags;	/* see below */
	ushort_t	rqs_buflen;	/* maximum number or bytes to return */
	ushort_t	rqs_resid;	/* untransferred length of RQS data */
	caddr32_t	rqs_bufaddr;	/* request sense buffer */
};
#endif /* _SYSCALL32 */


/*
 * uscsi_rqs flags
 */

#define	RQS_OVR		0x01	/* RQS data has been overwritten */
#define	RQS_VALID	0x02	/* RQS data is valid */

/*
 * User SCSI io control command
 */
#define	USCSIIOC	(0x04 << 8)
#define	USCSICMD	(USCSIIOC|201) 	/* user scsi command */
#define	USCSIGETRQS	(USCSIIOC|202) 	/* retrieve SCSI sense data */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_IMPL_USCSI_H */
