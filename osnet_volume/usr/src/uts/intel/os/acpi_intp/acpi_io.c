/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_io.c	1.1	99/05/21 SMI"


/* io and internal load/store ops */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/bootlink.h>
#include <sys/salib.h>
extern void *memset(void *dest, int c, size_t cnt); /* from misc_utls.c */
extern int doint_r(struct int_pb *sic);	/* from dosemul.c */
#endif

#ifdef ACPI_KERNEL
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ddi_isa.h>
#include <sys/pci_impl.h>
#include <sys/ddi_impldefs.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_bst.h"
#include "acpi_node.h"
#include "acpi_stk.h"
#include "acpi_par.h"

#include "acpi_elem.h"
#include "acpi_act.h"
#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"
#include "acpi_thr.h"
#include "acpi_inf.h"
#include "acpi_io.h"


#ifdef ACPI_BOOT
	extern int pci32_int1a();
	extern struct int_pb32 ic32;
#endif

/* XXX need to do gl */

/* fwd decl */
static int field_load(acpi_val_t *src, parse_state_t *pstp, int flags,
    acpi_val_t **valpp);
static int field_store(acpi_val_t *src, parse_state_t *pstp, acpi_val_t *dst);


/*
 * utility routines
 */

#define	DIV32_RND_UP(X) (X == 0 ? 1 : ((X + 31) >> 5))
#define	DIV8_RND_UP(X) ((X + 7) >> 3)

/* like bcopy, but for bits */
/* XXX not that efficient - use masking techniques */
static void
bit_copy(unsigned char *src, int src_off, unsigned char *dst, int dst_off,
    int length)
{
	int i;

	for (i = 0; i < length; i++, src_off++, dst_off++)
		if (*(src + (src_off >> 3)) & (1 << (src_off & 0x7)))
			*(dst + (dst_off >> 3)) |= 1 << (dst_off & 0x7);
		else
			*(dst + (dst_off >> 3)) &= ~(1 << (dst_off & 0x7));
}

static void
bit_put(unsigned char *ptr, int index, int bit)
{
	int byte_offset = index >> 3; /* divide by 8) */
	unsigned char bit_mask = 1 << (index & 0x7); /* mod 8 */

	*(ptr + byte_offset) = (*(ptr + byte_offset) & ~bit_mask) |
	    (bit ? bit_mask : 0);
}

static int
access_size_bits(int flags)
{
	switch (flags & ACPI_ACCESS_MASK) {
	default:		/* still try with 8 bits */
		(void) exc_warn("unknown access protocol");
		/*FALLTHRU*/
	case ACPI_ANY_ACC:	/* use 8 bit to simplify */
	case ACPI_BYTE_ACC:
		return (8);
	case ACPI_WORD_ACC:
		return (16);
	case ACPI_DWORD_ACC:
		return (32);
	}
}


/*
 * io_fn_t - read or write IO fn
 *
 * ctx: ptr to specific context structure, ignored for memory or sysIO or EC,
 *	for PCI contains bus/dev/fn, for SMBus contains cmd info
 * addr: in bytes
 * value: base addr
 */
typedef int (*io_fn_t)(void *ctx, unsigned int addr, int size,
    unsigned char *value, int write);

/*ARGSUSED*/
static int
memory_op(void *ctx, unsigned int addr, int size, unsigned char *value,
    int write)
{
	unsigned char *ba;
	unsigned short *wa, *wv;
	unsigned int *da, *dv;

	switch (size) {
	case 8:
		ba = acpi_trans_addr(addr);
		break;
	case 16:
		wa = acpi_trans_addr(addr);
		wv = (unsigned short *)value;
		break;
	case 32:
		da = acpi_trans_addr(addr);
		dv = (unsigned int *)value;
		break;
	}

	if (write)
		switch (size) {
		case 8: *ba = *value; break;
		case 16: *wa = *wv; break;
		case 32: *da = *dv; break;
		}
	else
		switch (size) {
		case 8: *value = *ba; break;
		case 16: *wv = *wa; break;
		case 32: *dv = *da; break;
		}
	return (ACPI_OK);
}

/*ARGSUSED*/
static int
io_op(void *ctx, unsigned int addr, int size, unsigned char *value, int write)
{

	if (write) {
		switch (size) {
			unsigned short *sp;
			unsigned int *ip;
		case 8:
			exc_debug(ACPI_DIO,
			    "IO STORE  addr 0x%x, size %d, value 0x%x",
			    addr, size, *value);
			break;
		case 16:
			sp = (unsigned short *)value;
			exc_debug(ACPI_DIO,
			    "IO STORE  addr 0x%x, size %d, value 0x%x",
			    addr, size, *sp);
			break;
		case 32:
			ip = (unsigned int *)value;
			exc_debug(ACPI_DIO,
			    "IO STORE  addr 0x%x, size %d, value 0x%x",
			    addr, size, *ip);
			break;
		}
#ifndef ACPI_USER
		switch (size) {
		case 8: io8_store(addr, value); break;
		case 16: io16_store(addr, value); break;
		case 32: io32_store(addr, value); break;
		}
#endif
	} else {
#ifndef ACPI_USER
		switch (size) {
		case 8: io8_load(addr, value); break;
		case 16: io16_load(addr, value); break;
		case 32: io32_load(addr, value); break;
		}
#endif
		switch (size) {
			unsigned short *sp;
			unsigned int *ip;
		case 8:
			exc_debug(ACPI_DIO,
			    "IO LOAD  addr 0x%x, size %d value 0x%x",
			    addr, size, *value);
			break;
		case 16:
			sp = (unsigned short *)value;
			exc_debug(ACPI_DIO,
			    "IO LOAD  addr 0x%x, size %d value 0x%x",
			    addr, size, *sp);
			break;
		case 32:
			ip = (unsigned int *)value;
			exc_debug(ACPI_DIO,
			    "IO LOAD  addr 0x%x, size %d value 0x%x",
			    addr, size, *ip);
			break;
		}
	}
	return (ACPI_OK);
}

