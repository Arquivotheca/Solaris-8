/*
 * types used internally to DL library
 */

struct dl_descriptor {
   int		openflag;
   int		error;
   dl_info_t	info;
};

#if !defined(NULL)
#define NULL (0)
#endif
