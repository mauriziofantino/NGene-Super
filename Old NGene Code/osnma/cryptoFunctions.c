/**
 * @file cryptoFunctionsTest.c
 *
 * @author Micaela Troglia Gamba
 *
 * @date 16/05/2019
 *
 * @version 0.1
 *
 * @brief Testbed for openssl library function to be used for OSNMA.
 */

#include <unistd.h>
#include "cryptoFunctions.h"
#include "ioFunctions.h"
#include "osnmaUtilities.h"
#include "mackList.h"
#include "osnmaValidations.h"
#include "timeConstants.h"

extern int threadTESLAKeyVerificationStopIsRunning;




#define handleErrors abort

/*If you have all of your data at once, use this:*/
int SHA256Native(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf)
{

    SHA256(ibuf, ibufSize, obuf);

	#ifdef DEBUG
		int i;
		for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif


    return 0;
}



/*If, on the other hand, you only get your data one piece at a time and you want to
 * compute the hash as you receive that data, then use this:
 */
int SHA256InitUpdateFinalPattern(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf)
{


	// Object to hold the current state of the hash
	SHA256_CTX ctx;
    SHA256_Init(&ctx);

    // Hash each piece of data as it comes in:
    SHA256_Update(&ctx, ibuf,ibufSize);


    // When you're done with the data, finalize it:
    SHA256_Final(obuf, &ctx);

	#ifdef DEBUG
		int i;
		for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif


    return 0;
}


///*If, on the other hand, you only get your data one piece at a time and you want to
// * compute the hash as you receive that data, then use this:
// */
int EVP_HASH(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf,const EVP_MD *function)
{

	unsigned int obuf_len;


	// Object to hold the current state of the hash
	EVP_MD_CTX *ctx;
	if((ctx = EVP_MD_CTX_create()) == NULL)
	    handleErrors();

	if(1 != EVP_DigestInit_ex(ctx, function, NULL))
		handleErrors();

	if(1 != EVP_DigestUpdate(ctx, ibuf, ibufSize))
	    handleErrors();

	if(1 != EVP_DigestFinal_ex(ctx, obuf, &obuf_len))
	    handleErrors();

	EVP_MD_CTX_destroy(ctx);

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < obuf_len; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif

    return 0;
}




int EVP_HMAC(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf,const EVP_MD *function, const unsigned char *key,size_t keySize)
{




	size_t obuf_len;
	EVP_PKEY *pkey = NULL;

	// Object to hold the current state of the hash
	EVP_MD_CTX *ctx;
	if((ctx = EVP_MD_CTX_create()) == NULL)
	    handleErrors();

	if(!(pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, key, keySize)))
		handleErrors();

	if(1 != EVP_DigestSignInit(ctx, NULL, function, NULL, pkey))
		handleErrors();

	/*EVP_DigestSignUpdate is the same as EVP_DigestUpdate*/
	if(1 != EVP_DigestUpdate(ctx, ibuf, ibufSize))
	    handleErrors();

	if(1 != EVP_DigestSignFinal(ctx, obuf, &obuf_len))
		handleErrors();

	EVP_PKEY_free(pkey);
	EVP_MD_CTX_destroy(ctx);

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < obuf_len; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif



    return 0;
}



int HMACNative(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf,const EVP_MD *function, const unsigned char *key,size_t keySize)
{
	unsigned int obuf_len;

	HMAC_CTX *ctx;
	if((ctx = HMAC_CTX_new()) == NULL)
	    handleErrors();

	if(!HMAC_Init_ex(ctx, key, keySize, function, NULL))
		handleErrors();

	if(!HMAC_Update(ctx, ibuf, ibufSize))
		handleErrors();

	if(!HMAC_Final(ctx, obuf, &obuf_len))
		handleErrors();

	HMAC_CTX_free(ctx);

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < obuf_len; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif


    return 0;
}



int CMAC_AES_function(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf, const unsigned char *key, size_t KSBytes)
{
	size_t obuf_len;

	CMAC_CTX *ctx;
	if((ctx = CMAC_CTX_new()) == NULL)
	    handleErrors();

	if(!CMAC_Init(ctx, key, KSBytes, EVP_aes_128_cbc(), NULL))
		handleErrors();

	if(!CMAC_Update(ctx, ibuf, ibufSize))
		handleErrors();

	if(!CMAC_Final(ctx, obuf, &obuf_len))
		handleErrors();

	CMAC_CTX_free(ctx);

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < obuf_len; i++) {
			printf("%02X", obuf[i]);
		}
		printf("\n");
	#endif

    return 0;
}


//****************************************************

int ECDSA(int nid, int generateKeyFLag, unsigned char *digest,int digestLenght)
{

	EC_KEY    *eckey;
	BIGNUM *prv= BN_new();
	const EC_GROUP *group;
	EC_POINT *pub= NULL;
	unsigned int sigLen;
	unsigned char *signature = NULL;
	char *privKey,*pubKey;
	char privKey_P224[]	="2e6f47d573e9febe81e463aba776bb498f75225a62a8f4128ffcdda8";/*224 bits (28 bytes)*/
	char pubKey_P224[]="02baeb3a28497186484bbb78d68a05ca18e5e39b002701436b168c9fac";/*compressed 232 bits (29 bytes)*/
	char privKey_P256[]	="d33712cdac9c79f332b5d44f0703596f87b8dd168d38811931b47daaaf29d25c";/*256 bits (32 bytes)*/
	char pubKey_P256[]="028ee90139bef9399ba692e4e252bc40aa83ddab72b0bb03600eee2aa93f70ab4e";/*compressed 264 bits (33 bytes)*/
	char privKey_P384[]	="2a929d3f320e80fb6b247c1b06a143260e3602511c3aa061fb25b6193cc69132db0f43cfbc6db35d573cb1a3d4e46a20";/*384 bits (48 bytes)*/
	char pubKey_P384[]="020404abf6b70c4b22870499ad50fd1ff40c6b7ae69e2cc236d7e6ca3bf6122c3d3d0dd17618afcd7db98b665966be8019";/*compressed 392 bits (49 bytes)*/
	char privKey_P521[]	="004311e43d4d22c6526cae3f0a2dfb2ce9dc532fd2566be746947e8b9a2c5693957bbc2e5174605b01cd5edbd03f6c0405e1e01767d62852cd654f97f036d76518cf";/*521 bits (66 bytes)*/
	char pubKey_P521[]="03018a86aebbb6b8470d44e25ddaf836d0d9918423ec25e800dffaa06202220d44cb1f15db89429ebf19943ea95898122e7ee1cab8b56de491351a47e8af4c77c5e070";/*compressed 536 bits (67 bytes)*/

	switch(digestLenght)
	{
	case SHA224_DIGEST_LENGTH:
			privKey=privKey_P224;
			pubKey=pubKey_P224;
		break;
	case SHA256_DIGEST_LENGTH:
			privKey=privKey_P256;
			pubKey=pubKey_P256;
		break;
	case SHA384_DIGEST_LENGTH:
			privKey=privKey_P384;
			pubKey=pubKey_P384;
		break;
	default:
			privKey=privKey_P521;
			pubKey=pubKey_P521;
		break;
	}

     //NID_ecdsa_with_SHA256
	/*To create the EC*/
	/*EC_GROUP *curve;

	if(NULL == (curve = EC_GROUP_new_by_curve_name(NID_secp224r1)))
	    handleErrors();*/
	/*To create keys and set the key to use the curve using the EC_KEY_set_group */
	/**/
	/*Alternatively, the creation of the curve and the key can be done in one step as shown below using*/
	eckey = EC_KEY_new_by_curve_name(nid);
	if (eckey == NULL) {
	   /* error */
		handleErrors();
	}
	group = EC_KEY_get0_group(eckey);

	if(generateKeyFLag==1)
	{
		/*At this point the EC_KEY object has been set up and associated with the curve - but it is empty. There is no key data in it, thus
		 * it has to be generated. Note that this operation generates a public and private key pair.*/
		if (EC_KEY_generate_key(eckey) == 0) {
		   /* error */
			handleErrors();
		}
	}
	else
	{
		/*Alternatively you may already know either the private key, the public key, or both.
		 * Setting the private key and/or public key is done as follows: */
		/*	 Set up private key in prv
		 Set up public key in pub*/

		BN_hex2bn(&prv, privKey);
		if(1 != EC_KEY_set_private_key(eckey, prv))
			{
				handleErrors();
			}

		pub = EC_POINT_new(group);
		pub = EC_POINT_hex2point(group,pubKey, pub, NULL);
		if(1 != EC_KEY_set_public_key(eckey, pub))
		{
			handleErrors();
		}

		/*If you set the private key then you must also set the public key. There have been occasional questions
		 * on the openssl-users email list from people who only have the private key but do not know the public key.
		 * Fortunately calculating the public key is simply a matter of multiplying the private key by the generator
		 * for the curve using EC_POINT_mul: */
		/*if (1 != EC_POINT_mul(curve, pub, prv, NULL, NULL, ctx))
			handleErrors();*/
	}


	/* Verifies that a private and/or public key is valid */
	if (!EC_KEY_check_key(eckey)) {
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"EC_KEY_check_key failed.\n");
		handleErrors();
	}


	/* Return the maximum number of bytes required for the signature */
	sigLen = ECDSA_size(eckey);
	if ((signature = OPENSSL_malloc(sigLen)) == NULL)
	{
		handleErrors();
	}

	#ifdef DEBUG
		printf("Maximum sigLen  = %d\n", (int)sigLen);
	#endif

	/*Compute the signature using the private key*/
	if (!ECDSA_sign(0, digest, digestLenght, signature, &sigLen, eckey)) {
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"ECDSA_sign failed.\n");
		handleErrors();
	}


	/* Verify signature using the public key*/
	if (ECDSA_verify(0, digest, digestLenght, signature, sigLen, eckey) != 1) {
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Signature not verified\n");
		handleErrors();
	}
	else
	{
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature successfully verified\n");
	}

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < sigLen; i++) {
			printf("%02X", signature[i]);
		}
		printf("\n");
		printf("sigLen = %d\n", sigLen);
	#endif

	BN_clear_free(prv);
	OPENSSL_free(signature);
	EC_POINT_free(pub);
	EC_KEY_free(eckey);

	return 0;
}

int EVP_ECDSA(unsigned char *ibuf,size_t ibufSize,int nid,const EVP_MD *function, int generateKeyFLag,int digestLenght)
{

	EC_KEY *eckey = NULL;		/* EC key pair */
	EVP_PKEY *evp_key = NULL;	/* EVP private key */

	EVP_MD_CTX *evpx = NULL;	/* EVP_MD context */
	unsigned char *signature = NULL;/*ASN1/DER encoded*/
	unsigned char *signatureRaw = NULL;
	unsigned char *signatureASN1DEREncoded = NULL;
	size_t sigLen = 0;
	size_t rawSigLen,sigLenReencoded;
	BIGNUM *prv= BN_new();
	const EC_GROUP *group;
	EC_POINT *pub= NULL;
	char *privKey,*pubKey;
	char privKey_P224[]	="2e6f47d573e9febe81e463aba776bb498f75225a62a8f4128ffcdda8";/*224 bits (28 bytes)*/
	char pubKey_P224[]="02baeb3a28497186484bbb78d68a05ca18e5e39b002701436b168c9fac";/*compressed 232 bits (29 bytes)*/
	char privKey_P256[]	="d33712cdac9c79f332b5d44f0703596f87b8dd168d38811931b47daaaf29d25c";/*256 bits (32 bytes)*/
	char pubKey_P256[]="028ee90139bef9399ba692e4e252bc40aa83ddab72b0bb03600eee2aa93f70ab4e";/*compressed 264 bits (33 bytes)*/
	char privKey_P384[]	="2a929d3f320e80fb6b247c1b06a143260e3602511c3aa061fb25b6193cc69132db0f43cfbc6db35d573cb1a3d4e46a20";/*384 bits (48 bytes)*/
	char pubKey_P384[]="020404abf6b70c4b22870499ad50fd1ff40c6b7ae69e2cc236d7e6ca3bf6122c3d3d0dd17618afcd7db98b665966be8019";/*compressed 392 bits (49 bytes)*/
	char privKey_P521[]	="004311e43d4d22c6526cae3f0a2dfb2ce9dc532fd2566be746947e8b9a2c5693957bbc2e5174605b01cd5edbd03f6c0405e1e01767d62852cd654f97f036d76518cf";/*521 bits (66 bytes)*/
	char pubKey_P521[]="03018a86aebbb6b8470d44e25ddaf836d0d9918423ec25e800dffaa06202220d44cb1f15db89429ebf19943ea95898122e7ee1cab8b56de491351a47e8af4c77c5e070";/*compressed 536 bits (67 bytes)*/

	switch(digestLenght)
	{
	case SHA224_DIGEST_LENGTH:
			privKey=privKey_P224;
			pubKey=pubKey_P224;
		break;
	case SHA256_DIGEST_LENGTH:
			privKey=privKey_P256;
			pubKey=pubKey_P256;
		break;
	case SHA384_DIGEST_LENGTH:
			privKey=privKey_P384;
			pubKey=pubKey_P384;
		break;
	default:
			privKey=privKey_P521;
			pubKey=pubKey_P521;
		break;
	}

	/*The creation of the curve and the key can be done in one step as shown below using*/
	eckey = EC_KEY_new_by_curve_name(nid);
	if (eckey == NULL) {
		handleErrors();
	}
	group = EC_KEY_get0_group(eckey);

	if(generateKeyFLag==1)
	{
		/*At this point the EC_KEY object has been set up and associated with the curve - but it is empty. There is no key data in it, thus
		* it has to be generated. Note that this operation generates a public and private key pair.*/
		if (!EC_KEY_generate_key(eckey)) {
			handleErrors();
		}
	}
	else
	{
		/*Alternatively you may already know either the private key, the public key, or both.
		 * Setting the private key and/or public key is done as follows: */
		/*	 Set up private key in prv
		 Set up public key in pub*/

		BN_hex2bn(&prv, privKey);
		if(1 != EC_KEY_set_private_key(eckey, prv))
			{
				handleErrors();
			}

		pub = EC_POINT_new(group);
		pub = EC_POINT_hex2point(group,pubKey, pub, NULL);
		if(1 != EC_KEY_set_public_key(eckey, pub))
		{
			handleErrors();
		}

		/*If you set the private key then you must also set the public key. There have been occasional questions
		 * on the openssl-users email list from people who only have the private key but do not know the public key.
		 * Fortunately calculating the public key is simply a matter of multiplying the private key by the generator
		 * for the curve using EC_POINT_mul: */
		/*if (1 != EC_POINT_mul(curve, pub, prv, NULL, NULL, ctx))
			handleErrors();*/

	}
	/*  Allocates an empty EVP_PKEY structure which is used by OpenSSL to store private key */
	if ((evp_key = EVP_PKEY_new()) == NULL) {
		handleErrors();
	}
	/*Set the key referenced by evp_key to eckey.*/
	if (!EVP_PKEY_set1_EC_KEY(evp_key, eckey)) {
		handleErrors();
	}

	/* EVP_MD context */
	if ((evpx = EVP_MD_CTX_create()) == NULL) {
		handleErrors();
	}

	/* Return the maximum number of bytes required for the signature */
	sigLen = EVP_PKEY_size(evp_key);
	signature = (unsigned char *) OPENSSL_malloc(sigLen);
	if (signature == NULL) {
		handleErrors();
	}

	#ifdef DEBUG
		printf("Maximum sigLen  = %d\n", (int)sigLen);
		//printf("mesgLen = %d\n", mesgLen);
	#endif

	if(1 != EVP_DigestSignInit(evpx, NULL, function, NULL, evp_key))
			handleErrors();

	/*EVP_DigestSignUpdate is the same as EVP_DigestUpdate*/
	if(1 != EVP_DigestUpdate(evpx, ibuf, ibufSize))
		handleErrors();

	/*ASN.1 DER encoded signature*/
	if(1 != EVP_DigestSignFinal(evpx, signature, &sigLen))
		handleErrors();
//*************************************************************

	/*Decode the ASN1/DER signature to raw bit stream*/
	signatureRaw = (unsigned char *) OPENSSL_malloc(sigLen);
	if (signatureRaw == NULL) {
		handleErrors();
	}
	rawSigLen=sigLen;
	memcpy(signatureRaw, signature, sigLen);
	rawSigLen= br_ecdsa_asn1_to_raw(signatureRaw, rawSigLen);

	signatureASN1DEREncoded = (unsigned char *) OPENSSL_malloc(sigLen);
	if (signatureASN1DEREncoded == NULL) {
		handleErrors();
	}
	sigLenReencoded=rawSigLen;
	memcpy(signatureASN1DEREncoded, signatureRaw, rawSigLen);
	sigLenReencoded=br_ecdsa_raw_to_asn1(signatureASN1DEREncoded, sigLenReencoded);

	#ifdef DEBUG
		unsigned int i;
		for (i = 0; i < rawSigLen; i++) {
			printf("%02X", signatureRaw[i]);
		}
		printf("\n");
	#endif
	SwrxLog(NGENE_LOG_SEVERITY_NOTE,"Raw signature length = %d bytes\n",(int) rawSigLen);
	//****************************************************************
	if(1 != EVP_DigestVerifyInit(evpx,NULL ,function,NULL,evp_key))
			handleErrors();

	/* Initialize `key` with a public key */
	if(1 != EVP_DigestVerifyUpdate(evpx, ibuf, ibufSize))
		handleErrors();

	if(1 == EVP_DigestVerifyFinal(evpx, signatureASN1DEREncoded, sigLenReencoded))
	{
	    /* Success */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature successfully verified\n");
	}
	else
	{
	    /* Failure */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature not verified\n");
	}


	#ifdef DEBUG
		for (i = 0; i < sigLen; i++) {
			printf("%02X", signature[i]);
		}
		printf("\n");
		printf("sigLen = %d\n",(int) sigLen);
	#endif


	BN_clear_free(prv);
	EC_POINT_free(pub);
	EVP_PKEY_free(evp_key);
	EC_KEY_free(eckey);
	EVP_MD_CTX_destroy(evpx);
	OPENSSL_free(signature);
	OPENSSL_free(signatureRaw);
	OPENSSL_free(signatureASN1DEREncoded);


	return 0;
}




