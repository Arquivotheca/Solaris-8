#! /bin/sh
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# ident	"@(#)info.sh	1.8	95/08/22 SMI"
#

Allflag=yes

Usage="usage: info [-r] [-s] filename"
set -- `getopt xrs $*`
if [ $? -ne 0 ] ; then
	echo $Usage
	exit 1
fi
for Arg in $*
do
	case $Arg in
	-x) set -x ; shift ;;
	-r) Allflag=no; Relocflag=yes ; shift ;;
	-s) Allflag=no; Stringflag=yes ; shift ;;
	--) shift ; break ;;
	-*) echo $Usage ; exit 1 ;;
	esac
done   

Filename=$1
if [ "$Filename" = "" ] ; then
	echo $Usage ; exit 1
fi
Machine=`uname -p`
if [ "$Machine" = "sparc" ] ; then
	Pnum=21
elif [ "$Machine" = "i386" ] ; then
	Pnum=7
elif [ "$Machine" = "ppc" ] ; then
	Pnum=21
else
	echo "info: unknown machine type: $Machine"
	exit 1
fi 

# Determine the number of relocations

if [ "$Allflag" = "yes" -o "$Relocflag" = "yes" ] ; then
	echo
	echo "$Filename relocations:"
	Snum=`elfdump -e $Filename | grep e_shnum | awk '{ print $6 }'`
	dump -r $Filename | nawk -v snum=$Snum -v pnum=$Pnum '
		BEGIN {
			total = 0;	# Total relocs
			symbolic = 0;	# Symbol relocs (not including plt)
			plt = 0;	# plt relocs
		}
		$1 ~ /^0x/ {
			if ($3 != 0) {
				if ($2 >= snum) {
					if ($3 == pnum)
						plt++;
					else
						symbolic++
				}
				total++;
			}
		}
		END {
			printf("           Symbolic  Non-symbolic  Total\n");
			printf(" Start-up =  %3d	%3d	   %4d\n",
				symbolic, total - symbolic - plt, total - plt);
			printf(" Plt      =  %3d		   %4d\n",
				plt, plt);
			printf("				   %4d\n",
				total)
		} '
fi

# Determine the strings used. On 5.0 this reflects .rodata, .rodata1, .data,
# and .data1.  On 4.1 this reflects all strings, including .comments.

if [ "$Allflag" = "yes" -o "$Stringflag" = "yes" ] ; then
	echo
	echo "$Filename strings:"
	strings -6 $Filename | sort | uniq -c | sort -rn
fi
