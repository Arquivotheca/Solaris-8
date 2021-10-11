/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyStore_Certs.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.security.cert.Certificate;
import java.security.cert.*;
import java.security.*;
import java.io.*;
import java.util.*;
import java.rmi.*;
import java.net.*;

import com.sun.ami.common.AMI_Constants;
import com.sun.ami.keygen.AMI_VirtualHost;

/**
 * This class provides the keystore implementation
 * referred to as "AMICERTS".
 */

public final class AMI_KeyStore_Certs extends KeyStoreSpi {

	protected boolean  loaded = false;

	/**
	 * Returns the key associated with the given alias, using the given
	 * password to recover it.
	 *
	 * @param alias the alias name
	 * @param password the password for recovering the key
	 *
	 * @return the requested key, or null if the given alias does not exist
	 * or does not identify a <i>key entry</i>.
	 *
	 * @exception NoSuchAlgorithmException if the algorithm for recovering
	 * the key cannot be found
	 * @exception UnrecoverableKeyException if the key cannot be recovered
	 * (e.g., the given password is wrong).
	 */
	public Key engineGetKey(String alias, char[] password)
	    throws NoSuchAlgorithmException, UnrecoverableKeyException {
		throw new NoSuchAlgorithmException(
		    "KeyStore type \"amicerts\" does not support private keys");
	}

	protected boolean isCertificatePresent(Vector certificates,
	    X509Certificate cert) {
		String certIssuer = AMI_KeyMgntClient.normalizeDN(
		    cert.getSubjectDN().toString());
		long certNbr = cert.getSerialNumber().longValue();
		X509Certificate newCert;
		Enumeration enum = certificates.elements();
		while (enum.hasMoreElements()) {
			newCert = (X509Certificate) enum.nextElement();
			if ((newCert.getSerialNumber().longValue() ==
			    certNbr) &&
			    certIssuer.equals(AMI_KeyMgntClient.normalizeDN(
			    newCert.getSubjectDN().toString())))
				return (true);
		}
		return (false);
	}

	/**
	 * Returns all certificates store with the given alias.
	 *
	 * @param alias the alias name
	 *
	 */
	public Certificate[] engineGetCertificates(String alias) {
		// Obtain the certificate Enumeration and return an
		// array of certificates

		String nameDN = null; /* Search is allways on DNs */
		Enumeration certs = null, certs2 = null;
		try {
			if (alias != null) {
				String amiType = AMI_KeyMgntClient.getAmiType(
				    alias, "certx509");

				// Normalize alias
				alias = AMI_KeyMgntClient.normalizeDN(alias);

				// Obtain the DN name of the alias
				if (alias.indexOf('=') == -1) {
					// Search for nameDN
					nameDN = AMI_KeyMgntClient.normalizeDN(
				AMI_KeyMgntClient.getDNNameFromLoginName(
					    alias));
				} else
					nameDN = new String(alias);

				certs = AMI_KeyMgntClient.getX509Certificates(
				    alias, amiType);
			}

			// Try "thisuser" certificates too
			String localName = System.getProperty("user.name");
			String localType = AMI_Constants.AMI_USER_OBJECT;
			if (AMI_KeyMgnt_FNS.fns_get_euid(null) == 0) {
				localType = AMI_Constants.AMI_HOST_OBJECT;
				localName = AMI_VirtualHost.getHostIP();
			}

			String nameDN2 =
			    AMI_KeyMgntClient.getDNNameFromLoginName(
			    localName, localType);
			if (nameDN2 != null) {
				nameDN2 = AMI_KeyMgntClient.normalizeDN(
				    nameDN2);
				if ((nameDN == null) ||
				    !nameDN.equalsIgnoreCase(nameDN2) ||
				    (certs == null)) {
					certs2 =
					AMI_KeyMgntClient.getX509Certificates(
					    localName, localType);
				}
				if (alias == null)
					nameDN = nameDN2;
			}
			if (nameDN == null)
				return (null);
		} catch (Exception e) {
			e.printStackTrace();
			return (null);
		}

		if ((certs == null) && (certs2 == null))
			return (null);

		Vector certificates = new Vector();
		X509Certificate cert;
		String certDN;
		boolean certsAdded = false;
		while ((certs != null) && certs.hasMoreElements()) {
			cert = (X509Certificate) certs.nextElement();
			certDN = AMI_KeyMgntClient.normalizeDN(
			    cert.getSubjectDN().getName());
			if (certDN.equalsIgnoreCase(nameDN)) {
				certsAdded = true;
				certificates.add(cert);
			}
		}
		while ((certs2 != null) && certs2.hasMoreElements()) {
			cert = (X509Certificate) certs2.nextElement();
			certDN = AMI_KeyMgntClient.normalizeDN(
			    cert.getSubjectDN().getName());
			if ((alias == null) ||
			    certDN.equalsIgnoreCase(nameDN)) {
				// Before adding this certifiate make sure
				// it is not already present
				if (!certsAdded || !isCertificatePresent(
				    certificates, cert))
					certificates.add(cert);
			}
		}

		if (certificates.size() == 0)
			return (null);

		Certificate answer[] = new Certificate[certificates.size()];
		Enumeration enum = certificates.elements();
		int index = 0;
		while (enum.hasMoreElements())
			answer[index++] = (Certificate) enum.nextElement();
		return (answer);
	}

