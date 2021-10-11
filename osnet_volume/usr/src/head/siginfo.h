/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SIGINFO_H
#define	_SIGINFO_H

#pragma ident	"@(#)siginfo.h	1.10	94/08/25 SMI"	/* SVr4.0 1.1	*/

#include <sys/types.h>
#include <sys/siginfo.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct siginfolist {
	int nsiginfo;
	char **vsiginfo;
};

#ifdef __STDC__
extern const char * _sys_illlist[];
extern const char * _sys_fpelist[];
extern const char * _sys_segvlist[];
extern const char * _sys_buslist[];
extern const char * _sys_traplist[];
extern const char * _sys_cldlist[];
extern const struct siginfolist *_sys_siginfolistp;
#define	_sys_siginfolist	_sys_siginfolistp
#else
extern char * _sys_illlist[];
extern char * _sys_fpelist[];
extern char * _sys_segvlist[];
extern char * _sys_buslist[];
extern char * _sys_traplist[];
extern char * _sys_cldlist[];
extern struct siginfolist *_sys_siginfolistp;
#define	_sys_siginfolist	_sys_siginfolistp
#endif

#if defined(__STDC__)

extern void psignal(int, const char *);
extern void psiginfo(siginfo_t *, char *);

#else

extern void psignal();
extern void psiginfo();

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SIGINFO_H */
