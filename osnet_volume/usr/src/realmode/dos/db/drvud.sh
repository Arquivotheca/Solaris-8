#!/bin/sh
#
# Copyright 1996 Sun Microsystems, Inc. 
# All Rights Reserved.
#
#ident "@(#)drvud.sh	1.50	96/02/08 SMI"
# 
#
# make Driver Update patch (cpio) and boot (custom) diskettes

. ./database
. ./du_env
PATCH_TOOLS=${PATCH_TOOLS:-/net/benet.eng/benet/Sustaining/PatchTools}
PATH=$PATH:/usr/ccs/bin:/opt/SUNWspro/bin:
export PATH

PATCH_IDS=`eval echo $PP_ALL $P_ALL | tr " " "\012" | sed -e 's/\/.*//' | tr "\012" " "`

for i in "$DU_NUM"           \
	 "$PART_NUM_FIRST_BOOT_3"  \
	 "$PART_NUM_SECOND_BOOT_5"  \
	 "$PART_NUM_FIRST_BOOT_3"  \
	 "$PART_NUM_SECOND_BOOT_5"  \
	 "$PART_NUM_PATCH_3" \
	 "$PART_NUM_PATCH_5" ; do
    [ ! "$i" ] && \
    ( echo "Database file incomplete: DU_NUM and/or PART_NUM variables not set." ; \
      exit 1 ;)
done

SCRIPT=`basename ${0}`

mkdir_ifneeded()
{
	if [ "$1" = "-p" ] 
	then
		P="-p"
		shift
	else
		P=""
	fi
	if [ ! -d $1 ]
	then
		mkdir $P $1
	fi
}

usage()
{
echo "\n\
Usage:  ${SCRIPT} [-a <alt_db>] [-d <drv_num>,<diskette_size>] [-f] [-p] [-u]

	   -a   Specify an alternate directory in which to create the
		BOOT and PATCH subdirectories containing the hierarchies
		to be put to diskette. By default, if the user does not
		have write permission in ./db, the BOOT and PATCH hierarchies
		will be created beneath /tmp/DU_db.

	   -d   Cause creation of diskettes. <drv_num>, e.g. 0 to 9, is
		the drive number of the device that will accept diskettes
		of size <diskette_size>, e.g., 3 or 5.  Requires that user
		have super-user privileges, either by being the super-user
		or becoming the super-user when prompted by ${SCRIPT}.

	   -f   Format patch distibution floppies before writing to them.
		(Boot floppies are always formatted DOS-style.)

	   -p   Implies -u, but only with regard to the Driver Update
		patch tree (PATCH); Driver Update boot proto tree
		BOOT/proto_du/$DU_NUM not updated.

	   -u   Update the Driver Update patch tree (PATCH/$DU_NUM) and
		Driver Update boot proto tree BOOT/proto_du/$DU_NUM

	Prerequisite: Patches corresponding to patch ID's

		      $PATCH_IDS

		      must exist within the following location(s):

		      $PATCH_LOCS
"
	exit 1
}

b_FLAG=1
unset a_FLAG d_FLAG f_FLAG u_FLAG

UID=`id | sed -e 's/^uid=//' -e 's/(.*//'`
DFLAG_MSG="
	*** Use of '-d' flag requires that you have ***
	*** the ability to become the super-user.   ***"

while getopts a:d:fpu FLAG ; do
    case $FLAG in
	a)  a_FLAG=1
	    ALT_DB=$OPTARG
	    ;;
	d)  d_FLAG=1
	    if [ ${UID} -ne 0 ] ; then
		echo "$DFLAG_MSG"
		while : ; do
		    echo "\nContinue? (y/n) \c"
		    read ans
		    case $ans in n) exit ;; y) break ;; *) continue ;; esac
		done
	    fi
	    drv_num=`echo $OPTARG | sed -n 's/^\([0-9]\),.*/\1/p'`
	    dsk_size=`echo $OPTARG | sed -n 's/^[0-9],\([35]\)$/\1/p'`
	    [ ! "$drv_num" -o ! "$dsk_size" ] && usage
	    DRIVE="/dev/diskette${drv_num}"
	    RDRIVE="/dev/rdiskette${drv_num}"
	    trap "umount_drv >/dev/null 2>&1; exit 0" 0 1 2 3 15
	    ;;
	f ) f_FLAG=1
	    ;;
	p ) unset b_FLAG  # Patch update only; no boot proto update
	    u_FLAG=1
	    ;;
	u ) u_FLAG=1      # Update prior to making diskettes
	    ;;
	? ) usage
		      ;;
    esac
