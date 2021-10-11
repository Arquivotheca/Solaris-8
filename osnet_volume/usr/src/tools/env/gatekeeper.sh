#
#ident	"@(#)gatekeeper.sh	1.1	99/01/11 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#	Configuration variables for the runtime environment of the nightly
# build script and other tools for construction and packaging of releases.
# This script is sourced by 'nightly' and 'bldenv' to set up the environment
# for the build. This example is suitable for building a gate, 
# which will contain the resulting packages and archives (builds of the gate
# are done in children and then the resulting archives, packages, and proto
# area are put into the parent for everyone to use). It is based off
# the on28 release. It sets NIGHTLY_OPTIONS to make nightly do:
#	DEBUG and non-DEBUG builds (-D)
#	GPROF build (-P)
#	TRACE build (-T)
#	creates cpio archives for bfu (-a)
#	creates packages for PIT/RE (-p)
#	checks for new interfaces in libraries (-A)
#	runs 'make check' (-C)
#	runs lint in usr/src (-l plus the LINTDIRS variable)
#	sends mail on completion (-m and the MAILTO variable)
#	updates the protolist in the parent for children to compare with (-u)
#	updates the proto area in the parent when done (-U)
#	checks for changes in ELF runpaths (-r)
#
NIGHTLY_OPTIONS="-aAPTDClmpuUr";		 export NIGHTLY_OPTIONS

# This is a variable for the rest of the script - GATE doesn't matter to
# nightly itself
GATE=on28-gate;					export GATE

# CODEMGR_WS - where is your workspace at (or what should nightly name it)
# there is only one definition here, which assumes all the gate build machines
# (sparc and x86) are set up the same. But remember, this is a script, so
# you _could_ look at $MACH or `uname -n` and set these variables differently.
CODEMGR_WS="/builds/$GATE";		export CODEMGR_WS

# PARENT_WS is used to determine the parent of this workspace. This is
# for the options that deal with the parent workspace (such as where the
# proto area will go).
PARENT_WS="/ws/$GATE";				export PARENT_WS

# CLONE_WS is the workspace nightly should do a bringover from. Since it's
# going to bringover usr/src, this could take a while, so we use the
# clone instead of the gate (see the gate's README).
CLONE_WS="/ws/on28-clone";			export CLONE_WS

# The bringover, if any, is done as STAFFER.
# Set STAFFER to your own login as gate keeper or integration engineer.
# The point is to use group "staff" and avoid referencing the parent
# workspace as root.
# Some scripts optionally send mail messages to MAILTO.
#
STAFFER=mike_s;				export STAFFER
MAILTO=gk@yavin;			export MAILTO

# You should not need to change the next four lines
LOCKNAME="`basename $CODEMGR_WS`_nightly.lock"; export LOCKNAME
ATLOG="$CODEMGR_WS/log";			export ATLOG
LOGFILE="$ATLOG/nightly.log";           	export LOGFILE
MACH=`uname -p`;				export MACH

# REF_PROTO_LIST - for comparing the list of stuff in your proto area
# with. Generally this should be left alone, since you want to see differences
# between todays build and yesterdays.
#
REF_PROTO_LIST=$PARENT_WS/usr/src/proto_list_${MACH}; export REF_PROTO_LIST

# where cpio archives of the OS are placed. Usually this should be left
# alone too. Here they don't go in the build workspace, but in the parent.
# Since this is done as root, the build machine needs root acces to
# the parent over NFS.
CPIODIR="${PARENT_WS}/archives/${MACH}/nightly";	export CPIODIR

#
#	build environment variables, including version info for mcs, motd,
# motd, uname and boot messages. Mostly you shouldn't change this except
# when the release slips (nah) or when starting a new release.
#
ROOT="$CODEMGR_WS/proto/root_${MACH}";	export ROOT
SRC="$CODEMGR_WS/usr/src";         	export SRC
RELEASE="5.8";				export RELEASE
VERSION="$GATE:`date '+%m/%d/%Y'`";	export VERSION
RELEASE_DATE="April 2000";		export RELEASE_DATE

# proto area in parent for optionally depositing a copy of headers and
# libraries corresponding to the protolibs target
#
PARENT_ROOT=$PARENT_WS/proto/root_$MACH; export PARENT_ROOT

#
#       package creation variable. This put the packages in the parent.
#
PKGARCHIVE="${PARENT_WS}/packages/${MACH}/nightly";	export PKGARCHIVE

# we want make to do as much as it can, just in case there's more than
# one problem. This is especially important with the gate, since multiple
# unrelated broken things can be integrated.
MAKEFLAGS=k;	export MAKEFLAGS

# Magic variable to prevent the devpro compilers/teamware from sending
# mail back to devpro on every use.
UT_NO_USAGE_TRACKING="1"; export UT_NO_USAGE_TRACKING

# Compiler root - don't set this unless you know what you're doing,
# since now the Makefiles hardcode the compiler location. This variable
# allows you to get the compilers locally or through cachefs.
#SPRO_ROOT=/opt/SUNWspro.40;	export SPRO_ROOT;

# This goes along with lint - it is a series of the form "A [y|n]" which
# means "go to directory A and run 'make lint'" Then mail me (y) the
# difference in the lint output. 'y' should only be used if the area you're
# linting is actually lint clean or you'll get lots of mail.
LINTDIRS="$SRC y";  export LINTDIRS

# These are for detecting interface changes (-A). You should
# probably leave these alone.
IROOT=$CODEMGR_WS/interface;		export IROOT
IBUILD=$CODEMGR_WS/usr/interface;	export IBUILD

# For finding the build tool 'stabs.' This is used to build the kernel,
# but the default is to get it from /ws/XXX-tools for developers. We
# usually want everything local.
STABS=/opt/onbld/bin/sparc/stabs;       export STABS
