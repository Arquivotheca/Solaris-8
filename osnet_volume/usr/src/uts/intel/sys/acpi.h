/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_H
#define	_ACPI_H

#pragma ident	"@(#)acpi.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * project private interface to the ACPI interpreter
 */



/*
 * section 1: data types
 */

/* interpreter objects */
typedef void *acpi_obj;

/* ACPI values */
typedef struct acpi_val {
	union {
		unsigned int intval;
		void *ptr;
	} val_u;
	unsigned int length;
	unsigned short refcnt;
	unsigned char type;
	unsigned char flags;
} acpi_val_t;
#define	acpi_ival val_u.intval
#define	acpi_valp val_u.ptr

#define	ACPI_ONES (unsigned int)0xFFFFFFFF /* ones object value */

/*
 * value types: 0-16 are same as object type
 * val_u.{ptr,ival} interpretation in comments
 */
#define	ACPI_UNINIT		0x00 /* ptr is NULL */
#define	ACPI_INTEGER		0x01 /* intval is value */
#define	ACPI_STRING		0x02 /* ptr is char * */
#define	ACPI_BUFFER		0x03 /* size in length, ptr is char * */
/* for package, length is number of elems, ptr is array of acpi_val_t * */
#define	ACPI_PACKAGE		0x04
#define	ACPI_FIELD		0x05 /* ptr is acpi_field_t * */
#define	ACPI_DEVICE		0x06 /* ptr is internal callback info */
#define	ACPI_EVENT		0x07 /* intval is pending */
#define	ACPI_METHOD		0x08 /* ptr is acpi_method_t * */
#define	ACPI_MUTEX		0x09 /* ptr is acpi_mutex_t * */
#define	ACPI_REGION		0x0A /* ptr is acpi_region_t * */
#define	ACPI_POWER_RES		0x0B /* ptr is acpi_powerres_t * */
#define	ACPI_PROCESSOR		0x0C /* ptr is acpi_processor_t * */
#define	ACPI_THERMAL_ZONE	0x0D /* ptr is internal callback info */
#define	ACPI_BUFFER_FIELD	0x0E /* ptr is acpi_buffield_t * */
#define	ACPI_DDB_HANDLE		0x0F /* ptr is acpi_header_t * */
#define	ACPI_DEBUG_OBJ		0x10 /* ptr is NULL */
/* 128+ are private additions */
#define	ACPI_REF		0x80 /* ptr is acpi_val_t * */

/* Nth element of a package */
#define	ACPI_PKG_N(AVP, N) \
(*(((acpi_val_t **)(((acpi_val_t *)(AVP))->acpi_valp)) + N))


/* fields */
typedef struct acpi_field_src {
	acpi_val_t *region;
} acpi_field_src_t;

typedef struct acpi_bankfield_src {
	acpi_val_t *region;
	acpi_val_t *bank;
	unsigned int value;
} acpi_bankfield_src_t;

typedef struct acpi_indexfield_src {
	acpi_val_t *index;
	acpi_val_t *data;
} acpi_indexfield_src_t;

typedef struct acpi_field {
	union {
		acpi_field_src_t field;
		acpi_bankfield_src_t bank;
		acpi_indexfield_src_t index;
	} src;
	int flags;
#define	ACPI_FIELD_TYPE_MASK	0x0003
#define	ACPI_REGULAR_TYPE	0x0000
#define	ACPI_BANK_TYPE		0x0001
#define	ACPI_INDEX_TYPE		0x0002
	unsigned int offset;
	unsigned int length;
	unsigned int cumul_offset; /* cumulative offset for indexfield */
	unsigned char fld_flags; /* Access:0-3, Lock:4, Update:5-6 */
	unsigned char acc_type;
	unsigned char acc_attrib;
} acpi_field_t;
#define	ACPI_ACCESS_MASK	0x000F
#define	ACPI_ANY_ACC    	0x0000
#define	ACPI_BYTE_ACC		0x0001
#define	ACPI_WORD_ACC		0x0002
#define	ACPI_DWORD_ACC		0x0003
#define	ACPI_BLOCK_ACC		0x0004
#define	ACPI_SMBSR_ACC		0x0005
#define	ACPI_SMBQ_ACC		0x0006
#define	ACPI_LOCK		0x0010
#define	ACPI_UPDATE_MASK	0x0060
#define	ACPI_PRESERVE		0x0000
#define	ACPI_WRITE_ONES		0x0020
#define	ACPI_WRITE_ZEROS	0x0040

typedef struct acpi_method {
	char *byte_code;
	int length;
	unsigned char flags;	/* Args:0-2, Serialize:3 */
#define	ACPI_ARGS_MASK		0x0007
#define	ACPI_SERIALIZE		0x0008
#define	ACPI_IEVAL		0x0010 /* internal flags */
#define	ACPI_IRET		0x0020
} acpi_method_t;

