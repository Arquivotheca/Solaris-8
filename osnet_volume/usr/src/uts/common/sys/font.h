/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992, 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_FONT_H
#define	_SYS_FONT_H

#pragma ident	"@(#)font.h	1.9	99/03/02 SMI"

#ifdef __cplusplus
extern "C" {
#endif

struct font {
	short	width;
	short	height;
	unchar	*char_ptr[256];
	void	*image_data;
	u_int	image_data_size;
};

typedef	struct  bitmap_data {
	short		width;
	short		height;
	unsigned char	*image;
	int		image_size;
	unsigned char	**encoding;
	int		encoding_size;
} bitmap_data_t;

struct fontlist {
	char		*name;
	bitmap_data_t	*data;
	bitmap_data_t   *(*fontload)(char *);
};

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_FONT_H */
