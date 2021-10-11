#pragma ident	"@(#)sys.s	1.3	98/09/24 SMI"
/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 */

	.file "sys.s"

#include <sys/asm_linkage.h>
#include "SYS.h"

/*
 * __libaio_read(int fd, char *buf, int bufsz, int *err)
 */
	ENTRY(__libaio_read)
	SYSTRAP(read)
	bcc	1f
	nop
	st	%o0, [%o3]
	mov	-1, %o0
1:
	RET
	SET_SIZE(__libaio_read)
/*
 * __libaio_write(int fd, char *buf, int bufsz, int *err)
 */
	ENTRY(__libaio_write)
	SYSTRAP(write)
	bcc	1f
	nop
	st	%o0, [%o3]
	mov	-1, %o0
1:
	RET
	SET_SIZE(__libaio_write)
/*
 * __libaio_pread(int fd, char *buf, int bufsz, offset_t off, int *err)
 */
	ENTRY(__libaio_pread)
	SYSTRAP(pread64)
	bcc	1f
	nop
	st	%o0, [%o5]
	mov	-1, %o0
1:
	RET
	SET_SIZE(__libaio_pread)
/*
 * __libaio_pwrite(int fd, char *buf, int bufsz, offset_t off, int *err)
 */
	ENTRY(__libaio_pwrite)
	SYSTRAP(pwrite64)
	bcc	1f
	nop
	st	%o0, [%o5]
	mov	-1, %o0
1:
	RET
	SET_SIZE(__libaio_pwrite)
/*
 * __libaio_fdsync(int fd, int flag, int *err)
 */
	ENTRY(__libaio_fdsync)
	SYSTRAP(fdsync)
	bcc	1f
	nop
	st	%o0, [%o2]
	mov	-1, %o0
1:
	RET
	SET_SIZE(__libaio_fdsync)
