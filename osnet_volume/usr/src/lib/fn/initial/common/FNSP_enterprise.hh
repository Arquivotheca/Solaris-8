/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _FNSP_ENTERPRISE_HH
#define	_FNSP_ENTERPRISE_HH

#pragma ident	"@(#)FNSP_enterprise.hh	1.7	96/04/05 SMI"

#include <xfn/xfn.hh>
#include <sys/types.h>  /* for uid_t */

typedef struct _FNSP_enterprise_user_info FNSP_enterprise_user_info;

class FNSP_enterprise {
protected:
	FN_string *root_directory;

public:
	virtual ~FNSP_enterprise() { }
	virtual FNSP_enterprise_user_info *init_user_info(uid_t)
		{ return (0); }
	virtual const FN_string *get_root_orgunit_name();
	virtual FN_string *get_user_orgunit_name(uid_t,
		const FNSP_enterprise_user_info *,
		FN_string ** = 0);
	virtual FN_string *get_user_name(uid_t,
		const FNSP_enterprise_user_info *);
	virtual FN_string *get_host_orgunit_name(FN_string ** = 0);
	virtual FN_string *get_host_name();
	virtual const FN_identifier *get_addr_type() = 0;
};

extern FNSP_enterprise *FNSP_get_enterprise(int ns);

#endif /* _FNSP_ENTERPRISE_HH */
