/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_syntax_standard.cc	1.6	96/03/31 SMI"

#include "FN_syntax_standard.hh"

static FN_identifier
    FN_ASCII_SYNTAX_ID((unsigned char *) "fn_attr_syntax_ascii");
static FN_identifier
    FN_LOCALE_SYNTAX_ID((unsigned char *) "fn_attr_syntax_locale_array");


FN_syntax_standard::~FN_syntax_standard()
{
	if (info.component_separator)
		delete ((FN_string *)info.component_separator);
	if (info.begin_quote)
		delete ((FN_string *)info.begin_quote);
	if (info.end_quote)
		delete ((FN_string *)info.end_quote);
	if (info.begin_quote2)
		delete ((FN_string *)info.begin_quote2);
	if (info.end_quote2)
		delete ((FN_string *)info.end_quote2);
	if (info.escape)
		delete ((FN_string *)info.escape);
	if (info.type_separator)
		delete ((FN_string *)info.type_separator);
	if (info.ava_separator)
		delete ((FN_string *)info.ava_separator);
	if (info.locales)
		delete info.locales;

	info.component_separator = info.escape = 0;
	info.end_quote = info.begin_quote = 0;
	info.end_quote2 = info.begin_quote2 = 0;
	info.type_separator = info.ava_separator = 0;
}


static FN_attr_syntax_locales_t *
copy_locales(const FN_attr_syntax_locales_t *locales)
{
	FN_attr_syntax_locales_t *nl = (FN_attr_syntax_locales_t *)
		(malloc(sizeof (FN_attr_syntax_locales_t)));
	if (nl != NULL) {
		nl->locales = (FN_attr_syntax_locale_info_t *)
			(malloc(locales->num_locales *
				sizeof (FN_attr_syntax_locale_info_t *)));

		if (nl->locales != NULL) {
			int i;
			nl->num_locales = locales->num_locales;
			for (i = 0; i < locales->num_locales; i++) {
				nl->locales[i].code_set =
					locales->locales[i].code_set;
				nl->locales[i].lang_terr =
					locales->locales[i].lang_terr;
			}
		} else {
			free(nl);
			nl = NULL;
		}
	}
	return (nl);
}

void
FN_syntax_standard::common_init(unsigned int dir,
    unsigned int scase,
    const FN_string *sep,
    const FN_string *esc,
    const FN_string *begin_q,
    const FN_string *end_q,
    const FN_string *begin_q2,
    const FN_string *end_q2,
    const FN_string *type_sep,
    const FN_string *ava_sep,
    const FN_attr_syntax_locales_t *locales)
{
	info.direction = dir;
	info.string_case = scase;
	if (sep)
		info.component_separator = (FN_string_t *)new FN_string(*sep);
	else
		info.component_separator = (FN_string_t *)0;
	if (esc)
		info.escape =  (FN_string_t *)new FN_string(*esc);
	else
		info.escape = (FN_string_t *)0;
	if (begin_q)
		info.begin_quote =  (FN_string_t *)new FN_string(*begin_q);
	else
		info.begin_quote = (FN_string_t *)0;
	if (end_q)
		info.end_quote =  (FN_string_t *)new FN_string(*end_q);
	else if (begin_q)
		info.end_quote =  (FN_string_t *)new FN_string(*begin_q);
	else
		info.end_quote = (FN_string_t *)0;
	if (begin_q2)
		info.begin_quote2 =  (FN_string_t *)new FN_string(*begin_q2);
	else
		info.begin_quote2 = (FN_string_t *)0;
	if (end_q2)
		info.end_quote2 =  (FN_string_t *)new FN_string(*end_q2);
	else if (begin_q2)
		info.end_quote2 =  (FN_string_t *)new FN_string(*begin_q2);
	else
		info.end_quote2 = (FN_string_t *)0;

	if (type_sep)
		info.type_separator =  (FN_string_t *)new FN_string(*type_sep);
	else
		info.type_separator = (FN_string_t *)0;
	if (ava_sep)
		info.ava_separator =  (FN_string_t *)new FN_string(*ava_sep);
	else
		info.ava_separator = (FN_string_t *)0;

	if (locales) {
		info.locales = copy_locales(locales);
	} else {
		info.locales = NULL;
	}
}

FN_syntax_standard::FN_syntax_standard(const FN_syntax_standard &s)
{
	common_init(s.info.direction,
		    s.info.string_case,
		    (const FN_string *)s.info.component_separator,
		    (const FN_string *)s.info.escape,
		    (const FN_string *)s.info.begin_quote,
		    (const FN_string *)s.info.end_quote,
		    (const FN_string *)s.info.begin_quote2,
		    (const FN_string *)s.info.end_quote2,
		    (const FN_string *)s.info.type_separator,
		    (const FN_string *)s.info.ava_separator,
		    s.info.locales);
}

