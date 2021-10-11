#!/usr/bin/ksh
#
#ident	"@(#)intf_cmp.ksh	1.4	96/03/27 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# intf_cmp will audit two interface definition databases (as created by
# intf_create) against one another and confirm that:
#
#  o	All versioned libraries that were present in the `previous' interface
#	are present in the `new' interface
#
#  o	for each non-private interface in a library confirm that no symbols
#	have been removed and that no symbols have been added to it between
#	the two revisions
#
#  o	If the -p option is specified, private interfaces are audited also.
#
# A typical example of running this verification would be:
#
#	intf_cmp -p /ws/on-old/proto/root_$MACH/usr/lib/interfaces \
#		    /ws/on-new/proto/root_$MACH/usr/lib/interfaces
#
# Return codes:
#
#  0	All interfaces in the new release are identical in old release.
#  1	Something is different refer to the error messages.


usage()
{
	echo "usage: $Name [-apv] <new_release_dir> <old_release_dir>"
	echo "  -a show any interface additions"
	echo "  -p audit private interfaces also"
	echo "  -v verbose output"
}

check()
{
	dir=$1
	if [[ ! -d $dir ]]; then
		echo "$dir: not a directory"
		usage
		exit 1
	fi
}

normalize()
{
	dir=$1
	if [[ `expr "$dir" : "\/"` = "0" ]]; then
		pwd=`pwd`
		dir="$pwd/$dir"
	fi
	echo $dir
}

compare()
{
	Newfile=$1
	Oldfile=$2
	nawk -v Newfile=$Newfile -v Oldfile=$Oldfile -v Private=$Private \
	    -v Addition=$Addition '
	BEGIN {
		FS=":";
		error=0;
		file=Newfile;
		sub(/.*\//, "", file);
		while (getline < Oldfile) {
			if (!Private && match($0, /SUNWprivate_*/) > 0)
				continue;
			interface[$0]="1";
			version[$1]="1";
		}
		close(Oldfile)

		while (getline < Newfile) {
			if (!Private && (match($0, /SUNWprivate_*/) > 0))
				continue;
			if (!($1 in version)) {
				if (Addition)
					printf("   NEW: %s: %s\n", file, $0);
				continue;
			}
			if ($0 in interface) {
				interface[$0]=2;
				continue;
			}
			printf(" ERROR: %s: %s: has been added\n", file, $0);
			error=1
		}
		close(Newfile)
   
		for (name in interface) {
			if (interface[name] == 2)
				continue;
			printf(" ERROR: %s: %s: has been deleted\n", file, name);
			error=1
		}
		exit error
	}
	'
	if [[ $? != 0 ]]; then
		Error=1
	fi
}


Verbose=no
Private=0
Error=0
Addition=0
Name="$0"

while getopts "apvx" Arg
do
	case $Arg in
	a)  Addition=1 ;;
	p)  Private=1 ;;
	v)  Verbose=yes ;;
	x)  set -x ;;
	\?) usage ; exit 1 ;;
	esac
done
shift `expr $OPTIND - 1`

if [[ $# -ne 2 ]]; then
	usage
	exit 1
fi

Newdir=$1
Olddir=$2

check $Olddir
check $Newdir

Newdir=`normalize $Newdir`/audit
Olddir=`normalize $Olddir`/audit

cd $Newdir

# Locate all consolidation files within the specified new directory and compare
# them with the equivalent files in the old directory.

for File in `find . ! -name '*-REDUCED' -print`
do
	if [[ ! -f $File ]]; then
		continue
	fi
	if [[ "$Verbose" == "yes" ]]; then
		echo "Auditing $File"
	fi
	if [[ ! -f $Olddir/$File ]]; then
		echo " ERROR: $File does not exist in $Olddir"
		Error=1
		continue;
	fi
	compare $Newdir/$File  $Olddir/$File
done

exit $Error
