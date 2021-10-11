#!/sbin/sh
#
#ident	"@(#)mountall.sh	1.28	99/09/01 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	checkmessage "fsck_device | mount_point"
#
# Simple auxilary routine to the shell function checkfs. Prints out
# instructions for a manual file system check before entering the shell.
#
checkmessage() {
	echo ""
	echo "WARNING - Unable to repair one or more of the following filesystem(s):"
	echo "\t$1"
	echo "Run fsck manually (fsck filesystem...)."
	echo "Exit the shell when done to continue the boot process."
	echo ""
}

#
#	checkfs raw_device fstype mountpoint
#
# Check the file system specified. The return codes from fsck have the
# following meanings.
#	 0 - file system is unmounted and okay
#	32 - file system is unmounted and needs checking (fsck -m only)
#	33 - file system is already mounted
#	34 - cannot stat device
#	36 - uncorrectable errors detected - terminate normally (4.1 code 8)
#	37 - a signal was caught during processing (4.1 exit 12)
#	39 - uncorrectable errors detected - terminate rightaway (4.1 code 8)
#	40 - for root, same as 0 (used by rcS to remount root)
# Note that should a shell be entered and the operator be instructed to
# manually check a file system, it is assumed the operator will do the right
# thing. The file system is not rechecked.
#
checkfs() {
	/usr/sbin/fsck -F $2 -m $1  >/dev/null 2>&1

	if [ $? -ne 0 ]
	then
		# Determine fsck options by file system type
		case "$2" in
		ufs)	foptions="-o p"
			;;
		s5)	foptions="-y -t /var/tmp/tmp$$ -D"
			;;
		*)	foptions="-y"
			;;
		esac

		echo "The "$3" file system ("$1") is being checked."
		/usr/sbin/fsck -F $2 ${foptions} $1
	
		case $? in
		0|40)	# file system OK
			;;

		*)	# couldn't fix the file system - enter a shell
			checkmessage "$1"
				/sbin/sulogin < /dev/console
			echo "resuming mountall"
			;;
		esac
	fi
}

#
# Used to save an entry that we will want to mount either in
# a command file or as a mount point list.
#
# saveentry fstype options special mountp
#
saveentry() {
	if [ "$ALTM" ]; then
		echo "/sbin/mount -F $1 $2 $3 $4" >> $ALTM
	else
		mntlist="$mntlist $4"
	fi
}


PATH=/usr/sbin:/usr/bin
USAGE="Usage:\nmountall [-F FSType] [-l|-r|-g] [file_system_table]"
TYPES=all
FSTAB=/etc/vfstab
err=0

#
# Process command line args
#
while getopts ?grlsF: c
do
	case $c in
	g)	GFLAG="g";;
	r)	RFLAG="r";;
	l)	LFLAG="l";;
	s)	SFLAG="s";;
	F)	FSType="$OPTARG";
		if [ "$TYPES" = "one" ]
		then
			echo "mountall: more than one FSType specified"
			exit 2
		fi
		TYPES="one";

		case $FSType in
		?????????*) 
			echo "mountall: FSType $FSType exceeds 8 characters"
			exit 2
		esac
		;;
	\?)	echo "$USAGE" 1>&2; exit 2;;
	esac
done

shift `/usr/bin/expr $OPTIND - 1`	# get past the processed args

if [ $# -gt 1 ]; then
	echo "mountall: multiple arguments not supported" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi

# get file system table name and make sure file exists
if [ $# = 1 ]; then
	case $1 in
	"-")	FSTAB=""
		;;
	*)	FSTAB=$1
		;;
	esac
