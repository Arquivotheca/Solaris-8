#!/bin/sh
#
# ident	"@(#)nisserver.sh	1.25	98/08/25 SMI"
#
# nisserver -- script to setup NIS+ servers
#
# Copyright (c) 1992-1998 Sun Microsystems, Inc.
# All Rights Reserved.
#


#
# print_usage(): ask user if they want to see detailed usage msg.
#
print_usage()
{
   echo
   get_yesno "  Do you want to see more information on this command? \\n\
  (type 'y' to get a detailed description, 'n' to exit)"
   if [ $ANS = "n" -o $ANS = "N" ]
   then
    echo
    return 1
   else
    print_more
   fi
   exit 1
}
  


#
# print_more(): print the usage message.
#
print_more()
{
	more << EOF
USAGE:
  o to set up root master server:
	$PROG  -r  [-f]  [-v]  [-x]  [-Y]  [-d <NIS+_domain>] 
		[-g <NIS+_groupname>]  [-l <NIS+_passwd>]

  o to set up non-root master server:
	$PROG  -M  [-f]  [-v]  [-x]  [-Y]  [-d <NIS+_domain>] 
		[-g <NIS+_groupname>]  [-h <NIS+_server_host>] 

  o to set up replica server:
	$PROG  -R  [-f]  [-v]  [-x]  [-Y]  [-d <NIS+_domain>] 
		[-h <NIS+_server_host>]
	
OPTIONS:
     -d <NIS+_domain>
	 specifies the name for the NIS+ domain.  The default is your 
	 local domain.

     -f  forces the NIS+ server setup without prompting for confirmation.

     -g <NIS+_groupname>
	 specifies the NIS+ group name for the new domain.  This option is
	 not valid with "-R" option.  The default group is admin.<domain>.

     -h <NIS+_server_host>
	 specifies the hostname for the NIS+ server.  It must be a
	 valid host in the local domain.  Use a fully qualified
	 hostname (for example, hostx.xyz.sun.com.) to specify a host
	 outside of your local domain.  The default for non-root master
	 server setup is to use the same list of servers as the parent
	 domain.  The default for replica server setup is the local
	 hostname.  This option is *ONLY* used for non-root master or
	 replica servers setup.

     -l <network_password>
	 specifies the network password with which to create the
	 credentials for the root master server.  This option is *ONLY*
	 used for master root server setup (-r).  If this option is not
	 specified, this script will prompt you for the login password.

     -M  sets up the specified host as a non-root master server.
     Make sure that rpc.nisd(1M) is running on the new master server
	 before this command is executed.
	
     -R  sets up the specified host as a replica server.  Make sure that
	 rpc.nisd(1M) is running on the new replica server.

     -r  sets up the server as a root master server.  Use the -R option
 	 to set up a root replica server.

     -v  runs this script in verbose mode.

     -x  turns the "echo" mode on.  This script just prints the commands
	 that it would have executed.  The commands are printed with
	 leading "+++".  Note that the commands are not actually executed.
	 The default is off.

     -Y  sets up an NIS+ server with NIS-compatibility mode.  The default
	 is to set up the server without NIS-compatibility mode.
EOF
}



#
#	print MR usage
#
print_MRr_usage()
{
   if [ "$ERRMRr_OPTION" = "$MRr_OPTION" ]
   then
  		echo "**WARNING: You have specified the '$MRr_OPTION' option twice."
		return 0
   fi
   echo
   echo "**ERROR: You have specified the '$ERRMRr_OPTION' option after"
   echo "         having selected the '$MRr_OPTION' option."
   echo "Please select only one of these options: '-M', '-R', or '-r'."
   print_usage
   exit 1
}



#
# Generic Routines:
# -----------------
# 
# This section contains general routines.
#	get_ans()	- prompts the message and waits for an input
#	get_yesno()	- prompts the message and waits for a y or n answer
#	get_pids()	- finds the PID for a specific process name
#	restart_process()
#			- kills and starts a specified process
#	kill_process()	- kills a specified process
#	check_dot()	- check if the argument ends with a dot
#	check_host()	- check if the hostname specified is a valid one
#	tolower(): converts upper to lower case.
#

#
# get_ans(): gets an asnwer from the user.
#		$1  instruction/comment/description/question
#		$2  default value
#
get_ans()
{
	if [ -z "$2" ]
	then
		if [ $OS -eq 5 ]
		then 
			echo "$1 \c"
		else
			echo -n "$1 "
		fi
	else
		if [ $OS -eq 5 ]
		then
			echo "$1 [$2] \c"
		else
			echo -n "$1 [$2] "
		fi
	fi
	read ANS
	if [ -z "$ANS" ]
	then
		ANS=$2
	fi
}



##########  get_yesno constants:
##
##  There are two very common phrases passed to get_yesno:
##   These have been parameterized to provide "ease of use".
##	 Thus, there are three message "types" which are possible:
##	 --$CONTINUE:  "Do you want to continue? (type 'y' to continue, 'n' to exit this script)"
##   --$CONFIRM:   "Is this information correct? (type 'y' to accept, 'n' to change)"
##   --actual string is passed.
##
##	If the message is $CONTINUE, get_yesno will exit if the response is no.
##
###########
CONTINUE=2
CONFIRM=1
#
# get_yesno(): get the yes or no answer.
#		$1  message type  or message.
#
#
#
get_yesno()
{
	ANS="X"

	case $1 in
	$CONTINUE )
		INFOTEXT="Do you want to continue? (type 'y' to continue, 'n' to exit this script)"
		;;
	$CONFIRM )
		INFOTEXT="Is this information correct? (type 'y' to accept, 'n' to change)"
		;;
    *) INFOTEXT="$1"
		;;
	esac

	while [ "$ANS" != "y" -a "$ANS" != "n" -a "$ANS" != "Y" -a "$ANS" != "N" ]
	do
		get_ans "$INFOTEXT" ""
	done

	if [ "$1" = "$CONTINUE" ]; then
		if [ $ANS = "n" -o $ANS = "N" ]
		then
			exit
		fi
	fi

}



