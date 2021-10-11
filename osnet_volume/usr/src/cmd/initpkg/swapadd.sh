#!/sbin/sh
#
# Copyright (c) 1991, 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
#ident	"@(#)swapadd.sh	1.8	99/03/23 SMI"

PATH=/usr/sbin:/usr/bin; export PATH
USAGE="Usage: swapadd [-12] [file_system_table]"

VFSTAB=/etc/vfstab	# Default file system table
PASS=2			# Default to checking for existing swap

#
# Check to see if there is an entry in the fstab for a specified file and
# mount it.  This allows swap files (e.g. nfs files) to be mounted before
# being added for swap.
#
checkmount()
{
	while read rspecial rfsckdev rmountp rfstype rfsckpass rautomnt rmntopts
	do
		#
		# Ignore comments, empty lines, and no-action lines
		#
		case "$rspecial" in
		'#'* | '' | '-') continue ;;
		esac

		if [ "x$rmountp" = "x$1" ]; then
			#
			# If mount options are '-', default to 'rw'
			#
			[ "x$rmntopts" = x- ] && rmntopts=rw

			if /sbin/mount -m -o $rmntopts $rspecial \
			    >/dev/null 2>&1; then
				echo "Mounting $rmountp for swap"
			else
				echo "Mount of $rmountp for swap failed"
			fi
			return
		fi
	done <$VFSTAB
}

die()
{
	echo "$*" >& 2
	exit 1
}

while getopts 12 opt; do
	case "$opt" in
	1|2) PASS=$opt ;;
	 \?) die "$USAGE" ;;
	esac
done
shift `expr $OPTIND - 1`

[ $# -gt 1 ] && die "$USAGE"
[ $# -gt 0 ] && VFSTAB="$1"

#
# If $VFSTAB is not "-" (stdin), re-open stdin as the specified file
#
if [ "x$VFSTAB" != x- ]; then
	[ -s "$VFSTAB" ] || die "swapadd: file system table ($VFSTAB) not found"
	exec <$VFSTAB
fi

#
# Read the file system table to find entries of file system type "swap".
# Add the swap device or file specified in the first column.
#
while read special t1 t2 fstype t3 t4 t5; do
	#
	# Ignore comments, empty lines, and no-action lines
	#
	case "$special" in
	'#'* | '' | '-') continue ;;
	esac

	#
	# Ignore non-swap fstypes
	#
	[ "$fstype" != swap ] && continue

	if [ $PASS = 1 ]; then
		#
		# Pass 1 should handle adding the swap files that
		# are accessable immediately; block devices, files
		# in / and /usr, and direct nfs mounted files.
		#
		if [ ! -b $special ]; then
			#
			# Read the file system table searching for mountpoints
			# matching the swap file about to be added.
			#
			# NB: This won't work correctly if the file to added
			# for swapping is a sub-directory of the mountpoint.
			# e.g.	swapfile-> servername:/export/swap/clientname
			# 	mountpoint-> servername:/export/swap
			#
			checkmount $special
		fi
		if [ -f $special -a -w $special -o -b $special ]; then
			swap -$PASS -a $special >/dev/null
		fi
	else
		#
		# Pass 2 should skip all the swap already added.  If something
		# added earlier uses the same name as something to be added
		# later, the following test won't work. This should only happen
		# if parts of a particular swap file are added or deleted by
		# hand between invocations.
		#
		swap -l 2>/dev/null | grep '\<'${special}'\>' >/dev/null 2>&1 \
		    || swap -$PASS -a $special >/dev/null
	fi
done
