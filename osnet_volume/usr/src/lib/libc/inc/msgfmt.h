/*
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MSGFMT_H
#define	_MSGFMT_H

#pragma ident	"@(#)msgfmt.h	1.1	98/03/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *
 *		+-------------------------------+
 *		| (int) middle message id	|
 *		+-------------------------------+
 *		| (int) total # of messages	|
 *		+-------------------------------+
 *		| (int) total msgid length	|
 *		+-------------------------------+
 *		| (int) total msgstr length	|
 *		+-------------------------------+
 *		| (int) size of msg_struct size	|
 *		+-------------------------------+
 *		+-------------------------------+
 *		| (int) less			|
 *		+-------------------------------+
 *		| (int) more			|
 *		+-------------------------------+
 *		| (int) msgid offset		|
 *		+-------------------------------+
 *		| (int) msgstr offset		|
 *		+-------------------------------+
 *			................
 *		+-------------------------------+
 *		| (variable str) msgid		|
 *		+-------------------------------+
 *		| (variable str) msgid		|
 *		+-------------------------------+
 *			................
 *		+-------------------------------+
 *		| (variable str) msgid		|
 *		+-------------------------------+
 *		+-------------------------------+
 *		| (variable str) msgstr		|
 *		+-------------------------------+
 *		| (variable str) msgstr		|
 *		+-------------------------------+
 *			................
 *		+-------------------------------+
 *		| (variable str) msgstr		|
 *		+-------------------------------+
 */

struct msg_info {
	int	msg_mid;			/* middle message id */
	int	msg_count;			/* total # of messages */
	int	str_count_msgid;	/* total msgid length */
	int	str_count_msgstr;	/* total msgstr length */
	int	msg_struct_size;	/* size of msg_struct_size */
};

struct msg_struct {
	int	less;				/* index of left leaf */
	int	more;				/* index of right leaf */
	int	msgid_offset;		/* msgid offset */
	int msgstr_offset;		/* msgstr offset */
};

#define	MSG_STRUCT_SIZE		(sizeof (struct msg_struct))

/*
 * The following is the size of the old msg_struct used be defined
 * in usr/src/cmd/msgfmt/msgfmt.c.
 * Old msg_struct contained:
 * struct msg_struct {
 *		char	*msgid;
 *		char	*msgstr;
 *		int	msgid_offset;
 *		int	msgstr_offset;
 *		struct msg_struct	*next;
 * };
 */
#define	OLD_MSG_STRUCT_SIZE	20

#define	LEAFINDICATOR		-99

#ifdef	__cplusplus
}
#endif

#endif /* _MSGFMT_H */
