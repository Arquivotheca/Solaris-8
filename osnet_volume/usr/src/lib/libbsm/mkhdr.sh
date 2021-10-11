#!/bin/sh
#
# mkhdr.sh
#
# Automagically generate the audit_uevents.h header file.
#
DATABASE=audit_event.txt
HEADER_FILE=audit_uevents.h

cat <<EOF > $HEADER_FILE
/*
 * Copyright (c) 1993, Sun Microsystems, Inc.
 */

#ifndef	_BSM_AUDIT_UEVENTS_H
#define	_BSM_AUDIT_UEVENTS_H

#pragma ident	"@(#)$HEADER_FILE	1.6	93/06/24 SMI"

/*
 * User level audit event numbers.
 *
 * 0			Reserved as an invalid event number.
 * 1		- 2047	Reserved for the SunOS kernel.
 * 2048		- 32767 Reserved for the SunOS TCB.
 * 32768	- 65535	Available for other TCB applications.
 *
 */

#ifdef	__cplusplus
extern "C" {
#endif

EOF

nawk -F: '{if ((NF == 4) && substr($1,0,1) != "#")
		if ($1 >= 2048) {
			printf("#define	%s	",$2)
			if (length($2) < 8)
				printf("	")
			if (length($2) < 16)
				printf("	")
			printf("%s	/* =%s %s */\n",$1,$4,$3)
		}
	  }' \
< $DATABASE >> $HEADER_FILE

cat <<EOF >> $HEADER_FILE

#ifdef	__cplusplus
}
#endif

#endif	/* _BSM_AUDIT_UEVENTS_H */
EOF

exit 0
