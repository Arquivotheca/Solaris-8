#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)pkgkit.ksh 1.7 99/09/30

# build (devconf boot) hard drive boot area prototype.  Then create
# a package from the prototype.
#

usage()
{
echo "\
Usage:  ${SCRIPT} [-u] [-p]

	   -u   Update the proto area from which package will be constructed

	   -p   Construct the package

	   At least one of the option flags must be specified.
"
	exit 1
}

makebtpkgdirs () {
	# Make necessary subdirs off given root
	for dir in $BTPKG_SUBDIRS; do
		mkdir -p $1/$dir
	done
}

makeprotodir () {
	if [ ! -d "$1" ] ; then
	    echo "Creating a new proto directory: $1."
	    mkdir -p $1
	else
	    echo "Updating an existing proto directory: $1."
	    rm -fr $1/*
	fi
	makebtpkgdirs $1
}

assemproto () {
	cp $BOOT_IMAGE_PATH/d1_image ../../proto
	echoMe='$BTPKG_PARTS'
	eval PARTLIST=`echo $echoMe`
	
	for part in $PARTLIST; do
		echo "Installing $part component."

		echoMe='$BTPKG_'$part'_PROTO'
		eval PROTODIR=`echo $echoMe`

		echoMe='$BTPKG_'$part'_FILES'
		eval FILELIST=`echo $echoMe`

		echoMe='$BTPKG_'$part'_DEST'
		eval TODIR=`echo $echoMe`

		for f in $FILELIST; do
			cp $PROTODIR/$f $PROTO/$TODIR
		done

		echoMe='$BTPKG_'$part'_POST'
		eval POSTPROC=`echo $echoMe`
		[ "$POSTPROC" ] && $POSTPROC $CPD;
	done
}

assembleprotos () {

	makeprotodir $PROTO
	assemproto

	# Chown all of the files to be owned by root
	find $PROTO -depth -exec chown $UID {} \; 2>/dev/null
	find $PROTO -depth -exec chgrp sys {} \; 2>/dev/null
	find $PROTO -depth -type f -exec chmod 755 {} \; 2>/dev/null
}

pkgprotos () {

	PKGNAM=$1
	mkdir -p $PKG
	PKGBLDDIR=`pwd`

	# First build the awk_pkginfo program
	echo ./bld_awk_pkginfo -m $MACH -p "$RELEASE/$VERSION" -o awk_pkginfo
	./bld_awk_pkginfo -m $MACH -p "$RELEASE/$VERSION" -o awk_pkginfo

	# Use the freshly built program to create pkginfo file
	echo "nawk -f ./awk_pkginfo $PKGNAM/pkginfo.tmpl > $PKGNAM/pkginfo"
	nawk -f ./awk_pkginfo $PKGNAM/pkginfo.tmpl > $PKGNAM/pkginfo

	# Finish making the package protoinfo
	cd $PROTOROOT
	cp $PKGBLDDIR/$PKGNAM/protoinfo.tmpl $PKGBLDDIR/$PKGNAM/protoinfo
	find . -print | pkgproto >> $PKGBLDDIR/$PKGNAM/protoinfo

	# Make the package
	cd $PKGBLDDIR/$PKGNAM
	pkgmk -o -b $PKGBLDDIR/$PROTOROOT -d $PKGBLDDIR/$PKG -f protoinfo

	cd $PKGBLDDIR
}

buildit () {
	# Root of hierarchy into which DOS-built modules are copied.
	PKGLIST=SUNWbtx86
	PROTOROOT=bootpkg/$PKGRELEASE/proto
	PROTO=$PROTOROOT/boot
	PKG=bootpkg/$PKGRELEASE/pkg

	if [ "$U_flag" ] ; then
		echo "Working in BTPKG proto area."
		assembleprotos
	fi

	if [ "$P_flag" ] ; then
		echo "Constructing the package."
		for p in $PKGLIST; do
			pkgprotos $p
		done
	fi
}

#
# Main
#
PATH=$PATH:/usr/ccs/bin:
export PATH

SCRIPT=`basename ${0}`
unset U_flag P_flag

case $1 in [Uu][Ss][Aa][Gg][Ee]) usage
		  ;;
esac

while getopts up FLAG ; do
	case $FLAG in
	u)  U_flag=1
	    ;;
	p)  P_flag=1
	    ;;
	\?) usage
	    ;;
    esac
done

[ ! -f database ] && \
        echo "No database file ?" && \
        exit 1 ;

. database

if [ ! "$U_flag" -a ! "$P_flag" ] ; then
    echo "$SCRIPT: Nothing to do. Specify the -u flag and/or -p flag."
    usage
fi

[ ! "$PKGRELEASE" ] && \
	echo "Database file incomplete: PKGRELEASE variable not set." && \
	exit 1 ;

UID=`id | sed -e 's/^uid=//' -e 's/(.*//'`
if [ ${UID} -ne 0 ]
then
	echo "${SCRIPT}: You must be root to run this program." >&2
	exit 1
fi

RELEASE="5.8";			export RELEASE
VERSION=$PKGRELEASE;		export VERSION
MACH=`uname -p`;		export MACH

buildit
