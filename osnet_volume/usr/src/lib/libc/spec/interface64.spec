#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)interface64.spec	1.2	99/05/14 SMI"
#
# lib/libc/spec/interface64.spec

function	creat64
include		<sys/types.h>,
include		<sys/stat.h>
include		<fcntl.h>
declaration	int creat64(const char *path, mode_t mode)
arch		i386 sparc
version		SUNW_1.1
end

function	_creat64
weak		creat64
arch		sparc i386
version		SUNW_1.1
end

function	fgetpos64
include		<stdio.h>
declaration	int fgetpos64(FILE *stream, fpos64_t *pos)
arch		sparc i386
version		SUNW_1.1
end

function	fopen64
include		<stdio.h>
declaration	FILE *fopen64(const char *filename, const char *mode)
arch		sparc i386
version		SUNW_1.1
end

function	freopen64
include		<stdio.h>
declaration	FILE *freopen64(const char *filename, const char *mode, \
			FILE *stream)
arch		sparc i386
version		SUNW_1.1
end

function	fseeko64
include		<stdio.h>
declaration	int fseeko64(FILE *stream, off64_t offset, int whence)
arch		sparc i386
version		SUNW_1.1
end

function	fsetpos64
include		<stdio.h>
declaration	int fsetpos64(FILE *stream, const fpos64_t *pos)
arch		sparc i386
version		SUNW_1.1
end

function	fstat64
include		<sys/types.h>
include		<sys/stat.h>
declaration	int fstat64(int fildes, struct stat64 *buf)
arch		sparc i386
version		SUNW_1.1
end

function	_fstat64
arch		sparc	i386
version		SUNW_1.1
end

function	fstatvfs64
include		<sys/types.h>
include		<sys/statvfs.h>
declaration	int fstatvfs64(int fildes, struct statvfs64 *buf)
arch		sparc i386
version		SUNW_1.1
end

function	_fstatvfs64
weak		fstatvfs64
arch		i386 sparc
version		SUNW_1.1
end

function	ftello64
include		<stdio.h>
declaration	off64_t ftello64(FILE *stream)
arch		sparc i386
version		SUNW_1.1
end

function	ftruncate64
include		<unistd.h>
declaration	int ftruncate64(int fildes, off64_t length)
arch		sparc i386
version		SUNW_1.1
end

function	_ftruncate64
weak		ftruncate64
arch		sparc i386
version		SUNW_1.1
end

function	ftw64
include		<ftw.h>
include		"interface64_spec.h"
declaration	int ftw64(const char *path, \
			int (*fn)(const char *, const struct stat64 *, int), \
			int depth)
arch		sparc i386
version		SUNW_1.1
end

function	_ftw64
weak		ftw64
arch		sparc i386
version		SUNW_1.1
end

function	getdents64
include		<sys/dirent.h>
declaration	int getdents64(int fildes, struct dirent64 *buf, size_t nbyte)
arch		sparc i386
version		SUNW_1.1
end

function	_getdents64
weak		getdents64
arch		i386 sparc
version		SUNW_1.1
end

function	getrlimit64
include		<sys/resource.h>
declaration	int getrlimit64(int resource, struct  rlimit64 *rlp)
arch		sparc i386
version		SUNW_1.1
end

function	_getrlimit64
weak		getrlimit64
arch		sparc i386
version		SUNW_1.1
end

function	lockf64
include		<unistd.h>
declaration	int lockf64(int fildes, int function, off64_t size)
arch		sparc i386
version		SUNW_1.1
end

function	_lockf64
weak		lockf64
arch		sparc i386
version		SUNW_1.1
end

function	lseek64
include		<sys/types.h>
include		<unistd.h>
declaration	off64_t lseek64(int fildes, off64_t offset, int whence)
arch		sparc i386
version		SUNW_1.1
end

function	_lseek64
weak		lseek64
arch		sparc i386
version		SUNW_1.1
end

function	lstat64
include		<sys/types.h>
include		<sys/stat.h>
declaration	int lstat64(const char *path, struct  stat64 *buf)
arch		sparc i386
version		SUNW_1.1
end

