#pragma ident "@(#)symtab.h   1.3     92/11/25 SMI"

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
//	Symbol and Symtab class definitions
//
//	$RCSfile: symtab.h $ $Revision: 1.3 $ $Date: 1992/09/12 15:19:06 $
//=============================================================================

#ifndef _CUI_SYMTAB_H
#define _CUI_SYMTAB_H

#ifndef PRE_COMPILED_HEADERS

#ifndef _CUI_H
#include "cui.h"
#endif

#endif // PRE_COMPILED_HEADERS


typedef int  (*DELFUNC)(void *);	// pointer to delfunc

class Symbol
{
	// friends

		friend class Symtab;

	protected:

		char   *name;
		void   *ptr;
		Symbol *next;

    public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		Symbol(char *n, void *p)
		{
			name  = CUI_newString(n);
			ptr   = p;
			next  = 0;
		}
		~Symbol(void)
		{
			CUI_free(name);
		}
};
#define NULL_SYMBOL (Symbol *)0


class Symtab
{
	protected:

		Symbol	**table;			// pointer to hash table
		int 	slots;				// number of slots in table

		char *makeName(char *name, int type = 0);
		int  hash(char *name, int size);

    public:

		void *operator new(size_t size) 	{ return(CUI_malloc(size)); 	}
		void operator delete(void *ptr) 	{ CUI_free(ptr);				}

		Symtab(int size = 127);
		virtual ~Symtab(void);
		int  insert(char *name, void *ptr, int type = 0);
		void *lookup(char *name, int type = 0);
		char *whatName(void *data);
        int  remove(char *name, DELFUNC delfunc = 0, int type = 0);
		void removeAll(DELFUNC delfunc = NULL);
};


#endif // _CUI_SYMTAB_H

