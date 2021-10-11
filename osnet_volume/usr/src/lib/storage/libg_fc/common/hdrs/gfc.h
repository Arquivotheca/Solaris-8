/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	Generic Fibre Channel Library definitions
 */

/*
 * I18N message number ranges
 *  This file: 19500 - 19999
 *  Shared common messages: 1 - 1999
 */

#ifndef	_GFC_H
#define	_GFC_H

#pragma ident	"@(#)gfc.h	1.20	99/08/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Put your include files here
 */
#ifndef _SYS_TYPES_H
#include <sys/types.h>
#endif /* _SYS_TYPES_H */

/* Defines */
#define		WWN_S_LEN	17 	/* NULL terminated string */
#define		WWN_SIZE	8

/*
 * The masks defined below are for the Fibre channel transport and FCAs.
 * Mask names starting with FC4 are for the non-fabric fibre channel driver
 * stack and those starting with FC are for the fabric fibre channel driver
 * stack.
 *
 * The transport values are represented in the low order 16 bits and FCA
 * values represented in the high order 16 bits.
 *
 * The notation used is as shown below :
 * (starting from the low order byte)
 * Byte 1 - holds the non-fabric FC transport driver defines
 * Byte 2 - holds the fabric FC transport driver defines
 * Byte 3 - holds the non-fabric FC FCA defines
 * Byte 4 - holds the fabric FC FCA defines
 */
/* Recognized Transport categories */
#define	FC4_SF_XPORT	0x00000001
#define	FC4_IFP_XPORT	0x00000002
#define	FC_GEN_XPORT	0x00000100

/* Transport masks */
#define	FC4_XPORT_MASK	0x000000FF
#define	FC_XPORT_MASK	0x0000FF00
#define	XPORT_MASK	(FC_XPORT_MASK | FC4_XPORT_MASK)

/* Recognized Fibre Channel Adapters */
#define	FC4_SOCAL_FCA	0x00010000
#define	FC4_PCI_FCA	0x00020000
#define	FC_USOC_FCA	0x01000000
#define	FC_PCI_FCA	0x02000000

/* FCA masks */
#define	FC4_FCA_MASK	0x00FF0000
#define	FC_FCA_MASK	0xFF000000
#define	FCA_MASK	(FC_FCA_MASK | FC4_FCA_MASK)

/*
 * Disk ports
 */
#define	PORT_B			0x00
#define	PORT_A			0x01
#define	FC_PORT_A		0x00
#define	FC_PORT_B		0x01
#define	PORT_A_B		0x02

/* Constants used by g_set_port_state() */
#define	PORT_OFFLINE	0
#define	PORT_ONLINE	1

/* Exported Variables */
extern uchar_t g_switch_to_alpa[];
extern uchar_t g_sf_alpa_to_switch[];


/* Exported Structures */

/*	Device Map	*/
typedef struct	al_rls {
	char			driver_path[MAXNAMELEN];
	uint_t			al_ha;
	struct rls_payload	payload;
	struct al_rls		*next;
} AL_rls;


/* Multi path list */
struct	dlist	{
	char	*dev_path;
	char	*logical_path;
	struct	dlist *multipath;
	struct	dlist *next;
	struct	dlist *prev;
};


/* Individual drive state */
typedef struct g_disk_state_struct {
	uint_t		num_blocks;		 /* Capacity */
	char		physical_path[MAXNAMELEN];	/* First one found */
	struct dlist	*multipath_list;
	char		node_wwn_s[WWN_S_LEN];	 /* NULL terminated str */
	int		persistent_reserv_flag;
	int		persistent_active, persistent_registered;
	int		d_state_flags[2];	 /* Disk state */
	int		port_a_valid;		 /* If disk state is valid */
	int		port_b_valid;		 /* If disk state is valid */
	char		port_a_wwn_s[WWN_S_LEN]; /* NULL terminated string */
	char		port_b_wwn_s[WWN_S_LEN]; /* NULL terminated string */
} G_disk_state;


typedef	struct hotplug_disk_list {
	struct dlist		*seslist;
	struct dlist		*dlhead;
	char			box_name[33];
	char			dev_name[MAXPATHLEN];
	char			node_wwn_s[17];
	int			tid;
	int			slot;
	int			f_flag; /* Front flag */
	int			dev_type;
	int			dev_location; /* device in A5000 or not */
	int			busy_flag;
	int			reserve_flag;
	struct hotplug_disk_list	*next;
	struct hotplug_disk_list	*prev;
} Hotplug_Devlist;

