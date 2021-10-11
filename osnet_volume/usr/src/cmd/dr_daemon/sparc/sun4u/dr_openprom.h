/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DR_OPENPROM_H
#define	_DR_OPENPROM_H

#pragma ident	"@(#)dr_openprom.h	1.11	98/11/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/openpromio.h>
#include "dr_daemon.h"

#define	MAXPROPSIZE	128
#define	MAXVALSIZE	(4096 - MAXPROPSIZE - sizeof (u_int))
#define	BUFSIZE	(MAXPROPSIZE + MAXVALSIZE + sizeof (u_int))

#define	MAX_PSYCHO	2

typedef union {
	char buf[BUFSIZE];
	struct openpromio opp;
} Oppbuf;


typedef enum	{
	DR_CPU_UNIT = 0,
	DR_MEM_UNIT,
	DR_IO_SBUS_UNIT,
	DR_IO_PCI_UNIT,
	DR_NO_UNIT,
	DR_UNIT_TYPES,
	DR_BAD_UNIT = -1
} dr_unit_type;

/* The lower two bits of a Starfire UPA port ID are the port number */
#define	DEVICEID_UNIT_NUM(did)		((did) & 0x03)

extern int do_prominfo(void (*fn)(int id, int level, void *argp), void *argp);
extern int getpropval(char *propname, void **result);

/*
 * structure used by obp_config_fn to keep trace of where we
 * are in the obp tree so we can figure out what info to collect.
 */
struct obp_level_info {
	dr_unit_type	unit_type;
	int		unit_level;
	struct {
		sbus_configp_t	cur_sbus;
		int		last_child;
		int		slot_level;
#ifdef _XFIRE
		int		sysio_num;	/* 2 sysios per board */
#endif _XFIRE
	} io;				/* temps for decoding io-unit */
};

/*
 * macro which clears out the level structure for tree searching.
 */
#define	CLEAR_LEVEL_INFO(lp) {\
	memset((void *)(lp), 0, sizeof (struct obp_level_info)); \
	(lp)->unit_type = DR_NO_UNIT; \
	(lp)->unit_level = -1; }

/*
 * Argument to the openprom callout routine obp_config_fn
 */
struct obp_config_fn_arg {
	int			board;
	board_configp_t 	brdcfgp;
	struct obp_level_info	level_info;
};

/*
 * Argument to the openprom callout routine obp_board_search
 */
struct obp_board_search_arg {
	int	board;
	int	board_found;
};

#ifdef	__cplusplus
}
#endif

#endif /* _DR_OPENPROM_H */
