#!/usr/bin/ksh
#
#ident	"@(#)intf_create.ksh	1.13	95/11/09 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Create an interface verification database.
#
# A sweep is made over the output of a build ($ROOT) to locate all shared
# objects.  These belong to one of two groups:
#
#   o	the compilation environment
#
#	These shared objects have a ".so" suffix and are suitable for use
#	by the link-editor (ld(1)) using the "-l" option.  These objects
#	should be symbolic links to the associated runtime environment
#	version of the shared object.
#
#   o	The runtime environment
#
#	These shared objects have a ".so.X" suffix, where "X" is a version
#	number.  These objects are suitable for binding by the runtime
#	linker (ld.so.1), either as explict dependencies or via dlopen(3x).
#	It is only necessary for a runtime shared object to have an
#	associated compilation environment symlink if the object is used
#	during the link-edit of other dynamic images.
#
# Using the above file list a number of validations are carried out and
# databases created:
#
#   o	Compilation environment files are validated to insure that they
#	are symbolic links to their runtime environment counterparts.
#
#   o	Create a "Version Control Database".
#
#	pvs -don	>> $INTFDIR/$RELEASE
#
#	This output file contains the version control directives for all
#	compilation environment shared objects that make up this
#	consolidation.  This database allows users to build dynamic images
#	specifically to the interfaces offered by this release.
#	This file can be packaged and delivered as part of the consolidation.
#
#	Note: no SUNWprivate interfaces are recorded in this output.
#
#   o	Create "Version Definition Databases".
#
#	pvs -dovs	>  $INTFDIR/$AUDIT/$File
#
#	These output files, one for each versioned shared object, contain
#	the complete version definition to symbol mapping definitions.
#	These files are used to validate interfaces from release to release
#	and should not be delivered as part of the consolidation.


# Determine the versions available within each file, and for each version
# generate a complete symbol list (this includes any inheritance).  Reduce the
# output to make it more compact and suitable for intf_cmp.

dump_vers() {
	path=$1
	pvs -d $path | sed -e 's/;$//' | \
	while read vers
	do
		pvs -ds -N $vers $path | sed -e 's/;$//' |\
		while read sym size
		do
			case $sym in
			*:) ;;
			*) echo "$vers: $sym$size" ;;
			esac
		done
	done
}

usage()
{
	echo "usage: $Name [-d directory] [-r] [-v]"
	echo "  -v verbose output"
}


if [[ "X$ROOT" = "X" || "X$RELEASE" = "X" ]] ; then
	echo "ROOT, and RELEASE environment variables must be set"
	exit 1
fi

Intfdir=usr/lib/interfaces
Audit=audit
Verbose=no
Reductions=no
Error=0
Name="$0"

while getopts "d:rvx" Arg
do
	case $Arg in
	d) Intfdir=$OPTARG ;;
	r) Reductions=yes ;;
	x) set -x ;;
	v) Verbose=yes ;;
	\?) usage ; exit 1 ;;
	esac
done
shift `expr $OPTIND - 1`

# Build up a list of shared objects, catching both the compilation environment
# and runtime environment names.

cd $ROOT
if [[ ! -d $Intfdir/$Audit ]] ; then
	mkdir -p $Intfdir/$Audit
fi

Tmpfile=$Intfdir/intf-$$
rm -f $Tmpfile
touch $Tmpfile
if [[ $? -ne 0 ]] ; then
	exit 1
fi

Sofiles=/tmp/intf-$$

trap "rm -f $Sofiles $Tmpfile; exit 0" 0 1 2 3 15

Srchdirs=" \
	usr/lib \
	usr/4lib \
	usr/ucblib "

Intfile=`basename $Intfdir`
find $Srchdirs -name $Intfile -prune -o \( -name '*.so*' -a ! -name ld.so \) \
	-print > $Sofiles
if [[ ! -s $Sofiles ]] ; then
	echo "ERROR: $Name: no files found"
	rm -f $Sofiles
	exit 1
fi


# From this list of files extract the compilation environment names.

if [[ $Verbose = "yes" ]] ; then
	echo "Searching for compilation environment versioned shared objects"
fi


# Cleanup old audit files.