FN_syntax_standard::FN_syntax_standard(const FN_syntax_standard_t &si)
{
	common_init(si.direction,
		    si.string_case,
		    (const FN_string *)si.component_separator,
		    (const FN_string *)si.escape,
		    (const FN_string *)si.begin_quote,
		    (const FN_string *)si.end_quote,
		    (const FN_string *)si.begin_quote2,
		    (const FN_string *)si.end_quote2,
		    (const FN_string *)si.type_separator,
		    (const FN_string *)si.ava_separator,
		    si.locales);
}

FN_syntax_standard::FN_syntax_standard(unsigned int dir,
    unsigned int c,
    const FN_string *sep,
    const FN_string *esc,
    const FN_string *begin_q,
    const FN_string *end_q,
    const FN_string *begin_q2,
    const FN_string *end_q2,
    const FN_string *tsep,
    const FN_string *asep,
    unsigned long code_set,
    unsigned long lang_terr)
{
	FN_attr_syntax_locales_t *nl = NULL;

	if (code_set != 0 || lang_terr != 0) {
		nl = (FN_attr_syntax_locales_t *)
			    (malloc(sizeof (FN_attr_syntax_locales_t)));
		if (nl != NULL) {
			nl->locales = (FN_attr_syntax_locale_info_t *)
			    (malloc(sizeof (FN_attr_syntax_locale_info_t *)));
			if (nl->locales != NULL) {
				nl->num_locales = 1;
				nl->locales[0].code_set = code_set;
				nl->locales[0].lang_terr = lang_terr;
			} else {
				delete nl;
				nl = NULL;
			}
		}
		info.locales = nl;
	} else {
		info.locales = NULL;
	}

	common_init(dir, c, sep, esc, begin_q, end_q, begin_q2, end_q2,
	    tsep, asep, nl);

	if (nl != NULL) {
		free(nl->locales);
		free(nl);
	}
}

// Assumes linearization algorithm {num_locales, {[code_set, lang_ter]}+}

static FN_attr_syntax_locales_t *
get_locales_from_attr(const FN_attrset &attrs, const char *attr_id)
{
	void *ip;
	const FN_attribute *attr = attrs.get((unsigned char *)attr_id);
	const FN_attrvalue *val = (attr != NULL ? attr->first(ip) : NULL);
	FN_attr_syntax_locales_t *nl = NULL;

	if (val != NULL) {
		if (*(attr->syntax()) != FN_LOCALE_SYNTAX_ID)
			return (NULL);

		unsigned long *buf = (unsigned long *)(val->contents());
		size_t buf_len = val->length();

		nl = (FN_attr_syntax_locales_t *)
			(malloc(sizeof (FN_attr_syntax_locales_t)));
		if (nl == NULL || buf_len == 0 || buf == NULL ||
		    (buf[0] != ((buf_len - 1)/2))) {
			free(nl);
			return (NULL);
		}

		nl->num_locales = (size_t)(buf[0]);
		nl->locales = (FN_attr_syntax_locale_info_t *)
			(malloc(sizeof (FN_attr_syntax_locale_info_t) *
				nl->num_locales));

		if (nl->locales == NULL) {
			free(nl);
			return (NULL);
		}

		size_t i, b = 1;
		for (i = 0; i < nl->num_locales; i++) {
			nl->locales[i].code_set = buf[b++];
			nl->locales[i].lang_terr = buf[b++];
		}
	}

	return (nl);
}

static FN_string *
get_string_from_attr(const FN_attrset &attrs, const char *attr_id)
{
	void *ip;
	const FN_attribute *attr = attrs.get((unsigned char *)attr_id);
	const FN_attrvalue *val = (attr != NULL ? attr->first(ip) : NULL);

	return (val != NULL ? val->string() : NULL);
}

static unsigned int
find_string_in_attr(const FN_attrset &attrs, const char *attr_id,
    const char *expected_value)
{
	void *ip;
	const FN_attribute *attr = attrs.get((unsigned char *)attr_id);
	const FN_attrvalue *val;
	FN_attrvalue expected_val((unsigned char *)expected_value);

	if (attr == NULL)
		return (FN_E_INVALID_SYNTAX_ATTRS);

	for (val = attr->first(ip); val != NULL; val = attr->next(ip)) {
		if (*val == expected_val) {
			return (FN_SUCCESS);
		}
	}

	return (FN_E_SYNTAX_NOT_SUPPORTED);
}

