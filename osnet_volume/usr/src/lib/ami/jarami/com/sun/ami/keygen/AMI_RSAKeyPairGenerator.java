/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RSAKeyPairGenerator.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.lang.*;
import java.security.*;
import java.security.spec.*;
import java.math.BigInteger;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Log;
import com.sun.ami.common.AMI_Debug;

/**
 * This class implements the RSA key generation methods.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see KeyPairGenerator
 * @see KeyPairGeneratorSpi
 *
 */

public class AMI_RSAKeyPairGenerator extends KeyPairGenerator 
{
    // Number of bytes in seed as recommended by RSA
    private final int NUM_RANDOM_BYTES = 256;
    private final int MIN_KEY_SIZE = 256; // minimum Keysize for RSA

    public AMI_RSAKeyPairGenerator()
    {
	super("RSA");
    }

    // Main key generation method
    public KeyPair generateKeyPair()
    {
        if (_random == null)
            _random = new SecureRandom();

        if (_modulus == 0 || _exponent == null || _len == 0)
        {
	  // Do default initialisation of the paramteres
            setParams(new AMI_RSAParameterSpec());               
        }
        return generateKeyPair(_modulus, _exponent, _len, _random);
    }

    public KeyPair generateKeyPair(int modulus, BigInteger exponent,
				int len, SecureRandom secureRandom)
    {
        AMI_KeyGen keygen  = null;
	AMI_RSAPrivateKey rsaPrivateKey = null;
	AMI_RSAPublicKey rsaPublicKey = null;

	byte[] seed = new byte[NUM_RANDOM_BYTES];

	secureRandom.nextBytes(seed);

	try {
		AMI_Debug.debugln(2,
			"AMI_RSAKeyPairGenerator::In generateKeyPair");
	} catch (Exception e) {
	   throw new RuntimeException(e.toString());
	}

        try {
            keygen = new AMI_KeyGen();
	} catch (AMI_Exception amie) {
	   try {
		   AMI_Debug.debugln(1, "AMI_RSAKeyPairGenerator::" +
				"Unable to initialise ski: " + amie.toString());
	   } catch (Exception e) {
	   	throw new RuntimeException(e.toString());
	   }
	   throw new RuntimeException(amie.toString());
	}

	// Invoke the Java native wrapper for rsa key generation
	try {
         	keygen.ami_gen_rsa_keypair(modulus,
			exponent.toString(16).getBytes(), len, seed);
	} catch (AMI_KeyGenException amie) {
	   try {
	   	AMI_Debug.debugln(1, "AMI_RSAKeyPairGenerator::" +
		  "Unable to generate keys: " + amie.toString());		
	   } catch (Exception e) {
	   	throw new RuntimeException(e.toString());
	   }
	   throw new RuntimeException(amie.getLocalizedMessage());
	}

	// Create a key pair for the generated keys
 	
	try {
		rsaPrivateKey = new AMI_RSAPrivateKey(keygen);
		rsaPublicKey = new AMI_RSAPublicKey(keygen);
	} catch (Exception e) {
		e.printStackTrace();
		throw new RuntimeException(e.toString());
	} 

	KeyPair keyPair = new KeyPair(rsaPublicKey, rsaPrivateKey);
        
	try {
		Object[] messageArguments = {
		    new String("RSA"),
		    new Integer(modulus)
		};
		AMI_Log.writeLog(1, "AMI_Keygen.generate", messageArguments);
	} catch (Exception e) {
		throw new RuntimeException(e.toString());
	}  
	// returning the key pair
	return keyPair;

    }

    public void initialize(int keysize, SecureRandom secureRandom) 
    {      
        _random = secureRandom;
        setParams(new AMI_RSAParameterSpec(keysize));    
    }

    // Initialise the RSA parameters
    public void initialize(AlgorithmParameterSpec algorithmParameterSpec,
			 SecureRandom secureRandom)
        throws InvalidAlgorithmParameterException
    {
        if (!(algorithmParameterSpec instanceof AMI_RSAParameterSpec))
            throw new 
		InvalidAlgorithmParameterException("Inappropriate parameter");

        _random = secureRandom;
        setParams((AMI_RSAParameterSpec)algorithmParameterSpec);
    }

    private void setParams(AMI_RSAParameterSpec rSAParameterSpec)
    {
        try {
                AMI_Debug.debugln(1, "AMI_RSAKeyPairGenerator::In set params");
        } catch (Exception e) {
                throw new RuntimeException(e.toString());
        }

        _modulus = rSAParameterSpec.getM();
        if (_modulus < MIN_KEY_SIZE) 
          throw new RuntimeException("RSA keysize cannot be less than 256");
        _exponent = rSAParameterSpec.getE();
        _len = rSAParameterSpec.getLen();
    }


    private int _modulus;
    private BigInteger _exponent;
    private int _len;
    private SecureRandom _random;
}
