/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_DHKeyFactory.java	1.2 99/07/11 SMI"
 *
 */

package com.sun.ami.dh;

import java.util.*;
import java.lang.*;
import java.security.Key;
import java.security.PublicKey;
import java.security.PrivateKey;
import java.security.KeyFactorySpi;
import java.security.InvalidKeyException;
import java.security.spec.KeySpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.X509EncodedKeySpec;
import java.security.spec.PKCS8EncodedKeySpec;
import com.sun.ami.dh.*;

/*
 * import javax.crypto.spec.DHPublicKeySpec;
 * import javax.crypto.spec.DHPrivateKeySpec;
 * import javax.crypto.spec.DHParameterSpec;
 */

/**
 * This class implements the Diffie-Hellman key factory of the Sun provider.
 *
 */

public final class AMI_DHKeyFactory extends KeyFactorySpi {

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
	    if (keySpec instanceof AMI_DHPublicKeySpec) {
		AMI_DHPublicKeySpec dhPubKeySpec = (AMI_DHPublicKeySpec)keySpec;
		/*
		 * Private-value length is OPTIONAL.
		 * Determine if it is provided, and call the appropriate
		 * DHPublicKey constructor.
		 */
		int exponentSize = dhPubKeySpec.getL();

		if (exponentSize > 0) {
		    return new AMI_DHPublicKey(dhPubKeySpec.getY(),
					   dhPubKeySpec.getP(),
					   dhPubKeySpec.getG(),
					   exponentSize);
		} else {
		    return new AMI_DHPublicKey(dhPubKeySpec.getY(),
					   dhPubKeySpec.getP(),
					   dhPubKeySpec.getG());
		}

	    } else if (keySpec instanceof X509EncodedKeySpec) {
		return new AMI_DHPublicKey
		    (((X509EncodedKeySpec)keySpec).getEncoded());

	    } else {
		throw new InvalidKeySpecException
		    ("Inappropriate key specification");
	    }
	} catch (InvalidKeyException e) {
	    throw new InvalidKeySpecException
		("Inappropriate key specification");
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
	    if (keySpec instanceof AMI_DHPrivateKeySpec) {
		AMI_DHPrivateKeySpec dhPrivKeySpec =
		    (AMI_DHPrivateKeySpec) keySpec;
		/*
		 * Private-value length is OPTIONAL.
		 * Determine if it is provided, and call the appropriate
		 * DHPrivateKey constructor.
		 */
		int exponentSize = dhPrivKeySpec.getL();
		if (exponentSize > 0) {
		    return new AMI_DHPrivateKey(dhPrivKeySpec.getX(),
					    dhPrivKeySpec.getP(),
					    dhPrivKeySpec.getG(),
					    exponentSize);
		} else {
		    return new AMI_DHPrivateKey(dhPrivKeySpec.getX(),
					    dhPrivKeySpec.getP(),
					    dhPrivKeySpec.getG());
		}

	    } else if (keySpec instanceof PKCS8EncodedKeySpec) {
		return new AMI_DHPrivateKey
		    (((PKCS8EncodedKeySpec)keySpec).getEncoded());

	    } else {
		throw new InvalidKeySpecException
		    ("Inappropriate key specification");
	    }
	} catch (InvalidKeyException e) {
	    throw new InvalidKeySpecException
		("Inappropriate key specification");
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
	
	AMI_DHParameterSpec params;

	try {

	    if (key instanceof AMI_DHPublicKey) {
		
		// Determine valid key specs
		Class dhPubKeySpec = Class.forName(
		    "com.sun.ami.dh.AMI_DHPublicKeySpec");
		Class x509KeySpec = Class.forName
		    ("java.security.spec.X509EncodedKeySpec");

		if (dhPubKeySpec.isAssignableFrom(keySpec)) {
		    AMI_DHPublicKey dhPubKey = (AMI_DHPublicKey) key;
		    params = dhPubKey.getParams();
		    /*
		     * Private-value length is OPTIONAL.
		     * Determine if it is provided, and call the appropriate
		     * DHPublicKeySpec constructor.
		     */
		    int exponentSize = params.getL();
		    if (exponentSize > 0) {
			return new AMI_DHPublicKeySpec(dhPubKey.getY(),
						   params.getP(),
						   params.getG(),
						   exponentSize);
		    } else {
			return new AMI_DHPublicKeySpec(dhPubKey.getY(),
						   params.getP(),
						   params.getG());
		    }

		} else if (x509KeySpec.isAssignableFrom(keySpec)) {
		    return new X509EncodedKeySpec(key.getEncoded());

		} else {
		    throw new InvalidKeySpecException
			("Inappropriate key specification");
		}
		 
	    } else if (key instanceof AMI_DHPrivateKey) {

		// Determine valid key specs
		Class dhPrivKeySpec = Class.forName(
		    "com.sun.ami.dh.AMI_DHPrivateKeySpec");
		Class pkcs8KeySpec = Class.forName
		    ("java.security.spec.PKCS8EncodedKeySpec");

		if (dhPrivKeySpec.isAssignableFrom(keySpec)) {
		    AMI_DHPrivateKey dhPrivKey = (AMI_DHPrivateKey) key;
		    params = dhPrivKey.getParams();
		    /*
		     * Private-value length is OPTIONAL.
		     * Determine if it is provided, and call the appropriate
		     * DHPrivateKeySpec constructor.
		     */
		    int exponentSize = params.getL();
		    if (exponentSize > 0) {
			return new AMI_DHPrivateKeySpec(dhPrivKey.getX(),
						    params.getP(),
						    params.getG(),
						    exponentSize);
		    } else {
			return new AMI_DHPrivateKeySpec(dhPrivKey.getX(),
						    params.getP(),
						    params.getG());
		    }

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
	    if (key instanceof com.sun.ami.dh.AMI_DHPublicKey) {
		    return key;
	    } else if (key instanceof com.sun.ami.dh.AMI_DHPrivateKey) {
		    return key;
	    } else {
		throw new InvalidKeyException("Wrong algorithm type");
	    }
    }
}


