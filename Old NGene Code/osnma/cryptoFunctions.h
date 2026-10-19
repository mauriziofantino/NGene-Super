/*
 * cryptoFunctionsTest.h
 *
 *  Created on: May 16, 2019
 *      Author: micaela
 */

#ifndef OSNMA_CRYPTOFUNCTIONS_H_
#define OSNMA_CRYPTOFUNCTIONS_H_

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/cmac.h>
#include "openssl/ecdsa.h"
#include "openssl/obj_mac.h"
#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include "base64.h"
#include "navigationMessageParser.h"
#include "../../asmFolder/arrays.h"
#include "ngeneDataStructure.h"
#include "osnmaManagement.h"


//#define DEBUG

/*Merkle Tree*/

/*Number of leaves of the OSNMA Merkle Tree (from ICD v1.1)*/
#define NUMBER_OF_LEAVES 16

/*Number of levels of the OSNMA Merkle Tree (from ICD v1.1)*/
#define NUMBER_OF_LEVELS 4

/*Number of nodes of the OSNMA Merkle Tree (from ICD v1.1)*/
#define NUMBER_OF_NODES 31



/*Duration of a Galileo Subframe (30 s)*/
#define SUBFRAME_DURATION 30

/*Maximum MS-MAC SIZE (bits)*/
#define MAX_MAC_SIZE_BITS 40

/*Maximum Number of MFLEX (MAC Info Flexible) in a MACK Block (from Table 30 of the ICD v1.1)*/
#define MAX_NUMBER_MFLEX_IN_MACK_BLOCK 13


/**
 * @def TEST_CRYPTO_FUNCTION
 * @brief If TEST_CRYPTO_FUNCTION is enabled, the code to test the cryptographic functions is enabled
 * */
#define TEST_CRYPTO_FUNCTION




int SHA256Native(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf);


int SHA256InitUpdateFinalPattern(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf);

int EVP_HASH(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf,const EVP_MD *function);
int EVP_HMAC(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf,const EVP_MD *function, const unsigned char *key,size_t keySize);


int HMACNative(unsigned char* ibuf, size_t ibufSize,unsigned char* obuf,const EVP_MD *function, const unsigned char *key,size_t keySize);


int CMAC_AES_function(unsigned char* ibuf,size_t ibufSize,unsigned char* obuf, const unsigned char *key,size_t KSBytes);

int ECDSA(int nid, int generateKeyFLag, unsigned char *digest,int digestLenght);

int EVP_ECDSA(unsigned char *ibuf,size_t ibufSize,int nid,const EVP_MD *function, int generateKeyFLag,int digestLenght);


/*Following functions are borrowed from bearssl library
 * (source code available at https://git.lecbna.org/daminetreg/xxhr/commit/088733e177c04c950303c0d04e55153969436117?view=parallel)*/
/*Decode the ASN1/DER encoded signature to raw binary*/
size_t br_ecdsa_asn1_to_raw(void *sig, size_t sig_len);
/*Encode the raw binary signature to ASN1/DER encoded*/
size_t br_ecdsa_raw_to_asn1(void *sig, size_t sig_len);

/*
 * Compute ASN.1 encoded length for the provided integer. The ASN.1
 * encoding is signed, so its leading bit must have value 0; it must
 * also be of minimal length (so leading bytes of value 0 must be
 * removed, except if that would contradict the rule about the sign
 * bit).*/
size_t _opt_libs_BearSSL_src_ec_ecdsa_rta_c_asn1_int_length(const unsigned char *x, size_t xlen);



/*Generation of TESLA key K(m) from K(m+1)
 * Knext: Received TESKA key K(m+1)
 * KnextNumOfElem: size of Knext (bites)
 * GST_SF: Galileo System Time of the TESLA key to be generated K(m)
 * GST_SFNumOfElem:
 */

