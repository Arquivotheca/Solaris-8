/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_INF_H
#define	_ACPI_INF_H

#pragma ident	"@(#)acpi_inf.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * definitions for ACPI interface code
 */

typedef struct acpi_cb {
	struct acpi_cb *next;
	struct acpi_cb *prev;
	void *obj;
	acpi_cbfn_t fn;
	void *cookie;
	short flags;
} acpi_cb_t;
#define	CB_FIRST	0x1

extern acpi_fatal_t fatal_info;

/* memory */
extern void *acpi_trans_addr(unsigned int addr);
extern int region_map(struct acpi_region *regionp);

/* threads */
extern int spec_thread;
extern struct acpi_thread main_thread, special_thread;

extern struct acpi_thread *acpi_thread_get(int need_excl);
extern void acpi_thread_release(struct acpi_thread *threadp);

extern struct acpi_thread *acpi_special_thread_get(void);
extern void acpi_special_thread_release(void);


/*
 * internal versions of fns
 */
extern int acpi_i_init(int state);
/* acpi_disable handled in wrapper function */

extern acpi_facs_t *acpi_i_facs_get(void);
extern acpi_facp_t *acpi_i_facp_get(void);
extern acpi_apic_t *acpi_i_apic_get(void);
extern acpi_sbst_t *acpi_i_sbst_get(void);

/* acpi_uninit_new handled in wrapper function */
/* acpi_integer_new handled in wrapper function */
/* acpi_string_new handled in wrapper function */
/* acpi_buffer_new handled in wrapper function */
/* acpi_package_new handled in wrapper function */
/* acpi_pkg_setn handled in wrapper function */
/* acpi_val_free handled in wrapper function */

extern acpi_nameseg_t acpi_i_nameseg(acpi_obj obj);
extern unsigned short acpi_i_objtype(acpi_obj obj);

extern int acpi_i_eval(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp);
extern int acpi_i_eval_nameseg(acpi_obj obj, acpi_nameseg_t *segp,
    acpi_val_t *args, acpi_val_t **retpp);

extern acpi_obj acpi_i_rootobj(void);
extern acpi_obj acpi_i_nextobj(acpi_obj obj);
extern acpi_obj acpi_i_childobj(acpi_obj obj);
extern acpi_obj acpi_i_parentobj(acpi_obj obj);
extern acpi_obj acpi_i_nextdev(acpi_obj obj);
extern acpi_obj acpi_i_childdev(acpi_obj obj);
extern acpi_obj acpi_i_findobj(acpi_obj obj, char *name, int flags);

extern int acpi_i_gl_acquire(void);
extern int acpi_i_gl_release(void);

extern acpi_cbid_t acpi_i_cb_register(acpi_obj obj, acpi_cbfn_t fn,
    void *cookie);
extern int acpi_i_cb_cancel(acpi_cbid_t id);

/* acpi_ld_register handled in wrapper function */
/* acpi_ld_cancel handled in wrapper function */


/* utility */
extern void acpi_delay_sig(unsigned int msec);


/* io stuff */
extern void io8_load(unsigned int addr, unsigned char *value);
extern void io16_load(unsigned int addr, unsigned char *value);
extern void io32_load(unsigned int addr, unsigned char *value);

extern void io8_store(unsigned int addr, unsigned char *value);
extern void io16_store(unsigned int addr, unsigned char *value);
extern void io32_store(unsigned int addr, unsigned char *value);

#ifdef __cplusplus
}
#endif

#endif /* _ACPI_INF_H */
