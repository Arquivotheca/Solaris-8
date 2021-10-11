#!/bin/ksh
#
#
# Copyright (c) 2000 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)build_osnet.sh	1.1	00/09/14 SMI"
#
# This script can be used to build the ON component of the source product.
# It should _not_ be used by developers, since it does not work with
# workspaces, or do many of the features that 'nightly' uses to help you
# (like detection errors and warnings, and send mail on completion).
# 'nightly' (and other tools we use) lives in usr/src/tools.
#
# examine arguments. Source customers probably use no arguments, which just
# builds in usr/src from the current directory. They might want to use
# the -B flag, but the others are for use internally for testing the
# compressed cpio archives we deliver to the folks who build the source product.
#
USAGE='build_osnet [-B dir] [-E export_archive ] [-C crypt_archive ]
	[ -H binary_archive ] [-D]

where:
    -B dir              - set build directory
    -E export_archive   - create build directory from export_archive
    -C crypt_archive    - extract crypt_archive on top of build area
    -H binary_archive   - extract binary_archive on top of build area
    -D                  - do a DEBUG build
'

BUILDAREA=`pwd`
EXPORT_CPIO=
CRYPT_CPIO=
BINARY_CPIO=
DEBUGFLAG="n"

OPTIND=1
while getopts B:E:C:D:H: FLAG
do
	case $FLAG in
	  B )	BUILDAREA="$OPTARG"
		;;
	  E )	EXPORT_CPIO="$OPTARG"
		;;
	  C )	CRYPT_CPIO="$OPTARG"
		;;
	  H )	BINARY_CPIO="$OPTARG"
		;;
	  D )	DEBUGFLAG="y"
		;;
	 \? )	echo "$USAGE"
		exit 1
		;;
	esac
done


# extract source

# verify you are root
/usr/bin/id | grep root >/dev/null 2>&1
if [ "$?" != "0" ]; then
	echo \"$0\" must be run as root.
	exit 1
fi

if [ ! -z "${EXPORT_CPIO}" -a ! -f "${EXPORT_CPIO}" ]; then
	echo "${EXPORT_CPIO} does not exist - aborting."
	exit 1
fi

if [ -z "${BUILDAREA}" ]; then
	echo "BUILDAREA must be set - aborting."
	exit 1
fi

if [ -z "${SPRO_ROOT}" ]; then
	echo "SPRO_ROOT is not set - cannot set default as it must point"
	echo "to a directory that contains the compilers in an SC5.0"
	echo "subdirectory."
	exit 1
fi

if [ -z "${JAVA_ROOT}" ]; then
	echo "JAVA_ROOT is not set - defaulting to /usr/java1.2."
	JAVA_ROOT=/usr/java1.2;		export JAVA_ROOT
fi

# in case you use dmake. Note that dmake on ON has only been
# tested in parallel make mode.
if [ -z "${DMAKE_MAX_JOBS}" ]; then
	DMAKE_MAX_JOBS=4;
	export DMAKE_MAX_JOBS
fi
DMAKE_MODE=parallel; export DMAKE_MODE

################################################################
# Uncomment the line below to change to a parallel make using
# dmake. Be sure to put a "#" in front of the other make line.
# SC5.0's dmake can help create builds much faster, though if
# you have problems you should go back to serial make.
################################################################
#MAKE=dmake;				export MAKE
MAKE=/usr/ccs/bin/make;				export MAKE

# set magic variables

MACH=`uname -p`;			export MACH
ROOT="${BUILDAREA}/proto/root_${MACH}";	export ROOT
SRC="${BUILDAREA}/usr/src";		export SRC
PKGARCHIVE="${BUILDAREA}/packages/${MACH}";	export PKGARCHIVE
UT_NO_USAGE_TRACKING="1";		export UT_NO_USAGE_TRACKING
RPCGEN=/usr/bin/rpcgen;			export RPCGEN
STABS=/tmp/opt/onbld/bin/sparc/stabs;	export STABS
TMPDIR=/tmp;				export TMPDIR
ENVLDLIBS1=
ENVLDLIBS2=
ENVLDLIBS3=
ENVCPPFLAGS1=
ENVCPPFLAGS2=
ENVCPPFLAGS3=
ENVCPPFLAGS4=
export ENVLDLIBS3 ENVCPPFLAGS1 ENVCPPFLAGS2 ENVCPPFLAGS3 ENVCPPFLAGS4
unset RELEASE RELEASE_DATE

ENVLDLIBS1="-L$ROOT/usr/lib -L$ROOT/usr/ccs/lib"
ENVCPPFLAGS1="-I$ROOT/usr/include"

export ENVLDLIBS1 ENVLDLIBS2

