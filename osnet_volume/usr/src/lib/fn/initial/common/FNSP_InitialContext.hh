/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_INITIALCONTEXT_HH
#define	_FNSP_INITIALCONTEXT_HH

#pragma ident	"@(#)FNSP_InitialContext.hh	1.11	96/04/05 SMI"

#include <xfn/fn_spi.hh>
#include <synch.h>
#include <stdlib.h>   /* for uid_t */
#include "FNSP_enterprise.hh"

#define	FNSP_HOST_TABLE 0
#define	FNSP_USER_TABLE 1
#define	FNSP_GLOBAL_TABLE 2
#define	FNSP_CUSTOM_TABLE 3

#define	FNSP_NUMBER_TABLES 4

typedef enum FNSP_IC_type {
	FNSP_ALL_IC,
	FNSP_HOST_IC,
	FNSP_USER_IC} FNSP_IC_type;

typedef enum FNSP_IC_name_type {
	FNSP_THISORGUNIT,
	FNSP_THISHOST,
	FNSP_THISSITE,
	FNSP_THISENS,
	FNSP_ORGUNIT,
	FNSP_SITE,
	FNSP_USER,
	FNSP_HOST,
	FNSP_MYSELF,
	FNSP_MYORGUNIT,
	FNSP_MYSITE,
	FNSP_MYENS,
	FNSP_GLOBAL,
	FNSP_DNS,
	FNSP_X500
	} FNSP_IC_name_type;

// Use csvc_weak_static in order to support "/..."

class FNSP_InitialContext : public FN_ctx_csvc_weak_static {
public:

	FN_composite_name *p_component_parser(const FN_composite_name &,
	    FN_composite_name **rest,
	    FN_status_psvc& s);

	// non-virtual declarations of the context service operations
	FN_ref *get_ref(FN_status &)const;
	FN_composite_name *equivalent_name(const FN_composite_name &,
					    const FN_string&,
					    FN_status &);