int PEM_ASN1DER_decoding(unsigned char *pubKeyPEMEncapsulated,size_t pubKeyPEMEncapsulatedSize, unsigned char *pubKeyPEM,size_t pubKeyPEMSize, unsigned char *pubKeyDecoded,size_t pubKeyUncompressedSize)
{

	/*PEM Header for Public Keys */
	char headerPEM[27]="-----BEGIN PUBLIC KEY-----\n";
	/*PEM Footer for Public Keys */
	char footerPEM[26]="\n-----END PUBLIC KEY-----";



	/*Header Bytes for the ASN1-DER encoded for the P-224 elliptic curve*/
	unsigned char headerBytes_P224[23]={0x30,0x4E,0x30,0x10,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,0x06,0x05,0x2B,0x81,0x04,0x00,0x21,0x03,0x3A,0x00};
	/*Header Bytes for the ASN1-DER encoded for the P-256 elliptic curve*/
	unsigned char headerBytes_P256[26]={0x30,0x59,0x30,0x13,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,0x06,0x08,0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07,0x03,0x42,0x00};
	/*Header Bytes for the ASN1-DER encoded for the P-384 elliptic curve*/
	unsigned char headerBytes_P384[23]={0x30,0x76,0x30,0x10,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,0x06,0x05,0x2B,0x81,0x04,0x00,0x22,0x03,0x62,0x00};
	/*Header Bytes for the ASN1-DER encoded for the P-521 elliptic curve*/
	unsigned char headerBytes_P521[25]={0x30,0x81,0x9B,0x30,0x10,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01,0x06,0x05,0x2B,0x81,0x04,0x00,0x23,0x03,0x81,0x86,0x00};

		/*Extract the pure 64 base Public Key*/
		if(!CRYPTO_memcmp(headerPEM, pubKeyPEMEncapsulated, 27))
		{
			if(!CRYPTO_memcmp(footerPEM, &pubKeyPEMEncapsulated[pubKeyPEMEncapsulatedSize-26], 26))
			{
				memcpy(pubKeyPEM, &pubKeyPEMEncapsulated[27], pubKeyPEMSize);
			}
		}

		/*PEM decoding of the Public Key*/
    	size_t out_len;
    	unsigned char *pubKeyASN1DER=base64_decode(pubKeyPEM, pubKeyPEMSize,&out_len);

		#ifdef DEBUG
    	int index;
    	printf("Unbase64: *");
		for(index=0; index<(int)out_len;index++)
		{
		  printf("%02X", pubKeyASN1DER[index]);
		}
		printf("*\n");
		#endif
		/*ASN1/DER decoding of the Public Key*/
		if(!CRYPTO_memcmp(headerBytes_P224, pubKeyASN1DER, 23))
		{
			memcpy(pubKeyDecoded, &pubKeyASN1DER[23], pubKeyUncompressedSize);
		}
		else
		{
			if(!CRYPTO_memcmp(headerBytes_P256, pubKeyASN1DER, 26))
			{
				memcpy(pubKeyDecoded, &pubKeyASN1DER[26], pubKeyUncompressedSize);
			}
			else
			{
				if(!CRYPTO_memcmp(headerBytes_P384, pubKeyASN1DER, 23))
				{
					memcpy(pubKeyDecoded, &pubKeyASN1DER[23], pubKeyUncompressedSize);
				}
				else
				{
					if(!CRYPTO_memcmp(headerBytes_P521, pubKeyASN1DER, 25))
					{
						memcpy(pubKeyDecoded, &pubKeyASN1DER[25], pubKeyUncompressedSize);
					}
					else
					{
						SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Public key type not supported\n");
					}

				}

			}
		}
		#ifdef DEBUG
		for(index=0; index<(int)pubKeyUncompressedSize;index++)
		{
		  printf("%02X", pubKeyDecoded[index]);
		}
		printf("\n");
		#endif

		free(pubKeyASN1DER);

	return 0;
}


int DSM_KROOT_SignatureVerification(unsigned char *M,size_t MSize,int NPKT, unsigned char * DS,unsigned char *pubKey,size_t pubKeySize)
{

	const EVP_MD *function;
	int DSSize;
	int nid;
	//******************************

	EC_KEY *eckey = NULL;		/* EC key pair */
	EVP_PKEY *evp_key = NULL;	/* EVP private key */

	EVP_MD_CTX *evpx = NULL;	/* EVP_MD context */
	unsigned char *signatureASN1DEREncoded = NULL;
	size_t sigLen = 0;
	size_t sigLenReencoded;
	const EC_GROUP *group;
	EC_POINT *pub= NULL;

	switch(NPKT)
	{
		case 0:
				function=EVP_sha224();
//				hashSize=SHA224_DIGEST_LENGTH;
				nid=NID_secp224r1;
				DSSize=56;
			break;
		case 1:
				function=EVP_sha256();
				//hashSize=SHA256_DIGEST_LENGTH;
				nid=NID_X9_62_prime256v1;
				DSSize=64;
			break;
		case 2:
				function=EVP_sha384();
				//hashSize=SHA384_DIGEST_LENGTH;
				nid=NID_secp384r1;
				DSSize=96;
			break;
		case 3:
				function=EVP_sha512();
				//hashSize=SHA512_DIGEST_LENGTH;
				nid=NID_secp521r1;
				DSSize=132;
			break;
		default:
				function=EVP_sha224();
				//hashSize=SHA224_DIGEST_LENGTH;
				nid=NID_secp224r1;
				DSSize=56;
			break;
	}

	/*The creation of the curve and the key can be done in one step as shown below using*/
	eckey = EC_KEY_new_by_curve_name(nid);
	if (eckey == NULL) {
		handleErrors();
	}
	group = EC_KEY_get0_group(eckey);

    /*Read the Public key (raw binary format)*/
	pub = EC_POINT_new(group);
	/*If pubKey="0473B440B0..." use the following function*/
	//pub = EC_POINT_hex2point(group,pubKey, pub, NULL);
	/*otherwise (pubKey={0x04,0x73,0xB4,0x40,0xB0,...} octet string) use the following one*/
	EC_POINT_oct2point(group, pub,pubKey, pubKeySize, NULL);
	if(1 != EC_KEY_set_public_key(eckey, pub))
	{
		handleErrors();
	}
	/*  Allocates an empty EVP_PKEY structure which is used by OpenSSL to store private key */
	if ((evp_key = EVP_PKEY_new()) == NULL) {
		handleErrors();
	}
	/*Set the key referenced by evp_key to eckey.*/
	if (!EVP_PKEY_set1_EC_KEY(evp_key, eckey)) {
		handleErrors();
	}

	/* EVP_MD context */
	if ((evpx = EVP_MD_CTX_create()) == NULL) {
		handleErrors();
	}

	/* Return the maximum number of bytes required for the signature */
	sigLen = EVP_PKEY_size(evp_key);


	signatureASN1DEREncoded = (unsigned char *) OPENSSL_malloc(sigLen);
	if (signatureASN1DEREncoded == NULL) {
		handleErrors();
	}
	sigLenReencoded=DSSize;
	memcpy(signatureASN1DEREncoded, DS, DSSize);
	sigLenReencoded=br_ecdsa_raw_to_asn1(signatureASN1DEREncoded, sigLenReencoded);

	if(1 != EVP_DigestVerifyInit(evpx,NULL ,function,NULL,evp_key))
			handleErrors();

	/* Initialize `key` with a public key */
	if(1 != EVP_DigestVerifyUpdate(evpx, M, MSize))
		handleErrors();

	if(1 == EVP_DigestVerifyFinal(evpx, signatureASN1DEREncoded, sigLenReencoded))
	{
	    /* Success */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature successfully verified\n");
	}
	else
	{
	    /* Failure */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature not verified\n");
	}


	#ifdef DEBUG
		for (i = 0; i < sigLen; i++) {
			printf("%02X", signature[i]);
		}
		printf("\n");
		printf("sigLen = %d\n",(int) sigLen);
	#endif


	EC_POINT_free(pub);
	EVP_PKEY_free(evp_key);
	EC_KEY_free(eckey);
	EVP_MD_CTX_destroy(evpx);
	OPENSSL_free(signatureASN1DEREncoded);


	return 0;
}




int DSM_KROOT_SignatureVerification_fromPEM(unsigned char *M,size_t MSize,int NPKT, unsigned char * DS, char *filename)
{

	const EVP_MD *function;
	int DSSize;
	//******************************

	EVP_PKEY *evp_key = NULL;	/* EVP private key */

	EVP_MD_CTX *evpx = NULL;	/* EVP_MD context */
	unsigned char *signatureASN1DEREncoded = NULL;
	size_t sigLen = 0;
	size_t sigLenReencoded;
	FILE *PEMFile;


	switch(NPKT)
	{
		case 0:
				function=EVP_sha224();
//				hashSize=SHA224_DIGEST_LENGTH;
				//nid=NID_secp224r1;
				DSSize=56;
			break;
		case 1:
				function=EVP_sha256();
				//hashSize=SHA256_DIGEST_LENGTH;
				//nid=NID_X9_62_prime256v1;
				DSSize=64;
			break;
		case 2:
				function=EVP_sha384();
				//hashSize=SHA384_DIGEST_LENGTH;
				//nid=NID_secp384r1;
				DSSize=96;
			break;
		case 3:
				function=EVP_sha512();
				//hashSize=SHA512_DIGEST_LENGTH;
				//nid=NID_secp521r1;
				DSSize=132;
			break;
		default:
				function=EVP_sha224();
				//hashSize=SHA224_DIGEST_LENGTH;
				//nid=NID_secp224r1;
				DSSize=56;
			break;
	}


	PEMFile=fopen(filename,"rb");
	/*Extract the Public Key from PEM format file*/
	evp_key = PEM_read_PUBKEY(PEMFile, NULL, NULL, NULL);
	/*ASN1-DER Decoding of the Public Key */
	d2i_PUBKEY_fp(PEMFile, &evp_key);


	/* EVP_MD context */
	if ((evpx = EVP_MD_CTX_create()) == NULL) {
		handleErrors();
	}

	/* Return the maximum number of bytes required for the signature */
	sigLen = EVP_PKEY_size(evp_key);


	signatureASN1DEREncoded = (unsigned char *) OPENSSL_malloc(sigLen);
	if (signatureASN1DEREncoded == NULL) {
		handleErrors();
	}
	sigLenReencoded=DSSize;
	memcpy(signatureASN1DEREncoded, DS, DSSize);
	sigLenReencoded=br_ecdsa_raw_to_asn1(signatureASN1DEREncoded, sigLenReencoded);

	if(1 != EVP_DigestVerifyInit(evpx,NULL ,function,NULL,evp_key))
			handleErrors();

	/* Initialize `key` with a public key */
	if(1 != EVP_DigestVerifyUpdate(evpx, M, MSize))
		handleErrors();

	if(1 == EVP_DigestVerifyFinal(evpx, signatureASN1DEREncoded, sigLenReencoded))
	{
	    /* Success */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature successfully verified\n");
	}
	else
	{
	    /* Failure */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Signature not verified\n");
	}


	#ifdef DEBUG
		for (i = 0; i < sigLen; i++) {
			printf("%02X", signature[i]);
		}
		printf("\n");
		printf("sigLen = %d\n",(int) sigLen);
	#endif



	EVP_PKEY_free(evp_key);
	EVP_MD_CTX_destroy(evpx);
	OPENSSL_free(signatureASN1DEREncoded);
	fclose(PEMFile);


	return 0;
}



EVP_PKEY * PublicKey_PEMToEVPKEY(FILE *PEMFile)
{

	EVP_PKEY *evp_key;

	/*Extract the Public Key from PEM format file*/
	evp_key=PEM_read_PUBKEY(PEMFile, NULL, NULL, NULL);
	/*ASN1-DER Decoding of the Public Key */
	d2i_PUBKEY_fp(PEMFile, &(evp_key));

	return evp_key;
}


int PublicKey_BNToEVPKEY(int nid,unsigned char *pubKey,size_t pubKeySize,EVP_PKEY *evp_key)
{

	EC_KEY *eckey = NULL;		/* EC key pair */
	const EC_GROUP *group;
	EC_POINT *pub= NULL;

	/*The creation of the curve and the key can be done in one step as shown below using*/
	eckey = EC_KEY_new_by_curve_name(nid);
	if (eckey == NULL) {
		handleErrors();
	}
	group = EC_KEY_get0_group(eckey);

    /*Read the Public key (raw binary format)*/
	pub = EC_POINT_new(group);
	/*If pubKey="0473B440B0..." use the following function*/
	//pub = EC_POINT_hex2point(group,pubKey, pub, NULL);
	/*otherwise (pubKey={0x04,0x73,0xB4,0x40,0xB0,...} octet string) use the following one*/
	EC_POINT_oct2point(group, pub,pubKey, pubKeySize, NULL);
	if(1 != EC_KEY_set_public_key(eckey, pub))
	{
		handleErrors();
	}

	/*Set the key referenced by evp_key to eckey.*/
	if (!EVP_PKEY_set1_EC_KEY(evp_key, eckey)) {
		handleErrors();
	}

	EC_POINT_free(pub);
	//EVP_PKEY_free(evp_key);
	EC_KEY_free(eckey);


	return 0;
}


int PublicKey_PkrToEVPKEY(TNewPublicKeyType npkt,unsigned char *pubKey,size_t pubKeySize,EVP_PKEY *evp_key)
{
	int nid;

	switch(npkt)
	{
	case ECDSA_P224:
		// NID:713    "NIST/SECG curve over a 224 bit prime field"
		nid=NID_secp224r1;
		break;
	case ECDSA_P256:
		// NID:415    "X9.62/SECG curve over a 256 bit prime field"
		nid=NID_X9_62_prime256v1;
		break;
	case ECDSA_P384:
		// NID:715    "NIST/SECG curve over a 384 bit prime field"
		nid=NID_secp384r1;
		break;
	case ECDSA_P521:
		// NID:716    "NIST/SECG curve over a 521 bit prime field"
		nid=NID_secp521r1;
		break;
	default:
		// put a default value
		nid=NID_secp224r1;
		break;
	}

	return PublicKey_BNToEVPKEY(nid,pubKey,pubKeySize,evp_key);
}


