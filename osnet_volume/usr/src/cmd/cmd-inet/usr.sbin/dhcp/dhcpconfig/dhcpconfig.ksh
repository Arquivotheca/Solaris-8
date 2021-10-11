#! /usr/bin/ksh
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)dhcpconfig.ksh	1.39	99/11/18 SMI"

DHCPCONFIG=/etc/default/dhcp
DHCPCONFIGLOCK=/etc/default/.dhcp_defaults_lock
DHCPTDIR=/var/run
DHCPC_GRP=sys
DHCPC_MODE=0644
SRVNAME=`uname -n`
LOOPBACK="127.0.0.1"
SAVEPATH=${PATH}
TRAPSIGS="0 1 2 3 15"

NISRESRC=nisplus
NISRPATH=`domainname`.
DEFNISGRP="admin"
DEFRESRC=${NISRESRC}
DEFRPATH=${NISRPATH}

FILESRPATH=/var/dhcp

DEFNMROOT="${SRVNAME}-"
DEFCNT=1
DEFPINGTIME=2		# default time for ping response timeout.

# defaults file settings
RUN_MODE=""
VERBOSE=""
RELAY_HOPS=""
INTERFACES=""
ETHERS_COMPAT=""
ICMP_VERIFY=""
OFFER_CACHE_TIMEOUT=""
RESCAN_INTERVAL=""
LOGGING_FACILITY=""
BOOTP_COMPAT=""
RELAY_DESTINATIONS=""
RESOURCE=""
RPATH=""

OFFER_CACHE_DEFAULT=10	# default timeout for DHCPOFFER cache.

BOOTP_DYNAMIC=0
DEF_LEASE=3		# In Days.
DNSSERV=""
DNSDMAIN=""

# administrative command return codes.
SUCCESS=0
EXISTS=1
ENOENT=2
WARNING=3
CRITICAL=4

PATH=/usr/sbin:/usr/bin:/sbin

# Functions

#
# lock the DHCPCONFIG file.
#
lock_dhcp_cfg()
{
	let LOCKED=0
	let trys=0
	while [ ${trys} -lt 3 ]
	do
		if [ -f ${DHCPCONFIGLOCK} ]
		then
			# Sleep for a bit and try again.
			sleep 1
			let trys=${trys}+1
			continue
		else
			> ${DHCPCONFIGLOCK}
			LOCKED=1
			break
		fi
	done
	if [ ${LOCKED} -eq 0 ]
	then
		print - "Cannot acquire lock file: ${DHCPCONFIGLOCK}" >&2
		return 1
	fi
	return 0
}

#
# unlock the DHCPCONFIG file
#
unlock_dhcp_cfg()
{
	rm -f ${DHCPCONFIGLOCK}
	return 0
}