	/**
	 * Returns the certificate chain associated with the given alias.
	 *
	 * @param alias the alias name
	 *
	 * @return the certificate chain (ordered with the user's certificate
	 * first
	 * and the root certificate authority last), or null if the given alias
	 * does not exist or does not contain a certificate chain (i.e.,
	 * the given 
	 * alias identifies either a <i>trusted certificate entry</i> or a
	 * <i>key entry</i> without a certificate chain).
	 */
	public Certificate[] engineGetCertificateChain(String alias) {
		Certificate[] answer = null;
		Certificate cert;
		int i;
		String issuerDN, subjectDN = new String(alias);
		Certificate[] newCerts;
		while ((cert = engineGetCertificate(subjectDN)) != null) {
			if (answer == null) {
				answer = new Certificate[1];
				answer[0] = cert;
			} else {
				newCerts = new Certificate[answer.length+1];
				for (i = 0; i < answer.length; i++)
					newCerts[i] = answer[i];
				newCerts[i] = cert;
				answer = newCerts;
			}
			subjectDN = AMI_KeyMgntClient.normalizeDN(
			    ((X509Certificate) cert).getSubjectDN().toString());
			issuerDN = AMI_KeyMgntClient.normalizeDN(
			    ((X509Certificate) cert).getIssuerDN().toString());
			if (subjectDN.equals(issuerDN))
				break;
			subjectDN = issuerDN;
		}
		return (answer);
	}

	/**
	 * Returns the certificate associated with the given alias.
	 *
	 * <p>If the given alias name identifies a
	 * <i>trusted certificate entry</i>, the certificate associated with
	 * that entry is returned. If the given alias name identifies a
	 * <i>key entry</i>, the first element of the certificate chain of that
	 * entry is returned, or null if that entry does not have a certificate
	 * chain.
	 *
	 * @param alias the alias name
	 *
	 * @return the certificate, or null if the given alias does not exist or
	 * does not contain a certificate.
	 */
	public Certificate engineGetCertificate(String alias) {
		Certificate[] answer = engineGetCertificates(alias);
		if (answer == null)
			return (null);
		return (answer[0]);
	}

	/**
	 * Returns the creation date of the entry identified by the given alias.
	 *
	 * @param alias the alias name
	 *
	 * @return the creation date of this entry, or null if the given
	 * alias does not exist
	 */
	public Date engineGetCreationDate(String alias) {
		// TBD
		return (null);
	}

	/**
	 * Assigns the given key to the given alias, protecting it with the
	 * given password.
	 *
	 * <p>If the given key is of type <code>java.security.PrivateKey</code>,
	 * it must be accompanied by a certificate chain certifying the
	 * corresponding public key.
	 *
	 * <p>If the given alias already exists, the keystore information
	 * associated with it is overridden by the given key (and possibly
	 * certificate chain).
	 *
	 * @param alias the alias name
	 * @param key the key to be associated with the alias
	 * @param password the password to protect the key
	 * @param chain the certificate chain for the corresponding public
	 * key (only required if the given key is of type
	 * <code>java.security.PrivateKey</code>).
	 *
	 * @exception KeyStoreException if the given key cannot be protected, or
	 * this operation fails for some other reason
	 */
	public void engineSetKeyEntry(String alias, Key key, char[] password,
	    Certificate[] chain) throws KeyStoreException {
		throw new KeyStoreException("KeyStore of type \"amicerts\"" +
		    " does not support private key operations");
	}

