#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)proc.spec	1.3	99/09/06 SMI"
#
# lib/libproc/spec/proc.spec

function	ps_lcontinue
include		<proc_service.h>
declaration	ps_err_e ps_lcontinue(struct ps_prochandle *ph, lwpid_t lwpid)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lgetfpregs
include		<proc_service.h>
declaration	ps_err_e ps_lgetfpregs(struct ps_prochandle *ph, lwpid_t lwpid, prfpregset_t *fpregset)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lgetregs
include		<proc_service.h>
declaration	ps_err_e ps_lgetregs(struct ps_prochandle *ph, lwpid_t lwpid, prgregset_t gregset)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lsetfpregs
include		<proc_service.h>
declaration	ps_err_e ps_lsetfpregs(struct ps_prochandle *ph, lwpid_t lwpid, const prfpregset_t *fpregset)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lsetregs
include		<proc_service.h>
declaration	ps_err_e ps_lsetregs(struct ps_prochandle *ph, lwpid_t lwpid, const prgregset_t gregset)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lstop
include		<proc_service.h>
declaration	ps_err_e ps_lstop(struct ps_prochandle *ph, lwpid_t lwpid)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pauxv
include		<proc_service.h>
declaration	ps_err_e ps_pauxv(struct ps_prochandle *, const auxv_t **)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pcontinue
include		<proc_service.h>
declaration	ps_err_e ps_pcontinue(struct ps_prochandle *ph)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pdmodel
include		<proc_service.h>
declaration	ps_err_e ps_pdmodel(struct ps_prochandle *, int *data_model)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pdread
include		<proc_service.h>
declaration	ps_err_e ps_pdread(struct ps_prochandle *ph, psaddr_t addr, void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pdwrite
include		<proc_service.h>
declaration	ps_err_e ps_pdwrite(struct ps_prochandle *ph, psaddr_t addr, const void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pglobal_lookup
include		<proc_service.h>
declaration	ps_err_e ps_pglobal_lookup(struct ps_prochandle *ph, const char *ld_object_name, const char *ld_symbol_name, psaddr_t *ld_symbol_addr)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pglobal_sym
include		<proc_service.h>
declaration	ps_err_e ps_pglobal_sym(struct ps_prochandle *ph, const char *object_name, const char *sym_name, ps_sym_t *sym)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_plog
include		<proc_service.h>
declaration	void ps_plog(const char *fmt, ...)
version		SUNW_1.1
end

function	ps_pread
include		<proc_service.h>
declaration	ps_err_e ps_pread(struct ps_prochandle *ph, psaddr_t  addr, void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pstop
include		<proc_service.h>
declaration	ps_err_e ps_pstop(struct ps_prochandle *ph)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_ptread
include		<proc_service.h>
declaration	ps_err_e ps_ptread(struct ps_prochandle *ph, psaddr_t addr, void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_ptwrite
include		<proc_service.h>
declaration	ps_err_e ps_ptwrite(struct ps_prochandle *ph, psaddr_t addr, const void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_pwrite
include		<proc_service.h>
declaration	ps_err_e ps_pwrite(struct ps_prochandle *ph, psaddr_t  addr, const void *buf, size_t size)
version		SUNW_1.1
exception	$return != PS_OK
end

function	ps_lgetLDT
include		<proc_service.h>
declaration	ps_err_e ps_lgetLDT(struct ps_prochandle *ph, lwpid_t lwpid, struct ssd *ldt)
arch		i386
version		i386=SUNW_1.1
exception	$return != PS_OK
end

function	ps_lgetxregs
include		<proc_service.h>
declaration	ps_err_e ps_lgetxregs(struct ps_prochandle *ph, lwpid_t lid, caddr_t xregset)
arch		sparc sparcv9
version		sparc=SUNW_1.1 sparcv9=SUNW_1.1
exception	$return != PS_OK
end

function	ps_lgetxregsize
include		<proc_service.h>
declaration	ps_err_e ps_lgetxregsize(struct ps_prochandle *ph, lwpid_t lwpid, int *xregsize)
arch		sparc sparcv9
version		sparc=SUNW_1.1 sparcv9=SUNW_1.1
exception	$return != PS_OK
end

function	ps_lsetxregs
include		<proc_service.h>
declaration	ps_err_e ps_lsetxregs(struct ps_prochandle *ph, lwpid_t lwpid, caddr_t xregset)
arch		sparc sparcv9
version		sparc=SUNW_1.1 sparcv9=SUNW_1.1
exception	$return != PS_OK
end

function	_libproc_debug
version		SUNW_1.1
end

function	Paddr_to_map
version		SUNW_1.1
end

function	Paddr_to_text_map
version		SUNW_1.1
end

function	Pasfd
version		SUNW_1.1
end

function	Pclearfault
version		SUNW_1.1
end

function	Pclearsig
version		SUNW_1.1
end

function	Pcreate
version		SUNW_1.1
end

function	Pcreate_agent
version		SUNW_1.1
end

function	Pcreate_error
version		SUNW_1.1
end

function	Pcred
version		SUNW_1.1
end

function	Pctlfd
version		SUNW_1.1
end

function	Pdelbkpt
version		SUNW_1.1
end

function	Pdestroy_agent
version		SUNW_1.1
end

function	Pexecname
version		SUNW_1.1
end

function	Pfault
version		SUNW_1.1
end

function	Pfgrab_core
version		SUNW_1.1
end

function	Pfree
version		SUNW_1.1
end

function	Pgetareg
version		SUNW_1.1
end

function	Pgetauxval
version		SUNW_1.1
end

function	Pgetenv
version		SUNW_1.1
end

function	Pgrab
version		SUNW_1.1
end

function	Pgrab_core
version		SUNW_1.1
end

function	Pgrab_error
version		SUNW_1.1
end

function	Pisprocdir
version		SUNW_1.1
end

function	Plookup_by_addr
version		SUNW_1.1
end

function	Plookup_by_name
version		SUNW_1.1
end

function	Plwp_getasrs
arch		sparcv9
version		sparcv9=SUNW_1.1
end

function	Plwp_getregs
version		SUNW_1.1
end

function	Plwp_getxregs
arch		sparc sparcv9
version		sparc=SUNW_1.1 sparcv9=SUNW_1.1
end

function	Plwp_getfpregs
version		SUNW_1.1
end

function	Plwp_getpsinfo
version		SUNW_1.1
end

function	Plwp_iter
version		SUNW_1.1
end

function	Plwp_setasrs
arch		sparcv9
version		sparcv9=SUNW_1.1
end

function	Plwp_setfpregs
version		SUNW_1.1
end

function	Plwp_setregs
version		SUNW_1.1
end

function	Plwp_setxregs
arch		sparc sparcv9
version		sparc=SUNW_1.1 sparcv9=SUNW_1.1
end

function	Pmapping_iter
version		SUNW_1.1
end

function	Pname_to_map
version		SUNW_1.1
end

function	Pobject_iter
version		SUNW_1.1
end

function	Pobjname
version		SUNW_1.1
end

function	Pplatform
version		SUNW_1.1
end

function	Ppltdest
version		SUNW_1.1
end

function	Ppsinfo
version		SUNW_1.1
end

function	Pputareg
version		SUNW_1.1
end

function	Prd_agent
version		SUNW_1.1
end

function	Pread
version		SUNW_1.1
end

function	Pread_string
version		SUNW_1.1
end

function	Prelease
version		SUNW_1.1
end

function	Preopen
version		SUNW_1.1
end

function	Preset_maps
version		SUNW_1.1
end

function	Psetbkpt
version		SUNW_1.1
end

function	Psetfault
version		SUNW_1.1
end

function	Psetflags
version		SUNW_1.1
end

function	Psetrun
version		SUNW_1.1
end

function	Psetsignal
version		SUNW_1.1
end

function	Psetsysentry
version		SUNW_1.1
end

function	Psetsysexit
version		SUNW_1.1
end

function	Psignal
version		SUNW_1.1
end

function	Pstack_iter
version		SUNW_1.1
end

function	Pstate
version		SUNW_1.1
end

function	Pstatus
version		SUNW_1.1
end

function	Pstop
version		SUNW_1.1
end

function	Psymbol_iter
version		SUNW_1.1
end

function	Psync
version		SUNW_1.1
end

function	Psyscall
version		SUNW_1.1
end

function	Psysentry
version		SUNW_1.1
end

function	Psysexit
version		SUNW_1.1
end

function	Puname
version		SUNW_1.1
end

function	Punsetflags
version		SUNW_1.1
end

function	Pupdate_maps
version		SUNW_1.1
end

function	Pwait
version		SUNW_1.1
end

function	Pwrite
version		SUNW_1.1
end

function	Pxecbkpt
version		SUNW_1.1
end

function	pr_close
version		SUNW_1.1
end

function	pr_creat
version		SUNW_1.1
end

function	pr_door_info
version		SUNW_1.1
end

function	pr_exit
version		SUNW_1.1
end

function	pr_fcntl
version		SUNW_1.1
end

function	pr_fstat
version		SUNW_1.1
end

function	pr_fstatvfs
version		SUNW_1.1
end

function	pr_getitimer
version		SUNW_1.1
end

function	pr_getpeername
version		SUNW_1.1
end

function	pr_getrlimit
version		SUNW_1.1
end

function	pr_getrlimit64
version		SUNW_1.1
end

function	pr_getsockname
version		SUNW_1.1
end

function	pr_ioctl
version		SUNW_1.1
end

function	pr_link
version		SUNW_1.1
end

function	pr_lseek
version		SUNW_1.1
end

function	pr_llseek
version		SUNW_1.1
end

function	pr_lstat
version		SUNW_1.1
end

function	pr_lwp_exit
version		SUNW_1.1
end

function	pr_memcntl
version		SUNW_1.1
end

function	pr_mmap
version		SUNW_1.1
end

function	pr_munmap
version		SUNW_1.1
end

function	pr_open
version		SUNW_1.1
end

function	pr_rename
version		SUNW_1.1
end

function	pr_setitimer
version		SUNW_1.1
end

function	pr_setrlimit
version		SUNW_1.1
end

function	pr_setrlimit64
version		SUNW_1.1
end

function	pr_sigaction
version		SUNW_1.1
end

function	pr_stat
version		SUNW_1.1
end

function	pr_statvfs
version		SUNW_1.1
end

function	pr_unlink
version		SUNW_1.1
end

function	pr_waitid
version		SUNW_1.1
end

function	pr_zmap
version		SUNW_1.1
end

function	proc_arg_grab
version		SUNW_1.1
end

function	proc_arg_psinfo
version		SUNW_1.1
end

function	proc_dirname
version		SUNW_1.1
end

function	proc_fltname
version		SUNW_1.1
end

function	proc_get_auxv
version		SUNW_1.1
end

function	proc_get_cred
version		SUNW_1.1
end

function	proc_get_psinfo
version		SUNW_1.1
end

function	proc_get_status
version		SUNW_1.1
end

function	proc_signame
version		SUNW_1.1
end

function	proc_sysname
version		SUNW_1.1
end

function	proc_unctrl_psinfo
version		SUNW_1.1
end
