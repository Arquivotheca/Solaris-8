/*
 * Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _CS_TYPES_H
#define	_CS_TYPES_H

#pragma ident	"@(#)cs_types.h	1.10	96/08/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PCMCIA Card Services types header file
 */

typedef uint32_t client_handle_t;
typedef	uint32_t window_handle_t;
typedef uint32_t event_t;
typedef uint8_t	cfg_regs_t;

typedef struct baseaddru_t {
	uint32_t		base;
	ddi_acc_handle_t	handle;
} baseaddru_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _CS_TYPES_H */
