/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fn_links.cc	1.1	96/03/31 SMI"
#include <xfn/fn_spi.hh>
#include "fn_links.hh"
#include "FN_ctx_func_info.hh"

static const FN_string FN_RELATIVE_INDICATOR((unsigned char *)".");
static int FN_SPI_LINK_LIMIT  = 4;

// Returns 1 if 'lname' is a relative link (i.e. starts off with '.')
// Set 'rest' to remaining part of 'lname' occurring after '.'
static int
is_relative_link(const FN_composite_name &lname, FN_composite_name **rest = 0)
{
	void *iter_pos;
	const FN_string *fnstr = lname.first(iter_pos);
	if (fnstr == 0) {
		return (0);
	}
	if (fnstr->compare(FN_RELATIVE_INDICATOR) == 0) {
		if (rest)
			*rest = lname.suffix(iter_pos);
		return (1);
	}
	return (0);
}

// A stack is maintained to keep track of the possibly many states of
// link resolution.  When a new link is encountered during the resolution
// of another link, it is pushed onto the stack.  When resolution of
// a link completes, another one is popped off the stack to continue
// the resultion.  A limit is kept on the size of the stack to prevent loops.

class FN_link_stack
{
private:
	int stack_top;
	FN_composite_name **stack;
public:
	FN_link_stack();
	~FN_link_stack();

	FN_composite_name *pop();
	int push(const FN_composite_name &);
	int is_empty(void);
};

FN_link_stack::FN_link_stack()
{
	stack_top = -1;  /* empty */
	stack = new FN_composite_name*[FN_SPI_LINK_LIMIT];
}

FN_link_stack::~FN_link_stack()
{
	FN_composite_name *stack_val;

	// clear stack contents first
	while (stack_val = pop())
		delete stack_val;

	delete [] stack;
}

FN_composite_name *
FN_link_stack::pop()
{
	if (stack_top >= 0) {
		FN_composite_name *stack_val = stack[stack_top--];
		return (stack_val);
	}
	return (0);
}

int
FN_link_stack::push(const FN_composite_name &item)
{
	if (stack_top < (FN_SPI_LINK_LIMIT - 1)) {
		stack[++stack_top] = new FN_composite_name(item);
		return (1);
	}
	return (0);
}

int
FN_link_stack::is_empty()
{
	return (stack_top == -1);
}


static inline int
should_follow_link(const FN_status &s)
{
	return (s.code() == FN_E_SPI_FOLLOW_LINK);
}

// Resolve given name
// If resolved successfully,
//	return resolved reference as return value
// 	set more_link to TRUE if resolved referece is another link
// 	set 'relative_ref' to point to penultimate context for
//	for resolving relative links
//

static FN_ref*
resolve_in_ctx(FN_ctx* ctx, const FN_composite_name &name,
    unsigned int &more_link,
    FN_ref *&relative_ref,
    FN_status &status)
{
	FN_ref *ref = ctx->lookup(name, status);

	more_link = (ref && ref->is_link());

	if (status.resolved_ref())
		relative_ref = new FN_ref(*status.resolved_ref());
	else
		relative_ref = 0; // %%% potential problem

	return (ref);
}

