#!/sbin/sh
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T.
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
# Copyright (c) 1991-1993, 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)rcS.sh	1.37	99/03/31 SMI"

# This file executes the commands in the rcS.d directory, which are necessary
# to get the system to single user mode:
#
# 	establish minimal network plumbing (for diskless and dataless)
#	mount /usr (if a separate file system)
#	set the system name
#	check the root (/) and /usr file systems
#	check and mount /var and /var/adm (if a separate file system)
#	mount pseudo file systems (/proc and /dev/fd)
#	if this is a reconfiguration boot, [re]build the device entries
#	check and mount other file systems to be mounted in single user mode

#
# Default definitions:
#
PATH=/usr/sbin:/usr/bin:/sbin
vfstab=/etc/vfstab
mnttab=/etc/mnttab
mntlist=
option=
otherops=

# Export boot parameters to rc scripts

if [ "x$1" != xsysinit -a -d /usr/bin ]; then
	set -- `/usr/bin/who -r`

	_INIT_RUN_LEVEL=${7:-S}   # Current run-level
	_INIT_RUN_NPREV=${8:-0}   # Number of times previously at current level
	_INIT_PREV_LEVEL=${9:-0}  # Previous run-level
else
	_INIT_RUN_LEVEL=S
	_INIT_RUN_NPREV=0
	_INIT_PREV_LEVEL=0
fi

set -- `/sbin/uname -a`

#
# If we're booting, uname -a will produce one fewer token than usual because
# the hostname has not yet been configured.  Leave NODENAME empty in this case.
#
if [ $# -eq 7 ]; then
	_INIT_UTS_SYSNAME="$1"  # Operating system name (uname -s)
	_INIT_UTS_NODENAME="$2" # Node name (uname -n)
	shift 2
else
	_INIT_UTS_SYSNAME="$1"  # Operating system name (uname -s)
	_INIT_UTS_NODENAME=	# Node name is not yet configured
	shift 1
fi

_INIT_UTS_RELEASE="$1"  # Operating system release (uname -r)
_INIT_UTS_VERSION="$2"  # Operating system version (uname -v)
_INIT_UTS_MACHINE="$3"  # Machine class (uname -m)
_INIT_UTS_ISA="$4"      # Instruction set architecture (uname -p)
_INIT_UTS_PLATFORM="$5" # Platform string (uname -i)

export _INIT_RUN_LEVEL _INIT_RUN_NPREV _INIT_PREV_LEVEL \
    _INIT_UTS_SYSNAME _INIT_UTS_NODENAME _INIT_UTS_RELEASE _INIT_UTS_VERSION \
    _INIT_UTS_MACHINE _INIT_UTS_ISA _INIT_UTS_PLATFORM

# Export net boot configuration strategy. _INIT_NET_IF is set to the
# interface name of the netbooted interface if this is a net boot.
# _INIT_NET_STRATEGY is set to the network configuration strategy.
set -- `/sbin/netstrategy`
if [ $? -eq 0 ]; then
	if [ "$1" = "nfs" -o "$1" = "cachefs" ]; then
		_INIT_NET_IF="$2"
	fi
	_INIT_NET_STRATEGY="$3"
	export _INIT_NET_IF _INIT_NET_STRATEGY 
fi

#
# Useful shell functions:
#

#
#	shcat file
#
# Simulates cat in sh so it doesn't need to be on the root filesystem.
#
shcat() {
	while [ $# -ge 1 ]; do
		while read i; do
                        echo "$i"
		done < $1
		shift
	done
}

#
#	readvfstab mount_point
#
# Scan vfstab for the mount point specified as $1. Returns the fields of
# vfstab in the following shell variables:
#	special		: block device
#	fsckdev		: raw device
#	mountp		: mount point (must match $1, if found)
#	fstype		: file system type
#	fsckpass	: fsck pass number
#	automnt		: automount flag (yes or no)
#	mntopts		: file system specific mount options.
# All fields are retuned empty if the mountpoint is not found in vfstab.
# This function assumes that stdin is already set /etc/vfstab (or other
# appropriate input stream).
#
readvfstab() {
	while read special fsckdev mountp fstype fsckpass automnt mntopts; do
		case "$special" in
		'#'* | '')	#  Ignore comments, empty lines
				continue ;;
		'-')		#  Ignore no-action lines
				continue
		esac

		[ "x$mountp" = "x$1" ] && break
	done
}

#
#	checkmessage raw_device fstype mountpoint
#
# Simple auxilary routine to the shell function checkfs. Prints out
# instructions for a manual file system check before entering the shell.
#
checkmessage() {
	echo ""
	echo "WARNING - Unable to repair the $3 filesystem. Run fsck"
	echo "manually (fsck -F $2 $1). Exit the shell when"
	echo "done to continue the boot process."
	echo ""
}