/**
 * @brief Generation of TESLA key K(m) from K(m+1): one step of the TESLA chain
 *
 * @param Knext is  the received TESLA key K(m+1)
 * @param KnextNumOfElem is the size of Knext (bites)
 * @param GST_SF is the Galileo System Time of the TESLA key to be generated K(m)
 * @param GST_SFNumOfElem is the size of GST_SF (bites)
 * @param alpha is a random pattern
 * @param alphaNumOfElem is the size of alpha (bites)
 * @param P3 is the number of padding bits (0 to 7) set to zero in order to fit an integer number of bytes
 * @param function is the Hash Function to be used
 * @param concatenatedInput is the array where the concatenated inputs have to be written
 * @param concatenatedInputRef is the reference concatenatedInput, just for debug purposes
 * @param hashedInput is the hashed concatenatedInput
 * @param hashedInputRef is the reference hashedInput, just for debug purposes
 * @param Km is the generated TESLA key K(m)
 *
 * @return 0 if successful
 */
#ifdef DEBUG
int TESLAKeyGenerationAndVerification(unsigned char* Knext,size_t KnextNumOfElem,unsigned char* GST_SF,size_t GST_SFNumOfElem,unsigned char* alpha,size_t alphaNumOfElem,unsigned int P3,const EVP_MD *function,unsigned char* concatenatedInput, unsigned char* concatenatedInputRef,unsigned char* hashedInput, size_t hashedInputSize,unsigned char* hashedInputRef, unsigned char* Km);
#else
int TESLAKeyGenerationAndVerification(unsigned char* Knext,size_t KnextNumOfElem,unsigned char* GST_SF,size_t GST_SFNumOfElem,unsigned char* alpha,size_t alphaNumOfElem,unsigned int P3,const EVP_MD *function,unsigned char* concatenatedInput, unsigned char* hashedInput, size_t hashedInputSize, unsigned char* Km);
#endif



/**
 * @brief One step of TESLA chain verification
 *
 * @param rxOsnma is the pointer to the OSNMA data structure
 * @param indexKey is the index of key in the TESLA chain
 * @param P3 is the number of Padding Bits (0 to 7) as per Section 5.4 at pag. 25 of ICD v1.1. In the current version, P3=0, but it's better to keep it as a reminder in case of changes in future releases.
 * @param blockIdx is the index of the current MACK BLOCK
 * @param i is the current index of the TESLA chain
 * @param enableLog is the flag enabling or not the log of the TESLA key on file
 *
 * @return 0 if successful
 */
void oneStepOfTESLAChainVerification(TOsnmaData *rxOsnma,int indexKey,unsigned int P3,int blockIdx,int i,int enableLog);


/**
 * @brief One step of TESLA chain verification called by TESLAKeyVerificationThreadFunction
 *
 * @param teslaKeyVerificationThFuncParam is the pointer to the thread structure
 * @param enableLog is the flag enabling or not the log of the TESLA key on file
 * @return NULL
 */
void oneStepOfTESLAChainVerificationThread(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam,int enableLog);

/**
 * @brief TESLA key verification: the number of steps of the TESLA chain to be executed depends on the index of the key to be verified and the index of the last key verified.
 *
 * @param rxOsnma is the pointer to the OSNMA data structure
 * @param key is key to be verified
 * @param prnA is the PRN (in OSNMA convention) that transmitted the key
 * @param indexKeyToStart is the index of key in the TESLA chain to be verified, i.e. the index of the key to start with
 * @param indexKeyToEnd is the index of key in the TESLA chain to end with
 * @param P3 is the number of Padding Bits (0 to 7) as per Section 5.4 at pag. 25 of ICD v1.1. In the current version, P3=0, but it's better to keep it as a reminder in case of changes in future releases.
 * @param blockIdx is the index of the current MACK BLOCK
 *
 * @return 0 if successful
 */
int TESLAKeyVerification(TOsnmaData *rxOsnma,unsigned char * key,int prnA,int indexKeyToStart,int indexKeyToEnd, unsigned int P3,int blockIdx);


/**
 * @brief Initialize the structure used by the thread in charge of performing the TESLA key verification (in case the number of TESLA chain steps to be performed is too high for a real-time execution
 *
 * @param teslaKeyVerificationThFuncParam is the pointer to the structure used by the thread
 * @param rxOsnma is the pointer to the OSNMA data structure
 * @param key is key to be verified
 * @param prnA is the PRN (in OSNMA convention) that transmitted the key
 * @param wnMackStart is the start time of the current MACK Block (WN)
 * @param towMackStart is the start time of the current MACK Block (TOW, [s])
 * @param indexKey is the index of key in the TESLA chain to be verified
 * @param blockIdx is the index of the MACK block
 * @return NULL
 */
