#! /bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)dcb.sh        1.29     99/09/30 SMI

# build (devconf boot) install floppy set for Solaris x86.

MEDIUM=diskette
MKFS=/usr/lib/fs/pcfs/mkfs
MNT=/mnt
MNTCMD='be_root "mount -f pcfs ${DRIVE} ${MNT} > /dev/null 2>&1"; sleep 5'
UMNTCMD='be_root "umount ${MNT} > /dev/null 2>&1"'

# error() is designed to be easily added where command result checking
# with optional reporting is required.  E.g. unprotected command:
#	cp $src $dest
# Command with error detection:
#	cp $src $dest || error
# Command with error detection and reporting:
#	cp $src $dest || error copying $src to $dest
# The presence or absence of errors will be reported on completion
# of this script and the exit value set accordingly.
error()
{
	if [ ! -z "$1" ]
	then
		echo "Error $*."
	fi
	ERRORSFOUND=YES
}

finish()
{
	eval ${UMNTCMD}
}

usage()
{
echo "
Usage:  ${SCRIPT} [-w <pkg_ws>] [-d <drv_num>] [-u] [-c] [-a] [-i] [-y] 
		[-D <blockdev>] [-U] [-v]

	   -w   pointer to a colon-separated list of package dirctories
	        contaning all realmode packages needed to build the DCB
		boot diskette. This will overwrite the value of REALMODE_PKGDIRS
		set in the environment file.

	   -d   Cause creation of diskettes. <drv_num>, e.g. 0 to 9, is
		the drive number of the device where the DCB boot diskette
		will be created.

	   -u   Update the devconf boot proto tree BOOT/proto_dcb/$RELEASE

	   -c   Compress the devconf boot proto tree BOOT/proto_dcb/$RELEASE
		All files with the extensions .bef, .exe, and .txt are
		compressed.

	   -a   Assume destination diskette(s) is/are preformatted.  Skip
		the low-level format.

	   -i	Build a diskette image file in BOOT/proto_dcb/$RELEASE
		rather than writing to a real diskette.

	   -y	Don't ask if diskette is in drive.	

	   -D   Cause creation of partition. <blockdev>, is
		the block device where the DCB boot partition
		will be created.

	   -U   Uncompress the devconf boot proto tree BOOT/proto_dcb/$RELEASE.
		All files with the extensions .bef, .exe, and .txt are
		uncompressed.

	   -V	Use volume management to format and write diskettes.
"
	exit 1
}

be_root () {
	# $1 = cmd to execute as root
	echo "Please enter the root password to execute the command:\n\t$1"
	su root -c "/bin/sh -c \"$1\""
}

makedcbdirs () {
	# Make necessary subdirs off given root
	for dir in $DCB_SUBDIR_NOT_IN_PKGS; do
		mkdir -p $1/$dir || error creating directory $1/$dir
	done
}