// Resolve given name until one of the following conditions are met
// 1.  given name is completely resolved
// 		return resolved reference as return value;
//		more_link is set to TRUE if resolved reference is a link
//		set 'relative_ref' to penultimate context for resolving
//		relative links
// 2.  another link is encountered in the middle of given name
//		return NULL as return value
//		more_link is set to TRUE;
// 		status contains information for following the link
// 3.  an error (other than FN_E_SPI_CONTINUE) is encountered
//		return NULL as return value
// 		more_link is set to FALSE
// 		status contains information on error
//
static FN_ref*
resolve_in_ctx_svc(FN_ctx_svc* ctx, const FN_composite_name &name,
    unsigned int authoritative,
    unsigned int &more_link,
    FN_ref *&relative_ref,
    FN_status &status)
{
	FN_status_psvc pstatus(status);
	FN_ref *ref = ctx->p_lookup(name,
	    FN_SPI_LEAVE_TERMINAL_LINK, pstatus);

	while (pstatus.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *nctx;
		FN_status from_stat;
		if (!(nctx = FN_ctx_svc::from_ref(*pstatus.resolved_ref(),
		    authoritative, from_stat))) {
			pstatus.set_code(from_stat.code());
			break;
		}
		FN_composite_name rn = *pstatus.remaining_name();
		ref = nctx->p_lookup(rn, FN_SPI_LEAVE_TERMINAL_LINK, pstatus);
		delete nctx;
	}

	more_link = ((ref && ref->is_link()) || should_follow_link(pstatus));

	if (should_follow_link(pstatus)) {
		if (pstatus.resolved_ref())
			ref = new FN_ref(*pstatus.resolved_ref());
		if (pstatus.link_resolved_ref())
			relative_ref = new FN_ref(*pstatus.link_resolved_ref());
		else
			relative_ref = 0; // for links relative to IC
	} else {
		if (pstatus.resolved_ref())
			relative_ref = new FN_ref(*pstatus.resolved_ref());
		else
			relative_ref = 0; // %%% potential problem
	}
	return (ref);
}

// Resolve 'name' in context of 'last_resolved_ref'.
// Set more_link to indicate whether this resolution another link
static FN_ref *
resolve_aux(const FN_composite_name &name,
	    const FN_ref *last_resolved_ref,
	    unsigned int authoritative,
	    unsigned int &more_link,
	    FN_status_psvc &ps)
{
	FN_status rs;
	FN_ctx_svc *sctx;
	FN_status_psvc ps2(rs);
	FN_ref *ref;
	FN_ref *last_relative_ref;

	// If context does not use framework (i.e. cannot get
	// FN_ctx_svc handle), use FN_ctx handle (this will
	// not allow for complete link limit detection)

	if (last_resolved_ref != NULL)
		sctx = FN_ctx_svc::from_ref(*last_resolved_ref,
		    authoritative, rs);
	else
		sctx = FN_ctx_svc::from_initial(authoritative, ps2);

	if (sctx) {
		ref = resolve_in_ctx_svc(sctx, name, authoritative,
		    more_link, last_relative_ref, rs);
		delete sctx;
	} else {
		FN_ctx *ctx;
		if (last_resolved_ref != NULL)
			ctx = FN_ctx::from_ref(*last_resolved_ref,
			    authoritative, rs);
		else
			ctx = FN_ctx::from_initial(authoritative, rs);

		// this lookup may involve links
		// that are not counted here

		if (ctx == NULL) {
			ps = rs;
			more_link = 0;
			return (NULL);
		}

		ref = resolve_in_ctx(ctx, name, more_link,
		    last_relative_ref, rs);
		delete ctx;
	}

	if (more_link && ref != NULL) {
		ps.set_resolved_ref(ref);
		ps.set_link_resolved_ref(last_relative_ref);
	}
	delete last_relative_ref;

	// if not making progress
	if (!more_link && ref == NULL) {
		ps = rs;
	}

	return (ref);
}


// Perform operation on given name, stops when
// 1.  operation on name is completed
// 2.  another link is encountered in the middle of given name
//		return NULL as return value
//		more_link is set to TRUE;
// 		status contains information for following the link
// 3.  an error (other than FN_E_SPI_CONTINUE) is encountered
//		return NULL as return value
// 		more_link is set to FALSE
// 		status contains information on error
//

static void
operate_in_ctx_svc(FN_ctx_svc* ctx, const FN_composite_name &name,
    unsigned int authoritative,
    unsigned int &more_link,
    FN_status_psvc &pstatus,
    FN_ctx_func_info_t *packet)
{
	ctx_svc_exec_func(ctx, name, pstatus, packet);

	while (pstatus.code() == FN_E_SPI_CONTINUE) {
		FN_ctx_svc *nctx;
		FN_status from_stat;
		if (!(nctx = FN_ctx_svc::from_ref(*pstatus.resolved_ref(),
		    authoritative, from_stat))) {
			pstatus.set_code(from_stat.code());
			break;
		}
		FN_composite_name rn = *pstatus.remaining_name();
		ctx_svc_exec_func(nctx, rn, pstatus, packet);
		delete nctx;
	}

	more_link = should_follow_link(pstatus);
}

