#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)librtld_db.msg	1.9	99/06/23 SMI"

# Message file for cmd/sgs/librtld_db.

@ MSG_ID_LIBRTLD_DB


# There presently seem little point in internationalizing these messages
# as they provide debugging information for dbx developers and shouldn't
# be see by anyone else.

@ _END_

# Basic strings

@ MSG_DB_LOGENABLE	"rtld_db: rl: logging enabled!\n"
@ MSG_DB_LOOKFAIL	"rtld_db: rr: lookup of %s failed\n"
@ MSG_DB_DMLOOKFAIL	"rtld_db: rr: lookup of data model failed\n"
@ MSG_DB_NOFINDRTLD	"rtld_db: rr: unable to find %s in ld.so.1\n"
@ MSG_DB_RESETFAIL	"rtld_db: rn: rd_reset failed\n"
@ MSG_DB_LKMAPFAIL	"rtld_db: im: failed in reading link_maps\n"
@ MSG_DB_READDBGFAIL_1	"rtld_db: rli: failed to read rtld_db_priv: 0x%llx\n"
@ MSG_DB_READDBGFAIL_2	"rtld_db: reg: failed to read rdebug: 0x%llx\n"
@ MSG_DB_READDBGFAIL_3	"rtld_db: rli: failed to read dynlm_list: 0x%llx\n"
@ MSG_DB_READDBGFAIL_4	"rtld_db: rli: failed to read listnode: 0x%llx\n"
@ MSG_DB_READDBGFAIL_5	"rtld_db: rli: failed to read listnode.data: 0x%llx\n"
@ MSG_DB_READPRIVFAIL_1	"rtld_db: rr: fail to read rtld_db_priv: 0x%llx\n"
@ MSG_DB_BADPVERS	"rtld_db: rr: rtld vs. rtld_db version mismatch: \
			 %d != %d\n"
@ MSG_DB_LKMAPNOINIT	"rtld_db: rli: link maps are not yet initialized: \
			 rtd_dynlmlst: 0x%llx\n"
@ MSG_DB_LKMAPNOINIT_1	"rtld_db: rli: dynlm_list is not yet initialized: \
			 dynlm_list.head: 0x%llx\n"
@ MSG_DB_NULLITER	"rtld_db: rli: called with null iterator\n"
@ MSG_DB_UNEXPEVENT	"rtld_db: rea: unexpected event num: %d\n"
@ MSG_DB_READFAIL_1	"rtld_db: ree: read of 0x%llx failed\n"
@ MSG_DB_READFAIL_2	"rtld_db: rpr: read of 0x%llx failed\n"
@ MSG_DB_READFAIL_3	"rtld_db: roe: read of 0x%llx failed\n"
@ MSG_DB_WRITEFAIL_1	"rtld_db: ree: write of 0x%llx failed\n"
@ MSG_DB_WRITEFAIL_2	"rtld_db: roe: write of 0x%llx failed\n"
@ MSG_DB_UNFNDSYM	"rtld_db: rbea: unable to find sym: %s\n"

@ MSG_ER_OK		"no error"
@ MSG_ER_ERR		"generic rtld_db.so error"
@ MSG_ER_DBERR		"debugger service failed"
@ MSG_ER_NOCAPAB	"capability not available"
@ MSG_ER_NODYNAM	"couldn't find '_DYNAMIC'"
@ MSG_ER_NOBASE		"couldn't find auxv tag 'AT_BASE'"
@ MSG_ER_NOMAPS		"link-maps are not initialized"
@ MSG_ER_DEFAULT	"unknown rtld_db.so error code"

@ MSG_SYM_DEBUG		"r_debug"
@ MSG_SYM_RTLDDBPV	"rtld_db_priv"
@ MSG_SYM_PREINIT	"rtld_db_preinit"
@ MSG_SYM_POSTINIT	"rtld_db_postinit"
@ MSG_SYM_DLACT		"rtld_db_dlactivity"
@ MSG_SYM_RTBIND	"elf_rtbndr"
