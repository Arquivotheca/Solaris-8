/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_BAD_H
#define	_ACPI_BAD_H

#pragma ident	"@(#)acpi_bad.h	1.1	99/11/05 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface to bad BIOS list */

/*
 * Bad BIOS List - contains a list of BIOSes that have ACPI semantic
 * errors in them.  If we have one of these, then we disable ACPI.
 *
 * Conceptually, the list is a list of descriptors (descs).  Each desc
 * identifies a (set of) bad BIOS(es) by a series of matching
 * predicates ANDed together.  Each predicate can list a set of
 * matching criteria on a specified ACPI table or on the BIOS date. If
 * the specified table does not exist (some like SBST are optional),
 * then we consider the matching criteria to be unsatisfied. (Note,
 * you can specify a particular table and no other matching criteria
 * to just probe for a table's existance.)  We examine each desc in
 * order until a match is found (and ACPI is disabled) or we get to
 * the end of the list (ACPI is okay).
 *
 * The predicates are encoded in a variant usage of the existing
 * acpi_header struct.  This struct contains all the identifiers used
 * by ACPI anyway.  A desc is an array of these, with the last having
 * an end of desc flag.  All descs are put into a single array with
 * the last having the end of list flag.
 *
 * predicates use the acpi_header struct as follows:
 * flags (table/date, end flags) = checksum field
 * flags2 (fields, operators) = length field
 *
 * for dates (flags has ACPI_BDATE):
 * day = table (signature) rev field
 * month = OEM table rev field
 * year = creator rev field
 *
 * If for some reason you have a BIOS with a wierd date, we allow any
 * two digits for month and day as well as year.
 */

/* struct field names */
#define	acpi_bflags checksum
#define	acpi_bflags2 length
#define	acpi_bday revision
#define	acpi_bmonth oem_rev
#define	acpi_byear creator_rev

/* bits for FLAGS */
#define	ACPI_BDESC_END	0x01
#define	ACPI_BLIST_END	0x02
#define	ACPI_BDATE	0x04

/* bits for FLAGS2 */
#define	ACPI_BSREV_MASK	0x0007
#define	ACPI_BSREV_EQ	0x0001
#define	ACPI_BSREV_LT	0x0002
#define	ACPI_BSREV_GT	0x0004
#define	ACPI_BSREV_LE	(ACPI_BSREV_EQ | ACPI_BSREV_LT)
#define	ACPI_BSREV_GE	(ACPI_BSREV_EQ | ACPI_BSREV_GT)
#define	ACPI_BSREV_NE	(ACPI_BSREV_LT | ACPI_BSREV_GT)
#define	ACPI_BSREV_TRUE 0x0000	/* for acpi_bpred_op */
#define	ACPI_BSREV_ANY	0x0007	/* for acpi_bpred_op */

#define	ACPI_BTREV_MASK	0x0070
#define	ACPI_BTREV_SHIFT (4)	/* for acpi_bpred_op */
#define	ACPI_BTREV_EQ	0x0010
#define	ACPI_BTREV_LT	0x0020
#define	ACPI_BTREV_GT	0x0040
#define	ACPI_BTREV_LE	(ACPI_BTREV_EQ | ACPI_BTREV_LT)
#define	ACPI_BTREV_GE	(ACPI_BTREV_EQ | ACPI_BTREV_GT)
#define	ACPI_BTREV_NE	(ACPI_BTREV_LT | ACPI_BTREV_GT)

#define	ACPI_BCREV_MASK	0x0700
#define	ACPI_BCREV_SHIFT (8)	/* for acpi_bpred_op */
#define	ACPI_BCREV_EQ	0x0100
#define	ACPI_BCREV_LT	0x0200
#define	ACPI_BCREV_GT	0x0400
#define	ACPI_BCREV_LE	(ACPI_BCREV_EQ | ACPI_BCREV_LT)
#define	ACPI_BCREV_GE	(ACPI_BCREV_EQ | ACPI_BCREV_GT)
#define	ACPI_BCREV_NE	(ACPI_BCREV_LT | ACPI_BCREV_GT)

#define	ACPI_BOEM_EQ	0x1000
#define	ACPI_BTAB_EQ	0x2000
#define	ACPI_BCRE_EQ	0x4000
#define	ACPI_BOEM_WIDTH (6)	/* for acpi_bpred_op */
#define	ACPI_BTAB_WIDTH (8)	/* for acpi_bpred_op */

#define	ACPI_BDATE_MASK	ACPI_BSREV_MASK
#define	ACPI_BDATE_EQ	ACPI_BSREV_EQ
#define	ACPI_BDATE_LT	ACPI_BSREV_LT
#define	ACPI_BDATE_GT	ACPI_BSREV_GT
#define	ACPI_BDATE_LE	ACPI_BSREV_LE
#define	ACPI_BDATE_GE	ACPI_BSREV_GE
#define	ACPI_BDATE_NE	ACPI_BSREV_NE
#define	ACPI_BDATE_TRUE	ACPI_BSREV_TRUE	/* for acpi_bpred_op */
#define	ACPI_BDATE_ANY	ACPI_BSREV_ANY /* for acpi_bpred_op */

/* placeholders for non-existent fields */
#define	NO_VAL	(0)
#define	NO_ID	"\x00"

#define	ACPI_BAD_TABLE(FLAGS, FLAGS2, SIG, REV, OEM, TAB, TABREV, CR, CRREV) \
{ SIG, FLAGS2, REV, FLAGS, OEM, TAB, TABREV, CR, CRREV, }

#define	ACPI_BAD_DATE(FLAGS, FLAGS2, YEAR, MONTH, DAY) \
{ 0, FLAGS2, DAY, FLAGS | ACPI_BDATE, "\x00", "\x00", MONTH, 0, YEAR, }

#define	ACPI_BAD_BIOS_LIST_END \
{ 0, 0, 0, ACPI_BLIST_END, "\x00", "\x00", 0, 0, 0, }

extern acpi_header_t acpi_bad_bios_list[];


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_BAD_H */
