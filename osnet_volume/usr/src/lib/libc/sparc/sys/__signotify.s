/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

.ident	"@(#)__signotify.s	1.4	98/02/24 SMI"

/* unpublished system call for librt -- __signotify		*/
/* int _signotify (int cmd, siginfo_t *siginfo,		*/
/*					signotify_id_t *sn_id);	*/

	.file	"__signotify.s"

#include "SYS.h"

	ENTRY(__signotify)
	SYSTRAP(signotify)
	SYSCERROR
	RET

	SET_SIZE(__signotify)
