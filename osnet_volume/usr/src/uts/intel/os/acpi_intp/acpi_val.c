/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_val.c	1.3	99/11/03 SMI"


/* ACPI value */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_KERNEL
#include <sys/cmn_err.h>
#endif

#ifdef ACPI_USER
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"

#include "acpi_elem.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_inf.h"


static void *
struct_copy(void *ptr, int size)
{
	char *new;

	if (size < 1)
		size = 1;
	if ((new = kmem_alloc(size, KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	bcopy(ptr, new, size);
	return (new);
}

int
value_elem(acpi_val_t *avp)
{
	if (avp == NULL)
		return (V_ACPI_VALUE); /* XXX okay? */
	switch (avp->type) {
	case ACPI_DEBUG_OBJ:
		return (V_DEBUG_OBJ);
	case ACPI_REF:
		return (V_REF);
	default:
		return (V_ACPI_VALUE);
	}

}


/*
 * "new" functions
 */

acpi_val_t *
value_new(void)
{
	acpi_val_t *new;

	if ((new = (acpi_val_t *)kmem_alloc(sizeof (acpi_val_t), KM_SLEEP)) ==
	    NULL)
		return (exc_null(ACPI_ERES));
	bzero(new, sizeof (acpi_val_t));
	new->refcnt = 1;
	return (new);
}

acpi_val_t *
uninit_new(void)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_UNINIT;
	return (new);
}

acpi_val_t *
integer_new(unsigned int value)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_INTEGER;
	new->acpi_ival = value;
	return (new);
}

acpi_val_t *
string_new(char *string)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_STRING;
	new->length = strlen(string);
	new->acpi_valp = string;
	return (new);
}

acpi_val_t *
buffer_new(char *buffer, int length)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_BUFFER;
	new->length = length;
	new->acpi_valp = buffer;
	return (new);
}

acpi_val_t *
package_new(int size)
{
	acpi_val_t *new;
	acpi_val_t **vary;
	int i;

	if (size < 0)
		return (NULL);
	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_PACKAGE;
	new->length = size;
	if (size == 0) {
		new->acpi_valp = NULL;
		return (new);
	}
	if ((vary = kmem_alloc(size * sizeof (acpi_val_t *), KM_SLEEP)) ==
	    NULL)
		return (exc_null(ACPI_ERES));
	new->acpi_valp = vary;
	for (i = 0; i < size; i++, vary++)
		if ((*vary = uninit_new()) == NULL)
			return (exc_null(ACPI_ERES));
	return (new);
}

acpi_val_t *
package_setn(acpi_val_t *pkg, int index, acpi_val_t *value)
{
	acpi_val_t **avpp;

	if (pkg->type != ACPI_PACKAGE ||
	    pkg->acpi_valp == NULL || pkg->length == 0 ||
	    index < 0 || index >= pkg->length)
		return (NULL);
	avpp = (acpi_val_t **)(pkg->acpi_valp) + index;
	value_free(*avpp);
	value_hold(value);
	*avpp = value;
	return (pkg);
}

