#! /bin/sh
# @(#) so425 1.2 94/03/27
#
# so425: convert a 4.x so to something suitable for linking on 5.x.
#
NMTMP=/tmp/so425.nm.$$
ASTMP=/tmp/so425.$$.s
# Set this to point at a 4.x "nm" command
NM4=$HOME/4bin/nm
trap 'rm -rf $NMTMP $ASTMP' 0 HUP INT TERM

# Get the 4.x namelist from the library
$NM4 -n $1 >$NMTMP
if [ $? != "0" ]; then
	exit 1
fi

# Convert the namelist to an assembler source file that will generate
# an appropriate 5.x .so that can be used for linking (but NOT for 
# running, at least, not correctly!) -- use the 4.x one for that.
nawk '
	function emit(s) {
		if (symbol &&
		    (name != "etext") &&
		    (name != "edata") &&
		    (name != "end") &&
		    (name != "_GLOBAL_OFFSET_TABLE_")) {
			printf("\t.global %s\n", name);
			printf("%s:\n", name);
			printf("\t.type %s,%s\n", name, type);
			printf("\t.size %s,0x%x\n", name, s);
		}
		symbol = 0;
	}
	function settype(t) {
		symbol = 1;
		type = t;
	}
	function xtoi(s) {
		sum = 0;
		for (cp = 1; cp <= length(s); cp++)
			sum = (sum * 16) + \
			    (index("0123456789abcdef", substr(s, cp, 1)) - 1);
		return (sum);
	}
	BEGIN {
		oa = 0;
		symbol = 0;
	}
	{
		na = xtoi($1);
		size = na - oa;
		emit(size);
		oa = na;
		if (substr($3, 1, 1) == "_")
			name = substr($3, 2, length($3) - 1);
		else
			name = $3;
		if ($2 == "T")
			settype("#function");
		else if (($2 == "D") || ($2 == "B"))
			settype("#object");
		else if ($2 == "A")
			printf("\t.global %s\n\t%s=0x%x\n", name, $3, oa);
	}
	END {
		emit(0);
	}' $NMTMP >$ASTMP

if [ -s $ASTMP ]; then
	cc -G $RP -o `nawk '
		BEGIN {
			split(ARGV[ARGC - 1], a, ".");
			printf("%s.%s", a[1], a[2]);
			exit 0;
		}' $1` -h `basename $1` $ASTMP
fi

exit 0
