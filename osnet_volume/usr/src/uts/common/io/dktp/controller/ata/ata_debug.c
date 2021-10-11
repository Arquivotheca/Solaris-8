/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)ata_debug.c	1.3	99/02/17 SMI"

#include <sys/types.h>
#include <sys/debug.h>

#include "ata_common.h"
#include "ata_disk.h"
#include "atapi.h"
#include "pciide.h"


#ifdef ATA_DEBUG

void
dump_ata_ctl(ata_ctl_t *P)
{
	ghd_err("dip 0x%x flags 0x%x timing 0x%x\n",
		P->ac_dip, P->ac_flags, P->ac_timing_flags);
	ghd_err("drvp[0][0..7] 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		P->ac_drvp[0][0], P->ac_drvp[0][1], P->ac_drvp[0][2],
		P->ac_drvp[0][3], P->ac_drvp[0][4], P->ac_drvp[0][5],
		P->ac_drvp[0][6], P->ac_drvp[0][7]);
	ghd_err("drvp[1][0..7] 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		P->ac_drvp[1][0], P->ac_drvp[1][1], P->ac_drvp[1][2],
		P->ac_drvp[1][3], P->ac_drvp[1][4], P->ac_drvp[1][5],
		P->ac_drvp[1][6], P->ac_drvp[1][7]);
	ghd_err("max tran 0x%x &ccc_t 0x%x actv drvp 0x%x actv pktp 0x%x\n",
		P->ac_max_transfer, &P->ac_ccc,
		P->ac_active_drvp, P->ac_active_pktp);
	ghd_err("hba tranp 0x%x state %d\n", P->ac_state, P->ac_atapi_tran);
	ghd_err("iohdl1 0x%x 0x%x D 0x%x E 0x%x F 0x%x C 0x%x S 0x%x LC 0x%x "
		"HC 0x%x HD 0x%x ST 0x%x CMD 0x%x\n",
		P->ac_iohandle1, P->ac_ioaddr1, P->ac_data, P->ac_error,
		P->ac_feature, P->ac_count, P->ac_sect, P->ac_lcyl,
		P->ac_hcyl, P->ac_drvhd, P->ac_status, P->ac_cmd);
	ghd_err("iohdl2 0x%x 0x%x AST 0x%x DC 0x%x\n",
		P->ac_iohandle2, P->ac_ioaddr2, P->ac_altstatus, P->ac_devctl);
	ghd_err("bm hdl 0x%x 0x%x pciide %d BM %d sg_list 0x%x paddr 0x%x "
		"acc hdl 0x%x sg hdl 0x%x\n",
		P->ac_bmhandle, P->ac_bmaddr, P->ac_pciide, P->ac_pciide_bm,
		P->ac_sg_list, P->ac_sg_paddr, P->ac_sg_acc_handle,
		P->ac_sg_handle);
	ghd_err("arq pktp 0x%x flt pktp 0x%x &cdb\n",
		P->ac_arq_pktp, P->ac_fault_pktp, P->ac_arq_cdb);
}

void
dump_ata_drv(ata_drv_t *P)
{


	ghd_err("ctlp 0x%x &ata_id 0x%x flags 0x%x pciide dma 0x%x\n",
		P->ad_ctlp, &P->ad_id, P->ad_flags, P->ad_pciide_dma);

	ghd_err("targ %d lun %d driv 0x%x state %d cdb len %d "
		"bogus %d nec %d\n", P->ad_targ, P->ad_lun, P->ad_drive_bits,
		P->ad_state, P->ad_cdb_len, P->ad_bogus_drq,
		P->ad_nec_bad_status);

	ghd_err("ata &scsi_dev 0x%x &scsi_inquiry 0x%x &ctl_obj\n",
		&P->ad_device, &P->ad_inquiry, &P->ad_ctl_obj);

	ghd_err("ata rd cmd 0x%x wr cmd 0x%x acyl 0x%x\n",
		P->ad_rd_cmd, P->ad_wr_cmd, P->ad_acyl);

	ghd_err("ata bios cyl %d hd %d sec %d  phs hd %d sec %d\n",
		P->ad_bioscyl, P->ad_bioshd, P->ad_biossec, P->ad_phhd,
		P->ad_phsec);

	ghd_err("block factor %d bpb %d\n",
		P->ad_block_factor, P->ad_bytes_per_block);
}

void
dump_ata_pkt(ata_pkt_t *P)
{
	ghd_err("gcmdp 0x%x flags 0x%x v_addr 0x%x dma %d\n",
		P->ap_gcmdp, P->ap_flags, P->ap_v_addr, P->ap_pciide_dma);
	ghd_err("&sg_list 0x%x sg cnt 0x%x resid 0x%x bcnt 0x%x\n",
		P->ap_sg_list, P->ap_sg_cnt, P->ap_resid, P->ap_bcount);
	ghd_err("sec 0x%x cnt 0x%x lc 0x%x hc 0x%x hd 0x%x cmd 0x%x\n",
		P->ap_sec, P->ap_count, P->ap_lwcyl, P->ap_hicyl,
		P->ap_hd, P->ap_cmd);
	ghd_err("status 0x%x error 0x%x\n", P->ap_status, P->ap_error);
	ghd_err("start 0x%x intr 0x%x complete 0x%x\n",
		P->ap_start, P->ap_intr, P->ap_complete);
	ghd_err("ata cdb 0x%x scb 0x%x bpb 0x%x wrt cnt 0x%x\n",
		P->ap_cdb, P->ap_scb, P->ap_bytes_per_block, P->ap_wrt_count);
	ghd_err("atapi cdbp 0x%x cdb len %d cdb pad %d\n",
		P->ap_cdbp, P->ap_cdb_len, P->ap_cdb_pad);
	ghd_err("scbp 0x%x statuslen 0x%x\n", P->ap_scbp, P->ap_statuslen);
}

#endif
