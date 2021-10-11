/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SET_HH
#define	_SET_HH

#pragma ident	"@(#)Set.hh	1.5	96/03/31 SMI"

#include <xfn/misc_codes.h>

// generic set (stores subclasses of SetItem)

class SetItem {
    public:
	virtual ~SetItem();
	virtual SetItem *copy() = 0;
	virtual int key_match(const void *key, int case_sense) = 0;
	virtual int item_match(SetItem &, int case_sense) = 0;
	virtual unsigned long key_hash() = 0;
	virtual unsigned long key_hash(const void *key) = 0;

	SetItem *next;
};

class Set {
    public:
	Set(int case_sense = 1);
	~Set();
	Set(const Set &);
	Set &operator=(const Set &);
	unsigned count() const;
	const SetItem *first(void *&iter_pos);
	const SetItem *next(void *&iter_pos);
	SetItem *get(const void *key);
	add(SetItem *, unsigned int exclusive = FN_OP_EXCLUSIVE);
	remove(const void *key);

    private:
	void __delete_all();
	void __copy(const Set &);
	SetItem *__locate(SetItem *n, SetItem **&head, unsigned long &bucket);
	SetItem *__locate(const void *, SetItem **&head);

	unsigned item_count;
	// SetItem *first_item;
	SetItem	**item_tab;
	int	item_tab_mod;
	int	p_case_sense;
};

#endif /* _SET_HH */
