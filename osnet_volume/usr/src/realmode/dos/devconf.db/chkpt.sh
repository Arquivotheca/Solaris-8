#!/bin/sh
#
# Copyright (c) 1994 Sun Microsystems, Inc. All rights reserved.
#
# @(#)chkpt.sh 1.8 99/03/08 SMI
#
# Create freezepoint and workspace listings for a given DCB release.
# Also, patch strap.rc with the appropriate release info.

percent=%

SCRIPT=`basename ${0}`

usage() {
echo "\
Usage:  ${SCRIPT} [ -d <proto_dir> ]

	proto_dir   The proto directory created by executing the disks
                    script in update mode for a particular DCB release
                    (as indicated by RELEASE in the database file).
                    Contains all realmode drivers, realmode bootstraps,
                    copyright and ident files for the release. The strap.rc
		    file in this location will be patched with relevant
                    information pertaining to the RELEASE.
"
	exit 1
}

unset d_FLAG

. `pwd`/dcb.env

if [ ! "$d_FLAG" ] ; then
    pd=$BOOT_DIR/proto_dcb/$RELEASE
fi

VOLNAMETAG=${VOLNAMETAG:=d}

[ ! -d $pd -o ! -f $pd/${VOLNAMETAG}1/strap.com ] && usage

WHAT=$RELEASE ; FULL_WHAT="$RELEASE DCB";

ident_string=`echo "#ident \"$percent"Z"$percent$percent"M"$percent $percent"I"$percent $percent"E"$percent" SMI - $FULL_WHAT\"`

export WHAT FULL_WHAT

if [ -d ../Codemgr_wsdata ] ; then
    type freezept | egrep -s "not found" 2>/dev/null && \
	( echo "freezept command is not in PATH"; exit ;)

    # Checkpt tool accessible.
    echo "Create freezepoint file and complete workspace listing ..."
    ( cd .. ;
	freezept create -w `pwd` \
		       -k devconf.db/chkpt_$WHAT \
		       -c "$ident_string" \
		       $SRC_DIRS_FILES | \
	    grep freezepoint | \
	    awk '{ print $2 }' > devconf.db/"$WHAT"_clear_list 2>&1 ;
#
# XXX Need some work here to capture boot.bin workspace info ???
#
	    find Codemgr_wsdata -depth -print | \
		egrep -v "backup/" | \
		sort -o devconf.db/"$WHAT"_ws_list ;
    )
    sed -n -e 's=^\(.*\)/\([^/]*\)$=\1/SCCS/s\.\2=p' \
	   -e 's=^\([^/]*\)$=SCCS/s\.\1=p' \
	"$WHAT"_clear_list > "$WHAT"_sccs_list

    echo "$ident_string" >> "$WHAT"_ws_list
    sort "$WHAT"_ws_list "$WHAT"_clear_list "$WHAT"_sccs_list -o \
	"$WHAT"_ws_list && rm -f "$WHAT"_clear_list "$WHAT"_sccs_list

    mkdir -p SCCS
    # Admin or delta freezepoint data for current DCB release
    echo "Put freezepoint data file for $FULL_WHAT under SCCS control ..."
    if [ -f SCCS/s.checkpts ] ; then
	rm -f checkpts
	get -e -s SCCS/s.checkpts
	mv chkpt_$WHAT checkpts
	delta -y"Freezepoint for $FULL_WHAT" -s SCCS/s.checkpts && \
	    rm -f checkpts
    else
	admin -r1 -y"Freezepoint for $FULL_WHAT" -ichkpt_$WHAT SCCS/s.checkpts > \
	    /dev/null && \
	    rm -f chkpt_$WHAT
    fi

    # Admin or delta ws_list
    echo "Put workspace list for $FULL_WHAT under SCCS control ..."
    if [ -f SCCS/s.ws_list ] ; then
	rm -f ws_list
	get -e -s SCCS/s.ws_list
	mv "$WHAT"_ws_list ws_list
	delta -y"$FULL_WHAT" -s SCCS/s.ws_list && \
	    rm -f ws_list
    else
	admin -r1 -y"$FULL_WHAT" -i"$WHAT"_ws_list SCCS/s.ws_list > /dev/null && \
	    rm -f "$WHAT"_ws_list
    fi

    echo "Retrieve $FULL_WHAT freezepoint data and workspace listing files ..."
    get -s SCCS/s.checkpts SCCS/s.ws_list
fi

# Patch mdboot and mdexec to replace a binary string placeholder
echo "Patch strap.rc with $FULL_WHAT version info ..."
[ -f SCCS/s.checkpts ] && \
    version=`get -g -r SCCS/s.checkpts` ||
    version=`what checkpts | tail -1 | awk '{ print $2 }'`
str_left="Solaris for x86 - $FULL_WHAT"
str_left_cnt=`echo "$str_left\c" | wc -c`
str_right="Version $version"
str_right_cnt=`echo "$str_right\c" | wc -c`
pad_cnt=`expr 80 - $str_left_cnt - $str_right_cnt`
until [ $pad_cnt -eq 0 ] ; do pad="$pad " ; pad_cnt=`expr $pad_cnt - 1` ; done
patch_str=`echo "$str_left$pad$str_right\c"`

cat $pd/${VOLNAMETAG}1/solaris/strap.rc | \
    sed 's/title="\(.Sc.Sp0\.0\)Solaris DCB"/title="\1'"$patch_str"'"/' \
    > /tmp/strap.$$

mv /tmp/strap.$$ $pd/${VOLNAMETAG}1/solaris/strap.rc
