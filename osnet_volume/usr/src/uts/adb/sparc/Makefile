#
#ident	"@(#)Makefile	1.34	99/09/28 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/sparc/Makefile
#

ADB_BASE_DIR	= ..
ISADIR		= .
COMMONDIR	= $(ADB_BASE_DIR)/common
SYSDIR		= $(ADB_BASE_DIR)/..

MACROS_EXT	= pci_fault_handle.dbg pci_common.dbg cb.dbg ib.dbg pbm.dbg \
		iommu.dbg sc.dbg config_header.dbg ecc.dbg pci_ispec.dbg \
		config_header_state.dbg ib_ino_info.dbg dvma_rec.dbg \
		pci_bus_range_t ddi_dma_impl.dbg i_ddi_soft_state.dbg \
		sfpkt sf_stats sftarget sftargets
SCRIPTS_EXT	= $(MACROS_EXT:.dbg=)

SRCS += \
audio_apm_info.dbg	audio_ch.dbg	audio_channel.dbg \
audio_state.dbg	audiotrace.dbg		audiotrace.nxt.dbg \
esp.dbg		esp_cb_info.dbg		esp_tagQ.dbg \
esp_cmd.dbg \
fas.dbg		fas_cmd.dbg		fas_tagQ.dbg \
fas_regs.dbg \
glm.dbg		glm_cmd.dbg		glm_tagQ.dbg \
glm_dsa.dbg	glm_unit.dbg \
dk_label.dbg		dk_vtoc.dbg \
scsi_disk.dbg \
ifp.dbg 	ifp_request.dbg		ifp_response.dbg \
ifp_cmd.dbg 	ifp_portdb.dbg		ifp_icb.dbg \
ifp_full.dbg	ifp_target.dbg \
isp.dbg		isp_request.dbg		isp_response.dbg \
isp_cmd.dbg \
sf.dbg \
le.dbg		lestr.dbg \
mixer_ad_info.dbg \
rwindow.dbg	ssa_disk.dbg 		stackcalls.dbg \
systemdump.dbg	zscom.dbg \
dcd_disk.dbg	dcd_address.dbg		dcd_pkt.dbg \
dcd_cmd.dbg	dcd_device.dbg		dcd_hba_tran.dbg \
cmd_ctl.dbg	ata_controller.dbg	ghd_cmd.dbg \
ata_pkt.dbg	dcd_identify.dbg	ata_drive.dbg \
common_service.dbg	cq.dbg			cq_hdr.dbg \
cqe.dbg 		ct_header.dbg 		d_id_hash.dbg \
fc_dataseg.dbg 		fc_device.dbg		fc_fca_bind.dbg \
fc_fca_port_info.dbg 	fc_lilpmap.dbg		fc_packet.dbg \
fc_port.dbg		fc_port_clist.dbg	fc_port_dev.dbg \
fc_portmap.dbg 		fca_port.dbg 		fca_tran.dbg \
fcio.dbg 		fcp_cmd.dbg		fcp_rsp.dbg \
fp_cmd.dbg 		frame_header.dbg	hard_addr.dbg \
job_request.dbg 	la_els_adisc.dbg	la_els_logi.dbg \
la_els_logo.dbg		la_wwn.dbg		ls_code.dbg \
ns_resp_gan.dbg		nwwn_elem.dbg		nwwn_list.dbg \
port_device.dbg		port_id.dbg 		port_type.dbg \
pwwn_hash.dbg 		service_param.dbg 	soft_attach.dbg \
ssfcp_ipkt.dbg		ssfcp_lun.dbg		ssfcp_pkt.dbg \
ssfcp_port.dbg		ssfcp_tgt.dbg		ulp_modinfo.dbg \
ulp_module.dbg		ulp_port_info.dbg	unsolicited_buffer.dbg \
usoc_cmdonly_request.dbg usoc_data_request.dbg 	usoc_diag_request.dbg \
usoc_hdr.dbg		usoc_kernel_cq.dbg	usoc_pkt_priv.dbg \
usoc_pool_request.dbg	usoc_port.dbg		usoc_priv_cmd.dbg \
usoc_request.dbg	usoc_request_descriptor.dbg	usoc_response.dbg \
usoc_state.dbg		usoc_timetag.dbg	usoc_ub_priv.dbg \
usoc_unsol_cb.dbg	usoc_idinfo.dbg \
pci.dbg		intrspec.dbg		$(MACROS_EXT)
include	$(COMMONDIR)/Makefile.com
INCLUDES	+= -I${SYSDIR}/${MACH}/v9 -I${SYSDIR}/sun4u

$(SCRIPTS_EXT):