typedef struct acpi_pci_ctx {
	struct parse_state *pstp;
	ns_elem_t *ns_ref;
	unsigned int bus;
	unsigned int dev;
	unsigned int fn;
#ifdef ACPI_KERNEL
	ddi_acc_handle_t ah;
#endif
} acpi_pci_ctx_t;

/* eval to find various bits of PCI info */
static int
pci_eval(acpi_nameseg_t *segp, ns_elem_t *ns_ref, unsigned int *retint)
{
	ns_elem_t *nsp;
	struct {
		name_t hdr;
		acpi_nameseg_t seg;
	} oneseg;
	acpi_thread_t *threadp;
	acpi_val_t *retval;

	/* setup a one-segment name */
	bzero(&oneseg, sizeof (oneseg));
	oneseg.hdr.segs = 1;
	oneseg.seg.iseg = segp->iseg;
	/* search starting from a specified place in the name space */
	if (ns_lookup(root_ns, ns_ref, (name_t *)&oneseg, 0, 0, &nsp, NULL) ==
	    ACPI_EXC) {
		*retint = 0;	/* not found defaults to zero (bus number) */
		return (ACPI_OK);
	} else if (nsp->valp->type == ACPI_INTEGER) {
		*retint = nsp->valp->acpi_ival;	/* found integer */
		return (ACPI_OK);
	} else if (nsp->valp->type == ACPI_METHOD) { /* run a method */
		if ((threadp = acpi_special_thread_get()) == NULL)
			return (ACPI_EXC);
		if (eval_driver(threadp, nsp, NULL, &retval, 256) == ACPI_EXC)
			return (ACPI_EXC);
		if (retval->type != ACPI_INTEGER)
			return (ACPI_EXC);
		*retint = retval->acpi_ival;
		value_free(retval);
		acpi_special_thread_release();
		return (ACPI_OK);
	}
	return (ACPI_EXC);
}

/*
 * setup the context for pci op
 * ADR for PCI is where you get device and function
 */
static int
pci_common(acpi_pci_ctx_t *ctx)
{
	static acpi_nameseg_t bbn = { 0x4E42425F, }; /* _BBN  = bus number */
	static acpi_nameseg_t adr = { 0x5244415F, }; /* _ADR = address */
	unsigned int addr;
#ifdef ACPI_KERNEL
	dev_info_t *dip;
#endif
	/* do evals on BBN and ADR */
	if (pci_eval(&bbn, ctx->ns_ref, &ctx->bus) == ACPI_EXC)
		return (ACPI_EXC);
	if (pci_eval(&adr, ctx->ns_ref, &addr) == ACPI_EXC)
		return (ACPI_EXC);
	ctx->dev = (addr >> 16) & 0xFFFF;
	ctx->fn = addr & 0xFFFF;
#ifdef ACPI_KERNEL
	/* XXX isn't going to work before PCI nexus attaches */
	if ((dip = ddi_find_devinfo("pci", -1, 1)) == NULL)
		return (ACPI_EXC);
	/* pci_config_setup expects a child of pci */
	if (DEVI(dip)->devi_child == 0)
		return (ACPI_EXC);
	dip = (dev_info_t *)DEVI(dip)->devi_child;
	if (pci_config_setup(dip, &ctx->ah) != DDI_SUCCESS)
		return (ACPI_EXC);
#endif
	return (ACPI_OK);
}

/*ARGSUSED*/
static void
pci_common_exit(acpi_pci_ctx_t *ctx)
{
#ifdef ACPI_KERNEL
	pci_config_teardown(&ctx->ah);
#endif
}


/*
 * pci8_load, pci16_load, pci32_load, pci8_store, pci16_store, pci32_store
 * all follow the same general pattern:
 *
 * for boot, they use a BIOS call
 * for Solaris, we munge the "opaque" handle & call the low-level nexus fn
 */
#ifdef ACPI_BOOT
#define	PCI_BIOS_INT		0x1A
#define	PCI_FUNCTION_ID		0xB1
#define	PCI_READ_CONFIG_BYTE	0x08
#define	PCI_READ_CONFIG_WORD	0x09
#define	PCI_READ_CONFIG_DWORD	0x0A
#define	PCI_WRITE_CONFIG_BYTE	0x0B
#define	PCI_WRITE_CONFIG_WORD	0x0C
#define	PCI_WRITE_CONFIG_DWORD	0x0D
#endif

static int
pci8_load(acpi_pci_ctx_t *ctx, int addr, unsigned char *value)
{
#ifdef ACPI_BOOT
	struct int_pb ir;	/* interrupt registers */

	ir.intval = PCI_BIOS_INT;
	ir.ax = (PCI_FUNCTION_ID << 8) | PCI_READ_CONFIG_BYTE;
	ir.bx = ((ctx->bus & 0xFF) << 8) | ((ctx->dev & 0x1F) << 3) |
	    (ctx->fn & 0x7);
	ir.di = addr & 0xFF;
	if (doint_r(&ir) || (ir.ax & 0xFF00))
		return (ACPI_EXC);
	*value = (ir.cx & 0xFF);
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	*value = (*ahp->ahi_get8)(ahp, (uint8_t *)addr);
#endif
	return (ACPI_OK);
}

