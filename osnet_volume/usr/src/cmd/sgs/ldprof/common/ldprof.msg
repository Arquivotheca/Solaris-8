#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)ldprof.msg	1.6	98/01/20 SMI"

@ _START_

# Message file for cmd/sgs/ldprof

@ MSG_ID_LIBPROF


# System call messages.

@ MSG_SYS_OPEN		"%s: open failed: %s\n"
@ MSG_SYS_MMAP		"%s: mmap failed: %s\n"
@ MSG_SYS_FTRUNC	"%s: ftruncate failed: %s\n"

# General error diagnostics
@ MSG_GEN_PROFSZCHG	"ldprof.so: %s: file being profiled has changed size;\n\
			 \trecreate profile buffer\n"
@ MSG_GEN_AUDITVERSION	"ldprof.so: audit version mismatch: expected %d \
			 but found %d\n"
@ MSG_GEN_PROFNOTSET	"ldprof.so: environment variable LD_PROFILE not set, \
			 unable to profile\n"

@ _END_

# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"

@ MSG_FMT_PROFILE	"%s/%s%s"
@ MSG_SUF_PROFILE	".profile"
@ MSG_SUF_PROFILE_64	".profilex"
@ MSG_LD_PROFOUT	"LD_PROFILE_OUTPUT"
@ MSG_LD_PROFILE	"LD_PROFILE"

@ MSG_PTH_VARTMP	"/var/tmp"
