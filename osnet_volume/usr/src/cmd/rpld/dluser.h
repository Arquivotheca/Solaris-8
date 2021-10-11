/*
 * definitions for use with DL library
 */

#pragma ident "@(#)dluser.h 1.2	94/03/21 SMI"

#define DLSTATE_UNBOUND 	0
#define DLSTATE_UNATTACHED	1
#define DLSTATE_IDLE    	3

#define DLTYPE_IEEE8023		0
#define DLTYPE_IEEE8024		1
#define DLTYPE_IEEE8025		2
#define DLTYPE_IEEE8026		3
#define DLTYPE_ETHER		4
#define DLTYPE_HDLC		5
#define DLTYPE_CHARSYNC		6
#define DLTYPE_IBMCHAN		7
#define DLTYPE_ETHEROR	     0xF0

#define DLCLASS_CODLS		1
#define DLCLASS_CLDLS		2

#define DLBADSAP		0
#define DLACCESS		2
#define DLOUTSTATE		3
#define DLSYSERR		4
/* defined by API */
#define DLBOUND		   	8 /* already bound */
#define DLBADPRIM		9
#define DLUNBOUND	       10
#define DLNOTSUPP	       11

#define	MAXPRIMSZ		100

/* address structure constructed with dl_mkaddr */
struct dl_address {
   int	          dla_dlen;
   int		  dla_dmax;
   unsigned char *dla_daddr;
   int		  dla_dflag;	/* multicast/broadcast flag */
   int	          dla_slen;
   int		  dla_smax;
   unsigned char *dla_saddr;
   int		  dla_sflag;	/* multicast/broadcast flag */
};

/* dl_alloc field definitions */
#define DL_ALL	0xf		/* both addresses */
#define DL_SRC  0x1		/* source address field */
#define DL_DST  0x2		/* destination address field */

/* Driver styles */
#define DL_STYLE_1	1
#define DL_STYLE_2	2

/* used both internally and externally */
typedef struct dl_information {
   unsigned long	version;
   unsigned long	state;
   unsigned long	style;
   unsigned long	max_lsdu;
   unsigned long	min_lsdu;
   unsigned long	addr_len;
   unsigned long	sap_len;
   unsigned long	mac_type;
   unsigned long	class;
   struct dl_address   *address;
   struct dl_address   *broadcast;
} dl_info_t;

/*
 * flags returned by dl_rcv
 */
#define DL_FLAG_MULTICAST	0x01 /* includes broadcast */
