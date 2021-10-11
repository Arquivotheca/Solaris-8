/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)aliases.c   1.1   98/10/26 SMI"

/*
 * Build aliases for some common devices.
 */

#include "types.h"
#include "devdb.h"
#include "boot.h"
#include "aliases.h"
#include "enum.h"
#include "resmgmt.h"

static void MakeComAlias(char *alias, int port);
static void MakeAliasForBoard(char *alias, Board *bp);
static void MakeAlias(char *alias, char *path);
static void MakeAliasByDriver(char *alias, char *driver);

void
MakeAliases()
{
	Board *bp;
	extern Board *find_video_board();

	MakeComAlias("com1", 0x3f8);
	MakeComAlias("com2", 0x2f8);
	MakeComAlias("com3", 0x3e8);
	MakeComAlias("com4", 0x2e8);
	MakeComAlias("ttya", 0x3f8);
	MakeComAlias("ttyb", 0x2f8);
	MakeComAlias("ttyc", 0x3e8);
	MakeComAlias("ttyd", 0x2e8);

	MakeAliasByDriver("keyboard", "key");

	bp = find_video_board();
	if (bp != NULL)
		MakeAliasForBoard("screen", bp);
}

static void
MakeComAlias(char *alias, int port)
{
	Board *bp;
	devtrans *dtp;

	bp = Query_Port(port, 8);

	if (bp != NULL) {
		dtp = bp->dbentryp;

		if (dtp && (strcmp(dtp->real_driver, "com") == 0))
			MakeAliasForBoard(alias, bp);
	}
}

static void
MakeAliasByDriver(char *alias, char *driver)
{
	Board *bp;
	devtrans *dtp;

	for (bp = Head_board; bp; bp = bp->link) {
		dtp = bp->dbentryp;

		if (dtp && (strcmp(dtp->real_driver, driver) == 0)) {
			MakeAliasForBoard(alias, bp);
			return;
		}
	}
}

static void
MakeAliasForBoard(char *alias, Board *bp)
{
	char path[100];

	get_path(bp, path);
	MakeAlias(alias, path);
}

static void
MakeAlias(char *alias, char *path)
{
	char buf[120];

	out_bop("cd /aliases\n");
	sprintf(buf, "setprop %s %s\n", alias, path);
	out_bop(buf);
}