acpi_val_t *
field_new(acpi_val_t *region, int flags, unsigned int offset,
    unsigned int length, unsigned char fld_flags, unsigned char acc_type,
    unsigned char acc_attrib)
{
	acpi_val_t *new;
	acpi_field_t *fieldp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((fieldp = (acpi_field_t *)kmem_alloc(sizeof (acpi_field_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_FIELD;
	new->acpi_valp = fieldp;
	fieldp->src.field.region = region;
	fieldp->flags = (flags & (~ACPI_FIELD_TYPE_MASK)) | ACPI_REGULAR_TYPE;
	fieldp->offset = offset;
	fieldp->length = length;
	fieldp->fld_flags = fld_flags;
	fieldp->acc_type = acc_type;
	fieldp->acc_attrib = acc_attrib;
	if (region)
		value_hold(region);
	return (new);
}

acpi_val_t *
bankfield_new(acpi_val_t *region, acpi_val_t *bank, unsigned int value,
    int flags, unsigned int offset, unsigned int length,
    unsigned char fld_flags, unsigned char acc_type, unsigned char acc_attrib)
{
	acpi_val_t *new;
	acpi_field_t *fieldp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((fieldp = (acpi_field_t *)kmem_alloc(sizeof (acpi_field_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_FIELD;
	new->acpi_valp = fieldp;
	fieldp->src.bank.region = region;
	fieldp->src.bank.bank = bank;
	fieldp->src.bank.value = value;
	fieldp->flags = (flags & (~ACPI_FIELD_TYPE_MASK)) | ACPI_BANK_TYPE;
	fieldp->offset = offset;
	fieldp->length = length;
	fieldp->fld_flags = fld_flags;
	fieldp->acc_type = acc_type;
	fieldp->acc_attrib = acc_attrib;
	if (region)
		value_hold(region);
	if (bank)
		value_hold(bank);
	return (new);
}

acpi_val_t *
indexfield_new(acpi_val_t *index, acpi_val_t *data, int flags,
    unsigned int offset, unsigned int length, unsigned char fld_flags,
    unsigned char acc_type, unsigned char acc_attrib)
{
	acpi_val_t *new;
	acpi_field_t *fieldp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((fieldp = (acpi_field_t *)kmem_alloc(sizeof (acpi_field_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_FIELD;
	new->acpi_valp = fieldp;
	fieldp->src.index.index = index;
	fieldp->src.index.data = data;
	fieldp->flags = (flags & (~ACPI_FIELD_TYPE_MASK)) | ACPI_INDEX_TYPE;
	fieldp->offset = offset;
	fieldp->length = length;
	fieldp->fld_flags = fld_flags;
	fieldp->acc_type = acc_type;
	fieldp->acc_attrib = acc_attrib;
	if (index)
		value_hold(index);
	if (data)
		value_hold(data);
	return (new);
}

acpi_val_t *
device_new(void)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_DEVICE;
	new->acpi_valp = NULL;
	return (new);
}

acpi_val_t *
event_new(void)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_EVENT;
	new->acpi_ival = 0;
	return (new);
}

acpi_val_t *
method_new(unsigned char flags, char *byte_code, int length)
{
	acpi_val_t *new;
	acpi_method_t *methodp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((methodp = (acpi_method_t *)kmem_alloc(sizeof (acpi_method_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_METHOD;
	new->acpi_valp = methodp;
	methodp->byte_code = byte_code;
	methodp->length = length;
	methodp->flags = flags;
	return (new);
}

acpi_val_t *
mutex_new(unsigned char sync)
{
	acpi_val_t *new;
	acpi_mutex_t *mutexp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((mutexp = (acpi_mutex_t *)kmem_alloc(sizeof (acpi_mutex_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_MUTEX;
	new->acpi_valp = mutexp;
	mutexp->owner = NULL;
	mutexp->next = NULL;
	mutexp->sync = sync;
	return (new);
}

acpi_val_t *
region_new(unsigned char space, unsigned int offset, unsigned int length,
    ns_elem_t *ns_ref)
{
	acpi_val_t *new;
	acpi_region_t *regionp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((regionp = (acpi_region_t *)kmem_alloc(sizeof (acpi_region_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_REGION;
	new->acpi_valp = regionp;
	regionp->ns_ref = ns_ref;
	regionp->mapping = 0;
	regionp->space = space;
	regionp->offset = offset;
	regionp->length = length;
	return (new);
}

acpi_val_t *
powerres_new(unsigned char system_level, unsigned short res_order)
{
	acpi_val_t *new;
	acpi_powerres_t *powerp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((powerp = (acpi_powerres_t *)kmem_alloc(sizeof (acpi_powerres_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_POWER_RES;
	new->acpi_valp = powerp;
	powerp->system_level = system_level;
	powerp->res_order = res_order;
	return (new);
}

acpi_val_t *
processor_new(unsigned char id, unsigned int PBaddr, unsigned char PBlength)
{
	acpi_val_t *new;
	acpi_processor_t *procp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((procp = (acpi_processor_t *)kmem_alloc(sizeof (acpi_processor_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_PROCESSOR;
	new->acpi_valp = procp;
	procp->id = id;
	procp->PBaddr = PBaddr;
	procp->PBlength = PBlength;
	return (new);
}

acpi_val_t *
thermal_new(void)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_THERMAL_ZONE;
	new->acpi_valp = NULL;
	return (new);
}

acpi_val_t *
buffield_new(acpi_val_t *buffer, unsigned int index, unsigned int width)
{
	acpi_val_t *new;
	acpi_buffield_t *bfp;

	if ((new = value_new()) == NULL)
		return (NULL);
	if ((bfp = (acpi_buffield_t *)kmem_alloc(sizeof (acpi_buffield_t),
	    KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->type = ACPI_BUFFER_FIELD;
	new->acpi_valp = bfp;
	bfp->buffer = buffer;
	bfp->index = index;
	bfp->width = width;
	if (buffer)
		value_hold(buffer);
	return (new);
}

/*LINTLIBRARY*/
acpi_val_t *
ddbh_new(acpi_header_t *ahp)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_DDB_HANDLE;
	if ((new->acpi_valp = struct_copy(ahp, sizeof (acpi_header_t))) ==
	    NULL) /* copy so can be freed separately */
		return (NULL);
	return (new);
}

acpi_val_t *
debug_obj_new(void)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_DEBUG_OBJ;
	return (new);
}

acpi_val_t *
ref_new(acpi_val_t *avp)
{
	acpi_val_t *new;

	if ((new = value_new()) == NULL)
		return (NULL);
	new->type = ACPI_REF;
	new->acpi_valp = avp;
	value_hold(avp);
	return (new);
}


/*
 * free
 */

/* XXX add new arg to avoid VSAVE bits */
void
value_free(acpi_val_t *avp)
{
	int i;
	acpi_field_t *fieldp;
	acpi_method_t *methodp;
	acpi_buffield_t *bfp;
	acpi_cb_t *cbp, *cnext;
	acpi_val_t **avpp;

	if (avp == NULL)
		return;
	if (avp->refcnt > 1) {
		avp->refcnt--;
		return;
	}
	if (avp->refcnt == 0) {
		(void) exc_warn("refcnt already zero on value 0x%x", avp);
		return;
	}
	/* maybe we should do an assert for negative refcnts? */
	avp->refcnt = 0;
	switch (avp->type) {
	case ACPI_PACKAGE:
		if (avp->acpi_valp == NULL)
			break;
		avpp = avp->acpi_valp;
		if (avp->length == 0)
			break;
		for (i = 0; i < avp->length; i++, avpp++)
			value_free(*avpp);
		kmem_free(avp->acpi_valp, sizeof (acpi_val_t *) * avp->length);
		break;

	case ACPI_STRING:	/* secondary structures */
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, avp->length + 1);
		break;
	case ACPI_BUFFER:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, RND_UP4(avp->length));
		break;
	case ACPI_FIELD:
		fieldp = avp->acpi_valp;
		switch (fieldp->flags & ACPI_FIELD_TYPE_MASK) {
		case ACPI_REGULAR_TYPE:
			if (fieldp->src.field.region)
				value_free(fieldp->src.field.region);
			break;
		case ACPI_BANK_TYPE:
			if (fieldp->src.bank.region)
				value_free(fieldp->src.bank.region);
			if (fieldp->src.bank.bank)
				value_free(fieldp->src.bank.bank);
			break;
		case ACPI_INDEX_TYPE:
			if (fieldp->src.index.index)
				value_free(fieldp->src.index.index);
			if (fieldp->src.index.data)
				value_free(fieldp->src.index.data);
			break;
		}
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_field_t));
		break;
	case ACPI_REGION:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_region_t));
		break;
	case ACPI_POWER_RES:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_powerres_t));
		break;
	case ACPI_PROCESSOR:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_processor_t));
		break;
	case ACPI_BUFFER_FIELD:
		bfp = avp->acpi_valp;
		if (bfp->buffer)
			value_free(bfp->buffer);
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_buffield_t));
		break;
	case ACPI_DDB_HANDLE:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_header_t));
		break;

	case ACPI_METHOD:
		methodp = avp->acpi_valp;
		if (methodp && methodp->byte_code)
			kmem_free(methodp->byte_code,
			    RND_UP4(methodp->length));
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_method_t));
		break;
	case ACPI_MUTEX:
		if (avp->acpi_valp)
			kmem_free(avp->acpi_valp, sizeof (acpi_mutex_t));
		break;
	case ACPI_DEVICE:
	case ACPI_THERMAL_ZONE:
		for (cbp = avp->acpi_valp; cbp; cbp = cnext) {
			cnext = cbp->next; /* must save next before free */
			kmem_free(cbp, sizeof (acpi_cb_t));
		}
		break;
	case ACPI_REF:
		value_free(avp->acpi_valp);
		/*FALLTHRU*/
	case ACPI_UNINIT:
	case ACPI_INTEGER:
	case ACPI_EVENT:
	case ACPI_DEBUG_OBJ:
	default:		/* no complaints */
		break;
	}
	if (avp->flags & ACPI_VSAVE)
		avp->flags &= ~ACPI_VSAVE;
	else
		kmem_free(avp, sizeof (acpi_val_t)); /* top level structure */
}

int
value_equal(acpi_val_t *v1, acpi_val_t *v2)
{
	int i;

	if (v1 == NULL || v2 == NULL)
		return (v1 == v2 ? ACPI_OK : ACPI_EXC);

	/* deref */
	for (; v1->type == ACPI_REF; )
		v1 = (acpi_val_t *)(v1->acpi_valp);
	for (; v2->type == ACPI_REF; )
		v2 = (acpi_val_t *)(v2->acpi_valp);

	if (v1 == v2)
		return (ACPI_OK);
	if (v1->type != v2->type)
		return (ACPI_EXC);

	switch (v1->type) {
	case ACPI_UNINIT:
	case ACPI_DEBUG_OBJ:
		return (ACPI_OK);
	case ACPI_INTEGER:
		if (v1->acpi_ival == v2->acpi_ival)
			return (ACPI_OK);
		break;
	case ACPI_STRING:
		if (strcmp(v1->acpi_valp, v2->acpi_valp) == 0)
			return (ACPI_OK);
		break;
	case ACPI_BUFFER:
		if (v1->length != v2->length)
			return (ACPI_EXC);
		if (bcmp(v1->acpi_valp, v2->acpi_valp, v1->length) == 0)
			return (ACPI_OK);
		break;
	case ACPI_PACKAGE:
		if (v1->length != v2->length)
			return (ACPI_EXC);
		for (i = 0; i < v1->length; i++)
			if (value_equal(ACPI_PKG_N(v1, i), ACPI_PKG_N(v2, i))
			    == ACPI_EXC)
				return (ACPI_EXC);
		return (ACPI_OK);
	case ACPI_FIELD:
	case ACPI_DEVICE:
		/* ACPI_EVENT can't be the same unless the same value */
	case ACPI_METHOD:
	case ACPI_MUTEX:
	case ACPI_REGION:
	case ACPI_POWER_RES:
	case ACPI_PROCESSOR:
	case ACPI_THERMAL_ZONE:
	case ACPI_BUFFER_FIELD:
	case ACPI_DDB_HANDLE:
		if (v1->acpi_valp == v2->acpi_valp)
			return (ACPI_OK);
	}
	return (ACPI_EXC);
}

/*
 * string print functions
 */

/*ARGSUSED*/
static void
uninit_print(acpi_val_t *avp)
{
	exc_cont("uninitialized");
}

static void
integer_print(acpi_val_t *avp)
{
	exc_cont("integer 0x%x", avp->acpi_ival);
}

static void
string_print(acpi_val_t *avp)
{
	exc_cont("string %s", avp->acpi_valp);
}

static void
buffer_print(acpi_val_t *avp)
{
	int i;
	char *ptr;

	exc_cont("buffer (length %d)\n\t0x0000: ", avp->length);
	for (i = 0, ptr = avp->acpi_valp; i < avp->length; i++, ptr++) {
		if ((i & 0xF) == 0 && i != 0)
			exc_cont("\n\t0x%04x: ", i);
		exc_cont(" %02X", *ptr & 0xFF);
	}
}

static void
package_print(acpi_val_t *avp)
{
	int i;
	acpi_val_t **avpp;

	exc_cont("package  elements %d", avp->length);
	avpp = (acpi_val_t **)(avp->acpi_valp);
	for (i = 0; i < avp->length; i++, avpp++) {
		exc_cont("\n\t[%03d] ", i);
		value_print(*avpp);
	}
}

static void
field_print(acpi_val_t *avp)
{
	acpi_field_t *fieldp;

	fieldp = avp->acpi_valp;
	switch (fieldp->flags & ACPI_FIELD_TYPE_MASK) {
	case ACPI_REGULAR_TYPE:
		exc_cont("field  region 0x%x", fieldp->src.field.region);
		break;
	case ACPI_BANK_TYPE:
		exc_cont("bank-field  region 0x%x  bank 0x%x  value 0x%x",
		    fieldp->src.bank.region,
		    fieldp->src.bank.bank,
		    fieldp->src.bank.value);
		break;
	case ACPI_INDEX_TYPE:
		exc_cont("index-field  index 0x%x  data 0x%x",
		    fieldp->src.index.index,
		    fieldp->src.index.data);
		break;
	default:
		exc_cont("unknown field type");
	}
	if (fieldp->acc_type || fieldp->acc_attrib)
		exc_cont("\n\tfield-flags 0x%x  offset 0x%x  length 0x%x"
		    "\n\taccess-type 0x%x  access-attribute 0x%x",
		    fieldp->fld_flags, fieldp->offset, fieldp->length,
		    fieldp->acc_type, fieldp->acc_attrib);
	else			/* print abbreviated version */
		exc_cont("\n\tfield-flags 0x%x  offset 0x%x  length 0x%x",
		    fieldp->fld_flags, fieldp->offset, fieldp->length);
}

/*ARGSUSED*/
static void
device_print(acpi_val_t *avp)
{
	acpi_cb_t *cbp;

	exc_cont("device");
	for (cbp = avp->acpi_valp; cbp; cbp = cbp->next)
		exc_cont("\n\tcallback 0x%x:  obj 0x%x  fn 0x%x  cookie 0x%x",
		    cbp, cbp->obj, cbp->fn, cbp->cookie);
}

/*ARGSUSED*/
static void
event_print(acpi_val_t *avp)
{
	exc_cont("event  pending %d", avp->acpi_ival);
}

static void
method_print(acpi_val_t *avp)
{
	acpi_method_t *methodp;
	int i;
	char *ptr;

	methodp = avp->acpi_valp;
	exc_cont("method  flags 0x%x  byte-code (length %d):\n\t0x0000: ",
	    methodp->flags, methodp->length);
	ptr = methodp->byte_code;
	for (i = 0; i < methodp->length; i++, ptr++) {
		if ((i & 0xF) == 0 && i != 0)
			exc_cont("\n\t0x%04x: ", i);
		exc_cont(" %02X", *ptr & 0xFF);
	}
}

static void
mutex_print(acpi_val_t *avp)
{
	acpi_mutex_t *mutexp;

	mutexp = avp->acpi_valp;
	exc_cont(" mutex  sync %d  owner 0x%x  next 0x%x",
	    mutexp->sync, mutexp->owner, mutexp->next);
}

static void
region_print(acpi_val_t *avp)
{
	acpi_region_t *regionp;
	char *space;

	regionp = avp->acpi_valp;
	switch (regionp->space) {
	case ACPI_MEMORY:
		space = "system-memory";
		break;
	case ACPI_IO:
		space = "system-I/O";
		break;
	case ACPI_PCI_CONFIG:
		space = "PCI-config";
		break;
	case ACPI_EC:
		space = "EC";
		break;
	case ACPI_SMBUS:
		space = "SMBus";
		break;
	default:
		exc_cont("region  space 0x%x  offset 0x%x  length 0x%x",
		    regionp->space, regionp->offset, regionp->length);
		return;
	}
	exc_cont("region  %s  offset 0x%x  length 0x%x",
	    space, regionp->offset, regionp->length);
}

static void
powerres_print(acpi_val_t *avp)
{
	acpi_powerres_t *powerp;

	powerp = avp->acpi_valp;
	exc_cont("power resource  system-level %d  resource-order %d",
	    powerp->system_level, powerp->res_order);
}

static void
processor_print(acpi_val_t *avp)
{
	acpi_processor_t *procp;

	procp = avp->acpi_valp;
	exc_cont("processor  id %d  pb-address %x  pb-length %d",
	    procp->id, procp->PBaddr, procp->PBlength);
}

/*ARGSUSED*/
static void
thermal_print(acpi_val_t *avp)
{
	acpi_cb_t *cbp;

	exc_cont("thermal zone");
	for (cbp = avp->acpi_valp; cbp; cbp = cbp->next)
		exc_cont("\n\tcallback 0x%x:  obj 0x%x  fn 0x%x  cookie 0x%x",
		    cbp, cbp->obj, cbp->fn, cbp->cookie);
}

static void
buffield_print(acpi_val_t *avp)
{
	acpi_buffield_t *bfp;

	bfp = avp->acpi_valp;
	exc_cont("buffer-field  buffer %x  index %d  width %d",
	    bfp->buffer, bfp->index, bfp->width);
}

#define	BLANK_NULL(X) (X ? X : ' ')
void
acpi_header_print(acpi_header_t *hp)
{
	exc_cont("DDB-handle  signature %c%c%c%c revision %d  "
	    "length 0x%x (%d)\n"
	    "\toem %c%c%c%c%c%c  table %c%c%c%c%c%c%c%c revision %d\n"
	    "\tcreator %c%c%c%c revision 0x%x",
	    BLANK_NULL(hp->signature.cseg[0]),
	    BLANK_NULL(hp->signature.cseg[1]),
	    BLANK_NULL(hp->signature.cseg[2]),
	    BLANK_NULL(hp->signature.cseg[3]),
	    hp->revision, hp->length, hp->length,
	    BLANK_NULL(hp->oem_id[0]),
	    BLANK_NULL(hp->oem_id[1]),
	    BLANK_NULL(hp->oem_id[2]),
	    BLANK_NULL(hp->oem_id[3]),
	    BLANK_NULL(hp->oem_id[4]),
	    BLANK_NULL(hp->oem_id[5]),
	    BLANK_NULL(hp->oem_table_id[0]),
	    BLANK_NULL(hp->oem_table_id[1]),
	    BLANK_NULL(hp->oem_table_id[2]),
	    BLANK_NULL(hp->oem_table_id[3]),
	    BLANK_NULL(hp->oem_table_id[4]),
	    BLANK_NULL(hp->oem_table_id[5]),
	    BLANK_NULL(hp->oem_table_id[6]),
	    BLANK_NULL(hp->oem_table_id[7]),
	    hp->oem_rev,
	    BLANK_NULL(hp->creator_id.cseg[0]),
	    BLANK_NULL(hp->creator_id.cseg[1]),
	    BLANK_NULL(hp->creator_id.cseg[2]),
	    BLANK_NULL(hp->creator_id.cseg[3]),
	    hp->creator_rev);
}
void
ddbh_print(acpi_val_t *avp)
{
	acpi_header_t *hp;

	hp = avp->acpi_valp;
	acpi_header_print(hp);
}

/*ARGSUSED*/
static void
debug_obj_print(acpi_val_t *avp)
{
	exc_cont("debug-object");
}

static void
ref_print(acpi_val_t *avp)
{
	exc_cont("ref to ");
	value_print(avp->acpi_valp);
}

void
value_print(acpi_val_t *avp)
{
	switch (avp->type) {
	case ACPI_UNINIT:
		uninit_print(avp);
		break;
	case ACPI_INTEGER:
		integer_print(avp);
		break;
	case ACPI_STRING:
		string_print(avp);
		break;
	case ACPI_BUFFER:
		buffer_print(avp);
		break;
	case ACPI_PACKAGE:
		package_print(avp);
		break;
	case ACPI_FIELD:
		field_print(avp);
		break;
	case ACPI_DEVICE:
		device_print(avp);
		break;
	case ACPI_EVENT:
		event_print(avp);
		break;
	case ACPI_METHOD:
		method_print(avp);
		break;
	case ACPI_MUTEX:
		mutex_print(avp);
		break;
	case ACPI_REGION:
		region_print(avp);
		break;
	case ACPI_POWER_RES:
		powerres_print(avp);
		break;
	case ACPI_PROCESSOR:
		processor_print(avp);
		break;
	case ACPI_THERMAL_ZONE:
		thermal_print(avp);
		break;
	case ACPI_BUFFER_FIELD:
		buffield_print(avp);
		break;
	case ACPI_DDB_HANDLE:
		ddbh_print(avp);
		break;
	case ACPI_DEBUG_OBJ:
		debug_obj_print(avp);
		break;
	case ACPI_REF:
		ref_print(avp);
		break;
	default:
		exc_cont("unknown type object");
	}
}

/*LINTLIBRARY*/
void
fatal_print(acpi_fatal_t *fatalp)
{
	exc_cont("fatal-object  type %d  code %d  arg %d",
	    fatalp->type, fatalp->code, fatalp->arg);
}


/* eof */