#
# get_pids(): returns the process id numbers for the specified process name.
#		$1  the process name
#
get_pids()
{
	PNAME=`expr "$1" : '\(.\{1,8\}\)'`
	if [ $OS -eq 5 ]
	then
		ps -e | awk '\
		{	if ($4 == cmd)
				printf("%d ", $1);
		}
		' cmd=$PNAME
	else
		ps -ax | awk '$5 ~ /'$1'/	{ printf("%d ", $1); }'
	fi
}



#
# restart_process(): kill and restart the specified process name.
# 		$1  the process name to be killed
#
restart_process()
{
	kill_process $1
	sleep 1
	eval "echo restarting process $1 ... $VERB"
	$ECHO $*
}



#
# kill_process(): kills the the specified process.
#		$1  the process name to be killed
#
kill_process()
{
	eval "echo killing process $1 ... $VERB"
	PROC=`get_pids $1`
	if [ ! -z "$PROC" ]
	then
		$ECHO kill $PROC
	fi
}



#
# check_dot(): checks if the argument specified ends with a dot.
#		$1  argument to be checked
#
check_dot()
{
	if [ "`echo $1 | sed -e 's/.*\(.\)$/\1/'`" != "." ]
	then
		return 1
	fi
	return 0
}



#
# check_host(): checks if the host specified is in the credential table of
# the its domain.  If it's a valid host, then it'll assign the host
# principal name to HOSTPRINC.
#		$1  host name (this can be a fully qualified name)
#
check_host()
{
	if [ -z "$1" ]
	then
		return 1
	fi

	if check_dot $1;
	then
		HOSTPRINC=$1
		MESS="principal"
		LDOM=`echo $1 | sed -e 's/[^\.]*\.\(.*\)$/\1/'`
	else
		LDOM=`nisdefaults -d`
		HOSTPRINC=$1.$LDOM
		MESS="host"
	fi

	nismatch $HOSTPRINC cred.org_dir.$LDOM > /dev/null
	if [ $? -eq 0 ]
	then
		return 1
	fi
	echo "**ERROR: the principal name for host $1 is not defined in domain"
	echo "\"$LDOM\".  You must either add the credential for host $1"
	echo "in domain \"$LDOM\" or specify a fully qualified hostname (with"
	echo "the ending dot \".\") if the principal name is defined in a"
	echo "different domain.  Use nisclient -c to create the host credential."
	return 0
}



#
# tolower_single(): converts upper to lower case and single token.
# Single token means the first token if the argument contains "."
# dots as in the fully qualified hostname.
#		$1  string to convert
#
tolower_single()
{
	echo "$1" | tr '[A-Z]' '[a-z]' | cut -d. -f1
}



#
# tolower(): converts upper to lower case.
#		$1  string to convert
#
tolower()
{
	echo "$1" | tr '[A-Z]' '[a-z]'
}



#
# Common Routines:
# ---------------
# 
# This section contains common routines for master and replica setups for
# root and non-root domains.
#	init()		- initializes all the variables
#	parse_arg()	- parses the command line arguments
#	get_security()	- gets the security information
#	update_info()	- updates the setup information
#	print_info()	- prints the setup information
#	confirm()	- confirms the setup information
#	setup_domain()	- sets up the domain
#	setup_switch()	- sets up the switch
#	nis_server()	- get the server's name for a domain
#	is_server()	- checks if the specified host is already a server
#			  for current domain $DOM.
#	check_perm()	- checks for the write permission for an object
#	nis_chown()	- changes the owner for a domain


#
# init(): initializes variables and options
#
init()
{

	PROG=`basename $0`
	VERB='> /dev/null'	# NULL or "> /dev/null"
	ECHO="eval"		# eval or echo
	BACKUP=no_nisplus	# backup suffix
	DOM=`nisdefaults -d`	# domainname with DOT
	NODOT=`echo $DOM | sed -e "s/\.$//"`
				# domainname without DOT
	SEC=2			# 2=DES or 3=RSA
	ACTION=""		# master or replica
	ROOT="nonroot"		# nonroot or root
	FORCE=""		# NULL or TRUE
	GROUP=""		# NULL or <group-name>
	HOST=""			# NULL or <hostname>
	YP=""			# NULL or -Y
	DEFSEC=2		# default security
## The following variable allows for variation in a specific user message:
	WITHOUT="without"	# without YP compatibility

	OS=`uname -r | cut -d. -f1`
	OSVER=`uname -r | cut -d. -f2`
	if [ $OS -eq 5 ]
	then
		LOCALHOST=`uname -n`
		PATH=/usr/lib/nis:/usr/sbin:/usr/bin:/usr/lib/netsvc/yp:$PATH; export PATH
	else
		LOCALHOST=`hostname`
		PATH=/usr/etc/nis:/usr/etc:/usr/bin:$PATH; export PATH
	fi
 	LOCALHOST=`tolower_single $LOCALHOST`
}



#
# parse_arg(): parses the input arguments.
# It returns the number to be shift in the argument list.
#
parse_arg()
{
	while getopts "d:fg:h:l:MRrvxY" ARG
	do
		case $ARG in
		d)      if [ "`echo $OPTARG | sed -e 's/.*\(.\)$/\1/'`" != "." ]
			then
				NODOT=$OPTARG
				DOM=${NODOT}.
			else
				DOM=$OPTARG
				NODOT=`echo $DOM | sed -e "s/\.$//"`
			fi ;;
		f)	FORCE="TRUE";;
		g)	if [ "`echo $OPTARG | sed -e 's/.*\(.\)$/\1/'`" != "." ]
			then
				echo "**ERROR: you must specify a fully qualified groupname."
				exit 1
			fi
			GROUP=$OPTARG;;
		h)	HOST=$OPTARG;;
		l)	PASSWD="-l $OPTARG";;

		M)	if [ -z "$ACTION" ]
			then
				ACTION="master"
				MRr_OPTION="-M"
			else
				ERRMRr_OPTION="-M"
				print_MRr_usage
			fi;;
		R)	if [ -z "$ACTION" ]
			then
				ACTION="replica"
				MRr_OPTION="-R"
			else
				ERRMRr_OPTION="-R"
				print_MRr_usage
			fi;;
		r)	ROOT="root"
			if [ -z "$ACTION" ]
			then
				ACTION="master"
				MRr_OPTION="-r"
			else
				ERRMRr_OPTION="-r"
				print_MRr_usage
			fi;;
		v)	VERB="";;
		x)	ECHO="echo +++";;
		Y)	YP="-Y"
			WITHOUT="with";;
		\?)	print_usage
			exit 1;;
		*)	echo "**ERROR: Should never get to this point!!!!!"
			print_usage
			exit 1;;
		esac
	done
	return `expr $OPTIND - 1`
}



