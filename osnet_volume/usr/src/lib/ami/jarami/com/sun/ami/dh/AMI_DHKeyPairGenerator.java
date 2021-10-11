/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_DHKeyPairGenerator.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.dh;

import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.KeyPair;
import java.security.KeyPairGeneratorSpi;
import java.security.SecureRandom;
import java.security.AlgorithmParameters;
import java.security.InvalidAlgorithmParameterException;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import java.security.InvalidParameterException;

import com.sun.ami.export.AMI_Export;
import com.sun.ami.export.AMI_Global_Export;
import com.sun.ami.export.AMI_Domestic_Export;
import com.sun.ami.common.*;

public final class AMI_DHKeyPairGenerator extends KeyPairGeneratorSpi {

	// The public value
	private BigInteger y;

	// The private value
	private BigInteger x;

	// The prime modulus
	private BigInteger p;

	// The base generator
	private BigInteger g;

	// The size in bits of the prime modulus
	private int primeSize = 512;

	// The size in bits of the random exponent (private value)
	private int exponentSize = 504;

	// The source of randomness
	private SecureRandom random;

	/**
	 * Initializes this key pair generator for a certain strength and
	 * source of randomness.
	 * The strength is specified as the size in bits of the prime modulus.
	 *
	 * @param strength the strength (size of prime modulus) in bits
	 * @param random the source of randomness
	 */
	public void initialize(int strength, SecureRandom random) 
			throws InvalidParameterException
	{
		primeSize = strength;
		exponentSize = 0;
		random = random;

		validateSizeLimits(strength);
	}

	/**
	 * Initializes this key pair generator for the specified parameter
	 * set and source of randomness.
	 *
	 * <p>The given parameter set contains the prime modulus, the base
	 * generator, and optionally the requested size in bits of the random
	 * exponent (private value).
	 *
	 * @param params the parameter set used to generate the key pair
	 * @param random the source of randomness
	 *
	 * @exception InvalidAlgorithmParameterException if the given parameters
	 * are inappropriate for this key pair generator
	 */
	public void initialize(AlgorithmParameterSpec params, SecureRandom rand)
	    throws InvalidAlgorithmParameterException, InvalidParameterException
	{
		if (!(params instanceof AMI_DHParameterSpec)) {
			throw new InvalidAlgorithmParameterException
			    ("Inappropriate parameter type");
		}
		p = ((AMI_DHParameterSpec) params).getP();
		g = ((AMI_DHParameterSpec) params).getG();
		primeSize = p.bitLength();

		// exponent size is optional, could be 0
		exponentSize = ((AMI_DHParameterSpec) params).getL();

		// Sucure random
		random = rand;

		// Require exponentSize < primeSize
		if ((exponentSize != 0) && (exponentSize >= primeSize)) {
			throw new InvalidAlgorithmParameterException
			    ("Exponent size must be less than modulus size");
		}

                validateSizeLimits(primeSize);
	}

	/**
	 * Generates a key pair.
	 *
	 * @return the new key pair
	 */
	public KeyPair generateKeyPair() {
		KeyPair pair = null;

		if (exponentSize == 0) {
			/*
			 * We must choose the size of the random exponent
			 * (private value) ourselves. The size of the random
			 * exponent must be less than the size of the
			 * prime modulus.
			 */
			exponentSize = primeSize - 1;
		}

		try {
			if (p == null || g == null) {
				// We have to create paremeter spec
				AMI_DHParameterSpec paramSpec =
				    AMI_DHKeyGenerationParameter.ParameterSpec(
				    primeSize);
				p = paramSpec.getP();
				g = paramSpec.getG();
			}

			if (random == null)
				random = new SecureRandom();
			x = new BigInteger(exponentSize, random);
			y = g.modPow(x, p);

			AMI_DHPublicKey pubKey = new AMI_DHPublicKey(y, p, g,
			    exponentSize);
			AMI_DHPrivateKey privKey = new AMI_DHPrivateKey(x, p, g,
			    exponentSize);
			pair = new KeyPair(pubKey, privKey);
		// } catch (InvalidAlgorithmParameterException e) {
		//	// this should never happen, because we create paramSpec
		//	throw new RuntimeException(e.getMessage());
		// } catch (InvalidParameterSpecException e) {
		//	// this should never happen
		//	throw new RuntimeException(e.getMessage());
		} catch (InvalidKeyException e) {
			// this should never happen
			throw new RuntimeException(e.getMessage());
		}
		return pair;
	}

        private void validateSizeLimits(int strength) {
		int maxStrength = 0;
		try {
			AMI_Export eVersion = null;
			try {
				// Instantiate domestic version
				eVersion = new AMI_Domestic_Export();
			} catch (Exception e) {
				// Instantiate global version
				eVersion = new AMI_Global_Export();
			}
			maxStrength = eVersion.getMaxDHKeySize();
			AMI_Debug.debugln(3, "Max keysize = " + maxStrength);
		} catch (Exception e) {
			// do nothing
		}

                if (strength > maxStrength)
			throw new InvalidParameterException(
			    "Unacceptable key strength for package : !"
			    + strength);
        }

}
