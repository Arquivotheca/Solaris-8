#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)weak.spec	1.1	99/01/25 SMI"
#
# cmd/sgs/libdl/spec/weak.spec

function	_dlinfo
weak		dlinfo
version		SUNWprivate_1.1
end		

function	_dldump
weak		dldump
version		SUNWprivate_1.1
end		

function	_dlmopen
weak		dlmopen
version		SUNWprivate_1.1
end		

function	_dlopen
weak		dlopen
version		SUNWprivate_1.1
end		

function	_dlerror
weak		dlerror
version		SUNWprivate_1.1
end		

function	_dlsym
weak		dlsym
version		SUNWprivate_1.1
end		

function	_dlclose
weak		dlclose
version		SUNWprivate_1.1
end		

function	_dladdr
weak		dladdr
version		SUNWprivate_1.1
end		

