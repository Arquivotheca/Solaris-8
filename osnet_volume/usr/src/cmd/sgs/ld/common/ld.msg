#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)ld.msg	1.36	99/09/14 SMI"

@ _START_

# Message file for cmd/sgs/ld.

@ MSG_ID_LD

# System error messages

@ MSG_SYS_OPEN		"file %s: open failed: %s"
@ MSG_SYS_MMAP		"file %s: cannot mmap file: %s"

@ MSG_LIB_DLSYM		"dlsym() failed for libld.so(ld_main)"


# Debugging messages

@ MSG_DBG_FMT		"debug: "

# Generic error diagnostic labels

@ MSG_ERR_WARNING	"warning: "
@ MSG_ERR_FATAL		"fatal: "
@ MSG_ERR_ELF		"elf error: "

@ _END_


@ MSG_LD_MAIN		"ld_main"
@ MSG_LD_LIB32		"libld.so.2"
@ MSG_LD_LIB64		"libld.so.3"


# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_NL		"\n"
@ MSG_STR_LDDIAG	"ld: "
@ MSG_STR_ELFDIAG	": %s"

@ MSG_STR_OPTIONS	"abc:d:e:f:h:il:mo:p:rstu:z:B:D:F:GI:L:M:N:P:Q:R:S:VY:?"

@ MSG_LD_OPTIONS	"LD_OPTIONS"

# Stuff from libld.msg
@ MSG_PTH_DEVZERO	"/dev/zero"


@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
