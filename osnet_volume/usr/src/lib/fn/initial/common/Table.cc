/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Table.cc	1.6	96/04/05 SMI"


#include "FNSP_InitialContext.hh"


// This file contains the code that implements the FNSP_InitialContext::Table
// class and its IterationPosition subclass.

FNSP_InitialContext::IterationPosition::IterationPosition()
{
	position = (void *)new int;
	*(int *)position = 0;
}


FNSP_InitialContext::IterationPosition::
IterationPosition(const IterationPosition &src)
{
	position = (void *)new int;
	*(int *)position = *(int *)src.position;
}


FNSP_InitialContext::IterationPosition&
FNSP_InitialContext::IterationPosition::operator=(const IterationPosition &src)
{
	*(int *)position = *(int *)src.position;
	return (*this);
}


FNSP_InitialContext::IterationPosition::~IterationPosition()
{
	delete position;
}


// return pointer to first entry containing given name
// return 0 if not found

FNSP_InitialContext::Entry*
FNSP_InitialContext::Table::find(const FN_string &name)
{
	int i;

	// find first entry with given name
	for (i = 0; i < size; i++)
		if (entry[i]->find_name(name))
			return (entry[i]);
	return (0);
}



// Return a pointer to the first entry and set iteration pointer.

FNSP_InitialContext::Entry*
FNSP_InitialContext::Table::first(IterationPosition &iter_pos)
{
	*(int *)iter_pos.position = 0;
	return (entry[0]);
}


// Advance the iteration pointer and return a pointer to the entry
// If the iteration position is out of bounds, return 0.

FNSP_InitialContext::Entry*
FNSP_InitialContext::Table::next(IterationPosition &iter_pos)
{
	int &p = *(int *)iter_pos.position;

	if ((0 <= p) && (p < (size - 1))) {
		p++;
		return (entry[p]);
	} else {
		return (0);
	}
}


// Constructor for Table

#include "FNSP_entries.hh"

FNSP_InitialContext::Table::Table()
{
}

FNSP_InitialContext::CustomTable::CustomTable() : Table()
{
	// %%% not supported yet
	size = 0;
}

FNSP_InitialContext::GlobalTable::GlobalTable() : Table()
{
	size = 3;
	entry = new Entry* [size];

	entry[0] = new FNSP_InitialContext_GlobalEntry;
	entry[1] = new FNSP_InitialContext_GlobalDNSEntry;
	entry[2] = new FNSP_InitialContext_GlobalX500Entry;
}


FNSP_InitialContext::HostTable::HostTable(int ns) : Table()
{
	size = 8;
	entry = new Entry* [size];

	entry[0] = new FNSP_InitialContext_ThisHostEntry(ns);
	entry[1] = new FNSP_InitialContext_HostOrgUnitEntry(ns);
	entry[2] = new FNSP_InitialContext_HostOrgEntry(ns);
	entry[3] = new FNSP_InitialContext_HostENSEntry(ns);
	entry[4] = new FNSP_InitialContext_HostUserEntry(ns);
	entry[5] = new FNSP_InitialContext_HostHostEntry(ns);
	entry[6] = new FNSP_InitialContext_HostSiteEntry(ns);
	entry[7] = new FNSP_InitialContext_HostSiteRootEntry(ns);
}

FNSP_InitialContext::UserTable::UserTable(int ns,
    uid_t uid, UserTable* nxt)
: Table()
{
	my_uid = uid;
#ifdef FN_IC_EXTENSIONS
	size = 8;
#else
	size = 4;
#endif /* FN_IC_EXTENSIONS */

	entry = new Entry* [size];
	next_table = nxt;

	my_user_info = FNSP_get_enterprise(ns)->init_user_info(uid);

	entry[0] =
	    new FNSP_InitialContext_ThisUserEntry(ns, uid, my_user_info);
	entry[1] =
	    new FNSP_InitialContext_UserOrgUnitEntry(ns, uid, my_user_info);
	entry[2] =
	    new FNSP_InitialContext_UserSiteEntry(ns, uid, my_user_info);
	entry[3] =
	    new FNSP_InitialContext_UserENSEntry(ns, uid, my_user_info);
#ifdef FN_IC_EXTENSIONS
	entry[4] = new FNSP_InitialContext_UserOrgEntry(ns);
	entry[5] = new FNSP_InitialContext_UserUserEntry(ns);
	entry[6] = new FNSP_InitialContext_UserHostEntry(ns);
	entry[7] = new FNSP_InitialContext_UserSiteRootEntry(ns);
#endif /* FN_IC_EXTENSIONS */
}

const FNSP_InitialContext::UserTable*
FNSP_InitialContext::UserTable::find_user_table(uid_t uid) const
{
	if (my_uid == uid)
		return (this);

	// try next table
	if (next_table)
		return (next_table->find_user_table(uid));

	// no more to try
	return (0);
}
