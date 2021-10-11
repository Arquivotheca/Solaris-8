/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Data Link Provider Interface, Version 2.0
 * Refer to document: "STREAMS DLPI Spec", 800-6915-01.
 */

#ifndef	_SYS_DLPI_H
#define	_SYS_DLPI_H

#pragma ident	"@(#)dlpi.h	1.23	98/04/28 SMI"	/* SVr4.0 1.2	*/

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sun additions.
 */
#define	DLIOC		('D' << 8)
#define	DLIOCRAW	(DLIOC|1)	/* M_DATA "raw" mode */
#define	DL_IOC_HDR_INFO	(DLIOC|10)	/* XXX reserved */

/*
 * DLPI revision definition history
 */
#define	DL_CURRENT_VERSION	0x02	/* current version of dlpi */
#define	DL_VERSION_2		0x02	/* version of dlpi March 12, 1991 */

/*
 * Primitives for Local Management Services
 */
#define	DL_INFO_REQ		0x00	/* Information Req */
#define	DL_INFO_ACK		0x03	/* Information Ack */
#define	DL_ATTACH_REQ		0x0b	/* Attach a PPA */
#define	DL_DETACH_REQ		0x0c	/* Detach a PPA */
#define	DL_BIND_REQ		0x01	/* Bind dlsap address */
#define	DL_BIND_ACK		0x04	/* Dlsap address bound */
#define	DL_UNBIND_REQ		0x02	/* Unbind dlsap address */
#define	DL_OK_ACK		0x06	/* Success acknowledgment */
#define	DL_ERROR_ACK		0x05	/* Error acknowledgment */
#define	DL_SUBS_BIND_REQ	0x1b	/* Bind Subsequent DLSAP address */
#define	DL_SUBS_BIND_ACK	0x1c	/* Subsequent DLSAP address bound */
#define	DL_SUBS_UNBIND_REQ	0x15	/* Subsequent unbind */
#define	DL_ENABMULTI_REQ	0x1d	/* Enable multicast addresses */
#define	DL_DISABMULTI_REQ	0x1e	/* Disable multicast addresses */
#define	DL_PROMISCON_REQ	0x1f	/* Turn on promiscuous mode */
#define	DL_PROMISCOFF_REQ	0x20	/* Turn off promiscuous mode */

/*
 * Primitives used for Connectionless Service
 */
#define	DL_UNITDATA_REQ		0x07	/* datagram send request */
#define	DL_UNITDATA_IND		0x08	/* datagram receive indication */
#define	DL_UDERROR_IND		0x09	/* datagram error indication */
#define	DL_UDQOS_REQ		0x0a	/* set QOS for subsequent datagrams */

/*
 * Primitives used for Connection-Oriented Service
 */
#define	DL_CONNECT_REQ		0x0d	/* Connect request */
#define	DL_CONNECT_IND		0x0e	/* Incoming connect indication */
#define	DL_CONNECT_RES		0x0f	/* Accept previous connect indication */
#define	DL_CONNECT_CON		0x10	/* Connection established */

#define	DL_TOKEN_REQ		0x11	/* Passoff token request */
#define	DL_TOKEN_ACK		0x12	/* Passoff token ack */

#define	DL_DISCONNECT_REQ	0x13	/* Disconnect request */
#define	DL_DISCONNECT_IND	0x14	/* Disconnect indication */

#define	DL_RESET_REQ		0x17	/* Reset service request */
#define	DL_RESET_IND		0x18	/* Incoming reset indication */
#define	DL_RESET_RES		0x19	/* Complete reset processing */
#define	DL_RESET_CON		0x1a	/* Reset processing complete */

/*
 *  Primitives used for Acknowledged Connectionless Service
 */

#define	DL_DATA_ACK_REQ		0x21	/* data unit transmission request */
#define	DL_DATA_ACK_IND		0x22	/* Arrival of a command PDU */
#define	DL_DATA_ACK_STATUS_IND	0x23	/* Status indication of DATA_ACK_REQ */
#define	DL_REPLY_REQ		0x24	/* Request a DLSDU from the remote */
#define	DL_REPLY_IND		0x25	/* Arrival of a command PDU */
#define	DL_REPLY_STATUS_IND	0x26	/* Status indication of REPLY_REQ */
#define	DL_REPLY_UPDATE_REQ	0x27	/* Hold a DLSDU for transmission */
#define	DL_REPLY_UPDATE_STATUS_IND	0x28 /* Status of REPLY_UPDATE req */

/*
 * Primitives used for XID and TEST operations
 */

#define	DL_XID_REQ	0x29		/* Request to send an XID PDU */
#define	DL_XID_IND	0x2a		/* Arrival of an XID PDU */
#define	DL_XID_RES	0x2b		/* request to send a response XID PDU */
#define	DL_XID_CON	0x2c		/* Arrival of a response XID PDU */
#define	DL_TEST_REQ	0x2d		/* TEST command request */
#define	DL_TEST_IND	0x2e		/* TEST response indication */
#define	DL_TEST_RES	0x2f		/* TEST response */
#define	DL_TEST_CON	0x30		/* TEST Confirmation */

/*
 * Primitives to get and set the physical address, and to get
 * Statistics
 */

#define	DL_PHYS_ADDR_REQ	0x31	/* Request to get physical addr */
#define	DL_PHYS_ADDR_ACK	0x32	/* Return physical addr */
#define	DL_SET_PHYS_ADDR_REQ	0x33	/* set physical addr */
#define	DL_GET_STATISTICS_REQ	0x34	/* Request to get statistics */
#define	DL_GET_STATISTICS_ACK	0x35	/* Return statistics */

/*
 * DLPI interface states
 */
