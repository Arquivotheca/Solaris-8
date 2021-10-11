/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_synonyms.h	1.5	98/08/28 SMI"

/*
 * Some synonyms definitions - the intent here is to insure we get the base
 * libc functionality without any thread interposition switch code.
 */
#if	!defined(__lint)
#define	close		_close
#define	fstat		_fstat
#define	ftruncate	_ftruncate
#define	getdents	_getdents
#define	getegid		_getegid
#define	geteuid		_geteuid
#define	getgid		_getgid
#define	getpid		_getpid
#define	getugid		_getugid
#define	getuid		_getuid
#define	ioctl		_ioctl
#define	kill		_libc_kill
#define	memcpy		_memcpy
#define	memset		_memset
#define	mmap		_mmap
#define	mprotect	_mprotect
#define	munmap		_munmap
#define	open		_open
#define	profil		_profil
#define	resolvepath	_resolvepath
#define	sigprocmask	_libc_sigprocmask
#define	stat		_stat
#define	strerror	_strerror
#define	sysinfo		_sysinfo
#define	umask		_umask
#define	write		_write

#endif /* !defined(__lint) */
