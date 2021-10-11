/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RC4KeyGenerator.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.security.SecureRandom;
import java.math.BigInteger;
import java.security.InvalidParameterException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.spec.AlgorithmParameterSpec;
import javax.crypto.KeyGeneratorSpi;
import javax.crypto.SecretKey;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;

public final class AMI_RC4KeyGenerator extends KeyGeneratorSpi {
    
    /**
     * Initializes this key generator.
     * 
     * @param random the source of randomness for this generator
     */
    protected void engineInit(SecureRandom random) {
	_random = random;
    }

    /**
     * Initializes this key generator with the specified parameter
     * set and a user-provided source of randomness.
     *
     * @param params the key generation parameters
     * @param random the source of randomness for this key generator
     *
     * @exception InvalidAlgorithmParameterException if <code>params</code> is
     * inappropriate for this key generator
     */
    protected void engineInit(AlgorithmParameterSpec params,
	SecureRandom random) throws InvalidAlgorithmParameterException {
	    throw new InvalidAlgorithmParameterException(
		"RC4 key generation does not take any parameters");
    }

    /**
     * Initializes this key generator for a certain strength, using the given
     * source of randomness.
     *
     * @param strength the strength of the key. This is an algorithm-specific
     * metric specified in number of bits.
     * @param random the source of randomness for this key generator
     */
    protected void engineInit(int strength, SecureRandom random) {
	_strength = strength;
	_random = random;
    }

    /**
     * Generates the RC4 key.
     *
     * @return the new RC4 key
     */
    protected SecretKey engineGenerateKey() {

	byte[] randomBytes = new byte[_strength/8];

	if (_random == null) {
		// generate a new random number
                _random = new SecureRandom();
	}

	_random.nextBytes(randomBytes);

	AMI_RC4Key rc4Key = new AMI_RC4Key(new BigInteger(randomBytes));

        try {
                Object[] messageArguments = {
                    new String("RC4"),
                    new Integer(_strength)
                };
                AMI_Log.writeLog(1, "AMI_Keygen.generate", messageArguments);
        } catch (Exception e) {
                throw new RuntimeException(e.toString());
        }

	// returning the secret key
	return (rc4Key);
    }

    private SecureRandom _random = null;
    private int          _strength = 40;  // Key size in bits
}