#
# check_rootgrp(): check if the group name specified is an valid group for
# root master server setup.
#		$1  the groupname to be checked
#
check_rootgrp()
{
	if [ ! -z "$1" ]
	then
		if [ $ROOT = "root" ]
		then
			GDOM=`expr "$1" : '[^\.]*\.\(.*\)'`
			if [ "`tolower $GDOM`" != "`tolower $DOM`" ]
			then
				return 1
			fi
		fi
	fi
}



#
# check_domainname(): check validity of a domain name.  Currently we check
#	that it has at least two components.
#		$1  the domain name to be checked
#
check_domainname()
{
	if [ ! -z "$1" ]
	then
		t=`expr "$1" : '[^.]\{1,\}[.][^.]\{1,\}'`
		if [ "$t" = 0 ]
		then
			echo '**ERROR: invalid domain name ('$1')'
			echo '  It must have at least two components.'
			echo '  For example, "company.com.".'
			print_usage
			exit 1
		fi
	fi
}



#
# get_security(): gets the security information
#
get_security()
{
	while [ /bin/true ]
	do
		get_ans "Security level (2=DES, 3=RSA):" $SEC
		VALUE=`expr "$ANS" : "\([23]\)"`
		if [ -z "$VALUE" -o "$VALUE" -lt 2 -o "$VALUE" -gt 3 ]
		then
			echo "**ERROR: invalid security level."
			echo "	It must be either 2 or 3."
		else
			SEC=$VALUE
			break
		fi
	done
	SEC=$ANS
}



#
# update_info(): updates the information.
#
update_info()
{
	echo ""
	# ...domainname
	get_ans "Domain name:" $DOM
	if [ "`echo $ANS | sed -e 's/.*\(.\)$/\1/'`" != "." ]
	then
		NODOT=$ANS
		DOM=${NODOT}.
	else
		DOM=$ANS
		NODOT=`echo $DOM | sed -e "s/\.$//"`
	fi

	# ...host name
	if [ $ACTION = "replica" ]
	then
		while [ /bin/true ]
		do
			get_ans "NIS+ Hostname:" $HOST
			if [ "$HOST" = "$ANS" ]
			then
				break
			fi
			if check_host $ANS;
			then
				HOST=$ANS
				break
			fi
		done
	elif [ $ROOT = "nonroot" ]
	then
		DEFAULT=${HOST:-"(use ${PAR_DOM} servers)"}
		while [ /bin/true ]
		do
			get_ans "NIS+ Hostname:" "$DEFAULT"
			if [ "$ANS" = "$DEFAULT" ]
			then
				break
			fi
			if check_host $ANS;
			then
				HOST=$ANS
				break
			fi
		done
	fi

	# ...group name
	if [ $ACTION = "master" ]
	then
		DEFAULT=${GROUP:-admin.$DOM}
		while [ /bin/true ]
		do
			get_ans "NIS+ group:" $DEFAULT
			if [ "$ANS" != "$DEFAULT" ]
			then
				if check_dot $ANS;
				then
					if check_rootgrp $ANS;
					then
						GROUP=$ANS
						break
					fi
					echo "**ERROR: invalid group name."
					echo "	It must be a group in the $DOM domain."
					continue
				fi
				echo "**ERROR: NIS+ group name must end with a \".\"."
			else
				break
			fi
		done
	fi

	# ...YP compatibility
	if [ $ACTION = "master" ]
	then
		DEFAULT=1
		if [ -z "$YP" ]
		then
			DEFAULT=0
		fi
		ANS=2
		while [ $ANS -ne 0 -a $ANS -ne 1 ]
		do
			get_ans "NIS (YP) compatibility (0=off, 1=on):" $DEFAULT
		done
		if [ $ANS -eq 1 ]
		then
			YP="-Y"
			echo ""
            echo "To activate NIS-compatibility mode, uncomment the line"
			echo "'EMULYP=\"-Y\"' in the /etc/init.d/rpc file and restart"
			echo "rpc.nisd with the '-Y' option on $HOST after this script"
			echo "completes."
			echo ""
		else
			YP=""
		fi
	fi

	# ...security level
#	if [ $ACTION = "master" ]
#	then
#		get_security
#	fi
}



#
# print_info(): prints the information on the screen.
#
print_info()
{
	# ...domainname
	echo "Domain name		: $DOM"

	# ...hostname
	if [ $ACTION = "replica" ]
	then
		USE_HOST=TRUE
		echo "NIS+ server		: $HOST"
	elif [ $ROOT = "nonroot" ]
	then
		USE_HOST=TRUE
		echo "NIS+ server		: "${HOST:-"(use $PAR_DOM servers)"}
	fi

	# ...admin group
	if [ $ACTION = "master" ]
	then
		MESS="admin.$DOM"
		echo "NIS+ group		: ${GROUP:-$MESS}"
	fi

	# ...YP compatibility
	if [ $ACTION = "master" ]
	then
		MESS=ON
		if [ -z "$YP" ]
		then
			MESS="OFF"
		fi
		echo "NIS (YP) compatibility	: $MESS"
	fi

	# ...security level
	if [ $ACTION = "master" ]
	then
		case $SEC in
		0)	MESS="0=NO_SEC";;
		1)	MESS="1=SYS";;
		2)	MESS="2=DES";;
		3)	MESS="3=RSA";;
		*)	MESS="INVALID";;
		esac
		echo "Security level		: $MESS"
	fi
}



#
# confirm(): asks for user confirmation.  If declined, then it will step
# the user through a question answer session.
#
confirm()
{
	while [ /bin/true ]
	do
		echo ""
		print_info
		echo ""

		get_yesno $CONFIRM
		if [ $ANS = "y" -o $ANS = "Y" ]
		then
			return
		fi

		update_info
	done
}



