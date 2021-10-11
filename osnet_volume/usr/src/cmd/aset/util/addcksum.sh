#! /bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
# sccsid = @(#) addcksum 1.2 1/2/91 14:49:04
#

# This script takes a checklist file from standard input and adds a checksum
# to each *file* entry. If the entry is not a file (e.g. directory
# or symbolic link), nothing is added. The result is written to standard
# output.

while read perm links user group size month date time year filename junk
do
	firstchar=`echo $perm | $SED "s/^\(.\).*/\1/"`
	if test "$firstchar" = "-"
	then
		cksum=`$SUM $filename | $SED "s/^\([0-9]* [0-9]*\) .*/\1/"`
		echo "$perm $links $user $group $size $month $date $time\c"
		echo " $year $filename $junk $cksum"
	else
		echo "$perm $links $user $group $size $month $date $time\c"
		echo " $year $filename $junk"
	fi
done
