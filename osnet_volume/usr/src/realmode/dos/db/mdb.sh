#! /bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)mdb.sh	1.24	96/02/14 SMI

# make MDB (Multiple Device Boot) diskette

. ./database

PATH=$PATH:/usr/ccs/bin:
export PATH

[ ! "$RELEASE" ] && \
    ( echo "Database file incomplete: RELEASE variable not set." ; \
      exit 1 ;)

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
echo "\
Usage:  ${SCRIPT} [-d <drv_num>] [-u]

	   -d   Cause creation of diskettes. <drv_num>, e.g. 0 to 9, is
		the drive number of the device where the MDB boot diskette
		will be created.

	   -u   Update the MDB boot proto tree BOOT/proto_mdb/$RELEASE
"
	exit 1
}

unset d_FLAG u_FLAG

UID=`id | sed -e 's/^uid=//' -e 's/(.*//'`
DFLAG_MSG="
	*** Use of '-d' flag requires that you have ***
	*** the ability to become the super-user.   ***"

while getopts d:u FLAG ; do
    case $FLAG in d)  d_FLAG=1
		      if [ ${UID} -ne 0 ] ; then
			  echo "$DFLAG_MSG"
			  while : ; do
			      echo "\nContinue? (y/n) \c"
			      read ans
			      case $ans in n) exit ;; y) break ;; *) continue ;; esac
			  done
		      fi
		      drv_num=`echo $OPTARG | sed -n 's/^[^0-9]*\([0-9]\)$/\1/p'`
		      [ ! "$drv_num" ] && usage
		      DRIVE="/dev/diskette${drv_num}"
		      RDRIVE="/dev/rdiskette${drv_num}"
		      trap "umount_drv >/dev/null 2>&1; exit 0" 0 1 2 3 15
		      ;;
		  u ) u_FLAG=1      # Update prior to making diskettes
		      ;;
		 \? ) usage
		      ;;
    esac
done

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

boot () {
    WHAT_IT_IS='Boot'
    # Root of heirarchy into which DOS-built modules are copied.
    PROTO=$BOOT_DIR/proto_mdb
    unset pd
    # Get or create the proto directory for this release
    pd=$PROTO/$RELEASE
    if [ "$u_FLAG" ] ; then
	echo "Working in MDB BOOT proto area."
	if [ ! -d "$pd" ] ; then
	    echo "Creating a new proto directory: $pd."
	    mkdir_ifneeded -p $pd
	else
	    echo "Updating an existing proto directory: $pd."
	    rm -fr $pd/*
	fi
    fi
    ident=$pd/ident
    if [ "$u_FLAG" ] ; then
	# Copy each DOS-built module (dbm) into the MDB boot proto tree from
	# the proto staging area.
	echo "Copying the .bef files to $pd."
	for dbm in $DOS_BOOT_DRVRS_ALL ; do
	    cp $PROTO_DOS/$dbm $pd || exit 1
	done
	for dbm in $DOS_BOOTSTRAPS ; do
	    cp $PROTO_DOS/$dbm $pd
	done
	echo "Copying weird system administrative files into boot proto tree."
	for admloc in $MDB_ADMIN_LOCS ; do
	    cd $admloc
	    find . -depth -print | grep -v 'SCCS' | cpio -pdum $pd 2>/dev/null
	    cd ..
	done
	echo "Copying generic administrative files into boot proto tree."
	cd $GENERIC_ADMIN_LOC
	for admfile in $MDB_ADMIN_INST ; do
	    find $admfile -print | cpio -pdum $pd 2>/dev/null
	done
	cd ..

	echo "Creating the mdb copyright/ident file."
	# identification
	sed -e "/^#ident/d" \
	    -e "s/RELEASE_NAME/$COPYRT_RELNM_MDB/" \
	    -e "s/DISK_NAME/$COPYRT_DSKNM_MDB/" \
	    -e "s/DU_NUM //" \
	    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" \
	    -e "/^PART_NUM/d" $TMPL_COPY > $ident
	echo " " >> $ident
	sed -e "/^#ident/d" \
	    -e "s/DISK_NAME/$COPYRT_DSKNM_MDB/" \
	    -e "s/DU_NUM //" \
	    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" $TMPL_IDENT >> $ident
	date >> $ident
	# Chown all of the files to be put to diskette to one ID
	find $pd -depth -exec chown $UID {} \;
	# Create the workspace checkpoint file, its ws_list derivative (used
	# in making the source delivery), and patch the bootstrap binaries
	# with relevant release info.
	$BASEDIR/chkpt -d $pd mdb
    fi
    if [ "$d_FLAG" ] ; then
	# Print the disk space requirements
	ds mdb
	insert_diskette
	rc=1
	# Format the disk DOS style and install mdboot as the bootstrap
	until [ "$rc" -eq 0 ] ; do
	    fdformat -Ufvd -B $pd/mdboot ${RDRIVE}
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

	echo "Copying files to bootable DOS diskette."

	# Copy Composite File System versions of mdexec to the diskette
	# THIS FILE MUST BE THE FIRST FILE PUT ON THE DISKETTE.
	# DO NOT INCLUDE THIS FILE IN ANY OTHER COPY COMMAND!
	cp $pd/mdexec /mnt

	# Copy the BEF files, the copyright file (ident), and any other
	# stuff that's in the proto directory to the diskette.
	(cd $pd ; \
	    find . -type f -print | egrep -v "mdexec|mdboot" | \
	    cpio -pduvm /mnt 2>/dev/null ;)

	remove_diskette
    fi
}

umount_drv () {
    if `mount | egrep -s $DRIVE` ; then
	echo "\nUnmount diskette drive ${DRIVE} ..."
	rc=2
	while [ "$rc" -eq 2 ] ; do
	    [ "$UID" != 0 ] && echo "su root ..."
	    su root -c "umount $DRIVE > /dev/null 2>&1"
	    rc=$?
	done
    fi
}

insert_diskette () {
    echo "\nPlease insert a blank diskette into diskette drive $drv_num"
    echo "and press ENTER to create the MDB diskette."
    read x
    umount_drv
}

remove_diskette () {
    umount_drv
    echo "\nPlease remove the MDB diskette from drive $drv_num."
}

boot