	/**
	 * Assigns the given key (that has already been protected) to the given
	 * alias.
	 * 
	 * <p>If the protected key is of type
	 * <code>java.security.PrivateKey</code>, it must be accompanied by a
	 * certificate chain certifying the corresponding public key. If the
	 * underlying keystore implementation is of type <code>jks</code>,
	 * <code>key</code> must be encoded as an
	 * <code>EncryptedPrivateKeyInfo</code> as defined in the
	 * PKCS #8 standard.
	 *
	 * <p>If the given alias already exists, the keystore information
	 * associated with it is overridden by the given key (and possibly
	 * certificate chain).
	 *
	 * @param alias the alias name
	 * @param key the key (in protected format) to be associated with
	 * the alias
	 * @param chain the certificate chain for the corresponding public
	 * key (only useful if the protected key is of type
	 * <code>java.security.PrivateKey</code>).
	 *
	 * @exception KeyStoreException if this operation fails.
	 */
	public void engineSetKeyEntry(String alias, byte[] key,
	    Certificate[] chain) throws KeyStoreException {
		throw new KeyStoreException("KeyStore of type \"amicerts\"" +
		    " does not support private key operations");
	}

	protected String[] checkForPermission(String alias)
	    throws KeyStoreException {
		// Local identity
		String amiType = AMI_Constants.AMI_USER_OBJECT;
		String userName = System.getProperty("user.name");
		if (AMI_KeyMgnt_FNS.fns_get_euid(null) == 0) {
			userName = AMI_VirtualHost.getHostIP();
			amiType = AMI_Constants.AMI_HOST_OBJECT;
		}
		String nameDN;
		try {
			nameDN = AMI_KeyMgntClient.getDNNameFromLoginName(
			    userName, amiType);
		} catch (Exception e) {
			throw new KeyStoreException(e.getMessage());
		}
		if (nameDN == null) {
			throw new KeyStoreException(
			    "Context does not exist");
		}

		// Check for permissions
		boolean permissionDenied = true;
		if (alias != null) {
			if (alias.indexOf('=') == -1) {
				// Check if user name is local UID
				if (amiType.equals(
				     AMI_Constants.AMI_USER_OBJECT) &&
				     userName.equalsIgnoreCase(alias))
					permissionDenied = false;
				else if (amiType.equals(
				     AMI_Constants.AMI_HOST_OBJECT)) {
					try {
						String ipAdd =
						    InetAddress.getByName(
						    alias).getHostAddress();
						if (ipAdd.equals(userName))
							permissionDenied =
							    false;
					} catch (java.net.UnknownHostException
					    e) {
						// Do nothing
					}
				}
			} else if (AMI_KeyMgntClient.normalizeDN(alias).equals(
			    AMI_KeyMgntClient.normalizeDN(nameDN))) {
				permissionDenied = false;
			}
		} else
			permissionDenied = false;
		if (permissionDenied)
			throw new KeyStoreException("Permission denied");

		String [] answer = new String[2];
		answer[0] = nameDN;
		answer[1] = amiType;
		return (answer);
	}

	/**
	 * Assigns the given certificate to the given alias.
	 *
	 * <p>If the given alias already exists in this keystore and
	 * identifies a <i>trusted certificate entry</i>, the certificate
	 * associated with
	 * it is overridden by the given certificate.
	 *
	 * @param alias the alias name
	 * @param cert the certificate
	 *
	 * @exception KeyStoreException if the given alias already exists and
	 * does not identify a <i>trusted certificate entry</i>, or this
	 * operation fails for some other reason.
	 */
	public void engineSetCertificateEntry(String alias, Certificate cert)
	    throws KeyStoreException {
		// Add the certificate to the directory service
		String names[] = checkForPermission(alias);

		try {
			X509Certificate certificate = (X509Certificate) cert;
		       	AMI_KeyMgntService service =
			    AMI_KeyMgntClient.getKeyMgntServiceDummyImpl();

			AMI_KeyMgntClient.addX509Certificate(service, names[0],
			    certificate, names[1]);
		} catch (Exception e) {
			throw new KeyStoreException(e.toString());
		}
	}

