#!/bin/sh
#
#  Copyright (c) 1999 by Sun Microsystems, Inc.
#  All rights reserved.
#
#pragma ident	"@(#)inst9.sh	1.20	99/04/23 SMI"


PATH=${PATH}:/sbin:/usr/sbin:/usr/bin
export PATH

if [ "$BASEDIR" = "" ]; then
	BASEDIR=/a
fi

# Look at the device tree to determine if this is driver update install,
# and if so, dig out the arguments for install.sh.
# Assumptions:
#	- format of prtconf -pv to look at devinfo tree
#	- 'itu-props' is the device node created by the boot system
#	- 'drivers' is a property on the 'itu-props' node 
#	   containing a comma separated list of driver objects.


#
# print the 'drivers' property on the 'itu-props' device node.
#
extract_drivers()
{
	nawk -v ARG=$1 'BEGIN {foundnode=0;foundprop=0;} {
	if ($1 == "Node") {
		#reset search within node
		foundnode = 0;
		foundprop = 0;
	}
	if ($1 == "name:") {
		# get rid of single quotes
		name = substr($2, 2, length($2)-2);
		if ( name == "itu-props" ) {
			foundnode = 1;
		}
	}
	if (ARG != NULL) {
		prop = substr($1, 1, length($1)-1);
		if  (prop == ARG) {
			# get rid of single quotes
			drv_list = substr($2, 2, length($2)-2);
			foundprop = 1;
		}
	} else if ($1 == "drivers:") {
		# get rid of single quotes
		drv_list = substr($2, 2, length($2)-2);
		foundprop = 1;
	}
	if (foundnode && foundprop) {
		print(drv_list);
		exit 0;
	}
	}'
	exit 1;
}


#
# Install the driver objects in the argument list.
# Argument list consists of tuples: vol1 drv1 vol1 drv2 vol2 drv3 ...
# The 'vol' argument is the floppy label followed by the driver 
# name located on that floppy. 
#
# Assumptions:
#    The list contains all the drivers on a given volume before going
#    to the next volume so that the user doesn't have to shuffle floppies.
#    

auto=check			#attempt to perform automated installation

