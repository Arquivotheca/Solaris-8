!	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
!	  All Rights Reserved

!	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
!	The copyright notice above does not evidence any
!	actual or intended publication of such source code.

! Portions Copyright(c) 1988, Sun Microsystems Inc.
! All Rights Reserved

! C library -- syscall
! From SunOS 4.1 libc/sys/common/sparc/syscall.s

!  Interpret a given system call

	.file	"syscall.s"

#include "SYS.h"

#define SYS_syscall 0		/* SYS_indir */

	SYSCALL(syscall)
	RET

	SET_SIZE(syscall)