int DSM_KROOT_SignatureVerification_fromEVPKEY(unsigned char *dsmBlocks,TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars,TChannelOsnmaVariables *chOsnma)
{
    unsigned char *DSToVerify=NULL;
    #ifdef PATCH_JRC_GENERATOR_DSM
    int byteIdxOut=0;
    int bitIdxOut=0;
    int byteIdxIn=0;
    int bitIdxIn=0;
    #endif

	EVP_MD_CTX *evpx = NULL;	/* EVP_MD context */
	unsigned char *signatureASN1DEREncoded = NULL;
	size_t sigLen = 0;
	size_t sigLenReencoded;
	unsigned int P0;/*Number of Padding Bits (0 to 7 bits)*/

	if(rxOsnma->evp_keyValid==FALSE_FLAG)
	{
		return INVALID_RETURN;
	}

	/*P0=0, the following line is just to remember that it exist in case of changes in the next future*/
	P0=((osnmaPars->MSize*8)%8);

//					memcpy (&NMA_Header, &chOsnma->chDsm[0], 1);
//					memcpy (&CIDKR_NMACK_HF_MF, &dsmData->dsmBlocks[1], 1);
//					memcpy (&KS_MS, &dsmData->dsmBlocks[2], 1);
//					memcpy (&MACLT, &dsmData->dsmBlocks[3], 1);
//					memcpy (&Rsvd_MO_KROOT_WN, &dsmData->dsmBlocks[4], 1);
//					memcpy (&KROOT_WN, &dsmData->dsmBlocks[5], 1);
//					memcpy (&KROOT_TOWH, &dsmData->dsmBlocks[6], 1);
//					memcpy (alpha, &dsmData->dsmBlocks[7], 6);
//					memcpy (KROOT, &dsmData->dsmBlocks[13], KSBytes);
	/**/


	/*Concatenate fields to build the message M*/
//					memcpy (M, &NMA_Header, 1);
//					memcpy (&M[1], &CIDKR_NMACK_HF_MF, 1);
//					memcpy (&M[2], &KS_MS, 1);
//					memcpy (&M[3], &MACLT, 1);
//					memcpy (&M[4], &Rsvd_MO_KROOT_WN, 1);
//					memcpy (&M[5], &KROOT_WN, 1);
//					memcpy (&M[6], &KROOT_TOWH, 1);
//					memcpy (&M[7], alpha, 6);
//					memcpy (&M[7+6], KROOT, KSBytes);

	ArrayClean((char *)rxOsnma->M,rxOsnma->MSizeMax);
	ArrayClean((char *)rxOsnma->DS,MAX_DS_SIZE_BYTES);
	memcpy (rxOsnma->M, &chOsnma->chDsm[0], 1);
	memcpy (&rxOsnma->M[1], &dsmBlocks[1], 12);
	memcpy (&rxOsnma->M[7+6], &dsmBlocks[13], osnmaPars->keySizeBytes);
    #ifdef PATCH_JRC_GENERATOR_DSM
    if(osnmaPars->DSSizeBits==1042)
    {
        byteIdxOut=0;
        bitIdxOut=0;
        CopyBitsUnaligned(0,0,&dsmBlocks[13+osnmaPars->keySizeBytes],&byteIdxOut,&bitIdxOut,rxOsnma->DS,osnmaPars->DSSizeBits);

        ArrayClean((char *)rxOsnma->DSToVerify,MAX_DS_SIZE_BYTES);
        byteIdxOut=0;
        bitIdxOut=8-(osnmaPars->DSSizeBits/2)%8;
        CopyBitsUnaligned(0,0,&dsmBlocks[13+osnmaPars->keySizeBytes],&byteIdxOut,&bitIdxOut,rxOsnma->DSToVerify,osnmaPars->DSSizeBits/2);
        byteIdxIn=osnmaPars->DSSizeBits/2/8;
        bitIdxIn=(osnmaPars->DSSizeBits/2)%8;
        byteIdxOut=(int)ceil((double)osnmaPars->DSSizeBits/2/8);
        bitIdxOut=8-(osnmaPars->DSSizeBits/2)%8;
        CopyBitsUnaligned(byteIdxIn,bitIdxIn,&dsmBlocks[13+osnmaPars->keySizeBytes],&byteIdxOut,&bitIdxOut,rxOsnma->DSToVerify,osnmaPars->DSSizeBits/2);
        DSToVerify=rxOsnma->DSToVerify;
    }
    else
    {
        memcpy (rxOsnma->DS, &dsmBlocks[13+osnmaPars->keySizeBytes], osnmaPars->DSSizeBytes);
        DSToVerify=rxOsnma->DS;
    }
    #else
    memcpy (rxOsnma->DS, &dsmBlocks[13+osnmaPars->keySizeBytes], osnmaPars->DSSizeBytes);
    DSToVerify=rxOsnma->DS;
    #endif

	if(P0==0)
	{

		/* EVP_MD context */
		if ((evpx = EVP_MD_CTX_create()) == NULL) {
			handleErrors();
		}

		/* Return the maximum number of bytes required for the signature */
		sigLen = EVP_PKEY_size(rxOsnma->evp_key);


		signatureASN1DEREncoded = (unsigned char *) OPENSSL_malloc(sigLen);
		if (signatureASN1DEREncoded == NULL) {
			handleErrors();
		}
		sigLenReencoded=osnmaPars->DSSizeBytes;
		memcpy(signatureASN1DEREncoded, DSToVerify, osnmaPars->DSSizeBytes);
		sigLenReencoded=br_ecdsa_raw_to_asn1(signatureASN1DEREncoded, sigLenReencoded);

		if(1 != EVP_DigestVerifyInit(evpx,NULL ,rxOsnma->function,NULL,rxOsnma->evp_key))
				handleErrors();

		if(1 != EVP_DigestVerifyUpdate(evpx, rxOsnma->M, osnmaPars->MSize))
			handleErrors();

		if(1 == EVP_DigestVerifyFinal(evpx, signatureASN1DEREncoded, sigLenReencoded))
		{
			/* Success */
			//printf("Signature successfully verified\n");
			EVP_MD_CTX_destroy(evpx);
			OPENSSL_free(signatureASN1DEREncoded);
			return SUCCESS_RETURN;
		}
		else
		{
			/* Failure */
			//printf("Signature not verified\n");
			EVP_MD_CTX_destroy(evpx);
			OPENSSL_free(signatureASN1DEREncoded);
			return FAILURE_RETURN;
		}

	}
	else
	{

	}

	return SUCCESS_RETURN;
}

int DSM_KROOT_P1Verification(unsigned char *dsmBlocks,TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars)
{
    #ifdef PATCH_JRC_GENERATOR_DSM
    int byteIdxOut=0;
	int bitIdxOut=0;
    int byteIdxIn=0;
	int bitIdxIn=0;
    #endif

	if(rxOsnma->evp_keyValid==FALSE_FLAG)
	{
		return INVALID_RETURN;
	}

	if((osnmaPars->M_DSSizeBytes<0) || (osnmaPars->LBytes<0) || (osnmaPars->MSize<0))
	{
		/* terminate the function in presence of funny parameters */
		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Wrong parameters found in DSM_KROOT_P1Verification, please check configuration");
		return FAILURE_RETURN;
	}

	ArrayClean((char *)rxOsnma->M_DS,osnmaPars->M_DSSizeBytes);
	ArrayClean((char *)rxOsnma->M_DSHashed,SHA256_DIGEST_LENGTH);
	ArrayClean((char *)rxOsnma->P1Computed,osnmaPars->LBytes);
	ArrayClean((char *)rxOsnma->P1,osnmaPars->LBytes);

	memcpy (rxOsnma->M_DS, rxOsnma->M, osnmaPars->MSize);
    #ifdef PATCH_JRC_GENERATOR_DSM
    if(osnmaPars->DSSizeBits==1042)
    {
        byteIdxOut=0;
        bitIdxOut=0;
        CopyBitsUnaligned(0,0,rxOsnma->DS,&byteIdxOut,&bitIdxOut,&rxOsnma->M_DS[osnmaPars->MSize],osnmaPars->DSSizeBits);
	}
    else
    {
        memcpy (&rxOsnma->M_DS[osnmaPars->MSize], rxOsnma->DS, osnmaPars->DSSizeBytes);
    }
    #else
    memcpy (&rxOsnma->M_DS[osnmaPars->MSize], rxOsnma->DS, osnmaPars->DSSizeBytes);
    #endif
    
    EVP_HASH(rxOsnma->M_DS,osnmaPars->M_DSSizeBytes, rxOsnma->M_DSHashed,EVP_sha256());

    #ifdef PATCH_JRC_GENERATOR_DSM
    if(osnmaPars->DSSizeBits==1042)
    {
        byteIdxOut=0;
        bitIdxOut=0;
        CopyBitsUnaligned(0,0,rxOsnma->M_DSHashed,&byteIdxOut,&bitIdxOut,rxOsnma->P1Computed,osnmaPars->LBits);
        byteIdxOut=0;
        bitIdxOut=0;
        byteIdxIn=osnmaPars->DSSizeBits/8;
        bitIdxIn=byteIdxIn%8;
        CopyBitsUnaligned(byteIdxIn,bitIdxIn,&dsmBlocks[13+osnmaPars->keySizeBytes],&byteIdxOut,&bitIdxOut,rxOsnma->P1,osnmaPars->LBits);
	}
    else
    {
        memcpy (rxOsnma->P1Computed, rxOsnma->M_DSHashed, osnmaPars->LBytes);
        memcpy (rxOsnma->P1, &dsmBlocks[13+osnmaPars->keySizeBytes+osnmaPars->DSSizeBytes], osnmaPars->LBytes);
    }
    #else
    memcpy (rxOsnma->P1Computed, rxOsnma->M_DSHashed, osnmaPars->LBytes);
    memcpy (rxOsnma->P1, &dsmBlocks[13+osnmaPars->keySizeBytes+osnmaPars->DSSizeBytes], osnmaPars->LBytes);
    #endif
    
    if(CRYPTO_memcmp(rxOsnma->P1, rxOsnma->P1Computed, osnmaPars->LBytes))
	{
		/*Verify failure*/
		return FAILURE_RETURN;
	}
	else
	{
		/*Verify success*/
		return SUCCESS_RETURN;
	}

}



size_t br_ecdsa_asn1_to_raw(void *sig, size_t sig_len)
  {
	  /* Note: this code is a bit lenient in that it accepts a few
		* deviations to DER with regards to minimality of encoding of
		* lengths and integer values. These deviations are still
		* unambiguous.
		*
		* Signature format is a SEQUENCE of two INTEGER values. We
		* support only integers of less than 127 bytes each (signed
		* encoding) so the resulting raw signature will have length
		* at most 254 bytes.
		*/

	   unsigned char *buf, *r, *s;
	   size_t zlen, rlen, slen, off;
	   unsigned char tmp[254];

	   buf = sig;
	   if (sig_len < 8) {
			   return 0;
	   }

	   /*
		* First byte is SEQUENCE tag.
		*/
	   if (buf[0] != 0x30) {
			   return 0;
	   }

	   /*
		* The SEQUENCE length will be encoded over one or two bytes. We
		* limit the total SEQUENCE contents to 255 bytes, because it
		* makes things simpler; this is enough for subgroup orders up
		* to 999 bits.
		*/
	   zlen = buf[1];
	   if (zlen > 0x80) {
			   if (zlen != 0x81) {
					   return 0;
			   }
			   zlen = buf[2];
			   if (zlen != sig_len - 3) {
					   return 0;
			   }
			   off = 3;
	   } else {
			   if (zlen != sig_len - 2) {
					   return 0;
			   }
			   off = 2;
	   }

	   /*
		* First INTEGER (r).
		*/
	   if (buf[off ++] != 0x02) {
			   return 0;
	   }
	   rlen = buf[off ++];
	   if (rlen >= 0x80) {
			   return 0;
	   }
	   r = buf + off;
	   off += rlen;

	   /*
		* Second INTEGER (s).
		*/
	   if (off + 2 > sig_len) {
			   return 0;
	  }
	  if (buf[off ++] != 0x02) {
			  return 0;
	  }
	  slen = buf[off ++];
	  if (slen >= 0x80 || slen != sig_len - off) {
			  return 0;
	  }
	  s = buf + off;

	  /*
	   * Removing leading zeros from r and s.
	   */
	  while (rlen > 0 && *r == 0) {
			  rlen --;
			  r ++;
	  }
	  while (slen > 0 && *s == 0) {
			  slen --;
			  s ++;
	  }

	  /*
	   * Compute common length for the two integers, then copy integers
	   * into the temporary buffer, and finally copy it back over the
	   * signature buffer.
	   */
	  zlen = rlen > slen ? rlen : slen;
	  sig_len = zlen << 1;
	  memset(tmp, 0, sig_len);
	  memcpy(tmp + zlen - rlen, r, rlen);
	  memcpy(tmp + sig_len - slen, s, slen);
	  memcpy(sig, tmp, sig_len);
	 return sig_len;
  }




size_t br_ecdsa_raw_to_asn1(void *sig, size_t sig_len)
{
	/*
	 * Internal buffer is large enough to accommodate a signature
	 * such that r and s fit on 125 bytes each (signed encoding),
	 * meaning a curve order of up to 999 bits. This is the limit
	 * that ensures "simple" length encodings.
	 */
	unsigned char *buf;
	size_t hlen, rlen, slen, zlen, off;
	unsigned char tmp[257];

	buf = sig;
	if ((sig_len & 1) != 0) {
		return 0;
	}

	/*
	 * Compute lengths for the two integers.
	 */
	hlen = sig_len >> 1;
	rlen = _opt_libs_BearSSL_src_ec_ecdsa_rta_c_asn1_int_length(buf, hlen);
	slen = _opt_libs_BearSSL_src_ec_ecdsa_rta_c_asn1_int_length(buf + hlen, hlen);
	if (rlen > 125 || slen > 125) {
		return 0;
	}

	/*
	 * SEQUENCE header.
	 */
	tmp[0] = 0x30;
	zlen = rlen + slen + 4;
	if (zlen >= 0x80) {
		tmp[1] = 0x81;
		tmp[2] = zlen;
		off = 3;
	} else {
		tmp[1] = zlen;
		off = 2;
	}

	/*
	 * First INTEGER (r).
	 */
	tmp[off ++] = 0x02;
	tmp[off ++] = rlen;
	if (rlen > hlen) {
		tmp[off] = 0x00;
		memcpy(tmp + off + 1, buf, hlen);
	} else {
		memcpy(tmp + off, buf + hlen - rlen, rlen);
	}
	off += rlen;

	/*
	 * Second INTEGER (s).
	 */
	tmp[off ++] = 0x02;
	tmp[off ++] = slen;
	if (slen > hlen) {
		tmp[off] = 0x00;
		memcpy(tmp + off + 1, buf + hlen, hlen);
	} else {
		memcpy(tmp + off, buf + sig_len - slen, slen);
	}
	off += slen;

	/*
	 * Return ASN.1 signature.
	 */
	memcpy(sig, tmp, off);
	return off;
}

size_t _opt_libs_BearSSL_src_ec_ecdsa_rta_c_asn1_int_length(const unsigned char *x, size_t xlen)
{
	while (xlen > 0 && *x == 0) {
		x ++;
		xlen --;
	}
	if (xlen == 0 || *x >= 0x80) {
		xlen ++;
	}
	return xlen;
}



