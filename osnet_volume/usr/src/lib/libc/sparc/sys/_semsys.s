/*	Copyright (c) 1995 by Sun Microsystems, Inc.		*/

.ident	"@(#)_semsys.s	1.2	96/03/19 SMI"

/* C library -- semsys					*/
/* int _semsys(int opcode, int a1, int a2, int a3);	*/
/* _semsys is the system call entry point for semctl, semop, and semget */

	.file	"_semsys.s"

#include "SYS.h"
	ENTRY(_semsys)
	SYSTRAP(semsys)
	SYSCERROR
	RET

	SET_SIZE(_semsys)
