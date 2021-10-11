#!/bin/sh
#
#ident	"@(#)fncheck.sh	1.6	96/05/03 SMI"
#
# Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#

AWK=/usr/bin/nawk
CAT=/usr/bin/cat
DIFF=/usr/bin/diff
EGREP=/usr/bin/egrep
FNSLIST=/usr/bin/fnlist
NISCAT=/usr/bin/niscat
YPCAT=/usr/bin/ypcat
YPMATCH=/usr/bin/ypmatch
RM="/usr/bin/rm -f"
SORT=/usr/bin/sort
UNIQ=/usr/bin/uniq
FNSCREATE=/usr/sbin/fncreate
FNSDESTROY=/usr/sbin/fndestroy
ORGUNIT=thisorgunit
NAMESERVICE=`/usr/sbin/fnselect -D`

usage()
{
        pgm=`basename $1`
	echo "$pgm: $2"
        ${CAT} <<END
Usage: $pgm [-r] [-s] [-u] [-t type] [domain_name]
END
#	hostname|username]
#	[-r(everse)|-s(ource)]
#	organization_name
	rmtmpfiles
        exit 1
}
 
hostnameflag=0
usernameflag=0
tflag=0
rsflag=1
reverseflag=0
sourceflag=0
updateflag=0
orgnameflag=1

TMP=/tmp
nishost=${TMP}/nishost.$$
nisuser=${TMP}/nisuser.$$
fnshost=${TMP}/fnshost.$$
fnsuser=${TMP}/fnsuser.$$
errorfile=${TMP}/errorfile.$$
REV_HOST=${TMP}/rev_host.$$
SOURCE_HOST=${TMP}/source_host.$$
REV_USER=${TMP}/rev_user.$$
SOURCE_USER=${TMP}/source_user.$$
RESULT=${TMP}/result.$$

# set -x 


while getopts t:rsu c
do
        case $c in
        t)      # hostname or username
                case $OPTARG in
                hostname) hostnameflag=1
                        ;;
                username) usernameflag=1
                        ;;
                *)      usage $0 "Wrong option for -t"
                        ;;
                esac
                tflag=1
                ;;
        r)      # reverse
                reverseflag=1
		rsflag=0
                ;;
        s)      # source
                sourceflag=1
		rsflag=0
                ;;
        u)      # update
                updateflag=1
                ;;
        *)      # illegal value
                usage $0 " "
                exit 1
                ;;
        esac
done
 
shift `expr $OPTIND - 1`
org_name=$1

if [ x"$org_name" = x ]
then
	org_name=`/usr/bin/domainname`.
	orgnameflag=0
fi

rmtmpfiles ()
{
	$RM $nishost $nisuser $fnshost $fnsuser $errorfile $REV_HOST $REV_USER \
		$SOURCE_HOST $SOURCE_USER $RESULT
}
 
trap `rmtmpfiles ; exit 1` 0 1 2 15

compare ()
{
	SOURCE=$1
	DEST=$2
	FILE=$3

	${DIFF} $SOURCE $DEST | ${EGREP} '^<' | \
		${AWK} '{print $2}' >> $FILE
}