// Resolve 'name' in context of 'last_resolved_ref'.
// Set more_link to indicate whether this resolution another link
static void
operate_aux(const FN_composite_name &name,
	    const FN_ref *last_resolved_ref,
	    unsigned int authoritative,
	    unsigned int &more_link,
	    FN_status_psvc &ps,
	    FN_ctx_func_info_t *packet)
{
	FN_status rs;
	FN_ctx_svc *sctx;
	FN_status_psvc ps2(rs);

	// If context does not use framework (i.e. cannot get
	// FN_ctx_svc handle), use FN_ctx handle (this will
	// not allow for complete link limit detection)

	if (last_resolved_ref != NULL)
		sctx = FN_ctx_svc::from_ref(*last_resolved_ref,
		    authoritative, rs);
	else
		sctx = FN_ctx_svc::from_initial(authoritative, ps2);

	if (sctx) {
		operate_in_ctx_svc(sctx, name, authoritative, more_link,
		    ps, packet);
		delete sctx;
	} else {
		FN_ctx *ctx;
		if (last_resolved_ref != NULL)
			ctx = FN_ctx::from_ref(*last_resolved_ref,
			    authoritative, rs);
		else
			ctx = FN_ctx::from_initial(authoritative, rs);

		// this lookup may involve links
		// that are not counted here

		if (ctx == NULL) {
			ps = rs;
		} else {
			ctx_exec_func(ctx, name, ps, packet);
			delete ctx;
		}
		// not possible to have more links using cxt interface
		more_link = 0;
	}
}

// Do link resolution with limit detection for those contexts that
// use FN_ctx_svc and its subclasses.
// For those contexts that use other service classes, link loop/limit
// detection is delegated and controlled by those classes.
//
// This routine takes the XFN link in 'link_ref' and resolves it.
// If it is a relative link, it is resolved relative to 'relative_ref'.

static FN_ref *
follow_link(const FN_ref *link_ref,
    const FN_ref *relative_ref,
    unsigned int authoritative,
    FN_status_psvc &ps,
    FN_ctx_func_info_t *packet = NULL)
{
	FN_link_stack lstack;
	unsigned int more_link = 1;
	unsigned int link_count = 0;	// number of links traversed
	FN_ref *last_resolved_ref = 0;
	FN_ref *ret = 0;

	// Initialize status object with link information
	ps.set_resolved_ref(link_ref);
	ps.set_link_resolved_ref(relative_ref);

	// keep resolving links while stack is not empty (indicates
	// trailing unresolved components -- link occurred in middle of name)
	// or a link is being followed (in case of terminal link)

	while (!lstack.is_empty() ||
	    ((link_count < FN_SPI_LINK_LIMIT) && more_link)) {
		if (more_link) {
			// will be using ref from status object
			delete last_resolved_ref;
			last_resolved_ref = NULL;

			++link_count;
			// 1.  Push unresolved part onto stack for
			// later processing
			const FN_composite_name *rn = ps.remaining_name();
			if (rn) {
				lstack.push(*rn);
			}

			// 2.  Get link name from status object
			const FN_ref *ref = 0;
			FN_composite_name *lname = 0;
			FN_composite_name *rest = 0;

			if ((ref = ps.resolved_ref()) &&
			    (lname = ref->link_name()))
				;
			else {
				// something is wrong; could not get link
				// name from status
				// %%% rl: what else need to be passed back
				ps.set(FN_E_MALFORMED_LINK);
				break;
			}

			// 3. If first component is ".", resolve relative to
			// link_resolved_ref; otherwise, resolve relative to IC
			if (is_relative_link(*lname, &rest)) {
				if (ps.link_resolved_ref() == NULL) {
					// no starting point;
					// %%% more appropriate error code?
					// %%% what other status fields need
					// to be passed back?
					ps.set(FN_E_MALFORMED_LINK);
					break;
				}
				delete lname;
				FN_ref *rel_ref =
					new FN_ref(*ps.link_resolved_ref());

				if (packet != NULL && lstack.is_empty())
					operate_aux(*rest, rel_ref,
					    authoritative, more_link, ps,
					    packet);
				else
					last_resolved_ref = resolve_aux(*rest,
					    rel_ref, authoritative, more_link,
					    ps);
				delete rest;
				delete rel_ref;
			} else {
				if (packet && lstack.is_empty())
					operate_aux(*lname, NULL, authoritative,
					    more_link, ps, packet);
				else
				// Absolute link to be resolved relative to IC
					last_resolved_ref = resolve_aux(*lname,
					    NULL, authoritative, more_link, ps);
				delete lname;
			}
		} else if (!lstack.is_empty() && last_resolved_ref) {
			FN_composite_name *next_name = lstack.pop();
			if (next_name) {
				if (packet && lstack.is_empty()) {
					operate_aux(*next_name,
					    last_resolved_ref,
					    authoritative,
					    more_link, ps, packet);
					delete last_resolved_ref;
					last_resolved_ref = NULL;
				} else {
					FN_ref *ref = resolve_aux(*next_name,
					    last_resolved_ref, authoritative,
					    more_link, ps);
					delete last_resolved_ref;
					delete next_name;
					last_resolved_ref = ref;
				}
			} else {
				// probably something wrong here;
				// null item on stack
				// %%% more appropriate error code here?
				ps.set(FN_E_LINK_ERROR);
				if (last_resolved_ref) {
					delete last_resolved_ref;
					last_resolved_ref = NULL;
				}
				break;
			}
		} else if (lstack.is_empty())
			break; // all done
		else {
			// something is wrong here
			// we are not following links,
			// or, either no trailing components left,
			// or, previous resolve failed.
			ps.set(FN_E_LINK_ERROR);
			break;
		}
	}

	if (link_count >= FN_SPI_LINK_LIMIT && more_link) {
		ps.set(FN_E_LINK_LOOP_LIMIT, last_resolved_ref);
		// %%% what other information is needed here
		ret = 0;
	} else if (packet == NULL && lstack.is_empty() && last_resolved_ref) {
		ps.set_success();
		ret = last_resolved_ref;
	} else {
		delete last_resolved_ref;
	}

	return (ret);
}

