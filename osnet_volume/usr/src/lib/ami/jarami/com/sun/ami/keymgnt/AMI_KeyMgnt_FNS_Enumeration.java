/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_KeyMgnt_FNS_Enumeration.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import com.sun.ami.common.AMI_Common;

class AMI_KeyMgnt_FNS_Enumeration extends AMI_Common
    implements Enumeration {

	native String fns_list_next(String attrID, String attrValue);

	String attrID, attrValue;
	String list_ctx;
	boolean next_status;
	String next_object;
	byte[] context;

	AMI_KeyMgnt_FNS_Enumeration(byte[] ctx, String id, String value) {
		context = ctx;
		attrID = new String(id);
		attrValue = new String(value);
		list_ctx = null;
		updateNextObject();
	}

	void updateNextObject() {
		next_object = fns_list_next(attrID, attrValue);
		if (next_object == null)
			next_status = false;
		else
			next_status = true;
	}

	public boolean hasMoreElements() {
		return (next_status);
	}

	public Object nextElement() {
		String answer = new String(next_object);
		updateNextObject();
		return (answer);
	}
}
