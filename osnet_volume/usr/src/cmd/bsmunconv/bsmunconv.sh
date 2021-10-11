#! /bin/sh
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# @(#)bsmunconv.sh 1.12 98/03/23 SMI
#
PROG=bsmunconv
STARTUP=/etc/security/audit_startup
TEXTDOMAIN="SUNW_OST_OSCMD"
export TEXTDOMAIN

#	/*
#	 * TRANSLATION_NOTE:
#	 * If you make changes to messages of bsmunconv command,
#	 * don't forget to make corresponding changes in bsmunconv.po file.
#	 */

permission()
{
cd /usr/lib
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
gettext "This script is used to disable the Basic Security Module (BSM).\n"
form=`gettext "Shall we continue the reversion to a non-BSM system now? [y/n]"`
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

bsmunconvert()
{
# Move the startup script aside

form=`gettext "%s: INFO: moving aside %s/etc/security/audit_startup."`
printf "${form}\n" $PROG $ROOT
if [ -f ${ROOT}/etc/security/audit_startup ]
then
    mv ${ROOT}/etc/security/audit_startup ${ROOT}/etc/security/audit_startup.sav
fi

# restore volume manager init file moved aside by bsmconv to prevent
# running volume manager when bsm is enabled

if [ ! -f ${ROOT}/etc/rc2.d/S92volmgt ]
then
    form=`gettext "%s: INFO: restore %s/etc/rc2.d/S92volmgt."`
    printf "${form}\n" $PROG $ROOT
    if [ -r ${ROOT}/etc/security/spool/S92volmgt ]
    then
	mv ${ROOT}/etc/security/spool/S92volmgt ${ROOT}/etc/rc2.d/S92volmgt
    else
	form=`gettext "%s: INFO: unable to restore file %s/etc/rc2.d/S92volmgt."`
	printf "${form}\n" $PROG $ROOT
    fi
fi

# Turn off auditing in the loadable module

if [ -f ${ROOT}/etc/system ]
then
	form=`gettext "%s: INFO: removing c2audit:audit_load from %s/etc/system."`
	printf "${form}\n" $PROG $ROOT
	grep -v "c2audit:audit_load" ${ROOT}/etc/system > /tmp/etc1.system.$$
	grep -v "abort_enable" /tmp/etc1.system.$$ > /tmp/etc.system.$$
	rm /tmp/etc1.system.$$
	mv /tmp/etc.system.$$ ${ROOT}/etc/system
else
	form=`gettext "%s: ERROR: can't find %s/etc/system."`
	printf "${form}\n" $PROG $ROOT
	form=`gettext "%s: ERROR: audit module may not be disabled."`
	printf "${form}\n" $PROG
fi

for jobname in `at -l | cut -f2`
do
	cp -p /var/spool/cron/atjobs/${jobname} /tmp
	atrm -f ${jobname}
	mv /tmp/${jobname} /var/spool/cron/atjobs/${jobname}
done

for jobname in /var/spool/cron/crontabs/*
do
	username=`basename ${jobname}`
	id ${username} > /dev/null 2> /dev/null
	if [ $? = 0 ]
	then
		cp -p /var/spool/cron/crontabs/${username} /tmp
		crontab -r ${username}
		mv /tmp/${username} /var/spool/cron/crontabs/${username}
	fi
done

}

# main

permission

if [ $# -eq 0 ]
then
	ROOT=
	bsmunconvert
	echo
	gettext "The Basic Security Module has been disabled.\n"
	gettext "Reboot this system now to come up without BSM.\n"
else
	for ROOT in $@
	do
		bsmunconvert $ROOT
	done
	echo
	gettext "The Basic Security Module has been disabled.\n"
	gettext "Reboot each system that was disabled to come up without BSM.\n"
fi

exit 0

