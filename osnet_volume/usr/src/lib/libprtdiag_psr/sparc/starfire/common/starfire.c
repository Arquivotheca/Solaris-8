/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Starfire Platform specific functions.
 *
 * 	called when :
 *	machine_type == MTYPE_STARFIRE
 *
 */

#pragma ident	"@(#)starfire.c	1.2	99/10/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/dkio.h>
#include "pdevinfo.h"
#include "display.h"
#include "pdevinfo_sun4u.h"
#include "display_sun4u.h"
#include "libprtdiag.h"

/*
 * these functions will overlay the symbol table of libprtdiag
 * at runtime (starfire systems only)
 */
int	error_check(Sys_tree *tree, struct system_kstat_data *kstats);
void	display_memoryconf(Sys_tree *tree, struct grp_info *grps);
void	display_hp_fail_fault(Sys_tree *tree, struct system_kstat_data *kstats);
void	display_diaginfo(int flag, Prom_node *root, Sys_tree *tree,
				struct system_kstat_data *kstats);
void	display_mid(int mid);
void	display_pci(Board_node *);
Prom_node	*find_device(Board_node *, int, char *);


int
error_check(Sys_tree *tree, struct system_kstat_data *kstats)
{
#ifdef lint
	tree = tree;
	kstats = kstats;
#endif
	return (0);
}

void
display_memoryconf(Sys_tree *tree, struct grp_info *grps)
{
	Board_node *bnode;
	char indent_str[] = "           ";

#ifdef lint
	grps = grps;
#endif

	/* Print the header for the memory section. */
	log_printf("\n", 0);
	log_printf("=========================", 0);
	log_printf(gettext(" Memory "), 0);
	log_printf("=========================", 0);
	log_printf("\n\n", 0);

	/* Print the header for the memory section. */
	log_printf(indent_str, 0);
	log_printf("Memory Units: Size \n", 0);
	log_printf(indent_str, 0);
	log_printf("0: MB   1: MB   2: MB   3: MB\n", 0);
	log_printf(indent_str, 0);
	log_printf("-----   -----   -----   ----- \n", 0);

	/* Run thru the board and display its memory if any */
	bnode = tree->bd_list;
	while (bnode != NULL) {
		Prom_node *pnode;
		unsigned int *memsize;
		unsigned int mbyte = 1024*1024;

		/*
		 * Find the mem-unit of the board.
		 * If the board has memory, a mem-unit pnode should
		 * be there.
		 */
		pnode = dev_find_node(bnode->nodes, "mem-unit");

		if (pnode != NULL) {
			/* there is a mem-unit in the board */

			/* Print the board header */
			log_printf("Board%2d  ", bnode->board_num, 0);

			memsize = get_prop_val(find_prop(pnode, "size"));

			log_printf("   %4d    %4d    %4d    %4d \n",
				memsize[0]/mbyte, memsize[1]/mbyte,
				memsize[2]/mbyte, memsize[3]/mbyte, 0);
		}
		bnode = bnode->next;
	}
	log_printf("\n", 0);
}

void
display_hp_fail_fault(Sys_tree *tree, struct system_kstat_data *kstats)
{
#ifdef lint
	tree = tree;
	kstats = kstats;
#endif
}

void
display_diaginfo(int flag, Prom_node *root, Sys_tree *tree,
	struct system_kstat_data *kstats)
{

	char hostname[128];	/* used for starfire output */
	struct utsname uts_buf;

#ifdef lint
	flag = flag;
	root = root;
	tree = tree;
	kstats = kstats;
#endif

	/*
	 * Get hostname from system Banner
	 */
	(void) uname(&uts_buf);
	strcpy(hostname, uts_buf.nodename);

	/*
	 * We can't display diagnostic/env information for starfire.
	 * The diagnostic information may be displayed through
	 * commands in ssp.
	 */
	log_printf(gettext("\nFor diagnostic information,"), 0);
	log_printf("\n", 0);
	log_printf(
	gettext("see /var/opt/SUNWssp/adm/%s/messages on the SSP."),
		hostname, 0);
	log_printf("\n", 0);
}

void
display_mid(int mid)
{
	log_printf("  %2d     ", mid % 4, 0);
}



/*
 * display_pci
 * Display all the PCI IO cards on this board.
 */