void oneStepOfTESLAChainVerification(TOsnmaData *rxOsnma,int indexKey,unsigned int P3,int blockIdx,int i,int enableLog)
{

	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	int GST_TOW_previousKey,GST_TOW_currentKey;
	int GST_WN_previousKey=pars->krootWn;
	int GST_WN_currentKey=pars->krootWn;
	unsigned int GST_SF_previousKey_Int;//,GST_SF_currentKey_Int;
	unsigned char GST_SF[4];
	size_t GST_SFNumOfElem=4;
	size_t alphaNumOfElem=6;
	int byteIdx;


		/**/
		if(i==1)
		{
			GST_TOW_previousKey=pars->krootTow-SUBFRAME_DURATION;
			GST_TOW_currentKey=pars->krootTow;
		}
		else
		{
			GST_TOW_previousKey=pars->krootTow+30*floor((i-2)/(pars->ns*pars->nmack));
			GST_TOW_currentKey=pars->krootTow+30*floor((i-1)/(pars->ns*pars->nmack));
			if(GST_TOW_previousKey>=SECONDS_PER_WEEK)
			{
				GST_TOW_previousKey-= SECONDS_PER_WEEK;
				GST_WN_previousKey++;
			}
			if(GST_TOW_currentKey>=SECONDS_PER_WEEK)
			{
				GST_TOW_currentKey-= SECONDS_PER_WEEK;
				GST_WN_currentKey++;
			}
		}
		GST_SF_previousKey_Int=(GST_WN_previousKey<<20 | GST_TOW_previousKey);
		GST_SF[0]=(GST_SF_previousKey_Int>>24)& 0xFF;
		GST_SF[1]=(GST_SF_previousKey_Int>>16)& 0xFF;
		GST_SF[2]=(GST_SF_previousKey_Int>>8)& 0xFF;
		GST_SF[3]=(GST_SF_previousKey_Int)& 0xFF;


		if(indexKey>rxOsnma->indexLastKeyVerified)
		{
			/*Save the TESLA key of PRN 1*/
			if(FromTeslaChainKeyIndexToPrn(pars->maxNs,pars->ns,pars->keyIndexTable,i,1)==1 && (i>rxOsnma->indexLastPrn1Key[blockIdx]) && (i>rxOsnma->indexLastPrn1KeyTmp[blockIdx]))
			{
				memcpy(rxOsnma->lastPrn1KeyTmp[blockIdx],rxOsnma->currentKey,pars->keySize/BITS_PER_BYTE);
				rxOsnma->indexLastPrn1KeyTmp[blockIdx]=i;
				rxOsnma->wnLastPrn1KeyTmp[blockIdx]=GST_WN_currentKey;
				rxOsnma->towLastPrn1KeyTmp[blockIdx]=GST_TOW_currentKey + blockIdx*pars->mackBlockDuration;
                //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Temporary TESLA key %d of PRN1 saved in blockIdx = %d",i, blockIdx);

            }
			#ifdef MACK_OFFSET_ICD2_DEFINITION
			if(IsThisKeyForMackOffset(pars->mackOffset,pars->ns,i)==TRUE_FLAG)
			#else
            if((pars->mackOffset==1) && (FromTeslaChainKeyIndexToPrn(pars->maxNs,pars->ns,pars->keyIndexTable,i,1)==19))
			#endif
			{
                //Find the correct blockIdx to check against
                if (i>rxOsnma->indexLastPrn1KeyTmp[blockIdx])
                {
                    rxOsnma->indexLastPrn19KeyToCheck=blockIdx;
                }
                else
                {
                    if(blockIdx==0)
                    {
                        rxOsnma->indexLastPrn19KeyToCheck=pars->nmack-1;
                    }
                    else
                    {
                        rxOsnma->indexLastPrn19KeyToCheck=blockIdx-1;
                    }
                }
                if((i>rxOsnma->indexLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck]) && (i>rxOsnma->indexLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck]))
                {
                    memcpy(rxOsnma->lastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck],rxOsnma->currentKey,pars->keySize/BITS_PER_BYTE);
                    rxOsnma->indexLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck]=i;
                    rxOsnma->wnLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck]=GST_WN_currentKey;
                    rxOsnma->towLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck]=GST_TOW_currentKey + rxOsnma->indexLastPrn19KeyToCheck*pars->mackBlockDuration;
                    //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Temporary TESLA key %d of PRN19 saved in blockIdx = %d",i, rxOsnma->indexLastPrn19KeyToCheck);
                }				
            }
            
		}

		if(P3==0)
		{
			memset(rxOsnma->concatenatedInput, 0, (MAX_KEY_SIZE_BYTES + GST_SFNumOfElem + alphaNumOfElem));
			memset(rxOsnma->hashedInput, 0, SHA256_DIGEST_LENGTH);
			/*Concatenate fields*/
			memcpy (rxOsnma->concatenatedInput, rxOsnma->currentKey, pars->keySizeBytes);
			memcpy (&rxOsnma->concatenatedInput[pars->keySizeBytes], GST_SF, GST_SFNumOfElem);
			memcpy (&rxOsnma->concatenatedInput[pars->keySizeBytes+GST_SFNumOfElem], pars->alpha, alphaNumOfElem);
			#ifdef DEBUG
				if(CRYPTO_memcmp(concatenatedInputRef, concatenatedInput, (KnextNumOfElem + GST_SFNumOfElem + alphaNumOfElem)))
				{
					/* Verify failure */
					printf("Concatenation wrong\n");
				}
				else
				{
					/* Verify success */;
					printf("Concatenation OK\n");
				}
			#endif
			EVP_HASH(rxOsnma->concatenatedInput,(pars->keySizeBytes + GST_SFNumOfElem + alphaNumOfElem), rxOsnma->hashedInput,pars->function);
			#ifdef DEBUG
				int i;
				for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
					printf("%02X", hashedInput[i]);
				}
				printf("\n");
				if(CRYPTO_memcmp(hashedInputRef, hashedInput, hashedInputSize))
				{
					/* Verify failure */
					printf("EVP_HASH with SHA-256 not verified\n");
				}
				else
				{
					/* Verify success */;
					printf("EVP_HASH with SHA-256 successfully verified\n");
				}
			#endif
			memcpy (rxOsnma->currentKey, rxOsnma->hashedInput, pars->keySizeBytes);
			/* log */
			if((enableLog==TRUE_FLAG) && (rxOsnma->teslaChainFile!=NULL))
			{
				fprintf(rxOsnma->teslaChainFile,"%7d %7d %3d    ",i-1,FromTeslaChainKeyIndexToTime(pars->nmack, pars->ns, pars->krootTow,i-1),FromTeslaChainKeyIndexToPrn(pars->maxNs,pars->ns,pars->keyIndexTable,i-1,1));
				for(byteIdx=0;byteIdx<pars->keySizeBytes;byteIdx++)
				{
					fprintf(rxOsnma->teslaChainFile,"%3d ",rxOsnma->currentKey[byteIdx]);
				}
				fprintf(rxOsnma->teslaChainFile,"\n");
			}
		}
		else
		{
			/* TBD */
		}

}




int TESLAKeyVerification(TOsnmaData *rxOsnma,unsigned char * key,int prnA,int indexKeyToStart,int indexKeyToEnd, unsigned int P3,int blockIdx)
{
	int i;
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	int indexStart, indexEnd;
	/* text log */
	int enableLog = FALSE_FLAG; // log is enabled only when the current key is following the last verified key
	int logPosition = 0; // current position in the log file
	
	if(rxOsnma->teslaChainFile!=NULL)
	{
		// save the position only when the log is enabled
		logPosition = ftell(rxOsnma->teslaChainFile);
	}

	if(indexKeyToStart>indexKeyToEnd)
	{
		indexStart=indexKeyToStart;
		indexEnd=indexKeyToEnd;
		memcpy(rxOsnma->currentKey,key,pars->keySize/BITS_PER_BYTE);
		memcpy(rxOsnma->referenceKey,rxOsnma->lastKeyVerified,pars->keySize/BITS_PER_BYTE);
		enableLog = TRUE_FLAG; // log enabled
	}
	else
	{
		if(indexKeyToStart<indexKeyToEnd)
		{
			indexStart=indexKeyToEnd;
			indexEnd=indexKeyToStart;
			memcpy(rxOsnma->currentKey,rxOsnma->lastKeyVerified,pars->keySize/BITS_PER_BYTE);
			memcpy(rxOsnma->referenceKey,key,pars->keySize/BITS_PER_BYTE);
		}
		else
		{
			indexStart=indexKeyToStart;
			indexEnd=indexKeyToStart;
			memcpy(rxOsnma->currentKey,key,pars->keySize/BITS_PER_BYTE);
			memcpy(rxOsnma->referenceKey,rxOsnma->lastKeyVerified,pars->keySize/BITS_PER_BYTE);
		}
	}


    rxOsnma->indexLastPrn1KeyTmp[blockIdx]=-1;
    rxOsnma->indexLastPrn19KeyTmp[blockIdx]=-1;
    rxOsnma->indexLastPrn19KeyToCheck=-1;
	unsigned int numberOfExecutions = 0;
	#ifdef OSNMA_PROFILING
	StartProfilingUpdate(&(rxOsnma->oneStepOfTeslaChainProfiling));
	#endif
	for(i=indexStart;i>indexEnd;i--)
	{
		oneStepOfTESLAChainVerification(rxOsnma,indexKeyToStart,P3,blockIdx,i,enableLog);
		numberOfExecutions++;
	}
	#ifdef OSNMA_PROFILING
	StopProfilingUpdate(&(rxOsnma->oneStepOfTeslaChainProfiling),numberOfExecutions);
	#endif

	if(CRYPTO_memcmp(rxOsnma->referenceKey, rxOsnma->currentKey, pars->keySizeBytes))
	{
		if(rxOsnma->teslaChainFile!=NULL)
		{
			/* Discard the new lines in the log file (if enabled)  */
			fflush(rxOsnma->teslaChainFile);
			ftruncate(fileno(rxOsnma->teslaChainFile),logPosition);
			fseek(rxOsnma->teslaChainFile,logPosition,SEEK_SET);
		}
		/* Verify failure */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d (PRN %2d, TOW %d) NOT verified",indexKeyToStart,prnA,FromTeslaChainKeyIndexToTime(pars->nmack,pars->ns,pars->krootTow,indexKeyToStart));
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key %d NOT verified against key %d. Number of TESLA chain steps performed= %d",indexKeyToStart, indexKeyToEnd, (indexKeyToStart-indexKeyToEnd));
		return FAILURE_RETURN;
	}
	else
	{
		/* Verify success */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d (PRN %2d, TOW %d) successfully verified",indexKeyToStart,prnA,FromTeslaChainKeyIndexToTime(pars->nmack,pars->ns,pars->krootTow,indexKeyToStart));
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key %d successfully verified against key %d. Number of TESLA chain steps performed= %d",indexKeyToStart, indexKeyToEnd, (indexKeyToStart-indexKeyToEnd));
		/*Save PRN1 and PRN19 keys if any*/
        if(rxOsnma->indexLastPrn1KeyTmp[blockIdx]!=-1)
        {
            memcpy(rxOsnma->lastPrn1Key[blockIdx],rxOsnma->lastPrn1KeyTmp[blockIdx],pars->keySize/BITS_PER_BYTE);
            rxOsnma->indexLastPrn1Key[blockIdx]=rxOsnma->indexLastPrn1KeyTmp[blockIdx];
            rxOsnma->wnLastPrn1Key[blockIdx]=rxOsnma->wnLastPrn1KeyTmp[blockIdx];
            rxOsnma->towLastPrn1Key[blockIdx]=rxOsnma->towLastPrn1KeyTmp[blockIdx];
            SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN1 saved",rxOsnma->indexLastPrn1Key[blockIdx]);
            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN1 saved in blockIdx = %d",rxOsnma->indexLastPrn1Key[blockIdx],blockIdx);
            
        }
        if(rxOsnma->indexLastPrn19KeyToCheck!=-1)
        {
            memcpy(rxOsnma->lastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck],rxOsnma->lastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck],pars->keySize/BITS_PER_BYTE);
            rxOsnma->indexLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck]=rxOsnma->indexLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck];
            rxOsnma->wnLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck]=rxOsnma->wnLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck];
            rxOsnma->towLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck]=rxOsnma->towLastPrn19KeyTmp[rxOsnma->indexLastPrn19KeyToCheck];
            SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN19 saved",rxOsnma->indexLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck]);
            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN19 saved in blockIdx = %d",rxOsnma->indexLastPrn19Key[rxOsnma->indexLastPrn19KeyToCheck],rxOsnma->indexLastPrn19KeyToCheck);
            
        }
        
        return SUCCESS_RETURN;
	}
}


//*****************************************************

void InitTESLAKeyVerificationThreadFunction(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam,TOsnmaData *rxOsnma, unsigned char * key, int prnA, int wnMackStart, int towMackStart, int indexKey,int blockIdx)
{
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	size_t alphaNumOfElem=6;
	int index;


	/*Temporary variables*/
	teslaKeyVerificationThFuncParam->currentKey = rxOsnma->currentKeyThread;
	teslaKeyVerificationThFuncParam->referenceKey = rxOsnma->referenceKeyThread;

	memcpy(rxOsnma->keyThread,key,pars->keySize/BITS_PER_BYTE);
	teslaKeyVerificationThFuncParam->key = rxOsnma->keyThread;
	memcpy(rxOsnma->lastKeyVerifiedThread,rxOsnma->lastKeyVerified,pars->keySize/BITS_PER_BYTE);
	teslaKeyVerificationThFuncParam->lastKeyVerified =rxOsnma->lastKeyVerifiedThread;
	rxOsnma->indexLastKeyVerifiedThread=rxOsnma->indexLastKeyVerified;
	teslaKeyVerificationThFuncParam->indexLastKeyVerified=&rxOsnma->indexLastKeyVerifiedThread;
	teslaKeyVerificationThFuncParam->krootWn=pars->krootWn;
	teslaKeyVerificationThFuncParam->krootTow=pars->krootTow;
	teslaKeyVerificationThFuncParam->ns=pars->ns;
	teslaKeyVerificationThFuncParam->nmack=pars->nmack;
	teslaKeyVerificationThFuncParam->mackBlockDuration=pars->mackBlockDuration;
	teslaKeyVerificationThFuncParam->mackOffset=pars->mackOffset;
	memcpy(teslaKeyVerificationThFuncParam->alpha,pars->alpha,alphaNumOfElem);
	teslaKeyVerificationThFuncParam->concatenatedInput=rxOsnma->concatenatedInputThread;
	teslaKeyVerificationThFuncParam->hashedInput=rxOsnma->hashedInputThread;
	teslaKeyVerificationThFuncParam->function=pars->functionThread;

	teslaKeyVerificationThFuncParam->keySize=pars->keySize;
	teslaKeyVerificationThFuncParam->keySizeBytes=pars->keySizeBytes;
	teslaKeyVerificationThFuncParam->indexKeyToEnd=rxOsnma->indexLastKeyVerified;
	teslaKeyVerificationThFuncParam->prnA = prnA;
	teslaKeyVerificationThFuncParam->wnMackStart = wnMackStart;
	teslaKeyVerificationThFuncParam->towMackStart = towMackStart;
	teslaKeyVerificationThFuncParam->indexKeyToStart=indexKey;
	teslaKeyVerificationThFuncParam->blockIdx=blockIdx;
	teslaKeyVerificationThFuncParam->P3=0;
	teslaKeyVerificationThFuncParam->maxNs=pars->maxNs;
	teslaKeyVerificationThFuncParam->TESLAKeyVerificationMutex=&rxOsnma->TESLAKeyVerificationMutex;


	//**************
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		rxOsnma->indexLastPrn1KeyThread[index]=rxOsnma->indexLastPrn1Key[index];
		rxOsnma->wnLastPrn1KeyThread[index]=rxOsnma->wnLastPrn1Key[index];/*GST WN of the last PRN1 key saved*/
		rxOsnma->towLastPrn1KeyThread[index]=rxOsnma->towLastPrn1Key[index];/*GST TOW of the last PRN1 key saved*/
		rxOsnma->indexLastPrn19KeyThread[index]=rxOsnma->indexLastPrn19Key[index];
		rxOsnma->wnLastPrn19KeyThread[index]=rxOsnma->wnLastPrn19Key[index];/*GST WN of the last PRN1 key saved*/
		rxOsnma->towLastPrn19KeyThread[index]=rxOsnma->towLastPrn19Key[index];/*GST TOW of the last PRN1 key saved*/
		memcpy(rxOsnma->lastPrn1KeyThread[index],rxOsnma->lastPrn1Key[index],pars->keySize/BITS_PER_BYTE);
		memcpy(rxOsnma->lastPrn19KeyThread[index],rxOsnma->lastPrn19Key[index],pars->keySize/BITS_PER_BYTE);
        //**
        teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[index]=-1;
        teslaKeyVerificationThFuncParam->wnLastPrn1KeyTmp[index]=-1;
        teslaKeyVerificationThFuncParam->towLastPrn1KeyTmp[index]=-1;
        teslaKeyVerificationThFuncParam->indexLastPrn19KeyTmp[index]=-1;
        teslaKeyVerificationThFuncParam->wnLastPrn19KeyTmp[index]=-1;
        teslaKeyVerificationThFuncParam->towLastPrn19KeyTmp[index]=-1;
        //**
	}
	teslaKeyVerificationThFuncParam->indexLastPrn1Key=rxOsnma->indexLastPrn1KeyThread;
	teslaKeyVerificationThFuncParam->wnLastPrn1Key=rxOsnma->wnLastPrn1KeyThread;
	teslaKeyVerificationThFuncParam->towLastPrn1Key=rxOsnma->towLastPrn1KeyThread;
	teslaKeyVerificationThFuncParam->indexLastPrn19Key=rxOsnma->indexLastPrn19KeyThread;
	teslaKeyVerificationThFuncParam->wnLastPrn19Key=rxOsnma->wnLastPrn19KeyThread;
	teslaKeyVerificationThFuncParam->towLastPrn19Key=rxOsnma->towLastPrn19KeyThread;
	teslaKeyVerificationThFuncParam->lastPrn1Key=rxOsnma->lastPrn1KeyThread;
	teslaKeyVerificationThFuncParam->lastPrn19Key=rxOsnma->lastPrn19KeyThread;
    //**
    teslaKeyVerificationThFuncParam->lastPrn1KeyTmp=rxOsnma->lastPrn1KeyThreadTmp;
	teslaKeyVerificationThFuncParam->lastPrn19KeyTmp=rxOsnma->lastPrn19KeyThreadTmp;

    teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck=-1;
    //**

	teslaKeyVerificationThFuncParam->keyIndexTable=pars->keyIndexTable;

	teslaKeyVerificationThFuncParam->teslaChainFile=rxOsnma->teslaChainFile;
}


