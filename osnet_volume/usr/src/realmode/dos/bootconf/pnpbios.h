/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * pnpbios.h - Plug and play bios definitions
 */

#ifndef	_PNPBIOS_H
#define	_PNPBIOS_H

#ident "@(#)pnpbios.h   1.8   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public pnpbios function prototypes
 */
void init_pnpbios();
void enumerator_pnpbios(int phase);
int get_pnp_info_pnpbios(u_char *num_csns, u_int *readport);

extern int Pnpbios;
extern int Parallel_ports_found;
extern int Serial_ports_found;


#define	PNPBIOS_GET_NODE_COUNT		0
#define	PNPBIOS_GET_DEV_NODE		1
#define	PNPBIOS_SET_DEV_NODE		2
#define	PNPBIOS_GET_PNP_ISA_INFO	0x40

#define	PNPBIOS_GET_CURRENT_INFO 1
#define	PNPBIOS_GET_NEXT_BOOT_INFO 2

#pragma pack(1)

typedef struct {
				/* hex offset: description */
	u_int size;		/*  0: size of device node in bytes */
	u_char devno;		/*  2: device number */
	u_long eisa_id;		/*  3: eisa style id eg PNP0C30 */
	u_char base_type;	/*  7: device class */
	u_char sub_type;	/*  8: device sub class */
	u_char interface_type;	/*  9: device sub sub class */
	u_int attr;		/*  A: device attributes */
	u_char resources[1];	/*  C: start of variable length array */
} dev_node_t;

typedef struct {
				/* hex offset: description */
	char signature[4];	/*  0: should be "$PnP" */
	u_char version;		/*  4: major in high nibble, minor in low */
	u_char length;		/*  5: length starting from signature */
	u_int control;		/*  6: event notification flags */
	u_char checksum;	/*  8: sum of all header fields should be 0 */
	u_long event_flags;	/*  9: */
	int (*rm_entry)();	/*  d: Real mode entry address */
	u_int pm_entry_offset;	/* 11: Prot mode entry address offset */
	u_long pm_entry_seg;	/* 13: Prot mode entry address segment base */
	u_long oem_dev_id;	/* 17: */
	u_int rm_data_seg;	/* 1b: Real mode data segment */
	u_long pm_data_seg;	/* 1d: Prot mode data segment base */
} pnpbios_hdr_t;

#define	LPT_COM_PNPBIOS 0
#define	REST_PNPBIOS 1


#pragma pack()

#ifdef	__cplusplus
}
#endif

#endif	/* _PNPBIOS_H */