fi
#
# if an alternate vfstab file is used or serial mode is specified, then
# use a mount command file
#
if [ $# = 1 -o "$SFLAG" ]; then
	ALTM=/var/tmp/mount$$
	rm -f $ALTM
fi

if [ "$FSTAB" != ""  -a  ! -s "$FSTAB" ]
then
	echo "mountall: file system table ($FSTAB) not found"
	exit 1
fi

#
# Check for incompatible args
#
if [ "$GFLAG" = "g" -a "$RFLAG$LFLAG" != "" -o \
     "$RFLAG" = "r" -a "$GFLAG$LFLAG" != "" -o \
     "$LFLAG" = "l" -a "$RFLAG$GFLAG" != "" ]
then
	echo "mountall: options -g, -r and -l are mutually exclusive" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi

if [ \( "$FSType" = "cachefs" -o "$FSType" = "nfs" \) -a "$LFLAG" = "l" ]
then
	echo "mountall: option -l and FSType are incompatible" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if [ "$FSTYPE" -a "$FSType" != "nfs" -a "$RFLAG" = "r" ]
then
	echo "mountall: option -r and FSType are incompatible" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi

#	file-system-table format:
#
#	column 1:	special- block special device or resource name
#	column 2: 	fsckdev- char special device for fsck 
#	column 3:	mountp- mount point
#	column 4:	fstype- File system type
#	column 5:	fsckpass- number if to be checked automatically
#	column 6:	automnt-	yes/no for automatic mount
#	column 7:	mntopts- -o specific mount options

#	White-space separates columns.
#	Lines beginning with \"#\" are comments.  Empty lines are ignored.
#	a '-' in any field is a no-op.

#
# Read FSTAB, fsck'ing appropriate filesystems:
#
exec < $FSTAB
while  read special fsckdev mountp fstype fsckpass automnt mntopts
do
	case $special in
	'#'* | '')	#  Ignore comments, empty lines
			continue ;;
	'-')		#  Ignore no-action lines
			continue
	esac

	if [ "$automnt" != "yes" ]; then
		continue
	fi
	if [ "$FSType" -a "$FSType" != "$fstype" ]; then
		# ignore different fstypes
		continue
	fi

	if [ "$LFLAG" ]; then
		# Skip entries that have the "global" option.
		g=`/usr/bin/grep '\<global\>' << EOF
			$mntopts
		EOF`
		if [ "$fstype" = "cachefs" -o "$fstype" = "nfs" -o "$g" ]; then
			continue
		fi
	elif [ "$RFLAG" -a "$fstype" != "nfs" ]; then
		continue
	elif [ "$GFLAG" ]; then
		# Skip entries that have don't the "global" option.
		g=`/usr/bin/grep '\<global\>' << EOF
			$mntopts
		EOF`
		if [ "$fstype" = "cachefs" -o "$fstype" = "nfs" -o -z "$g" ]
		then
			continue
		fi
	fi

	if [ "$fstype" = "-" ]; then
		echo "mountall: FSType of $special cannot be identified" 1>&2
		continue
	fi

	if [ "$ALTM" -a "$mntopts" != "-" ]; then
		OPTIONS="-o $mntopts"		# Use mount options if any
	else
		OPTIONS=""
	fi

	#
	# Can't fsck if no fsckdev is specified
	#
	if [ "$fsckdev" = "-" ]; then
		saveentry $fstype "$OPTIONS" $special $mountp
		continue
	fi
	#
	# For fsck purposes, we make a distinction between ufs and other
	# file systems. Here use check if that the file system is not
	# mounted using fsck -m before adding it to the list to be checked
	# 
	if [ "$fstype" = "ufs" ]; then
		/usr/sbin/fsck -m -F $fstype $fsckdev >/dev/null 2>&1
		if [ $? != 33 ]; then
			ufs_fscklist="$ufs_fscklist $fsckdev"
			saveentry $fstype "$OPTIONS" $special $mountp
			continue
		else
			echo "$fsckdev already mounted"
			continue
		fi
	fi
	#
	# fsck everything else:
 	#
 	# fsck -m simply returns true if the filesystem is suitable for
 	# mounting.
 	#
	/usr/sbin/fsck -m -F $fstype $fsckdev >/dev/null 2>&1
	case $? in
	0|40)	saveentry $fstype "$OPTIONS" $special $mountp
		continue
		;;
	32)	checkfs $fsckdev $fstype $mountp
		saveentry $fstype "$OPTIONS" $special $mountp
		continue
		;;
	33)	# already mounted
		echo "$special already mounted"
		;;
	34)	# bogus special device
		echo "Cannot stat $fsckdev - ignoring"
		err=1
		;;
	*)	# uncorrectable errors
		echo "$fsckdev uncorrectable error"
		err=1
		;;
	esac
done

#
# fsck ufs's
#
if [ "$ufs_fscklist" ]; then
	echo "checking ufs filesystems"
	/usr/sbin/fsck -o p $ufs_fscklist
	case $? in
	0|40|33)	# file system OK
			;;

	*)	# couldn't fix the filesystems - enter a shell
		checkmessage "$ufs_fscklist"
		/sbin/sulogin < /dev/console
		echo "resuming mountall"
		;;
	esac
fi

if [ "$ALTM" ]; then
	if [ ! -f "$ALTM" ]; then
		exit
	fi
	/sbin/sh $ALTM		# run the saved mount commands
	/usr/bin/rm -f $ALTM
	exit
fi

if [ -n "$FSType" ]; then
	/sbin/mount -a -F $FSType
	exit
fi

if [ "$RFLAG" ]; then
	/sbin/mount -a -F nfs
	exit
fi

if [ "$LFLAG" -o "$GFLAG" -o $err != 0 ]; then
	test -n "$mntlist" && /sbin/mount -a $mntlist
	exit
fi

# else mount them all

/sbin/mount -a
