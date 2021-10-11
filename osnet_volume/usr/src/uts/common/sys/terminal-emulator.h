/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_TERMINAL_EMULATOR_H
#define	_SYS_TERMINAL_EMULATOR_H

#pragma ident	"@(#)terminal-emulator.h	1.10	99/03/02 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL
struct terminal_emulator;
int tem_init(struct terminal_emulator **, char *, cred_t *, int, int);
int tem_write(struct terminal_emulator *, unsigned char *, int, cred_t *);
int tem_polled_write(struct terminal_emulator *, unsigned char *, int);
int tem_fini(struct terminal_emulator *);
void tem_get_size(struct terminal_emulator *, int *, int *, int *, int *);
#endif _KERNEL

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TERMINAL_EMULATOR_H */