FN_syntax_standard*
FN_syntax_standard::from_syntax_attrs(const FN_attrset &a, FN_status &s)
{
	unsigned int status =
	    find_string_in_attr(a, "fn_syntax_type", "standard");
	unsigned int c;

	if (status != FN_SUCCESS) {
		s.set_code(status);
		return (0);
	}

	// determine case sensitivity

	if (a.get((unsigned char *)"fn_std_syntax_case_insensitive") != NULL) {
		c = FN_STRING_CASE_INSENSITIVE;
	} else {
		c = FN_STRING_CASE_SENSITIVE;
	}

	// determine syntax direction
	FN_string *dir_str = get_string_from_attr(a, "fn_std_syntax_direction");
	unsigned int dir;
	if (dir_str == NULL) {
		s.set_code(FN_E_INVALID_SYNTAX_ATTRS);
		return (0);
	}
	if (dir_str->compare((unsigned char *)"flat") == 0) {
		dir = FN_SYNTAX_STANDARD_DIRECTION_FLAT;
	} else if (dir_str->compare((unsigned char *)"left_to_right") == 0) {
		dir = FN_SYNTAX_STANDARD_DIRECTION_LTR;
	} else if (dir_str->compare((unsigned char *)"right_to_left") == 0) {
		dir = FN_SYNTAX_STANDARD_DIRECTION_RTL;
	} else {
		delete dir_str;
		s.set_code(FN_E_INVALID_SYNTAX_ATTRS);
		return (0);
	}
	delete dir_str;

	// determine separator (if there should be one)
	FN_string *sep = get_string_from_attr(a, "fn_std_syntax_separator");

	if ((dir == FN_SYNTAX_STANDARD_DIRECTION_FLAT && sep != NULL) ||
	    (dir != FN_SYNTAX_STANDARD_DIRECTION_FLAT && sep == NULL)) {
		delete sep;
		s.set_code(FN_E_INVALID_SYNTAX_ATTRS);
		return (0);
	}

	// determine quotes, if any
	FN_string *begin_q = get_string_from_attr(a,
	    "fn_std_syntax_begin_quote1");
	FN_string *end_q = get_string_from_attr(a, "fn_std_syntax_end_quote1");
	int q_copy = 0;

	if (begin_q == NULL && end_q != NULL) {
		begin_q = end_q;
		q_copy = 1;
	} else if (begin_q != NULL && end_q == NULL) {
		end_q = begin_q;
		q_copy = 1;
	}

	FN_string *begin_q2 = get_string_from_attr(a,
	    "fn_std_syntax_begin_quote2");
	FN_string *end_q2 = get_string_from_attr(a, "fn_std_syntax_end_quote2");
	int q2_copy = 0;

	if (begin_q2 == NULL && end_q2 != NULL) {
		begin_q2 = end_q2;
		q2_copy = 1;
	} else if (begin_q2 != NULL && end_q2 == NULL) {
		end_q2 = begin_q2;
		q2_copy = 1;
	}

	// determine escape if any
	FN_string *esc = get_string_from_attr(a, "fn_std_syntax_escape");

	// determine type value separator, if any
	FN_string *type_sep = get_string_from_attr(a,
	    "fn_std_syntax_typeval_separator");

	// determine ava separator, if any
	FN_string *ava_sep = get_string_from_attr(a,
	    "fn_std_syntax_ava_separator");

	// construct syntax object
	FN_syntax_standard *ret =
	    new FN_syntax_standard(dir, c, sep, esc, begin_q, end_q,
	    begin_q2, end_q2, type_sep, ava_sep);

	// determine locales, if any
	FN_attr_syntax_locales_t *nl = get_locales_from_attr(a,
	    "fn_std_syntax_locales");
	if (nl) {
		ret->set_locales(nl);
		// don't free nl; consumed by set_locales
	}

	// cleanup
	delete sep;
	delete esc;
	delete begin_q;
	if (q_copy == 0)
		delete end_q;
	delete begin_q2;
	if (q2_copy == 0)
		delete end_q2;
	delete type_sep;
	delete ava_sep;

	if (ret == 0) {
		s.set_code(FN_E_INSUFFICIENT_RESOURCES);
	} else {
		s.set_success();
	}
	return (ret);
}

static void
add_syntax_attr(FN_attrset *as, const char *attr_id,
    const FN_string *valstr = 0)
{
	FN_attribute attr((unsigned char *)attr_id, FN_ASCII_SYNTAX_ID);

	if (valstr) {
		FN_attrvalue attrval(*valstr);
		attr.add(attrval);
	}

	as->add(attr);
}