typedef struct acpi_mutex {
	void *owner;
	acpi_val_t *next;
	unsigned char sync;
} acpi_mutex_t;

typedef struct acpi_region {
	acpi_obj ns_ref;
	unsigned int mapping;
	unsigned int offset;
	unsigned int length;
	unsigned char space;
} acpi_region_t;
#define	ACPI_MEMORY		0x0000
#define	ACPI_IO			0x0001
#define	ACPI_PCI_CONFIG		0x0002
#define	ACPI_EC			0x0003
#define	ACPI_SMBUS		0x0004

typedef struct acpi_powerres {
	unsigned short res_order;
	unsigned char system_level;
} acpi_powerres_t;

typedef struct acpi_processor {
	unsigned int PBaddr;
	unsigned char id;
	unsigned char PBlength;
} acpi_processor_t;

typedef struct acpi_buffield {
	acpi_val_t *buffer;
	unsigned int index;
	unsigned int width;
} acpi_buffield_t;


/* results of the fatal operator */
typedef struct acpi_fatal {
	unsigned int code;
	unsigned int arg;
	unsigned char type;
} acpi_fatal_t;


/*
 * callbacks
 * if a callback fn returns EXC, the rest of the callbacks are skipped
 */
typedef int (*acpi_cbfn_t)(acpi_obj obj, void *cookie,
    unsigned char notify_code);
typedef void *acpi_cbid_t;	/* callback id */



/*
 * section 2: standard data types from ACPI specification
 */

/* name segment */
typedef union acpi_nameseg {
	uint32_t iseg;
	int8_t cseg[4];	/* trailing NULLs equivalent to underscore */
} acpi_nameseg_t;

/* generic table header */
typedef struct acpi_header {
	acpi_nameseg_t signature;
#define	ACPI_APIC (uint32_t)0x43495041 /* table signatures */
#define	ACPI_DSDT (uint32_t)0x54445344
#define	ACPI_FACP (uint32_t)0x50434146
#define	ACPI_FACS (uint32_t)0x53434146
#define	ACPI_PSDT (uint32_t)0x54445350
#define	ACPI_RSDT (uint32_t)0x54445352
#define	ACPI_SSDT (uint32_t)0x54445353
#define	ACPI_SBST (uint32_t)0x54534253
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	int8_t oem_id[6];
	int8_t oem_table_id[8];
	uint32_t oem_rev;
	acpi_nameseg_t creator_id;
	uint32_t creator_rev;
} acpi_header_t;


/* FACS */
typedef struct acpi_facs {
	acpi_nameseg_t signature;
	uint32_t length;
	uint32_t hard_sig;
	uint32_t wake_vector;
	uint32_t global_lock;
#define	ACPI_FACS_GL_PENDING	0x01 /* global lock */
#define	ACPI_FACS_GL_OWNED	0x02
	uint32_t flags;
	uint32_t _res1;	/* reserved */
	uint32_t _res2;
	uint32_t _res3;
	uint32_t _res4;
	uint32_t _res5;
	uint32_t _res6;
	uint32_t _res7;
	uint32_t _res8;
	uint32_t _res9;
	uint32_t _res10;
} acpi_facs_t;
#define	ACPI_FACS_S4BIOS_F 	0x01 /* FACS flags */


/* FACP */
typedef struct acpi_facp {
	acpi_header_t header;
	uint32_t facs;
	uint32_t dsdt;
	uint8_t int_model;
#define	ACPI_FACP_PIC		0x0000 /* interrupt model */
#define	ACPI_FACP_APIC		0x0001
	uint8_t _res1;		/* reserved */
	uint16_t sci_int;
	uint32_t smi_cmd;
	uint8_t acpi_enable;
	uint8_t acpi_disable;
	uint8_t s4bios_req;
	uint8_t _res2;		/* reserved */
	uint32_t pm1a_evt_blk;
	uint32_t pm1b_evt_blk;
	uint32_t pm1a_cnt_blk;
	uint32_t pm1b_cnt_blk;
	uint32_t pm2_cnt_blk;
	uint32_t pm_tmr_blk;
	uint32_t gpe0_blk;
	uint32_t gpe1_blk;
	uint8_t pm1_evt_len;
	uint8_t pm1_cnt_len;
	uint8_t pm2_cnt_len;
	uint8_t pm_tmr_len;
	uint8_t gpe0_blk_len;
	uint8_t gpe1_blk_len;
	uint8_t gpe1_base;
	uint8_t _res3;		/* reserved */
	uint16_t p_lvl2_lat;
	uint16_t p_lvl3_lat;
	uint16_t flush_size;
	uint16_t flush_stride;
	uint8_t duty_offset;
	uint8_t duty_width;
	uint8_t day_alrm;
	uint8_t mon_alrm;
	uint8_t century;
	uint8_t _res4;		/* reserved */
	uint8_t _res5;		/* reserved */
	uint8_t _res6;		/* reserved */
	uint32_t flags;
} acpi_facp_t;
#define	ACPI_FACP_WBINVD	0x0001 /* FACP flags */
#define	ACPI_FACP_WBINVD_FLUSH	0x0002
#define	ACPI_FACP_PROC_C1	0x0004
#define	ACPI_FACP_P_LVL2_UP	0x0008
#define	ACPI_FACP_PWR_BUTTON	0x0010
#define	ACPI_FACP_SLP_BUTTON	0x0020
#define	ACPI_FACP_FIX_RTC	0x0040
#define	ACPI_FACP_RTC_S4	0x0080
#define	ACPI_FACP_TMR_VAL_EXT	0x0100
#define	ACPI_FACP_DCK_CAP	0x0200


