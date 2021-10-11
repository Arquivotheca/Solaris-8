#!/bin/ksh
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)bfu.sh	1.38	99/11/10 SMI"
#ident	"@(#)bfu	1.128	99/01/04 SMI"
#
# Upgrade a machine from a cpio archive area in about 5 minutes.
# By Roger Faulkner and Jeff Bonwick, April 1993.
# (bfu == Bonwick/Faulkner Upgrade, a.k.a. Blindingly Fast Upgrade)
#
# Usage: bfu    <archive_dir> [root-dir]	# for normal machines
#	 bfu -c <archive_dir> <exec-dir>	# for diskless clients
#
# You have to be super-user.  It's safest to run this from the
# system console, although I've run it under OW and even via
# remote login with no problems.
#
# You will have to reboot the system when the upgrade is complete.
#
# You should add any administrative files you care about to this list.
# Warning: there had better be no leading '/' on any of these filenames.

if [ -z "$GATEPATH" ]; then
	GATEPATH=/ws/on28-gate
	test -d $GATEPATH || GATEPATH=/net/on28.eng/on28-gate
fi
export GATE=${GATEPATH}
export ARCHIVE=${ARCHIVEPATH-$GATE}
export PSFILES=$GATE/public/console.1998.026.cpio

files="
	boot/solaris/devicedb/master
	etc/.login
	etc/acct/holidays
	etc/apache/magic
	etc/apache/mime.types
	etc/asppp.cf
	etc/user_attr
	etc/auto_*
	etc/bootrc
	etc/cron.d/at.deny
	etc/cron.d/cron.deny
	etc/default/*
	etc/devlink.tab
	etc/dfs/dfstab
	etc/driver_aliases
	etc/driver_classes
	etc/dumpdates
	etc/group
	etc/gss/gsscred.conf
	etc/gss/mech
	etc/gss/qop
	etc/inet/*
	etc/init.d/*
	etc/inittab
	etc/iu.ap
	etc/krb5/krb5.conf
	etc/krb5/warn.conf
	etc/logindevperm
	etc/lp/Systems
	etc/mach
	etc/mail/aliases
	etc/mail/*.cf
	etc/mail/*.hf
	etc/mail/*.rc
	etc/minor_perm
	etc/name_to_sysnum
	etc/name_to_major
	etc/nca/*
	etc/net/*/*
	etc/netconfig
	etc/nfs/nfslog.conf
	etc/nfssec.conf
	etc/nscd.conf
	etc/nsswitch.*
	etc/openwin/server/etc/OWconfig
	etc/pam.conf
	etc/passwd
	etc/path_to_inst
	etc/policy.conf
	etc/power.conf
	etc/printers.conf
	etc/profile
	etc/publickey
	etc/remote
	etc/resolv.conf
	etc/rmmount.conf
	etc/rpc
	etc/rpld.conf
	etc/saf/_sactab
	etc/saf/_sysconfig
	etc/saf/zsmon/_pmtab
	etc/security/*_attr
	etc/security/audit_*
	etc/shadow
	etc/skel/.profile
	etc/skel/local.*
	etc/syslog.conf
	etc/system
	etc/ttydefs
	etc/ttysrch
	etc/uucp/[A-Z]*
	etc/vfstab
	etc/vold.conf
	kernel/drv/ra.conf
	kernel/misc/sysinit
	platform/i86pc/kernel/drv/aha.conf
	platform/i86pc/kernel/drv/asy.conf
	platform/sun4u/boot.conf
	usr/kernel/drv/audiocs.conf
	usr/openwin/server/etc/OWconfig
	var/apache/htdocs/index.html
	var/apache/logs/*
	var/spool/cron/crontabs/*
	var/yp/aliases
	var/yp/nicknames
"

#
# files to be preserved, ie unconditionally restored to "child" versions
#
preserve_files="
	kernel/misc/sysinit
"

filelist() {
	find $files -depth -type f ! -name core -print 2>/dev/null | sort ||
	    fail "sort failed"
}

#
# Make a local copy of bfu in /tmp and execute that instead.
# This makes us immune to loss of networking and/or changes
# to the original copy that might occur during execution.
#
cd .
abspath=`[[ $0 = /* ]] && print $0 || print $PWD/$0`
if [[ $abspath != /tmp/* ]]; then
	localpath=/tmp/bfu.$$
	print "Copying $abspath to $localpath"
	cp $abspath $localpath
	print "Executing $localpath $*\n"
	exec $localpath $*
fi

export PATH=/usr/bin:/usr/sbin:/sbin

fail() {
	print "$*"
	print "bfu aborting"
	exit 1
}

if [ "$1" = "-c" ]; then
	diskless=yes
	shift
else
	diskless=no
fi

test $# -ge 1 || fail "usage: bfu archive-dir [root-dir]"

isa=`uname -p`
karch=`uname -m`
plat=`uname -i`

cpiodir=$1

if [ "$cpiodir" = again ]; then
	cpiodir=`nawk '/^bfu.ed from / { print $3; exit }' /etc/motd`
fi

[[ "$cpiodir" = */* ]] || cpiodir=$ARCHIVE/archives/$isa/$1