void InitTESLAKeyVerificationThreadFunction(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam,TOsnmaData *rxOsnma, unsigned char * key, int prnA, int wnMackStart, int towMackStart, int indexKey,int blockIdx);

/**
 * @brief TESLA key verification allocated to a separate thread. It is executed only in case the number of steps of the TESLA chain to be performed is too high for real-time running.
 *
 * @param teslaKeyVerificationThFuncParam is the pointer to the thread structure
 * @return NULL
 */
void *TESLAKeyVerificationThreadFunction(TTeslaKeyVerificationThreadFunctionParametersStruct *teslaKeyVerificationThFuncParam);

/**
 * @brief Verification of the DS field (Digital Signature) of the DSM-ROOT, using the Public Key as raw binary
 *
 * @param M is Message signed
 * @param MSize is the size of M
 * @param NPKT is the New Public Key Type
 * @param DS is the ASN1-DER encoded Digital Signature field in the DSM-ROOT
 * @param pubKey is the Public Key (raw binary format!!)
 * @param pubKeySize is the size of pubKey
 *
 * @return 0 if successful
 */
int DSM_KROOT_SignatureVerification(unsigned char *M,size_t MSize,int NPKT, unsigned char * DS,unsigned char *pubKey,size_t pubKeySize);


/**
 * @brief Verification of the DS field (Digital Signature) of the DSM-ROOT, reading the PEM format file (?) of Public Key from a server (?)
 *
 * @param M is Message signed
 * @param MSize is the size of M
 * @param NPKT is the New Public Key Type
 * @param DS is the ASN1-DER encoded Digital Signature field in the DSM-ROOT
 * @param filename is the path+name to the PEM file containing the Public Key
 *
 * @return 0 if successful
 */
int DSM_KROOT_SignatureVerification_fromPEM(unsigned char *M,size_t MSize,int NPKT, unsigned char * DS, char *filename);

/**
 * @brief Conversion of the Public Key from PEM format file to internal format EVP_PKEY
 *
 * @param filename is the path+name to the PEM file containing the Public Key
 *
 * @return evp_key that is the pointer to a structure containing the Public Key (internal format)
 */
EVP_PKEY * PublicKey_PEMToEVPKEY(FILE *PEMFile);


/**
 * @brief Conversion of the Public Key from Raw Binary to internal format EVP_PKEY
 *
 * @param nid is the Elliptic Curve ID
 * @param pubKey is the Public Key (raw binary format!!)
 * @param pubKeySize is the size of pubKey
 * @param evp_key is the pointer to a structure containing the Public Key (internal format)
 *
 * @return 0 if successful
 */
int PublicKey_BNToEVPKEY(int nid, unsigned char *pubKey,size_t pubKeySize,EVP_PKEY *evp_key);


/**
 * @brief Conversion of the Public Key from Raw Binary to internal format EVP_PKEY
 *
 * @param npkt is the New Public Key Type as in the DSM-PKR section.
 * @param pubKey is the Public Key (raw binary format!!)
 * @param pubKeySize is the size of pubKey
 * @param evp_key is the pointer to a structure containing the Public Key (internal format)
 *
 * @return 0 if successful
 */
int PublicKey_PkrToEVPKEY(TNewPublicKeyType npkt, unsigned char *pubKey,size_t pubKeySize,EVP_PKEY *evp_key);

/**
 * @brief Verification of the DS field (Digital Signature) of the DSM-ROOT, using the Public Key expressed as EVP_KEY type
 *
 * @param dsmBlocks is the pointer to the structure containing the DSM data (EXCLUDED the headers)
 * @param rxOsnma is the pointer to the structure containing the OSNMA data
 * @param osnmaPars is the pointer to the structure containing the OSNMA parameters
 * @param chOsnma is the pointer to the OSNMA variable of the channel
 *
 * @return 0 if successful
 */
