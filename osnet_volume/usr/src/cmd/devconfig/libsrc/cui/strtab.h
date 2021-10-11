#pragma ident "@(#)strtab.h   1.3     92/11/25 SMI"

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

//============================== -*-Mode: c++;-*- =============================
//	StringTable class definition
//
//	Compiles strings to integers for fast comparison by storing them
//	in a Symtab.  We also copy strings into an array, for fastest
//	possible lookup by ID.
//
//	$RCSfile: strtab.h $ $Revision: 1.4 $ $Date: 1992/09/12 15:16:47 $
//=============================================================================

#ifndef _CUI_STRTAB_H
#define _CUI_STRTAB_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_SYMTAB_H
#include "symtab.h"
#endif

#endif // PRE_COMPILED_HEADERS


class StringTable : public Symtab
{
	protected:

		char **strings; 		// saved copies of strings
		int  arraySize; 		// current size of strings array
		int  numStrings;		// number of strings currently in array
		int  firstReallocated;	// index of first reallocated string

    public:

		// memory management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor

		StringTable(int size = 127);
		~StringTable(void);

		// 'compile' string and return its ID

		int compile(char *string, bool addUnique = FALSE);

		// return string associated with ID

		char *value(int id);
};

#endif // _CUI_STRTAB_H