rm -f $Intfdir/$Audit/*

for Path in `grep '.so$' $Sofiles`
do
	# Make sure these are symbolic links to the appropriate runtime
	# environment counterpart.

	if [[ ! -h $Path ]] ; then
		echo " WARNING: $Path: file is not a symbolic link"
		Error=1
	fi

	# Determine if this file has any version definitions and if so add
	# them to the consolidation file.

	pvs -don $Path 	>>	$Tmpfile
	if [[ $? -eq 0 ]] ; then
		if [[ $Verbose = "yes" ]] ; then
			echo " $Path"
		fi
	else
		if [[ $Verbose = "yes" ]] ; then
			echo " WARNING: $Path: no versions found"
		fi
	fi
done

# Convert the relative names of the version control directives to full paths.
# Note: any SUNWprivate interfaces are also stripped from the output.

if [[ -f $Tmpfile ]] ; then
	fgrep -v SUNWprivate_ < $Tmpfile | sed -e "s/^/\//" > $Intfdir/$RELEASE
	rm -f	$Tmpfile
fi


# From this list of files extract the runtime environment names.

if [[ $Verbose = "yes" ]] ; then
	echo
	echo "Searching for runtime environment versioned shared objects"
fi

for Path in `fgrep '.so.' $Sofiles`
do
	# Determine if the file has any version definitions, and if so obtain
	# the symbol associations.

	pvs -d $Path		>	/dev/null
	if [[ $? -eq 0 ]] ; then
		if [[ $Verbose = "yes" ]] ; then
			echo " $Path"
		fi
		File=`basename $Path`
		rm -f			  	$Intfdir/$Audit/$File
		dump_vers $Path >		$Intfdir/$Audit/$File
		if [[ $Reductions = "yes" ]] ; then
			rm -f		  	$Intfdir/$Audit/$File-REDUCED
			pvs -dosl -N _REDUCED_ $Path \
				2> /dev/null  >	$Intfdir/$Audit/$File-REDUCED
		fi
	else
		if [[ $Verbose = "yes" ]] ; then
			echo " WARNING: $Path: no versions found"
		fi
	fi
done

# Create the SYSVABI specific version control definitions.  Here we know
# exactly what shared objects are defined for this ABI.  In this case convention
# dictates that the ABI version name is SYSVABI_1.3;

Version=SYSVABI_1.3

Files=" \
	usr/lib/libc.so.1 \
	usr/lib/libsys.so.1 \
	usr/lib/libnsl.so.1"

if [[ $Verbose = "yes" ]] ; then
	echo
	echo "Searching for $Version versioned shared objects"
fi

for Path in $Files
do
	if [[ -f $Path ]] ; then
		pvs -do -N $Version $Path	>> $Tmpfile
		if [[ $? -eq 0 ]] ; then
			if [[ $Verbose = "yes" ]] ; then
				echo " $Path"
			fi
		else
			echo " ERROR: $Path: no $Version versions found"
			Error=1;
		fi
	else
		echo " ERROR: $Path: file not found"
		Error=1
	fi
done

# Convert the relative names of the version control directives to full paths
 
if [[ -f $Tmpfile ]] ; then
	sed -e "s/^/\//"	< $Tmpfile	> $Intfdir/$Version
	rm -f	$Tmpfile
fi

if [[ `uname -p` != "sparc" ]] then
	exit $Error
fi

# Create the SISCD specific version control definitions.  Here we know
# exactly what shared objects are defined for this ABI.  In this case convention
# dictates that the ABI version name is SISCD_2.3.

Version=SISCD_2.3

Files=" \
	usr/lib/libc.so.1 \
	usr/lib/libdl.so.1 \
	usr/lib/libsys.so.1 \
	usr/lib/libnsl.so.1 \
	usr/lib/libsocket.so.1 \
	usr/lib/libthread.so.1 \
	usr/lib/libaio.so.1"

if [[ $Verbose = "yes" ]] ; then
	echo
	echo "Searching for $Version versioned shared objects"
fi

for Path in $Files
do
	if [[ -f $Path ]] ; then
		pvs -do -N $Version $Path	>> $Tmpfile
		if [[ $? -eq 0 ]] ; then
			if [[ $Verbose = "yes" ]] ; then
				echo " $Path"
			fi
		else
			echo " ERROR: $Path: no $Version versions found"
			Error=1;
		fi
	else
		echo " ERROR: $Path: file not found"
		Error=1
	fi
done

# Convert the relative names of the version control directives to full paths
 
if [[ -f $Tmpfile ]] ; then
	sed -e "s/^/\//"	< $Tmpfile	> $Intfdir/$Version
	rm -f	$Tmpfile
fi

exit $Error
