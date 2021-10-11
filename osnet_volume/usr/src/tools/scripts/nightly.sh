#!/bin/ksh -p
#
# Copyright (c) 1993-2000 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)nightly	1.20	98/10/20 SMI" (from mike_s)
#ident	"@(#)nightly.sh	1.11	00/10/13 SMI"
#
# Based on the nightly script from the integration folks,
# Mostly modified and owned by mike_s.
# Changes also by kjc, dmk.
#
# BRINGOVER_WS may be specified in the env file.
# The default is the old behavior of CLONE_WS
#
# -i on the command line, means fast options, so when it's on the
# command line (only), lint, check, GPROF and TRACE builds are skipped
# no matter what the setting of their individual flags are in NIGHTLY_OPTIONS.
#
# LINTDIRS can be set in the env file, format is a list of:
#
#	/dirname-to-run-lint-on flag
#
#	Where flag is:	y - enable lint noise diff output
#			n - disable lint noise diff output
#
#	For example: LINTDIRS="$SRC/uts n $SRC/stand y $SRC/psm y"
#
# -A flag in NIGHTLY_OPTIONS checks ABI diffs in .so files
# This option requires a couple of scripts.
#
# OPTHOME and TEAMWARE may be set in the env. to overide /opt and
# /opt/teamware defaults.
#

# function to do a DEBUG and non-DEBUG build. Needed because we might
# need to do another for the source build, and since we only deliver DEBUG or
# non-DEBUG packages.

normal_build() {

	# non-DEBUG build begins

	if [ "$F_FLAG" = "n" ]; then
		export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
		export RELEASE_BUILD ; RELEASE_BUILD=
		unset EXTRA_OPTIONS
		unset EXTRA_CFLAGS

		build non-DEBUG -nd

		if [ "$build_ok" = "y" -a "$X_FLAG" = "y" -a "$p_FLAG" = "y" ]; then
			build_SUNWbtx86 non-DEBUG -nd
			copy_ihv_pkgs non-DEBUG -nd
		fi
	else
		echo "\n==== No non-DEBUG build ====\n" >> $LOGFILE
	fi

	# non-DEBUG build ends

	# DEBUG build begins

	if [ "$D_FLAG" = "y" ]; then

		export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
		unset RELEASE_BUILD
		unset EXTRA_OPTIONS
		unset EXTRA_CFLAGS

		build DEBUG ""

		if [ "$build_ok" = "y" -a "$X_FLAG" = "y" -a "$p_FLAG" = "y" ]; then
			build_SUNWbtx86 DEBUG ""
			copy_ihv_pkgs DEBUG ""
		fi

	else
		echo "\n==== No DEBUG build ====\n" >> $LOGFILE
	fi

	# DEBUG build ends
}