install_drv_list ()
{
	vol=""
	mounted=0
	while [ $# -gt 1 ]; do
		if [ X$vol != X$1 ]; then
			if [ "$auto" != check ]; then
				auto=no		#no automatic processing
				if [ $mounted -eq 1 ]; then
					umount /mnt
					mounted=0
				fi
				echo " "
				echo "Please insert the Driver Update diskette labeled $1"
				echo "Press <ENTER> when ready."
				read x
			fi

			mount -F pcfs -o ro /dev/diskette0 /mnt > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				if [ "$auto" = check ]; then
					auto=no		#no automatic processing
					continue
				fi
				echo "attempt to access floppy failed!"
				continue
			fi
			mounted=1

			# Newer Driver Update diskettes have the diskette
			# label name stored in a file called label.vol in the
			# DU directory. If present, check the contents to
			# verify that the diskette requested is the one
			# inserted. The label.vol file is created by
			# make_ITU_floppy.sh.

			# First, check to see that the diskette is a DU.
			if [ ! -d "/mnt/DU" ]; then
				umount /mnt
				mounted=0
				if [ "$auto" = check ]; then
					auto=no		#no automatic processing
					continue
				fi
				echo "Not a Driver Update diskette."
				continue
			fi

			# Look for label.vol file and compare label names.
			if [ -f "/mnt/DU/label.vol" ]; then
				# label.vol contents are uppercase, so convert
				# actual label name to uppercase for comparison
				label=`cat /mnt/DU/label.vol`
				uc_label=`echo $1 | tr "[a-z]" "[A-Z]"`
				if [ "X$uc_label" != "X$label" ]; then
					umount /mnt
					mounted=0
					if [ "$auto" = check ]; then
						auto=no	#no automatic processing
						continue
					fi
					echo "Wrong diskette label, $label."
					continue
				fi
			else
				drvdir=/mnt/DU/sol_$VERS/$MACH/Tools/Boot
				if [ ! -d "$drvdir/$drv" ]; then
					drvdir=/mnt/[D\|d][U\|u]/sol_${VERS}/$MACH/[T\|t]ools/[B\|B]oot
				fi
				drv="`basename $2`"
				if [ ! -d "$drvdir/$drv" ]; then
					umount /mnt
					mounted=0
					if [ "$auto" = check ]; then
						auto=no	#no automatic processing
						continue
					fi
					echo "Driver $drv not on diskette."
					continue
				fi
			fi
		fi

		vol=$1
		drv="`basename $2`"
		shift; shift

		# collect all drivers on this volume
		while [ $# -gt 1 -a "$vol" = "$1" ]; do
			drv="$drv `basename $2`"
			shift; shift
		done
		ICMD=/mnt/DU/sol_${VERS}/$MACH/Tools/install.sh
		if [ ! -x "${ICMD}" ]; then
			ICMD=/mnt/[D\|d][U\|u]/sol_${VERS}/$MACH/[T\|t]ools/install.sh
		fi

		if [ -x "${ICMD}" ]; then
			${ICMD} -I -R $BASEDIR $drv
		else
			echo ${ICMD} not found on $vol
		fi
		if [ "$auto" = check -a -f /mnt/DU/auto.itu ]; then
			auto=yes	#auto-processing should be allowed
		fi
	done
	if [ $mounted -eq 1 ]; then
		umount /mnt
	fi
}

#
# Check and install an old DU style cpio archive
#

install_old_drv()
{
	dd < /dev/diskette > /tmp/tmp.$$ count=1
	file /tmp/tmp.$$ | if grep 'cpio' > /dev/null 2>&1; then
		mkdir -p /$BASEDIR/tmp/Drivers
		cd /$BASEDIR/tmp/Drivers
		cpio -icduB -I /dev/rdiskette0 -M "Insert Old Driver Update Distribution diskette %d
		.  Press <ENTER> when ready"

		echo " "
		echo "Please remove the Old Driver Update diskette from drive zero."
		echo "Press <ENTER> when ready."
		read x

		odir=`pwd`
		cd /$BASEDIR/tmp/Drivers
		./installdu.sh /$BASEDIR
		cd $odir
		rm -rf /$BASEDIR/tmp/Drivers
		rm -f /tmp/tmp.$$
		return 1
	fi
	rm -f /tmp/tmp.$$
	return 0
}


# Reset modes to workaround upgrade problem with raw mode
modes="`stty -g < /dev/console`"
stty opost icrnl < /dev/console
(

echo Extracting driver list from tree..

VERS="`uname -r | sed -e's/\.//g' -e 's/5/2/'`"
MACH=`uname -m`
OPT=-pv
case $VERS in
        5|51) REL=old; OPT="-v";;
esac

cp $BASEDIR/etc/name_to_major /tmp/name_to_major.$$

#
# Some properties can be large enough to cause nawk to report errors.
# Use 'cut' to limit lines to 2000 characters.
#
DRV_LIST=`prtconf $OPT | cut -c1-2000 | extract_drivers`
DRV_NUM=`prtconf $OPT | cut -c1-2000 | extract_drivers itu-diskettes`
#
# convert list from comma separated to space separated
#
OLDIFS=$IFS
IFS=','
DRV_LIST=`echo $DRV_LIST`
IFS=$OLDIFS
echo $DRV_LIST

#
# give up if no ITU drivers or ITU diskettes have been processed
# if on new system.
if [ "$REL" != "old" -a "$DRV_LIST" = "" -a "$DRV_NUM" = "" ]; then
	exit 0
fi

if [ ! -z ${DRV_LIST:-""} ]; then
	install_drv_list $DRV_LIST
fi

if [ "$auto" = check ]; then
	mount -F pcfs -o ro /dev/diskette0 /mnt > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		if [ -f /mnt/DU/auto.itu ]; then
			auto=yes	#auto-processing should be allowed
		fi
		umount /mnt
	fi
fi

#prompt unless an automatic diskette was inserted
if [ "$auto" != yes ]; then
   while true; do
	echo " "
	echo "If you have additional Update diskettes to install"
	echo "(such as video), please insert diskette now."
	/usr/bin/echo "Additional Update diskettes to install? (y/n) [y] \c"
	read x
	if [ X$x = Xn ]; then
		echo "Configuring newly installed devices"
		cd $BASEDIR
		echo "	- Configuring devices in /devices"
		drvconfig -R $BASEDIR
		echo "	- Configuring devices in /dev"
		devlinks -r $BASEDIR -t $BASEDIR/etc/devlink.tab
		disks -r $BASEDIR
		ports -r $BASEDIR
		exit 0
	fi
	if [ X$x = X -o X$x = Xy -o X$x = Xyes ]; then
		mount -F pcfs -o ro /dev/diskette0 /mnt
		if [ $? -ne 0 ]; then
			#
			# attempt to process old-style diskettes
			#
			if [ "$REL" = "old" ]; then
				install_old_drv
				if [ $? -ne 0 ]; then
					continue
				fi
			fi
			echo "attempt to access floppy failed!"
			continue
		fi
		#
		# XXX It would be better to
		# extract the real floppy label. Note that
		# as a driver may not have an associated
		# patch, don't fail in this case.
		#
		# Ugly workarounds for pcfs wildcard issues across releases
		#
		mach=/mnt/DU/sol_$VERS/$MACH
		if [ ! -d "$mach" ]; then
			mach=/mnt/[D\|d][U\|u]/sol_$VERS/$MACH
		fi
		if [ ! -d "$mach" ]; then
			echo "Cannot locate release sol_$VERS on diskette,"
			echo "wrong update diskette inserted?"
			umount /mnt
			continue
		fi
		tools=$mach/Tools
		if [ ! -d "$tools" ]; then
			tools=$mach/[T\|t]ools
		fi
		if [ ! -d "$tools" ]; then
			echo "Diskette not in proper update format"
			umount /mnt
			continue
		fi
	else
		echo must enter 'y' or 'n'
		continue
	fi
	ICMD=$tools/install.sh
	if [ -x ${ICMD} ]; then
		${ICMD} -I -R $BASEDIR
	else
		echo install.sh not found on diskette
	fi
	umount /mnt
   done
fi

# XXX: install/upgrade workaround II - rebuild nodes after new driver install
if diff $BASEDIR/etc/name_to_major /tmp/name_to_major.$$ > /dev/null 2>&1; then
	:
else
	echo "Configuring newly installed devices"
	cd $BASEDIR
	echo "	- Configuring devices in /devices"
	drvconfig -R $BASEDIR
	echo "	- Configuring devices in /dev"
	devlinks -r $BASEDIR -t $BASEDIR/etc/devlink.tab
	disks -r $BASEDIR
	ports -r $BASEDIR
fi

) < /dev/console > /dev/console 2>&1
stty "$modes" < /dev/console

exit 0
