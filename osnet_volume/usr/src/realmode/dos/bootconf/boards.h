/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * boards.h -- public definitions for boards routines
 */

#ifndef	_BOARDS_H
#define	_BOARDS_H

#ident "@(#)boards.h   1.23   99/05/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct board;
struct menu_list;

struct board *copy_boards(struct board *src);
extern char *format_device_id(struct board *bp, char *buf, int verbose);
int equal_boards(struct board *bp, struct board *bq);
void menu_list_boards(struct menu_list **listp, int *nlistp, int verbose);
void free_boards_list(struct menu_list *listp, int nlistp);
void free_board(struct board *bp);
void add_board(struct board *bp);
void del_board(struct board *bp);
struct board *new_board();
void free_chain_boards(struct board **head);
char *list_resources_boards(struct board *bp);

#ifdef	__cplusplus
}
#endif

#endif	/* _BOARDS_H */
