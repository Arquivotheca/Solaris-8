/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SHARETAB_H
#define	_SHARETAB_H

#pragma ident	"@(#)sharetab.h	1.17	99/07/18 SMI"	/* SVr4.0 1.2	*/

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
 *     Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *  (c) 1986,1987,1988,1989,1996,1999  Sun Microsystems, Inc
 *  (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *            All rights reserved.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

struct share {
	char *sh_path;
	char *sh_res;
	char *sh_fstype;
	char *sh_opts;
	char *sh_descr;
};

struct sh_list {		/* cached share list */
	struct sh_list *shl_next;
	struct share   *shl_sh;
};

#define	SHARETAB	"/etc/dfs/sharetab"
#define	MAXBUFSIZE	4096

#define	SHOPT_RO	"ro"
#define	SHOPT_RW	"rw"

#define	SHOPT_SEC	"sec"
#define	SHOPT_SECURE	"secure"
#define	SHOPT_ROOT	"root"
#define	SHOPT_ANON	"anon"
#define	SHOPT_WINDOW	"window"
#define	SHOPT_NOSUB	"nosub"
#define	SHOPT_NOSUID	"nosuid"
#define	SHOPT_ACLOK	"aclok"
#define	SHOPT_PUBLIC	"public"
#define	SHOPT_INDEX	"index"
#define	SHOPT_LOG	"log"

int		getshare(FILE *, struct share **);
int		putshare(FILE *, struct share *);
int		remshare(FILE *, char *, int *);
char 		*getshareopt(char *, char *);
struct share	*sharedup(struct share *);
void		sharefree(struct share *);

#ifdef __cplusplus
}
#endif

#endif /* !_SHARETAB_H */
