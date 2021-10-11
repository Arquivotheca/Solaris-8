#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)kvm.spec	1.2	99/05/14 SMI"
#
# lib/libkvm/spec/kvm.spec
#
# NOTE on SUNW_1.1 interfaces:
#   The following symbols are all UNCOMMITTED and documented in section 3K.
#
#   The UNCOMMITTED classification is due to the fact that there is almost
#   nothing you can put as a symbol in a namelist which has any form of
#   release to release stability.  The syntax of these routines is actually
#   pretty stable, but being UNCOMMITTED, the door is always open for change.
#
# NOTE on SUNprivate_1.1 interfaces:
#
# The [private] symbol[s] [are] an unofficial private interface between
# the crash command and libkvm.  The interface classification level must
# be consolidation private (or more restrictive).
#

function	kvm_getu
include		<kvm.h>, <sys/param.h>, <sys/user.h>, <sys/proc.h>
declaration	user_t *kvm_getu(kvm_t *kd, proc_t *proc)
version		SUNW_1.1
exception	( $return == 0 )
end

function	kvm_getcmd
include		<kvm.h>, <sys/param.h>, <sys/user.h>, <sys/proc.h>
declaration	int kvm_getcmd(kvm_t *kd, proc_t *proc, user_t *u, \
			char ***arg, char ***env)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_getproc
include		<kvm.h>, <sys/param.h>, <sys/time.h>, <sys/proc.h>
declaration	proc_t *kvm_getproc(kvm_t *kd, pid_t pid)
version		SUNW_1.1
exception	( $return == 0 )
end

function	kvm_nextproc
include		<kvm.h>, <sys/param.h>, <sys/time.h>, <sys/proc.h>
declaration	proc_t *kvm_nextproc(kvm_t *kd)
version		SUNW_1.1
exception	( $return == 0 )
end

function	kvm_setproc
include		<kvm.h>, <sys/param.h>, <sys/time.h>, <sys/proc.h>
declaration	int kvm_setproc (kvm_t *kd)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_nlist
include		<kvm.h>, <nlist.h>
declaration	int kvm_nlist(kvm_t *kd, struct nlist *nl)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_open
include		<kvm.h>, <fcntl.h>
declaration	kvm_t *kvm_open(const char *namelist, const char *corefile, \
			const char *swapfile, int flag, const char *errstr)
version		SUNW_1.1
exception	( $return == 0 )
end

function	kvm_close
include		<kvm.h>, <fcntl.h>
declaration	int kvm_close(kvm_t *kd)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_read
include		<kvm.h>
declaration	ssize_t kvm_read(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_write
include		<kvm.h>
declaration	ssize_t kvm_write(kvm_t *kd, uintptr_t addr, const void *buf, \
			size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_uread
include		<kvm.h>
declaration	ssize_t kvm_uread(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_uwrite
include		<kvm.h>
declaration	ssize_t kvm_uwrite(kvm_t *kd, uintptr_t addr, const void *buf, \
			size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_kread
include		<kvm.h>
declaration	ssize_t kvm_kread(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_kwrite
include		<kvm.h>
declaration	ssize_t kvm_kwrite(kvm_t *kd, uintptr_t addr, const void *buf, size_t nbytes)
version		SUNW_1.1
exception	( $return == -1 )
end

function	kvm_physaddr
declaration	uint64_t kvm_physaddr(kvm_t *kd, struct as *as, uintptr_t vaddr)
version		SUNWprivate_1.1
end

function	kvm_aread
declaration	ssize_t kvm_aread(kvm_t *kd, uintptr_t addr, void *buf, \
			size_t size, struct as *as)
version		SUNWprivate_1.1
end

function	kvm_awrite
declaration	ssize_t kvm_awrite(kvm_t *kd, uintptr_t addr, \
			const void *buf, size_t size, struct as *as)
version		SUNWprivate_1.1
end

function	kvm_pread
declaration	ssize_t kvm_pread(kvm_t *kd, uint64_t addr, void *buf, \
			size_t size)
version		SUNWprivate_1.1
end

function	kvm_pwrite
declaration	ssize_t kvm_pwrite(kvm_t *kd, uint64_t addr, \
			const void *buf, size_t size)
version		SUNWprivate_1.1
end
