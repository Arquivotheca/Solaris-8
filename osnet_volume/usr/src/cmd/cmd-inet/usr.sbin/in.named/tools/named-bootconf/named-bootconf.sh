#!/bin/ksh
#ident   "@(#)named-bootconf.sh 1.4     99/03/16 SMI"
USAGE="usage: $0 [-i infile] [-o outfile]"
InFile=/etc/named.boot
OutFile=/etc/named.conf
#Options[1024]
#Zones[1024]
typeset -i NoOpt=1
typeset -i zCnt=0
typeset -i inCnt=0
typeset -i optCnt=0

initialize()
{
	(( optCnt = optCnt + 1 ))
	Options[optCnt]="options {\n"
}

processComments()
{
	CLine="$*"
	if [[ inZone == 1 ]]
	then
		(( zCnt = zCnt + 1 ))
		Zones[zCnt]="//${CLine}\n" 
	else
		(( optCnt = optCnt + 1 ))
		Options[optCnt]="\t//${CLine}\n" 
	fi
	#print $CurLine
}

processZone()
{
	#print $CurLine
	(( zCnt = zCnt + 1 ))
	OptLine="$*"
	set ${OptLine}
	ArgCount="$#"
	FirstWord=$1
	RestOfLine=${CurLine#*$FirstWord}
	if [[ $FirstWord == "primary" ]]
	then
		set ${RestOfLine}
		Zone[zCnt]="zone \"${1}\" in {\n\ttype	master;\n"
		Zone[zCnt]="${Zone[zCnt]}\tfile	\"${2}\";\n};\n\n" 
	elif [[ $FirstWord == "secondary" ]]
	then
		set ${RestOfLine}
		Zone[zCnt]="zone \"${1}\" in {\n\ttype	slave;\n"
		Zone[zCnt]="${Zone[zCnt]}\tfile	\"${3}\";\n" 
		Zone[zCnt]="${Zone[zCnt]}\tmasters	{ ${2}; };\n};\n\n" 
	elif [[ $FirstWord == "stub" ]]
	then
		set ${RestOfLine}
		Zone[zCnt]="zone \"${1}\" in {\n\ttype	stub;\n"
		Zone[zCnt]="${Zone[zCnt]}\tfile	\"${3}\";\n" 
		Zone[zCnt]="${Zone[zCnt]}\tmasters	{ ${2}; }\n};\n\n" 
	elif [[ $FirstWord == "cache" ]]
	then
		set ${RestOfLine}
		Zone[zCnt]="zone \".\" in {\n\ttype	hint;\n"
		Zone[zCnt]="${Zone[zCnt]}\tfile	\"${2}\";\n};\n\n" 
	fi
}

processBogus()
{
	(( zCnt = zCnt + 1 ))
	RestOfLine="$*"
	ArgCount="$#"
	let loopCnt=0
	while (( loopCnt < ArgCount ))
	do
		set ${RestOfLine}
		NextAddr=$1
		RestOfLine=${RestOfLine#*$NextAddr}
		(( zCnt = zCnt + 1 ))
		Zone[zCnt]="server	\"${1}\"\t{\n\tbogus yes;\n};\n\n"
		((loopCnt=loopCnt+1))
	done
}

processOptions()
{
	# If we got here, then we only have atleast 1 option to deal with
	NoOpt=0;

	#Increment the optCnt
	(( optCnt = optCnt + 1 ))
	OptLine="$*"
	set ${OptLine}
	ArgCount="$#"
	FirstWord=$1
	RestOfLine=${CurLine#*$FirstWord}
	if [[ $FirstWord == "directory" ]]
	then
                dirName=`echo ${RestOfLine} | awk '{print $1}'`
		Options[optCnt]="${Options[optCnt]}\tdirectory\t \"${dirName}\";\n" 
	elif [[ $FirstWord == "xfrnets" ]]
	then
		Options[optCnt]="${Options[optCnt]}\tallow-transfer\t {\n" 
		let loopCnt=0
		while (( loopCnt < ArgCount-1 ))
		do
			set ${RestOfLine}
			NextAddr=$1
			RestOfLine=${RestOfLine#*$NextAddr}
			Options[optCnt]="${Options[optCnt]}\t\t${NextAddr};\n"
			((loopCnt=loopCnt+1))
		done
		Options[optCnt]="${Options[optCnt]}\t };\n"
	elif [[ $FirstWord == "limit" ]]
	then
		set ${RestOfLine}
		if [[ $1 == "transfers-in" ]]
		then
			Options[optCnt]="${Options[optCnt]}\ttransfers-in\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
	
		elif [[ $1 == "transfers-per-ns" ]]
		then
			Options[optCnt]="${Options[optCnt]}\ttransfers-per-ns\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
		elif [[ $1 == "datasize" ]]
		then
			Options[optCnt]="${Options[optCnt]}\tdatasize\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
		elif [[ $1 == "pollfd-chunk-size" ]]
		then
			Options[optCnt]="${Options[optCnt]}\tpollfd-chunk-size\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
		elif [[ $1 == "open-fd-offset" ]]
		then
			Options[optCnt]="${Options[optCnt]}\topen-fd-offset\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
		elif [[ $1 == "listen-backlog" ]]
		then
			Options[optCnt]="${Options[optCnt]}\tlisten-backlog\t" 
			Options[optCnt]="${Options[optCnt]} $2;\n"
		fi
	elif [[ $FirstWord == "forwarders" ]]
	then
		Options[optCnt]="${Options[optCnt]}\tforwarders\t {\n" 
		let loopCnt=0
		while (( loopCnt < ArgCount-1 ))
		do
			set ${RestOfLine}
			NextAddr=$1
			RestOfLine=${RestOfLine#*$NextAddr}
			Options[optCnt]="${Options[optCnt]}\t\t${NextAddr};\n"
			((loopCnt=loopCnt+1))
		done
		Options[optCnt]="${Options[optCnt]}\t };\n"
	elif [[ $FirstWord == "slave" ]]
	then
		Options[optCnt]="${Options[optCnt]}\tforward only;\n"
	elif [[ $FirstWord == "options" ]]
	then
		Opt=$2;
	        #print $RestOfLine
		let loopCnt=0
		while (( loopCnt < ArgCount-1 ))
		do
			set ${RestOfLine}
			NextOpt=$1
			if [[ $1 == "forward-only" ]]
			then
				Options[optCnt]="${Options[optCnt]}\tforward only;\n" 
			elif [[ $1 == "no-recursion" ]]
			then
				Options[optCnt]="${Options[optCnt]}\trecursion no;\n" 
		
			elif [[ $1 == "fake-iquery" ]]
			then
				Options[optCnt]="${Options[optCnt]}\tfake-iquery yes;\n" 
			elif [[ $1 == "no-fetch-glue" ]]
			then
				Options[optCnt]="${Options[optCnt]}\tfetch-glue no;\n" 
			elif [[ $1 == "query-log" ]]
			then
				(( zCnt = zCnt + 1 ))
				Zones[zCnt]="logging {\n\tcategory queries { default_syslog; };" 
				Zones[zCnt]="${Zones[zCnt]} };" 
			fi
			RestOfLine=${RestOfLine#*$NextOpt}
#;			Options[optCnt]="${Options[optCnt]}\t\t${NextOpt};\n"
			((loopCnt=loopCnt+1))
		done
	elif [[ $FirstWord == "check-names" ]]
	then
		set ${RestOfLine}
		if [[ $1 == "primary" ]]
		then
			Options[optCnt]="\tcheck-names master $2;\n" 
		elif [[ $1 == "secondary" ]]
		then
			Options[optCnt]="\tcheck-names slave $2;\n" 
		elif [[ $1 == "response" ]]
		then
			Options[optCnt]="\tcheck-names response $2;\n" 
		fi
	
	fi
}

processInclude ()
{
	typeset -L CurLine="$*"
	(( inCnt = inCnt + 1 ))
	/bin/mv $CurLine ${CurLine}~
	named-bootconf -i ${CurLine}~ -o ${CurLine}
	Include[inCnt]="include\t\"${CurLine}\";\n"
}

processLine ()
{
	CurLine="$*"
	set ${CurLine}
	ArgCount="$#"
	FirstWord=$1
	RestOfLine=${CurLine#*$FirstWord}
	case $FirstWord in
		"primary"|"secondary"|"cache")	inZone=1
					processZone $CurLine;;
		"bogusns")		processBogus $RestOfLine;;
		";")			processComments $RestOfLine;;
		"include")		processInclude $RestOfLine;;
		*)			processOptions $CurLine;;
	esac
	
}

usage ()
{
	while getopts ":i:o:" opt
	do
		case $opt in
			i)	InFile=$OPTARG;;
			o)	OutFile=$OPTARG;;
			\?)	print "$OPTARG is not a valid option"
				print "$USAGE"
				exit ;;
		esac
	done
	return 0;
}

# Script begins execution here by checking the proper usage  
usage "$@"

# Open the Input and Output files
if [ ! -f $InFile ]
then
	print "${InFile} does not exist"
	exit
fi
exec 4< $InFile
exec 5> $OutFile

typeset -l Line
# Set IFS to new-line, tab and space
IFS="
 	"

# Set up some counters
let lineCount=0
let argCount=0

# Run the initialize function
initialize

# Read every line, parse and process it
while read -u4 Line
do

	# Ignore any blank lines
	if [[ "$Line" == "" ]]
	then
		continue
	fi

	# Process the Line
	processLine "$Line"

	# Print out the Options first followed by the zones
done
Options[optCnt]="${Options[optCnt]}\n};\n" 
if [[ "$NoOpt" == 0 ]]
then
	print -u5 ${Options[*]}
fi
print -u5 ${Include[*]}
print -u5 ${Zone[*]}
exit 0

