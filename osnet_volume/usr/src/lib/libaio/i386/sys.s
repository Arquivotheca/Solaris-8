#pragma ident	"@(#)sys.s	1.4	98/09/14 SMI"
/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 */
	.file "sys.s"

#include <sys/syscall.h>

#define SYSCALL_TRAPNUM	0x07

/*
 * __libaio_read(int fd, char *buf, int bufsz, int *err)
 */
	.text
	.globl	__libaio_read
__libaio_read:
	movl	$SYS_read, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	jae	noerror_read
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	movl	$-1, %eax
noerror_read:
	ret
/*
 * __libaio_write(int fd, char *buf, int bufsz, int *err)
 */
	.text
	.globl	__libaio_write
__libaio_write:
	movl	$SYS_write, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	jae	noerror_write
	movl	16(%esp), %edx
	movl	%eax, (%edx)
	movl	$-1, %eax
noerror_write:
	ret
/*
 * __libaio_pread(int fd, char *buf, int bufsz, offset_t off, int *err)
 */
	.text
	.globl	__libaio_pread
__libaio_pread:
	movl	$SYS_pread64, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	jae	noerror_pread
	movl	24(%esp), %edx
	movl	%eax, (%edx)
	movl	$-1, %eax
noerror_pread:
	ret
/*
 * __libaio_pwrite(int fd, char *buf, int bufsz, offset_t off, int *err)
 */
	.text
	.globl	__libaio_pwrite
__libaio_pwrite:
	movl	$SYS_pwrite64, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	jae	noerror_pwrite
	movl	24(%esp), %edx
	movl	%eax, (%edx)
	movl	$-1, %eax
noerror_pwrite:
	ret
/*
 * __libaio_fdsync(int fd, int flag, int *err)
 */
	.text
	.globl	__libaio_fdsync
__libaio_fdsync:
	movl	$SYS_fdsync, %eax
	lcall	$SYSCALL_TRAPNUM, $0
	jae	noerror_fdsync
	movl	12(%esp), %edx
	movl	%eax, (%edx)
	movl	$-1, %eax
noerror_fdsync:
	ret
