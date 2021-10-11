#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#	Copyright (c) 1997-1999 by Sun Microsystems, Inc.
#	All rights reserved.
#
#ident	"@(#)umountall.sh	1.18	99/08/19 SMI"

PATH=/usr/sbin:/usr/bin
USAGE="Usage:\numountall [-k] [-F FSType] [-l|-r|-h host] "
FSTAB=/etc/vfstab
FSType=
kill=
FCNT=0
CNT=0
HOST=
HFLAG=
LFLAG=
RFLAG=
LOCALNAME=
while getopts ?rslkF:h: c
do
	case $c in
	r)	RFLAG="r"; CNT=`/usr/bin/expr $CNT + 1`;;
	l)	LFLAG="l"; CNT=`/usr/bin/expr $CNT + 1`;;
	s)	SFLAG="s";;
	k) 	kill="yes";;
	h)	HOST=$OPTARG;
		HFLAG="h";
		LOCALNAME=`uname -n`;; 
	F)	FSType=$OPTARG; 
		case $FSType in
		?????????*) 
			echo "umountall: FSType $FSType exceeds 8 characters"
			exit 2
		esac;
		FCNT=`/usr/bin/expr $FCNT + 1`;;
	\?)	echo "$USAGE" 1>&2;
		exit 2;;
	esac
done
shift `/usr/bin/expr $OPTIND - 1`
if test $FCNT -gt 1
then
	echo "umountall: more than one FStype specified" 1>&2
	exit 2
fi
if test $CNT -gt 1
then
	echo "umountall: options -r and -l incompatible" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test \( $CNT -gt 0 \) -a "$HFLAG" = "h"
then
	echo "umountall: options -r and -l incompatible with -h option" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test \( $FCNT -gt 0 \) -a "$HFLAG" = "h"
then
	echo "umountall: Specifying FStype incompatible with -h option" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test \( "$HOST" = "$LOCALNAME" \) -a "$HFLAG" = "h"
then
	echo "umountall: Specifying local host illegal for -h option" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test $# -gt 0
then
	echo "umountall: arguments not supported" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test \( "$FSType" = "nfs" \) -a "$LFLAG" = "l"
then
	echo "umountall: option -l and FSType are incompatible" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi
if test \( "$FSType" = "s5" -o "$FSType" = "ufs" -o "$FSType" = "bfs" \) -a "$RFLAG" = "r"
then
	echo "umountall: option -r and FSType are incompatible" 1>&2
	echo "$USAGE" 1>&2
	exit 2
fi

if [ ! "$SFLAG" -a ! "$LFLAG" -a ! "$RFLAG" -a ! "$HFLAG" -a ! "$kill" -a \
	! "$FSType" ]; then
	#
	# Then let's take advantage of parallel unmounting
	#
	/sbin/umount -a
	exit
fi

#
# Read /etc/mnttab in reverse order
#
fslist=""
/usr/bin/tail -r /etc/mnttab |
(
	while read dev mountp fstype mode dummy
	do
		case "${mountp}" in
		/ | /usr | /var | /var/adm | /var/run | /proc | /dev/fd | /etc/mnttab | '' )
			# file systems possibly mounted in rcS (and hence
			# single user mode.
			continue
			;;
		* )
			if [ "$HOST" ]; then
				thishost=`echo $dev | /usr/bin/cut -f1 -d: -s`
				if [ "$HOST" != "$thishost" ]; then
					continue
				fi
			fi
			if [ "$FSType" -a "$FSType" != "$fstype" ]; then
				continue
			fi
			if [ "$LFLAG" -a "$fstype" = "nfs" ]; then
				continue
			fi
			if [ "$RFLAG" -a "$fstype" != "nfs" ]; then
				continue
			fi
			if [ ${kill} ]; then
				/usr/sbin/fuser -c -k $mountp
				/usr/bin/sleep 2
			fi
			if [ "$SFLAG" ]; then
				/sbin/umount ${mountp}
				continue
			fi
			#
			# Ok, then we want to umount this one in parallel
			#
			fslist="$fslist $mountp"	# put them in reverse
							# mnttab order
		esac
	done

	if [ "$fslist" ]; then
		/sbin/umount -a $fslist
	fi
)