makeprotodir () {
	if [ ! -d "$1" ] ; then
	    echo "Creating a new proto directory: $1."
	    mkdir -p $1 || error creating directory $1
	else
	    echo "Updating an existing proto directory: $1."
	    rm -fr $1/*
	fi
	makedcbdirs $1
}

assemfloppy () {

	[ ! -d "$INS_BASEDIR" ] && mkdir $INS_BASEDIR
	be_root "$BASEDIR/proc_pkg"
	cd $INS_BASEDIR/boot
	echo "Installing all the components of floppy $1"
	for part in `find . -depth -print`
		do
			# Try to remove destination if it exists
		  	if [ -r $CPD/$part ] && [ -f $CPD/$part ]
		  	then
				rm -f $CPD/$part
		  	fi

			# Report error if destination still exists, otherwise
			# proceed with the copy then make the file writeable
			if [ -r $CPD/$part ] && [ -f $CPD/$part ]
			then
				error removing old copy of file $CPD/$part
			else
				find $part -print|cpio -pdumBc $CPD > /dev/null 2>&1
			fi
		done
	cd $BASEDIR
	be_root "rm -rf $INS_BASEDIR"
	for f in $RC_FILES_NOT_IN_PKGS
	do
		cp $f $CPD/$DCB_SUBDIR_NOT_IN_PKGS
	done 
	for f in $FILES_NOT_IN_PKGS
	do
		cp $f $CPD
	done
	for f in $REPLACE_FILES_IN_BOOT
	do
		cp $f $CPD/solaris
	done
	cp $ONPROTO/$BOOT_BIN $CPD/solaris
}

identfloppy () {
#
#	Create identification file for floppy
#
	WHAT_IT_IS=$1
	ident=$CPD/ident

	echo "Creating the dcb copyright/ident file."
	# identification
	sed -e "/^#ident/d" \
	    -e "s/RELEASE_NAME/$COPYRT_RELNM_DCB/" \
	    -e "s/DISK_NAME/$COPYRT_DSKNM_DCB/" \
	    -e "s/DU_NUM //" \
	    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" \
	    -e "/^PART_NUM/d" $TMPL_COPY > $ident
	echo " " >> $ident
	sed -e "/^#ident/d" \
	    -e "s/DISK_NAME/$COPYRT_DSKNM_DCB/" \
	    -e "s/DU_NUM //" \
	    -e "s/WHAT_IT_IS/$WHAT_IT_IS/" $TMPL_IDENT >> $ident
	date >> $ident
}

assembleprotos () {

	volno=1
	while (compare=`expr $volno \<= $DCB_VOLUMES`)
	do
		flp=$VOLNAMETAG$volno
		CPD=$PD/$flp
		makeprotodir $CPD
		assemfloppy $flp
		identfloppy $flp
		volno=`expr $volno + 1`
	done

	# Make the files to be put on the diskette writeable
	echo "Make the files writable ...."
	find $PD -depth -type f -exec chmod 775 {} \;

	# Create the workspace checkpoint file, its ws_list derivative (used
	# in making the source delivery), and patch the strap.rc file
	# with relevant release info.
	$BASEDIR/chkpt -d $PD
}

compressprotos () {

	volno=1
	mapfile=$PD/$VOLNAMETAG$volno/solaris.map
	while (compare=`expr $volno \<= $DCB_VOLUMES`)
	do
		flp=$VOLNAMETAG$volno
		CPD=$PD/$flp

		cd $CPD
		find . \( -name '*.bef' -o -name '*.exe' -o -name '*.txt' \) \
			-print > /tmp/clist.$$
		for f in `cat /tmp/clist.$$`; do
			if (compress $f); then
				mv $f.Z $f
			else
				echo "Leaving $f uncompressed."
			fi
		done

		volno=`expr $volno + 1`
	done
	rm -f /tmp/clist.$$
}

uncompressprotos () {

	volno=1
	mapfile=$PD/$VOLNAMETAG$volno/solaris.map
	while (compare=`expr $volno \<= $DCB_VOLUMES`)
	do
		flp=$VOLNAMETAG$volno
		CPD=$PD/$flp

		cd $CPD
		find . \( -name '*.bef' -o -name '*.exe' -o -name '*.txt' \) \
			-print > /tmp/clist.$$
		for f in `cat /tmp/clist.$$`; do
			echo $f
			file $f | if grep 'ompr' > /dev/null 2>&1
			then
				mv $f $f.Z
				if uncompress $f > /dev/null 2>&1
				then
					:
				else
					mv $f.Z $f
				fi
			fi
		done

		volno=`expr $volno + 1`
	done
	rm -f /tmp/clist.$$
}

insert_diskette () {
	if [ "$y_FLAG" -o "$i_FLAG" -o "$D_FLAG" ] ; then return ; fi
	echo "\nPlease insert a blank diskette into drive $drv_num."
	echo Press enter to create DCB diskette in drive $drv_num.
	read x
	eval ${UMNTCMD}
}

remove_diskette () {
	eval ${UMNTCMD}
	if [ "$i_FLAG" ] ; then return ; fi
	echo "\nPlease remove the DCB diskette from drive $drv_num."
}

writeprotos () {

	bootable='yes'  #  Only the first floppy will be made bootable
	volno=1
	while (compare=`expr $volno \<= $DCB_VOLUMES`)
	do
	    flp=$VOLNAMETAG$volno
	    CPD=$PD/$flp
	    DOSDR=${CPD}_image
	    export DOSDR

	    # Patch solaris.map with appropriate volume name.
	    # 
	    # Recent changes have caused us not to need a second
	    # volume, so this stuff is effectively a NO-OP right
	    # now.  I left it so we could use it later, though,
	    # when the inevitable happens and we have to split
	    # the floppy into multiple volumes again.
	    #
	    if [ "$bootable" = "yes" ]
	    then
		cat $CPD/solaris.map | \
		    sed 's/:SUNOSBEFS\(.*\):/:'$RELEASE'_'$VOLNAMETAG'\1:/' \
		    > /tmp/map.$$

		mv /tmp/map.$$ $CPD/solaris.map
	    fi

	    # Print the disk space requirements
	    $BASEDIR/ds $CPD
	    insert_diskette $DRIVE

	    if [ "$Skipformat" = "no" ]
	    then
		echo "Formatting $MEDIUM."
		rc=1
		# Format the disk DOS style
		until [ "$rc" -eq 0 ] ; do
		    if [ "$i_FLAG" != "" ]
		    then
			rc=0
		    elif [ "$D_FLAG" != "" ]
		    then
			rc=0
		    else
		    	fdformat -Ufvd ${RDRIVE}
		    	rc=`expr $?`
		    fi
		done
	    fi

	    # Put mdboot and strap.com on the first diskette
	    if [ "$bootable" = "yes" ]
	    then
		echo 'Installing SunSoft devconf boot sector';
		if [ "$i_FLAG" != "" ]
		then
			rm -f $DOSDR
			echo y | $MKFS -F pcfs -o S,s,B=$CPD/mdboot,b=$RELEASE'_'$flp,i=$CPD/strap.com,f=$DOSDR
			chmod 664 $DOSDR

			# This mcd command sets a state file to prevent
			# getting a warning on every mtools command that
			# the state file is out of date.  Hide the output
			# because the warning occurs here too.
			mcd i:/ > /dev/null 2>&1

		elif [ "$D_FLAG" != "" ]
		then
			be_root "$MKFS -F pcfs -o S,s,B=$CPD/mdboot,b=$RELEASE'_'$flp,i=$CPD/strap.com ${RDRIVE}"
		else
			if [ "$V_FLAG" != "" ]
			then
				# This grotty hack forces the unmount
				# under vold without invalidating the name.
				# The subshell and redirection are for
				# hiding the "nnnn Killed" message.
				( fdformat -U ${RDRIVE} & \
				  k=$! ; \
				  sleep 5 ; \
				  kill -9 $k ; \
				  sleep 1 ) > /dev/null 2>&1
			fi
			$MKFS -F pcfs -o S,s,B=$CPD/mdboot,b=$RELEASE'_'$flp,i=$CPD/strap.com ${RDRIVE}
		fi
	    else
		if [ "$D_FLAG" = "" ]
		then
			$MKFS -F pcfs -o b=$RELEASE'_'$flp ${RDRIVE}
		fi
	    fi

	    echo Copying files to $MEDIUM

	    # Mount the DOS diskette
	    eval ${MNTCMD}

	    # Copy the entire contents of the prototype tree to the diskette
	    # with the exception of mdboot and strap.com which have already
	    # been used.  Changing directory to the prototype tree allows the
	    # code to run in the same shell which makes error reporting
	    # work properly.
	    OWD=`pwd`
	    cd $CPD
	    find . -type d -print | while read D
	    do
		if [ "$i_FLAG" != "" ]
		then
		    mmd i:/$D || error creating directory $D in diskette image
		elif [ ! -d $MNT/$D ]
		then
		    mkdir $MNT/$D || error creating directory $D on diskette
		fi
	    done
	    find . -type f -print | egrep -v "mdboot|strap\.com" | while read F
	    do
		if [ "$i_FLAG" != "" ]
		then
		    mcopy $F i:/$F || error copying file $F to diskette image
		else
		    if [ -r $MNT/$F ]
		    then
		        rm -f $MNT/$F
		    fi
		    if [ -r $MNT/$F ]
		    then
		        error removing old copy of file $F from diskette
		    else
		        cp $F $MNT/$F || error copying file $F to diskette
		        chmod 664 $MNT/$F
		    fi
		fi
	    done
	    cd $OWD

	    echo
	    echo Label this $MEDIUM '"'$RELEASE'_'$flp'"\c'
	    if [ "$i_FLAG" != "" ]
	    then
		echo ":\n$DOSDR"
	    else
		echo
	    fi
	    remove_diskette $DRIVE
	    bootable='no'
	    volno=`expr $volno + 1`
	done
}

boot () {
	# Root of hierarchy into which DOS-built modules are copied.
	PROTO=$BOOT_DIR/proto_dcb
	PD=$PROTO/$RELEASE

	if [ "$u_FLAG" ] ; then
		echo "Working in DCB BOOT proto area."
		assembleprotos
	fi

	if [ "$c_FLAG" ] ; then
		echo "Compressing .BEFs, .EXEs, and .TXTs"
		compressprotos
	fi

	if [ "$U_FLAG" ] ; then
		echo "Uncompressing .BEFs, .EXEs, and .TXTs"
		uncompressprotos
	fi

	if [ "$d_FLAG" -o "$i_FLAG" ] ; then
		writeprotos
	fi
}

#
# Main
#
PATH=$PATH:/usr/ccs/bin:/usr/local/bin
export PATH

ERRORSFOUND=NO

SCRIPT=`basename ${0}`

Skipformat='no'
unset d_FLAG i_FLAG u_FLAG c_FLAG y_FLAG D_FLAG U_FLAG V_FLAG

while getopts acw:d:is:uyD:UV FLAG ; do
	case $FLAG in
	a)  Skipformat='yes'
	    ;;
	c)  c_FLAG=1
	    ;;
	w)  REALMODE_PKGDIRS=$OPTARG
	    export REALMODE_PKGDIRS
	    ;;
	d)  d_FLAG=1
	    drv_num=`echo $OPTARG | sed -n 's/^[^0-9]*\([0-9]\)$/\1/p'`
		[ ! "$drv_num" ] && usage
		if [ "$V_FLAG" != "" ]
		then
		        DRIVE="/vol/dev/aliases/floppy${drv_num}"
		        RDRIVE="/vol/dev/aliases/floppy${drv_num}"
		else
			DRIVE="/dev/diskette${drv_num}"
			RDRIVE="/dev/rdiskette${drv_num}"
		fi
		trap "finish; exit 0" 0 1 2 3 15
	    ;;
	i)  MNTCMD=``
	    UMNTCMD=``
	    MEDIUM='diskette image'
	    i_FLAG=1
	    ;;
	s)  Dbdir=$OPTARG
	    [ ! "$Dbdir" ] && usage
	    [ ! -d $Dbdir ] && usage
	    ;;
	u)  u_FLAG=1      # Update prior to making diskettes
	    ;;
	y)  y_FLAG=1
	    ;;
	D)  d_FLAG=1
	    D_FLAG=1
	    drv_num="$OPTARG"
		[ ! "$drv_num" ] && usage
		DRIVE="${drv_num}:boot"
		RDRIVE="`echo $DRIVE | sed 's/dsk/rdsk/'`"
		trap "finish; exit 0" 0 1 2 3 15
	    ;;
	U)  U_FLAG=1
	    ;;
	V)  V_FLAG=1
		MNT=/floppy/floppy0
		MNTCMD='eject -p > /dev/null 2>&1; volcheck > /dev/null 2>&1; sleep 5'
		UMNTCMD='eject -p > /dev/null 2>&1'
	        DRIVE="/vol/dev/aliases/floppy${drv_num}"
	        RDRIVE="/vol/dev/aliases/floppy${drv_num}"
	    ;;
	\?) usage
	    ;;
    esac
done

Dbdir=${Dbdir:=default}

. `pwd`/dcb.env

[ ! "$RELEASE" ] && \
	echo "Database file incomplete: RELEASE variable not set." && \
	exit 1 ;

[ -z "$REALMODE_PKGDIRS" ] && echo "REALMODE_PKGDIRS variable not set. Use -w option.\n" && usage

[ ! "$ONPROTO" ] && \
	echo "Database file incomplete: ONPROTO variable not set." && \
	exit 1 ;

# Be careful that the value assigned to VOLNAMETAG does not lengthen
# the volume name beyond 11 characters (the DOS file name length
# limit).  This variable is combined with $RELEASE and $vol, e.g.,
# $RELEASE_$VOLNAMETAG_$volno.
VOLNAMETAG=${VOLNAMETAG:=d}

shift `expr $OPTIND - 1`

case $1 in [Uu][Ss][Aa][Gg][Ee]) usage
		  ;;
esac

if [ ! "$d_FLAG" -a ! "$i_FLAG" -a ! "$u_FLAG" -a ! "$c_FLAG" ] ; then
    echo "$SCRIPT: Nothing to do. "
    echo "Specify at least one of -c, -d, -i or -u flags."
    usage
fi

boot

if [ "$D_FLAG" != "" ] ; then
	echo << EOFN
Enabling the solaris boot partition requires that the fdisk
be run to make the solaris boot partition active.
EOFN

fi

if [ "$ERRORSFOUND" != NO ]
then
	echo "Script completed but one or more errors were detected."
	exit 1
fi

echo
echo "Script finished with no errors."
exit 0
