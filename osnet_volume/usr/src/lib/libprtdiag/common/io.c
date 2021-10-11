/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)io.c	1.2	99/10/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <kvm.h>
#include <varargs.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/openpromio.h>
#include <sys/systeminfo.h>
#include <kstat.h>
#include <libintl.h>
#include <syslog.h>
#include <sys/dkio.h>
#include "pdevinfo.h"
#include "display.h"
#include "pdevinfo_sun4u.h"
#include "display_sun4u.h"
#include "libprtdiag.h"


Prom_node *
find_pci_bus(Prom_node *node, int id, int bus)
{
	Prom_node *pnode;

	/* find the first pci node */
	pnode = dev_find_node(node, "pci");

	while (pnode != NULL) {
		int tmp_id;
		int tmp_bus;

		tmp_id = get_id(pnode);
		tmp_bus = get_pci_bus(pnode);

		if ((tmp_id == id) &&
		    (tmp_bus == bus)) {
			break;
		}

		pnode = dev_next_node(pnode, "pci");
	}
	return (pnode);
}

/*
 * get_pci_bus
 *
 * Determines the PCI bus, either A (0) or B (1). If the function cannot
 * find the bus-ranges property, it returns -1.
 */
int
get_pci_bus(Prom_node *pnode)
{
	int *value;

	/* look up the bus-range property */
	if ((value = (int *)get_prop_val(find_prop(pnode, "bus-range"))) ==
	    NULL) {
		return (-1);
	}

	if (*value == 0) {
		return (1);	/* B bus has a bus-range value = 0 */
	} else {
		return (0);
	}
}



/*
 * Find the PCI slot number of this PCI device. If no slot number can
 * be determined, then return -1.
 */
int
get_pci_device(Prom_node *pnode)
{
	void *value;

	if ((value = get_prop_val(find_prop(pnode, "assigned-addresses"))) !=
		NULL) {
		return (PCI_DEVICE(*(int *)value));
	} else {
		return (-1);
	}
}

/*
 * Find the PCI slot number of this PCI device. If no slot number can
 * be determined, then return -1.
 */
int
get_pci_to_pci_device(Prom_node *pnode)
{
	void *value;

	if ((value = get_prop_val(find_prop(pnode, "reg"))) !=
		NULL) {
		return (PCI_DEVICE(*(int *)value));
	} else {
		return (-1);
	}
}

/*
 * free_io_cards
 * Frees the memory allocated for an io card list.
 */
void
free_io_cards(struct io_card *card_list)
{
	/* Free the list */
	if (card_list != NULL) {
		struct io_card *p, *q;

		for (p = card_list, q = NULL; p != NULL; p = q) {
			q = p->next;
			free(p);
		}
	}
}


/*
 * insert_io_card
 * Inserts an io_card structure into the list.  The list is maintained
 * in order based on board number and slot number.  Also, the storage
 * for the "card" argument is assumed to be handled by the caller,
 * so we won't touch it.
 */
struct io_card *
insert_io_card(struct io_card *list, struct io_card *card)
{
	struct io_card *newcard;
	struct io_card *p, *q;

	if (card == NULL)
		return (list);

	/* Copy the card to be added into new storage */
	newcard = (struct io_card *)malloc(sizeof (struct io_card));
	if (newcard == NULL) {
		perror("malloc");
		exit(1);
	}
	(void) memcpy(newcard, card, sizeof (struct io_card));
	newcard->next = NULL;

	if (list == NULL)
	return (newcard);

	/* Find the proper place in the list for the new card */
	for (p = list, q = NULL; p != NULL; q = p, p = p->next) {
		if (newcard->board < p->board)
			break;
		if ((newcard->board == p->board) && (newcard->slot < p->slot))
			break;
	}

	/* Insert the new card into the list */
	if (q == NULL) {
		newcard->next = p;
		return (newcard);
	} else {
		newcard->next = p;
		q->next = newcard;
		return (list);
	}
}


