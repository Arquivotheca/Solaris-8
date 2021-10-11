#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)rcs9.sh 1.33 99/08/27 SMI"
#
# this script is copied from the boot floppy very early by the install
# program and later run after the packages have been added.



########################patch space requirement#################################
# This section is required for installation to allocate space for patches in
# DU distribution diskette. 

#gather version number
VERS="sol_`uname -r | sed -e's/\.//g' -e 's/5/2/'`"
OPT=-pv
case $VERS in
        sol_25)VERS="sol_25"; OPT="-v" ;;
        sol_251)VERS="sol_251"; OPT="-v";;
esac

#
# print the 'itu-diskettes' property on the 'itu-props' device node.
#
extract_diskettes()
{
	nawk 'BEGIN {foundnode=0;foundprop=0;} {
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
	if ($1 == "itu-diskettes:") {
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

if [ -f /tmp/diskette_rc.d/patch_sp ]; then
	# use old method if it is on diskette
        /usr/bin/cp /tmp/diskette_rc.d/patch_sp /tmp/patch_space_reqd.$$
else
	# otherwise leave a mb per itu diskette on / and /usr
	DRV_NUM="`prtconf $OPT | cut -c1-2000 | extract_diskettes`"
	if [ "$DRV_NUM" -gt "0" ]
	then
		maxk="`expr $DRV_NUM \* 1024`"
		cat > /tmp/patch_space_reqd.$$ <<EOF
# rcs9.sh generated patch_space_reqd file based on $DRV_NUM ITU diskettes
ARCH=i386
/ $maxk $DRV_NUM
ARCH=i386
/usr $maxk $DRV_NUM
EOF
	fi
fi

################################################################################

# This script will create a nawk script to map the 2.4 PCI physical
# pathnames (only devices on bus 0) to 2.5 PCI physical pathnames.  If
# the script is invoked with no argument, it's being executed from the
# beginning of upgrade, simply to do the mapping of physical pathnames
# for internal upgrade mechanism support.  If given a single argument
# "post", it's being executed from a package postinstall script or from a
# Driver Update inst9.sh; in this case, it updates the /devices and /dev
# directories,  /etc/bootrc and /etc/path_to_inst files according to new
# PCI device paths so that the resulting system is consistent with the
# new device pathnames.
#
# The nawk script has been updated to also map names for drivers converted
# to understand and use the 2.6 device tree.  This device tree is of course
# being built by the 'configuration assistant' before the kernel boots.
# Solaris drivers that use the information in this tree will have new device
# names under /devices and /dev just as occurred with the 2.4->2.5 PCI
# transition.
#

PATH_TO_INST=$BASEDIR/etc/path_to_inst
BOOTRC=$BASEDIR/etc/bootrc
POSTINSTALL="post"
TARGETNAWK=/tmp/physdevmap.nawk.$$
NAME_TO_MAJOR=$BASEDIR/etc/name_to_major

DRIVER_ALIASES=/etc/driver_aliases
TMP_DIR=/tmp/diskette.d

# find the correct driver_aliases to use
if [ $# -ne 0 -a X$1 = X${POSTINSTALL} ] 
then
	DRIVER_ALIASES=$BASEDIR$DRIVER_ALIASES
else
	if [ -s $TMP_DIR/alias.txt ] 
	then
		DRIVER_ALIASES=$TMP_DIR/alias.txt
	fi

	if [ -s $TMP_DIR/alias.add ] 
	then
		DRIVER_ALIASES="$DRIVER_ALIASES $TMP_DIR/alias.add"
	fi
fi



# Generating the nawk script

echo '
# This nawk script searches the /devices/pci@0,0 tree, looking for a
# physical pathname corresponding to the pathname supplied on stdin
# (presumably from a old-style PCI device in $BASEDIR).  If a new-style
# name is found in /devices, the script returns the mapped path on
# stdout, else it returns nothing.  
# Devices on buses other than 0 are not yet supported.
# EXAMPLE:
# input: /devices/eisa/ncrs@f,0/cmdk@0,0 
# output: /devices/pci@0,0/pci1000,1@f/cmdk@0,0
#
# This script now has the additional responsibility of mapping old device
# names to new ones in the case where a Solaris driver has been converted
# to use the 2.6 device tree and hence has a new name.
#
BEGIN{
	FS = "/" ' > ${TARGETNAWK}

# Initialize two tables in the nawk script. Table one is the array 
# drvinaliases for checking if a given driver is in driver_aliases. 
# The other table is aliastodrv, to map an alias to its driver name.

/usr/bin/egrep -v -h  "^[ 	]*#|^[ 	]*$" $DRIVER_ALIASES | \
while read drvname aliaslist
do

	echo drvinaliases[\"$drvname\"] = 1 >> ${TARGETNAWK}
	#drop the double quotes from aliasname
	aliaslist=`echo $aliaslist |/usr/bin/sed s/\"//g`
	for aliasname in $aliaslist
	do
		echo aliastodrv[\"$aliasname\"] = \"$drvname\" >> ${TARGETNAWK}
	done
done

echo aliastodrv[\"cpqncr\"] = \"ncrs\"  >> ${TARGETNAWK}


# Two tables are also necessary for 2.6 name changes.  One array indicates
# a Solaris driver has been converted.  The second array provides mappings for
# drivers where the name change is to the Plug and Play namespace; providing
# a bridge between the driver module name and the actual new device name.
#	Examples:
#		converted["esa"] = 1
#		pnprename["pnpADP,1542"] = "aha"
	echo pnprename[\"pnpADP,1542\"] = \"aha\" >> ${TARGETNAWK}
	echo drvrename[\"cpqncr\"] = \"ncrs\" >> ${TARGETNAWK}
	echo converted[\"aic\"] = 1 >> ${TARGETNAWK}
	echo converted[\"aha\"] = 1 >> ${TARGETNAWK}
	echo converted[\"corvette\"] = 1 >> ${TARGETNAWK}
	echo converted[\"dpt\"] = 1 >> ${TARGETNAWK}
	echo converted[\"dsa\"] = 1 >> ${TARGETNAWK}
	echo converted[\"esa\"] = 1 >> ${TARGETNAWK}
	echo converted[\"eha\"] = 1 >> ${TARGETNAWK}
	echo converted[\"iss\"] = 1 >> ${TARGETNAWK}
	echo converted[\"mlx\"] = 1 >> ${TARGETNAWK}
	echo converted[\"ata\"] = 1 >> ${TARGETNAWK}
	echo converted[\"trantor\"] = 1 >> ${TARGETNAWK}
	echo converted[\"ncrs\"] = 1 >> ${TARGETNAWK}
	echo converted[\"cpqncr\"] = 1 >> ${TARGETNAWK}
        echo converted[\"cnft\"] = 1 >> ${TARGETNAWK}
        echo converted[\"csa\"] = 1 >> ${TARGETNAWK}
        echo converted[\"smartii\"] = 1 >> ${TARGETNAWK}
        echo converted[\"blogic\"] = 1 >> ${TARGETNAWK}
        echo converted[\"fdc\"] = 1 >> ${TARGETNAWK}

echo '
}
function hex2dec(s) {
	hexdig = "0123456789ABCDEF";
	s = toupper(s);
	accum = 0;
	for (i = 1; i <= length(s); i++) {
		dec = match(hexdig, substr(s, i, 1)) - 1;
		if (dec == -1) {
			return accum;
		}
		accum = (accum * 16) + dec;
	}
	return accum;
}



function checkout(path)
{
	# print path if we had a mapping. This should be done
	# as last step before finishing the mappings
	if (wasmapped == 1)
		print path
	return
}



# First argument path is the path that was either mapped for other
# reasons or it is the original input to the nawk script if no
# other mapping was needed. wasmappedother indicates whether
# this is already mapped for other reasons 
# It returns a new path if mapped, or the same old path it no mapping necessary
# for sd. it will also update the wasmapped global variable if
# sd mapping was necessary.

function sd_check (path) {

	
	# Check to see if we need to map from cmdk to sd. We are checking
	# new booted  /devices from install, so if the user has turned
	# off their hard disk this map will not work. If it is turned off
	# then it is not needed during upgrade and the related nodes
	# will be created upon boot -r.


	newtmppath=path

	if (cmdkindex = match (path, "/cmdk@") != 0) {

		checkrec=path;
		sub("/cmdk@", "/sd@", checkrec);
	
		if ( x = match(checkrec, "devices/") != 0) {
			# checkrec is everything after devices/ from the 
			# cmdk path
			checkrec= substr(checkrec, x + length("devices/"))
		} 

		checkrec ="/devices/"checkrec;

		if (ignoreminor != 0)
			minor=":*"
		else
			minor=""

		if (((system( "test -c " checkrec minor )) == 0) || \
			((system( "test -b " checkrec minor )) == 0))   {
			sub("/cmdk@", "/sd@", newtmppath);
			wasmapped=1;
		}
	}

	return newtmppath
}


# For an ata@addr1,addr2 node, search /devices/pci* to see if 
# we need to convert to pci-ide@X,Y/ata@[0 or 1]
# if we need to convert the return value of this function is
# the absolute pci-ide path upto ata node
# e.g /devices/pci@0,0/pci-ide@7,1/ata@0, otherwise returns  null string.


# The algorithm for matching old ata node to a pciide node is
# that for any pciide ata node, it opens the control  node 
# and read 4 bytes from it that is supposed to be the ioaddress of
# that node if that ioaddress matches the old nodes ioaddress we got a 
# match.


function pciidecheck(addr1, addr2)
{
	# if addr1 is 1, and addr2 is not 0, the old path is a 2.6 
	# path e.g ata@1,1f0, otherwise if addr1 is not 1 (it should really be
	# either 1f0 or 170 for ata), adddr2 then is supposed to be 0 
	# and old path  is a pre-2.6 ata node


	if (addr1 == "1"  && addr2 != "" && addr2 != "0") {
		# old path is 2.6 devconf path
		oldataioaddr = addr2;
	} else if  (addr1 != "1"  &&  addr1 != "0") {
		# oldpath is pre devconf path
		oldataioaddr = addr1
	} else {
		# old path is either a pciide node itself 
		# or some unrecognizable path, no need to do anything
		# in this case.
		return ""
	}


	findcmd = "/usr/bin/find /devices/pci* ! -type d";
	findcmd = findcmd" -name *ide@[01]:control -print 2>/dev/null";

	while (findcmd | getline ctlpath) {

		cmd="cat "ctlpath 

		cmd | getline newataaddr

		if ( hex2dec(oldataioaddr) == hex2dec(newataaddr) ) {
		 	#we got a match
			wasmapped=1
			ctlind=match (ctlpath, ":control")
			ctlpath=substr(ctlpath, 1,  ctlind - 1)

			return  ctlpath
		} else {
			# no match go to next one
			continue
		}
	}
	return "";


}



# main action

{
	remainder = "";
	oldind = NF;

	oldelement = $oldind;

	wholerec = $0

	wasmapped=0

	if (match(wholerec, "pci-ide@.*/ata@")) {
		resultpath = wholerec;
		sub("/ata@", "/ide@", resultpath);
		wasmapped = 1;
		checkout(resultpath);
		continue;
	}

	# hunt for a path element with an @ sign, and drvname in inaliases
        while ( oldind != 0 ) {
		if (((atpos = match(oldelement, "@")) == 0) ||
		   ((olddrvname = substr(oldelement, 1, atpos - 1 )) &&
		   (drvinaliases[olddrvname] == 0) &&
		   (converted[olddrvname] == 0)) && 
		   (olddrvname != "pci1000,3")) {
			remainder = "/"oldelement""remainder;
			oldind -= 1;
			oldelement = $oldind;
			continue;
		} else {
			buspart = $(oldind - 1)
			break;
		}
	}

	if (oldind == 0) {
		resultpath=sd_check(wholerec)
		checkout(resultpath);
		continue
	}


	# The variable, startwithdevices, is used to check whether
	# the input starts with "/devices" string or not.
	# If it does the output will also be printed with "/devices",
	# otherwise the ouput would not start with "/devices".
	# The second field, $2, is being examined because nawk assigns
	# "" to the first field, when "/" as field separator and
	# the first character of input is "/"
	

	startwithdevices=(($2 != "devices") ? 0 : 1);


	newpath=""
	if (olddrvname == "pci1000,3") {
		# special HACK for pci1000,3 to cpqncr
		checkrec=wholerec	
		# dtake everything after devices/, 
		# /devices will be aded to it. This is in case there
		# is more stuff before devices/ e.g /a/devices/
		if ( x = match(checkrec, "devices/") != 0) {
			checkrec= substr(checkrec, x + length("devices/"))
		}
		checkrec = "/devices/"checkrec

		sub("/pci1000,3@", "/cpqncr@", checkrec) 
		#chop the remainder
		if ( remainder != "")
			sub(remainder,"", checkrec)

		if ((system( "test -d " checkrec )) == 0) {
			sub("/pci1000,3@", "/cpqncr@",  wholerec)
			newpath=wholerec
		}

		wasmapped=1	
		resultpath=sd_check(newpath)
		checkout(resultpath);
		continue
	}

	if (olddrvname == "cpqncr") {
		# no need to do anything with a pci path with coqncr
		# if this check is omitted, the 2.6 conversion may 
		# incorrectly convert
		if (match(wholerec, "/pci@") != 0) {
			resultpath=sd_check(wholerec)
			checkout(resultpath);
			continue;
		}
	}

	after = substr(oldelement, atpos + 1);
	if ((commapos = match(after, ",")) == 0) {
		resultpath=sd_check(wholerec)
		checkout(resultpath);
		continue;
	}
	


	#  Driver conversions to 2.6
	# get first address field of  oldelement
	prevaddr = substr(after, 1, commapos - 1);

#
#	IF the previous address starts with pnp then we have definitely
#	already converted that driver name on the target root.
#
	if (substr(prevaddr, 3, 3) == "pnp")
		searchname = "XXX"
	else
		searchname = olddrvname"@.*"

	#  Driver update of PCI pre 2.5 -> present
	slotnum = hex2dec(substr(after, 1, commapos - 1));

	dopcifind = "y";
	if (olddrvname == "ncrs") {
		funcnum = 0;
		if ((devicenum = slotnum) > 31) 
			dopcifind = "n"
		else
		# In 2.4 ncrs only works on bus 0
		busnum = 0;
	} else {
		funcnum = slotnum % 8;
		devicenum = int(slotnum / 8) % 32;
		if ((busnum = int(slotnum / 256) % 256) != 0)
			dopcifind = "n";
	}

	# get everything after comma
	after = substr(after, commapos + 1);

	# get minor node name and second address field of oldelement
	if ((minorpos = match(after, ":")) == 0) {
		oldminor = "";
		prevaddr2 = after;
	} else {
		oldminor = substr(after, minorpos);
		prevaddr2 = substr(after, 1, minorpos - 1);
	}



	pcifind = "/usr/bin/find /devices/pci@0,0 ! -type d";
	pcifind = pcifind" -print 2>/dev/null";

	while ((dopcifind == "y") && pcifind | getline) {
		newind = NF;
		newelement = $newind;
        	while ( newind != 0 ) {
			if (((newatpos = match(newelement, "@")) == 0) ||
			    ((match($newind, "pci[0-9A-Fa-f]+,[0-9A-Fa-f]+@") != 1) &&
			     (match ($newind, "cpqncr@") != 1)) ||
			    ((alias = substr(newelement, 1, newatpos - 1)) &&
			     (aliastodrv[alias] == ""))) {

				newind -= 1;
				newelement = $newind;
				continue;
			} else
				break;
		}

		if (newind == 0)
			continue;

		# make sure that the new path is a pci bus 0 path
		if ($(newind-1) != "pci@0,0")
			continue;

		if ((newdrv = aliastodrv[alias]) != olddrvname)
			continue;

		# after is everything beyond @
		after = substr(newelement, newatpos + 1);
		if ((puncpos = match(after, "[^0-9a-fA-F]")) == 0) {
			newminor = "";
			newfunc = 0;
			newdevicenum = hex2dec(after);
		} else {
			newdevicenum = hex2dec(substr(after, 1, puncpos - 1));
			newfunc = 0;
			after = substr(after, puncpos);
			if (substr(after, 1, 1) == ",") {
				newfunc = hex2dec(substr(after, 2, 1));
				after = ((length (after) > 2) ? substr(after, 3) : "");
			}
			if ((colonpos = match(after, ":")) != 0)
				newminor = substr(after, colonpos);
			else
				newminor = "";
		}


		if ((newdevicenum != devicenum ) ||  (newfunc != funcnum))
			continue;

		# if ignoreminor is set, we do not check for
		# minor name matching

		if ((ignoreminor == 0) && (newminor != oldminor))
			continue;


		# The first field will be "" if the first
		# element is a field separator
		# the second field is devices that will be printed
		# depending on starting string of input

		if (startwithdevices == 1)
			newpath=sprintf("/devices");

		i = 3;
		while (i < newind) {
			newpath=sprintf("%s/%s", newpath, $i);
			i += 1;
		}

		# if ignoreminor is set, minor names are chopped
		# from new device path
		if (ignoreminor == 0){
			newpath=sprintf("%s/%s",newpath,  $newind)
			newpath=sprintf("%s%s", newpath, remainder)
		} else {
			if ((minorind = match($newind,":")) == 0)
				newpath=sprintf("%s%s%s", newpath, "/", \
					    $newind);
			else{
				newpath=sprintf("%s%s%s", newpath, "/", \
					    substr($newind, 1, minorind - 1));
			}
			newpath=sprintf("%s%s", newpath,remainder)
		}

		break;
	}

	if (searchname == "XXX") {
		if (newpath != "")  {
			wasmapped=1
			resultpath=sd_check(newpath)
		} else {
			resultpath=sd_check(wholerec)
		}	
		checkout(resultpath);
		continue
	}

	checkaddr = "n"

	findcmd = "/usr/bin/find /devices ! -type d";
	findcmd = findcmd" -print 2>/dev/null";

	while (findcmd | getline) {
		newind = NF;
		newelement = $newind;
		while (newind != 0) {
			if ((newat = match(newelement, "@")) == 0) {
				newind -= 1;
				newelement = $newind;
				continue;
			} else if (match(newelement, searchname) != 0) {
				renamed = "n";
				break;
			} else if ((al = substr(newelement, 1, newat - 1)) &&
			    pnprename[al] == olddrvname) {
#
# We are wholly screwed if there are two pnp cards with the same
# driver in the system, because there is no mention of I/O
# address in the new name, and thus no tie between old and new.
#
				renamed = "y";
				break;
			} else if ((al = substr(newelement, 1, newat - 1)) &&
			    drvrename[al] == olddrvname) {
				renamed = "y";
				checkaddr = "y";	
				break;

			} else {
				newind -= 1;
				newelement = $newind;
				continue;
			}
		}

		if (newind == 0)
			continue;

		#
		# Do not advertise a new name if the new name is the same
		# as the "old" one.  We want to show changes to paths --
		# callers expect non-replies to indicate no change.
		#
		if (oldelement == newelement)
			continue;

		# after is everything beyond @
		after = substr(newelement, newat + 1);
		if ((cp = match(after, ",")) == 0)
			continue;

		# If possible, try to sanity check against the previous
		# address.  Network drivers have kind of bogus previous
		# addresses, though, so we may not have much luck.
		after = substr(after, cp + 1);
		if ((endnew = match(after, "[^0-9a-fA-F]")) != 0) {
			newaddr = substr(after, 1, endnew - 1)
		} else
			newaddr = substr(after, 1)

		prevaddrn = hex2dec(prevaddr);
		#
		#  Assumption here is that all old style names are of
		#  the form
		#	driver@(something > 255),...
		#
		#  This is relatively safe, but we may have to add special
		#  cases if that turns out not to be true for something.
		#
		if (prevaddrn <= 255) {
			continue;
		}

		if ((renamed == "y" && checkaddr == "y") || (renamed == "n")) {
			if (newaddr != prevaddr) {
				#
				#  Might just be a mismatch because of rounding
				#  to slot number that is happening with EISA
				#  devices in 2.6.
				#
				if (buspart == "eisa") {
					altprvaddr = int(prevaddrn/4096) * 4096;
					altnewaddr = hex2dec(newaddr);
					if (altprvaddr != altnewaddr) {
						continue;
					}
				} else {
					continue;
				}
			}
		}

		if ((mp = match(after, ":")) == 0) {
			newminor = "";
		} else {
			newminor = substr(after, mp);
		}

		# if ignoreminor is set, we do not check for
		# minor name matching

		if ((ignoreminor == 0) && (newminor != oldminor))
			continue;

		# The first field will be "" if the first
		# element is a field separator
		# the second field is devices that will be printed
		# depending on starting string of input

		newpath=""
		if (startwithdevices == 1)
			newpath=sprintf("%s%s", newpath, "/devices");

		i = 3;
		while (i < newind) {
			newpath=sprintf("%s/%s",newpath, $i);
			i += 1;
		}

		i += 1;
		addremain = "";
		while (i < NF) {
			addremain = addremain"/"$i
			i += 1;
		}

		# if ignoreminor is set, minor names are chopped
		# from new device path
		if (ignoreminor == 0) {
			newpath=sprintf("%s/%s", newpath, $newind)
			if (addremain != "")
				newpath=sprintf("%s%s",newpath, addremain);
			newpath=sprintf("%s%s", newpath, remainder);
			break;
		} else if ((minorind = match($newind,":")) == 0) {
			newpath=sprintf("%s%s%s",newpath, "/", $newind);
		} else {
			newpath=sprintf("%s%s%s",newpath, "/", \
				    substr($newind, 1, minorind - 1));
		}
		if (addremain != "")
			newpath=sprintf("%s%s",newpath, addremain);

		newpath=sprintf("%s%s", newpath, remainder);
		break;
	}


	if (newpath == "" && olddrvname == "ata") {
		# ata to pciide  conversion if necessary

		newpath= pciidecheck(prevaddr, prevaddr2);

		if( newpath != "")	{
			#we got a conversion, complete the new path
	
			# newpath returned from pciidecheck
			# always starts with /devices, if the input
			# did not have it drop it from newpath
			if (startwithdevices != 1)
				# drop /devices it from newpath
				newpath=\
				    substr(newpath, length("/devices") + 1)

			# sine we didnot have any ata nodes with minor names
			# I do not think it should matter here to check for 
			# exact match of minor node names.

			# However  the output of pciidecheck has the
			# minor node chopped already. we need to add it
			# if any. (should not be any anyway).
			if (ignoreminor == 0) {
				newpath=sprintf("%s%s", newpath, oldminor)
			}
			newpath=sprintf("%s%s", newpath,remainder)
		}
	} 

	if (newpath != "")  {
		wasmapped=1
		resultpath=sd_check(newpath)
	} else {
		resultpath=sd_check(wholerec)
	}

	checkout(resultpath);
	continue;


}

' >> ${TARGETNAWK}



 

if [ $# -eq 0 -o X$1 != X${POSTINSTALL} ] ; then  exit; fi

# The following if statement's  purpose is to install files previously 
# preserved by rcs3.sh # onto the live system.  the file bootenv.rc is a 
# special case since it must be merged.
#
# to merge bootenv.rc we do these steps:
#
#	1. remove the confflags line from the floppy version
#	2. remove any auto-boot lines from the floppy version
#	3. remove any bootpath lines from the floppy version
#	4. preserve the confflags, auto-boot, and bootpath lines from the
#	   $BASEDIR version (the bootpath should  be mapped if mapping
#	   is necessry.)
#	5. get the values of probed-arch-name, probed-compatible and
#	   probed-si-hw-provider properties which setup by bootconf
#	   and merge them to bootenv.rc
#

if [ "$1" = "post" ]
then
	BOOTENV="bootenv.rc"
	DEST_DIR=/boot
	TMP_ROOT=/tmp/root
	#
	# find all the files preserved by rcs3.sh
	#
	cd ${TMP_ROOT}/${DEST_DIR}
	find . -type f -print | while read file
	do
		src=${file}
		dst=${BASEDIR}/${DEST_DIR}/${file}
		dstdir=`dirname $dst`
		[ -d $dstdir ] || mkdir -p $dstdir
		if [ `basename $dst` = $BOOTENV ]
		then
			# special case -- merge the two files
			t=/tmp/benv$$
			grep -v '^setprop[ 	][ 	]*confflags' $src > $t
			grep -v '^setprop[ 	][ 	]*auto-boot' $t > $src
			grep -v '^setprop[ 	][ 	]*bootpath' $src > $t
			sed 's///' < $t > $src
			rm $t
			# if $dst isn't there yet, supply the lines
			if [ -f $dst ]
			then
				grep '^setprop[ 	][ 	]*confflags' $dst >> $src
				grep '^setprop[ 	][ 	]*auto-boot' $dst >> $src
				
				oldln=`grep '^setprop[ 	][ 	]*bootpath' $dst`

				if [ $? -eq 0 ]
				then 
					oldpath=`echo $oldln | sed 's/^M//' |\
						/usr/bin/nawk '{print $3}'`
					newpath=`echo "$oldpath" | \
						/usr/bin/nawk -f ${TARGETNAWK}`

					if [ X$newpath != X"" ]
					then
						#convert ata to ide for bootpath
						echo $newpath | \
						    grep  '/pci-ide@' \
							>/dev/null 2>&1
						if [ $? -eq 0 ]	
						then
							echo $newpath | \
							    grep '/ata@'\
								> /dev/null 2>&1
							if [ $? -eq 0 ]
							then
								newpath=`echo $newpath | sed s\;/ata@\;/ide@\;`
							fi	
						fi


						oldln=`echo $oldln | \
						    sed s\;$oldpath\;$newpath\;`
					fi

					echo $oldln >> $src
				fi
				#
				# preserve properties created after last
				# installation 
				#
				# substitute all tabs to spaces
				#
				sed 's/	/ /g' < $src > $t
				while read line
				do
					echo $line | grep "^setprop" > /dev/null 2> /dev/null
					if [ $? -eq 0 ]
					then
						prop=`echo $line | /usr/bin/nawk '{print $2}'`
						grep " $prop " $t > /dev/null 2> /dev/null
						if [ $? -eq 1 ]
						then
							echo $line >> $src
						fi
					fi
				done < $dst
				rm $t
			else
				cat >> $src << EOF
setprop auto-boot? true
setprop auto-boot-cfg-num -1
setprop auto-boot-timeout 5
EOF
			fi
			#
			# if properties are no longer in used or need
			# to be removed, do it here or i.bootenvrc.
			#
			cp $src $t
			grep -v '^setprop[ 	][ 	]*probed-arch-name' $t > $src
			grep -v '^setprop[ 	][ 	]*probed-compatible' $src > $t
			grep -v '^setprop[ 	][ 	]*probed-si-hw-provider' $t > $src
			rm $t
			#
			# add the "probed-*" properties from /option node
			# defined by bootconf to bootenv.rc 
			#
			prtconf $OPT | grep "probed-" > $t
			if [ -f $t ]
			then
				proplist="probed-arch-name probed-compatible probed-si-hw-provider"
				for prop in $proplist
				do
					line=`grep $prop $t | line`
					if [ "$line" != "" ]
					then 
						val=`echo $line | /usr/bin/nawk '{print $2}' | sed "s/'//g"`
						if [ "$val" != "" ]
						then
							echo "setprop $prop $val" >> $src
						fi
					fi
				done
				rm $t
			fi
		fi
		cp $src $dst
		boot_filesys=`df $dst | awk '{print $1}'`
		boot_filesys_type=`mount -v | grep "on $boot_filesys type" |\
				   awk '{print $5}'`
		#
		# Set file permissions and ownerships only if $BASEDIR/boot
		# is not a DOS filesystem.  chmod, chown, and chgrp fail on
		# DOS filesystems.
		#
		if [ "$boot_filesys_type" != "pcfs" ]; then
			chmod 644 $dst
			chown root $dst
			chgrp sys $dst
		fi
	done
fi


# update files 

# updating ${BOOTRC}
# get bootpath see if it maps if so update the file
update_bootrc()
{
	echo "Updating etc/bootrc file"
	/usr/bin/cp ${BOOTRC} ${BOOTRC}.old
	rcln=`/usr/bin/egrep "^[ 	]*setprop[ 	]+boot-path" ${BOOTRC} `
	bootpath=`echo $rcln | /usr/bin/nawk '{ print $3 }'`
	newbootpath=`echo "$bootpath" | /usr/bin/nawk -f ${TARGETNAWK}`
	if [ X$newbootpath != X"" ] 
	then
		# adding "\" before "/" to quote / for ed
		editrcln=`echo $rcln | /usr/bin/sed 's;[/];\\\\/;'g`
		editpath=`echo $bootpath | /usr/bin/sed 's;[/];\\\\/;'g`
		newbootpath=`echo $newbootpath | /usr/bin/sed 's;[/];\\\\/;'g`
	
		/usr/bin/ed -s ${BOOTRC} <<-!   >/dev/null 2>&1
			/$editrcln
			.s/$editpath/$newbootpath
			.
			w
			q
			!
	fi
}




# check the path_to_inst file of target image and
# return 1 plus highest sd instance number used.
# if there was no sd entries in the file, returns 0

get_highest_sd_inst_number()
{
	grep "/sd@" ${PATH_TO_INST} > /dev/null 2>&1
	
	if [ $? -eq 1 ] 
	then
		echo 0
	else
		lastinstnum=`grep "/sd@" ${PATH_TO_INST} | \
			/usr/bin/sort -k 2,2 | tail -1 |  nawk '{print $2}'`

		nextinstnum=`expr $lastinstnum + 1`
		echo $nextinstnum
	fi
}



# updating path_to_inst file
update_path_to_inst()
{

	echo "Updating etc/path_to_inst file"	

	/usr/bin/cp ${PATH_TO_INST} ${PATH_TO_INST}.old

	sdinstnumcheck=0

	/usr/bin/egrep -v "^[ 	]*#|^[ 	]*$" ${PATH_TO_INST} |\
	while read name instance bindingname
	do
		# remove double quotes
		olddevice=`echo $name |/usr/bin/sed s/\"//g`
		newdevice=`echo $olddevice | /usr/bin/nawk -v "ignoreminor=1" -f ${TARGETNAWK}`
		if [ X$newdevice = X"" ] ; then continue; fi


		# If mapping cmdk to sd, we would add entries for sd.
		# however the cmdk path still may need to be updated in
		# path_to_inst for other kind of mappings, just to have
		# consistent entries in path_to_inst file

		#if $olddevice contains cmdk@ and newdevice contains sd@

		echo $olddevice |grep "/cmdk@" > /dev/null 2>&1
		rc1=$?		
		echo $newdevice |grep "/sd@" > /dev/null 2>&1
		rc2=$?

		sdmap=0
		nonsdmapping=1

		if [ $rc1 -eq 0 -a $rc2 -eq 0 ]
		then
			# old path had cmdk and new path had sd
			# we are mapping cmdk to sd. See if this is 
			# the only map that was done.

			sdmap=1
			newsddevice=$newdevice
			newdevice=`echo $newdevice | /usr/bin/sed s/sd@/cmdk@/`
			if [ $newdevice = $olddevice ]
			then
				nonsdmapping=0
			fi
		fi


		if [ $nonsdmapping -eq 1 ]	 
		then
			# adding "\" before "/" to quote / for ed
			name=`echo $name | /usr/bin/sed 's;/;\\\\/;'g`
			newdevice=`echo $newdevice | /usr/bin/sed 's;/;\\\\/;'g`
	
			# In 2.5.1 a field "driver binding name" was added to
			# this file that would also need to be updated.

			if [ X$bindingname = X"" ]
			then
				# 2.5 version no need to do anything for
				# bindingname
				/usr/bin/ed -s ${PATH_TO_INST} <<-! > /dev/null\ 									2>&1
					/^[ 	]*$name
					.s/$name/"$newdevice"/
					w
					q
					!
	
			else
				#2.5.1 version
	
				if [ $bindingname = "\"ncrs\"" ]
				then
					echo $newdevice | grep cpqncr > \
								/dev/null 2>&1
					if  [ $?  -eq 0 ]
					then
						newbind="\"cpqncr\""
					else
						newbind=$bindingname
					fi
				else
					newbind=$bindingname
				fi
	
				/usr/bin/ed -s ${PATH_TO_INST} <<-! > \
								/dev/null 2>&1
					/^[ 	]*$name
					.s/$name/"$newdevice"/
					.s/$bindingname/$newbind/
					w
					q
					!
			fi
		fi


		if [ $sdmap -eq 1 ]
		then
			# add newsddevice to path_to_inst if not already there
			# get the highest used sd instance number, an add
			# entries from one plus that number

			grep $newsddevice ${PATH_TO_INST} > /dev/null 2>&1
			if [ $? -ne 1 ]
			then
				continue
			fi

			if [ $sdinstnumcheck -eq 0 ]
			then
				nextinst=`get_highest_sd_inst_number`
				sdinstnumcheck=1
			fi
			# Need to check whether 3 fields or
			# 2 fields in path_to_inst
			if [ X$bindingname = X"" ]
			then 
				echo \"$newsddevice\" $nextinst >> \
								${PATH_TO_INST}
			else
				echo \"$newsddevice\" $nextinst "\"sd\"" >> \
								${PATH_TO_INST}
			fi
			nextinst=`expr $nextinst + 1`

		fi
	
	done
}



# input is the whole minor name, output minor number's partition bits in
# decimal: e.g. (a,raw -> 0), (a -> 0), (b,raw -> 1). there are
# minor nodes go from :a(,raw) -> :u(,raw)  for cmdk/sd

minor_data_to_num ()
{
	# should return $1[0] - 'a' 

	s=$1
	echo $s | nawk ' { 
		table="abcdefghijklmnopqrstu"

		x= index (table, substr($0, 1, 1))
		print (x - 1)
	}'

}


update_devices_dir()
{
# update $BASEDIR/devices tree:
# check /devices see if there exists any pci nodes
echo "Updating devices directory"

(cd $BASEDIR; 
oldlist=""

sdmajorvalid=0

/usr/bin/find ./devices  ! -type d -print | while read oldpath
do 
	newpath=`echo $oldpath | /usr/bin/sed 's/\.//' | /usr/bin/nawk -f ${TARGETNAWK}`
	if [ X$newpath = X"" ] ; then continue; fi

	# extracting the leaf
	newdir=`echo $newpath |/usr/bin/sed 's/\/[^\/]*$//g'`
	/usr/bin/ls $BASEDIR/$newpath > /dev/null 2>&1
	if [ $? -ne 0 ]
	then
		/usr/bin/mkdir -p $BASEDIR$newdir


		echo $oldpath |grep "/cmdk@" > /dev/null 2>&1
		rc1=$?		
		echo $newpath |grep "/sd@" > /dev/null 2>&1
		rc2=$?

		if [ $rc1 -eq 0 -a $rc2 -eq 0 ]
		then
			# mapping cmdk to sd
			# already moved to sd, need to create correct node
			# with correct major number and instance number
			if [ $sdmajorvalid -eq 0 ]
			then 
				sdmajornumber=`egrep  "^[ 	]*sd[ 	]" \
					${NAME_TO_MAJOR} | nawk '{print $2}'`
				[ $? -eq 0 ] && sdmajorvalid=1
			fi


			if [ $sdmajorvalid -eq 0 ]
			then
				# Disaster the old name_to_major does not
				# have an entry for sd. I can not create
				# new sd nodes.
				echo "NOTICE: No major number entry for sd\c"
				echo " driver in ${NAME_TO_MAJOR}"
				continue;
			fi
			
			newpathnominor=`echo $newpath |/usr/bin/sed 's/:.*//g'`

			# drop everything up to end of devices from the 
			# nominorpath to be able to grep in path_to_inst

			newpathnominor=`echo $newpathnominor | \
							sed 's/.*devices//g'`


			sdinstnum=`egrep "$newpathnominor" $PATH_TO_INST | \
							nawk '{ print $2}'`

			if [ $? -ne 0 ] 
			then
				# Did not find an entry in target 
				# image path_to_inst file
				# It should have been created by 
				# update_path_to_inst.
				# This case should not happen unless
				# there are entries in devices dir that
				# do not correspond to the path_to_inst file
				# of the target image.
				
				echo "entry: $oldpath in $BASEDIR/devices \c"
				echo "did not exist in $PATH_TO_INST"
				echo "Will not be mapped to $newpath"

				continue;
			fi


	
			# get minor name
			minorname=`echo $newpath | /usr/bin/sed 's/.*://g'`


			# The minor number is:
			# (instance number << 6) | partitionnumber.
			# instance number is gotten from path_to_inst file
			# of target image. partition number is really
			# from sd's internal table (sd_minor_data)
			# converted to the algorithm used in minor_data_to_num.

			partitionnum=`minor_data_to_num $minorname`

			instshifted=`expr $sdinstnum \* 64`
			sdminor=`expr $instshifted + $partitionnum`

			type="b"

			echo $minorname | grep ",raw" > /dev/null 2>&1
			[ $? -eq 0 ] && type="c"

			mknod $BASEDIR$newpath $type $sdmajornumber $sdminor

			#set permission and owner	
			chmod 0640 $BASEDIR$newpath
			chown root $BASEDIR$newpath
			chgrp sys  $BASEDIR$newpath

			/usr/bin/rm $oldpath
			
		else

			/usr/bin/mv $oldpath $BASEDIR$newpath
		fi
	fi
	# Need to keep track of what was mapped to delete later
	oldlist="$oldlist $oldpath"
done	

for i in $oldlist
do
	/usr/bin/rm -f $i
done

)

}

update_dev_dir()
{

	/usr/bin/find $BASEDIR/dev  -type l  -ls  | while read line
	do
		oldlink=`echo "$line"  | /usr/bin/nawk '{print $NF}' `
		name=`echo "$line"  | /usr/bin/nawk '{print $(NF-2)}' `
		# save everything before the "/devices" in old link
		# to add to the new path to be linked to
		prefix=`echo $oldlink | /usr/bin/sed 's;/devices.*;;g'`
		# remove everything up to "/devices" from oldlink to 
		# give it to nawk script to map
		oldlink=`echo $oldlink | /usr/bin/sed 's;.*/devices;/devices;g'`
		newlink=`echo $oldlink | /usr/bin/nawk -f ${TARGETNAWK}`
		if [ X$newlink = X"" ] ; then continue; fi
		/usr/bin/rm $name
		/usr/bin/ln -s $prefix$newlink $name
	done
}

# When moving support of a network device from one driver to another,
# we may need to rename hostname files in /etc
update_net_devices()
{
	# The elxl driver now supports some devices that were previously
	# supported by the elx driver.  Thus, some /dev/elx[0-9] devices
	# will have changed to /dev/elxl[0-9].
	#
	# We may therefore need to rename some files /etc/hostname.elx*
	#
	# Example:
	# There were 3 elx devices in the system before elxl was installed.
	# Now, /dev/elx1 is being supported by the elxl driver. thus:
	#
	#  elx0 remains elx0.
	#  elx1 becomes elxl0.
	#  elx2 becomes elx1.
	# (gld-assigned network PPAs aren't bound to device instances)

	if [ -f ${BASEDIR}/etc/hostname.elxl* ] ; then
		return 0
	fi

	if [ ! -x ${BASEDIR}/dev/elx* ]
	then
		exit
	fi

	# remember what the device nodes used to look like
	t=/tmp/elx_devices
	touch $t
	if [ -x ${BASEDIR}/dev/elx[0-9]* ]
	then
		ls -l ${BASEDIR}/dev/elx[0-9]* | sed -e 's/.*devices/devices/' > $t
	fi

	# Make sure devices can ifconfig on reboot
	devlinks -r $BASEDIR
	# if only you could tell drvconfig where to look for system files.
	ls -l ${BASEDIR}/dev/elx* | while read DEVICE
	do
		DEVNODE=`echo $DEVICE | \
			sed -e 's/.*\.\.\/devices/\.\.\/devices/'`
		echo $DEVNODE | grep elxl > /dev/null 2>&1
		if [ $? = 1 ] ; then
			DEVNAME="elx"
		else
			DEVNAME="elxl"
		fi
		MINOR=`echo $DEVICE | sed -e 's/.*'$DEVNAME//`
		if [ ${MINOR}X = "X" ] ; then
			MINOR=0
		else
			MINOR=`expr $MINOR + 1`
		fi
		rm ${BASEDIR}/dev/$DEVNODE > /dev/null 2>&1
		mknod ${BASEDIR}/dev/$DEVNODE c \
			`cat ${BASEDIR}/etc/name_to_major | \
			awk '$1 == "'$DEVNAME'" { print $2 }'` $MINOR > \
			/dev/null 2>&1
	done

	ELXCOUNT=0
	ELXLCOUNT=0
	DEVICECOUNT=0

	cat $t | while read DEVLINK
	do
		while [ -x ${BASEDIR}/dev/elxl${ELXLCOUNT} ]
		do
			# Look for elxl devices which were elxl from the outset
			ELXLSTRING=`ls -l ${BASEDIR}/dev/elxl${ELXLCOUNT} | \
				sed -e 's/.*devices/devices/'`
			DEVSTRING=`echo $ELXLSTRING | grep pci10b7,9000``echo $ELXLSTRING | grep pci10b7,9001``echo $ELXLSTRING | grep pci10b7,9050`
echo ${DEVSTRING} ${ELXLCOUNT}
			if [ ${DEVSTRING}X != "X" ]
			then
				break;
			fi

			# Not going from elx to elxl, skip over it
			ELXLCOUNT=`expr $ELXLCOUNT + 1`
		done 

		# See if this device has come over from elx
		DEVSTRING=`echo $DEVLINK | grep pci10b7,9000``echo $DEVLINK | grep pci10b7,9001``echo $DEVLINK | grep pci10b7,9050`
		if [ ${DEVSTRING}X = "X" ] ; then
			if [ -f ${BASEDIR}/etc/hostname.elx${DEVICECOUNT} -a \
			    ${BASEDIR}/etc/hostname.elx${DEVICECOUNT} != \
			    ${BASEDIR}/etc/hostname.elx${ELXCOUNT} ] ;
			then
				mv ${BASEDIR}/etc/hostname.elx${DEVICECOUNT} \
				    ${BASEDIR}/etc/hostname.elx${ELXCOUNT}
			fi
			ELXCOUNT=`expr $ELXCOUNT + 1`
		else
			if [ -f ${BASEDIR}/etc/hostname.elx${DEVICECOUNT} ]
			then
				mv ${BASEDIR}/etc/hostname.elx${DEVICECOUNT} \
				    ${BASEDIR}/etc/hostname.elxl${ELXLCOUNT}
			fi
			ELXLCOUNT=`expr $ELXLCOUNT + 1`
		fi

		DEVICECOUNT=`expr $DEVICECOUNT + 1`
	done

	rm $t
}


# Given an argument, this function checks the argument to see
# to see if it is a directory, and if so is it empty or not.
# It returns 1 if either the directory does not exist, or it is an 
# empty directory, otherwise returns 0.

nonempty ()
{
	dir=$1
	if [ -d $dir ] ; then
		if [ `/usr/bin/ls -a $dir | /usr/bin/wc -l` -gt 2 ] ; then
			return 0;		# true
		fi
	fi
	return 1;		# false
}

# If $basedir/devices is empty we are doing a pure install and
# not an upgrade. In this case exit without doing anything

nonempty ${BASEDIR}/devices
if  [ $? -eq 1 ] ; then  exit; fi


# Note: update_path_to_inst has to be done before update_devices_dir
# so that the map for sd work, since updating devices dir depends
# on getting the instance number from path_to_inst for sd.

update_path_to_inst
update_bootrc
update_devices_dir
update_dev_dir
update_net_devices

