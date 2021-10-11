/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_ENTRIES_HH
#define	_FNSP_ENTRIES_HH

#pragma ident	"@(#)FNSP_entries.hh	1.5	96/04/05 SMI"


#include "FNSP_InitialContext.hh"
#include "FNSP_enterprise.hh"

// These are the definitions of the subclasses of FNSP_InitialContext::Entry
// that define specific resolution methods.  The code implementing these
// subclasses is in entries.cc.


// ******************** Host-related entries *****************************

class FNSP_InitialContext_ThisHostEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_ThisHostEntry::
	    FNSP_InitialContext_ThisHostEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
	void generate_equiv_names(void);
};

class FNSP_InitialContext_HostOrgUnitEntry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostOrgUnitEntry::
	    FNSP_InitialContext_HostOrgUnitEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
	void generate_equiv_names(void);
};

class FNSP_InitialContext_HostENSEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostENSEntry::
	    FNSP_InitialContext_HostENSEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_HostSiteEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostSiteEntry::
	    FNSP_InitialContext_HostSiteEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_HostSiteRootEntry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostSiteRootEntry::
	    FNSP_InitialContext_HostSiteRootEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_HostOrgEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostOrgEntry::
	    FNSP_InitialContext_HostOrgEntry(int ns);
	// non-virtual definition of resolution method
	void generate_equiv_names(void);
	void resolve(unsigned int);
};

class FNSP_InitialContext_HostUserEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostUserEntry::
	    FNSP_InitialContext_HostUserEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_HostHostEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_HostHostEntry::
	    FNSP_InitialContext_HostHostEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

// ******************** User-related entries *****************************


class FNSP_InitialContext_UserOrgUnitEntry :
public FNSP_InitialContext::UserEntry {
public:
	FNSP_InitialContext_UserOrgUnitEntry::
	FNSP_InitialContext_UserOrgUnitEntry(int ns, uid_t,
	    const FNSP_enterprise_user_info *);
	// non-virtual definition of resolution method
	void generate_equiv_names(void);
	void resolve(unsigned int);
};

class FNSP_InitialContext_UserSiteEntry :
public FNSP_InitialContext::UserEntry {
public:
	FNSP_InitialContext_UserSiteEntry::
	FNSP_InitialContext_UserSiteEntry(int ns, uid_t,
	    const FNSP_enterprise_user_info *);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_UserENSEntry :
public FNSP_InitialContext::UserEntry {
public:
	FNSP_InitialContext_UserENSEntry::
	FNSP_InitialContext_UserENSEntry(int ns, uid_t,
	    const FNSP_enterprise_user_info *);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
	int is_equiv_name(const FN_string *);
	void generate_equiv_names(void);
};

class FNSP_InitialContext_ThisUserEntry :
public FNSP_InitialContext::UserEntry {
public:
	FNSP_InitialContext_ThisUserEntry::
	FNSP_InitialContext_ThisUserEntry(int ns, uid_t,
	    const FNSP_enterprise_user_info*);
	// non-virtual definition of resolution method
	void generate_equiv_names(void);
	void resolve(unsigned int);
};

#ifdef FN_IC_EXTENSIONS

/* the following code are currently not used */

class FNSP_InitialContext_UserUserEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_UserUserEntry::
	    FNSP_InitialContext_UserUserEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_UserHostEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_UserHostEntry::
	    FNSP_InitialContext_UserHostEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_UserOrgEntry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_UserOrgEntry::
	    FNSP_InitialContext_UserOrgEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_UserSiteRootEntry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_UserSiteRootEntry::
	    FNSP_InitialContext_UserSiteRootEntry(int ns);
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

#endif /* FN_IC_EXTENSIONS */


/* ******************** Global entries ***************************** */

class FNSP_InitialContext_GlobalEntry : public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_GlobalEntry::FNSP_InitialContext_GlobalEntry();
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_GlobalDNSEntry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_GlobalDNSEntry::
	FNSP_InitialContext_GlobalDNSEntry();
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

class FNSP_InitialContext_GlobalX500Entry :
public FNSP_InitialContext::Entry {
public:
	FNSP_InitialContext_GlobalX500Entry::
	FNSP_InitialContext_GlobalX500Entry();
	// non-virtual definition of resolution method
	void resolve(unsigned int);
};

#endif /* _FNSP_ENTRIES_HH */
