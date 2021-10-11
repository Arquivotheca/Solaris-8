/*
 * Copyright (c) 1992,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PPP_IOCTL_H
#define	_SYS_PPP_IOCTL_H

#pragma ident	"@(#)ppp_ioctl.h	1.15	98/06/11 SMI"

#include <netinet/in.h>
#include <sys/types32.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Declare different ioctls for communication between the user and
 * the PPP implementation
 *
 *
 *		Message direction	User | Kernel | msgtype */

#define	NO_AUTH		0
#define	DO_CHAP		(1 << 0)
#define	DO_PAP		(1 << 1)

enum ppp_ioctls	 {
	PPP_SET_CONF = 0x100,	/*    --->   pppLinkControlEntry_t	*/
	PPP_GET_CONF,		/*    <---   pppLinkControlEntry_t	*/
	PPP_SET_AUTH,		/*    --->   pppAuthControlEntry_t	*/
	PPP_GET_AUTH,		/*    <---   pppAuthControlEntry_t	*/
	PPP_ACTIVE_OPEN,	/*    --->   void  (Old protocol)	*/
	PPP_PASSIVE_OPEN,	/*    --->   void  (Old protocol)	*/
	PPP_CLOSE,		/*    --->   pppExEvent_t		*/
	PPP_UP,			/*    --->   pppExEvent_t		*/
	PPP_DOWN,		/*    --->   pppExEvent_t		*/
	PPP_SET_LOCAL_PASSWD,	/*    --->   papPasswdEntry_t		*/
	PPP_GET_REMOTE_PASSWD,	/*    <---   papPasswdEntry_t		*/
	PPP_REMOTE_OK,		/*    --->   pppPAPMessage_t		*/
	PPP_REMOTE_NOK,		/*    --->   pppPAPMessage_t		*/
	PPP_GET_STATE,		/*    <---   pppLinkStatusEntry_t	*/
	PPP_GET_LCP_STATS,	/*    <---   pppCPEntry_t		*/
	PPP_GET_IPNCP_STATS,	/*    <---   pppCPEntry_t		*/
	PPP_GET_IP_STATS,	/*    <---   pppIPEntry_t		*/
	PPP_GET_ERRS,		/*    <---   pppLinkErrorsEntry_t	*/
	PPP_SET_DEBUG,		/*    --->   unsigned int		*/
	PPP_DELETE_MIB_ENTRY,	/*    --->   unsigned int		*/
	PPP_GET_VERSION,	/*    <---   unsigned int		*/
	PPP_OPEN,		/*    --->   pppExEvent_t		*/
	PPP_AUTH_LOC,
	PPP_AUTH_REM,
	PPP_AUTH_BOTH,
	PPP_FORCE_REM,
	PPP_SET_REMOTE_PASSWD	/*    --->   pppPAP_t			*/
};

typedef enum ppp_ioctls ppp_ioctl_t;

/*
 * Note Get Version returns the version of the *Module* *Not* of the
 * the PPP protocol.
 */

/*
 * Declare different asynchronous indications which PPP can send to the
 * user at any time
 */
enum ppp_messages {
	PPP_TL_UP,		/* pppProtoUp_t		*/
	PPP_TL_DOWN,		/* pppProtoDown_t	*/
	PPP_TL_START,		/* pppProtoStart_t	*/
	PPP_TL_FINISH,		/* pppProtoFinish_t	*/
	PPP_NEED_VALIDATION,	/* pppReqValidation_t	*/
	PPP_CONFIG_CHANGED,	/* pppConfigChange_t	*/
	PPP_ERROR_IND,		/* pppError_t		*/
	PPP_AUTH_SUCCESS,
	PPP_REMOTE_FAILURE,
	PPP_LOCAL_FAILURE
};


/*
 * PPP protocol fields currently defined
 */
