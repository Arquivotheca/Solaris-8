#!/bin/ksh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#pragma ident	"@(#)wx	1.12	98/11/09 SMI" (from bonwick)
#pragma ident	"@(#)wx.sh	1.1	99/01/11 SMI"

# wx -- workspace extensions.  Jeff Bonwick, December 1992.

PATH=/usr/bin:/usr/sbin:/usr/ccs/bin:$PATH
unset CDPATH  # if set "cd" will print the new directory on stdout
              # which screws up wx_eval

fail() {
	echo $*
	exit 1
}

dot=$PWD
workspace=$CODEMGR_WS
test -z "$workspace" && fail "No active workspace"

workspace_basename=`basename $workspace`
wxdir=$workspace/wx
wxtmp=$wxdir/tmp
wsdata=$workspace/Codemgr_wsdata
node=`uname -n`

parent=`tail -1 $wsdata/parent`
if [[ $parent = *:* ]]; then
	parentdir=${parent#*:}
	parentnode=${parent%%:*}
	if [ $parentnode = $node ]; then
		parent=$parentdir
	else
		parent=/net/$parentnode$parentdir
	fi
fi
test -d $parent || fail "parent $parent does not exist"

export workspace parent wxdir file dir filepath

ask() {
	question=$1
	default_answer=$2
	if [ -z "$default_answer" ]; then
		/usr/bin/echo "$question \c"
	else
		/usr/bin/echo "$question [$default_answer]: \c"
	fi
	read answer
	test -z "$answer" && answer="$default_answer"
}

yesno() {
	question="$1"
	answer=bogus
	while [ "$answer" = bogus ]
	do
		ask "$question" y/n
		case "$answer" in
			y|yes)	answer=yes;;
			n|no)	answer=no;;
			*)	answer=bogus;;
		esac
	done
}

ok_to_proceed() {
	yesno "$*"
	if [ "$answer" = no ]; then
		echo "Exiting, no action performed"
		exit 1
	fi
}

