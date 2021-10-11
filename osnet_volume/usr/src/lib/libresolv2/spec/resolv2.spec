#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)resolv2.spec	1.4	99/05/14 SMI"
#
# lib/libresolv2/spec/resolv2.spec

function	__b64_ntop
version		SUNWprivate_2.1
end

function	__b64_pton
version		SUNWprivate_2.1
end

function	__dn_count_labels
version		SUNWprivate_2.1
end

function	__dn_skipname
version		SUNW_2.1
end

function	__evAddTime
version		SUNWprivate_2.1
end

function	__evCancelConn
version		SUNWprivate_2.1
end

function	__evCancelRW
version		SUNWprivate_2.1
end

function	__evClearTimer
version		SUNWprivate_2.1
end

function	__evConnect
version		SUNWprivate_2.1
end

function	__evConsIovec
version		SUNWprivate_2.1
end

function	__evConsTime
version		SUNWprivate_2.1
end

function	__evCreate
version		SUNWprivate_2.1
end

function	__evDeselectFD
version		SUNWprivate_2.1
end

function	__evDispatch
version		SUNWprivate_2.1
end

function	__evDo
version		SUNWprivate_2.1
end

function	__evGetNext
version		SUNWprivate_2.1
end

function	__evListen
version		SUNWprivate_2.1
end

function	__evNowTime
version		SUNWprivate_2.1
end

function	__evRead
version		SUNWprivate_2.1
end

function	__evResetTimer
version		SUNWprivate_2.1
end

function	__evSelectFD
version		SUNWprivate_2.1
end

function	__evSetDebug
version		SUNWprivate_2.1
end

function	__evSetTimer
version		SUNWprivate_2.1
end

function	__evWaitFor
version		SUNWprivate_2.1
end

function	__fp_nquery
version		SUNWprivate_2.1
end

function	__fp_query
version		SUNW_2.1
end

function	__fp_resstat
version		SUNWprivate_2.1
end

function	__hostalias
version		SUNW_2.1
end

function	__loc_aton
version		SUNWprivate_2.1
end

function	__loc_ntoa
version		SUNWprivate_2.1
end

function	__log_add_channel
version		SUNWprivate_2.1
end

function	__log_category_is_active
version		SUNWprivate_2.1
end

function	__log_close_stream
version		SUNWprivate_2.1
end

function	__log_free_context
version		SUNWprivate_2.1
end

function	__log_get_stream
version		SUNWprivate_2.1
end

function	__log_inc_references
version		SUNWprivate_2.1
end

function	__log_new_context
version		SUNWprivate_2.1
end

function	__log_new_file_channel
version		SUNWprivate_2.1
end

function	__log_new_null_channel
version		SUNWprivate_2.1
end

function	__log_new_syslog_channel
version		SUNWprivate_2.1
end

function	__log_open_stream
version		SUNWprivate_2.1
end

function	__log_option
version		SUNWprivate_2.1
end

function	__log_vwrite
version		SUNWprivate_2.1
end

function	__log_write
version		SUNWprivate_2.1
end

function	__ns_get16
version		SUNWprivate_2.1
end

function	__ns_get32
version		SUNWprivate_2.1
end

function	__ns_initparse
version		SUNWprivate_2.1
end

function	__ns_name_ntop
version		SUNWprivate_2.1
end

function	__ns_name_pton
version		SUNWprivate_2.1
end

function	__ns_parse_ttl
version		SUNWprivate_2.1
end

function	__ns_parserr
version		SUNWprivate_2.1
end

function	__ns_put16
version		SUNWprivate_2.1
end

function	__ns_sprintrr
version		SUNWprivate_2.1
end

function	__ns_sprintrrf
version		SUNWprivate_2.1
end

function	__p_cdname
version		SUNW_2.1
end

function	__p_class
version		SUNW_2.1
end

function	__p_class_syms
version		SUNWprivate_2.1
end

function	__p_query
version		SUNW_2.1
end

function	__p_secstodate
version		SUNWprivate_2.1
end

function	__p_section
version		SUNWprivate_2.1
end

function	__p_time
version		SUNW_2.1
end

function	__p_type
version		SUNW_2.1
end

function	__p_type_syms
version		SUNWprivate_2.1
end

function	__putlong
version		SUNW_2.1
end

function	__putshort
version		SUNWprivate_2.1
end

function	__res_dnok
version		SUNWprivate_2.1
end

function	__res_hnok
version		SUNWprivate_2.1
end

function	__res_mailok
version		SUNWprivate_2.1
end

