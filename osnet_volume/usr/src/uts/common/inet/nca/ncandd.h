/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_NCANDD_H
#define	_SYS_NCANDD_H

#pragma ident	"@(#)ncandd.h	1.1	99/08/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#if	SunOS >= SunOS_5_8

#undef	ip_respond_to_address_mask_broadcast
#undef	ip_g_send_redirects
#undef	ip_debug
#undef	ip_mrtdebug
#undef	ip_timer_interval
#undef	ip_def_ttl
#undef	ip_wroff_extra
#undef	ip_path_mtu_discovery
#undef	ip_ignore_delete_time
#undef	ip_output_queue
#undef	ip_broadcast_ttl
#undef	ip_icmp_err_interval
#undef	ip_reass_queue_bytes
#undef	ip_addrs_per_if
#undef	IP_TCP_CONN_HASH
#undef	IP_TCP_CONN_MATCH


#endif 	/* SunOS >= SunOS_5_8 */

/* Named Dispatch Parameter Management Structure */
typedef struct ncaparam_s {
	u_long	param_min;
	u_long	param_max;
	u_long	param_val;
	char	*param_name;
} ncaparam_t;

/*
 * ip_g_forward controls IP forwarding.  It takes two values:
 * 	0: IP_FORWARD_NEVER	Don't forward packets ever.
 *	1: IP_FORWARD_ALWAYS	Forward packets for elsewhere.
 *
 * RFC1122 says there must be a configuration switch to control forwarding,
 * but that the default MUST be to not forward packets ever.  Implicit
 * control based on configuration of multiple interfaces MUST NOT be
 * implemented (Section 3.1).  SunOS 4.1 did provide the "automatic" capability
 * and, in fact, it was the default.  That capability is now provided in the
 * /etc/rc2.d/S69inet script.
 */
#define	ip_g_forward			(uint32_t)nca_param_arr[0].param_val
/* Following line is external, and in ip.h.  Normally marked with * *. */

#define	ip_respond_to_address_mask_broadcast \
	(uint32_t)nca_param_arr[1].param_val
#define	ip_g_resp_to_echo_bcast		(uint32_t)nca_param_arr[2].param_val
#define	ip_g_resp_to_timestamp		(uint32_t)nca_param_arr[3].param_val
#define	ip_g_resp_to_timestamp_bcast	(uint32_t)nca_param_arr[4].param_val
#define	ip_g_send_redirects		(uint32_t)nca_param_arr[5].param_val
#define	ip_g_forward_directed_bcast	(uint32_t)nca_param_arr[6].param_val
#define	ip_debug			(uint32_t)nca_param_arr[7].param_val
#define	ip_mrtdebug			(uint32_t)nca_param_arr[8].param_val
#define	ip_timer_interval		(uint32_t)nca_param_arr[9].param_val
#define	ip_ire_flush_interval		(uint32_t)nca_param_arr[10].param_val
#define	ip_ire_redir_interval		(uint32_t)nca_param_arr[11].param_val
#define	ip_def_ttl			(uint32_t)nca_param_arr[12].param_val
#define	ip_forward_src_routed		(uint32_t)nca_param_arr[13].param_val
#define	ip_wroff_extra			(uint32_t)nca_param_arr[14].param_val
#define	ip_ire_pathmtu_interval		(uint32_t)nca_param_arr[15].param_val
#define	ip_icmp_return			(uint32_t)nca_param_arr[16].param_val
#define	ip_send_source_quench		(uint32_t)nca_param_arr[17].param_val
#define	ip_path_mtu_discovery		(uint32_t)nca_param_arr[18].param_val
#define	ip_ignore_delete_time		(uint32_t)nca_param_arr[19].param_val
#define	ip_ignore_redirect		(uint32_t)nca_param_arr[20].param_val
#define	ip_output_queue			(uint32_t)nca_param_arr[21].param_val
#define	ip_broadcast_ttl		(uint32_t)nca_param_arr[22].param_val
#define	ip_icmp_err_interval		(uint32_t)nca_param_arr[23].param_val
#define	ip_reass_queue_bytes		(uint32_t)nca_param_arr[24].param_val
#define	ip_strict_dst_multihoming	(uint32_t)nca_param_arr[25].param_val
#define	ip_addrs_per_if			(uint32_t)nca_param_arr[26].param_val

