#!/bin/ksh -p
#
#ident	"@(#)bldenv.sh	1.5	00/10/13 SMI"
#
# Copyright (c) 1998, 2000 by Sun Microsystems, Inc.
# All rights reserved.
#
# Uses supplied "env" file, based on /opt/onbld/etc/env, to set shell variables
# before spawning a C shell for doing a release-style builds interactively
# and incrementally.
#
USAGE='Usage: bldenv [-fd] [ -S E|C|D ] <env_file>

Where:
	-f	Invoke csh with -f
	-d	Setup a DEBUG build (default: non-DEBUG)
        -S      Build a variant of the source product
		E - build exportable source
		C - build crypt source
		D - build domestic source (exportable + crypt)
'

f_FLAG=n
d_FLAG=n
SE_FLAG=n
SH_FLAG=n
SD_FLAG=n

OPTIND=1
while getopts dfS: FLAG
do
	case $FLAG in
	  f )	f_FLAG=y
		;;
	  d )	d_FLAG=y
		;;
	  S )
		if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
			echo "Can only build one source variant at a time."
			exit 1
		fi
		if [ "${OPTARG}" = "E" ]; then
			SE_FLAG=y
		elif [ "${OPTARG}" = "D" ]; then
			SD_FLAG=y
		elif [ "${OPTARG}" = "H" ]; then
			SH_FLAG=y
		else
			echo "$USAGE"
			exit 1
		fi
		;;
	  \?)	echo "$USAGE"
		exit 1
		;;
	esac
done

# correct argument count after options
shift `expr $OPTIND - 1`

# test that the path to the environment-setting file was given
if [ $# -ne 1 ]
then
	echo "$USAGE"
	exit 1
fi

# force locale to C
LC_COLLATE=C;   export LC_COLLATE
LC_CTYPE=C;     export LC_CTYPE
LC_MESSAGES=C;  export LC_MESSAGES
LC_MONETARY=C;  export LC_MONETARY
LC_NUMERIC=C;   export LC_NUMERIC
LC_TIME=C;      export LC_TIME

# clear environment variables we know to be bad for the build
unset LD_OPTIONS LD_LIBRARY_PATH LD_AUDIT LD_BIND_NOW LD_BREADTH LD_CONFIG
unset LD_DEBUG LD_FLAGS LD_LIBRARY_PATH_64 LD_NOVERSION LD_ORIGIN 
unset LD_LOADFLTR LD_NOAUXFLTR LD_NOCONFIG LD_NODIRCONFIG LD_NOOBJALTER 
unset LD_PRELOAD LD_PROFILE  
unset CONFIG
unset GROUP
unset OWNER
unset REMOTE

# setup environmental variables
if [ -f $1 ]; then
	. $1
else
	if [ -f /opt/onbld/env/$1 ]; then
		. /opt/onbld/env/$1
	else
		echo "Cannot find env file as either $1 or /opt/onbld/env/$1"
		exit 1
	fi
fi

#MACH=`uname -p`

OPTIND=1
while getopts ABDFNPTCGIRainlmptuUxdrzWS: FLAG $NIGHTLY_OPTIONS
do
	case $FLAG in
	  S )
		if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
			echo "Can only build one source variant at a time."
			exit 1
		fi
		if [ "${OPTARG}" = "E" ]; then
			SE_FLAG=y
		elif [ "${OPTARG}" = "D" ]; then
			SD_FLAG=y
		elif [ "${OPTARG}" = "H" ]; then
			SH_FLAG=y
		else
			echo "$USAGE"
			exit 1
		fi
		;;
	  *)    ;;
	esac
done

echo "Build type   is  \c"
if [ ${d_FLAG} = "y" ]; then
	echo "DEBUG"
	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	unset RELEASE_BUILD
	unset EXTRA_OPTIONS
	unset EXTRA_CFLAGS
else
	# default is a non-DEBUG build
	echo "non-DEBUG"
	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	export RELEASE_BUILD ; RELEASE_BUILD=
	unset EXTRA_OPTIONS
	unset EXTRA_CFLAGS
fi

if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" ]; then
        if [ -z "${EXPORT_SRC}" ]; then
                echo "EXPORT_SRC must be set for a source build."
                exit 1
        fi
        if [ -z "${CRYPT_SRC}" ]; then
                echo "CRYPT_SRC must be set for a source build."
                exit 1
        fi
fi

if [ "$SH_FLAG" = "y" ]; then
        if [ -z "${EXPORT_SRC}" ]; then
                echo "EXPORT_SRC must be set for a source build."
                exit 1
        fi
fi
 
# Append source version
if [ "$SE_FLAG" = "y" ]; then
        VERSION="${VERSION}:EXPORT"
	SRC=${EXPORT_SRC}/usr/src
fi
 
if [ "$SD_FLAG" = "y" ]; then
        VERSION="${VERSION}:DOMESTIC"
	SRC=${EXPORT_SRC}/usr/src
fi

if [ "$SH_FLAG" = "y" ]; then
        VERSION="${VERSION}:HYBRID"
	SRC=${EXPORT_SRC}/usr/src
fi
 
# 	Set PATH for a build
PATH="/opt/onbld/bin:/opt/onbld/bin/${MACH}:/opt/SUNWspro/bin:/opt/teamware/ParallelMake/bin:/usr/ccs/bin:/usr/bin:/usr/sbin:/usr/ucb:/usr/etc:/usr/openwin/bin:/opt/sfw/bin:."
if [ "${SUNWSPRO}" != "" ]; then 
	PATH="${SUNWSPRO}/bin:$PATH" 
	export PATH 
fi 

CH=
TMPDIR="/tmp"

export	PATH CH TMPDIR
unset	CFLAGS LD_LIBRARY_PATH

# a la ws
ENVLDLIBS1=
ENVLDLIBS2=
ENVLDLIBS3=
ENVCPPFLAGS1=
ENVCPPFLAGS2=
ENVCPPFLAGS3=
ENVCPPFLAGS4=

ENVLDLIBS1="-L$ROOT/usr/lib -L$ROOT/usr/ccs/lib"
ENVCPPFLAGS1="-I$ROOT/usr/include"
MAKEFLAGS=e

export ENVLDLIBS1 ENVLDLIBS2 ENVLDLIBS3 \
	ENVCPPFLAGS1 ENVCPPFLAGS2 ENVCPPFLAGS3 \
	ENVCPPFLAGS4 MAKEFLAGS

echo "RELEASE      is  $RELEASE"
echo "VERSION      is  $VERSION"
echo "RELEASE_DATE is  $RELEASE_DATE"
echo ""

if [ "$f_FLAG" = "y" ]; then
	echo "The following csh will be invoked with -f by design."
	echo "Manually set your aliases and other customizations with care."
	exec csh -f
else
	echo "The following csh was not invoked with -f."
	echo "Hope your aliases and other customizations do not"
	echo "conflict with the build environment."
	exec csh
fi
