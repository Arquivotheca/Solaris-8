/*
 *	nis_callback.x
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

%#pragma ident	"@(#)nis_callback.x	1.4	92/07/14 SMI"

/*
 * "@(#)zns_cback.x 1.2 90/09/10 Copyr 1990 Sun Micro" 
 *
 * RPCL description of the Callback Service.
 */

#ifdef RPC_HDR
%#include <rpcsvc/nis.h>
#endif
#ifdef RPC_XDR
%#include "nis_clnt.h"
#endif

typedef nis_object	*obj_p;

struct cback_data {
	obj_p		entries<>;	/* List of objects */
};

program CB_PROG {
	version CB_VERS {
		bool	CBPROC_RECEIVE(cback_data) = 1;
		void	CBPROC_FINISH(void) = 2;
		void	CBPROC_ERROR(nis_error) = 3;
	} = 1;
} = 100302;
