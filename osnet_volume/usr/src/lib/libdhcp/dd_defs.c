/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd_defs.c	1.24	96/04/22 SMI"

#include <dd_impl.h>
#include <rpcsvc/nis.h>

static struct tbl_trans_data hosts_trans = {
	TBL_HOSTS, 2, 4, DEFAULT_COLUMN_SEP, DEFAULT_COMMENT_SEP,
	DEFAULT_ALIAS_SEP, 0,
	{
		{ TBL_VALID_HOST_IP, TBL_INV_HOST_IP },
		{ TBL_VALID_HOSTNAME, TBL_INV_HOSTNAME },
		{ 0, 0 },
		{ 0, 0 }
	},
	{
		{ TBL_INET_PREFIX, TBL_NULL_FIX, "hosts", 2, 3,
		    &_dd_compare_ufs_col1, 4,
			{
				{ COL_KEY|COL_UNIQUE, 0, 0, 0, 0 },
				{ COL_KEY|COL_UNIQUE, 1, 1, 1, 2 },
				{ 0, 2, -1, -1, -1 },
				{ 0, 3, -1, -1, -1 }
			}
		},
		{ TBL_NULL_FIX, TBL_DFLT_SUFFIX, "hosts", 1, 3,
		    &_dd_compare_nisplus_aliased, 4,
			{
				{ COL_KEY|COL_CASEI, 1, 1, 1, 1 },
				{ COL_KEY|COL_CASEI|COL_UNIQUE, 2, 1, -1, -1 },
				{ COL_KEY|COL_UNIQUE, 0, 0, 2, 2 },
				{ 0, 3, -1, -1, -1 }
			}
		}
	}
};

static struct tbl_trans_data dhcpip_trans = {
	TBL_DHCPIP, 5, 7, DEFAULT_COLUMN_SEP, DEFAULT_COMMENT_SEP,
	DEFAULT_ALIAS_SEP, 0,
	{
		{ TBL_VALID_CLIENT_ID, TBL_INV_CLIENT_ID },
		{ TBL_VALID_DHCP_FLAG, TBL_INV_DHCP_FLAG },
		{ TBL_VALID_HOST_IP, TBL_INV_HOST_IP },
		{ TBL_VALID_HOST_IP, TBL_INV_HOST_IP },
		{ TBL_VALID_LEASE_EXPIRE, TBL_INV_LEASE_EXPIRE },
		{ TBL_VALID_PACKET_MACRO, TBL_INV_PACKET_MACRO },
		{ 0, 0 }
	},
	{
		{ TBL_DHCP_DB"/", TBL_NULL_FIX, TBL_NULL_NAME, -1, 6,
		    &_dd_compare_ufs_col0, 7,
			{
				{ COL_KEY, 0, 0, 0, 0 },
				{ 0, 1, -1, -1, -1 },
				{ COL_KEY|COL_UNIQUE, 2, 1, 2, 2 },
				{ COL_KEY, 3, 2, 3, 3 },
				{ COL_KEY, 4, 3, 4, 4 },
				{ COL_KEY, 5, 4, 5, 5 },
				{ 0, 6, -1, -1, -1 }
			}
		},
		{ TBL_NULL_FIX, TBL_DFLT_SUFFIX, TBL_NULL_NAME, -1, 6,
		    &_dd_compare_nisplus_col0, 7,
			{
				{ COL_KEY, 0, 0, 0, 0 },
				{ 0, 1, -1, -1, -1 },
				{ COL_KEY|COL_UNIQUE, 2, 1, 2, 2 },
				{ COL_KEY, 3, 2, 3, 3 },
				{ COL_KEY, 4, 3, 4, 4 },
				{ COL_KEY, 5, 4, 5, 5 },
				{ 0, 6, -1, -1, -1 }
			}
		}
	}
};

static struct tbl_trans_data dhcptab_trans = {
	TBL_DHCPTAB, 2, 3, DEFAULT_COLUMN_SEP, DEFAULT_COMMENT_SEP,
	DEFAULT_ALIAS_SEP, 0,
	{
		{ TBL_VALID_DHCPTAB_KEY, TBL_INV_DHCPTAB_KEY },
		{ TBL_VALID_DHCPTAB_TYPE, TBL_INV_DHCPTAB_TYPE },
		{ 0, 0 }
	},
	{
		{ TBL_DHCP_DB"/", TBL_NULL_FIX, "dhcptab", 2, -1,
		    &_dd_compare_ufs_dhcptab, 3,
			{
				{ COL_KEY|COL_UNIQUE, 0, 0, 0, 0 },
				{ COL_KEY, 1, 1, 1, 1 },
				{ 0, 2, -1, -1, -1 }
			}
		},
		{ TBL_NULL_FIX, TBL_DFLT_SUFFIX, "dhcptab", -1, -1,
		    &_dd_compare_nisplus_dhcptab, 3,
			{
				{ COL_KEY|COL_UNIQUE, 0, 0, 0, 0 },
				{ COL_KEY, 1, 1, 1, 1 },
				{ 0, 2, -1, -1, -1 }
			}
		}
	}
};

struct tbl_trans_data *_dd_ttd[] = {
	&hosts_trans,
	&dhcpip_trans,
	&dhcptab_trans
};


/*
 * Table column object definitions for creating new tables in NIS+
 */

static struct tbl_make_data tm_dhcpip = {
	"dhcp_ip_tbl",
	"",
	7,
	' ',
	{
		{"client_id", TA_SEARCHABLE, 0},
		{"flags", 0, 0},
		{"client_ip", TA_SEARCHABLE, 0},
		{"server_ip", TA_SEARCHABLE, 0},
		{"expire", TA_SEARCHABLE, 0},
		{"macro", TA_SEARCHABLE, 0},
		{"comment", 0, 0},
	}
};

static struct tbl_make_data tm_dhcptab = {
	"dhcp_tbl",
	"",
	3,
	' ',
	{
		{"key", TA_SEARCHABLE, 0},
		{"flag", TA_SEARCHABLE, 0},
		{"value", 0, 0},
	}
};

struct tbl_make_data *_dd_tmd[] = {
	NULL,
	(struct tbl_make_data *)&tm_dhcpip,
	(struct tbl_make_data *)&tm_dhcptab
};
