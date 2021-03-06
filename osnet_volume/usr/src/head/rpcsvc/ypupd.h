/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	Copyright (c) 1986-1989,1997-1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 *	Copyright (c) 1983-1989 by AT&T.
 *	All rights reserved.
 */

#ifndef	_RPCSVC_YPUPD_H
#define	_RPCSVC_YPUPD_H

#pragma ident	"@(#)ypupd.h	1.8	98/01/06 SMI"	/* SVr4.0 1.1	*/

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Compiled from ypupdate_prot.x using rpcgen
 * This is NOT source code!
 * DO NOT EDIT THIS FILE!
 */
#define	MAXMAPNAMELEN 255
#define	MAXYPDATALEN 1023
#define	MAXERRMSGLEN 255

#define	YPU_PROG ((ulong_t)100028)
#define	YPU_VERS ((ulong_t)1)
#define	YPU_CHANGE ((ulong_t)1)
extern uint_t *ypu_change_1();
#define	YPU_INSERT ((ulong_t)2)
extern uint_t *ypu_insert_1();
#define	YPU_DELETE ((ulong_t)3)
extern uint_t *ypu_delete_1();
#define	YPU_STORE ((ulong_t)4)
extern uint_t *ypu_store_1();

typedef struct {
	uint_t yp_buf_len;
	char *yp_buf_val;
} yp_buf;
bool_t xdr_yp_buf();

struct ypupdate_args {
	char *mapname;
	yp_buf key;
	yp_buf datum;
};
typedef struct ypupdate_args ypupdate_args;
bool_t xdr_ypupdate_args();

struct ypdelete_args {
	char *mapname;
	yp_buf key;
};
typedef struct ypdelete_args ypdelete_args;
bool_t xdr_ypdelete_args();

#ifdef	__cplusplus
}
#endif

#endif	/* _RPCSVC_YPUPD_H */
