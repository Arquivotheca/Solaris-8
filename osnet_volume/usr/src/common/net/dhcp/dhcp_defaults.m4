divert(-1)
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# @(#)dhcp_defaults.m4	1.1	99/10/21 SMI
#
# Select the appropriate type of output format based on whether -Djava is set
# on the command line
ifdef(`java', `define(defdef, `    public static final String	$1 = "$1";')', `define(defdef, `defint($1,"$1")')')
ifdef(`java', `define(defstr, `    public static final String	$1 = $2;')', `define(defstr, `defint($1,$2)')')
ifdef(`java', `define(defint, `    public static final int	$1 = $2;')', `define(defint, `#define	$1	$2')')
# End of opening definitions; everything after next line is going in the output
divert(0)dnl
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * This include file is generated from a m4 source file. Do not
 * modify this file.
 */

ifdef(`java', package com.sun.dhcpmgr.data;, `#ifndef _DHCP_DEFAULTS_H')
ifdef(`java', ,`#define	_DHCP_DEFAULTS_H

`#pragma ident	"%Z'`%%'`M%	%'`I%	%'`E% SMI"'
')
ifdef(`java', public interface DhcpDefaults {, `#ifdef	__cplusplus')
ifdef(`java', `dnl', extern "C" {)
ifdef(`java', `dnl', `#endif
')
/* Definitions for valid defaults file parameters */
defdef(RUN_MODE)
defdef(VERBOSE)
defdef(RELAY_HOPS)
defdef(INTERFACES)
defdef(ETHERS_COMPAT)
defdef(ICMP_VERIFY)
defdef(OFFER_CACHE_TIMEOUT)
defdef(RESCAN_INTERVAL)
defdef(LOGGING_FACILITY)
defdef(BOOTP_COMPAT)
defdef(RELAY_DESTINATIONS)
defdef(RESOURCE)
defdef(PATH)

/* Definitions for valid RESOURCE settings */
defstr(NISPLUS, "nisplus")
defstr(FILES, "files")

/* Definitions for valid BOOTP_COMPAT settings */
defstr(AUTOMATIC, "automatic")
defstr(MANUAL, "manual")

/* Definitions for valid RUN_MODE settings */
defstr(RELAY, "relay")
defstr(SERVER, "server")

/* Definitions for valid boolean values */
defstr(DEFAULT_TRUE, "TRUE")
defstr(DEFAULT_FALSE, "FALSE")

/* Definitions for server defaults for unspecified options */
defint(DEFAULT_HOPS, 4)
defint(DEFAULT_OFFER_TTL, 10)

ifdef(`java', `dnl', `#ifdef	__cplusplus')
}
ifdef(`java', `dnl', `#endif
')
ifdef(`java', `dnl', `#endif	/* !_DHCP_DEFAULTS_H */')