checkmessage2() {
	echo ""
	echo "WARNING - fatal error from fsck - error $4"
	echo "Unable to repair the $3 filesystem. Run fsck manually"
	echo "(fsck -F $2 $1). System will reboot when you exit the shell."
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
#	40 - for root, same as 0 (used here to remount root)
# Note that should a shell be entered and the operator be instructed to
# manually check a file system, it is assumed the operator will do the right
# thing. The file system is not rechecked.
#
checkfs() {
	# skip checking if the fsckdev is "-"
	[ "x$1" = x- ] && return

	# if fsck isn't present, it is probably because either the mount of
	# /usr failed or the /usr filesystem is badly damanged.  In either
	# case, there is not much to be done automatically.  Halt the system
	# with instructions to either reinstall or `boot -b'.

	if [ ! -x /usr/sbin/fsck ]; then
		echo ""
		echo "WARNING - /usr/sbin/fsck not found.  Most likely the"
		echo "mount of /usr failed or the /usr filesystem is badly"
		echo "damaged.  The system is being halted.  Either reinstall"
		echo "the system or boot with the -b option in an attempt"
		echo "to recover."
		echo ""
		uadmin 2 0
	fi

	/usr/sbin/fsck -F $2 -m $1 >/dev/null 2>&1

	if [ $? -ne 0 ]; then
		# Determine fsck options by file system type
		case $2 in
			ufs)	foptions="-o p"
				;;
			s5)	foptions="-y -t /tmp/tmp$$ -D"
				;;
			*)	foptions="-y"
				;;
		esac

		echo "The $3 file system ($1) is being checked."
		/usr/sbin/fsck -F $2 $foptions $1
	
		case $? in
		0|40)	# File system OK
			;;

		1|34|36|37|39)	# couldn't fix the file system - enter a shell
			checkmessage "$1" "$2" "$3"
			/sbin/sulogin < /dev/console
			echo "resuming system initialization"
			;;
         
       		*)	# fsck child process killed (+ error code 35)
			checkmessage2 "$1" "$2" "$3" "$?"
			/sbin/sulogin < /dev/console
			echo "*** SYSTEM WILL REBOOT AUTOMATICALLY ***"
			sleep 5
			/sbin/uadmin 2 1
			;;
		esac
	fi
}

#
#	checkopt option option-string
#
#	Check to see if a given mount option is present in the comma
#	separated list gotten from vfstab.
#
#	Returns:
#	${option}       : the option if found the empty string if not found
#	${otherops}     : the option string with the found option deleted
#
checkopt() {
	option=
	otherops=

	[ "x$2" = x- ] && return

	searchop="$1"
	set -- `IFS=, ; echo $2`

	while [ $# -gt 0 ]; do
		if [ "x$1" = "x$searchop" ]; then
			option="$1"
		else
			if [ -z "$otherops" ]; then
				otherops="$1"
			else
				otherops="${otherops},$1"
			fi
		fi
		shift
	done
}

#
#	umount_fsys	umountall-args
#
#	Calls umountall with the specified arguments and reports progress
#	as file systems are unmounted if umountall -k is invoked.
#
umount_fsys ()
{
	/sbin/umountall "$@" 2>&1 | while read fs; do \
		shift $#; set -- $fs
		if [ "x$1" = xumount: ]; then
			echo "$*"	# Most likely an error message
		else
			echo "$1 \c" | /usr/bin/tr -d :
		fi
	done
	echo "done."
}

#
# Start here: If requested, attempt to fall through to sulogin without
# doing anything. This is a clear act of desperation.
#
[ "x$RB_NOBOOTRC" = xYES ] && exit 0

#
# Make the old, deprecated environment variable (_DVFS_RECONFIG) and the new
# supported environment variable (_INIT_RECONFIG) to be synonyms.  Set both
# if the file /reconfigure exists.  _INIT_RECONFIG is the offical, advertized
# way to identify a reconfiguration boot.  Note that for complete backwards
# compatibility the value "YES" is significant with _DVFS_RECONFIG.  The
# value associated with _INIT_RECONFIG is insignificant.  What is significant
# is only that the environment variable is defined.
#
if [ "x$_DVFS_RECONFIG" = xYES -o -n "$_INIT_RECONFIG" -o -f /reconfigure ]
then
	_DVFS_RECONFIG=YES; export _DVFS_RECONFIG
	_INIT_RECONFIG=set; export _INIT_RECONFIG
fi

#
# If we have been at this run-level before, then we're coming down to single-
# user mode from a higher run-level.  The SXX scripts in single-user mode are
# once-only configuration items, so we don't run them again.  We only
# execute the KXX scripts to make sure that everything which shouldn't be
# running now is dead.  We also touch /etc/nologin to make sure single-user
# mode means no one else can log in.  This will be removed by RMTMPFILES
# on transition back to run-level 2.
#


if [ $_INIT_RUN_NPREV -ne 0 ]; then
	echo 'The system is coming down for administration.  Please wait.'
	>/etc/nologin

        echo "Unmounting remote filesystems: \c"
	umount_fsys -r -k -s

	if [ -d /etc/rcS.d ]; then
		for f in /etc/rcS.d/K*; do
			if [ -s $f ]; then
				case $f in
					*.sh)	.	 $f ;;
					*)	/sbin/sh $f stop ;;
				esac
			fi
		done
	fi

	echo "Killing user processes: \c"
	#
	# Look for ttymon, in.telnetd, in.rlogind and processes
	# in their process groups so they can be terminated.
	#
	/usr/sbin/killall
	/usr/sbin/killall 9
	/usr/bin/pkill -TERM -v -u 0,1; sleep 5
	/usr/bin/pkill -KILL -v -u 0,1
	echo "done."

else
	if [ -d /etc/rcS.d ]; then
		for f in /etc/rcS.d/S*; do
			if [ -s $f ]; then
				case $f in
					*.sh)	.	 $f ;;
					*)	/sbin/sh $f start ;;
				esac
			fi
		done
	fi

	#
	# Clean up the /reconfigure file and sync the new entries to stable 
	# media.
	#
	if [ -n "$_INIT_RECONFIG" ]; then
		[ -f /reconfigure  ] && /usr/bin/rm -f /reconfigure
		/sbin/sync
	fi
fi
