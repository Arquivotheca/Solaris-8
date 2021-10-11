/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * "@(#)FN_ref_serial.x	1.1 94/07/25 SMI"
 */

struct xFN_identifier {
	unsigned int	format;
	opaque		contents<>;
};

struct xFN_ref_addr {
	xFN_identifier	type;
	opaque		addr<>;
};

struct xFN_ref {
	xFN_identifier	type;
	xFN_ref_addr	addrs<>;
};