[[ "$cpiodir" = /* ]] || fail "archive-dir must be an absolute path"

cd $cpiodir
case `echo generic.root*` in
	generic.root)		ZFIX="";	ZCAT="cat";;
	generic.root.gz)	ZFIX=".gz";	ZCAT="gzip -d -c";;
	generic.root.Z)		ZFIX=".Z";	ZCAT="zcat";;
	*) fail "generic.root missing or in unknown compression format";;
esac

if [ $diskless = no ]; then
	root=${2:-/}
	[[ "$root" = /* ]] || fail "root-dir must be an absolute path"
	usrroot=$root
	usr=${usrroot%/}/usr
	rootlist=$root
	if [ $plat != $karch -a -f ${cpiodir}/${plat}.root$ZFIX \
	    -a -f ${cpiodir}/${plat}.usr$ZFIX ]
	then
		allarchs="$karch $plat"

		#
		# PSARC/1999/249 fix
		#
		# Some of the sun4u platforms now have dependencies on each
		# other as a part of this project. We need to detect 
		# these dependencies and extract the dependent archives
		# also. This fix to bfu will work for other platforms also.
		#
		###########################################################
		
		#
		# We need to remove the $usr/platform entry for this platform
		# if it's a link so that the archives get installed correctly.
		#
		cd $usr/platform
		if test -h $plat
		then
			rm -f $plat > /dev/null 2>&1
			
			#
			# The archive for SUNW,Ultra-Enterprise.usr contains the 
			# archives for Enterprise-10000 also. If we are bfu-ing
			# an Enterprise we need to remove Enterprise-10000 in 
			# /usr/platform if it's a link.
			#
			sunfire="SUNW,Ultra-Enterprise"
			starfire="SUNW,Ultra-Enterprise-10000"
			if [ $plat = $sunfire ]
			then
				if test -h $starfire
				then
					rm -f $starfire > /dev/null 2>&1
				fi
			fi
		fi

		#
		# Check if this $plat is dependent on another platform. We do
		# this by checking the archive for this $plat. If it contains
		# a single file that is a link, then it is dependent on the
		# target of the link.
		#

		#
		# Set path to include gzip
		#
		export PATH=$PATH:$GATE/public/bin/$isa

		cd ${cpiodir}/
		num_of_files=`$ZCAT $plat.usr$ZFIX | cpio -icdBtv 2>/dev/null \
			| wc -l`
		target_is_a_link=`$ZCAT $plat.usr$ZFIX | cpio -icdBtv \
			2>/dev/null | cut -c 1`

		if [ $num_of_files = "1" ] && [ $target_is_a_link = "l" ]
		then
			depends_on=`$ZCAT $plat.usr$ZFIX | cpio -icdBtv \
				2>/dev/null | awk -F'>' '{ print $2 }' \
				| awk '{ print $1}'`

			#
			# If the dependency is a symbolic link we need to
			# remove it. 
			#
			cd $usr/platform
			if [ $depends_on != $karch ] && [ -h $depends_on ]
			then
				#
				# Verify that $depends_on platform has an
				# archive.
				if [ -f ${cpiodir}/${depends_on}.usr$ZFIX ]
				then
					rm -rf ./$depends_on > /dev/null 2>&1
				else
					echo "Missing ${depends_on}.usr$ZFIX "\
						"in ${cpiodir}"	
					exit 1
				fi
			fi

			#
			# add the dependency to the list of archives to 
			# install. $karch is already in the list.
			#
			if [ $depends_on != $karch ]
			then
				allarchs="$allarchs $depends_on"
			fi
		fi
		#
		# end of PSARC/1999/249 fix
		#
	else
		allarchs=$karch
	fi

	rootslice=`df -k $root | nawk 'NR > 1 { print $1 }' | sed s/dsk/rdsk/`
	print "Loading $cpiodir on $root"
else
	usrroot=$2
	usr=$2/usr
	[[ "$usr" = /export/exec/* ]] || fail "exec-dir $usrroot sounds bogus"
	case $2 in
	    *sparc*)
		isa=sparc;;
	    *i386*)
		isa=i386;;
	esac
	cd $cpiodir
	test -f generic.root$ZFIX || fail "$cpiodir/generic.root$ZFIX missing"
	allarchs=`ls *.root$ZFIX | grep -v generic.root$ZFIX | \
		sed -e 's/.root.*//'`
	rootlist=""
	for root in /export/root/*
	do
		test -f $root/etc/vfstab &&
			egrep -s $usrroot $root/etc/vfstab &&
			rootlist="$rootlist $root"
	done
	test -n "$rootlist" || fail "no clients to upgrade"
	print "Loading $cpiodir usr archives on:\n\t$usr\n"
	print "Loading $cpiodir root archives on:"
	for root in $rootlist
	do
		print "\t$root"
	done
fi

set `id`
test "$1" = "uid=0(root)" || fail "You must be super-user to run this script."

print "\nCreating bfu execution environment ..."

#
# Save off a few critical libraries and commands, so that bfu will
# continue to function properly even in the face of major
# kernel/library/command incompatibilities during a live upgrade.
# (You can successfully upgrade from Jupiter FCS to the latest stuff!)
#
bfucmd="
	/usr/bin/awk
	/usr/bin/cat
	/usr/bin/chgrp
	/usr/bin/chmod
	/usr/bin/chown
	/usr/sbin/chroot
	/usr/bin/cmp
	/usr/bin/cp
	/usr/bin/cpio
	/usr/bin/csh
	/usr/bin/cut
	/usr/bin/date
	/usr/bin/dd
	/usr/bin/df
	/usr/bin/diff
	/usr/bin/du
	/usr/bin/echo
	/usr/bin/ed
	/usr/bin/egrep
	/usr/bin/env
	/usr/bin/ex
	/usr/bin/expr
	/usr/bin/fgrep
	/usr/bin/file
	/usr/bin/find
	/usr/bin/grep
	/usr/sbin/halt
	/usr/bin/head
	/usr/bin/id
	/usr/bin/ksh
	/usr/sbin/lockfs
	/usr/bin/ln
	/usr/bin/ls
	/usr/bin/mkdir
	/usr/sbin/mknod
	/usr/bin/more
	/usr/bin/mv
	/usr/bin/nawk
	/usr/bin/ps
	/usr/sbin/reboot
	/usr/bin/rm
	/usr/bin/rmdir
	/usr/bin/sed
	/usr/bin/sh
	/usr/bin/sleep
	/usr/bin/sort
	/usr/bin/stty
	/usr/bin/strings
	/usr/bin/su
	/usr/bin/sum
	/usr/sbin/sync
	/usr/bin/tail
	/usr/sbin/tar
	/usr/bin/tee
	/usr/bin/touch
	/usr/bin/tr
	/usr/bin/truss
	/usr/bin/tty
	/usr/sbin/uadmin
	/usr/bin/uname
	/usr/bin/uniq
	/usr/bin/uptime
	/usr/bin/vi
	/usr/bin/w
	/usr/sbin/wall
	/usr/bin/wc
	/usr/bin/xargs
	/usr/bin/zcat
	/usr/proc/bin/ptree
	${FASTFS-$GATE/public/bin/$isa/fastfs}
	${GZIPBIN-$GATE/public/bin/$isa/gzip}
"

#
# shell scripts, not supported by ldd
#
bfushcmd="
	/usr/bin/dirname
"

rm -rf /tmp/bfubin
mkdir /tmp/bfubin
set $bfucmd $bfushcmd
isalist=`isalist`
while [ $# -gt 0 ]
do
	dir=${1%/*}
	cmd=${1##*/}
	cd $dir
	isacmd=`(find $isalist -name $cmd 2>/dev/null; echo $cmd) | head -1`
	cp $dir/$isacmd /tmp/bfubin || fail "cannot copy $dir/$isacmd"
	shift
done

#
# If available, use ldd to determine which libraries bfu depends on.
# Otherwise, just make an educated guess.
#
if [ -x /usr/bin/ldd ]; then
	bfulib="`ldd $bfucmd | nawk '$3 ~ /lib/ { print $3 }' | sort | uniq`"
else
	bfulib="
		/usr/lib/libbsm.so.1
		/usr/lib/libc.so.1
		/usr/lib/libc2.so
		/usr/lib/libdl.so.1
		/usr/lib/libelf.so.1
		/usr/lib/libkstat.so.1
		/usr/lib/libmapmalloc.so.1
		/usr/lib/libmp.so.1
		/usr/lib/libnsl.so.1
		/usr/lib/libpam.so.1
		/usr/lib/libsec.so.1
		/usr/lib/libsocket.so.1
	"
fi

bfulib="$bfulib /usr/lib/nss_*"		# add dlopen()'ed stuff

rm -rf /tmp/bfulib
mkdir /tmp/bfulib
cp $bfulib /tmp/bfulib
cp /usr/lib/ld.so.1 /tmp/bfulib/bf.1	# bfu's private runtime linker
${BFULD-$GATE/public/bin/$isa/bfuld} /tmp/bfubin/*

remove_initd_links()
{
	# If we're delivering a new version of an existing /etc/init.d script,
	# remove all hard links to the existing file in /etc/rc?.d whose
	# names begin with [SK][0-9][0-9].  Additionally, in case an S or K
	# file was previously delivered as a symbolic link or the hard link
	# was broken, remove any file in /etc/rc?.d whose name is
	# [SK][0-9][0-9] followed by the basename of the file we're going
	# to update in /etc/init.d.

	for archive in $*; do
		scripts=`$ZCAT $cpiodir/$archive.root$ZFIX |
			cpio -it 2>/dev/null | grep '^etc/init\.d/'`
		test -z "$scripts" && continue
		inodes=`ls -li $scripts 2>/dev/null | \
			nawk '{ print "-inum " $1 " -o " }'`
		names=`ls -1 $scripts 2>/dev/null | \
			nawk -F/ '{ print "-name [SK][0-9][0-9]" $NF " -o " }'`
		find etc/rc?.d \( $inodes $names -name S00sxcmem \) -print |
			xargs rm -f
	done
}

add_inetd_rpc()
{
	# Add an entry to inetd.conf(4) for the RPC-based service.
	# This function takes the desired entry as an argument list,
	# and adds the entry if the service name is not found.
	# If the service name is already present in the file, and there
	# are any differences in the entry fields, the old entry is
	# removed and the new entry is added in its place.  The first
	# argument (the service name) should be of the form
	# rpc_prog/version; the comparison to existing entries is done
	# using only the rpc_prog portion of the first token.

	rpc_prog=`echo "$1" | cut -d/ -f1`
	conf=$rootprefix/etc/inet/inetd.conf
	tmpf=/tmp/inetd.$$
	append=true

	nawk "\$1 ~ \"$rpc_prog/\"{ print }" $conf | \
	while read svc type proto flags user path args; do

		if [ "$svc" != "$1" -o "$type" != "$2" -o "$proto" != "$3" -o \
		     "$flags" != "$4" -o "$user"  != "$5" -o \
		     "$path"  != "$6" -o "$args"  != "$7" ]; then

			nawk "\$1 !~ \"$rpc_prog/\"{ print }" $conf >$tmpf && \
			    cat $tmpf >$conf
			rm -f $tmpf
		else
			append=false	# No changes needed
		fi
	done

	if $append; then
		line="$1\t$2\t$3\t$4\t$5\t$6"; shift 6
		echo "$line\t$*" >>$conf
	fi
}

# Update to new PSARC 1998/026 console subsystem on Solaris/Intel.
update_console()
{
	if test ! -r $PSFILES
	then
		echo "WARNING:"
		echo "    This archive implements the PSARC 1998/026 console"
		echo "    subsystem and this system predates that project."
		echo "    Updates to non-ON components are necessary, but the"
		echo "    cpio archive of non-ON components"
		echo "        $PSFILES"
		echo "   does not exist."
		echo "   THIS SYSTEM MAY NOT BE USABLE AFTER THIS UPDATE."
		return 1
	fi
	echo "Updating to new PSARC 1998/026 console subsystem ... \c"

	cd $root
	# Update non-ON files
	cpio -iucdm < $PSFILES

	# Move /etc/openwin/server/etc/OWconfig to force a rerun of
	# kdmconfig.
	mv -f etc/openwin/server/etc/OWconfig \
		etc/openwin/server/etc/OWconfig.oldconsole 2>/dev/null

	# I'm going to leave kd and its friends installed.
	# The new name_to_major removes their entries, so they should
	# be unused.  The upside is that I think a "bfu" back to the
	# old system will bring them back to life.  The downside is that
	# if the user mismerges name_to_major they might get run too
	# and cause problems.

	# These aren't required (haven't been since 2.6) and some of the
	# cleanups in the console project exposed a minor bug where having
	# them present will result in a warning message on boot.  Work
	# around the bug by deleting them.
	rm -f platform/i86pc/kernel/drv/isa.conf
	rm -f platform/i86pc/kernel/drv/eisa.conf
	rm -f platform/i86pc/kernel/drv/mc.conf

	# Update "master" as required.
	# Also delete MicroChannel entries, because the bootconf.exe supplied
	# can't handle them any more.
	awk '
		$1=="#" { print; next; }
		$2=="chanmux" { next; }
		$2=="kdmouse" { next; }
		$2=="kd" { next; }
		$4=="mca" { next; }
		{ print; }'			\
		boot/solaris/devicedb/master	\
		> /tmp/master.$$
	cat boot/solaris/devicedb/master.newconsole >> /tmp/master.$$
	cp /tmp/master.$$ boot/solaris/devicedb/master
	rm -f boot/solaris/devicedb/master.newconsole

	return 0
}

# Back out PSARC 1998/026 console subsystem on Solaris/Intel, as for
# "backwards" BFU.
backout_console()
{
	if test ! -r $PSFILES.backout
	then
		echo "WARNING:"
		echo "    This archive predates the PSARC 1998/026 console"
		echo "    subsystem and this system includes that project."
		echo "    Backout of non-ON components is necessary, but the"
		echo "    cpio archive of non-ON components"
		echo "        $PSFILES.backout"
		echo "   does not exist."
		echo "   THIS SYSTEM MAY NOT BE USABLE AFTER THIS UPDATE."
		return 1
	fi
	echo "Backing out PSARC 1998/026 console subsystem ... \c"

	cd $root
	# Back out non-ON files.  The restored versions might not match
	# the version of ON being installed; caveat emptor.
	cpio -iucdm < $PSFILES.backout

	# Move /etc/openwin/server/etc/OWconfig to force a rerun of
	# kdmconfig.
	mv -f etc/openwin/server/etc/OWconfig \
		etc/openwin/server/etc/OWconfig.newconsole 2>/dev/null

	# Backout "master" changes as required.
	# Don't bother putting back MicroChannel entries; we can't figure
	# out whether we should, and MCA is dead anyway.
	awk '
		$1=="#" { print; next; }
		$2=="display" { next; }
		$2=="mouse" { next; }
		$2=="keyboard" { next; }
		{ print; }'			\
		boot/solaris/devicedb/master	\
		> /tmp/master.$$
	cat boot/solaris/devicedb/master.oldconsole >> /tmp/master.$$
	cp /tmp/master.$$ boot/solaris/devicedb/master
	rm -f boot/solaris/devicedb/master.oldconsole

	# Remove /dev/kbd so that a subsequent BFU will know that this is
	# an "old" system.
	rm -f dev/kbd

	return 0
}

# pci-ide nodes now use "ide" as their node name, not "ata". If we're
# crossing that transition, update /devices, /dev, and /etc/path_to_inst.
update_ata2ide() {
	# On a machine with pci-ide nodes, rename the "ata" portion of the
	# path name to be "ide", e.g., /devices/pci@0,0/pci-ide@7,1/ata@0 ->
	# /devices/pci@0,0/pci-ide@7,1/ide@0.
	atanodes=`find $rootprefix/devices/pci* -type d -name "ata@[01]" \
			-print 2>/dev/null`

	# If the machine doesn't have pci-ide "ata" nodes, exit the function.
	if [ -z "$atanodes" ]
	then
		return 1
	fi

	ata0node=`find $rootprefix/devices/pci* -type d -name "ata@0" -print`
	ata1node=`find $rootprefix/devices/pci* -type d -name "ata@1" -print`

	if [ -d "$ata0node" ]
	then
		cd `dirname $ata0node`
		mv ata@0 ide@0
	fi
	if [ -d "$ata1node" ]
	then
		cd `dirname $ata1node`
		mv ata@1 ide@1
	fi

	# Now that the device nodes have been renamed to use ide@[01] instead
	# of ata@[01], update the symbolic links in /dev/dsk and /dev/rdsk to
	# point to the newly-named device nodes.

	cd $rootprefix/dev/dsk

	ls -l | grep "\/ata@" | nawk '{ print "rm -f " $9 }' >/tmp/dsk.remove

	ls -l | grep "\/ata@" | sed 's/\/ata@/\/ide@/' \
		| nawk '{ print "ln -s " $11 " " $9 }' >/tmp/dsk.rename

	sh /tmp/dsk.remove
	sh /tmp/dsk.rename

	cd $rootprefix/dev/rdsk

	ls -l | grep "\/ata@" | nawk '{ print "rm -f " $9 }' >/tmp/rdsk.remove

	ls -l | grep "\/ata@" | sed 's/\/ata@/\/ide@/' \
		| nawk '{ print "ln -s " $11 " " $9 }' >/tmp/rdsk.rename

	sh /tmp/rdsk.remove
	sh /tmp/rdsk.rename

	rm -f /tmp/dsk.rename /tmp/rdsk.rename /tmp/dsk.remove /tmp/rdsk.remove

	# Finally, update the /etc/path_to_inst file to change the device
	# node names to use "ide" instead of "ata".

	cd $rootprefix/etc

	sed 's/\/ata@/\/ide@/' path_to_inst >/tmp/pti
	cp -f /tmp/pti path_to_inst
	chmod 0444 path_to_inst
	rm -f /tmp/pti

	return 0
}

# pci-ide nodes now use "ata" as their node name, not "ide". If we're
# crossing that transition, update /devices, /dev, and /etc/path_to_inst.
backout_ata2ide() {
	# On a machine with pci-ide nodes, rename the "ide" portion of the
	# path name to be "ata", e.g., /devices/pci@0,0/pci-ide@7,1/ide@0 ->
	# /devices/pci@0,0/pci-ide@7,1/ata@0.
	idenodes=`find $rootprefix/devices/pci* -type d -name "ide@[01]" \
			-print 2>/dev/null`

	# If the machine doesn't have pci-ide "ide" nodes, exit the function.
	if [ -z "$idenodes" ]
	then
		return 1
	fi

	ide0node=`find $rootprefix/devices/pci* -type d -name "ide@0" -print`
	ide1node=`find $rootprefix/devices/pci* -type d -name "ide@1" -print`

	if [ -d "$ide0node" ]
	then
		cd `dirname $ide0node`
		mv ide@0 ata@0
	fi
	if [ -d "$ide1node" ]
	then
		cd `dirname $ide1node`
		mv ide@1 ata@1
	fi

	# Now that the device nodes have been renamed to use ata@[01] instead
	# of ide@[01], update the symbolic links in /dev/dsk and /dev/rdsk to
	# point to the newly-named device nodes.

	cd $rootprefix/dev/dsk

	ls -l | grep "\/ide@" | nawk '{ print "rm -f " $9 }' >/tmp/dsk.remove

	ls -l | grep "\/ide@" | sed 's/\/ide@/\/ata@/' \
		| nawk '{ print "ln -s " $11 " " $9 }' >/tmp/dsk.rename

	sh /tmp/dsk.remove
	sh /tmp/dsk.rename

	cd $rootprefix/dev/rdsk

	ls -l | grep "\/ide@" | nawk '{ print "rm -f " $9 }' >/tmp/rdsk.remove

	ls -l | grep "\/ide@" | sed 's/\/ide@/\/ata@/' \
		| nawk '{ print "ln -s " $11 " " $9 }' >/tmp/rdsk.rename

	sh /tmp/rdsk.remove
	sh /tmp/rdsk.rename

	rm -f /tmp/dsk.rename /tmp/rdsk.rename /tmp/dsk.remove /tmp/rdsk.remove

	# Finally, update the /etc/path_to_inst file to change the device
	# node names to use "ata" instead of "ide".

	cd $rootprefix/etc

	sed 's/\/ide@/\/ata@/' path_to_inst >/tmp/pti
	cp -f /tmp/pti path_to_inst
	chmod 0444 path_to_inst
	rm -f /tmp/pti

	return 0
}

print "Verifying archives ..."

for a in generic $allarchs
do
	test -r $cpiodir/$a.usr$ZFIX ||
		fail "bfu archive $cpiodir/$a.usr$ZFIX missing"
	test -r $cpiodir/$a.root$ZFIX ||
		fail "bfu archive $cpiodir/$a.root$ZFIX missing"
done

for root in $rootlist
do
	cd $root || fail "Cannot cd $root"
	prologue=${root%/}/bfu.prologue
	if [ -f $prologue ]; then
		print "Executing $prologue"
		$prologue || fail "$prologue failed with code $?"
	fi
done

print "Performing basic sanity checks ..."

for dir in $usr $rootlist
do
	test -d $dir || fail "$dir does not exist"
	test -w $dir || fail "$dir is not writable"
	cd $dir || fail "Cannot cd $dir"
done

#
# Perform additional sanity checks if we are upgrading the live system.
#
if [ "$rootlist" = "/" ]
then
	#
	# Filesystem space checks
	#
	set $root 4 $usr 6
	while [ $# -gt 0 ]
	do
		test "`df -b $1 | tail -1 | nawk '{ print $2 }'`" -ge ${2}000 ||
			fail "Less than $2 MB free on $1 -- bfu not safe."
		shift 2
	done
	#
	# Disable kernel module unloading
	#
	print "Disabling kernel module unloading ... \c"
	test -x /usr/bin/adb || fail "/usr/bin/adb not found: bfu not safe."
	echo "moddebug/W10" | adb -kw /dev/ksyms /dev/mem | grep moddebug
	ls $cpiodir >>/dev/null		# loads elfexec and networking
	#
	# Save and restore system-maintained files.
	# This allows 'who' and 'df' to work after the upgrade is finished.
	#
	live_files="
		var/adm/utmpx
		var/adm/wtmpx
		var/adm/spellhist
		var/adm/aculog
		var/log/syslog
		var/log/authlog
		var/saf/zsmon/log
	"
	rm -rf /tmp/live_files.cpio
	ls $live_files 2>/dev/null | cpio -ocB >/tmp/live_files.cpio 2>/dev/null

	#
	# Kill off sendmail so that (1) mail doesn't bounce during the
	# interval where /etc/aliases is (effectively) empty, and (2)
	# the alias database doesn't get rebuilt against the empty
	# /etc/aliases file due to incoming mail.  If that happens,
	# then since bfu preserves modification time when it restores
	# the real /etc/aliases the database will incorrectly appear
	# to be up-to-date.  To fully nail case (2) bfu also removes
	# the alias database files after restoring configuration files
	# in case another user triggered an alias rebuild by sending
	# mail during the bfu.
	#
	print "Killing sendmail ..."
	/etc/init.d/sendmail stop

	# kill off utmpd and remove utmp & wtmp - they are EOL'd as of
	# s28_18, but killing them here allows them to come back if
	# older archives are bfu'd
	#
	print "Killing utmpd ..."
	/etc/init.d/utmpd stop

	rm -f $rootprefix/var/adm/utmp
	rm -f $rootprefix/etc/utmp
	rm -f $rootprefix/var/adm/wtmp
	rm -f $rootprefix/etc/wtmp

	# remove devctl, as it is not part of the shipping system
	#
	rm -f $usr/sbin/devctl

	print "Disabling remote logins ..."
	echo "bfu in progress -- remote logins disabled" >/etc/nologin
fi

#
# Save contents of /etc/mnttab, allowing possible change
# of /etc/mnttab to symlink to /var/tmp/mnttab or back again.
#
cp -f $rootprefix/etc/mnttab /tmp/live_files.mnttab

export PATH=/tmp/bfubin:$PATH
export LD_LIBRARY_PATH=/tmp/bfulib

print "Turning on delayed i/o ..."
fastfs -f $rootlist $usr
fastfs $rootlist $usr

#
# Usage: extract_archives (root|usr) arch-list
#
extract_archives() {
	base=$1
	shift
	test $base = root && cd $root || cd $usrroot
	for archive in $*
	do
		print "Extracting $archive.$base$ZFIX ... \c"
		test -h platform/$archive && rm platform/$archive
		$ZCAT $cpiodir/$archive.$base$ZFIX |
			cpio -idmucB 2>&1 | egrep -v \
		'"usr/openwin"|"usr/share/man"|"dev/fd"|"home"|"proc"| error.s'
		sleep 2	# ensures that later archives have later timestamps
	done
	cd $root
}

#
# Hacks to work around minor annoyances and make life more pleasant.
# Part 1 of 4: pre-archive-extraction usr stuff
#
rm -f $usr/sbin/dmesg
rm -f $usr/sbin/*/dmesg
find $usr/platform/$karch/lib/adb $usr/lib/adb -name 'msgbuf*' | xargs rm -f

test $diskless = yes && extract_archives usr generic $allarchs

for root in $rootlist
do
	SECONDS=0		# time each iteration

	cd $root || fail "Cannot cd $root"
	rootprefix=${root%/}

	print "\nSaving configuration files in $rootprefix/bfu.child ... \c"
	rm -rf bfu.default bfu.restore	# historical
	rm -rf bfu.child bfu.conflicts
	mkdir bfu.child bfu.conflicts
	filelist | cpio -pdmu bfu.child
	test -f etc/motd && mv etc/motd etc/motd.old

	#
	# bfu'ed systems are not upgradeable; prevent suninstall from
	# even *presenting* the upgrade option by removing INST_RELEASE.
	#
	rm -f var/sadm/system/admin/INST_RELEASE

	#
	# Hacks to work around minor annoyances and make life more pleasant.
	# Part 2 of 4: pre-archive-extraction root stuff
	#
	remove_initd_links generic $archlist
	cd $root
	find platform/*/kernel -name pci | xargs rm -f

	# Remove Sunfire drivers which have moved
	if [ -d platform/sun4u -a -d platform/SUNW,Ultra-Enterprise ]; then
		for pdir in platform/sun4u/kernel/drv \
		    platform/SUNW,Ultra-Enterprise/kernel/drv
		do
			if [ ! -d $pdir ]
			then
				continue
			fi
			find $pdir \( -name ac -o -name sysctrl -o \
			    -name simmstat -o -name fhc -o -name central -o \
			    -name environ -o -name sram \) -print | xargs rm -f
		done
	fi

	#
	# Remove MDB pts module: obsoleted by pty changes.
	# If this is a backwards-BFU, it will be extracted again by cpio.
	#
	cd ${root}/usr/lib
	find mdb/kvm -name pts.so | xargs rm -f
	cd $root
	
	if [ $diskless = no ]; then
		$ZCAT $cpiodir/$karch.usr$ZFIX |
			cpio -idmucB 'usr/platform/'$karch'/lib/fs/ufs/*' \
			2>/dev/null
		case $isa in
		    sparc)
			if [[ "$rootslice" = /dev/rdsk/* ]]; then
				print "Installing boot block."
				cd $usr/platform/$karch/lib/fs/ufs
				installboot ./bootblk $rootslice
			fi
			;;
		    i386)
			NEWPBOOTDIR=$GATE/public/pboot
			NEWPBOOT=${NEWPBOOTDIR}/pboot
			NEWBOOTBLK=${NEWPBOOTDIR}/bootblk
			PBOOTDIR=$usr/platform/$karch/lib/fs/ufs
			PBOOT=${PBOOTDIR}/pboot
			BOOTBLK=${PBOOTDIR}/bootblk

			# they should already be there, but...
			if [ -f $NEWPBOOT -a ! -f $PBOOT ]; then
				print "Installing pboot from $NEWPBOOTDIR"
				cp $NEWPBOOT $PBOOT
			fi
			if [ -f $NEWBOOTBLK -a ! -f $BOOTBLK ]; then
				print "Installing bootblk from $NEWPBOOTDIR"
				cp $NEWBOOTBLK $BOOTBLK
			fi

			if [ -f $NEWPBOOT -a -f $PBOOT ]; then
				LATEST=`ls -Lt $PBOOT $NEWPBOOT | head -1`
				if [ "$LATEST" = "$NEWPBOOT" ]; then
					print "Updating pboot from $NEWPBOOT"
					cp $NEWPBOOT $PBOOT
				fi
			fi
			if [ -f $NEWBOOTBLK -a -f $BOOTBLK ]; then
				LATEST=`ls -Lt $BOOTBLK $NEWBOOTBLK | head -1`
				if [ "$LATEST" = "$NEWBOOTBLK" ]; then
					print "Updating bootblk from $NEWBOOTBLK"
					cp $NEWBOOTBLK $BOOTBLK
				fi
			fi
			if [[ "$rootslice" = /dev/rdsk/* ]]; then
				print "Installing boot block."
				cd $PBOOTDIR
				installboot ./pboot ./bootblk ${rootslice%??}s2
			fi
			#
			# Since /platform/i86pc/boot/solaris/boot.bin is moved
			# to /boot/solaris, remove the old one if it really
			# exists.
			#
			OLDBOOTDIR=${root}/platform/i86pc/boot/solaris
			OLDBOOTBIN=${OLDBOOTDIR}/boot.bin
			if [ ! -h ${OLDBOOTDIR} -a -f ${OLDBOOTBIN} ] ;
			then
				print "Removing old boot.bin."
				rm -rf ${OLDBOOTBIN}
			fi 
			;;
		    *)
			;;	# unknown ISA
		esac
		
	fi

	if [ $diskless = yes ]; then
		node=${root##*/}
		archlist=""
		for arch in $allarchs
		do
			egrep -s '/export/exec/.*'$arch'/usr/kvm' \
				$root/etc/vfstab ||
				test -d $root/platform/$arch &&
				archlist="$archlist $arch"
		done
	else
		export PATH=/tmp/bfubin
		node=`uname -n`
		archlist=$allarchs
		extract_archives usr generic $archlist
	fi

	extract_archives root generic $archlist

	#
	# Restore live_files if we are upgrading the live system.
	#
	test "$root" = "/" && cpio -idmucB </tmp/live_files.cpio 2>/dev/null

	#
	# Restore contents of mnttab, if we have a writeable mnttab
	#
	if [ -w $rootprefix/etc/mnttab ]; then
		cp -f /tmp/live_files.mnttab $rootprefix/etc/mnttab
	fi

	touch reconfigure

	print "Removing duplicate kernel binaries."

	for arch in $archlist
	do
		p=$rootprefix/platform/$arch/kernel
		k=$rootprefix/kernel
		u=$usr/kernel

		test -d $p || continue

		for d in `find $p $k $u -type f -print | sed 's,.*/kernel/,,' |
			sort | uniq -d`
		do
			set `ls -ct $p/$d $k/$d $u/$d 2>/dev/null`
			print rm $2 $3
			rm $2 $3
		done
	done

	# This is here (as opposed to someplace else) so that (a) it can
	# look at the files just installed, and (b) it can install files
	# itself that will then be subject to conflict resolution.
	# Install or uninstall the PSARC 1998/026 console subsystem if
	# necessary.
	cd $root
	if [ $isa = i386 ]; then
		if test -c dev/kbd; then
			existing_console=new
		else
			existing_console=old
		fi
		if grep conskbd etc/name_to_major > /dev/null
		then
			installing_console=new
		else
			installing_console=old
		fi
		case ${existing_console}_to_${installing_console} in
		old_to_old) ;; # No special action required
		old_to_new) update_console ;;
		new_to_old) backout_console ;;
		new_to_new) ;; # No special action required
		esac
	fi

	print "\nRestoring configuration files.\n"

	cd $root
	rm -rf bfu.ancestor
	test -d bfu.parent && mv bfu.parent bfu.ancestor
	mkdir bfu.parent
	filelist | cpio -pdmu bfu.parent 2>/dev/null
	cd bfu.child
	for file in `filelist`
	do
		# parent: freshly-BFUed version
		# child: pre-BFU version
		# ancestor: installed from archives the last time you BFUed
		# actual: in the root filesystem at this moment (same as parent)
		
		parent=$rootprefix/bfu.parent/$file
		child=$rootprefix/bfu.child/$file
		ancestor=$rootprefix/bfu.ancestor/$file
		conflicts=$rootprefix/bfu.conflicts/$file
		actual=$rootprefix/$file

		# if there's been no change by the BFU, skip it
		cmp -s $child $actual && continue

		# if the file was not installed by the BFU, skip it
		test -f $parent || continue

		# if this is a file which should never be updated by BFU,
		# preserve the original (child) version
		if (echo $preserve_files | grep $file >/dev/null 2>&1)
		then
			print "    preserve: $file"
			cp -p $child $actual
			continue
		fi

		# if the file was accepted from the parent on the last BFU,
		# then accept it again this time without argument
		if cmp -s $child $ancestor; then
			print "      update: $file"
			continue
		fi

		# if the BFU'ed file is the same as the beginning of the
		# pre-BFUed file, assume the user has added lines to the
		# end, and restore the pre-BFUed version
		if (cmp $child $parent 2>&1) | egrep -s 'EOF on '$parent; then
			print "     restore: $file"
			cp -p $child $actual
			continue
		fi

		# if the new version is the same as it was the last time
		# BFU was run, but still different than the pre-BFU version,
		# this is an "old" conflict; otherwise, it's a "NEW"
		# conflict.  Old conflicts can usually be safely 
		# ignored.
		if cmp -s $parent $ancestor; then
			print "old \c"
		else
			print "NEW \c"
			print $file >>$rootprefix/bfu.conflicts/NEW
		fi
		print "conflict: $file"
		(cd $root; print $file | cpio -pdmu bfu.conflicts 2>/dev/null)

		# for all conflicts, restore the pre-BFU version and let
		# the user decide what to do.
		cp -p $child $actual
	done

	print "\nFor each file in conflict, your version has been restored."
	print "The new versions are under $rootprefix/bfu.conflicts."
	print "\nMAKE SURE YOU RESOLVE ALL CONFLICTS BEFORE REBOOTING.\n"

	cd $root

	print "bfu'ed from $cpiodir on `date +%m/%d/%y`" >>etc/motd
	tail +`nawk '/bfu.ed from/ { x=NR }; END { print x+1 }' \
		etc/motd.old` etc/motd.old >> etc/motd

	#
	# Hacks to work around minor annoyances and make life more pleasant.
	# Part 3 of 4: post-archive-extraction root stuff
	#
	rm -f var/statmon/state			# to prevent lockd/statd hangs
	rm -f etc/mail/aliases.dir etc/mail/aliases.pag	# force alias rebuild
	for f in etc/auto_*			# to make autofs happy
	do
		file $f | grep executable >/dev/null || chmod -x $f
	done

	#
	# E10K ONLY - Add DR daemon (dr_daemon) inetd.conf entry
	#
	if [ "$plat" = "SUNW,Ultra-Enterprise-10000" ]; then
		add_inetd_rpc 300326/4 tli rpc/tcp wait root \
			/platform/SUNW,Ultra-Enterprise-10000/lib/dr_daemon \
			dr_daemon
	fi

	#
	# Add device nodes which are new to 2.8 but must exist prior to
	# the execution of drvconfig on a reconfiguration boot:
	#
	devnode=$rootprefix/devices/pseudo/sysmsg@0:sysmsg
	if [ ! -c $devnode ]; then
		rm -f $devnode
		mknod $devnode c 97 0
		chown root:sys $devnode
		chmod 0600 $devnode

		devnode=../devices/pseudo/sysmsg@0:sysmsg

		devlink=sysmsg
		( cd $rootprefix/dev ; \
			rm -f $devlink ; ln -s $devnode $devlink )
	fi
	devnode=$rootprefix/devices/pseudo/sysmsg@0:msglog
	if [ ! -c $devnode ]; then
		grep sysmsg $rootprefix/etc/name_to_major >/dev/null 2>&1
		if [ "$?" = "0" ]; then
        		major_sysmsg=`grep sysmsg \
			    $rootprefix/etc/name_to_major | nawk '{ print $2 }'`
		else
			major_sysmsg=97
		fi
		rm -f $devnode
		mknod $devnode c $major_sysmsg 1
		chown root:sys $devnode
		chmod 0600 $devnode

		devnode=../devices/pseudo/sysmsg@0:msglog

		devlink=msglog
		( cd $rootprefix/dev ; \
			rm -f $devlink ; ln -s $devnode $devlink )
	fi

	# Remove /etc/inet/ipsecinit.conf if it matches the default one
	# in build 14.
	if [ -f $rootprefix/etc/inet/ipsecinit.conf ]; then
		# NOTE:  If by some chance one has an ipsecinit.conf
		#	 file that has the same checksum as the old default
		#	 one, then you're hosed.
		csum=`sum $rootprefix/etc/inet/ipsecinit.conf | \
			awk '{print $1}'`
		if [ $csum = 39159 -o $csum = 39158 ]; then
			rm -f $rootprefix/etc/inet/ipsecinit.conf
		fi
	fi

	#
	# pci-ide nodes now use "ide" as their node name, not "ata".
	# If we're crossing that transition, update /devices, /dev,
	# and /etc/path_to_inst.
	#
	if [ $isa = i386 ]; then
		cd $root
		if ls -l dev/dsk | grep '/pci-ide@.*/ata@' >/dev/null
		then
			existing=old
		else
			existing=new
		fi
		if grep 'parent="pci-ide"' \
			platform/i86pc/kernel/drv/ata.conf >/dev/null
		then
			installing=old
		else
			installing=new
		fi
		case ${existing}_to_${installing} in
		old_to_old) ;; # No special action required
		old_to_new) update_ata2ide ;;
		new_to_old) backout_ata2ide ;;
		new_to_new) ;; # No special action required
		esac
	fi

	epilogue=$rootprefix/bfu.epilogue
	if [ -f $epilogue ]; then
		print "Executing $epilogue"
		$epilogue || print "WARNING: $epilogue failed with code $?"
	fi

	((min = (SECONDS + 30) / 60))

	print "Upgrade of $node took $min minutes."

	#
	# Do logging in the background so that if the automounter is gone,
	# bfu doesn't wedge at this point.
	#
	log=$GATE/public/bfu.log
	(test -w $log && print \
		"`date +%y%m%d` $node `uname -rv` $arch $cpiodir $min" >>$log) &

done

#
# Hacks to work around minor annoyances and make life more pleasant.
# Part 4 of 4: post-archive-extraction usr stuff
#
# (nothing here now)

print "Turning off delayed i/o and syncing filesystems ..."
sync
fastfs -s $rootlist $usr
fastfs $rootlist $usr
sync
lockfs -f $rootlist $usr

if [ -t 0 -a -t 1 -a -t 2 ]; then
	print "\nEntering post-bfu protected environment (shell: ksh)."
	print "Edit configuration files as necessary, then reboot.\n"
	cd $rootprefix/bfu.conflicts
	PS1='bfu# ' ksh -ip
fi

print "Exiting post-bfu protected environment.  To reenter, type:"
print "LD_LIBRARY_PATH=/tmp/bfulib PATH=/tmp/bfubin /tmp/bfubin/ksh"

exit 0
