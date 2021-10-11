#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getuserattr.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/getuserattr.spec

function	getuserattr
include		<user_attr.h>
declaration	userattr_t *getuserattr()
version		SUNW_1.1
exception	$return == NULL
end

function	fgetuserattr
include		<user_attr.h>
declaration	userattr_t *fgetuserattr(FILE *f)
version		SUNW_1.1
exception	$return == NULL
end

function	getusernam
include		<user_attr.h>
declaration	userattr_t *getusernam(const char *name)
version		SUNW_1.1
exception	$return == NULL
end

function	getuseruid
include		<user_attr.h>
declaration	userattr_t *getuseruid(uid_t u)
version		SUNW_1.1
exception	$return == NULL
end

function	setuserattr
include		<user_attr.h>
declaration	void setuserattr()
version		SUNW_1.1
end

function	enduserattr
include		<user_attr.h>
declaration	void enduserattr()
version		SUNW_1.1
end

function	free_userattr
include		<user_attr.h>
declaration	void free_userattr(userattr_t *user)
version		SUNW_1.1
end