wx_usage() {
	cat <<!
Usage:	wx command [args]
	wx init		initialize workspace for wx usage
	wx list		list active files (the ones you're working on)
	wx new		list new active files (files that exist in child only)
	wx update	update the active file list by appending names
			of all currently checked-out files
	wx out		find all checked-out files in workspace
	wx diffs	show sccs diffs for all active files
	wx pdiffs	show diffs against parent's files
	wx edit	[-s]	check out all active files
	wx delget [-s]	check in all active files
	wx prt [-y]	show sccs history for all active files
	wx comments	display check-in comments for active files
	wx bugs		display all bugids in check-in comments
	wx info		show all info about active files
	wx reedit [-s]	run this right after a resolve to make
			all your changes look like a single delta.
			This eliminates the uninteresting leaf deltas
			that arise from resolving conflicts, so your
			putbacks don't contain a bunch of noise about
			every bringover/resolve you did in the interim.
			NOTE: reedit is appropriate for leaf workspaces
			ONLY -- applying reedit to an interior-node
			workspace would delete all children's comments.
	wx cstyle	run cstyle over all active .c and .h files
	wx hdrchk	run hdrchk over all active .h files
	wx makestyle	run makestyle over all active Makefiles
	wx keywords	run keywords over all active files
	wx copyright	make sure there's a correct copyright message
			that contains the current year
	wx nits		run cstyle, hdrchk, copyright, and keywords
			over all files to which they're applicable
			(makestyle is not currently run because it seems
			to be quite broken -- more noise than data)
	wx backup	make backup copies of all active files
	wx apply <cmd>	apply cmd to all active files; for example,
			"wx apply cat" cats every file
	wx eval <cmd>	like apply, but more general.  In fact,
			"wx apply cmd" is implemented internally as
			"wx eval 'cmd \$file'".  When using eval,
			you can refer to \$dir, \$file, \$filepath,
			\$parent, and \$workspace.  For example:
			wx eval 'echo \$dir; sccs prt \$file | more'
			will show the sccs history for each active file,
			preceded by its directory.
	wx grep		search all active files for pattern; equivalent to
			"wx eval 'echo \$filepath; grep pattern \$file'"
	wx egrep	see wx grep
	wx sed		see wx grep
	wx nawk		see wx grep
	wx codereview	generate codereview diffs for all active files
	wx dir		echo the wx directory path (\$workspace/wx)
	wx e <file>	edit the named wx control file, e.g. "wx e active".
			The editor is \$EDITOR if set, else vi.
	wx ea		shorthand for "wx e active"
	wx ws <file>	cat the named workspace control file, i.e.
			\$workspace/Codemgr_wsdata/file
	wx args		shorthand for "wx ws args"
	wx access	shorthand for "wx ws access_control"
!
	exit 1
}

#
# list all active files
#

wx_active() {
	nawk '
	{
		print
		getline
		while (getline) {
			if (length == 0)
				next
		}
	}' $wxdir/active
}

#
# show the comment for $filepath
#

wx_show_comment() {
	nawk '
	{
		filename=$1
		getline
		while (getline) {
			if (length == 0)
				next
			if (filename == target) {
				if ($0 == "NO_COMMENT")
					exit
				found = 1
				print
			}
		}
	}
	END {
		if (found == 0)
			print "NO_COMMENT"
		exit 1 - found
	}' target=$filepath $wxdir/active
	return $?
}

#
# Evaluate a command for all listed files.  This is the basic building
# block for most wx functionality.
#

wx_eval() {
	pre_eval=$*
	/usr/bin/cat -s $wxdir/$command.NOT >$wxtmp/NOT
	for filepath in $file_list
	do
		if egrep '^'"$filepath"'$' $wxtmp/NOT >>/dev/null; then
			echo "$filepath (skipping)"
		else
			cd $workspace
			dir=`dirname $filepath`
			file=`basename $filepath`
			cd $dir
			eval $pre_eval
		fi
	done
}

#
# Initialize a workspace for wx.
#

wx_init() {
	if [ -d $wxtmp ]; then
		echo "This workspace has already been initialized."
		ok_to_proceed 'Do you really want to re-initialize?'
	else
		mkdir -p $wxtmp
	fi
	backup_dir=$HOME/wx.backup/$workspace_basename
	test -d $backup_dir || mkdir -p $backup_dir
	cd $backup_dir
	rm -f $wxdir/backup_dir
	pwd >$wxdir/backup_dir
	echo "Creating list of SCCS directories...this may take a few minutes."
	rm -f $wxdir/sccs_dirs
	cd $workspace
	find usr/src -name SCCS -print | sort >$wxdir/sccs_dirs
	echo "Looking for checked out files (to create active file list)..."
	touch $wxdir/active
	wx_update
	echo
	if [ -s $wxdir/active ]; then
		basedir=$workspace
		file_list=`wx_active`
		echo "Making backup copies of all active files"
		wx_backup
	else
		echo "No files currently checked out"
		echo
	fi
	echo "wx initialization complete"
}

#
# Find all checked out files
#

wx_checked_out() {
	cd $workspace
	x=`ls -t $wsdata/nametable $wxdir/sccs_dirs`
	if [ "`basename $x`" = nametable ]; then
		echo "Workspace nametable changed: sccs_dirs out of date"
		echo "Updating $wxdir/sccs_dirs...this may take a few minutes."
		rm -f $wxdir/sccs_dirs
		find usr/src -name SCCS -print | sort >$wxdir/sccs_dirs
	fi
	cd $workspace
	rm -f $wxtmp/checked_out
	echo `sed -e 's,$,/p.*,' $wxdir/sccs_dirs` | \
		tr \\040 \\012 | \
		grep -v '*' | \
		sed -e 's,SCCS/p.,,' >$wxtmp/checked_out
}

#
# Update the active file list (by appending all checked out files).
#

wx_update() {
	wx_checked_out
	cd $wxdir
	rm -f tmp/files.old tmp/files.new active.old active.new
	wx_active >tmp/files.old
	cat tmp/checked_out tmp/files.old | sort | uniq >tmp/files.new
	touch active.new
	for filepath in `cat tmp/files.new`
	do
		(echo $filepath; echo; wx_show_comment; echo) >>active.new
	done
	mv -f active active.old
	mv -f active.new active
	echo
	echo "New active file list:"
	echo
	cat tmp/files.new
	echo
	echo "Diffs from previous active file list:"
	echo
	diff tmp/files.old tmp/files.new
}

wx_edit() {
	if [ -f SCCS/p.$file ]; then
		echo "$filepath already checked out"
	else
		echo $filepath
		sccs edit $silent $file
	fi
}

wx_delget() {
	if [ -f SCCS/p.$file ]; then
		rm -f $wxtmp/comment
		if wx_show_comment >$wxtmp/comment; then
			echo $filepath
			cat $wxtmp/comment
			if [ -f $parent/$dir/SCCS/s.$file ]; then
				sccs delget $silent -y"`cat $wxtmp/comment`" $file
			else
				rm -f SCCS/s.$file SCCS/p.$file
				sccs create -y"`cat $wxtmp/comment`" $file
				rm -f ,$file
			fi
		else
			echo "No comments registered for $filepath"
			yesno "Invoke $EDITOR to edit $wxdir/active"'?'
			if [ "$answer" = yes ]; then
				$EDITOR $wxdir/active
				wx_delget
			else
				echo "Edit $wxdir/active and try again."
				exit 1
			fi
		fi
	else
		echo "$filepath already checked in"
	fi
}

wx_info() {
	if [ -f SCCS/p.$file ]; then
		echo "$filepath (checked out)"
	else
		echo "$filepath (checked in)"
	fi
	echo "Check-in comment:"
	wx_show_comment
	/usr/bin/echo "Most recent delta: \c"
	sccs prt -y $file
	echo
}

wx_copyright() {
	case $file in
		*.adb)	return;;
		*.fdbg)	return;;
		*.in)	return;;
	esac
	year=`date +%Y`
	copyright='Copyright \(c\).*'$year' by Sun Microsystems, Inc.'
	reserved='All rights reserved.'
	egrep -s ''"$copyright"'' $file ||
		echo "$year copyright missing in $filepath"
	egrep -s ''"$reserved"'' $file ||
		echo "'$reserved' message missing in $filepath"
}

