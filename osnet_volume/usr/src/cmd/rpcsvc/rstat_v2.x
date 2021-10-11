%/*
% * Version 2 rstat; for backwards compatibility only.
% */

%/*
% * Copyright (c) 1985, 1990, 1991 by Sun Microsystems, Inc.
% */

%/* from rstat_v2.x */

#ifdef RPC_HDR
%
%#pragma ident	"@(#)rstat_v2.x	1.4	97/04/29 SMI"
%
#endif

const RSTAT_V2_CPUSTATES = 4;
const RSTAT_V2_DK_NDRIVE = 4;

/*
 * the cpu stat values
 */

const RSTAT_V2_CPU_USER = 0;
const RSTAT_V2_CPU_NICE = 1;
const RSTAT_V2_CPU_SYS = 2;
const RSTAT_V2_CPU_IDLE = 3;

/*
 * GMT since 0:00, January 1, 1970
 */
struct rstat_v2_timeval {
	int tv_sec;	/* seconds */
	int tv_usec;	/* and microseconds */
};

struct statsswtch {				/* RSTATVERS_SWTCH */
	int cp_time[RSTAT_V2_CPUSTATES];
	int dk_xfer[RSTAT_V2_DK_NDRIVE];
	int v_pgpgin;	/* these are cumulative sum */
	int v_pgpgout;
	int v_pswpin;
	int v_pswpout;
	int v_intr;
	int if_ipackets;
	int if_ierrors;
	int if_oerrors;
	int if_collisions;
	int v_swtch;
	int avenrun[3];
	rstat_v2_timeval boottime;
};

program RSTATPROG {
	/*
	 * Does not have current time
	 */
	version RSTATVERS_SWTCH {
		statsswtch
		RSTATPROC_STATS(void) = 1;

		unsigned int
		RSTATPROC_HAVEDISK(void) = 2;
	} = 2;
} = 100001;
