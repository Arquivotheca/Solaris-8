#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident   "@(#)devlink.tab.sh 1.70     99/02/15 SMI"
#
# This is the script that generates the devlink.tab file. It is
# architecture-aware, and dumps different stuff for x86 and sparc.
# There is a lot of common entries, which are dumped first.
#
# the SID of this script, and the SID of the dumped script are
# always the same.
#

cat <<EOM
#ident   "@(#)devlink.tab.sh 1.70     99/02/15 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
#
# This is the table used by devlinks
#
# Each entry should have 2 fields; but may have 3.  Fields are separated
# by single tab ('\t') characters.
#
# The fields are:
#
# devfs-spec: a keyword-value set of devfs specifications, describing the set
#	of devfs node entries to be linked.
#
#	The keywords are:
#
#	type - The devinfo node type (see <sys/sunddi.h> for possible values)
#
#	name - the devinfo node name (the part of a /devices entry that appears
#		before the '@' or ':').
#
#	addr - the devinfo node address part (the portion of the name between
#		the '@' and the ':').
#
#	minor - the minor-attributes (the portion of a /devices name after the
#		':').
#
#	The keywords are separated from their valuse by an equals ('=') sign;
#	keyword-value pairs are separated from each other by semicolons (';').
#
# dev name - the /dev name corresponding to the devfs node described by
#	the devfs-spec field.  This specification is assume to start rooted at
#	/dev; THE INITIAL /dev/ SHOULD NOT BE SPECIFIED!
#	The name can contain a number of escape-sequences to include parts of
#	the devfs-name in the /dev/-name.  These escape-sequences all start with
#	a backslash ('\') character.  The current sequences are:
#
#	\D - the devfs 'name' field
#
#	\An - the 'n'th component of the address field (n=0 means the whole
#		address field)
#
#	\Mn - the 'n'th component of the minor field (n=0 means the entire
#		minor field).
#
#	\Nn - a sequential counter, starting at n (a *single* digit, giving
#		a starting range of 0 through 9).
#
# extra dev link - a few devices need a second link; that is, a second link
#	pointing to the first link.  This optional field specifies the /dev
#	format of this second link.  This entry can also use the above-described
#	escape-sequences.
#
# Fields can be blank; seperated by single tab characters,
# Spaces are significant, and are considered part of a field. IN GENERAL THIS
# MEANS THERE SHOULD BE NO SPACE CHARACTERS IN THIS FILE!
# All fields must be present (even if blank)
#
#
# devfs-spec	Dev-Namespec	Extra-Link
#
type=ddi_pseudo;name=md;minor=admin	md/admin
type=ddi_pseudo;name=md;minor2=blk	md/dsk/d\M1
type=ddi_pseudo;name=md;minor2=raw	md/rdsk/d\M1
EOM

case "$MACH" in
  "i386" ) 
	# 
	# These are the x86 specific entries
	# It depends on the build machine being an x86
	#
	cat <<-EOM
	EOM
	;;
  "sparc" )
	#
	# These are the sparc specific entries
	# It depends on the build machine being an sparc
	#
	cat <<-EOM
	EOM
	;;
  * )
	echo "Unknown Architecture"
	exit 1
	;;
esac
