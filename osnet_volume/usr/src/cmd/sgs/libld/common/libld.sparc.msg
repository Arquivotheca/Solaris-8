#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)libld.sparc.msg	1.1	99/06/04 SMI"

@ _START_

# Message file for cmd/sgs/libld - sparc specific.

@ MSG_REL_UNIMPL	"relocation error: file %s:  symbol %s: \
			 unimplemented relocation type: %d"
@ MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s: \
			 unsupported number of bits: %d"
@ MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx looses %d bits at offset 0x%llx\n"
@ MSG_REL_UNRELREL	"relocation error: %s: file %s: symbol %s: \
			 non-relative PLT relocation can't be used in \
			 building a shared object"

@ MSG_REL_ASSIGNGOT	"internal error: assign_got() invalid got index \
			 0x%llx for symbol `%s'"
@ MSG_REL_SMALLGOT	"too many symbols require `small' PIC references:\n\
			 \thave %d, maximum %d -- recompile some \
			 modules -K PIC."

@ MSG_SYM_INCOMPREG	"register symbols used incompatibly, symbol `%s' \
			 reg: %s in file `%s'\n\
			 \tincompatible with symbol `%s' reg: %s in file `%s'"
@ MSG_SYM_MULTINIREG	"register symbol multiply initialized, symbol `%s' \
			 reg: %s in file `%s'\n\
			 \tincompatible with symbol `%s' reg: %s in file `%s'"
@ MSG_SYM_BADSCRATCH	"malformed scratch register symbol reg: %s in file `%s'"
@ MSG_SYM_BADREG	"invalid register symbol reg: %s in file `%s'"

@ _END_
