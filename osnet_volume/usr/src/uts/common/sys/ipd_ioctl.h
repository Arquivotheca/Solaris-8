/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_IPD_IOCTL_H
#define	_SYS_IPD_IOCTL_H

#pragma ident	"@(#)ipd_ioctl.h	1.9	98/06/11 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Declare different interface types
 */
enum ipd_iftype { IPD_NULL = 0, IPD_PTP = 1, IPD_MTP };

/*
 *  Declare different message types for communication between connection manager
 * and kernel
 *
 *		Message direction	Cnx Mgr  | Kernel
 */
enum ipd_msgs {	IPD_MAKE_IF = 0x100, 	/*	--->	*/
		IPD_CON_REQ,		/*	<---	*/
		IPD_DIS_REQ,		/*	<---	*/
		IPD_DIS_IND,		/*	<---	*/
		IPD_ERR_IND, 		/*	<---	*/
		IPD_SET_TIM,		/*	--->	*/
		IPD_BLACKLIST,		/*	--->	*/
		IPD_GET_INFO,		/*	<--> (not used)	*/
		IPD_CON_DECLINE,	/*	---> (not used)	*/
		IPD_GET_VERSION,	/*	<--- (not used)	*/
		IPD_REGISTER,		/*	--->	*/
		IPD_UNREGISTER		/*	--->	*/ };
/*
 * Structures for messages passed between the kernel and the ipd connection mgr
 */

/*
 * IPD_MAKE_IF message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_MAKE_IF */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface quantity */
} ipd_make_if_t;

/*
 * IPD_CON_REQ and IPD_DIS_REQ messages from IP/dialup
 * also sent down to IP/dialup with IPD_CON_DECLINE.
 */
typedef struct {
	uint_t			msg;		/* IPD_CON/DIS_REQ */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
} ipd_con_dis_t;

/*
 * IPD_DIS_IND message from IP/dialup
 */
typedef struct {
	uint_t			msg;		/* IPD_DIS_IND */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
	t_uscalar_t		reason;		/* disconnect reason */
} ipd_dis_ind_t;

/*
 * IPD_ERR_IND message from IP/dialup
 */
typedef struct {
	uint_t			msg;		/* IPD_ERR_IND */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
	uint_t			error;		/* error code */
} ipd_err_ind_t;

/*
 * IPD_SET_TIM message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_SET_TIM */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
	uint_t			timeout;	/* timeout in seconds */
} ipd_set_tim_t;

/*
 * IPD_REGISTER message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_REGISTER */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
} ipd_register_t;

/*
 * IPD_UNREGISTER message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_UNREGISTER */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
} ipd_unregister_t;

/*
 * IPD_BLACKLIST message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_BLACKLIST */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
	uint_t			blacklisttime;	/* blacklist time in seconds */
} ipd_blacklist_t;

/*
 * IPD_GET_INFO message from user
 */
typedef struct {
	uint_t			msg;		/* IPD_GET_INFO */
	enum ipd_iftype		iftype;		/* IPD_PTP or IPD_MTP */
	uint_t			ifunit;		/* interface unit number */
	struct sockaddr		sa;		/* address for this request */
	struct timeval		timeout;	/* current timeout in seconds */
	struct timeval		act_time;	/* when stream last accessed */
} ipd_get_info_t;

/*
 * and a union of them all
 */
union ipd_messages {
	uint_t			msg;
	ipd_make_if_t		make_if;
	ipd_con_dis_t		con_dis;
	ipd_dis_ind_t		dis_ind;
	ipd_err_ind_t		err_ind;
	ipd_set_tim_t		set_tim;
	ipd_blacklist_t		blacklist;
	ipd_get_info_t		get_info;
	ipd_register_t		regist;
	ipd_unregister_t	unregist;
	uint_t			version;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_IPD_IOCTL_H */
