/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_keymgnt_common.c	1.2 99/07/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <strings.h>
#include <sys/systeminfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread.h>
#include <synch.h>
#include <ctype.h>
#include <ami.h>
#include <ami_proto.h>

/* Various backends supported */
#define	AMI_NS_NONE 0
#define	AMI_NS_FILE 1
#define	AMI_NS_FNS 2
#define	AMI_NS_NIS 3
#define	AMI_NS_NISPLUS 4
#define	AMI_NS_LDAP 5

/* Required system wide properties */
#define	nsKeyStoreRSA_property "ami.keymgnt.ns.keystorersa"
#define	nsKeyStoreDSA_property "ami.keymgnt.ns.keystoredsa"
#define	nsKeyStoreDH_property "ami.keymgnt.ns.keystoredh"
#define	nsUserProfile_property "ami.keymgnt.ns.objectprofile"
#define	nsCertX509_property "ami.keymgnt.ns.certx509"

/* System wide name service backend properties */
static int nsKeystoreRSA;
static int nsUserProfile;
static int nsCertX509;

/* Default file name for FILE as the backend */
#define	AMI_PROPERTY_FILE "/etc/ami/ami.properties"
#define	DEFAULT_PROFILE_FILENAME ".amiprofile";
#define	DEFAULT_KEYSTORE_FILENAME ".keystore";
#define	DEFAULT_CERT_FILENAME ".certx509";

typedef struct _ami_user_profile {
	char *id;
	char *value;
	struct _ami_user_profile *next;
} ami_user_profile;

/* Initialize system wide properties */
static int initialized;
static mutex_t init_mutex_lock = DEFAULTMUTEX;

