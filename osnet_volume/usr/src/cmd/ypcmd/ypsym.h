/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	__YPSYM_H
#define	__YPSYM_H

#pragma ident	"@(#)ypsym.h	1.12	97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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
*  publication.
*
*	(c) 1986,1987,1988,1989,1990  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
*          All rights reserved.
*/ 

/*
 * This contains symbol and structure definitions for modules in the YP server 
 */

#include <ndbm.h>			/* Pull this in first */
#define DATUM
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <rpcsvc/yp_prot.h>
#include "ypv1_prot.h"
#include <rpcsvc/ypclnt.h>

typedef void (*PFV)();
typedef int (*PFI)();
typedef unsigned int (*PFU)();
typedef long int (*PFLI)();
typedef unsigned long int (*PFULI)();
typedef short int (*PFSI)();
typedef unsigned short int (*PFUSI)();

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef NULL
#undef NULL
#endif
#define NULL 0

/* Maximum length of a yp map name in the system v filesystem */
#define MAXALIASLEN 8

#define YPINTERTRY_TIME 10		/* Secs between tries for peer bind */
#define YPTOTAL_TIME 30			/* Total secs until timeout */
#define YPNOPORT ((unsigned short) 0)	/* Out-of-range port value */

/* External refs to yp server data structures */

extern bool ypinitialization_done;
extern struct timeval ypintertry;
extern struct timeval yptimeout;
extern char myhostname[];
extern bool silent;
#ifdef MINUS_C_OPTION
extern bool multiflag;
#endif

/* External ref to logging func */
extern void logprintf(char *format, ...);

/* External refs for /var/yp/securenets support */
extern void get_secure_nets(char *daemon_name);

/* External refs to yp server-only functions */
extern bool ypcheck_map_existence(char *pname);
extern bool ypget_map_master(char **owner, DBM *fdb);
extern DBM *ypset_current_map(char *map, char *domain, uint_t *error);
extern void ypclr_current_map(void);
extern void ypmkfilename();
extern int yplist_maps();
extern bool yp_map_access(SVCXPRT *transp, uint_t *error, DBM *fdb);
extern bool ypget_map_order(char *map, char *domain, uint_t *order);

extern bool ypcheck_domain();
extern datum dbm_do_nextkey();
extern void ypclr_current_map(void);

extern void ypdomain(SVCXPRT *transp, bool always_respond);
extern void ypmatch(SVCXPRT *transp, struct svc_req *rqstp);
extern void ypfirst(SVCXPRT *transp);
extern void ypnext(SVCXPRT *transp);
extern void ypxfr(SVCXPRT *transp, int prog);
extern void ypall(SVCXPRT *transp);
extern void ypmaster(SVCXPRT *transp);
extern void yporder(SVCXPRT *transp);
extern void ypmaplist(SVCXPRT *transp);
extern void ypoldmatch(SVCXPRT *transp, struct svc_req *rqstp);
extern void ypoldfirst(SVCXPRT *transp);
extern void ypoldnext(SVCXPRT *transp);
extern void ypoldpoll(SVCXPRT *transp);
extern void ypoldpush(SVCXPRT *transp);
extern void ypoldpull(SVCXPRT *transp);
extern void ypoldget(SVCXPRT *transp);
extern int yp_matchdns(DBM *, struct ypreq_key *, struct ypresp_val *);
extern int yp_oldmatchdns(DBM *fdb,
			  struct yprequest *req, struct ypresponse *resp);

extern bool _xdr_ypreqeust(XDR *xdrs, struct yprequest *ps);
extern bool _xdr_ypresponse(XDR *xdrs, struct ypresponse *ps);

extern void setup_resolv(bool *fwding, int *child, CLIENT **client,
			 char *tp_type, long prognum);
extern int resolv_req(bool *fwding, CLIENT **client, int *pid,
		      char *tp, SVCXPRT *xprt, struct ypreq_key *req,
		      char *map);


/* definitions for reading files of lists */

struct listofnames
{
	struct listofnames *nextname;
	char *name;
};
typedef struct listofnames listofnames;

/* XXX- NAME_MAX can't be defined in <limits.h> in a POSIX conformant system
 *	(under conditions which apply to Sun systems). Removal of this define
 *	caused yp to break (and only yp!). Hence, NAME_MAX is defined here
 *	*exactly* as it was in <limits.h>. I suspect this may not be the
 *	desired value. I suspect the desired value is either:
 *		- the maxumum name length for any file system type, or
 *		- should be _POSIX_NAME_MAX which is the minimum-maximum name
 *		  length in a POSIX conformant system (which just happens to
 *		  be 14), or
 *		- should be gotten by pathconf() or fpathconf().
 * XXX- I leave this to the owners of yp!
 */
#define	NAME_MAX	14	/* s5 file system maximum name length */

#ifdef	__cplusplus
}
#endif

#endif	/* __YPSYM_H */
