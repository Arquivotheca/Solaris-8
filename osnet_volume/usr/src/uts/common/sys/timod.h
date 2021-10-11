/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TIMOD_H
#define	_SYS_TIMOD_H

#pragma ident	"@(#)timod.h	1.31	98/04/28 SMI"	/* SVr4.0 11.4 */

#include <sys/types.h>
#include <sys/stream.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Timod ioctls */
#define		TIMOD 		('T'<<8)
#define		TI_GETINFO	(TIMOD|140)
#define		TI_OPTMGMT	(TIMOD|141)
#define		TI_BIND		(TIMOD|142)
#define		TI_UNBIND	(TIMOD|143)
#define		TI_GETMYNAME	(TIMOD|144)
#define		TI_GETPEERNAME	(TIMOD|145)
#define		TI_SETMYNAME	(TIMOD|146)
#define		TI_SETPEERNAME	(TIMOD|147)
#define		TI_SYNC		(TIMOD|148)
#define		TI_GETADDRS	(TIMOD|149)
#define		TI_CAPABILITY	(TIMOD|150)

/*
 * There are two ioctls to get information from the kernel. One is TI_SYNC
 * and it should be only used to exchange information between the library and
 * timod. It should not request any TPI information. The second ioctl is
 * TI_CAPABILITY which is extensible ioctl for getting all the information from
 * transport provider.
 */

/* sent with TI_SYNC */
struct ti_sync_req {
	uint32_t	tsr_flags;
	/* can grow at the end */
};

/*
 * For use with tsr_flags
 * TSRF_INFO_REQ is obsolete and shouldn't be used in new code. Use
 * TI_CAPABILITY ioctl instead.
 */
#define	TSRF_INFO_REQ		0x1 /* get info about transport endpoint */
#define	TSRF_IS_EXP_IN_RCVBUF	0x2 /* look for exp ind in rcvbuf */
#define	TSRF_QLEN_REQ		0x4 /* get qlen from timod */

/* returned by TI_SYNC  */
struct ti_sync_ack {
	/*
	 * - initial part derived from and matches T_info_ack
	 * - returned when TSRF_INFO_REQ is set on request
	 */
	t_scalar_t	PRIM_type;
	t_scalar_t	TSDU_size;
	t_scalar_t	ETSDU_size;
	t_scalar_t	CDATA_size;
	t_scalar_t	DDATA_size;
	t_scalar_t	ADDR_size;
	t_scalar_t	OPT_size;
	t_scalar_t	TIDU_size;
	t_scalar_t	SERV_type;
	t_scalar_t	CURRENT_state;
	t_scalar_t	PROVIDER_flag;

	/*
	 * endpoint qlen backlog, returned when TSRF_INFO_REQ is set on request
	 */
	t_uscalar_t	tsa_qlen;

	/*
	 * misc flags info - bits set based on what is requested.
	 */
	uint32_t tsa_flags;
	/* can grow at the end */
};

/*
 * Flag bits for use with tsa_flags
 */

/*
 * TSAF_EXP_QUEUED:
 * 	set/clear significant when TSRF_IS_EXP_IN_RCVBUF is set on request
 */
#define	TSAF_EXP_QUEUED	0x1

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIMOD_H */
