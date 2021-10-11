#!/bin/sh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)keywords.sh	1.3	99/03/29 SMI"
#
# Checks the list of files to make sure that each given
# file has a SMI standard ident string.
#
# If the file is checked in it simply checks that keywords exist,
# if the file is checked out it verifies the string.
# By default, all allowable forms of keywords (according to
# the ON documentation) are acceptable.  The '-p' option (pedantic)
# allows only the preferred form of keywords. See below for allowable
# forms.
#
# Use as "keywords filelist" where filelist is the list of
# plain files, which could be the edited output of 'putback -n'.
#
# The following command will check a reasonable subset of ON source
# in a teamware workspace.
#
#	% cd $CODEMGR_WS/usr/src
# 	% keywords \
#	`find . -name SCCS -prune -o -name '*Make*' -print -o '*.csh' -print`
#
# Output is sent to stderr, and consists of filenames with
# unexpanded (or no) sccs keywords and/or filenames that were
# not SCCS files.
#
# Exits with status 0 if all files are sccs files and all files have
# unexpanded keywords. Otherwise, exits with a non-zero status.

pedantic=0
cwd=`pwd`
exitcode=0
rm -f /tmp/xxx$$

while getopts p c
do
    case $c in
    p)	pedantic=1;;
    \?)	echo $USAGE
	exit 2;;
    esac
done
shift `expr $OPTIND - 1`

# Prefered form
PREFERED="#pragma ident	\"\%\Z\%\%\M\%	\%\I\%	\%\E\% SMI\""
# Allowed form but not ANSI-C conformant
ALLOWED="#ident	\"\%\Z\%\%\M\%	\%\I\%	\%\E\% SMI\""
# Allowed old SunSoft South code
SSSOLD1="#pragma ident	\"\%\W\%	\%\E\% SMI\""
SSSOLD2="#ident	\"\%\W\%	\%\E\% SMI\""

# Java keywords
JAVA_KW="ident	\"\%\Z\%\%\M\%	\%\I\%	\%\E\% SMI\""

for i
do
    dir=`dirname $i`
    file=`basename $i`

    cd $dir
    if [ -f SCCS/s.$file ]; then
	if [ -f SCCS/p.$file ]; then
	    #
	    # If it is a java file check for java keywords
	    #
	    echo "$file" | egrep -s '\.java$' && {
		egrep -s "$JAVA_KW" $file || {
				echo "Incorrect ident string in $i" >&2
				exitcode=1
		}
		continue
	    }

	    egrep -s "$PREFERED" $file
	    if [ $? -ne 0 ]; then
		if [ $pedantic -eq 1 ]; then
		    echo "Incorrect ident string in $i" >&2
		    exitcode=1
		else 
		    egrep -s -e "$ALLOWED" $file
		    if [ $? -ne 0 ]; then
			egrep -s -e "$SSSOLD1" $file
			if [ $? -ne 0 ]; then
			    egrep -s -e "$SSSOLD2" $file
			    if [ $? -ne 0 ]; then
				echo "Incorrect ident string in $i" >&2
				exitcode=1
			    fi
			fi
		    fi
		fi
	    fi
	else
	    sccs get -p $file > /dev/null 2>/tmp/xxx$$
	    egrep -s "cm7" /tmp/xxx$$
	    if [ $? -eq 0 ]; then
		echo "Missing keywords in $i" >&2
		exitcode=1
    	    fi
    	fi
    else
    	echo "Not an SCCS file: $i"
    	exitcode=1
    fi
    cd $cwd
done

rm -f /tmp/xxx$$
exit $exitcode

