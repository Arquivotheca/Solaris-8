/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDIPROPDEFS_H
#define	_SYS_DDIPROPDEFS_H

#pragma ident	"@(#)ddipropdefs.h	1.15	99/04/05 SMI"	/* SVr4.0 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ddiprops.h:	All definitions related to DDI properties.
 *		Structure definitions are private to the DDI
 *		implementation.  See also, ddipropfuncs.h
 */

/*
 * ddi_prop_op_t:	Enum for prop_op functions
 */

typedef enum {
	PROP_LEN = 0,		/* Get prop len only */
	PROP_LEN_AND_VAL_BUF,	/* Get len+val into callers buffer */
	PROP_LEN_AND_VAL_ALLOC,	/* Get len+val into alloc-ed buffer */
	PROP_EXISTS		/* Does the property exist? */
} ddi_prop_op_t;

/*
 * ddi_prop_t:	The basic item used to store software defined propeties.
 *		Note that properties are always stored by reference.
 */

typedef struct ddi_prop {
	struct ddi_prop	*prop_next;
	dev_t		prop_dev;	/* specific match/wildcard */
	char		*prop_name;	/* Property name */
	int		prop_flags;	/* See flags below */
	int		prop_len;	/* Prop length (0 == Bool. prop) */
	caddr_t		prop_val;	/* ptr to property value */
} ddi_prop_t;


/*
 * Handle passed around to encode/decode a property value.
 */
typedef struct ddi_prop_handle {
	void			*ph_data;	/* Encoded data */
	void			*ph_cur_pos;	/* encode/decode position */
	void			*ph_save_pos;	/* Save/restore position */
	uint_t			ph_size;	/* Size of encoded data */
	uint_t			ph_flags;	/* See below */
	struct prop_handle_ops	*ph_ops;	/* Encode/decode routines */
} prop_handle_t;

/*
 * Property handle encode/decode ops
 */
typedef struct prop_handle_ops {
	int (*op_prop_int)(prop_handle_t *ph, uint_t cmd, int *data);
	int (*op_prop_str)(prop_handle_t *ph, uint_t cmd, char *data);
	int (*op_prop_bytes)(prop_handle_t *ph, uint_t cmd,
				uchar_t *data, uint_t size);
} prop_handle_ops_t;

/*
 * Data passed back to driver.  The driver gets a pointer to driver_data.
 * When we get it back we do negative indexing to find the size and free
 * routine to call
 */
struct prop_driver_data {
	size_t	pdd_size;
	void	(*pdd_prop_free)(struct prop_driver_data *);
};


/*
 * Macros to call the integer/string/byte OBP 1275 operators
 */
#define	DDI_PROP_INT(ph, cmd, data)		\
		(*(ph)->ph_ops->op_prop_int)((ph), (cmd), (data))
#define	DDI_PROP_STR(ph, cmd, data)		\
		(*(ph)->ph_ops->op_prop_str)((ph), (cmd), (data))
#define	DDI_PROP_BYTES(ph, cmd, data, size)	\
		(*(ph)->ph_ops->op_prop_bytes)((ph), (cmd), (data), (size))

/*
 * Property handle commands
 */
typedef enum {
	DDI_PROP_CMD_GET_ESIZE,		/* Get encoded size of data  */
	DDI_PROP_CMD_GET_DSIZE,		/* Get decoded size of data */
	DDI_PROP_CMD_DECODE,		/* Decode the current data */
	DDI_PROP_CMD_ENCODE,		/* Encode the current data */
	DDI_PROP_CMD_SKIP		/* Skip the current data */
} ddi_prop_cmd_t;

/*
 * Return values from property handle encode/decode ops
 * Positive numbers are used to return the encoded or
 * decode size of the object, so an ok return must be positive,
 * and all error returns negative.
 */
typedef enum {
	DDI_PROP_RESULT_ERROR = -2,	/* error in encoding/decoding data */
	DDI_PROP_RESULT_EOF,		/* end of data reached */
	DDI_PROP_RESULT_OK		/* if >= to DDI_PROP_RESULT_OK, */
					/* operation was successful */
} ddi_prop_result_t;

/* 1275 property cell */
typedef uint32_t prop_1275_cell_t;

/* Length of a 1275 property cell */
#define	PROP_1275_CELL_SIZE	sizeof (prop_1275_cell_t)
#define	CELLS_1275_TO_BYTES(n)	((n) * PROP_1275_CELL_SIZE)
#define	BYTES_TO_1275_CELLS(n)	((n) / PROP_1275_CELL_SIZE)

/*
 * Property handle flags
 */
#define	PH_FROM_PROM	0x01	/* Property came from the prom */

/*
 * Return values from property functions:
 */

