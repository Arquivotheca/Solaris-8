/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnattr_utils.cc	1.1	96/04/05 SMI"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libintl.h>
#include <xfn/xfn.hh>
#include "fnattr_utils.hh"

static const FN_identifier
attribute_syntax((const unsigned char *)"fn_attr_syntax_ascii");


char *
convert_to_char(int length, const void *contents)
{
	char *buf, *bp;
	const unsigned char *p;
	int l, i, j, dl;

	l = length;
	if (l > 100)
		dl = 100;
	else
		dl = l;
	int size = 70 + (l / 10) * 80; // formated data
	buf = new char[size];
	bp = buf;
	strcpy(bp, "\n        ");
	bp += 9;
	for (p = (const unsigned char *) (contents), i = 0; i < dl; ) {
		for (j = 0; j < 10 && i+j < dl; j++) {
			sprintf(bp, "0x%.2x ", p[i+j]);
			bp += 5;
		}
		for (; j < 10; j++) {
			strcpy(bp, "     ");
			bp += 5;
		}
		*bp++ = ' ';
		for (j = 0; j < 10 && i < dl; j++, i++)
			*bp++ = (isprint(p[i]))?p[i]:'.';
		if (i < dl) {
			strcpy(bp, "\n         ");
			bp += 9;
		}
	}
	if (dl < l) {
		strcpy(bp, "\n         ...");
		bp += 12;
	}

	*bp++ = '\n';
	*bp = 0;

	return (buf);
}

void
print_attribute(const FN_attribute *attribute, FILE *outstream)
{
	void *ip;
	const FN_identifier *syntax;
	const FN_identifier *identifier;
	const FN_attrvalue *attrvalue;

	// An empty line to start printing the output
	fprintf(outstream, "\n");

	// Print the identifier format and contents: flags UUID and OID
	identifier = attribute->identifier();
	unsigned int format = identifier->format();
	switch (format) {
	case FN_ID_STRING:
		fprintf(outstream, "%s: %s\n",
			gettext("Identifer"),
			identifier->str());
		break;
	case FN_ID_DCE_UUID:
		fprintf(outstream, "%s: %s\n",
			gettext("UUID Identifier"),
			identifier->str());
		break;
	case FN_ID_ISO_OID_STRING:
		fprintf(outstream, "%s: %s\n",
			gettext("Object Identifier"),
			identifier->str());
		break;
	default:
		fprintf(outstream, "%s\n",
		    gettext("Unknown Identifier Format"));
		fprintf(outstream, "%s: %s\n", gettext("Identifer"),
		    convert_to_char(identifier->length(),
		    identifier->contents()));
		break;
	}

	// Print attribute syntax format and contents: flags UUID or OID
	syntax = attribute->syntax();
	format = syntax->format();
	unsigned int not_ascii_format = 1;
	switch (format) {
	case FN_ID_STRING:
		// fprintf(outstream, "Syntax Format: FN_ID_STRING\n");
		if ((*syntax) != attribute_syntax)
			fprintf(outstream, "%s: %s\n",
			    gettext("Attribute Syntax"), syntax->str());
		else
			not_ascii_format = 0;
		break;
	case FN_ID_DCE_UUID:
		fprintf(outstream, "%s\n",
			gettext("Syntax Format: FN_ID_DCE_UUID"));
		fprintf(outstream, "%s: %s\n", gettext("Attribute Syntax"),
			syntax->str());
		break;
	case FN_ID_ISO_OID_STRING:
		fprintf(outstream, "%s\n",
			gettext("Syntax Format: FN_ID_ISO_OID_STRING"));
		fprintf(outstream, "%s: %s\n",
			gettext("Attribute Syntax"),
			syntax->str());
		break;
	default:
		fprintf(outstream, "%s\n", gettext("Unknown Syntax Format"));
		fprintf(outstream, "%s: %s\n", gettext("Attribute Syntax"),
		    convert_to_char(syntax->length(), syntax->contents()));
		break;
	}

	// Print the attribute values
	attrvalue = attribute->first(ip);
	while (attrvalue) {
		if (not_ascii_format) {
			fprintf(outstream, "%s:   %s\n",
				gettext("Value"),
				convert_to_char(attrvalue->length(),
						attrvalue->contents()));
		} else {
			fprintf(outstream, "%s: %s\n",
				gettext("Value"),
				attrvalue->string()->str());
		}
		attrvalue = attribute->next(ip);
	}
}

void
print_attrset(const FN_attrset *attrset, FILE *outstream)
{
	const FN_attribute *attr;
	void *ip;
	for (attr = attrset->first(ip);
	    attr != NULL;
	    attr = attrset->next(ip))
		print_attribute(attr, outstream);
}
