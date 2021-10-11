/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRT_CONF_H
#define	_PRT_CONF_H

#pragma ident	"@(#)prtconf.h	1.1	99/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <libdevinfo.h>
#include <sys/utsname.h>

extern void init_priv_data(struct di_priv_data *);
extern void dump_priv_data(int, di_node_t);
extern void indent_to_level(int);
extern void prtconf_devinfo();
extern int do_fbname();
extern int do_promversion();
extern int do_prom_version64(void);
extern int do_prominfo();
void indent_to_level(int);

extern void dprintf(const char *, ...);

struct prt_opts {
	int o_verbose;
	int o_drv_name;
	int o_pseudodevs;
	int o_fbname;
	int o_noheader;
	int o_prominfo;
	int o_promversion;
	int o_prom_ready64;
	const char *o_promdev;
	const char *o_progname;
	struct utsname o_uts;
};

struct prt_dbg {
	int d_debug;
	int d_bydriver;
	int d_forceload;
	char *d_drivername;
};

extern struct prt_opts opts;
extern struct prt_dbg dbg;

#ifdef	__cplusplus
}
#endif

#endif	/* _PRT_CONF_H */
