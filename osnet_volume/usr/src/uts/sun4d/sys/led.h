/*
 * Copyright (c) 1988, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LED_H
#define	_SYS_LED_H

#pragma ident	"@(#)led.h	1.8	98/01/06 SMI"	/* SunOS-4.1 1.9 */

#ifdef	__cplusplus
extern "C" {
#endif

extern void led_init(void);
extern void led_blink_all(void);
extern void led_set_cpu(uchar_t cpu_id, uchar_t pattern);
extern int led_get_ecsr(int cpu_id);
extern void led_set_ecsr(int cpu_id, uchar_t pattern);

#define	LED_CPU_RESUME	0x0

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LED_H */
