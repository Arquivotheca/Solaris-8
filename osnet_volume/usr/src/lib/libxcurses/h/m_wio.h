/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)m_wio.h 1.1	96/01/17 SMI"

/*
 * m_wio.h
 *
 * Wide I/O Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 *
 * $Header: /rd/h/rcs/m_wio.h 1.3 1995/10/02 15:36:15 ant Exp $
 */

#ifndef __M_WIO_H__
#define __M_WIO_H__

#include <stdlib.h>
#include <wchar.h>

typedef struct {
        /* Public. */
        void *object;                   /* I/O object (normally a stream). */
        int (*get)(void *);             /* Get byte from input object. */
        int (*put)(int, void *);        /* Put byte to output object. */
        int (*unget)(int, void *);      /* Push byte onto input object. */
        int (*iseof)(void *);           /* Eof last read? */
        int (*iserror)(void *);         /* Error last read/write? */
        void (*reset)(void *);		/* Reset error flags. */

        /* Private. */
        int _next;
        int _size;
        mbstate_t _state;
        unsigned char _mb[MB_LEN_MAX];
} t_wide_io;

extern wint_t m_wio_get(t_wide_io *);
extern int m_wio_put(int, t_wide_io *);

#endif /* __M_WIO_H__ */