#define	DL_UNATTACHED		0x04	/* PPA not attached */
#define	DL_ATTACH_PENDING	0x05	/* Waiting ack of DL_ATTACH_REQ */
#define	DL_DETACH_PENDING	0x06	/* Waiting ack of DL_DETACH_REQ */
#define	DL_UNBOUND		0x00	/* PPA attached */
#define	DL_BIND_PENDING		0x01	/* Waiting ack of DL_BIND_REQ */
#define	DL_UNBIND_PENDING	0x02	/* Waiting ack of DL_UNBIND_REQ */
#define	DL_IDLE			0x03	/* dlsap bound, awaiting use */
#define	DL_UDQOS_PENDING	0x07	/* Waiting ack of DL_UDQOS_REQ */
#define	DL_OUTCON_PENDING	0x08	/* awaiting DL_CONN_CON */
#define	DL_INCON_PENDING	0x09	/* awaiting DL_CONN_RES */
#define	DL_CONN_RES_PENDING	0x0a	/* Waiting ack of DL_CONNECT_RES */
#define	DL_DATAXFER		0x0b	/* connection-oriented data transfer */
#define	DL_USER_RESET_PENDING	0x0c	/* awaiting DL_RESET_CON */
#define	DL_PROV_RESET_PENDING	0x0d	/* awaiting DL_RESET_RES */
#define	DL_RESET_RES_PENDING	0x0e	/* Waiting ack of DL_RESET_RES */
#define	DL_DISCON8_PENDING	0x0f	/* Waiting ack of DL_DISC_REQ */
#define	DL_DISCON9_PENDING	0x10	/* Waiting ack of DL_DISC_REQ */
#define	DL_DISCON11_PENDING	0x11	/* Waiting ack of DL_DISC_REQ */
#define	DL_DISCON12_PENDING	0x12	/* Waiting ack of DL_DISC_REQ */
#define	DL_DISCON13_PENDING	0x13	/* Waiting ack of DL_DISC_REQ */
#define	DL_SUBS_BIND_PND	0x14	/* Waiting ack of DL_SUBS_BIND_REQ */
#define	DL_SUBS_UNBIND_PND	0x15	/* Waiting ack of DL_SUBS_UNBIND_REQ */


/*
 * DL_ERROR_ACK error return values
 */
#define	DL_ACCESS	0x02	/* Improper permissions for request */
#define	DL_BADADDR	0x01	/* DLSAP addr in improper format or invalid */
#define	DL_BADCORR	0x05	/* Seq number not from outstand DL_CONN_IND */
#define	DL_BADDATA	0x06	/* User data exceeded provider limit */
#define	DL_BADPPA	0x08	/* Specified PPA was invalid */
#define	DL_BADPRIM	0x09	/* Primitive received not known by provider */
#define	DL_BADQOSPARAM	0x0a	/* QOS parameters contained invalid values */
#define	DL_BADQOSTYPE	0x0b	/* QOS structure type is unknown/unsupported */
#define	DL_BADSAP	0x00	/* Bad LSAP selector */
#define	DL_BADTOKEN	0x0c	/* Token used not an active stream */
#define	DL_BOUND	0x0d	/* Attempted second bind with dl_max_conind */
#define	DL_INITFAILED	0x0e	/* Physical Link initialization failed */
#define	DL_NOADDR	0x0f	/* Provider couldn't allocate alt. address */
#define	DL_NOTINIT	0x10	/* Physical Link not initialized */
#define	DL_OUTSTATE	0x03	/* Primitive issued in improper state */
#define	DL_SYSERR	0x04	/* UNIX system error occurred */
#define	DL_UNSUPPORTED	0x07	/* Requested serv. not supplied by provider */
#define	DL_UNDELIVERABLE 0x11	/* Previous data unit could not be delivered */
#define	DL_NOTSUPPORTED  0x12	/* Primitive is known but not supported */
#define	DL_TOOMANY	0x13	/* limit exceeded	*/
#define	DL_NOTENAB	0x14	/* Promiscuous mode not enabled */
#define	DL_BUSY		0x15	/* Other streams for PPA in post-attached */

#define	DL_NOAUTO	0x16	/* Automatic handling XID&TEST not supported */
#define	DL_NOXIDAUTO	0x17    /* Automatic handling of XID not supported */
#define	DL_NOTESTAUTO	0x18	/* Automatic handling of TEST not supported */
#define	DL_XIDAUTO	0x19	/* Automatic handling of XID response */
#define	DL_TESTAUTO	0x1a	/* AUtomatic handling of TEST response */
#define	DL_PENDING	0x1b	/* pending outstanding connect indications */

/*
 * DLPI media types supported
 */
#define	DL_CSMACD	0x0	/* IEEE 802.3 CSMA/CD network */
#define	DL_TPB		0x1	/* IEEE 802.4 Token Passing Bus */
#define	DL_TPR		0x2	/* IEEE 802.5 Token Passing Ring */
#define	DL_METRO	0x3	/* IEEE 802.6 Metro Net */
#define	DL_ETHER	0x4	/* Ethernet Bus */
#define	DL_HDLC		0x05	/* ISO HDLC protocol support */
#define	DL_CHAR		0x06	/* Character Synchronous protocol support */
#define	DL_CTCA		0x07	/* IBM Channel-to-Channel Adapter */
#define	DL_FDDI		0x08	/* Fiber Distributed data interface */
#define	DL_FC		0x10	/* Fibre Channel interface */
#define	DL_ATM		0x11	/* ATM */
#define	DL_IPATM	0x12	/* ATM Classical IP interface */
#define	DL_X25		0x13	/* X.25 LAPB interface */
#define	DL_ISDN		0x14	/* ISDN interface */
#define	DL_HIPPI	0x15	/* HIPPI interface */
#define	DL_100VG	0x16	/* 100 Based VG Ethernet */
#define	DL_100VGTPR	0x17	/* 100 Based VG Token Ring */
#define	DL_ETH_CSMA	0x18	/* ISO 8802/3 and Ethernet */
#define	DL_100BT	0x19	/* 100 Base T */
#define	DL_FRAME	0x0a	/* Frame Relay LAPF */
#define	DL_MPFRAME	0x0b	/* Multi-protocol over Frame Relay */
#define	DL_ASYNC	0x0c	/* Character Asynchronous Protocol */
#define	DL_IPX25	0x0d	/* X.25 Classical IP interface */
#define	DL_LOOP		0x0e	/* software loopback */
#define	DL_OTHER	0x09	/* Any other medium not listed above */

