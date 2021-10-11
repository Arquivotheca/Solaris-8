#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)mtmalloc.spec	1.1	99/01/25 SMI"
#
# lib/libmtmalloc/spec/mtmalloc.spec

function	free extends libc/spec/gen.spec free
version		SUNW_1.1
end		

function	malloc extends libc/spec/gen.spec malloc
version		SUNW_1.1
end		

function	realloc extends libc/spec/gen.spec realloc
version		SUNW_1.1
end		

function	mallocctl
declaration	void mallocctl(int cmd, long value)
version		SUNW_1.1
end		

