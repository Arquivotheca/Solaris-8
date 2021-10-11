/*
 * Copyright (c) 1996, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)chs_viper.h	1.3	99/03/16 SMI"

/* Viper HIST register bits */
#define	VIPER_HIST_SCE		0x01
#define	VIPER_HIST_SQO		0x02
#define	VIPER_HIST_GHI 		0x04
#define	VIPER_HIST_EI		0x80

#define	VIPER_CCCR_SS		0x0002
#define	VIPER_CCCR_SEMBIT	0x0008
#define	VIPER_CCCR_ILE		0x10

#define	VIPER_CMD_BYTECNT	0x10
#define	VIPER_RESET_ADAPTER	0x80

#define	VIPER_OP_PENDING	0x01
#define	VIPER_ENABLE_BUS_MASTER 0x02

#define	VIPER_STATUS_MASK	0xff0f
#define	VIPER_STATUS_GSC_MASK	0x000f
#define	VIPER_STATUS_ESB_MASK	0xff

#define	CHS_V_GSC_SUCCESS	0x0
#define	CHS_V_GSC_RECOVERED	0x01
#define	CHS_V_GSC_CHECK_COND	0x02
#define	CHS_V_GSC_INVAL_OPCODE	0x03
#define	CHS_V_GSC_INVAL_CMD_BLK	0x04
#define	CHS_V_GSC_INVAL_PARAM_BLK	0x05
#define	CHS_V_GSC_UNDEF1	0x06
#define	CHS_V_GSC_UNDEF2	0x07
#define	CHS_V_GSC_BUSY		0x08
#define	CHS_V_GSC_HW_ERR	0x09
#define	CHS_V_GSC_FW_ERR	0x0A
#define	CHS_V_GSC_JUMPER_SET	0x0B
#define	CHS_V_GSC_CMPLT_WERR	0x0C
#define	CHS_V_GSC_LOG_DRV_ERR	0x0D
#define	CHS_V_GSC_CMD_TIMEOUT	0x0E
#define	CHS_V_GSC_PHYS_DEV_ERR	0x0F

#define	CHS_V_ESB_NOSTAT	0

#define	CHS_V_ESB_NORBLD	0x30

#define	CHS_V_ESB_SEL_TIMEOUT	0xf0	/* Selection timeout */
#define	CHS_V_ESB_UNX_BUS_FREE	0xf1	/* Unexpected bus free */
#define	CHS_V_ESB_DATA_RUN		0xf2	/* Data Over or Under run */
#define	CHS_V_ESB_SCSI_PHASE_ERR	0xf3	/* SCSI Phase error */

#define	CHS_V_ESB_CMD_P_ABORTED	0xf4	/* Command aborted by */
					/* PDM_KILL_BUCKET    */

#define	CHS_V_ESB_CMD_ABORTED	0xf5	/* Command aborted by scsi */
					/* controller		   */

#define	CHS_V_ESB_FAILED_ABORT	0xf6	/* Abort failed due to SCSI */
					/* bus reset		    */

#define	CHS_V_ESB_BUS_RESET	0xf7	/* SCSI bus reset by adapter */

#define	CHS_V_ESB_BUS_RESET_OTHER 0xf8	/* SCSI bus reset by other device */

#define	CHS_V_ESB_ARQ_FAILED	0xf9	/* ARQ attempt failed */

#define	CHS_V_ESB_MSG_REJECTED	0xfa	/* Device rejected a SCSI message */

#define	CHS_V_ESB_PARITY_ERR	0xfb	/* SCSI parity error detected */

#define	CHS_V_ESB_RECOVERED	0xfc	/* SCSI bus reset by adapter */

#define	CHS_V_ESB_TGT_NORESPOND	0xfd	/* Target device not responding */

#define	CHS_V_ESB_CHN_UNFUNC	0xfe	/* SCSI channel(s) not functioning */

#define	CHS_V_ESB_CHECK_CONDITION 0xff	/* Check condition received */




#define	CHS_V_RAID_MASK	0x07;

#define	VIPER_CMDBLK_BYTE16	0x0;
#define	VIPER_CMDBLK_BYTE17	0x0;
#define	VIPER_CMDBLK_BYTE18	0x0;
#define	VIPER_CMDBLK_BYTE19	0x0;
#define	VIPER_CMDBLK_BYTE20	0x10;
#define	VIPER_CMDBLK_BYTE21	0x0;
#define	VIPER_CMDBLK_BYTE22	0x0;
#define	VIPER_CMDBLK_BYTE23	0x0;


#pragma pack(1)
typedef struct {
	uchar_t fill1;
	uchar_t stat_id;
	uchar_t bsb;
	uchar_t esb;
} viper_statusq_element;
#pragma pack()



extern char *viper_gsc_errmsg[];