#
# setup_domain(): sets up "domainname" and "/etc/defaultdomain" with the specified
# domain information.
#
setup_domain()
{
	echo "setting up domain information \"$DOM\" ..."
	if [ `nisdefaults -d` != $DOM ]
	then
		# NODOT is used to support 4.x YP clients
		$ECHO domainname $NODOT
		if [ ! -f /etc/defaultdomain.$BACKUP ]
		then
			$ECHO mv /etc/defaultdomain /etc/defaultdomain.$BACKUP
		fi
		$ECHO "domainname > /etc/defaultdomain"
	fi
	echo ""
}



#
# setup_switch(): copies the nisplus switch configuration file to nsswitch.conf.
#
setup_switch()
{
	if [ $OS -eq 5 ]
	then
		echo "setting up switch information ..."
		diff /etc/nsswitch.conf /etc/nsswitch.nisplus > /dev/null
		if [ $? -eq 0 ]
		then
			eval "echo switch configuration file already set to use NIS+. $VERB"
			restart_process keyserv
			echo ""
			return
		fi

		if [ ! -f /etc/nsswitch.conf.$BACKUP ]
		then
			$ECHO mv /etc/nsswitch.conf /etc/nsswitch.conf.$BACKUP
		fi
		$ECHO cp /etc/nsswitch.nisplus /etc/nsswitch.conf
		echo ""
	fi

	$ECHO "rm -f /etc/.rootkey > /dev/null"
#	$ECHO "rm -f /etc/security/.rsarootkey > /dev/null"
	restart_process keyserv
}



#
# nis_server(): returns the master server for specified domain.
#		$1  domain
#
nis_server()
{
	niscat -M -o $1 > /tmp/$PROG.$$
	if [ $? -ne 0 ]
	then
		rm -f /tmp/$PROG.$$ > /dev/null
		exit 1
	fi
	ALLSERVERS=""
	exec < /tmp/$PROG.$$
	while read LINE
	do
		EA=`echo $LINE | sed -n -e "s/^Name : \([^\'].*\)/\1/p"`
		if [ ! -z "$EA" ]
		then
			ALLSERVERS="${ALLSERVERS} $EA"
		fi
	done
	exec < /dev/tty
	GROUPSERVER=`sed -n -e "s/Group[	 ]*: //p" /tmp/$PROG.$$`
	MASTER_SERVER=`echo $ALLSERVERS | cut -d' ' -f1`
	rm -f /tmp/$PROG.$$ > /dev/null
#	SER_NAME=`echo $MASTER_SERVER | cut -d. -f1`
	eval "echo nis_servers: ALLSERVERS=$ALLSERVERS ... $VERB"
#	echo $SER_NAME
}



#
# is_server(): checks if the specified host is already a server for 
# current domain $DOM.
#		$1  server principal
#
is_server()
{
	V1=`tolower $1`
	for EA in $ALLSERVERS
	do
		if [ "$V1" = "`tolower $EA`" ]
		then
			return 0
		fi
	done
	return 1
}



#
# check_perm(): checks if we have write permission to the NIS+ object
# This should be replaced with nisaccess command when it's available
#		$1  the table to be checked.
#
check_perm()
{
	if [ "$ECHO" = "echo" ]
	then
		return
	fi

	eval "echo checking $1 permission ... $VERB"
	MYPRINC=`nisdefaults -p`
	if [ $MYPRINC = "nobody" ]
	then
		if nistest -a n=c $1;
		then
			return
		else
			return 1
		fi
	fi

	DUMMY=`nisls -ld $1`
	if [ $? -ne 0 ]
	then
		exit 1
	fi
	OWN=`echo $DUMMY | cut -d" " -f3`
	if [ "$OWN" = $MYPRINC ]
	then
		if nistest -a o=c $1;
		then
			return
		else
			return 1
		fi
	fi

	DUMMY=`nisls -ldg $1`
	if [ $? -ne 0 ]
	then
		exit 1
	fi
	OWN=`echo $DUMMY | cut -d" " -f3`
	if [ ! -z "$OWN" ]
	then
		if nisgrpadm -t -s "$OWN" $MYPRINC;
		then
			if nistest -a g=c $1;
			then
				return
			else
				return 1
			fi
		fi
	fi

	if nistest -a w=c $1;
	then
		return
	else
		return 1
	fi
}



#
# nis_chown(): changes the owner for the entire domain specified.
#		$1  the new owner's principal
#		$2  the domain
#
nis_chown()
{
	if [ "$ECHO" = "echo" ]
	then
		return
	fi

	nisls org_dir.$2 > /tmp/${PROG}_nisls.$$
	sed -e "1d" -e "s/^\(.*\)/nischown $1 \1.org_dir.$2/" \
		/tmp/${PROG}_nisls.$$ > /tmp/${PROG}_chown.$$
	chmod +x /tmp/${PROG}_chown.$$
	/tmp/${PROG}_chown.$$
	rm -f /tmp/${PROG}_chown.$$ /tmp/${PROG}_nisls.$$ > /dev/null
	$ECHO nischown $1 org_dir.$2 groups_dir.$2 $2
}



