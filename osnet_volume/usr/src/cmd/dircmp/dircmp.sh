#!/usr/bin/ksh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#	Copyright (c) 1996, 1999 by Sun Microsystems, Inc.
#	All rights reserved.
#ident	"@(#)dircmp.sh 1.16	99/08/24 SMI"	/* SVr4.0 1.13  */
PATH=/usr/bin
USAGE="usage: dircmp [-d] [-s] [-wn] dir1 dir2"
trap "rm -f /usr/tmp/dc$$*;exit" 1 2 3 15
typeset -i exitstat=0
typeset -i sizediff
typeset -i cmpdiff
typeset -i Sflag=0
typeset -i Dflag=0
typeset -i fsize1
typeset -i fsize2
typeset -l LFBOUND=2147483648
width=72

#
# function to generate consistent "diff" output whether or not files are intact
#
function dodiffs {

	type=`LC_MESSAGES=C file $D1/"$a"`
	case "$type" in
		*text)  ;;
		*script) ;;
		*empty*) echo $D1/`basename "$a"` is an empty file |
			  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
        	*cannot*) echo $D1/`basename "$a"` does not exist |
			  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
        	*)	echo $D1/`basename "$a"` is an object file |
		   	  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
	esac
	type=`LC_MESSAGES=C file $D2/"$a"`
	case "$type" in
        	*text)  ;;
        	*script) ;;
        	*empty*) echo $D2/`basename "$a"` is an empty file |
			  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
        	*cannot*) echo $D2/`basename "$a"` does not exist |
			  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
        	*)	echo $D2/`basename "$a"` is an object file |
			  pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
			continue
        	;;
	esac
	#   
	# If either is a "large file" use bdiff (LF aware),
	# else use diff.
	#
	if (( fsize1 < LFBOUND && fsize2 < LFBOUND ))
	then cmd="diff"
	else cmd="bdiff"
	fi
	($cmd $D1/"$a" $D2/"$a"; echo $? > /usr/tmp/dc$$status) | \
	    pr -h "diff of $a in $D1 and $D2" >> /usr/tmp/dc$$g
	if [[ `cat /usr/tmp/dc$$status` != 0 ]]
	then exitstat=$diffstat
	fi
}
#
# dircmp entry point
#
while getopts dsw: i
do
	case $i in
	d)	Dflag=1;; 
	s)	Sflag=1;; 
	w)	width=`expr $OPTARG + 0 2>/dev/null`
		if [ $? = 2 ]
		then echo "dircmp: numeric argument required"
			exit 2
		fi
		;;
	\?)	echo $USAGE
		exit 2;;
	esac
done
shift `expr $OPTIND - 1`
#
D0=`pwd`
D1=$1
D2=$2
if [ $# -lt 2 ]
then echo $USAGE
     exit 1
elif [ ! -d "$D1" ]
then echo $D1 not a directory !
     exit 2
elif [ ! -d "$D2" ]
then echo $D2 not a directory !
     exit 2
fi
#
# find all dirs/files in both directory hierarchies. Use "comm" to identify
# which are common to both hierarchies as well as unique to each.
# At this point, print those that are unique.
#
cd $D1
find . -print | sort > /usr/tmp/dc$$a
cd $D0
cd $D2
find . -print | sort > /usr/tmp/dc$$b
comm /usr/tmp/dc$$a /usr/tmp/dc$$b | sed -n \
	-e "/^		/w /usr/tmp/dc$$c" \
	-e "/^	[^	]/w /usr/tmp/dc$$d" \
	-e "/^[^	]/w /usr/tmp/dc$$e"
rm -f /usr/tmp/dc$$a /usr/tmp/dc$$b
pr -w${width} -h "$D1 only and $D2 only" -m /usr/tmp/dc$$e /usr/tmp/dc$$d
rm -f /usr/tmp/dc$$e /usr/tmp/dc$$d
#
# Generate long ls listings for those dirs/files common to both hierarchies.
# 
sed -e s/..// < /usr/tmp/dc$$c > /usr/tmp/dc$$f
rm -f /usr/tmp/dc$$c
#
# Use -lgn to avoid problem when user or group names are too long, causing
# expected field separator to be missing
# Avoid other potential problems by piping through sed:
#  - Remove: Spaces in size field for block & character special files
#      '71, 0' becomes '710'
#  - For file name, do not print '-> some_file'
#      '/tmp/foo -> FOO' becomes '/tmp/foo'

cat /usr/tmp/dc$$f | xargs ls -lLgnd | \
  sed -e '/^[bc]/ s/, *//' -e '/^l/ s/ -> .*//' > /usr/tmp/dc$$i 2>/dev/null
cd $D0
cd $D1
cat /usr/tmp/dc$$f | xargs ls -lLgnd | \
sed -e '/^[bc]/ s/, *//' -e '/^l/ s/ -> .*//' > /usr/tmp/dc$$h 2>/dev/null
cd $D0
> /usr/tmp/dc$$g
#
# Process the results of the 'ls -lLgnd' to obtain file size info
# and identify a large file's existence.
#
while read -u3 tmp tmp tmp fsize1 tmp tmp tmp a &&
      read -u4 tmp tmp tmp fsize2 tmp tmp tmp b; do
	#
	# A window of opportunity exists where the ls -lLgnd above
	# could produce different
	# results if any of the files were removed since the find command.
	# If the pair of reads above results in different values (file names) for 'a'
	# and 'b', then get the file pointers in sync before continuing, and display
	# "different" message as customary.
	#
	if [[ $a != $b ]]; then
	while [[ $a < $b ]]; do
		if (( Sflag != 1 ))
		then echo "different	$a"
		dodiffs
		fi
		read -u3 tmp tmp tmp fsize1 tmp tmp tmp a
	done
	while [[ $a > $b ]]; do
		if (( Sflag != 1 ))
		then echo "different	$b"
		dodiffs
		fi
		read -u4 tmp tmp tmp fsize2 tmp tmp tmp b
	done
	fi
	cmpdiff=0
	sizediff=0
	if [ -d $D1/"$a" ]
	then if (( Sflag != 1 ))
	     then echo "directory	$a"
	     fi
	elif [ -f $D1/"$a" ]
	then 
	     #
	     # If the file sizes are different, then we can skip the run
	     # of "cmp" which is only used to determine 'same' or 'different'.
	     # If the file sizes are the same, we still need to run "cmp"
	     #
	     if (( fsize1 != fsize2 ))
	     then
		sizediff=1
	     else
		cmp -s $D1/"$a" $D2/"$a"
		cmpdiff=$?
	     fi
	     if (( sizediff == 0 && cmpdiff == 0 ))
	     then if (( Sflag != 1 ))
		  then echo "same     	$a"
		  fi
	     else echo "different	$a"
		  if (( Dflag == 1 ))
		  then
			dodiffs
		  fi
	     fi
	elif (( Sflag != 1 ))
	then echo "special  	$a"
	fi
done 3</usr/tmp/dc$$h 4</usr/tmp/dc$$i | pr -r -h "Comparison of $D1 $D2"
if (( Dflag == 1 ))
then cat /usr/tmp/dc$$g
fi
rm -f /usr/tmp/dc$$*
exit $exitstat

