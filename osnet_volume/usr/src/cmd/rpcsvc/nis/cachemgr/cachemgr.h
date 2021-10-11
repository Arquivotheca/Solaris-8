/*
 *	Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */


#ifndef	__CACHEMGR_H
#define	__CACHEMGR_H

#pragma ident	"@(#)cachemgr.h	1.10	96/05/22 SMI"

extern int xdr_fd_result(XDR *, fd_result *);
extern int xdr_directory_obj(XDR *, directory_obj *);
extern int xdr_nis_error(XDR *, int *);
extern int xdr_nis_server(XDR *, nis_server *);

#endif	/* __CACHEMGR_H */
