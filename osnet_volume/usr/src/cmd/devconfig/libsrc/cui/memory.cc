#pragma ident "@(#)memory.cc   1.4     93/07/23 SMI"

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

//=============================================================================
//	Widget class implementation
//
//	$RCSfile: memory.cc $ $Revision: 1.11 $ $Date: 1992/09/12 15:24:14 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_H
#include "cui.h"
#endif
#ifdef MSDOS
#include <alloc.h>
#endif

#include "cuilib.h"

#endif	// PRE_COMPILED_HEADERS


static bool initialized = FALSE;		// have we initialized?
bool CUI_debuggingMemory = FALSE;		// are we debugging memory allocations?



//
//	malloc and zero-fill space, and quit if we fail
//

#ifdef MEMDB
void *emalloc(size_t size, char *file, int line)
#else
void *emalloc(size_t size)
#endif
{
	// first time through, check whether we should debug memory allocations

	if(!initialized)
	{
		char *env = getenv("MEMDEBUG");
		if(env)
			CUI_debuggingMemory = TRUE;
		initialized = TRUE;
	}

#ifdef MSDOS
	unsigned long memFree = farcoreleft();
	if(memFree < 4096L)
		CUI_fatal("Less than 4K free memory");
    if(farheapcheck() == _HEAPCORRUPT)
		CUI_fatal("Heap is corrupt");
#endif

    void *ptr = new char[size];
    if(ptr == NULL)
#ifdef MEMDB
		CUI_fatal(dgettext( CUI_MESSAGES, "out of memory at %s %d (requested %d bytes)"),
				  file, line, (int)size);
#else
		CUI_fatal(dgettext( CUI_MESSAGES, "out of memory (requested %d bytes)"), size);
#endif
	memset(ptr, 0, size);

#ifdef MEMDB
	if(CUI_debuggingMemory)
		recordMalloc(ptr, size, file, line);
#endif

    return(ptr);
}


//
//	free up malloc'd space (check for NULL ptr)
//

#ifdef MEMDB
void efree(void *ptr, char *file, int line)
#else
void efree(void *ptr)
#endif
{
	if(!ptr)
		return;
#ifdef MEMDB
	if(CUI_debuggingMemory)
		recordFree(ptr, file, line);
#endif
	delete(ptr);
}


//
//	reallocate possibly null string
//

char *CUI_newString(char *string)
{
	if(string == NULL)
        return(NULL);
	char *newstr = (char *)CUI_malloc(strlen(string) + 1);
    strcpy(newstr, string);
    return(newstr);
}

