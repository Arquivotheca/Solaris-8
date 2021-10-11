/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_PRV_H
#define	_ACPI_PRV_H

#pragma ident	"@(#)acpi_prv.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * project private interfaces between ACPI/x86-boot and ACPI/Solaris
 * the interpreter interface is in acpi.h
 */

extern int acpi_state;
#define	ACPI_DISABLED	(-1)
#define	ACPI_START	(0)
#define	ACPI_INIT0	(1)	/* partial init only */
#define	ACPI_INIT1	(2)	/* partial init only */
#define	ACPI_INITED	(3)


/* acpi-memory root property */
typedef struct acpi_memory {
	uint64_t rsdt_paddr;
	uint64_t reclaim_paddr;
	uint64_t nvs_paddr;
	uint32_t reclaim_len;
	uint32_t nvs_len;
} acpi_memory_t;
extern acpi_memory_t acpi_memory_prop;

/* acpi-status root property */
extern unsigned int acpi_status_prop;
#define	ACPI_BOOT_INIT		0x00000001
#define	ACPI_BOOT_ENABLE	0x00000002
#define	ACPI_BOOT_CMASK		0x000000F0
#define	ACPI_BOOT_BOOTCONF	0x00000010
#define	ACPI_BOOT_CLIENT2	0x00000020
#define	ACPI_BOOT_CLIENT3	0x00000040
#define	ACPI_BOOT_CLIENT4	0x00000080
#define	ACPI_OS_INIT		0x00000100
#define	ACPI_OS_ENABLE		0x00000200
#define	ACPI_OS_CMASK		0xFFFFF000
#define	ACPI_OS_PCPLUSMP	0x00001000
#define	ACPI_OS_CLIENT2		0x00002000
#define	ACPI_OS_CLIENT3		0x00004000
#define	ACPI_OS_CLIENT4		0x00008000
#define	ACPI_OS_CLIENT5		0x00010000
#define	ACPI_OS_CLIENT6		0x00020000
#define	ACPI_OS_CLIENT7		0x00040000
#define	ACPI_OS_CLIENT8		0x00080000
#define	ACPI_OS_CLIENT9		0x00100000
#define	ACPI_OS_CLIENT10	0x00200000
#define	ACPI_OS_CLIENT11	0x00400000
#define	ACPI_OS_CLIENT12	0x00800000
#define	ACPI_OS_CLIENT13	0x01000000
#define	ACPI_OS_CLIENT14	0x02000000
#define	ACPI_OS_CLIENT15	0x04000000
#define	ACPI_OS_CLIENT16	0x08000000
#define	ACPI_OS_CLIENT17	0x10000000
#define	ACPI_OS_CLIENT18	0x20000000
#define	ACPI_OS_CLIENT19	0x40000000
#define	ACPI_OS_CLIENT20	0x80000000


/* acpi-debug options property */
extern unsigned int acpi_debug_prop;
#define	ACPI_DVERB_MASK		0x00000003 /* verbosity */
#define	ACPI_DVERB_PANIC	0x00000000
#define	ACPI_DVERB_WARN		0x00000001
#define	ACPI_DVERB_NOTE		0x00000002
#define	ACPI_DVERB_DEBUG	0x00000003
#define	ACPI_DOUT_MASK		0x00000030 /* output destination */
#define	ACPI_DOUT_DFLT		0x00000000
#define	ACPI_DOUT_CONS		0x00000010
#define	ACPI_DOUT_LOG		0x00000020
#define	ACPI_DOUT_USER		0x00000030
#define	ACPI_DOUT_FILE		0x00000040
#define	ACPI_DFAC_MASK		0xFFFFFF00 /* facilities */
#define	ACPI_DPARSE		0x00000100 /* parsing */
#define	ACPI_DLEX		0x00000200 /* lex action */
#define	ACPI_DREDUCE		0x00000400 /* reduce action */
#define	ACPI_DEXE		0x00000800 /* execution */
#define	ACPI_DNS		0x00001000 /* name space */
#define	ACPI_DTABLE		0x00002000 /* table loading */
#define	ACPI_DMEM		0x00004000 /* memory mapping */
#define	ACPI_DIO		0x00008000 /* io */
#define	ACPI_DBOOT		0x00010000
#define	ACPI_DRESOURCE		0x00020000

/* acpi-user-options options property */
extern unsigned int acpi_options_prop;
#define	ACPI_OUSER_MASK	0x0003
#define	ACPI_OUSER_DFLT	0x0000
#define	ACPI_OUSER_ON	0x0001
#define	ACPI_OUSER_OFF	0x0002


#if defined(_KERNEL) && !defined(_BOOT)
/* interpreter ops vector */
typedef struct acpi_ops {
	int (*fn_init)(int state);
	/* disable */
	/* acpi_facs_get */
	/* acpi_facp_get */
	/* acpi_apic_get */
	/* acpi_sbst_get */
	acpi_val_t *(*fn_uninit_new)(void);
	acpi_val_t *(*fn_integer_new)(unsigned int value);
	acpi_val_t *(*fn_string_new)(char *string);
	acpi_val_t *(*fn_buffer_new)(char *buffer, int length);
	acpi_val_t *(*fn_package_new)(int size);
	acpi_val_t *(*fn_pkg_setn)(acpi_val_t *pkg, int index,
	    acpi_val_t *value);
	void (*fn_val_free)(acpi_val_t *valp);
	acpi_nameseg_t (*fn_nameseg)(acpi_obj obj);
	unsigned short (*fn_objtype)(acpi_obj obj);
	int (*fn_eval)(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp);
	int (*fn_eval_nameseg)(acpi_obj obj, acpi_nameseg_t *segp,
	    acpi_val_t *args, acpi_val_t **retpp);
	acpi_obj (*fn_rootobj)(void);
	acpi_obj (*fn_nextobj)(acpi_obj obj);
	acpi_obj (*fn_childobj)(acpi_obj obj);
	acpi_obj (*fn_parentobj)(acpi_obj obj);
	acpi_obj (*fn_nextdev)(acpi_obj obj);
	acpi_obj (*fn_childdev)(acpi_obj obj);
	acpi_obj (*fn_findobj)(acpi_obj obj, char *name, int flags);
	int (*fn_gl_acquire)(void);
	int (*fn_gl_release)(void);
	acpi_cbid_t (*fn_cb_register)(acpi_obj obj, acpi_cbfn_t fn,
	    void *cookie);
	int (*fn_cb_cancel)(acpi_cbid_t id);
	/* ld_register */
	/* ld_cancel */
} acpi_ops_t;
extern acpi_ops_t acpi_ops_vector;
#endif


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_PRV_H */