static int
pci16_load(acpi_pci_ctx_t *ctx, int addr, unsigned short *value)
{
#ifdef ACPI_BOOT
	struct int_pb ir;	/* interrupt registers */

	ir.intval = PCI_BIOS_INT;
	ir.ax = (PCI_FUNCTION_ID << 8) | PCI_READ_CONFIG_WORD;
	ir.bx = ((ctx->bus & 0xFF) << 8) | ((ctx->dev & 0x1F) << 3) |
	    (ctx->fn & 0x7);
	ir.di = addr & 0xFF;
	if (doint_r(&ir) || (ir.ax & 0xFF00))
		return (ACPI_EXC);
	*value = (ir.cx & 0xFFFF);
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	*value = (*ahp->ahi_get16)(ahp, (uint16_t *)addr);
#endif
	return (ACPI_OK);
}

static int
pci32_load(acpi_pci_ctx_t *ctx, int addr, unsigned int *value)
{
#ifdef ACPI_BOOT
	ic32.eax = (unsigned int)((PCI_FUNCTION_ID << 8) |
	    PCI_READ_CONFIG_DWORD);
	ic32.ebx = (unsigned int)(((ctx->bus & 0xFF) << 8) |
	    ((ctx->dev & 0x1F) << 3) | (ctx->fn & 0x7));
	ic32.ecx = ic32.edx = ic32.esi = 0;
	ic32.edi = (unsigned int)(addr & 0xFF);
	if (pci32_int1a() || (ic32.eax & 0xFF00))
		return (ACPI_EXC);
	*value = ic32.ecx;
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	*value = (*ahp->ahi_get32)(ahp, (uint32_t *)addr);
#endif
	return (ACPI_OK);
}

static int
pci8_store(acpi_pci_ctx_t *ctx, int addr, unsigned char *value)
{
#ifdef ACPI_BOOT
	struct int_pb ir;	/* interrupt registers */

	ir.intval = PCI_BIOS_INT;
	ir.ax = (PCI_FUNCTION_ID << 8) | PCI_WRITE_CONFIG_BYTE;
	ir.bx = ((ctx->bus & 0xFF) << 8) | ((ctx->dev & 0x1F) << 3) |
	    (ctx->fn & 0x7);
	ir.cx = *value & 0xFF;
	ir.di = addr & 0xFF;
	if (doint_r(&ir) || (ir.ax & 0xFF00))
		return (ACPI_EXC);
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	(*ahp->ahi_put8)(ahp, (uint8_t *)addr, *value);
#endif
	return (ACPI_OK);
}

static int
pci16_store(acpi_pci_ctx_t *ctx, int addr, unsigned short *value)
{
#ifdef ACPI_BOOT
	struct int_pb ir;	/* interrupt registers */

	ir.intval = PCI_BIOS_INT;
	ir.ax = (PCI_FUNCTION_ID << 8) | PCI_WRITE_CONFIG_WORD;
	ir.bx = ((ctx->bus & 0xFF) << 8) | ((ctx->dev & 0x1F) << 3) |
	    (ctx->fn & 0x7);
	ir.cx = *value & 0xFFFF;
	ir.di = addr & 0xFF;
	if (doint_r(&ir) || (ir.ax & 0xFF00))
		return (ACPI_EXC);
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	(*ahp->ahi_put16)(ahp, (uint16_t *)addr, *value);
#endif
	return (ACPI_OK);
}

static int
pci32_store(acpi_pci_ctx_t *ctx, int addr, unsigned int *value)
{
#ifdef ACPI_BOOT
	ic32.eax = (unsigned int)((PCI_FUNCTION_ID << 8) |
	    PCI_WRITE_CONFIG_DWORD);
	ic32.ebx = (unsigned int)(((ctx->bus & 0xFF) << 8) |
	    ((ctx->dev & 0x1F) << 3) | (ctx->fn & 0x7));
	ic32.ecx = *value;
	ic32.edx = ic32.esi = 0;
	ic32.edi = (unsigned int)(addr & 0xFF);
	if (pci32_int1a() || (ic32.eax & 0xFF00))
		return (ACPI_EXC);
#endif
#ifdef ACPI_KERNEL
	ddi_acc_impl_t *ahp = (ddi_acc_impl_t *)ctx->ah;
	pci_acc_cfblk_t *confp;

	confp = (pci_acc_cfblk_t *)&ahp->ahi_common.ah_bus_private;
	confp->c_busnum = ctx->bus;
	confp->c_devnum = ctx->dev;
	confp->c_funcnum = ctx->fn;
	(*ahp->ahi_put32)(ahp, (uint32_t *)addr, *value);
#endif
	return (ACPI_OK);
}

