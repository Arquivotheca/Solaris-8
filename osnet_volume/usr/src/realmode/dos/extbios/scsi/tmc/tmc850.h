/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */
 
#ident	"@(#)tmc850.h	1.5	94/05/23 SMI\n"

/*	TMC-850 Real Mode driver header file 
 *	Version 1.1
 */

/*	LIST OF SCSI MESSAGES ( sent/received during the MESSAGE phases )  */

#define	CMD_COMPLETE				0x00
#define	EXTENDED				0x01
#define	MSG_REJECT				0x07
#define 	IDENTIFY_MSG_WITH_NO_DISCONN	0x80
#define 	IDENTIFY_MSG_WITH_DISCONN		0xC0


/*	Various CDB groups	*/

#define	GROUP1_CDB_LEN	10
#define	GROUP0_CDB_LEN	06

/*	TMC REGISTER OFFSETS  */

#define	CONTROL_OFF		0x1C00
#define	STATUS_OFF		0x1C00
#define	READ_OFF		0x1E00
#define	WRITE_OFF		0x1E00


/*	CONTROL REGISTER BITS	*/

#define 	C_RESET		0x01
#define	C_SELECT		0x02
#define	C_BUSY			0x04
#define	C_ATTENTION		0x08
#define	C_ARBITRATION		0x10
#define	C_PARITY_ENABLE	0x20
#define	C_INT			0x40
#define	C_BUS_ENABLE		0x80

/*	STATUS REGISTER BITS 	*/

#define	I_BUSY			0x01
#define	I_MESSAGE		0x02
#define	I_IO			0x04
#define	I_CD			0x08
#define	I_REQUEST		0x10
#define	I_SELECT		0x20
#define	I_PARITY		0x40
#define	I_ARB_COMPLETE	0x80



/** LIST OF SCSI PHASES  **/

#define 	MSG_OUT_PH		(I_CD|I_MESSAGE)
#define	CMD_PH			(I_CD)
#define 	DATA_IN_PH		(I_IO)
#define	DATA_OUT_PH		0
#define	STATUS_PH		(I_CD|I_IO)
#define	MSG_IN_PH		(I_MESSAGE|I_CD|I_IO)

/***  OTHER RETURN VALUES ***/

#define 	TRUE			1
#define	FALSE			0
#define	SUCCESS		1
#define	BUSY			1
#define	FAILURE		-1
#define	NO_IDENTIFY		2
#define	ST_TIME_OUT		-1


/** MACROS **/

/* Macro TMC_WRITE writes a unchar value at the base:offset location */
/* Macro TMC_READ reads the unchar value at the base:offset location */
/* Macro TMC_STATUS_CHK returns TRUE/FALSE if the SCSI bus status anded
   with the mask matches the value passed */

#define  TMC_WRITE(base,off,val)  \
		 *((unchar far *)( (((ulong)(base)) << 16)+(ulong)(off)))=(val)

#define  TMC_READ(base,off)  \
		 *((unchar far *)((((ulong)(base)) << 16) + (ulong) (off)))

#define  TMC_STATUS_CHK(base,mask,val) \
		(((TMC_READ((base),STATUS_OFF) & (mask))==(val)) ? TRUE : FALSE)


/***** TMC PACKET STRUCTURE : for all command types ***/

typedef struct  {		
 	unchar tmc_target;	/* Command to be sent to this id */
	unchar tmc_lun;	/* Command to be sent to this lun */
	unchar cdb_len;	/* Indicates the length of the cdb */
	unchar pad;		/* Padding to align cdb */
	unchar cdb[12];	/* The actual cdb */
	ulong data_len;	/* The data length to be transferred */
	ushort data_segment; /* The data segment address */
	ushort data_offset;	/* Data offset where the data is returned */
}PKT_STRUCTURE;


/************************ END OF HEADER FILE DECL **************************/
