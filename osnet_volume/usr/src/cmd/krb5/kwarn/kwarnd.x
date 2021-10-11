%/*
% *  Copyright (c) 1990,1991,1995-1999, by Sun Microsystems, Inc.
% *  All rights reserved.
% */
%
/* %#pragma ident	"@(#)kwarnd.x	1.1	99/07/18 SMI" *
%
%/*
% *  RPC protocol information for kwarnd, the usermode daemon that
% *  assists kinit, kdestroy with kwarnapi. It is kwarnd that executes all
% *  kwarnapi calls and sends credential cache expiration warning messages.

% *
% *  File generated from kwarnd.x
% */
%
%
%#include <sys/types.h>
%#include <sys/time.h>
%#include <rpc/auth_sys.h>
%#include <locale.h>
%
/*
 * These are the definitions for the interface to KWARND.
 */

#define MAX_PRINCIPAL_LEN 128

typedef string WARNING_NAME_T<MAX_PRINCIPAL_LEN>;

typedef unsigned int				OM_UINT32;

struct kwarn_add_warning_arg {
	WARNING_NAME_T warning_name;
	int cred_exp_time;			/* time in secs after epoch */
};

struct kwarn_add_warning_res {
	OM_UINT32	status;			/* status of kwarn call */
};

struct kwarn_del_warning_arg {
	WARNING_NAME_T warning_name;
};

struct kwarn_del_warning_res {
	OM_UINT32	status;			/* status of kwarn call */
};

/*
 *  The server accepts requests only from the loopback address.
 *  Unix authentication is used, and the port must be in the reserved range.
 */

program KWARNPROG {
    version KWARNVERS {

	/*
	 *  Called by the client to add a cred expiration warning
	 */
	kwarn_add_warning_res
		KWARN_ADD_WARNING(kwarn_add_warning_arg)			= 1;

	/*
	 *  Called by the client to delete a cred expiration warning
	 */
	kwarn_del_warning_res
		KWARN_DEL_WARNING(kwarn_del_warning_arg)			= 2;


    } = 1;
} = 100134;
