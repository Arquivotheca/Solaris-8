!	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
!	  All Rights Reserved

!	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
!	The copyright notice above does not evidence any
!	actual or intended publication of such source code.

!		PROPRIETARY NOTICE (Combined)
!
!This source code is unpublished proprietary information
!constituting, or derived under license from AT&T's UNIX(r) System V.
!In addition, portions of such source code were derived from Berkeley
!4.3 BSD under license from the Regents of the University of
!California.
!
!
!
!		Copyright Notice 
!
!Notice of copyright on this source code product does not indicate 
!publication.
!
!	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
!	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
!	          All rights reserved.

.ident	"@(#)_times.s	1.2	92/07/14 SMI"

! C library -- times
! Taken from svr4 port on rainman, libc/sparc/sys/times.s
! 

	.file	"_times.s"

#include <sys/syscall.h>
#include <sys/asm_linkage.h>

#include "SYS.h"

        SYSCALL(times)
        RET

	SET_SIZE(times)

