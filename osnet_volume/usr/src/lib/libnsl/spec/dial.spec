#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)dial.spec	1.1	99/01/25 SMI"
#
# lib/libnsl/spec/dial.spec

function	dial
include		<dial.h>
declaration	int dial(CALL call)
version		SUNW_0.7
end		

function	undial
include		<dial.h>
declaration	void undial(int fd)
version		SUNW_0.7
end		

