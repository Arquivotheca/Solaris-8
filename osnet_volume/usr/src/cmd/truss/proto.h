/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_PROTO_H
#define	_PROTO_H

#pragma ident	"@(#)proto.h	1.22	99/05/04 SMI"

#include <sys/procset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Function prototypes for most external functions.
 */

extern	void	make_pname(struct ps_prochandle *, id_t);
extern	int	requested(struct ps_prochandle *, int);
extern	int	jobcontrol(struct ps_prochandle *);
extern	int	signalled(struct ps_prochandle *, int);
extern	int	faulted(struct ps_prochandle *);
extern	int	sysentry(struct ps_prochandle *);
extern	int	sysexit(struct ps_prochandle *);
extern	void	showbuffer(struct ps_prochandle *, long, long);
extern	void	showbytes(const char *, int, char *);
extern	void	accumulate(timestruc_t *, timestruc_t *, timestruc_t *);

extern	const char *ioctlname(uint_t);
extern	const char *fcntlname(int);
extern	const char *sfsname(int);
extern	const char *plockname(int);
extern	const char *si86name(int);
extern	const char *utscode(int);
extern	const char *sigarg(int);
extern	const char *openarg(int);
extern	const char *whencearg(int);
extern	const char *msgflags(int);
extern	const char *semflags(int);
extern	const char *shmflags(int);
extern	const char *msgcmd(int);
extern	const char *semcmd(int);
extern	const char *shmcmd(int);
extern	const char *strrdopt(int);
extern	const char *strevents(int);
extern	const char *tiocflush(int);
extern	const char *strflush(int);
extern	const char *mountflags(int);
extern	const char *svfsflags(ulong_t);
extern	const char *sconfname(int);
extern	const char *pathconfname(int);
extern	const char *fuiname(int);
extern	const char *fuflags(int);

extern	void	expound(struct ps_prochandle *, long, int);
extern	void	prtimestruc(const char *, timestruc_t *);
extern	void	print_siginfo(const siginfo_t *);

extern	void	Flush(void);
extern	void	Eserialize(void);
extern	void	Xserialize(void);
extern	void	procadd(pid_t);
extern	void	procdel(void);
extern	int	checkproc(struct ps_prochandle *, char *);

extern	int	syslist(char *, sysset_t *, int *);
extern	int	siglist(char *, sigset_t *, int *);
extern	int	fltlist(char *, fltset_t *, int *);
extern	int	fdlist(char *, fileset_t *);
extern	int	liblist(char *, int);

extern	char 	*fetchstring(long, int);
extern	void	show_cred(struct ps_prochandle *, int);
extern	void	errmsg(const char *, const char *);
extern	void	abend(const char *, const char *);

extern	void	outstring(const char *);

extern	void	show_procset(struct ps_prochandle *, long);
extern	const char *idtype_enum(long);
extern	const char *woptions(int);

extern	void	putpname(void);
extern	void	timestamp(const pstatus_t *);

extern	const char *errname(int);
extern	const char *sysname(int, int);
extern	const char *rawsigname(int);
extern	const char *signame(int);
extern	const char *rawfltname(int);
extern	const char *fltname(int);

extern	void	show_stat(struct ps_prochandle *, long);
extern	void	show_xstat(struct ps_prochandle *, int, long);
extern	void	show_stat64_32(struct ps_prochandle *, long);

extern	void	establish_breakpoints(struct ps_prochandle *);
extern	void	establish_stacks(struct ps_prochandle *);
extern	void	reset_breakpoints(struct ps_prochandle *);
extern	void	clear_breakpoints(struct ps_prochandle *);
extern	int	function_trace(struct ps_prochandle *, int, int);
extern	void	reestablish_traps(struct ps_prochandle *);
extern	void	report_htable_stats(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PROTO_H */
