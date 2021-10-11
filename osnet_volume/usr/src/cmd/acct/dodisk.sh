#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)dodisk.sh	1.8	98/11/23 SMI"	/* SVr4.0 1.13	*/
# 'perform disk accounting'
PATH=:/usr/lib/acct:/usr/bin:/usr/sbin
export PATH
if [ -f  /usr/bin/uts -a /usr/bin/uts ]
then
	format="dev mnt type comment"
else 
	format="special dev mnt fstype fsckpass automnt mntflags"
fi
_dir=/var/adm
_pickup=acct/nite
set -- `getopt o $*`
if [ $? -ne 0 ]
then
	echo "Usage: $0 [ -o ] [ filesystem ... ]" >&2
	exit 1
fi
for i in $*; do
	case $i in
	-o)	SLOW=1; shift;;
	--)	shift; break;;
	esac
done

cd ${_dir}

if [ "$SLOW" = "" ]
then
	if [ $# -lt 1 ]
	then
		if [ -f  /usr/bin/uts -a /usr/bin/uts ]
		then
			DEVLIST=/etc/checklist
		else
			DEVLIST=/etc/vfstab
		fi
		while :
		do
			if read $format
		       	then
				if [ -z "$special" ]
				then
					continue
				elif [ `expr $special : '\(.\)'` = \# ]
		       		then
		               		continue
		        	fi
				if [ "$fsckpass" = "-" ]
				then
					continue
				fi
				if [ $fstype != ufs ] && [ $fstype != vxfs ]
				then
					continue
				fi

				# Make sure FS is mounted
				if egrep -s "^${special}[ 	]+${mnt}[ 	]" /etc/mnttab
				then
					find $mnt -mount -print | \
					acctdusg > `basename $dev`.dtmp &
				fi
			else
				wait
				break
			fi
		done < $DEVLIST
		if [ "`echo *.dtmp`" != "*.dtmp" ]
		then
			awk -e '
	{tot[$1] += $3; if (name[$1] == "") name[$1] = "" $2}
END	{for (i in tot) printf "%d\t%s\t%d\n", i, name[i], tot[i]}' *.dtmp > dtmp
			rm -f *.dtmp
		else
			> dtmp
		fi
	else
		find $* -mount -print | acctdusg > dtmp
	fi
else
	if [ $# -lt 1 ]
	then
		args="/"
	else
		args="$*"
	fi
	for i in $args; do
		if [ ! -d $i ]
		then
			echo "$0: $i is not a directory -- ignored" >&2
		else
			dir="$i $dir"
		fi
	done
	if [ "$dir" = "" ]
	then
		echo "$0: No data" >&2
		> dtmp
	else
		find $dir -print | acctdusg > dtmp
	fi
fi

sort +0n +1 dtmp | acctdisk > ${_pickup}/disktacct
chmod 644 ${_pickup}/disktacct
chown adm ${_pickup}/disktacct
