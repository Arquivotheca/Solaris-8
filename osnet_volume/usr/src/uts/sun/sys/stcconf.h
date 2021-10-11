/*
 * Copyright (c) 1990-1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_STCCONF_H
#define	_SYS_STCCONF_H

#pragma ident	"@(#)stcconf.h	1.14	93/04/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * stcconf.h - misc default configuration parameters
 */

/*
 * default setting for the serial lines
 */
static struct stc_defaults_t stc_initmodes = {
	SDFLAGS,				/* flags */
	STC_DRAIN_BSIZE,			/* drain_size */
	STC_HIWATER,				/* stc_hiwater */
	STC_LOWWATER,				/* stc_lowwater */
	STC_RTPR,				/* rtpr */
	RX_FIFO_SIZE,				/* rx_fifo_thld */

	/* struct termios */
	{
		BRKINT|ICRNL|IXON|ISTRIP,		/* iflag */
		OPOST|ONLCR|XTABS,			/* oflag */
		CFLAGS|TX_BAUD,				/* cflag */
		ISIG|ICANON|ECHO,			/* lflag */
		{					/* cc[NCCS] */
			CINTR, CQUIT, CERASE, CKILL,
			CEOF, CEOL, CEOL2, CSWTCH,
			CSTART, CSTOP, CSUSP, CDSUSP,
			CRPRNT, CFLUSH, CWERASE, CLNEXT,
		},
	},
};

/*
 * default setting for the ppc line
 */
static struct stc_defaults_t ppc_initmodes = {
	PDFLAGS,				/* flags */
	0,					/* drain_size */
	0,					/* stc_hiwater */
	0,					/* stc_lowwater */
	0,					/* rtpr */
	0,					/* rx_fifo_thld */

	/* struct termios */
	{
		0,					/* iflag */
		0,					/* oflag */
		0,					/* cflag */
		0,					/* lflag */
		{ 					/* cc[NCCS] */
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
		},
	},
	PPC_STROBE_W,				/* strobe_w */
	PPC_DATA_SETUP,				/* data_setup */
	PPC_ACK_TIMEOUT,			/* ack_timeout */
	PPC_ERR_TIMEOUT,			/* error_timeout */
	PPC_BSY_TIMEOUT,			/* busy_timeout */
};

#ifdef	__cplusplus
}
#endif

#endif	/* !_SYS_STCCONF_H */
