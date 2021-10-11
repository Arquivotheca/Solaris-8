/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dr_obp_config.c	1.16	98/08/12 SMI"

/*
 * This file implements the Dynamic Reconfiguration OBP Configuration Routine.
 */

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/autoconf.h>
#include <sys/sunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddipropdefs.h>
#include <sys/obpdefs.h>
#include <sys/openpromio.h>
#include <wait.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "dr_subr.h"
#include "dr_openprom.h"

extern void obp_config_fn(int id, int level, void *argp);
static void obp_board_search(int id, int level, void *argp);

/*
 * dr_get_obp_board_configuration
 *
 * Called in response to the RPC GET_OBP_BOARD_CONFIGURATION request.
 * This routine can be called by the application to find out what's on
 * the board prior to it actually being attached.  Once it is attached,
 * GET_BOARD_CONFIGURATION gives a better picture of what is actually
 * on the board.
 *
 * Input:
 * 	board
 *
 * Output:
 *	board_config structure filled in
 */
void
dr_get_obp_board_configuration(int board, board_configp_t brdcfgp)
{
	struct obp_config_fn_arg 	config_fn_arg;

	brdcfgp->board_slot = board;

	/*
	 * Find the memory config from the kernel.
	 */
	(void) get_mem_config(board, &brdcfgp->mem);

	/*
	 * Now call the openprom tree traversal routines.
	 */
	config_fn_arg.board = board;
	config_fn_arg.brdcfgp = brdcfgp;
	CLEAR_LEVEL_INFO(&config_fn_arg.level_info);

	/* walk the prom, calling fn(argp) for each node */
	(void) do_prominfo(obp_config_fn, (void *)&config_fn_arg);
}

/*
 * board_in_use
 *
 * This support routine for get_dr_state determines if the
 * board is in use by the kernel.  We search the openprom structures
 * looking for an io-unit, mem-unit, or cpu-unit which is on the
 * board.  If one is found, than we determine that the board is in
 * use.
 *
 * Input: board
 *
 * Return: true (non-zero), false (0)
 */
int
board_in_use(int board)
{
	struct obp_board_search_arg	search_arg;

	search_arg.board = board;
	search_arg.board_found = 0;

	/* walk the prom, calling fn(argp) for each node */
	(void) do_prominfo(obp_board_search, (void *)&search_arg);

	return (search_arg.board_found);
}

/*
 * obp_board_search
 *
 * This routine is called during the OBP walk when a node is
 * found.  Determine if it belongs to this board.  If so, then this
 * board is configured and in use by the kernel.
 *
 * Input:
 *	id	- of the obp node used only in debug printout
 *	level	- obp tree level
 * 	ap	- obp_board_search_arg pointer where
 *			argp->board
 *			argp->board_found is our result
 */
/* ARGSUSED */
static void
obp_board_search(id, level, ap)
	int	id;
	int	level;
	void	*ap;
{
	struct obp_board_search_arg *argp;
	char			*val_char;
	int			*val_int;

	argp = (struct obp_board_search_arg *)ap;

	/*
	 * If we've already determine the board is present, no more work to do
	 */
	if (argp->board_found)
		return;
	/*
	 * All units have a board# property.  See if this
	 * is the board we're interested in.
	 */
	if (getpropval("board#", (void *)&val_int) || argp->board != *val_int)
		return;

	if (getpropval("name", (void *)&val_char))
		return;

#ifdef _XFIRE
	/*
	 * the above criteria ("board#" property = argp->board && "name"
	 * property) is good enough for xfire.
	 */
	argp->board_found = 1;
	return;
#endif /* _XFIRE */
}
