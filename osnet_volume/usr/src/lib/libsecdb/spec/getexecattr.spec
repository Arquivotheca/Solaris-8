#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getexecattr.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/getexecattr.spec

function	getexecattr
include		<exec_attr.h>
declaration	execattr_t *getexecattr()
version		SUNW_1.1
exception	$return == NULL
end

function	getexecprof
include		<exec_attr.h>
declaration	execattr_t *getexecprof(const char *name, const char *type, \
	const char *id, int search_flag)
version		SUNW_1.1
exception	$return == NULL
end

function	getexecuser
include		<exec_attr.h>
declaration	execattr_t *getexecuser(const char *username, const char *type,\
	const char *id, int search_flag)
version		SUNW_1.1
exception	$return == NULL
end

function	match_execattr
include		<exec_attr.h>
declaration	execattr_t *match_execattr(execattr_t *exec, \
	const char *profname, const char *type, const char *id)
version		SUNW_1.1
exception	$return == NULL
end

function	setexecattr
include		<exec_attr.h>
declaration	void setexecattr()
version		SUNW_1.1
end

function	endexecattr
include		<exec_attr.h>
declaration	void endexecattr()
version		SUNW_1.1
end

function	free_execattr
include		<exec_attr.h>
declaration	void free_execattr(execattr_t *exec)
version		SUNW_1.1
end
