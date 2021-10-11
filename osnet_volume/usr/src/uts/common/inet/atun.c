/*
 * Copyright (c) 1995-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* Automatic tunnel module */

#pragma ident   "@(#)atun.c 1.1     99/03/21 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>

#include <sys/socket.h>
#include <sys/isa_defs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/arp.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <net/if_dl.h>
#include <inet/ip_if.h>
#include <inet/tun.h>

#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/stat.h>

/* streams linkages */
static struct module_info atuninfo = {
	ATUN_MODID, ATUN_NAME, 1, INFPSZ, 65536, 1024
};

static struct qinit atunrinit = {
	(pfi_t)tun_rput,
	(pfi_t)tun_rsrv,
	tun_open,
	tun_close,
	NULL,
	&atuninfo,
	NULL
};

static struct qinit atunwinit = {
	(pfi_t)tun_wput,
	(pfi_t)tun_wsrv,
	NULL,
	NULL,
	NULL,
	&atuninfo,
	NULL
};

static struct streamtab atun_strtab = {
		&atunrinit, &atunwinit, NULL, NULL
};

extern struct mod_ops mod_strmodops;

static struct fmodsw atun_fmodsw = {
	ATUN_NAME,
	&atun_strtab,
	(D_NEW | D_MP | D_MTQPAIR | D_MTPUTSHARED)
	};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "auto-tunneling module", &atun_fmodsw
	};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *) &modlstrmod,
	NULL
	};



_init()
{
	return (mod_install(&modlinkage));
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
