#!/bin/sh
#pragma ident	"@(#)dummy_mech_token.conf.sh	1.2	97/11/13 SMI"
#
cat << EOF > dummy_mech_token.conf.tmp
2
EOF

chmod 0644 dummy_mech_token.conf.tmp
mv dummy_mech_token.conf.tmp  dummy_mech_token.conf
