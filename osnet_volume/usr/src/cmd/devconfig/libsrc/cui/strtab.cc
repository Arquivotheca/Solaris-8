#pragma ident "@(#)strtab.cc   1.3     92/11/25 SMI"

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
//	StringTable class implementation
//
//	$RCSfile: strtab.cc $ $Revision: 1.8 $ $Date: 1992/09/12 15:24:50 $
//
//	Compiling a string involves associating it with a unique integer, which
//	serves as an index into an array of strings (this enables us to very
//	quickly 'de-compile' a string).  Compiling a string involves adding it
//	to the array at the next available slot, and then inserting it into
//	a symbol-table (so that we can quickly look-up a string's compiled value)
//
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_STRTAB_H
#include "strtab.h"
#endif

#endif	// PRE_COMPILED_HEADERS


#define BUMP 64 	// grow array by this number of elements


//
//	constructor
//

StringTable::StringTable(int size)
	: Symtab(size)
{
	numStrings = 0;
	arraySize  = 256;	// start with a decent-sized array
	firstReallocated = 0;
	MEMHINT();
	strings = (char **)CUI_malloc(arraySize * sizeof(char *));
}


//
//	destructor
//

StringTable::~StringTable(void)
{
	for(int i = firstReallocated; i < numStrings; i++)
	{
		MEMHINT();
        CUI_free(strings[i]);
	}
	MEMHINT();
    CUI_free(strings);
}


//
//	'compile' a string into an integer
//
//	(If addUnique is TRUE, we're adding a string known to be unique,
//	and we can save a little time by not doing a lookup.  Also, since
//	all our unique strings are added at the beginning, we don't bother
//	to reallocate them.  We simply keep track of the index value of
//	the maximum 'unique' string, and take care not to deallocate these
//	when we're done.)
//

int StringTable::compile(char *string, bool addUnique)
{
	// treat NULL specially (we can't store NULLs in our array)
	// (-1 signifies NULL)

	if(!string)
		return(-1);

	// if string is already in symtab, return its ID
	// else insert it and return new ID

	int index = 0;
	if(!addUnique)
		index = (int)lookup(string);
    if(!index)
	{
		// bump our count and insert into Symtab

		index = numStrings++;
		insert(string, (void *)index);

		// grow strings array if necessary

		if(index == arraySize - 1)	// keep final NULL!
		{
			strings = CUI_arrayGrow(strings, BUMP);
			arraySize += BUMP;
		}

		char *ptr;

		// if this string is not 'unique', reallocate it

		if(!addUnique)
		{
			MEMHINT();
			ptr = CUI_newString(string);
		}

		// else bump our firstReallocated index and simply copy the pointer

		else
		{
			firstReallocated++;
			ptr = string;
		}

		// insert string into array

		strings[index] = ptr;
    }

	// apply some aliases

	switch(index)
	{
		case xId:
			return((int)colId);
		case yId:
			return((int)rowId);
		default:
			return(index);
	}
}


//
//	return string associated with ID
//	(by simply indexing into our array)
//

char *StringTable::value(int id)
{
	// -1 signifies NULL

	if (id > arraySize || id < 0)
		return(NULL);
	else
		return(strings[id]);
}