wx_reedit() {
	numkids=`workspace children | wc -l`
	if [ $numkids -gt 0 ]; then
		echo "WARNING: This workspace has children.  The reedit"
		echo "command will coalesce all children's deltas into one,"
		echo "losing all delta comments in the process."
		ok_to_proceed 'Are you sure you want to proceed?'
	fi
	echo
	echo "Making backups of files..."
	wx_backup
	echo "Re-edit beginning..."
	echo
	wx_eval wx_reedit_file
	echo
	echo "Re-edit complete"
	echo
}

wx_reedit_file() {
	if [ ! -f SCCS/p.$file ]; then
		sccs edit $silent $file
	fi
	echo $filepath
	if [ -f $parent/$dir/SCCS/s.$file ]; then
		rm -f $wxtmp/s.$file
		cp -p $parent/$dir/SCCS/s.$file $wxtmp/s.$file
		newer=`ls -t $wxtmp/s.$file SCCS/s.$file | head -1`
		if [ "$newer" != "SCCS/s.$file" ]; then
			echo "reedit: skipping $filepath because:"
			echo "parent's version of $filepath"
			echo "is newer than child's -- bringover required."
			return
		fi
		mv -f $file ${file}.wx_reedit
		rm -f SCCS/s.$file SCCS/p.$file
		cp $wxtmp/s.$file SCCS/s.$file
		sccs edit $silent $file
		mv -f ${file}.wx_reedit $file
		touch $file
	else
		rm -f SCCS/s.$file SCCS/p.$file
		sccs create $file
		rm -f ,$file
		sccs edit $silent $file
	fi
}

wx_cstyle() {
	case $file in
		*.[ch])	;;
		*)	return;;
	esac
	((CSTYLE_INDEX = CSTYLE_INDEX + 1))
	(cd $workspace; cstyle -P -p $args $filepath >$wxtmp/wx.cstyle.$CSTYLE_INDEX) &
}

wx_backup() {
	backup_dir=`cat $wxdir/backup_dir`
	cd $backup_dir
	version=0
	while [ -f $version.clear.tar -o -f $version.sdot.tar ]
	do
		version=`expr $version + 1`
	done

	backup_file="$backup_dir/$version.clear.tar"
	echo
	echo "Saving clear files to $backup_file"
	echo
	cd $workspace
	tar cvf $backup_file `wx_eval 'echo $filepath'`

	backup_file="$backup_dir/$version.sdot.tar"
	echo
	echo "Saving sdot files to $backup_file"
	echo
	cd $workspace
	tar cvf $backup_file `wx_eval 'echo $dir/SCCS/s.$file'`

	backup_file="$backup_dir/$version.active"
	echo
	echo "Saving active file list to $backup_file"
	echo
	cp $wxdir/active $backup_file
}