/*
 * DLPI provider service supported.
 * These must be allowed to be bitwise-OR for dl_service_mode in
 * DL_INFO_ACK.
 */
#define	DL_CODLS	0x01	/* support connection-oriented service */
#define	DL_CLDLS	0x02	/* support connectionless data link service */
#define	DL_ACLDLS	0x04	/* support acknowledged connectionless serv. */


/*
 * DLPI provider style.
 * The DLPI provider style which determines whether a provider
 * requires a DL_ATTACH_REQ to inform the provider which PPA
 * user messages should be sent/received on.
 */
#define	DL_STYLE1	0x0500	/* PPA is implicitly bound by open(2) */
#define	DL_STYLE2	0x0501	/* PPA must be expl. bound via DL_ATTACH_REQ */


/*
 * DLPI Originator for Disconnect and Resets
 */
#define	DL_PROVIDER	0x0700
#define	DL_USER		0x0701

/*
 * DLPI Disconnect Reasons
 */
#define	DL_CONREJ_DEST_UNKNOWN			0x0800
#define	DL_CONREJ_DEST_UNREACH_PERMANENT	0x0801
#define	DL_CONREJ_DEST_UNREACH_TRANSIENT	0x0802
#define	DL_CONREJ_QOS_UNAVAIL_PERMANENT		0x0803
#define	DL_CONREJ_QOS_UNAVAIL_TRANSIENT		0x0804
#define	DL_CONREJ_PERMANENT_COND		0x0805
#define	DL_CONREJ_TRANSIENT_COND		0x0806
#define	DL_DISC_ABNORMAL_CONDITION		0x0807
#define	DL_DISC_NORMAL_CONDITION		0x0808
#define	DL_DISC_PERMANENT_CONDITION		0x0809
#define	DL_DISC_TRANSIENT_CONDITION		0x080a
#define	DL_DISC_UNSPECIFIED			0x080b

/*
 * DLPI Reset Reasons
 */
#define	DL_RESET_FLOW_CONTROL	0x0900
#define	DL_RESET_LINK_ERROR	0x0901
#define	DL_RESET_RESYNCH	0x0902

/*
 * DLPI status values for acknowledged connectionless data transfer
 */
#define	DL_CMD_MASK	0x0f	/* mask for command portion of status */
#define	DL_CMD_OK	0x00	/* Command Accepted */
#define	DL_CMD_RS	0x01	/* Unimplemented or inactivated service */
#define	DL_CMD_UE	0x05	/* Data Link User interface error */
#define	DL_CMD_PE	0x06	/* Protocol error */
#define	DL_CMD_IP	0x07	/* Permanent implementation dependent error */
#define	DL_CMD_UN	0x09	/* Resources temporarily unavailable */
#define	DL_CMD_IT	0x0f	/* Temporary implementation dependent error */
#define	DL_RSP_MASK	0xf0	/* mask for response portion of status */
#define	DL_RSP_OK	0x00	/* Response DLSDU present */
#define	DL_RSP_RS	0x10	/* Unimplemented or inactivated service */
#define	DL_RSP_NE	0x30	/* Response DLSDU never submitted */
#define	DL_RSP_NR	0x40	/* Response DLSDU not requested */
#define	DL_RSP_UE	0x50	/* Data Link User interface error */
#define	DL_RSP_IP	0x70	/* Permanent implementation dependent error */
#define	DL_RSP_UN	0x90	/* Resources temporarily unavailable */
#define	DL_RSP_IT	0xf0	/* Temporary implementation dependent error */

/*
 * Service Class values for acknowledged connectionless data transfer
 */
#define	DL_RQST_RSP	0x01	/* Use acknowledge capability in MAC sublayer */
#define	DL_RQST_NORSP	0x02	/* No acknowledgement service requested */

/*
 * DLPI address type definition
 */
#define	DL_FACT_PHYS_ADDR	0x01	/* factory physical address */
#define	DL_CURR_PHYS_ADDR	0x02	/* current physical address */

/*
 * DLPI flag definitions
 */
#define	DL_POLL_FINAL	0x01		/* indicates poll/final bit set */

/*
 *	XID and TEST responses supported by the provider
 */
#define	DL_AUTO_XID	0x01		/* provider will respond to XID */
#define	DL_AUTO_TEST	0x02		/* provider will respond to TEST */

/*
 * Subsequent bind type
 */
#define	DL_PEER_BIND	0x01		/* subsequent bind on a peer addr */
#define	DL_HIERARCHICAL_BIND	0x02	/* subs_bind on a hierarchical addr */

/*
 * DLPI promiscuous mode definitions
 */
#define	DL_PROMISC_PHYS		0x01	/* promiscuous mode at phys level */
#define	DL_PROMISC_SAP		0x02	/* promiscous mode at sap level */
#define	DL_PROMISC_MULTI	0x03	/* promiscuous mode for multicast */

/*
 * DLPI Quality Of Service definition for use in QOS structure definitions.
 * The QOS structures are used in connection establishment, DL_INFO_ACK,
 * and setting connectionless QOS values.
 */

/*
 * Throughput
 *
 * This parameter is specified for both directions.
 */
typedef struct {
	t_scalar_t	dl_target_value;	/* bits/second desired */
	t_scalar_t	dl_accept_value;	/* min. ok bits/second */
} dl_through_t;

/*
 * transit delay specification
 *
 * This parameter is specified for both directions.
 * expressed in milliseconds assuming a DLSDU size of 128 octets.
 * The scaling of the value to the current DLSDU size is provider dependent.
 */
typedef struct {
	t_scalar_t	dl_target_value;	/* desired value of service */
	t_scalar_t	dl_accept_value;	/* min. ok value of service */
} dl_transdelay_t;

