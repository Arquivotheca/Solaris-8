/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_TAB_H
#define	_ACPI_TAB_H

#pragma ident	"@(#)acpi_tab.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface to tables */

extern int rsdt_entries;
extern struct  acpi_header *rsdt_p;
extern struct acpi_facp *facp_p;
extern struct acpi_facs *facs_p;
extern struct acpi_apic *apic_p;
extern struct acpi_sbst *sbst_p;
extern struct ddb_desc *ddb_descs;

typedef struct ddb_desc {
	struct ddb_desc *next;	/* must be first field */
	short type;		/* must be second, distinguish from exe_desc */
#define	KEY_DDB 0x1
	short flags;
	acpi_header_t *base;
	acpi_header_t header;
	acpi_val_t *handle;
} ddb_desc_t;

extern ddb_desc_t *table_add(acpi_header_t *tablep, acpi_val_t *handle);
extern ddb_desc_t *table_remove(acpi_val_t *handle);
extern int table_check(acpi_header_t *base, unsigned int sig, int rev);
extern int table_all_ddb_load(struct acpi_thread *threadp);
extern int table_std_load(void);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_TAB_H */
