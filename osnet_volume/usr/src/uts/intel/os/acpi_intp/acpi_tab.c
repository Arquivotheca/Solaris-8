/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_tab.c	1.2	99/11/05 SMI"


/*
 * ACPI table routines
 */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>
#include "acpi_bad.h"

#ifdef ACPI_USER
#include <stdlib.h>
#endif

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_stk.h"
#include "acpi_node.h"
#include "acpi_par.h"

#include "acpi_elem.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_thr.h"
#include "acpi_val.h"
#include "acpi_tab.h"
#include "acpi_inf.h"


#if defined(ACPI_BOOT) || defined(ACPI_USER)
static int acpi_bios_check(void); /* forward decl */
#endif

/* check integrity of acpi_header_t table */
int
table_check(acpi_header_t *base, unsigned int sig, int rev)
{
	int i, sum;
	char *ptr;

	if (base->signature.iseg != sig)
		return (exc_code(ACPI_ETABLE));
	if (rev && base->revision != rev)
		return (exc_code(ACPI_ETABLE));
	sum = 0;
	ptr = (char *)base;
	for (i = 0; i < base->length; i++)
		sum += *ptr++;
	sum &= 0xFF;
	return (sum == 0 ? ACPI_OK : exc_code(ACPI_ECHKSUM));
}

/* oem_table_id equal */
static int
table_id_equal(acpi_header_t *t1, acpi_header_t *t2)
{
	int i;

	for (i = 0; i < 8; i++)
		if (t1->oem_table_id[i] != t2->oem_table_id[i])
			return (0);
	return (1);
}


/* RSDT table pointers */
#define	ACPI_RSDT_N(N) \
*(((unsigned int *)(((char *)rsdt_p) + sizeof (acpi_header_t))) + N)

/* find tables under RSDT */
static acpi_header_t *
table_find(unsigned int sig)
{
	int i;
	acpi_header_t *tablep;

	if (rsdt_p == NULL)
		return (NULL);
	for (i = 0; i < rsdt_entries; i++) {
		tablep = (acpi_header_t *)acpi_trans_addr(ACPI_RSDT_N(i));
		if (tablep->signature.iseg == sig)
			return (tablep);
	}
	return (NULL);
}


