/*
 * Copyright (c) 1996 Sun Microsystems, Inc. All rights reserved.
 */
 
#ident	"@(#)tmc850.c	1.9	96/07/16 SMI\n"

/*
 *  VIEW OPTIONS : Set tabstop=7
 *
 *  NAME :	tmc850.c Ver. 1.1
 *  
 *  SYNOPSIS:  Real Mode driver code for TMC-850 SCSI controller from Future
 *   		 Domain Inc. written for Solaris 2.1 for x86
 *
 *
 *  FRAMEWORK entry points :
 *  i.	  	dev_find()
 *  ii.  	dev_init() 
 *  iii. 	dev_sense()
 *  iv.  	dev_inquire()
 *  v.	  	dev_readcap()
 *  vi.  	dev_lock()
 *  vii. 	dev_motor()
 *  viii.	dev_read()
 *  ix.  	init_dev()
 *  
 *
 *  SUMMARY :	The TMC-850 Real mode driver is a SCSI HBA driver for Solaris
 *		2.1 for x86, which provides read services to read data from
 *		a SCSI CD-ROM in order to install/boot Solaris for x86 from
 *		the CD-ROM connected to this controller.
 *		
 *		Booting of Solaris for x86 from a SCSI disk involves the use
 *		of the IBM BIOS INT 13H interface. Typical ROM BIOS INT 13H
 *		interface does not support booting from CD-ROM. The Real mode
 *		driver provides an extended INT 13H interface to read data
 *		from CD-ROM, to install/boot Solaris.
 *
 *
 *  NOTES : 	The init_dev entry point is called from the Framework when
 *		the driver has been relocated to the high memory to enable the
 *		driver to recalculate any addresses that were valid prior to
 *		the relocation. Since the tmc-850 real mode driver has no
 *		relocatable addresses, this entry point is implemented as
 *		a stub.
 *		Also the driver only checks for LUN 0 only for each target.
 *		Changing the LAST_LUN macro in the scsi.h header file will
 *		enable checking for other LUNs if required.
 *
 *
 *  MODIFICATION HISTORY :
 *		* 
 *		* First Release : Version 1.0 on 09/17/93.
 *		*
 *		* Second Release : Version 1.1
 *		*		   The DATA-IN-PHASE was speeded up
 *		*		   by implementing a scsibcopy routine which
 *		*		   does a repeated move of bytes from the 
 *		*		   controller's read register to the Host
 *		*		   Memory, with the REQ-ACK handshake performed
 *		*		   only for the first two bytes and the last byte
 *		*		   of a block (512 bytes).
 *		*		   Also the HBA ID was set to 7.
 */

/*
 *  Developed by : WIPRO INFOTECH LTD.
 *		     88, M.G. Road,
 *		     Bangalore - 560 001
 *		     India.
 *		
 *  For Acceptance by Sunsoft 
 */

/**  INCLUDE HEADERS  **/

#include <types.h>
#include <bef.h>
#include <common.h>
#include "..\scsi.h"
#include <dev_info.h>
#include "tmc850.h"
#include <befext.h>

/** GLOBAL VARIABLES USED BY THE DRIVER FRAMEWORK **/

char ident[]="tmc";
#define STACKSIZE 1000
ushort stack[STACKSIZE] = { 0 };
ushort stacksize = STACKSIZE;


#define ARBITRATE_TIMEOUT	0x05		/* For selection timeout delay */
#define TMC_ID		0x07		/* SCSI ID of the TMC Controller */
#define NUM_HBA_ADDR		0x06


/** FUNCTION PROTOTYPES **/

short start_arb_sel(PKT_STRUCTURE *, ushort);
unchar tmc_ph_handler(PKT_STRUCTURE *, ushort);

/* Entry Points  */

int dev_find(void);

int dev_sense(ushort, ushort, ushort);
int dev_inquire(ushort, ushort, ushort);

/*** EXTERNAL ROUTINE ***/

extern void scsibcopy(ushort,ushort,ushort,ushort,ushort);

/*** GLOBAL DATA CONTAINERS  ***/

extern struct readcap_data readcap_data; 	/* defined in scsi.h */
extern struct inquiry_data inqd; 		/* defined in scsi.h */

/*** GLOBAL DATA STRUCTURES ***/

struct exsense_data sense_info={0, 0, 0, 0, 0, 0, 0, 0, 0}; 
int num_targets_identified=0;

/*  Valid TMC base addresses */
ushort base_addr[NUM_HBA_ADDR] = { 0xCA00, 0xC800, 0xDE00, 0xCE00, 0xE800, 
					0xEC00 };