	/**
	 * Deletes the entry identified by the given alias from this keystore.
	 *
	 * @param alias the alias name
	 *
	 * @exception KeyStoreException if the entry cannot be removed.
	 */
	public void engineDeleteEntry(String alias) throws KeyStoreException {
		// Delete the certificate in the directory service
		String names[] = checkForPermission(alias);

		try {
			AMI_KeyMgntClient.deleteX509Certificate(
			    names[0], null, names[1]);

		} catch (Exception e) {
			throw new KeyStoreException(e.getMessage());
		}
	}

	/**
	 * Lists all the alias names of this keystore.
	 *
	 * @return enumeration of the alias names
	 */
	public Enumeration engineAliases() {
		return (null);
	}

	/**
	 * Checks if the given alias exists in this keystore.
	 *
	 * @param alias the alias name
	 *
	 * @return true if the alias exists, false otherwise
	 */
	public boolean engineContainsAlias(String alias) {
		// Check in the directory service if a certificate exists
		if (engineGetCertificate(alias) != null)
			return (true);
		else
			return (false);
	}

	/**
	 * Retrieves the number of entries in this keystore.
	 *
	 * @return the number of entries in this keystore
	 */
	public int engineSize() {
		return (0);
	}

	/**
	 * Returns true if the entry identified by the given alias is a
	 * <i>key entry</i>, and false otherwise.
	 *
	 * @return true if the entry identified by the given alias is a
	 * <i>key entry</i>, false otherwise.
	 */
	public boolean engineIsKeyEntry(String alias) {
		return (false);
	}

	/**
	 * Returns true if the entry identified by the given alias is a
	 * <i>trusted certificate entry</i>, and false otherwise.
	 *
	 * @return true if the entry identified by the given alias is a
	 * <i>trusted certificate entry</i>, false otherwise.
	 */
	public boolean engineIsCertificateEntry(String alias) {
		// Lookup the certificate and return true
		if (engineGetCertificate(alias) != null)
			return (true);
		else
			return (false);
	}

	/**
	 * Returns the (alias) name of the first keystore entry whose
	 *  certificate matches the given certificate.
	 *
	 * <p>This method attempts to match the given certificate with each
	 * keystore entry. If the entry being considered
	 * is a <i>trusted certificate entry</i>, the given certificate is
	 * compared to that entry's certificate. If the entry being considered
	 * is a <i>key entry</i>, the given certificate is compared to the
	 * first element of that entry's certificate chain (if a chain exists).
	 *
	 * @param cert the certificate to match with.
	 *
	 * @return the (alias) name of the first entry with matching
	 * certificate, or null if no such entry exists in this keystore.
	 */
	public String engineGetCertificateAlias(Certificate cert) {
		// %%% Check the subject name, alternate names of the
		// certificate and return that as the alias
		return (null);
	}

	/**
	 * Stores this keystore to the given output stream, and protects its
	 * integrity with the given password.
	 *
	 * @param stream the output stream to which this keystore is written.
	 * @param password the password to generate the keystore integrity check
	 *
	 * @exception IOException if there was an I/O problem with data
	 * @exception NoSuchAlgorithmException if the appropriate data integrity
	 * algorithm could not be found
	 * @exception CertificateException if any of the certificates included
	 * in the keystore data could not be stored
	 */
	public void engineStore(OutputStream stream, char[] password)
	    throws IOException, NoSuchAlgorithmException, CertificateException {
		// Make sure there is a binding to the directory service
	}

	/**
	 * Loads the keystore from the given input stream.
	 *
	 * <p>If a password is given, it is used to check the integrity of the
	 * keystore data. Otherwise, the integrity of the keystore is not
	 * checked.
	 *
	 * @param stream the input stream from which the keystore is loaded
	 * @param password the (optional) password used to check the integrity
	 * of the keystore.
	 *
	 * @exception IOException if there is an I/O or format problem with the
	 * keystore data
	 * @exception NoSuchAlgorithmException if the algorithm used to check
	 * the integrity of the keystore cannot be found
	 * @exception CertificateException if any of the certificates in the
	 * keystore could not be loaded
	 */
	public void engineLoad(InputStream stream, char[] password)
	    throws IOException, NoSuchAlgorithmException, CertificateException {
		// Make sure there is a binding to the directory service
	}  
}
