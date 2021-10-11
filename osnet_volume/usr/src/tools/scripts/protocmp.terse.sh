#!/bin/ksh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)protocmp.terse.sh	1.1	99/01/11 SMI"

old=$1
new=$2
differ=$3
shift 3
errlog=/tmp/protocmp.err.$$

protocmp $* 2>$errlog | nawk -v old="$old" -v new="$new" -v differ="$differ" '
	/^\**$/ {
		next;
	}
	/^\* Entries/ {
		category++;
		next;
	}
	/^\* filea ==/ {
		filea = $NF;
		next;
	}
	/^\* fileb ==/ {
		fileb = $NF;
		next;
	}
	{
		buf[category, ++line[category]] = $0
	}
	END {
		if (line[1] > 2) {
			printf("\n%s\n\n", old);
			for (i = 1; i <= line[1]; i++) {
				print buf[1, i];
			}
		}
		if (line[2] > 2) {
			printf("\n%s\n\n", new);
			for (i = 1; i <= line[2]; i++) {
				print buf[2, i];
			}
		}
		if (line[3] > 2) {
			printf("\n%s\n\n", differ);
			for (i = 1; i <= line[3]; i++) {
				print buf[3, i];
			}
		}
	}'

if [ -s $errlog ]; then
	echo "\n====== protocmp ERRORS =====\n" 
	cat $errlog
fi
rm -f $errlog
