#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)chkauthattr.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/chkauthattr.spec

function	chkauthattr
include		<auth_attr.h>, <prof_attr.h>, <user_attr.h>
declaration	int chkauthattr(const char *authname, const char *username)
version		SUNW_1.1
exception	$return == 0
end		
