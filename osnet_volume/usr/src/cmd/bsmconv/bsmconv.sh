#! /bin/sh
#
# @(#)bsmconv.sh 1.14 98/01/21 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
PROG=bsmconv
STARTUP=/etc/security/audit_startup
DEVALLOC=/etc/security/device_allocate
DEVMAPS=/etc/security/device_maps
TEXTDOMAIN="SUNW_OST_OSCMD"
export TEXTDOMAIN

#	/*
#	 * TRANSLATION_NOTE:
#	 * If you make changes to messages of bsmconv command,
#	 * don't forget to make corresponding changes in bsmconv.po file.
#	 */

permission()
{
WHO=`id | cut -f1 -d" "`
if [ ! "$WHO" = "uid=0(root)" ]
then
	form=`gettext "%s: ERROR: you must be super-user to run this script."`
	printf "${form}\n" $PROG
	exit 1
fi

RESP="x"
while [ "$RESP" != `gettext "y"` -a "$RESP" != `gettext "n"` ]
do
gettext "This script is used to enable the Basic Security Module (BSM).\n"
form=`gettext "Shall we continue with the conversion now? [y/n]"`
echo "$form \c"
read RESP
done

if [ "$RESP" = `gettext "n"` ]
then
	form=`gettext "%s: INFO: aborted, due to user request."`
	printf "${form}\n" $PROG
	exit 2
fi
}

# Do some sanity checks to see if the arguments to bsmconv
# are, in fact, root directories for clients.
sanity_check()
{
for ROOT in $@
do

	if [ -d $ROOT -a -w $ROOT -a -f $ROOT/etc/system -a -d $ROOT/usr ]
	then
		# There is a root directory to write to,
		# so we can potentially complete the conversion.
		:
	else
		form=`gettext "%s: ERROR: %s doesn't look like a client's root."`
		printf "${form}\n" $PROG $ROOT
		form=`gettext "%s: ABORTED: nothing done."`
		printf "${form}\n" $PROG
		exit 4
	fi
done
}

# bsmconvert
#	All the real work gets done in this function

bsmconvert()
{

# If there is not startup file to be ready by /etc/rc2.d/S99audit,
# then make one.

form=`gettext "%s: INFO: checking startup file."`
printf "${form}\n" $PROG 
if [ ! -f ${ROOT}/${STARTUP} ]
then
	cat > ${ROOT}/${STARTUP} <<EOF
#!/bin/sh
auditconfig -conf
auditconfig -setpolicy none
auditconfig -setpolicy +cnt
EOF
fi

if [ ! -f ${ROOT}/${STARTUP} ]
then
	form=`gettext "%s: ERROR: no %s file."`
	printf "${form}\n" $PROG $STARTUP
	form=`gettext "%s: Continuing ..."`
	printf "${form}\n" $PROG
fi

chgrp sys ${ROOT}/${STARTUP} > /dev/null 2>&1
chmod 0744 ${ROOT}/${STARTUP} > /dev/null 2>&1

# move aside volume manager init file to prevent
# running volume manager when bsm is enabled

form=`gettext "%s: INFO: move aside %s/etc/rc2.d/S92volmgt."`
printf "${form}\n" $PROG $ROOT
if [ -r ${ROOT}/etc/rc2.d/S92volmgt ]
then
    if [ ! -d ${ROOT}/etc/security/spool ]
    then
    	mkdir ${ROOT}/etc/security/spool
	if [ $? != 0 ]
	then
	    form=`gettext "%s: ERROR: unable to create %s/etc/security/spool"`
	    printf "${form}\n" $PROG $ROOT
	    exit 5
	fi
    fi
    mv ${ROOT}/etc/rc2.d/S92volmgt ${ROOT}/etc/security/spool/S92volmgt
fi

# Turn on auditing in the loadable module

form=`gettext "%s: INFO: turning on audit module."`
printf "${form}\n" $PROG
if [ ! -f ${ROOT}/etc/system ]
then
	echo "" > ${ROOT}/etc/system
fi

grep -v "c2audit:audit_load" ${ROOT}/etc/system > /tmp/etc.system.$$
echo "set c2audit:audit_load = 1" >> /tmp/etc.system.$$
echo "set abort_enable = 0" >> /tmp/etc.system.$$
mv /tmp/etc.system.$$ ${ROOT}/etc/system
grep "set c2audit:audit_load = 1" ${ROOT}/etc/system > /dev/null 2>&1
if [ $? -ne 0 ]
then
    form=`gettext "%s: ERROR: cannot 'set c2audit:audit_load = 1' in %s/etc/system"`
    printf "${form}\n" $PROG $ROOT
    form=`gettext "%s: Continuing ..."`
    printf "${form}\n" $PROG
fi

# Initial device allocation files

form=`gettext "%s: INFO: initializing device allocation files."`
printf "${form}\n" $PROG
if [ ! -f ${ROOT}/$DEVALLOC ]
then
	mkdevalloc > ${ROOT}/$DEVALLOC
fi
if [ ! -f $DEVMAPS ]
then
	mkdevmaps > ${ROOT}/$DEVMAPS
fi

}

# main loop

permission
sanity_check
if [ $# -eq 0 ]
then
	ROOT=
	bsmconvert
	echo
	gettext "The Basic Security Module is ready.\n"
	gettext "If there were any errors, please fix them now.\n"
	gettext "Configure BSM by editing files located in /etc/security.\n"
	gettext "Reboot this system now to come up with BSM enabled.\n"
else
	for ROOT in $@
	do
		conv_host=`basename $ROOT`
		form=`gettext "%s: INFO: converting host %s ..."`
		printf "${form}\n" $PROG $conv_host
		bsmconvert $ROOT
		form=`gettext "%s: INFO: done with host %s"`
		printf "${form}\n" $PROG $conv_host
	done
	echo
	gettext "The Basic Security Module is ready.\n"
	gettext "If there were any errors, please fix them now.\n"
	gettext "Configure BSM by editing files located in /etc/security\n"
	gettext "in the root directories of each host converted.\n"
	gettext "Reboot each system converted to come up with BSM active.\n"
fi

exit 0