/*
 * priority specification
 * priority range is 0-100, with 0 being highest value.
 */
typedef struct {
	t_scalar_t	dl_min;
	t_scalar_t	dl_max;
} dl_priority_t;


/*
 * protection specification
 *
 */
#define	DL_NONE			0x0B01	/* no protection supplied */
#define	DL_MONITOR		0x0B02	/* prot. from passive monit. */
#define	DL_MAXIMUM		0x0B03	/* prot. from modification, replay, */
					/* addition, or deletion */

typedef struct {
	t_scalar_t	dl_min;
	t_scalar_t	dl_max;
} dl_protect_t;


/*
 * Resilience specification
 * probabilities are scaled by a factor of 10,000 with a time interval
 * of 10,000 seconds.
 */
typedef struct {
	t_scalar_t	dl_disc_prob;	/* prob. of provider init DISC */
	t_scalar_t	dl_reset_prob;	/* prob. of provider init RESET */
} dl_resilience_t;


/*
 * QOS type definition to be used for negotiation with the
 * remote end of a connection, or a connectionless unitdata request.
 * There are two type definitions to handle the negotiation
 * process at connection establishment. The typedef dl_qos_range_t
 * is used to present a range for parameters. This is used
 * in the DL_CONNECT_REQ and DL_CONNECT_IND messages. The typedef
 * dl_qos_sel_t is used to select a specific value for the QOS
 * parameters. This is used in the DL_CONNECT_RES, DL_CONNECT_CON,
 * and DL_INFO_ACK messages to define the selected QOS parameters
 * for a connection.
 *
 * NOTE
 *	A DataLink provider which has unknown values for any of the fields
 *	will use a value of DL_UNKNOWN for all values in the fields.
 *
 * NOTE
 *	A QOS parameter value of DL_QOS_DONT_CARE informs the DLS
 *	provider the user requesting this value doesn't care
 *	what the QOS parameter is set to. This value becomes the
 *	least possible value in the range of QOS parameters.
 *	The order of the QOS parameter range is then:
 *
 *		DL_QOS_DONT_CARE < 0 < MAXIMUM QOS VALUE
 */
#define	DL_UNKNOWN		-1
#define	DL_QOS_DONT_CARE	-2

/*
 * Every QOS structure has the first 4 bytes containing a type
 * field, denoting the definition of the rest of the structure.
 * This is used in the same manner has the dl_primitive variable
 * is in messages.
 *
 * The following list is the defined QOS structure type values and structures.
 */
#define	DL_QOS_CO_RANGE1	0x0101	/* CO QOS range struct. */
#define	DL_QOS_CO_SEL1		0x0102	/* CO QOS selection structure */
#define	DL_QOS_CL_RANGE1	0x0103	/* CL QOS range struct. */
#define	DL_QOS_CL_SEL1		0x0104	/* CL QOS selection */

typedef struct {
	t_uscalar_t	dl_qos_type;
	dl_through_t	dl_rcv_throughput;	/* desired and accep. */
	dl_transdelay_t	dl_rcv_trans_delay;	/* desired and accep. */
	dl_through_t	dl_xmt_throughput;
	dl_transdelay_t	dl_xmt_trans_delay;
	dl_priority_t	dl_priority;		/* min and max values */
	dl_protect_t	dl_protection;		/* min and max values */
	t_scalar_t	dl_residual_error;
	dl_resilience_t	dl_resilience;
}	dl_qos_co_range1_t;

typedef struct {
	t_uscalar_t	dl_qos_type;
	t_scalar_t	dl_rcv_throughput;
	t_scalar_t	dl_rcv_trans_delay;
	t_scalar_t	dl_xmt_throughput;
	t_scalar_t	dl_xmt_trans_delay;
	t_scalar_t	dl_priority;
	t_scalar_t	dl_protection;
	t_scalar_t	dl_residual_error;
	dl_resilience_t	dl_resilience;
}	dl_qos_co_sel1_t;

typedef struct {
	t_uscalar_t	dl_qos_type;
	dl_transdelay_t	dl_trans_delay;
	dl_priority_t	dl_priority;
	dl_protect_t	dl_protection;
	t_scalar_t	dl_residual_error;
}	dl_qos_cl_range1_t;

typedef struct {
	t_uscalar_t	dl_qos_type;
	t_scalar_t	dl_trans_delay;
	t_scalar_t	dl_priority;
	t_scalar_t	dl_protection;
	t_scalar_t	dl_residual_error;
}	dl_qos_cl_sel1_t;

/*
 * DLPI interface primitive definitions.
 *
 * Each primitive is sent as a stream message.  It is possible that
 * the messages may be viewed as a sequence of bytes that have the
 * following form without any padding. The structure definition
 * of the following messages may have to change depending on the
 * underlying hardware architecture and crossing of a hardware
 * boundary with a different hardware architecture.
 *
 * Fields in the primitives having a name of the form
 * dl_reserved cannot be used and have the value of
 * binary zero, no bits turned on.
 *
 * Each message has the name defined followed by the
 * stream message type (M_PROTO, M_PCPROTO, M_DATA)
 */

/*
 *	LOCAL MANAGEMENT SERVICE PRIMITIVES
 */

/*
 * DL_INFO_REQ, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;			/* set to DL_INFO_REQ */
} dl_info_req_t;

