#!/bin/sh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)build_cscope.sh	1.2	99/10/04 SMI"
#
MACH=`uname -p`

PATH=/usr/ccs/bin:/opt/onbld/bin:/opt/onbld/bin/${MACH}:/opt/teamware/bin:/opt/SUNWspro/bin:$PATH
export PATH

# test that the path to the environment-setting file was given
if [ $# -ne 1 ]; then
	echo "Usage: build_cscope <env_file>"
	exit 1
fi

#
#       Setup environmental variables
#
if [ -f $1 ]; then
        . $1
else
        if [ -f /opt/onbld/env/$1 ]; then
                . /opt/onbld/env/$1
        else
                echo "Cannot find env file as either $1 or /opt/onbld/env/$1"
                exit 1
        fi
fi

TMPDIR=/tmp

export MACH TMPDIR

cd $SRC
for dir in uts uts/sun4d uts/sun4m uts/sun4u uts/i86pc
do
	if [ -d $dir ]; then
        	cd $dir
        	rm -f cscope.* tags tags.list
        	make cscope.files >/dev/null 2>&1
        	make tags >/dev/null 2>&1
        	cscope-fast -bq >/dev/null 2>&1
        	cd $SRC
	fi
done


# full
INCLUDEDIRS=${CODEMGR_WS}/proto/root_$MACH/usr/include
export INCLUDEDIRS
rm -f cscope.*
find . -name SCCS -prune -o -type d -name '.del-*' -prune -o -type f \( \
        -name '*.[Ccshlxy]' -o \
        -name 'Makefile*' -o \
        -name '*.adb' -o \
        -name '*.il' -o \
        -name '*.cc' \
        \) -print > cscope.files
cscope-fast -bq >/dev/null 2>&1
