/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Domestic_Export.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.export;

import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Debug;

public class AMI_Domestic_Export extends AMI_Export {
  
	public int getMaxDHKeySize() {
        	try {
                	AMI_Debug.debugln(3,
			    "Getting DH key size from global lib");
		} catch (Exception e) {
			// do nothing
		}
		return (AMI_Constants.AMI_MAX_DH_DOMESTIC_KEYSIZE);
	}
}
