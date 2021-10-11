#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)catalog.awk	1.6	99/06/23 SMI"

#
# Extract MACROs from .msg file
# The MACROs which are referenced by MSG_INTL() go to CATA_MSG_INTL_LIST
# The MACROs which are referenced by MSG_ORIG() go to CATA_MSG_ORIG_LIST
#

BEGIN {
	# skip == 0
	#	The MACRO will not be recorded
	skip = 0

	# which == 0
	#	Collecting MACRO's in between _START_ and _END_
	# which == 1
	#	Collecting MACRO's in after _END_
	which = 0
}

#
# If the MACROs are surrounded by _CHKMSG_SKIP_BEGIN_ and
# _CHKMSG_SKIP_END_, these MACRO will not be recorded for checking.
# It is assumed that the use of MACRO are checked by developers.
#
/_CHKMSG_SKIP_BEGIN_/ {
	if ($3 == mach)
		skip = 1
}
/_CHKMSG_SKIP_END_/ {
	if ($3 == mach)
		skip = 0
}

/^@/ {
	dontprint = 0

	if ($2 == "_START_") {
		which = 0
		dontprint = 1
	} else if ($2 == "_END_") {
		which = 1
		dontprint = 1
	} else if (match($2, "MSG_ID_") != 0) {
		dontprint = 1
	}

	if (skip == 1 || dontprint == 1)
		next

	if (which == 0)
		print $2 >> "CATA_MSG_INTL_LIST"
	else
		print $2 >> "CATA_MSG_ORIG_LIST"
}