function	_lstat64
weak		lstat64
arch		sparc i386
version		SUNW_1.1
end

function	mkstemp64
include		<stdlib.h>
declaration	int mkstemp64(char *template)
arch		sparc i386
version		SUNW_1.1
end

function	_mkstemp64
weak		mkstemp64
arch		sparc i386
version		SUNW_1.1
end

function	mmap64
include		<sys/types.h>
include		<sys/mman.h>
declaration	caddr_t mmap64(caddr_t addr, size_t len, int prot, int flags, \
			int fildes, off64_t off)
arch		sparc i386
version		SUNW_1.1
end

function	_mmap64
weak		mmap64
arch		sparc i386
version		SUNW_1.1
end

function	nftw64
include		<ftw.h>
declaration	int nftw64(const char *path, \
			int (*fn)(const char *, const struct stat64 *, \
				int, struct FTW *), \
			int depth, int flags)
arch		sparc i386
version		SUNW_1.1
end

function	_nftw64
weak		nftw64
arch		i386 sparc
version		SUNW_1.1
end

function	open64
include		<sys/types.h>
include		<sys/stat.h>
include		<fcntl.h>
declaration	int open64(const char *path, int oflag, ...)
arch		sparc i386
version		SUNW_1.1
end

function	_open64
weak		open64
arch		i386 sparc
version		SUNW_1.1
end

function	pread64
include		<unistd.h>
declaration	ssize_t pread64(int fildes, void *buf, size_t nbyte, \
			off64_t offset)
arch		sparc i386
version		SUNW_1.1
end

function	_pread64
weak		pread64
arch		i386 sparc
version		SUNW_1.1
end

function	pwrite64
include		<unistd.h>
declaration	ssize_t pwrite64(int fildes, const void *buf, size_t nbyte, \
			off64_t offset)
arch		sparc i386
version		SUNW_1.1
end

function	_pwrite64
weak		pwrite64
arch		sparc i386
version		SUNW_1.1
end

function	readdir64
include		<sys/types.h>
include		<dirent.h>
declaration	struct dirent64 *readdir64(DIR *dirp)
arch		sparc i386
version		SUNW_1.1
end

function	_readdir64
weak		readdir64
arch		sparc i386
version		SUNW_1.1
end

function	readdir64_r
include		<sys/types.h>
include		<dirent.h>
declaration	struct dirent64 *readdir64_r(DIR *dirp, struct dirent64 *entry)
arch		sparc i386
version		SUNW_1.1
end

function	_readdir64_r
weak		readdir64_r
arch		sparc	i386
version		SUNW_1.1
end

function	setrlimit64
include		<sys/resource.h>
declaration	int setrlimit64(int resource, const struct rlimit64 *rlp)
arch		sparc i386
version		SUNW_1.1
end

function	_setrlimit64
weak		setrlimit64
arch		sparc i386
version		SUNW_1.1
end

function	stat64
include		<sys/types.h>
include		<sys/stat.h>
declaration	int stat64(const char *path,  struct stat64 *buf)
arch		sparc i386
version		SUNW_1.1
end

function	_stat64
weak		stat64
arch		i386 sparc
version		SUNW_1.1
end

function	statvfs64
include		<sys/types.h>
include		<sys/statvfs.h>
declaration	int statvfs64(const char *path, struct statvfs64 *buf)
arch		sparc i386
version		SUNW_1.1
end

function	_statvfs64 
weak		statvfs64
arch		sparc i386
version		SUNW_1.1
end

function	tmpfile64
include		<stdio.h>
declaration	FILE *tmpfile64(void)
arch		sparc i386
version		SUNW_1.1
end

function	truncate64
include		<unistd.h>
declaration	int truncate64(const  char *path, off64_t length)
arch		sparc i386
version		SUNW_1.1
end

function	_truncate64
weak		truncate64
arch		sparc i386
version		SUNW_1.1
end

function	tell64
include		<unistd.h>
declaration	off64_t tell64(int)
arch		sparc i386
version		SUNW_1.1 
end

function	_tell64
weak		tell64
arch		sparc i386
version		SUNW_1.1
end