/*
 * DL_INFO_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* set to DL_INFO_ACK */
	t_uscalar_t	dl_max_sdu;		/* Max bytes in a DLSDU */
	t_uscalar_t	dl_min_sdu;		/* Min bytes in a DLSDU */
	t_uscalar_t	dl_addr_length;		/* length of DLSAP address */
	t_uscalar_t	dl_mac_type;		/* type of medium supported */
	t_uscalar_t	dl_reserved;		/* value set to zero */
	t_uscalar_t	dl_current_state;	/* state of DLPI interface */
	t_scalar_t	dl_sap_length;		/* length of dlsap SAP part */
	t_uscalar_t	dl_service_mode;	/* CO, CL or ACL */
	t_uscalar_t	dl_qos_length;		/* length of qos values */
	t_uscalar_t	dl_qos_offset;		/* offset from start of block */
	t_uscalar_t	dl_qos_range_length;	/* available range of qos */
	t_uscalar_t	dl_qos_range_offset;	/* offset from start of block */
	t_uscalar_t	dl_provider_style;	/* style1 or style2 */
	t_uscalar_t	dl_addr_offset;		/* offset of the dlsap addr */
	t_uscalar_t	dl_version;		/* version number */
	t_uscalar_t	dl_brdcst_addr_length;	/* length of broadcast addr */
	t_uscalar_t	dl_brdcst_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_growth;		/* set to zero */
} dl_info_ack_t;

/*
 * DL_ATTACH_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* set to DL_ATTACH_REQ */
	t_uscalar_t	dl_ppa;			/* id of the PPA */
} dl_attach_req_t;

/*
 * DL_DETACH_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* set to DL_DETACH_REQ */
} dl_detach_req_t;

/*
 * DL_BIND_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* set to DL_BIND_REQ */
	t_uscalar_t	dl_sap;		/* info to identify dlsap addr */
	t_uscalar_t	dl_max_conind;	/* max # of outstanding con_ind */
	uint16_t	dl_service_mode;	/* CO, CL or ACL */
	uint16_t	dl_conn_mgmt;	/* if non-zero, is con-mgmt stream */
	t_uscalar_t	dl_xidtest_flg;	/* auto init. of test and xid */
} dl_bind_req_t;

/*
 * DL_BIND_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_BIND_ACK */
	t_uscalar_t	dl_sap;		/* DLSAP addr info */
	t_uscalar_t	dl_addr_length;	/* length of complete DLSAP addr */
	t_uscalar_t	dl_addr_offset;	/* offset from start of M_PCPROTO */
	t_uscalar_t	dl_max_conind;	/* allowed max. # of con-ind */
	t_uscalar_t	dl_xidtest_flg;	/* responses supported by provider */
} dl_bind_ack_t;

/*
 * DL_SUBS_BIND_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_SUBS_BIND_REQ */
	t_uscalar_t	dl_subs_sap_offset;	/* offset of subs_sap */
	t_uscalar_t	dl_subs_sap_length;	/* length of subs_sap */
	t_uscalar_t	dl_subs_bind_class;	/* peer or hierarchical */
} dl_subs_bind_req_t;

/*
 * DL_SUBS_BIND_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t dl_primitive;	/* DL_SUBS_BIND_ACK */
	t_uscalar_t dl_subs_sap_offset;	/* offset of subs_sap */
	t_uscalar_t dl_subs_sap_length;	/* length of subs_sap */
} dl_subs_bind_ack_t;

/*
 * DL_UNBIND_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_UNBIND_REQ */
} dl_unbind_req_t;

/*
 * DL_SUBS_UNBIND_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_SUBS_UNBIND_REQ */
	t_uscalar_t	dl_subs_sap_offset;	/* offset of subs_sap */
	t_uscalar_t	dl_subs_sap_length;	/* length of subs_sap */
} dl_subs_unbind_req_t;

/*
 * DL_OK_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_OK_ACK */
	t_uscalar_t	dl_correct_primitive;	/* primitive acknowledged */
} dl_ok_ack_t;

/*
 * DL_ERROR_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_ERROR_ACK */
	t_uscalar_t	dl_error_primitive;	/* primitive in error */
	t_uscalar_t	dl_errno;		/* DLPI error code */
	t_uscalar_t	dl_unix_errno;		/* UNIX system error code */
} dl_error_ack_t;

/*
 * DL_ENABMULTI_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_ENABMULTI_REQ */
	t_uscalar_t	dl_addr_length;	/* length of multicast address */
	t_uscalar_t	dl_addr_offset;	/* offset from start of M_PROTO block */
} dl_enabmulti_req_t;

/*
 * DL_DISABMULTI_REQ, M_PROTO type
 */

typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_DISABMULTI_REQ */
	t_uscalar_t	dl_addr_length;	/* length of multicast address */
	t_uscalar_t	dl_addr_offset;	/* offset from start of M_PROTO block */
} dl_disabmulti_req_t;

/*
 * DL_PROMISCON_REQ, M_PROTO type
 */

typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_PROMISCON_REQ */
	t_uscalar_t	dl_level;	/* physical,SAP, or ALL multicast */
} dl_promiscon_req_t;

/*
 * DL_PROMISCOFF_REQ, M_PROTO type
 */

typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_PROMISCOFF_REQ */
	t_uscalar_t	dl_level;	/* Physical,SAP, or ALL multicast */
} dl_promiscoff_req_t;

/*
 *	Primitives to get and set the Physical address
 */

/*
 * DL_PHYS_ADDR_REQ, M_PROTO type
 */
typedef	struct {
	t_uscalar_t	dl_primitive;	/* DL_PHYS_ADDR_REQ */
	t_uscalar_t	dl_addr_type;	/* factory or current physical addr */
} dl_phys_addr_req_t;

/*
 * DL_PHYS_ADDR_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_PHYS_ADDR_ACK */
	t_uscalar_t	dl_addr_length;	/* length of the physical addr */
	t_uscalar_t	dl_addr_offset;	/* offset from start of block */
} dl_phys_addr_ack_t;

/*
 * DL_SET_PHYS_ADDR_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_SET_PHYS_ADDR_REQ */
	t_uscalar_t	dl_addr_length;	/* length of physical addr */
	t_uscalar_t	dl_addr_offset;	/* offset from start of block */
} dl_set_phys_addr_req_t;

/*
 *	Primitives to get statistics
 */

/*
 * DL_GET_STATISTICS_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_GET_STATISTICS_REQ */
} dl_get_statistics_req_t;

