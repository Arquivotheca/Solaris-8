#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
#ident	"@(#)rcs9.sh	1.4	96/02/15 SMI"

########################patch space requirement#################################
# This section is required for installation to allocate space for patches in
# DU distribution diskette. 
if [ -f /tmp/diskette_rc.d/patch_sp ]; then
        /usr/bin/cp /tmp/diskette_rc.d/patch_sp /tmp/patch_space_reqd.$$
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

PATH_TO_INST=$BASEDIR/etc/path_to_inst
BOOTRC=$BASEDIR/etc/bootrc
POSTINSTALL="post"
TARGETNAWK=/tmp/physdevmap.nawk.$$


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



# main action

{
	remainder = "";
	oldind = NF;

	oldelement = $oldind;

	# hunt for a path element with an @ sign, and drvname in inaliases
        while ( oldind != 0 ) {
		if (((atpos = match(oldelement, "@")) == 0) ||
		   ((olddrvname = substr(oldelement, 1, atpos - 1 )) &&
		   (drvinaliases[olddrvname] == 0))) {
			remainder = "/"oldelement""remainder;
			oldind -= 1;
			oldelement = $oldind;
			continue;
		} else
			break;
	}

	if (oldind == 0)
		continue;

	# The variable, startwithdevices, is used to check whether
	# the input starts with "/devices" string or not.
	# If it does the output will also be printed with "/devices",
	# otherwise the ouput would not start with "/devices".
	# The second field, $2, is being examined because nawk assigns
	# "" to the first field, when "/" as field separator and
	# the first character of input is "/"
	
	startwithdevices=(($2 != "devices") ? 0 : 1);


	after = substr(oldelement, atpos + 1);
	if ((commapos = match(after, ",")) == 0)
		continue;
	
	slotnum = hex2dec(substr(after, 1, commapos - 1));

	if (olddrvname == "ncrs") {
		funcnum = 0;
		if ((devicenum = slotnum) > 31)
			continue;
		# In 2.4 ncrs only works on bus 0
		busnum = 0;
	} else {
		funcnum = slotnum % 8;
		devicenum = int(slotnum / 8) % 32;
		if ((busnum = int(slotnum / 256) % 256) != 0)
			continue;
	}

	# get everything after comma
	after = substr(after, commapos + 1);
	if ((minorpos = match(after, "[^0-9a-fA-F]")) == 0)
		oldminor = "";
	else
		oldminor = substr(after, minorpos);

	while ("/usr/bin/find /devices/pci@0,0  ! -type d  -print 2>/dev/null" | getline) {
		newind = NF;
		newelement = $newind;
        	while ( newind != 0 ) {
			if (((newatpos = match(newelement, "@")) == 0) ||
			   (match($newind, "pci[0-9A-Fa-f]+,[0-9A-Fa-f]+@") != 1)||
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
			newminor = after;
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
			printf("/devices");

		i = 3;
		while (i < newind) {
			printf("/%s", $i);
			i += 1;
		}
		# if ignoreminor is set, minor names are chopped
		# from new device path
		if (ignoreminor == 0){
			printf("/%s", $newind)
			print remainder;
		} else {
			if ((minorind = match($newind,":")) == 0)
				printf("%s%s", "/", $newind);
			else{
				printf("%s%s", "/", (substr($newind, 1, minorind - 1)));
			}
			print remainder	
		}

		break;
	}
}

' >> ${TARGETNAWK}



 


if [ $# -eq 0 -o X$1 != X${POSTINSTALL} ] ; then  exit; fi

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


# updating path_to_inst file
update_path_to_inst()
{

	echo "Updating etc/path_to_inst file"	

	/usr/bin/cp ${PATH_TO_INST} ${PATH_TO_INST}.old
	
	/usr/bin/egrep -v "^[ 	]*#|^[ 	]*$" ${PATH_TO_INST} |\
	while read name instance
	do
		# remove double quotes
		olddevice=`echo $name |/usr/bin/sed s/\"//g`
		newdevice=`echo $olddevice | /usr/bin/nawk -v "ignoreminor=1" -f ${TARGETNAWK}`
		if [ X$newdevice = X"" ] ; then continue; fi
	
		# adding "\" before "/" to quote / for ed
		name=`echo $name | /usr/bin/sed 's;/;\\\\/;'g`
		newdevice=`echo $newdevice |  /usr/bin/sed 's;/;\\\\/;'g`
	
		/usr/bin/ed -s ${PATH_TO_INST} <<-!   > /dev/null 2>&1
			/^[ 	]*$name
			.s/$name/"$newdevice"/
			.
			w
			q
			!
	
	done
}

update_slashdevices()
{
# update $BASEDIR/devices tree:
# check /devices see if there exists any pci nodes
echo "Updating devices directory"

(cd $BASEDIR; 
oldlist=""
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
		/usr/bin/mv $oldpath $BASEDIR$newpath
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

update_bootrc
update_path_to_inst
update_slashdevices
update_dev_dir
