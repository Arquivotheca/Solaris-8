#pragma	ident	"@(#)@(#)hsfs_sig.h	1.4	1.4	96/03/11	SMI"

static char *hsfs_sig_tab[] = {
	SUSP_SP,
	SUSP_CE,
	SUSP_PD,
	SUSP_ST,
	SUSP_ER,
	RRIP_PX,
	RRIP_PN,
	RRIP_SL,
	RRIP_CL,
	RRIP_PL,
	RRIP_RE,
	RRIP_TF,
	RRIP_RR,
	RRIP_NM
};

static int hsfs_num_sig = sizeof (hsfs_sig_tab) / sizeof (hsfs_sig_tab[0]);

#define	SUSP_SP_IX	0
#define	SUSP_CE_IX	1
#define	SUSP_PD_IX	2
#define	SUSP_ST_IX	3
#define	SUSP_ER_IX	4

#define	RRIP_PX_IX	5
#define	RRIP_PN_IX	6
#define	RRIP_SL_IX	7
#define	RRIP_CL_IX	8
#define	RRIP_PL_IX	9
#define	RRIP_RE_IX	10
#define	RRIP_RF_IX	11
#define	RRIP_RR_IX	12
#define	RRIP_NM_IX	13
