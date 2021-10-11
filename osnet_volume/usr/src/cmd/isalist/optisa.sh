#!/bin/sh
#       Copyright (c) 1996 by Sun Microsystems, Inc. 
#	All Rights reserved.

PATH=/usr/bin:/usr/sbin

#ident  "@(#)optisa.sh 1.5     96/10/28 SMI"        /* SVr4.0 1.8   */
#       isalist command 

if test $# -eq 0
then 
	echo "usage: $0 isalist"
	exit 1
fi

for i in `isalist`
do
        for j
        do
                if [ $i = $j ]
                then
                        echo $i
                        exit 0
                fi
        done
done
exit 1

