#! /bin/sh
#
# Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
#
# @(#)ds.sh	1.17	96/01/16 SMI

# Calculate DU or MDB diskette space requirements

SCRIPT=`basename ${0}`
usage()
{
echo "\
Usage:  ${SCRIPT} du|mdb [ REALMODE | SOLARIS ] [ 3 | 5 ]

        Print the diskette space requirements for either the Driver Update 
        (du) boot diskette(s) or the MDB (mdb) multiple device boot 
        diskette.  For Driver Update, specify either REALMODE or SOLARIS 
	for the type of boot diskette being prepared and the size of the
	disk (3.5 or 5.25 inch) being prepared.
"
exit 1
}

get_info () {
    if [ "$1" ] ; then
	[ "$1" = REALMODE ] && type="REALMODE DRIVERS " || type="SOLARIS DRIVERS "
	[ "$ds" = 3 ] && otherds=5 || otherds=3
    fi

    WHAT_IT_IS="$WHAT_IT_IS ${type}boot"

    echo "\n$WHAT_IT_IS diskette space requirements:"

    if [ "$1" = SOLARIS ] ; then
	type=Solaris
	cd drv
	bytes=`wc -c * | tail -1 | sed 's/total//'`
	echo "$type drivers: \t\c"
	cd ..
	echo $bytes | awk '\
	{ s += $1 }
	END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

	ebytes=`find . -depth -type f -print | egrep -v "\/drv\/|ident\..*\.$otherds" | \
	    xargs wc -c | tail -1 | sed 's/total//'`
    else
	# We're dealing with a DU or WOS realmode boot disk.
	type=BEF
	bytes=`wc -c *.bef *.xxx | tail -1 | sed 's/total//'`
	echo "$type drivers: \t\t\c"
	echo $bytes | awk '\
	{ s += $1 }
	END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

	ebytes=`find . -depth -type f -print | egrep -v \
	    "\.bef|\.xxx|ident\..*\.$otherds" | \
	    xargs wc -c | tail -1 | sed 's/total//'`
    fi

    echo "Everything else: \t\c"
    echo $ebytes | awk '\
    { s += $1 }
    END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'

    echo "Total: \t\t\t\c"

    expr $bytes + $ebytes | awk '\
    { s += $1 }
    END { printf( "%6.2f BLKS\t%8d BYTES\t%.3f MBs\n", (s/512), s, s/1024000 ) }'
    sdl_bytes=`expr $bytes + $ebytes | awk '{ printf ("%8d", (1.44*1024000) - $1) }'`
    ldl_bytes=`expr $bytes + $ebytes | awk '{ printf ("%8d", (1.2*1024000) - $1) }'`

    echo
    if [ "$1" = REALMODE ] ; then
	num_drv=`ls -1 *.bef | wc -l | sed 's/[ ]*//'`
	avg_drv=`expr $bytes \/ $num_drv`
    else
	cd drv
	num_drv=`ls -1 * | wc -l | sed 's/[ ]*//'`
	avg_drv=`expr $bytes \/ $num_drv`
	cd ..
    fi

    echo "Number of $type drivers: $num_drv"
    echo "Average $type driver size: $avg_drv"

    echo
    if [ "$sdl_bytes" -lt 0 ] ; then
	num_sd=`echo $ebytes | awk '{ printf ("%2.3f", $1/(1.44*1024000)) }'`
	echo "Expect to use $num_sd 3.5\" diskettes."
    else
	echo "Space left on 3.5\" diskette: \t\t$sdl_bytes BYTES"
    fi
    if [ "$ldl_bytes" -lt 0 ] ; then
	num_ld=`echo $ebytes | awk '{ printf ("%2.3f", $1/(1.2*1024000)) }'`
	echo "Expect to use $num_ld 5.25\" diskettes."
    else
	echo "Space left on 5.25\" diskette:\t\t$ldl_bytes BYTES"
    fi
    echo
}

. ./database

case $1 in du ) subdir=$DU_NUM
		WHAT_IT_IS='Driver Update'
		;;
	   mdb) WHAT_IT_IS='MDB'
		subdir=$RELEASE
		;;
	   *  ) usage
		;;
esac

export WHAT_IT_IS

cd $BOOT_DIR/proto_$1/$subdir

if [ "$1" = du ] ; then
    case $3 in
	3|5) ds=$3
	     case $2 in
		REALMODE )   ( cd FIRST ; get_info REALMODE $ds ;)
			     ;;
		 SOLARIS )   ( cd SECOND ; get_info SOLARIS $ds ;)
			     ;;
		       * )   usage
			     ;;
	     esac
	     ;;
       *|"") usage
	     ;;
    esac
else
    get_info
fi
