/* @(#)log.h 1.2 91/02/26 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef log_h
#define log_h

#ifdef DEBUGGING

typedef struct LogCategory {
    char *name;
    struct LogCategory **back_ptr;
} LogCategory_t;


/*
 * Let the log module know of a category map/database.
 * 'map' is the name occurring on the first line of a ".db" file.
 */

#define LOG_LINK(map) \
	if (1) {extern LogCategory_t map[]; _log_link(map);} else

/*
 * Generic category predicate. Two reasons for making it a macro:
 * 1. The implicit declaration of the category keeps code from getting
 *    cluttred.
 * 2. We could have made this a function call, but then it would have to
 *    be called all the time just to test whether the particular log category
 *    is on or not.
 */
#define LOG_IF(CATEGORY,STUFF) \
	if (log_onoff) { \
	    extern LogCategory_t *CATEGORY; \
	    if (CATEGORY) { \
		_log_category = CATEGORY; \
		STUFF; \
	    } \
	} else

#else

#define LOG_LINK(map)
#define LOG_IF(CATEGORY,STUFF)

#endif

extern int	log_onoff;		/* global log flag. See LOG_IF() */


/*
 * Start and stop logging to the given file.
 * Both 'filename' and 'alternate_out_fp' are optional.
 * 'log_echo()' toggles the actual outputting to the alternate fp.
 */
void	log_start(char *out_filename, File alternate_out_fp);
void	log_finish(void);
void	log_echo(Boolean);


/*
 * enable disable logging of given category
 */
void	log_enable (char * category_name);
void	log_disable (char * category_name);

void	log_dump(void);		/* dump info to stdout */


/*
 * Apply log_enable() to every category in 'category_filename'
 * Format of 'category_filename' is a log category name per line
 * '#' can be used for EOL comments
 */
Boolean	log_enable_from_file(char * category_filename);


#ifdef DEBUGGING
#define LOG(Category, stuff) LOG_IF(Category, log_printf stuff)
#else
#define LOG(Category, stuff)
#endif

#ifdef DEBUGGING
/*
 * private routines
 */

void log_printf (const char fmt[], ...);
void _log_link(LogCategory_t *);

extern LogCategory_t *_log_category;

#endif

#endif /* log_h */
