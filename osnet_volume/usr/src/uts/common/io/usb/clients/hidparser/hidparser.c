/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)hidparser.c	1.5	99/10/22 SMI"

#include <sys/usb/usba.h>
#include <sys/usb/usba/usba_types.h>
#include <sys/usb/usba/usba_impl.h>
#include <sys/usb/clients/hid/hid.h>
#include <sys/usb/clients/hidparser/hidparser.h>
#include <sys/usb/clients/hid/hidvar.h>
#include <sys/usb/clients/hidparser/hid_parser_driver.h>
#include <sys/usb/clients/hidparser/hidparser_impl.h>

/*
 * hidparser : Parser to generate parse tree for Report Deccriptors
 * in HID devices.
 */

static uint_t hparser_errmask = (uint_t)PRINT_MASK_ALL;
static uint_t  hparser_errlevel = (uint_t)USB_LOG_L1;
static usb_log_handle_t hparser_log_handle;

/*
 * Array used to store corresponding strings for the
 * different item types for debugging.
 */
char		*items[500];	/* Print items */

/*
 * modload support
 */
extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc	= {
	&mod_miscops,	/* Type	of module */
	"HID PARSER 1.5"
};

static struct modlinkage modlinkage = {
	MODREV_1, (void	*)&modlmisc, NULL
};

_init(void)
{
	int rval = mod_install(&modlinkage);

	if (rval == 0) {
		hparser_log_handle = usb_alloc_log_handle(
			NULL, "hidparser", &hparser_errlevel,
			&hparser_errmask, NULL, NULL, 0);
	}

	return (rval);
}


_fini()
{
	int rval = mod_remove(&modlinkage);

	if (rval == 0) {
		usb_free_log_handle(hparser_log_handle);
	}

	return (rval);
}


_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}



/*
 * These functions are used internally in the parser.
 * local declarations
 */
static void		hidparser_scan(hidparser_tok_t	*); /* The scanner */
static int		hidparser_Items(entity_item_t **, hidparser_tok_t *);
static int		hidparser_Temp2(hidparser_tok_t *);
static int		hidparser_LocalItem(hidparser_tok_t *);
static int		hidparser_LocalItemList(hidparser_tok_t *);
static int		hidparser_GlobalItem(hidparser_tok_t *);
static int		hidparser_ItemList(entity_item_t **,
					hidparser_tok_t *);
static int		hidparser_ReportDescriptor(entity_item_t **,
					hidparser_tok_t *);
static int		hidparser_ReportDescriptorDash(entity_item_t **,
					hidparser_tok_t *);
static int		hidparser_MainItem(entity_item_t **,
					hidparser_tok_t *);

static void		hidparser_free_attribute_list(entity_attribute_t *);
static entity_item_t	*hidparser_allocate_entity(hidparser_tok_t	*);
static void		hidparser_add_attribute(hidparser_tok_t	*);
static entity_attribute_t	*hidparser_cp_attribute_list(
				entity_attribute_t *);
static entity_attribute_t 	*hidparser_find_attribute_end(
				entity_attribute_t *);
static entity_attribute_t	*hidparser_alloc_attrib_list(int);
static	entity_item_t		*hidparser_find_item_end(entity_item_t *);
static void			hidparser_report_err(int, int,
					int, int, char *);
static int			hidparser_isvalid_item(int);
static entity_attribute_t	*hidparser_lookup_attribute(entity_item_t *,
					int);
static void			hidparser_global_err_check(entity_item_t *);
static void			hidparser_local_err_check(entity_item_t *);
static void			hidparser_mainitem_err_check(entity_item_t *);
static unsigned int		hidparser_find_unsigned_val(
					entity_attribute_t *);
static int			hidparser_find_signed_val(
					entity_attribute_t *);
static void			hidparser_check_correspondence(
					entity_item_t *, int, int, int,
					int, char *, char *);
static void			hidparser_check_minmax_val(entity_item_t *,
					int, int, int, int);
static void			hidparser_check_minmax_val_signed(
					entity_item_t *,
					int, int, int, int);
static void			hidparser_error_delim(entity_item_t *, int);
static int			hidparser_get_usage_attribute_report_des(
					entity_item_t *,
					uint32_t, uint32_t, uint32_t,
					uint32_t, uint32_t, int32_t *);
static int			hidparser_get_packet_size_report_des(
					entity_item_t *, uint32_t, uint32_t,
					uint32_t *);
static int			hidparser_get_main_item_data_descr_main(
					entity_item_t *, uint32_t,
					uint32_t, uint32_t, uint32_t,
					int32_t	*);
static void			hidparser_print_entity(
					entity_item_t *entity,
					int indent_level);
static void			hidparser_print_attributes(
					entity_attribute_t *attribute,
					char *ident_space);
static int			hidparser_main(unsigned char *, size_t,
					entity_item_t **);
static void			hidparser_initialize_items();
static void			hidparser_free_report_descr_handle(
					entity_item_t *);
static int			hidparser_print_report_descr_handle(
					entity_item_t	*handle,
					int		indent_level);



/*
 * The hidparser_lookup_first(N) of a non terminal N is stored as an array of
 * integer tokens, terminated by 0. Right now there is only one element.
 */
static hidparser_terminal_t	first_Items[] = {
	R_ITEM_USAGE_PAGE, R_ITEM_LOGICAL_MINIMUM, R_ITEM_LOGICAL_MAXIMUM, \
	R_ITEM_PHYSICAL_MINIMUM, R_ITEM_PHYSICAL_MAXIMUM, R_ITEM_UNIT, \
	R_ITEM_EXPONENT, R_ITEM_REPORT_SIZE, R_ITEM_REPORT_COUNT, \
	R_ITEM_REPORT_ID, \
	R_ITEM_USAGE, R_ITEM_USAGE_MIN, R_ITEM_USAGE_MAX, \
	R_ITEM_DESIGNATOR_INDEX, \
	R_ITEM_DESIGNATOR_MIN, R_ITEM_STRING_INDEX, R_ITEM_STRING_MIN, \
	R_ITEM_STRING_MAX, \
	R_ITEM_SET_DELIMITER, \
	0

};


/*
 * Each nonterminal is represented by a function. In a top down parser,
 * whenever a non terminal is encountered on the state diagram, the
 * corresponding function is called. Because of the grammer, there is NO
 * backtracking. If there is an error in the middle, the parser returns
 * HIDPARSER_FAILURE
 */

static hidparser_terminal_t *hid_first_list[] = {
				first_Items
};

/*
 * hidparser_parse_report_descriptor :
 *	Calls the main parser routine
 */
int
hidparser_parse_report_descriptor(unsigned char *descriptor, size_t size,
				usb_hid_descr_t *hid_descriptor,
				hidparser_handle_t *parse_handle)

{
	int	error = 0;

	entity_item_t	*root;

	hidparser_initialize_items();

	error = hidparser_main(descriptor, size, &root);

	if (error == 1) {
		return (HIDPARSER_FAILURE);
	} else {
		*parse_handle = (hidparser_handle_t)kmem_zalloc(
				sizeof (hidparser_handle), 0);
		if (*parse_handle == NULL) {
			cmn_err(CE_PANIC, "kmem_zalloc() failed");
		}
		(*parse_handle)->hidparser_handle_hid_descr = hid_descriptor;
		(*parse_handle)->hidparser_handle_parse_tree = root;
		return (HIDPARSER_SUCCESS);
	}
}


/*
 * hidparser_free_report_descriptor_handle :
 *	Frees the parse_handle which consists of a pointer to the parse
 *	tree and a pointer to the Hid descriptor structure
 */
int
hidparser_free_report_descriptor_handle(hidparser_handle_t parse_handle)
{
	if (parse_handle != NULL) {
		hidparser_free_report_descr_handle(
				parse_handle->hidparser_handle_parse_tree);
		if (parse_handle != NULL)
			kmem_free(parse_handle, sizeof (hidparser_handle));
	}

	/* Always return SUCCESS, since kmem_free is of type void */
	return (HIDPARSER_SUCCESS);
}

/*
 * hidparser_get_country_code :
 *	Return the bCountryCode from the Hid Descriptor
 *	to the hid module.
 */
int
hidparser_get_country_code(hidparser_handle_t parser_handle,
			uint16_t *country_code)
{
	if ((parser_handle == NULL) ||
		(parser_handle->hidparser_handle_hid_descr == NULL))
		return (HIDPARSER_FAILURE);

	*country_code =
		parser_handle->hidparser_handle_hid_descr->bCountryCode;
	return (HIDPARSER_SUCCESS);
}

/*
 * hidparser_get_packet_size :
 *	Get the packet size(sum of REPORT_SIZE * REPORT_COUNT)
 *	corresponding to a report id and an item type
 */