	FN_ref *c_lookup(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	FN_namelist* c_list_names(const FN_string &name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);
	int c_bind(const FN_string &name, const FN_ref &,
	    unsigned BindFlags, FN_status_csvc&);
	int c_unbind(const FN_string &name, FN_status_csvc&);
	int c_rename(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc&);
	FN_ref *c_create_subcontext(const FN_string &name, FN_status_csvc&);
	int c_destroy_subcontext(const FN_string &name, FN_status_csvc&);
	FN_attrset* c_get_syntax_attrs(const FN_string &name,
	    FN_status_csvc&);
	// Attribute Operations
	FN_attribute *c_attr_get(const FN_string &,
	    const FN_identifier &,
	    unsigned int,
	    FN_status_csvc&);
	int c_attr_modify(const FN_string &,
	    unsigned int,
	    const FN_attribute&,
	    unsigned int,
	    FN_status_csvc&);
	FN_valuelist *c_attr_get_values(const FN_string &,
	    const FN_identifier &,
	    unsigned int,
	    FN_status_csvc&);
	FN_attrset *c_attr_get_ids(const FN_string &,
	    unsigned int,
	    FN_status_csvc&);
	FN_multigetlist *c_attr_multi_get(const FN_string &,
	    const FN_attrset *,
	    unsigned int,
	    FN_status_csvc&);
	int c_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist&,
	    unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);
	int c_attr_bind(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

	FN_ref *c_lookup_nns(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	FN_namelist* c_list_names_nns(const FN_string &name,
	    FN_status_csvc&);
	FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc&);
	int c_bind_nns(const FN_string &name, const FN_ref &,
	    unsigned BindFlags, FN_status_csvc&);
	int c_rename_nns(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc&);
	int c_unbind_nns(const FN_string &name, FN_status_csvc&);
	FN_ref *c_create_subcontext_nns(const FN_string &name,
	    FN_status_csvc&);
	int c_destroy_subcontext_nns(const FN_string &name, FN_status_csvc&);
	FN_attrset* c_get_syntax_attrs_nns(const FN_string &name,
	    FN_status_csvc&);

	// Attribute Operations
	FN_attribute *c_attr_get_nns(const FN_string &,
	    const FN_identifier &,
	    unsigned int,
	    FN_status_csvc&);
	int c_attr_modify_nns(const FN_string &,
	    unsigned int,
	    const FN_attribute&,
	    unsigned int,
	    FN_status_csvc&);
	FN_valuelist *c_attr_get_values_nns(const FN_string &,
	    const FN_identifier &,
	    unsigned int,
	    FN_status_csvc&);
	FN_attrset *c_attr_get_ids_nns(const FN_string &,
	    unsigned int,
	    FN_status_csvc&);
	FN_multigetlist *c_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *,
	    unsigned int,
	    FN_status_csvc&);
	int c_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist&,
	    unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);

	int c_attr_bind_nns(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search_nns(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

	// The state consists of a read-only table in
	// which each entry records a binding.
	// Note that the binding that is recorded is
	// the binding of "<name>:".
	// e.g. the binding of "org:" not "org" itself.

	// The name portion of each entry in the table is set up
	// at the time the entry is made and, the entry is
	// placed in the table
	// at table construction time.  The table interface supports only
	// "read"-like operations thereafter.

	// We defer actually resolving each name until there is a
	// an operation that forces us to determine the
	// reference or simply whether or not the name is bound.

	// This is done by making the implementation of
	// Entry objects (defined
	// just below) determine the reference for the entry on the first
	// get_ref() or is_bound() call.  When this happens,
	// the entry object calls a protected virtual
	// resolve() method to actually
	// resolve its name.  Each actual entry in the table is an instance
	// of a subclass of Entry, and defines its particular resolve method.

	// This implementation is MT-safe: many threads may safely share
	// the same initial context object.  Locking for multiple threads is
	// handled by private methods on each Entry.  Subclasses of class
	// Entry need not worry about locking in their
	// specific resolve() methods.
	// The locking policy is described further in Entry.cc

	class Entry {
	public:
		const FN_string *first_name(void *&iter_pos);
		const FN_string *next_name(void *&iter_pos);
		// equiv_name(-1) returns the last (i.e. short) name.
		const FN_string *equiv_name(int = -1);
		virtual int is_equiv_name(const FN_string *);
		FNSP_IC_name_type name_type(void);

		// returns whether the given name is in this entry
		int find_name(const FN_string &name);

		// Forces resolution of the name, and if successful,
		// allocates a copy of the reference and returns it;
		// returns 0 if the FN_status_code is not success.
		FN_ref *reference(unsigned int auth,
				    unsigned &FN_status_code);

		// constructor
		Entry(int ns = 0);
		virtual ~Entry();

	protected:
		virtual void resolve(unsigned int auth) = 0;
		virtual void generate_equiv_names(void);
		const FN_string * unlocked_equiv_name(int = 0);
		size_t num_names;
		int num_equiv_names;
		FNSP_IC_name_type stored_name_type;
		FN_string **stored_names;
		FN_string **stored_equiv_names;
		unsigned stored_status_code;
		FN_ref *stored_ref;
		int name_service;
		void lock_and_generate_equiv_names(void);
		void get_reader_lock();
		void release_reader_lock();

	private:
		rwlock_t entry_lock;
		void lock_and_resolve(unsigned int auth);
		void get_writer_lock();
		void release_writer_lock();
	};

	class UserEntry : public Entry {
	public:
		UserEntry(int ns, uid_t,
		    const FNSP_enterprise_user_info*);
		UserEntry(int ns, uid_t);
	protected:
		uid_t my_uid;
		const FNSP_enterprise_user_info *my_user_info;
	};

	// forward declaration
	class IterationPosition;

private:
	class Table {
	public:
		// find first entry with given name, return a pointer to it
		// if not found return 0
		virtual Entry* find(const FN_string &name);
		// iterator functions
		// first puts iteration position at beginning of table
		// next advances the iteration position by one
		// both return a pointer to the entry at the new position
		// next returns a 0 pointer if already at the end
		virtual Entry* first(IterationPosition& iter_pos);
		virtual Entry* next(IterationPosition& iter_pos);
		Table();
	protected:
		Entry** entry;
		int size;
	};

	Entry *find_entry(const FN_string &);

public:
	class UserTable : public Table {
	private:
		uid_t my_uid;
		FNSP_enterprise_user_info *my_user_info;
		UserTable* next_table;
	public:
		UserTable(int ns, uid_t, UserTable*);
		~UserTable();

		const UserTable* find_user_table(uid_t) const;
	};

private:
	class HostTable : public Table {
	public:
		HostTable(int ns);
		~HostTable();
	};

	class GlobalTable : public Table {
	public:
		GlobalTable();
		~GlobalTable();
	};

	class CustomTable : public Table {
		/* *** probably will have storage location */
	public:
		CustomTable();
		~CustomTable();

		// %%% probably will define own methods for find,
		// first/next
	};

	class IterationPosition {
	public:
		IterationPosition();
		IterationPosition(const IterationPosition&);
		IterationPosition& operator=(const IterationPosition&);
		~IterationPosition();
		friend Entry* Table::first(IterationPosition& iter_pos);
		friend Entry* Table::next(IterationPosition& iter_pos);

	private:
		void *position;
	};

	Table* tables[FNSP_NUMBER_TABLES];

	friend FN_ctx_svc* FNSP_InitialContext_from_initial(
	    unsigned int auth, int ns,
	    FNSP_IC_type ic_type, uid_t uid, FN_status &);

	// constructor for use by from_initial();
	FNSP_InitialContext(
	    unsigned int authoritative,
	    int ns,
	    uid_t uid,
	    HostTable*& hosts,
	    UserTable*&,
	    GlobalTable*& global,
	    CustomTable*&);

	// for host entries only
	FNSP_InitialContext(unsigned int auth,
	    int ns, HostTable*& hosts);

	// for user entries only
	FNSP_InitialContext(
	    unsigned int auth,
	    int ns,
	    uid_t uid,
	    UserTable*&);

	virtual ~FNSP_InitialContext();

	// for performing equivalence calculations
	FN_composite_name *
		global_equivs(FNSP_IC_name_type lead_type,
					const FN_string &leading_name,
					Entry *lead_entry,
					Entry *orig_entry,
					const FN_composite_name &orig_name,
					void *ip);
	FN_composite_name *
		thisorgunit_equivs(FNSP_IC_name_type lead_type,
					const FN_string &leading_name,
					Entry *lead_entry,
					Entry *orig_entry,
					const FN_composite_name &orig_name,
					void *ip);
	FN_composite_name *
		thishost_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		thisens_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		myself_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		orgunit_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		site_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		user_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		host_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		myorgunit_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
	FN_composite_name *
		myens_equivs(FNSP_IC_name_type lead_type,
				    const FN_string &leading_name,
				    Entry *lead_entry,
				    Entry *orig_entry,
				    const FN_composite_name &orig_name,
				    void *ip);
};

#endif /* _FNSP_INITIALCONTEXT_HH */