#define	tcp_time_wait_interval		(uint32_t)nca_param_arr[27].param_val
#define	tcp_conn_req_max_q		(uint32_t)nca_param_arr[28].param_val
#define	tcp_conn_req_max_q0		(uint32_t)nca_param_arr[29].param_val
#define	tcp_conn_req_min		(uint32_t)nca_param_arr[30].param_val
#define	tcp_conn_grace_period		(uint32_t)nca_param_arr[31].param_val
#define	tcp_cwnd_max_			(uint32_t)nca_param_arr[32].param_val
#define	tcp_dbg				(uint32_t)nca_param_arr[33].param_val
#define	tcp_smallest_nonpriv_port	(uint32_t)nca_param_arr[34].param_val
#define	tcp_ip_abort_cinterval		(uint32_t)nca_param_arr[35].param_val
#define	tcp_ip_abort_linterval		(uint32_t)nca_param_arr[36].param_val
#define	tcp_ip_abort_interval		(uint32_t)nca_param_arr[37].param_val
#define	tcp_ip_notify_cinterval		(uint32_t)nca_param_arr[38].param_val
#define	tcp_ip_notify_interval		(uint32_t)nca_param_arr[39].param_val
#define	tcp_ip_ttl			(uint32_t)nca_param_arr[40].param_val
#define	tcp_keepalive_interval		(uint32_t)nca_param_arr[41].param_val
#define	tcp_maxpsz_multiplier		(uint32_t)nca_param_arr[42].param_val
#define	tcp_mss_def			(uint32_t)nca_param_arr[43].param_val
#define	tcp_mss_max			(uint32_t)nca_param_arr[44].param_val
#define	tcp_mss_min			(uint32_t)nca_param_arr[45].param_val
#define	tcp_naglim_def			(uint32_t)nca_param_arr[46].param_val
#define	tcp_rexmit_interval_initial	(uint32_t)nca_param_arr[47].param_val
#define	tcp_rexmit_interval_max		(uint32_t)nca_param_arr[48].param_val
#define	tcp_rexmit_interval_min		(uint32_t)nca_param_arr[49].param_val
#define	tcp_wroff_xtra			(uint32_t)nca_param_arr[50].param_val
#define	tcp_deferred_ack_interval	(uint32_t)nca_param_arr[51].param_val
#define	tcp_snd_lowat_fraction		(uint32_t)nca_param_arr[52].param_val
#define	tcp_sth_rcv_hiwat		(uint32_t)nca_param_arr[53].param_val
#define	tcp_sth_rcv_lowat		(uint32_t)nca_param_arr[54].param_val
#define	tcp_dupack_fast_retransmit	(uint32_t)nca_param_arr[55].param_val
#define	tcp_ignore_path_mtu		(uint32_t)nca_param_arr[56].param_val
#define	tcp_rcv_push_wait		(uint32_t)nca_param_arr[57].param_val
#define	tcp_smallest_anon_port		(uint32_t)nca_param_arr[58].param_val
#define	tcp_largest_anon_port		(uint32_t)nca_param_arr[59].param_val
#define	tcp_xmit_hiwat			(uint32_t)nca_param_arr[60].param_val
#define	tcp_xmit_lowat			(uint32_t)nca_param_arr[61].param_val
#define	tcp_recv_hiwat			(uint32_t)nca_param_arr[62].param_val
#define	tcp_recv_hiwat_minmss		(uint32_t)nca_param_arr[63].param_val
#define	tcp_fin_wait_2_flush_interval	(uint32_t)nca_param_arr[64].param_val
#define	tcp_co_min			(uint32_t)nca_param_arr[65].param_val
#define	tcp_max_buf			(uint32_t)nca_param_arr[66].param_val
#define	tcp_zero_win_probesize		(uint32_t)nca_param_arr[67].param_val
#define	tcp_strong_iss			(uint32_t)nca_param_arr[68].param_val
#define	tcp_rtt_updates			(uint32_t)nca_param_arr[69].param_val
#define	tcp_wscale_always		(uint32_t)nca_param_arr[70].param_val
#define	tcp_tstamp_always		(uint32_t)nca_param_arr[71].param_val
#define	tcp_tstamp_if_wscale		(uint32_t)nca_param_arr[72].param_val
#define	tcp_rexmit_interval_extra	(uint32_t)nca_param_arr[73].param_val
#define	tcp_deferred_acks_max		(uint32_t)nca_param_arr[74].param_val
#define	tcp_slow_start_after_idle	(uint32_t)nca_param_arr[75].param_val
#define	tcp_slow_start_initial		(uint32_t)nca_param_arr[76].param_val
#define	tcp_co_timer_interval		(uint32_t)nca_param_arr[77].param_val
#define	tcp_sack_permitted		(uint32_t)nca_param_arr[78].param_val
#define	nca_log_cycle			nca_param_arr[79].param_val
#define	no_caching			nca_param_arr[80].param_val
#define	nca_log_size			nca_param_arr[81].param_val
#ifdef	DEBUG
#define	tcp_drop_oob			(uint32_t)nca_param_arr[82].param_val
#else
#define	tcp_drop_oob				0
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NCANDD_H */
