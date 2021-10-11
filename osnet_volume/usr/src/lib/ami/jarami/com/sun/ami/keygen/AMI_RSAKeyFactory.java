/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RSAKeyFactory.java	1.1 99/07/11 SMI"
 *
 */
package com.sun.ami.keygen;

import java.security.Key;
import java.security.PublicKey;
import java.security.PrivateKey;
import java.security.KeyFactorySpi;
import java.math.BigInteger;

import java.security.spec.KeySpec;
import java.security.spec.RSAPublicKeySpec;
import java.security.spec.RSAPrivateKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.security.spec.PKCS8EncodedKeySpec;

import com.sun.ami.common.AMI_Debug;

import java.security.InvalidKeyException;
import java.security.spec.InvalidKeySpecException;


/**
 * This class implements a key factory for the RSA keys 
 * as part of the SunAMI provider.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_RSAKeyFactory extends KeyFactorySpi {

    /**
     * Generates a public key object from the provided key specification
     * (key material).
     *
     * @param keySpec the specification (key material) of the public key
     *
     * @return the public key
     *
     * @exception InvalidKeySpecException if the given key specification
     * is inappropriate for this key factory to produce a public key.
     */
    protected PublicKey engineGeneratePublic(KeySpec keySpec)
    throws InvalidKeySpecException {

	try {
            AMI_Debug.debugln(3,
		"In AMI_RSAKeyFactory::generatePublicKey, Spec = " +keySpec);

	    // RSAPublicKeySpec
	    if (keySpec instanceof RSAPublicKeySpec) {
		return (new AMI_RSAPublicKey(
		    (RSAPublicKeySpec) keySpec));

	    // X.509 Encoded RSA Public key
	    } else if (keySpec instanceof X509EncodedKeySpec) {
		return (new AMI_RSAPublicKey(
		    (X509EncodedKeySpec) keySpec));

	    // Invalid RSA public key
	    } else {
		throw new InvalidKeySpecException
		    ("Inappropriate key specification");
	    }
	} catch (InvalidKeyException e) {
	    throw new InvalidKeySpecException
		("Inappropriate key specification: " + e.getMessage());
	} catch (Exception e) {
	    throw new InvalidKeySpecException(e.getMessage());
	}
    }

    /**
     * Generates a private key object from the provided key specification
     * (key material).
     *
     * @param keySpec the specification (key material) of the private key
     *
     * @return the private key
     *
     * @exception InvalidKeySpecException if the given key specification
     * is inappropriate for this key factory to produce a private key.
     */
    protected PrivateKey engineGeneratePrivate(KeySpec keySpec)
    throws InvalidKeySpecException {
	try {
            AMI_Debug.debugln(3,
		"In AMI_RSAKeyFactory::generatePrivateKey, Spec = " +keySpec);

	    // RSA Private Key Spec
	    if (keySpec instanceof RSAPrivateKeySpec) {
		RSAPrivateKeySpec rsaPrivKeySpec =
			(RSAPrivateKeySpec) keySpec;
		return (new AMI_RSAPrivateKey(rsaPrivKeySpec));

	    // PKCS8 Key spec
	    } else if (keySpec instanceof PKCS8EncodedKeySpec) {
		return new AMI_RSAPrivateKey(
		    (PKCS8EncodedKeySpec) keySpec);

	    // Invalid RSA private key
	    } else {
		throw new InvalidKeySpecException
		    ("Inappropriate key specification");
	    }
	} catch (InvalidKeyException e) {
	    throw new InvalidKeySpecException
		("Inappropriate key specification: " + e.getMessage());
        } catch (Exception e) {
            throw new InvalidKeySpecException(e.getMessage());
        }

    }

    /**
     * Returns a specification (key material) of the given key object
     * in the requested format.
     *
     * @param key the key 
     *
     * @param keySpec the requested format in which the key material shall be
     * returned
     *
     * @return the underlying key specification (key material) in the
     * requested format
     *
     * @exception InvalidKeySpecException if the requested key specification is
     * inappropriate for the given key, or the given key cannot be processed
     * (e.g., the given key has an unrecognized algorithm or format).
     */
    protected KeySpec engineGetKeySpec(Key key, Class keySpec)
    throws InvalidKeySpecException {
	
	try {
	    if (key instanceof AMI_RSAPublicKey) {
		// Determine valid key specs
		Class rsaPubKeySpec = Class.forName
		    ("java.security.spec.RSAPublicKeySpec");
		Class x509KeySpec = Class.forName
		    ("java.security.spec.X509EncodedKeySpec");

		if (rsaPubKeySpec.isAssignableFrom(keySpec)) {
		    AMI_RSAPublicKey rsaPubKey = (AMI_RSAPublicKey) key;
		    return (new RSAPublicKeySpec(
			rsaPubKey.getModulus(),
			rsaPubKey.getPublicExponent()));
		} else if (x509KeySpec.isAssignableFrom(keySpec)) {
		    return new X509EncodedKeySpec(key.getEncoded());

		} else {
		    throw new InvalidKeySpecException
			("Inappropriate key specification");
		}
	    } else if (key instanceof AMI_RSAPrivateKey) {
		// Determine valid key specs
		Class rsaPrivKeySpec = Class.forName(
		    "com.sun.ami.keygen.AMI_RSAPrivateKeySpec");
		Class pkcs8KeySpec = Class.forName(
		    "java.security.spec.PKCS8EncodedKeySpec");

		if (rsaPrivKeySpec.isAssignableFrom(keySpec)) {
		    AMI_RSAPrivateKey rsaPrivKey = (AMI_RSAPrivateKey) key;
		    return (new RSAPrivateKeySpec(
			rsaPrivKey.getModulus(),
			rsaPrivKey.getPrivateExponent()));

		} else if (pkcs8KeySpec.isAssignableFrom(keySpec)) {
		    return new PKCS8EncodedKeySpec(key.getEncoded());

		} else {
		    throw new InvalidKeySpecException
			("Inappropriate key specification");
		}

	    } else {
		throw new InvalidKeySpecException("Inappropriate key type");
	    }

	} catch (ClassNotFoundException e) {
	    throw new InvalidKeySpecException
		("Unsupported key specification: " + e.getMessage());
	}
    }

    /**
     * Translates a key object, whose provider may be unknown or potentially
     * untrusted, into a corresponding key object of this key factory.
     *
     * @param key the key whose provider is unknown or untrusted
     *
     * @return the translated key
     *
     * @exception InvalidKeyException if the given key cannot be processed by
     * this key factory.
     */
    protected Key engineTranslateKey(Key key) throws InvalidKeyException {
        return key;
    }
}