int
hidparser_get_packet_size(hidparser_handle_t parser_handle,
				uint_t report_id,
				uint_t main_item_type,
				uint_t *size)
{

	if ((parser_handle == NULL) ||
		(parser_handle->hidparser_handle_parse_tree == NULL))
		return (HIDPARSER_FAILURE);

	*size = 0;

	return (hidparser_get_packet_size_report_des(
		parser_handle->hidparser_handle_parse_tree,
		report_id, main_item_type, size));
}

/*
 * hidparser_get_packet_size_report_des :
 *	Get the packet size(sum of REPORT_SIZE * REPORT_COUNT)
 *	corresponding to a report id and an item type
 */
int
hidparser_get_packet_size_report_des(entity_item_t *parser_handle,
				uint32_t report_id,
				uint32_t main_item_type,
				uint32_t *size)
{
	entity_item_t *current = parser_handle;
	entity_attribute_t *attribute;
	unsigned char temp;
	int foundsize, foundcount, right_report_id, foundreportid;

	while (current) {
		if (current->entity_item_type == R_ITEM_COLLECTION) {
			(void) hidparser_get_packet_size_report_des(
					current->info.child,
					report_id, main_item_type, size);
		} else if (current->entity_item_type == main_item_type) {
			attribute = current->entity_item_attributes;

			temp = 1;
			foundsize = 0;
			foundcount = 0;

			foundreportid = 0;
			right_report_id = 0;
			while (attribute != NULL) {
				if (attribute->entity_attribute_tag ==
					R_ITEM_REPORT_ID) {
					foundreportid = 1;
					if (attribute->
						entity_attribute_value[0]
						== report_id)
						right_report_id = 1;
				}
				if (attribute->entity_attribute_tag ==
					R_ITEM_REPORT_SIZE) {
					foundsize = 1;
					temp = temp *
					attribute->entity_attribute_value[0];
					if (foundcount == 1)
						break;
				} else if (attribute->entity_attribute_tag ==
					    R_ITEM_REPORT_COUNT) {
					foundcount = 1;
					temp = temp *
					attribute->entity_attribute_value[0];
					if (foundsize == 1)
						break;
				}
				attribute = attribute->entity_attribute_next;
			} /* end while */

			if (foundreportid) {
				if (right_report_id)
					*size = *size + temp;
			} else {
				*size = *size + temp;
			}
			foundreportid = 0;
			right_report_id = 0;
		} /* end else if */

		current = current->entity_item_right_sibling;
	} /* end while current */

	return (HIDPARSER_SUCCESS);
}

/*
 * hidparser_get_usage_attribute :
 *	Get the attribute value corresponding to a particular
 *	report id, main item and usage
 */
int
hidparser_get_usage_attribute(hidparser_handle_t parser_handle,
				uint_t report_id,
				uint_t main_item_type,
				uint_t usage_page,
				uint_t usage,
				uint_t usage_attribute,
				int *usage_attribute_value)
{
	return (hidparser_get_usage_attribute_report_des(
			parser_handle->hidparser_handle_parse_tree,
			report_id, main_item_type, usage_page,
			usage, usage_attribute, usage_attribute_value));
}

/*
 * hidparser_get_usage_attribute_report_des :
 *	Called by the wrapper function hidparser_get_usage_attribute()
 */
static int
hidparser_get_usage_attribute_report_des(entity_item_t *parser_handle,
				uint_t report_id,
				uint_t main_item_type,
				uint_t usage_page,
				uint_t usage,
				uint_t usage_attribute,
				int *usage_attribute_value)

{
	entity_item_t *current = parser_handle;
	entity_attribute_t *attribute;
	unsigned char found_page = 0, found_ret_value = 0, found_usage = 0;


	if (usage == 0) found_usage = 1;
	if (current->entity_item_type == R_ITEM_COLLECTION) {

		return (hidparser_get_usage_attribute_report_des(
						current->info.child,
						report_id,
						main_item_type, usage_page,
						usage, usage_attribute,
						usage_attribute_value));
	} else if (current->entity_item_type == main_item_type) {
		/* Match Item Type */
		attribute = current->entity_item_attributes;

		while (attribute != NULL) {
			if ((attribute->entity_attribute_tag ==
				R_ITEM_USAGE) &&
				(attribute->entity_attribute_value[0] ==
				usage)) {
				found_usage = 1;
				if (found_usage && found_page &&
					found_ret_value)
					return (HIDPARSER_SUCCESS);
			}
			if ((attribute->entity_attribute_tag ==
				R_ITEM_USAGE_PAGE) &&
				(attribute->entity_attribute_value[0] ==
				usage_page)) {
				/* Match Usage Page */
				found_page = 1;
				if (found_usage && found_page &&
					found_ret_value)
					return (HIDPARSER_SUCCESS);
			} else if (attribute->entity_attribute_tag ==
					usage_attribute) {
				/* Match attribute */
				found_ret_value = 1;
				*usage_attribute_value =
				attribute->entity_attribute_value[0];
				if (found_usage && found_page &&
					found_ret_value)
					return (HIDPARSER_SUCCESS);
			}
			attribute = attribute->entity_attribute_next;
		}
		found_ret_value = 0;
		if (current->entity_item_right_sibling != NULL) {
			found_page = 0;
			found_usage = 0;

			return (hidparser_get_usage_attribute_report_des(
					current->entity_item_right_sibling,
						report_id,
						main_item_type, usage_page,
						usage, usage_attribute,
						usage_attribute_value));
		}
		else
			return (HIDPARSER_NOT_FOUND);
	}

	else {
		found_ret_value = 0;
		if (current->entity_item_right_sibling != NULL) {
			found_page = 0;
			found_usage = 0;

			return (hidparser_get_usage_attribute_report_des(
					current->entity_item_right_sibling,
						report_id,
						main_item_type, usage_page,
						usage, usage_attribute,
						usage_attribute_value));
		}
		else
			return (HIDPARSER_NOT_FOUND);
	}
}

/*
 * hidparser_get_main_item_data_descr :
 *	Get the data value corresponding to a particular
 *	Main Item (Input, Output, Feature)
 */

int
hidparser_get_main_item_data_descr(
			hidparser_handle_t	parser_handle,
			uint_t		report_id,
			uint_t		main_item_type,
			uint_t		usage_page,
			uint_t		usage,
			int		*main_item_descr_value)
{
	return hidparser_get_main_item_data_descr_main(
			parser_handle->hidparser_handle_parse_tree,
			report_id, main_item_type, usage_page, usage,
			main_item_descr_value);
}

/*
 * hidparser_get_main_item_data_descr_main :
 *	Called by the wrapper function hidparser_get_main_item_data_descr()
 */
static int
hidparser_get_main_item_data_descr_main(
			entity_item_t *parser_handle,
			uint_t		report_id,
			uint_t		main_item_type,
			uint_t		usage_page,
			uint_t		usage,
			int		*main_item_descr_value)
{
	entity_item_t *current = parser_handle;
	entity_attribute_t *attribute;
	unsigned char found_page = 0, found_usage = 0;


	if (usage == 0) found_usage = 1;
	if (current->entity_item_type == R_ITEM_COLLECTION) {

		return (hidparser_get_main_item_data_descr_main(
						current->info.child,
						report_id,
						main_item_type, usage_page,
						usage,
						main_item_descr_value));
	} else if (current->entity_item_type == main_item_type) {
		/* Match Item Type */
		attribute = current->entity_item_attributes;

		while (attribute != NULL) {
			if ((attribute->entity_attribute_tag ==
				R_ITEM_USAGE) &&
				(attribute->entity_attribute_value[0] ==
				usage)) {
				found_usage = 1;
				if (found_usage && found_page) {
					*main_item_descr_value =
						current->entity_item_params[0];
					return (HIDPARSER_SUCCESS);
				}
			}
			if ((attribute->entity_attribute_tag ==
				R_ITEM_USAGE_PAGE) &&
				(attribute->entity_attribute_value[0] ==
				usage_page)) {
				/* Match Usage Page */
				found_page = 1;
				if (found_usage && found_page) {
					*main_item_descr_value =
						current->entity_item_params[0];
					return (HIDPARSER_SUCCESS);
				}
			}
			attribute = attribute->entity_attribute_next;
		}
		if (current->entity_item_right_sibling != NULL) {
			found_page = 0;
			found_usage = 0;

			return (hidparser_get_main_item_data_descr_main(
					current->entity_item_right_sibling,
					report_id,
					main_item_type, usage_page,
					usage,
					main_item_descr_value));
		}
		else
			return (HIDPARSER_NOT_FOUND);
	}

	else {
		if (current->entity_item_right_sibling != NULL) {
			found_page = 0;
			found_usage = 0;

			return (hidparser_get_main_item_data_descr_main(
					current->entity_item_right_sibling,
					report_id,
					main_item_type, usage_page,
					usage,
					main_item_descr_value));
		}
		else
			return (HIDPARSER_NOT_FOUND);
	}
}

