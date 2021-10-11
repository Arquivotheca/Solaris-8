/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident  "@(#)gsscred_file.c 1.10     98/07/01 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "gsscred.h"

/*
 *  gsscred utility
 *  Manages mapping between a security principal name and unix uid.
 *  Implementation file for the file based gsscred utility.
 */

#define	MAX_ENTRY_LEN 1024
static const char credFile[] = "/etc/gss/gsscred_db";
static const char credFileTmp[] = "/etc/gss/gsscred_db.tmp";

static int matchEntry(const char *entry, const gss_buffer_t name,
		const char *uid, uid_t *uidOut);

/*
 * file_addGssCredEntry
 *
 * Adds a new entry to the gsscred table.
 * Does not check for duplicate entries.
 */
int file_addGssCredEntry(const gss_buffer_t hexName, const char *uid,
		const char *comment, char **errDetails)
{
	FILE *fp;
	char tmpBuf[256];

	if ((fp = fopen(credFile, "a")) == NULL) {
		if (errDetails) {
			sprintf(tmpBuf,
				gettext("Unable to open gsscred file [%s]"),
				credFile);
			*errDetails = strdup(tmpBuf);
		}
		return (0);
	}

	fprintf(fp, "%s\t%s\t%s\n", (char *)hexName->value, uid, comment);
	fclose(fp);
	return (1);
}  /* *******  file_addGssCredEntry ****** */



/*
 * file_getGssCredEntry
 *
 * Searches the file for the file matching the name.  Since the name
 * contains a mechanism identifier, to search for all names for a given
 * mechanism just supply the mechanism portion in the name buffer.
 * To search by uid only, supply a non-null value of uid.
 */
int file_getGssCredEntry(const gss_buffer_t name, const char *uid,
		char **errDetails)
{
	FILE *fp;
	char entry[MAX_ENTRY_LEN+1];

	if ((fp = fopen(credFile, "r")) == NULL) {

		if (errDetails) {
			sprintf(entry,
				gettext("Unable to open gsscred file [%s]"),
				credFile);
			*errDetails = strdup(entry);
		}

		return (0);
	}

	/* go through the file in sequential order */
	while (fgets(entry, MAX_ENTRY_LEN, fp) != NULL) {
		/* is there any search criteria */
		if (name == NULL && uid == NULL) {
			fprintf(stdout, "%s", entry);
			continue;
		}

		if (matchEntry(entry, name, uid, NULL))
			fprintf(stdout, "%s", entry);

	}	 /* while */

	fclose(fp);
	return (1);
}  /* file_getGssCredEntry */

/*
 * file_getGssCredUid
 *
 * GSS entry point for retrieving user uid information.
 * We need to go through the entire file to ensure that
 * the last matching entry is retrieved - this is because
 * new entries are added to the end, and in case of
 * duplicates we want to get the latest entry.
 */
int
file_getGssCredUid(const gss_buffer_t expName, uid_t *uidOut)
{
	FILE *fp;
	char entry[MAX_ENTRY_LEN+1];
	int retVal = 0;

	if ((fp = fopen(credFile, "r")) == NULL)
		return (0);

	/* go through the entire file in sequential order */
	while (fgets(entry, MAX_ENTRY_LEN, fp) != NULL) {
		if (matchEntry(entry, expName, NULL, uidOut)) {
			retVal = 1;
		}
	} /* while */

	fclose(fp);
	return (retVal);
} /* file_getGssCredUid */



/*
 *
 * file_deleteGssCredEntry
 *
 * removes entries form file that match the delete criteria
 */
int file_deleteGssCredEntry(const gss_buffer_t name, const char *uid,
		char **errDetails)
{
	FILE *fp, *tempFp;
	char entry[MAX_ENTRY_LEN+1];
	int foundOne = 0;

	/* are we deleting everyone? */
	if (name == NULL && uid == NULL) {

		if ((fp = fopen(credFile, "w")) == NULL) {

			if (errDetails) {
				sprintf(entry,
					gettext("Unable to open gsscred"
						" file [%s]"),
					credFile);
				*errDetails = strdup(entry);
			}
			return (0);
		}

		fclose(fp);
		return (1);
	}

	/* selective delete - might still be everyone */
	if ((fp = fopen(credFile, "r")) == NULL) {

		if (errDetails) {
			sprintf(entry,
				gettext("Unable to open gsscred file [%s]"),
				credFile);
			*errDetails = strdup(entry);
		}
		return (0);
	}

	/* also need to open temp file */
	if ((tempFp = fopen(credFileTmp, "w")) == NULL) {
		if (errDetails) {
			sprintf(entry,
				gettext("Unable to open gsscred temporary"
					" file [%s]"),
				credFileTmp);
			*errDetails = strdup(entry);
		}

		fclose(fp);
		return (0);
	}

	/* go through all the entries sequentially removing ones that match */
	while (fgets(entry, MAX_ENTRY_LEN, fp) != NULL) {

		if (!matchEntry(entry, name, uid, NULL))
			fputs(entry, tempFp);
		else
			foundOne = 1;
	}
	fclose(tempFp);
	fclose(fp);

	/* now make the tempfile the gsscred file */
	rename(credFileTmp, credFile);
	unlink(credFileTmp);

	if (!foundOne) {
		*errDetails = strdup(gettext("No users found"));
		return (0);
	}
	return (1);
}  /* file_deleteGssCredEntry */



/*
 *
 * match entry
 *
 * checks if the specified entry matches the supplied criteria
 * returns 1 if yes, 0 if no
 * uidOut value can be used to retrieve the uid from the entry
 * when the uid string is passed in, the uidOut value is not set
 */
static int matchEntry(const char *entry, const gss_buffer_t name,
		const char *uid, uid_t *uidOut)
{
	char fullEntry[MAX_ENTRY_LEN+1], *item;
	char dilims[] = "\t \n";


	if (entry == NULL || isspace(*entry))
		return (0);

	/* save the entry since strtok will chop it up */
	strcpy(fullEntry, entry);

	if ((item = strtok(fullEntry, dilims)) == NULL)
		return (0);

	/* do wee need to search the name */
	if (name != NULL) {
		/* we can match the prefix of the string */
		if (strlen(item) < name->length)
			return (0);

		/* check if the prefix of the name matches */
		if (memcmp(item, name->value, name->length) != 0)
			return (0);

		/* do we need to check the uid - if not then we found it */
		if (uid == NULL) {
			/* do we ned to parse out the uid ? */
			if (uidOut) {
				if ((item = strtok(NULL, dilims)) == NULL)
					return (0);
				*uidOut = atol(item);
			}
			return (1);
		}

		/* continue with checking the uid */
	}

	if (uid == NULL)
		return (1);

	/* get the next token from the string - the uid */
	if ((item = strtok(NULL, dilims)) == NULL)
		return (0);

	if (strcmp(item, uid) == 0)
		return (1);

	return (0);
}  /* *******  matchEntry ****** */