typedef enum {
	pppDEVICE	   = 0x0000,	/* Device layer, not for transport */
	pppIP_PROTO	   = 0x0021,	/* Internet Protocol		*/
	pppOSI_PROTO	   = 0x0023,	/* OSI Network Layer		*/
	pppXNS_PROTO	   = 0x0025,	/* Xerox NS IDP			*/
	pppDECNET_PROTO	   = 0x0027,	/* DECnet phase IV		*/
	pppAPPLETALK_PROTO = 0x0029,	/* Appletalk			*/
	pppIPX_PROTO	   = 0x002b,	/* Novell IPX			*/
	pppVJ_COMP_TCP	   = 0x002d,	/* Van J Compressed TCP/IP	*/
	pppVJ_UNCOMP_TCP   = 0x002f,	/* Van J Uncompressed TCP/IP	*/
	pppBRIDGING_PDU	   = 0x0031,	/* Bridging PDU			*/
	pppSTREAM_PROTO	   = 0x0033,	/* Stream Protocol (ST-II)	*/
	pppBANYAN_VINES	   = 0x0035,	/* Banyan Vines			*/
	ppp802_1D	   = 0x0201,	/* 802.1d Hello Packets		*/
	pppLUXCOM	   = 0x0231,	/* Luxcom			*/
	pppSIGMA	   = 0x0232,	/* Sigma Network Systems	*/
	pppIP_NCP	   = 0x8021,	/* Internet Protocol NCP	*/
	pppOSI_NCP	   = 0x8023,	/* OSI Network Layer NCP	*/
	pppXNS_NCP	   = 0x8025,	/* Xerox NS IDP NCP		*/
	pppDECNET_NCP	   = 0x8027,	/* DECnet phase IV NCP		*/
	pppAPPLETALK_NCP   = 0x8029,	/* Appletalk NCP		*/
	pppIPX_NCP	   = 0x802b,	/* Novell IPX NCP		*/
	pppBRIDGING_NCP	   = 0x8031,	/* Bridging NCP			*/
	pppSTREAM_NCP	   = 0x8033,	/* Stream Protocol NCP		*/
	pppBANYAN_NCP	   = 0x8035,	/* Banyan Vines NCP		*/
	pppLCP		   = 0xc021,	/* Link Control Protocol	*/
	pppAuthPAP	   = 0xc023,	/* Password Authentication	*/
	pppLQM_REPORT	   = 0xc025,	/* Link Quality Report		*/
	pppCHAP		   = 0xc223	/* Challenge Handshake		*/

} pppProtocol_t;

/*
 * PPP link configuration structure; used by PPP_SET_CONF/PPP_GET_CONF
 */
typedef struct {
	ushort_t	pppLinkControlIndex;		/* 0 => this link */
	uchar_t		pppLinkMaxRestarts;
	uint_t		pppLinkRestartTimerValue;	/* millisecs */
	uint_t		pppLinkMediaType;		/* Async/Sync */

	uint_t		pppLinkAllowMRU;		/* MRU negotiation */
	uint_t		pppLinkAllowHdrComp;		/* IP Header comp */
	uint_t		pppLinkAllowPAComp;		/* Proto address comp */
	uint_t		pppLinkAllowACC;		/* Char mapping */
	uint_t		pppLinkAllowAddr;		/* IP address neg. */
	uint_t		pppLinkAllowAuth;		/* Authentication */
	uint_t		pppLinkAllowQual;		/* Link quality */
	uint_t		pppLinkAllowMagic;		/* Magic number */

	uint_t		pppLinkLocalMRU;		/* bytes */
	uint_t		pppLinkRemoteMRU;		/* bytes */
	uint_t		pppLinkLocalACCMap;		/* not used */
	ipaddr_t	pppIPLocalAddr;			/* for IP addr neg */
	ipaddr_t	pppIPRemoteAddr;
	uint_t		pppLinkMaxLoopCount;
#if defined(_LP64) || defined(_I32LPx)
	clock32_t	pppLinkMaxNoFlagTime;
#else
	clock_t		pppLinkMaxNoFlagTime;
#endif
} pppLinkControlEntry_t;

typedef enum {
	pppVer1,
	pppVer2
} pppVer_t;

#define	OLD_ALLOW	1
#define	OLD_DISALLOW	0

#define	REM_OPTS	(0x0000ffff)
#define	REM_DISALLOW	(1 << 1)
#define	REM_OPTIONAL	(1 << 2)
#define	REM_MAND	(1 << 3)

#define	LOC_OPTS	(0xffff0000)
#define	LOC_DISALLOW	(1 << 16)
#define	LOC_OPTIONAL	(1 << 17)
#define	LOC_MAND	(1 << 18)

#define	DISALLOW_BOTH	(REM_DISALLOW | LOC_DISALLOW)

/*
 * Maximum PAP Peer ID/Password Length supported [RFC1172 Page 29]
 */
#define	PPP_MAX_PASSWD	(255)
#define	PPP_MAX_ERROR 	(255)

/*
 * Message which indicates a "this-layer-up" action (protocol is up)
 */
typedef struct {
	uint_t			ppp_message;
	pppProtocol_t		protocol;
	uchar_t			data[1];
} pppProtoCh_t;

typedef struct {
	uint_t			ppp_message;	/* set to PPP_TL_UP */
	pppProtocol_t		protocol;
} pppProtoUp_t;


