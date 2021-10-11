#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)libcrle.msg	1.1	99/08/13 SMI"


@ _START_

# Messages for cmd/sgs/libcrle

@ MSG_ID_LIBCRLE

# System error messages

@ MSG_SYS_OPEN		"%s: %s: open failed: %s\n"
@ MSG_SYS_READ		"%s: %s: read failed: %s\n"
@ MSG_SYS_MMAP		"%s: mmap failed: %s\n"
@ MSG_SYS_ALLOC		"%s: memory allocation failed: %s\n"

# Runtime linker error messages

@ MSG_DL_OPEN		"%s: dlopen failed: %s\n"
@ MSG_DL_DUMP		"%s: dldump failed: %s\n"
@ MSG_DL_INFO		"%s: dlinfo failed: %s\n"

@ _END_


# The following strings represent reserved section and symbol names.  Reference
# to these strings is via the MSG_ORIG() macro, and thus no translations are
# required.

@ MSG_AUD_PRF		"__CRLE_DEP__:%s\n"
@ MSG_AUD_RESBGN	"__CRLE_DEP__:resbgn:%llu\n"
@ MSG_AUD_RESEND	"__CRLE_DEP__:resend:%llu\n"

@ MSG_ENV_AUD_FD	"CRLE_FD"
@ MSG_ENV_AUD_FLAGS	"CRLE_FLAGS"

@ MSG_FIL_LIBCRLE	"libcrle"

@ MSG_PTH_PROCRMAP	"/proc/self/rmap"
@ MSG_PTH_DEVNULL	"/dev/null"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