# add entries in FNSP from NIS+
# if updateflag is not set then add it to the RESULTS file
# $1 - the string for type of entry "user" or "host"
# $2 - file that contains the entries to be added
add()
{

	TYPE=$1
	FILE=$2
	if [ $updateflag -eq 0 ]
	then
		echo "${TYPE}s in ${NAMESERVICE} table with no FNS contexts :" >> $RESULT
		${CAT} $FILE >> ${RESULT}
	else
		echo "Adding ${TYPE}s in FNS"
		if [ "$NAMESERVICE" = "nis" -a "${TYPE}" = "host" ]
		then
			TEMP_FILE=$FILE.tmp
			${RM} $TEMP_FILE
			${CAT} /dev/null >> $TEMP_FILE
			AWK__FILE=$FILE.awk
			${RM} $AWK__FILE
			echo "BEGIN { ORS=\" \" }" > $AWK__FILE
			echo "{ for (i = 2; i <= NF; i++) print $i }" >> $AWK__FILE
			for u in `${CAT} $FILE`
			do
				${YPMATCH} $u hosts.byname | awk -f $AWK__FILE >> $TEMP_FILE
				echo " " > $TEMP_FILE
			done
			${CAT} ${TEMP_FILE} | ${SORT} | ${UNIQ} > $FILE
		fi
		if [ "$NAMESERVICE" = "files" -a "${TYPE}" = "host" ]
		then
			TEMP_FILE=$FILE.tmp
			${RM} $TEMP_FILE
			${CAT} /dev/null >> $TEMP_FILE
			AWK__FILE=$FILE.awk
			${RM} $AWK__FILE
			echo "BEGIN { ORS=\" \" }" > $AWK__FILE
			echo "{ for (i = 2; i <= NF; i++) print $i }" >> $AWK__FILE
			for u in `${CAT} $FILE`
			do
				${EGREP} $u /etc/hosts | awk -F# '{print $1}' \
					| awk -f $AWK__FILE >> $TEMP_FILE
				echo " " > $TEMP_FILE
			done
			${CAT} ${TEMP_FILE} | ${SORT} | ${UNIQ} > $FILE
		fi

#		Update the FNS maps
		if [ orgnameflag -eq 1 ]
		then
			${FNSCREATE} -t ${TYPE}name -f $FILE org/${org_name}/${TYPE}
		else
			${FNSCREATE} -t ${TYPE}name -f $FILE ${ORGUNIT}/${TYPE}
		fi
	fi
}


# delete entries in FNSP that don't exist in NIS+
# $1 - the string for type of entry "user" or "host"
# $2 - file that contains the entries to be deleted from FNSP
delete()
{
	TYPE=$1
	FILE=$2

	if [ $updateflag -eq 0 ]
	then
		echo "${TYPE} contexts in FNS that are not in corresponding ${NAMESERVICE} table :">> $RESULT
		${CAT} $FILE >> ${RESULT}
	else
		echo "Deleting ${TYPE}s in FNS"
		for u in `${CAT} $FILE`
		do
			# If there are subcontext, error message will be
			# printed to that effect
		if [ orgnameflag -eq 1 ]
		then
			${FNSDESTROY} org/${org_name}/$TYPE/$u/service
			${FNSDESTROY} org/${org_name}/$TYPE/$u
		else
			${FNSDESTROY} ${ORGUNIT}/$TYPE/$u/service
			${FNSDESTROY} ${ORGUNIT}/$TYPE/$u
		fi
		done
	fi
}

# Updates the users and hosts by calling the add/delete functions
# $1 nis file name
# $2 type - user/host
# $3 fns file name
# $4 source file name
# $5 reverese file name
update()
{
	NISFILE=$1
	HU_TYPE=$2
	FNSFILE=$3
	SRC_FILE=$4
	REV_FILE=$5

	# Check if name_service specific output file exists
	if [ ! -s $NISFILE ]
	then
		usage $0 "${NAMESERVICE} Error: Incorrect domain_name"
	fi

	if [ $orgnameflag -eq 1 ]
	then
		${FNSLIST} org/${org_name}/$HU_TYPE |\
			 ${SORT} > $FNSFILE
	else
		${FNSLIST} ${ORGUNIT}/$HU_TYPE |\
			 ${SORT} > $FNSFILE
	fi

	# Check the correctness of domain name for FNS
	${EGREP} "Error:" -f $FNSFILE > $errorfile 2> /dev/null
	if [ -s $errorfile ]
	then
		usage $0 "FNS Error: Incorrect domain_name"
	fi

	if [ $rsflag -eq 1 ]
	then
		compare $NISFILE $FNSFILE $SRC_FILE
		compare $FNSFILE $NISFILE $REV_FILE

		if [ ! -s $REV_FILE -a ! -s $SRC_FILE ]
		then
			echo "${NAMESERVICE} $HU_TYPE table and FNS contexts are consistent."
		else
			if [ -s $SRC_FILE ]
			then
				add $HU_TYPE $SRC_FILE
			else
				echo "All ${NAMESERVICE} ${HU_TYPE}s table have $HU_TYPE contexts in FNS."
			fi
			if [ -s $REV_FILE ]
			then
				delete $HU_TYPE $REV_FILE
			else
				echo "All ${HU_TYPE}s contexts in FNS are in the ${NAMESERVICE} $HU_TYPE table."
			fi
		fi
	fi
	# source flag only
	if [ $sourceflag -eq 1 ]
	then
		compare $NISFILE $FNSFILE $SRC_FILE

		if [ -s $SRC_FILE ]
		then
			add $HU_TYPE $SRC_FILE
		else
			echo "All ${NAMESERVICE} ${HU_TYPE}s table have $HU_TYPE contexts in FNS."
		fi
	fi
	# reverse flag only
	if [ $reverseflag -eq 1 ]
	then
		compare $FNSFILE $NISFILE $REV_FILE

		if [ -s $REV_FILE ]
		then
			delete $HU_TYPE $REV_FILE
		else
			echo "All ${HU_TYPE}s contexts in FNS are in the ${NAMESERVICE} $HU_TYPE table."
		fi
	fi
}	


