/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * $Id: exceptions.h,v 1.9 1998/10/01 11:05:04 casper Exp $
 *
 * List of files/directories supposed to be group/world writable
 * May need to be updated for each OS release
 *
 */

#pragma ident	"@(#)exceptions.h	1.1	99/03/22 SMI"

	"/etc/dumpdates",
	"/etc/lp",
	"/var/mail/:saved",
	"/var/preserve",
	/*
	 * Lp stuff is chmod'ed back by the lp system; prevent pkgchk errors
	 * later by listing them here.
	 */
	"/etc/lp/Systems",
	"/etc/lp/classes",
	"/etc/lp/forms",
	"/etc/lp/interfaces",
	"/etc/lp/printers",
	"/etc/lp/pwheels",
	"/var/lp",
	"/var/lp/logs",
	"/var/spool/lp",
	"/var/spool/lp/admins",
	"/var/spool/lp/fifos",
	"/var/spool/lp/fifos/private",
	"/var/spool/lp/fifos/public",
	"/var/spool/lp/requests",
	"/var/spool/lp/system",

	/* another strange logfile */
	"/usr/oasys/tmp/TERRLOG",

	/* /var/adm stuff added because std cron jobs for sys/adm expect this */
	"/var/adm",
	"/var/adm/acct",
	"/var/adm/acct/fiscal",
	"/var/adm/acct/nite",
	"/var/adm/acct/sum",
	"/var/adm/sa",
	"/var/adm/spellhist",

	/* 5.1, 5.2 */
	"/devices/pseudo/clone:ip",
	"/devices/pseudo/clone:ticlts",
	"/devices/pseudo/clone:ticots",
	"/devices/pseudo/clone:ticotsord",
	"/devices/pseudo/clone:udp",
	"/devices/pseudo/cn:console",
	"/devices/pseudo/cn:syscon",
	"/devices/pseudo/cn:systty",
	"/devices/pseudo/log:conslog",
	"/devices/pseudo/mm:null",
	"/devices/pseudo/mm:zero",
	"/devices/pseudo/sad:user",
	"/devices/pseudo/sy:tty",
	/* 5.3, 5.4, 5.5, ... */
	"/devices/pseudo/clone@0:ip",
	"/devices/pseudo/clone@0:ticlts",
	"/devices/pseudo/clone@0:ticots",
	"/devices/pseudo/clone@0:ticotsord",
	"/devices/pseudo/clone@0:udp",
	"/devices/pseudo/clone@0:tcp",
	"/devices/pseudo/clone@0:rts",
	"/devices/pseudo/cn@0:console",
	"/devices/pseudo/cn@0:syscon",
	"/devices/pseudo/cn@0:systty",
	"/devices/pseudo/ksyms@0:ksyms",
	"/devices/pseudo/log@0:conslog",
	"/devices/pseudo/mm@0:null",
	"/devices/pseudo/mm@0:zero",
	"/devices/pseudo/sad@0:user",
	"/devices/pseudo/sy@0:tty",

	/* 5.6 .... */
	"/devices/pseudo/tl@0:ticlts",
	"/devices/pseudo/tl@0:ticots",
	"/devices/pseudo/tl@0:ticotsord",

	/* Starfire console */
	"/devices/pseudo/cvc@0:cvc",
	"/devices/pseudo/cvcredir@0:cvcredir",
