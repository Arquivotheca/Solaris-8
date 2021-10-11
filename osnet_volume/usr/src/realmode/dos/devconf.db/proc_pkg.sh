#ident "@(#)proc_pkg.sh 1.2 99/08/18"

# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.

PKG_WS=`echo $REALMODE_PKGDIRS|sed 's/:/ /g'`
if [ -z "$PKG_WS" ]; then
	echo "error: pkg_ws not set."
	exit 1
else
	for p in $REALMODE_PKGS
	do
		echo "Processing $p ..."
		for pd in $PKG_WS
		do
			[ -d $pd/$p ] && cat pkg.in|pkgadd -a $BASEDIR/admin -d $pd -R $INS_BASEDIR $p >> /tmp/pkglog.$$ 2>&1
		done
		rc=$?
		if [ $rc != 0 ]; then			
			echo "$p pkgadd error output in /tmp/pkglog.$$"
		fi
	done
fi
