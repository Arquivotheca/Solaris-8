/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _PRBK_H
#define	_PRBK_H

#pragma ident	"@(#)prbk.h	1.3	96/08/27 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declarations
 */

void prbk_buffer_list(void);
void prbk_buffer_alloc(int size);
void prbk_buffer_dealloc(void);
void *prbk_pidlist_add(void *, int);
void prbk_pfilter_add(void *);
void prbk_pfilter_drop(void *);
void prbk_set_pfilter_mode(boolean_t);
void prbk_show_pfilter_mode(void);
void prbk_set_tracing(boolean_t);
void prbk_show_tracing(void);
void prbk_warn_pfilter_empty(void);

#ifdef __cplusplus
}
#endif

#endif /* _PRBK_H */