/*
 * hidparser_get_top_level_collection_usage :
 *	Get the usage page and usage for the top level collectoin item
 */
int
hidparser_get_top_level_collection_usage(hidparser_handle_t parse_handle,
					uint_t *usage_page,
					uint_t *usage)
{
	entity_item_t *current;
	entity_attribute_t *attribute;
	int found_usage = 0;
	int found_page = 0;

	if ((parse_handle == NULL) ||
		(parse_handle->hidparser_handle_parse_tree == NULL))
		return (HIDPARSER_FAILURE);

	current = parse_handle->hidparser_handle_parse_tree;

	if (current->entity_item_type != R_ITEM_COLLECTION)
		return (HIDPARSER_FAILURE);
	attribute = current->entity_item_attributes;
	while (attribute != NULL) {
		if (attribute->entity_attribute_tag == R_ITEM_USAGE) {
			found_usage = 1;
			*usage = attribute->entity_attribute_value[0];
			if (found_usage && found_page) {
				return (HIDPARSER_SUCCESS);
			}
		}
		if (attribute->entity_attribute_tag == R_ITEM_USAGE_PAGE) {
			found_page = 1;
			*usage_page = attribute->entity_attribute_value[0];
			if (found_usage && found_page) {
				return (HIDPARSER_SUCCESS);
			}
		}
		attribute = attribute->entity_attribute_next;
	}
	return (HIDPARSER_FAILURE);
}

/*
 * hidparser_print_report_descr_handle :
 *	Functions to print the parse tree. Currently not
 *	being called.
 */
static int
hidparser_print_report_descr_handle(entity_item_t *handle, int indent_level)
{
	entity_item_t *current = handle;

	while (current) {
		if (current->info.child) {
			hidparser_print_entity(current, indent_level);
			/* do children */
			(void) hidparser_print_report_descr_handle(
				current->info.child, indent_level+1);
		} else /* just a regular entity */ {
			hidparser_print_entity(current, indent_level);
		}
		current = current->entity_item_right_sibling;
	}

	return (HIDPARSER_SUCCESS);
}


#define	SPACE_PER_LEVEL 5

/*
 * hidparser_print_entity ;
 * Prints the entity items recursively
 */
static void
hidparser_print_entity(entity_item_t *entity, int indent_level)
{
	char indent_space[256];
	int count;
	entity_attribute_t *attr;

	indent_level *= SPACE_PER_LEVEL;

	for (count = 0; indent_level--; indent_space[count++] = ' ');

	indent_space[count] = 0;


	attr = entity->entity_item_attributes;
	while (attr) {
		USB_DPRINTF_L2(PRINT_MASK_ALL,
			hparser_log_handle, "%s%s",
			indent_space, items[attr->entity_attribute_tag]);

		attr = attr->entity_attribute_next;
	}

	USB_DPRINTF_L2(PRINT_MASK_ALL, hparser_log_handle, "%s",
		indent_space, items[entity->entity_item_type]);
}


/*
 * hidparser_print_attributes :
 *	Prints the attributes recusively.
 */
static void
hidparser_print_attributes(entity_attribute_t *attribute, char *ident_space)
{
	if (ident_space == NULL) {
		USB_DPRINTF_L2(PRINT_MASK_ALL,
			hparser_log_handle,
			"%s  value_length = %d ",
			items[attribute->entity_attribute_tag],
			attribute->entity_attribute_length);
		if (attribute->entity_attribute_value != NULL) {
			USB_DPRINTF_L2(PRINT_MASK_ALL,
				hparser_log_handle,
				"\tentity_attribute_value = 0x%x",
				attribute->entity_attribute_value[0]);
		}
	} else {
		USB_DPRINTF_L2(PRINT_MASK_ALL,
			hparser_log_handle,
			"%s%s  value_length = %d ", ident_space,
			items[attribute->entity_attribute_tag],
			attribute->entity_attribute_length);
		if (attribute->entity_attribute_value != NULL) {
			USB_DPRINTF_L2(PRINT_MASK_ALL,
			hparser_log_handle,
			"%s\tentity_attribute_value = 0x%x",
			ident_space, attribute->entity_attribute_value[0]);
		}
	}

	if (attribute->entity_attribute_next)
		hidparser_print_attributes(attribute->entity_attribute_next,
						ident_space);
}

/*
 * The next few functions will be used for parsing using the
 * grammar :
 *
 *	Start			-> ReportDescriptor <EOF>
 *
 *	ReportDescriptor	-> ItemList
 *
 *	ItemList		-> Items MainItem ItemList
 *				   | epsilon
 *
 *	MainItem		-> BeginCollection ItemList  EndCollection
 *				  | Input
 *				  | Output
 *				  | Feature
 *
 *	Items			-> GlobalItem  Items
 *				   | LocalItem Items
 *				   | SetDelimeterOpen LocalItemList
 *					SetDelimeterClose Items
 *				   | epsilon
 *
 *	LocalItemList		-> LocalItem Temp2
 *
 *	Temp2			-> LocalItem Temp2
 *				   | epsilon
 *
 *	GlobalItem		-> UsagePage
 *				   | LogicalMinimum
 *				   | LocgicalMaximum
 *				   | PhysicalMinimum
 *				   | PhysicalMaximum
 *				   | Unit
 *				   | Exponent
 *				   | ReportSize
 *				   | ReportCount
 *				   | ReportID
 *
 *	LocalItem		-> Usage
 *				   | UsageMinimum
 *				   | UsageMaximum
 *				   | DesignatorIndex
 *				   | DesignatorMinimum
 *				   | StringIndex
 *				   | StringMinimum
 *				   | StringMaximum
 *
 */


/*
 * hidparser_lookup_first :
 *	Looks up if token belongs to the FIRST of the function tag
 *	that is passed through the first argument
 */
static int
hidparser_lookup_first(int func_index, int token)
{

	int	*itemp;

	itemp = hid_first_list[func_index];
	while (*itemp != 0) {
		/* get the next terminal on the list */
		if (*itemp == token)
			return (HIDPARSER_SUCCESS);
		itemp++;
	}

	/* token is not on the FIRST list */
	return (HIDPARSER_FAILURE);

}


/*
 * hidparser_main :
 *	Function called from hparser_main.c to parse the Report Descriptor
 */
static int
hidparser_main(unsigned char *descriptor, size_t size,
			entity_item_t **item_ptr)
{
	hidparser_tok_t	*scan_ifp;
	int retval;

	scan_ifp = kmem_zalloc(sizeof (hidparser_tok_t), 0);
	if (scan_ifp == NULL) {
		cmn_err(CE_PANIC, "kmem_zalloc failed in hidparser_main");
	}
	scan_ifp->hidparser_tok_text =
		kmem_zalloc(HIDPARSER_TEXT_LENGTH, 0);
	if (scan_ifp->hidparser_tok_text == NULL) {
		cmn_err(CE_PANIC, "kmem_zalloc failed in hidparser_main");
	}
	scan_ifp->hidparser_tok_max_bsize = size;
	scan_ifp->hidparser_tok_entity_descriptor = descriptor;

	*item_ptr = NULL;
	retval =  hidparser_ReportDescriptorDash(item_ptr, scan_ifp);

	/*
	 * Free the Local & Global item list
	 * It maybe the case that no tree has been built
	 * up but there have been allocation in the attribute
	 * & control lists
	 */
	if (scan_ifp->hidparser_tok_gitem_head)
		hidparser_free_attribute_list(
			scan_ifp->hidparser_tok_gitem_head);

	if (scan_ifp->hidparser_tok_litem_head)
		hidparser_free_attribute_list(
			scan_ifp->hidparser_tok_litem_head);
	kmem_free(scan_ifp->hidparser_tok_text, HIDPARSER_TEXT_LENGTH);
	kmem_free(scan_ifp, sizeof (hidparser_tok_t));

	return (retval);
}

/*
 * hidparser_ReportDescriptorDash :
 *	Synthetic start symbol, implements
 *	hidparser_ReportDescriptor <EOF>
 */
static int
hidparser_ReportDescriptorDash(entity_item_t ** item_ptr,
			hidparser_tok_t *scan_ifp)
{

	if ((hidparser_ReportDescriptor(item_ptr, scan_ifp)
		== HIDPARSER_SUCCESS) &&
		(scan_ifp->hidparser_tok_token == 0)) {
		return (HIDPARSER_SUCCESS);
	}

	/*
	 * In case of failure, free the kernel memory
	 * allocated for partial building of the tree,
	 * if any
	 */
	if (*item_ptr != NULL)
		(void) hidparser_free_report_descr_handle(*item_ptr);

	*item_ptr = NULL;
	return (HIDPARSER_FAILURE);
}

