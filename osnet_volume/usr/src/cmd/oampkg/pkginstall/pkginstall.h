/*	Copyright (c) 1994 SMI	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)pkginstall.h	1.4	96/04/05 SMI"

#ifndef __PKG_PKGINSTALL_H__
#define	__PKG_PKGINSTALL_H__

/* cppath() variables */
#define	DISPLAY		0x0001
#define	KEEPMODE	0x0002	/* mutex w/ SETMODE (setmode preempts) */
#define	SETMODE		0x0004	/* mutex w/ KEEPMODE */

/* special stdin for request scripts */
#define	REQ_STDIN	"/dev/tty"

/* response file writability status */
#define	RESP_WR		0	/* Response file is writable. */
#define	RESP_RO		1	/* Read only. */

#if defined(__STDC__)
#define	__P(protos) protos
#else	/* __STDC__ */
#define	__P(protos) ()
#endif	/* __STDC__ */

extern int	cppath __P((int ctrl, char *f1, char *f2, mode_t mode));
extern void	backup __P((char *path, int mode));
extern void	pkgvolume __P((struct pkgdev *devp, char *pkg, int part,
		    int nparts));
extern void	trap __P((int signo));
extern void	quit __P((int exitval));
extern void	ckreturn __P((int retcode, char *msg));
extern int	sortmap __P((struct cfextra ***extlist, FILE *pkgmapfp,
			FILE *mapfp, FILE *tmpfp));
extern void	merginfo __P((struct cl_attr **pclass));
extern void	set_infoloc __P((char *real_pkgsav));
extern int	pkgenv __P((char *pkginst, char *p_pkginfo, char *p_pkgmap));
extern void	instvol __P((struct cfextra **extlist, char *srcinst, int part,
			int nparts));
extern int	reqexec __P((char *script));
extern int	chkexec __P((char *script));
extern int	rdonly_respfile __P((void));
extern int	is_a_respfile __P((void));
extern char	*get_respfile __P((void));
extern int	set_respfile __P((char *respfile, char *pkginst,
		    int resp_stat));
extern void	predepend __P((char *oldpkg));
extern int	cksetuid __P((void));
extern int	ckconflct __P((void));
extern int	ckpkgdirs __P((void));
extern int	ckspace __P((void));
extern int	ckdepend __P((void));
extern int	ckrunlevel __P((void));
extern int	ckpartial __P((void));
extern int	ckpkgfiles __P((void));
extern int	ckpriv __P((void));
extern void	is_WOS_arch __P((void));
extern void	ckdirs __P((void));
extern char	*getinst __P((struct pkginfo *info, int npkgs));
extern int	is_samepkg __P((void));
extern int	dockspace __P((char *spacefile));

#endif	/* __PKG_PKGINSTALL_H__ */