#
# setup_rpc(): modifies the /etc/init.d/rpc files to run rpc.nisd with
# the proper options.
#		$*  list of servers
#
setup_rpc()
{
	eval "echo modifying the /etc/init.d/rpc file ... $VERB"
	if [ "`tolower $HOST`" != "$LOCALHOST" ]
	then
		if [ "$SEC" -ne $DEFSEC -o ! -z "$YP" ]
		then
			echo ""
			echo "You should modify the /etc/init.d/rpc file on the following"
			echo "servers to set the proper options for rpc.nisd:"
			echo "servers: $*"
			echo "Options:"
			if [ ! -z "$YP" ]
			then
				echo "	-Y        to turn on YP compatibility mode"
				OPT=" -Y"
			fi
			if [ "$SEC" -ne $DEFSEC ]
			then
				echo "	-S $SEC   to run at security level $SEC"
				OPT="$OPT -S $SEC"
			fi
		fi
		echo ""

	elif [ ! -z "$YP" -o "$SEC" -ne $DEFSEC ]
	then
		if [ -z "$YP" ]
		then
			eval "echo commenting -Y ... $VERB"
			sed -e "s/^\([	 ]*EMULYP=\"-Y\"\)/#\1/" \
				/etc/init.d/rpc > /tmp/${PROG}_1.$$
		else
			eval "echo adding -Y ... $VERB"
			sed -e "s/^#\([	 ]*EMULYP=\"-Y\"\)/\1/" \
				/etc/init.d/rpc > /tmp/${PROG}_1.$$
		fi

		if [ "$SEC" -ne $DEFSEC ]
		then
			eval "echo adding -S $SEC ... $VERB"
			sed \
	 -e "s'^\([	 ]*/usr/sbin/rpc.nisd .*\$EMULYP\).*'\1 -S $SEC'" \
				/tmp/${PROG}_1.$$ > /tmp/${PROG}_2.$$
		else
			eval "echo using default security ... $VERB"
			sed \
	 -e "s'^\([	 ]*/usr/sbin/rpc.nisd .*\$EMULYP\).*'\1'" \
				/tmp/${PROG}_1.$$ > /tmp/${PROG}_2.$$
		fi

		diff /etc/init.d/rpc /tmp/${PROG}_2.$$ > /dev/null
		if [ $? -ne 0 ]
		then
			if [ ! -f /etc/init.d/rpc.$BACKUP ]
			then
				$ECHO mv /etc/init.d/rpc /etc/init.d/rpc.$BACKUP
			fi
			$ECHO mv /tmp/${PROG}_2.$$ /etc/init.d/rpc
			$ECHO chmod 744 /etc/init.d/rpc
		fi
		$ECHO "rm -f /tmp/${PROG}_?.$$ > /dev/null"
	fi
}



#
# root master setup Routine:
# -------------------------
# 
# This section contains the routine to setup a ROOT master server.
# NOTE: you can only configure your local machine as a root master.  You
# cannot configure other machines across the net.
#	root_master()	- sets up the local machine as a root master server
#

#
# root_master(): sets up a the local machine as the root master server.
#
root_master()
{
	WHO=`id | sed -e "s/uid=[0-9]*(\([^ )]*\)).*/\1/"`
	if [ $WHO != "root" ]
	then
		echo "This script must be run as root ..."
		exit 1
	fi

 	if [ ! -z "$HOST" -a "`tolower_single $HOST`" != "$LOCALHOST" ]
	then
		echo "**ERROR: you cannot set up $HOST remotely."
		echo "	To set up $HOST as an NIS+ root master server, run"
		echo "	nisserver on $HOST."
		exit 1
	fi
	HOST=$LOCALHOST

	if [ -z "$SEC" ]     # NOTE: This conditional currently always false
	then
		echo ""
		echo "You must specify the security level:"
		eval "echo get security info ... $VERB"
		get_security
		echo ""
	fi

	echo "This script sets up this machine \"$LOCALHOST\" as an NIS+"
	echo "root master server for domain $DOM."

	if [ -z "$FORCE" ]
	then
		confirm
	else
		echo ""
		print_info
	fi

	echo ""
	echo "This script will set up your machine as a root master server for"
	echo "domain $DOM $WITHOUT NIS compatibility at security level 2."
	echo ""
	if [ -f /var/nis/NIS_COLD_START ]
	then
		echo "WARNING: this script removes directories and files"
		echo "related to NIS+ under /var/nis directory with the"
		echo "exception of the client_info NIS_COLD_START file which"
		echo "will be renamed to <file>.${BACKUP}.  If you want to save"
		echo "these files, you should abort from this script now to"
		echo "save these files first."
		echo ""
		if [ -d /var/nis/data ]
		then
			echo "WARNING: once this script is executed, you will not be able to"
			echo "restore the existing NIS+ server environment.  However, you can"
			echo "restore your NIS+ client environment using \"nisclient -r\""
			echo "with the proper domain name and server information."
		else
			echo "Use \"nisclient -r\" to restore your NIS+ client environment."
		fi
		echo ""
	else
		echo "Use \"nisclient -r\" to restore your current network service environment."
		echo ""
	fi

	if [ -z "$FORCE" ]
	then
		get_yesno $CONTINUE
		echo ""
	fi

	setup_domain

	setup_switch

	eval "echo killing NIS and NIS+ processes ... $VERB"
	kill_process ypbind
	kill_process rpc.nisd
	kill_process rpc.nispasswdd
	kill_process nis_cachemgr
	eval "echo stopping nscd ... $VERB"
	$ECHO /etc/init.d/nscd stop
	eval "echo '' $VERB"

	eval "echo setup NIS_GROUP environment variable ... $VERB"
	GROUP=${GROUP:-admin.$DOM}
	$ECHO "NIS_GROUP=$GROUP; export NIS_GROUP"
	eval "echo '' $VERB"
	
	# Save NIS_COLD_START file
	if [ -f /var/nis/NIS_COLD_START ]
	then
		$ECHO cp /var/nis/NIS_COLD_START /var/nis/NIS_COLD_START.$BACKUP
	fi

	eval "echo rm /var/nis files ... $VERB"
	$ECHO "rm -f /var/nis/NIS_COLD_START > /dev/null"
	$ECHO "rm -f /var/nis/NIS_SHARED_DIRCACHE > /dev/null"
	$ECHO "rm -f /var/nis/.NIS_PRIVATE_DIRCACHE > /dev/null"
	$ECHO "rm -f /var/nis/client_info > /dev/null"
	$ECHO "rm -f /var/nis/.pref_servers > /dev/null"
	$ECHO "rm -f /var/nis/trans.log > /dev/null"
	$ECHO "rm -f /var/nis/data.dict* > /dev/null"
	$ECHO "rm -rf /var/nis/data > /dev/null"
	eval "echo '' $VERB"

	echo "running nisinit ..."
	$ECHO "nisinit -r"
	if [ $? -ne 0 ]
	then
		echo "**ERROR: it failed to initialize the root server."
		exit 1
	fi
	echo ""

	echo "starting root server at security level 0 to create credentials..."
	if [ $OS -ne 5 -o $OSVER -lt 4 ]
	then
		$ECHO "rpc.nisd -r -S 0"
	else
		$ECHO "rpc.nisd -S 0"
	fi
	echo ""
	sleep 2

	echo "running nissetup to create standard directories and tables ..."
	$ECHO "nissetup $YP"
	if [ $? -ne 0 ]
	then
		echo "**ERROR: it failed to create the tables."
		exit 1
	fi
	if [ $OS -ne 5 -o $OSVER -lt 3 ]
	then
		$ECHO "nischmod n+r cred.org_dir.$DOM"
	fi
	echo ""

	echo "adding credential for $HOST.$DOM.."
	case $SEC in
	2)	$ECHO "nisaddcred $PASSWD des > /dev/null";;
	3)	$ECHO "nisaddcred $PASSWD rsa > /dev/null";;
	*)	;;
	esac
	if [ $? -ne 0 ]
	then
		echo "**ERROR: it failed to add the credential for root."
		exit 1
	fi
	sleep 1

	echo ""
	echo "creating NIS+ administration group: ${GROUP} ..."
	$ECHO "nisgrpadm -c $GROUP > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "**WARNING: failed to create the $GROUP group."
		echo "	You will need to create this group manually:"
		echo "	  1. /usr/bin/nisgrpadm -c $GROUP"
		echo "	  2. /usr/bin/nisgrpadm -a $GROUP $HOST.$DOM"
	else
		echo "adding principal ${HOST}.${DOM} to $GROUP ..."
		$ECHO "nisgrpadm -a $GROUP ${HOST}.${DOM} > /dev/null"
		if [ $? -ne 0 ]
		then
			echo "**WARNING: failed to add new member $HOST.$DOM into"
			echo "the $GROUP group."
			echo "	You will need to add this member manually:"
			echo "	  1. /usr/bin/nisgrpadm -a $GROUP $HOST.$DOM"
		fi
	fi
	echo ""

	eval "echo updating the keys for directories ... $VERB"
	$ECHO "nisupdkeys $DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory $DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys $DOM"
		echo ""
	fi
	$ECHO "nisupdkeys org_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory org_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys org_dir.$DOM"
		echo ""
	fi
	$ECHO "nisupdkeys groups_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory groups_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys groups_dir.$DOM"
		echo ""
	fi
	eval "echo $VERB"

	if [ $OS -ne 5 -o $OSVER -lt 3 ]
	then
		eval "echo change group owner for $DOM.. $VERB"
		$ECHO nischgrp $GROUP $DOM
		# ... g=rmcd is just a temporary fix for nisinit bug
		eval "echo add read access for nobody ... $VERB"
		$ECHO nischmod n+r,g=rmcd $DOM
	fi

	echo "restarting NIS+ root master server at security level $SEC ..."
	if [ $OS -ne 5 -o $OSVER -lt 4 ]
	then
		$ECHO restart_process rpc.nisd -r $YP
	else
		$ECHO restart_process rpc.nisd $YP
	fi

	echo "starting NIS+ password daemon ..."
	$ECHO rpc.nispasswdd

	echo "starting NIS+ cache manager ..."
	$ECHO nis_cachemgr -i

	setup_rpc
	if [ -f /etc/init.d/nscd ]
	then
		eval "echo starting Name Service Cache Daemon nscd ... $VERB"
		$ECHO /etc/init.d/nscd start
	fi

	echo ""
	echo "This system is now configured as a root server for domain $DOM"
	echo "You can now populate the standard NIS+ tables by using the"
	echo "nispopulate script or /usr/lib/nis/nisaddent command."
}



