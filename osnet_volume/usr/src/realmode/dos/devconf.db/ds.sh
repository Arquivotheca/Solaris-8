#!/bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)ds.sh        1.4     99/03/08 SMI

# Calculate DCB diskette space requirements

SCRIPT=`basename ${0}`
usage()
{
echo "\
Usage:  ${SCRIPT} directory

	    Print the diskette space requirements for a DCB (device
	    configuration boot) floppy.  The directory argument
	    is the name of a prototype directory for the floppy's
	    contents.
"
	exit 1
}

[ ! -d $1 ] && usage;

WHAT_IT_IS='DCB'; 

cd $1
echo "\n$WHAT_IT_IS diskette space requirements:"

#
#	Compute overhead associated with pcfs.
#
#	Each pcfs has a boot sector, 2 FATs, and a root directory.
#
#	Boot sector is 512 bytes.
#
#	Size of the FAT depends on the disk type
#	(1.44M floppies use 9 sectors/FAT, 1.2M floppies use 7)
#
#	For both of these floppy types the root directories have 224 entries
#	of 32 bytes a piece = 7168 bytes.
#
#	
sharedover=7680
extra3=`expr 2 \* 9`
extra3=`expr $extra3 \* 512`
extra5=`expr 2 \* 7`
extra5=`expr $extra5 \* 512`
over3=`expr $sharedover + $extra3`
over5=`expr $sharedover + $extra5`

befbytes=`find . -depth -name '*.bef' -print | \
    xargs wc -c | sed '/total/d' | awk '{ s += $1 } END { print s }'`

echo "BEF files: \t\t\c"
echo $befbytes | awk '\
{ s += $1 }
END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

ebytes=`find . -depth -type f -print | egrep -v '\.bef|ident\..' | \
    xargs wc -c | sed '/total/d' | awk '{ s += $1 } END { print s}'`

echo "Everything else: \t\c"
echo $ebytes | awk '\
{ s += $1 }
END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

echo "Total: \t\t\t\c"

tbytes=`expr $befbytes + $ebytes`
echo $tbytes | awk '\
{ s += $1 }
END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

echo
echo File system overhead for 3.5\" floppy is $over3 bytes
echo File system overhead for 5.25\" floppy is $over5 bytes

t3bytes=`expr $tbytes + $over3`
t5bytes=`expr $tbytes + $over5`

sdl_bytes=`echo $t3bytes | \
	awk '{ printf ("%8d", (1.44*1024000) - $1) }'`
ldl_bytes=`echo $t5bytes | \
	awk '{ printf ("%8d", (1.2*1024000) - $1) }'`

echo
num_bef=`find . -name '*.bef' -print | wc -l | sed 's/[ ]*//'`
avg_bef=`expr $befbytes \/ $num_bef`
echo "Number of BEF drivers: $num_bef"
echo "Average BEF driver size: $avg_bef"

echo
if [ "$sdl_bytes" -lt 0 ] ; then
    num_sd=`echo $t3bytes | awk '{ printf ("%2.3f", $1/(1.44*1024000)) }'`
    echo "Expect to use $num_sd 3.5\" diskettes."
else
    echo "Space left on 3.5\" diskette: \t\t$sdl_bytes BYTES"
fi
if [ "$ldl_bytes" -lt 0 ] ; then
    num_ld=`echo $t5bytes | awk '{ printf ("%2.3f", $1/(1.2*1024000)) }'`
    echo "Expect to use $num_ld 5.25\" diskettes."
else
    echo "Space left on 5.25\" diskette:\t\t$ldl_bytes BYTES"
fi
echo
