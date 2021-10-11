#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)malloc.spec	1.1	99/01/25 SMI"
#
# lib/libmalloc/spec/malloc.spec

function	free	extends		libc/spec/gen.spec free
version		SUNW_1.1
end		

function	calloc	extends		libc/spec/gen.spec calloc
version		SUNW_1.1
end		

function	malloc	extends		libc/spec/gen.spec malloc
version		SUNW_1.1
end		

function	realloc	extends		libc/spec/gen.spec realloc
version		SUNW_1.1
end		

function	mallopt
include		<malloc.h>
declaration	int mallopt(int cmd, int value)
version		SUNW_1.1
exception	$return != 0
end		

function	_mallopt
weak		mallopt
version		SUNW_1.1
end		

function	mallinfo
include		<malloc.h>
declaration	struct mallinfo mallinfo(void)
version		SUNW_1.1
end		

function	_mallinfo
weak		mallinfo
version		SUNW_1.1
end		

function	cfree
declaration	void cfree(char *p, unsigned int num, unsigned int size)
version		SUNW_1.1
end		

function	_cfree
weak		cfree
version		SUNW_1.1
end		