#define	DDI_PROP_SUCCESS	0
#define	DDI_PROP_NOT_FOUND	1	/* Prop not defined */
#define	DDI_PROP_UNDEFINED	2	/* Overriden to undefine a prop */
#define	DDI_PROP_NO_MEMORY	3	/* Unable to allocate/no sleep */
#define	DDI_PROP_INVAL_ARG	4	/* Invalid calling argument */
#define	DDI_PROP_BUF_TOO_SMALL	5	/* Callers buf too small */
#define	DDI_PROP_CANNOT_DECODE	6	/* Could not decode prop */
#define	DDI_PROP_CANNOT_ENCODE	7	/* Could not encode prop */
#define	DDI_PROP_END_OF_DATA	8	/* Prop found in an encoded format */

/*
 * used internally in the framework only
 */
#define	DDI_PROP_FOUND_1275	255	/* Prop found in OPB 1275 format */

/*
 * Size of a 1275 int in bytes
 */
#define	PROP_1275_INT_SIZE	4

/*
 * Property flags:
 */

#define	DDI_PROP_DONTPASS	0x0001	/* Don't pass request to parent */
#define	DDI_PROP_CANSLEEP	0x0002	/* Memory allocation may sleep */

/*
 * Used internally by the DDI property rountines and masked in DDI(9F)
 * interfaces...
 */

#define	DDI_PROP_SYSTEM_DEF	0x0004	/* System defined property */

/*
 * Used in framework only, to inhibit certain pre-defined s/w property
 * names from coming from the prom.
 */
#define	DDI_PROP_NOTPROM	0x0008	/* Don't look at prom properties */

/*
 * Used interally by the DDI property routines to implement the old
 * depricated functions with the new functions
 */
#define	DDI_PROP_DONTSLEEP	0x0010	/* Memory allocation may not sleep */
#define	DDI_PROP_STACK_CREATE	0x0020	/* Do a LIFO stack of properties */
#define	DDI_PROP_UNDEF_IT	0x0040	/* Undefine a property */
#define	DDI_PROP_HW_DEF		0x0080	/* Hardware defined property */

/*
 * Type of data property contains
 */
#define	DDI_PROP_TYPE_INT		0x0100
#define	DDI_PROP_TYPE_STRING		0x0200
#define	DDI_PROP_TYPE_BYTE		0x0400
#define	DDI_PROP_TYPE_COMPOSITE		0x0800

#define	DDI_PROP_TYPE_ANY		(DDI_PROP_TYPE_INT	|	\
					DDI_PROP_TYPE_STRING	|	\
					DDI_PROP_TYPE_BYTE	|	\
					DDI_PROP_TYPE_COMPOSITE)

#define	DDI_PROP_TYPE_MASK		(DDI_PROP_TYPE_INT	|	\
					DDI_PROP_TYPE_STRING	|	\
					DDI_PROP_TYPE_BYTE	|	\
					DDI_PROP_TYPE_COMPOSITE)

/*
 * DDI_DEV_T_NONE:	When creating, property is not associated with
 *			particular dev_t.
 * DDI_DEV_T_ANY:	Wildcard dev_t when searching properties.
 * DDI_MAJOR_T_UNKNOWN	Used when a driver does not know its dev_t during
 *			a property create.
 */

#define	DDI_DEV_T_NONE	((dev_t)-1)
#define	DDI_DEV_T_ANY	((dev_t)-2)
#define	DDI_MAJOR_T_UNKNOWN	((major_t)0)

/*
 * Some DDI property names...
 */

/*
 * One of the following boolean properties shall be defined in the
 * root node, and defines the addressing mode understood by the root
 * node of the implementation....
 */

#define	DDI_RELATIVE_ADDRESSING		"relative-addressing"
#define	DDI_GENERIC_ADDRESSING		"generic-addressing"

/*
 * Common property encoded data search routine.  Returns the encoded data
 * in valuep.  Match is done on dip, dev, data type (in flags), and name.
 */
int ddi_prop_search_common(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op,
    uint_t flags, char *name, void *valuep, uint_t *lengthp);


/*
 * Property debugging support in kernel...
 */

/*
 * Property debugging support...  Be careful about enabling this when
 * you are tipping in to the console.  Undefine PROP_DEBUG to remove
 * all support from the code. (c.f. autoconf.c and zs_common.c)
 *
 * It does no good to enable this if the rest of the kernel was built with
 * this disabled (specifically, the core kernel module.)
 *
 * #define	DDI_PROP_DEBUG	1
 */

#ifdef	DDI_PROP_DEBUG
#define	ddi_prop_printf	if (ddi_prop_debug_flag != 0) printf

/*
 * Returns prev value of debugging flag, non-zero enables debug printf's
 */

int ddi_prop_debug(int enable);

#endif	/* DDI_PROP_DEBUG */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DDIPROPDEFS_H */