/*
 * DL_GET_STATISTICS_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_GET_STATISTICS_ACK */
	t_uscalar_t	dl_stat_length;	/* length of statistics structure */
	t_uscalar_t	dl_stat_offset;	/* offset from start of block */
} dl_get_statistics_ack_t;

/*
 *	CONNECTION-ORIENTED SERVICE PRIMITIVES
 */

/*
 * DL_CONNECT_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_CONNECT_REQ */
	t_uscalar_t	dl_dest_addr_length;	/* len. of dlsap addr */
	t_uscalar_t	dl_dest_addr_offset;	/* offset */
	t_uscalar_t	dl_qos_length;		/* len. of QOS parm val */
	t_uscalar_t	dl_qos_offset;		/* offset */
	t_uscalar_t	dl_growth;		/* set to zero */
} dl_connect_req_t;

/*
 * DL_CONNECT_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_CONNECT_IND */
	t_uscalar_t	dl_correlation;		/* provider's correl. token */
	t_uscalar_t	dl_called_addr_length;  /* length of called address */
	t_uscalar_t	dl_called_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_calling_addr_length;	/* length of calling address */
	t_uscalar_t	dl_calling_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_qos_length;		/* length of qos structure */
	t_uscalar_t	dl_qos_offset;		/* offset from start of block */
	t_uscalar_t	dl_growth;		/* set to zero */
} dl_connect_ind_t;

/*
 * DL_CONNECT_RES, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_CONNECT_RES */
	t_uscalar_t	dl_correlation; /* provider's correlation token */
	t_uscalar_t	dl_resp_token;	/* token of responding stream */
	t_uscalar_t	dl_qos_length;  /* length of qos structure */
	t_uscalar_t	dl_qos_offset;	/* offset from start of block */
	t_uscalar_t	dl_growth;	/* set to zero */
} dl_connect_res_t;

/*
 * DL_CONNECT_CON, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_CONNECT_CON */
	t_uscalar_t	dl_resp_addr_length;	/* responder's address len */
	t_uscalar_t	dl_resp_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_qos_length;		/* length of qos structure */
	t_uscalar_t	dl_qos_offset;		/* offset from start of block */
	t_uscalar_t	dl_growth;		/* set to zero */
} dl_connect_con_t;

/*
 * DL_TOKEN_REQ, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_TOKEN_REQ */
} dl_token_req_t;

/*
 * DL_TOKEN_ACK, M_PCPROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_TOKEN_ACK */
	t_uscalar_t	dl_token;	/* Connection response token */
}dl_token_ack_t;

/*
 * DL_DISCONNECT_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_DISCONNECT_REQ */
	t_uscalar_t	dl_reason;	/* norm., abnorm., perm. or trans. */
	t_uscalar_t	dl_correlation; /* association with connect_ind */
} dl_disconnect_req_t;

/*
 * DL_DISCONNECT_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_DISCONNECT_IND */
	t_uscalar_t	dl_originator;	/* USER or PROVIDER */
	t_uscalar_t	dl_reason;	/* permanent or transient */
	t_uscalar_t	dl_correlation;	/* association with connect_ind */
} dl_disconnect_ind_t;

/*
 * DL_RESET_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_RESET_REQ */
} dl_reset_req_t;

/*
 * DL_RESET_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_RESET_IND */
	t_uscalar_t	dl_originator;	/* Provider or User */
	t_uscalar_t	dl_reason;	/* flow control, link error, resync */
} dl_reset_ind_t;

/*
 * DL_RESET_RES, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_RESET_RES */
} dl_reset_res_t;

/*
 * DL_RESET_CON, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_RESET_CON */
} dl_reset_con_t;


/*
 *	CONNECTIONLESS SERVICE PRIMITIVES
 */

/*
 * DL_UNITDATA_REQ, M_PROTO type, with M_DATA block(s)
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_UNITDATA_REQ */
	t_uscalar_t	dl_dest_addr_length;	/* DLSAP length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	dl_priority_t	dl_priority;	/* priority value */
} dl_unitdata_req_t;

/*
 * DL_UNITDATA_IND, M_PROTO type, with M_DATA block(s)
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_UNITDATA_IND */
	t_uscalar_t	dl_dest_addr_length;	/* DLSAP length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* DLSAP addr length sender */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_group_address;	/* one if multicast/broadcast */
} dl_unitdata_ind_t;

/*
 * DL_UDERROR_IND, M_PROTO type
 *	(or M_PCPROTO type if LLI-based provider)
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_UDERROR_IND */
	t_uscalar_t	dl_dest_addr_length;	/* Destination DLSAP */
	t_uscalar_t	dl_dest_addr_offset;	/* Offset from start of block */
	t_uscalar_t	dl_unix_errno;		/* unix system error code */
	t_uscalar_t	dl_errno;		/* DLPI error code */
} dl_uderror_ind_t;

/*
 * DL_UDQOS_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_UDQOS_REQ */
	t_uscalar_t	dl_qos_length;	/* requested qos byte length */
	t_uscalar_t	dl_qos_offset;	/* offset from start of block */
} dl_udqos_req_t;

/*
 *	Primitives to handle XID and TEST operations
 */

/*
 * DL_TEST_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_TEST_REQ */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* DLSAP length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
} dl_test_req_t;

/*
 * DL_TEST_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_TEST_IND */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* dlsap length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* dlsap length of source */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
} dl_test_ind_t;

/*
 *	DL_TEST_RES, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_TEST_RES */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* DLSAP length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
} dl_test_res_t;

/*
 *	DL_TEST_CON, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_TEST_CON */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* dlsap length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* dlsap length of source */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
} dl_test_con_t;

/*
 * DL_XID_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_XID_REQ */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* dlsap length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
} dl_xid_req_t;

/*
 * DL_XID_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_XID_IND */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* dlsap length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* dlsap length of source */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
} dl_xid_ind_t;

/*
 *	DL_XID_RES, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_XID_RES */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* DLSAP length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
} dl_xid_res_t;

