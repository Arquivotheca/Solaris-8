#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)dlfunc.spec	1.3	99/10/07 SMI"
#
# cmd/sgs/libdl/spec/dlfunc.spec

function	dladdr
include		<dlfcn.h>
declaration	int dladdr(void *address, Dl_info *dlip)
version		SUNW_0.8
exception	$return == 0
end		

function	dlclose
include		<dlfcn.h>
declaration	int dlclose(void *handle)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return != 0
end		

function	dldump
include		<dlfcn.h>
declaration	int dldump(const char *ipath, const char *opath, int flags)
version		SUNW_1.1
exception	$return != 0
end		

function	dlerror
include		<dlfcn.h>
declaration	char *dlerror(void)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == NULL
end		

function	dlopen
include		<dlfcn.h>
declaration	void * dlopen(const char *pathname, int mode)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == NULL
end		

function	dlsym
include		<dlfcn.h>
declaration	void *dlsym(void *handle, const char *name)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == NULL
end		

function	dlinfo
include		<dlfcn.h>
declaration	int dlinfo(void *handle, int request, void *p)
version		SUNW_1.1
exception	$return < 0
end		

function	dlmopen
include		<dlfcn.h>
declaration	void * dlmopen(Lmid_t lmid, const char *pathname, int mode)
version		SUNW_1.1
exception	$return == NULL
end		

function	_ld_concurrency
version		SUNWprivate_1.1
end		

function	_ld_libc
version		SUNWprivate_1.1
end		