done

ALT_DB=${ALT_DB:-/tmp/DU_db}
if `touch $ALT_DB/TOUCHED 2>/dev/null` ; then
    rm -f $ATL_DB/TOUCHED
else
    mv $ALT_DB ${ALT_DB}.$$ 2>/dev/null
fi

shift `expr $OPTIND - 1`

case $1 in [Uu][Ss][Aa][Gg][Ee]) usage
		  ;;
esac

if [ ! "$d_FLAG" -a ! "$u_FLAG" ] ; then
    echo "$SCRIPT: Nothing to do. Specify the -u flag and/or -d flag."
    usage
fi

update_check () {
if [ "$d_FLAG" -a ! "$u_FLAG" -a ! -d $1 ] ; then
    echo "$SCRIPT:\t$1 non-existent.\n\tRe-execute with the -u option.\n"
    usage
fi
}

TMP=/tmp/DU

patch () {
    WHAT_IT_IS='Patch Distribution'
    ACCESSIBLE_P=yes
    PATCH_SUBDIR=$PATCH_DIR/$DU_NUM
    if `touch $BASEDIR/TOUCHED 2>/dev/null` ; then
	rm -f $BASEDIR/TOUCHED
    else
	ACCESSIBLE_P=no
	PATCH_SUBDIR=$ALT_DB/`echo $PATCH_SUBDIR | sed -n 's/^.*\/db\/\(.*\)$/\1/p'`
    fi
    ident=$PATCH_SUBDIR/ident
    copyright=$PATCH_SUBDIR/copyright
    cpioimage=$PATCH_SUBDIR/cpioimage
    if [ "$u_FLAG" -a "$1" != -d ] ; then
        if [ -d $PATCH_SUBDIR ] ; then
            echo "\nRemoving old Driver Update PATCH area ..."
	    rm -fr $PATCH_SUBDIR
        fi
	echo "\nBuilding Driver Update PATCH area ..."
	echo "\tCopying Drivers patch to:\n\t\t$PATCH_SUBDIR."
	for pp in $PP_ALL ; do
	    # Get the actual value of the variable to which $pp is pointing.
	    var=`eval echo $pp`
	    # Get the root directory of the variable to which $pp is pointing.
	    NUM=`echo $var | sed -e 's/\/.*$//'`
	    # add this to the growing list in PATCHNUMS if not already there
	    expr "$PATCHNUMS" : ".*$NUM.*" >/dev/null || \
			PATCHNUMS="$PATCHNUMS $NUM"
		pd=""	
		for d in $PATCH_LOCS ; do
			if [ -d $d/$NUM ] ; then pd=$d ; fi
		done
	    if [ "$pd" = "" ] ; then
			echo "You're hosed. No patches with ID # $NUM found."
		exit 1
	    elif [ ! -d $pd/$var ] ; then
			echo "Patch #$NUM in $pd is incomplete."
		exit 1
	    else
			mkdir_ifneeded -p $PATCH_SUBDIR
			cp -r $pd/$NUM $PATCH_SUBDIR
			P_N=`basename $pd/$NUM`
 			rm -f $PATCH_SUBDIR/$P_N/installpatch
        		rm -f $PATCH_SUBDIR/$P_N/backoutpatch
	    fi
	done
	if [ ! -f $PATCH_SUBDIR/installpatch ] ; then
        	cp $PATCH_TOOLS/installpatch $PATCH_SUBDIR
		chmod +x $PATCH_SUBDIR/installpatch
        fi
	echo "\tCopying other patches to:\n\t\t$PATCH_SUBDIR."
	# less complex for P_ALL: just copy the whole thing
	for p in $P_ALL ; do
	    expr "$PATCHNUMS" : ".*$p.*" >/dev/null || \
			PATCHNUMS="$PATCHNUMS $p"
		pd=""	
		for d in $PATCH_LOCS ; do
			if [ -d $d/$p ] ; then pd=$d ; fi
		done
	    if [ -d $pd/$p ] ; then
			mkdir_ifneeded -p $PATCH_SUBDIR
			rm -fr $PATCH_SUBDIR/$p
			cp -r $pd/$p $PATCH_SUBDIR
			P_N=`basename $pd/$p`
                        rm -f $PATCH_SUBDIR/$P_N/installpatch
	    else
			echo "You're hosed. No patches with ID # $p found."
			usage
	    fi
	done
	

	# copyright files - need one for 3.5" and one for 5.25" diskettes
	echo "\tCreating the patch copyright files."
	for ds in 3 5 ; do
	    N=$ds
	    NN='$PART_NUM_PATCH_'`echo $N`
	    PART_NUM=`eval echo $NN`
	    sed -e "/^#ident/d" \
		-e "s/RELEASE_NAME/$COPYRT_RELNM_DU/" \
		-e "s/DISK_NAME/$COPYRT_DSKNM_DU/" \
		-e "s/DU_NUM/$DU_NUM/" \
		-e "s/PART_NUM/$PART_NUM/" \
		-e "s/WHAT_IT_IS/$WHAT_IT_IS/" $TMPL_COPY > "$copyright".$ds
	done

	# identification
	sed -e "/^#ident/d" \
	    -e "s/DISK_NAME/$COPYRT_DSKNM_DU/" \
	    -e "s/DU_NUM/$DU_NUM/" \
	    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" $TMPL_IDENT > $ident
	date >> $ident
	chmod 666 $ident

    echo "\tCreating the compressed cpio file containing the following patches:"
    echo "\t    $PATCHNUMS"
    ( cd $PATCH_SUBDIR ; \
     find $PATCHNUMS -depth -print | cpio -oc > $cpioimage 2>/dev/null \
       && rm -f "$cpioimage".Z && compress $cpioimage >/dev/null ;)
    # Pick up the patch distribution installation scripts
    for i in $PATCH_SCRIPTS ; do
      rm -f $PATCH_SUBDIR/$i
      cp $i $PATCH_SUBDIR
      chmod 755 $PATCH_SUBDIR/$i
	done
	# Chown all of the files to be put to diskette to one ID
	find $PATCH_SUBDIR -depth -exec chown $UID {} \;
    fi
    if [ "$d_FLAG" -a "$1" != -u ] ; then
	echo "\nCreating the Driver Update Patch Distribution Diskette ..."
	update_check $PATCH_SUBDIR
	bytes_to_archive=`wc -c "$cpioimage".Z                         \
				"$copyright".$dsk_size                 \
				$PATCH_SUBDIR/installdu.sh             \
				$PATCH_SUBDIR/installpatch	       \
				$PATCH_SUBDIR/ident | grep total |     \
				awk '{ print $1 }'`
	[ $dsk_size = 3 ] && bytes_per_diskette=1474560 || \
			     bytes_per_diskette=1229760
	diskettes_req=`echo $bytes_to_archive $bytes_per_diskette | \
				awk '{ printf( "%2.3f\n", $1 / $2 ) }'`
	whole_diskettes=`echo $diskettes_req | \
				sed -n 's/^\([^.]*\)\.\(.*\)$/\1/p'`
	diskette_fraction=`echo $diskettes_req | \
				sed -n 's/^\(.*\)\.\(.*\)$/\2/p'`
	total_diskettes=$whole_diskettes
	[ $diskette_fraction -gt 0 ] && total_diskettes=`expr $whole_diskettes + 1`
	echo "\t$total_diskettes formatted diskette(s) required for $WHAT_IT_IS ..."
	if [ "$f_FLAG" ] ; then
	    diskettes_left=$total_diskettes
	    which=1
	    umount_drv
	    until [ $diskettes_left -eq 0 ] ; do
		echo "\tInsert diskette $which of $diskettes_left in drive $drv_num and press Enter."
		read enter
		rc=1
		until [ "$rc" -eq 0 ] ; do
		    fdformat -fv ${RDRIVE}
		    rc=`expr $?`
		done
		echo "\tLabel disk:\tDriver Update $DU_NUM $WHAT_IT_IS\n\t\t$which of $total_diskettes."
		which=`expr $which + 1`
		diskettes_left=`expr $diskettes_left - 1`
	    done
	else
	    echo "\tLabel disks:\tDriver Update $DU_NUM $WHAT_IT_IS\n\t\tN of $total_diskettes."
	fi
	insert_diskette "$WHAT_IT_IS" $drv_num
	mkdir_ifneeded -p $TMP
	(   cd $PATCH_SUBDIR
	    cp copyright.$dsk_size $TMP/copyright
	    cp ident installdu.sh cpioimage.z installpatch $TMP && \
	    (   cd $TMP
		ls copyright ident installdu.sh cpioimage.z installpatch | \
		    cpio -ovcB -M "Place Driver Update $WHAT_IT_IS disk %d in drive $drv_num and press enter" -O $RDRIVE;
	    ) || \
	    { echo "Problem copying Driver Update patch files to:\n\t\t$TMP."; exit ;}
	)
	rm -fr $TMP
	remove_diskette "$WHAT_IT_IS" $drv_num
    fi
}

