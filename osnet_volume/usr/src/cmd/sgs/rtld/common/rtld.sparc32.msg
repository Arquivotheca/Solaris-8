#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rtld.sparc32.msg	1.1	99/06/04 SMI"

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1) - 32bit sparc specific.

# General error diagnostics (a.out)

@ MSG_REL_OVERFLOW      "relocation error: file %s: symbol %s: \
			 value 0x%x overflows %d bits at offset 0x%x"

@ MSG_GEN_OLDREV	"%s: has older revision than expected %s"

@ _END_

@ MSG_STR_AOUT		"AOUT"

@ MSG_FMT_4XPATH	"./%s"
@ MSG_FMT_4XLIB		"lib%s.so.%d.%d"

@ MSG_FIL_DOTSODOT	".so."
@ MSG_FIL_LIB		"lib"

@ MSG_PTH_USRLCLIB	"/usr/local/lib"
@ MSG_PTH_USR4LIB	"/usr/4lib"
@ MSG_PTH_USRUCBLIB	"/usr/ucblib"

@ MSG_SYM_LAV8PLTENTER	"la_sparcv8_pltenter"