// Uses linearization algorithm {num_locales, {[code_set, lang_ter]}+}

static void
add_locale_attr(FN_attrset *as, const char *attr_id,
	const FN_attr_syntax_locales_t *locales)
{
	FN_attribute attr((unsigned char *)attr_id, FN_LOCALE_SYNTAX_ID);

	// allocate space for count and locale array

	size_t buf_len = (1 + (locales->num_locales * 2)) *
	    sizeof (unsigned long);
	unsigned long *buf = (unsigned long *)(malloc(buf_len));

	buf[0] = (unsigned long)(locales->num_locales);
	int i;
	for (i = 0; i < locales->num_locales; i++) {
		buf[(i * 2)+1] = locales->locales[i].code_set;
		buf[(i * 2)+2] = locales->locales[i].lang_terr;
	}

	FN_attrvalue val((const void*)buf, buf_len);
	attr.add(val);
	as->add(attr);

	free(buf);
}

FN_attrset*
FN_syntax_standard::get_syntax_attrs() const
{
	FN_attrset *ret = new FN_attrset();
	if (ret == 0)
		return (0);

	FN_string sstr((unsigned char *)"standard");
	add_syntax_attr(ret, "fn_syntax_type", &sstr);

	FN_string dir;
	switch (direction()) {
	case FN_SYNTAX_STANDARD_DIRECTION_FLAT:
		dir = ((unsigned char *)"flat");
		break;
	case FN_SYNTAX_STANDARD_DIRECTION_LTR:
		dir = ((unsigned char *)"left_to_right");
		break;
	case FN_SYNTAX_STANDARD_DIRECTION_RTL:
		dir = ((unsigned char *)"right_to_left");
		break;
	}
	add_syntax_attr(ret, "fn_std_syntax_direction", &dir);

	if (component_separator()) {
		add_syntax_attr(ret, "fn_std_syntax_separator",
		    component_separator());
	}

	if (begin_quote()) {
		add_syntax_attr(ret, "fn_std_syntax_begin_quote1",
		    begin_quote());
	}
	if (end_quote()) {
		add_syntax_attr(ret, "fn_std_syntax_end_quote1",
		    end_quote());
	}

	if (begin_quote2()) {
		add_syntax_attr(ret, "fn_std_syntax_begin_quote2",
		    begin_quote2());
	}
	if (end_quote2()) {
		add_syntax_attr(ret, "fn_std_syntax_end_quote2",
		    end_quote2());
	}

	if (escape()) {
		add_syntax_attr(ret, "fn_std_syntax_escape", escape());
	}

	if (string_case() == FN_STRING_CASE_INSENSITIVE) {
		add_syntax_attr(ret, "fn_std_syntax_case_insensitive");
	}

	if (ava_separator()) {
		add_syntax_attr(ret, "fn_std_syntax_separator",
		    ava_separator());
	}

	if (type_separator()) {
		add_syntax_attr(ret, "fn_std_syntax_typeval_separator",
		    type_separator());
	}

	if (locales()) {
		add_locale_attr(ret, "fn_std_syntax_locales", locales());
	}

	return (ret);
}

unsigned int
FN_syntax_standard::direction(void) const
{
	return (info.direction);
}

unsigned int
FN_syntax_standard:: string_case(void) const
{
	return (info.string_case);
}

const FN_string *
FN_syntax_standard::component_separator(void) const
{
	return ((const FN_string *)info.component_separator);
}

const FN_string *
FN_syntax_standard::begin_quote(void) const
{
	return ((const FN_string *)info.begin_quote);
}

const FN_string *
FN_syntax_standard::end_quote(void) const
{
	return ((const FN_string *)info.end_quote);
}

const FN_string *
FN_syntax_standard::begin_quote2(void) const
{
	return ((const FN_string *)info.begin_quote2);
}

const FN_string *
FN_syntax_standard::end_quote2(void) const
{
	return ((const FN_string *)info.end_quote2);
}

const FN_string *
FN_syntax_standard::escape(void) const
{
	return ((const FN_string *)info.escape);
}

const FN_string *
FN_syntax_standard::type_separator(void) const
{
	return ((const FN_string *)info.type_separator);
}

const FN_string *
FN_syntax_standard::ava_separator(void) const
{
	return ((const FN_string *)info.ava_separator);
}

const FN_attr_syntax_locales_t *
FN_syntax_standard::locales(void) const
{
	return (info.locales);
}

void
FN_syntax_standard::set_locales(FN_attr_syntax_locales_t *locales)
{
	if (info.locales) {
		free(info.locales->locales);
		free(info.locales);
	}

	info.locales = locales;
}
