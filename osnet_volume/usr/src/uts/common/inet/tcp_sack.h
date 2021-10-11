/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_SACK_H
#define	_INET_SACK_H

#pragma ident	"@(#)tcp_sack.h	1.1	97/12/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Maximum num of receiver's SACK blocks */
#define	MAX_SACK_BLK	5

/* Receiver's SACK blk structure */
typedef struct sack_blk
{
	tcp_seq	begin;
	tcp_seq	end;
} sack_blk_t;

/* Sender's notsack'ed blk structure */
typedef struct notsack_blk
{
	struct notsack_blk	*next;
	tcp_seq			begin;
	tcp_seq			end;
	uint32_t		sack_cnt; /* Dup SACK count */
} notsack_blk_t;


/* SACK information in the tcp_t structure. */
typedef struct
{
	int32_t	tcp_pipe;	/* # of bytes in network */
	tcp_seq	tcp_fack;	/* highest sack'ed seq num */
	tcp_seq	tcp_sack_snxt;	/* next seq num to be rexmited using SACK. */

	int32_t	tcp_max_sack_blk; /* max # of SACK info blk in a segment */
	int32_t	tcp_num_sack_blk; /* num of blks in sack list */
	sack_blk_t	tcp_sack_list[MAX_SACK_BLK]; /* the sack list */

	/* num of blks in notsack list */
	int32_t		tcp_num_notsack_blk;
	/* # of bytes represented in blks in notsack list */
	uint32_t	tcp_cnt_notsack_list;
	/* the notsack list */
	notsack_blk_t	*tcp_notsack_list;
} tcp_sack_info_t;

extern void tcp_sack_insert(sack_blk_t *, tcp_seq, tcp_seq, int32_t *);
extern void tcp_sack_remove(sack_blk_t *, tcp_seq, int32_t *);
extern void tcp_notsack_insert(notsack_blk_t **, tcp_seq, tcp_seq,
    int32_t *, uint32_t *);
extern void tcp_notsack_remove(notsack_blk_t **, tcp_seq, int32_t *,
    uint32_t *);
extern void tcp_notsack_update(notsack_blk_t **, tcp_seq, tcp_seq,
    int32_t *, uint32_t *);


/*
 * Macro to remove all the notsack'ed blks in sender.
 *
 * Param:
 * notsack_blk_t *head: pointer to the head of the list of notsack'ed blks.
 */
#define	TCP_NOTSACK_REMOVE_ALL(head) \
{ \
	notsack_blk_t *prev, *tmp; \
	tmp = (head); \
	do { \
		prev = tmp; \
		tmp = tmp->next; \
		kmem_free(prev, sizeof (notsack_blk_t)); \
	} while (tmp != NULL); \
	(head) = NULL; \
}


#ifdef	__cplusplus
}
#endif

#endif	/* _INET_SACK_H */
