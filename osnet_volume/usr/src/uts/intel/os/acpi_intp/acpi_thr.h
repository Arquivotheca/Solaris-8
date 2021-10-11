/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_THR_H
#define	_ACPI_THR_H

#pragma ident	"@(#)acpi_thr.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* if executing, add to dynamic list */
#define	DYNAMIC_IF_EXE(NSP) \
if (pstp->pctx & PCTX_EXECUTE) { \
	(NSP)->dyn = ((struct exe_desc *)(pstp->key))->dyn; \
	((struct exe_desc *)(pstp->key))->dyn = (NSP); \
	}

typedef struct saved_parse {
	void *key;
	struct byst *bp;
	int bst_stack;
	int parse_stack;
	int value_stack;
	int ns_stack;
	int pctx;
} saved_parse_t;

typedef struct exe_desc {
	struct exe_desc *next;	/* must be first field */
	short type;		/* must be second, distinguish from ddb_desc */
#define	KEY_EXE 0x2
	int flags;		/* method flags plus some exe_desc ones */
	struct acpi_thread *thread;
	void *dyn;		/* dynamic object list */
	struct value_entry *ret;
	struct acpi_val *args[7]; /* args */
	struct acpi_val *locals[8]; /* locals */
	saved_parse_t saved;
} exe_desc_t;

typedef struct acpi_thread {
	struct acpi_thread *next;
	struct ddb_desc *ddp;
	struct exe_desc *edp;
	struct acpi_val *mutex_list;
	unsigned char sync;
} acpi_thread_t;

extern int parse_driver(acpi_thread_t *threadp, void *key, char *buf,
    int length, struct ns_elem *initial_ns, int initial_symbol,
    int initial_pctx, value_entry_t **ret_vep, acpi_val_t **retval,
    int stack_size);
extern int eval_driver(acpi_thread_t *threadp, ns_elem_t *method_nsp,
    acpi_val_t *args, acpi_val_t **retpp, int stack_size);
extern int value_copy(acpi_val_t *src, acpi_val_t *dst);

acpi_thread_t *acpi_threads;
acpi_thread_t *current_acpi_thread;

extern acpi_thread_t *acpi_thread_new(struct ddb_desc *ddp,
    struct exe_desc *edp);
extern exe_desc_t *exe_desc_push(acpi_thread_t *threadp);
extern void exe_desc_pop(acpi_thread_t *threadp);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_THR_H */
