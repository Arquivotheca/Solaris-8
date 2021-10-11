/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)genassym.c	1.4	97/11/12 SMI"

#include <sys/synch.h>
#include <sys/synch32.h>
#include <sys/ucontext.h>

/*
 * This file generates two values used by _lwp_mutex_unlock.s:
 *	a) the byte offset (in lwp_mutex_t) of the word containing the lock byte
 *	b) a mask to extract the waiter field from the word containing it
 * It also generates offsets into the ucontext_t structure, needed by the
 * getcontext() function, which is written in assembler.
 */

main()
{
	register ucontext_t *ucp = (ucontext_t *)0;
	lwp_mutex_t *lm = (lwp_mutex_t *)0;

	printf("_m4_define_(`M_LOCK_WORD', 0x%x)\n",
		(void *)&lm->mutex_lockword);
	printf("_m4_define_(`M_WAITER_MASK', 0x00ff0000)\n");
	printf("_m4_define_(`UC_ALL', 0x%x)\n", UC_ALL);
	printf("_m4_define_(`UC_MCONTEXT', 0x%x)\n", &ucp->uc_mcontext.gregs);
	printf("_m4_define_(`EIP', 0x%x)\n", EIP);
	printf("_m4_define_(`EBX', 0x%x)\n", EBX);
	printf("_m4_define_(`EAX', 0x%x)\n", EAX);
	printf("_m4_define_(`UESP', 0x%x)\n", UESP);
	return (0);
}