/*
 * Message which indicates a "this-layer-down" action (protocol is down)
 */
typedef struct {
	uint_t			ppp_message;	/* set to PPP_TL_DOWN */
	pppProtocol_t		protocol;
} pppProtoDown_t;

/*
 * Message which indicates a "this-layer-finish" action (protocol is finish)
 */
typedef struct {
	uint_t			ppp_message;	/* set to PPP_TL_FINISH */
	pppProtocol_t		protocol;
} pppProtoFinish_t;

/*
 * Message which indicates a "this-layer-start" action (starting protocol)
 */
typedef struct {
	uint_t			ppp_message;	/* set to PPP_TL_START */
	pppProtocol_t		protocol;
} pppProtoStart_t;

/*
 * Message which indicates a "this-layer-start" action (starting protocol)
 */
typedef struct {
	uint_t			message;	/* set to PPP_TL_START */
	pppProtocol_t		protocol;
} pppAuthMsg_t;

/*
 *  Message which indicates that the user is required to validate a PPP peer
 * using PPP_GET_REMOTE_PASSWD/PPP_REMOTE_OK/NOK
 */

typedef struct {
	uint_t			ppp_message;    /* set to PPP_NEED_VALIDATION */
} pppReqValidation_t;

/*
 * Message which indicates an error has occurred
 */
typedef struct {
	uint_t			ppp_message;	/* set to PPP_ERROR_IND */
	uint_t			code;
	uint_t			errlen;		/* optional error data */
	uchar_t			errdata[PPP_MAX_ERROR];
} pppError_t;

typedef struct {
	uint_t			ppp_message;
	pppLinkControlEntry_t	config;
} pppConfig_t;


/*
 * and a union of the PPP asynchronous indications
 */
union PPPmessages {
	uint_t			ppp_message;
	pppProtoUp_t		proto_up;
	pppProtoDown_t		proto_down;
	pppProtoFinish_t	proto_finish;
	pppProtoStart_t		proto_start;
	pppError_t		error_ind;
	pppAuthMsg_t		auth_msg;
	pppConfig_t		config;
};

/*
 * Error codes from PPP
 */
enum ppp_errors {
	pppConfigFailed,	/* Maximum number of configure reqs exceeded */
	pppNegotiateFailed,	/* Negotiation of mandatory options failed */
	pppAuthFailed,		/* Authentication Failed */
	pppProtoClosed,		/* Protocol closed */
	pppLocalAuthFailed,
	pppRemoteAuthFailed,
	pppLoopedBack
};

/*
 * PPP status and control information, derived from the
 * draft PPP MIB
 */

/*
 * enumeration to indicate mode in which options are used
 */
enum pppSense {
	pppReceiveOnly = 1,
	pppSendOnly,
	pppReceiveAndSend,
	pppNone
};

/*
 * enumeration to indicate PPP state
 */
enum pppState {
	pppInitial = 0,
	pppStarting,
	pppClosed,
	pppStopped,
	pppClosing,
	pppStopping,
	pppReqSent,
	pppAckRcvd,
	pppAckSent,
	pppOpened
};

/*
 * enumeration to indicate link quality estimate
 */
enum pppLinkQuality {
	pppGood = 1,
	pppBad
};

/*
 * enumeration to indicate size of CRC in use
 */
enum pppLinkCRCSize {
	pppCRC16 = 16,
	pppCRC32 = 32
};

/*
 * enumeration to indicate link media type
 */
enum pppLinkMediaType {
	pppSync,
	pppAsync
};

/*
 * enumeration to indicate PPP protocol version
 */
enum pppLinkVersions {
	pppRFC1171 = 1,
	pppRFC1331
};


/*
 * PPP Authentication Control; used by PPP_SET_AUTH/PPP_GET_AUTH
 */
typedef struct {
	ushort_t	pppAuthControlIndex;		/* 0 => this link */
	ushort_t	pppAuthTypeLocal;		/* 0 => none */
	ushort_t	pppAuthTypeRemote;		/* 0 => none */
} pppAuthControlEntry_t;

/*
 * Structure used to indicate protocol level for PPP_OPEN, PPP_CLOSE, PPP_UP,
 * PPP_DOWN ioclts
 */
typedef struct {
		pppProtocol_t	protocol;	/* Protocol to receive event */
} pppExEvent_t;

/*
 * Optional PAP message carried on Authenticate Ack/Nak
 */
typedef struct {
	uchar_t		pppPAPMessageLen;
	ushort_t	pppPAPMessage[1];
} pppPAPMessage_t;

/*
 * PPP status structure; read-only values
 */
