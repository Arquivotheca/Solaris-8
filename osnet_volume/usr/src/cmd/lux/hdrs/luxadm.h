/*
 * Copyright 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * luxadm.h
 *
 * External functions and global variables needed for PHOTON
 */

/*
 * I18N message number ranges
 *  This file: 13500 - 13999
 *  Shared common messages: 1 - 1999
 */

#ifndef	_LUXADM_H
#define	_LUXADM_H

#pragma ident	"@(#)luxadm.h	1.13	99/06/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/* External functions */
void	fc_update(unsigned, unsigned, char *);
void	fcal_update(unsigned, char *);
int	q_qlgc_update(unsigned, char *);
int	setboot(unsigned, unsigned, char *);
int	sysdump(int);
int	h_insertSena_fcdev();
int	hotplug(int, char **, int, int);
int	hotplug_e(int, char **, int, int);
/* SSA and RSM */
int	p_download(char *, char *, int, int, uchar_t *);
void	ssa_fast_write(char *);
void	ssa_perf_statistics(char *);
void	ssa_cli_start(char **, int);
void	ssa_cli_stop(char **, int);
void	ssa_cli_display_config(char **argv, char *, int, int, int);
void	cli_display_envsen_data(char **, int);
int	p_sync_cache(char *);
int	p_purge(char *);
void	led(char **, int, int);
void	alarm_enable(char **, int, int);
void	alarm_set(char **, int);
void	power_off(char **, int);
char 	*get_physical_name(char *);

/* SSA LIB environment sense */
int	scsi_get_envsen_data(int, char *, int);
int	scsi_put_envsen_data(int, char *, int);

/* hotplug */
void	print_errString(int, char *);
int	print_devState(char *, char *, int, int, int);
void	print_dev_state(char *, int);
void	print_bus_state(char *, int);
int	dev_handle_insert(char *, int);
int	dev_handle_remove(char *, int);
int	dev_handle_replace(char *, int);


#ifdef	__cplusplus
}
#endif

#endif	/* _LUXADM_H */