/*
 * hidparser_ReportDescriptor :
 *	Implements the Rule :
 *	ReportDescriptor -> ItemList
 */
static int
hidparser_ReportDescriptor(entity_item_t ** item_ptr,
			hidparser_tok_t	*scan_ifp)
{
	hidparser_scan(scan_ifp);

	/*
	 * We do not search for the token in FIRST(ReportDescriptor)
	 * since -
	 *
	 * FIRST(ReportDescriptor) == FIRST(ItemList)
	 * ReportDescriptor ----> ItemList
	 */
	if (hidparser_ItemList(item_ptr, scan_ifp) == HIDPARSER_SUCCESS) {
		return (HIDPARSER_SUCCESS);
	}

	return (HIDPARSER_FAILURE);
}


/*
 * hidparser_ItemList :
 *	Implements the Rule :
 *	ItemList -> Items MainItem ItemList | epsilon
 */
static int
hidparser_ItemList(entity_item_t ** item_ptr, hidparser_tok_t *scan_ifp)
{
	entity_item_t *itemp1, *itemp2, *itemp3, *last_item;

	itemp1 = itemp2 = itemp3 = NULL;
	if (hidparser_Items(&itemp1, scan_ifp) == HIDPARSER_SUCCESS) {
		if (hidparser_MainItem(&itemp2, scan_ifp) ==
				HIDPARSER_SUCCESS) {
			if (hidparser_ItemList(&itemp3, scan_ifp) ==
					HIDPARSER_SUCCESS) {
				if (itemp1) {
					last_item =
					hidparser_find_item_end(itemp1);
					last_item->entity_item_right_sibling =
						itemp2 ? itemp2 : itemp3;
					*item_ptr = itemp1;
				}
				if (itemp2) {
					last_item =
					hidparser_find_item_end(itemp2);
					last_item->entity_item_right_sibling =
						itemp3;

					if (!itemp1)
						*item_ptr = itemp2;
				}
				if (itemp1 == NULL && itemp2 == NULL) {
					*item_ptr = itemp3;
				}
				return (HIDPARSER_SUCCESS);
			}
		}
	}
	/*
	 * If there has been failure at any point of time,
	 * Free the partially allocated trees
	 */
	if (itemp1)
		(void) hidparser_free_report_descr_handle(itemp1);

	*item_ptr = NULL;
	return (HIDPARSER_SUCCESS); /* epsilon */
}

/*
 * hidparser_MainItem :
 *	Implements the Rule :
 *	MainItem -> 	BeginCollection ItemList  EndCollection
 *			| Input
 *			| Output
 *			| Feature
 */
static int
hidparser_MainItem(entity_item_t ** item_ptr, hidparser_tok_t *scan_ifp)
{
	entity_item_t  *itemp, *itemp1, *itemp2;


	itemp = itemp1 = itemp2 = NULL;

	switch (scan_ifp->hidparser_tok_token) {
		case R_ITEM_INPUT:
			/* FALLTHRU */
		case R_ITEM_OUTPUT:
			/* FALLTHRU */
		case R_ITEM_FEATURE:
			*item_ptr = hidparser_allocate_entity(scan_ifp);
			hidparser_scan(scan_ifp);
			hidparser_global_err_check(*item_ptr);
			hidparser_local_err_check(*item_ptr);
			hidparser_mainitem_err_check(*item_ptr);
			return (HIDPARSER_SUCCESS);

			/*NOTREACHED*/

		case R_ITEM_COLLECTION:
			itemp = hidparser_allocate_entity(scan_ifp);
			hidparser_global_err_check(itemp);
			hidparser_local_err_check(itemp);
			hidparser_scan(scan_ifp);
			if (hidparser_ItemList(&itemp1, scan_ifp) ==
					HIDPARSER_SUCCESS) {
				itemp->info.child = itemp1;
				if (scan_ifp->hidparser_tok_token ==
					R_ITEM_END_COLLECTION) {
					itemp2 =
					hidparser_allocate_entity(scan_ifp);
					itemp->entity_item_right_sibling =
						itemp2;
					*item_ptr = itemp;

					hidparser_scan(scan_ifp);
					if (scan_ifp->hidparser_tok_leng !=
							0) {
						hidparser_report_err(
						HIDPARSER_ERR_WARN,
						HIDPARSER_ERR_STANDARD,
						R_ITEM_END_COLLECTION,
						1,
						"Data size should be zero");
					}
					return (HIDPARSER_SUCCESS);
				} else {
					hidparser_report_err(
					HIDPARSER_ERR_ERROR,
					HIDPARSER_ERR_STANDARD,
					R_ITEM_COLLECTION,
					0,
				"Must have a corresponding end collection");
				}
			}
			break;
		default :
			break;
	}
	/*
	 * If there has been failure at any point of time,
	 * Free the partially allocated trees
	 */
	if (itemp)
		(void) hidparser_free_report_descr_handle(itemp);

	*item_ptr = NULL;
	return (HIDPARSER_FAILURE);
}


/*
 * hidparser_Items :
 *	Implements the Rule :
 *	Items -> 	GlobalItem  Items
 *			| LocalItem Items
 *			| SetDelimeterOpen LocalItemList
 *				SetDelimeterClose Items
 *			| epsilon
 */
static int
hidparser_Items(entity_item_t ** item_ptr, hidparser_tok_t *scan_ifp)
{

	entity_item_t *iptr, *iptr1, *iptr2;

	int		token = scan_ifp->hidparser_tok_token;

	iptr = iptr1 = iptr2 = NULL;

	if (hidparser_lookup_first(HIDPARSER_ITEMS, token) ==
					HIDPARSER_SUCCESS) {
		if (token == R_ITEM_SET_DELIMITER) {
			if (scan_ifp->hidparser_tok_text[0] != 1)
				hidparser_error_delim(NULL,
					HIDPARSER_DELIM_ERR1);
			iptr = hidparser_allocate_entity(scan_ifp);
			hidparser_scan(scan_ifp);
			if (hidparser_LocalItemList(scan_ifp) ==
					HIDPARSER_SUCCESS) {
				token = scan_ifp->hidparser_tok_token;
				if (token == R_ITEM_SET_DELIMITER) {
					if (scan_ifp->hidparser_tok_text[0]
						!= 0)
						hidparser_error_delim(NULL,
							HIDPARSER_DELIM_ERR2);
					iptr1 = hidparser_allocate_entity(
						scan_ifp);
						hidparser_error_delim(iptr1,
							HIDPARSER_DELIM_ERR3);
					iptr->entity_item_right_sibling = iptr1;
					hidparser_scan(scan_ifp);
					if (hidparser_Items(&iptr2, scan_ifp)
						== HIDPARSER_SUCCESS) {
						if (iptr2 != NULL) {
					iptr1->entity_item_right_sibling =
								iptr2;
						}
						*item_ptr = iptr;
						return (HIDPARSER_SUCCESS);
					}
				}
			}
		} else if (hidparser_GlobalItem(scan_ifp) ==
						HIDPARSER_SUCCESS) {
			if (hidparser_Items(&iptr, scan_ifp) ==
						HIDPARSER_SUCCESS) {
				*item_ptr = iptr;
				return (HIDPARSER_SUCCESS);
			}
		} else if (hidparser_LocalItem(scan_ifp) ==
						HIDPARSER_SUCCESS) {
			if (hidparser_Items(&iptr, scan_ifp) ==
						HIDPARSER_SUCCESS) {
				*item_ptr = iptr;
				return (HIDPARSER_SUCCESS);
			}
		}

		/*
		 * If there has been failure at any point of time,
		 * Free the partially allocated trees
		 */
		if (iptr)
			(void) hidparser_free_report_descr_handle(iptr);

		*item_ptr = NULL;
		return (HIDPARSER_FAILURE);

	} else {
		*item_ptr = NULL;
		return (HIDPARSER_SUCCESS);	/* epsilon */
	}
}


/*
 * hidparser_LocalItemList :
 *	Implements the Rule :
 *	LocalItemList -> 	LocalItem Temp2
 */
static int
hidparser_LocalItemList(hidparser_tok_t	*scan_ifp)
{
	/*
	 * We do not search for the token in FIRST(LocalItemList)
	 * since -
	 *
	 * FIRST(LocalItemList) == FIRST(LocalItem)
	 * LocalItemList ------> LocalItem ---->...
	 */
	if (hidparser_LocalItem(scan_ifp) == HIDPARSER_SUCCESS) {
		if (hidparser_Temp2(scan_ifp) == HIDPARSER_SUCCESS) {
			return (HIDPARSER_SUCCESS);
		}
	}
	return (HIDPARSER_FAILURE);
}

/*
 * hidparser_Temp2 :
 *	Implements the Rule :
 *	Temp2 ->	LocalItem Temp2
 *			| epsilon
 */