filelist() {
	if  [ $# -ne 2 ]; then
		echo "usage: filelist DESTDIR PATTERN"
		exit 1;
	fi
	DEST=$1
	PATTERN=$2
	cd ${DEST}

	OBJFILES=${ORIG_SRC}/xmod/obj_files
	if [ ! -f ${OBJFILES} ]; then
		return;
	fi
	for i in `grep -v '^#' ${ORIG_SRC}/xmod/obj_files | \
	    grep ${PATTERN} | cut -d: -f2 | tr -d ' \t'`
	do
		# wildcard expansion
		for j in $i
		do
			if [ -f "$j" ]; then
				echo $j
			fi
			if [ -d "$j" ]; then
				echo $j
			fi
		done
	done | sort | uniq
}

# function to save off binaries after a full build for later
# restoration
save_binaries() {
	# save off list of binaries
	echo "\n==== Saving binaries from build at `date` ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE
	rm -f ${BINARCHIVE}
	cd ${CODEMGR_WS}
	filelist ${CODEMGR_WS} '^preserve:' >> $LOGFILE
	filelist ${CODEMGR_WS} '^preserve:' | \
	    cpio -ocB 2>/dev/null | compress \
	    > ${BINARCHIVE}
}

# delete files 
hybridize_files() {
	if  [ $# -ne 2 ]; then
		echo "usage: hybridize_files DESTDIR MAKE_TARGET"
		exit 1;
	fi

	DEST=$1
	MAKETARG=$2

	echo "\n==== Hybridizing files at `date` ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE
	for i in `filelist ${DEST} '^delete:'`
	do
		echo "removing ${i}." | tee -a $mail_msg_file >> $LOGFILE
		rm -rf "${i}"
	done
	for i in `filelist ${DEST} '^hybridize:' `
	do
		echo "hybridizing ${i}." | tee -a $mail_msg_file >> $LOGFILE
		rm -f ${i}+
		sed -e "/^# HYBRID DELETE START/,/^# HYBRID DELETE END/d" \
		    < ${i} > ${i}+
		mv ${i}+ ${i}
	done
}

# restore binaries into the proper source tree.
restore_binaries() {
	if  [ $# -ne 2 ]; then
		echo "usage: restore_binaries DESTDIR MAKE_TARGET"
		exit 1;
	fi

	DEST=$1
	MAKETARG=$2

	echo "\n==== Restoring binaries to ${MAKETARG} at `date` ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE
	cd ${DEST}
	zcat ${BINARCHIVE} | \
	    cpio -idmucvB 2>/dev/null | tee -a $mail_msg_file >> ${LOGFILE}
}

# rename files we save binaries of
rename_files() {
	if  [ $# -ne 2 ]; then
		echo "usage: rename_files DESTDIR MAKE_TARGET"
		exit 1;
	fi

	DEST=$1
	MAKETARG=$2
	echo "\n==== Renaming source files in ${MAKETARG} at `date` ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE
	for i in `filelist ${DEST} '^rename:'`
	do
		echo ${i} | tee -a $mail_msg_file >> ${LOGFILE}
		rm -f ${i}.export
		mv ${i} ${i}.export
	done
}

# function to create the export/crypt source tree
# usage: clone_source CODEMGR_WS DESTDIR MAKE_TARGET

clone_source() {

	if  [ $# -ne 3 ]; then
		echo "usage: clone_source CODEMGR_WS DESTDIR MAKE_TARGET"
		exit 1;
	fi
	WS=$1
	DEST=$2
	MAKETARG=$3

	echo "\n==== Creating ${DEST} source from ${WS} (${MAKETARG}) ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE

	echo "cleaning out ${DEST}." >> $LOGFILE
	rm -rf "${DEST}" >> $LOGFILE 2>&1

	mkdir -p ${DEST}
	cd ${WS}
	
	echo "creating ${DEST}." >> $LOGFILE
	find usr/src -name 's\.*' -a -type f -print | \
	    sed -e 's,SCCS\/s.,,' | \
	    grep -v '/\.del-*' | \
	    cpio -pd ${DEST} >>$LOGFILE 2>&1

	SRC=${DEST}/usr/src

	cd $SRC
	rm -f ${MAKETARG}.out
	echo "making ${MAKETARG} in ${SRC}." >> $LOGFILE
	/bin/time $MAKE -e ${MAKETARG} 2>&1 | \
	    tee -a $SRC/${MAKETARG}.out >> $LOGFILE
	echo "\n==== ${MAKETARG} build errors ====\n" >> $mail_msg_file
	egrep ":" $SRC/${MAKETARG}.out |
		egrep -e "(${MAKE}:|[ 	]error[: 	\n])" | \
		egrep -v warning >> $mail_msg_file

	echo "clearing state files." >> $LOGFILE
	find . -name '.make*' -exec rm -f {} \;

	cd ${DEST}
	if [ "${MAKETARG}" = "CRYPT_SRC" ]; then
		rm -f ${CODEMGR_WS}/crypt_files.cpio.Z
		echo "checking that all cry_files exist." >> ${LOGFILE}
		CRYPT_FILES=${WS}/usr/src/xmod/cry_files
		for i in `cat ${CRYPT_FILES}`
		do
			# make sure the files exist
			if [ -f "$i" ]; then
				continue
			fi
			if [ -d "$i" ]; then
				continue
			fi
			echo "$i does not exist." >> ${LOGFILE}
		done
		find `cat ${CRYPT_FILES}` -print | \
		    cpio -ocB 2>/dev/null | \
		    compress > ${CODEMGR_WS}/crypt_files.cpio.Z
	fi

	if [ "${MAKETARG}" = "EXPORT_SRC" ]; then
		# rename first, since we might restore a file
		# of the same name (mapfiles)
		rename_files ${EXPORT_SRC} EXPORT_SRC
		if [ "$SH_FLAG" = "y" ]; then
			hybridize_files ${EXPORT_SRC} EXPORT_SRC
		fi
	fi

	# save the cleartext
	echo "\n==== Creating ${MAKETARG}.cpio.Z ====\n" | \
	    tee -a $mail_msg_file >> $LOGFILE
	cd ${DEST}
	rm -f ${MAKETARG}.cpio.Z
	find usr/src -depth -print | \
	    grep -v usr/src/${MAKETARG}.out | \
	    cpio -ocB 2>/dev/null | \
	    compress > ${CODEMGR_WS}/${MAKETARG}.cpio.Z
	if [ "${MAKETARG}" = "EXPORT_SRC" ]; then
		restore_binaries ${EXPORT_SRC} EXPORT_SRC
	fi

	if [ "${MAKETARG}" = "CRYPT_SRC" ]; then
		restore_binaries ${CRYPT_SRC} CRYPT_SRC
	fi

}

# function to do the build.
# usage: build LABEL SUFFIX

build() {

	if  [ $# -ne 2 ]; then
		echo "usage: build LABEL SUFFIX"
		exit 1;
	fi

	LABEL=$1
	SUFFIX=$2
	INSTALLOG=install${SUFFIX}-${MACH}
	NOISE=noise${SUFFIX}-${MACH}
	CPIODIR=${CPIODIR_ORIG}${SUFFIX}
	PKGARCHIVE=${PKGARCHIVE_ORIG}${SUFFIX}

	#remove old logs
	OLDINSTALLOG=install${SUFFIX}
	OLDNOISE=noise${SUFFIX}
	rm -f $SRC/${OLDINSTALLOG}.out
	rm -f $SRC/${OLDNOISE}.ref
	if [ -f $SRC/${OLDNOISE}.out ]; then
		mv $SRC/${OLDNOISE}.out $SRC/${NOISE}.ref
	fi

	this_build_ok=y
	#
	#	Build OS-Networking source
	#
	echo "\n==== Building OS-Net source at `date` ($LABEL) ====\n" \
		>> $LOGFILE

	rm -f $SRC/${INSTALLOG}.out
	cd $SRC
	/bin/time $MAKE -e install 2>&1 | \
	    tee -a $SRC/${INSTALLOG}.out >> $LOGFILE
	echo "\n==== Build errors ($LABEL) ====\n" >> $mail_msg_file
	egrep ":" $SRC/${INSTALLOG}.out |
		egrep -e "(${MAKE}:|[ 	]error[: 	\n])" | \
		egrep -v "Ignoring unknown host" | \
		egrep -v warning >> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi
	grep "bootblock image is .* bytes too big" $SRC/${INSTALLOG}.out \
		>> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi

	if [ "$W_FLAG" = "n" ]; then
		echo "\n==== Build warnings ($LABEL) ====\n" >>$mail_msg_file
		# should be none, but there are a few that are pmake
		# related, and a couple of silly ones.
		egrep -i warning: $SRC/${INSTALLOG}.out \
			| egrep -v '^tic:' \
			| egrep -v '^mcs:' \
			| egrep -v '^LD_LIBRARY_PATH=' \
			| egrep -v 'multiple use of -K option' \
			| egrep -v 'ar: creating' \
			| egrep -v 'ar: writing' \
			| egrep -v 'conflicts:' \
			| egrep -v ':saved created' \
			| egrep -v '^stty.*c:' \
			| egrep -v '^mfgname.c:' \
			| egrep -v '^uname-i.c:' \
			| egrep -v '^volumes.c:' \
			| egrep -v '^lint library construction:' \
			| egrep -v 'tsort: INFORM:' \
			| egrep -v 'stripalign:' \
			| egrep -v 'chars, width' \
			| egrep -v 'option -zdefs/nodefs appears more than' \
			| egrep -v "symbol \`timezone' has differing types:" \
			| egrep -v "parameter <PSTAMP> set to" \
			| egrep -v "^Manifying" \
			| egrep -v "Ignoring unknown host" \
			>> $mail_msg_file
	fi

	echo "\n==== Ended OS-Net source build at `date` ($LABEL) ====\n" \
		>> $LOGFILE

	echo "\n==== Elapsed build time ($LABEL) ====\n" >>$mail_msg_file
	tail -3  $SRC/${INSTALLOG}.out >>$mail_msg_file

	if [ "$i_FLAG" = "n" -a "$W_FLAG" = "n" ]; then
		rm -f $SRC/${NOISE}.ref
		if [ -f $SRC/${NOISE}.out ]; then
			mv $SRC/${NOISE}.out $SRC/${NOISE}.ref
		fi
		grep : $SRC/${INSTALLOG}.out \
			| egrep -v '^/' \
			| egrep -v '^(Start|Finish|real|user|sys|./bld_awk)' \
			| egrep -v '^tic:' \
			| egrep -v '^mcs' \
			| egrep -v '^LD_LIBRARY_PATH=' \
			| egrep -v 'multiple use of -K option' \
			| egrep -v 'ar: creating' \
			| egrep -v 'ar: writing' \
			| egrep -v 'conflicts:' \
			| egrep -v ':saved created' \
			| egrep -v '^stty.*c:' \
			| egrep -v '^mfgname.c:' \
			| egrep -v '^uname-i.c:' \
			| egrep -v '^volumes.c:' \
			| egrep -v '^lint library construction:' \
			| egrep -v 'tsort: INFORM:' \
			| egrep -v 'stripalign:' \
			| egrep -v 'chars, width' \
			| egrep -v 'option -zdefs/nodefs appears more than' \
			| egrep -v "symbol \`timezone' has differing types:" \
			| egrep -v 'PSTAMP' \
			| egrep -v '|%WHOANDWHERE%|' \
			| egrep -v '^Manifying' \
			| sort | uniq >$SRC/${NOISE}.out
		if [ ! -f $SRC/${NOISE}.ref ]; then
			cp $SRC/${NOISE}.out $SRC/${NOISE}.ref
		fi
		echo "\n==== Build noise differences ($LABEL) ====\n" \
			>>$mail_msg_file
		diff $SRC/${NOISE}.ref $SRC/${NOISE}.out >>$mail_msg_file
	fi

	#
	#	Create cpio archives for preintegration testing (PIT)
	#
	if [ "$a_FLAG" = "y" -a "$this_build_ok" = "y" ]; then
		echo "\n==== Creating $LABEL cpio archives at `date` ====\n" \
			>> $LOGFILE
		mkbfu $ROOT ${CPIODIR} 2>&1 | \
			tee -a /tmp/mkbfu.$$ >> $LOGFILE
		echo "\n==== cpio archives build errors ($LABEL) ====\n" \
			>> $mail_msg_file
		grep -v "archive:" /tmp/mkbfu.$$ >> $mail_msg_file
		rm -f /tmp/mkbfu.$$
		if [ "$z_FLAG" = "y" ]; then
			echo "" >> $LOGFILE
			gzip -v ${CPIODIR}/* >> $LOGFILE 2>&1
			if [ "$?" != "0" ]; then
				echo "\n==== cpio archives compression failed ($LABEL) ====\n" \
					>> $mail_msg_file
			fi
		fi
		# hack for test folks
		if [ -z "`echo $PARENT_WS|egrep '^\/ws\/'`" ]; then
			X=/net/`uname -n`${CPIODIR}
		else
			X=${CPIODIR}
		fi
		echo "Archive_directory: ${X}" >/tmp/f$$
		cp /tmp/f$$ ${CPIODIR}/../../.${MACH}_wgtrun
		rm -f /tmp/f$$

	else
		echo "\n==== Not creating $LABEL cpio archives ====\n" \
			>> $LOGFILE
	fi

	#
	#	Building Packages
	#
	if [ "$p_FLAG" = "y" -a "$this_build_ok" = "y" ]; then
		echo "\n==== Creating $LABEL packages at `date` ====\n" \
			>> $LOGFILE
		rm -f $SRC/pkgdefs/${INSTALLOG}.out
		echo "Clearing out $PKGARCHIVE ..." >> $LOGFILE
		rm -rf $PKGARCHIVE
		mkdir -p $PKGARCHIVE
		cd $SRC/pkgdefs
		$MAKE -e install 2>&1 | \
			tee -a $SRC/pkgdefs/${INSTALLOG}.out >> $LOGFILE
		echo "\n==== Package build errors ($LABEL) ====\n" \
			>> $mail_msg_file
		egrep "${MAKE}|ERROR|WARNING" $SRC/pkgdefs/${INSTALLOG}.out | \
			grep ':' | \
			grep -v PSTAMP \
			>> $mail_msg_file

		#
		#	Build realmode packages
		#
		if [ "$X_FLAG" = "y" ]; then
			LABEL="$LABEL realmode"
			INSTALLOG=install${SUFFIX}-${MACH}
			NOISE=noise${SUFFIX}-${MACH}

			SPARC_RM_PKGARCHIVE=${SPARC_RM_PKGARCHIVE_ORIG}${SUFFIX}

			echo "Clearing out $SPARC_RM_PKGARCHIVE ..." >> $LOGFILE
			rm -rf $SPARC_RM_PKGARCHIVE
			mkdir -p $SPARC_RM_PKGARCHIVE

			echo "\n==== Creating $LABEL packages at `date` ====\n" \
				>> $LOGFILE
			rm -f $SRC/realmode/pkgdefs/${INSTALLOG}.out
			cd $SRC/realmode/pkgdefs
			$MAKE -e install 2>&1 | \
				tee -a $SRC/realmode/pkgdefs/${INSTALLOG}.out \
					>> $LOGFILE
			echo "\n==== Package build errors ($LABEL) ====\n" \
				>> $mail_msg_file
			egrep "ERROR|WARNING" \
				$SRC/realmode/pkgdefs/${INSTALLOG}.out | \
				grep -v PSTAMP >> $mail_msg_file
		fi
	else
		echo "\n==== Not creating $LABEL packages ====\n" >> $LOGFILE
	fi
}

dolint() {

	#
	# Arg. 2 is a flag to turn on/off the lint diff output
	#
	dl_usage="Usage: dolint /dir y|n"

	if [ $# -ne 2 ]; then
		echo $dl_usage
		exit 1
	fi

	if [ ! -d "$1" ]; then
		echo $dl_usage
		exit 1
	fi

	if [ "$2" != "y" -a "$2" != "n" ]; then
		echo $dl_usage
		exit 1
	fi

	lintdir=$1
	dodiff=$2
	base=`basename $lintdir`
	LINTOUT=$lintdir/lint-${MACH}.out
	LINTNOISE=$lintdir/lint-noise-${MACH}

	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	unset RELEASE_BUILD
	unset EXTRA_OPTIONS
	unset EXTRA_CFLAGS

	#
	#	'$MAKE lint' in $lintdir
	#
	echo "\n==== Begin '$MAKE lint' of $base at `date` ====\n" >> $LOGFILE

	# remove old lint.out
	rm -f $lintdir/lint.out $lintdir/lint-noise.out
	if [ -f $lintdir/lint-noise.ref ]; then
		mv $lintdir/lint-noise.ref ${LINTNOISE}.ref
	fi

	rm -f $LINTOUT
	cd $lintdir
	#
	# Remove all .ln files to ensure a full reference file
	#
	rm -f Nothing_to_remove \
	    `find . -name SCCS -prune -o -type f -name '*.ln' -print `

	/bin/time $MAKE -ek lint 2>&1 | \
	    tee -a $LINTOUT >> $LOGFILE
	echo "\n==== '$MAKE lint' of $base ERRORS ====\n" >> $mail_msg_file
	grep "$MAKE:" $LINTOUT >> $mail_msg_file

	echo "\n==== Ended '$MAKE lint' of $base at `date` ====\n" >> $LOGFILE

	echo "\n==== Elapsed time of '$MAKE lint' of $base ====\n" \
		>>$mail_msg_file
	tail -3  $LINTOUT >>$mail_msg_file

	rm -f ${LINTNOISE}.ref
	if [ -f ${LINTNOISE}.out ]; then
		mv ${LINTNOISE}.out ${LINTNOISE}.ref
	fi
        #grep : $LINTOUT |
		#egrep -v '^(name|function|value|argument|real|user|sys)' |
		#egrep -v 'warning: (name|function|value|possibly)' |
		#egrep -v 'warning: argument used' |
        grep : $LINTOUT | \
		egrep -v '^(real|user|sys)' |
		egrep -v '(library construction)' | \
		egrep -v ': global crosschecks' | \
		sort > ${LINTNOISE}.out
	if [ ! -f ${LINTNOISE}.ref ]; then
		cp ${LINTNOISE}.out ${LINTNOISE}.ref
	fi
	if [ "$dodiff" != "n" ]; then
		echo "\n==== lint warnings $base ====\n" \
			>>$mail_msg_file
		# should be none, though there are a few that were filtered out
		# above
		egrep -i '(warning|lint):' ${LINTNOISE}.out \
			| sort | uniq >> $mail_msg_file
		echo "\n==== lint noise differences $base ====\n" \
			>> $mail_msg_file
		diff ${LINTNOISE}.ref ${LINTNOISE}.out \
			>> $mail_msg_file
	fi
}

#
# Function to derive the java compiler version from
# the path defined by JAVA_ROOT in Makefile.master
#
java_version() {
        MF=$1
	bt=`egrep "^BUILD_TOOLS" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${bt}" != "" -a "${BUILD_TOOLS}" = "" ]; then
		eval $bt
	fi

	jr=`egrep "^JAVA_ROOT" ${MF} | tr '()' '{}' | sed -e 's/[ 	]//g'`
	if [ "${jr}" != "" -a "${JAVA_ROOT}" = "" ]; then
		eval $jr
	fi
	echo "${JAVA_ROOT}/bin/javac"
	${JAVA_ROOT}/bin/java -fullversion 2>&1 | head -1
}

#
# Function to derive the 64-bit compiler version from
# the path defined by SPRO_ROOT and the environment
# Makefile.master variables.
#
cc_version64() {
	MF=$1
	bt=`egrep "^BUILD_TOOLS" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${bt}" != "" -a "${BUILD_TOOLS}" = "" ]; then
		eval $bt
	fi
	spr=`egrep "^SPRO_ROOT" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${spr}" != "" -a "${SPRO_ROOT}" = "" ]; then
		eval $spr
	fi
	inr=`egrep "^INTC_ROOT" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${inr}" != "" -a "${INTC_ROOT}" = "" ]; then
		eval $inr
	fi
	comp=`egrep "^[a-z0-9]*_CC[ 	]*=" ${MF} | \
	    tr '()' '{}' | sed -e 's/[	 ]//g' | \
	    tr '\n' ';'`
	eval $comp
	case "$MACH" in
	sparc)
		MACH64=sparcv9;;
	i386|ia64)
		MACH64=ia64;;
	esac
	comp=CC=\$${MACH64}_CC
	eval $comp
	if [ "${CC}" = "" ]; then
		# no special CC for 64-bit
		cc_version $MF
	elif [ -x "${CC}" ]; then
		eval echo \${CC}
		eval \${CC} -V 2>&1 | head -1
	else
		echo "No 64-bit compiler found"
	fi
}

#
# Function to derive the compiler version from
# the path defined by SPRO_ROOT and the environment
# Makefile.master variables.
#
cc_version() {
	MF=$1
	bt=`egrep "^BUILD_TOOLS" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${bt}" != "" -a "${BUILD_TOOLS}" = "" ]; then
		eval $bt
	fi
	spr=`egrep "^SPRO_ROOT" ${MF} | tr '()' '{}' | sed -e 's/[	 ]//g'`
	if [ "${spr}" = "" ]; then
		# No SPRO_ROOT in this build
		eval echo $OPTHOME/SUNWspro/bin/cc
		$OPTHOME/SUNWspro/bin/cc -V 2>&1 | head -1
	else
		if [ "${SPRO_ROOT}" = "" ]; then
			eval $spr
		fi
		# allow overriding of ${MACH}_CC
		if [ "`eval echo \$\{${MACH}_CC\}`" = "" ]
		then
			spr_env=`grep SPRO_ROOT ${MF} | \
				egrep -v "^SPRO_ROOT" | \
				egrep -v "^#" | \
				tr '()' '{}' | sed -e 's/[	 ]//g' | \
				tr '\n' ';'`
			eval $spr_env
		fi
		eval echo \$\{${MACH}_CC\}
		eval \$\{${MACH}_CC\} -V 2>&1 | head -1
	fi
}


# Install proto area from IHV build

copy_ihv_proto() {

	echo "\n==== Installing $IA32_IHV_ROOT  ====\n" \
		>> $LOGFILE
	if [ -d "$IA32_IHV_ROOT" ]; then
		if [ ! -d "$ROOT" ]; then
			echo "mkdir -p $ROOT" >> $LOGFILE
			mkdir -p $ROOT
		fi
		echo "cd $IA32_IHV_ROOT\n" >> $LOGFILE
		cd $IA32_IHV_ROOT
		tar -cf - . | (cd $ROOT; umask 0; tar xpf - ) 2>&1 >> $LOGFILE
	else
		echo "$IA32_IHV_ROOT: not found" >> $LOGFILE
	fi
}

# Install IHV packages in PKGARCHIVE

copy_ihv_pkgs() {

	if  [ $# -ne 2 ]; then
		echo "usage: copy_ihv_pkgs LABEL SUFFIX"
		exit 1;
	fi

	LABEL=$1
	SUFFIX=$2
	# always use non-DEBUG IHV packages
	IA32_IHV_PKGS=${IA32_IHV_PKGS_ORIG}-nd
	PKGARCHIVE=${PKGARCHIVE_ORIG}${SUFFIX}

	echo "\n==== Installing IHV packages from $IA32_IHV_PKGS ($LABEL) ====\n" \
		>> $LOGFILE
	if [ -d "$IA32_IHV_PKGS" ]; then
		cd $IA32_IHV_PKGS
		tar -cf - * | \
		   (cd $PKGARCHIVE; umask 0; tar xpf - ) 2>&1 >> $LOGFILE
	else
		echo "$IA32_IHV_PKGS: not found" >> $LOGFILE
	fi

	echo "\n==== Installing IHV packages from $IA32_IHV_BINARY_PKGS ($LABEL) ====\n" \
		>> $LOGFILE
	if [ -d "$IA32_IHV_BINARY_PKGS" ]; then
		cd $IA32_IHV_BINARY_PKGS
		tar -cf - * | \
		    (cd $PKGARCHIVE; umask 0; tar xpf - ) 2>&1 >> $LOGFILE
	else
		echo "$IA32_IHV_BINARY_PKGS: not found" >> $LOGFILE
	fi
}

# Build ON IA32 realmode using an NT build machine (NTSERVER)

build_realmode() {

	if  [ $# -ne 2 ]; then
		echo "usage: build LABEL SUFFIX"
		exit 1;
	fi

	LABEL=$1
	SUFFIX=$2
	INSTALLOG=install${SUFFIX}-${MACH}
	NOISE=noise${SUFFIX}-${MACH}
	CPIODIR=${CPIODIR_ORIG}${SUFFIX}
	PKGARCHIVE=${PKGARCHIVE_ORIG}${SUFFIX}

	#remove old logs
	OLDINSTALLOG=install${SUFFIX}
	OLDNOISE=noise${SUFFIX}
	rm -f $SRC/realmode/${OLDINSTALLOG}.out
	rm -f $SRC/realmode/${OLDNOISE}.ref
	if [ -f $SRC/realmode/${OLDNOISE}.out ]; then
		mv $SRC/realmode/${OLDNOISE}.out $SRC/realmode/${NOISE}.ref
	fi

	this_build_ok=y
	#
	#	Build OS-Networking source
	#
	echo "\n==== Building realmode source at `date` ($LABEL) ====\n" \
		>> $LOGFILE

	rm -f $SRC/realmode/${INSTALLOG}.out
	cd $SRC/realmode
	/bin/time $MAKE -e install 2>&1 | \
	    tee -a $SRC/realmode/${INSTALLOG}.out >> $LOGFILE
	echo "\n==== Realmode build errors ($LABEL) ====\n" >> $mail_msg_file

	# These errors usually mean the workspace is not exported to the
	# NT server properly.  No warning or error in the msg so we
	# search for them manually.
	egrep "System error 53 has occurred." $SRC/realmode/${INSTALLOG}.out \
		>> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi
	egrep "The network path was not found." $SRC/realmode/${INSTALLOG}.out \
		>> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi
	egrep ":" $SRC/realmode/${INSTALLOG}.out |
		egrep -e "(${MAKE}:|[ 	]error[: 	\n])" | \
		egrep -v warning >> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi

	if [ "$W_FLAG" = "n" ]; then
		echo "\n==== Realmode build warnings ($LABEL) ====\n" >>$mail_msg_file
		egrep -i warning $SRC/realmode/${INSTALLOG}.out \
			| egrep -v "LINK : warning L4021: no stack segment" \
			| egrep -v "Warning: entry point not 0, strip will lose data" \
			| egrep -v "used #pragma pack to change alignment" \
			| egrep -v "Warning: .EXE has relocation items" \
			| egrep -v "warning C4068: unknown pragma" \
			| egrep -v "warning C4761: integral size mismatch in argument; conversion supplied" \
			| egrep -v "LINK : warning L4038: program has no starting address" \
			| egrep -v "warning C4142: benign redefinition of type" \
			| egrep -v "warning C4028: formal parameter 2 different from declaration" \
			| egrep -v "WARNING! DEBUG MODE " \
			| egrep -v "warning C4113: function parameter lists differed" \
			| egrep -v "NMAKE : warning U4010: 'clobber' : build failed; /K specified, continuing" \
			| egrep -v "Output\/Warnings\/Errors" \
			>> $mail_msg_file
	fi

	echo "\n==== Ended realmode source build at `date` ($LABEL) ====\n" \
		>> $LOGFILE

	echo "\n==== Elapsed realmode build time ($LABEL) ====\n" >>$mail_msg_file
	tail -3  $SRC/realmode/${INSTALLOG}.out >>$mail_msg_file

	if [ "$i_FLAG" = "n" -a "$W_FLAG" = "n" ]; then
		rm -f $SRC/realmode/${NOISE}.ref
		if [ -f $SRC/realmode/${NOISE}.out ]; then
			mv $SRC/realmode/${NOISE}.out $SRC/realmode/${NOISE}.ref
		fi

		sed	-e "s///" \
			-e "/^$/d" \
			-e "/	NMAKE/d" \
			-e "/	cd /d" \
			-e "/	copy [a-z0-9_.]*/d" \
			-e "/	del [a-z0-9_.]*/d" \
			-e "/	if exist [a-z0-9_.\*]* del [a-z0-9_.\*]*/d" \
			-e "/	makesub.bat [a-z][a-z_]* [a-z][a-z]*/d" \
			-e "/Assembling: .*[a-z][a-z]*.s$/d" \
			-e "/Assemblin .*.asm$/d" \
			-e "/The directory is not empty./d" \
			-e "/C\/C++ Optimizing Compiler Version 8.00c/d" \
			-e "/Copyright ([cC]) Microsoft Corp/d" \
			-e "/Microsoft (R)/d" \
			-e "/Could Not Find [A-Z]:[a-z0-9_\.]*$/d" \
			-e "/Could Not Find .*mboot$/d" \
			-e "/Could Not Find .*pboot$/d" \
			-e "/Could Not Find .*\.map$/d" \
			-e "/Could Not Find .*\.exe$/d" \
			-e "/Could Not Find .*\.obj$/d" \
			-e "/Could Not Find .*\.lst$/d" \
			-e "/Could Not Find .*\.cod$/d" \
			-e "/Could Not Find .*\.bef$/d" \
			-e "/Could Not Find .*\.lib$/d" \
			-e "/Could Not Find .*\.bin$/d" \
			-e "/real.*[0-9]*:[0-9]*.[0-9]*/d" \
			-e "/user.*[0-9]*.[0-9]*/d" \
			-e "/sys.*[0-9]*.[0-9]*/d" \
			-e "/masm [a-z][a-z_]*.s/d" \
			-e "/rd proto.*/d" \
			-e "/md proto.*/d" \
			-e "/del \/f \/q proto.*/d" \
			-e "/^[a-z0-9][a-z0-9_]*.c$/d" \
			-e "/^	cl.*[a-z][a-z0-9_]*\.c$/d" \
			-e "/^\.\.\\\\[a-z\.]*\\\\[a-z0-9_]*.c$/d" \
			-e "/^install /d" \
			-e "/ --> serial$/d" \
			-e "/\`[a-z][a-z]*\' is up to date./d" \
			-e "/drv_mnt/d" \
			-e "/makesub.bat/d" \
			-e "/^Library name:/d" \
			-e "/^Operations:/d" \
			-e "/1 file(s) copied./d" \
			-e "/^[ 	]*del /d" \
			-e "/^[ 	]*copy /d" \
			-e "/\\\\tmp\\\\exe2bin -strip_to_entry/d" \
			-e "/Warning: entry point not 0, strip will lose data/d" \
			-e "s/[A-Za-z]://" \
			-e "/\/DFARDATA/d" \
			-e "/lib @smalllib.rsp/d" \
			-e "/\/Tacallmdbb.s/d" \
			-e "/warning C4142: benign redefinition of type/d" \
			-e "/bld_awk_pkginfo/d" \
		    < $SRC/realmode/${INSTALLOG}.out \
		    > $SRC/realmode/${NOISE}.out

		if [ ! -f $SRC/realmode/${NOISE}.ref ]; then
			cp $SRC/realmode/${NOISE}.out $SRC/realmode/${NOISE}.ref
		fi
		echo "\n==== Realmode build noise differences ($LABEL) ====\n" \
			>>$mail_msg_file
		diff $SRC/realmode/${NOISE}.ref $SRC/realmode/${NOISE}.out >>$mail_msg_file
	fi
}

# Build IA32 boot floppy, using ON realmode and IHV drivers

build_SUNWbtx86() {

	if  [ $# -ne 2 ]; then
		echo "usage: build LABEL SUFFIX"
		exit 1;
	fi

	LABEL=$1
	SUFFIX=$2
	INSTALLOG=install${SUFFIX}-${MACH}
	NOISE=noise${SUFFIX}-${MACH}
	PKGARCHIVE=${PKGARCHIVE_ORIG}${SUFFIX}

	# Boot floppy is always built with non-DEBUG packages
	IA32_IHV_PKGS=${IA32_IHV_PKGS_ORIG}-nd

	if [ ! -d "$IA32_IHV_PKGS" ]; then
		echo "$IA32_IHV_PKGS: not found - error building SUNWbtx86" \
			>>$mail_msg_file
		return
	fi
	if [ ! -d "$PKGARCHIVE" ]; then
		echo "$PKGARCHIVE: not found - error building SUNWbtx86" \
			>>$mail_msg_file
		return
	fi

	#remove old logs
	OLDINSTALLOG=install${SUFFIX}
	OLDNOISE=noise${SUFFIX}
	rm -f $SRC/realmode/dos/devconf.db/${OLDINSTALLOG}.out
	rm -f $SRC/realmode/dos/devconf.db/${OLDNOISE}.ref
	if [ -f $SRC/realmode/dos/devconf.db/${OLDNOISE}.out ]; then
		mv $SRC/realmode/dos/devconf.db/${OLDNOISE}.out \
			$SRC/realmode/dos/devconf.db/${NOISE}.ref
	fi

	this_build_ok=y
	#
	#	Build OS-Networking source
	#
	echo "\n==== Building SUNWbtx86 at `date` ($LABEL) ====\n" >> $LOGFILE

	rm -f $SRC/realmode/dos/devconf.db/${INSTALLOG}.out
	cd $SRC/realmode/dos/devconf.db

	umask 022
	/bin/time $MAKE -e install 2>&1 | \
	    tee -a $SRC/realmode/dos/devconf.db/${INSTALLOG}.out >> $LOGFILE

	echo "\n==== SUNWbtx86 build errors ($LABEL) ====\n" >> $mail_msg_file

	egrep ":" $SRC/realmode/dos/devconf.db/${INSTALLOG}.out |
		egrep -e "(${MAKE}:|[ 	]error[: 	\n])" | \
		egrep -v warning >> $mail_msg_file
	if [ "$?" = "0" ]; then
		build_ok=n
		this_build_ok=n
	fi


	echo "\n==== Ended SUNWbtx86 build at `date` ($LABEL) ====\n" \
		>> $LOGFILE

	echo "\n==== Elapsed SUNWbtx86 build time ($LABEL) ====\n" >>$mail_msg_file
	tail -3  $SRC/realmode/dos/devconf.db/${INSTALLOG}.out >>$mail_msg_file

	echo "" >>$mail_msg_file
	grep "Number of BEF drivers:" \
		$SRC/realmode/dos/devconf.db/${INSTALLOG}.out \
		>>$mail_msg_file
	echo "" >>$mail_msg_file

	if [ "$i_FLAG" = "n" -a "$W_FLAG" = "n" ]; then
		rm -f $SRC/realmode/dos/devconf.db/${NOISE}.ref
		if [ -f $SRC/realmode/dos/devconf.db/${NOISE}.out ]; then
			mv $SRC/realmode/dos/devconf.db/${NOISE}.out \
			    $SRC/realmode/dos/devconf.db/${NOISE}.ref
		fi
		# Trim common noise, diff everything else
		sed	-e "s///" \
			-e "/^$/d" \
			-e "/pkgadd error output in \/tmp\/pkglog\./d" \
			-e "/rm -rf \/tmp\/root\./d" \
			-e "/Output\/Warnings\/Errors/d" \
			-e "/rm -f pkgkit/d" \
			-e "/cp pkgkit.ksh pkgkit/d" \
			-e "/chmod +x pkgkit/d" \
			-e "/WARNING: parameter <PSTAMP> set to /d" \
			-e "/real.*[0-9]*:[0-9]*.[0-9]*/d" \
			-e "/real.*[0-9]*.[0-9]*/d" \
			-e "/user.*[0-9]*.[0-9]*/d" \
			-e "/sys.*[0-9]*.[0-9]*/d" \
			-e "/Everything else:/d" \
			-e "/Total:/d" \
			-e "/^rm -f/d" \
			-e "/^cat /d" \
			-e "/^chmod /d" \
			-e "/^ln /d" \
			-e "/^cp /d" \
			-e "/^sccs /d" \
			-e "/dcb -uci/d" \
			-e "/--> [1-9] job/d" \
			-e "/bld_awk_pkginfo/d" \
		    <$SRC/realmode/dos/devconf.db/${INSTALLOG}.out \
		    >$SRC/realmode/dos/devconf.db/${NOISE}.out

		if [ ! -f $SRC/realmode/dos/devconf.db/${NOISE}.ref ]; then
			cp $SRC/realmode/dos/devconf.db/${NOISE}.out \
			    $SRC/realmode/dos/devconf.db/${NOISE}.ref
		fi
		echo "\n==== SUNWbtx86 build noise differences ($LABEL) ====\n" \
			>>$mail_msg_file
		diff $SRC/realmode/dos/devconf.db/${NOISE}.ref \
		    $SRC/realmode/dos/devconf.db/${NOISE}.out >>$mail_msg_file
	fi
}


MACH=`uname -p`

if [ "$OPTHOME" = "" ]; then
	OPTHOME=/opt
	export OPTHOME
fi
if [ "$TEAMWARE" = "" ]; then
	TEAMWARE=$OPTHOME/teamware
	export TEAMWARE
fi

USAGE='Usage: nightly [-in] [-V VERS ] [ -S E|D|H ] <env_file>

Where:
	-i	Fast incremental options (no clobber, lint, check, gprof, trace)
	-n      Do not do a bringover
	-V VERS set the build version string to VERS
	-S	Build a variant of the source product
		E - build exportable source
		D - build domestic source (exportable + crypt)
		H - build hybrid source (binaries + deleted source)

	<env_file>  file in Bourne shell syntax that sets and exports
	variables that configure the operation of this script and many of
	the scripts this one calls. If <env_file> does not exist,
	it will be looked for in $OPTHOME/onbld/env.

non-DEBUG is the default build type. Build options can be set in the
NIGHTLY_OPTIONS variable in the <env_file> as follows:

	-A	check for ABI differences in .so files
	-C	check for cstyle/hdrchk errors
	-D	do a build with DEBUG on
	-F	do _not_ do a non-DEBUG build
	-G	gate keeper default group of options (-abu)
	-I	integration engineer default group of options (-abimpu)
	-N	do not run protocmp
	-P	do a build with GPROF on
	-R	default group of options for building a release (-cimp)
	-T	do a build with TRACE on
	-U	update proto area in the parent
	-V VERS set the build version string to VERS
	-X	build usr/src/realmode, x86 only, requires NT build machine
	-a	create cpio archives
	-d	use Distributed Make (default uses Parallel Make)
	-i	do an incremental build (no "make clobber")
	-l	do "make lint" in $LINTDIRS (default: $SRC y)
	-m	send mail to $MAILTO at end of build
	-n      Do not do a bringover
	-p	create packages
	-r	check ELF file runpaths in the proto area
	-u	update proto_list_${MACH} in the parent workspace
	-z	compress cpio archives with gzip
	-W	Do not report warnings (freeware gate ONLY)
	-S	Build a variant of the source product
		E - build exportable source
		D - build domestic source (exportable + crypt)
'
#
#	-x	less public handling of xmod source for the source product
#
#	A log file will be generated under the name $LOGFILE
#	for partially completed build and log.`date '+%m%d%y'`
#	in the same directory for fully completed builds.
#

# default values for low-level FLAGS; G I R are group FLAGS
A_FLAG=n
a_FLAG=n
d_FLAG=n
C_FLAG=n
F_FLAG=n
D_FLAG=n
P_FLAG=n
T_FLAG=n
n_FLAG=n
i_FLAG=n; i_CMD_LINE_FLAG=n
l_FLAG=n
m_FLAG=n
p_FLAG=n
r_FLAG=n
s_FLAG=n
u_FLAG=n
U_FLAG=n
V_FLAG=n
N_FLAG=n
z_FLAG=n
W_FLAG=n
SE_FLAG=n
SD_FLAG=n
SH_FLAG=n
X_FLAG=n
#
XMOD_OPT=
#
build_ok=y
#
# examine arguments
#

OPTIND=1
while getopts inV:S: FLAG
do
	case $FLAG in
	  i )	i_FLAG=y; i_CMD_LINE_FLAG=y
		;;
	  n )	n_FLAG=y
		;;
	  V )	V_FLAG=y
		V_ARG="$OPTARG"
		;;
	  S )
		if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
			echo "Can only build one source variant at a time."
			exit 1
		fi
		if [ "${OPTARG}" = "E" ]; then
			SE_FLAG=y
		elif [ "${OPTARG}" = "D" ]; then
			SD_FLAG=y
		elif [ "${OPTARG}" = "H" ]; then
			SH_FLAG=y
		else
			echo "$USAGE"
			exit 1
		fi
		;;
	 \? )	echo "$USAGE"
		exit 1
		;;
	esac
done

# correct argument count after options
shift `expr $OPTIND - 1`

# test that the path to the environment-setting file was given
if [ $# -ne 1 ]; then
	echo "$USAGE"
	exit 1
fi

# verify you are root
/usr/bin/id | grep root >/dev/null 2>&1
if [ "$?" != "0" ]; then
	echo \"nightly\" must be run as root.
	exit 1
fi

#
# force locale to C
LC_COLLATE=C;	export LC_COLLATE
LC_CTYPE=C;	export LC_CTYPE
LC_MESSAGES=C;	export LC_MESSAGES
LC_MONETARY=C;	export LC_MONETARY
LC_NUMERIC=C;	export LC_NUMERIC
LC_TIME=C;	export LC_TIME

# clear environment variables we know to be bad for the build
unset LD_OPTIONS LD_LIBRARY_PATH LD_AUDIT LD_BIND_NOW LD_BREADTH LD_CONFIG
unset LD_DEBUG LD_FLAGS LD_LIBRARY_PATH_64 LD_NOVERSION LD_ORIGIN
unset LD_LOADFLTR LD_NOAUXFLTR LD_NOCONFIG LD_NODIRCONFIG LD_NOOBJALTER
unset LD_PRELOAD LD_PROFILE
unset CONFIG
unset GROUP
unset OWNER
unset REMOTE
unset ENV

#
# 	Setup environmental variables
#
if [ -f $1 ]; then
	. $1
else
	if [ -f $OPTHOME/onbld/env/$1 ]; then
		. $OPTHOME/onbld/env/$1
	else
		echo "Cannot find env file as either $1 or $OPTHOME/onbld/env/$1"
		exit 1
	fi
fi

#
# See if NIGHTLY_OPTIONS is set
#
if [ "$NIGHTLY_OPTIONS" = "" ]; then
	NIGHTLY_OPTIONS="-aBm"
fi

#
# If BRINGOVER_WS was not specified, let it default to CLONE_WS
#
if [ "$BRINGOVER_WS" = "" ]; then
	BRINGOVER_WS=$CLONE_WS
fi

OPTIND=1
while getopts ABDFNPTCGIRainlmptuUxdrzWSX FLAG $NIGHTLY_OPTIONS
do
	case $FLAG in
	  A )	A_FLAG=y
		;;
	  B )	D_FLAG=y
		;; # old version of D
	  F )	F_FLAG=y
		;;
	  D )	D_FLAG=y
		;;
	  P )	P_FLAG=y
		;;
	  T )	T_FLAG=y
		;;
	  C )	C_FLAG=y
		;;
	  N )	N_FLAG=y
		;;
	  G )	a_FLAG=y
		u_FLAG=y
		;;
	  I )	a_FLAG=y
		m_FLAG=y
		p_FLAG=y
		u_FLAG=y
		;;
	  R )	m_FLAG=y
		p_FLAG=y
		;;
	  a )	a_FLAG=y
		;;
	  d )	d_FLAG=y
		;;
	  i )	i_FLAG=y
		;;
	  n )	n_FLAG=y
		;;
	  l )	l_FLAG=y
		;;
	  m )	m_FLAG=y
		;;
	  p )	p_FLAG=y
		;;
	  r )	r_FLAG=y
		;;
	  u )	u_FLAG=y
		;;
	  z )	z_FLAG=y
		;;
	  U )	U_FLAG=y
		;;
	  x )	XMOD_OPT="-x"
		;;
	  W )	W_FLAG=y
		;;
	  S )
		if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
			echo "Can only build one source variant at a time."
			exit 1
		fi
		if [ "${OPTARG}" = "E" ]; then
			SE_FLAG=y
		elif [ "${OPTARG}" = "D" ]; then
			SD_FLAG=y
		elif [ "${OPTARG}" = "H" ]; then
			SH_FLAG=y
		else
			echo "$USAGE"
			exit 1
		fi
		;;

	  X )	# only turn on realmode flag on i386 so env
		# files can remain common across platforms
		if [ "$MACH" = "i386" ]; then
			X_FLAG=y
		fi
		;;
	 \? )	echo "$USAGE"
		exit 1
		;;
	esac
done

PATH="$OPTHOME/onbld/bin:$OPTHOME/onbld/bin/${MACH}:/usr/ccs/bin"
PATH="$PATH:$OPTHOME/SUNWspro/bin:$TEAMWARE/bin:/usr/bin:/usr/sbin:/usr/ucb"
PATH="$PATH:/usr/openwin/bin:/opt/sfw/bin:."
export PATH

if [ "$d_FLAG" = "y" ]; then
	# dmake link already in /opt/SUNWspro/bin
	#PATH="$TEAMWARE/TW2.1/bin:$PATH"
	maketype="distributed"
	MAKE=dmake
else
	PATH="$TEAMWARE/ParallelMake/bin:$PATH"
	maketype="parallel"
	MAKE=make
fi
export PATH
export MAKE

if [ "${SUNWSPRO}" != "" ]; then
	PATH="${SUNWSPRO}/bin:$PATH"
	export PATH
fi

hostname=`uname -n`
if [ ! -f $HOME/.make.machines ]; then
	DMAKE_MAX_JOBS=4
else
	DMAKE_MAX_JOBS="`grep $hostname $HOME/.make.machines | \
	    tail -1 | awk -F= '{print $ 2;}'`"
	if [ "$DMAKE_MAX_JOBS" = "" ]; then
		DMAKE_MAX_JOBS=4
	fi
fi
DMAKE_MODE=parallel;
export DMAKE_MODE
export DMAKE_MAX_JOBS

if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" ]; then
        if [ -z "${EXPORT_SRC}" ]; then
		echo "EXPORT_SRC must be set for a source build."
		exit 1
	fi
        if [ -z "${CRYPT_SRC}" ]; then
		echo "CRYPT_SRC must be set for a source build."
		exit 1
	fi
fi

if [ "$SH_FLAG" = "y" ]; then
        if [ -z "${EXPORT_SRC}" ]; then
		echo "EXPORT_SRC must be set for a source build."
		exit 1
	fi
fi

#
# Check variables required for realmode builds
#
if [ "$X_FLAG" = "y" ]; then

	args_ok=y
	if [ "$NTSERVER" = "" ]; then
		echo "NTSERVER: must be set for a realmode build"
		args_ok=n
	fi
	ping $NTSERVER >/dev/null 2>&1
	if [ ! "$?" = "0" ]; then
		echo "NTSERVER: no response - machine may be down"
		args_ok=n
	fi

	if [ "$IA32_IHV_WS" = "" ]; then
		echo "IA32_IHV_WS: must be set for a realmode build"
		args_ok=n
	fi
	if [ ! -d "$IA32_IHV_WS" ]; then
		echo "$IA32_IHV_WS: not found"
		args_ok=n
	fi

	if [ "$IA32_IHV_ROOT" = "" ]; then
		echo "IA32_IHV_ROOT: must be set for a realmode build"
		args_ok=n
	fi
	if [ ! -d "$IA32_IHV_ROOT" ]; then
		echo "$IA32_IHV_ROOT: not found"
		args_ok=n
	fi

	if [ "$IA32_IHV_PKGS" = "" ]; then
		echo "IA32_IHV_PKGS: must be set for a realmode build"
		args_ok=n
	fi
	# non-DEBUG IHV packages required
	if [ ! -d ${IA32_IHV_PKGS}-nd ]; then
		echo "${IA32_IHV_PKGS}-nd: not found"
		args_ok=n
	fi

	if [ "$IA32_IHV_BINARY_PKGS" = "" ]; then
		echo "IA32_IHV_BINARY_PKGS: must be set for a realmode build"
		args_ok=n
	fi
	if [ ! -d "$IA32_IHV_BINARY_PKGS" ]; then
		echo "$IA32_IHV_BINARY_PKGS: not found"
		args_ok=n
	fi

	if [ "$MTOOLS_PATH" = "" ]; then
		echo "MTOOLS_PATH: must be set for a realmode build"
		args_ok=n
	fi
	if [ ! -d "$MTOOLS_PATH" ]; then
		echo "$MTOOLS_PATH: not found"
		args_ok=n
	fi

	if [ "$DCB_ROOT" = "" ]; then
		echo "DCB_ROOT: must be set for a realmode build"
		args_ok=n
	fi

	if [ "$BOOTFLOPPY_ROOT" = "" ]; then
		echo "BOOTFLOPPY_ROOT: must be set for a realmode build"
		args_ok=n
	fi

	if [ "$SPARC_RM_PKGARCHIVE" = "" ]; then
		echo "SPARC_RM_PKGARCHIVE: must be set for a realmode build"
		args_ok=n
	fi
	if [ "$args_ok" = "n" ]; then
		exit 1
	fi
fi

#
# if -V flag was given, reset VERSION to V_ARG
#
if [ "$V_FLAG" = "y" ]; then
	VERSION=$V_ARG
fi

# Append source version
if [ "$SE_FLAG" = "y" ]; then
	VERSION="${VERSION}:EXPORT"
fi

if [ "$SD_FLAG" = "y" ]; then
	VERSION="${VERSION}:DOMESTIC"
fi

if [ "$SH_FLAG" = "y" ]; then
	VERSION="${VERSION}:MODIFIED_SOURCE_PRODUCT"
fi

TMPDIR="/tmp/nightly.tmpdir.$$"
export TMPDIR
rm -rf $TMPDIR
mkdir -p $TMPDIR

CH=
export  CH

unset   CFLAGS LD_LIBRARY_PATH

[ -d $CODEMGR_WS ] || mkdir -p $CODEMGR_WS && chown $STAFFER $CODEMGR_WS

# since this script assumes the build is from full source, it nullifies
# variables likely to have been set by a "ws" script; nullification
# confines the search space for headers and libraries to the proto area
# built from this immediate source.
ENVLDLIBS1=
ENVLDLIBS2=
ENVLDLIBS3=
ENVCPPFLAGS1=
ENVCPPFLAGS2=
ENVCPPFLAGS3=
ENVCPPFLAGS4=

export ENVLDLIBS3 ENVCPPFLAGS1 ENVCPPFLAGS2 ENVCPPFLAGS3 ENVCPPFLAGS4

ENVLDLIBS1="-L$ROOT/usr/lib -L$ROOT/usr/ccs/lib"
ENVCPPFLAGS1="-I$ROOT/usr/include"

export ENVLDLIBS1 ENVLDLIBS2

CPIODIR_ORIG=$CPIODIR
PKGARCHIVE_ORIG=$PKGARCHIVE
IA32_IHV_PKGS_ORIG=$IA32_IHV_PKGS
SPARC_RM_PKGARCHIVE_ORIG=$SPARC_RM_PKGARCHIVE

#
# 	Ensure no other instance of this script is running
#	LOCKNAME should be set in <env_file>
#
if [ -f /tmp/$LOCKNAME ]; then
	echo "${MACH} build of `basename ${CODEMGR_WS}` already running."
	exit 0
else
	touch /tmp/$LOCKNAME
fi
#
# Create mail_msg_file
#
mail_msg_file="/tmp/mail_msg.$$"
touch $mail_msg_file
build_time_file="/tmp/build_time.$$"
#
# 	Remove this lock on any exit
#
trap "rm -f /tmp/$LOCKNAME $mail_msg_file $build_time_file; exit 0" 0 1 2 3 15
#
# 	Move old LOGFILE aside; make ATLOG directory if missing.
#
if [ -f $LOGFILE ]; then
	mv -f $LOGFILE ${LOGFILE}-
else
	[ -d $ATLOG ] || mkdir -p $ATLOG
fi
#
#	Build OsNet source
#
START_DATE=`date`
SECONDS=0
echo "\n==== Nightly $maketype build started:   $START_DATE ====" \
    | tee -a $LOGFILE > $build_time_file

echo "\n==== list of environment variables ====\n" >> $LOGFILE
env >> $LOGFILE

echo "\n==== Build environment ====\n" | tee -a $mail_msg_file >> $LOGFILE

if [ "$N_FLAG" = "y" ]; then
	if [ "$p_FLAG" = "y" ]; then
		cat <<EOF | tee -a $mail_msg_file >> $LOGFILE
WARNING: the p option (create packages) is set, but so is the N option (do
         not run protocmp); this is dangerous; you should unset the N option
EOF
	else
		cat <<EOF | tee -a $mail_msg_file >> $LOGFILE
Warning: the N option (do not run protocmp) is set; it probably shouldn't be
EOF
	fi
	echo "" | tee -a $mail_msg_file >> $LOGFILE
fi

# System
which uname | tee -a $mail_msg_file >> $LOGFILE
uname -a 2>&1 | tee -a $mail_msg_file >> $LOGFILE
echo "" | tee -a $mail_msg_file >> $LOGFILE

# make
which $MAKE | tee -a $mail_msg_file >> $LOGFILE
echo "number of concurrent jobs = $DMAKE_MAX_JOBS" | \
    tee -a $mail_msg_file >> $LOGFILE
echo "" | tee -a $mail_msg_file >> $LOGFILE

# C compiler
# first try our workspace, then the parent, then give up.
if [ -f $SRC/Makefile.master ]; then
	cc_version $SRC/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
elif [ -f $BRINGOVER_WS/usr/src/Makefile.master ]; then
	cc_version $BRINGOVER_WS/usr/src/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
else
	echo "Unable to find Makefile.master in $BRINGOVER_WS or $SRC." | \
	    tee -a $mail_msg_file >> $LOGFILE
fi
echo "" | tee -a $mail_msg_file >> $LOGFILE

# 64-bit C compiler
# first try our workspace, then the parent, then give up.
if [ -f $SRC/Makefile.master ]; then
	echo "64-bit compiler" | tee -a $mail_msg_file >> $LOGFILE
	cc_version64 $SRC/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
	echo "" | tee -a $mail_msg_file >> $LOGFILE
elif [ -f $BRINGOVER_WS/usr/src/Makefile.master ]; then
	echo "64-bit compiler" | tee -a $mail_msg_file >> $LOGFILE
	cc_version64 $BRINGOVER_WS/usr/src/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
	echo "" | tee -a $mail_msg_file >> $LOGFILE
fi

# Java compiler
# first try our workspace, then the parent, then give up.
if [ -f $SRC/Makefile.master ]; then
	java_version $SRC/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
elif [ -f $BRINGOVER_WS/usr/src/Makefile.master ]; then
	java_version $BRINGOVER_WS/usr/src/Makefile.master | \
	    tee -a $mail_msg_file >> $LOGFILE
else
	echo "Unable to find Makefile.master in $BRINGOVER_WS or $SRC." | \
	    tee -a $mail_msg_file >> $LOGFILE
fi
echo "" | tee -a $mail_msg_file >> $LOGFILE

# as
which as | tee -a $mail_msg_file >> $LOGFILE
as -V 2>&1 | head -1 | tee -a $mail_msg_file >> $LOGFILE

echo "\n==== Build version ====\n" | tee -a $mail_msg_file >> $LOGFILE
echo $VERSION | tee -a $mail_msg_file >> $LOGFILE

#
# 	Decide whether to clobber
#
if [ "$i_FLAG" = "n" -a -d "$SRC" ]; then
	echo "\n==== Make clobber at `date` ====\n" >> $LOGFILE

	cd $SRC
	# remove old clobber file
	rm -f $SRC/clobber.out

	rm -f $SRC/clobber-${MACH}.out
	$MAKE -ek clobber 2>&1 | tee -a $SRC/clobber-${MACH}.out >> $LOGFILE
	echo "\n==== Make clobber ERRORS ====\n" >> $mail_msg_file
	grep "$MAKE:" $SRC/clobber-${MACH}.out >> $mail_msg_file

	if [ "$X_FLAG" = "y" ]; then
		echo "\n==== Make realmode clobber at `date` ====\n" >> $LOGFILE
		cd $SRC/realmode
		rm -f $SRC/realmode/clobber-${MACH}.out
		$MAKE -ek clobber 2>&1 | \
			tee -a $SRC/realmode/clobber-${MACH}.out >> $LOGFILE
		echo "\n==== Make realmode clobber ERRORS ====\n" \
			>> $mail_msg_file
		grep "$MAKE:" $SRC/realmode/clobber-${MACH}.out \
			>> $mail_msg_file
	fi

	rm -rf $ROOT
	if [ "$X_FLAG" = "y" ]; then
		rm -rf $DCB_ROOT
		rm -rf $BOOTFLOPPY_ROOT
	fi

	# Get back to a clean workspace as much as possible to catch
	# problems that only occur on fresh workspaces.
	# Remove all .make.state* files, libraries, and .o's that may
	# have been ommitted from clobber.
	# We should probably blow away temporary directories too.
	find . -name SCCS -prune -o \
	    \( -name '.make.*' -o -name 'lib*.a' -o -name 'lib*.so*' -o \
	       -name '*.o' \) \
	    -exec rm -f {} \;
else
	echo "\n==== No clobber at `date` ====\n" >> $LOGFILE
fi

#
# 	Decide whether to bringover to the codemgr workspace 
#
if [ "$n_FLAG" = "n" ]; then
	echo "\n==== bringover to $CODEMGR_WS at `date` ====\n" >> $LOGFILE
	# sleep on the parent workspace's lock
	while egrep -s write $BRINGOVER_WS/Codemgr_wsdata/locks
	do
		sleep 120
	done

	echo "\n==== BRINGOVER LOG ====\n" >> $mail_msg_file
	# do update as a member of the staff rather than root
	# STAFFER should be set in <env_file>
	# later in this script, mail is optionally sent to STAFFER
	( su $STAFFER -c '$TEAMWARE/bin/bringover \
	   -c "nightly update" -p '$BRINGOVER_WS' \
	   -w '$CODEMGR_WS' usr/src' ) < /dev/null 2>&1 | \
	    tee -a  $mail_msg_file >> $LOGFILE
	if [ $? -eq 1 ]
	then
        	echo "trouble with bringover, quitting at `date`." >> $LOGFILE
		exit 1
	fi
	if [ -d $SRC/cmd/lp/cmd/lpsched/lpsched -a \
	    ! -f $SRC/cmd/lp/cmd/lpsched/lpsched/Makefile ]; then
		# on297 printing
		rm -rf $SRC/cmd/lp/cmd/lpsched/lpsched
	fi
	if [ -d $SRC/cmd/localedef/localedef -a \
	    ! -f $SRC/cmd/localedef/localedef/Makefile ]; then
		# on297 CSI project
		rm -rf $SRC/cmd/localedef/localedef
	fi
else
	echo "\n==== No bringover to $CODEMGR_WS ====\n" >> $LOGFILE
fi

# Realmode: copy ihv proto area in addition to the build itself
# Build first since we only build it once.

if [ "$X_FLAG" = "y" ]; then

	# Install IA32 IHV proto area
	copy_ihv_proto

	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	export RELEASE_BUILD ; RELEASE_BUILD=
	unset EXTRA_OPTIONS
	unset EXTRA_CFLAGS

	build_realmode non-DEBUG -nd
fi

if [ "$i_FLAG" = "y" -a "$SH_FLAG" = "y" ]; then
	echo "\n==== NOT Building base OS-Net source ====\n" | \
	    tee -a $LOGFILE >> $mail_msg_file
else
	normal_build
fi

ORIG_SRC=$SRC
BINARCHIVE=${CODEMGR_WS}/bin-${MACH}.cpio.Z

if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
	save_binaries

	echo "\n==== Retrieving SCCS files at `date` ====\n" >> $LOGFILE
	SCCSHELPER=/tmp/sccs-helper.$$
	rm -f ${SCCSHELPER}
cat >${SCCSHELPER} <<EOF
#!/bin/ksh 
cd \$1   
cd .. 
sccs get SCCS >/dev/null 2>&1 
EOF
	cd $SRC
	chmod +x ${SCCSHELPER}
	find . -name SCCS | xargs -L 1 ${SCCSHELPER}
	rm -f ${SCCSHELPER}
fi

if [ "$SD_FLAG" = "y" ]; then
	clone_source ${CODEMGR_WS} ${CRYPT_SRC} CRYPT_SRC
fi

# EXPORT_SRC comes after CRYPT_SRC since a domestic build will need
# $SRC pointing to the export_source usr/src.
if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
	clone_source ${CODEMGR_WS} ${EXPORT_SRC} EXPORT_SRC
fi

if [ "$SD_FLAG" = "y" ]; then
	# drop the crypt files in place.
	cd ${EXPORT_SRC}
	echo "\nextracting crypt_files.cpio.Z onto export_source.\n" \
	    >> ${LOGFILE}
	zcat ${CODEMGR_WS}/crypt_files.cpio.Z | \
	    cpio -idmucvB 2>/dev/null >> ${LOGFILE}
	if [ "$?" = "0" ]; then
		echo "\n==== DOMESTIC extraction succeeded ====\n" \
		    >> $mail_msg_file
	else
		echo "\n==== DOMESTIC extraction failed ====\n" \
		    >> $mail_msg_file
	fi

fi

if [ "$SE_FLAG" = "y" -o "$SD_FLAG" = "y" -o "$SH_FLAG" = "y" ]; then
	# remove proto area here, since we don't clobber
	rm -rf "$ROOT"
	normal_build
fi

if [ "$build_ok" = "y" ]; then
	echo "\n==== Creating protolist system file at `date` ====" \
		>> $LOGFILE
	protolist $ROOT > $ATLOG/proto_list_${MACH}
	echo "==== protolist system file created at `date` ====\n" \
		>> $LOGFILE

	if [ "$N_FLAG" != "y" ]; then
  		echo "\n==== Impact on packages ====\n" >> $mail_msg_file

		# If there is a reference proto list, compare the build's proto
		# list with the reference to see changes in proto areas.
		# Use the current exception list.
		if [ -f $SRC/pkgdefs/etc/exception_list_$MACH ]; then
			ELIST="-e $SRC/pkgdefs/etc/exception_list_$MACH"
		fi
		if [ -f "$REF_PROTO_LIST" ]; then
			# For builds without realmode, add a realmode proto
			# list to comparisons against the reference if
			# the reference includes realmode
			if [ "$MACH" = "i386" ]; then
			    if [ "$X_FLAG" = "y" ]; then
				# For builds with realmode, add a realmode
				# proto to the reference if the reference
				# was built without it
				grep "boot/solaris/devicedb/master" \
					$REF_PROTO_LIST >/dev/null
				if [ ! "$?" = "0" ]; then
				    REF_REALMODE_PROTO="-d $SRC/realmode/pkgdefs/etc/proto_list_$MACH"
				fi
			    else
				# For builds without realmode, add a realmode
				# proto list to comparisons against the
				# reference if the reference includes realmode
				grep "boot/solaris/devicedb/master" \
					$REF_PROTO_LIST >/dev/null
				if [ "$?" = "0" ]; then
				    REALMODE_PROTO_LIST="$SRC/realmode/pkgdefs/etc/proto_list_$MACH"
				fi
			    fi
			fi
			protocmp.terse \
			  "Files in yesterday's proto area, but not today's:" \
			  "Files in today's proto area, but not yesterday's:" \
			  "Files that changed between yesterday and today:" \
			  ${ELIST} \
			  -d $REF_PROTO_LIST \
			  $REF_REALMODE_PROTO \
			  $ATLOG/proto_list_${MACH} \
			  $REALMODE_PROTO_LIST \
				>> $mail_msg_file
		fi
		# Compare the build's proto list with current package
		# definitions to audit the quality of package definitions
		# and makefile install targets. Use the current exception list.
		PKGDEFS_LIST="-d $SRC/pkgdefs"
		if [ "$X_FLAG" = "y" ]; then
			REALMODE_PKGDEFS_LIST="-d $SRC/realmode/pkgdefs"
			REALMODE_ELIST="-e $SRC/realmode/pkgdefs/etc/exception_list_$MACH"
		fi
		protocmp.terse \
		    "Files missing from the proto area:" \
		    "Files missing from packages:" \
		    "Inconsistencies between pkgdefs and proto area:" \
		    ${ELIST} ${REALMODE_ELIST} \
		    ${PKGDEFS_LIST} ${REALMODE_PKGDEFS_LIST} \
		    $ATLOG/proto_list_${MACH} \
		    >> $mail_msg_file
	
		if [ "$X_FLAG" = "y" ]; then
  			echo "\n==== Impact on DCB and BootFloppy ====\n" >> $mail_msg_file

			#
			# DCB proto comparison
			#
			echo "\n==== Creating DCB protolist system file at `date` ====" \
				>> $LOGFILE
			protolist $DCB_ROOT > $ATLOG/proto_list_dcb_${MACH}

			# compare the build's proto list with the reference
			# to see changes in proto areas.
			if [ -f $SRC/realmode/pkgdefs/etc/exception_list_dcb_${MACH} ]; then
			    ELIST="-e $SRC/realmode/pkgdefs/etc/exception_list_dcb_${MACH}"
			fi
			if [ -f "$REF_PROTO_LIST_DCB" ]; then
			    protocmp.terse \
				"Files in yesterday's DCB proto area, but not today's:" \
				"Files in today's DCB proto area, but not yesterday's:" \
				"Files that changed between yesterday and today:" \
				${ELIST} \
				-d $REF_PROTO_LIST_DCB \
				$ATLOG/proto_list_dcb_${MACH} \
				>> $mail_msg_file
			fi
			# Compare the build's proto list with current package
			PKGDEFS_LIST="-d $SRC/realmode/pkgdefs/etc/dcb"
			protocmp.terse \
			    "Files missing from the DCB proto area:" \
			    "Files missing from DCB package:" \
			    "Inconsistencies between DCB pkgdefs and proto area:" \
			    ${ELIST} \
			    ${PKGDEFS_LIST} \
			    $ATLOG/proto_list_dcb_${MACH} \
		    	    >> $mail_msg_file

			#
			# BootFloppy proto comparison
			#

			echo "\n==== Creating BootFloppy protolist system file at `date` ====" \
				>> $LOGFILE
			protolist $BOOTFLOPPY_ROOT > $ATLOG/proto_list_bootfloppy_${MACH}

			# compare the build's proto list with the reference
			# to see changes in proto areas.
			if [ -f "$REF_PROTO_LIST_BOOTFLOPPY" ]; then
			    protocmp.terse \
				"Files in yesterday's BootFloppy proto area, but not today's:" \
				"Files in today's BootFloppy proto area, but not yesterday's:" \
				"Files that changed between yesterday and today:" \
				${ELIST} \
				-d $REF_PROTO_LIST_BOOTFLOPPY \
				$ATLOG/proto_list_bootfloppy_${MACH} \
				>> $mail_msg_file
			fi
# Compare the build's proto list with current package
			PKGDEFS_LIST="-d $SRC/realmode/pkgdefs/etc/BootFloppy"
			protocmp.terse \
			    "Files missing from the BootFloppy proto area:" \
			    "Files missing from BootFloppy package:" \
			    "Inconsistencies between BootFloppy pkgdefs and proto area:" \
			    ${ELIST} \
			    ${PKGDEFS_LIST} \
			    $ATLOG/proto_list_bootfloppy_${MACH} \
		    	    >> $mail_msg_file
		fi
	fi
fi

if [ "$u_FLAG" = "y"  -a "$build_ok" = "y" ]; then
	( su $STAFFER -c \
	 cp' '$ATLOG/proto_list_${MACH}' '$PARENT_WS/usr/src/proto_list_${MACH})
	if [ "$X_FLAG" = "y" ]; then
		( su $STAFFER -c \
			cp' '$ATLOG/proto_list_dcb_${MACH}' \
				'$PARENT_WS/usr/src/proto_list_dcb_${MACH})
		( su $STAFFER -c \
			cp' '$ATLOG/proto_list_bootfloppy_${MACH}' \
				'$PARENT_WS/usr/src/proto_list_bootfloppy_${MACH})
	fi
fi

# Update parent proto area if necessary. This is done now
# so that the proto area has either DEBUG or non-DEBUG kernels.
if [ "$U_FLAG" = "y" -a "$build_ok" = "y" ]; then
	echo "\n==== Copying proto area to $PARENT_ROOT ====\n" | \
	    tee -a $LOGFILE >> $mail_msg_file
	rm -rf $PARENT_WS/proto/root_$MACH/*
	mkdir -p $PARENT_ROOT
	cd $ROOT
	tar cf - . | ( cd $PARENT_ROOT;  umask 0; tar xpf - ) 2>&1 |
		tee -a $mail_msg_file >> $LOGFILE
	if [ "$X_FLAG" = "y" ]; then
		PARENT_BF_ROOT=`dirname $PARENT_ROOT`/`basename $BOOTFLOPPY_ROOT`
		rm -rf $PARENT_BF_ROOT/*
		cd $BOOTFLOPPY_ROOT
		mkdir -p $PARENT_BF_ROOT
		tar cf - . | ( cd $PARENT_BF_ROOT; umask 0; tar xpf - ) 2>&1 |
			tee -a $mail_msg_file >> $LOGFILE

		PARENT_DCB_ROOT=`dirname $PARENT_ROOT`/`basename $DCB_ROOT`
		rm -rf $PARENT_DCB_ROOT/*
		cd $DCB_ROOT
		mkdir -p $PARENT_DCB_ROOT
		tar cf - . | ( cd $PARENT_DCB_ROOT; umask 0; tar xpf - ) 2>&1 |
			tee -a $mail_msg_file >> $LOGFILE
	fi
fi

#
# do shared library interface verification
#

if [ "$A_FLAG" = "y" -a "x"$IROOT != "x" -a "$build_ok" = "y" ]; then
	echo "\n==== Interface changes ====\n"  | \
	    tee -a $LOGFILE >> $mail_msg_file

	if [ "$IBUILD" = "" ]; then
		IBUILD=$CODEMGR_WS/usr/interface; export IBUILD
	fi
	if [ "$IROOT" = "" ]; then
		IROOT=$CODEMGR_WS/interface; export IROOT
	fi
	intf_create -v -r -d $IBUILD 2>/tmp/interface_$$ 1>/dev/null
	cat /tmp/interface_$$  >> $mail_msg_file
	cat /tmp/interface_$$  >> $LOGFILE
	rm -f /tmp/interface_$$

	if [ -f "$IROOT" -o ! -d "$IROOT" ]; then
		rm -rf $IROOT;
		mkdir -p $IROOT;
		if [ -d "$IBUILD" ]; then
			cp -r $IBUILD/* $IROOT;
		fi
	fi

	intf_cmp -p $IBUILD $IROOT > /tmp/interface_$$
	cat /tmp/interface_$$  >> $mail_msg_file
	cat /tmp/interface_$$  >> $LOGFILE
	rm -f /tmp/interface_$$

	cp -r $IBUILD/* $IROOT;
fi

# For now, don't make archives or packages for GPROF/TRACE builds
a_FLAG=n
p_FLAG=n

# GPROF build begins

if [ "$i_CMD_LINE_FLAG" = "n" -a "$P_FLAG" = "y" ]; then

	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	export RELEASE_BUILD ; RELEASE_BUILD=
	export EXTRA_OPTIONS ; EXTRA_OPTIONS="-DGPROF"
	export EXTRA_CFLAGS ; EXTRA_CFLAGS="-xpg"

	build GPROF -prof

else
	echo "\n==== No GPROF build ====\n" >> $LOGFILE
fi

# GPROF build ends

# TRACE build begins

if [ "$i_CMD_LINE_FLAG" = "n" -a "$T_FLAG" = "y" ]; then

	export INTERNAL_RELEASE_BUILD ; INTERNAL_RELEASE_BUILD=
	export RELEASE_BUILD ; RELEASE_BUILD=
	export EXTRA_OPTIONS ; EXTRA_OPTIONS="-DTRACE"
	unset EXTRA_CFLAGS

	build TRACE -trace

else
	echo "\n==== No TRACE build ====\n" >> $LOGFILE
fi

# TRACE build ends

# DEBUG lint of kernel begins

if [ "$i_CMD_LINE_FLAG" = "n" -a "$l_FLAG" = "y" ]; then
	if [ "$LINTDIRS" = "" ]; then
		# LINTDIRS="$SRC/uts y $SRC/stand y $SRC/psm y"
		LINTDIRS="$SRC y"
	fi
	set $LINTDIRS
	while [ $# -gt 0 ]; do
		dolint $1 $2; shift; shift
	done
else
	echo "\n==== No '$MAKE lint' ====\n" >> $LOGFILE
fi

# "make check" begins

if [ "$i_CMD_LINE_FLAG" = "n" -a "$C_FLAG" = "y" ]; then
	# remove old check.out
	rm -f $SRC/check.out

	rm -f $SRC/check-${MACH}.out
	cd $SRC
	$MAKE -ek check 2>&1 | tee -a $SRC/check-${MACH}.out >> $LOGFILE
	echo "\n==== cstyle/hdrchk errors ====\n" >> $mail_msg_file

	nawk '
		$2 == "-->" || $1 == "checking" { next }
		NF == 1 { dir=$1; next }
		$1 ~ /:$/ { print dir "/" $0 }
	' $SRC/check-${MACH}.out | sort | uniq >> $mail_msg_file
else
	echo "\n==== No '$MAKE check' ====\n" >> $LOGFILE
fi

echo "\n==== Find core files ====\n" | \
    tee -a $LOGFILE >> $mail_msg_file

find $SRC -name core -exec file {} \; | \
	tee -a $LOGFILE >> $mail_msg_file

if [ "$r_FLAG" = "y" -a "$build_ok" = "y" ]; then
	echo "\n==== Check ELF runpaths ====\n" | \
	    tee -a $LOGFILE >> $mail_msg_file
	rm -f $SRC/runpath.ref
	if [ -f $SRC/runpath.out ]; then
		mv $SRC/runpath.out $SRC/runpath.ref
	fi
	check_rpaths $ROOT | sort >$SRC/runpath.out
	# probably should compare against a 'known ok runpaths' list
	if [ ! -f $SRC/runpath.ref ]; then
		cp $SRC/runpath.out $SRC/runpath.ref
	fi
	echo "\n==== ELF runpath differences ====\n" \
		>>$mail_msg_file
	diff $SRC/runpath.ref $SRC/runpath.out >>$mail_msg_file
fi

END_DATE=`date`
echo "==== Nightly $maketype build completed: $END_DATE ====" | \
    tee -a $LOGFILE >> $build_time_file

typeset -Z2 minutes
typeset -Z2 seconds

elapsed_time=$SECONDS
((hours = elapsed_time / 3600 ))  
((minutes = elapsed_time / 60  % 60))
((seconds = elapsed_time % 60))

echo "\n==== Total build time ====" | \
    tee -a $LOGFILE >> $build_time_file
echo "\nreal    ${hours}:${minutes}:${seconds}" | \
    tee -a $LOGFILE >> $build_time_file

LLOG="$ATLOG/log.`date '+%m%d'`"
rm -rf $ATLOG/log.??`date '+%d'`
if [ -f $LLOG -o -d $LLOG ]; then
	LLOG=$LLOG.$$
fi

mkdir $LLOG
if [ "$m_FLAG" = "y" ]; then
	cat $build_time_file $mail_msg_file | \
		/usr/bin/mailx -s \
		"Nightly ${MACH} Build of `basename ${CODEMGR_WS}` Completed." \
		${MAILTO} 
fi

cat $build_time_file $mail_msg_file > ${LLOG}/mail_msg
if [ "$u_FLAG" = "y"  -a "$build_ok" = "y" ]; then
	( su $STAFFER -c \
	 cp' '$LOGFILE' '$PARENT_WS/usr/src/nightly-${MACH}.log)
	( su $STAFFER -c \
	 cp' '${LLOG}/mail_msg' '$PARENT_WS/usr/src/mail_msg-${MACH})
fi
mv $LOGFILE $LLOG
rm -f $build_time_file $mail_msg_file
rm -rf $TMPDIR
if [ "$build_ok" = "y" ]; then
	mv $ATLOG/proto_list_${MACH} $LLOG
	if [ "$X_FLAG" = "y" ]; then
		mv $ATLOG/proto_list_dcb_${MACH} $LLOG
		mv $ATLOG/proto_list_bootfloppy_${MACH} $LLOG
	fi
fi
