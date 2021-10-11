#!/usr/bin/ksh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Copyright (c) 1998 by Sun Microsystems, Inc.
#	ll rights reserved.

#ident	"@(#)spell.sh	1.17	99/05/21 SMI"	/* SVr4.0 1.11	*/
#	spell program
# B_SPELL flags, D_SPELL dictionary, F_SPELL input files, H_SPELL history,
# S_SPELL stop, V_SPELL data for -v
# L_SPELL sed script, I_SPELL -i option to deroff
PATH=/usr/lib/spell:/usr/bin:$PATH

SPELLPROG=/usr/lib/spell/spellprog

H_SPELL=${H_SPELL:-/var/adm/spellhist}
V_SPELL=/dev/null
F_SPELL=
FT_SPELL=
B_SPELL=
L_SPELL="/usr/bin/sed -e \"/^[.'].*[.'][ 	]*nx[ 	]*\/usr\/lib/d\" -e \"/^[.'].*[.'][ 	]*so[ 	]*\/usr\/lib/d\" -e \"/^[.'][ 	]*so[ 	]*\/usr\/lib/d\" -e \"/^[.'][ 	]*nx[ 	]*\/usr\/lib/d\" "

# mktmpdir - Create a private (mode 0700) temporary directory inside of /tmp
# for this process's temporary files.  We set up a trap to remove the
# directory on exit (trap 0), and also on SIGHUP, SIGINT, SIGQUIT, and
# SIGTERM.
#
mktmpdir() {
	tmpdir=/tmp/spell.$$
	trap "/usr/bin/rm -rf $tmpdir; exit" 0 1 2 13 15
	/usr/bin/mkdir -m 700 $tmpdir || exit 1
}

mktmpdir

# figure out whether or not we can use deroff
if [ -x /usr/bin/deroff ]
then
	DEROFF="deroff \$I_SPELL"
else
	DEROFF="cat"
fi

# Filter out + arguments that are incorrectly handled by getopts
set -A args xxx "$@"
while [ x${args[$OPTIND]#+} = x${args[$OPTIND]} ] && getopts ablvxi A
do
	case $A in
	v)	if [ -r /bin/pdp11 ] && /bin/pdp11 
		then	gettext "spell: -v option not supported on pdp11\n" 1>&2
			EXIT_SPELL="exit 1"
		else	B_SPELL="$B_SPELL -v"
			V_SPELL=$tmpdir/spell.$$
		fi ;;
	b) 	D_SPELL=${LB_SPELL:-/usr/lib/spell/hlistb}
		B_SPELL="$B_SPELL -b" ;;
	x)	B_SPELL="$B_SPELL -x" ;;
	l)	L_SPELL="cat" ;;
	i)	I_SPELL="-i" ;;
	?)	gettext "Usage: spell [-bvxli] [+local_file] [files...]\n" 1>&2
		exit 1;;
	esac
done
shift $(($OPTIND - 1))

for A in $*
do
	case $A in
	+*)	if [ "$FIRSTPLUS" = "+" ]
			then	gettext "spell: multiple + options in spell, all but the last are ignored" 1>&2
		fi;
		FIRSTPLUS="$FIRSTPLUS"+
		if  LOCAL=`expr $A : '+\(.*\)' 2>/dev/null`;
		then if test ! -r $LOCAL;
			then printf "`gettext 'spell: Cannot read %s'`\n" "$LOCAL" 1>&2; EXIT_SPELL="exit 1";
		     fi
		else gettext "spell: Cannot identify local spell file\n" 1>&2; EXIT_SPELL="exit 1";
		fi ;;
	*)	FT_SPELL="$FT_SPELL $A"
		if [ -r $A ]; then
			F_SPELL="$F_SPELL $A"
		else
			printf "`gettext 'spell: Cannot read file %s'`\n" "$A" 1>&2
		fi
	esac
done
${EXIT_SPELL:-:}

if [ "x$FT_SPELL" != "x$F_SPELL" ] && [ "x$F_SPELL" = "x" ]; then
	exit 1
fi

cat $F_SPELL | eval $L_SPELL |\
 eval $DEROFF |\
 /usr/bin/tr -cs "[A-Z][a-z][0-9]\'\&\.\,\;\?\:" "[\012*]" |\
 /usr/bin/sed '1,$s/^[^A-Za-z0-9]*//' | /usr/bin/sed '1,$s/[^A-Za-z0-9]*$//' |\
 /usr/bin/sed -n "/[A-Za-z]/p" | /usr/bin/sort -u +0 |\
 $SPELLPROG ${S_SPELL:-/usr/lib/spell/hstop} 1 |\
 $SPELLPROG $B_SPELL ${D_SPELL:-/usr/lib/spell/hlista} $V_SPELL |\
 comm -23 - ${LOCAL:-/dev/null} |\
 tee -a $H_SPELL
/usr/bin/who am i >>$H_SPELL 2>/dev/null
case $V_SPELL in
/dev/null)
	exit
esac
/usr/bin/sed '/^\./d' $V_SPELL | /usr/bin/sort -u +1f +0
