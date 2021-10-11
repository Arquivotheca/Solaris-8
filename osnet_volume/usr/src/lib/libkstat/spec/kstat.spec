#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)kstat.spec	1.2	99/05/14 SMI"
#
# lib/libkstat/spec/kstat.spec

function	kstat_chain_update
include		<kstat.h>
declaration	kid_t kstat_chain_update(kstat_ctl_t *kc)
version		SUNW_0.7
exception	((int)$return == -1)
end

function	kstat_lookup
include		<kstat.h>
declaration	kstat_t *kstat_lookup(kstat_ctl_t *kc, char *ks_module, \
			int ks_instance, char	*ks_name)
version		SUNW_0.7
errno		
exception	($return == 0)
end

function	kstat_data_lookup
include		<kstat.h>
declaration	void *kstat_data_lookup(kstat_t *ksp, char *name)
version		SUNW_0.7
exception	($return == 0)
end

function	kstat_open
include		<kstat.h>
declaration	kstat_ctl_t *kstat_open(void)
version		SUNW_0.7
exception	($return == 0)
end

function	kstat_close
include		<kstat.h>
declaration	int kstat_close(kstat_ctl_t *kc)
version		SUNW_0.7
exception	($return == -1)
end

function	kstat_read
include		<kstat.h>
declaration	kid_t kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *buf)
version		SUNW_0.7
exception	($return == -1)
end

function	kstat_write
include		<kstat.h>
declaration	kid_t kstat_write(kstat_ctl_t *kc, kstat_t *ksp, void *buf)
version		SUNW_0.7
exception	($return == -1)
end