static ddb_desc_t *
ddb_desc_new(acpi_header_t *tablep, acpi_val_t *handle)
{
	ddb_desc_t *new;

	if ((new = kmem_alloc(sizeof (ddb_desc_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = KEY_DDB;
	new->flags = 0;
	new->next = NULL;
	new->header = *tablep;
	new->base = tablep;
	new->handle = handle;
	return (new);
}

void
ddb_desc_free(ddb_desc_t *ddb)
{
	if (ddb->handle)
		value_free(ddb->handle);
	kmem_free(ddb, sizeof (ddb_desc_t));
}

/* add to end of ddb_desc chain */
ddb_desc_t *
table_add(acpi_header_t *tablep, acpi_val_t *handle)
{
	ddb_desc_t *new, *ptr, *trail;

	if ((new = ddb_desc_new(tablep, handle)) == NULL)
		return (NULL);

	/* add to end */
	trail = (ddb_desc_t *)&ddb_descs;
	for (ptr = ddb_descs; ptr; trail = ptr, ptr = ptr->next)
		;
	trail->next = new;
	return (new);
}


/* check ddb_desc chain and updates to higher rev, otherwise adds to end */
static int
table_update(acpi_header_t *tablep, acpi_val_t *handle)
{
	ddb_desc_t *new, *ptr, *trail;

	trail = (ddb_desc_t *)&ddb_descs;
	for (ptr = ddb_descs; ptr; trail = ptr, ptr = ptr->next)
		if (tablep->signature.iseg == ptr->header.signature.iseg ||
		    table_id_equal(tablep, &(ptr->header))) {
			if (tablep->oem_rev <= ptr->header.oem_rev)
				return (ACPI_OK); /* no update necessary */

			/* update to later rev */
			ptr->header = *tablep;
			ptr->base = tablep;
			return (ACPI_OK);
		}

	/* no match, so add to end */
	if ((new = ddb_desc_new(tablep, handle)) == NULL)
		return (ACPI_EXC);
	trail->next = new;
	return (ACPI_OK);
}


ddb_desc_t *
table_remove(acpi_val_t *handle)
{
	ddb_desc_t *ptr, *trail;

	trail = (ddb_desc_t *)&ddb_descs;
	for (ptr = ddb_descs; ptr; trail = ptr, ptr = ptr->next)
		if (value_equal(handle, ptr->handle) == ACPI_OK) {
			trail->next = ptr->next;
			ddb_desc_free(ptr);
			return (ptr);
		}
	return (NULL);
}

static void
table_header_print(acpi_header_t *tablep)
{
	if (tablep == NULL)
		return;
	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_DEBUG &&
	    (ACPI_DTABLE & acpi_debug_prop))
		acpi_header_print(tablep);
	exc_cont("\n");
}

/*
 * load particular tables
 * must be done in order
 */
static int
table_rsdt_load(unsigned int phys)
{
	acpi_header_t *tablep;

	if ((tablep = acpi_trans_addr(phys)) == NULL)
		return (exc_code(ACPI_ETABLE));
	if (table_check(tablep, ACPI_RSDT, 1) == ACPI_EXC)
		return (ACPI_EXC);
	rsdt_p = tablep;
	rsdt_entries = (tablep->length - sizeof (acpi_header_t)) / 4;
	exc_debug(ACPI_DTABLE, "rsdt addr 0x%x, entries %d", rsdt_p,
	    rsdt_entries);
	table_header_print(tablep);
	return (ACPI_OK);
}

static int
table_facp_load(void)
{
	acpi_header_t *tablep;	/* actually acpi_facp_t * */

	if (rsdt_p == 0 || rsdt_entries == 0)
		return (ACPI_EXC);
	if ((tablep = acpi_trans_addr(ACPI_RSDT_N(0))) == NULL)
		return (exc_code(ACPI_ETABLE));
	if (table_check(tablep, ACPI_FACP, 1) == ACPI_EXC)
		return (ACPI_EXC);
	if (tablep->length != sizeof (acpi_facp_t))
		return (exc_code(ACPI_ETABLE));
	facp_p = (acpi_facp_t *)tablep;
	exc_debug(ACPI_DTABLE, "facp addr 0x%x", facp_p);
	table_header_print(tablep);
	return (ACPI_OK);
}

static int
table_facs_load(void)
{
	acpi_facs_t *tablep;

	if (facp_p == NULL)
		return (ACPI_EXC);
	if ((tablep = acpi_trans_addr(facp_p->facs)) == NULL)
		return (exc_code(ACPI_ETABLE));
	if (tablep->signature.iseg != ACPI_FACS)
		return (ACPI_EXC);
	if (tablep->length >= sizeof (acpi_facp_t))
		return (exc_code(ACPI_ETABLE));
	facs_p = tablep;
	exc_debug(ACPI_DTABLE, "facs addr 0x%x", facs_p);
	return (ACPI_OK);
}

static int
table_apic_load(void)
{
	acpi_header_t *tablep;	/* actually acpi_apic_t * */

	if ((tablep = table_find(ACPI_APIC)) == NULL)
		return (ACPI_OK);	/* not required to be there */
	if (table_check(tablep, ACPI_APIC, 1) == ACPI_EXC)
		return (ACPI_EXC);
	if (tablep->length < sizeof (acpi_apic_t))
		return (exc_code(ACPI_ETABLE));
	apic_p = (acpi_apic_t *)tablep;
	exc_debug(ACPI_DTABLE, "apic addr 0x%x", apic_p);
	table_header_print(tablep);
	return (ACPI_OK);
}

static int
table_sbst_load(void)
{
	acpi_header_t *tablep;	/* actually acpi_sbst_t * */

	if ((tablep = table_find(ACPI_SBST)) == NULL)
		return (ACPI_OK);	/* not required to be there */
	if (table_check(tablep, ACPI_SBST, 1) == ACPI_EXC)
		return (ACPI_EXC);
	if (tablep->length != sizeof (acpi_sbst_t))
		return (exc_code(ACPI_ETABLE));
	sbst_p = (acpi_sbst_t *)tablep;
	exc_debug(ACPI_DTABLE, "sbdt addr 0x%x", sbst_p);
	table_header_print(tablep);
	return (ACPI_OK);
}

/* load all ddb (byte code) tables */
int
table_all_ddb_load(struct acpi_thread *threadp)
{
	ddb_desc_t *ptr;

	if (facp_p == NULL)
		return (ACPI_EXC);

	root_ns = ns_root_new(ddb_descs);

	/*
	 * now we have the complete list of unique ddbs with the
	 * highest revs in the right order, so now we load and parse them
	 */
	for (ptr = ddb_descs; ptr; ptr = ptr->next) {
		threadp->ddp = ptr;
		if (parse_driver(threadp, ptr,
		    (char *)(ptr->base) + sizeof (acpi_header_t),
		    ptr->header.length - sizeof (acpi_header_t),
		    root_ns, N_START, 0, NULL, NULL, 256) == ACPI_EXC)
			return (ACPI_EXC);
	}
	threadp->ddp = NULL;
	return (ACPI_OK);
}

/* load all non-byte code standard tables */
int
table_std_load(void)
{
	uint32_t rsdt_addr;
	acpi_header_t *tablep;
	int i;

	rsdt_addr = acpi_memory_prop.rsdt_paddr;
	if (rsdt_addr == 0)
		return (ACPI_EXC);
	if (table_rsdt_load(rsdt_addr) == ACPI_EXC)
		return (ACPI_EXC);
	if (table_facp_load() == ACPI_EXC)
		return (ACPI_EXC);
	if (table_facs_load() == ACPI_EXC)
		return (ACPI_EXC);
	if (table_apic_load() == ACPI_EXC)
		return (ACPI_EXC);
	if (table_sbst_load() == ACPI_EXC)
		return (ACPI_EXC);

				/* DSDT */
	if ((tablep = acpi_trans_addr(facp_p->dsdt)) == NULL)
		return (ACPI_EXC);
	if (table_check(tablep, ACPI_DSDT, 0) == ACPI_EXC)
		return (ACPI_EXC);
	if (table_add(tablep, 0) == NULL)
		return (ACPI_EXC);
	exc_debug(ACPI_DTABLE, "dsdt addr 0x%x", tablep);
	table_header_print(tablep);

				/* SSDT */
	for (i = 0; i < rsdt_entries; i++) {
		tablep = acpi_trans_addr(ACPI_RSDT_N(i));
		if (tablep->signature.iseg == ACPI_SSDT) {
			if (table_update(tablep, 0) == ACPI_EXC)
				return (ACPI_EXC);
			exc_debug(ACPI_DTABLE, "ssdt addr 0x%x", tablep);
			table_header_print(tablep);
		}
	}
				/* PSDT */
	for (i = 0; i < rsdt_entries; i++) {
		tablep = acpi_trans_addr(ACPI_RSDT_N(i));
		if (tablep->signature.iseg == ACPI_PSDT) {
			if (table_update(tablep, 0) == ACPI_EXC)
				return (ACPI_EXC);
			exc_debug(ACPI_DTABLE, "psdt addr 0x%x", tablep);
			table_header_print(tablep);
		}
	}

#if defined(ACPI_BOOT) || defined(ACPI_USER)
	return (acpi_bios_check());
#else
	return (ACPI_OK);
#endif
}


/*
 * bad BIOS list routines
 */

#if defined(ACPI_BOOT) || defined(ACPI_USER)
/* extra wide for all possible two digit values */
#define	ACPI_BDAY_WIDTH		7
#define	ACPI_BMONTH_WIDTH	7
#define	ACPI_BDATE_INT(YEAR, MONTH, DAY) \
(((YEAR) << (ACPI_BDAY_WIDTH + ACPI_BMONTH_WIDTH)) | \
((MONTH) << ACPI_BMONTH_WIDTH) | (DAY))

/* returns ACPI_OK on match, ACPI_EXC otherwise */
static int
acpi_bdate_match(int bdate, acpi_header_t *predp)
{
	int desc_date;

	/* allow any two digits for month and day */
	if (predp->acpi_byear < 1980 || predp->acpi_byear > 2099 ||
	    predp->acpi_bmonth > 99 || predp->acpi_bday > 99)
		return (ACPI_EXC); /* error */
	desc_date = ACPI_BDATE_INT(predp->acpi_byear, predp->acpi_bmonth,
		predp->acpi_bday);

	switch (predp->acpi_bflags2 & ACPI_BDATE_MASK) {
	case ACPI_BDATE_TRUE:
	case ACPI_BDATE_ANY:
		return (ACPI_OK);
	case ACPI_BDATE_EQ:
		return ((bdate == desc_date) ? ACPI_OK : ACPI_EXC);
	case ACPI_BDATE_LT:
		return ((bdate < desc_date) ? ACPI_OK : ACPI_EXC);
	case ACPI_BDATE_GT:
		return ((bdate > desc_date) ? ACPI_OK : ACPI_EXC);
	case ACPI_BDATE_LE:
		return ((bdate <= desc_date) ? ACPI_OK : ACPI_EXC);
	case ACPI_BDATE_GE:
		return ((bdate >= desc_date) ? ACPI_OK : ACPI_EXC);
	case ACPI_BDATE_NE:
		return ((bdate != desc_date) ? ACPI_OK : ACPI_EXC);
	}
	return (ACPI_EXC);
}

/* returns ACPI_OK on match, ACPI_EXC otherwise */
static int
acpi_bpred_op(int op, unsigned int bios, unsigned int pred)
{
	switch (op) {
	case ACPI_BSREV_TRUE:
	case ACPI_BSREV_ANY:
		return (ACPI_OK);
	case ACPI_BSREV_EQ:
		return ((bios == pred) ? ACPI_OK : ACPI_EXC);
	case ACPI_BSREV_LT:
		return ((bios < pred) ? ACPI_OK : ACPI_EXC);
	case ACPI_BSREV_GT:
		return ((bios > pred) ? ACPI_OK : ACPI_EXC);
	case ACPI_BSREV_LE:
		return ((bios <= pred) ? ACPI_OK : ACPI_EXC);
	case ACPI_BSREV_GE:
		return ((bios >= pred) ? ACPI_OK : ACPI_EXC);
	case ACPI_BSREV_NE:
		return ((bios != pred) ? ACPI_OK : ACPI_EXC);
	}
	return (ACPI_EXC);
}

/* returns ACPI_OK on match, ACPI_EXC otherwise */
static int
acpi_bpred_match(acpi_header_t *btablep, acpi_header_t *predp)
{
	/* id strings */
	if ((predp->acpi_bflags2 & ACPI_BOEM_EQ) &&
	    bcmp(predp->oem_id, btablep->oem_id, ACPI_BOEM_WIDTH) != 0)
		return (ACPI_EXC);
	if ((predp->acpi_bflags2 & ACPI_BTAB_EQ) &&
	    bcmp(predp->oem_table_id, btablep->oem_table_id,
		ACPI_BTAB_WIDTH) != 0)
		return (ACPI_EXC);
	if ((predp->acpi_bflags2 & ACPI_BCRE_EQ) &&
	    predp->creator_id.iseg != btablep->creator_id.iseg)
		return (ACPI_EXC);

	/* revision numbers */
	if ((predp->acpi_bflags2 & ACPI_BSREV_MASK) &&
	    acpi_bpred_op((predp->acpi_bflags2 & ACPI_BSREV_MASK),
		btablep->revision, predp->revision) == ACPI_EXC)
		return (ACPI_EXC);
	if ((predp->acpi_bflags2 & ACPI_BTREV_MASK) &&
	    acpi_bpred_op((predp->acpi_bflags2 & ACPI_BTREV_MASK) >>
		ACPI_BTREV_SHIFT,
		btablep->oem_rev, predp->oem_rev) == ACPI_EXC)
		return (ACPI_EXC);
	if ((predp->acpi_bflags2 & ACPI_BCREV_MASK) &&
	    acpi_bpred_op((predp->acpi_bflags2 & ACPI_BCREV_MASK) >>
		ACPI_BCREV_SHIFT,
		btablep->creator_rev, predp->creator_rev) == ACPI_EXC)
		return (ACPI_EXC);

	return (ACPI_OK);
}

/* returns ACPI_OK on match, ACPI_EXC otherwise */
static int
acpi_btable_match(acpi_header_t *predp)
{
	acpi_header_t *btablep;
	ddb_desc_t *ptr;

	switch (predp->signature.iseg) {
	/* FACS can't really be checked meaningfully */
	case ACPI_APIC: btablep = (acpi_header_t *)apic_p; break;
	case ACPI_FACP: btablep = (acpi_header_t *)facp_p; break;
	case ACPI_RSDT: btablep = (acpi_header_t *)rsdt_p; break;
	case ACPI_SBST: btablep = (acpi_header_t *)sbst_p; break;

	case ACPI_DSDT:		/* these tables require a list search */
	case ACPI_PSDT:
	case ACPI_SSDT:
		for (ptr = ddb_descs; ptr; ptr = ptr->next) {
			btablep = &ptr->header;
			if (predp->signature.iseg == btablep->signature.iseg &&
			    acpi_bpred_match(btablep, predp) == ACPI_OK)
				return (ACPI_OK);
		}
		return (ACPI_EXC);

	default:
		return (ACPI_EXC);
	}

	return ((btablep == NULL) ?
	    ACPI_EXC : acpi_bpred_match(btablep, predp));
}

/* returns ACPI_EXC for a bad BIOS match or error, ACPI_OK otherwise */
static int
acpi_bdesc_check(int bdate, acpi_header_t **pp)
{
	acpi_header_t *ptr = *pp;

	for (; ; ptr++) {
		/* anything to check? */
		if (ptr->signature.iseg || ptr->acpi_bflags2)
			if (ptr->acpi_bflags & ACPI_BDATE) {
				if (acpi_bdate_match(bdate, ptr) == ACPI_EXC)
					break; /* date does not match */
			} else {
				if (acpi_btable_match(ptr) == ACPI_EXC)
					break; /* table does not match */
			}
		if (ptr->acpi_bflags & ACPI_BDESC_END) {
			*pp = ptr;
			return (ACPI_EXC); /* bad BIOS match */
		}
		if (ptr->acpi_bflags & ACPI_BLIST_END) {
			*pp = ptr;
			return (ACPI_OK); /* end of list, no match */
		}
	}
				/* move to the end of the desc */
	for (; !(ptr->acpi_bflags & (ACPI_BDESC_END | ACPI_BLIST_END)); ptr++)
		;
	*pp = ptr;
	return (ACPI_OK);	/* no bad BIOS match */
}

/* returns ACPI_EXC for a bad BIOS match or error, ACPI_OK otherwise */
static int
acpi_bios_check(void)
{
	acpi_header_t *ptr;
	char *datep;
	int year, month, day, bdate;

	/* establish BIOS date */
	datep = (char *)0xFFFF5;
	/* year */
	year = ((int)(*(datep + 6) - '0') * 10) + (*(datep + 7) - '0');
	/* month */
	month = ((int)(*datep - '0') * 10) + (*(datep + 1) - '0');
	/* day */
	day = ((int)(*(datep + 3) - '0') * 10) + (*(datep + 4) - '0');
	if (year < 0 || year > 99 || month < 0 || month > 99 ||
	    day < 0 || day > 99) { /* non-digit chars in BIOS date */
		exc_debug(ACPI_DTABLE, "invalid BIOS date");
		return (ACPI_EXC);
	}
	year += (year >= 80 && year <= 99) ? 1900 : 2000; /* 2 digit year */
	bdate = ACPI_BDATE_INT(year, month, day);
	exc_debug(ACPI_DTABLE, "BIOS date: %d/%d/%d", year, month, day);

	for (ptr = &acpi_bad_bios_list[0]; ; ptr++) {
		if (acpi_bdesc_check(bdate, &ptr) == ACPI_EXC) {
			exc_debug(ACPI_DTABLE, "ACPI bad BIOS found");
			return (ACPI_EXC); /* bad BIOS match found */
		}
		if (ptr->acpi_bflags & ACPI_BLIST_END)
			break;
	}
	return (ACPI_OK);	/* no bad BIOS match found */
}
#endif


/* eof */
