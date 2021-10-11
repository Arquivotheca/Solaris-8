#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)vfstab.sh	1.9	92/07/14 SMI"	/* SVr4.0 1.1	*/
#defined later
echo "#device		device		mount		FS	fsck	mount	mount
#to mount	to fsck		point		type	pass	at boot	options
#
#/dev/dsk/c1d0s2 /dev/rdsk/c1d0s2 /usr		ufs	1	yes	-
/proc		-		/proc		proc	-	no	-
fd		-		/dev/fd		fd	-	no	-
swap		-		/tmp		tmpfs	-	yes	-
">vfstab
