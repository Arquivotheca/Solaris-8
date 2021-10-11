#!/bin/sh

#
#ident	"@(#)fnsetup.sh 1.2	94/07/27 SMI"
#

PATH=$PATH:/usr/lib/nis:/usr/sbin:/usr/bin
CREATE=fncreate

if [ $# -eq 0 ]
then
	D=`domainname`.
	D=`echo $D | sed -e "s/\.\./\./gp"`
else
	D=$1.
	D=`echo $D | sed -n -e "s/\.\./\./gp"`
fi

out()
{
	echo "-- Installation not complete --"		
	exit 1
}


if nistest $D 
then
	:
else
	echo "NIS+ service is not available for domain '$D'"
	echo "Please install and run the NIS+ service."
	out
fi

if nistest org_dir.$D
then
	:
else
	echo ""
	echo "No NIS+ 'org_dir' directory for domain '$D'"
	echo "Creating an org_dir.$D directory for system tables."
	echo "Note that the org_dir directroy will not be populated"
	echo "with the system tables. For a complete installation of"
        echo "NIS+ and system tables see relevant administrative manuals."
	echo ""
	if nismkdir org_dir.$D
	then
		:
	else
		echo ""
		echo "Could not create NIS+ directory 'org_dir.$D'"
		echo ""
		out
	fi
fi

if $CREATE -t org -ov org/$D/ >/dev/null
then
		:
else
		echo ""
		echo "Could not create "org/$D/" context"
		echo "Check to see if you have permission to create/write"
		echo "NIS+ directory: ctx_dir.$D"
		echo ""
		out
fi

if $CREATE -t service -ov org/$D/service/ >/dev/null
then
	:
else
	echo ""
	echo "Could not create "org/$D/service/" context"
	echo "Check to see if you have permission to create/write"
	echo "NIS+ directory: service.ctx_dir.$D"
	echo ""
	out
fi

echo "Installation was successful"
exit 0
