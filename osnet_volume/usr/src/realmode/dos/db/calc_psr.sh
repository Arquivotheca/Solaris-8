#!/bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)calc_psr.sh        1.2     95/11/22 SMI 

# This script will copy the contents of Distribution Patches to a
# temporary space, calculates the 1KB_blocks and number of files. 

echo "Calculating Disk-space Allocation for Driver Update"
. ./database
cd PATCH/$DU_NUM
DPWD=`pwd`
for i in `ls -d *-*`
do
	cd $i
	for n in `ls -d SUNW*`
	do
		cd $n
		ARCH_SPEC=`grep ARCH pkginfo|sed 's/.*://'`
		for m in `find reloc -type f -depth -print`
		do
			DIR=`dirname $m`
			DIR_N=`echo $DIR |sed -e 's/.*reloc//' -e 's/share/usr\/share/'`
			FILE=`basename $m`
			PATCH_NUM=`echo $DIR|sed 's/\/.*//'`
			[ -d /var/tmp/sp_req/$DIR_N ]|| mkdir -p /var/tmp/sp_req/$DIR_N
			if [ $ARCH_SPEC"x" != $ARCH_FLAG"x" ]; then
				echo $ARCH_SPEC > /var/tmp/sp_req/$DIR_N/arch_file
				ARCH_FLAG=$ARCH_SPEC
			else
				touch	/var/tmp/sp_req/$DIR_N/arch_file
			fi
			cp $DIR/$FILE /var/tmp/sp_req/$DIR_N
			echo .\\c
		done
	cd .. 
	done
	cd $DPWD
done
cd /var/tmp/sp_req
[ -f /tmp/patch_space_reqd ] && rm /tmp/patch_space_reqd
for i in `find . -type d -print|sed 's/\.//'`
do
	cd /var/tmp/sp_req/$i
	# Do the actual calculation here using sp_req which does fstat
	# on all the required files. 
        [ -f arch_file ] && cat arch_file >> /tmp/patch_space_reqd &&  rm arch_file && ${BASEDIR}/sp_req * | sed 's/.*\/sp_req//' >> /tmp/patch_space_reqd 
        echo .\\c	
	cd /var/tmp/sp_req
done
echo "done"
cd $DPWD
DB_LOC=`echo $DPWD|sed 's/PATCH.*//'`
cp /tmp/patch_space_reqd $DB_LOC/BOOT/proto_du/$DU_NUM/SECOND/rc.d
rm -r /var/tmp/sp_req
rm /tmp/patch_space_reqd
