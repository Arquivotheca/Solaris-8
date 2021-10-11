#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)shareall.sh 1.10     97/04/09 SMI"        /* SVr4.0 1.5   */
# shareall  -- share resources

USAGE="shareall [-F fsys[,fsys...]] [- | file]"
fsys=
set -- `getopt F: $*`
if [ $? != 0 ]		# invalid options
	then
	echo $USAGE >&2
	exit 1
fi
for i in $*		# pick up the options
do
	case $i in
	-F)  fsys=$2; shift 2;;
	--)  shift; break;;
	esac
done

if [ $# -gt 1 ]		# accept only one argument
then
	echo $USAGE >&2
	exit 1
elif [ $# = 1 ]
then
	case $1 in
	-)	infile=;;	# use stdin
	*)	infile=$1;;	# use a given source file
	esac
else
	infile=/etc/dfs/dfstab	# default
fi


if [ "$fsys" ]		# for each file system ...
then
	while read line				# get complete lines
	do
		echo $line
	done < $infile |

	`egrep "^[^#]*[ 	][ 	]*-F[ 	]*(\`echo $fsys|tr ',' '|'\`)" |
	/sbin/sh`

	fsys_file=/etc/dfs/fstypes
	if [ -f $fsys_file ]    		# get default file system type
	then
		def_fs=`egrep '^[^#]' $fsys_file | awk '{print $1; exit}'`
		if [ "$def_fs" = "$fsys" ]      # if default is what we want ...
		then            		# for every file system ...
			while read line
			do
				echo $line
			done < $infile |

			# not a comment and no -F option
			`egrep -v "(^[#]|-F)" | /sbin/sh`
		fi
	else
		echo "shareall: can't open $fsys_file"
	fi
else			# for every file system ...
	cat $infile|/sbin/sh
fi