int DSM_KROOT_SignatureVerification_fromEVPKEY(unsigned char *dsmBlocks,TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars,TChannelOsnmaVariables *chOsnma);


/**
 * @brief Verification of the Padding Field P1 of the DSM-ROOT. (This verification is optional)
 *
 * @param dsmBlocks is the pointer to the structure containing the DSM data (EXCLUDED the headers)
 * @param rxOsnma is the pointer to the structure containing the OSNMA data
 * @param osnmaPars is the pointer to the structure containing the OSNMA parameters
 *
 * @return 0 if successful
 */
int DSM_KROOT_P1Verification(unsigned char *dsmBlocks,TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars);




/**
 * @brief Conversion of the Public Key from PEM format (string) to raw binary
 *
 * @param pubKeyPEMEncapsulated is Public Key in PEM format (complete with Header and Footer)
 * @param pubKeyPEMEncapsulatedSize is the size of pubKeyPEMEncapsulated
 * @param pubKeyPEM is Public Key in PEM format without Header and Footer)
 * @param pubKeyPEMSize is the size of pubKeyPEM
 * @param pubKeyDecoded is Public Key in raw binary format
 * @param pubKeyUncompressedSize is the size of pubKeyDecoded
 *
 * @return 0 if successful
 */
int PEM_ASN1DER_decoding(unsigned char *pubKeyPEMEncapsulated,size_t pubKeyPEMEncapsulatedSize, unsigned char *pubKeyPEM,size_t pubKeyPEMSize, unsigned char *pubKeyDecoded,size_t pubKeyUncompressedSize);

/**
 * @brief MAC verification
 *
 * @param m is message to be authenticated
 * @param mSize is the size of m (bytes)
 * @param CIDKR_NMACK_HF_MF is one byte, consisting in CIDKR (2 bits): Chain ID of KROOT; NMACK (2 bits): Number of MACK blocks; HF (2 bits): Hash Function; MF (2 bits): MAC Function
 * @param mHashed is the message authentication code generated from m
 * @param mHashedRef is the reference keyed hashed value of m (for debug purposes only)
 * @param K is the TESLA key used for the MAC generation. Note that K may not be the key transmitted in the MACK block to which the MAC belongs!!
 * @param KSBytes is the size of key K (bytes)
 * @param KS_MS is one byte, consisting in KS (4 bits): Key Size and MS (4 bits): MAC Size
 * @param tag is the truncated mHashed
 * @param MAC is the truncated message authentication code transmitted in the signal
 * @return 0 if successful
 */
int MACVerificationOld(unsigned char *m, size_t mSize, unsigned char *CIDKR_NMACK_HF_MF, unsigned char *mHashed, unsigned char *mHashedRef,unsigned char* K,size_t KSBytes, unsigned char *KS_MS, unsigned char *tag, unsigned char *MAC);


/**
 * @brief MAC verification
 *
 * @param rxOsnma is the poiter to the OSNMA data
 * @param mackPtr is the pointer to the MACK element in the MACK list
 * @param macPtr is the pointer to the MAC list of the current element in the MACK list
 * @param K is the key  (of Galileo PRN1 or Galileo PRN19 in case of MACK offsetting )to be used for the MAC verification
 * @param MAC is the MAC/MAC0 received to be compared with the one computed
 * @param flagMAC0 is TRUE_FLAG if the MAC0 has to be verified, FALSE_FLAG if the MAC has to be verified
 *
 * @return VALIDATED if successful, NOT_VALID otherwise
 */
TValidationStatus MACVerification(TOsnmaData *rxOsnma, TMackList *mackPtr,TMacList *macPtr,unsigned char* K, unsigned char *MAC, int flagMAC0);

/**
 * @brief MACSEQ verification
 *
 * @param rxOsnma is the poiter to the OSNMA data
 * @param mackPtr is the pointer to the MACK element in the MACK list
 * @param macPtr is the pointer to the MAC list of the current element in the MACK list
 * @param K is the key  (of Galileo PRN1 or Galileo PRN19 in case of MACK offsetting )to be used for the MAC verification
 * @param MACSEQ is the MACSEQ received to be compared with the one computed
 *
 * @return VALIDATED if successful, NOT_VALID otherwise
 */