/* ARGSUSED */
void
display_pci(Board_node *board)
{
	struct io_card *card_list = NULL;
	struct io_card card;
	void *value;
	Prom_node *pci;
	Prom_node *card_node;

	if (board == NULL)
		return;

	/* Initialize all the common information */
	card.display = 1;
	card.board = board->board_num;
	(void) sprintf(card.bus_type, "PCI");

	for (pci = dev_find_node(board->nodes, PCI_NAME); pci != NULL;
	    pci = dev_next_node(pci, PCI_NAME)) {
		char *child_name;
		char *name;
		char buf[MAXSTRLEN];
		int pci_pci_bridge = 0;

		/*
		 * If we have reached a pci-to-pci bridge node,
		 * we are one level below the 'pci' nodes level
		 * in the device tree. To get back to that level,
		 * the search should continue with the sibling of
		 * the parent or else the remaining 'pci' cards
		 * will not show up in the output.
		 */
		if (find_prop(pci, "upa-portid") == NULL) {
			if ((pci->parent->sibling != NULL) &&
				(strcmp(get_prop_val(
				find_prop(pci->parent->sibling,
				"name")), PCI_NAME) == 0))
				pci = pci->parent->sibling;
			else {
				pci = pci->parent->sibling;
				continue;
			}
		}

		/* Skip all failed nodes for now */
		if (node_failed(pci))
			continue;

		/* Fill in frequency */
		value = get_prop_val(find_prop(pci, "clock-frequency"));
		if (value == NULL)
			card.freq = -1;
		else
			card.freq = ((*(int *)value) + 500000) / 1000000;

		/* Walk through the PSYCHO children */
		card_node = pci->child;
		while (card_node != NULL) {

			/* If it doesn't have a name, skip it */
			name = (char *)get_prop_val(
				find_prop(card_node, "name"));
			if (name == NULL) {
				card_node = card_node->sibling;
				continue;
			}

			/*
			 * If this is a PCI bridge, then display its
			 * children.
			 */
			if (strcmp(name, "pci") == 0) {
				card_node = card_node->child;
				pci_pci_bridge = 1;
				continue;
			}

			/* Get the slot number for this card */
			card.slot = get_pci_device(card_node);

			if (card.slot == -1 || strstr(name, "ebus") ||
			    (strstr(name, "pci"))) {
				card_node = card_node->sibling;
				continue;
			}

			/* XXX - Don't know how to get status for PCI cards */
			card.status[0] = '\0';

			/* Get the model of this card */
			value = get_prop_val(find_prop(card_node, "model"));
			if (value == NULL)
				card.model[0] = '\0';
			else
				(void) sprintf(card.model, "%s",
					(char *)value);

			/*
			 * If we haven't figured out the frequency yet,
			 * try and get it from the card.
			 */
			value = get_prop_val(find_prop(pci, "clock-frequency"));
			if (value != NULL && card.freq == -1)
				card.freq = ((*(int *)value) + 500000)
					/ 1000000;


			value = get_prop_val(find_prop(card_node,
				"compatible"));

			if (value != NULL) {
				(void) sprintf(buf, "%s-%s", name,
					(char *)value);
			} else
				(void) sprintf(buf, "%s", name);

			name = buf;

			/* Figure out how we want to display the name */
			child_name = (char *)get_node_name(card_node->child);

			if ((card_node->child != NULL) &&
			    (child_name != NULL)) {
				value = get_prop_val(find_prop(card_node->child,
					"device_type"));
				if (value != NULL)
					(void) sprintf(card.name, "%s/%s (%s)",
						name, child_name,
						(char *)value);
				else
					(void) sprintf(card.name, "%s/%s", name,
						child_name);
			} else
				(void) sprintf(card.name, "%s", (char *)name);

			if (card.freq != -1)
				card_list = insert_io_card(card_list, &card);

			/*
			 * If we are done with the children of the pci bridge,
			 * we must continue with the remaining siblings of
			 * the pci-to-pci bridge.
			 */
			if ((card_node->sibling == NULL) && pci_pci_bridge) {
				card_node = card_node->parent->sibling;
				pci_pci_bridge = 0;
			} else
				card_node = card_node->sibling;

		}
	}

	display_io_cards(card_list);
	free_io_cards(card_list);
}

/*
 * Find the device on the current board with the requested device ID
 * and name. If this rountine is passed a NULL pointer, it simply returns
 * NULL.
 */
Prom_node *
find_device(Board_node *board, int id, char *name)
{
	Prom_node *pnode;
	int mask;

	/* find the first cpu node */
	pnode = dev_find_node(board->nodes, name);

	mask = 0x7F;
	while (pnode != NULL) {
		if ((get_id(pnode) & mask) == id)
			return (pnode);

		pnode = dev_next_node(pnode, name);
	}
	return (NULL);
}
