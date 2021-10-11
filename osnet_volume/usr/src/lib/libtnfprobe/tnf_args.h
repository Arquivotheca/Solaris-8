/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TNF_ARGS_H
#define	_TNF_ARGS_H

#pragma ident	"@(#)tnf_args.h	1.10	97/10/29 SMI"

#include <tnf/probe.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * In process interface for getting probe arguments and types and attributes
 */

#define	tnf_probe_get_int(x) 		(*((int *)(x)))
#define	tnf_probe_get_uint(x) 		(*((unsigned int *)(x)))
#define	tnf_probe_get_long(x) 		(*((long *)(x)))
#define	tnf_probe_get_ulong(x) 		(*((unsigned long *)(x)))
#define	tnf_probe_get_longlong(x) 	(*((long long *)(x)))
#define	tnf_probe_get_ulonglong(x) 	(*((unsigned long long *)(x)))
#define	tnf_probe_get_float(x) 		(*((float *)(x)))
#define	tnf_probe_get_double(x) 	(*((double *)(x)))

char * tnf_probe_get_chars(void *);

int tnf_probe_get_num_args(tnf_probe_control_t *);

void *tnf_probe_get_arg_indexed(tnf_probe_control_t *, int, void *);

tnf_arg_kind_t tnf_probe_get_type_indexed(tnf_probe_control_t *, int);

const char * tnf_probe_get_value(tnf_probe_control_t *, char *, ulong_t *);

#ifdef __cplusplus
}
#endif

#endif /* _TNF_ARGS_H */
