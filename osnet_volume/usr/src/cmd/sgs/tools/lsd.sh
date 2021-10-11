#! /bin/sh 
# @(#)lsd 1.6 93/06/06
#
# lsd: list .so differences
#
USAGE='usage: lsd [-c] [-p] old.so new.so'
OTMP=/tmp/lsd.o.$$
NTMP=/tmp/lsd.n.$$
OCTMP=/tmp/lsd.oc.$$
NCTMP=/tmp/lsd.nc.$$
NM=/usr/ccs/bin/nm
OLD=""
NEW=""
status=0
trap 'rm -f $OTMP $NTMP $OCTMP $NCTMP' 0 HUP INT TERM

usage() {
	echo "$USAGE"
	exit 2
}

#
# Process options -- by default, ANSI checking, and ignore internal names
#	-c: suppress ANSI checking
#	-p: do not suppress "internal" name convention
#
ansi=1
physical="egrep -v ^_[_A-Z]"
while getopts cp c; do
	case $c in
	c) ansi=0;;
	p) physical="cat";;
	'?') usage;;
	esac
done

#
# Pick up the base file
#
while [ $# -ge 1 ]; do
	if [ -r $1 ]; then
		OLD=$1
		break
	fi
	shift
done

if [ -z "$OLD" ]; then
	usage
fi

#
# Get the new file
#
if [ -r "$2" ]; then
	NEW=$2
else
	usage
fi

#
# Extract the interface
#
program='
	$5 ~ /(^GLOB|^WEAK)/ && $4 ~ /(^FUNC|^OBJT)/ && $7 !~ /^UNDEF/ {
		print $8, $4, $3
	}'
$NM -h $OLD | nawk -F\| "$program" | sort | $physical >$OTMP
$NM -h $NEW | nawk -F\| "$program" | sort | $physical >$NTMP

#
# If necessary, do the ANSI C check: note "cat" added to end of pipeline
# to defeat "-f" pause from "pr".
#
if [ $ansi -eq 1 ]; then
	nawk '{print $1}' $OTMP | egrep '^_[a-z]' >$OCTMP
	nawk '{print $1}' $NTMP | egrep '^_[a-z]' >$NCTMP
	if [ -s $OCTMP ]; then
		pr -2f -w80 -h "$OLD Violations of ANSI C rules" $OCTMP | cat
		status=1
	fi
	if [ -s $NCTMP ]; then
		pr -2f -w80 -h "$NEW Violations of ANSI C rules" $NCTMP | cat
		status=1
	fi
fi

#
# Scan both temporary files, separate out all the data, and then
# report incompatibilities
#
nawk '
	function fn(n, t, s) {
		if (t == "")
			return "deleted"
		else if (t == "FUNC")
			return sprintf("%s()", n);
		else
			return sprintf("%s[0x%x]", n, s);
	}
	BEGIN {
		old = 0;		# we start by reading the old file
	}
	FNR != NR {
		if (old == 0) {
			old = 1;
			NR = FNR;
		} else
			exit;
	}
	{
		symbol[$1] = $1;
		if (old == 0) {
			ot[$1] = $2
			os[$1] = int($3)
		} else {
			nt[$1] = $2
			ns[$1] = int($3)
		}
	}
	END {
		#
		# Flag all symbols where the symbol is:
		#	a) undefined in the new file; or
		#	b) has changed types; or
		#	c) is an OBJT that has changed size
		#
		for (sym in symbol)
			if ((nt[sym] == "") ||
			    ((ot[sym] != nt[sym]) && (ot[sym] != "")) ||
			    ((ot[sym] == "OBJT") && (os[sym] != ns[sym])))
				printf("%s -> %s\n",
				    fn(sym, ot[sym], os[sym]),
				    fn("", nt[sym], ns[sym]));
	}' $OTMP $NTMP >$OCTMP

if [ -s $OCTMP ]; then
	sort $OCTMP | pr -2f -w80 -h "Incompatibilities: $OLD to $NEW" | cat
	status=1
fi
			
exit $status
