#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)data.spec	1.2	99/05/04 SMI"
#
# lib/libc/spec/data.spec

data		__ctype extends libc/spec/sys.spec _ctype
weak		_ctype 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		__huge_val 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		__iob #extends libc/spec/stdio.spec _iob
weak		_iob 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		__loc1 
version		SUNW_1.1
end

data		__xpg4 
version		SUNW_0.8
end

data		_altzone extends libc/spec/sys.spec altzone
weak		altzone 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		_bufendtab 
arch		sparc	i386
version		SUNW_0.7
end

data		_daylight extends libc/spec/sys.spec daylight
weak		daylight 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		_environ extends libc/spec/sys.spec environ
weak		environ 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		_iob 
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		_lastbuf 
version		i386=SUNW_0.7 sparc=SUNW_0.7
end

data		_numeric 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		_sibuf 
version		SUNW_0.7
end

data		_sobuf 
version		SUNW_0.7
end

data		_sys_buslist 
version		SUNW_0.7
end

data		_sys_cldlist 
version		SUNW_0.7
end

data		_sys_fpelist 
version		SUNW_0.7
end

data		_sys_illlist 
version		SUNW_0.7
end

data		_sys_nsig 
arch		sparc	i386
version		SUNW_0.7
end

data		_sys_segvlist 
version		SUNW_0.7
end

data		_sys_siginfolistp 
version		SUNW_0.7
end

data		_sys_siglist 
version		SUNW_0.7
end

data		_sys_siglistn 
version		SUNW_0.7
end

data		_sys_siglistp 
version		SUNW_0.7
end

data		_sys_traplist 
version		SUNW_0.7
end

data		_timezone extends  libc/spec/sys.spec timezone
weak		timezone 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		_tzname extends  libc/spec/sys.spec tzname
weak		tzname 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		errno 
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		getdate_err 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

function	_getdate_err
weak		getdate_err
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

data		nss_default_finders 
version		SUNW_0.7 
end

data		optarg 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		opterr 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		optind 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		optopt 
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
end

data		sys_errlist 
arch		i386 sparc
version		SUNW_0.7 
end

data		sys_nerr 
arch		i386 sparc
version		SUNW_0.7 
end