void oneStepOfTESLAChainVerificationThread(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam,int enableLog)
{

	unsigned char *currentKey=teslaKeyVerificationThFuncParam->currentKey;
	unsigned char *concatenatedInput=teslaKeyVerificationThFuncParam->concatenatedInput;
	unsigned char *hashedInput=teslaKeyVerificationThFuncParam->hashedInput;
	int GST_TOW_previousKey,GST_TOW_currentKey;
	int GST_WN_previousKey=teslaKeyVerificationThFuncParam->krootWn;
	int GST_WN_currentKey=teslaKeyVerificationThFuncParam->krootWn;
	int GST_TOW_rootKey=teslaKeyVerificationThFuncParam->krootTow;
	unsigned int GST_SF_previousKey_Int;//,GST_SF_currentKey_Int;
	unsigned char GST_SF[4];
	size_t GST_SFNumOfElem=4;
	int maxNs=teslaKeyVerificationThFuncParam->maxNs;
	int ns=teslaKeyVerificationThFuncParam->ns;
	int nmack=teslaKeyVerificationThFuncParam->nmack;
	int krootTow=teslaKeyVerificationThFuncParam->krootTow;
	int *keyIndexTable=teslaKeyVerificationThFuncParam->keyIndexTable;
	int blockIdx=teslaKeyVerificationThFuncParam->blockIdx;
	int keySize=teslaKeyVerificationThFuncParam->keySize;
	int keySizeBytes=teslaKeyVerificationThFuncParam->keySizeBytes;
	int mackOffset=teslaKeyVerificationThFuncParam->mackOffset;
	size_t alphaNumOfElem=6;
	int byteIdx;
	int index=teslaKeyVerificationThFuncParam->index;
	int mackBlockDuration=teslaKeyVerificationThFuncParam->mackBlockDuration;


		/**/
		if(index==1)
		{
			GST_TOW_previousKey=GST_TOW_rootKey-SUBFRAME_DURATION;
			GST_TOW_currentKey=GST_TOW_rootKey;
		}
		else
		{
			GST_TOW_previousKey=GST_TOW_rootKey+30*floor((index-2)/(ns*nmack));
			GST_TOW_currentKey=GST_TOW_rootKey+30*floor((index-1)/(ns*nmack));
			if(GST_TOW_previousKey>=SECONDS_PER_WEEK)
			{
				GST_TOW_previousKey-= SECONDS_PER_WEEK;
				GST_WN_previousKey++;
			}
			if(GST_TOW_currentKey>=SECONDS_PER_WEEK)
			{
				GST_TOW_currentKey-= SECONDS_PER_WEEK;
				GST_WN_currentKey++;
			}
		}
		GST_SF_previousKey_Int=(GST_WN_previousKey<<20 | GST_TOW_previousKey);
		GST_SF[0]=(GST_SF_previousKey_Int>>24)& 0xFF;
		GST_SF[1]=(GST_SF_previousKey_Int>>16)& 0xFF;
		GST_SF[2]=(GST_SF_previousKey_Int>>8)& 0xFF;
		GST_SF[3]=(GST_SF_previousKey_Int)& 0xFF;

        
		if(teslaKeyVerificationThFuncParam->indexKeyToStart>(*teslaKeyVerificationThFuncParam->indexLastKeyVerified))
		{
			/*Save the TESLA key of PRN 1*/
            if(FromTeslaChainKeyIndexToPrn(maxNs,ns,keyIndexTable,index,1)==1 && (index>teslaKeyVerificationThFuncParam->indexLastPrn1Key[blockIdx]) && (index>teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx]))
			{
				memcpy(teslaKeyVerificationThFuncParam->lastPrn1KeyTmp[blockIdx],currentKey,keySize/BITS_PER_BYTE);
				teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx]=index;
				teslaKeyVerificationThFuncParam->wnLastPrn1KeyTmp[blockIdx]=GST_WN_currentKey;
				teslaKeyVerificationThFuncParam->towLastPrn1KeyTmp[blockIdx]=GST_TOW_currentKey + blockIdx*mackBlockDuration;
				//SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Thread Temporary TESLA key %d of PRN1 saved in blockIdx = %d",index, blockIdx);
			}
			/*If MACK Offset --> Save the TESLA key of PRN 19*/
			if((mackOffset==1) && (FromTeslaChainKeyIndexToPrn(maxNs,ns,keyIndexTable,index,1)==19))
			{
                //Find the correct blockIdx to check against
                if (index>teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx])
                {
                    teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck=blockIdx;
                }
                else
                {
                    if(blockIdx==0)
                    {
                        teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck=nmack-1;
                    }
                    else
                    {
                        teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck=blockIdx-1;
                    }
                }
                if((index>teslaKeyVerificationThFuncParam->indexLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]) && (index>teslaKeyVerificationThFuncParam->indexLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]))
                {
                    memcpy(teslaKeyVerificationThFuncParam->lastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck],currentKey,keySize/BITS_PER_BYTE);
                    teslaKeyVerificationThFuncParam->indexLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=index;
                    teslaKeyVerificationThFuncParam->wnLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=GST_WN_currentKey;
                    teslaKeyVerificationThFuncParam->towLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=GST_TOW_currentKey + teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck*mackBlockDuration;;
                    //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Thread Temporary TESLA key %d of PRN19 saved in blockIdx = %d",index,teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck);
                }
			
            }
		}

		if(teslaKeyVerificationThFuncParam->P3==0)
		{
			memset(concatenatedInput, 0, (MAX_KEY_SIZE_BYTES + GST_SFNumOfElem + alphaNumOfElem));
			memset(hashedInput, 0, SHA256_DIGEST_LENGTH);
			/*Concatenate fields*/
			memcpy (concatenatedInput, currentKey, keySizeBytes);
			memcpy (&concatenatedInput[keySizeBytes], GST_SF, GST_SFNumOfElem);
			memcpy (&concatenatedInput[keySizeBytes+GST_SFNumOfElem], teslaKeyVerificationThFuncParam->alpha, alphaNumOfElem);
			#ifdef DEBUG
				if(CRYPTO_memcmp(concatenatedInputRef, concatenatedInput, (KnextNumOfElem + GST_SFNumOfElem + alphaNumOfElem)))
				{
					/* Verify failure */
					printf("Concatenation wrong\n");
				}
				else
				{
					/* Verify success */;
					printf("Concatenation OK\n");
				}
			#endif
			EVP_HASH(concatenatedInput,(keySizeBytes + GST_SFNumOfElem + alphaNumOfElem), hashedInput,teslaKeyVerificationThFuncParam->function);
			#ifdef DEBUG
				int i;
				for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
					printf("%02X", hashedInput[i]);
				}
				printf("\n");
				if(CRYPTO_memcmp(hashedInputRef, hashedInput, hashedInputSize))
				{
					/* Verify failure */
					printf("EVP_HASH with SHA-256 not verified\n");
				}
				else
				{
					/* Verify success */;
					printf("EVP_HASH with SHA-256 successfully verified\n");
				}
			#endif
			memcpy (currentKey, hashedInput, keySizeBytes);
			/* log */
			if((enableLog==TRUE_FLAG) && (teslaKeyVerificationThFuncParam->teslaChainFile!=NULL))
			{
				fprintf(teslaKeyVerificationThFuncParam->teslaChainFile,"%7d %7d %3d    ",index-1,FromTeslaChainKeyIndexToTime(nmack, ns, krootTow,index-1),FromTeslaChainKeyIndexToPrn(maxNs,ns,keyIndexTable,index-1,1));
				for(byteIdx=0;byteIdx<keySizeBytes;byteIdx++)
				{
					fprintf(teslaKeyVerificationThFuncParam->teslaChainFile,"%3d ",currentKey[byteIdx]);
				}
				fprintf(teslaKeyVerificationThFuncParam->teslaChainFile,"\n");
			}
		}
		else
		{
			/* TBD */
		}

}




void *TESLAKeyVerificationThreadFunction(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam)
{
	int prnA = teslaKeyVerificationThFuncParam->prnA;
	int indexKeyToStart=teslaKeyVerificationThFuncParam->indexKeyToStart;
	int indexKeyToEnd=teslaKeyVerificationThFuncParam->indexKeyToEnd;
	unsigned char *currentKey=teslaKeyVerificationThFuncParam->currentKey;
	unsigned char *key=teslaKeyVerificationThFuncParam->key;
	unsigned char *referenceKey=teslaKeyVerificationThFuncParam->referenceKey;
	unsigned char *lastKeyVerified=teslaKeyVerificationThFuncParam->lastKeyVerified;
	int keySize=teslaKeyVerificationThFuncParam->keySize;
	int keySizeBytes=teslaKeyVerificationThFuncParam->keySizeBytes;
	//int maxNs=teslaKeyVerificationThFuncParam->maxNs;
	int ns=teslaKeyVerificationThFuncParam->ns;
	int krootTow=teslaKeyVerificationThFuncParam->krootTow;
	int nmack=teslaKeyVerificationThFuncParam->nmack;
	//int *keyIndexTable=teslaKeyVerificationThFuncParam->keyIndexTable;
    int blockIdx=teslaKeyVerificationThFuncParam->blockIdx;


	int indexStart, indexEnd;
	/* text log */
	int enableLog = FALSE_FLAG; // log is enabled only when the current key is following the last verified key
	int logPosition = 0; // current position in the log file
	
	if(teslaKeyVerificationThFuncParam->teslaChainFile!=NULL)
	{
		// save the position only when the log is enabled
		ftell(teslaKeyVerificationThFuncParam->teslaChainFile); 
	}

    pthread_detach(pthread_self());

	if(indexKeyToStart>indexKeyToEnd)
	{
		indexStart=indexKeyToStart;
		indexEnd=indexKeyToEnd;
		memcpy(currentKey,key,keySize/BITS_PER_BYTE);
		memcpy(referenceKey,lastKeyVerified,keySize/BITS_PER_BYTE);
		enableLog = TRUE_FLAG; // log enabled
	}
	else
	{
		if(indexKeyToStart<indexKeyToEnd)
		{
			indexStart=indexKeyToEnd;
			indexEnd=indexKeyToStart;
			memcpy(currentKey,lastKeyVerified,keySize/BITS_PER_BYTE);
			memcpy(referenceKey,key,keySize/BITS_PER_BYTE);
		}
		else
		{
			indexStart=indexKeyToStart;
			indexEnd=indexKeyToStart;
			memcpy(currentKey,key,keySize/BITS_PER_BYTE);
			memcpy(referenceKey,lastKeyVerified,keySize/BITS_PER_BYTE);
		}
	}


    teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx]=-1;
    teslaKeyVerificationThFuncParam->indexLastPrn19KeyTmp[blockIdx]=-1;
    teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck=-1;
	for(teslaKeyVerificationThFuncParam->index=indexStart;teslaKeyVerificationThFuncParam->index>indexEnd;teslaKeyVerificationThFuncParam->index--)
	{
		oneStepOfTESLAChainVerificationThread(teslaKeyVerificationThFuncParam,enableLog);
	}

	if(CRYPTO_memcmp(referenceKey, currentKey, keySizeBytes))
	{
		if(teslaKeyVerificationThFuncParam->teslaChainFile!=NULL)
		{
			/* Discard the new lines in the log file (if enabled) */
			ftruncate(fileno(teslaKeyVerificationThFuncParam->teslaChainFile),logPosition);
		}
		/* Verify failure */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d (PRN %2d, TOW %d) NOT verified by the TESLA verification thread",indexKeyToStart,prnA,FromTeslaChainKeyIndexToTime(nmack,ns,krootTow,indexKeyToStart));
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key %d NOT verified against key %d. Number of TESLA chain steps performed= %d",indexKeyToStart, indexKeyToEnd, (indexKeyToStart-indexKeyToEnd));
		teslaKeyVerificationThFuncParam->keyVerificationStatus=NOT_VALID;
		teslaKeyVerificationThFuncParam->returnValue=FAILURE_RETURN;
	}
	else
	{
		/* Verify success */
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d (PRN %2d, TOW %d) successfully verified by the TESLA verification thread",indexKeyToStart,prnA,FromTeslaChainKeyIndexToTime(nmack,ns,krootTow,indexKeyToStart));
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key %d successfully verified against key %d. Number of TESLA chain steps performed= %d",indexKeyToStart, indexKeyToEnd, (indexKeyToStart-indexKeyToEnd));
		*teslaKeyVerificationThFuncParam->indexLastKeyVerified=indexStart;
		memcpy(lastKeyVerified,key,keySize/BITS_PER_BYTE);
		teslaKeyVerificationThFuncParam->keyVerificationStatus=VALIDATED;
		teslaKeyVerificationThFuncParam->returnValue=SUCCESS_RETURN;
        //**
        /*Save PRN1 and PRN19 keys if any*/
        if(teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx]!=-1)
        {
            memcpy(teslaKeyVerificationThFuncParam->lastPrn1Key[blockIdx],teslaKeyVerificationThFuncParam->lastPrn1KeyTmp[blockIdx],keySize/BITS_PER_BYTE);
            teslaKeyVerificationThFuncParam->indexLastPrn1Key[blockIdx]=teslaKeyVerificationThFuncParam->indexLastPrn1KeyTmp[blockIdx];
            teslaKeyVerificationThFuncParam->wnLastPrn1Key[blockIdx]=teslaKeyVerificationThFuncParam->wnLastPrn1KeyTmp[blockIdx];
            teslaKeyVerificationThFuncParam->towLastPrn1Key[blockIdx]=teslaKeyVerificationThFuncParam->towLastPrn1KeyTmp[blockIdx];
            SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN1 saved",teslaKeyVerificationThFuncParam->indexLastPrn1Key[blockIdx]);
            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Thread TESLA key %d of PRN1 saved in blockIdx = %d",teslaKeyVerificationThFuncParam->indexLastPrn1Key[blockIdx],blockIdx);
            
        }
        if(teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck!=-1)
        {
            memcpy(teslaKeyVerificationThFuncParam->lastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck],teslaKeyVerificationThFuncParam->lastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck],keySize/BITS_PER_BYTE);
            teslaKeyVerificationThFuncParam->indexLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=teslaKeyVerificationThFuncParam->indexLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck];
            teslaKeyVerificationThFuncParam->wnLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=teslaKeyVerificationThFuncParam->wnLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck];
            teslaKeyVerificationThFuncParam->towLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]=teslaKeyVerificationThFuncParam->towLastPrn19KeyTmp[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck];
            SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"TESLA key %d of PRN19 saved",teslaKeyVerificationThFuncParam->indexLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck]);
            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Thread TESLA key %d of PRN19 saved in blockIdx = %d",teslaKeyVerificationThFuncParam->indexLastPrn19Key[teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck],teslaKeyVerificationThFuncParam->indexLastPrn19KeyToCheck);
        }
        //**
	}



	pthread_mutex_lock((teslaKeyVerificationThFuncParam->TESLAKeyVerificationMutex));
	threadTESLAKeyVerificationStopIsRunning=FALSE_FLAG;
	pthread_mutex_unlock((teslaKeyVerificationThFuncParam->TESLAKeyVerificationMutex));
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key Verification Thread terminates");


	return NULL;
}






//*******************************************************





#ifdef DEBUG
int TESLAKeyGenerationAndVerification(unsigned char* Knext,size_t KnextNumOfElem,unsigned char* GST_SF,size_t GST_SFNumOfElem,unsigned char* alpha,size_t alphaNumOfElem,unsigned int P3,const EVP_MD *function,unsigned char* concatenatedInput, unsigned char* concatenatedInputRef,unsigned char* hashedInput, size_t hashedInputSize,unsigned char* hashedInputRef, unsigned char* Km)
#else
	int TESLAKeyGenerationAndVerification(unsigned char* Knext,size_t KnextNumOfElem,unsigned char* GST_SF,size_t GST_SFNumOfElem,unsigned char* alpha,size_t alphaNumOfElem,unsigned int P3,const EVP_MD *function,unsigned char* concatenatedInput, unsigned char* hashedInput, size_t hashedInputSize, unsigned char* Km)
#endif
{
		if(P3==0)
		{
			memset(concatenatedInput, 0, (KnextNumOfElem + GST_SFNumOfElem + alphaNumOfElem));
			memset(hashedInput, 0, hashedInputSize);
			/*Concatenate fields*/
			memcpy (concatenatedInput, Knext, KnextNumOfElem);
			memcpy (&concatenatedInput[KnextNumOfElem], GST_SF, GST_SFNumOfElem);
			memcpy (&concatenatedInput[KnextNumOfElem+GST_SFNumOfElem], alpha, alphaNumOfElem);
			#ifdef DEBUG
				if(CRYPTO_memcmp(concatenatedInputRef, concatenatedInput, (KnextNumOfElem + GST_SFNumOfElem + alphaNumOfElem)))
				{
					/* Verify failure */
					printf("Concatenation wrong\n");
				}
				else
				{
					/* Verify success */;
					printf("Concatenation OK\n");
				}
			#endif
			EVP_HASH(concatenatedInput,(KnextNumOfElem + GST_SFNumOfElem + alphaNumOfElem), hashedInput,function);
			#ifdef DEBUG
				int i;
				for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
					printf("%02X", hashedInput[i]);
				}
				printf("\n");
				if(CRYPTO_memcmp(hashedInputRef, hashedInput, hashedInputSize))
				{
					/* Verify failure */
					printf("EVP_HASH with SHA-256 not verified\n");
				}
				else
				{
					/* Verify success */;
					printf("EVP_HASH with SHA-256 successfully verified\n");
				}
			#endif
			memcpy (Km, hashedInput, KnextNumOfElem);
		}
        else
        {
        	/*TBD*/

        }


		return 0;


}


