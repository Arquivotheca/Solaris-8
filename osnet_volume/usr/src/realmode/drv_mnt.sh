#!/bin/ksh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)drv_mnt.sh 1.11 99/10/11"

# build realmode sources on an NT server.

TARGET=$@

NTSERVER=${NTSERVER=ntre}
NTSERVER_PRIMARY=$NTSERVER
NTSERVER_SECONDARY=${NTSERVER_SECONDARY=ntre-backup}
NTSERVER_LIST="$NTSERVER_PRIMARY $NTSERVER_SECONDARY"
ntserver=

for server in $NTSERVER_LIST
do
	ping $server > /dev/null 2>&1 &
	if [ $? -eq 0 ]; then
		ntserver=$server
		break
	fi
done

if [ -n "$ntserver" ]; then
	echo "Using \"$ntserver\" as the NT server ..."
else
	echo "Error: NT server(s) \"$NTSERVER_LIST\" are not responding."
	exit 1
fi
		
AUTOFSSERVER=${AUTOFSSERVER=df1}
FS=`pwd`
NODE=$(uname -n)
FS_TYPE=$(df -n $FS |awk -F: '{print $2}')
MNT_PT=$(df -k $FS|sed 1d|awk '{print $6}'|sed -e 's/.*net\///')
BASE=$(basename $MNT_PT)
df -F ufs $FS > /dev/null 2>&1 && NODE_NM=$NODE && LOCAL=1 || \
		 NODE_NM=$(dirname $MNT_PT|sed 's/\/.*//') 

# if NODE_NM is not set yet, then the filesystem is exported
# from the samba server.

[ -z "$NODE_NM" ] && NODE_NM=$SMBSERVER && HOME_WS=1

do_make ()
{
# set up the bat file

[ -f dos/rmake.bat ] && rm -f dos/rmake.bat
cp build.bat dos/rmake.bat
chmod +w dos/rmake.bat

# For sparc, build mboot only.
case `uname -p` in
	i386)  DOS_SUBDIR= ;;
	sparc) DOS_SUBDIR="hd\mboot" ;;
esac

SOL_SUBDIR=`echo "${DOS_SUBDIR}" | sed 's:\\\:/:g'`
rm -f dos/${SOL_SUBDIR}/.done

ed - dos/rmake.bat << EOF > /dev/null 2>&1
1,$ s/TARGET/$TARGET
/DEV_NM
d
i
$DEV_NM 
.
/NT_PATH
d
i
cd ${NT_PATH}\dos\\${DOS_SUBDIR}
.
w
q
EOF
rsh -n $ntserver "$DEV_NM$NT_PATH\dos\rmake.bat"
rsh -n $ntserver "net use $DEV_NM /DELETE"
rm -f dos/rmake.bat
[ ! -f dos/${SOL_SUBDIR}/.done ] && exit 1
rm -f dos/${SOL_SUBDIR}/.done
exit 0
}

#
# main ()
#
# find the service name by looking at the config file in samba directory

if [ $FS_TYPE = "ufs" ]
then
	NFS_PATH=/$NODE$MNT_PT
	NT_PT=$(echo $NFS_PATH|sed -e 's/\//\\\\/' -e 's/\//\'\\'/g')
			
elif [ $FS_TYPE = "nfs" ]
then
	# Workaround for workspaces under /la in LASC.
	if expr "$FS" : "^/la/" > /dev/null ; then
		FILE_SRVR=$(df -k $FS| sed -e 1d -e 's/:.*//g' -e 's/[0-9]*$//')
		NT_PT='\\'${FILE_SRVR}'\'la
		BASE="la"
	else
		NFS_PATH=/$(df -k $FS| sed -e 1d -e 's/://g' | awk '{print $1}')
		NT_PT=$(echo $NFS_PATH|sed -e 's/\//\\\\/' -e 's/\//\'\\'/g')
	fi
elif [ $FS_TYPE = "autofs" ]
then
	NFS_PATH=$(df $FS|awk '{print $1}')
	FS_PT=$(echo $NFS_PATH| sed -e 's/\//\'\\'/g')
	NT_PT=\\$AUTOFSSERVER$FS_PT
fi
NT_PATH=$(echo $FS|sed -e 's/.*'"$BASE"'//' -e 's/\//\\/g')

# run 'net use' on the NT server

DEV_NM=$(rsh -n $ntserver net use '*' $NT_PT | \
		 awk '{print $2}'|sed 3d )
rsh -n $ntserver "dir $DEV_NM$NT_PATH" > /tmp/dos_err.$$ 2>&1
egrep "cannot find|denied|Not Found" /tmp/dos_err.$$ > /dev/null 
rc=$?
if [ "$rc" = 0 ]; then
	rsh -n $ntserver "net use $DEV_NM /DELETE" > /dev/null 
         rm /tmp/dos_err.$$
else
	do_make
fi

exit 1
