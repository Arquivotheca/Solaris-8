#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)dfstab.sh	1.14	96/01/08 SMI"	/* SVr4.0 1.1.2.1	*/
case "$MACH" in
  "u3b2"|"sparc"|"i386"|"ppc" )
	echo "
#	Place share(1M) commands here for automatic execution
#	on entering init state 3.
#
#	Issue the command '/etc/init.d/nfs.server start' to run the NFS
#	daemon processes and the share commands, after adding the very
#	first entry to this file.
#
#	share [-F fstype] [ -o options] [-d \"<text>\"] <pathname> [resource]
#	.e.g,
#	share  -F nfs  -o rw=engineering  -d \"home dirs\"  /export/home2
" >dfstab
	;;
  * )
	echo "Unknown architecture."
	exit 1
	;;
esac
