/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)shmsys.s	1.1	97/04/06 SMI"

/*
 * void *shmat(int shmid, const void *shmaddr, int shmflg);
 * int shmctl(int shmid, int cmd, struct shmid_ds *buf);
 * int shmdt(const void *shmaddr);
 * int shmget(key_t key, size_t size, int shmflg);
 */
	.file	"shmsys.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(shmat,function)
	ANSI_PRAGMA_WEAK(shmctl,function)
	ANSI_PRAGMA_WEAK(shmdt,function)
	ANSI_PRAGMA_WEAK(shmget,function)

#include "SYS.h"

#define	SUBSYS_shmat		0
#define	SUBSYS_shmctl		1
#define	SUBSYS_shmdt		2
#define	SUBSYS_shmget		3

	ENTRY(shmat)
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_shmat, %o0

	ENTRY(shmctl)
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_shmctl, %o0

	ENTRY(shmdt)
	mov	%o0, %o1
	b	.sys
	mov	SUBSYS_shmdt, %o0

	
	ENTRY(shmget)
	mov	%o2, %o3
	mov	%o1, %o2
	mov	%o0, %o1
	mov	SUBSYS_shmget, %o0

.sys:
	SYSTRAP(shmsys)
	SYSCERROR
	RET

	SET_SIZE(shmget)
	SET_SIZE(shmdt)
	SET_SIZE(shmctl)
	SET_SIZE(shmat)