TValidationStatus MACVerification(TOsnmaData *rxOsnma, TMackList *mackPtr,TMacList *macPtr,unsigned char* K, unsigned char *MAC, int flagMAC0)
{

	int byteIdxOut=0;
	int bitIdxOut=0;
	int MACSizeBytes; /*MAC Size represented as number of bytes*/
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	int mSize;
	unsigned int P4;/*Number of Padding LBits (0 to 7 bits)*/
	int NMA_Status_Header_SizeBits=2;/*Size of NMA Status (bits)*/
	int GST_SFSize=4;
	unsigned char zeroByte={0x00};
	unsigned int GST_SF_Int;
	unsigned char GST_SF[GST_SFSize];/*GST_SF (32 bits): Galileo System Time Subframe*/
	unsigned char prnN=(unsigned char)(mackPtr->prnN);
	unsigned char prnA=(unsigned char)(mackPtr->prnA);
	int offsetIndex;
	char MACString[5],satTypeString[8],prnAString[40],prnString[40];
	int validationResult;

	#ifdef PATCH_PRNN_255
	if(prnN==OSNMA_GENERAL_GALILEO_INFO)
	{
		/* from the new ICD (oct.2020):
		 * In the case PRN_D (former PRN_N, ndMN) is set to the value 255 (e.g. ADKD=4), PRN_D = PRN_A shall be used */
		prnN = prnA;
	}
	#endif

	if(flagMAC0==TRUE_FLAG)
	{
		offsetIndex=-1;
	}
	else
	{
		offsetIndex=0;
	}

	P4=8-(NMA_Status_Header_SizeBits+rxOsnma->navdataBitsForMacLen)%8;
	if(P4==8)
	{
		// if (NMA_Status_Header_SizeBits+rxOsnma->navdataBitsForMacLen)%8==0, padding is not needed!
		P4 = 0;
	}
	mSize=3+offsetIndex+GST_SFSize+(NMA_Status_Header_SizeBits+rxOsnma->navdataBitsForMacLen+P4)/8;/*See pag 27 of ICD v1.1*/

	GST_SF_Int=(macPtr->wnSubframeStart<<20|(macPtr->towSubframeStart));
	GST_SF[0]=(GST_SF_Int>>24)& 0xFF;
	GST_SF[1]=(GST_SF_Int>>16)& 0xFF;
	GST_SF[2]=(GST_SF_Int>>8)& 0xFF;
	GST_SF[3]=(GST_SF_Int)& 0xFF;

	/*Concatenate fields*/
	ArrayClean((char *)rxOsnma->message,rxOsnma->messageSizeMax);
	ArrayClean((char *)rxOsnma->messageHashed,SHA256_DIGEST_LENGTH);
	ArrayClean((char *)rxOsnma->tag,rxOsnma->tagSizeMax);
	if(flagMAC0==FALSE_FLAG)
	{
		memcpy (rxOsnma->message, &prnN, 1);
	}
	memcpy (&rxOsnma->message[1+offsetIndex], &prnA, 1);
	#ifdef DEBUG
	printf("%02X\n", rxOsnma->message[1+offsetIndex]);
	#endif

	memcpy (&rxOsnma->message[2+offsetIndex], GST_SF, GST_SFSize);
	memcpy (&rxOsnma->message[2+GST_SFSize+offsetIndex], &macPtr->ctr, 1);
	byteIdxOut=0;
	bitIdxOut=0;
	CopyBitsUnaligned(0,0,&pars->nmaHeader,&byteIdxOut,&bitIdxOut,&rxOsnma->message[2+GST_SFSize+1+offsetIndex],NMA_Status_Header_SizeBits);
	CopyBitsUnaligned(0,0,rxOsnma->navdataBitsForMac,&byteIdxOut,&bitIdxOut,&rxOsnma->message[2+GST_SFSize+1+offsetIndex],rxOsnma->navdataBitsForMacLen);
	if(P4>0)
	{
		// if P4==0, no padding
		CopyBitsUnaligned(0,0,&zeroByte,&byteIdxOut,&bitIdxOut,&rxOsnma->message[2+GST_SFSize+1+offsetIndex],P4);
	}

	/* MAC Computation */
	switch(pars->macFunction)
	{
	case HMAC_SHA_256:
		EVP_HMAC(rxOsnma->message,mSize,rxOsnma->messageHashed,EVP_sha256(),K,pars->keySizeBytes);
		break;
	case 1:
		CMAC_AES_function(rxOsnma->message,mSize,rxOsnma->messageHashed,K,pars->keySizeBytes);
		break;
	case MF_2_RSVD:
		/*Rsvd*/
		break;
	case MF_3_RSVD:
		/*Rsvd*/
		break;
	default:
		EVP_HMAC(rxOsnma->message,mSize,rxOsnma->messageHashed,EVP_sha256(),K,pars->keySizeBytes);
	}
	//**
	#ifdef DEBUG
	int i;
	for (i = 0; i < pars->keySizeBytes; i++) {
		printf("%02X", K[i]);
	}
	printf("\n");
	#endif
	//**

	MACSizeBytes=ceil((double)pars->macSize/8);
	byteIdxOut=0;
	bitIdxOut=0;
	CopyBitsUnaligned(0,0,rxOsnma->messageHashed,&byteIdxOut,&bitIdxOut,rxOsnma->tag,pars->macSize);

	#ifdef DEBUG
	int i;
	for (i = 0; i < MACSizeBytes; i++) {
		printf("%02X", rxOsnma->tag[i]);
	}
	printf("\n");
	for (i = 0; i < MACSizeBytes; i++) {
		printf("%02X", MAC[i]);
	}
	printf("\n");
	#endif

	/* Check the MAC! */
	validationResult = CRYPTO_memcmp(MAC, rxOsnma->tag, MACSizeBytes);

	/* Optional debug */
	#ifdef OSNMA_MAC_DEBUG
	{
		FILE *macLogFile = fopen("osnmaMacValidation.txt","a");
		fprintf(macLogFile,"%d,%d,%d,%d,%d,",macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->wnMackStart,macPtr->towMackStart,macPtr->adkd);
		if(flagMAC0==FALSE_FLAG)
		{
			fprintf(macLogFile,"%d",prnN);
		}
		fprintf(macLogFile,",");
		fprintf(macLogFile,"%d,",prnA);
		PrintABitString(macLogFile,GST_SF,GST_SFSize*BITS_PER_BYTE);
		fprintf(macLogFile,",");
		fprintf(macLogFile,"%u,",macPtr->ctr);
		fprintf(macLogFile,"%u,",(pars->nmaHeader>>6) & 0x03);
		PrintABitString(macLogFile,rxOsnma->navdataBitsForMac,rxOsnma->navdataBitsForMacLen);
		fprintf(macLogFile,",");
		if(P4>0)
		{
			// if P4==0, no padding
			PrintABitString(macLogFile,&zeroByte,P4);
		}
		fprintf(macLogFile,",");
		PrintABitString(macLogFile,K,pars->keySizeBytes*BITS_PER_BYTE);
		fprintf(macLogFile,",");
		PrintABitString(macLogFile,rxOsnma->tag,pars->macSize);
		fprintf(macLogFile,",");
		PrintABitString(macLogFile,MAC,pars->macSize);
		fprintf(macLogFile,",%d\n",validationResult);
		fclose(macLogFile);
	}
	#endif

	/* other information */
	if(flagMAC0==TRUE_FLAG)
	{
		sprintf(MACString,"MAC0");
	}
	else
	{
		sprintf(MACString,"MAC");
	}

	if(prnA==prnN)
	{
		sprintf(prnAString,"authenticated by itself");
	}
	else
	{
		if(prnN==255 || prnN==254)
		{
			sprintf(prnAString," ");
		}
		else
		{
			sprintf(prnAString,"authenticated by Galileo PRN %d", prnA);
		}
	}
	if((prnN>=1 && prnN<=36) || prnN==255)
	{
		sprintf(satTypeString,"Galileo");

	}
	else
	{
		if((prnN>=64 && prnN<=95) || prnN==254)
		{
			sprintf(satTypeString,"GPS");
			prnN -= 63; // only for the text log (if 254, it is not used!)
		}
	}
	if(prnN==255 || prnN==254)
	{
		sprintf(prnString,"PRN %d",prnA);
	}
	else
	{
		sprintf(prnString,"PRN %d",prnN);
	}

	/* Log to the user and return value */
	if(validationResult)
	{
		/*Verify failure*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"%s %s, %s: %s with ADKD %d not verified",satTypeString,prnString,prnAString,MACString, macPtr->adkd);
		return NOT_VALID;
	}
	else
	{
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"%s %s, %s: %s with ADKD %d successfully verified",satTypeString,prnString,prnAString,MACString, macPtr->adkd);
		return VALIDATED;
	}
}




TValidationStatus MACSEQVerification(TOsnmaData *rxOsnma, TMackList *mackPtr,TMacList *macPtr,unsigned char* K, unsigned char *MACSEQ)
{

	int byteIdxOut=0;
	int bitIdxOut=0;
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	int mSize;


	int GST_SFSize=4;
	unsigned int GST_SF_Int;
	unsigned char GST_SF[GST_SFSize];/*GST_SF (32 bits): Galileo System Time Subframe*/
	unsigned char prnA=(unsigned char)(mackPtr->prnA);
	char satTypeString[8],prnString[40];


	mSize=1+GST_SFSize+macPtr->macInfoFlexSizeBytes;/*See pag 28 of ICD v1.1*/


	GST_SF_Int=(macPtr->wnSubframeStart<<20|(macPtr->towSubframeStart));
	GST_SF[0]=(GST_SF_Int>>24)& 0xFF;
	GST_SF[1]=(GST_SF_Int>>16)& 0xFF;
	GST_SF[2]=(GST_SF_Int>>8)& 0xFF;
	GST_SF[3]=(GST_SF_Int)& 0xFF;

	/*Concatenate fields*/

	ArrayClean((char *)rxOsnma->messageForMacseq,rxOsnma->messageForMacseqSizeMax);
	ArrayClean((char *)rxOsnma->messageHashed,SHA256_DIGEST_LENGTH);
	ArrayClean((char *)rxOsnma->macSeqComputed,MACSEQ_SIZE_BYTES);
	memcpy (&rxOsnma->messageForMacseq[0], &prnA, 1);
	memcpy (&rxOsnma->messageForMacseq[1], GST_SF, GST_SFSize);
	if(macPtr->macInfoFlexSizeBytes>0)
	{
		memcpy (&rxOsnma->messageForMacseq[1+GST_SFSize], macPtr->macInfoFlex, macPtr->macInfoFlexSizeBytes);
	}


	switch(pars->macFunction)
	{
	case HMAC_SHA_256:
		EVP_HMAC(rxOsnma->messageForMacseq,mSize,rxOsnma->messageHashed,EVP_sha256(),K,pars->keySizeBytes);
		break;
	case 1:
		CMAC_AES_function(rxOsnma->messageForMacseq,mSize,rxOsnma->messageHashed,K,pars->keySizeBytes);
		break;
	case MF_2_RSVD:
		/*Rsvd*/
		break;
	case MF_3_RSVD:
		/*Rsvd*/
		break;
	default:
		EVP_HMAC(rxOsnma->messageForMacseq,mSize,rxOsnma->messageHashed,EVP_sha256(),K,pars->keySizeBytes);
	}
	//**
	#ifdef DEBUG
	int i;
	for (i = 0; i < pars->keySizeBytes; i++) {
		printf("%02X", K[i]);
	}
	printf("\n");
	#endif
	//**

	byteIdxOut=0;
	bitIdxOut=0;
	CopyBitsUnaligned(0,0,rxOsnma->messageHashed,&byteIdxOut,&bitIdxOut,rxOsnma->macSeqComputed,12);

	#ifdef DEBUG
	int i;
	for (i = 0; i < MACSEQ_SIZE_BYTES; i++) {
		printf("%02X", rxOsnma->macSeqComputed[i]);
	}
	printf("\n");
	for (i = 0; i < MACSEQ_SIZE_BYTES; i++) {
		printf("%02X", MACSEQ[i]);
	}
	printf("\n");
	#endif




		if((prnA>=1 && prnA<=36) || prnA==255)
		{
			sprintf(satTypeString,"Galileo");

		}
		else
		{
			if((prnA>=64 && prnA<=95) || prnA==254)
			{
				sprintf(satTypeString,"GPS");
			}
		}
		sprintf(prnString,"PRN %d",prnA);

	if(CRYPTO_memcmp(MACSEQ, rxOsnma->macSeqComputed, MACSEQ_SIZE_BYTES))
	{
		/*Verify failure*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"%s %s:  MACSEQ not verified",satTypeString,prnString);
		return NOT_VALID;
	}
	else
	{
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"%s %s:  MACSEQ verified",satTypeString,prnString);
		return VALIDATED;
	}

}



unsigned char* FindKeyForMacVerification(TOsnmaParameters *pars,TOsnmaData *rxOsnma,TMackList *mackPtr,int wnMackStart,int towMackStart)
{

int indexMackBlock;
int GST_TOW_LastPrnKey,GST_WN_LastPrnKey;
unsigned char *lastKey;

	for(indexMackBlock=0;indexMackBlock<pars->nmack;indexMackBlock++)
	{
		#ifdef MACK_OFFSET_ICD2_DEFINITION
		if(IsThisPrnOffset(mackPtr->prnA,pars->mackOffset,pars->ns)==FALSE_FLAG)
		{
			GST_WN_LastPrnKey=rxOsnma->wnLastPrn1Key[indexMackBlock];
			GST_TOW_LastPrnKey=rxOsnma->towLastPrn1Key[indexMackBlock];
			lastKey=rxOsnma->lastPrn1Key[indexMackBlock];
		}
		else
		{
			GST_WN_LastPrnKey=rxOsnma->wnLastPrn19Key[indexMackBlock];
			GST_TOW_LastPrnKey=rxOsnma->towLastPrn19Key[indexMackBlock];
			lastKey=rxOsnma->lastPrn19Key[indexMackBlock];
		}
		#else
		if(pars->mackOffset==0)
		{
			GST_WN_LastPrnKey=rxOsnma->wnLastPrn1Key[indexMackBlock];
			GST_TOW_LastPrnKey=rxOsnma->towLastPrn1Key[indexMackBlock];
			lastKey=rxOsnma->lastPrn1Key[indexMackBlock];
		}
		else
		{
		        if(mackPtr->prnA<=18)
                {
                    GST_WN_LastPrnKey=rxOsnma->wnLastPrn1Key[indexMackBlock];
                    GST_TOW_LastPrnKey=rxOsnma->towLastPrn1Key[indexMackBlock];
                    lastKey=rxOsnma->lastPrn1Key[indexMackBlock];
                }
                else
                {
                    GST_WN_LastPrnKey=rxOsnma->wnLastPrn19Key[indexMackBlock];
                    GST_TOW_LastPrnKey=rxOsnma->towLastPrn19Key[indexMackBlock];
                    lastKey=rxOsnma->lastPrn19Key[indexMackBlock];
                }
		}
		#endif

		if((GST_WN_LastPrnKey==wnMackStart) && (GST_TOW_LastPrnKey==towMackStart))
		{
			return lastKey;
		}
	}

	return NULL;
}


int MACVerificationAll(TNGeneConfiguration *ngeneCfg, TNGeneData *ngene)
{
	TMackList *mackPtr=NULL; //, *mackPtrToFree=NULL;

	TOsnmaData *rxOsnma=&(ngene->osnmaData);
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	unsigned char *keyForMacVerification;
	int indexAuthenticatedChannel,indexAuthenticatingChannel;
	TMacList *macPtr;
	TMacList *macPtrNext;
	TChannel *ch;
	int navMsgWn;
	int navMsgTow;
	//TNavDataList *navDataToFree=NULL;
	int towToRemove, wnToRemove;
	//**
	int ExtractAndCheckBitsFlag;
	int numberOfMacsVerified=0;
	// navdata log
	TNavDataList *navDataCurrPtr;
	TNavDataList *navDataPrevPtr;
	int navDataCurrWn;
	int navDataCurrTow;
	int navDataPrevWn;
	int navDataPrevTow;

	/* If needed, clean all the validity flag */
	if(rxOsnma->revocationStarted==TRUE_FLAG)
	{
		for(indexAuthenticatedChannel=0;indexAuthenticatedChannel<ngeneCfg->numberOfChannels;indexAuthenticatedChannel++)
		{
			// useful for GPS and F/NAV.
			// For I/NAV, other flags are cleaned by RestartChannelOsnmaVariables called by UpdateChannelOsnmaVariables
			ch = &(ngene->channelArray[indexAuthenticatedChannel]);
			ch->msgStatus = TO_BE_VALIDATED;
		}
	}

	/* Check the availability of MACs */
	if(rxOsnma->newMacsAvailable==FALSE_FLAG)
	{
		/* if no MACs have been saved since last MAC verification
		 * do nothing */
		return numberOfMacsVerified;
	}

	/* MAC will be verified! */
	rxOsnma->newMacsAvailable = FALSE_FLAG;

	for(indexAuthenticatedChannel=0;indexAuthenticatedChannel<ngeneCfg->numberOfChannels;indexAuthenticatedChannel++)
	{
		ch = &(ngene->channelArray[indexAuthenticatedChannel]);

		for(indexAuthenticatingChannel=0;indexAuthenticatingChannel<ngeneCfg->numberOfChannels;indexAuthenticatingChannel++)
		{
			mackPtr = FindInMackList(rxOsnma->mackList,ch->satType,ch->satSignal,ch->satId,indexAuthenticatingChannel);
			if(mackPtr!=NULL)
			{
				/* Remove NavData older than 10 minutes (plus margin) */
				#ifdef MACK_OFFSET_ICD2_DEFINITION
				if(IsThisPrnOffset(mackPtr->prnA,pars->mackOffset,pars->ns)==TRUE_FLAG)
				#else
				if(pars->mackOffset==1 &&  (mackPtr->prnA>18))
				#endif
				{
					towToRemove=rxOsnma->towLastPrn19Key[0];
					wnToRemove=rxOsnma->wnLastPrn19Key[0];
				}
				else
				{
					towToRemove=rxOsnma->towLastPrn1Key[0];
					wnToRemove=rxOsnma->wnLastPrn1Key[0];
				}

				if(towToRemove!=-1)
				{
					/* 10 is the delay of the key in the ADKD 12
					 * 1 additional subframe added to be compliant with possible Mack Block Offset
					 * 1 additional subframe added to cope with possible latencies in the elaboration */
					towToRemove -= ((10+2)*GAL_SECONDS_PER_SUBFRAME);
					if(towToRemove<0)
					{
						towToRemove += SECONDS_PER_WEEK;
						wnToRemove--;
					}
					DeleteNavDataOlderThan(mackPtr,wnToRemove,towToRemove);
				}
				if(mackPtr->macList!=NULL)
				{
					macPtr = mackPtr->macList;
					while(macPtr!=NULL)
					{
						macPtrNext = macPtr->nextMac;
						if(macPtr->macToBeVerified==TRUE_FLAG)
						{
							#ifdef OSNMA_PROFILING
							StartProfilingUpdate(&(rxOsnma->findKeyForMACVerification));
							#endif
							keyForMacVerification=FindKeyForMacVerification(pars,rxOsnma,mackPtr,macPtr->wnKey,macPtr->towKey);
							#ifdef OSNMA_PROFILING
							StopProfilingUpdate(&(rxOsnma->findKeyForMACVerification),1);
							#endif
							if(keyForMacVerification!=NULL)
							{
								navMsgWn = macPtr->wnSubframeStart;
								navMsgTow = macPtr->towSubframeStart;
								#ifdef OSNMA_PROFILING
								StartProfilingUpdate(&(rxOsnma->extractAndCheckBitsForMACVerification));
								#endif
								#ifndef PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION
								ExtractAndCheckBitsFlag=ExtractAndCheckBitsForMacVerification(rxOsnma,ch->satType,ch->satSignal,ch->satId,indexAuthenticatedChannel,navMsgWn,navMsgTow,macPtr->adkd,macPtr->iod,&navDataCurrPtr,&navDataPrevPtr);
								#else
								ExtractAndCheckBitsFlag=ExtractAndCheckBitsForMacVerification(rxOsnma,ch->satType,ch->satSignal,ch->satId,indexAuthenticatedChannel,navMsgWn,navMsgTow,macPtr->adkd,&(macPtr->iod),&navDataCurrPtr,&navDataPrevPtr);
								#endif
								#ifdef OSNMA_PROFILING
								StopProfilingUpdate(&(rxOsnma->extractAndCheckBitsForMACVerification),1);
								#endif
								if(ExtractAndCheckBitsFlag==SUCCESS_RETURN)
								{
									rxOsnma->newMacsAvailable = FALSE_FLAG; /* MAC will be verified! */
									if(macPtr->wnMackStart==macPtr->wnSubframeStart && macPtr->towMackStart==macPtr->towSubframeStart && macPtr->ctr==1)
									{
										//*********MAC0 verification*********************
										#ifdef OSNMA_PROFILING
										StartProfilingUpdate(&(rxOsnma->macProfiling));
										#endif
										ch->msgStatus=MACVerification(rxOsnma, mackPtr,macPtr,keyForMacVerification, macPtr->mac,TRUE_FLAG);
										#ifdef OSNMA_PROFILING
										StopProfilingUpdate(&(rxOsnma->macProfiling),1);
										#endif
										numberOfMacsVerified++;
										/* MACSEQ verification */
										if(rxOsnma->osnmaPars.macLut.lutValid==TRUE_FLAG)
										{
											// perform MACSEQ verification only when MAC LUT is valid!
											#ifdef OSNMA_PROFILING
											StartProfilingUpdate(&(rxOsnma->macSeqProfiling));
											#endif
											ch->macseqStatus=MACSEQVerification(rxOsnma, mackPtr,macPtr,keyForMacVerification, macPtr->macseq);
											#ifdef OSNMA_PROFILING
											StopProfilingUpdate(&(rxOsnma->macSeqProfiling),1);
											#endif
											numberOfMacsVerified++;
										}

										/* log about navigation data used for MAC auth */
										if(rxOsnma->navdataLogFile!=NULL)
										{
											navDataCurrWn = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->wnSubframeStart;
											navDataCurrTow = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->towSubframeStart;
											navDataPrevWn = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->wnSubframeStart;
											navDataPrevTow = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->towSubframeStart;
											fprintf(rxOsnma->navdataLogFile,"%4d %6d %4d %6d %3d %3d %2d %2u    %4d %6d   %4d %6d\n",macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->wnMackStart,macPtr->towMackStart,mackPtr->prnA,mackPtr->prnN,(int)macPtr->adkd,macPtr->ctr,navDataCurrWn,navDataCurrTow,navDataPrevWn,navDataPrevTow);
										}
										/* validation log (MAC) */
										if((ch->msgStatus==VALIDATED) && (rxOsnma->validationLogFile!=NULL))
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,1);
										}
										else if(rxOsnma->validationLogFile!=NULL)
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,0);
										}
										/* validation log (MACSEQ) */
										if((ch->macseqStatus==VALIDATED) && (rxOsnma->validationLogFile!=NULL))
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MACSEQ_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,-1,-1,-1,1);
										}
										else if(rxOsnma->validationLogFile!=NULL)
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MACSEQ_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,-1,-1,-1,0);
										}
									}
									else
									{
										//*********MAC verification*********************
										#ifdef OSNMA_PROFILING
										StartProfilingUpdate(&(rxOsnma->macProfiling));
										#endif
										ch->msgStatus=MACVerification(rxOsnma, mackPtr,macPtr,keyForMacVerification, macPtr->mac,FALSE_FLAG);
										#ifdef OSNMA_PROFILING
										StopProfilingUpdate(&(rxOsnma->macProfiling),1);
										#endif
										numberOfMacsVerified++;
										
										/* log about navigation data used for MAC auth */
										if(rxOsnma->navdataLogFile!=NULL)
										{
											navDataCurrWn = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->wnSubframeStart;
											navDataCurrTow = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->towSubframeStart;
											navDataPrevWn = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->wnSubframeStart;
											navDataPrevTow = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->towSubframeStart;
											fprintf(rxOsnma->navdataLogFile,"%4d %6d %4d %6d %3d %3d %2d %2u    %4d %6d   %4d %6d\n",macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->wnMackStart,macPtr->towMackStart,mackPtr->prnA,mackPtr->prnN,(int)macPtr->adkd,macPtr->ctr,navDataCurrWn,navDataCurrTow,navDataPrevWn,navDataPrevTow);
										}
										/* validation log (MAC) */
										if((ch->msgStatus==VALIDATED) && (rxOsnma->validationLogFile!=NULL))
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,1);
										}
										else if(rxOsnma->validationLogFile!=NULL)
										{
											fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,0);
										}
									}

									/* save the validation for further analysis */
									InsertMacValidation(&(ngene->osnmaAuth),indexAuthenticatingChannel,indexAuthenticatedChannel,mackPtr->prnN,macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->adkd,macPtr->iod,ch->msgStatus);

									/* clean variables */
									macPtr->macToBeVerified=FALSE_FLAG;
									/**/
									DeleteAMacFromMacList(mackPtr,macPtr);
									/**/
								}
							}
							else
							{
								#ifdef MACK_OFFSET_ICD2_DEFINITION
								if(IsThisPrnOffset(mackPtr->prnA,pars->mackOffset,pars->ns)==TRUE_FLAG)
								#else
								if(pars->mackOffset==1 &&  (mackPtr->prnA>18))
								#endif
								{
									DeleteMacIfKeyOlderThan(mackPtr,macPtr, rxOsnma->wnLastPrn19Key[0],rxOsnma->towLastPrn19Key[0]);
								}
								else
								{
									DeleteMacIfKeyOlderThan(mackPtr,macPtr, rxOsnma->wnLastPrn1Key[0],rxOsnma->towLastPrn1Key[0]);
								}
							}
						}
						macPtr = macPtrNext;
					}
				}
			}
		}
	}

	/* PRN 255 */
	for(indexAuthenticatingChannel=0;indexAuthenticatingChannel<ngeneCfg->numberOfChannels;indexAuthenticatingChannel++)
	{
		ch = &(ngene->channelArray[indexAuthenticatingChannel]);
		mackPtr = FindInMackList(rxOsnma->mackList,GALILEO_SAT,E1_BC,255,indexAuthenticatingChannel); // 255 is used only for I/NAV
		if(mackPtr!=NULL && mackPtr->macList!=NULL)
		{
			macPtr = mackPtr->macList;
			while(macPtr!=NULL)
			{
				macPtrNext = macPtr->nextMac;
				if(macPtr->macToBeVerified==TRUE_FLAG)
				{
					#ifdef OSNMA_PROFILING
					StartProfilingUpdate(&(rxOsnma->findKeyForMACVerification));
					#endif
					keyForMacVerification=FindKeyForMacVerification(pars,rxOsnma,mackPtr,macPtr->wnKey,macPtr->towKey);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->findKeyForMACVerification),1);
					#endif
					if(keyForMacVerification!=NULL)
					{
						navMsgWn = macPtr->wnSubframeStart;
						navMsgTow = macPtr->towSubframeStart;
						#ifdef OSNMA_PROFILING
						StartProfilingUpdate(&(rxOsnma->extractAndCheckBitsForMACVerification));
						#endif
						#ifndef PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION
						ExtractAndCheckBitsFlag=ExtractAndCheckBitsForMacVerification(rxOsnma,mackPtr->satType,mackPtr->satSignal,mackPtr->prnA,indexAuthenticatingChannel,navMsgWn,navMsgTow,macPtr->adkd,macPtr->iod,&navDataCurrPtr,&navDataPrevPtr);
						#else
						ExtractAndCheckBitsFlag=ExtractAndCheckBitsForMacVerification(rxOsnma,mackPtr->satType,mackPtr->satSignal,mackPtr->prnA,indexAuthenticatingChannel,navMsgWn,navMsgTow,macPtr->adkd,&(macPtr->iod),&navDataCurrPtr,&navDataPrevPtr);
						#endif
						#ifdef OSNMA_PROFILING
						StopProfilingUpdate(&(rxOsnma->extractAndCheckBitsForMACVerification),1);
						#endif
						if(ExtractAndCheckBitsFlag==SUCCESS_RETURN)
						{
							rxOsnma->newMacsAvailable = FALSE_FLAG; /* MAC will be verified! */
							//*********MAC verification*********************
							#ifdef OSNMA_PROFILING
							StartProfilingUpdate(&(rxOsnma->macProfiling));
							#endif
							ch->msgStatus=MACVerification(rxOsnma, mackPtr,macPtr,keyForMacVerification, macPtr->mac,FALSE_FLAG);
							#ifdef OSNMA_PROFILING
							StopProfilingUpdate(&(rxOsnma->macProfiling),1);
							#endif
							numberOfMacsVerified++;

							/* log about navigation data used for MAC auth */
							if(rxOsnma->navdataLogFile!=NULL)
							{
								navDataCurrWn = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->wnSubframeStart;
								navDataCurrTow = (navDataCurrPtr==NULL) ? INVALID_VALUE : navDataCurrPtr->towSubframeStart;
								navDataPrevWn = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->wnSubframeStart;
								navDataPrevTow = (navDataPrevPtr==NULL) ? INVALID_VALUE : navDataPrevPtr->towSubframeStart;
								fprintf(rxOsnma->navdataLogFile,"%4d %6d %4d %6d %3d %3d %2d %2u    %4d %6d   %4d %6d\n",macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->wnMackStart,macPtr->towMackStart,mackPtr->prnA,mackPtr->prnN,(int)macPtr->adkd,macPtr->ctr,navDataCurrWn,navDataCurrTow,navDataPrevWn,navDataPrevTow);
							}
							/* validation log (MAC) */
							if((ch->msgStatus==VALIDATED) && (rxOsnma->validationLogFile!=NULL))
							{
								fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,1);
							}
							else if(rxOsnma->validationLogFile!=NULL)
							{
								fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_MAC_VALIDATION,macPtr->wnKey,macPtr->towKey+pars->mackBlockDuration,macPtr->wnMackStart,macPtr->towMackStart+pars->mackBlockDuration,mackPtr->prnA,-1,-1,-1,-1,-1,mackPtr->prnN,macPtr->adkd,macPtr->iod,0);
							}
							/* save the validation for further analysis */
							// when prnN==255, indexAuthenticatedChannel is set to indexAuthenticatingChannel, i.e.
							// prnN==255 is used for self-authentication only
							InsertMacValidation(&(ngene->osnmaAuth),indexAuthenticatingChannel,indexAuthenticatingChannel,mackPtr->prnA,macPtr->wnSubframeStart,macPtr->towSubframeStart,macPtr->adkd,macPtr->iod,ch->msgStatus);

							/* clean variables */
							macPtr->macToBeVerified=FALSE_FLAG;
							/**/
							DeleteAMacFromMacList(mackPtr,macPtr);
							/**/
						}
					}
					else
					{
						#ifdef MACK_OFFSET_ICD2_DEFINITION
						if(IsThisPrnOffset(mackPtr->prnA,pars->mackOffset,pars->ns)==TRUE_FLAG)
						#else
						if(pars->mackOffset==1 &&  (mackPtr->prnA>18))
						#endif
						{
							DeleteMacIfKeyOlderThan(mackPtr,macPtr, rxOsnma->wnLastPrn19Key[0],rxOsnma->towLastPrn19Key[0]);
						}
						else
						{
							DeleteMacIfKeyOlderThan(mackPtr,macPtr, rxOsnma->wnLastPrn1Key[0],rxOsnma->towLastPrn1Key[0]);
						}
					}
				}
				macPtr = macPtrNext;
			}
		}
	}
	#ifdef OSNMA_PROFILING
	SwrxLog(NGENE_LOG_SEVERITY_INFO,"Number of MAC verified = %d",numberOfMacsVerified);
	#endif
	return numberOfMacsVerified;
}


