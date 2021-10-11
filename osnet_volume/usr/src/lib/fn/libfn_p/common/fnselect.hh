/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSELECT_HH
#define	_FNSELECT_HH

#pragma ident	"@(#)fnselect.hh	1.1	96/03/31 SMI"

#define	FNS_CONFIG_FILE	"/etc/fn.conf"
#define	FNS_NS_PREFIX	"enterprise:"

enum FNSP_naming_service_types {
	FNSP_default_ns = 100,
	FNSP_unknown_ns = 0,
	FNSP_nisplus_ns = 1,
	FNSP_nis_ns = 2,
	FNSP_files_ns = 3
};

extern int fnselect();
extern int fnselect_from_config_file();
extern int fnselect_from_probe();

extern int FNSP_get_naming_service_type(const char *);
extern const char *FNSP_naming_service_name(int ns);

#endif	/* _FNSELECT_HH */