/*
 *	DL_XID_CON, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_XID_CON */
	t_uscalar_t	dl_flag;		/* poll/final */
	t_uscalar_t	dl_dest_addr_length;	/* dlsap length of dest. user */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* dlsap length of source */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
} dl_xid_con_t;

/*
 *	ACKNOWLEDGED CONNECTIONLESS SERVICE PRIMITIVES
 */

/*
 * DL_DATA_ACK_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_DATA_ACK_REQ */
	t_uscalar_t	dl_correlation;		/* User's correlation token */
	t_uscalar_t	dl_dest_addr_length;	/* length of destination addr */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* length of source address */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_priority;		/* priority */
	t_uscalar_t	dl_service_class;	/* DL_RQST_RSP|DL_RQST_NORSP */
} dl_data_ack_req_t;

/*
 * DL_DATA_ACK_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_DATA_ACK_IND */
	t_uscalar_t	dl_dest_addr_length;	/* length of destination addr */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* length of source address */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_priority;		/* pri. for data unit transm. */
	t_uscalar_t	dl_service_class;	/* DL_RQST_RSP|DL_RQST_NORSP */
} dl_data_ack_ind_t;

/*
 * DL_DATA_ACK_STATUS_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_DATA_ACK_STATUS_IND */
	t_uscalar_t	dl_correlation;	/* User's correlation token */
	t_uscalar_t	dl_status;	/* success or failure of previous req */
} dl_data_ack_status_ind_t;

/*
 * DL_REPLY_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_REPLY_REQ */
	t_uscalar_t	dl_correlation;		/* User's correlation token */
	t_uscalar_t	dl_dest_addr_length;	/* destination address length */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* source address length */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_priority;		/* pri for data unit trans. */
	t_uscalar_t	dl_service_class;
} dl_reply_req_t;

/*
 * DL_REPLY_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_REPLY_IND */
	t_uscalar_t	dl_dest_addr_length;	/* destination address length */
	t_uscalar_t	dl_dest_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_src_addr_length;	/* length of source address */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
	t_uscalar_t	dl_priority;		/* pri for data unit trans. */
	t_uscalar_t	dl_service_class;	/* DL_RQST_RSP|DL_RQST_NORSP */
} dl_reply_ind_t;

/*
 * DL_REPLY_STATUS_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_REPLY_STATUS_IND */
	t_uscalar_t	dl_correlation;	/* User's correlation token */
	t_uscalar_t	dl_status;	/* success or failure of previous req */
} dl_reply_status_ind_t;

/*
 * DL_REPLY_UPDATE_REQ, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;		/* DL_REPLY_UPDATE_REQ */
	t_uscalar_t	dl_correlation;		/* user's correlation token */
	t_uscalar_t	dl_src_addr_length;	/* length of source address */
	t_uscalar_t	dl_src_addr_offset;	/* offset from start of block */
} dl_reply_update_req_t;

/*
 * DL_REPLY_UPDATE_STATUS_IND, M_PROTO type
 */
typedef struct {
	t_uscalar_t	dl_primitive;	/* DL_REPLY_UPDATE_STATUS_IND */
	t_uscalar_t	dl_correlation;	/* User's correlation token */
	t_uscalar_t	dl_status;	/* success or failure of previous req */
} dl_reply_update_status_ind_t;

union DL_primitives {
	t_uscalar_t		dl_primitive;
	dl_info_req_t		info_req;
	dl_info_ack_t		info_ack;
	dl_attach_req_t		attach_req;
	dl_detach_req_t		detach_req;
	dl_bind_req_t		bind_req;
	dl_bind_ack_t		bind_ack;
	dl_unbind_req_t		unbind_req;
	dl_subs_bind_req_t	subs_bind_req;
	dl_subs_bind_ack_t	subs_bind_ack;
	dl_subs_unbind_req_t	subs_unbind_req;
	dl_ok_ack_t		ok_ack;
	dl_error_ack_t		error_ack;
	dl_connect_req_t	connect_req;
	dl_connect_ind_t	connect_ind;
	dl_connect_res_t	connect_res;
	dl_connect_con_t	connect_con;
	dl_token_req_t		token_req;
	dl_token_ack_t		token_ack;
	dl_disconnect_req_t	disconnect_req;
	dl_disconnect_ind_t	disconnect_ind;
	dl_reset_req_t		reset_req;
	dl_reset_ind_t		reset_ind;
	dl_reset_res_t		reset_res;
	dl_reset_con_t		reset_con;
	dl_unitdata_req_t	unitdata_req;
	dl_unitdata_ind_t	unitdata_ind;
	dl_uderror_ind_t	uderror_ind;
	dl_udqos_req_t		udqos_req;
	dl_enabmulti_req_t	enabmulti_req;
	dl_disabmulti_req_t	disabmulti_req;
	dl_promiscon_req_t	promiscon_req;
	dl_promiscoff_req_t	promiscoff_req;
	dl_phys_addr_req_t	physaddr_req;
	dl_phys_addr_ack_t	physaddr_ack;
	dl_set_phys_addr_req_t	set_physaddr_req;
	dl_get_statistics_req_t	get_statistics_req;
	dl_get_statistics_ack_t	get_statistics_ack;
	dl_test_req_t		test_req;
	dl_test_ind_t		test_ind;
	dl_test_res_t		test_res;
	dl_test_con_t		test_con;
	dl_xid_req_t		xid_req;
	dl_xid_ind_t		xid_ind;
	dl_xid_res_t		xid_res;
	dl_xid_con_t		xid_con;
	dl_data_ack_req_t	data_ack_req;
	dl_data_ack_ind_t	data_ack_ind;
	dl_data_ack_status_ind_t	data_ack_status_ind;
	dl_reply_req_t		reply_req;
	dl_reply_ind_t		reply_ind;
	dl_reply_status_ind_t	reply_status_ind;
	dl_reply_update_req_t	reply_update_req;
	dl_reply_update_status_ind_t	reply_update_status_ind;
};

