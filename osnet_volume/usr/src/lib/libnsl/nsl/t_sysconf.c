/*	Copyright (c) 1998 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)t_sysconf.c	1.2	98/04/19 SMI"


#include <rpc/trace.h>
#include <unistd.h>
#include <errno.h>
#include <stropts.h>
#include <sys/stream.h>
#include <assert.h>
#include <xti.h>
#include "timt.h"
#include "tx.h"

int
_tx_sysconf(int name, int api_semantics)
{
	assert(api_semantics == TX_XTI_XNS5_API);
	if (name != _SC_T_IOV_MAX) {
		trace1(TR_t_sysconf, name);
		t_errno = TBADFLAG;
		return (-1);
	}
	return ((int)_sysconf(_SC_T_IOV_MAX));
}
