#
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)logindevperm.sh	1.5	99/03/02 SMI"
#
#
# This is the script that generates the logindevperm file. It is
# architecture-aware, and dumps different stuff for x86 and sparc.
# There is a lot of common entries, which are dumped first.
#
# the SID of this script, and the SID of the dumped script are
# always the same.
#

cat <<EOM
#
# Copyright 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)logindevperm	1.5	99/03/02 SMI"
#
# /etc/logindevperm - login-based device permissions
#
# If the user is logging in on a device specified in the "console" field
# of any entry in this file, the owner/group of the devices listed in the
# "devices" field will be set to that of the user.  Similarly, the mode
# will be set to the mode specified in the "mode" field.
#
# "devices" is a colon-separated list of device names.  A device name
# ending in "/*", such as "/dev/fbs/*", specifies all entries (except "."
# and "..") in a directory.  A '#' begins a comment and may appear
# anywhere in an entry.
#
# console	mode	devices
#
/dev/console	0600	/dev/mouse:/dev/kbd
/dev/console	0600	/dev/sound/*		# audio devices
/dev/console	0600	/dev/fbs/*		# frame buffers
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
	# It depends on the build machine being a sparc
	#
	cat <<-EOM
	EOM
	;;
    "ppc" )
	# 
	# These are the ppc specific entries
	# It depends on the build machine being a ppc
	#
	cat <<-EOM
	EOM
	;;
    * )
	echo "Unknown Architecture"
		exit 1
	;;
esac
