/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_LIBPRTDIAG_H
#define	_SYS_LIBPRTDIAG_H

#pragma ident	"@(#)libprtdiag.h	1.2	99/10/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* global data */
#define	PCI_DEVICE(x)   ((x  >> 11) & 0x1f)
#define	BUS_TYPE	"UPA"

int	sys_clk;  /* System clock freq. (in MHz) */

/*
 * display functions
 */
int	error_check(Sys_tree *tree, struct system_kstat_data *kstats);
int	disp_fail_parts(Sys_tree *tree);
void	display_hp_fail_fault(Sys_tree *tree, struct system_kstat_data *kstats);
void	display_diaginfo(int flag, Prom_node *root, Sys_tree *tree,
	    struct system_kstat_data *kstats);
void	resolve_board_types(Sys_tree *);
void	display_boardnum(int num);

/*
 * cpu functions
 */
void	display_cpu_devices(Sys_tree *);
void	display_mid(int mid);

/*
 * io functions
 */
Prom_node *find_pci_bus(Prom_node *, int, int);
int	get_pci_bus(Prom_node *);
int	get_pci_device(Prom_node *);
int	get_pci_to_pci_device(Prom_node *);
void	free_io_cards(struct io_card *);
struct	io_card *insert_io_card(struct io_card *, struct io_card *);
char	*fmt_manf_id(unsigned int, char *);
int	get_sbus_slot(Prom_node *);
void	display_io_devices(Sys_tree *tree);
void	display_pci(Board_node *bnode);
void	display_io_cards(struct io_card *);
void	display_ffb(Board_node *, int);
void	display_sbus(Board_node *);

/*
 * kstat functions
 */
void	read_platform_kstats(Sys_tree *tree,
	    struct system_kstat_data *sys_kstat,
	    struct bd_kstat_data *bdp, struct envctrl_kstat_data *ep);
void	read_sun4u_kstats(Sys_tree *, struct system_kstat_data *);

/*
 * memory functions
 */
void	display_memorysize(Sys_tree *tree, struct system_kstat_data *kstats,
	    struct grp_info *grps, struct mem_total *memory_total);
void	display_memoryconf(Sys_tree *tree, struct grp_info *grps);

/*
 * prom functions
 */
void	platform_disp_prom_version(Sys_tree *);
void	disp_prom_version(Prom_node *);
void	add_node(Sys_tree *, Prom_node *);
Prom_node *find_device(Board_node *, int, char *);
Prom_node *walk(Sys_tree *, Prom_node *, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LIBPRTDIAG_H */
