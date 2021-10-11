#pragma ident "@(#)memconf.h   1.3     92/11/25 SMI"

/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved. Sun
 * considers its source code as an unpublished, proprietary trade secret, and
 * it is available only under strict license provisions.  This copyright
 * notice is placed here only to protect Sun in the event the source is
 * deemed a published work.  Dissassembly, decompilation, or other means of
 * reducing the object code to human readable form is prohibited by the
 * license agreement under which this code is provided to the user or company
 * in possession of this copy.
 * 
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the Government
 * is subject to restrictions as set forth in subparagraph (c)(1)(ii) of the
 * Rights in Technical Data and Computer Software clause at DFARS 52.227-7013
 * and in similar clauses in the FAR and NASA FAR Supplement.
 */

/* $RCSfile: memconf.h $ $Revision: 1.2 $ $Date: 1992/09/12 15:31:38 $ */

/************************************************************************
 *																		*
 *			Copyright (c) 1985 by										*
 *		Digital Equipment Corporation, Maynard, MA						*
 *			All rights reserved.										*
 *																		*
 *	 The information in this software is subject to change	without 	*
 *	 notice  and should not be construed as a commitment by Digital 	*
 *	 Equipment Corporation. 											*
 *																		*
 *	 Digital assumes no responsibility for the use	or	reliability 	*
 *	 of its software on equipment which is not supplied by Digital. 	*
 *																		*
 *   Redistribution and use in source and binary forms are permitted	*
 *   provided that the above copyright notice and this paragraph are	*
 *	 duplicated in all such forms and that any documentation,			*
 *	 advertising materials, and other materials related to such 		*
 *   distribution and use acknowledge that the software was developed	*
 *   by Digital Equipment Corporation. The name of Digital Equipment	*
 *   Corporation may not be used to endorse or promote products derived	*
 *	 from this software without specific prior written permission.		*
 *	 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR     *
 *	 IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED 	*
 *   WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.*
 *   Do not take internally. In case of accidental ingestion, contact	*
 *	 your physician immediately.										*
 *																		*
 ************************************************************************/


#ifndef _MEMCONF_H
#define _MEMCONF_H

#ifndef PRE_COMPILED_HEADERS
#include <alloc.h>
#endif /* PRE_COMPILED_HEADERS


/*
 *	site specific and shared internal data structures used by mnemosyne.
 *	the only data structure that may need to be shared is the struct ptr,
 *	which is defined herein.
 *
 *	Marcus J. Ranum, 1990. (mjr@decuac.dec.com)
 */


/* size of internal hash tables - don't go wild - this is slow anyhow  */

#define MEM_HASHSIZE 127

/* names of files to write */

#define LINESFILE	"mem.sym"
#define PTRFILE 	"mem.dat"

/*
 *	storage for a pointer map entry - the only data structure we share
 *	a whole mess of these get written to mnem.dat as calls to malloc and
 *	whatnot are made. the distinction between an *allocated* pointer and
 *	and unallocated one is that 'siz' is 0 in freed ptrs. this is used
 *	by the post-processor to look for memory leaks.
 */

struct	ptr
{
	void   *ptr;		/* pointer to allocated memory */
	int 	map;		/* this pointer's map # */
	struct	ptr	*next;

	/* only part that gets written to the disk */

	struct
	{
		size_t siz;    /* size allocated (or 0) */
		int    smap;   /* symbol map # */
	} dsk;
};

#endif /* _MEMCONF_H */

