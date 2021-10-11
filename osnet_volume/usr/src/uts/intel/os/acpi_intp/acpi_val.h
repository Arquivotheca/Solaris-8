/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_VAL_H
#define	_ACPI_VAL_H

#pragma ident	"@(#)acpi_val.h	1.2	99/11/03 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for ACPI values */

/* flags for values */
#define	ACPI_VSAVE	0x01

extern int value_elem(acpi_val_t *avp);

extern struct acpi_val *value_new(void);

extern struct acpi_val *uninit_new(void);
extern struct acpi_val *integer_new(unsigned int value);
extern struct acpi_val *string_new(char *string);
extern struct acpi_val *buffer_new(char *buffer, int length);
extern struct acpi_val *package_new(int size);
extern struct acpi_val *package_setn(struct acpi_val *pkg, int index,
    struct acpi_val *value);

extern struct acpi_val *field_new(struct acpi_val *region, int flags,
    unsigned int offset, unsigned int length, unsigned char fld_flags,
    unsigned char acc_type, unsigned char acc_attrib);
extern struct acpi_val *bankfield_new(struct acpi_val *region,
    struct acpi_val *bank, unsigned int value, int flags, unsigned int offset,
    unsigned int length, unsigned char fld_flags, unsigned char acc_type,
    unsigned char acc_attrib);
extern struct acpi_val *indexfield_new(struct acpi_val *index,
    struct acpi_val *data, int flags, unsigned int offset, unsigned int length,
    unsigned char fld_flags, unsigned char acc_type, unsigned char acc_attrib);
extern struct acpi_val *device_new(void);
extern struct acpi_val *event_new(void);
extern struct acpi_val *method_new(unsigned char flags, char *byte_code,
    int length);
extern struct acpi_val *mutex_new(unsigned char sync);
extern struct acpi_val *region_new(unsigned char space, unsigned int offset,
    unsigned int length, ns_elem_t *ns_ref);
extern struct acpi_val *powerres_new(unsigned char system_level,
    unsigned short res_order);
extern struct acpi_val *processor_new(unsigned char id, unsigned int PBaddr,
    unsigned char PBlength);
extern struct acpi_val *thermal_new(void);
extern struct acpi_val *buffield_new(struct acpi_val *buffer,
    unsigned int index, unsigned int width);
extern struct acpi_val *ddbh_new(acpi_header_t *ahp);
extern struct acpi_val *debug_obj_new(void);

extern struct acpi_val *fatal_new(unsigned char type, unsigned int code,
    unsigned int arg);
extern struct acpi_val *ref_new(struct acpi_val *avp);

#define	value_hold(AVP) ((AVP)->refcnt++)

extern void value_free(struct acpi_val *avp);
extern int value_equal(acpi_val_t *v1, acpi_val_t *v2);

extern void acpi_header_print(struct acpi_header *hp);
extern void ddbh_print(struct acpi_val *avp);
extern void value_print(struct acpi_val *avp);
extern void fatal_print(struct acpi_fatal *fatalp);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_VAL_H */
