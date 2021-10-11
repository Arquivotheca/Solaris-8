#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)audit.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/audit.spec

function	audit
include		<sys/param.h>, <bsm/audit.h>
declaration	int audit(	caddr_t	record,	int length)
version		SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return == -1
end		

