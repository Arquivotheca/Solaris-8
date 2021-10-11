#! /bin/sh
#
# Copyright (c) 1994 Sun Microsystems, Inc. All rights reserved.
#
# @(#)move.sh	1.8	94/05/23 SMI

#
# Put bootstrap and realmode driver sources to DOS-format diskette.
# Extract DOS-built bootstraps and realmode drivers from DOS-format diskette.

. database

SCRIPT=`basename ${0}`

usage()
{
echo "\
Usage:  ${SCRIPT} [-i] [-o] [-s <realmode_source_dir>] <drv_num>

	   -i   Extract the DOS-built bootstraps and realmode drivers from
		the specified diskette into the ../proto directory for
		inclusion on the Driver Update and MDB diskettes.
		Excludes use of -o flag.

	   -o   Copy the realmode bootstrap and driver source heirarchy to
		the specified diskette for transfer to the DOS machine.
		Excludes use of -i flag.

	   -s   Specifies the location of the realmode source directory.
		Required if -o flag specified and SRC variable not set in
		database. Overrides SRC assignment made in database if
		specified.

    <drv_num>   The diskette drive for the applicable operation, e.g., 0 to 9.
"
	exit 1
}

unset d_FLAG i_FLAG o_FLAG

while getopts ios: FLAG ; do
    case $FLAG in i ) i_FLAG=1
		      [ "$o_FLAG" ] && usage
		      ;;
		  o ) o_FLAG=1
		      [ "$i_FLAG" ] && usage
		      ;;
		  s ) SRC=$OPTARG
		      ;;
		 \? ) usage
		      ;;
    esac
done

shift `expr $OPTIND - 1`

case $1 in [0-9]) d_FLAG=1
		  drv_num=$1
		  DRIVE="/dev/fd$drv_num"
		  RDRIVE="/dev/rfd$drv_num"
		  trap "umount ${DRIVE} >/dev/null 2>&1; exit 0" 0 1 2 3 15
		  ;;
	       *) usage
		  ;;
esac

if [ ! "$d_FLAG" ] ; then
    echo "$SCRIPT: Drive number not specified."
    usage
fi

[ ! "$SRC" -a "$o_FLAG" ] && \
    ( echo "Location of realmode sources not defined. Set SRC." ; usage ;)

[ ! -d "$SRC" ] && \
    ( echo "SRC set to invalid directory name: $SRC" ; usage ;)

UID=`id | sed -e 's/^uid=//' -e 's/(.*//'`
if [ ${UID} -ne 0 ]
then
	echo "${SCRIPT}: You must be root to run this program." >&2
	exit 1
fi

insert_diskette () {
    echo "\nPlease insert $1 diskette into drive $drv_num."
    echo "Press enter to $2 diskette in drive $drv_num."
    read x
    umount $DRIVE > /dev/null 2>&1
}

remove_diskette () {
    umount $DRIVE > /dev/null 2>&1
    echo "\nPlease remove the $1 diskette from drive $drv_num."
}

source () {
	list="$1"
	last_in_list=`echo $list | tr "\040" "\012" | tail -1`
	[ -n "$list" ] && \
	    echo "source() restarted with the following list:\n$list"
	cd $SRC
	[ ! "$list" ] && \
	    list=`find $SRC_DIRS_FILES -type f -print | \
		  egrep -v "SCCS|\.del" | sort`
	insert_diskette "a blank" "create the realmode source"
	rc=1
	# Format the disk DOS style
	until [ "$rc" -eq 0 ] ; do
	    fdformat -d ${RDRIVE}
	    rc=`expr $?`
	done

	# Mount the DOS diskette
	mount -F pcfs ${DRIVE} /mnt

	echo "Copying the following files to the diskette ..."

	newlist="$list"
	for i in $list ; do
	    # Change /'s to ='s
	    ni=`echo $i | tr "\057" "\075"`
	    dir=/mnt/`dirname $i`
	    [ -d $dir ] || mkdir -p $dir
	    cp $i /mnt/$i 2>/dev/null
	    if [ "$?" -ne 0 ] ; then
		remove_diskette "realmode source" ; source "$newlist"
	    else
		if [ "$i" = "$last_in_list" ] ; then
		    exit
		fi
	    fi
	    # Take ni off the list
	    newlist=`echo $newlist | tr "\040" "\012" | tr "\057" "\075" | \
		  sed -e "/$ni/d" | tr "\075" "\057"`
	    echo "$i"
	done

	remove_diskette "realmode source"
}

extract () {
	insert_diskette "the realmode binary" "copy the contents of the"
	[ -d $PROTO_DOS ] && rm -f $PROTO_DOS/* || mkdir -p $PROTO_DOS
	# Mount the DOS diskette
	mount -r -F pcfs ${DRIVE} /mnt
	cd /mnt ; ls -1 | cpio -pdumv $PROTO_DOS
	remove_diskette "realmode binary"
}
[ "$o_FLAG" ] && source
[ "$i_FLAG" ] && extract
