/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BOOTSVCS_H
#define	_SYS_BOOTSVCS_H

#pragma ident	"@(#)bootsvcs.h	1.17	99/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Boot time configuration information objects
 */

/*
 * Declarations for boot service routines
 */
#ifndef _KERNEL
extern void	printf(), putchar(), *memcpy(), *memset();
extern int	strlen();
extern char	*strcpy(), *strcat(), *strncpy();
extern int	getchar();
extern int	goany();
extern int	gets(), memcmp(), ischar();
extern int 	open(), read(), close(), fstat();
extern off_t	lseek();
#ifndef KADB	/* kadb has its own malloc with different args */
extern char 	*malloc();
#endif
extern char	*get_fonts();
extern unsigned int vlimit();
#endif

#if defined(__ia64)

struct boot_syscalls {
	uchar_t	(*getchar)();
	void	(*putchar)();
	int	(*ischar)();
};

#else

struct boot_syscalls {				/* offset */
	void	(*unsup_0)(char *, ...);	/*  0 - printf */
	char	*(*unsup_1)();			/*  1 - strcpy */
	char	*(*unsup_2)();			/*  2 - strncpy */
	char	*(*unsup_3)();			/*  3 - strcat */
	size_t	(*unsup_4)();			/*  4 - strlen */
	void	*(*unsup_5)();			/*  5 - memcpy */
	int	(*unsup_6)();			/*  6 - memcmp */
	int	(*getchar)();			/*  7 - getchar */
	void	(*putchar)();			/*  8 - putchar */
	int	(*ischar)();			/*  9 - ischar */
	int	(*unsup_10)();			/* 10 - goany */
	int	(*unsup_11)();			/* 11 - gets */
	void	*(*unsup_12)();			/* 12 - memset */
	int	(*unsup_13)();			/* 13 - open */
	ssize_t	(*unsup_14)();			/* 14 - read */
	off_t	(*unsup_15)();			/* 15 - lseek */
	int	(*unsup_16)();			/* 16 - close */
	int	(*unsup_17)();			/* 17 - fstat */
	char	*(*malloc)();			/* 18 - malloc */
	char	*(*unsup_19)();			/* 19 - get_fonts */
	unsigned int  (*vlimit)();		/* 20 - vlimit */
};

#endif

extern struct	boot_syscalls *sysp;

/*
 * Boot system syscall functions
 */

#define	getchar sysp->getchar
#define	putchar sysp->putchar
#define	ischar sysp->ischar

#if !defined(__ia64)
#define	printf sysp->printf
#define	get_fonts sysp->get_fonts
#ifndef _KERNEL
#define	strcpy sysp->strcpy
#define	strncpy sysp->strncpy
#define	strcat sysp->strcat
#define	strlen sysp->strlen
#define	memcpy sysp->memcpy
#define	memcmp sysp->memcmp
#define	goany sysp->goany
#define	gets sysp->gets
#define	memset sysp->memset
#define	open sysp->open
#define	read sysp->read
#define	lseek sysp->lseek
#define	close sysp->close
#define	fstat sysp->fstat
#define	malloc sysp->malloc
#define	vlimit sysp->vlimit
#endif	/* !_KERNEL  */
#endif	/* !__ia64 */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_BOOTSVCS_H */
