/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CMN_ERR_H
#define	_SYS_CMN_ERR_H

#pragma ident	"@(#)cmn_err.h	1.29	99/03/23 SMI"

#if defined(_KERNEL) && !defined(_ASM)
#include <sys/va_list.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

/* Common error handling severity levels */

#define	CE_CONT		0	/* continuation		*/
#define	CE_NOTE		1	/* notice		*/
#define	CE_WARN		2	/* warning		*/
#define	CE_PANIC	3	/* panic		*/
#define	CE_IGNORE	4	/* print nothing	*/

#ifndef _ASM

#ifdef _KERNEL

/*PRINTFLIKE2*/
extern void cmn_err(int, const char *, ...);
extern void vcmn_err(int, const char *, __va_list);
/*PRINTFLIKE1*/
extern void printf(const char *, ...);
extern void vprintf(const char *, __va_list);
/*PRINTFLIKE1*/
extern void uprintf(const char *, ...);
extern void vuprintf(const char *, __va_list);
/*PRINTFLIKE3*/
extern size_t snprintf(char *, size_t, const char *, ...);
extern size_t vsnprintf(char *, size_t, const char *, __va_list);
/*PRINTFLIKE2*/
extern char *sprintf(char *, const char *, ...);
extern char *vsprintf(char *, const char *, __va_list);
/*PRINTFLIKE1*/
extern void panic(const char *, ...);
extern void vpanic(const char *, __va_list);

extern void cnputs(const char *, uint_t, int);
extern void cnputc(int, int);
extern void gets(char *);

#endif /* _KERNEL */
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CMN_ERR_H */