static int
hidparser_Temp2(hidparser_tok_t	*scan_ifp)
{
	if (hidparser_LocalItem(scan_ifp) == HIDPARSER_SUCCESS) {
		if (hidparser_Temp2(scan_ifp) == HIDPARSER_SUCCESS) {
			return (HIDPARSER_SUCCESS);
		}
		return (HIDPARSER_FAILURE);
	} else {
		/* Epsilon */
		return (HIDPARSER_SUCCESS);
	}
}

/*
 * hidparser_GlobalItem :
 *	Implements the Rule :
 *	GlobalItem ->	UsagePage
 *			| LogicalMinimum
 *			| LocgicalMaximum
 *			| PhysicalMinimum
 *			| PhysicalMaximum
 *			| Unit
 *			| Exponent
 *			| ReportSize
 *			| ReportCount
 *			| ReportID
 */
static int
hidparser_GlobalItem(hidparser_tok_t	*scan_ifp)
{

	int i;

	switch (scan_ifp->hidparser_tok_token) {
		case R_ITEM_USAGE_PAGE:
			/* Error check */
			for (i = 0; i < scan_ifp->hidparser_tok_leng; i++) {
				/* Undefined data value : 0 */
				if (scan_ifp->hidparser_tok_text[i] == 0) {
					hidparser_report_err(
					HIDPARSER_ERR_WARN,
					HIDPARSER_ERR_STANDARD,
					R_ITEM_USAGE_PAGE,
					0,
					"Data field should be non-Zero");
				}
				/* Reserved values 0x0A-0xFE */
				else if ((scan_ifp->hidparser_tok_text[i] >=
					0x0a) &&
					(scan_ifp->hidparser_tok_text[i] <=
					0xFE)) {
					hidparser_report_err(
					HIDPARSER_ERR_WARN,
					HIDPARSER_ERR_STANDARD,
					R_ITEM_USAGE_PAGE,
					1,
				"Data field should not use reserved values");
				}
			}
			break;
		case R_ITEM_UNIT:
			/* FALLTHRU */
		case R_ITEM_EXPONENT:
			/*
			 * Error check :
			 * Nibble 7 should be zero
			 */
			if (scan_ifp->hidparser_tok_leng == 4) {
				if ((scan_ifp->hidparser_tok_text[3] &
					0xf0) != 0) {
					hidparser_report_err(
					HIDPARSER_ERR_WARN,
					HIDPARSER_ERR_STANDARD,
					scan_ifp->hidparser_tok_token,
					0,
			"Data field reserved bits should be Zero");
				}
			}
			break;
		case R_ITEM_REPORT_COUNT:
			/*
			 * Error Check :
			 * Report Count should be nonzero
			 */
			for (i = 0; i < scan_ifp->hidparser_tok_leng; i++) {
				if (scan_ifp->hidparser_tok_text[i])
					break;
			}
			if (i == scan_ifp->hidparser_tok_leng) {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				R_ITEM_REPORT_COUNT,
				0,
				"Report Count = 0");
			}
			break;
		case R_ITEM_REPORT_ID:
			/*
			 * Error check :
			 * Report Id should be nonzero & <= 255
			 */
			if (scan_ifp->hidparser_tok_leng != 1)  {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				R_ITEM_REPORT_ID,
				1,
				"Must be contained in a byte");
			}
			if (!scan_ifp->hidparser_tok_text[0]) {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				R_ITEM_REPORT_ID,
				0,
				"Report Id must be non-zero");
			}
			break;
		case R_ITEM_LOGICAL_MINIMUM:
			break;
		case R_ITEM_LOGICAL_MAXIMUM:
			break;
		case R_ITEM_PHYSICAL_MINIMUM:
			break;
		case R_ITEM_PHYSICAL_MAXIMUM:
			break;
		case R_ITEM_REPORT_SIZE:
			break;
		case R_ITEM_PUSH:
			/* FALLTHRU */
		case R_ITEM_POP:
			if (scan_ifp->hidparser_tok_leng != 0)  {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				scan_ifp->hidparser_tok_token,
				0,
				"Data Field size should be zero");
			}
			break;
		default:
			return (HIDPARSER_FAILURE);

			/*NOTREACHED*/
	}

	hidparser_add_attribute(scan_ifp);
	hidparser_scan(scan_ifp);
	return (HIDPARSER_SUCCESS);


}

/*
 * hidparser_LocalItem :
 *	Implements the Rule :
 *	LocalItem ->	Usage
 *			| UsageMinimum
 *			| UsageMaximum
 *			| DesignatorIndex
 *			| DesignatorMinimum
 *			| StringIndex
 *			| StringMinimum
 *			| StringMaximum
 */
static int
hidparser_LocalItem(hidparser_tok_t	*scan_ifp)
{
	int i;

	switch (scan_ifp->hidparser_tok_token) {
		case R_ITEM_USAGE:
			/*
			 * Error Check :
			 * Data Field should be nonzero
			 */
			for (i = 0; i < scan_ifp->hidparser_tok_leng; i++) {
				if (scan_ifp->hidparser_tok_text[i])
					break;
			}
			if (i == scan_ifp->hidparser_tok_leng) {
				hidparser_report_err(
				HIDPARSER_ERR_WARN,
				HIDPARSER_ERR_STANDARD,
				R_ITEM_USAGE,
				0,
				"Data Field should be non-zero");
			}
			/* FALLTHRU */
		case R_ITEM_USAGE_MIN:
			/* FALLTHRU */
		case R_ITEM_USAGE_MAX:
			/* FALLTHRU */
		case R_ITEM_DESIGNATOR_INDEX:
			/* FALLTHRU */
		case R_ITEM_DESIGNATOR_MIN:
			/* FALLTHRU */
		case R_ITEM_STRING_INDEX:
			/* FALLTHRU */
		case R_ITEM_STRING_MIN:
			/* FALLTHRU */
		case R_ITEM_STRING_MAX:
			hidparser_add_attribute(scan_ifp);
			hidparser_scan(scan_ifp);

			return (HIDPARSER_SUCCESS);

			/*NOTREACHED*/
		default:
			break;
	}

	return (HIDPARSER_FAILURE);
}



/*
 * hidparser_allocate_entity :
 *	Allocate Item of type 'type', length 'leng' and
 *	params 'text'. Fill in the attributes allocated
 *	so far from both the local and global item lists.
 *	Make the child and sibling of the item NULL.
 */
static entity_item_t *
hidparser_allocate_entity(hidparser_tok_t	*scan_ifp)
{

	entity_item_t *entity;
	entity_attribute_t *aend;

	int	entity_type = scan_ifp->hidparser_tok_token;
	unsigned char	*text = scan_ifp->hidparser_tok_text;
	int	len = scan_ifp->hidparser_tok_leng;

	if (!(entity = (entity_item_t *)
	kmem_zalloc(sizeof (entity_item_t), 0))) {
		cmn_err(CE_PANIC, "kmem_zalloc failed in allocate_entity() "
			"for entity.");
	}

	entity->entity_item_type = entity_type;

	entity->entity_item_params_leng = len;

	if (len != 0) {
		entity->entity_item_params = (char *)kmem_zalloc(len, 0);
		if (!entity->entity_item_params) {
			cmn_err(CE_PANIC, "kmem_zalloc failed "
				"in allocate_entity()");
		}
		(void) bcopy(text, entity->entity_item_params, len);
	}


	/* copy attributes from entity attribute state table */
	entity->entity_item_attributes = hidparser_cp_attribute_list(
				scan_ifp->hidparser_tok_gitem_head);

	/*
	 * append the control attributes, then clear out the control
	 * attribute state table list
	 */
	if (entity->entity_item_attributes) {
		aend = hidparser_find_attribute_end(
			entity->entity_item_attributes);
		aend->entity_attribute_next =
			scan_ifp->hidparser_tok_litem_head;
		scan_ifp->hidparser_tok_litem_head = NULL;
	} else {
		entity->entity_item_attributes =
			scan_ifp->hidparser_tok_litem_head;
		scan_ifp->hidparser_tok_litem_head = NULL;
	}

	entity->info.child = entity->entity_item_right_sibling = 0;


	return (entity);
}

/*
 * hidparser_add_attribute :
 *	Add an attribute to the global or local item list
 *	If the last 4th bit from right is 1, add to the local item list
 *	Else add to the global item list
 */
