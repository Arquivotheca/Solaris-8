#!/bin/ksh 
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)check_rpaths.sh	1.1	99/01/11 SMI"

if [ $# != 1 ]; then
	echo "Usage: $0 <directory>"
	exit 1;
fi

if [ ! -d $1 ]; then
	echo "$1 is not a directory"
	exit 1;
fi

cd $1

for file in *
do
	if [ -h $file ]; then
		continue;
	fi
	if [ -f $file ]; then
		ldd $file >/dev/null 2>&1
		if [ $? != 0 ]; then
			continue;
		fi
		# ELF file
		rpath=`dump -Lv $file | egrep RPATH`
		if [ $? = 0 ]; then
			rpath=`echo $rpath | nawk '{print $ 3}'`
			echo "$file: $rpath"
		fi
	fi
	if [ -d $file ]; then
		dir=`pwd`
		cd ${file}
		$0 .
		cd ${dir}
	fi
done
