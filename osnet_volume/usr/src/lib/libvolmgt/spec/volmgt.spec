#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)volmgt.spec	1.1	99/01/25 SMI"
#
# lib/libvolmgt/spec/volmgt.spec

function	media_getid
version		SUNW_0.7
end		

function	volmgt_acquire
version		SUNW_1.1
end		

function	volmgt_feature_enabled
version		SUNW_1.1
end		

function	volmgt_ownspath
version		SUNW_0.7
end		

function	volmgt_release
version		SUNW_1.1
end		

function	media_findname
include		<volmgt.h>
declaration	char *media_findname(char *start)
version		SUNW_0.7
errno		ENXIO
exception	$return == 0
end		

function	media_getattr
declaration	char *media_getattr(char *vol_path, char *attr)
version		SUNW_0.7
errno		ENXIO EINTR
exception	$return == 0
end		

function	media_setattr
declaration	int media_setattr(char *vol_path, char *attr, char *value)
version		SUNW_0.7
errno		ENXIO EINTR
exception	$return == 0
end		

function	volmgt_check
include		<volmgt.h>
declaration	int volmgt_check(char *pathname)
version		SUNW_0.7
errno		ENXIO EINTR 
exception	$return == 0
end		

function	volmgt_inuse
include		<volmgt.h>
declaration	int volmgt_inuse(char *pathname)
version		SUNW_0.7
errno		ENXIO EINTR 
exception	$return == 0
end		

function	volmgt_root
declaration	const char *volmgt_root(void)
version		SUNW_0.7
exception	$return == 0
end		

function	volmgt_running
include		<volmgt.h>
declaration	int volmgt_running(void)
version		SUNW_0.7
errno		ENXIO EINTR 
exception	$return == 0
end		

function	volmgt_symname
include		<volmgt.h>
declaration	char *volmgt_symname(char *pathname)
version		SUNW_0.7
errno		ENXIO EINTR
exception	$return != 0
end		

function	volmgt_symdev
include		<volmgt.h>
declaration	char *volmgt_symdev(char *symname)
version		SUNW_0.7
errno		ENXIO EINTR
exception	$return != 0
end		

function	_dev_mounted
version		SUNWprivate_1.1
end		

function	_dev_unmount
version		SUNWprivate_1.1
end		

function	_media_oldaliases
version		SUNWprivate_1.1
end		

function	_media_printaliases
version		SUNWprivate_1.1
end		