static void
hidparser_add_attribute(hidparser_tok_t	*scan_ifp)
{

	entity_attribute_t *newattrib, **previous, *elem;

	int	entity = scan_ifp->hidparser_tok_token;
	unsigned char	*text = scan_ifp->hidparser_tok_text;
	int	len = scan_ifp->hidparser_tok_leng;

	if (len == 0) {
		USB_DPRINTF_L2(PRINT_MASK_ALL,
			hparser_log_handle,
			"hidparser_add_attribute: len = 0 for item = 0x%x",
			entity);
		return;
	}

	if (entity & HIDPARSER_ISLOCAL_MASK) {
		previous = &scan_ifp->hidparser_tok_litem_head;
	} else {
		previous = &scan_ifp->hidparser_tok_gitem_head;
	}

	elem = *previous;

	/*
	 * remove attribute if it is already on list, except
	 * for control attributes(local items), as we could have
	 * multiple usages...
	 * unless we want to hassle with checking for unique parameters.
	 */
	while (elem) {
		if (elem->entity_attribute_tag == entity &&
			!(entity & HIDPARSER_ISLOCAL_MASK)) {
			*previous = elem->entity_attribute_next;
			kmem_free(elem->entity_attribute_value,
				elem->entity_attribute_length);
			kmem_free(elem, sizeof (entity_attribute_t));
			elem = *previous;
		} else {
			previous = &elem->entity_attribute_next;
			elem = elem->entity_attribute_next;
		}
	}

	/* create new attribute for this entry */
	newattrib = hidparser_alloc_attrib_list(1);
	newattrib->entity_attribute_tag = entity;
	if (!(newattrib->entity_attribute_value =
		(char *)kmem_zalloc(len, 0))) {
	    cmn_err(CE_PANIC, "kmem_zalloc failed in hidparser_add_attribute.");
	}
	(void) bcopy(text, newattrib->entity_attribute_value, len);
	newattrib->entity_attribute_length = len;

	/* attach to end of list */
	*previous = newattrib;
}

/*
 * hidparser_alloc_attrib_list :
 *	Allocate space for n attributes , create a linked list and
 *	return the head
 */
static entity_attribute_t *
hidparser_alloc_attrib_list(int count)
{
	entity_attribute_t *head, *current;

	if (count <= 0) {
		return (NULL);
	}

	if (!(head = (entity_attribute_t *)
		kmem_zalloc(sizeof (entity_attribute_t), 0))) {
			cmn_err(CE_PANIC, "kmem_zalloc failed in "
				"entity_attribute().");
	}
	count--;
	current = head;
	while (count--) {
		if (!(current->entity_attribute_next =
			(entity_attribute_t *)kmem_zalloc
			(sizeof (entity_attribute_t), 0))) {
				cmn_err(CE_PANIC, "kmem_zalloc failed in "
					"entity_attribute().");
		}
		current = current->entity_attribute_next;
	}
	current->entity_attribute_next = NULL;

	return (head);
}

/*
 * hidparser_cp_attribute_list :
 *	Copies the Global item list pointed to by head
 *	We create a clone of the global item list here
 *	because we want to retain the Global items to
 *	the next Main Item.
 */
static entity_attribute_t *
hidparser_cp_attribute_list(entity_attribute_t *head)
{
	entity_attribute_t *return_value, *current_src, *current_dst;

	if (!head) {

		return (NULL);
	}

	current_src = head;
	current_dst = return_value = hidparser_alloc_attrib_list(1);

	while (current_src) {
		current_dst->entity_attribute_tag =
			current_src->entity_attribute_tag;
		current_dst->entity_attribute_length =
			current_src->entity_attribute_length;
		if (!(current_dst->entity_attribute_value = (char *)kmem_zalloc
			(current_dst->entity_attribute_length, 0))) {
			cmn_err(CE_PANIC,
		"kmem_zalloc failed in hidparser_cp_attribute_list.");
		}
		(void) bcopy(current_src->entity_attribute_value,
			current_dst->entity_attribute_value,
			current_src->entity_attribute_length);
		if (current_src->entity_attribute_next) {
			current_dst->entity_attribute_next =
				hidparser_alloc_attrib_list(1);
		}
		current_src = current_src->entity_attribute_next;
		current_dst = current_dst->entity_attribute_next;
	}

	return (return_value);
}

/*
 * hidparser_find_attribute_end :
 *	Find the last item in the attribute list pointed to by head
 */
static entity_attribute_t *
hidparser_find_attribute_end(entity_attribute_t *head)
{
	if (head == NULL) {

		return (NULL);
	}
	while (head->entity_attribute_next != NULL)
		head = head->entity_attribute_next;

	return (head);
}

/*
 * hidparser_find_item_end :
 *	Search the siblings of items and find the last item in the list
 */
static entity_item_t *
hidparser_find_item_end(entity_item_t *head)
{
	if (!head)
		return (NULL);
	while (head->entity_item_right_sibling)
		head = head->entity_item_right_sibling;

	return (head);
}


/*
 * hidparser_free_report_descr_handle :
 *	Free the parse tree pointed to by handle
 */
static void
hidparser_free_report_descr_handle(entity_item_t *handle)
{
	entity_item_t *next, *current, *child;

	current = handle;

	while (current) {
		child = current->info.child;
		next = current->entity_item_right_sibling;
		if (current->entity_item_type == R_ITEM_COLLECTION) {
			if (current->entity_item_params != NULL)
				kmem_free(current->entity_item_params,
					    current->entity_item_params_leng);
			if (current->entity_item_attributes != NULL)
				hidparser_free_attribute_list(
					current->entity_item_attributes);
			USB_DPRINTF_L4(PRINT_MASK_ALL, hparser_log_handle,
				    "FREE 1: %s",
				    items[current->entity_item_type]);
			kmem_free(current, sizeof (entity_item_t));
			(void) hidparser_free_report_descr_handle(child);
		} else {
			if (current->entity_item_params != NULL) {
				kmem_free(current->entity_item_params,
					    current->entity_item_params_leng);
			}
			if (current->entity_item_attributes != NULL) {
				hidparser_free_attribute_list(
					current->entity_item_attributes);
			}
			USB_DPRINTF_L4(PRINT_MASK_ALL,
				    hparser_log_handle, "FREE 2: %s",
				    items[current->entity_item_type]);
			kmem_free(current, sizeof (entity_item_t));
		}
		current = next;
	}

}


/*
 * hidparser_free_attribute_list :
 *	Free the attribue list ponited to by head
 */
static void
hidparser_free_attribute_list(entity_attribute_t *head)
{
	entity_attribute_t *next, *current;

	current = head;

	while (current) {
		next = current->entity_attribute_next;
		USB_DPRINTF_L4(PRINT_MASK_ALL,
			hparser_log_handle, "FREE: %s value_length = %d",
			items[current->entity_attribute_tag],
			current->entity_attribute_length);

		if (current->entity_attribute_value != NULL) {
			USB_DPRINTF_L4(PRINT_MASK_ALL,
				hparser_log_handle,
				"\tvalue = 0x%x",
				current->entity_attribute_value[0]);
			kmem_free(current->entity_attribute_value,
					current->entity_attribute_length);
		}

		kmem_free(current, sizeof (entity_attribute_t));
		current = next;
	}
}


/*
 * hidparser_initialize_items :
 *	Initialize items array before start scanning and parsing.
 *	This array of strings are used for printing purpose.
 */
static void
hidparser_initialize_items(void)
{
	items[R_ITEM_USAGE] = "item_usage";
	items[R_ITEM_USAGE_MIN] = "item_usage_min";
	items[R_ITEM_USAGE_MAX] = "item_usage_max";
	items[R_ITEM_DESIGNATOR_INDEX] = "item_desginator_index";
	items[R_ITEM_DESIGNATOR_MIN] = "item_designator_min";
	items[R_ITEM_DESIGNATOR_MAX] = "item_designator_max";
	items[R_ITEM_STRING_INDEX] = "item_string_index";
	items[R_ITEM_STRING_MIN] = "item_string_min";
	items[R_ITEM_STRING_MAX] = "item_string_max";


	items[R_ITEM_USAGE_PAGE] = "item_usage_page";
	items[R_ITEM_LOGICAL_MINIMUM] = "item_logical_minimum";
	items[R_ITEM_LOGICAL_MAXIMUM] = "item_logical_maximum";
	items[R_ITEM_PHYSICAL_MINIMUM] = "item_physical_minimum";
	items[R_ITEM_PHYSICAL_MAXIMUM] = "item_physical_maximum";
	items[R_ITEM_EXPONENT] = "item_exponent";
	items[R_ITEM_UNIT] = "item_unit";
	items[R_ITEM_REPORT_SIZE] = "item_report_size";
	items[R_ITEM_REPORT_ID] = "item_report_id";
	items[R_ITEM_REPORT_COUNT] = "item_report_count";
	items[R_ITEM_PUSH] = "item_push";
	items[R_ITEM_POP] = "item_pop";


	items[R_ITEM_INPUT] = "item_input";
	items[R_ITEM_OUTPUT] = "item_output";
	items[R_ITEM_COLLECTION] = "item_collection";
	items[R_ITEM_FEATURE] = "item_feature";
	items[R_ITEM_END_COLLECTION] = "item_end_collection";

	items[R_ITEM_SET_DELIMITER] = "item_delimiter";
}



