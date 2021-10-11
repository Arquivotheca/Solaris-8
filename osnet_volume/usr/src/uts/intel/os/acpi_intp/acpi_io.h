/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_IO_H
#define	_ACPI_IO_H

#pragma ident	"@(#)acpi_io.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for internal load/store */

#define	_NO_EVAL  (0x0001)

/* XXX cleanup with general expected types */
#define	_EXPECT_BUF (0x0002)

extern int acpi_load(struct value_entry *srcp, struct parse_state *pstp,
    struct acpi_val **valpp, int flags);
extern int acpi_store(struct acpi_val *valp, struct parse_state *pstp,
    struct value_entry *destp);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_IO_H */