#
# Non-root master setup Routine:
# ------------------------------
# 
# This section contains the routine to setup a non-ROOT master server.
# NOTE: If the -h <hostname> is specified, then it will configure the
# specified host as the master of the new domain.  Otherwise, it will
# use the same servers information as in the parent domain.
#	nonroot_master()
#			- sets up a non-root master server.
#

#
# nonroot_master(): sets up a nonroot master server. 
#
nonroot_master()
{
	# ... local variables
	PROMOTE="make"

	# ...check parent domain
	PAR_DOM=`expr "$DOM" : '[^\.]*\.\(.*\)'`
	if nistest -t D $PAR_DOM;
	then
		:
	else
		echo "**ERROR: $PAR_DOM does not exist."
		exit 1
	fi

	if [ -z "$SEC" ]     # NOTE: This conditional currently always false
	then
		echo ""
		echo "You must specify the security level:"
		eval "echo get security info ... $VERB"
		get_security
		echo ""
	fi

	if check_host "$HOST";
	then
		exit 1
	fi

	# ...check permission
	check_perm $PAR_DOM
	if [ $? -ne 0 ]
	then
		echo "**ERROR: no permission to create directory $DOM"
		exit 1
	fi

	echo "This script sets up a non-root NIS+ master server for domain"
	echo "$DOM"
	if [ -z "$FORCE" ]
	then
		confirm
	else
		echo ""
		print_info
	fi

	echo ""
	if [ -z "$HOST" ]
	then
		HOSTDEF=""
		echo "This script will set up an NIS+ non-root master for domain"
		echo "$DOM $WITHOUT NIS compatibility, using the same servers for"
		echo "domain $PAR_DOM."
		nis_server $PAR_DOM
		echo "servers: $ALLSERVERS"
	else
		echo "This script sets up machine \"$HOST\" as an NIS+"
		HOSTDEF="yes"
		echo "non-root master server for domain $DOM $WITHOUT NIS compatibility."
		MASTER_SERVER=$HOST
		ALLSERVERS=$HOST
		eval "echo $VERB"
		eval "echo checking rpc.nisd process on $HOST ... $VERB"
		rpcinfo -u $HOST 100300 3 > /dev/null
		if [ $? -ne 0 ]
		then
			echo "**ERROR: NIS+ server is not running on $HOST."
			echo "	You must do the following before becoming an NIS+ server:"
			echo "	1. become an NIS+ client of the parent domain or any domain"
			echo "	   above the domain which you plan to serve. (nisclient)"
			echo "	2. start the NIS+ server. (rpc.nisd)"
			exit 1
		fi
	fi
	echo ""

	if [ -z "$FORCE" ]
	then
		get_yesno $CONTINUE
		echo ""
	fi

	# ...check domain
	if nistest -t D $DOM;
	then
		echo "**WARNING: domain $DOM already exists."
		if [ ! -z "$HOST" ]
		then
			nis_server $DOM
			if [ "`tolower $HOSTPRINC`" = "`tolower $MASTER_SERVER`" ]
			then
				echo "$HOSTPRINC is already a master server for thisdomain."
				echo "If you choose to continue with this script, it will"
				echo "try to create the groups_dir and org_dir directories"
				echo "for this domain."
				IGNORE="yes"
			else
				is_server $HOSTPRINC
				if [ $? -eq 0 ]
				then
					echo "$HOSTPRINC is already a replica server for this domain."
				fi
				echo "If you choose to continue with this script, it will"
				echo "promote $HOSTPRINC to be the new master for $DOM"
				PROMOTE="promote new master for"
			fi
		else
			echo "If you choose to continue with this script, it will"
			echo "try to create the groups_dir and org_dir directories"
			echo "for this domain."
			IGNORE="yes"
		fi

		echo ""
		if [ -z "$FORCE" ]
		then
			get_yesno $CONTINUE
			echo ""
		fi
	fi


	eval "echo setup NIS_GROUP environment variable ... $VERB"
	GROUP=${GROUP:-admin.$DOM}
	$ECHO "NIS_GROUP=$GROUP; export NIS_GROUP"
	eval "echo '' $VERB"

	eval "echo running nismkdir ... $VERB"
	DEF_PERM="-D access=g=rmcd,n=r"
	if [ -z "$HOST" ]
	then
		$ECHO nismkdir $DEF_PERM $DOM
	else
		$ECHO nismkdir $DEF_PERM -m $HOSTPRINC $DOM
	fi

	if [ -z "$IGNORE" -a $? -ne 0 ]
	then
		echo "**ERROR: it failed to $PROMOTE the $DOM directory."
		exit 1
	fi

	$ECHO "nisupdkeys $DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "**WARNING: nisupdkeys failed on directory $DOM"
		echo "	This script will not be able to continue."
		echo "	Please remove the $DOM directory using 'nisrmdir'."
		exit 1
	fi

	$ECHO "nisping $PAR_DOM > /dev/null"
	sleep 4

	if [ $OS -ne 5 -o $OSVER -lt 3 ]
	then
		DEF_PERM="-D access=g=rmcd,n=r"
	else
		DEF_PERM=""
	fi
	if [ "$PROMOTE" != "make" ]
	then
		$ECHO nismkdir $DEF_PERM -m $HOSTPRINC org_dir.$DOM
		if [ $? -ne 0 ]
		then
			echo "**ERROR: it failed to $PROMOTE the org_dir.$DOM directory."
			exit 1
		fi
		$ECHO nismkdir $DEF_PERM -m $HOSTPRINC groups_dir.$DOM
		if [ $? -ne 0 ]
		then
			echo "**ERROR: it failed to $PROMOTE the groups_dir.$DOM directory."
			exit 1
		fi
	fi

	echo "running nissetup ..."
	$ECHO "nissetup $YP $DOM"
	if [ -z "$IGNORE" -a $? -ne 0 ]
	then
		echo "**ERROR: it failed to create the tables."
		exit 1
	fi
	if [ $OS -ne 5 -o $OSVER -lt 3 ]
	then
		$ECHO "nischmod n+r cred.org_dir.$DOM"
	fi
	echo ""

	if [ `echo $GROUP | cut -d. -f2-` = $DOM ]
	then
		echo "setting NIS+ group to ${GROUP} ..."
		$ECHO "nisgrpadm -c $GROUP > /dev/null"
		if [ $? -ne 0 ]
		then
			echo "**WARNING: failed to create the $GROUP group."
			echo "	You will need to create this group manually:"
			echo "	  1. /usr/bin/nisgrpadm -c $GROUP"
			echo "	  2. /usr/bin/nisgrpadm -a $GROUP $ALLSERVERS"
		else
			$ECHO "nisgrpadm -a $GROUP $ALLSERVERS > /dev/null"
			if [ $? -ne 0 ]
			then
				echo "**WARNING: failed to add the following members into"
				echo "the $GROUP group:"
				echo $ALLSERVERS
				echo ""
				echo "	You will need to add this member manually:"
				echo "	  1. /usr/bin/nisgrpadm -a $GROUP $ALLSERVERS"
			else
				$ECHO "nisctl -f g $DOM > /dev/null"
			fi
		fi
	fi
	echo ""

	eval "echo updating the keys for directories ... $VERB"
	$ECHO "nisupdkeys org_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory org_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys org_dir.$DOM"
		echo ""
	fi
	$ECHO "nisupdkeys groups_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory groups_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys groups_dir.$DOM"
		echo ""
	fi
	eval "echo $VERB"

 	if [ ! -z "$HOST" -a "`tolower $HOST`" != "$LOCALHOST" ]
	then
		eval "echo changing the owner on the directory ... $VERB"
		nis_chown $MASTER_SERVER $DOM
		eval "echo $VERB"
	fi

	if [ ! -z "$HOSTDEF" ]
	then
		setup_rpc
		echo ""
	fi

	# start rpc.nispasswdd if setting up on localhost
 	if [ ! -z "$HOST" -a "`tolower $HOST`" = "$LOCALHOST" ]
	then
		# check to see if already running...
		PROC=`get_pids rpc.nispasswdd`
		if [ -z "$PROC" ]
		then
			eval "echo starting NIS+ password daemon ... $VERB"
			$ECHO rpc.nispasswdd
		else
			eval "echo NIS+ password daemon already running ... $VERB"
		fi
	else
		# else need to print message saying start it up
		echo ""
		echo "**IMPORTANT:"
		echo "	Be sure to start the NIS+ password daemon (rpc.nispasswdd) on the"
		if [ -z "$HOST" ]
		then
		    echo "	new NIS+ non-root (subdomain) master server IF NOT ALREADY."
		else
		    echo "	new NIS+ non-root (subdomain) master server $HOST IF NOT ALREADY."
		fi
		echo ""
	fi

	echo ""
	echo "The server(s) for the non-root domain $DOM is(are) now"
	echo "configured.  You can now populate the standard NIS+ tables by"
	echo "using the nispopulate or /usr/lib/nis/nisaddent commands."
}