int MACVerificationOld(unsigned char *m, size_t mSize, unsigned char *CIDKR_NMACK_HF_MF, unsigned char *mHashed, unsigned char *mHashedRef,unsigned char* K,size_t KSBytes, unsigned char *KS_MS, unsigned char *tag, unsigned char *MAC)
{

	int byteIdxOut=0;
	int bitIdxOut=0;
	int MFInt; /*MAC Function represented as integer*/
	int MSInt; /*MAC Size field represented as integer*/
	int MACSizeBits,MACSizeBytes; /*MAC Size represented as number of bits and bytes respectively*/
	size_t hashSize=0;




	MFInt= (int)(CIDKR_NMACK_HF_MF[0] & 0x03);

	switch(MFInt)
	{
	case 0:
		EVP_HMAC(m,mSize,mHashed,EVP_sha256(),K,KSBytes);
		hashSize=SHA256_DIGEST_LENGTH;
		break;
	case 1:
		CMAC_AES_function(m,mSize,mHashed,K,KSBytes);
		hashSize=16;
		break;
	case 2:
		/*rsv*/
		break;
	case 3:
		/*rsv*/
		break;
	default:
		EVP_HMAC(m,mSize,mHashed,EVP_sha256(),K,KSBytes);
		hashSize=SHA256_DIGEST_LENGTH;
	}

	if(mHashedRef!=NULL)
	{
		if(CRYPTO_memcmp(mHashedRef, mHashed, hashSize))
		{
			/*Verify failure*/
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC not correct\n \n");
		}
		else
		{
			/*Verify success*/
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC correct\n");
		}
	}

	MSInt= (int)(KS_MS[0] & 0x03);
	switch(MSInt)
	{
	case 0:
		MACSizeBits=10;
		break;
	case 1:
		MACSizeBits=12;
		break;
	case 2:
		MACSizeBits=14;
		break;
	case 3:
		MACSizeBits=16;
		break;
	case 4:
		MACSizeBits=18;
		break;
	case 5:
		MACSizeBits=20;
		break;
	case 6:
		MACSizeBits=24;
		break;
	case 7:
		MACSizeBits=28;
		break;
	case 8:
		MACSizeBits=32;
		break;
	case 9:
		MACSizeBits=40;
		break;
	case 10:
		/*rsv*/
		break;
	case 11:
		/*rsv*/
		break;
	case 12:
		/*rsv*/
		break;
	case 13:
		/*rsv*/
		break;
	case 14:
		/*rsv*/
		break;
	case 15:
		/*rsv*/
		break;
	default:
		MACSizeBits=10;
	}

	MACSizeBytes=ceil((double)MACSizeBits/8);
	byteIdxOut=0;
	bitIdxOut=0;
	CopyBitsUnaligned(0,0,mHashed,&byteIdxOut,&bitIdxOut,tag,MACSizeBits);
	if(CRYPTO_memcmp(MAC, tag, MACSizeBytes))
	{
		/*Verify failure*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC not verified\n \n");
	}
	else
	{
		/*Verify success*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC successfully verified\n");
	}

/*		for (index = 0; index < MACSizeBytes; index++)
	{
		printf("%02X", tag0[index]);
	}
	printf("\n");*/



	return 0;
}

int MACSEQVerificationForTestVector(unsigned char *m, size_t mSize, unsigned char *CIDKR_NMACK_HF_MF, unsigned char *mHashed, unsigned char *mHashedRef,unsigned char* K,size_t KSBytes, unsigned char *MACSEQGenerated, unsigned char *MACSEQ)
{

	int byteIdxOut=0;
	int bitIdxOut=0;
	int MFInt; /*MAC Function represented as integer*/
	size_t MACSEQSizeBytes; /*MACSEQ Size represented as number of bits and bytes respectively*/
	size_t hashSize=0;




	MFInt= (int)(CIDKR_NMACK_HF_MF[0] & 0x03);

	switch(MFInt)
	{
	case 0:
		EVP_HMAC(m,mSize,mHashed,EVP_sha256(),K,KSBytes);
		hashSize=SHA256_DIGEST_LENGTH;
		break;
	case 1:
		CMAC_AES_function(m,mSize,mHashed,K,KSBytes);
		hashSize=16;
		break;
	case 2:
		/*rsv*/
		break;
	case 3:
		/*rsv*/
		break;
	default:
		EVP_HMAC(m,mSize,mHashed,EVP_sha256(),K,KSBytes);
		hashSize=SHA256_DIGEST_LENGTH;
	}
	if(mHashedRef!=NULL)
	{
		if(CRYPTO_memcmp(mHashedRef, mHashed, hashSize))
		{
			/*Verify failure*/
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC not correct\n \n");
		}
		else
		{
			/*Verify success*/
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MAC correct\n");
		}
	}

	MACSEQSizeBytes=ceil((double)12/8);
	byteIdxOut=0;
	bitIdxOut=0;
	CopyBitsUnaligned(0,0,mHashed,&byteIdxOut,&bitIdxOut,MACSEQGenerated,12);
	if(CRYPTO_memcmp(MACSEQ, MACSEQGenerated, MACSEQSizeBytes))
	{
		/*Verify failure*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MACSEQ not verified\n \n");
	}
	else
	{
		/*Verify success*/
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"MACSEQ successfully verified\n");
	}

/*		for (index = 0; index < MACSizeBytes; index++)
	{
		printf("%02X", tag0[index]);
	}
	printf("\n");*/



	return 0;
}

