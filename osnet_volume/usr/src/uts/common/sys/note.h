/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

/*
 * sys/note.h:	interface for annotating source with info for tools
 *
 * This is the underlying interface; NOTE (/usr/include/note.h) is the
 * preferred interface, but all exported header files should include this
 * file directly and use _NOTE so as not to take "NOTE" from the user's
 * namespace.  For consistency, *all* kernel source should use _NOTE.
 *
 * By default, annotations expand to nothing.  This file implements
 * that.  Tools using annotations will interpose a different version
 * of this file that will expand annotations as needed.
 */

#ifndef	_SYS_NOTE_H
#define	_SYS_NOTE_H

#pragma ident	"@(#)note.h	1.3	94/11/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _NOTE
#define	_NOTE(s)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NOTE_H */