char *
fmt_manf_id(unsigned int encoded_id, char *outbuf)
{
	union manuf manuf;

	/*
	 * Format the manufacturer's info.  Note a small inconsistency we
	 * have to work around - Brooktree has it's part number in decimal,
	 * while Mitsubishi has it's part number in hex.
	 */
	manuf.encoded_id = encoded_id;
	switch (manuf.fld.manf) {
	case MANF_BROOKTREE:
		(void) sprintf(outbuf, "%s %d, version %d", "Brooktree",
			manuf.fld.partno, manuf.fld.version);
		break;

	case MANF_MITSUBISHI:
		(void) sprintf(outbuf, "%s %x, version %d", "Mitsubishi",
			manuf.fld.partno, manuf.fld.version);
		break;

	default:
		(void) sprintf(outbuf, "JED code %d, Part num 0x%x, version %d",
			manuf.fld.manf, manuf.fld.partno, manuf.fld.version);
	}
	return (outbuf);
}


/*
 * Find the sbus slot number of this Sbus device. If no slot number can
 * be determined, then return -1.
 */
int
get_sbus_slot(Prom_node *pnode)
{
	void *value;

	if ((value = get_prop_val(find_prop(pnode, "reg"))) != NULL) {
		return (*(int *)value);
	} else {
		return (-1);
	}
}


/*
 * This routine is the generic link into displaying system IO
 * configuration. It displays the table header, then displays
 * all the SBus cards, then displays all fo the PCI IO cards.
 */
void
display_io_devices(Sys_tree *tree)
{
	Board_node *bnode;

	/*
	 * TRANSLATION_NOTE
	 * Following string is used as a table header.
	 * Please maintain the current alignment in
	 * translation.
	 */
	log_printf("\n", 0);
	log_printf("=========================", 0);
	log_printf(gettext(" IO Cards "), 0);
	log_printf("=========================", 0);
	log_printf("\n", 0);
	log_printf("\n", 0);
	bnode = tree->bd_list;
	while (bnode != NULL) {
		display_sbus(bnode);
		display_pci(bnode);
		display_ffb(bnode, 1);
		bnode = bnode->next;
	}
}

void
display_pci(Board_node *bnode)
{
#ifdef  lint
	bnode = bnode;
#endif
	/*
	 * This function is intentionally empty
	 */
}


/*
 * Print out all the io cards in the list.  Also print the column
 * headers if told to do so.
 */
void
display_io_cards(struct io_card *list)
{
	static int banner = 0; /* Have we printed the column headings? */
	struct io_card *p;

	if (list == NULL)
		return;

	if (banner == 0) {
		log_printf("     Bus   Freq\n", 0);
		log_printf("Brd  Type  MHz   Slot  "
			"Name                              "
			"Model", 0);
		log_printf("\n", 0);
		log_printf("---  ----  ----  ----  "
			"--------------------------------  "
			"----------------------", 0);
		log_printf("\n", 0);
		banner = 1;
	}

	for (p = list; p != NULL; p = p -> next) {
		log_printf("%2d   ", p->board, 0);
		log_printf("%-4s  ", p->bus_type, 0);
		log_printf("%3d   ", p->freq, 0);
		log_printf("%3d   ", p->slot, 0);
		log_printf("%-32.32s", p->name, 0);
		if (strlen(p->name) > 32)
			log_printf("+ ", 0);
		else
			log_printf("  ", 0);
		log_printf("%-22.22s", p->model, 0);
		if (strlen(p->model) > 22)
			log_printf("+", 0);
		log_printf("\n", 0);
	}
}



/*
 * Display all FFBs on this board.  It can either be in tabular format,
 * or a more verbose format.
 */