static int
pci_op(void *ctx_opaque, unsigned int addr, int size, unsigned char *value,
    int write)
{
	unsigned short *sp;
	unsigned int *ip;
	int ret;
	acpi_pci_ctx_t *ctx = (acpi_pci_ctx_t *)ctx_opaque;

	if (pci_common(ctx) == ACPI_EXC) /* setup context */
		return (ACPI_EXC);
	if (write) {
		switch (size) {
		case 8:
			exc_debug(ACPI_DIO, "PCI store op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%02x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size,
			    *value & 0xFF);
			ret = pci8_store(ctx, addr, value);
			break;
		case 16:
			sp = (unsigned short *)value;
			exc_debug(ACPI_DIO, "PCI store op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%04x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size,
			    *sp & 0xFFFF);
			ret = pci16_store(ctx, addr, sp);
			break;
		case 32:
			ip = (unsigned int *)value;
			exc_debug(ACPI_DIO, "PCI store op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%08x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size, *ip);
			ret = pci32_store(ctx, addr, ip);
			break;
		}
	} else {
		switch (size) {
		case 8:
			ret = pci8_load(ctx, addr, value);
			exc_debug(ACPI_DIO, "PCI load op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%02x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size,
			    *value & 0xFF);
			break;
		case 16:
			sp = (unsigned short *)value;
			ret = pci16_load(ctx, addr, sp);
			exc_debug(ACPI_DIO, "PCI load op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%04x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size,
			    *sp & 0xFFFF);
			break;
		case 32:
			ip = (unsigned int *)value;
			ret = pci32_load(ctx, addr, ip);
			exc_debug(ACPI_DIO, "PCI load op: bus %d, dev 0x%x, "
			    "fn 0x%x, addr 0x%x size %d value 0x%08x",
			    ctx->bus, ctx->dev, ctx->fn, addr, size, *ip);
			break;
		}
	}
	pci_common_exit(ctx);	/* teardown context, if need be */
	return (ret);
}


/*
 * field_read
 *
 * width: in bits, provided separate from field in case it is adjusted
 * value: will always be 4-byte multiple wide
 *	can be NULL, if sole purpose was to just get the preserve bits
 * tmpp: if non-NULL, returns the temporary read buffer for preserve bits
 */

int
field_read(void *ctx, acpi_region_t *regionp, unsigned int offset, int width,
    unsigned char *value, io_fn_t io_fn, int flags, unsigned char **tmpp,
    int *tmp_sizep)
{
	int acc_size_bits = access_size_bits(flags);
	int acc_size_bytes = acc_size_bits >> 3; /* divide by 8 */
	unsigned char *tmp_buf, *byte_ptr;
	unsigned int buf_offset, start_bit, end_bit, addr_byte, map_offset;
	int i, tmp_size;

	map_offset = regionp->mapping;
	/* get it into the tmp_buf */
	end_bit = (map_offset << 3) + offset + width;
	addr_byte = map_offset + (offset >> 3);
	addr_byte &= ~(acc_size_bytes - 1); /* aligned addr */
	start_bit = addr_byte << 3;
	buf_offset = (map_offset << 3) + offset - start_bit;
	/* round up temporary buf to multiple of 4 bytes */
	tmp_size = DIV32_RND_UP(end_bit - start_bit) * 4;
	if ((tmp_buf = kmem_alloc(tmp_size, KM_SLEEP)) == NULL)
		return (ACPI_EXC);
	byte_ptr = tmp_buf;

	for (i = start_bit; i < end_bit; i += acc_size_bits) {
		if ((*io_fn)(ctx, addr_byte, acc_size_bits, byte_ptr, 0) ==
		    ACPI_EXC)
			return (ACPI_EXC);
		addr_byte += acc_size_bytes;
		byte_ptr += acc_size_bytes;
	}
	if (tmpp)
		*tmpp = tmp_buf;
	if (tmp_sizep)
		*tmp_sizep = tmp_size;
	if (value == NULL)
		return (ACPI_OK);

	/* copy, shift & mask tmp_buf into value */
	bit_copy(tmp_buf, buf_offset, value, 0, width);

	if (tmpp == NULL)
		kmem_free(tmp_buf, tmp_size);
	return (ACPI_OK);
}

int
field_write(unsigned char *value, void *ctx, acpi_region_t *regionp,
    unsigned int offset, int width, io_fn_t io_fn, int flags)
{
	int acc_size_bits = access_size_bits(flags);
	int acc_size_bytes = acc_size_bits >> 3; /* divide by 8 */
	unsigned char *tmp_buf, *byte_ptr;
	unsigned int buf_offset, start_bit, end_bit, addr_byte, map_offset;
	int i, tmp_size;

	map_offset = regionp->offset;
	/* get it into the tmp_buf */
	end_bit = (map_offset << 3) + offset + width;
	addr_byte = map_offset + (offset >> 3);
	addr_byte &= ~(acc_size_bytes - 1); /* aligned addr */
	start_bit = addr_byte << 3;
	buf_offset = (map_offset << 3) + offset - start_bit;

	/* get preserve bits */
	if (flags & ACPI_PRESERVE) {
		if (field_read(ctx, regionp, offset, width, NULL, io_fn, flags,
		    &tmp_buf, &tmp_size) == ACPI_EXC)
			return (ACPI_EXC);
	} else {
		/* round up temporary buf to multiple of 4 bytes */
		tmp_size = DIV32_RND_UP(end_bit - start_bit) * 4;
		if ((tmp_buf = kmem_alloc(tmp_size, KM_SLEEP)) == NULL)
			return (ACPI_EXC);
		if (flags & ACPI_WRITE_ONES)
			for (i = 0, byte_ptr = tmp_buf; i < tmp_size;
					i++, byte_ptr++)
				*byte_ptr = 0xFF;
		else
			bzero(tmp_buf, tmp_size);
	}
	/* copy, shift & mask from value into tmp_buf */
	bit_copy(value, 0, tmp_buf, buf_offset, width);

	byte_ptr = tmp_buf;
	for (i = start_bit; i < end_bit; i += acc_size_bits) {
		if ((*io_fn)(ctx, addr_byte, acc_size_bits, byte_ptr, 1) ==
		    ACPI_EXC)
			return (ACPI_EXC);
		addr_byte += acc_size_bytes;
		byte_ptr += acc_size_bytes;
	}
	kmem_free(tmp_buf, tmp_size);
	return (ACPI_OK);
}


/*
 * for bank-field, field, index-field
 */

static int
field_common(acpi_field_t *fieldp, parse_state_t *pstp,
    acpi_region_t **regionpp, acpi_val_t **tmp_valpp)
{
	acpi_field_src_t *field_src;
	acpi_bankfield_src_t *bank_src;
	acpi_indexfield_src_t *index_src;
	acpi_val_t *tmp_value;
	acpi_field_t *overlay;
	int bit_offset, length;

	switch (fieldp->flags & ACPI_FIELD_TYPE_MASK) {
	case ACPI_REGULAR_TYPE:
		field_src = &fieldp->src.field;
		*regionpp = (acpi_region_t *)(field_src->region->acpi_valp);
		break;
	case ACPI_BANK_TYPE:
		bank_src = &fieldp->src.bank;
		/* write bank register */
		if ((tmp_value = integer_new(bank_src->value)) == NULL)
			return (ACPI_EXC);
		if (field_store(tmp_value, pstp, bank_src->bank) == ACPI_EXC)
			return (ACPI_EXC);
		*regionpp = (acpi_region_t *)(bank_src->region->acpi_valp);
		break;
	case ACPI_INDEX_TYPE:
		index_src = &fieldp->src.index;
		/* determine index granularity */
		switch (fieldp->fld_flags & ACPI_ACCESS_MASK) {
		case ACPI_WORD_ACC: length = 16; break;
		case ACPI_DWORD_ACC: length = 32; break;
		case ACPI_ANY_ACC:
		case ACPI_BYTE_ACC:
		default:
			length = 8;
		}
		/* write index register */
		if ((tmp_value = integer_new(fieldp->offset / length)) ==
		    NULL)
			return (ACPI_EXC);
		if (field_store(tmp_value, pstp, index_src->index) == ACPI_EXC)
			return (ACPI_EXC);
		value_free(tmp_value);
		/* setup data register, adjusted for this field */
		overlay = (acpi_field_t *)(index_src->data->acpi_valp);
		switch (overlay->flags & ACPI_FIELD_TYPE_MASK) {
		case ACPI_REGULAR_TYPE:
			tmp_value = field_new(overlay->src.field.region,
			    overlay->flags, overlay->offset, overlay->length,
			    overlay->fld_flags, overlay->acc_type,
			    overlay->acc_attrib);
			break;
		case ACPI_BANK_TYPE:
			tmp_value = bankfield_new(overlay->src.bank.region,
			    overlay->src.bank.bank, overlay->src.bank.value,
			    overlay->flags, overlay->offset, overlay->length,
			    overlay->fld_flags, overlay->acc_type,
			    overlay->acc_attrib);
			break;
		case ACPI_INDEX_TYPE:
			tmp_value = indexfield_new(overlay->src.index.index,
			    overlay->src.index.data,
			    overlay->flags, overlay->offset, overlay->length,
			    overlay->fld_flags, overlay->acc_type,
			    overlay->acc_attrib);
			break;
		default:
			return (ACPI_EXC);
		}
		if (tmp_value == NULL)
			return (ACPI_EXC);
		overlay = (acpi_field_t *)(tmp_value->acpi_valp);
		bit_offset = fieldp->offset % length;
		overlay->offset += bit_offset;
		overlay->length -= bit_offset;
		if (fieldp->length < overlay->length)
			overlay->length = fieldp->length;
		*tmp_valpp = tmp_value;
		break; 		/* actual operation done in caller */
	default:
		return (ACPI_EXC);
	}
	return (ACPI_OK);
}

static int
field_load(acpi_val_t *src, parse_state_t *pstp, int flags, acpi_val_t **valpp)
{
	acpi_region_t *regionp;
	acpi_field_t *fieldp = src->acpi_valp;
	acpi_val_t *tmp_val;
	unsigned char *dst_buf;
	int length, value;
	acpi_pci_ctx_t ctx;

	if (field_common(fieldp, pstp, &regionp, &tmp_val) == ACPI_EXC)
		return (ACPI_EXC);
	if ((fieldp->flags & ACPI_FIELD_TYPE_MASK) == ACPI_INDEX_TYPE) {
		if (field_load(tmp_val, pstp, flags, valpp) == ACPI_EXC)
			return (ACPI_EXC);
		value_free(tmp_val);
		return (ACPI_OK);
	}

	if (flags & _EXPECT_BUF) {
		length = RND_UP4(DIV8_RND_UP(fieldp->length));
		if ((dst_buf = kmem_alloc(length, KM_SLEEP)) == NULL)
			return (ACPI_EXC);
		bzero(dst_buf, length);
		length = fieldp->length;
	} else {		/* integer expected, limit to integer size */
		length = (fieldp->length > 32) ? 32 : fieldp->length;
		dst_buf = (unsigned char *)&value;
		value = 0;
	}

	switch (regionp->space) {
	case ACPI_MEMORY:
		if (field_read(NULL, regionp, fieldp->offset, length, dst_buf,
		    memory_op, fieldp->fld_flags, NULL, NULL) == ACPI_EXC)
			return (ACPI_EXC);
		break;
	case ACPI_IO:
		if (field_read(NULL, regionp, fieldp->offset, length, dst_buf,
		    io_op, fieldp->fld_flags, NULL, NULL) == ACPI_EXC)
			return (ACPI_EXC);
		break;
	case ACPI_PCI_CONFIG:
		ctx.pstp = pstp;
		ctx.ns_ref = regionp->ns_ref;
		if (field_read(&ctx, regionp, fieldp->offset, length, dst_buf,
		    pci_op, fieldp->fld_flags, NULL, NULL) == ACPI_EXC)
			return (ACPI_EXC);
		break;
	case ACPI_EC:
	case ACPI_SMBUS:
		return (exc_warn("unsupported field type"));
	default:
		return (exc_warn("unknown field type"));
	}

	if (flags & _EXPECT_BUF)
		return ((*valpp = buffer_new((char *)dst_buf, length)) == NULL
		    ? ACPI_EXC : ACPI_OK);
	else
		return ((*valpp = integer_new(value)) == NULL ?
		    ACPI_EXC : ACPI_OK);
}

static int
field_store(acpi_val_t *src, parse_state_t *pstp, acpi_val_t *dst)
{
	acpi_region_t *regionp;
	acpi_field_t *fieldp = dst->acpi_valp;
	acpi_val_t *tmp_val;
	unsigned char *src_buf;
	int length;
	acpi_pci_ctx_t ctx;

	if (field_common(fieldp, pstp, &regionp, &tmp_val) == ACPI_EXC)
		return (ACPI_EXC);
	if ((fieldp->flags & ACPI_FIELD_TYPE_MASK) == ACPI_INDEX_TYPE) {
		if (field_store(src, pstp, tmp_val) == ACPI_EXC)
			return (ACPI_EXC);
		value_free(tmp_val);
		return (ACPI_OK);
	}

	if (src->type == ACPI_INTEGER) {
		src_buf = (unsigned char *)&src->acpi_ival;
		length = 32;
	} else if (src->type == ACPI_BUFFER) {
		src_buf = src->acpi_valp;
		length = src->length * 8;
	}
	if (fieldp->length < length)
		length = fieldp->length;

	switch (regionp->space) {
	case ACPI_MEMORY:
		return (field_write(src_buf, NULL, regionp, fieldp->offset,
		    length, memory_op, fieldp->fld_flags));
	case ACPI_IO:
		return (field_write(src_buf, NULL, regionp, fieldp->offset,
		    length, io_op, fieldp->fld_flags));
	case ACPI_PCI_CONFIG:
		ctx.pstp = pstp;
		ctx.ns_ref = regionp->ns_ref;
		return (field_write(src_buf, &ctx, regionp, fieldp->offset,
		    length, pci_op, fieldp->fld_flags));
	case ACPI_EC:
	case ACPI_SMBUS:
		return (exc_warn("unsupported field type"));
	default:
		return (exc_warn("unknown field type"));
	}
}


/*
 * buffer fields, buffers
 */

static int
buffield_load(acpi_val_t *src, int flags, acpi_val_t **valpp)
{
	acpi_buffield_t *bfp = src->acpi_valp;
	unsigned char *buf = bfp->buffer->acpi_valp;
	unsigned char *dst_buf;
	unsigned int value;
	int width;

	/* buffer length is bytes, bufffield in bits */
	if (bfp->buffer->length * 8 < bfp->index + bfp->width)
		return (ACPI_EXC);
	if (bfp->index % 8) {	/* common cases */
		if (bfp->width == 8)
			value = *(buf + bfp->index / 8);
		else if (bfp->width == 16)
			value = *((unsigned short *)(buf + bfp->index / 8));
		else if (bfp->width == 32)
			value = *((unsigned int *)(buf + bfp->index / 8));
		return ((*valpp = integer_new(value)) == NULL ?
		    ACPI_EXC : ACPI_OK);
	}

	if (flags & _EXPECT_BUF) {
		width = RND_UP4(DIV8_RND_UP(bfp->width));
		if ((dst_buf = kmem_alloc(width, KM_SLEEP)) == NULL)
			return (ACPI_EXC);
		bzero(dst_buf, width);
		bit_copy(buf, bfp->index, dst_buf, 0, bfp->width);
		width = DIV8_RND_UP(bfp->width);
		return ((*valpp = buffer_new((char *)dst_buf, width)) == NULL ?
		    ACPI_EXC : ACPI_OK);
	} else {		/* integer expected, limit to integer size */
		width = (bfp->width > 32) ? 32 : bfp->width;
		value = 0;
		bit_copy(buf, bfp->index, (unsigned  char *)&value, 0, width);
		return ((*valpp = integer_new(value)) == NULL ?
		    ACPI_EXC : ACPI_OK);
	}
}

static int
buffield_store(acpi_val_t *src, acpi_val_t *dst)
{
	int length_bits;
	unsigned char *src_buf;
	unsigned char *dst_buf;
	acpi_buffield_t *bfp;
	int i;

	if (src->type == ACPI_INTEGER) {
		length_bits = 32;
		src_buf = (unsigned char *)&src->acpi_ival;
	} else {
		length_bits = src->length * 8;
		src_buf = src->acpi_valp;
	}
	bfp = dst->acpi_valp;
	dst_buf = bfp->buffer->acpi_valp;

	if (length_bits > bfp->width)
		length_bits = bfp->width;
	bit_copy(src_buf, 0, dst_buf, bfp->index, length_bits);
	if (length_bits < bfp->width)
		for (i = length_bits; i < bfp->width; i++)
			bit_put(dst_buf, bfp->index + i, 0);
	return (ACPI_OK);
}

static int
buffer_store(acpi_val_t *src, acpi_val_t *dst)
{
	int length_bytes;
	unsigned char *src_buf;

	if (src->type == ACPI_INTEGER) {
		length_bytes = 4;
		src_buf = (unsigned char *)&src->acpi_ival;
	} else {
		length_bytes = src->length;
		src_buf = src->acpi_valp;
	}

	if (length_bytes > dst->length)
		length_bytes = dst->length;
	bcopy(src_buf, dst->acpi_valp, length_bytes);
	if (length_bytes < dst->length)
		bzero(((unsigned char *)(dst->acpi_valp)) + length_bytes,
		    dst->length - length_bytes);
	return (ACPI_OK);
}


/*
 * locals, args
 */

static void
local_load(struct parse_state *pstp, int argnum, struct acpi_val **valpp)
{
	struct exe_desc *edp = pstp->key;

	*valpp = edp->locals[argnum];
	value_hold(*valpp);
}

static void
local_store(struct parse_state *pstp, int argnum, acpi_val_t **val,
    acpi_val_t ***ref)
{
	struct exe_desc *edp = pstp->key;

	*val = edp->locals[argnum];
	*ref = &edp->locals[argnum];
}

static void
arg_load(struct parse_state *pstp, int argnum, struct acpi_val **valpp)
{
	struct exe_desc *edp = pstp->key;

	*valpp = edp->args[argnum];
	value_hold(*valpp);
}

static void
arg_store(struct parse_state *pstp, int argnum, acpi_val_t **val,
    acpi_val_t ***ref)
{
	struct exe_desc *edp = pstp->key;

	*val = edp->args[argnum];
	*ref = &edp->args[argnum];
}


/*
 * these functions exist to deal with termarg and supername
 * neither of these can be properly evaluated without knowing if we are
 *	loading from and/or storing to them, therefore we must defer final
 *	evaluation until here
 *
 * XXX fix with new termarg_r, termarg_w, termarg_rw, supername_r, super_rw
 * 	and maybe type tags too, termarg_r_int, etc.
 */

int
acpi_load(struct value_entry *srcp, struct parse_state *pstp,
    struct acpi_val **valpp, int flags)
{
	ns_elem_t *src_nsp;
	acpi_val_t *src_valp;

	switch (srcp->elem) {
	case V_NAME_STRING:
		if (ns_lookup(NSP_ROOT, NSP_CUR, srcp->data, KEY_IF_EXE, 0,
		    &src_nsp, NULL) != ACPI_OK)
			return (exc_code(ACPI_EUNDEF));
		src_valp = src_nsp->valp;
		value_hold(src_valp);
		break;

	case V_ACPI_VALUE:
	case V_DEBUG_OBJ:
	case V_REF:
		src_valp = srcp->data;
		break;

	case V_LOCAL0: local_load(pstp, 0, &src_valp); break;
	case V_LOCAL1: local_load(pstp, 1, &src_valp); break;
	case V_LOCAL2: local_load(pstp, 2, &src_valp); break;
	case V_LOCAL3: local_load(pstp, 3, &src_valp); break;
	case V_LOCAL4: local_load(pstp, 4, &src_valp); break;
	case V_LOCAL5: local_load(pstp, 5, &src_valp); break;
	case V_LOCAL6: local_load(pstp, 6, &src_valp); break;
	case V_LOCAL7: local_load(pstp, 7, &src_valp); break;

	case V_ARG0: arg_load(pstp, 0, &src_valp); break;
	case V_ARG1: arg_load(pstp, 1, &src_valp); break;
	case V_ARG2: arg_load(pstp, 2, &src_valp); break;
	case V_ARG3: arg_load(pstp, 3, &src_valp); break;
	case V_ARG4: arg_load(pstp, 4, &src_valp); break;
	case V_ARG5: arg_load(pstp, 5, &src_valp); break;
	case V_ARG6: arg_load(pstp, 6, &src_valp); break;

	default:
		return (exc_code(ACPI_EPARSE));
	}

	if (flags & _NO_EVAL) {
		*valpp = src_valp;
		return (ACPI_OK);
	}

	switch (src_valp->type) {
	case ACPI_UNINIT:
	case ACPI_INTEGER:
	case ACPI_STRING:
	case ACPI_BUFFER:
	case ACPI_PACKAGE:

	case ACPI_DEVICE:	/* notify */
	case ACPI_EVENT:	/* signal */
	case ACPI_MUTEX:	/* acquire */
	case ACPI_POWER_RES:	/* notify */
	case ACPI_THERMAL_ZONE:	/* notify */
	case ACPI_DDB_HANDLE:	/* unload */
	case ACPI_REF:		/* deref */
		*valpp = src_valp;
		break;

	case ACPI_FIELD:
		return (field_load(src_valp, pstp, flags, valpp));

	case ACPI_BUFFER_FIELD:
		return (buffield_load(src_valp, flags, valpp));

	case ACPI_DEBUG_OBJ:	/* can't read debug object */
		return (exc_code(ACPI_EEVAL));

		/* no known usage of these as termarg or supername */
	case ACPI_METHOD:
	case ACPI_REGION:
	case ACPI_PROCESSOR:
	default:
		return (exc_code(ACPI_EPARSE));
	}

	return (ACPI_OK);
}

int
acpi_store(struct acpi_val *valp, struct parse_state *pstp,
    struct value_entry *dstp)
{
	ns_elem_t *dst_nsp;
	acpi_val_t save;
	acpi_val_t *dst_valp;
	acpi_val_t **dst_ref;
	name_t *namep;

	switch (dstp->elem) {

	case V_DEBUG_OBJ:	/* printout */
		exc_cont("\n");
		value_print(valp);
		exc_cont("\n");
		return (ACPI_OK);

	case V_ACPI_VALUE:
		dst_valp = dstp->data;
		if (dst_valp->type == ACPI_FIELD || /* handled below */
		    dst_valp->type == ACPI_BUFFER_FIELD)
			break;
		else if (dst_valp->type == ACPI_DEBUG_OBJ) { /* repeat above */
			dstp->elem = V_DEBUG_OBJ; /* should be this anyway */
			exc_cont("\n");
			value_print(valp);
			exc_cont("\n");
			return (ACPI_OK);
		} else if (dst_valp->type != ACPI_REF)
			/* others types are not okay */
			return (exc_code(ACPI_ETYPE));
		/* fall through for REF */
		/*FALLTHRU*/
	case V_REF:
		/* get to underlying ACPI value */
		dst_valp = ((acpi_val_t *)(dstp->data))->acpi_valp;
		/* save old */
		save = *dst_valp;
		/* in place replacement */
		*dst_valp = *valp;
		dst_valp->refcnt = save.refcnt;
		save.flags |= ACPI_VSAVE;
		value_free(&save);
		/* setup value entry */
		return (ACPI_OK);

		/* these cases need further handling below */
	case V_NAME_STRING:
		namep = dstp->data;
		if (namep->segs == 0 && namep->gens == 0 &&
		    (namep->flags & NAME_ROOT) == 0)
			return (ACPI_OK); /* NULL name */
		if (ns_lookup(NSP_ROOT, NSP_CUR, namep, KEY_IF_EXE, 0,
		    &dst_nsp, NULL) != ACPI_OK)
			return (exc_code(ACPI_EUNDEF));
		dst_valp = dst_nsp->valp;
		dst_ref = &dst_nsp->valp;
		break;

	case V_LOCAL0: local_store(pstp, 0, &dst_valp, &dst_ref); break;
	case V_LOCAL1: local_store(pstp, 1, &dst_valp, &dst_ref); break;
	case V_LOCAL2: local_store(pstp, 2, &dst_valp, &dst_ref); break;
	case V_LOCAL3: local_store(pstp, 3, &dst_valp, &dst_ref); break;
	case V_LOCAL4: local_store(pstp, 4, &dst_valp, &dst_ref); break;
	case V_LOCAL5: local_store(pstp, 5, &dst_valp, &dst_ref); break;
	case V_LOCAL6: local_store(pstp, 6, &dst_valp, &dst_ref); break;
	case V_LOCAL7: local_store(pstp, 7, &dst_valp, &dst_ref); break;

	case V_ARG0: arg_store(pstp, 0, &dst_valp, &dst_ref); break;
	case V_ARG1: arg_store(pstp, 1, &dst_valp, &dst_ref); break;
	case V_ARG2: arg_store(pstp, 2, &dst_valp, &dst_ref); break;
	case V_ARG3: arg_store(pstp, 3, &dst_valp, &dst_ref); break;
	case V_ARG4: arg_store(pstp, 4, &dst_valp, &dst_ref); break;
	case V_ARG5: arg_store(pstp, 5, &dst_valp, &dst_ref); break;
	case V_ARG6: arg_store(pstp, 6, &dst_valp, &dst_ref); break;
	}

	switch (dst_valp->type) {
	case ACPI_UNINIT:	/* anything is okay */
		value_free(dst_valp);
		*dst_ref = valp;
		break;

	case ACPI_INTEGER:
		if (valp->type != ACPI_INTEGER)
			return (exc_code(ACPI_ETYPE));
		value_free(dst_valp);
		*dst_ref = valp;
		break;

	case ACPI_STRING:
		if (valp->type != ACPI_STRING)
			return (exc_code(ACPI_ETYPE));
		value_free(dst_valp);
		*dst_ref = valp;
		break;

	case ACPI_BUFFER:
		if (valp->type != ACPI_INTEGER && valp->type != ACPI_BUFFER)
			return (exc_code(ACPI_ETYPE));
		if (buffer_store(valp, dst_valp) == ACPI_EXC)
			return (ACPI_EXC);
		break;

	case ACPI_FIELD:
		if (valp->type != ACPI_INTEGER && valp->type != ACPI_BUFFER)
			return (exc_code(ACPI_ETYPE));
		if (field_store(valp, pstp, dst_valp) == ACPI_EXC)
			return (ACPI_EXC);
		break;

	case ACPI_BUFFER_FIELD:
		if (valp->type != ACPI_INTEGER && valp->type != ACPI_BUFFER)
			return (exc_code(ACPI_ETYPE));
		if (buffield_store(valp, dst_valp) == ACPI_EXC)
			return (ACPI_EXC);
		break;

	case ACPI_PACKAGE:	/* individual elements via index operator */
	case ACPI_REF:		/* should not happen as handled above */
	case ACPI_DEBUG_OBJ:
		/* no known usage of these as write supername */
	case ACPI_DEVICE:
	case ACPI_EVENT:
	case ACPI_MUTEX:
	case ACPI_POWER_RES:
	case ACPI_THERMAL_ZONE:
	case ACPI_DDB_HANDLE:
	case ACPI_METHOD:
	case ACPI_REGION:
	case ACPI_PROCESSOR:
	default:
		return (exc_code(ACPI_ETYPE));
	}
	return (ACPI_OK);
}


/* eof */