#define	DL_INFO_REQ_SIZE	sizeof (dl_info_req_t)
#define	DL_INFO_ACK_SIZE	sizeof (dl_info_ack_t)
#define	DL_ATTACH_REQ_SIZE	sizeof (dl_attach_req_t)
#define	DL_DETACH_REQ_SIZE	sizeof (dl_detach_req_t)
#define	DL_BIND_REQ_SIZE	sizeof (dl_bind_req_t)
#define	DL_BIND_ACK_SIZE	sizeof (dl_bind_ack_t)
#define	DL_UNBIND_REQ_SIZE	sizeof (dl_unbind_req_t)
#define	DL_SUBS_BIND_REQ_SIZE	sizeof (dl_subs_bind_req_t)
#define	DL_SUBS_BIND_ACK_SIZE	sizeof (dl_subs_bind_ack_t)
#define	DL_SUBS_UNBIND_REQ_SIZE	sizeof (dl_subs_unbind_req_t)
#define	DL_OK_ACK_SIZE		sizeof (dl_ok_ack_t)
#define	DL_ERROR_ACK_SIZE	sizeof (dl_error_ack_t)
#define	DL_CONNECT_REQ_SIZE	sizeof (dl_connect_req_t)
#define	DL_CONNECT_IND_SIZE	sizeof (dl_connect_ind_t)
#define	DL_CONNECT_RES_SIZE	sizeof (dl_connect_res_t)
#define	DL_CONNECT_CON_SIZE	sizeof (dl_connect_con_t)
#define	DL_TOKEN_REQ_SIZE	sizeof (dl_token_req_t)
#define	DL_TOKEN_ACK_SIZE	sizeof (dl_token_ack_t)
#define	DL_DISCONNECT_REQ_SIZE	sizeof (dl_disconnect_req_t)
#define	DL_DISCONNECT_IND_SIZE	sizeof (dl_disconnect_ind_t)
#define	DL_RESET_REQ_SIZE	sizeof (dl_reset_req_t)
#define	DL_RESET_IND_SIZE	sizeof (dl_reset_ind_t)
#define	DL_RESET_RES_SIZE	sizeof (dl_reset_res_t)
#define	DL_RESET_CON_SIZE	sizeof (dl_reset_con_t)
#define	DL_UNITDATA_REQ_SIZE	sizeof (dl_unitdata_req_t)
#define	DL_UNITDATA_IND_SIZE	sizeof (dl_unitdata_ind_t)
#define	DL_UDERROR_IND_SIZE	sizeof (dl_uderror_ind_t)
#define	DL_UDQOS_REQ_SIZE	sizeof (dl_udqos_req_t)
#define	DL_ENABMULTI_REQ_SIZE	sizeof (dl_enabmulti_req_t)
#define	DL_DISABMULTI_REQ_SIZE	sizeof (dl_disabmulti_req_t)
#define	DL_PROMISCON_REQ_SIZE	sizeof (dl_promiscon_req_t)
#define	DL_PROMISCOFF_REQ_SIZE	sizeof (dl_promiscoff_req_t)
#define	DL_PHYS_ADDR_REQ_SIZE	sizeof (dl_phys_addr_req_t)
#define	DL_PHYS_ADDR_ACK_SIZE	sizeof (dl_phys_addr_ack_t)
#define	DL_SET_PHYS_ADDR_REQ_SIZE	sizeof (dl_set_phys_addr_req_t)
#define	DL_GET_STATISTICS_REQ_SIZE	sizeof (dl_get_statistics_req_t)
#define	DL_GET_STATISTICS_ACK_SIZE	sizeof (dl_get_statistics_ack_t)
#define	DL_XID_REQ_SIZE		sizeof (dl_xid_req_t)
#define	DL_XID_IND_SIZE		sizeof (dl_xid_ind_t)
#define	DL_XID_RES_SIZE		sizeof (dl_xid_res_t)
#define	DL_XID_CON_SIZE		sizeof (dl_xid_con_t)
#define	DL_TEST_REQ_SIZE	sizeof (dl_test_req_t)
#define	DL_TEST_IND_SIZE	sizeof (dl_test_ind_t)
#define	DL_TEST_RES_SIZE	sizeof (dl_test_res_t)
#define	DL_TEST_CON_SIZE	sizeof (dl_test_con_t)
#define	DL_DATA_ACK_REQ_SIZE	sizeof (dl_data_ack_req_t)
#define	DL_DATA_ACK_IND_SIZE	sizeof (dl_data_ack_ind_t)
#define	DL_DATA_ACK_STATUS_IND_SIZE	sizeof (dl_data_ack_status_ind_t)
#define	DL_REPLY_REQ_SIZE	sizeof (dl_reply_req_t)
#define	DL_REPLY_IND_SIZE	sizeof (dl_reply_ind_t)
#define	DL_REPLY_STATUS_IND_SIZE	sizeof (dl_reply_status_ind_t)
#define	DL_REPLY_UPDATE_REQ_SIZE	sizeof (dl_reply_update_req_t)
#define	DL_REPLY_UPDATE_STATUS_IND_SIZE	sizeof (dl_reply_update_status_ind_t)

#ifdef	_KERNEL
/*
 * The following are unstable, internal DLPI utility routines.
 */
extern void	dlbindack(queue_t *, mblk_t *, t_scalar_t, void *, t_uscalar_t,
		    t_uscalar_t, t_uscalar_t);
extern void	dlokack(queue_t *, mblk_t *, t_uscalar_t);
extern void	dlerrorack(queue_t *, mblk_t *, t_uscalar_t, t_uscalar_t,
		    t_uscalar_t);
extern void	dluderrorind(queue_t *, mblk_t *, void *, t_uscalar_t,
		    t_uscalar_t, t_uscalar_t);
extern void	dlphysaddrack(queue_t *, mblk_t *, void *, t_uscalar_t);
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DLPI_H */
