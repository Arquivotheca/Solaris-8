#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rtld.chk.msg	1.1	99/06/04 SMI"

@ _START_

# Messages to satisfy chkmsg target.  Numerous messages within common code
# aren't required for each target, but chkmsg has no way of dealing with
# ifdefs, thus this file exists to satisfy common code messages.

@ _END_

@ MSG_FIL_IA64_LDPROF	"/usr/lib/link_audit/ia64/ldprof.so.1"
@ MSG_FIL_SP64_LDPROF	"/usr/lib/link_audit/sparcv9/ldprof.so.1"

@ MSG_LD_LIBPATH_64	"LIBRARY_PATH_64="

@ MSG_PTH_SP64_RTLD	"/usr/lib/sparcv9/ld.so.1"
@ MSG_PTH_IA64_RTLD	"/usr/lib/ia64/ld.so.1"
@ MSG_PTH_SP64_USRLIB	"/usr/lib/sparcv9"
@ MSG_PTH_IA64_USRLIB	"/usr/lib/ia64"
@ MSG_PTH_SP64_USRLIBSE	"/usr/lib/secure/sparcv9"
@ MSG_PTH_IA64_USRLIBSE	"/usr/lib/secure/ia64"

@ MSG_SYM_LAV8PLTENTER	"la_sparcv8_pltenter"
@ MSG_SYM_LAV9PLTENTER	"la_sparcv9_pltenter"
@ MSG_SYM_LAX86PLTENTER	"la_i86_pltenter"

@ MSG_SYM_LAPLTEXIT	"la_pltexit"
@ MSG_SYM_LAPLTEXIT64	"la_pltexit64"
@ MSG_SYM_LASYMBIND	"la_symbind"
@ MSG_SYM_LASYMBIND64	"la_symbind64"
