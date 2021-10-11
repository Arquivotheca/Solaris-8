#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)mapmalloc.spec	1.1	99/01/25 SMI"
#
# lib/libmapmalloc/spec/mapmalloc.spec

function	mallopt
include		<stdlib.h>, <malloc.h>
declaration	int mallopt(int cmd, int value )
version		SUNW_0.7
errno		ENOMEM EAGAIN
exception	$return != 0
end		

function	mallinfo
include		<stdlib.h>, <malloc.h>
declaration	struct mallinfo mallinfo(void )
version		SUNW_0.7
errno		ENOMEM EAGAIN
end		

function	malloc	extends	libc/spec/gen.spec	malloc
version		SUNW_0.7
end		

function	calloc	extends	libc/spec/gen.spec	calloc
version		SUNW_0.7
end		

function	free	extends	libc/spec/gen.spec	free
version		SUNW_0.7
end		

function	memalign	extends	libc/spec/gen.spec	memalign
version		SUNW_0.7
end		

function	realloc	extends	libc/spec/gen.spec	realloc
version		SUNW_0.7
end		

function	valloc	extends	libc/spec/gen.spec	valloc
version		SUNW_0.7
end		

function	cfree	extends	libc/spec/gen.spec	cfree
version		SUNW_0.7
end		

# required by sbcp.
function	__mallinfo
version		SUNWprivate_1.1
end		