/* APIC */
typedef struct acpi_apic {
	acpi_header_t header;
	uint32_t local_apic_addr;
	uint32_t flags;
} acpi_apic_t;
#define	ACPI_APIC_PCAT_COMPAT 0x1

typedef struct acpi_apic_header {
	uint8_t type;
#define	ACPI_APIC_LOCAL		0x0000 /* APIC record type */
#define	ACPI_APIC_IO		0x0001
#define	ACPI_APIC_ISO		0x0002
#define	ACPI_APIC_NMI_SRC	0x0003
#define	ACPI_APIC_NMI_CNCT	0x0004
	uint8_t length;
} acpi_apic_header_t;

typedef struct acpi_local_apic {
	acpi_apic_header_t header;
	uint8_t proc_id;
	uint8_t apic_id;
	uint32_t flags;
} acpi_local_apic_t;
#define	ACPI_APIC_ENABLED	 0x0001	/* local APIC flags */

typedef struct acpi_io_apic {
	acpi_apic_header_t header;
	uint8_t apic_id;
	uint8_t _res1;		/* reserved */
	uint32_t address;
	uint32_t vector_base;
} acpi_io_apic_t;

typedef struct acpi_iso {
	acpi_apic_header_t header;
	uint8_t bus;
	uint8_t source;
	uint32_t int_vector;
	uint16_t flags;
} acpi_iso_t;
#define	ACPI_APIC_POLARITY	0x0003 /* ISO, NMI flags */
#define	ACPI_APIC_POL_BUS	0x0000
#define	ACPI_APIC_POL_AHI	0x0001
#define	ACPI_APIC_POL_ALO	0x0003
#define	ACPI_APIC_TRIGGER	0x000B
#define	ACPI_APIC_TRG_BUS	0x0000
#define	ACPI_APIC_TRG_EDGE	0x0004
#define	ACPI_APIC_TRG_LEVEL	0x000B
typedef struct acpi_apic_nmi_src {
	acpi_apic_header_t header;
	uint16_t flags;
	uint32_t int_vector;
} acpi_apic_nmi_src_t;

typedef struct acpi_apic_nmi_cnct {
	acpi_apic_header_t header;
	uint8_t proc_id;
	uint16_t flags;
	uint8_t lint_in;
} acpi_apic_nmi_cnct_t;


/* SBST */
typedef struct acpi_sbst {
	acpi_header_t header;
	uint32_t warn_level;
	uint32_t low_level;
	uint32_t crit_level;
} acpi_sbst_t;


#define	ACPI_NO_TIMEOUT 0xFFFF	/* for acquire and wait operators */


/*
 * section 3: constants
 */

/* return codes */
#define	ACPI_OK		(0)
#define	ACPI_EXC	(-1)	/* unspecified exception */
#define	ACPI_EINTERNAL	(1)	/* internal error, should not happen! */
#define	ACPI_ERES	(2)	/* resource (e.g. memory) exhaustion */
#define	ACPI_EEOF	(3)	/* premature end of definition */
#define	ACPI_ERANGE	(4)	/* out of range */
#define	ACPI_ELIMIT	(5)	/* over or underflow */
#define	ACPI_EPARSE	(6)	/* parse error, possibly runtime */
#define	ACPI_EREDUCE	(7)	/* reduce error */
#define	ACPI_EOTHER	(8)	/* other conditions */
#define	ACPI_EOBJ	(9)	/* bad object */
#define	ACPI_EARGS	(10)	/* not enough args */
#define	ACPI_EEVAL	(11)	/* cannot eval this object */
#define	ACPI_ETYPE	(12)	/* wrong type in expression */
#define	ACPI_EFATAL	(13)	/* Fatal operator, return values are valid */
#define	ACPI_EBCD	(14)	/* bad BCD value */
#define	ACPI_EREFOF	(15)	/* RefOf failed */
#define	ACPI_EREGION	(16)	/* operating region access failed */
#define	ACPI_ESYNC	(17)	/* mutex violation */
#define	ACPI_ECHAR	(18)	/* bad character in name */
#define	ACPI_ECHKSUM	(19)	/* definition block has bad checksum */
#define	ACPI_EALREADY	(20)	/* already defined */
#define	ACPI_EUNDEF	(21)	/* undefined */
#define	ACPI_ETABLE	(22)	/* bad ACPI table */
/* XXX more error codes to be added */
#define	ACPI_EMAX	(22)


