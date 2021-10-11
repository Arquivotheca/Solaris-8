#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rtld.sparc.msg	1.2	99/07/25 SMI"

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1) - sparc specific.

@ MSG_REL_UNIMPL	"relocation error: file %s: symbol %s: \
			 unimplemented relocation type: %d"
@ MSG_REL_NONALIGN	"relocation error: %s: file %s: symbol %s: \
			 offset 0x%llx is non-aligned"
@ MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s: \
			 unsupported number of bits: %d"
@ MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx looses %d bits at offset 0x%llx"
@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx does not fit"
@ MSG_REL_BADREG	"relocation error: file %s: REGISTER relocation \
			 against bad register offset: 0x%llx"
@ MSG_REL_PLTOFF	"non-aligned .plt offset"


@ MSG_REG_SYMCONF	"register symbol conflict: %s: \
			 file %s symbol %s; file %s symbol %s"

@ MSG_LDD_REG_SYMCONF	"\tregister symbol conflict: %s\n\
			 \t\t(file %s symbol %s; file %s symbol %s)\n"

@ _END_