// It does not change s.remaining_name(); that needs to be deal with
// by the caller of this routine how to continue the operation on
// the remaining name.  This is so that this routine does not resolve
// more than it intends to (it does not know which is the target context
// and when to stop resolution).
//

int
fn_process_link(FN_status &s,
    unsigned int authoritative,
    unsigned int continue_code,
    FN_ref **answer)
{
	if (should_follow_link(s)) {
		FN_status_psvc lstatus;
		FN_ref *lref = follow_link(s.resolved_ref(),
		    s.link_resolved_ref(), authoritative, lstatus);

		if (lstatus.is_success()) {
			s.set_resolved_ref(lref);
			if (s.remaining_name())
				s.set_code(FN_E_SPI_CONTINUE);
			else {
				s.set_code(continue_code);
				// for continue case; need to supply empty name
				if (continue_code == FN_E_SPI_CONTINUE)
					s.append_remaining_name(
					    (unsigned char *)"");
			}
			if (answer)
				*answer = lref;
			else
				delete lref;
		} else {
			FN_status_psvc temp(s);
			temp.set_link_error(lstatus);
		}
	}
	return (1);
}


// It does not change s.remaining_name(); that needs to be deal with
// by the caller of this routine how to continue the operation on
// the remaining name.  This is so that this routine does not resolve
// more than it intends to (it does not know which is the target context
// and when to stop resolution).

int
fn_attr_process_link(FN_status &s,
    unsigned int authoritative,
    unsigned int follow_flag,
    FN_ctx_func_info_t *packet)
{
	if (should_follow_link(s)) {
		if (follow_flag && s.remaining_name() == NULL) {
			// If following terminal link, perform operation
			// after resolving link
			FN_status_psvc lstatus;
			follow_link(s.resolved_ref(),
			    s.link_resolved_ref(), authoritative, lstatus,
			    packet);

			if (lstatus.is_success()) {
				s.set_success();
			} else {
				FN_status_psvc temp(s);
				temp.set_link_error(lstatus);
			}
		} else {
			// otherwise, traverse link
			return (fn_process_link(s, authoritative));
		}
	}
	return (1);
}
