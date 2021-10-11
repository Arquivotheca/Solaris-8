/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_CMPKT_H
#define	_SYS_DKTP_CMPKT_H

#pragma ident	"@(#)cmpkt.h	1.5	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	cmpkt {
	opaque_t	cp_objp;	/* ptr to generic ctlr object	*/
	opaque_t	cp_ctl_private;	/* ptr to controller private	*/
	opaque_t	cp_dev_private; /* ptr to device driver private */

	int		cp_scblen;	/* len of status control blk	*/
	opaque_t	cp_scbp;	/* status control blk		*/
	int		cp_cdblen;	/* len of cmd description blk	*/
	opaque_t	cp_cdbp;	/* command description blk	*/
	long		cp_reason;	/* error status			*/
	void		(*cp_callback)(); /* callback function		*/
	long		cp_time;	/* timeout values		*/
	long		cp_flags;

	struct buf	*cp_bp;		/* link to buf structure	*/
	long		cp_resid;	/* data bytes not transferred	*/
	long		cp_byteleft;	/* remaining bytes to do	*/

					/* for a particular disk section */
	long		cp_bytexfer;	/* bytes xfer in this operation */

	daddr_t		cp_srtsec;	/* starting sector number	*/
	long		cp_secleft;	/* # of sectors remains		*/

	ushort_t	cp_retry;	/* retry count			*/
	ushort_t	cp_resv;

	void		(*cp_iodone)(); /* target driver iodone()	*/
	struct cmpkt 	*cp_fltpktp;	/* fault recovery pkt pointer	*/
	opaque_t	cp_private;
	opaque_t	cp_passthru;	/* pass through command ptr	*/
};

/*	reason code for completion status				*/
#define	CPS_SUCCESS	0		/* command completes with no err */
#define	CPS_FAILURE	1		/* command fails		*/
#define	CPS_CHKERR	2		/* command fails with status	*/
#define	CPS_ABORTED	3		/* command aborted		*/

/*	flags definitions						*/
#define	CPF_NOINTR	0x0001		/* polling mode			*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CMPKT_H */
