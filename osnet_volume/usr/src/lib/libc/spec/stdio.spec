#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)stdio.spec	1.3	99/05/14 SMI"
#
# lib/libc/spec/stdio.spec

function	clearerr
include		<stdio.h>, "stdio_spec.h"
declaration	void clearerr(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	ctermid
include		<stdio.h>
declaration	char *ctermid(char *s)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	is_empty_string($return)
end

function	ctermid_r
include		<stdio.h>
declaration	char *ctermid_r(char *s)
version		SUNW_0.7
exception	is_empty_string($return)
end

function	cuserid
include		<stdio.h>
declaration	char *cuserid(char *s)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_cuserid # extends libc/spec/stdio.spec cuserid
weak		cuserid
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fclose
include		<stdio.h>
declaration	int fclose(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EFBIG EINTR EIO ENOSPC EPIPE ENXIO
end

function	fdopen
include		<stdio.h>
declaration	FILE *fdopen(int fildes, const char *mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EBADF EINVAL EMFILE ENOMEM
end

function	_fdopen
weak		fdopen
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	feof
include		<stdio.h>
declaration	int feof(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	ferror
include		<stdio.h>
declaration	int ferror(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fflush
include		<stdio.h>
declaration	int fflush(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EFBIG EINTR EIO ENOSPC EPIPE ENXIO
end

function	fgetc
include		<stdio.h>
declaration	int fgetc(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	fgetpos
include		<stdio.h>
declaration	int fgetpos(FILE *stream, fpos_t *pos)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EBADF ESPIPE EOVERFLOW
exception	$return == -1
end

function	fgets
include		<stdio.h>
declaration	char *fgets(char *s, int n, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	fileno
include		<stdio.h>
declaration	int fileno(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_fileno
weak		fileno
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	flockfile
include		<stdio.h>
declaration	void flockfile(FILE *stream)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fopen
include		<stdio.h>
declaration	FILE *fopen(const char *filename, const char *mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EACCES EINTR EISDIR ELOOP EMFILE ENAMETOOLONG ENFILE ENOENT \
			ENOSPC ENOTDIR ENXIO EOVERFLOW EROFS EINVAL ENOMEM \
			ETXTBSY
end

function	fputc
include		<stdio.h>
declaration	int fputc(int c, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG
end

function	fputs
include		<stdio.h>
declaration	int fputs(const char *s, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG
end

function	fread
include		<stdio.h>, <errno.h>
declaration	size_t fread(void *ptr, size_t size, size_t nitems, \
			FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW EFBIG
exception	$return == 0 && (errno == EOVERFLOW || errno == EFBIG)
end

function	freopen
include		<stdio.h>
declaration	FILE *freopen(const char *filename, const char *mode, \
			FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EACCES EINTR EISDIR ELOOP EMFILE ENAMETOOLONG ENFILE \
			ENOENT ENOSPC ENOTDIR ENXIO EOVERFLOW EROFS \
			EINVAL ENOMEM ETXTBSY
end

function	fscanf
include		<stdio.h>
declaration	int fscanf(FILE *strm, const char *format, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW
exception	$return == -1
end

function	fwscanf
include		<stdio.h>, <wchar.h>
declaration	int fwscanf(FILE *stream, const wchar_t *format, ...)
version		SUNW_1.18
end

function	fseek
include		<stdio.h>
declaration	int fseek(FILE *stream, long offset, int whence)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EAGAIN EBADF EFBIG EINTR EINVAL EIO ENOSPC EPIPE ENXIO EOVERFLOW
end

function	fseeko
include		<stdio.h>
declaration	int fseeko(FILE *stream, off_t offset, int whence)
version		SUNW_1.1
errno		EAGAIN EBADF EFBIG EINTR EINVAL EIO ENOSPC EPIPE ENXIO EOVERFLOW
end

function	fsetpos
include		<stdio.h>
declaration	int fsetpos(FILE *stream, const fpos_t *pos)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EBADF ESPIPE
exception	$return == -1
end

function	ftell
include		<stdio.h>
declaration	long ftell(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EBADF ESPIPE EOVERFLOW
end

function	ftello
include		<stdio.h>
declaration	off_t ftello(FILE *stream)
version		SUNW_1.1
errno		EBADF ESPIPE EOVERFLOW
end

function	funlockfile
include		<stdio.h>
declaration	void funlockfile(FILE *stream)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	fwrite
include		<stdio.h>
declaration	size_t fwrite(const void *ptr, size_t size, size_t nitems, \
			FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW EFBIG
exception	$return == 0
end

function	getc
include		<stdio.h>
declaration	int getc(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	getc_unlocked
include		<stdio.h>
declaration	int getc_unlocked(FILE *stream)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	getchar
include		<stdio.h>
declaration	int getchar(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	getchar_unlocked
include		<stdio.h>
declaration	int getchar_unlocked(void)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	getpass
include		<unistd.h>
declaration	char *getpass(const char *prompt)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EINTR EIO EMFILE ENFILE ENXIO
end

function	_getpass
weak		getpass
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	getpassphrase
include		<unistd.h>
declaration	char *getpassphrase(const char *prompt)
version		SUNW_1.1
errno		EINTR EIO EMFILE ENFILE ENXIO
end

function	gets
include		<stdio.h>
declaration	char *gets(char *s)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	getw
include		<stdio.h>
declaration	int getw(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EOVERFLOW
end

function	_getw
weak		getw
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	pclose
include		<stdio.h>
declaration	int pclose(FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == -1
end

function	_pclose
weak		pclose
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	popen
include		<stdio.h>
declaration	FILE *popen(const char *command, const char *mode)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	_popen
weak		popen
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	putc
include		<stdio.h>
declaration	int putc(int c, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG
end

function	putc_unlocked
include		<stdio.h>
declaration	int putc_unlocked(int c, FILE *stream)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG
end

function	putchar
include		<stdio.h>
declaration	int putchar(int c)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG
end

function	putchar_unlocked
include		<stdio.h>
declaration	int putchar_unlocked(int c)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG
end

function	puts
include		<stdio.h>
declaration	int puts(const char *s)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG
end

function	putw
include		<stdio.h>
declaration	int putw(int w, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG
end

function	_putw
weak		putw
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	scanf
include		<stdio.h>
declaration	int scanf(const char *format, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
exception	$return == -1
end

function	setbuf
include		<stdio.h>
declaration	void setbuf(FILE *stream, char *buf)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	setbuffer
include		<stdio.h>
declaration	void setbuffer(FILE *iop, char *abuf, size_t asize)
version		SUNW_0.9
end

function	setlinebuf
include		<stdio.h>
declaration	int setlinebuf(FILE *iop)
version		SUNW_0.9
end

function	setvbuf
include		<stdio.h>
declaration	int setvbuf(FILE *stream, char *buf, int type, size_t size)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	sscanf
include		<stdio.h>
declaration	int sscanf(const char *s, const char *format, ...)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EOVERFLOW
exception	$return == -1
end

function	system
declaration	int system(const char *string )
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
exception	$return == 0
end

function	tempnam
include		<stdio.h>
declaration	char *tempnam(const char *dir, const char *pfx)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	_tempnam
weak		tempnam
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	tmpfile
include		<stdio.h>
declaration	FILE *tmpfile(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EINTR EMFILE ENFILE ENOSPC ENOMEM
end

function	tmpnam
include		<stdio.h>
declaration	char *tmpnam(char *s)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
exception	$return == 0
end

function	tmpnam_r
include		<stdio.h>
declaration	char *tmpnam_r(char *s)
version		SUNW_0.7
exception	$return == 0
end

function	ungetc
include		<stdio.h>
declaration	int ungetc(int c, FILE *stream)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	wscanf
include		<stdio.h>, <wchar.h>
declaration	int wscanf(const wchar_t *format, ...)
version		SUNW_1.18
end
