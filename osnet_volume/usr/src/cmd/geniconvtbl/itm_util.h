/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ITM_UTIL_H
#define	_ITM_UTIL_H

#pragma ident	"@(#)itm_util.h	1.4	99/10/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "iconv_tm.h"
#include "itmcomp.h"


/*
 * function prototype
 */

void		itm_def_process(itm_data_t *);

itmc_obj_t	*direction_unit(itmc_ref_t *, itm_data_t *,
				itmc_action_t *, itm_data_t *);

itm_tbl_hdr_t	*obj_table(itm_type_t, itm_data_t *,
				itmc_obj_t *, itm_size_t);
itmc_ref_t	*obj_register(itm_type_t, itm_data_t *,
				void *, size_t, itm_place_t *, itm_type_t);
itmc_obj_t	*obj_list_append(itmc_obj_t *, itmc_obj_t *);

/* conditions: range, escape sequence */
itm_tbl_hdr_t	*range_table(itm_data_t *, itmc_obj_t *);
itm_tbl_hdr_t	*escseq_table(itm_data_t *, itmc_obj_t *);

/* action: map, operation */
itm_tbl_hdr_t	*map_table(itm_data_t *, itmc_map_t *, itmc_map_attr_t *);
itmc_map_t	*map_list_append(itmc_map_t *, itmc_map_t *);
itmc_obj_t	*op_self(itm_op_type_t);
itmc_obj_t	*op_unary(itm_op_type_t, void *, size_t);
itmc_obj_t	*op_unit(itm_op_type_t,
			void *, size_t, void *, size_t, void *, size_t);
itmc_obj_t	*op_self_num(itm_op_type_t, itm_num_t);

/* expressions */
itm_expr_t	*expr_self_num(itm_expr_type_t, itm_num_t);
itm_expr_t	*expr_self(itm_expr_type_t, itm_data_t *);
itm_expr_t	*expr_unary(itm_expr_type_t, itm_expr_t *);
itm_expr_t	*expr_binary(itm_expr_type_t, itm_expr_t *, itm_expr_t *);
itm_expr_t	*expr_binary2(itm_expr_type_t, itm_expr_t *, itm_expr_t *);
itm_expr_t	*expr_assign(itm_expr_type_t, itm_data_t *, itm_expr_t *);
itm_expr_t	*expr_seq_to_int(itm_expr_t *);


extern int	data_compare(const itm_data_t *, const itm_data_t *);
extern char    	*dense_enc_index_to_byte_seq(long, long,
					unsigned char *, unsigned char *);

#define	OBJ_REG_HEAD	(0)
#define	OBJ_REG_TAIL	(1)

#ifdef	__cplusplus
}
#endif

#endif /* !_ITM_UTIL_H */
