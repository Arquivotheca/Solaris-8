#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)getauid.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/getauid.spec

function	getauid
include		<sys/param.h>, <bsm/audit.h>
declaration	int getauid( au_id_t *auid)
version		SUNW_0.7
errno		EFAULT EPERM
exception	$return == -1
end		

function	setauid
include		<sys/param.h>, <bsm/audit.h>
declaration	int setauid( au_id_t *auid)
version		SUNW_0.7
errno		EFAULT EPERM
exception	$return == -1
end		

