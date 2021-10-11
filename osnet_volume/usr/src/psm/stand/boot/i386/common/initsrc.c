/*
 * Copyright (c) 1995,1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)initsrc.c	1.4	98/03/10 SMI"

/* initial source executed at startup */

unsigned char config_source[] =
	"source /boot/solaris/boot.rc\n";

unsigned char old_config_source[] =
	"source /platform/i86pc/boot/solaris/boot.rc\n";

unsigned char boot_source[] =
	"source /etc/bootrc\n";