export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
export RELEASE_BUILD ; RELEASE_BUILD=
unset EXTRA_OPTIONS
unset EXTRA_CFLAGS

if [ "${DEBUGFLAG}" = "y" ]; then
	unset RELEASE_BUILD
fi

unset CFLAGS LD_LIBRARY_PATH LD_OPTIONS

PATH="/opt/SUNWspro/bin:/usr/ccs/bin:/usr/bin:/usr/sbin"
export PATH

if [ -z "${ROOT}" ]; then
	echo "ROOT must be set - aborting."
	exit 1
fi

if [ -z "${PKGARCHIVE}" ]; then
	echo "PKGARCHIVE must be set - aborting."
	exit 1
fi

if [ -d "${BUILDAREA}" ]; then
	if [ -z "${EXPORT_CPIO}" ]; then
		# clobber doesn't work on the free source product,
		# since it will destroy the preinstalled object modules
		# so we just comment it out for now
		echo "\n==== Not clobbering in ${BUILDAREA} ====\n"
		#echo "\n==== Clobbering in ${BUILDAREA} ====\n"
		#cd $SRC
		#rm -f clobber.out
		#/bin/time ${MAKE} -e clobber | tee -a clobber.out
		#find . -name SCCS -prune -o \
		#    \( -name '.make.*' -o -name 'lib*.a' -o -name 'lib*.so*' -o \
		#    -name '*.o' \) \
		#    -exec rm -f {} \;

	else
		echo "\n==== Removing ${BUILDAREA} ====\n"
		sleep 15
		rm -rf ${BUILDAREA}
	fi
fi

if [ -d "${ROOT}" ]; then
	echo "\n==== Removing ${ROOT} ====\n"
	sleep 15
	rm -rf ${ROOT}
fi

if [ -d "${PKGARCHIVE}" ]; then
	echo "\n==== Removing ${PKGARCHIVE} ====\n"
	sleep 15
	rm -rf ${PKGARCHIVE}
fi

mkdir -p ${BUILDAREA}

cd ${BUILDAREA}

if [ ! -z "${EXPORT_CPIO}" ]; then
	echo "\n==== Extracting export source ====\n"
	zcat ${EXPORT_CPIO} | cpio -idmucB
fi

# hack
if [ -d usr/src/cmd/sendmail ]; then
	VERSION="Source"
else
	VERSION="MODIFIED_SOURCE_PRODUCT"
fi

if [ ! -z "${CRYPT_CPIO}" -a -f "${CRYPT_CPIO}" ]; then
	echo "\n==== Extracting crypt source ====\n"
	zcat ${CRYPT_CPIO} | cpio -idmucB
	VERSION="Source:Crypt"
	echo "\n==== Performing crypt build ====\n"
elif [ ! -z "${BINARY_CPIO}" -a -f "${BINARY_CPIO}" ]; then
	echo "\n==== Extracting binary modules ====\n"
	zcat ${BINARY_CPIO} | cpio -idmucB
	VERSION="MODIFIED_SOURCE_PRODUCT"
	echo "\n==== Performing hybrid build ====\n"
else
	VERSION="Source:Export"
	echo "\n==== Performing export build ====\n"
fi
export VERSION

echo "\n==== Disk space used (Source) ====\n"

cd ${BUILDAREA}
/usr/bin/du -s -k usr/src

mkdir -p ${ROOT}
mkdir -p ${PKGARCHIVE}

echo "\n==== Building osnet tools ====\n"
rm -rf /tmp/opt
cd $SRC/tools;
rm -f install.out
/bin/time env ROOT=/tmp ${MAKE} -e install | tee -a install.out
PATH="/tmp/opt/onbld/bin:/tmp/opt/onbld/bin/${MACH}:$PATH"
export PATH

echo "\n==== Build environment ====\n"
env

if [ "${DEBUGFLAG}" = "y" ]; then
	echo "\n==== Building osnet (DEBUG) ====\n"
else
	echo "\n==== Building osnet ====\n"
fi

cd $SRC
rm -f install.out
/bin/time ${MAKE} -e install | tee -a install.out

echo "\n==== Build errors ====\n"

egrep ":" install.out | \
	egrep -e "(${MAKE}:|[ 	]error[: 	\n])" | \
	egrep -v warning

echo "\n==== Building osnet packages ====\n"
cd $SRC/pkgdefs
rm -f install.out
/bin/time ${MAKE} -e install | tee -a install.out

echo "\n==== Package build errors ====\n"

egrep "${MAKE}|ERROR|WARNING" $SRC/pkgdefs/install.out | \
	grep ':' | \
	grep -v PSTAMP

echo "\n==== Disk space used (Source/Build/Packages) ====\n"

cd ${BUILDAREA}
/usr/bin/du -s -k usr/src proto packages

