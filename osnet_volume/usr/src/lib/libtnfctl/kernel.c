/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)kernel.c	1.9	96/05/20 SMI"

/*
 * Published kernel specific interfaces for libtnfctl.
 * After doing some error checking, these just turn around and call
 * internal interfaces that do the real work.
 */

#include <stdio.h>
#include <errno.h>

#include "tnfctl_int.h"
#include "kernel_int.h"

tnfctl_errcode_t
tnfctl_trace_state_set(tnfctl_handle_t *hdl, boolean_t mode)
{
	if (hdl->mode != KERNEL_MODE)
		return (TNFCTL_ERR_BADARG);

	/* KERNEL_MODE */
	return (_tnfctl_prbk_set_tracing(hdl, mode));
}

tnfctl_errcode_t
tnfctl_filter_state_set(tnfctl_handle_t *hdl, boolean_t mode)
{
	if (hdl->mode != KERNEL_MODE)
		return (TNFCTL_ERR_BADARG);

	/* KERNEL_MODE */
	return (_tnfctl_prbk_set_pfilter_mode(hdl, mode));
}

tnfctl_errcode_t
tnfctl_filter_list_get(tnfctl_handle_t *hdl, pid_t **pid_list, int *pid_count)
{
	if (hdl->mode != KERNEL_MODE)
		return (TNFCTL_ERR_BADARG);

	/* KERNEL_MODE */
	return (_tnfctl_prbk_get_pfilter_list(hdl, pid_list, pid_count));
}

tnfctl_errcode_t
tnfctl_filter_list_add(tnfctl_handle_t *hdl, pid_t pid)
{
	if (hdl->mode != KERNEL_MODE)
		return (TNFCTL_ERR_BADARG);

	/* KERNEL_MODE */
	return (_tnfctl_prbk_pfilter_add(hdl, pid));
}

tnfctl_errcode_t
tnfctl_filter_list_delete(tnfctl_handle_t *hdl, pid_t pid)
{
	if (hdl->mode != KERNEL_MODE)
		return (TNFCTL_ERR_BADARG);

	/* KERNEL_MODE */
	return (_tnfctl_prbk_pfilter_delete(hdl, pid));
}