void
display_ffb(Board_node *board, int table)
{
	Prom_node *ffb;
	void *value;
	struct io_card *card_list = NULL;
	struct io_card card;

	if (board == NULL)
		return;

	/* Fill in common information */
	card.display = 1;
	card.board = board->board_num;
	(void) sprintf(card.bus_type, BUS_TYPE);
	card.freq = sys_clk;

	for (ffb = dev_find_node(board->nodes, FFB_NAME); ffb != NULL;
	    ffb = dev_next_node(ffb, FFB_NAME)) {
		if (table == 1) {
			/* Print out in table format */

			/* XXX - Get the slot number (hack) */
			card.slot = get_id(ffb);

			/* Find out if it's single or double buffered */
			(void) sprintf(card.name, "FFB");
			value = get_prop_val(find_prop(ffb, "board_type"));
			if (value != NULL)
				if ((*(int *)value) & FFB_B_BUFF)
					(void) sprintf(card.name, "FFB, "
						"Double Buffered");
				else
					(void) sprintf(card.name, "FFB, "
						"Single Buffered");

			/* Print model number */
			card.model[0] = '\0';
			value = get_prop_val(find_prop(ffb, "model"));
			if (value != NULL)
				(void) sprintf(card.model, "%s",
					(char *)value);

			card_list = insert_io_card(card_list, &card);
		} else {
			/* print in long format */
			char device[MAXSTRLEN];
			int fd = -1;
			struct dirent *direntp;
			DIR *dirp;
			union strap_un strap;
			struct ffb_sys_info fsi;

			/* Find the device node using upa-portid/portid */
			value = get_prop_val(find_prop(ffb, "upa-portid"));
			if (value == NULL)
				value = get_prop_val(find_prop(ffb, "portid"));

			if (value == NULL)
			    continue;

			(void) sprintf(device, "%s@%x", FFB_NAME,
				*(int *)value);
			if ((dirp = opendir("/devices")) == NULL)
				continue;

			while ((direntp = readdir(dirp)) != NULL) {
				if (strstr(direntp->d_name, device) != NULL) {
					(void) sprintf(device, "/devices/%s",
						direntp->d_name);
					fd = open(device, O_RDWR, 0666);
					break;
				}
			}
			(void) closedir(dirp);

			if (fd == -1)
				continue;

			if (ioctl(fd, FFB_SYS_INFO, &fsi) < 0)
				continue;

			log_printf("FFB Hardware Configuration:\n", 0);
			log_printf("-----------------------------------\n", 0);

			strap.ffb_strap_bits = fsi.ffb_strap_bits;
			log_printf("\tBoard rev: %d\n",
				(int)strap.fld.board_rev, 0);
			log_printf("\tFBC version: 0x%x\n", fsi.fbc_version, 0);
			log_printf("\tDAC: %s\n",
				fmt_manf_id(fsi.dac_version, device), 0);
			log_printf("\t3DRAM: %s\n",
				fmt_manf_id(fsi.fbram_version, device), 0);
			log_printf("\n", 0);
		}
	}

	display_io_cards(card_list);
	free_io_cards(card_list);
}


/*
 * Display all the SBus IO cards on this board.
 */
void
display_sbus(Board_node *board)
{
	struct io_card card;
	struct io_card *card_list = NULL;
	int freq;
	int card_num;
	void *value;
	Prom_node *sbus;
	Prom_node *card_node;

	if (board == NULL)
		return;

	for (sbus = dev_find_node(board->nodes, SBUS_NAME); sbus != NULL;
	    sbus = dev_next_node(sbus, SBUS_NAME)) {

		/* Skip failed nodes for now */
		if (node_failed(sbus))
			continue;

		/* Calculate SBus frequency in MHz */
		value = get_prop_val(find_prop(sbus, "clock-frequency"));
		if (value != NULL)
			freq = ((*(int *)value) + 500000) / 1000000;
		else
			freq = -1;

		for (card_node = sbus->child; card_node != NULL;
		    card_node = card_node->sibling) {
			char *model;
			char *name;
			char *child_name;

			card_num = get_sbus_slot(card_node);
			if (card_num == -1)
				continue;

			/* Fill in card information */
			card.display = 1;
			card.freq = freq;
			card.board = board->board_num;
			(void) sprintf(card.bus_type, "SBus");
			card.slot = card_num;
			card.status[0] = '\0';

			/* Try and get card status */
			value = get_prop_val(find_prop(card_node, "status"));
			if (value != NULL)
				(void) strncpy(card.status, (char *)value,
					MAXSTRLEN);

			/* XXX - For now, don't display failed cards */
			if (strstr(card.status, "fail") != NULL)
				continue;

			/* Now gather all of the node names for that card */
			model = (char *)get_prop_val(find_prop(card_node,
				"model"));
			name = get_node_name(card_node);

			if (name == NULL)
				continue;

			card.name[0] = '\0';
			card.model[0] = '\0';

			/* Figure out how we want to display the name */
			child_name = get_node_name(card_node->child);
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
			} else {
				(void) strncpy(card.name, name, MAXSTRLEN);
			}

			if (model != NULL)
				(void) strncpy(card.model, model, MAXSTRLEN);

			card_list = insert_io_card(card_list, &card);
		}
	}

	/* We're all done gathering card info, now print it out */
	display_io_cards(card_list);
	free_io_cards(card_list);
}
