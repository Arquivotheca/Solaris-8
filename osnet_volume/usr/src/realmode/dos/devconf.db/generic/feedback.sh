#!/bin/sh
#
#	feedback.sh -- gather feedback on the DevConf install floppy and return
#			via e-mail.
#
#ident "@(#)feedback.sh	1.3 99/10/13"

MAILTO="ihv-drvpkg@Sun.COM"
TMPFILE=/tmp/devconf.$$

# Ensure that usage statement is printed if script run with arguments.
if [ "$*" != "" ]
   then
	echo "usage: ./feedback.sh"
	exit 1
fi

# Make sure participant is running feedback.sh from the install floppy.
if [ ! -f ./solaris/boot.bin ]
   then
	echo "ERROR: feedback.sh must be run from floppy's root directory."
	exit 2
fi

# Display welcome and instructions.
/bin/cat << EOF

Thank you for participating in the x86 Device Configuration Trial!

This script will send your comments and configuration files to the
DevConf group.

EOF

# Select your editor of choice for comments.
DEF_EDITOR=/usr/bin/vi
[ -z "$EDITOR" ] && EDITOR=${DEF_EDITOR}
[ -x `which $EDITOR` ] || EDITOR=${DEF_EDITOR}
echo "Which editor do you wish to use [$EDITOR]? "
read x
[ -n "${x}" ] && EDITOR=${x}
if [ ! -x `which ${EDITOR}` ]
   then
	echo "Cannot run \"${EDITOR}\". Using \"${DEF_EDITOR}\".\n"
        EDITOR=${DEF_EDITOR}
fi

/bin/cat > $TMPFILE << EOF
Feedback collected by feedback.sh on `date`:
EOF

/bin/cat >>$TMPFILE << EOF

Please fill in the following information and answer the questions to
the best of your ability.  Thank you for your participation.

===============================================================================

Your Name:
Your E-mail Address (or "none"):
Your Phone Number:

===============================================================================

1. Which devices, if any, were not detected?

2. Which devices, if any, were detected by the legacy scan?

3. Which bootable devices, if any, were not shown on the boot menu?

4. Were you able to successfully boot each bootable device?

	Device		Solaris Release		Boots?
	----------------------------------------------
	CD-ROM
	network
	disk

5. Is the configuration assistant's user interface acceptable?

6. Other comments, suggestions, or ideas?

===============================================================================
EOF

if [ -f "/`uname -n`.conf" ]
   then
	/bin/cat /`uname -n`.conf >>$TMPFILE
   else
	echo "No output file found from system_info.sh script;"
	echo "using a blank template instead."
	/bin/cat >>$TMPFILE <<EOF

If any information in this form is not relevant for the machine under
test, feel free to type none in the field or delete it altogether.

Host Name:
IP Address:
Operating System:
Manufacturer:
Model:

MOTHERBOARD:
	Processor:
	Clock Speed:
	Memory:
	BIOS:
	PnP BIOS exists/enabled?:

If the machine supports PCI, and the BIOS has options for controlling
IRQ usage, indicate the BIOS settings below.  Indicate ONLY the IRQ
values as displayed in the BIOS setup, deleting the ones NOT displayed.

	Available for PCI:	3, 4, 5, 7, 9, 10, 11, 12, 14, 15
	Used by ISA Cards:	3, 4, 5, 7, 9, 10, 11, 12, 14, 15

BUS TYPE:

Enter the make and model of the following devices. Also, please list
the peripherals attached to the HBAs and IDE controllers.

	SCSI #1:
	SCSI #2:
	IDE #1:
		Master:
		Slave:
	IDE #2:
		Master:
		Slave:
	NETWORK #1:
	NETWORK #2:
	VIDEO:
	POINTING DEVICE:
		Bus, PS/2, Serial?:
	AUDIO:

Device
Resources:	Device 		IRQ	I/O	DMA	Memory
		----------------------------------------------

-------------------------------------------------------------------------------
EOF
fi

${EDITOR} $TMPFILE

addfile() {
	if [ -f $* ]
	   then
		uuencode `basename $*` < $* >> $TMPFILE
	fi
}
echo "Gathering files from floppy..."
addfile escd.rf
addfile solaris/bootenv.rc
for i in 0 1 2 3 4 5 6 7 8 9
do
	addfile solaris/machines/conf00$i.nam
	addfile solaris/machines/conf00$i.rc
	addfile solaris/machines/conf00$i.rf
done

# Send the report to the DevConf group.
echo "Okay to e-mail the information to ihv-drvpkg@Sun.COM? (y|n) \c"
read x
if [ `expr "$x" : "[yY].*"` != 0 ]
   then
	/bin/cat - $TMPFILE << EOF | /usr/lib/sendmail -t $MAILTO
To: $MAILTO
Subject: SSS DevConf Trial Feedback

EOF
	rm ${TMPFILE}
   else
	echo "ERROR: feedback NOT mailed.  Information left in: ${TMPFILE}"
	exit 4
fi
echo "Done.  Thanks for your time!"
echo "(Don't forget to cd out of this directory and eject the floppy.)"