TValidationStatus MACSEQVerification(TOsnmaData *rxOsnma, TMackList *mackPtr,TMacList *macPtr,unsigned char* K, unsigned char *MACSEQ);

/**
 * @brief MAC verification of all MAC to be verified in the MACK LIST
 *
 * @param ngeneCfg is the pointer to the receiver configuration structure
 * @param ngene is the pointer to the receiver structure
 * @return the number of validated MACs
 */
int MACVerificationAll(TNGeneConfiguration *ngeneCfg, TNGeneData *ngene);

unsigned char* FindKeyForMacVerification(TOsnmaParameters *pars,TOsnmaData *rxOsnma,TMackList *mackPtr,int wnMackStart,int towMackStart);



/**
 * @brief MACSEQ verification
 *
 * @param m is message to be authenticated
 * @param mSize is the size of m (bytes)
 * @param CIDKR_NMACK_HF_MF is one byte, consisting in CIDKR (2 bits): Chain ID of KROOT; NMACK (2 bits): Number of MACK blocks; HF (2 bits): Hash Function; MF (2 bits): MAC Function
 * @param mHashed is the message authentication code generated from m
 * @param mHashedRef is the reference keyed hashed value of m (for debug purposes only)
 * @param K is the TESLA key used for the MAC generation. Note that K may not be the key transmitted in the MACK block to which the MAC belongs!!
 * @param KSBytes is the size of key K (bytes)
 * @param MACSEQGenerated is the truncated mHashed
 * @param MACSEQ is the truncated message authentication code transmitted in the signal
 * @return 0 if successful
 */
int MACSEQVerificationForTestVector(unsigned char *m, size_t mSize, unsigned char *CIDKR_NMACK_HF_MF, unsigned char *mHashed, unsigned char *mHashedRef,unsigned char* K,size_t KSBytes, unsigned char *MACSEQGenerated, unsigned char *MACSEQ);


/**
 * @brief Public Key Verification through the Merkle Tree
 *
 * @param IT0 is the first Intermediate Node received
 * @param IT1 is the second Intermediate Node received
 * @param IT2 is the third Intermediate Node received
 * @param IT3 is the fourth Intermediate Node received
 * @param MID is the Message ID in the DSM-PKR, identifying which leaf of the Merkle Tree is provided (see Table 16 pag 15 of ICD v1.1)
 * @param leaf is the leaf of the Merkle Tree, obtained concatenating NPKT,NPKID and NPK as indicated at pag. 24 of ICD v1.1
 * @param leafSizeBytes is the size (bytes) leaf. It is variable, depending on the size of NPK
 * @param nodeMatrix is the matrix of the Merkle Tree nodes
 * @param nodesConcatenated is the concatenation of two nodes, before being hashed, as indicated at pag. 24 of ICD v1.1
 * @param nodesConcatenatedBytes is the size (bytes) of nodesConcatenated. It is fixed (64 B), since the size of one node is 32 B in the current version
 * @param rootMerkleTree is the Merkle Tree Root, loaded in the receiver from factory or retrieved from the OSNMA server
 * @return 0 if successful
 */
int MerkleTreePublicKeyVerificationTestVector(unsigned char *IT0,unsigned char *IT1,unsigned char *IT2,unsigned char *IT3,int MID, unsigned char *leaf,size_t leafSizeBytes, unsigned char ***nodeMatrix,unsigned char *nodesConcatenated, size_t nodesConcatenatedBytes, unsigned char *rootMerkleTree);

/**
 * @brief Public Key Verification through the Merkle Tree
 *
 * @param rxOsnma is the poiter to the OSNMA data
 * @param pars is the pointer to the structure containing the OSNMA parameters
 * @param dsmBlocks is the pointer to the structure containing the DSM data (EXCLUDED the headers)
 * @return 0 if successful
 */
int MerkleTreePublicKeyVerification(TOsnmaData *rxOsnma, TOsnmaParameters *pars, unsigned char *dsmBlocks);

int DSM_PKR_P2Verification(TOsnmaData *rxOsnma,TOsnmaParameters *osnmaPars);


#endif /* OSNMA_CRYPTOFUNCTIONS_H_ */
