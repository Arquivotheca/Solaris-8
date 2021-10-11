/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)fnsypprot.x	1.4 97/02/12 SMI"
 */

/*
 *
 *  Protocol to transfer the FNS ypupdate key/value pairs
 *  between the FNS NIS master server and the client
 *
 */

%#include <rpcsvc/yp_prot.h>

struct xFNS_YPUPDATE_DATA
{
	string username<YPMAXDOMAIN>;
	int user_host;
	string domain<YPMAXDOMAIN>;
	string map<YPMAXMAP>;
	string key<YPMAXRECORD>;
	string value<>;
	string old_value<>;
	int op;
	int timestamp;
};

struct SignedData
{
	opaque data<>;
};

program FNS_YPUPDATE_PROG {
	version FNS_YPUPDATE_VERS {
		unsigned fnsp_rpc_server_update_nis_database(
		    SignedData) = 1;
	} = 1;
} = 100304;