/*************************** Module Entry Points ****************************/									
/*
 * Name		: dev_find
 * Purpose		: Identifies the Controllers, targets & the LUNs
 * Called by	 	: Driver frame work
 * Arguments	 	: None
 * Return Value	: The number of HBAs detected.
 * Calls		: scsi_dev() a driver framework routine 
 */

/*
 * This function attempts to find the all HBAs(TMC-850) supported by this 
 * module and for each HBA found, determines the Target and Logical Unit 
 * Number of each device connected to the HBA.
 */

int 
dev_find(void)
{
	int	nbrd = 0;
	ushort	Idx,
		base;


#ifdef DEBUG
	putstr("Inside dev_find .. \n\r");
#endif

	for (Idx = 0; Idx <NUM_HBA_ADDR; Idx++) {
		base = base_addr[Idx];
		if (common_probe(base))
			nbrd++;
	}
        return(nbrd);  /* return the no. of HBAs detected */
}

legacyprobe()
{
	DWORD val[6], len;
	ushort hbas, base;
	
	for (hbas = 0; hbas < NUM_HBA_ADDR; hbas++) {
		if (node_op(NODE_START) != NODE_OK)
		  return (BEF_FAIL);

		base = base_addr[hbas];
		val[0] = (DWORD)base << 16 + CONTROL_OFF;
		val[1] = 1;
		val[2] = 0;
		val[3] = (DWORD)base << 16 + READ_OFF;
		val[4] = 1;
		val[5] = 0;
		len = 6;
		if (set_res("mem", val, &len, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}

		if (!common_probe(base)) {
			node_op(NODE_FREE);
			continue;
		}

		node_op(NODE_DONE);
	}
	return (BEF_OK);
}

installonly()
{
	DWORD val[6], len;
	ushort base, Rtn;

	Rtn = BEF_FAIL;
	do {
		if (node_op(NODE_START) != NODE_OK)
		  return Rtn;

		len = 6;
		if (get_res("mem", val, &len) != RES_OK)
		  return BEF_FAIL;

		base = val[0] >> 16;
		if (common_probe(base))
		  Rtn = BEF_OK;
	} while (1);
}

/*
 * common_probe -- probe for controller at addr and add drives to bootdevs
 *	returns 1 on success and 0 on failure.
 */
common_probe(ushort base)
{
	ushort targets, luns;
	
	TMC_WRITE(base,CONTROL_OFF,C_SELECT|C_BUSY);
	if(TMC_READ(base,STATUS_OFF) ==(I_BUSY|I_SELECT)) {

		TMC_WRITE(base,CONTROL_OFF,0x01);
		TMC_WRITE(base,CONTROL_OFF,0x00);
		for(targets=0;targets<8;targets++) {
			if(targets==TMC_ID) continue;
			for(luns=0;luns<LAST_LUN;luns++)
			  scsi_dev(base,targets,luns);
		}
		return (1);
	}
	else
	  return (0);
}

/*
 * Name		: dev_init
 * Purpose		: Initialize (Reset) the SCSI bus.
 * Called From	: Driver frame work.
 * Arguments		: struct bdev_info *info
 * Returns		: 0 always
 * Calls		: macro TMC_WRITE
 */

/*
 * The dev_init entry point resets the HBA whose base_port address is in the 
 * dev_info structure. This function always succeeds.
 */

int
dev_init(DEV_INFO *info)
{

#ifdef DEBUG
	putstr("Inside Dev_init\n\r");
#endif

	TMC_WRITE(info->base_port,CONTROL_OFF,0x01);
	TMC_WRITE(info->base_port,CONTROL_OFF,0x00);
	return(0);
}




/*
 * Name		: dev_sense
 * Purpose		: Issue a SCSI request sense command to a target
 * Called From 	: Drive frame work
 * Arguments		: ushort base_port, ushort targetid, ushort lun
 * Returns		: 0 on success/ -1 on failure,chk_condition or timeout
 * Calls		: tmc_ph_handler()
 */

/*
 * This entry point issues a SCSI Request Sense command to the logical unit of
 * a target whose id  and logical Unit number are provided as arguments. The 
 * function returns -1 on a failure or the least significant four bits of the
 * Sense key field of the extended sense data structure (declared globally &
 * defined in scsi.h) on success.
 */

int 
dev_sense(ushort base_port,ushort targetid,ushort lun)
{
	unchar stat;
	PKT_STRUCTURE tmc_pkt;


#ifdef DEBUG
	putstr("inside dev_sense\n\r");
#endif
	tmc_pkt.tmc_target=(unchar)targetid;
	tmc_pkt.tmc_lun=(unchar)lun;
	tmc_pkt.cdb_len=GROUP0_CDB_LEN;
	tmc_pkt.cdb[0]=SC_RSENSE;
	tmc_pkt.cdb[1]=(unchar)(lun <<5)&0x1F;
	tmc_pkt.cdb[2]=0x00;
	tmc_pkt.cdb[3]=0x00;
	tmc_pkt.cdb[4]=sizeof(struct exsense_data);
	tmc_pkt.cdb[5]=0x00;
	tmc_pkt.data_len=(ulong) sizeof(struct exsense_data);
	tmc_pkt.data_segment=myds();
	tmc_pkt.data_offset=(ushort)&sense_info;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif

	stat=tmc_ph_handler(&tmc_pkt,base_port);

#ifdef DEBUG
	printstatus(&tmc_pkt,stat);
#endif

	if((stat == S_CK_COND) || (stat == S_BUSY) ||
	   (stat == (unchar)ST_TIME_OUT))
		return(-1);
	else
		return(sense_info.exsd_key&0x0f);
}




/*
 * Name		: dev_inquire
 * Purpose		: Issues a SCSI Inquiry command to a target
 * Called From	: Driver frame work (scsi_dev SCSI utility routine).
 * Arguments		: ushort base_port, ushort targetid, ushort lun
 * Returns		: 0 on success/1 on failure,chk_condition/ -1 on timeout
 * Calls		: tmc_ph_handler(),myds()
 */

/*
 * The dev_inquire entry point attempts to issue a SCSI Inquiry command to the
 * logical Unit of the target passed as arguments. This function is called by
 * the scsi_dev routine which records the information about each of the logical
 * units of all targets and HBAs identified in a table maintained by the driver 
 * development framework. The function returns 0 on success or 1 on failure/
 * chk_condition or -1 on timeout.
 */

int 
dev_inquire(ushort base_port,ushort targetid,ushort lun)
{

	unchar stat;
	PKT_STRUCTURE tmc_pkt;
	ushort inquiry_len=sizeof(struct inquiry_data);


#ifdef DEBUG
	putstr("Inside Dev _inquire\n\r");
#endif
	tmc_pkt.tmc_target=(unchar)targetid;
	tmc_pkt.tmc_lun=(unchar)lun;
	tmc_pkt.cdb_len=GROUP0_CDB_LEN;
	tmc_pkt.cdb[0]=SC_INQUIRY;
	tmc_pkt.cdb[1]=(unchar)(lun <<5)&0x1F;
	tmc_pkt.cdb[2]=0x00;
	tmc_pkt.cdb[3]=0x00;
	tmc_pkt.cdb[4]=inquiry_len;
	tmc_pkt.cdb[5]=0x00;
	tmc_pkt.data_len=(ulong) inquiry_len;
	tmc_pkt.data_segment=myds();
	tmc_pkt.data_offset=(ushort)&inqd;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif

	stat=tmc_ph_handler(&tmc_pkt,base_port);

#ifdef DEBUG
	printstatus(stat);
#endif

	if( (stat == S_CK_COND) || ( stat == S_BUSY))
		return(1);
	else if( stat == (unchar)ST_TIME_OUT )
		return(-1);
	else
	{
		num_targets_identified++;
		return(0);
	}
}




/*
 * Name		: dev_readcap
 * Purpose		: Issue a SCSI read capacity command to a target
 * Called From	: Driver frame work
 * Arguments		: struct bdev_info *info
 * Returns		: 0 on success/1 on failure(chk_condition)/ -1 on timeout
 * Calls		: tmc_ph_handler(),myds()
 */

/*
 * The dev_readcap entry point attempts to determine the storage capacity of the
 * logical unit of the target whose ID/LUN is in the DEV_INFO structure, by
 * issuing a SCSI Read capacity command to it. The capacity and the block size 
 * of the logical unit returned by the target are put into a global readcap_data
 * structure. The function returns 0 on success, 1 on failure/chk_condition or
 * a -1 on timeout.
 */

int 
dev_readcap(DEV_INFO *info)
{

	unchar stat;
	ushort t_base_port=info->base_port;
	PKT_STRUCTURE tmc_pkt;

	tmc_pkt.tmc_target=info->MDBdev.scsi.targ;
	tmc_pkt.tmc_lun=info->MDBdev.scsi.lun;
	tmc_pkt.cdb_len=GROUP1_CDB_LEN;
	tmc_pkt.cdb[0]=SX_READCAP;
	tmc_pkt.cdb[1]=(info->MDBdev.scsi.lun <<5)&0x1F;
	tmc_pkt.cdb[2]=0x00;
	tmc_pkt.cdb[3]=0x00;
	tmc_pkt.cdb[4]=0x00;
	tmc_pkt.cdb[5]=0x00;
	tmc_pkt.data_len=(ulong) sizeof(struct readcap_data);
	tmc_pkt.data_segment=myds();
	tmc_pkt.data_offset=(ushort)&readcap_data;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif

	stat=tmc_ph_handler(&tmc_pkt,t_base_port);

#ifdef DEBUG
	printstatus(stat);
#endif
	if( (stat == S_CK_COND) || ( stat == S_BUSY))
		return(1);
	else if( stat == (unchar)ST_TIME_OUT )
		return(-1);
	else
		return(0);
}




/*
 * Name		: dev_motor
 * Purpose		: Issue a SCSI start/stop motor command
 * Called From	: Driver frame work
 * Arguments		: struct bdev_info *info, int start
 * Returns		: 0 on success/1 on failure/chk_condition
 * Calls		: tmc_ph_handler()
 */

/*
 * This entry point attempts to issue a SCSI Start/stop motor command to the 
 * target which sets the device motor to full speed/stop condition, following
 * which regular reads( or writes if supported) can be performed. The start/
 * stop function is indicated by the start argument, a one value indicating a 
 * start and a zero value indicating a stop of the motor. The function returns 
 * 0 on success or 1 on failure/chk_condition/timeout.
 */

int 
dev_motor(DEV_INFO *info,int start)
{
	unchar stat;
	PKT_STRUCTURE tmc_pkt;


#ifdef DEBUG
	putstr("Inside dev_motor\n\r");
#endif
	tmc_pkt.tmc_target=info->MDBdev.scsi.targ;
	tmc_pkt.tmc_lun=info->MDBdev.scsi.lun;
	tmc_pkt.cdb_len=GROUP0_CDB_LEN;
	tmc_pkt.cdb[0]=SC_STRT_STOP;
	tmc_pkt.cdb[1]=(info->MDBdev.scsi.lun <<5)&0x1F;
	tmc_pkt.cdb[2]=0x00;
	tmc_pkt.cdb[3]=0x00;
	if(start)
		tmc_pkt.cdb[4]=(unchar)0x01;
	else
		tmc_pkt.cdb[4]=(unchar)0x00;
	tmc_pkt.cdb[5]=0x00;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif

	stat=tmc_ph_handler(&tmc_pkt,info->base_port);

#ifdef DEBUG
	printstatus(stat);
#endif
	if((stat == S_CK_COND) || ( stat == S_BUSY) ||
		(stat == (unchar)ST_TIME_OUT))
		return(1);
	else
		return(0);
}




/*
 * Name		: dev_lock
 * Purpose		: Issue a SCSI Prevent removal of medium command
 * Called From	: Driver frame work
 * Arguments		: struct bdev_info *info, int lock
 * Returns		: 0 on success/1 on failure/chk_condition
 * Calls		: tmc_ph_handler()
 */

/*
 * This function attempts to issue a SCSI Prevent Removal of Medium command to
 * the target whose ID is in the bdev_info structure, which locks the medium
 * preventing its removal during its use (Reads,Writes etc). The lock argument,
 * if one indicates a lock operation and if zero indicates a unlock operation.
 * The function returns 0 on success or 1 on failure/chk_condition/timeout.
 */

int
dev_lock(DEV_INFO *info,int lock)
{
	unchar stat;
	PKT_STRUCTURE tmc_pkt;


#ifdef DEBUG
	putstr("Inside dev_lock\n\r");
#endif
	tmc_pkt.tmc_target=info->MDBdev.scsi.targ;
	tmc_pkt.tmc_lun=info->MDBdev.scsi.lun;
	tmc_pkt.cdb_len=GROUP0_CDB_LEN;
	tmc_pkt.cdb[0]=SC_REMOV;
	tmc_pkt.cdb[1]=(info->MDBdev.scsi.lun <<5)&0x1F;
	tmc_pkt.cdb[2]=0x00;
	tmc_pkt.cdb[3]=0x00;
	tmc_pkt.cdb[4]=(unchar)lock;
	tmc_pkt.cdb[5]=0x00;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif
	stat=tmc_ph_handler(&tmc_pkt,info->base_port);
#ifdef DEBUG
	printstatus(&tmc_pkt,stat);
#endif
	if( (stat == S_CK_COND) || ( stat == S_BUSY) ||
		(stat == (unchar)ST_TIME_OUT ))
		return(1);
	else
		return(0);
}




/*
 * Name		: dev_read
 * Purpose		: Issue a extended SCSI read command to a target
 * Called From	: Driver frame work
 * Arguments		: struct bdev_info *info, long start_block, ushort count
 *		  	  ushort buf_seg, ushort buf_offset
 * Returns		: 0 on success/1 on failure/chk_condition
 * Calls		: tmc_ph_handler()
 */

/*
 * This function attempts to issue a SCSI Extended Read (Group 1 Command) to 
 * the logical unit of the target  whose ID/LUN are provided in the dev_info 
 * structure and attempts to read in 'count' blocks starting at the start_block
 * location, into the memory area whose segment and offset values are provided 
 * as arguments to the function.  The function returns 0 on success or 1 on
 * failure/chk_condition/timeout.
 */

int
dev_read(DEV_INFO *info,long start_block,ushort count,
	     ushort buf_offset,ushort buf_seg)
{
	unchar stat;
	union halves st_blk;
	PKT_STRUCTURE tmc_pkt;


	st_blk.l=start_block;

	tmc_pkt.tmc_target=info->MDBdev.scsi.targ;
	tmc_pkt.tmc_lun=info->MDBdev.scsi.lun;
	tmc_pkt.cdb_len=GROUP1_CDB_LEN;
	tmc_pkt.cdb[0]=SX_READ;
	tmc_pkt.cdb[1]=(info->MDBdev.scsi.lun <<5)&0x1F;
	tmc_pkt.cdb[2]=st_blk.c[3];
	tmc_pkt.cdb[3]=st_blk.c[2];
	tmc_pkt.cdb[4]=st_blk.c[1];
	tmc_pkt.cdb[5]=st_blk.c[0];
	tmc_pkt.cdb[6]=0x00;
	st_blk.s[0]=count;
	tmc_pkt.cdb[7]=st_blk.c[1];
	tmc_pkt.cdb[8]=st_blk.c[0];
	tmc_pkt.cdb[9]=0x00;
	tmc_pkt.data_len=(ulong) (count * info->MDBdev.scsi.bsize);
	tmc_pkt.data_segment=buf_seg;
	tmc_pkt.data_offset=buf_offset;

#ifdef DEBUG
	printpkt(&tmc_pkt);
#endif
	stat=tmc_ph_handler(&tmc_pkt,info->base_port);
#ifdef DEBUG
	printstatus(&tmc_pkt,stat);
#endif
	if( (stat == S_CK_COND) || ( stat == S_BUSY)||
		(stat == (unchar)ST_TIME_OUT ))
		return(1);
	else
		return(0);
}



/*
 * Name		: init_dev
 * Purpose		: Recalculate relocatable addresses 
 * Called From	: Driver frame work
 * Arguments		: ushort base_port, ushort newds
 * Returns		: void
 * Calls		: None
 */

/*
 * Since our driver has no relocatable addresses, this routine is implemented
 * as a stub.
 */

init_dev(ushort a, ushort b)
{

#ifdef DEBUG
	putstr("\n\rInit_dev called\n\r");
#endif
	
}



/********************* Utility Routines ************************************/

/*
 * Name 		: start_arb_sel
 * Purpose		: Acquire the bus for the controller & select the target
 * Called From	: tmc_ph_handler
 * Arguments		: PKT_STRUCTURE *tmc_pkt, ushort base_port
 * Returns		: SUCCESS/FAILURE/NO_IDENTIFY
 * Calls		: macros TMC_STATUS_CHK,TMC_WRITE
 */

/*
 * start_arb_sel handles the SCSI Arbitration phase , trying to acquire the SCSI
 * bus for the HBA. It starts the bus arbitration on detecting a Bus Free Phase
 * and tries to select the desired target for the HBA. It returns Success if
 * the desired target is selected. It returns a failure if the arbitration 
 * fails or the target was not selected or a No identify if the target was not
 * found.
 */

short 
start_arb_sel(PKT_STRUCTURE *tmc_pkt, ushort base_port)
{
       ulong arbit_delay,timeout_delay;

	if((TMC_STATUS_CHK(base_port,I_BUSY,I_BUSY)))
		return(FAILURE);
	if((TMC_STATUS_CHK(base_port,I_SELECT,I_SELECT)))
		return(FAILURE);

/* 	Bus Free Phase detected (BSY & SEL negated for Bus settle delay) */

	TMC_WRITE(base_port,WRITE_OFF,(unchar)(0x01<<TMC_ID));
	TMC_WRITE(base_port,CONTROL_OFF,
		  C_BUS_ENABLE|C_ARBITRATION|C_PARITY_ENABLE);

/* 	Wait for an arbitration delay */

	arbit_delay=0xfff;
	while(!(TMC_STATUS_CHK(base_port,I_ARB_COMPLETE,I_ARB_COMPLETE)) 
		&& arbit_delay) arbit_delay--;
	if(!(TMC_STATUS_CHK(base_port,I_ARB_COMPLETE,I_ARB_COMPLETE))) 
	{	
		TMC_WRITE(base_port,CONTROL_OFF,C_BUS_ENABLE|C_PARITY_ENABLE);
		return(FAILURE);
	}


/* 	Wait for a bus clear delay  + Bus settle delay */

	milliseconds(2);
	TMC_WRITE(base_port,WRITE_OFF,(unchar)((0x01<<TMC_ID)
			|(0x01<<tmc_pkt->tmc_target)));


/*	Clear BSY after 2*deskew delays */

	TMC_WRITE(base_port,CONTROL_OFF,
		  C_BUS_ENABLE|C_PARITY_ENABLE|C_SELECT|C_ATTENTION);

/* 	Wait for a selection TIMEOUT delay */

        timeout_delay=0xffff;
        while((TMC_STATUS_CHK(base_port,I_BUSY,0)) && timeout_delay) 
		timeout_delay--;

/*	If target does not assert BUSY within the selection timeout delay
 *	perform the timeout procedure and release bus  	*/

	if(!timeout_delay) 
	{

       	TMC_WRITE(base_port,CONTROL_OFF,
			   C_PARITY_ENABLE|C_SELECT|C_ATTENTION);
              if((TMC_STATUS_CHK(base_port,I_BUSY,0)))
		{

/* 	After selection abort time + 2*deskew delays clear SEL */

        	    TMC_WRITE(base_port,CONTROL_OFF,C_PARITY_ENABLE); 

/* 	If BUS FREE Phase, stop driving signals on bus by clearing C_BUS_ENABLE
 *	within BUS FREE delay */

		    TMC_WRITE(base_port,CONTROL_OFF,C_PARITY_ENABLE);
#ifdef DEBUG
		    putstr("Target not identified ...\n\r");
#endif
		    return(NO_IDENTIFY);
		}

	}    /* End of selection timeout block */

/*	Target successfully selected. Return Success */

	TMC_WRITE(base_port,CONTROL_OFF,
		  C_BUS_ENABLE|C_PARITY_ENABLE|C_ATTENTION);
	return(SUCCESS);
}





/*
 * Name		: tmc_ph_handler
 * Purpose		: Handles the SCSI phases for the HBA
 * Called From 	: dev_sense,dev_find,dev_inquire,dev_motor,dev_readcap
 *		  	  dev_read,dev_lock.
 * Arguments		: PKT_STRUCTURE *tmc_pkt, ushort base_port
 * Returns		: S_BUSY/S_CK_COND/S_GOOD/ST_TIME_OUT
 * Calls		: macros TMC_READ,TMC_WRITE,TMC_STATUS_CHK,
		  	  start_arb_sel(),scsibcopy
 */
/*
 * The tmc_ph_handler routine handles the various information transfer phases 
 * of the SCSI protocol to execute the requested commands at the target. This
 * function does not support interrupts as the disconnect feature of SCSI is 
 * not exploited because multithreading of commands under real mode is not 
 * possible and also int 13H waits until the request is completely satisfied.
 * The HBA  operates in a polled mode waiting for the target to send the 
 * information across.The phase handler returns a Good status on successful 
 * execution of the command by the target, a Check_condition status if 
 * error(s) were detected during execution, a Busy status if bus was busy 
 * during execution or a timeout status ifa time out occurred during selection 
 * or arbitration. The DATA-IN phase of the phase handler uses a burst mode
 * transfer for data transfers more than 512 bytes, performing a REQ-ACK hand-
 * shake for the first 2 bytes and the last byte alone and transferring the
 * other bytes without handshake as it is done by the hardware.
 */

unchar 
tmc_ph_handler( PKT_STRUCTURE *t_tmc_pkt,ushort base_port)
{
	register unchar val_read_in;
	register ulong buf_resid=0;
	register ushort cmd_len; 
	unchar msg_rcvd=0x00;
	unchar ret_val=0;
	int i;
	unchar arb_sel_status = FAILURE;
	unchar statvalue;
	ulong arbit_timeout;
	register char *cdb_ptr=(char *)&(t_tmc_pkt->cdb[0]); 

	
/*	Return if BUSY is set before the arbitration_selection phase is
 *	entered, indicating that a Bus Free Phase was not detected     */

	if((TMC_STATUS_CHK(base_port,I_BUSY,I_BUSY)))
	{
#ifdef DEBUG
		putstr("Returned because BUSY was set\n\r");
#endif
		ret_val = S_BUSY ;
		return(ret_val);
	}

/*	Start arbitration and selection procedure    */

       arbit_timeout=ARBITRATE_TIMEOUT;   
	while(arb_sel_status!=SUCCESS && arbit_timeout--)
	{
   		arb_sel_status=start_arb_sel(t_tmc_pkt,base_port);
		if(arb_sel_status==SUCCESS) break;
		else if(arb_sel_status==NO_IDENTIFY)
		{
			return (unchar)ST_TIME_OUT;
		}
	}
	if(!arbit_timeout) 
	{

#ifdef DEBUG
			putstr("Arb/Sel failure \n\r");
#endif

			return (unchar)ST_TIME_OUT; 
	}

/*	Initialize the No. of bytes to be Xferred in Data In Phase */

	buf_resid=t_tmc_pkt->data_len;


	while(1) 
	{

/*		Return the status if Bus Free phase is detected 	*/

		if((TMC_STATUS_CHK(base_port,I_BUSY|I_SELECT,0)))
		{            
			return(ret_val);
		}
	       if((TMC_STATUS_CHK(base_port,I_REQUEST,I_REQUEST)))
		{
#ifdef DEBUG
			statvalue= TMC_READ(base_port,STATUS_OFF);
			putstr("TMC status Register in Phase Handler :");
			put2hex(statvalue);
			putstr("\n\r");
#endif

			switch((TMC_READ(base_port,STATUS_OFF))
				&(I_MESSAGE|I_IO|I_CD)) 
			{

				case MSG_OUT_PH : 

		 			if((TMC_STATUS_CHK(base_port,I_REQUEST,
							    I_REQUEST)) && !msg_rcvd)
					{
		 	     			TMC_WRITE(base_port,CONTROL_OFF,
					        C_PARITY_ENABLE|C_BUS_ENABLE);

					/*	No Reselection is supported */

				 		TMC_WRITE(base_port,WRITE_OFF,
						 IDENTIFY_MSG_WITH_NO_DISCONN);
	              		}			  	
		 			break;

				case CMD_PH :

					/* Transfer cmd_len no. of bytes */

		 			for(cmd_len=0;cmd_len<t_tmc_pkt->cdb_len;
					    cmd_len++)
		 			{
						while(!(TMC_STATUS_CHK(base_port									,I_REQUEST,I_REQUEST)));
						if(TMC_STATUS_CHK(base_port,
						    CMD_PH,CMD_PH) && 
						   (TMC_STATUS_CHK(base_port,
						    I_REQUEST,I_REQUEST)))
						{
				       		TMC_WRITE(base_port,
							 WRITE_OFF,*(cdb_ptr+cmd_len));
						}
	   				}
	   				break;

				case DATA_IN_PH :

					if(buf_resid==0x00) 
					{
#ifdef DEBUG
						putstr("Nothing to read ?\n\r");
#endif
						continue;
		  			}
#if 0
		  			while((buf_resid) && 
			  		(TMC_STATUS_CHK(base_port,DATA_IN_PH,
							  DATA_IN_PH))) 
				   	{
						while(!(TMC_STATUS_CHK(base_port,
							I_REQUEST,I_REQUEST)));

					/*	Read the data on SCSI bus	*/

		        			val_read_in =
						 TMC_READ( base_port,READ_OFF);	

		       			buf_resid--;

					/*	Write the Value into the Seg:Offset
				        *	Address provided	*/

						TMC_WRITE(t_tmc_pkt->data_segment,
					        	t_tmc_pkt->data_offset++,
							val_read_in); 

		  			}
#endif

		  			if( (TMC_STATUS_CHK(base_port,DATA_IN_PH,
					  DATA_IN_PH)))
				   	{
					for(i=0;i<(buf_resid/512);i++)
					{
						while(!(TMC_STATUS_CHK(base_port,
							I_REQUEST,I_REQUEST)));
		        			val_read_in =
						 TMC_READ( base_port,READ_OFF);	
						TMC_WRITE(t_tmc_pkt->data_segment,
					        	t_tmc_pkt->data_offset++,
							val_read_in); 
						while(!(TMC_STATUS_CHK(base_port,
							I_REQUEST,I_REQUEST)));
		        			val_read_in =
						 TMC_READ( base_port,READ_OFF);	
						TMC_WRITE(t_tmc_pkt->data_segment,
					        	t_tmc_pkt->data_offset++,
							val_read_in); 
							
						scsibcopy(t_tmc_pkt->data_offset,
							t_tmc_pkt->data_segment,
							READ_OFF,base_port,
							(ushort)509);

						t_tmc_pkt->data_offset+=509;

						while(!(TMC_STATUS_CHK(base_port,
							I_REQUEST,I_REQUEST)));
		        			val_read_in =
						 TMC_READ( base_port,READ_OFF);	
						TMC_WRITE(t_tmc_pkt->data_segment,
					        	t_tmc_pkt->data_offset++,
							val_read_in); 
					}
					buf_resid=buf_resid%512;
		  			while((buf_resid) && 
			  		(TMC_STATUS_CHK(base_port,DATA_IN_PH,
							  DATA_IN_PH))) 
				   	{
						while(!(TMC_STATUS_CHK(base_port,
							I_REQUEST,I_REQUEST)));
		        			val_read_in =
						 TMC_READ( base_port,READ_OFF);	
						TMC_WRITE(t_tmc_pkt->data_segment,
					        	t_tmc_pkt->data_offset++,
							val_read_in); 
						buf_resid--;
					}

					}
		  			break;

/*			The Data Out Phase is not required in our Real mode
 *			Driver since the media supported is a CD-ROM 	*/

				case DATA_OUT_PH:

		  			if(buf_resid==0) 
					{
#ifdef DEBUG
						putstr("No Data left to write \n\r");
#endif
						continue;
		  			}
		  			while((buf_resid) && 
					      (TMC_STATUS_CHK(base_port,
						DATA_OUT_PH,DATA_OUT_PH))) 
					{
						while(!(TMC_STATUS_CHK(base_port						  		      ,I_REQUEST,I_REQUEST)));
						val_read_in = 
						 TMC_READ(t_tmc_pkt->data_segment,
							   t_tmc_pkt->data_offset++);
						TMC_WRITE(base_port,WRITE_OFF,
							   val_read_in); 
				 
						buf_resid--;
		  			}
		  			break;

				case STATUS_PH :

		  			if((TMC_STATUS_CHK(base_port,I_REQUEST,
					    I_REQUEST)))
				   	{
			    			ret_val =
						 TMC_READ(base_port,READ_OFF);
					}
					break;

				case MSG_IN_PH:

					if((TMC_STATUS_CHK(base_port,
					    I_REQUEST,I_REQUEST)))
					{
						msg_rcvd = 
						 TMC_READ(base_port,READ_OFF);
					}
					break;

				default :
					break;

			} /* End of Outer Switch   */
		} /* End of If loop   */
	} /* End of while loop */
} /* End of Phase Handler routine */

/* printpkt routine to print the TMC packet fields. This routine is called 
 * during debugging only							   */

int
printpkt(PKT_STRUCTURE *pkt)
{
	int i;
	putstr("target id :");
	put2hex(pkt->tmc_target);
	putstr("\n\r");
	putstr("LUN :");
	put2hex(pkt->tmc_lun);
	putstr("\n\r");
	putstr("cdb_len :");
	put2hex(pkt->cdb_len);
	putstr("\n\r");
	putstr("Bytes :");
	putstr("  ");
	for(i=0;i<pkt->cdb_len;i++)
	{
		put2hex(pkt->cdb[i]);
		putstr(" ");
	}
	putstr("\n\r");
	putstr("data len :");
	puthex((ushort) pkt->data_len);
	putstr("\n\r");
	putstr("data segment :");
	puthex(pkt->data_segment);
	putstr("\n\r");
	putstr("data offset :");
	puthex(pkt->data_offset);
	putstr("\n\r");
	return 0;
}

/* printstatus routine to print the status returned by the tmc_ph_handler
 * routine. It is called only during debugging. 				*/

int
printstatus(PKT_STRUCTURE *pkt,unchar stat)
{
	putstr("Status ret by phase handler :");
	put2hex(stat);
	putstr("\n\r");
	return 0;
}

