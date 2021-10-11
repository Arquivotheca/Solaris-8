#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)libld.intel64chk.msg	1.1	99/06/04 SMI"

@ _START_

# Messages to satisfy chkmsg target.  Numerous messages within common code
# aren't required for each target, but chkmsg has no way of dealing with
# ifdefs, thus this file exists to satisfy common code messages.

@ MSG_REL_LOOSEBITS	"relocation error: %s: file %s: symbol %s: \
@ MSG_REL_NOREG		"relocation error: REGISTER relocation not supported \
@ MSG_REL_UNIMPL	"relocation error: file %s:  symbol %s: \
@ MSG_REL_UNNOBITS	"relocation error: %s: file %s: symbol %s: \

@ MSG_REL_ERR_STR	"relocation error: %s:"
@ MSG_REL_ERR_FILE	" file %s: "
@ MSG_REL_ERR_SYM	" symbol %s: "

@ _END_
