/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RSAParams.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.math.BigInteger;

/**
 * Interface to the RSA specific set of key paramteres.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */


public interface AMI_RSAParams 
{

    /**
     * Returns the modulus size <code>m</code>.
     *
     * @return the modulus size <code>m</code>.
     */
    public int getM();

    /**
     * Returns the exponent <code>e</code>.
     *
     * @return the exponent <code>e</code>.
     */
    public BigInteger getE();

    /**
     * Returns the length <code>len</code> of the hex 
     * representaion of the exponent.
     *
     * @return the exponent <code>len</code>.
     */
    public int getLen();
}