# Routine that will be called if NIS+ is the underlying naming server
nisplus()
{
	if [ $hostnameflag -eq 1 -o $tflag -eq 0 ]
	then	
		${NISCAT} hosts.org_dir.${org_name} 2> /dev/null | ${AWK} '{print $2}' |\
			 ${SORT} | ${UNIQ} > $nishost

		update $nishost host $fnshost $SOURCE_HOST $REV_HOST
	fi

	if [ $usernameflag -eq 1 -o $tflag -eq 0 ]
	then
		${NISCAT} passwd.org_dir.${org_name} 2> /dev/null | ${AWK} -F":" '{print $1}' | \
			${SORT} | ${UNIQ} > $nisuser

		update $nisuser user $fnsuser $SOURCE_USER $REV_USER
	fi
}

#Routine to be called if NIS is the underlying name service
nis()
{
	org_name=`/usr/bin/domainname`
	orgnameflag=0
	if [ $usernameflag -eq 1 -o $tflag -eq 0 ]
	then
		if [ ! -f /var/yp/${org_name}/fns_user.ctx.pag ]
		then
			echo "Not FNS NIS server"
			exit 1;
		fi
		/usr/bin/ypcat passwd | awk -F: '{print $1}' |\
			${SORT} | ${UNIQ} > $nisuser
		update $nisuser user $fnsuser $SOURCE_USER $REV_USER
	fi

	if [ $hostnameflag -eq 1 -o $tflag -eq 0 ]
	then
		if [ ! -f /var/yp/${org_name}/fns_host.ctx.pag ]
		then
			echo "Not FNS NIS server"
			exit 1;
		fi
		/usr/bin/ypcat -k hosts.byname | awk '{print $1}' | \
			${SORT} | ${UNIQ} > $nishost
		update $nishost host $fnshost $SOURCE_HOST $REV_HOST
	fi
}

files()
{
	orgnameflag=0
	if [ $usernameflag -eq 1 -o $tflag -eq 0 ]
	then
		if [ ! -f /var/fn/fns_user.ctx.pag ]
		then
			echo "Not FNS /etc files server"
			exit 1;
		fi
		awk -F: '{print $1}' /etc/passwd | ${SORT} | ${UNIQ} > $nisuser
		update $nisuser user $fnsuser $SOURCE_USER $REV_USER
	fi

	if [ $hostnameflag -eq 1 -o $tflag -eq 0 ]
	then
		if [ ! -f /var/fn/fns_host.ctx.pag ]
		then
			echo "Not FNS /etc files server"
			exit 1;
		fi
		awk 'BEGIN { FS ="#" } { print $1 }' /etc/hosts |\
			awk '{ for (i = 2; i <= NF; i++) print $i }' \
			${SORT} | ${UNIQ} > $nishost
		update $nishost host $fnshost $SOURCE_HOST $REV_HOST
	fi
}

if [ "$NAMESERVICE" = "nisplus" ]
then
	nisplus
else	if [ "$NAMESERVICE" = "nis" ]
	then
		nis
	else	if [ "$NAMESERVICE" = "files" ]
		then
			files
		else
			echo "Unknown name service: ${NAMESERVICE}"
		fi
	fi
fi

if [ -f $RESULT ]
then 
	${CAT} $RESULT
fi

rmtmpfiles

exit 0
