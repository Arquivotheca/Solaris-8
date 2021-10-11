#pragma ident "@(#)symtab.cc   1.4     93/07/23 SMI"

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
//	Symbol and Symtab class implementation
//
//	$RCSfile: symtab.cc $ $Revision: 1.4 $ $Date: 1992/09/12 15:25:07 $
//=============================================================================

#include "precomp.h"

#ifndef PRE_COMPILED_HEADERS

#ifndef  _CUI_SYMTAB_H
#include "symtab.h"
#endif

#endif	// PRE_COMPILED_HEADERS


//
//	Symtab constructor
//
//	Make a Symbol table of the indicated size.	For best results, "size"
//	should be prime, eg. 47 61 89 113 127 157 193 211 257 293 359 401.
//

Symtab::Symtab(int size)
{
	slots = size;

	// allocate an array of lists and set each to null

	MEMHINT();
    table = (Symbol **)CUI_malloc(size * sizeof(Symbol *));
	for(int i = 0; i < size; i++)
		table[i] = NULL_SYMBOL;
}


//
//	Symtab destructor
//

Symtab::~Symtab(void)
{
	MEMHINT();
    CUI_free(table);
}


//
//	add a symbol to the symbol table
//

int Symtab::insert(char *name, void *ptr, int type)
{
	// NULL name is error

    if(name == NULL)
		return(-1);

	// if symbol is already in table, return error

	if(lookup(name, type))
    {
		CUI_errno = E_NOT_UNIQUE;
		return(CUI_errno);
    }

	// make unique name from name and type

	name = makeName(name, type);

	// make new Symbol and hash the name

	MEMHINT();
    Symbol *symbol = new Symbol(name, ptr);
	int hashval = hash(name, slots);

	// insert Symbol at head of the appropriate list

	if(table[hashval])
		symbol->next = table[hashval];
	table[hashval] = symbol;

    return(0);
}


//
//	look up symbol in symtab and return its pointer
//	(if we have duplicates, we'll return the latest added)
//

void *Symtab::lookup(char *name, int type)
{
	// NULL name is error

    if(name == NULL)
		return(NULL);

	// make unique name from name and type

	name = makeName(name, type);

    /* hash the name, and get head of list for that slot */

	int hashval    = hash(name, slots);
	Symbol *symbol = table[hashval];

	// search list for match

    while(symbol)
    {
		if(strcmp(name, symbol->name) == 0)
			return(symbol->ptr);
		symbol = symbol->next;
    }
	return(NULL_SYMBOL);
}


//
//	remove symbol from symbtab
//	(if delfunc is non-null, it will be called with pointer to data
//	for each entry in symtab; this function will presumably delete
//	the private data)
//

int Symtab::remove(char *name, DELFUNC delfunc, int type)
{
	Symbol *head, *current, *previous;

    // anything to do?

	if(!name)
		return(0);

	// make unique name from name and type

	name = makeName(name, type);

	// hash the name, and get head of list for that slot

	int hashval = hash(name, slots);
	head = table[hashval];

	/* chase down list looking for a match */

    previous = current = head;
    while(current)
    {
		// if the names match...

		if(strcmp(name, current->name) == 0)
		{
			// if we have a delfunc, call it with the Symbol's data

			if(delfunc)
				delfunc(current->ptr);

			//
			//	if this is head, point table entry to our next
			//	else point previous to our next
			//

			if(current == head)
				table[hashval] = current->next;
			else
				previous->next = current->next;

			// delete the symbol and return success

			MEMHINT();
            delete(current);
			return(0);
		}

		// go on to next

		previous = current;
		current  = current->next;
    }

	// no match - return failure

    return(-1);
}


//
//	remove all entries from symtab
//	(if delfunc is non-null, it will be called with pointer to data
//	for each entry in symtab; this function will presumably delete
//	the private data)
//

void Symtab::removeAll(DELFUNC delfunc)
{
    Symbol *current, *next;

	for(int i = 0; i < slots; i++)
    {
		current = table[i];
		while(current)
		{
			if(delfunc)
				delfunc(current->ptr);
			next = current->next;
			MEMHINT();
            delete(current);
			current = next;
		}
    }
}


//
//	brute-force reverse lookup routine
//	(given a data pointer, return the (first) name it's stored under)
//

char *Symtab::whatName(void *data)
{
	Symbol *current, *next;

	for(int i = 0; i < slots; i++)
    {
		current = table[i];
		while(current)
		{
			if(current->ptr == data)
				return(current->name);
			next = current->next;
			current = next;
		}
    }
	return(NULL);
}


//
//	make unique name from name and type
//

char *Symtab::makeName(char *name, int type)
{
	// if type not specified, just return the name

	if(!type)
		return(name);

	// else return <type>name

    static char buffer[80];
	sprintf(buffer, "%d", type);
	strncpy(buffer + strlen(buffer), name, 70);
    return(buffer);
}


//
//	compute hash value
//

int Symtab::hash(char *name, int size)
{
	int hashval;

	// simply add up the numeric value of each char

	for (hashval = 0; *name; hashval += *(unsigned char *)name++)
		;
    return(hashval % size);
}


