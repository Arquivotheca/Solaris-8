/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KBIO_H
#define	_SYS_KBIO_H

#pragma ident	"@(#)kbio.h	1.37	99/08/19 SMI"	/* SunOS4.0 1.23 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Keyboard related ioctls
 */

/*
 * See sys/kbd.h for TR_NONE (don't translate) and TR_ASCII
 * (translate to ASCII) TR_EVENT (translate to virtual input
 * device codes)
 */
#define	KIOC		('k'<<8)

#if !defined(i386) && !defined(__i386) && !defined(__ia64)
#define	KIOCTRANS	(KIOC|0)	/* set keyboard translation */
#define	KIOCGTRANS	(KIOC|5)	/* get keyboard translation */

#define	KIOCTRANSABLE	(KIOC|6) 	/* set keyboard translatability */
#define	KIOCGTRANSABLE	(KIOC|7)	/* get keyboard translatability */
#else
/*
 * For x86, these numbers conflict with KD "Xenix" ioctl numbers, so each
 * conflicting command has been offset by 30 to eliminate the conflict.
 */
#define	KIOCTRANS	(KIOC|30)	/* set keyboard translation */
#define	KIOCGTRANS	(KIOC|35)	/* get keyboard translation */

#define	KIOCTRANSABLE	(KIOC|36) 	/* set keyboard translatability */
#define	KIOCGTRANSABLE	(KIOC|37)	/* get keyboard translatability */
#endif


#define	TR_CANNOT	0	/* Cannot translate keyboard using tables */
#define	TR_CAN		1	/* Can translate keyboard using tables */

/*
 * Old-style keymap entry, for backwards compatibility only.
 */
struct	kiockey {
	int	kio_tablemask;	/* Translation table (one of: 0, CAPSMASK, */
				/* SHIFTMASK, CTRLMASK, UPMASK, */
				/* ALTGRAPHMASK, NUMLOCKMASK) */
#define	KIOCABORT1	-1	/* Special "mask": abort1 keystation */
#define	KIOCABORT2	-2	/* Special "mask": abort2 keystation */
#define	KIOCABORT1A	-3	/* Special "mask": alt abort1 keystation */
	uchar_t	kio_station;	/* Physical keyboard key station (0-127) */
	uchar_t	kio_entry;	/* Translation table station's entry */
	char	kio_string[10];	/* Value for STRING entries (null terminated) */
};

/*
 * Set kio_tablemask table's kio_station to kio_entry.
 * Copy kio_string to string table if kio_entry is between STRING and
 * STRING+15.  EINVAL is possible if there are invalid arguments.
 */
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
#define	KIOCSETKEY	(KIOC|1)
#else
/* For x86, this number conflicts with the KD "SETFKEY" ioctl number */
#define	KIOCSETKEY	(KIOC|31)
#endif
/*
 * Get kio_tablemask table's kio_station to kio_entry.
 * Get kio_string from string table if kio_entry is between STRING and
 * STRING+15.  EINVAL is possible if there are invalid arguments.
 */
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
#define	KIOCGETKEY	(KIOC|2)
#else
/* For x86, this number conflicts with the KD "GIO_SCRNMAP" ioctl number */
#define	KIOCGETKEY	(KIOC|32)
#endif

/*
 * Send the keyboard device a control command.  sys/kbd.h contains
 * the constants that define the commands.  Normal values are:
 * KBD_CMD_BELL, KBD_CMD_NOBELL, KBD_CMD_CLICK, KBD_CMD_NOCLICK.
 * Inappropriate commands for particular keyboard types are ignored.
 *
 * Since there is no reliable way to get the state of the bell or click
 * or LED (because we can't query the kdb, and also one could do writes
 * to the appropriate serial driver--thus going around this ioctl)
 * we don't provide an equivalent state querying ioctl.
 */
#define	KIOCCMD		(KIOC|8)

/*
 * Get keyboard type.  Return values are one of KB_* from sys/kbd.h,
 * e.g., KB_KLUNK, KB_VT100, KB_SUN2, KB_SUN3, KB_SUN4, KB_ASCII.
 * -1 means that the type is not known.
 */
#define	KIOCTYPE	(KIOC|9)	/* get keyboard type */

/*
 * Set flag indicating whether keystrokes get routed to /dev/console.
 */
#define	KIOCSDIRECT	(KIOC|10)

/*
 * Get flag indicating whether keystrokes get routed to /dev/console.
 */
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
#define	KIOCGDIRECT	(KIOC|11)
#else
/* For x86, this number conflicts with the KD "GIO_STRMAP" ioctl number */
#define	KIOCGDIRECT	(KIOC|41)
#endif

/*
 * New-style key map entry.
 */
struct	kiockeymap {
	int	kio_tablemask;	/* Translation table (one of: 0, CAPSMASK, */
				/*  SHIFTMASK, CTRLMASK, UPMASK, */
				/*  ALTGRAPHMASK) */
	uchar_t	kio_station;	/* Physical keyboard key station (0-127) */
	ushort_t kio_entry;	/* Translation table station's entry */
	char	kio_string[10];	/* Value for STRING entries (null terminated) */
};

/*
 * Set kio_tablemask table's kio_station to kio_entry.
 * Copy kio_string to string table if kio_entry is between STRING and
 * STRING+15.  EINVAL is possible if there are invalid arguments.
 */
#if !defined(i386) && !defined(__i386) && !defined(__ia64)
#define	KIOCSKEY	(KIOC|12)
#else
/* For x86, this number conflicts with the KD "PIO_STRMAP" ioctl number */
#define	KIOCSKEY	(KIOC|42)
#endif

/*
 * Get kio_tablemask table's kio_station to kio_entry.
 * Get kio_string from string table if kio_entry is between STRING and
 * STRING+15.  EINVAL is possible if there are invalid arguments.
 */
#define	KIOCGKEY	(KIOC|13)

/*
 * Set and get LED state.
 */
#define	KIOCSLED	(KIOC|14)
#define	KIOCGLED	(KIOC|15)

/*
 * Set and get compatibility mode.
 */
#define	KIOCSCOMPAT	(KIOC|16)
#define	KIOCGCOMPAT	(KIOC|17)

/*
 * Set and get keyboard layout.
 */
#define	KIOCSLAYOUT	(KIOC|19)
#define	KIOCLAYOUT	(KIOC|20)

/*
 * KIOCSKABORTEN:
 *
 * Enable/Disable/Alternate Keyboard abort effect (Stop/A, Break or other seq).
 * The argument is a pointer to an integer.  If the integer is zero,
 * keyboard abort is disabled, one will enable keyboard abort (hardware BREAK
 * signal), two will revert to the Alternative Break Sequence.  NB: This ioctl
 * requires root credentials and applies to serial input devices and keyboards.
 * When the Alternative Break Sequence is enabled it applies to serial input
 * devices ONLY.
 */
#define	KIOCSKABORTEN	(KIOC|21)

#define	KIOCABORTDISABLE	0	/* Disable Aborts  */
#define	KIOCABORTENABLE		1	/* Enable BREAK Signal Aborts  */
#define	KIOCABORTALTERNATE	2	/* Enable Alternative Aborts   */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KBIO_H */
