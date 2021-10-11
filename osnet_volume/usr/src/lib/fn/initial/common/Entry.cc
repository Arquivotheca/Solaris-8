/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Entry.cc	1.8	97/10/16 SMI"

#include "FNSP_InitialContext.hh"
#include <string.h>
#include <synch.h>
#include <stdlib.h>


// This file contains the code implementing the FNSP_InitialContext::Entry
// base class.  Definitions and code for the resolve() methods of the
// specific subclasses is in entries.hh/cc

FNSP_InitialContext::Entry::Entry(int ns)
{
	name_service = ns;
	stored_ref = NULL;
	stored_status_code = FN_E_NAME_NOT_FOUND;
	stored_names = NULL;
	stored_equiv_names = NULL;
	num_equiv_names = 0;

	// Work around a libthread bug (bugid 1181133 ??)
	memset(&entry_lock, 0, sizeof (entry_lock));
	rwlock_init(&entry_lock, USYNC_THREAD, 0);
}

FNSP_InitialContext::Entry::~Entry()
{
	if (stored_names) {
		int i;
		for (i = 0; i < num_names; i++)
			delete stored_names[i];
		delete [] stored_names;
	}
	if (stored_equiv_names) {
		int i;
		for (i = 0; i < num_equiv_names; i++)
			delete stored_equiv_names[i];
		delete [] stored_equiv_names;
	}
	delete stored_ref;
}

// Our locking policy is as follows:
// No lock is needed to inspect the member 'stored_name', which is
// set once at entry construction time and is never modified.
//
// A reader's lock is needed to inspect the 'stored_status_code' or
// 'stored_ref' member.
//
// The writer's lock is needed to modify the 'stored_status_code' or
// 'stored_ref' member.
//
// Our resolution policy is that we attempt to resolve the name whenever
// a reference() call is made for the entry and the current stored_status
// code is not success.  The resolve() method is called and should attempt
// to resolve the name and set the new stored_status, which is returned to
// by the reference() call.
//
// Note: Once the status is success, no further attempt made to resolve the
// entry.  The current implementation does not support repeated successful
// dynamic resolution.


int
FNSP_InitialContext::Entry::find_name(const FN_string &name)
{
	int i;

	// find first entry with given name
	for (i = 0; (i < num_names) && (name.compare(*(stored_names[i]),
	    FN_STRING_CASE_INSENSITIVE) != 0); i++) {
		// empty loop
	}

	return (i < num_names);
}


const FN_string *
FNSP_InitialContext::Entry::first_name(void *&iter_pos)
{
	iter_pos = (void *)1;
	return (stored_names[0]);
}


const FN_string *
FNSP_InitialContext::Entry::next_name(void *&iter_pos)
{
	const FN_string *answer = 0;
	size_t pos = (size_t)iter_pos;
	if (pos < num_names) {
		answer = stored_names[pos];
		iter_pos = (void *)++pos;
	}
	return (answer);
}

void
FNSP_InitialContext::Entry::generate_equiv_names(void)
{
	// default is no op
	stored_equiv_names = new FN_string*[0];
}

int
FNSP_InitialContext::Entry::is_equiv_name(const FN_string *str)
{
	if (str == NULL)
		return (0);

	get_reader_lock();

	if (stored_equiv_names == NULL) {
		release_reader_lock();
		lock_and_generate_equiv_names();
		get_reader_lock();
	}

	const FN_string *myequiv;
	int i;

	// use case-insensitive compare for most liberal interpretation of eq.
	// However, strictly speaking, true eq can only be determined
	// at the context where name is bound

	for (i = 0; i < num_equiv_names; i++) {
		myequiv = stored_equiv_names[i];
		if (myequiv != NULL &&
		    myequiv->compare(*str, FN_STRING_CASE_INSENSITIVE) == 0) {
			release_reader_lock();
			return (1);
		}
	}
	release_reader_lock();
	return (0);
}


void
FNSP_InitialContext::Entry::lock_and_resolve(unsigned int auth)
{
	// Get exclusive access
	get_writer_lock();

	// Yes, we must check the status code again at this point!
	// Some other thread may have gotten the lock just before us
	// and resolved the name for us.

	// if the entry has not yet been resolved, call the specific
	// resolution method for this entry it will also set status,
	// as appropriate.

	if (stored_status_code != FN_SUCCESS) resolve(auth);

	// release lock
	release_writer_lock();
}

const FN_string *
FNSP_InitialContext::Entry::unlocked_equiv_name(int which)
{
	// Used only after a lock has been acquired (e.g. equiv_name())

	if (stored_equiv_names == NULL)
		generate_equiv_names();

	if (which < num_equiv_names)
		return (stored_equiv_names[which]);
	else
		return (NULL);
}

void
FNSP_InitialContext::Entry::lock_and_generate_equiv_names()
{
	// Get exclusive access
	get_writer_lock();

	// Yes, we must check that the work hasn't been done again.
	// Some other thread may have gotten the lock just before us
	// and generated the name before us.

	// if the entry has not yet been generated, call the specific
	// generation method

	if (stored_equiv_names == NULL)
		generate_equiv_names();

	// release lock
	release_writer_lock();
}


const FN_string *
FNSP_InitialContext::Entry::equiv_name(int which)
{
	const FN_string *ename = NULL;

	get_reader_lock();

	if (stored_equiv_names == NULL) {
		release_reader_lock();
		lock_and_generate_equiv_names();
		get_reader_lock();
	}

	if (which < 0)
		which = num_equiv_names - 1;	// get last (i.e. short) name
	if (which >= 0 && which < num_equiv_names)
		ename = stored_equiv_names[which];
	release_reader_lock();

	return (ename);
}

FN_ref *
FNSP_InitialContext::Entry::reference(unsigned int auth, unsigned &scode)
{
	FN_ref *retref = 0;

	get_reader_lock();

	if (stored_status_code != FN_SUCCESS) {
		release_reader_lock();
		lock_and_resolve(auth);
		get_reader_lock();
	}

	scode = stored_status_code;

	if (stored_status_code == FN_SUCCESS) {
		retref = new FN_ref(*stored_ref);
	}

	release_reader_lock();

	return (retref);
}

void
FNSP_InitialContext::Entry::get_writer_lock()
{
	rw_wrlock(&entry_lock);
}

void
FNSP_InitialContext::Entry::release_writer_lock()
{
	rw_unlock(&entry_lock);
}


void
FNSP_InitialContext::Entry::get_reader_lock()
{
	rw_rdlock(&entry_lock);
}

void
FNSP_InitialContext::Entry::release_reader_lock()
{
	rw_unlock(&entry_lock);
}

FNSP_IC_name_type
FNSP_InitialContext::Entry::name_type(void)
{
	return (stored_name_type);
}
