#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)libld.sparc64chk.msg	1.1	99/06/04 SMI"

@ _START_

# Messages to satisfy chkmsg target.  Numerous messages within common code
# aren't required for each target, but chkmsg has no way of dealing with
# ifdefs, thus this file exists to satisfy common code messages.

@ MSG_REL_PICREDLOC	"relocation error: -z redlocsym may not be used for \
			 pic code.\n\t\
			 symbol %s: input file %s: relocation %s"

@ _END_
