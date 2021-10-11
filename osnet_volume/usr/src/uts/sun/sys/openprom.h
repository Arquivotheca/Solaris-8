/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_OPENPROM_H
#define	_SYS_OPENPROM_H

#pragma ident	"@(#)openprom.h	1.20	99/10/22 SMI"

/*
 * NOTICE: This should only be included by a very limited number
 * of PROM library functions that interface directly to the PROM.
 * All others should use the library interface functions to avoid
 * dealing with the various differences in the PROM interface versions.
 *
 * The address of the romvec is passed as the first argument to the standalone
 * program, obviating the need for the address to be at a known fixed location.
 * Typically, the stand-alone's srt0.s file (which contains the _start entry)
 * would take care of all of `this'.
 * In SPARC assembler, `this' would be:
 *
 *	.data
 *	.global _romp
 * _romp:
 *	.word 0
 *	.text
 * _start:
 *	set	_romp, %o1	! any register other than %o0 will probably do
 *	st	%o0, [%o1]	! save it away
 *	.......			! set up your stack, etc......
 */

#include <sys/comvec.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Structure into which a V0 PROM will stick memory
 *  list information.  This layout is of course assumed
 *  by the firmware, so no dorking is allowed with it.
 */

struct prom_memlist {
	struct prom_memlist 	*next;
	uint_t			address;
	uint_t			size;
};

/*
 * This is not, strictly speaking, correct: we should never
 * depend upon "magic addresses".  However, the SunMON start
 * and end addresses are needed by boot so that it can forge
 * the appropriate memory lists on SunMON and OBPV0 machines,
 * so that (in turn) no-one else needs to depend on these horrors.
 *
 * Note: It is a gross error to use these in any standalone.
 */
#define	SUNMON_START		(0xffd00000)
#define	SUNMON_END		(0xfff00000)

/*
 * Common Interfaces in all OBP versions:
 */

/*
 * boot(char *bootcommand)
 */
#define	OBP_BOOT		(*romp->obp.op_boot)

/*
 * Abort/enter to the monitor (may be resumed)
 */
#define	OBP_ENTER_MON		(*romp->obp.op_enter)

/*
 * Abort/exit to monitor (may not be resumed)
 */
#define	OBP_EXIT_TO_MON		(*romp->obp.op_exit)

/*
 * PROM Interpreter: (char *forth_string)
 */
#define	OBP_INTERPRET		(*romp->obp.op_interpret)

/*
 * Callback handler:  (Set it to your handler)
 *
 * WARNING: The handler code is different in V0 vs. Later interfaces.
 */
#define	OBP_CB_HANDLER	(*romp->obp.op_vector_cmd)

/*
 * Unreferenced time in milliseconds
 */
#define	OBP_MILLISECONDS	(*romp->obp.op_milliseconds)

/*
 * Configuration ops: (all OBP versions)
 */
#define	OBP_DEVR_NEXT		(*romp->obp.op_config_ops->devr_next)
#define	OBP_DEVR_CHILD		(*romp->obp.op_config_ops->devr_child)
#define	OBP_DEVR_GETPROPLEN	(*romp->obp.op_config_ops->devr_getproplen)
#define	OBP_DEVR_GETPROP	(*romp->obp.op_config_ops->devr_getprop)
#define	OBP_DEVR_SETPROP	(*romp->obp.op_config_ops->devr_setprop)
#define	OBP_DEVR_NEXTPROP	(*romp->obp.op_config_ops->devr_nextprop)

/*
 * V0/V2 Only Interfaces: (They're only in V2 for compatability.)
 * Where possible, should use the V2/V3 interfaces.  For V2, probably
 * the only required interface is the insource/outsink since there's
 * no V2 interface corresponding to root node properties stdin-path/stdout-path.
 */

#define	OBP_V0_PHYSMEMORY	(*romp->obp.v_physmemory)
#define	OBP_V0_VIRTMEMORY	(*romp->obp.v_virtmemory)
#define	OBP_V0_AVAILMEMORY	(*romp->obp.v_availmemory)

#define	OBP_V0_BOOTCMD		(*romp->obp.v_bootcmd)

#define	OBP_V0_OPEN		(*romp->obp.v_open)
#define	OBP_V0_CLOSE		(*romp->obp.v_close)

#define	OBP_V0_READ_BLOCKS	(*romp->obp.v_read_blocks)
#define	OBP_V0_WRITE_BLOCKS	(*romp->obp.v_write_blocks)

#define	OBP_V0_XMIT_PACKET	(*romp->obp.v_xmit_packet)
#define	OBP_V0_POLL_PACKET	(*romp->obp.v_poll_packet)

#define	OBP_V0_READ_BYTES	(*romp->obp.v_read_bytes)
#define	OBP_V0_WRITE_BYTES	(*romp->obp.v_write_bytes)

#define	OBP_V0_SEEK		(*romp->obp.v_seek)

#define	OBP_V0_INSOURCE		(*romp->obp.v_insource)
#define	OBP_V0_OUTSINK		(*romp->obp.v_outsink)

#define	OBP_V0_GETCHAR		(*romp->obp.v_getchar)
#define	OBP_V0_PUTCHAR		(*romp->obp.v_putchar)

#define	OBP_V0_MAYGET		(*romp->obp.v_mayget)
#define	OBP_V0_MAYPUT		(*romp->obp.v_mayput)

#define	OBP_V0_FWRITESTR	(*romp->obp.v_fwritestr)

#define	OBP_V0_PRINTF		(*romp->obp.v_printf)

#define	OBP_V0_BOOTPARAM	(*romp->obp.v_bootparam)

#define	OBP_V0_MAC_ADDRESS	(*romp->obp.v_mac_address)


/*
 * V2 and V3 interfaces:
 */

#define	OBP_V2_BOOTPATH		(*romp->obp.op2_bootpath)
#define	OBP_V2_BOOTARGS		(*romp->obp.op2_bootargs)

#define	OBP_V2_STDIN		(*romp->obp.op2_stdin)
#define	OBP_V2_STDOUT		(*romp->obp.op2_stdout)

#define	OBP_V2_PHANDLE		(*romp->obp.op2_phandle)

#define	OBP_V2_ALLOC		(*romp->obp.op2_alloc)
#define	OBP_V2_FREE		(*romp->obp.op2_free)

#define	OBP_V2_MAP		(*romp->obp.op2_map)
#define	OBP_V2_UNMAP		(*romp->obp.op2_unmap)

#define	OBP_V2_OPEN		(*romp->obp.op2_open)
#define	OBP_V2_CLOSE		(*romp->obp.op2_close)

#define	OBP_V2_READ		(*romp->obp.op2_read)
#define	OBP_V2_WRITE		(*romp->obp.op2_write)
#define	OBP_V2_SEEK		(*romp->obp.op2_seek)

#define	OBP_V2_CHAIN		(*romp->obp.op2_chain)
#define	OBP_V2_RELEASE		(*romp->obp.op2_release)

/*
 * V3 Only interfaces:
 */
#define	OBP_V3_ALLOC		(*romp->obp.op3_alloc)
#define	OBP_V3_HEARTBEAT	(*romp->obp.op3_heartbeat)

/*
 * V3 Only MP Interfaces:
 */
#define	OBP_V3_STARTCPU		(*romp->obp.op3_startcpu)
#define	OBP_V3_STOPCPU		(*romp->obp.op3_stopcpu)
#define	OBP_V3_IDLECPU		(*romp->obp.op3_idlecpu)
#define	OBP_V3_RESUMECPU	(*romp->obp.op3_resumecpu)

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_OPENPROM_H */