# takes two arguments, a PROMPT and the default value (Y | N).
# Returns 0 if the user specified "Y", nonzero otherwise.
yes_or_no()
{
	if [ ${#} -ne 2 ]
	then
		return 1
	fi
	if [ "${2}" = "Y" ]
	then
		DEFPMPT="([Y]/N):\c "
		DEFVAL="Y"
	else
		DEFPMPT="(Y/[N]):\c "
		DEFVAL="N"
	fi
	print - "${1} ${DEFPMPT}"
	read ANS
	: ${ANS:="${DEFVAL}"}
	if [ "${ANS}" != "Y" -a "${ANS}" != "y" ]
	then
		return 1
	fi
	return 0
}

#
# Make startup links.
#
makelinks()
{
	if [ -f /etc/init.d/dhcp ]
	then
		FOO=/etc/init.d/dhcp
		for i in rc3.d/S34dhcp rc2.d/K34dhcp rc1.d/K34dhcp \
		    rcS.d/K34dhcp rc0.d/K34dhcp
		do
			if [ ! -f /etc/${i} ]
			then
				ln ${FOO} /etc/${i}
			fi
		done
		unset FOO
		return 0
	fi
	return 1
}

#
# Destroy startup links
#
zaplinks()
{
	rm -f /etc/rc3.d/S34dhcp /etc/rc2.d/K34dhcp /etc/rc1.d/K34dhcp \
	    /etc/rcS.d/K34dhcp /etc/rc0.d/K34dhcp
	return 0
}

# Determine if pcadmin is installed, and offer possiblity to select backwards
# compatibility.
handle_pcadmin()
{
	if [ -f /etc/opt/SUNWpcnet/nameservice ]
	then
		# 2.0+
		PCD_VERS=2
	elif [ -f /opt/SUNWpcnet/etc/nameservice ]
	then
		# 1.0 - 1.5
		PCD_VERS=1
	else
		# No pcadmin installed.
		return 0
	fi

	print - "\n###\tSolarNet PC-Admin Compatibility\t###\n"

	case ${PCD_VERS} in
	1)
		# PCAdmin 1.[0, 5] installed.
		print - "You have SolarNet PC-Admin 1.[0,5] installed."
	;;
	2)
		# PCAdmin 2+ installed.
		print - "You have SolarNet PC-Admin 2.0 installed."
	;;
	esac

	yes_or_no "Would you like to configure the DHCP service such that you can continue\nto use the PC-Admin administration and data layout in `uname -sr`" Y
	if [ ${?} -ne 0 ]
	then
		return 0
	fi
	case ${PCD_VERS} in
	1)
		# 1.[0,5]. Need to read nameservice file in
		# /opt/SUNWpcnet/etc to determine datastore in use.
		# Also need to set Default path of DHCP data to point
		# to /opt/SUNWpcnet/etc, and make a symbolic link between
		# /opt/SUNWpcnet/etc/dhcp_ip and /opt/SUNWpcnet/etc.
		# Finally, we need to warn the user to remove the startup of
		# DHCP from the solarnet startup file.
		SNSTART=/etc/init.d/solarnet
		VERSION=`pkginfo -c networking -x SUNWpcnet 2> /dev/null | \
		awk '/^ / {a=$2; FS = "."; split(a,b); printf("%s%s", b[1], b[2]); }'`

		if [ ${VERSION:=0} -lt 20 ]
		then
			if [ -f ${SNSTART} ] && grep -s -i dhcp ${SNSTART} > /dev/null 2>&1
			then
				print - "Error: You must edit ${SNSTART} and remove all lines referring to DHCP before continuing." >&2
				return 1
			fi
		fi
		NS=`cat /opt/SUNWpcnet/etc/nameservice 2> /dev/null`
		LNKSRC=/opt/SUNWpcnet/etc
		LNKDST=/opt/SUNWpcnet/etc/dhcp_ip
	;;
	2)
		# 2.+. Need to read nameservice file in
		# /etc/opt/SUNWpcnet to determine datastore in use.
		# Also need to set Default path of DHCP data to point
		# to /etc/opt/SUNWpcnet, and make a symbolic link between
		# /etc/opt/SUNWpcnet/dhcp_ip and /etc/opt/SUNWpcnet.
		NS=`cat /etc/opt/SUNWpcnet/nameservice 2> /dev/null`
		LNKSRC=/etc/opt/SUNWpcnet
		LNKDST=/etc/opt/SUNWpcnet/dhcp_ip
	;;
	esac
	case "${NS}" in
	"")
		print - "Error: missing/empty nameservice file in ${LNKSRC}" >&2
		return 1
	;;
	"nis" | "files")
		# Have we been down this road before?
		typeset -i TRANSFER
		TRANSFER=0
		if [ -s ${LNKDST} ]
		then
			TMPDST=`ls -l ${LNKDST} | awk ' { print $10 } '`
			if [ "${TMPDST}" = "${LNKSRC}" ]
			then
				let TRANSFER=1
			fi
		fi
		if [ ${TRANSFER} -eq 0 ]
		then
			# As far as DHCP is concerned, it's really files.
			# move the dhcp-network data files.
			for i in ${LNKDST}/*
			do
				if [ -f ${i} ]
				then
					if mv ${i} ${LNKSRC}
					then
						print - "Transferred: ${i}..." 
					else
						print - "Warning: could not move dhcp network table ${i} from ${LNKDST} to ${LNKSRC}." >&2
					fi
				fi
			done
			# Make the link.
			mv ${LNKDST} ${LNKDST}.old
			if ln -s ${LNKSRC} ${LNKDST}
			then
				:
			else
				print - "Error: could not link ${LNKSRC} to ${LNKDST}." >&2
				return 1
			fi
		fi
		RESOURCE=${NS}
		RPATH="${LNKSRC}"
	;;
	"nisplus")
		# Only need setup the dhcp data conf file.
		RESOURCE=${NISRESRC}
		RPATH=${NISRPATH}
	;;
	*)
		print - "Error: Unrecognized nameservice: ${NS}" >&2
		return 1
	;;
	esac

	return 0
}

#
# Unconfigure: Stops DHCP service, nukes startup links, optionally nukes
# dhcptab and dhcp-network tables, and nukes ${DHCPCONFIG}. Use with CARE!
#
unconfigure()
{

	# load config from file.
	load_dhcp_cfg
	if [ ${?} -ne 0 ]
	then
		# No config file exists.
		let CONFIG_FILE=0
	else
		let CONFIG_FILE=1
		# Issue warning
		print - "Unconfigure will stop the DHCP service and remove ${DHCPCONFIG}.">&2
	fi

	yes_or_no "Are you SURE you want to disable the DHCP service?" Y
	if [ ${?} -ne 0 ]
	then
		return 0
	fi

	# Stop DHCP service.
	/etc/init.d/dhcp stop

	# Zap the startup links
	zaplinks

	# If no known resource, then don't try to delete anything.
	if [ -z "${RESOURCE}" ]
	then
		return 0
	fi

	# Issue warning
	print - "\n\n###\t\tWARNING\tWARNING\tWARNING\t\t###\n" >&2
	print - "Unconfigure can delete the following tables in the current" >&2
	print - "resource (${RESOURCE}):\n" >&2
	print - "\t1) dhcptab.\n\t2) ALL dhcp-network tables.\n" >&2
	print - "If you are sharing the DHCP service tables either via NISplus or file" >&2
	print - "sharing among multiple DHCP servers, those servers will be unable to" >&2
	print - "service requests after these tables are removed." >&2
	print - "\nNote that any hosts table entries which have been added will need to" >&2
	print - "be manually removed.\n" >&2

	yes_or_no "Are you SURE you want to remove the DHCP tables?" N
	if [ ${?} -ne 0 ]
	then
		return 0
	fi

	# Based on RESOURCE type, nuke the tables.

	if [ ${CONFIG_FILE} -eq 0 ]
	then
		return 0
	fi

	# dhcptab
	print - "Removing: dhcptab..." >&2
	dhtadm -R
	if [ ${?} -ne 0 ]
	then
		print - "WARNING: Failed to delete dhcptab table..." >&2
	fi

	# dhcp-network tables
	case ${RESOURCE} in
	"nisplus")

		TMPFILES=`nisls org_dir.${RPATH} | grep -s '^[0-9]*_[0-9]*_[0-9]*_[0-9]*$'`
	;;
	"files")
		TMPFILES=`ls ${RPATH} | grep -s '^[0-9]*_[0-9]*_[0-9]*_[0-9]*$'`
	;;
	esac
	for PTABLE in `print - ${TMPFILES}`
	do
		print - "Removing: ${PTABLE}..." >&2
		TMP=`print - ${PTABLE} | sed -e 's/_/\./g'`
		pntadm -R ${TMP}
		if [ ${?} -ne 0 ]
		then
			print - "WARNING: Failed to delete dhcp-network table ${PTABLE}..." >&2
		fi
	done

	# Unset RESOURCE and RPATH.
	unset RESOURCE RPATH

	# Nuke configuration file.
	rm -f ${DHCPCONFIG}

	# Pause so user can see messages 
	sleep 1

	return 0
}

# Load config file parameters.
load_dhcp_cfg()
{
	let error=1

	# Load defaults from file if it currently exists.
	trap "" ${TRAPSIGS}
	if lock_dhcp_cfg
	then
		if [ -f ${DHCPCONFIG} ]
		then
			. ${DHCPCONFIG}
			if [ "${SAVEPATH}" != "${PATH}" ]
			then
				RPATH=${PATH}
				PATH=${SAVEPATH}
			fi
			let error=0
		fi
		unlock_dhcp_cfg
	else
		trap - ${TRAPSIGS}
		exit 1
	fi
	trap - ${TRAPSIGS}

	return ${error}
}

# Prompt user for creation of dhcp configuration file.
create_dhcp_cfg()
{
	print - "###\tConfigure DHCP Database Type and  Location\t###\n"

	if [ -n "${RESOURCE}" ]
	then
		DEFRESRC=${RESOURCE}
	fi
	if [ -n "${RPATH}" ]
	then
		DEFRPATH=${RPATH}
	fi

	# Resource
	while :
	do
		print - "Enter datastore (files or nisplus) [${DEFRESRC}]: \c"
		read DATA
		: ${DATA:=${DEFRESRC}}
		case "${DATA}" in
		files)
			RESOURCE=${DATA}
			if [ -z "${RPATH}" ]
			then
				DEFRPATH=${FILESRPATH}
			fi
			break;
		;;
		nisplus)
			RESOURCE=${DATA}
			if [ -z "${RPATH}" ]
			then
				DEFRPATH=${NISRPATH}
			fi
			break;
		;;
		*)
			# print valid values, pause to allow user to read them
			print - "Valid values are 'files' or 'nisplus'"
			sleep 1
		;;
		esac
	done

	# Path
	while :
	do
		print - "Enter absolute path to datastore directory [${DEFRPATH}]: \c"
		read DATA
		: ${DATA:=${DEFRPATH}}
		case "${RESOURCE}" in
		files)
			TMP=`print - ${DATA} | cut -c1`
			if [ "${TMP}" != "/" ]
			then
				print - "You must specify an absolute path." >&2
				continue
			fi
			test -d ${DATA} || mkdir ${DATA}
			RPATH=${DATA}
			break;
		;;
		nisplus)
			# are we even running nis+?
			if [ -f /var/nis/NIS_COLD_START ] &&  \
				/usr/lib/nis/nisstat > /dev/null 2>&1
			then
				/usr/lib/nis/nisstat ${DATA} > /dev/null 2> ${DHCPTDIR}/DhcpErr.$$
				if [ ${?} -eq 0 ]
				then
					RPATH=${DATA}
					rm -f ${DHCPTDIR}/DhcpErr.$$
				else
					print - "Error: `cat ${DHCPTDIR}/DhcpErr.$$`" >&2
					rm -f ${DHCPTDIR}/DhcpErr.$$
					continue
				fi
			else
				print - "This machine doesn't appear to be running nis+.\nTo set it up, see the  nisclient(1) and nisserver(1) manual pages." >&2
				return 1 
			fi
			RPATH=${DATA}
			break;
		;;
		esac
	done

	trap "" ${TRAPSIGS}
	if lock_dhcp_cfg
	then
		OLD_RESOURCE=""
		if grep -s '^RESOURCE=' ${DHCPCONFIG} > /dev/null 2>&1
		then
			OLD_RESOURCE=`grep '^RESOURCE=' ${DHCPCONFIG} | \
 			    sed -e 's/^RESOURCE=//' 2> /dev/null`
		fi
		if [ -z "${OLD_RESOURCE}" -o \( "${OLD_RESOURCE}" != "${RESOURCE}" \) ]
		then
			print - "RESOURCE=${RESOURCE}" >> ${DHCPCONFIG}
		fi
		OLD_RPATH=""
		if grep -s '^PATH=' ${DHCPCONFIG} > /dev/null 2>&1
		then
			OLD_RPATH=`grep  '^PATH=' ${DHCPCONFIG} | \
			    sed -e 's/^PATH=//' 2> /dev/null`
		fi
		if [ -z "${OLD_RPATH}" -o \( "${OLD_RPATH}" != "${RPATH}" \) ]
		then
			print - "PATH=${RPATH}" >> ${DHCPCONFIG}
		fi
		chgrp ${DHCPC_GRP} ${DHCPCONFIG}
		chmod ${DHCPC_MODE} ${DHCPCONFIG}
		unlock_dhcp_cfg
	else
		trap - ${TRAPSIGS}
		exit 1
	fi
	trap - ${TRAPSIGS}

	if [ "${RESOURCE}" = "${NISRESRC}" ]
	then
		if [ -z "${NIS_GROUP}" ]
		then
			if [ -z "${RPATH}" ]
			then
				X=`domainname`.
			else
				X=${RPATH}
			fi
			print - "Warning: Setting NIS_GROUP to ${DEFNISGRP}.${X}" >&2
			export NIS_GROUP=${DEFNISGRP}.${X}
			unset X
		fi
	fi

	return 0
}

#
# Commit updated defaults file settings to defaults file.
#
commit_dhcp_cfg()
{
	trap "" ${TRAPSIGS}
	if lock_dhcp_cfg
	then
		if [ -n "${RUN_MODE}" ]
		then
			print - "RUN_MODE=${RUN_MODE}" > ${DHCPCONFIG}
		fi
		if [ -n "${VERBOSE}" ]
		then
			print - "VERBOSE=${VERBOSE}" >> ${DHCPCONFIG}
		fi
		if [ -n "${RELAY_HOPS}" ]
		then
			print - "RELAY_HOPS=${RELAY_HOPS}" >> ${DHCPCONFIG}
		fi
		if [ -n "${INTERFACES}" ]
		then
			print - "INTERFACES=${INTERFACES}" >> ${DHCPCONFIG}
		fi
		if [ -n "${ETHERS_COMPAT}" ]
		then
			print - "ETHERS_COMPAT=${ETHERS_COMPAT}" >> ${DHCPCONFIG}
		fi
		if [ -n "${ICMP_VERIFY}" ]
		then
			print - "ICMP_VERIFY=${ICMP_VERIFY}" >> ${DHCPCONFIG}
		fi
		if [ -n "${OFFER_CACHE_TIMEOUT}" ]
		then
			print - "OFFER_CACHE_TIMEOUT=${OFFER_CACHE_TIMEOUT}" >> ${DHCPCONFIG}
		fi
		if [ -n "${RESCAN_INTERVAL}" ]
		then
			print - "RESCAN_INTERVAL=${RESCAN_INTERVAL}" >> ${DHCPCONFIG}
		fi
		if [ -n "${LOGGING_FACILITY}" ]
		then
			print - "LOGGING_FACILITY=${LOGGING_FACILITY}" >> ${DHCPCONFIG}
		fi
		if [ -n "${BOOTP_COMPAT}" ]
		then
			print - "BOOTP_COMPAT=${BOOTP_COMPAT}" >> ${DHCPCONFIG}
		fi
		if [ -n "${RELAY_DESTINATIONS}" ]
		then
			print - "RELAY_DESTINATIONS=${RELAY_DESTINATIONS}" >> ${DHCPCONFIG}
		fi
		if [ -n "${RESOURCE}" ]
		then
			print - "RESOURCE=${RESOURCE}" >> ${DHCPCONFIG}
		fi
		if [ -n "${RPATH}" ]
		then
			print - "PATH=${RPATH}" >> ${DHCPCONFIG}
		fi
		chgrp ${DHCPC_GRP} ${DHCPCONFIG}
		chmod ${DHCPC_MODE} ${DHCPCONFIG}
		unlock_dhcp_cfg
	else
		trap - ${TRAPSIGS}
		exit 1
	fi
	trap - ${TRAPSIGS}
	return 0
}

#
# Set common deamon options.
# We just handle transaction logging now. We should
# handle hop count as well.
#
common_options()
{
	print - "\n###\tCommon daemon option setup\t###\n"
	yes_or_no "Would you like to specify nondefault daemon options" N
	if [ ${?} -ne 0 ]
	then
		return 0
	fi
	unset VAL
	typeset -i VAL

	yes_or_no "Do you want to enable transaction logging?" N
	if [ ${?} -eq 0 ]
	then
		while :
		do
			if [ -n "${LOGGING_FACILITY}" ]
			then
				let DVAL=${LOGGING_FACILITY}
			else
				let DVAL=0
			fi
			print - "Which syslog local facility [0-7] do you wish to log to? [${DVAL}]:\c "
			read OPT
			let VAL=${OPT:=${DVAL}}
			unset DVAL
			if [ ${VAL} -lt 0 -o ${VAL} -gt 7 ]
			then
				print - "\nlocal facility must be an integer between 0 and 7.\n" >&2
				continue
			fi
			LOGGING_FACILITY="${VAL}"
			print - "\nRemember to add a line to /etc/syslog.conf, e.g:\n\n\tlocal${VAL}.notice\t\t\t/var/log/dhcpsvc"
			break
		done
	fi
	return 0
}

#
# Set dhcp server specific options.
# Handle DHCP offer TTL, rescan interval, and BOOTP compatibility.
#
server_options()
{
	RUN_MODE=server
	print - "\n###\tDHCP server option setup\t###\n"
	yes_or_no "Would you like to specify nondefault server options" N
	if [ ${?} -ne 0 ]
	then
		return 0
	fi

	print - "How long (in seconds) should the DHCP server keep outstanding OFFERs? [10]:\c "
	read OPT
	if [ ! -z "${OPT}" ]
	then
		VAL=`print - ${OPT} | cut -c1`
		if [ ${VAL} -ge 1 -o ${VAL} -le 9 ]
		then
			OFFER_CACHE_TIMEOUT=${OPT}
		else
			OFFER_CACHE_TIMEOUT=${OFFER_CACHE_DEFAULT}
			print - "Defaulting to ${OFFER_CACHE_TIMEOUT} seconds."
		fi
	fi

	print - "How often (in minutes) should the DHCP server rescan the dhcptab? [Never]:\c "
	read OPT
	if [ ! -z "${OPT}" ]
	then
		let VAL=${OPT}
		VAL=`print - ${OPT} | cut -c1`
		if [ ${VAL} -ge 1 -o ${VAL} -le 9 ]
		then
			RESCAN_INTERVAL=${OPT}
		else
			print - "Defaulting to no rescan interval."
		fi
	fi

	yes_or_no "Do you want to enable BOOTP compatibility mode?" N
	if [ ${?} -eq 0 ]
	then
		yes_or_no "Do you want the server to allocate IP addresses to new BOOTP clients?" Y
		if [ ${?} -eq 0 ]
		then
			BOOTP_COMPAT=automatic
			BOOTP_DYNAMIC=1
		else
			BOOTP_COMPAT=manual
			BOOTP_DYNAMIC=0
		fi
	fi
	return 0
}

#
# Set options in the dhcp startup file for the BOOTP relay service.
# Only handle relay destination option.
#
relay_options()
{
	RUN_MODE=relay
	# prompt the user for BOOTP/DHCP servers
	AGENTS=""
	print - "Enter destination BOOTP/DHCP servers. Type '.' when finished.\n"
	print - "IP address or Hostname: \c"
	read info
	while [ "${info}" != "." ]
	do
		if [ "${AGENTS}" != "" ]
		then
			AGENTS="${AGENTS},${info}"
		else
			AGENTS=${info}
		fi
		print - "IP address or Hostname: \c"
		read info
	done

	RELAY_DESTINATIONS="${AGENTS}"
	unset AGENTS

	return 0
}

#
# Display UTC offset. Positive offsets east of GMT; negative west of GMT.
# Attempt to handle the boundary cases where time roles over (minutes, hours,
# or days).
#
get_time_offset()
{
	# Contents of the below while loop must match the following 4 lines.
	local_time=`date '+%d %H %M'`
	gmt_time=`date -u '+%d %H %M'`
	local_mins=`print - ${local_time} | awk ' { print $3 } '`
	gmt_mins=`print - ${gmt_time} | awk ' { print $3 } '`

	while [ ${local_mins} != ${gmt_mins} ]
	do
		local_time=`date '+%d %H %M'`
		gmt_time=`date -u '+%d %H %M'`
		local_mins=`print - ${local_time} | awk ' { print $3 } '`
		gmt_mins=`print - ${gmt_time} | awk ' { print $3 } '`
	done

	local_day=`print - ${local_time} | awk ' { print $1 }'`
	gmt_day=`print - ${gmt_time} | awk ' { print $1 }'`

	let local_hours=`print - ${local_time} | awk ' { print $2 }'`
	let gmt_hours=`print - ${gmt_time} | awk ' { print $2 }'`

	# adjust to total minutes (include hours)
	let local_mins=${local_mins}+\(${local_hours}\*60\)
	let gmt_mins=${gmt_mins}+\(${gmt_hours}\*60\)

	if [ ${gmt_day} -lt ${local_day} ]
	then
		# increment total local mins by one day.
		let local_mins=${local_mins}+1440
	elif [ ${gmt_day} -gt ${local_day} ]
	then
		# decrement total local mins by one day
		let local_mins=${local_mins}-1440
	fi
	let time=${local_mins}-${gmt_mins}

	# Convert to seconds
	let time=${time}\*60
	print - "UTCoffst=${time}\c"
}

#
# Set NISPLUSSERV and NISPLUSDMAIN if server running NIS+, and/or NISSERV and
# NISDMAIN if NIS or NIS+ in YP compat mode. Returns 0 if these variables
# are set, nonzero otherwise.
#
get_nis_parms()
{
	unset NISPLUSSERV
	unset NISPLUSDMAIN
	unset NISSERV
	unset NISDMAIN
	NSSHOSTS=`grep '^hosts:' /etc/nsswitch.conf`
	for TMP in ${NSSHOSTS}
	do
		case "${TMP}" in
		"nis")
			ypwhich > /dev/null 2>&1
			if [ ${?} -eq 0 ]
			then
				X=`ypwhich -m hosts`
				NISSERV=""
				for NSERV in `ypmatch ${X} hosts.byname | awk '{ print $1 }'`
				do
					if [ -z "${NISSERV}" ]
					then
						NISSERV=${NSERV}
					else
						NISSERV="${NISSERV} ${NSERV}"
					fi
				done
				unset NSERV
				NISDMAIN=`domainname`
			fi
		;;
		"nisplus")
			# are we even running nis+?
			if [ -f /var/nis/NIS_COLD_START ] &&  \
				/usr/lib/nis/nisstat > /dev/null 2>&1
			then
				NISPLUSDMAIN=`domainname`
				NH=`/usr/lib/nis/nisstat 2> /dev/null | awk '\
				BEGIN { string = ""; }
				/^Statistics from server :/ {
					num = split($5, servers, ".");
					if (string == "")
						string=servers[1];
					else
						string=string" "servers[1];
				} END { printf("%s", string); }'`
				for i in ${NH}
				do
					FOO=`nismatch name=${i} hosts.org_dir.${NISPLUSDMAIN}. 2> /dev/null | head -1 | awk ' { print $3 }'`
					if [ ${?} -eq 0 ]
					then
						if [ -z "${NISPLUSSERV}" ]
						then
							NISPLUSSERV=${FOO}
						else
							NISPLUSSERV="${NISPLUSSERV} ${FOO}"
						fi
					fi
				done
				unset NH
				unset i
				unset FOO
			fi
		;;
		esac
		unset TMP
	done
}

# Set DNSSERV and DNSDMAIN if server has a resolv.conf. Return 0 if it does,
# nonzero otherwise.
get_dns_parms()
{
	if [ ! -f /etc/resolv.conf ]
	then
		return 1
	fi
	
	nsr=0
	dmn=0
	DNSRV=""
	DNAME=""
	for item in `cat /etc/resolv.conf`
	do
		if [ "${item}" = "nameserver" ]
		then
			nsr=1
			continue
		fi
		if [ "${item}" = "domain" ]
		then
			dmn=1
			continue
		fi
		if [ ${nsr} -eq 1 ]
		then
			if [ -z "${DNSRV}" ]
			then
				DNSRV=${item}
			else
				DNSRV="${DNSRV} ${item}"
			fi
			nsr=0
			continue
		fi
		if [ ${dmn} -eq 1 ]
		then
			DNAME="${item}"
			dmn=0
			continue
		fi
	done
	if [ ! -z "${DNSRV}" ]
	then
		DNSSERV="DNSserv=${DNSRV}"
	fi
	if [ ! -z "${DNAME}" ]
	then
		DNSDMAIN="DNSdmain=${DNAME}"
	fi
	if [ ! -z "${DNSSERV}" -o ! -z "${DNSDMAIN}" ]
	then
		return 0
	fi
	return 1
}

#
# Return the primary interface's IP address.
#
get_server_ip()
{
	awk '$1 ~ /^[0-9]/ && $2 == "'$SRVNAME'" { printf "%s", $1; exit }' /etc/inet/hosts
}

#
# Display default routers
#
get_default_routers()
{
	netstat -f inet -rn | grep default | awk '
	BEGIN { FS = " "; Routers = ""; count = 0; }
	{
		if ($2 != "") {
			if (count++ == 0)
				Routers=$2
			else
				Routers=Routers" "$2
		}
	}
	END { if (Routers != "") printf("%s", Routers); }'
}

#
# Display nonloopback, UP, and IPv4 network interfaces.
#
get_interfaces()
{
	ifconfig -a4 | grep 'flags=[0-9]*.*[^A-Z]UP[^A-Z]' |  cut -d: -f1 | sed -e '/lo0/d' | sort -u
}

#
# Verify a dotted IP address.
#
ipcheck()
{
        # check with expr
        IP_ADDR=`expr ${1} : '^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)$'`
        if [ ! "${IP_ADDR}" ] ; then
                print - "mal-formed IP address: $1" >&2
                return 1
        fi
	NNO1=${1%%.*}
	tmp=${1#*.}
	NNO2=${tmp%%.*}
	tmp=${tmp#*.}
	NNO3=${tmp%%.*}
	tmp=${tmp#*.}
	NNO4=${tmp%%.*}
	if [ ${NNO1} -gt 255 -o ${NNO2} -gt 255 -o ${NNO3} -gt 255 -o ${NNO4} -gt 255 ]
	then
                print - "mal-formed IP address: $1" >&2
                return 1
	fi
	return 0
}

#
# Based on the network specification, determine whether or not network is 
# subnetted or supernetted.
# Given a dotted IP network number, convert it to the default class
# network.(used to detect subnetting). Requires one argument, the
# network number. (e.g. 10.0.0.0) Echos the default network and default
# mask for success, null if error.
#
get_default_class()
{
	NN01=${1%%.*}
	tmp=${1#*.}
	NN02=${tmp%%.*}
	tmp=${tmp#*.}
	NN03=${tmp%%.*}
	tmp=${tmp#*.}
	NN04=${tmp%%.*}
	RETNET=""
	RETMASK=""

	typeset -i16 ONE=10#${1%%.*}
	typeset -i10 X=$((${ONE}&16#f0))
	if [ ${X} -eq 224 ]
	then
		# Multicast
		typeset -i10 TMP=$((${ONE}&16#f0))
		RETNET="${TMP}.0.0.0"
		RETMASK="240.0.0.0"
	fi
	typeset -i10 X=$((${ONE}&16#80))
	if [ -z "${RETNET}" -a ${X} -eq 0 ]
	then
		# Class A
		RETNET="${NN01}.0.0.0"
		RETMASK="255.0.0.0"
	fi
	typeset -i10 X=$((${ONE}&16#c0))
	if [ -z "${RETNET}" -a ${X} -eq 128 ]
	then
		# Class B
		RETNET="${NN01}.${NN02}.0.0"
		RETMASK="255.255.0.0"
	fi
	typeset -i10 X=$((${ONE}&16#e0))
	if [ -z "${RETNET}" -a ${X} -eq 192 ]
	then
		# Class C
		RETNET="${NN01}.${NN02}.${NN03}.0"
		RETMASK="255.255.255.0"
	fi
	print - ${RETNET} ${RETMASK}
	unset NNO1 NNO2 NNO3 NNO4 RETNET RETMASK X ONE
}

#
# Based on the nsswitch setting, query the netmasks table for a netmask.
# Accepts one argument, a dotted IP address.
#
get_netmask()
{
	MTMP=`getent netmasks ${1} | awk '{ print $2 }'`
	if [ ! -z "${MTMP}" ]
	then
		print - ${MTMP}
	fi
}

# Given a network number and subnetmask, return the broadcast address.
get_bcast_addr()
{
	typeset -i16 NNO1=10#${1%%.*}
	tmp=${1#*.}
	typeset -i16 NNO2=10#${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i16 NNO3=10#${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i16 NNO4=10#${tmp%%.*}

	typeset -i16 NMO1=10#${2%%.*}
	tmp=${2#*.}
	typeset -i16 NMO2=10#${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i16 NMO3=10#${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i16 NMO4=10#${tmp%%.*}

	typeset -i16 ONE
	typeset -i16 TWO
	typeset -i16 THREE
	typeset -i16 FOUR
	let ONE=\~${NMO1}\|${NNO1}
	let ONE=${ONE}\&16#ff
	let TWO=\~${NMO2}\|${NNO2}
	let TWO=${TWO}\&16#ff
	let THREE=\~${NMO3}\|${NNO3}
	let THREE=${THREE}\&16#ff
	let FOUR=\~${NMO4}\|${NNO4}
	let FOUR=${FOUR}\&16#ff
	typeset -i10 ONE
	typeset -i10 TWO
	typeset -i10 THREE
	typeset -i10 FOUR
	print - "${ONE}.${TWO}.${THREE}.${FOUR}"
}

# Given a network address and client address, return the index.
client_index()
{
	typeset -i NNO1=${1%%.*}
	tmp=${1#*.}
	typeset -i NNO2=${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i NNO3=${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i NNO4=${tmp%%.*}

	typeset -i16 NNF1
	let NNF1=${NNO1}
	typeset -i16 NNF2
	let NNF2=${NNO2}
	typeset -i16 NNF3
	let NNF3=${NNO3}
	typeset -i16 NNF4
	let NNF4=${NNO4}
	typeset +i16 NNF1
	typeset +i16 NNF2
	typeset +i16 NNF3
	typeset +i16 NNF4
	NNF1=${NNF1#16\#}
	NNF2=${NNF2#16\#}
	NNF3=${NNF3#16\#}
	NNF4=${NNF4#16\#}
	if [ ${#NNF1} -eq 1 ]
	then
		NNF1="0${NNF1}"
	fi
	if [ ${#NNF2} -eq 1 ]
	then
		NNF2="0${NNF2}"
	fi
	if [ ${#NNF3} -eq 1 ]
	then
		NNF3="0${NNF3}"
	fi
	if [ ${#NNF4} -eq 1 ]
	then
		NNF4="0${NNF4}"
	fi
	typeset -i16 NN
	let NN=16#${NNF1}${NNF2}${NNF3}${NNF4}
	unset NNF1 NNF2 NNF3 NNF4

	typeset -i NNO1=${2%%.*}
	tmp=${2#*.}
	typeset -i NNO2=${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i NNO3=${tmp%%.*}
	tmp=${tmp#*.}
	typeset -i NNO4=${tmp%%.*}
	typeset -i16 NNF1
	let NNF1=${NNO1}
	typeset -i16 NNF2
	let NNF2=${NNO2}
	typeset -i16 NNF3
	let NNF3=${NNO3}
	typeset -i16 NNF4
	let NNF4=${NNO4}
	typeset +i16 NNF1
	typeset +i16 NNF2
	typeset +i16 NNF3
	typeset +i16 NNF4
	NNF1=${NNF1#16\#}
	NNF2=${NNF2#16\#}
	NNF3=${NNF3#16\#}
	NNF4=${NNF4#16\#}
	if [ ${#NNF1} -eq 1 ]
	then
		NNF1="0${NNF1}"
	fi
	if [ ${#NNF2} -eq 1 ]
	then
		NNF2="0${NNF2}"
	fi
	if [ ${#NNF3} -eq 1 ]
	then
		NNF3="0${NNF3}"
	fi
	if [ ${#NNF4} -eq 1 ]
	then
		NNF4="0${NNF4}"
	fi
	typeset -i16 NC
	let NC=16#${NNF1}${NNF2}${NNF3}${NNF4}
	typeset -i10 ANS
	let ANS=${NC}-${NN}
	print - $ANS
}

#
# Given a dotted form of an IP address, convert it to it's hex equivalent.
#
convert_dotted_to_hex()
{
	typeset -i10 one=${1%%.*}
	typeset -i16 one=${one}
	typeset -Z2 one=${one}
	tmp=${1#*.}

	typeset -i10 two=${tmp%%.*}
	typeset -i16 two=${two}
	typeset -Z2 two=${two}
	tmp=${tmp#*.}

	typeset -i10 three=${tmp%%.*}
	typeset -i16 three=${three}
	typeset -Z2 three=${three}
	tmp=${tmp#*.}

	typeset -i10 four=${tmp%%.*}
	typeset -i16 four=${four}
	typeset -Z2 four=${four}

	 hex=`print - ${one}${two}${three}${four} | sed -e 's/#/0/g'`
	 print - 16#${hex}
	 unset one two three four tmp
}

# get the network number from an IP addr. Requires two arguments,
# ip address and netmask.
get_net_num()
{
	typeset -i16 net=`convert_dotted_to_hex ${1}`
	typeset -i16 mask=`convert_dotted_to_hex ${2}`

	typeset -i16 addr=$((${net}\&${mask}))
	typeset -i16 a=$((${addr}\&16#ff000000))
	typeset -i10 a=$((${a}>>24))

	typeset -i16 b=$((${addr}\&16#ff0000))
	typeset -i10 b=$((${b}>>16))

	typeset -i16 c=$((${addr}\&16#ff00))
	typeset -i10 c=$((${c}>>8))

	typeset -i10 d=$((${addr}\&16#ff))
	print - "${a}.${b}.${c}.${d}"
	unset net mask addr a b c d
}

#
# Generate an IP address given the network address, mask, increment.
# 
get_addr()
{
	typeset -i16 net=`convert_dotted_to_hex ${1}`
	typeset -i16 mask=`convert_dotted_to_hex ${2}`
	typeset -i16 incr=10#${3}

	# Maximum legal value - invert the mask, add to net.
	typeset -i16 mhosts=~${mask}
	typeset -i16 maxnet=${net}+${mhosts}

	# Add the incr value.
	let net=${net}+${incr}

	if [ $((${net} < ${maxnet})) -eq 1 ]
	then
		typeset -i16 a=${net}\&16#ff000000
		typeset -i10 a="${a}>>24"

		typeset -i16 b=${net}\&16#ff0000
		typeset -i10 b="${b}>>16"

		typeset -i16 c=${net}\&16#ff00
		typeset -i10 c="${c}>>8"

		typeset -i10 d=${net}\&16#ff
		print - "${a}.${b}.${c}.${d}"
	fi
	unset net mask incr mhosts maxnet a b c d
}

#
# Display / generate per interface information.
#
get_interface_info()
{
	if [ ${#} -eq 0 ]
	then
		return 
	fi

	IEINFO=`ifconfig ${1}`
	if [ ${?} -ne 0 ]
	then
		return 
	fi

	NETWORK=`netstat -f inet -n -I ${1} | awk ' NR == 2 { print $3 } '`

	print - ${IEINFO} | grep -s POINT > /dev/null 2>&1
	if [ ${?} -eq 0 ]
	then
		if [ "${NETWORK}" = "${LOOPBACK}" ]
		then
			# PPP client interface - uninteresting from a server
			# point of view.
			return
		else
			TYPE="P"
		fi
	else
		TYPE="L"
	fi

	PARMS=`print - ${IEINFO} | awk '
	BEGIN { FS = " "; b[0] = ""; b[1] = ""; b[2] = ""; count = 0; line = ""; }
	{
		split($0, a);
		for (count = 0; count < NF; count++) {
			if (a[count] == "mtu") {
				b[0]="MTU="a[count + 1];
			}
			if (a[count] == "netmask") {
				n = length(a[count + 1]);
				m = "";
				for (i = 1; i < n; i += 2) {
					d = substr(a[count + 1], i, 1);
					y = 0;
					if (d == "f")
						y = 240;
					if (d == "e")
						y = 224;
					if (d == "d")
						y = 208;
					if (d == "c")
						y = 192;
					if (d == "b")
						y = 176;
					if (d == "a")
						y = 160;
					if (d == "9")
						y = 144;
					if (d == "8")
						y = 128;
					if (d == "7")
						y = 112;
					if (d == "6")
						y = 96;
					if (d == "5")
						y = 80;
					if (d == "4")
						y = 64;
					if (d == "3")
						y = 48;
					if (d == "2")
						y = 32;
					if (d == "1")
						y = 16;
					if (d == "0")
						y = 0;
					d = substr(a[count + 1], i + 1, 1);
					if (d == "f")
						y += 15;
					if (d == "e")
						y += 14;
					if (d == "d")
						y += 13;
					if (d == "c")
						y += 12;
					if (d == "b")
						y += 11;
					if (d == "a")
						y += 10;
					if (d == "9")
						y += 9;
					if (d == "8")
						y += 8;
					if (d == "7")
						y += 7;
					if (d == "6")
						y += 6;
					if (d == "5")
						y += 5;
					if (d == "4")
						y += 4;
					if (d == "3")
						y += 3;
					if (d == "2")
						y += 2;
					if (d == "1")
						y += 1;
					if (d == "0")
						y += 0;

					if (m == "")
						m=y;
					else
						m=m"."y;
				}
				b[1]="Subnet="m;
			}
			if (a[count] == "broadcast") {
				b[2]="Broadcst="a[count + 1];
			}
		}
	} END {
		for (i = 0; i < 3; i++) {
			if (b[i] != "") {
				if (line == "")
					line=b[i];
				else
					line=b[i]":"line;
			}
		}
		if (line != "")
			printf("%s :%s:", b[1], line);
	}'`
	# apply netmask if network is point to point.
	if [ "${TYPE}" = "P" ]
	then
		TMASK=`print - ${PARMS} | awk ' { split($1, foo, "="); printf("%s", foo[2]); } '`
		NETWORK=`get_net_num ${NETWORK} ${TMASK}`
		unset TMASK
	fi
	print - "${NETWORK} ${TYPE} ${PARMS}"
}

# initialize the dhcptab. This means create it, and add the server
# macro. build_dhcp_network deals with the network macros.
build_dhcptab()
{
	print - "\n###\tInitialize dhcptab table\t###\n"

	# Create the table if it doesn't exist.
	dhtadm -C 2> ${DHCPTDIR}/DhcpErr.$$
	ERR=${?}
	if [ ${ERR} -ne ${SUCCESS} ]
	then
		if [ ${ERR} -eq ${EXISTS} ]
		then
			yes_or_no "The dhcptab table already exists.\nDo you want to merge initialization data with the existing table?" N
			if [ ${?} -ne 0 ]
			then
				rm -f ${DHCPTDIR}/DhcpErr.$$
				return 0
			fi
		else
			print - "Error: Cannot create dhcptab: `cat ${DHCPTDIR}/DhcpErr.$$`" >&2
			rm -f ${DHCPTDIR}/DhcpErr.$$
			return 1
		fi
	fi

	rm -f ${DHCPTDIR}/DhcpErr.$$

	# Assemble the Locale macro, and add it if it isn't already there.
	dhtadm -A -m Locale -d ":`get_time_offset`:" 2> ${DHCPTDIR}/DhcpErr.$$
	case ${?} in
	${SUCCESS})
		# great.
		:
	;;
	${EXISTS})
		# It already exists. Change the UTCoffst value anyway.
		dhtadm -M -m Locale -e "`get_time_offset`" 2> ${DHCPTDIR}/DhcpErr.$$
		if [ ${?} -ne ${SUCCESS} ]
		then
			print - "Warning: Unable to update UTCoffst value in 'Locale' macro definition." >&2
		fi
	;;
	*)
		print - "Error: Cannot add 'Locale' macro to dhcptab: `cat ${DHCPTDIR}/DhcpErr.$$`" >&2
		rm -f ${DHCPTDIR}/DhcpErr.$$
		return 1
	;;
	esac

	rm -f ${DHCPTDIR}/DhcpErr.$$

	# Assemble Server macro, and add it if it isn't already there.
	# Include of locale, timeserv, DNS, Lease time and Lease Neg.
	print - "Enter default DHCP lease policy (in days) [${DEF_LEASE}]: \c"
	read LEASE
	: ${LEASE:=${DEF_LEASE}}
	VAL=`print - ${LEASE} | cut -c1`
	if [ ${VAL} -ge 1 -o ${VAL} -le 9 ]
	then
		:
	else
		print - "Defaulting to ${DEF_LEASE} days."
		LEASE=${DEF_LEASE}
	fi
	let LEASE=${LEASE}\*86400

	yes_or_no "Do you want to allow clients to renegotiate their leases?" Y
	if [ ${?} -eq 0 ]
	then
		LEASENEG=":LeaseNeg"
	else
		LEASENEG=""
	fi

	TIMESERV="Timeserv=`get_server_ip`"
	SRV_MACRO=":Include=Locale:${TIMESERV}:LeaseTim=${LEASE}${LEASENEG}"

	# get dns information.
	get_dns_parms

	if [ ! -z "${DNSDMAIN}" ]
	then
		SRV_MACRO="${SRV_MACRO}:${DNSDMAIN}"
	fi
	if [ ! -z "${DNSSERV}" ]
	then
		SRV_MACRO="${SRV_MACRO}:${DNSSERV}"
	fi

	SRV_MACRO="${SRV_MACRO}:"

	dhtadm -A -m ${SRVNAME} -d "${SRV_MACRO}" 2> ${DHCPTDIR}/DhcpErr.$$
	case ${?} in
	${SUCCESS})
		# great.
		:
	;;
	${EXISTS})
		# It already exists. Change each value anyway.
		dhtadm -M -m ${SRVNAME} -e "${TIMESERV}" 2> ${DHCPTDIR}/DhcpErr.$$
		if [ ${?} -ne ${SUCCESS} ]
		then
			print - "Warning: Unable to update Timeserv value in server macro definition: ${SRVNAME}" >&2
		fi
		if [ ! -z "${LEASENEG}" ]
		then
			LEASENEG="_NULL_VALUE_"
		fi
		dhtadm -M -m ${SRVNAME} -e "LeaseNeg=${LEASENEG}" 2> ${DHCPTDIR}/DhcpErr.$$
		if [ ${?} -ne ${SUCCESS} ]
		then
			print - "Warning: Unable to update LeaseNeg value in server macro definition: ${SRVNAME}" >&2
		fi
		dhtadm -M -m ${SRVNAME} -e "LeaseTim=${LEASE}" 2> ${DHCPTDIR}/DhcpErr.$$
		if [ ${?} -ne ${SUCCESS} ]
		then
			print - "Warning: Unable to update LeaseTim value in server macro definition: ${SRVNAME}" >&2
		fi
		if [ ! -z "${DNSDMAIN}" ]
		then
			dhtadm -M -m ${SRVNAME} -e "${DNSDMAIN}" 2> ${DHCPTDIR}/DhcpErr.$$
			if [ ${?} -ne ${SUCCESS} ]
			then
				print - "Warning: Unable to update DNSdmain value in server macro definition: ${SRVNAME}" >&2
			fi
		fi
		if [ ! -z "${DNSSERV}" ]
		then
			dhtadm -M -m ${SRVNAME} -e "${DNSSERV}" 2> ${DHCPTDIR}/DhcpErr.$$
			if [ ${?} -ne ${SUCCESS} ]
			then
				print - "Warning: Unable to update DNSserv value in server macro definition: ${SRVNAME}, error:\n`cat ${DHCPTDIR}/DhcpErr.$$`" >&2
			fi
		fi
	;;
	*)
		print - "Error: Cannot add server macro ${SRVNAME} to dhcptab: `cat ${DHCPTDIR}/DhcpErr.$$`" >&2
		rm -f ${DHCPTDIR}/DhcpErr.$$
		return 1
	;;
	esac

	rm -f ${DHCPTDIR}/DhcpErr.$$
	return 0
}

# Build a dhcp network table but only if interface is not Point to Point.
# Arguments:	1) Type (local | remote)
#		2) Network address (dotted).
#		3) Network type (L == LAN, P == PPP)
cfg_network()
{
	if [ ${#} -ne 3 ]
	then
		return 1
	fi
	MACRO=""

	if [ "${3}" = "L" ]
	then
		yes_or_no "Do you want hostnames generated and inserted in the ${RESOURCE} hosts table?" N
		if [ ${?} -eq ${SUCCESS} ]
		then
			while :
			do
				print - "What rootname do you want to use for generated names? [${DEFNMROOT}]: \c"
				read ROOTNAME
				: ${ROOTNAME:=${DEFNMROOT}}
				yes_or_no "Is Rootname ${ROOTNAME} correct?" Y
				if [ ${?} -eq ${SUCCESS} ]
				then
					break
				fi
			done
			while :
			do
				print - "What base number do you want to start with? [${DEFCNT}]: \c"
				read ROOTCNT
				: ${ROOTCNT:=${DEFCNT}}
				TMP=`print - ${ROOTCNT} | cut -c1`
				if [ ${TMP} -gt 0 -a ${TMP} -le 9 ]
				then
					break
				else
					print - "Error, enter digits." >&2
				fi
			done
		else
			ROOTNAME=""
			typeset -i ROOTCNT=${DEFCNT}
		fi

		case "${1}" in
		remote)
			MACRO=""
			SUBNET=""
			# Check if mask in netmasks table. First try
			# for network address as given, in case VLSM
			# is in use.
			get_default_class ${2} | read DEFNET DEFMASK
			SUBNET=`get_netmask ${2}`
			if [ -z "${SUBNET}" ]
			then
				# use the default.
				if [ "${DEFNET}" != "${2}" ]
				then
					# likely subnetted/supernetted.
					print - "\n\n###\tWarning\t###\n"
					print - "Network ${2} is netmasked, but no entry was found in the 'netmasks'\ntable; please update the 'netmasks' table in the appropriate\nnameservice before continuing (see /etc/nsswitch.conf).\n" >&2
					return 1
				else
					# use the default.
					SUBNET="${DEFMASK}"
				fi
			fi
			MACRO="${MACRO}:Subnet=${SUBNET}"

			# Remote Network. No way to tell what the router is
			# from the client's perspective. Must ask the User.
			# prompt the user for BOOTP/DHCP servers
			ROUTERS=""
			print - "Enter Router (From client's perspective), or <RETURN> if finished."
			info=""
			while :
			do
				print - "IP address: \c"
				read info
				if [ -z "${info}" ]
				then
					break;
				fi

				# validate ip.
				ipcheck ${info}
				if [ ${?} -ne 0 ]
				then
					continue
				fi
				TMPNET=`get_net_num ${info} ${SUBNET}`
				if [ "${TMPNET}" != "${2}" ]
				then
					print - "\nRouter ${info} isn't on client's network: ${2}." >&2
					continue
				fi
				if [ ! -z "${ROUTERS}" ]
				then
					ROUTERS="${ROUTERS} ${info}"
				else
					ROUTERS="${info}"
				fi
			done

			if [ ! -z "${ROUTERS}" ]
			then
				MACRO="${MACRO}:Router=${ROUTERS}"
			fi

			MTU=""
			while :
			do
				print - "Optional: Enter Remote Network's MTU (e.g. ethernet == 1500): \c"
				read MTU
				if [ ! -z "${MTU}" ]
				then
					TMP=`print - ${MTU} | cut -c1`
					if [ ${TMP} -gt 0 -a ${TMP} -le 9 ]
					then
						MACRO="${MACRO}:MTU=${MTU}"
						break
					else
						print - "Error, enter digits." >&2
					fi
				else
					break
				fi
			done
		;;
		local)
			# Local Network.
			MACRO=`grep "^${2}" ${DHCPTDIR}/DhcpLclNet.$$`
			if [ ${?} -eq ${SUCCESS} ]
			then
				SUBNET="`print - ${MACRO} | cut -d' ' -f3 | sed -e 's/Subnet=//'`"
				MACRO="`print - ${MACRO} | cut -d' ' -f4-`"
				ROUTERS=`get_default_routers`
				if [ ! -z "${ROUTERS}" ]
				then
					MACRO="${MACRO}Router=${ROUTERS}"
				else
					let tnum=${#MACRO}-1
					MACRO="`print - ${MACRO} | cut -c1-${tnum}`"
				fi
			else
				print - "Unexpected error (LclNet missing)." >&2
				return 1
			fi
		;;
		*)
			print - "Error, bad network type." >&2
			return 1
		;;
		esac

		BCAST=`get_bcast_addr ${2} ${SUBNET}`
		typeset -i MAX=`client_index ${2} ${BCAST}`
		while :
		do
			print - "Enter starting IP address [${2}]: \c"
			read STRTIP
			: ${STRTIP:="${2}"}
			ipcheck ${STRTIP}
			if [ ${?} -ne 0 ]
			then
				continue
			fi
			if [ "${STRTIP}" = "${BCAST}" ]
			then
				print - "\nStarting address ${STRTIP} is the network broadcast address.\nYou must specify an IP address such that ${TMPNET} < X < ${BCAST}." >&2
				continue
			fi
			TMPNET=`get_net_num ${STRTIP} ${SUBNET}`
			if [ "${TMPNET}" = "${2}" ]
			then
				break
			fi
			print - "\nStarting address ${STRTIP} isn't on network ${2}!" >&2
		done

		typeset -i STRTNUM=`client_index ${2} ${STRTIP}`
		#
		# We need to normalize the starting index for the case where
		# the user accepts the network address as the starting
		# address, and adjust it to the next legal address.
		#
		if [ ${STRTNUM} -eq 0 ]
		then
			let STRTNUM=1
		fi

		while :
		do
			print - "Enter the number of clients you want to add (x < ${MAX}): \c"
			read NUM
			: ${NUM:="NONE"}
			TMP=`print - ${NUM} | cut -c1`
			if [ ${TMP} -gt 0 -a ${TMP} -le 9 ]
			then
				let ENDNUM=${STRTNUM}+${NUM}
				if [ ${ENDNUM} -gt ${MAX} ]
				then
					let TNUM=${MAX}-1
					print - "Error, maximum clients on ${2} is ${TNUM}, not ${ENDNUM}" >&2
					unset TNUM
				else
					break
				fi
			else
				print - "Error, enter digits." >&2
			fi
		done
		let MAXNUM=${ENDNUM}-${STRTNUM}

		# Handle allocation of BOOTP specific addresses.
		let BOOTP_NUM=0
		if [ ${BOOTP_DYNAMIC} -eq 1 ]
		then
			yes_or_no "\nBOOTP compatibility with automatic allocation is enabled.\nDo you want any of your ${MAXNUM} addresses to be BOOTP specific?" Y
			if [ ${?} -eq 0 ]
			then
				while :
				do
					print - "How many (x <= ${MAXNUM}): \c"
					read BOOTP_NUM
					: ${BOOTP_NUM:=0}
					TMP=`print - ${BOOTP_NUM} | cut -c1`
					if [ ${TMP} -gt 0 -a ${TMP} -le 9 ]
					then
						if [ ${BOOTP_NUM} -gt ${MAXNUM} ]
						then
							print - "Error, you have only specified a total of ${MAXNUM} clients on ${2}." >&2
						else
							break
						fi
					else
						print - "Error, enter digits." >&2
					fi
				done
			fi
		fi

		# create the table.
		pntadm -C ${2} 2> ${DHCPTDIR}/DhcpErr.$$
		case ${?} in
		${SUCCESS})
			# cool..
			status=${SUCCESS}
		;;
		${EXISTS})
			yes_or_no "The dhcp network table: ${2} already exists.\nDo you want to add entries to it?" Y
			if [ ${?} -ne ${SUCCESS} ]
			then
				rm -f ${DHCPTDIR}/DhcpErr.$$
				return 0
			fi
			status=${EXISTS}
		;;
		*)
			print - "Error: `cat ${DHCPTDIR}/DhcpErr.$$` creating dhcp network table: ${2}" >&2
			rm -f ${DHCPTDIR}/DhcpErr.$$
			return 1
		;;
		esac

		rm -f ${DHCPTDIR}/DhcpErr.$$
	fi

	# Add broadcast address if network is LAN.
	if [ "${3}" = "L" ] 
	then
		MACRO="${MACRO}:Broadcst=${BCAST}"
	fi

	# get nis/nis+ information.
	get_nis_parms

	if [ ! -z "${NISPLUSDMAIN}" ]
	then
		MACRO="${MACRO}:NIS+dom=${NISPLUSDMAIN}"
	fi

	if [ ! -z "${NISPLUSSERV}" ]
	then
		MACRO="${MACRO}:NIS+serv=${NISPLUSSERV}"
	fi

	if [ ! -z "${NISDMAIN}" ]
	then
		MACRO="${MACRO}:NISdmain=${NISDMAIN}"
	fi

	if [ ! -z "${NISSERV}" ]
	then
		MACRO="${MACRO}:NISservs=${NISSERV}"
	fi

	if [ ! -z "${MACRO}" ]
	then
		MACRO="${MACRO}:"
		dhtadm -A -m ${2} -d "${MACRO}" 2> ${DHCPTDIR}/DhcpErr.$$
		case ${?} in
		${SUCCESS})
			# great.
			:
		;;
		${EXISTS})
			# It already exists. Allow user to merge values.
			yes_or_no "\ndhcptab macro \"${2}\" already exists.\nDo you want to merge initialization data with the existing macro?" Y
			if [ ${?} -eq 0 ]
			then
				SYMTMP=${MACRO#*:}
				while [ ! -z "${SYMTMP}" ]
				do
					SYM=${SYMTMP%%:*}
					LEN=${#SYM}
					TTMP=${SYM##*=}
					if [ -z "${TTMP}" -o \( ${LEN} -ne ${#TTMP} \) ]
					then
						# Either deletion/addition/modification
						:
					else
						# No value associated with symbol.
						SYM="${SYM}=_NULL_VALUE_"
					fi
					dhtadm -M -m ${2} -e "${SYM}" 2> ${DHCPTDIR}/DhcpErr.$$
					if [ ${?} -ne 0 ]
					then
						print - "WARNING: cannot process (${SYM}) in the dhcptab macro ${2}." >&2
					fi
					SYMTMP=${SYMTMP#*:}
				done
			fi
		;;
		*)
			print - "Error: `cat ${DHCPTDIR}/DhcpErr.$$`" >&2
			print - "Unable to add the following macro to the dhcptab in support of network: ${2}." >&2
			print - "Intended value:\n${2} m ${MACRO}" >&2
		;;
		esac

		rm -f ${DHCPTDIR}/DhcpErr.$$
	fi

	if [ "${3}" = "L" ]
	then
		yes_or_no "Disable (ping) verification of ${2} address(es)?" N
		if [ ${?} -eq ${SUCCESS} ]
		then
			PING=""
		else
			PING="ping"
		fi
		status=${EXISTS}

		# Now add the entries to the dhcp network table.
		spinner[0]="/"
		spinner[1]="-"
		spinner[2]="\\"
		spinner[3]="|"
		CMD=""
		typeset -Z2 PERCENT=0
		typeset -i SPIN=0
		typeset -i CNT=0
		while [ ${STRTNUM} -lt ${ENDNUM} ]
		do
			CMD=""
			if [ ! -z "${ROOTNAME}" ]
			then
				CMD="-h ${ROOTNAME}${ROOTCNT}"
			fi
			ADDR=`get_addr ${2} ${SUBNET} ${STRTNUM}`
			# ignore certain entries.
			if [ "${ADDR}" = "${2}" -o "${ADDR}" = "${SUBNET}" ]
			then
				let STRTNUM=${STRTNUM}+1
				continue
			fi
			if [ ! -z "${PING}" ]
			then
				ping ${ADDR} ${DEFPINGTIME} > /dev/null 2>&1
				if [ ${?} -eq 0 ]
				then
					print - "\nWarning: Address ${ADDR} in ${2} in use... skipping..." >&2
					let STRTNUM=${STRTNUM}+1
					continue
				fi
			fi

			if [ ${BOOTP_NUM} -gt 0 ]
			then
				CMD="${CMD} -f BOOTP"
				let BOOTP_NUM=${BOOTP_NUM}-1
			fi

			pntadm -A ${ADDR} ${CMD} -m ${SRVNAME} ${2} 2> ${DHCPTDIR}/DhcpErr.$$
			case ${?} in
			${SUCCESS})
				# great.
				let PERCENT=${CNT}\*100/${MAXNUM}
				let SPIN=${SPIN}+1
				if [ ${SPIN} -gt 3 ]
				then
					let SPIN=0
				fi
				print "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b${spinner[${SPIN}]} ${PERCENT}% Complete.\c"
				let CNT=${CNT}+1
			;;
			${EXISTS} | ${WARNING})
				# It already exists. Warn user.
				print - "Warning: ${ADDR} entry in ${2} already exists." >&2
			;;
			*)
				print - "Unexpected Error: `cat ${DHCPTDIR}/DhcpErr.$$` occurred.\nUnable to add ${ADDR} to the ${2} network." >&2
				break
			;;
			esac
			let STRTNUM=${STRTNUM}+1
			let ROOTCNT=${ROOTCNT}+1
		done
		rm -f ${DHCPTDIR}/DhcpErr.$$

		print - "\nConfigured ${CNT} entries for network: ${2}."
	fi

	return 0
}

# initialize various dhcp network tables.
build_dhcp_network()
{
	print - "\n###\tSelect Networks For BOOTP/DHCP Support\t###\n"
	yes_or_no "Enable DHCP/BOOTP support of networks you select?" Y
	if [ ${?} -ne ${SUCCESS} ]
	then
		return 0
	fi

	# first handle locally attached networks.
	print - "\n###\tConfigure Local Networks \t###\n"
	> ${DHCPTDIR}/DhcpLclNet.$$
	for ITR in `get_interfaces`
	do
		get_interface_info ${ITR} >> ${DHCPTDIR}/DhcpLclNet.$$
	done
	NETWORKS=""
	for TMP in `awk '{ print $1 }' ${DHCPTDIR}/DhcpLclNet.$$`
	do
		NETWORKS="${NETWORKS} ${TMP}"
	done

	for NET in ${NETWORKS}
	do
		IFTYPE=`grep "^${NET}" ${DHCPTDIR}/DhcpLclNet.$$ | head -1 | awk ' { print $2 }'`
		if [ "${IFTYPE}" = "L" ]
		then
			TMP="LAN"
		else
			TMP="Point To Point"
		fi
		yes_or_no "Configure BOOTP/DHCP on local ${TMP} network: ${NET}?" Y
		if [ ${?} -eq ${SUCCESS} ]
		then
			if cfg_network local ${NET} ${IFTYPE}
			then
				:
			else
				yes_or_no "\nWould you like to continue?" N
				if [ ${?} -ne ${SUCCESS} ]
				then
					break
				fi
			fi
		fi
	done

	# then handle any remote networks.
	print - "\n###\tConfigure Remote Networks \t###\n"
	yes_or_no "Would you like to configure BOOTP/DHCP service on remote networks?" Y
	if [ ${?} -eq ${SUCCESS} ]
	then
		NET=""
		while :
		do
			NET=""
			print - "Enter Network Address of remote network, or <RETURN> if finished: \c"
			read NET
			if [  -z "${NET}" ]
			then
				break
			fi
			ipcheck ${NET}
			if [ ${?} -ne 0 ]
			then
				continue
			fi
			IFTYPE=""
			print - "Do clients access this remote network via LAN or PPP connection? ([L]/P): \c"
			read IFTYPE
			: ${IFTYPE:="L"}
			if cfg_network remote ${NET} ${IFTYPE}
			then
				print - "\nNetwork: ${NET} complete."
			else
				yes_or_no "\nWould you like to continue?" N
				if [ ${?} -ne ${SUCCESS} ]
				then
					break
				fi
			fi
		done
	fi

	rm -f ${DHCPTDIR}/DhcpLclNet.$$

	return 0
}

# Configure DHCP
cfg_dhcp()
{
	print - "\n###\tDHCP Service Configuration\t###"

	if [ ! -f ${DHCPCONFIG} ]
	then
		# handle pcadmin compatibility.
		handle_pcadmin
		if [ ${?} -ne ${SUCCESS} ]
		then
			return ${?}
		fi
	fi

	# setup DHCP service configuration file.
	create_dhcp_cfg
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# common daemon startup options.
	common_options
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# server specific startup options.
	server_options
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# build dhcptab.
	build_dhcptab
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# build dhcp network tables.
	build_dhcp_network
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# Make startup links if needed.
	makelinks

	return ${?}
}

# Configure relay
cfg_relay()
{
	print - "\n###\tBOOTP Relay Agent Configuration\t###\n"

	# common daemon startup options.
	common_options
	if [ ${?} -ne ${SUCCESS} ]
	then
		return ${?}
	fi

	# relay specific options
	relay_options

	# Make startup links if needed.
	makelinks

	return ${?}
}

# Stop current dhcp daemon.
stop_daemon()
{
	if [ ! -f /etc/init.d/dhcp ]
	then
		print - "Error: DHCP doesn't appear to be installed on this machine." >&2
		exit 1
	else
		TMP="yes"
		yes_or_no "Would you like to stop the DHCP service? (recommended)" Y
		if [ ${?} -eq 0 ]
		then
			/etc/init.d/dhcp stop
			let STOP=1
		fi
	fi
}

# Restart dhcp daemon
start_daemon()
{
	if [ ${STOP} -eq 1 ]
	then
		yes_or_no "Would you like to restart the DHCP service? (recommended)" Y
		if [ ${?} -eq 0 ]
		then
			/etc/init.d/dhcp start
		fi
	fi
}


###############################################################################
# Main starts here.
###############################################################################

if [ `id | cut -c5` -ne 0 ]
then
	print - "You must have super user (root) privileges." >&2
	exit 1
fi

# First, determine if user would like to configure dhcp as a server or
# as a relay agent.

let STOP=0
while :
do
	clear
	print - "***\t\tDHCP Configuration\t\t***\n\n"
	print - "Would you like to:\n"
	print - "\t1) Configure DHCP Service\n"
	print - "\t2) Configure BOOTP Relay Agent\n"
	print - "\t3) Unconfigure DHCP or Relay Service\n"
	print - "\t4) Exit\n"
	print - "\n\nChoice: \c"
	read RESP
	case ${RESP} in
		1)
			stop_daemon
			load_dhcp_cfg
			cfg_dhcp
			commit_dhcp_cfg
			start_daemon
		;;
		2)
			stop_daemon
			load_dhcp_cfg
			cfg_relay
			commit_dhcp_cfg
			start_daemon
		;;
		3)
			# unconfigure loads the cfg file.
			unconfigure
		;;
		4)
			exit 0
		;;
		*)
			# print valid values, pause to allow user to read them
			print - "You must select from 1, 2, 3, or 4."
			sleep 1
		;;
	esac
done

exit ${?}
