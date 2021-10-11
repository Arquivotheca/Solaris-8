/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)DerEncoder.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.io.IOException;
import java.io.OutputStream;

/**
 * Interface to an object that knows how to write its own DER 
 * encoding to an output stream.
 *
 * @version 1.3 98/08/24
 * @author D. N. Hoover
 */
public interface DerEncoder {
    
    /**
     * DER encode this object and write the results to a stream.
     *
     * @param out  the stream on which the DER encoding is written.
     */
    public void derEncode(OutputStream out) 
	throws IOException;

}
