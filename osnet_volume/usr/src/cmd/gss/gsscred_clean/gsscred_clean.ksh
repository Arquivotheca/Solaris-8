#!/bin/ksh
# 
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)gsscred_clean.ksh	1.3	98/07/01 SMI" 
#
# gsscred_db clean up script
#
# This file is used to remove duplicate entries from
# the gsscred_db file. It is activated as a root cron
# job once a day. It only performs cleanup when
# the gsscred_db file has changed since last operation.

FILE_TO_CLEAN=/etc/gss/gsscred_db
CLEAN_TIME=/etc/gss/.gsscred_clean
TMP_FILE=/etc/gss/gsscred_clean$$

trap "rm -f $TMP_FILE; exit" 0 1 2 3 13 15


if [ -s $FILE_TO_CLEAN ] && [ $FILE_TO_CLEAN -nt $CLEAN_TIME ]
then

#
#	The file being sorted has the following format:
#		name	uid	comment
#
#	We are trying to remove duplicate entries for the name
#	which may have different uids. Entries lower in the file
#	are newer since addition performs an append. We use cat -n
#	in order to preserve the order of the duplicate entries and
#	only keep the latest. We then sort on the name, and line
#	number (line number in reverse). The line numbers are then
#	removed and duplicate entries are cut out.
#
	cat -n $FILE_TO_CLEAN | sort -k 2,2 -k 1,1nr 2> /dev/null \
		| cut -f2- | \
		awk ' (NR > 1 && $1 != key) || NR == 1 { 
				key = $1;
				print $0;
			}
		' > $TMP_FILE

	if [ $? -eq 0 ] && mv $TMP_FILE $FILE_TO_CLEAN; then
#
#		update time stamp for this sort
#
		touch -r $FILE_TO_CLEAN $CLEAN_TIME
	else
		rm -f $TMP_FILE
	fi
fi
