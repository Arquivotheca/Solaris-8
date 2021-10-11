/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _KERNEL_INT_H
#define	_KERNEL_INT_H

#pragma ident	"@(#)kernel_int.h	1.12	98/01/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "tnfctl_int.h"

/*
 * interfaces to control kernel tracing and kernel probes
 */

tnfctl_errcode_t _tnfctl_prbk_init(tnfctl_handle_t *);
tnfctl_errcode_t _tnfctl_prbk_close(tnfctl_handle_t *);
void _tnfctl_prbk_get_other_funcs(uintptr_t *allocp, uintptr_t *commitp,
	uintptr_t *rollbackp, uintptr_t *endp);
void _tnfctl_prbk_test_func(uintptr_t *);
tnfctl_errcode_t _tnfctl_prbk_buffer_alloc(tnfctl_handle_t *, int);
tnfctl_errcode_t _tnfctl_prbk_buffer_dealloc(tnfctl_handle_t *);
tnfctl_errcode_t _tnfctl_prbk_set_tracing(tnfctl_handle_t *, boolean_t);
tnfctl_errcode_t _tnfctl_prbk_set_pfilter_mode(tnfctl_handle_t *, boolean_t);
tnfctl_errcode_t _tnfctl_prbk_get_pfilter_list(tnfctl_handle_t *, pid_t **,
	int *);
tnfctl_errcode_t _tnfctl_prbk_pfilter_add(tnfctl_handle_t *, pid_t);
tnfctl_errcode_t _tnfctl_prbk_pfilter_delete(tnfctl_handle_t *, pid_t);
tnfctl_errcode_t _tnfctl_prbk_flush(tnfctl_handle_t *, prbctlref_t *);
tnfctl_errcode_t _tnfctl_refresh_kernel(tnfctl_handle_t *hndl);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_INT_H */
