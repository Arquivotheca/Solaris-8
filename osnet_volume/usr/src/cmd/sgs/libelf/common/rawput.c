/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)rawput.c	1.10	98/08/28 SMI" 	/* SVr4.0 1.2	*/

#include "syn.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libelf.h"
#include "decl.h"
#include "msg.h"


/*
 * Raw file input/output
 * Read pieces of input files.
 */


char *
_elf_read(int fd, off_t off, size_t fsz)
{
	char		*p;

	if (fsz == 0)
		return (0);

	if (fd == -1) {
		_elf_seterr(EREQ_NOFD, 0);
		return (0);
	}

	if (lseek(fd, off, 0) != off) {
		_elf_seterr(EIO_SEEK, errno);
		return (0);
	}
	if ((p = (char *)malloc(fsz)) == 0) {
		_elf_seterr(EMEM_DATA, errno);
		return (0);
	}

	if (read(fd, p, fsz) != fsz) {
		_elf_seterr(EIO_READ, errno);
		free(p);
		return (0);
	}
	return (p);
}
