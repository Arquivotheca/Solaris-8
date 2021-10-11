#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
# sccsid = @(#) file_attr 1.1 1/2/91 14:40:37

# file_attr - takes as argument a pathname and
#              prints file attributes in the following format:
#
#              pathname mode owner group type
#

if [ $# -ne 1 ]
then
	echo
	exit
fi

lsline=`$LS -ld $1`

name=`echo $lsline | $AWK '{print $9}'`
mode=`echo $lsline | $AWK '{print $1}'`
owner=`echo $lsline | $AWK '{print $3}'`
group=`echo $lsline | $AWK '{print $4}'`

type=`echo $mode | $AWK '{ if (substr($0, 1, 1)=="d") { \
	                      print "directory"
                           } else if (substr($0, 1, 1)=="l") { \
	                      print "symlink"
                           } else {
	                      print "file"
	                   }
                         }'`

echo "$name `$STR_TO_MODE $mode` $owner $group $type"
