#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getprofattr.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/getprofattr.spec

function	getprofattr
include		<prof_attr.h>
declaration	profattr_t *getprofattr()
version		SUNW_1.1
exception	$return == NULL
end

function	getprofnam
include		<prof_attr.h>
declaration	profattr_t *getprofnam(const char *name)
version		SUNW_1.1
exception	$return == NULL
end

function	setprofattr
include		<prof_attr.h>
declaration	void setprofattr()
version		SUNW_1.1
end

function	endprofattr
include		<prof_attr.h>
declaration	void endprofattr()
version		SUNW_1.1
end

function	free_profattr
include		<prof_attr.h>
declaration	void free_profattr(profattr_t *prof)
version		SUNW_1.1
end