typedef struct {
	ushort_t	pppLinkStatusIndex;
	uchar_t		pppLinkVersion;
	uchar_t		pppLinkCurrentState;
	uchar_t		pppLinkPreviousState;
	uchar_t		pppLinkQuality;
#ifndef _SunOS4
	timestruc_t	pppLinkChangeTime;
#endif
	uint_t		pppLinkMagicNumber;
	uint_t		pppLinkLocalQualityPeriod;	/* microseconds */
	uint_t		pppLinkRemoteQualityPeriod;	/* microseconds */
	uchar_t		pppLinkProtocolCompression;	/* pppSense */
	uchar_t		pppLinkACCompression;		/* pppSense */
	uchar_t		pppLinkMeasurementsValid;
	uchar_t		pppLinkPhysical;
} pppLinkStatusEntry_t;

/*
 * PPP error report structure; read-only values
 */
typedef struct {
	ushort_t	pppLinkErrorsIndex;
	ushort_t	pppLinkLastUnknownProtocol;
	uint_t		pppLinkBadAddresses;
	uint_t		pppLinkBadControls;
	ushort_t	pppLinkLastInvalidProtocol;
	uchar_t		pppLinkLastBadControl;
	uchar_t		pppLinkLastBadAddress;
	uint_t		pppLinkInvalidProtocols;
	uint_t		pppLinkUnknownProtocols;
	uint_t		pppLinkPacketTooLongs;
	uint_t		pppLinkPacketTooShorts;
	uint_t		pppLinkHeaderTooShorts;
	uint_t		pppLinkBadCRCs;
	uint_t		pppLinkConfigTimeouts;
	uint_t		pppLinkTerminateTimeouts;
} pppLinkErrorsEntry_t;

/*
 * PPP IP status; read-only values
 */
typedef struct {
	ushort_t	pppIPLinkNumber;
	uint_t		pppIPRejects;
	uint_t		pppIPInPackets;
	uint_t		pppIPInOctets;
	uint_t		pppIPOutPackets;
	uint_t		pppIPOutOctets;
	uint_t		pppIPInVJcomp;
	uint_t		pppIPInVJuncomp;
	uint_t		pppIPInIP;
	uint_t		pppIPOutVJcomp;
	uint_t		pppIPOutVJuncomp;
	uint_t		pppIPOutIP;
} pppIPEntry_t;

/*
 * PPP IP NCP/LCP status; read-only values
 */
typedef struct {
	ushort_t	pppCPLinkNumber;
	uint_t		pppCPRejects;
	uint_t		pppCPInPackets;
	uint_t		pppCPInOctets;
	uint_t		pppCPOutPackets;
	uint_t		pppCPOutOctets;
	uint_t		pppCPOutCRs;
	uint_t		pppCPInCRs;
	uint_t		pppCPOutCAs;
	uint_t		pppCPInCAs;
	uint_t		pppCPOutCNs;
	uint_t		pppCPInCNs;
	uint_t		pppCPOutCRejs;
	uint_t		pppCPInCRejs;
	uint_t		pppCPOutTRs;
	uint_t		pppCPInTRs;
	uint_t		pppCPOutTAs;
	uint_t		pppCPInTAs;
	uint_t		pppCPOutCodeRejs;
	uint_t		pppCPInCodeRejs;
	uint_t		pppCPOutEchoReqs;
	uint_t		pppCPInEchoReqs;
	uint_t		pppCPOutEchoReps;
	uint_t		pppCPInEchoReps;
	uint_t		pppCPOutDiscReqs;
	uint_t		pppCPInDiscReqs;
} pppCPEntry_t;

#define	CHAP_MAX_PASSWD 255
#define	CHAP_MAX_NAME	255

/*
 * CHAP password struct
 */
typedef struct {
	uint_t		protocol;	/* pppCHAP */
	uchar_t		chapPasswdLen;
	uchar_t		chapPasswd[CHAP_MAX_PASSWD];
	uchar_t		chapNameLen;
	uchar_t		chapName[CHAP_MAX_NAME];
}chapPasswdEntry_t;

#define	PAP_MAX_PASSWD	255

/*
 * PAP password struct
 */
typedef struct {
	uint_t		protocol;	/* pppPAPAuth */
	uchar_t		papPeerIdLen;
	uchar_t		papPeerId[PAP_MAX_PASSWD];
	uchar_t		papPasswdLen;
	uchar_t		papPasswd[PAP_MAX_PASSWD];
} papPasswdEntry_t;

typedef struct {
	papPasswdEntry_t passwd;
} papValidation_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PPP_IOCTL_H */