typedef struct l_inquiry_struct {
	/*
	* byte 0
	*
	* Bits 7-5 are the Peripheral Device Qualifier
	* Bits 4-0 are the Peripheral Device Type
	*
	*/
	uchar_t	inq_dtype;
	/* byte 1 */
	uchar_t	inq_rmb		: 1,	/* removable media */
		inq_qual	: 7;	/* device type qualifier */

	/* byte 2 */
	uchar_t	inq_iso		: 2,	/* ISO version */
		inq_ecma	: 3,	/* ECMA version */
		inq_ansi	: 3;	/* ANSI version */

	/* byte 3 */
#define	inq_aerc inq_aenc	/* SCSI-3 */
	uchar_t	inq_aenc	: 1,	/* async event notification cap. */
		inq_trmiop	: 1,	/* supports TERMINATE I/O PROC msg */
		inq_normaca	: 1,	/* Normal ACA Supported */
				: 1,	/* reserved */
		inq_rdf		: 4;	/* response data format */

	/* bytes 4-7 */
	uchar_t	inq_len;		/* additional length */
	uchar_t			: 8;	/* reserved */
	uchar_t			: 2,	/* reserved */
		inq_port	: 1,	/* Only defined when dual_p set */
		inq_dual_p	: 1,	/* Dual Port */
		inq_mchngr	: 1,	/* Medium Changer */
		inq_SIP_1	: 3;	/* Interlocked Protocol */

	union {
		uchar_t	inq_2_reladdr	: 1,	/* relative addressing */
			inq_wbus32	: 1,	/* 32 bit wide data xfers */
			inq_wbus16	: 1,	/* 16 bit wide data xfers */
			inq_sync	: 1,	/* synchronous data xfers */
			inq_linked	: 1,	/* linked commands */
			inq_res1	: 1,	/* reserved */
			inq_cmdque	: 1,	/* command queueing */
			inq_sftre	: 1;	/* Soft Reset option */
		uchar_t	inq_3_reladdr	: 1,	/* relative addressing */
			inq_SIP_2	: 3,	/* Interlocked Protocol */
			inq_3_linked	: 1,	/* linked commands */
			inq_trandis	: 1,	/* Transfer Disable */
			inq_3_cmdque	: 1,	/* command queueing */
			inq_SIP_3	: 1;	/* Interlocked Protocol */
	} ui;


	/* bytes 8-35 */

	uchar_t	inq_vid[8];		/* vendor ID */

	uchar_t	inq_pid[16];		/* product ID */

	uchar_t	inq_revision[4];	/* product revision level */

	/*
	 * Bytes 36-55 are vendor-specific parameter bytes
	 */

	/* SSA specific definitions */
	/* bytes 36 - 39 */
#define	inq_ven_specific_1 inq_firmware_rev
	uchar_t	inq_firmware_rev[4];	/* firmware revision level */

	/* bytes 40 - 51 */
	uchar_t	inq_serial[12];		/* serial number */

	/* bytes 52-53 */
	uchar_t	inq_res2[2];

	/* byte 54, 55 */
	uchar_t	inq_ssa_ports;		/* number of ports */
	uchar_t	inq_ssa_tgts;		/* number of targets */

	/*
	 * Bytes 56-95 are reserved.
	 */
	uchar_t	inq_res3[40];
	/*
	 * 96 to 'n' are vendor-specific parameter bytes
	 */
	uchar_t	inq_box_name[32];
	uchar_t	inq_avu[256];
} L_inquiry;


typedef struct wwn_list_struct {
	char	*logical_path;
	char	*physical_path;
	char	node_wwn_s[WWN_S_LEN];	/* NULL terminated string */
	uchar_t	w_node_wwn[WWN_SIZE];
	char	port_wwn_s[WWN_S_LEN];	/* NULL terminated string */
	uchar_t	device_type;	/* disk or tape (Peripheral Device Type) */
	struct	wwn_list_struct	*wwn_prev;
	struct	wwn_list_struct	*wwn_next;
} WWN_list;



/*
 * Prototypes of Exported functions which are defined in libg_fc
 * They are all CONTRACT PRIVATE
 */

#if defined(__STDC__)

extern int	g_dev_start(char *, int);
extern int	g_dev_stop(char *, struct wwn_list_struct *, int);
extern int	g_force_lip(char *, int);
extern int	g_forcelip_all(struct hotplug_disk_list *);
extern void	g_free_multipath(struct dlist *);
extern void	g_free_wwn_list(struct wwn_list_struct **);
extern int	g_get_dev_map(char *, sf_al_map_t *, int);
extern char 	*g_get_dev_or_bus_phys_name(char *);
extern char 	*g_get_errString(int);
extern int	g_get_inquiry(char *, L_inquiry *);
extern int	g_get_limited_map(char *, struct lilpmap *, int);
extern int	g_get_multipath(char *, struct dlist **,
		struct wwn_list_struct *, int);
extern int	g_get_nexus_path(char *, char **);
extern char 	*g_get_physical_name_from_link(char *);
extern char 	*g_get_physical_name(char *);
extern int	g_get_wwn(char *, uchar_t *, uchar_t *, int *, int);
extern int	g_get_wwn_list(struct wwn_list_struct **, int);
extern int	g_i18n_catopen(void);
extern int	g_offline_drive(struct dlist *, int);
extern void	g_online_drive(struct dlist *, int);
extern int	g_rdls(char *, struct al_rls **, int);
extern uint_t	g_get_path_type(char *);
extern int	g_get_host_params(int, fc_port_dev_t *, int);
extern int	g_port_offline(char *);
extern int	g_port_online(char *);

#else /* __STDC__ */

extern int	g_dev_start();
extern int	g_dev_stop();
extern int	g_force_lip();
extern int	g_forcelip_all();
extern void	g_free_multipath();
extern void	g_free_wwn_list();
extern int	g_get_dev_map();
extern char 	*g_get_dev_or_bus_phys_name();
extern char 	*g_get_errString();
extern int	g_get_inquiry();
extern int	g_get_limited_map();
extern int	g_get_multipath();
extern int	g_get_nexus_path();
extern int	g_get_wwn_list();
extern int	g_offline_drive();
extern void	g_online_drive();
extern char 	*g_get_physical_name();
extern char 	*g_get_physical_name_from_link();
extern int	g_get_wwn();
extern int	g_i18n_catopen();
extern int	g_rdls();
extern uint_t	g_get_path_type();
extern int	g_get_host_params();
extern int	g_port_offline();
extern int	g_port_online();

#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif /* _GFC_H */