boot () {
    WHAT_IT_IS='Boot'
    ACCESSIBLE_B=yes
    # Root of heirarchy into which driver update files are copied.
    PROTO=$BOOT_DIR/proto_du
    unset pd
    pd=$PROTO/$DU_NUM
    if [ "$pd" ] ; then
	if `touch $BASEDIR/TOUCHED 2>/dev/null` ; then
	    rm -f $BASEDIR/TOUCHED
	else
	    ACCESSIBLE_B=no
	    tmp_pd=$ALT_DB/`echo $pd | sed -n 's/^.*\/db\/\(.*\)$/\1/p'`
	    tmp_dname=`dirname $tmp_pd`
	    pd=$tmp_dname/$DU_NUM
	fi
    fi
    [ "$pd" ] && pdDU_NUM=`basename $pd`
    if [ "$u_FLAG" -a "$1" != -d ] ; then
        if [ -d $PROTO/$DU_NUM ] ; then
	    echo "\nRemoving old Driver Update BOOT proto area ..."
	    rm -fr $PROTO/$DU_NUM
	fi
	echo "\nBuilding Driver Update BOOT proto area ..."
	if [ "$DU_NUM" != "$pdDU_NUM" -o ! "$pd" ] ; then
	    pd=$PROTO/$DU_NUM
	    if `touch $BASEDIR/TOUCHED 2>/dev/null` ; then
		rm -f $BASEDIR/TOUCHED
	    else
		ACCESSIBLE_B=no
		pd=$ALT_DB/`echo $pd | sed -n 's/^.*\/db\/\(.*\)$/\1/p'`
	    fi
	    echo "\tCreating a new proto directory:\n\t\t$pd."
	    mkdir_ifneeded -p $pd
	else
	    echo "\tUpdating an existing proto directory:\n\t\t$pd."
	    rm -fr $pd/*
	fi
    fi
    for TYPE in FIRST SECOND ; do
	mkdir -p $pd/$TYPE
	[ "$TYPE" = FIRST ] && type="REALMODE" || type="SOLARIS"
	ident=$pd/$TYPE/ident
	if [ "$u_FLAG" -a "$1" != -d ] ; then
	    echo "\tCopying the administrative files into boot proto tree."
	    for admloc in $ALL_ADMIN_LOCS ; do
		cd $admloc
		if [ "$TYPE" = FIRST ] ; then
			echo > $pd/$TYPE/diskette.001
			if [ -f renbef.bat ]
			then cp renbef.bat $pd/$TYPE
			fi
		elif [ "$TYPE" = SECOND ] ; then
			 find . -depth -print | grep -v 'SCCS' | cpio -pdu $pd/$TYPE
			echo "menu" > $pd/$TYPE/diskette.002
		fi
		cd ..
	    done
	    if [ "$TYPE" = SECOND ] ; then
	    	for script in $BOOT_SCRIPTS ; do
			find $pd/$TYPE -name `basename $script` -exec chmod 755 {} \;
	    	done
               # Create patch_space_reqd and copy it to rc. This is required 
               # for patch space allocation.
		${BASEDIR}/calc_psr
	    fi
	    # Copy each DOS-built module (dbm) into the DU boot proto tree from
	    # the proto staging area.
	    if [ "$TYPE" = FIRST ] ; then	
	    	echo "\tCopying the .bef files to:\n\t\t$pd/$TYPE."
	    	TYPE_BOOT='$DOS_BOOT_DRVRS_ALL'
	    	DOS_BOOT_DRVRS=`eval echo $TYPE_BOOT`
	    	for dbm in $DOS_BOOT_DRVRS ; do
			cp $PROTO_DOS/$dbm $pd/$TYPE || exit
	    	done
	    	for dbm in $DOS_BOOTSTRAPS ; do
			# After modification by the chkpt script, these are copied
			# on to the boot diskette as appropriate (either into
			# the boot sector (mdboot) or into the boot floppy PCFS
			# file system (mdexec)).
			cp $PROTO_DOS/$dbm $pd
	    	done
	    fi
	    # Copy the components of the various patched packages (PPNs) into
	    # the DU boot proto tree.
	    if [ "$TYPE" = SECOND ] ; then 
	    	echo "\tCopying the patch components to:\n\t\t$pd/$TYPE."
	    	for pp in $PP_ALL ; do
			# Get the actual value of the variable to which $pp is pointing.
			dir=`eval echo "$pp"`
			# Get patch ID #
			patch_id=`echo $dir | sed 's/^\([^/]*\)\/.*/\1/p'`
			# Now get the contents variable associated with the variable to
			# which $pp is pointing.
			contents_list_var="$pp"_CONTENTS
			# Finally, get the list of files associated with the variable
			# to which $pp is pointing.
			contents_list=`eval echo $contents_list_var`
			cd $PATCH_SUBDIR/$dir
			for entry in `echo "$contents_list"` ; do
		    	file=`echo $entry | sed -n 's/^\([^+=!]*\).*/\1/p'`
		    	strip=`echo $entry | sed -n 's/^[^!]*\!\(.*\)/\1/p'`
		    	add=`echo $entry | sed -n 's/^[^+]*\+\([^!]*\).*/\1/p'`
		    	chg=`echo $entry | sed -n 's/^[^=]*\=\([^!]*\).*/\1/p'`
		    	nf=$file
		    	[ -n "$strip" ] && nf=`echo $nf | sed -e "s=$strip=="`
		    	[ -n "$chg" ] && nf=`echo $nf | sed -e "s=\.[^.]*$=\.$chg="`
		    	[ -n "$add" ] && nf=`echo $nf | sed -e "s=$=\.$add="`
		    	mkdir_ifneeded -p $pd/$TYPE/`dirname $nf`
			cp $file $pd/$TYPE/$nf || \
				{ echo "Patch #$patch_id in $PATCH_SUBDIR not complete." ; exit ;}
			done
	    	done
		fi
	    	cd $BASEDIR
	    # If the time ever comes in which all the drivers won't fit on
	    # to one diskette, then a new method for creating boot diskettes
	    # will probably need to be created and the method to determine
	    # the number of disks processed vs the total number of disks
	    # required will certainly be tied to the new method used to
	    # create multiple boot diskettes.  Until that time arrives
	    # (and it may not before this whole DU build mechanism is
	    # re-engineered), to satisfy the part number mongers' desire
	    # for a numbering scheme at the top of the copyright file,
	    # the following brute-force numbering scheme will be used.
	    NUM_USED=1
	    NUM_REQD=1
	    NUM_IN_NUM="$NUM_USED in $NUM_REQD"
	    # identification - need one for 3.5" and one for 5.25" diskettes
	    echo "\tCreating the boot copyright (ident) files."
	    for ds in 3 5 ; do
		N=$ds
		NN='$PART_NUM_'`echo $TYPE`'_BOOT_'`echo $N`
		PART_NUM=`eval echo $NN`
		sed -e "/^#ident/d" \
		    -e "s/RELEASE_NAME/$COPYRT_RELNM_DU/" \
		    -e "s/DISK_NAME/$COPYRT_DSKNM_DU/" \
		    -e "s/DU_NUM/$DU_NUM/" \
		    -e "s/PART_NUM/$PART_NUM/" \
		    -e "s/WHAT_IT_IS/$WHAT_IT_IS $NUM_IN_NUM/" $TMPL_COPY > ${ident}.${TYPE}.${ds}
		echo " " >> ${ident}.${TYPE}.${ds}
		sed -e "/^#ident/d" \
		    -e "s/DISK_NAME/$COPYRT_DSKNM_DU/" \
		    -e "s/DU_NUM/$DU_NUM/" \
		    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" $TMPL_IDENT >> ${ident}.${TYPE}.${ds}
		date >> ${ident}.${TYPE}.${ds}
	    done
	    # Chown all of the files to be put to diskette to one ID
	    find $pd -depth -exec chown $UID {} \;
	fi
    done
    if [ "$u_FLAG" -a "$1" != -d ] ; then
	# Create the workspace checkpoint file, its ws_list derivative
	# (used in making the source delivery), and patch the bootstrap
	# binaries with relevant release info.  This is outside of the
	# previous for loop so that we don't get multiple checkpts for
	# the same run of drvud.
	if [ "$ACCESSIBLE_B" = "yes" ] ; then
	    echo
	    $BASEDIR/chkpt -d $pd du ; chkpt_rc=$?
	fi
	# Only the mdexec module is put into the file system on the boot
	# diskette.  Mdboot is put in the first sector of the diskette with
	# fdformat.
	if [ "$chkpt_rc" -ne 1 ] ; then
	    for TYPE in FIRST ; do
		cp $pd/mdexec $pd/$TYPE
	    done
	fi
    fi

    if [ "$d_FLAG" -a "$1" != -u ] ; then
	for TYPE in FIRST SECOND ; do
	    [ "$TYPE" = FIRST ] && type="REALMODE" || type="SOLARIS"
	    echo "\nCreating the Driver Update $type Boot Diskette ..."
	    update_check $pd
	    # Print the disk space requirements
	    $BASEDIR/ds du $type $dsk_size
	    insert_diskette "$type $WHAT_IT_IS" $drv_num
	    rc=1
	    # Format the disk DOS style and install mdboot as the bootstrap
	    until [ "$rc" -eq 0 ] ; do
	    # Format the 2nd disk DOS style but non-bootable
	        if [ "$TYPE" = SECOND ] ; then
			fdformat -fvd ${RDRIVE}
			DOS_LABEL=non-bootable
		elif [ "$TYPE" = FIRST ] ; then
			fdformat -fvd -B $pd/mdboot ${RDRIVE}
			DOS_LABEL=bootable
		fi
		rc=`expr $?`
	    done

	    # Mount the DOS diskette
	    echo "Mounting ${DRIVE} on to /mnt ..."
	    rc=2
	    while [ "$rc" -eq 2 ] ; do
		[ "$UID" != 0 ] && echo "su root ..."
		su root -c "mount -F pcfs ${DRIVE} /mnt"
		rc=$?
	    done

	    echo "\n\tCopying files to ${DOS_LABEL} DOS diskette."

	    # Copy mdexec to the diskette first (Order is important!!)
	    if [ "$TYPE" = FIRST ] ; then 
		echo mdexec
	    	cp $pd/$TYPE/mdexec /mnt
	    fi

	    # Copy all files, then directories from the proto directory to the
	    # diskette. Put the proper copyright notice (dictated by diskette
	    # size) on to the diskette.
	    for f in $pd/$TYPE/* ; do
		if [ ! -d $f ] ; then
		    case $f in
			*ident* ) [ `basename "$f"` = "ident.${TYPE}.${dsk_size}" ] && \
				  { basename $f ; cp $f /mnt/ident ;}
				  ;;
			      * ) basename $f
				  cp $f /mnt
				  ;;
		    esac
		fi
	    done
	    for f in $pd/$TYPE/* ; do
		if [ -d $f ] ; then
		    ls -R $f
		    cp -r $f /mnt
		fi
	    done
	    remove_diskette "$type $WHAT_IT_IS" $DRIVE
	done
    fi
}

umount_drv () {
    if `mount | egrep -s $DRIVE` ; then
	echo "\nUnmount diskette drive ($DRIVE) ..."
	rc=2
	while [ "$rc" -eq 2 ] ; do
	    [ "$UID" != 0 ] && echo "su root ..."
	    su root -c "umount $DRIVE > /dev/null 2>&1"
	    rc=$?
	done
    fi
}

insert_diskette () {
    echo "\nPlease insert a diskette into drive $2."
    echo Press ENTER to create Driver Update $1 diskette in drive $2.
    read x
    umount_drv
}

remove_diskette () {
    umount_drv
    echo "\nPlease remove the Driver Update $1 diskette from drive $2."
}

if [ ! "$b_FLAG" ] ; then
    patch
else
    if [ "$d_FLAG" ] ; then
	patch -u ; boot -u
	patch -d ; boot -d
    else
	patch ; boot
    fi
fi