static char *
ami_keymgnt_get_property_value(const char *properties, const char *id)
{
	char *answer, *ptr;
	size_t length = 0;

	ptr = strstr(properties, id);
	if (ptr == NULL)
		return (NULL);
	ptr += strlen(id);

	/* Obtain the value */
	while ((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n'))
		ptr++;
	while ((*(ptr+length) != ' ') &&
	    (*(ptr+length) != '\t') && (*(ptr+length) != '\n'))
		length++;
	answer = (char *) calloc(sizeof (char), length + 1);
	memcpy(answer, ptr, length);
	return (answer);
}

static int
ami_keymgnt_get_ns_from_string(const char *value)
{
	if (!value)
		return (AMI_NS_NONE);

	if (strcmp(value, "file") == 0)
		return (AMI_NS_FILE);
	else if (strcmp(value, "fns") == 0)
		return (AMI_NS_FNS);
	else if (strcmp(value, "nis") == 0)
		return (AMI_NS_NIS);
	else if (strcmp(value, "nisplus") == 0)
		return (AMI_NS_NISPLUS);
	else if (strcmp(value, "ldap") == 0)
		return (AMI_NS_LDAP);
	return (AMI_NS_NONE);
}

static void ami_keymgnt_initialize()
{
	int propFile;
	struct stat propFileStat;
	char *properties, *ptr, *value;

	if (initialized)
		return;

	mutex_lock(&init_mutex_lock);
	if (initialized) {
		mutex_unlock(&init_mutex_lock);
		return;
	}

	/* Open /etc/ami/ami.properties */
	if (stat(AMI_PROPERTY_FILE, &propFileStat) != NULL) {
		/* printf("File not found: %s\n", AMI_PROPERTY_FILE); */
		mutex_unlock(&init_mutex_lock);
		return;
	}
	propFile = open(AMI_PROPERTY_FILE, O_RDONLY);
	if (propFile == NULL) {
		/* printf("Error in opening %s file\n", AMI_PROPERTY_FILE); */
		mutex_unlock(&init_mutex_lock);
		return;
	}

	/* Copy the properties */
	properties = (char *) calloc(sizeof (char), propFileStat.st_size + 1);
	if (read(propFile, properties, propFileStat.st_size)
	    != propFileStat.st_size) {
		/* printf("Unable to read file %s\n", AMI_PROPERTY_FILE); */
		mutex_unlock(&init_mutex_lock);
		return;
	}
	close(propFile);

	/* Obtain the required properties */
	for (ptr = properties; (*ptr); ptr++) {
		if (isupper(*ptr))
			(*ptr) = tolower(*ptr);
	}

	/* KeyStore RSA property */
	value =
	    ami_keymgnt_get_property_value(properties, nsKeyStoreRSA_property);
	nsKeystoreRSA = ami_keymgnt_get_ns_from_string(value);
	free(value);
	value =
	    ami_keymgnt_get_property_value(properties, nsKeyStoreDSA_property);
	(void) ami_keymgnt_get_ns_from_string(value);
	free(value);
	value =
	    ami_keymgnt_get_property_value(properties, nsKeyStoreDH_property);
	ami_keymgnt_get_ns_from_string(value);
	free(value);
	value =
	    ami_keymgnt_get_property_value(properties, nsUserProfile_property);
	nsUserProfile = ami_keymgnt_get_ns_from_string(value);
	free(value);
	value =
	    ami_keymgnt_get_property_value(properties, nsCertX509_property);
	nsCertX509 = ami_keymgnt_get_ns_from_string(value);
	free(value);

	/* Clean-up */
	initialized = 1;
	mutex_unlock(&init_mutex_lock);
	free(properties);
}

static void ami_keymgnt_read_int(const char *buffer, int *pointer, int *value)
{
#ifdef sparc
	memcpy(value, buffer+(*pointer), sizeof (int));
#else
	char length[4];
	memcpy(&length, buffer+(*pointer), sizeof (int));
	(*value) = (((((int) length[0]) & 0xff) << 24) |
		    ((((int) length[1]) & 0xff) << 16) |
		    ((((int) length[2]) & 0xff) << 8) |
		    (((int) length[3]) & 0xff));
#endif

	(*pointer) += sizeof (int);
}

static void ami_keymgnt_read_UTF(const char *buffer, int *pointer, char **value)
{
	/* Read the length of string */
	short length;
#ifdef sparc
	memcpy(&length, buffer+(*pointer), sizeof (short));
#else
	char alength[2];
	memcpy(alength, buffer+(*pointer), sizeof (short));
	length = (((((int) alength[0]) & 0xff) << 0) |
		(((int) alength[1]) & 0xff));
#endif
	(*pointer) += sizeof (short);

	if (*value == NULL)
		*value = (char *) calloc(length + 1, sizeof (char));
	strncpy(*value, (buffer+(*pointer)), (size_t) length);
	(*pointer) += (int) length;
}

static void ami_keymgnt_read_bytes(const char *buffer, int *pointer,
    char **value, int length)
{
	if (*value == NULL)
		*value = (void *) calloc(length, sizeof (char));
	memcpy(*value, (buffer+(*pointer)), (size_t) length);
	(*pointer) += (int) length;
}

static ami_user_profile *
ami_keymgnt_get_user_profile(const char *userProfile)
{
	ami_user_profile *current, *previous = NULL, *answer = NULL;
	int i, version = 0, num_items, pointer = 0;
	char *keyType = 0, *keyAlias = 0;

	/* Read version and discard */
	ami_keymgnt_read_int(userProfile, &pointer, &version);

	/* Read keyType and discard */
	ami_keymgnt_read_UTF(userProfile, &pointer, &keyType);
	if (keyType)
		free(keyType);

	/* Read keyAlias and discard */
	ami_keymgnt_read_UTF(userProfile, &pointer, &keyAlias);
	if (keyAlias)
		free(keyAlias);

	/* Read number of items */
	ami_keymgnt_read_int(userProfile, &pointer, &num_items);

	/* Read all the items */
	for (i = 0; i < num_items; i++) {
		current =
		    (ami_user_profile *) calloc(1, sizeof (ami_user_profile));
		ami_keymgnt_read_UTF(userProfile, &pointer,
		    &current->id);
		ami_keymgnt_read_UTF(userProfile, &pointer,
		    &current->value);
		if (answer) {
			previous->next = current;
			previous = current;
		} else {
			answer = current;
			previous = current;
		}
	}
	return (answer);
}
/*
static char *
ami_get_hostname_from_ip_address(const char *ip_addr)
{
	char *machine;
	struct hostent host_result;
	char host_buffer[1024];
	int host_error;
	struct hostent *hostentry;
	ulong_t addr = inet_addr(ip_addr);

	hostentry = gethostbyaddr_r((char *) &addr, sizeof (addr),
	    AF_INET, &host_result, host_buffer, 1024, &host_error);
	if (hostentry == 0)
		return (0);

	machine = (char *) malloc(strlen(hostentry->h_name) + 1);
	if (machine == 0)
		return (0);
	strcpy(machine, hostentry->h_name);

	machine = strtok(machine, ".");
	return (machine);
}
*/

static char *
ami_get_ip_from_hostname(const char *hostname)
{
	struct hostent host_result;
	char host_buffer[1024];
	int host_error;
	struct in_addr in;
	char **p;

	struct hostent *hostentry = gethostbyname_r(
	    hostname, &host_result, host_buffer,
	    1024, &host_error);
	if (hostentry == NULL)
		return (0);
	p = hostentry->h_addr_list;
	memcpy(&in.s_addr, *p, sizeof (in.s_addr));
	return (strdup(inet_ntoa(in)));
}

static ami_user_profile *
ami_keymgnt_read_user_profile_from_file(const char *name)
{
	ami_user_profile *answer;
	char *bin_profile, filename[1024];
	struct stat filestat;
	char buf[2048];
	struct passwd userPasswd;
	char *username = 0;
	int file, obtainedFileName = 0;

	/* Get the user name */
	if (name == NULL) {
		/* construct the username */
		if (getuid() != 0) {
		    if (getpwuid_r(getuid(), &userPasswd, buf, 2048) == NULL)
			return (NULL);
			strcpy(filename, userPasswd.pw_dir);
			strcat(filename, "/.amiprofile");
			obtainedFileName = 1;
		} else {
			if (sysinfo(SI_HOSTNAME, buf, 2048) < 0)
				return (NULL);
			username = ami_get_ip_from_hostname(buf);
		}
	} else
		username = strdup(name);

	/* Construct file name from user's home directory */
	if (!obtainedFileName && ((int)inet_addr(username) != -1)) {
	/* Host object */
	    strcpy(filename, "/etc/ami/keys/");
	    strcat(filename, username);
	    strcat(filename, ".amiprofile");
	} else if (!obtainedFileName) {
		/* User object */
		if (getpwnam_r(username, &userPasswd, buf, 2048) == NULL)
			return (NULL);
		strcpy(filename, userPasswd.pw_dir);
		strcat(filename, "/.amiprofile");
	}
	if (username)
		free(username);

	if (stat(filename, &filestat) != NULL) {
		/* printf("File not found\n"); */
		return (NULL);
	}

	if ((file = open(filename, O_RDONLY)) == NULL) {
		/* printf("Unbale to open file\n"); */
		return (NULL);
	}

	bin_profile = (char *) malloc(filestat.st_size);
	if (read(file, bin_profile, filestat.st_size) != filestat.st_size)
		return (NULL);
	close(file);

	answer = ami_keymgnt_get_user_profile(bin_profile);
	free(bin_profile);
	return (answer);
}

static char *
ami_keymgnt_get_property_from_user_profile(const char *name, const char *id)
{
	char *answer = NULL;
	ami_user_profile *profile, *current;

	ami_keymgnt_initialize();
	switch (nsUserProfile) {
	case AMI_NS_FILE:
		profile = ami_keymgnt_read_user_profile_from_file(name);
		break;
	case AMI_NS_FNS:
		/* %%% TBI */
		break;
	case AMI_NS_NIS:
		/* %%% TBI */
		break;
	case AMI_NS_NISPLUS:
		/* %%% TBI */
		break;
	case AMI_NS_LDAP:
		/* %%% TBI */
		break;
	case AMI_NS_NONE:
	default:
		answer = NULL;
		break;
	}
	if (profile == NULL)
		return (NULL);
	while ((!answer) && profile) {
		if (strcasecmp(id, profile->id) == 0) {
			answer = strdup(profile->value);
			break;
		}
		current = profile;
		profile = profile->next;
		free(current->id);
		free(current->value);
		free(current);
	}
	while (profile) {
		current = profile;
		profile = profile->next;
		free(current->id);
		free(current->value);
		free(current);
	}
	return (answer);
}

char *
ami_keymgnt_get_default_rsa_signature_key_alias(const char *name)
{
	return (ami_keymgnt_get_property_from_user_profile(name,
	    "defaultRSAsignaturekey"));
}

char *
ami_keymgnt_get_default_rsa_encryption_key_alias(const char *name)
{
	return (ami_keymgnt_get_property_from_user_profile(name,
	    "defaultRSAencryptionkey"));
}

char *
ami_keymgnt_get_default_dsa_key_alias(const char *name)
{
	return (ami_keymgnt_get_property_from_user_profile(name,
	    "defaultDSAkey"));
}

char *
ami_keymgnt_get_default_dh_key_alias(const char *name)
{
	return (ami_keymgnt_get_property_from_user_profile(name,
	    "defaultDHkey"));
}

AMI_STATUS
ami_keymgnt_get_keystore(const char *name, void **ks, int *length)
{
	AMI_STATUS status = AMI_KEYPKG_NOT_FOUND;

	ami_keymgnt_initialize();
	(*ks) = NULL;
	(*length) = 0;

	/*
	 * Since the current implementation does not
	 * have different attributes for various keystore,
	 * the keytype defaults to RSA
	 */
	switch (nsKeystoreRSA) {
	case AMI_NS_FILE:
	{
		struct stat filestat;
		int file;
		char *filename =
			ami_keymgnt_get_property_from_user_profile(name,
			    "keystorefile");
		if (filename == NULL) {
			char fname[512], buf[2048];
			struct passwd userPasswd;
			if (getpwuid_r(getuid(), &userPasswd, buf, 2048) ==
				NULL)
				break;
			strcpy(fname, userPasswd.pw_dir);
			strcat(fname, "/.keystore");
			filename = strdup(fname);
		}
		if (stat(filename, &filestat) != NULL) {
			/* printf("File not found\n"); */
			break;
		}
		if ((file = open(filename, O_RDONLY)) == NULL) {
			/* printf("Unbale to open file\n"); */
			free(filename);
			break;
		}
		free(filename);
		(*ks) = (void *) malloc(filestat.st_size);
		if (read(file, (*ks), filestat.st_size) != filestat.st_size) {
			/* printf("Unable to read file\n"); */
			close(file);
			free(*ks);
			break;
		}
		close(file);
		(*length) = filestat.st_size;
		status = AMI_OK;
		break;
	}
	case AMI_NS_NONE:
	default:
		break;
	}
	return (status);
}

AMI_STATUS
ami_keymgnt_set_keystore(const char *name, void *ks, int length)
{
	AMI_STATUS status = AMI_KEYPKG_NOT_FOUND;

	ami_keymgnt_initialize();

	/*
	 * Since the current implementation does not
	 * have different attributes for various keystore,
	 * the keytype defaults to RSA
	 */
	switch (nsKeystoreRSA) {
	case AMI_NS_FILE:
	{
		int file;
		char *filename =
			ami_keymgnt_get_property_from_user_profile(name,
			    "keystorefile");
		if (filename == NULL) {
			char fname[512], buf[2048];
			struct passwd userPasswd;
			if (getpwuid_r(getuid(), &userPasswd, buf, 2048) ==
				NULL)
				break;
			strcpy(fname, userPasswd.pw_dir);
			strcat(fname, "/.keystore");
			filename = strdup(fname);
		}
		if ((file = open(filename, O_WRONLY | O_CREAT)) == NULL) {
			/* printf("Unbale to open file\n"); */
			free(filename);
			break;
		}
		free(filename);
		if (write(file, ks, length) != length) {
			/* printf("Unable to read file\n"); */
			close(file);
			break;
		}
		close(file);
		status = AMI_OK;
		break;
	}
	case AMI_NS_NONE:
	default:
		break;
	}
	return (status);
}

static AMI_STATUS
ami_keymgnt_decode_certificate(const void *certBin, int cert_len,
    ami_cert *certificate)
{
	AMI_STATUS ami_status;

	/* decode the encoded certificates */
	ami_status = __ami_rpc_decode_certificate(certBin, cert_len,
	    certificate);

	return (ami_status);
}

static int
isCertificatePresent(const ami_cert *certs, int count, const ami_cert cert)
{
	int i, dnEqual, serEqual;
	ami_handle_t amih = NULL;

	__ami_init(&amih, NULL, NULL, 0, 0, 0);
	for (i = 0; i < count; i++) {
		if ((__ami_cmp_serial(&(certs[i].info.serial),
		    &(cert.info.serial), &serEqual) == AMI_OK) &&
		    (serEqual == 0) && (__ami_cmp_dn(amih, certs[i].info.issuer,
		    cert.info.issuer, &dnEqual) == AMI_OK) && (dnEqual == 0)) {
			ami_end(amih);
			return (1);
		}
	}
	ami_end(amih);
	return (0);
}

extern void ami_nfree_cert(ami_cert_ptr pCert);

static AMI_STATUS
ami_keymgnt_get_certificates_from_file(const char *filename,
    const char *nameDN, ami_cert **certs, int *count)
{
	AMI_STATUS status = AMI_CERT_NOT_FOUND;
	int file, i, length, version, ptr, num_certs, dnEqual;
	struct stat filestat;
	char *cert_bin, *certs_bin;
	ami_handle_t amih = NULL;
	ami_name *inNameDN = NULL;

	/* Initialize */
	*certs = NULL;
	*count = 0;

	/* Open the file */
	if ((filename) && (stat(filename, &filestat) != NULL)) {
		/* Error message */
		return (status);
	}

	if ((file = open(filename, O_RDONLY)) == -1) {
		return (status);
	}

	certs_bin = (char *) malloc(filestat.st_size);
	if (read(file, certs_bin, filestat.st_size) != filestat.st_size) {
		close(file);
		free(certs_bin);
		return (status);
	}
	close(file);

	/* Read version */
	ptr = 0;
	ami_keymgnt_read_int(certs_bin, &ptr, &version);
	if (version != 1) {
		/* Illegal version */
		free(certs_bin);
		return (status);
	}

	/* Initialize AMI handle */
	__ami_init(&amih, NULL, NULL, 0, 0, 0);
	if ((status = ami_str2dn(amih, (char *) nameDN, &inNameDN)) != AMI_OK) {
		ami_end(amih);
		free(certs_bin);
		return (status);
	}

	/* Read count */
	ami_keymgnt_read_int(certs_bin, &ptr, &num_certs);
	(*certs) = (ami_cert *) calloc(num_certs, sizeof (ami_cert));

	/* Read individual cerificates and decode them */
	for (i = 0; i < num_certs; i++) {
		ami_keymgnt_read_int(certs_bin, &ptr, &length);
		cert_bin = 0;
		ami_keymgnt_read_bytes(certs_bin, &ptr, &cert_bin, length);
		if ((status = ami_keymgnt_decode_certificate(cert_bin,
		    length, &((*certs)[*count]))) != AMI_OK) {
			ami_end(amih);
			ami_free_dn(&inNameDN);
			free(certs_bin);
			free(cert_bin);
			return (status);
		}
		free(cert_bin);
		/* Compare the DNs */
		if ((__ami_cmp_dn(amih, inNameDN, ((*certs)
		    [*count]).info.subject,
		    &dnEqual) == AMI_OK) && (dnEqual == 0)) {
			(*count)++;
		} else {
			/* Delete the certificate */
			ami_nfree_cert(&((*certs)[*count]));
		}
	}
	ami_end(amih);
	ami_free_dn(&inNameDN);
	free(certs_bin);
	if ((*count) == 0)
		free(*certs);
	return (AMI_OK);
}

AMI_STATUS
ami_keymgnt_get_certificates(const char *name, ami_cert **certs, int *count)
{
	AMI_STATUS status = AMI_DN_NOT_FOUND;
	char buf[2048]; /* Buffer for passwd entry */
	struct passwd userPasswd;

	ami_keymgnt_initialize();
	(*certs) = NULL;
	(*count) = 0;

	switch (nsCertX509) {
	case AMI_NS_FILE:
	{
		AMI_STATUS status2;
		char *filename, *nameUnix, fname[512];
		char *nameUnix2 = NULL, *nameDN = NULL;
		ami_cert *certs1, *certs2;
		int i, count1, count2;

		/* Get the local identity */
		if (getuid() != 0) {
			if (getpwuid_r(getuid(), &userPasswd,
			    buf, 2048) == NULL)
				break;
			nameUnix = strdup(userPasswd.pw_name);
		} else {
			if (sysinfo(SI_HOSTNAME, buf, 2048) < 0)
				break;
			nameUnix = ami_get_ip_from_hostname(buf);
		}

		/* Construct the DN name */
		if (name == NULL) {
			nameDN = ami_keymgnt_get_property_from_user_profile(
			    nameUnix, "namedn");
		} else if (strchr(name, '=') == NULL) {
			if (strcasecmp(name, nameUnix) != 0)
				nameUnix2 = strdup(name);
			nameDN = ami_keymgnt_get_property_from_user_profile(
			    name, "namedn");
			if (nameDN == NULL) {
			/* Try locally created in ami_profile */
			    nameDN = ami_keymgnt_get_property_from_user_profile(
				    nameUnix, name);
			}
		} else {
			nameDN = strdup(name);
		}
		if (nameDN == NULL) {
			/* DN not found */
			free(nameUnix);
			if (nameUnix2)
				free(nameUnix2);
			break;
		}

		/* Get the filename for nameUnix */
		filename = ami_keymgnt_get_property_from_user_profile(
		    nameUnix, "certx509file");
		if (filename == NULL) {
			if ((int)inet_addr(nameUnix) == -1) {
				strcpy(fname, userPasswd.pw_dir);
				strcat(fname, "/.certx509");
				filename = strdup(fname);
			} else {
				strcpy(fname, "/etc/ami/keys/");
				strcat(fname, nameUnix);
				strcat(fname, ".certx509");
				filename = strdup(fname);
			}
		}
		status = ami_keymgnt_get_certificates_from_file(
		    filename, nameDN, &certs1, &count1);
		free(filename);
		free(nameUnix);
		if ((status != AMI_OK) && (nameUnix2 == NULL)) {
			free(nameDN);
			break;
		}

		if (nameUnix2 == NULL) {
			/* break */
			free(nameDN);
			*certs = certs1;
			*count = count1;
			break;
		}

		/* Construct the second file name if required */
		filename = ami_keymgnt_get_property_from_user_profile(
		    nameUnix2, "certx509file");
		if (filename == NULL) {
			if ((int)inet_addr(nameUnix2) == -1) {
				if (getpwnam_r(nameUnix2, &userPasswd,
				    buf, 2048) != NULL) {
					strcpy(fname, userPasswd.pw_dir);
					strcat(fname, "/.certx509");
					filename = strdup(fname);
				}
			} else {
				strcpy(fname, "/etc/ami/keys/");
				strcat(fname, nameUnix2);
				strcat(fname, ".certx509");
				filename = strdup(fname);
			}
		}
		status2 = ami_keymgnt_get_certificates_from_file(
		    filename, nameDN, &certs2, &count2);
		free(filename);
		free(nameUnix2);
		free(nameDN);
		if ((status2 != AMI_OK) && (status != AMI_OK)) {
			status = AMI_DN_NOT_FOUND;
			/* Do nothing */
		} else if ((status2 != AMI_OK) && (status == AMI_OK)) {
			*certs = certs1;
			*count = count1;
		} else if ((status2 == AMI_OK) && (status != AMI_OK)) {
			status = AMI_OK;
			*certs = certs2;
			*count = count2;
		} else if ((*count = count1 + count2) > 0) {
			/* Merge the results */
			*certs = (ami_cert *) calloc(*count, sizeof (ami_cert));
			for (i = 0; i < count1; i++)
				(*certs)[i] = certs1[i];
			for (i = 0; i < count2; i++) {
				/* Check if the certificate is present */
				if (isCertificatePresent((*certs),
				    (i+count1), certs2[i])) {
					(*count)--;
					ami_nfree_cert(&certs2[i]);
				} else {
					(*certs)[i+count1] = certs2[i];
				}
			}
			free(certs1);
			free(certs2);
		}
		break;
	}
	case AMI_NS_NONE:
	default:
		break;
	}
	return (status);
}