/*
 * hidparser_scan :
 *	This function scans  the input entity descriptor, sees the data
 *	length, returns the next token, data bytes and length in the
 *	scan_ifp structure.
 */

static void
hidparser_scan(hidparser_tok_t	*scan_ifp)
{
	int count;
	int ch;
	int parsed_leng;
	unsigned char *parsed_text;
	unsigned char *entity_descriptor;
	char err_str[32];
	size_t	entity_buffer_size, index;

	index = scan_ifp->hidparser_tok_index;
	entity_buffer_size = scan_ifp->hidparser_tok_max_bsize;
	parsed_leng = 0;
	parsed_text = scan_ifp->hidparser_tok_text;
	entity_descriptor = scan_ifp->hidparser_tok_entity_descriptor;

next_item :
	if (index <= entity_buffer_size -1) {

		USB_DPRINTF_L4(PRINT_MASK_ALL,
			hparser_log_handle,
			"scanner: index 1 = %d", index);

		ch = 0xFF & entity_descriptor[index++];
		/*
		 * Error checking :
		 * Unrecognized items should be passed over
		 * by the parser.
		 * Section 5.4
		 */
		if (!(hidparser_isvalid_item(ch))) {
			(void) sprintf(err_str, "%s : 0x%2x",
				"Unknown or reserved item",
				ch);
			hidparser_report_err(HIDPARSER_ERR_ERROR,
					HIDPARSER_ERR_STANDARD,
					0,
					0x3F,
					err_str);
			goto next_item;
		}
		USB_DPRINTF_L4(PRINT_MASK_ALL,
			hparser_log_handle,
			"scanner: index 2 = %d, ch = %x", index, ch);

		if (ch == EXTENDED_ITEM) {
			parsed_leng = entity_descriptor[index++];
			ch = entity_descriptor[index++];
			hidparser_report_err(HIDPARSER_ERR_WARN,
					HIDPARSER_ERR_STANDARD,
					0,
					0x3E,
					"Long item defined");
		} else {
			parsed_leng = ch & 0x03;
			USB_DPRINTF_L4(PRINT_MASK_ALL,
				hparser_log_handle,
				"scanner: parsed_leng = %x", parsed_leng);
			/* 3 really means 4.. see p.21 HID */
			if (parsed_leng == 3)
				parsed_leng++;
		}
		for (count = 0; count < parsed_leng; count++) {
			parsed_text[count] = entity_descriptor[index++];
			USB_DPRINTF_L4(PRINT_MASK_ALL,
				hparser_log_handle,
				"scanner: parsed_text[%d] = %x, index3 = %d",
				count, parsed_text[count], index);
		}

		USB_DPRINTF_L4(PRINT_MASK_ALL,
			hparser_log_handle,
			"scanner: lexical analyzer found %x "
			"before translation", ch);

		scan_ifp->hidparser_tok_index = index;
		scan_ifp->hidparser_tok_leng = parsed_leng;
		scan_ifp->hidparser_tok_token = ch & 0xFC;
	} else {
		scan_ifp->hidparser_tok_leng = 0;
		scan_ifp->hidparser_tok_token = 0;	/* EOF */
	}
}



/*
 * hidparser_report_err :
 *	Construct and print the error code
 *	Ref : Hidview error check list
 */
static void
hidparser_report_err(int err_level, int	err_type, int tag,
			int subcode, char *msg)
{
	unsigned int	BmParserErrorCode = 0;

	if (err_level) {
		BmParserErrorCode |= HIDPARSER_ERR_ERROR;
	}
	if (err_type) {
		BmParserErrorCode |= HIDPARSER_ERR_STANDARD;
	}
	BmParserErrorCode |= (tag << 8) & HIDPARSER_ERR_TAG_MASK;
	BmParserErrorCode |= subcode & HIDPARSER_ERR_SUBCODE_MASK;

	if (err_level) {
		USB_DPRINTF_L3(PRINT_MASK_ALL, hparser_log_handle,
			"err code = 0x%4x, err str = %s",
			BmParserErrorCode, msg);

	} else {
		USB_DPRINTF_L3(PRINT_MASK_ALL, hparser_log_handle,
			"wrn code = 0x%4x, wrn str = %s",
			BmParserErrorCode, msg);
	}
}

/*
 * hidparser_isvalid_item :
 *	Find  if the item tag is a valid one
 */
static int
hidparser_isvalid_item(int tag)
{
	if (tag == EXTENDED_ITEM)
		return (1);
	tag &= 0xFC;
	if ((tag == R_ITEM_INPUT) ||
		(tag == R_ITEM_OUTPUT) ||
		(tag == R_ITEM_COLLECTION) ||
		(tag == R_ITEM_FEATURE) ||
		(tag == R_ITEM_END_COLLECTION) ||
		(tag == R_ITEM_USAGE_PAGE) ||
		(tag == R_ITEM_LOGICAL_MINIMUM) ||
		(tag == R_ITEM_LOGICAL_MAXIMUM) ||
		(tag == R_ITEM_PHYSICAL_MINIMUM) ||
		(tag == R_ITEM_PHYSICAL_MAXIMUM) ||
		(tag == R_ITEM_EXPONENT) ||
		(tag == R_ITEM_UNIT) ||
		(tag == R_ITEM_REPORT_SIZE) ||
		(tag == R_ITEM_REPORT_ID) ||
		(tag == R_ITEM_REPORT_COUNT) ||
		(tag == R_ITEM_PUSH) ||
		(tag == R_ITEM_POP) ||
		(tag == R_ITEM_USAGE) ||
		(tag == R_ITEM_USAGE_MIN) ||
		(tag == R_ITEM_USAGE_MAX) ||
		(tag == R_ITEM_DESIGNATOR_INDEX) ||
		(tag == R_ITEM_DESIGNATOR_MIN) ||
		(tag == R_ITEM_DESIGNATOR_MAX) ||
		(tag == R_ITEM_STRING_INDEX) ||
		(tag == R_ITEM_STRING_MIN) ||
		(tag == R_ITEM_STRING_MAX) ||
		(tag == R_ITEM_SET_DELIMITER))
		return (1);
	else
		return (0);

}

/*
 * hidparser_lookup_attribute :
 *	Takes an item pointer(report structure) and a tag(e.g Logical
 *	Min) as input. Returns the corresponding attribute structure.
 *	Presently used for error checking only.
 */
static entity_attribute_t *
hidparser_lookup_attribute(entity_item_t *item, int attr_tag)
{
	entity_attribute_t *temp;

	if (item == NULL)
		return (NULL);
	temp = item->entity_item_attributes;
	while (temp != NULL) {
		if (temp->entity_attribute_tag == attr_tag)
			return (temp);
		temp = temp->entity_attribute_next;
	}
	return (NULL);
}


/*
 * hidparser_global_err_check :
 *	Error checking for Global Items that need to be
 *	performed in MainItem
 */
static void
hidparser_global_err_check(entity_item_t *mainitem)
{

	hidparser_check_minmax_val_signed(mainitem, R_ITEM_LOGICAL_MINIMUM,
				R_ITEM_LOGICAL_MAXIMUM, 0, 0);
	hidparser_check_minmax_val_signed(mainitem, R_ITEM_PHYSICAL_MINIMUM,
				R_ITEM_PHYSICAL_MAXIMUM, 0, 0);
	hidparser_check_correspondence(mainitem, R_ITEM_PHYSICAL_MINIMUM,
				R_ITEM_PHYSICAL_MAXIMUM, 0, 0,
				"Must have a corresponging Physical min",
				"Must have a corresponging Physical max");
	hidparser_check_correspondence(mainitem, R_ITEM_PUSH, R_ITEM_POP,
				1, 0, "Should have a corresponging Pop",
				"Must have a corresponging Push");

}

/*
 * hidparser_mainitem_err_check :
 *	Error checking for Main Items
 */
static void
hidparser_mainitem_err_check(entity_item_t *mainitem)
{
	int	itemmask = 0;
	entity_attribute_t	*attr;

	attr = mainitem->entity_item_attributes;

	if (attr != NULL) {
		while (attr) {
			switch (attr->entity_attribute_tag) {
				case R_ITEM_LOGICAL_MINIMUM:
					itemmask |= 0x01;
					break;
				case R_ITEM_LOGICAL_MAXIMUM:
					itemmask |= 0x02;
					break;
				case R_ITEM_REPORT_SIZE:
					itemmask |= 0x04;
					break;
				case R_ITEM_REPORT_COUNT:
					itemmask |= 0x08;
					break;
				case R_ITEM_USAGE_PAGE:
					itemmask |= 0x10;
					break;
				default :
					break;
			} /* switch */
			attr = attr->entity_attribute_next;
		} /* while */
	} /* if */
	if (itemmask != 0x1f) {
			hidparser_report_err(
			HIDPARSER_ERR_ERROR,
			HIDPARSER_ERR_STANDARD,
			mainitem->entity_item_type,
			0,
			"Required Global/Local items must be defined");
	}
}

