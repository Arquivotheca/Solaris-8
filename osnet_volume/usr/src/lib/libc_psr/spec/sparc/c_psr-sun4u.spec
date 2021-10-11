#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)c_psr-sun4u.spec	1.1	99/01/25 SMI"
#
# lib/libc_psr/spec/sparc/c_psr-sun4u.spec

function	memcmp extends libc/spec/gen.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	_memcmp extends libc/spec/private.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	memcpy extends libc/spec/gen.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	_memcpy extends libc/spec/private.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	memmove extends libc/spec/gen.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	_memmove extends libc/spec/private.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	memset extends libc/spec/gen.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	_memset extends libc/spec/private.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__umul64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__mul64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__rem64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__urem64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__div64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

function	__udiv64 extends libc/spec/sys.spec
arch		sparc
version		SUNWprivate_1.1
end		

