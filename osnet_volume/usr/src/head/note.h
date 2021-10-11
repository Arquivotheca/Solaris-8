/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

/*
 * note.h:	interface for annotating source with info for tools
 *
 * NOTE is the default interface, but if the identifier NOTE is in use for
 * some other purpose, you may prepare a similar header file using your own
 * identifier, mapping that identifier to _NOTE.  Also, exported header
 * files should *not* use NOTE, since the name may already be in use in
 * a program's namespace.  Rather, exported header files should include
 * sys/note.h directly and use _NOTE.  For consistency, all kernel source
 * should use _NOTE.
 */

#ifndef	_NOTE_H
#define	_NOTE_H

#pragma ident	"@(#)note.h	1.2	94/11/02 SMI"

#include <sys/note.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	NOTE _NOTE

#ifdef	__cplusplus
}
#endif

#endif	/* _NOTE_H */
