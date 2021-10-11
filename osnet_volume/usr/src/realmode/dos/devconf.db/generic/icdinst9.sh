#!/bin/sh
#
#  Copyright (c) 1999 by Sun Microsystems, Inc.
#  All rights reserved.
#

#ident "@(#)icdinst9.sh 1.1 99/12/06"

# Current usage:
# The script is invoked with -i option, near the end of
# creation of mini-root on swap space during cd0 install,
# so that it can turn autoboot off in case drivers from itu
# were used.

# Also the script would be called after all pkgs are added,
# so that for cd0 it can pop up a window console to call inst9.sh



PATH=${PATH}:/sbin:/usr/sbin:/usr/bin
export PATH


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
	if (ARG != "") {
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
# Some properties can be large enough to cause nawk to report errors.
# Use 'cut' to limit lines to 2000 characters.
#
DRV_LIST=`prtconf -vp | cut -c1-2000 | extract_drivers`
DRV_NUM=`prtconf -vp | cut -c1-2000 | extract_drivers itu-diskettes`

#
# give up if no ITU drivers or ITU diskettes have been processed
# if on new system.
if [ "$DRV_LIST" = "" -a "$DRV_NUM" = "" ]; then
	exit 0
fi


icdinstall=false

while getopts i c 
do
	case $c in
	i)	icdinstall="true";
	esac
done



# the script is called at end of creation of swap space to reset auto_boot
if [ $icdinstall = "true" ]
then
	eeprom auto-boot?=false
	echo "After the system reboots, please select \"F4 Add Driver\" again"
	echo "so that the Install Time Update media can be processed again"
	exit 0
fi



# this is called at end of install to invoke inst9.sh
# if /swap2 does not exist it means we are not doing install through install cd

if [ -f /tmp/.nowin -o ! -f /swap2 ]
then
	sh /tmp/diskette_rc.d/inst9.sh
else
	/usr/dt/bin/dtterm -C -e sh /tmp/diskette_rc.d/inst9.sh
fi
exit 0