int MerkleTreePublicKeyVerification(TOsnmaData *rxOsnma, TOsnmaParameters *osnmaPars, unsigned char *dsmBlocks)
{
	int i=0;
	int j,index,indexRow,indexColumn;
	unsigned char  *firstNode,*secondNode,*thirdNode,*fourthNode;

	/*Concatenate fields to get the m[i] leaf*/
	for(index=0;index<NUMBER_OF_LEAVES;index++)
	{
		ArrayClean((char *)rxOsnma->leaves[index],ECDSA_COMPRESSED_KEY_LEN_P521/8);
	}

	for(indexRow=0;indexRow<(NUMBER_OF_LEVELS+1);indexRow++)
	{
		for(indexColumn=0;indexColumn<NUMBER_OF_LEAVES;indexColumn++)
		{
			ArrayClean((char *)rxOsnma->nodeMatrix[indexRow][indexColumn],SHA256_DIGEST_LENGTH);
		}
	}

	memcpy (rxOsnma->leaves[osnmaPars->dsmPkrMessageId], &(dsmBlocks[129]), 1);
	memcpy (&rxOsnma->leaves[osnmaPars->dsmPkrMessageId][1],rxOsnma->pkrPublicKeyTmp, rxOsnma->pkrNpkLen/8);
	rxOsnma->leafSizeBytes=1+rxOsnma->pkrNpkLen/8;

	switch(osnmaPars->dsmPkrMessageId)
	{
	case 0:
		firstNode=rxOsnma->nodeMatrix[0][1];
		secondNode=rxOsnma->nodeMatrix[1][1];
		thirdNode=rxOsnma->nodeMatrix[2][1];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 1:
		firstNode=rxOsnma->nodeMatrix[0][0];
		secondNode=rxOsnma->nodeMatrix[1][1];
		thirdNode=rxOsnma->nodeMatrix[2][1];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 2:
		firstNode=rxOsnma->nodeMatrix[0][3];
		secondNode=rxOsnma->nodeMatrix[1][0];
		thirdNode=rxOsnma->nodeMatrix[2][1];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 3:
		firstNode=rxOsnma->nodeMatrix[0][2];
		secondNode=rxOsnma->nodeMatrix[1][0];
		thirdNode=rxOsnma->nodeMatrix[2][1];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 4:
		firstNode=rxOsnma->nodeMatrix[0][5];
		secondNode=rxOsnma->nodeMatrix[1][3];
		thirdNode=rxOsnma->nodeMatrix[2][0];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 5:
		firstNode=rxOsnma->nodeMatrix[0][4];
		secondNode=rxOsnma->nodeMatrix[1][3];
		thirdNode=rxOsnma->nodeMatrix[2][0];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 6:
		firstNode=rxOsnma->nodeMatrix[0][7];
		secondNode=rxOsnma->nodeMatrix[1][2];
		thirdNode=rxOsnma->nodeMatrix[2][0];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 7:
		firstNode=rxOsnma->nodeMatrix[0][6];
		secondNode=rxOsnma->nodeMatrix[1][2];
		thirdNode=rxOsnma->nodeMatrix[2][0];
		fourthNode=rxOsnma->nodeMatrix[3][1];
		break;
	case 8:
		firstNode=rxOsnma->nodeMatrix[0][9];
		secondNode=rxOsnma->nodeMatrix[1][5];
		thirdNode=rxOsnma->nodeMatrix[2][3];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 9:
		firstNode=rxOsnma->nodeMatrix[0][8];
		secondNode=rxOsnma->nodeMatrix[1][5];
		thirdNode=rxOsnma->nodeMatrix[2][3];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 10:
		firstNode=rxOsnma->nodeMatrix[0][11];
		secondNode=rxOsnma->nodeMatrix[1][4];
		thirdNode=rxOsnma->nodeMatrix[2][3];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 11:
		firstNode=rxOsnma->nodeMatrix[0][10];
		secondNode=rxOsnma->nodeMatrix[1][4];
		thirdNode=rxOsnma->nodeMatrix[2][3];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 12:
		firstNode=rxOsnma->nodeMatrix[0][13];
		secondNode=rxOsnma->nodeMatrix[1][7];
		thirdNode=rxOsnma->nodeMatrix[2][2];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 13:
		firstNode=rxOsnma->nodeMatrix[0][12];
		secondNode=rxOsnma->nodeMatrix[1][7];
		thirdNode=rxOsnma->nodeMatrix[2][2];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 14:
		firstNode=rxOsnma->nodeMatrix[0][15];
		secondNode=rxOsnma->nodeMatrix[1][6];
		thirdNode=rxOsnma->nodeMatrix[2][2];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	case 15:
		firstNode=rxOsnma->nodeMatrix[0][14];
		secondNode=rxOsnma->nodeMatrix[1][6];
		thirdNode=rxOsnma->nodeMatrix[2][2];
		fourthNode=rxOsnma->nodeMatrix[3][0];
		break;
	default:
		firstNode=rxOsnma->nodeMatrix[0][1];
		secondNode=rxOsnma->nodeMatrix[1][1];
		thirdNode=rxOsnma->nodeMatrix[2][1];
		fourthNode=rxOsnma->nodeMatrix[3][1];

	}
	memcpy (firstNode, &rxOsnma->mtIntermediateNodes[0], SHA256_DIGEST_LENGTH);
	memcpy (secondNode, &rxOsnma->mtIntermediateNodes[SHA256_DIGEST_LENGTH], SHA256_DIGEST_LENGTH);
	memcpy (thirdNode, &rxOsnma->mtIntermediateNodes[2*SHA256_DIGEST_LENGTH], SHA256_DIGEST_LENGTH);
	memcpy (fourthNode, &rxOsnma->mtIntermediateNodes[3*SHA256_DIGEST_LENGTH], SHA256_DIGEST_LENGTH);


	EVP_HASH(rxOsnma->leaves[osnmaPars->dsmPkrMessageId],rxOsnma->leafSizeBytes, rxOsnma->nodeMatrix[0][osnmaPars->dsmPkrMessageId],EVP_sha256());
	i=osnmaPars->dsmPkrMessageId>>1;
	for (j=1;j<(NUMBER_OF_LEVELS+1);j++)
	{
		ArrayClean((char *)rxOsnma->nodesConcatenated,2*SHA256_DIGEST_LENGTH);
		memcpy (rxOsnma->nodesConcatenated, rxOsnma->nodeMatrix[j-1][2*i], SHA256_DIGEST_LENGTH);
		memcpy (&rxOsnma->nodesConcatenated[SHA256_DIGEST_LENGTH], rxOsnma->nodeMatrix[j-1][2*i+1], SHA256_DIGEST_LENGTH);
		EVP_HASH(rxOsnma->nodesConcatenated,2*SHA256_DIGEST_LENGTH, rxOsnma->nodeMatrix[j][i],EVP_sha256());
		if(i>0)
		{
			i=i>>1;
		}
	}

	if(CRYPTO_memcmp(osnmaPars->mtRoot, rxOsnma->nodeMatrix[4][0], SHA256_DIGEST_LENGTH))
	{
		/*Verify failure*/
		return FAILURE_RETURN;
	}
	else
	{
		/*Verify success*/
		return SUCCESS_RETURN;
	}

	return 0;
}

int DSM_PKR_P2Verification(TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars)
{
	if(rxOsnma->pkrP2Len==0)
	{
		// if the padding is empty, does not need verification
		return SUCCESS_RETURN;
	}

	ArrayClean((char *)rxOsnma->mtRoot_leaf,SHA256_DIGEST_LENGTH+MAX_COMPRESSED_PUBLICKEY_SIZE+1);
	ArrayClean((char *)rxOsnma->mtRoot_leafHashed,SHA256_DIGEST_LENGTH);
	ArrayClean((char *)rxOsnma->P2Computed,DSM_BLOCK_SIZE_BITS/8);

	memcpy (rxOsnma->mtRoot_leaf, osnmaPars->mtRoot, SHA256_DIGEST_LENGTH);
	memcpy (&rxOsnma->mtRoot_leaf[SHA256_DIGEST_LENGTH], rxOsnma->leaves[osnmaPars->dsmPkrMessageId], rxOsnma->leafSizeBytes);

	EVP_HASH(rxOsnma->mtRoot_leaf,SHA256_DIGEST_LENGTH+rxOsnma->leafSizeBytes, rxOsnma->mtRoot_leafHashed,EVP_sha256());

	memcpy (rxOsnma->P2Computed, rxOsnma->mtRoot_leafHashed, rxOsnma->pkrP2Len/8);
	if(CRYPTO_memcmp(rxOsnma->pkrP2, rxOsnma->P2Computed, rxOsnma->pkrP2Len/8))
	{
		/*Verify failure*/
		return FAILURE_RETURN;
	}
	else
	{
		/*Verify success*/
		return SUCCESS_RETURN;
	}
}




int MerkleTreePublicKeyVerificationTestVector(unsigned char *IT0,unsigned char *IT1,unsigned char *IT2,unsigned char *IT3,int MID, unsigned char *leaf,size_t leafSizeBytes, unsigned char ***nodeMatrix,unsigned char *nodesConcatenated, size_t nodesConcatenatedBytes, unsigned char *rootMerkleTree)
{
	int i=0;
	int j;
	unsigned char  *firstNode,*secondNode,*thirdNode,*fourthNode;



	switch(MID)
			{
			case 0:
				firstNode=nodeMatrix[0][1];
				secondNode=nodeMatrix[1][1];
				thirdNode=nodeMatrix[2][1];
				fourthNode=nodeMatrix[3][1];
				break;
			case 1:
				firstNode=nodeMatrix[0][0];
				secondNode=nodeMatrix[1][1];
				thirdNode=nodeMatrix[2][1];
				fourthNode=nodeMatrix[3][1];
				break;
			case 2:
				firstNode=nodeMatrix[0][3];
				secondNode=nodeMatrix[1][0];
				thirdNode=nodeMatrix[2][1];
				fourthNode=nodeMatrix[3][1];
				break;
			case 3:
				firstNode=nodeMatrix[0][2];
				secondNode=nodeMatrix[1][0];
				thirdNode=nodeMatrix[2][1];
				fourthNode=nodeMatrix[3][1];
				break;
			case 4:
				firstNode=nodeMatrix[0][5];
				secondNode=nodeMatrix[1][3];
				thirdNode=nodeMatrix[2][0];
				fourthNode=nodeMatrix[3][1];
				break;
			case 5:
				firstNode=nodeMatrix[0][4];
				secondNode=nodeMatrix[1][3];
				thirdNode=nodeMatrix[2][0];
				fourthNode=nodeMatrix[3][1];
				break;
			case 6:
				firstNode=nodeMatrix[0][7];
				secondNode=nodeMatrix[1][2];
				thirdNode=nodeMatrix[2][0];
				fourthNode=nodeMatrix[3][1];
				break;
			case 7:
				firstNode=nodeMatrix[0][6];
				secondNode=nodeMatrix[1][2];
				thirdNode=nodeMatrix[2][0];
				fourthNode=nodeMatrix[3][1];
				break;
			case 8:
				firstNode=nodeMatrix[0][9];
				secondNode=nodeMatrix[1][5];
				thirdNode=nodeMatrix[2][3];
				fourthNode=nodeMatrix[3][0];
				break;
			case 9:
				firstNode=nodeMatrix[0][8];
				secondNode=nodeMatrix[1][5];
				thirdNode=nodeMatrix[2][3];
				fourthNode=nodeMatrix[3][0];
				break;
			case 10:
				firstNode=nodeMatrix[0][11];
				secondNode=nodeMatrix[1][4];
				thirdNode=nodeMatrix[2][3];
				fourthNode=nodeMatrix[3][0];
				break;
			case 11:
				firstNode=nodeMatrix[0][10];
				secondNode=nodeMatrix[1][4];
				thirdNode=nodeMatrix[2][3];
				fourthNode=nodeMatrix[3][0];
				break;
			case 12:
				firstNode=nodeMatrix[0][13];
				secondNode=nodeMatrix[1][7];
				thirdNode=nodeMatrix[2][2];
				fourthNode=nodeMatrix[3][0];
				break;
			case 13:
				firstNode=nodeMatrix[0][12];
				secondNode=nodeMatrix[1][7];
				thirdNode=nodeMatrix[2][2];
				fourthNode=nodeMatrix[3][0];
				break;
			case 14:
				firstNode=nodeMatrix[0][15];
				secondNode=nodeMatrix[1][6];
				thirdNode=nodeMatrix[2][2];
				fourthNode=nodeMatrix[3][0];
				break;
			case 15:
				firstNode=nodeMatrix[0][14];
				secondNode=nodeMatrix[1][6];
				thirdNode=nodeMatrix[2][2];
				fourthNode=nodeMatrix[3][0];
				break;
			default:
				firstNode=nodeMatrix[0][1];
				secondNode=nodeMatrix[1][1];
				thirdNode=nodeMatrix[2][1];
				fourthNode=nodeMatrix[3][1];

			}
			memcpy (firstNode, IT0, SHA256_DIGEST_LENGTH);
			memcpy (secondNode, IT1, SHA256_DIGEST_LENGTH);
			memcpy (thirdNode, IT2, SHA256_DIGEST_LENGTH);
			memcpy (fourthNode, IT3, SHA256_DIGEST_LENGTH);


		//for (i=0;i<NUMBER_OF_LEAVES;i++)
		//{
			EVP_HASH(leaf,leafSizeBytes, nodeMatrix[0][MID],EVP_sha256());
			i=MID>>1;
			for (j=1;j<(NUMBER_OF_LEVELS+1);j++)
			{
				ArrayClean((char *)nodesConcatenated,nodesConcatenatedBytes);
				memcpy (nodesConcatenated, nodeMatrix[j-1][2*i], 32);
				memcpy (&nodesConcatenated[SHA256_DIGEST_LENGTH], nodeMatrix[j-1][2*i+1], 32);
				EVP_HASH(nodesConcatenated,nodesConcatenatedBytes, nodeMatrix[j][i],EVP_sha256());
				if(i>0)
				{
					i=i>>1;
				}
			}
		//}
			if(CRYPTO_memcmp(rootMerkleTree, nodeMatrix[4][0], SHA256_DIGEST_LENGTH))
			{
				/*Verify failure*/
				SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Merkle Tree Root not correct\n \n");
			}
			else
			{
				/*Verify success*/
				SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Merkle Tree Root correct\n");
			}

			return 0;
}


