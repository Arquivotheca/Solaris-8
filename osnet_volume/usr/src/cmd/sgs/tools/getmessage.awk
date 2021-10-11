#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# ident	"@(#)getmessage.awk	1.3	97/04/08 SMI"
#

#
# Extract MACROs referenced by MSG_INTL and MSG_ORIG
#	The MACROS referenced by MSG_INTL() go to MSG_INTL_LIST
#	The MACROS referenced by MSG_ORIG() go to MSG_ORIG_LIST
#

BEGIN {
	FS = "[,(){]|[ ]+|[\t]+"

	# These variables are used to handle the lines such as:
	#		MSG_INTL(
	#		MSG_FORMAT);
	watchme_intl = 0
	watchme_orig = 0
}

#
# If the input line has MSG_INTL or MSG_ORIG, collect the
# MACRO used. Assumption is that the MACRO names have to be
# composed of upper characters.
#
/MSG_INTL|MSG_ORIG|_elf_seterr/ {
	for (i = 1; i <= NF; ++i) {
		if ($i == "MSG_INTL" || $i == "_elf_seterr") {
			if (i == NF - 1) {
				watchme_intl = 1
				next
			}
			j = i + 1
			while ($j == "")
				j++
			if (match($j, /[a-z]/) == 0 &&
			    match($j, /[A-Z]/) != 0) 
				print $j	> "MSG_INTL_LIST"
		}

		if ($i == "MSG_ORIG") {
			if (i == NF - 1) {
				watchme_orig = 1
				next
			}
			j = i + 1
			while ($j == "")
				j++
			if (match($j, /[a-z]/) == 0 &&
			    match($j, /[A-Z]/) != 0) 
				print $j	> "MSG_ORIG_LIST"
		}
	}
}

#
# If the previous line ended with MSG_INTL or MSG_ORIG not
# having the MACRO name, pick it from the next line.
#
{
	if (watchme_intl == 1) {
		if (match($1, /[a-z]/) == 0 &&
		    match($1, /[A-Z]/) != 0) 
			print $1	> "MSG_INTL_LIST"
		watchme_intl = 0;
	} else if (watchme_orig == 1) {
		if (match($1, /[a-z]/) == 0 &&
		    match($1, /[A-Z]/) != 0) 
			print $1	> "MSG_INTL_ORIG"
		watchme_orig = 0;
	}
}
