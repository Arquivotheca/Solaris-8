/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_synonyms.h 1.2	96/05/22 SMI"

/*
 * Some synonyms definitions - ld.so.1 exports sufficient functions from its
 * libc contents for liblddbg to bind.  The intention is insure that liblddbg
 * doesn't require a dependency on libc itself, and thus debugging with the
 * runtime linker is as optimal as possible.
 */
#define	open	_open
#define	write	_write