#
# replica setup Routine:
# ----------------------
# 
# This section contains the routine to setup a replica server.
# NOTE: If the -h <hostname> is specified, then it will configure the
# specified host as a replica of the domain.  Otherwise, it will
# configure the local machine as a replica of the domain.
#	replica() 	- sets up replica server.
#
#
# replica(): sets up a replica server. 
#
replica()
{
	# ...check domain
	if nistest -t D $DOM;
	then
		:
	else
		echo "**ERROR: $DOM does not exist."
		exit 1
	fi

	if check_host "$HOST";
	then
		exit 1
	fi

	# ...check permission 
	check_perm $DOM
	if [ $? -ne 0 ]
	then
		echo "**ERROR: no permission to replicate directory $DOM"
		exit 1
	fi

	echo "This script sets up an NIS+ replica server for domain"
	echo "$DOM"

	if [ -z "$HOST" ]
	then
 		HOST=$LOCALHOST
		HOSTPRINC=$HOST.`nisdefaults -d`
	fi

	if [ -z "$FORCE" ]
	then
		confirm
	else
		echo ""
		print_info
	fi

	echo ""
	nis_server $DOM
	if [ "`tolower $HOSTPRINC`" = "`tolower $MASTER_SERVER`" ]
	then
		echo "ERROR: $HOST is a master server for this domain."
		echo "You cannot demote a master server to replica."
		echo "If you really want to demote this master, you should"
		echo "promote a replica server to master using nisserver"
		echo "with the -M option."
		exit 1
	fi

	is_server $HOSTPRINC
	if [ $? -eq 0 ]
	then
		echo "WARNING: $HOST is already a server for this domain."
		echo "If you choose to continue with this script, it will"
		echo "try to replicate the groups_dir and org_dir directories"
		echo "for this domain."
		IGNORE="yes"
	else
		echo "This script will set up machine \"$HOST\" as an NIS+"
		echo "replica server for domain $DOM $WITHOUT NIS compatibility."
		echo "The NIS+ server daemon, rpc.nisd, must be running on $HOST"
		echo "with the proper options to serve this domain."	
	fi
	echo ""

	if [ -z "$FORCE" ]
	then
		get_yesno $CONTINUE
		echo ""
	fi

	$ECHO "rpcinfo -u $HOST 100300 3 > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "**ERROR: NIS+ server is not running on $HOST."
		echo "	You must do the following before becoming an NIS+ server:"
		echo "	1. become an NIS+ client of the parent domain or any domain"
		echo "	   above the domain which you plan to serve. (nisclient)"
		echo "	2. start the NIS+ server. (rpc.nisd)"
		exit 1
	fi

	eval "echo running nismkdir ... $VERB"
	$ECHO nismkdir -s $HOSTPRINC $DOM
	if [ -z "$IGNORE" -a $? -ne 0 ]
	then
		echo "**ERROR: it failed to replicate the directory."
		exit 1
	fi
	sleep 3
	$ECHO "nisupdkeys $DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "**WARNING: nisupdkeys failed on directory $DOM"
		echo "	This script will not be able to continue."
		echo "	Please remove the $DOM directory using 'nisrmdir'."
		exit 1
	fi

	$ECHO nismkdir -s $HOSTPRINC org_dir.$DOM
	if [ -z "$IGNORE" -a $? -ne 0 ]
	then
		echo "**ERROR: it failed to replicate the org_dir directory."
		exit 1
	fi
	sleep 3
	$ECHO nismkdir -s $HOSTPRINC groups_dir.$DOM
	if [ -z "$IGNORE" -a $? -ne 0 ]
	then
		echo "**ERROR: it failed to replicate the groups_dir directory."
		exit 1
	fi
	sleep 3

	eval "echo updating the keys for directories ... $VERB"
	$ECHO "nisupdkeys org_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory org_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys org_dir.$DOM"
		echo ""
	fi
	$ECHO "nisupdkeys groups_dir.$DOM > /dev/null"
	if [ $? -ne 0 ]
	then
		echo "WARNING: nisupdkeys failed on directory groups_dir.$DOM"
		echo "	You will need to run nisupdkeys manually:"
		echo "	  1. /usr/lib/nis/nisupdkeys groups_dir.$DOM"
		echo ""
	fi
	eval "echo $VERB"

	if [ ! -z "$GROUPSERVER" ]
	then
		if nisgrpadm -s -t $GROUPSERVER $HOSTPRINC;
		then
			:
		else
			eval "echo adding replica principal into group owner ...$VERB"
			$ECHO nisgrpadm -a $GROUPSERVER $HOSTPRINC
			$ECHO "nisctl -f g $DOM > /dev/null"
		fi
	fi

	eval "echo pinging $DOM directory object on new replica ... $VERB"
	$ECHO "nisping -H ${HOST} $DOM > /dev/null"
	sleep 10
	eval "echo pinging $DOM groups_dir object on new replica ... $VERB"
	$ECHO "nisping -H ${HOST} groups_dir.$DOM > /dev/null"
	sleep 10
	eval "echo pinging $DOM org_dir object on new replica ... $VERB"
	$ECHO "nisping -H ${HOST} org_dir.$DOM > /dev/null"

	echo ""
	echo "The system ${HOST} is now configured as a replica server for"
	echo "domain $DOM."
	echo "The NIS+ server daemon, rpc.nisd, must be running on $HOST"
	echo "with the proper options to serve this domain."
	echo ""
	echo "If you want to run this replica in NIS (YP) compatibility mode,"
	echo "edit the /etc/init.d/rpc file on the replica server to uncomment"
	echo "the line which sets EMULYP to '-Y'.  This will ensure that rpc.nisd"
	echo "will boot in NIS-compatibility mode.  Then, restart rpc.nisd"
	echo "with the '-Y' option. These actions should be taken after this"
	echo "script completes."
}



#
#
# 			* * * MAIN * * *
#

init

parse_arg $*
shift $?

check_domainname "$DOM"

check_rootgrp "$GROUP"
if [ $? -ne 0 ]
then
	echo "**ERROR: invalid group name."
	echo "	It must be a group in the $DOM domain."
	exit 1
fi


case $ACTION in
"master")
	${ROOT}_master;;
"replica")
	replica;;
*)
	echo "**ERROR: you must specify one of these options: -r, -M or -R"
	print_usage
	exit 1
esac