function	__res_nameinquery
version		SUNWprivate_2.1
end

function	__res_ownok
version		SUNWprivate_2.1
end

function	__res_randomid
version		SUNWprivate_2.1
end

function	__sym_ntop
version		SUNWprivate_2.1
end

function	__sym_ntos
version		SUNWprivate_2.1
end

function	__sym_ston
version		SUNWprivate_2.1
end

function	_getlong
version		SUNW_2.1
end

function	_getshort
version		SUNW_2.1
end

function	_ns_flagdata
version		SUNWprivate_2.1
end

function	_res
version		SUNW_2.1
end

function	_res_opcodes
version		SUNWprivate_2.1
end

function	_res_resultcodes
version		SUNWprivate_2.1
end

function	daemon
version		SUNWprivate_2.1
end

function	dn_comp
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int dn_comp(const char *exp_dn, uchar_t *comp_dn, int length, \
			uchar_t **dnptrs, uchar_t **lastdnptr)
version		SUNW_2.1
exception	$return == -1
end

function	dn_expand
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int dn_expand(const uchar_t *msg, const uchar_t *eomorig, \
			const uchar_t *comp_dn, char *exp_dn, int length)
version		SUNW_2.1
exception	$return == -1
end

function	evPollfdAdd
version		SUNWprivate_2.1
end

function	evPollfdDel
version		SUNWprivate_2.1
end

function	h_errlist
version		SUNWprivate_2.1
end

function	h_errno
version		SUNW_2.1
end

function	herror
version		SUNWprivate_2.1
end

function	hstrerror
version		SUNW_2.1
end

function	inet_aton
version		SUNWprivate_2.1
end

function	inet_nsap_addr
version		SUNWprivate_2.1
end

function	inet_nsap_ntoa
version		SUNWprivate_2.1
end

function	res_endhostent
version		SUNWprivate_2.1
end

function	res_gethostbyaddr
version		SUNWprivate_2.1
end

function	res_gethostbyname
version		SUNWprivate_2.1
end

function	res_gethostbyname2
version		SUNWprivate_2.2
end

function	res_gethostent
version		SUNWprivate_2.1
end

function	res_init
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_init(void)
version		SUNW_2.1
end

function	res_mkquery
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_mkquery(int op, const char *dname, int class, \
			int type, const uchar_t *data, int datalen, \
			const uchar_t *newrr, uchar_t *buf, int buflen)
version		SUNW_2.1
exception	$return == -1
end

function	res_mkupdrec
version		SUNWprivate_2.1
end

function	res_query
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_query(const char *dname, int class, int type, \
			uchar_t *answer, int anslen)
version		SUNW_2.1
end

function	res_querydomain
version		SUNW_2.1
end

function	res_search
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_search(const char *dname, int class, int type, \
			uchar_t *answer, int anslen)
version		SUNW_2.1
end

function	res_send
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_send(const uchar_t *msg, int msglen, uchar_t *answer, \
			int anslen)
version		SUNW_2.1
exception	$return == -1
end

function	res_sethostent
version		SUNWprivate_2.1
end

function	res_update
version		SUNW_2.1
end

function	tree_add
version		SUNWprivate_2.1
end

function	tree_init
version		SUNWprivate_2.1
end

function	tree_srch
version		SUNWprivate_2.1
end

function	tree_trav
version		SUNWprivate_2.1
end

function	__assertion_failed
version		SUNWprivate_2.1
end

function	__evDestroy
version		SUNWprivate_2.1
end

function	__evUnwait
version		SUNWprivate_2.1
end

function	__log_free_channel
version		SUNWprivate_2.1
end

function	__log_get_channel_type
version		SUNWprivate_2.1
end

function	__memget
version		SUNWprivate_2.1
end

function	__memput
version		SUNWprivate_2.1
end

function	__memstats
version		SUNWprivate_2.1
end

function	__res_disable_mt
version		SUNWprivate_2.1
end

function	__res_enable_mt
version		SUNWprivate_2.1
end

function	__res_get_h_errno
version		SUNWprivate_2.1
end

function	__res_get_res
version		SUNWprivate_2.1
end

function	__res_set_no_hosts_fallback
version		SUNWprivate_2.1
end

function	assertion_type_to_text
version		SUNWprivate_2.1
end

function	res_freeupdrec
version		SUNWprivate_2.1
end

function	set_assertion_failure_callback
version		SUNWprivate_2.1
end

function	tree_mung
version		SUNWprivate_2.1
end
