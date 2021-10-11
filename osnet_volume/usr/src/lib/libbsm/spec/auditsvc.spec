#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)auditsvc.spec	1.1	99/01/25 SMI"
#
# lib/libbsm/spec/auditsvc.spec

function	auditsvc
include		<sys/param.h>, <bsm/audit.h>
declaration	int auditsvc( int fd, int limit)
version		SUNW_0.7
errno		EAGAIN EBADF EBUSY EFBIG EINTR EINVAL EIO ENOSPC \
			ENXIO EPERM EWOULDBLOCK
exception	$return == -1
end		

