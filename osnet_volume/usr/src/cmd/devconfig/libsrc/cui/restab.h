#pragma ident "@(#)restab.h   1.3     92/11/25 SMI"

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
//	Resource Table class definition
//
//	$RCSfile: restab.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:31:48 $
//=============================================================================

#ifndef _CUI_RESTAB_H
#define _CUI_RESTAB_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_STRINGID_H
#include "stringid.h"
#endif

#endif // PRE_COMPILED_HEADERS


//=============================================================================
//  Resource Table Entry
//=============================================================================

class RestabEntry
{
	friend class ResourceTable;

	protected:

		char			*resource;	// resource hierarchy specification
		CUI_StringId	name;		// resource name (compiled)
		int 			value;		// value (compiled if string)

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor (strings are copied/compiled before we get them...)

		RestabEntry(char *Resource, CUI_StringId Name, int Value)
		{
			resource = Resource;
			name	 = Name;
            value    = Value;
		}

		// destructor

		~RestabEntry(void)
		{
			CUI_free(resource);
		}

        // does specified resource match this entry?

		int match(CUI_StringId resource, char *cName, char *iName);

#ifdef TEST

        // print entry to file descriptor

		void printOn(FILE *fd);

#endif // TEST

};


//=============================================================================
//	Resource Table
//=============================================================================

class ResourceTable
{
	protected:

		char		*appName;
		RestabEntry **entries;
		int 		arraySize;
        int         used;

		int processLine(char *buffer, int line, char *file, int createWidgets);

    public:

		// memory-management

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		// constructor & destructor

		ResourceTable(char *name);
		~ResourceTable(void);

		// load resources

		int load(int createWidgets);
		int load(char *file, int createWidgets);

        // return value for specified resource, or NULL if no matching entry

		int lookup(CUI_StringId rName, char *iName, char *cName,
				   int &value);

		// report an error

		static int formatError(int line, char *file);

#ifdef TEST

        // print table to file descriptor

		void printOn(FILE *fd);

#endif // TEST

};


#endif // _CUI_RESTAB_H

