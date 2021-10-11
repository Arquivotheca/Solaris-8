#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)privatedata.spec	1.4	99/12/04 SMI"
#
# lib/libc/spec/privatedata.spec

data		___Argv 
version		SUNWprivate_1.1
end		

data		__ctype_mask 
version		SUNWprivate_1.1
end		

data		__door_create_pid 
arch		i386 ia64 sparc sparcv9
version		SUNWprivate_1.1
end		

data		__door_server_func 
arch		i386 ia64 sparc sparcv9
version		SUNWprivate_1.1
end		

data		__environ_lock 
version		SUNWprivate_1.1
end		

data		__i_size 
version		SUNWprivate_1.1
end		

data		__inf_read 
version		SUNWprivate_1.1
end		

data		__inf_written 
version		SUNWprivate_1.1
end		

data		__lc_charmap 
version		SUNWprivate_1.1
end		

data		__lc_collate 
version		SUNWprivate_1.1
end		

data		__lc_ctype 
version		SUNWprivate_1.1
end		

data		__lc_locale 
version		SUNWprivate_1.1
end		

data		__lc_messages 
version		SUNWprivate_1.1
end		

data		__lc_monetary 
version		SUNWprivate_1.1
end		

data		__lc_numeric 
version		SUNWprivate_1.1
end		

data		__lc_time 
version		SUNWprivate_1.1
end		

data		__lyday_to_month 
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end		

data		__malloc_lock 
version		SUNWprivate_1.1
end		

data		__mon_lengths 
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end		

data		__nan_read 
version		SUNWprivate_1.1
end		

data		__nan_written 
version		SUNWprivate_1.1
end		

data		__thr_door_server_func 
arch		i386 ia64 sparc sparcv9
version		SUNWprivate_1.1
end		

data		__threaded 
version		SUNWprivate_1.1
end		

data		__trans_lower 
version		SUNWprivate_1.1
end		

data		__trans_upper 
version		SUNWprivate_1.1
end		

data		__yday_to_month 
version		sparc=SUNWprivate_1.1 sparcv9=SUNWprivate_1.1
end		

data		_cswidth 
version		SUNWprivate_1.1
end		

data		_lflag 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_lib_version 
version		SUNWprivate_1.1
end		

data		_locale_lock 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_lone extends libc/spec/sys.spec lone
weak		lone 
version		SUNWprivate_1.1 
end		

data		_lten extends libc/spec/sys.spec lten
weak		lten 
version		SUNWprivate_1.1 
end		

data		_lzero extends libc/spec/sys.spec lzero
weak		lzero 
version		SUNWprivate_1.1 
end		

data		_nss_default_finders 
weak		nss_default_finders
version		SUNWprivate_1.1 
end		

data		_siguhandler 
version		SUNWprivate_1.1
end		

data		_smbuf 
version		SUNWprivate_1.1
end		

data		_sp 
version		SUNWprivate_1.1
end		

data		_libc_tsd_common
version		SUNWprivate_1.1
end

data		_sys_errlist 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_sys_errs 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_sys_index 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_sys_nerr 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_sys_num_err 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

data		_wcptr 
version		i386=SUNWprivate_1.1 sparc=SUNWprivate_1.1
end		

# Bugid 4296198, had to move these from libnsl/nis/cache/cache_api.cc BEGIN

data		__nis_debug_bind
version		SUNWprivate_1.1
end

data		__nis_debug_calls
version		SUNWprivate_1.1
end

data		__nis_debug_file
version		SUNWprivate_1.1
end

data		__nis_debug_rpc
version		SUNWprivate_1.1
end

data		__nis_prefsrv
version		SUNWprivate_1.1
end

data		__nis_preftype
version		SUNWprivate_1.1
end

data		__nis_server
version		SUNWprivate_1.1
end

# Bugid 4296198, had to move these from libnsl/nis/cache/cache_api.cc END
