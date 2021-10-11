#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)auditon.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/auditon.spec

function	auditon
include		<sys/param.h>, <bsm/audit.h>
declaration	int auditon(int cmd, caddr_t data,	int length)
version		SUNW_0.7
errno		EFAULT EINVAL EPERM
exception	$return == -1
end		