/*
 * hidparser_local_err_check :
 *	Error checking for Local items that is done when a MainItem
 *	is encountered
 */
static void
hidparser_local_err_check(entity_item_t *mainitem)
{

	hidparser_check_correspondence(mainitem, R_ITEM_USAGE_MIN,
				R_ITEM_USAGE_MAX, 0, 0,
				"Must have a corresponging Usage Min",
				"Must have a corresponging Usage Max");
	hidparser_check_minmax_val(mainitem, R_ITEM_USAGE_MIN,
				R_ITEM_USAGE_MAX, 1, 1);
	hidparser_check_correspondence(mainitem, R_ITEM_DESIGNATOR_MIN,
				R_ITEM_DESIGNATOR_MAX, 0, 0,
				"Must have a corresponging Designator min",
				"Must have a corresponging Designator Max");
	hidparser_check_minmax_val(mainitem, R_ITEM_DESIGNATOR_MIN,
				R_ITEM_DESIGNATOR_MAX, 1, 1);
	hidparser_check_correspondence(mainitem, R_ITEM_STRING_MIN,
				R_ITEM_STRING_MAX, 0, 0,
				"Must have a corresponging String min",
				"Must have a corresponging String Max");
	hidparser_check_minmax_val(mainitem, R_ITEM_STRING_MIN,
				R_ITEM_STRING_MAX, 1, 1);
}

/*
 * hidparser_find_unsigned_val :
 *	Find the value for multibye data
 *	Ref : Section 5.8 of HID Spec 1.0
 */
static unsigned int
hidparser_find_unsigned_val(entity_attribute_t *attr)
{
	char *text;
	int len, i;
	unsigned int ret = 0;

	text = attr->entity_attribute_value;
	len = attr->entity_attribute_length;
	for (i = 0; i < len; i++)
		ret |=	(text[i] << (8*i));
	return (ret);
}

/*
 * hidparser_find_signed_val :
 *	Find the value for signed multibye data
 *	Ref : Section 5.8 of HID Spec 1.0
 */
static signed int
hidparser_find_signed_val(entity_attribute_t *attr)
{
	char *text;
	int len, i;
	int	ret = 0;

	text = attr->entity_attribute_value;
	len = attr->entity_attribute_length;
	for (i = 0; i < len; i++)
		ret |=	(text[i] << (8*i));
	return (ret);
}

/*
 * hidparser_check_correspondence :
 *	Check if the item item2 corresponding to item1 exists and vice versa
 *	If not report the appropriate error
 */
static void
hidparser_check_correspondence(entity_item_t *mainitem,
				int item_tag1, int item_tag2, int val1,
				int val2, char *str1, char *str2)
{
	entity_attribute_t *temp1, *temp2;

	temp1 = hidparser_lookup_attribute(mainitem, item_tag1);
	temp2 = hidparser_lookup_attribute(mainitem, item_tag2);
	if (item_tag1 == R_ITEM_PUSH) {
		(void) strcpy(str1, "must have a corresponding pop");
		(void) strcpy(str2, "must have a corresponding push");
	}
	if ((temp1 != NULL) && (temp2 == NULL)) {
		hidparser_report_err(
		HIDPARSER_ERR_ERROR,
		HIDPARSER_ERR_STANDARD,
		item_tag1,
		val1,
		str1);
	}
	if ((temp2 != NULL) && (temp1 == NULL)) {
		hidparser_report_err(
		HIDPARSER_ERR_ERROR,
		HIDPARSER_ERR_STANDARD,
		item_tag2,
		val2,
		str2);
	}
}

/*
 * hidparser_check_minmax_val :
 *	Check if the Min value <= Max and vice versa
 *	Print for warnings and errors have been taken care separately.
 */
static void
hidparser_check_minmax_val(entity_item_t *mainitem,
			int item_tag1, int item_tag2,
			int val1, int val2)
{
	entity_attribute_t *temp1, *temp2;

	temp1 = hidparser_lookup_attribute(mainitem, item_tag1);
	temp2 = hidparser_lookup_attribute(mainitem, item_tag2);
	if ((temp1 != NULL) && (temp2 != NULL)) {
		if (hidparser_find_unsigned_val(temp1) >
			hidparser_find_unsigned_val(temp2)) {
			if ((item_tag1 == R_ITEM_LOGICAL_MINIMUM) ||
				(item_tag1 == R_ITEM_PHYSICAL_MINIMUM)) {
				hidparser_report_err(
				HIDPARSER_ERR_WARN,
				HIDPARSER_ERR_STANDARD,
				item_tag1,
				val1,
				"unsigned : Min should be <= to Max");
			} else {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag1,
				val1,
				"Min must be <= to Max");
			}
		}
		if (hidparser_find_unsigned_val(temp2) <
			hidparser_find_unsigned_val(temp1)) {
			if ((item_tag2 == R_ITEM_LOGICAL_MAXIMUM) ||
				(item_tag2 == R_ITEM_PHYSICAL_MAXIMUM)) {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag2,
				val2,
				"unsigned : Max should be >= to Min");
			} else {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag2,
				val2,
				"Max must be >= to Min");
			}
		}
	}	/* if (temp1 != NULL) && (temp2 != NULL) */
}

/*
 * hidparser_check_minmax_val_signed :
 *	Check if the Min value <= Max and vice versa
 *	Print for warnings and errors have been taken care separately.
 */
static void
hidparser_check_minmax_val_signed(entity_item_t *mainitem,
			int item_tag1, int item_tag2,
			int val1, int val2)
{
	entity_attribute_t *temp1, *temp2;

	temp1 = hidparser_lookup_attribute(mainitem, item_tag1);
	temp2 = hidparser_lookup_attribute(mainitem, item_tag2);
	if ((temp1 != NULL) && (temp2 != NULL)) {
		if (hidparser_find_signed_val(temp1) >
			hidparser_find_signed_val(temp2)) {
			if ((item_tag1 == R_ITEM_LOGICAL_MINIMUM) ||
				(item_tag1 == R_ITEM_PHYSICAL_MINIMUM)) {
				hidparser_report_err(
				HIDPARSER_ERR_WARN,
				HIDPARSER_ERR_STANDARD,
				item_tag1,
				val1,
				"signed : Min should be <= to Max");
			} else {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag1,
				val1,
				"Min must be <= to Max");
			}
		}
		if (hidparser_find_signed_val(temp2) <
			hidparser_find_signed_val(temp1)) {
			if ((item_tag2 == R_ITEM_LOGICAL_MAXIMUM) ||
				(item_tag2 == R_ITEM_PHYSICAL_MAXIMUM)) {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag2,
				val2,
				"signed : Max should be >= to Min");
			} else {
				hidparser_report_err(
				HIDPARSER_ERR_ERROR,
				HIDPARSER_ERR_STANDARD,
				item_tag2,
				val2,
				"Max must be >= to Min");
			}
		}
	}	/* if (temp1 != NULL) && (temp2 != NULL) */
}


/*
 * hidparser_error_delim :
 *	Error check for Delimiter Sets
 */
static void
hidparser_error_delim(entity_item_t *item, int err)
{
	entity_attribute_t *attr;
	switch (err) {
		case HIDPARSER_DELIM_ERR1:
			hidparser_report_err(
			HIDPARSER_ERR_ERROR,
			HIDPARSER_ERR_STANDARD,
			R_ITEM_SET_DELIMITER,
			0,
			"Must be Delimiter Open");
			break;
		case HIDPARSER_DELIM_ERR2:
			hidparser_report_err(
			HIDPARSER_ERR_ERROR,
			HIDPARSER_ERR_STANDARD,
			R_ITEM_SET_DELIMITER,
			0,
			"Must be Delimiter Close");
			break;
		case HIDPARSER_DELIM_ERR3:
			attr = item->entity_item_attributes;
			while (attr != NULL) {
				if ((attr->entity_attribute_tag !=
					R_ITEM_USAGE) &&
					(attr->entity_attribute_tag !=
					R_ITEM_USAGE_MIN) &&
					(attr->entity_attribute_tag !=
					R_ITEM_USAGE_MAX)) {
					hidparser_report_err(
					HIDPARSER_ERR_ERROR,
					HIDPARSER_ERR_STANDARD,
					R_ITEM_SET_DELIMITER,
					3,
			"May only contain Usage,Usage Min and Usage Max");
				}
				attr = attr->entity_attribute_next;
			}
			break;
		default :
			break;
	}
}