/* for client status */
#define	ACPI_CLIENT_OFF	0x0000
#define	ACPI_CLIENT_ON	0x0001


/*
 * section 4: functions
 */


/*
 * init
 * call before using ACPI, can be called more than once
 * can be used to see if ACPI is available
 * returns OK or EXC
 */
extern int acpi_init(void);

/* disable - call to disable ACPI permanently until next reboot */
extern void acpi_disable(void);

/* get fatal information from fatal operator */
extern acpi_fatal_t *acpi_fatal_get(void);

/* get standard data types, returns NULL on error */
extern acpi_facs_t *acpi_facs_get(void);
extern acpi_facp_t *acpi_facp_get(void);
extern acpi_apic_t *acpi_apic_get(void);
extern acpi_sbst_t *acpi_sbst_get(void);

/* make ACPI values, returns NULL on error */
extern acpi_val_t *acpi_uninit_new(void);
extern acpi_val_t *acpi_integer_new(unsigned int value);
extern acpi_val_t *acpi_string_new(char *string);
extern acpi_val_t *acpi_buffer_new(char *buffer, int length);

/* returns a package of uninit values */
extern acpi_val_t *acpi_package_new(int size);
/* set package element to a new value, returns pkg or NULL on error */
extern acpi_val_t *acpi_pkg_setn(acpi_val_t *pkg, int index,
    acpi_val_t *value);

/* val_free will free sub-structures of values too */
extern void acpi_val_free(acpi_val_t *valp);

/* object attributes, returns EXC on error (nameseg.iseg will be ACPI_ONES) */
extern acpi_nameseg_t acpi_nameseg(acpi_obj obj);
extern unsigned short acpi_objtype(acpi_obj obj);

/*
 * eval - get value or run method
 * eval_nameseg - find object named by nameseg relative to specified object,
 *	then eval
 *
 * args is a package or NULL for no args
 * **retpp will be the ACPI value or value returned by an ACPI method
 *	(uninit value if no return op), NULL if no value is desired
 * return codes listed above in constants section
 */
extern int acpi_eval(acpi_obj obj, acpi_val_t *args, acpi_val_t **retpp);
extern int acpi_eval_nameseg(acpi_obj obj, acpi_nameseg_t *segp,
    acpi_val_t *args, acpi_val_t **retpp);

/* name space navigation, returns NULL on error */
extern acpi_obj acpi_rootobj(void);
extern acpi_obj acpi_nextobj(acpi_obj obj);
extern acpi_obj acpi_childobj(acpi_obj obj);
extern acpi_obj acpi_parentobj(acpi_obj obj);

/* same as above but only look for ACPI device objects */
extern acpi_obj acpi_nextdev(acpi_obj obj);
extern acpi_obj acpi_childdev(acpi_obj obj);

/*
 * findobj
 * specified object is starting point for relative search
 * returns NULL on error
 */
extern acpi_obj acpi_findobj(acpi_obj obj, char *name, int flags);
/* find obj flags */
#define	ACPI_EXACT	0x0001	/* no ancestor matches for 1-segment names */

/* ACPI global lock, returns OK or EXC */
extern int acpi_gl_acquire(void);
extern int acpi_gl_release(void);

/*
 * callbacks
 * A notify event on the object will trigger callbacks in order registered.
 * If a callback fn returns EXC, the rest are skipped.
 */

/* returns NULL on error */
extern acpi_cbid_t acpi_cb_register(acpi_obj obj, acpi_cbfn_t fn,
    void *cookie);

/* returns OK or EXC */
extern int acpi_cb_cancel(acpi_cbid_t id);


/*
 * interpreter kernel residency
 * request/recind interpreter kernel residency requirement
 * ld_register returns OK or EXC
 */
extern int acpi_ld_register(void);
extern void acpi_ld_cancel(void);


/* change client status bits */
extern void acpi_client_status(unsigned int client, int status);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_H */
