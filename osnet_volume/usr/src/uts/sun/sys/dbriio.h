/*
 * Copyright (c) 1991-92 by Sun Microsystems, Inc.
 */

#ifndef _SYS_DBRIIO_H
#define	_SYS_DBRIIO_H

#pragma ident	"@(#)dbriio.h	1.7	92/11/16 SMI"

/*
 * DBRI specific ioctl descriptions
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DBRI-specific parameters for the ISDN_SET_PARAM ioctl.
 */
	/*
	 * Set required application service interval.  If application
	 * does not service within this interval, then the device
	 * driver will assume that the upper layer software is no
	 * longer functioning and will behave as if power has been
	 * removed from the interface.
	 *
	 * The duration is expressed in milliseconds.
	 *
	 * A duration of zero millseconds disables the sanity timer.
	 * When the sanity timer is disabled, the applications is assumed
	 * to be sane whenever the interface's D-channel is open.
	 *
	 * The sanity timer is disabled by default.
	 */
#define	DBRI_PARAM_SANITY_INTERVAL	ISDN_PARAM_VENDOR(1)

	/*
	 * Application executes this ioctl at least once every
	 * sanity timer interval in order to assure the driver of
	 * working upper layers.
	 */
#define	DBRI_PARAM_SANITY_PING		ISDN_PARAM_VENDOR(2)


	/*
	 * Application can set this parameter to B_TRUE or B_FALSE
	 * to force activation on an interface.
	 */
#define	DBRI_PARAM_FORCE_ACTIVATION	ISDN_PARAM_VENDOR(3)

	/*
	 * Accept an F bit which is not a bipolar violation after an
	 * errored frame. Argument is ==0 (false, default) or !=0 (true).
	 */
#define	DBRI_PARAM_F	ISDN_PARAM_VENDOR(4)

	/*
	 * Number of bad frames to lose framing.  Legal values are 2 and 3.
	 * 3 is the default.
	 */
#define	DBRI_PARAM_NBF	ISDN_PARAM_VENDOR(5)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DBRIIO_H */
