/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi_dki_impl.c	1.2	99/10/07 SMI"

/*
 * ddi_dki_impl.c - A pseudo-kernel to use when analyzing drivers with warlock.
 *
 * The main idea here is to represent all of the ways that the kernel can
 * call into the driver, so that warlock has the correct view of the call
 * graph.
 *
 * This version differs from ddi_dki_spec.c in that it represents the
 * current implementation of the DDI/DKI rather than the specification.
 */
#include "ddi_dki_comm.inc"
#include <sys/esunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi.h>


main() {

	/*
	 * The following call will cause warlock to know about
	 * warlock_dummy as a func that can be used to satisfy
	 * unbound function pointers.  It shouldn't be needed
	 * with the new warlock on suntools.
	 */
	warlock_dummy();

	/*
	 * When the following functions are called, there is never
	 * more than one thread running in the driver.
	 */
	_init();
	_fini();
	_info();
	(*devops_p->devo_identify)(0);
	(*devops_p->devo_probe)(0);
	(*devops_p->devo_attach)(0, 0);

	/*
	 * When the following functions are called, there may be
	 * more than one thread running in the driver.
	 */
	_NOTE(COMPETING_THREADS_NOW)


	scsi_init();

	(*devops_p->devo_getinfo)(0, 0, 0, 0);
	(*devops_p->devo_reset)(0, 0);
	(*devops_p->devo_power)(0, 0, 0);

	(*cbops_p->cb_open)(0, 0, 0, 0);
	(*cbops_p->cb_close)(0, 0, 0, 0);
	(*cbops_p->cb_strategy)(0);
	(*cbops_p->cb_print)(0, 0);
	(*cbops_p->cb_dump)(0, 0, 0, 0);
	(*cbops_p->cb_read)(0, 0, 0);
	(*cbops_p->cb_write)(0, 0, 0);
	(*cbops_p->cb_ioctl)(0, 0, 0, 0, 0, 0);
	(*cbops_p->cb_devmap)(0, 0, 0, 0, 0, 0);
	(*cbops_p->cb_mmap)(0, 0, 0);
	(*cbops_p->cb_segmap)(0, 0, 0, 0, 0, 0, 0, 0, 0);
	(*cbops_p->cb_chpoll)(0, 0, 0, 0, 0);
	(*cbops_p->cb_prop_op)(0, 0, 0, 0, 0, 0, 0);
	(*cbops_p->cb_aread)(0, 0, 0);
	(*cbops_p->cb_awrite)(0, 0, 0);

	(*busops_p->bus_map)(0, 0, 0, 0, 0, 0);
	(*busops_p->bus_get_intrspec)(0, 0, 0);
	(*busops_p->bus_add_intrspec)(0, 0, 0, 0, 0, 0, 0, 0);
	(*busops_p->bus_remove_intrspec)(0, 0, 0, 0);
	(*busops_p->bus_map_fault)(0, 0, 0, 0, 0, 0, 0, 0, 0);
	(*busops_p->bus_dma_map)(0, 0, 0, 0);
	(*busops_p->bus_dma_ctl)(0, 0, 0, 0, 0, 0, 0, 0);
	(*busops_p->bus_ctl)(0, 0, 0, 0, 0);
	(*busops_p->bus_prop_op)(0, 0, 0, 0, 0, 0, 0, 0);

	(*busops_p->bus_get_eventcookie)(0, 0, 0, 0, 0, 0);
	(*busops_p->bus_add_eventcall)(0, 0, 0, 0, 0);
	(*busops_p->bus_remove_eventcall)(0, 0, 0);
	(*busops_p->bus_post_event)(0, 0, 0, 0);

	ndi_devi_offline(0, 0);
	_NOTE(NO_COMPETING_THREADS_NOW)
}

ndi_devi_offline(dev_info_t *dip, uint_t flags) {
	(*busops_p->bus_dma_ctl)(0, 0, 0, 0, 0, 0, 0, 0);
	(*busops_p->bus_ctl)(0, 0, 0, 0, 0);
	(*busops_p->bus_get_eventcookie)(0, 0, 0, 0, 0, 0);
	(*busops_p->bus_add_eventcall)(0, 0, 0, 0, 0);
	(*busops_p->bus_remove_eventcall)(0, 0, 0);
	(*busops_p->bus_post_event)(0, 0, 0, 0);
}
