#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauthattr.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/getauthattr.spec

function	getauthattr
include		<auth_attr.h>
declaration	authattr_t *getauthattr()
version		SUNW_1.1
exception	$return == NULL
end

function	getauthnam
include		<auth_attr.h>
declaration	authattr_t *getauthnam(const char *name)
version		SUNW_1.1
exception	$return == NULL
end

function	setauthattr
include		<auth_attr.h>
declaration	void setauthattr()
version		SUNW_1.1
end

function	endauthattr
include		<auth_attr.h>
declaration	void endauthattr()
version		SUNW_1.1
end

function	free_authattr
include		<auth_attr.h>
declaration	void free_authattr(authattr_t *auth)
version		SUNW_1.1
end