wx_restore() {
	backup_dir=`cat $wxdir/backup_dir`
	cd $backup_dir
	version=0
	while [ -f $version.clear.tar -o -f $version.sdot.tar ]
	do
		version=`expr $version + 1`
	done
	version=`expr $version - 1`
	ask 'Version to restore from' $version
	version=$answer
	backup_file="$backup_dir/$version.clear.tar"
	test -s $backup_file || fail "$backup_file: no such file"
	echo
	echo "Restoring clear files from $backup_file"
	echo
	cd $workspace
	tar xvpf $backup_file
	backup_file="$backup_dir/$version.sdot.tar"
	test -s $backup_file || fail "$backup_file: no such file"
	echo
	echo "Restoring sdot files from $backup_file"
	echo
	cd $workspace
	tar xvpf $backup_file
	echo
}

#
# main section
#

test $# -lt 1 && wx_usage
command=$1
comlist=$command
shift
case "$command" in
	apply|eval)		subcommand=$1; shift;;
	grep|egrep|sed|nawk)	pattern=$1; shift;;
	nits)			comlist="cstyle hdrchk copyright keywords";;
esac
orig_args="$*"
silent=
args=
file_list=

while [ $# -gt 0 ]; do
	case $1 in
		-s)	silent=-s;;
		-*)	args="$args $1";;
		*)	file_list="$file_list $1";;
	esac
	shift
done

if [ "$command" = init ]; then
	wx_init
	exit 0
fi

if [ ! -d $wxdir/tmp ]; then
	echo "Workspace does not appear to be initialized for wx."
	echo "The initialization process will create a few files under"
	echo "$wxdir but will not otherwise affect your workspace."
	ok_to_proceed 'OK to proceed?'
	wx_init
	exit 0
fi

if [ -z "$file_list" ]; then
	basedir=$workspace
	file_list=`wx_active`
else
	basedir=$dot
	base_file_list=$file_list
	file_list=
	for basefile in $base_file_list
	do
		abspath=$basedir/$basefile
		filepath=${abspath##$workspace/}
		file_list="$file_list $filepath"
	done
fi

for command in $comlist
do
cd $dot
case "$command" in
	init)	wx_init;;
	list)	wx_active;;
	new)	wx_eval 'test -f $parent/$filepath || echo $filepath';;
	update)	wx_update;;
	out)	wx_checked_out; cat $wxtmp/checked_out;;
	diffs)	wx_eval 'print -- "\n------- $filepath -------\n";
			sccs diffs $args $file | tail +3';;
	pdiffs)	wx_eval '
		echo $filepath;
		if [ -s $parent/$filepath ]; then
			diff $args $parent/$filepath $workspace/$filepath;
		else
			echo "(Does not exist in parent)";
		fi';;
	pvi)	wx_eval '
		echo $filepath;
		if [ -s $parent/$filepath ]; then
			${EDITOR-vi} $args $parent/$filepath;
		else
			echo "(Does not exist in parent)";
		fi';;
	edit)	wx_eval wx_edit;;
	delget)	wx_eval wx_delget;;
	prt)	wx_eval 'sccs prt $args $file';;
	comments) wx_eval 'echo $filepath; echo; wx_show_comment; echo';;
	bugs)	egrep '^[1234][0-9][0-9][0-9][0-9][0-9][0-9]|^xxxxxxx' \
			$wxdir/active | sort -u;;
	info)	wx_eval wx_info;;
	reedit)	wx_reedit;;
	cstyle)	rm -f $wxtmp/wx.cstyle.*;
		export CSTYLE_INDEX=0;
		wx_eval wx_cstyle;
		wait;
		sort -k 1,1 -k 2,2n $wxtmp/wx.cstyle.*
		;;
	hdrchk)	cd $workspace; hdrchk $args `wx_active | egrep '\.h$'`;;
	makestyle) cd $workspace; mlist=`wx_active | egrep '[Mm]akefile'`;
		test -n "$mlist" && makestyle $args $mlist;;
	keywords) cd $workspace; keywords $args $file_list;;
	copyright) wx_eval wx_copyright;;
	backup)	wx_backup;;
	restore)	wx_restore;;
	apply)	wx_eval "$subcommand \$file";;
	eval)	wx_eval "$subcommand";;
	grep|egrep|nawk|sed)
		wx_eval 'echo $filepath; $command $args '\'$pattern\'' $file';;
	codereview) wx_eval \
		'codereview $args $parent/$filepath $workspace/$filepath';;
	dir)	echo $wxdir;;
	e)	cd $wxdir; exec ${EDITOR-vi} $orig_args;;
	ea)	cd $wxdir; exec ${EDITOR-vi} active;;
	ws)	cd $wsdata; cat $orig_args;;
	args)	cat $wsdata/args;;
	access)	cat $wsdata/access_control;;
	*)	wx_usage;;
esac
done
