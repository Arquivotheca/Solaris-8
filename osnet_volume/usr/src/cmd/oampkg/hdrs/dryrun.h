/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dryrun.h	1.1	96/04/05 SMI"

#ifndef __DRYRUN_H__
#define	__DRYRUN_H__

/* The various types of status entry in the info file. */
#define	PARTIAL	1
#define	RUNLEVEL 2
#define	PKGFILES 3
#define	DEPEND 4
#define	SPACE 5
#define	CONFLICT 6
#define	SETUID 7
#define	PRIV 8
#define	PKGDIRS 9
#define	REQUESTEXITCODE 10
#define	CHECKEXITCODE 11
#define	EXITCODE 12
#define	DR_TYPE 13

#define	INSTALL_TYPE	1
#define	REMOVE_TYPE	0

#if defined(__STDC__)
#define	__P(protos) protos
#else	/* __STDC__ */
#define	__P(protos) ()
#endif	/* __STDC__ */

extern void	set_dryrun_mode __P((void));
extern int	in_dryrun_mode __P((void));
extern void	set_continue_mode __P((void));
extern int	in_continue_mode __P((void));
extern void	init_contfile __P((char *cn_dir));
extern void	init_dryrunfile __P((char *dr_dir));
extern int	read_continuation __P((void));
extern void	set_dr_info __P((int type, int value));
extern int	cmd_ln_respfile __P((void));
extern int	is_a_respfile __P((void));
extern void	write_dryrun_file __P((struct cfextra **extlist));

#endif	/* __DRYRUN_H__ */
