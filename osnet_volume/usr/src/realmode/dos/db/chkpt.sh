#!/bin/sh
#
# Copyright (c) 1995 Sun Microsystems, Inc. All rights reserved.
#
# @(#)chkpt.sh 1.18 95/04/28 SMI
#
# Create checkpoint and workspace listings for a given MDB or DU release.
# Also, patch bootstrap binaries with the appropriate release info.

percent=%

SCRIPT=`basename ${0}`

usage() {
echo "\
Usage:  ${SCRIPT} [ -d <proto_dir> ] du|mdb

	proto_dir   The proto directory created by executing the mdb or
                    drvud scripts in update mode for a particular MDB or 
		    Driver Update release (as indicated by RELEASE or
		    DU_NUM, respectively, in the database file).  Contains
                    all realmode drivers, realmode bootstraps, copyright 
		    and ident files for the release. The mdboot and mdexec
		    bootstrap binaries in this location will be patched
		    with relevant information pertaining to the RELEASE
		    or DU_NUM. E.g. $BOOT_DIR/proto_mdb/$RELEASE,
		    $BOOT_DIR/proto_du/$DU_NUM.

	du          Generate checkpoint information and a complete workspace
		    listing for a Driver Update delivery.

	mdb         Generate checkpoint information and a complete workspace
		    listing for a Multiple Device Boot (MDB) delivery.
"
	exit 1
}

. ./database

unset d_FLAG

while getopts d: FLAG ; do
    case $FLAG in d)  d_FLAG=1
		      pd=$OPTARG
		      ;;
		 \? ) usage
		      ;;
    esac
done

shift `expr $OPTIND - 1`

if [ ! "$d_FLAG" ] ; then
    [ $1 = du ] && pd=$BOOT_DIR/proto_du/$DU_NUM
    [ $1 = mdb ] && pd=$BOOT_DIR/proto_mdb/$RELEASE
fi

[ ! -d $pd -o ! -f $pd/mdboot -o ! -f $pd/mdexec -o ! "$1" ] && usage

case $1 in    du) WHAT=$DU_NUM ; FULL_WHAT="Driver Update $DU_NUM" ;;
	     mdb) WHAT=$RELEASE ; FULL_WHAT="$RELEASE MDB" ;;
	      \?) usage ;;
esac

ident_string=`echo "#ident \"$percent"Z"$percent$percent"M"$percent $percent"I"$percent $percent"E"$percent" SMI - $FULL_WHAT\"`

export WHAT FULL_WHAT

# create checkpoint file and complete workspace listing

if [ -d ../Codemgr_wsdata ] ; then
# determine whether to use checkpt or freezept command
    if type checkpt 1>/dev/null
    then ckpttool=checkpt		# command name in 2.x TW
         ckptlongname=checkpoint	# name that appears in tool output
    else if type freezept 1>/dev/null
         then ckpttool=freezept		# checkpt became freezept in 3.x TW
              ckptlongname=freezepoint	# name that appears in tool output
         else echo "checkpt or freezept command not in PATH"
              exit 1
         fi
    fi
    echo "using $ckpttool to create checkpoint file and workspace listing..."
    ( cd .. ;
	$ckpttool create -w `pwd` \
		       -k db/chkpt_$WHAT \
		       -c "$ident_string" \
		       $SRC_DIRS_FILES | \
	    grep $ckptlongname | \
	    awk '{ print $2 }' > db/"$WHAT"_clear_list 2>&1 ;
	find Codemgr_wsdata -depth -print | \
	    egrep -v "backup/" | \
	    sort -o db/"$WHAT"_ws_list ;
    )
    sed -n -e 's=^\(.*\)/\([^/]*\)$=\1/SCCS/s\.\2=p' \
	   -e 's=^\([^/]*\)$=SCCS/s\.\1=p' \
	"$WHAT"_clear_list > "$WHAT"_sccs_list

    echo "$ident_string" >> "$WHAT"_ws_list
    sort "$WHAT"_ws_list "$WHAT"_clear_list "$WHAT"_sccs_list -o \
	"$WHAT"_ws_list && rm -f "$WHAT"_clear_list "$WHAT"_sccs_list

    mkdir -p SCCS
    # Admin or delta checkpoint data for current MDB or DU release
    echo "Put checkpoint data file for $FULL_WHAT under SCCS control ..."
    if [ -f SCCS/s.checkpts ] ; then
	rm -f checkpts
	get -e -s SCCS/s.checkpts
	mv chkpt_$WHAT checkpts
	delta -y"Checkpoint for $FULL_WHAT" -s SCCS/s.checkpts && \
	    rm -f checkpts
    else
	admin -r1 -y"Checkpoint for $FULL_WHAT" -ichkpt_$WHAT SCCS/s.checkpts > \
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

    echo "Retrieve $FULL_WHAT checkpoint data and workspace listing files ..."
    get -s SCCS/s.checkpts SCCS/s.ws_list
fi

# Patch mdboot and mdexec to replace a binary string placeholder
echo "Patch mdboot and mdexec with $FULL_WHAT version info ..."
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
for i in $DOS_BOOTSTRAPS ; do
    cp $PROTO_DOS/$i $pd/$i
    patch_bootstrap $pd/$i "$patch_str"
done
