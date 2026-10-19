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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "chimeraCryptoFunctions.h"
#include "cryptoFunctions.h"
#include "ioFunctions.h"
#include "osnmaUtilities.h"
#include "mackList.h"

//***********************************************CHIMERA**************************


void ReadHexBytes(char *hexArray,unsigned char *byteArray,int num)
{
	int rdIdx = 0;
    
    for (rdIdx= 0; rdIdx < num; rdIdx++)
    {
        sscanf(&(hexArray[2*rdIdx]),"%2hhX",&(byteArray[rdIdx]));
    }
}


int InitChimeraParameters(TChimeraParameters *chimeraPars,char *directory)
{
    char stringPk[]="ChimeraPublicKey.pem";
    char stringFMK[]="dbg_GPS_L1C_fastMarkerKeys.txt";


    //**
    chimeraPars->publicKeyFilename[0] = '\0';
    strcat(chimeraPars->publicKeyFilename, directory);
    strcat(chimeraPars->publicKeyFilename, stringPk);
    if(CheckFileExistence(chimeraPars->publicKeyFilename)==FALSE_FLAG)
	{
		//SwrxLog(NGENE_LOG_SEVERITY_WARNING,"CHIMERA: specified Public Key file does not exist.");
        printf("CHIMERA: specified Public Key file does not exist!\n");
        return FAILURE_RETURN;
	}
    chimeraPars->DSSizeBits=448;
    chimeraPars->DSSizeBytes=56;
    chimeraPars->MSizeBits=7902;
    chimeraPars->MSizeBytes=988;
    chimeraPars->DSHashedSizeBytes=64;
    chimeraPars->markerKeySizeBytes=32;
    chimeraPars->markerKeySizeBits=chimeraPars->markerKeySizeBytes*8;
    chimeraPars->textSizeBytes=16;
    chimeraPars->zCountSizeBytes=4;
    
    chimeraPars->fastMarkerKeysFilename[0] = '\0';
    strcat(chimeraPars->fastMarkerKeysFilename, directory);
    strcat(chimeraPars->fastMarkerKeysFilename, stringFMK);
    if(CheckFileExistence(chimeraPars->fastMarkerKeysFilename)==FALSE_FLAG)
	{
		//SwrxLog(NGENE_LOG_SEVERITY_WARNING,"CHIMERA: specified Public Key file does not exist.");
        printf("CHIMERA: specified Fast Marker Keys file does not exist!\n");
        return FAILURE_RETURN;
	}

    
    
    return SUCCESS_RETURN;
}


int InitChimeraData(TChimeraData *chimera)
{
    int prn;
    int indexMsg,indexKey;
    int pattern;
    FILE *PEMFile;
    int DSMaxSize;
    /*READ THE PUBLIC KEY FROM FILE*/
    chimera->evp_key=NULL;
    PEMFile=fopen(chimera->chimeraPars.publicKeyFilename,"r");
    if(PEMFile!=NULL) 
    {
        chimera->evp_key=PublicKey_PEMToEVPKEY(PEMFile);
        fclose(PEMFile);
        DSMaxSize=EVP_PKEY_size(chimera->evp_key);
        if(DSMaxSize!=64)
        {
            printf("Error: the public key has not the right size\n");
            return FAILURE_RETURN;
        }
        else
        {
            chimera->function=EVP_sha224();
            //hashSize=SHA224_DIGEST_LENGTH;
            //nid=NID_secp224r1;
        }
        
        //chimera->osnmaPars.DSSizeBytes=chimera->osnmaPars.DSSizeBits/8;
    } 
    else 
    {
        SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot read the public key from the specified file");
        return FAILURE_RETURN;
    }
    
    /*Allocate Memory for variables to verify the Digital Signature*/
    chimera->M=(unsigned char*)calloc(chimera->chimeraPars.MSizeBytes,sizeof(unsigned char));
    chimera->DS=(unsigned char*)calloc(chimera->chimeraPars.DSSizeBytes,sizeof(unsigned char));
    chimera->DSHashed=(unsigned char*)calloc(chimera->chimeraPars.DSHashedSizeBytes,sizeof(unsigned char));
    
    chimera->slowMarkerKey=(unsigned char**)calloc(32,sizeof(unsigned char*));
    for(prn=0;prn<32;prn++)
    {
        chimera->slowMarkerKey[prn]=(unsigned char*)calloc(chimera->chimeraPars.markerKeySizeBytes,sizeof(unsigned char));
    }
    chimera->fastMarkerKeyList=(unsigned char**)calloc(MAX_NUMBER_OF_FAST_KEYS_IN_ONE_EPOCH,sizeof(unsigned char*));
    for(indexKey=0;indexKey<MAX_NUMBER_OF_FAST_KEYS_IN_ONE_EPOCH;indexKey++)
    {
        chimera->fastMarkerKeyList[indexKey]=(unsigned char*)calloc(chimera->chimeraPars.markerKeySizeBytes,sizeof(unsigned char));
    }
    chimera->fastMarkerKey=(unsigned char*)calloc(chimera->chimeraPars.markerKeySizeBytes,sizeof(unsigned char));

    

    chimera->plaintext=(unsigned char*)calloc(chimera->chimeraPars.textSizeBytes,sizeof(unsigned char));
    chimera->ciphertext=(unsigned char*)calloc(chimera->chimeraPars.textSizeBytes,sizeof(unsigned char));

    chimera->zCount=(unsigned char**)calloc(32,sizeof(unsigned char*));
    for(indexMsg=0;indexMsg<NUMBER_OF_MESSAGE_IN_A_EPOCH;indexMsg++)
    {
        chimera->zCount[indexMsg]=(unsigned char*)calloc(chimera->chimeraPars.zCountSizeBytes,sizeof(unsigned char));
    }


    chimera->markerIndicatorBlock=(unsigned char*)calloc(chimera->chimeraPars.textSizeBytes*MAX_NUMBER_OF_BLOCK_FOR_INDICATOR,sizeof(unsigned char));
    chimera->markerValueBlock=(unsigned char*)calloc(chimera->chimeraPars.textSizeBytes*MAX_NUMBER_OF_BLOCK_FOR_VALUE,sizeof(unsigned char));


    chimera->sectorPattern=(unsigned char**)calloc(SECTOR_PATTERNS_ENTRIES,sizeof(unsigned char*));
    for(pattern=0;pattern<SECTOR_PATTERNS_ENTRIES;pattern++)
    {
        chimera->sectorPattern[pattern]=(unsigned char*)calloc(SECTOR_PATTERNS_ENTRIES,sizeof(unsigned char));
    }
    
    if(LoadSectorPatternTable(chimera)==FAILURE_RETURN)
    {
        return FAILURE_RETURN;
    }
    
    chimera->markerLocationListPerSector=(unsigned char*)calloc(MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION,sizeof(unsigned char));
    chimera->occupiedMarkerLocationFlag=(unsigned char*)calloc(MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION,sizeof(unsigned char));

    chimera->markerLocationListPerPilot=(unsigned char*)calloc(MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION*10,sizeof(unsigned char));
    chimera->markerValuesPerPilot=(unsigned char*)calloc((int)ceil(MAX_NUMBER_OF_MARKER_FOR_PILOT/8.0),sizeof(unsigned char));

    chimera->chips=(unsigned char*)calloc(MAX_NUMBER_OF_MARKER_FOR_PILOT,sizeof(unsigned char));

    #ifdef CHIMERA_PROFILING
    InitProfilingSupport(&(chimera->dsProfiling),"Digital Signature Verification"); 
    InitProfilingSupport(&(chimera->slowKeyProfiling),"Slow Marker Key Generation"); 
    InitProfilingSupport(&(chimera->oneCiphertextProfiling),"One 128-bit Ciphertext Generation"); 
    InitProfilingSupport(&(chimera->tenMarkerLocationProfiling),"Ten Marker Indicators (40 bits) Generation");
    InitProfilingSupport(&(chimera->tenMarkerValueProfiling),"Ten Marker Values (290 bits) Generation"); 
    InitProfilingSupport(&(chimera->markerGenerationPerPilotProfiling),"Markers Generation per Pilot Symbol"); 
    InitProfilingSupport(&(chimera->markerLocationGenerationPerPilotProfiling),"Markers Location Generation per Pilot Symbol"); 
    InitProfilingSupport(&(chimera->markerValueGenerationPerPilotProfiling),"Markers Value Generation per Pilot Symbol"); 
    #endif


    return SUCCESS_RETURN;
}

void DeinitChimeraData(TChimeraData *chimera)
{
    int prn;
    int indexMsg,indexKey;
    int pattern;
    
    EVP_PKEY_free(chimera->evp_key);
    free(chimera->M);
    free(chimera->DS);
    free(chimera->DSHashed);
    for(prn=0;prn<32;prn++)
    {
        free(chimera->slowMarkerKey[prn]);
    }
    free(chimera->slowMarkerKey);
    for(indexKey=0;indexKey<MAX_NUMBER_OF_FAST_KEYS_IN_ONE_EPOCH;indexKey++)
    {
        free(chimera->fastMarkerKeyList[indexKey]);
    }
    free(chimera->fastMarkerKeyList);
    free(chimera->fastMarkerKey);
    free(chimera->plaintext);
    free(chimera->ciphertext);
    free(chimera->markerIndicatorBlock);
    free(chimera->markerValueBlock);
    for(indexMsg=0;indexMsg<NUMBER_OF_MESSAGE_IN_A_EPOCH;indexMsg++)
    {
        free(chimera->zCount[indexMsg]);
    }
    free(chimera->zCount);
    for(pattern=0;pattern<SECTOR_PATTERNS_ENTRIES;pattern++)
    {
        free(chimera->sectorPattern[pattern]);
    }
    free(chimera->sectorPattern);
    
    free(chimera->markerLocationListPerSector);
    free(chimera->occupiedMarkerLocationFlag);
    free(chimera->markerLocationListPerPilot);
    free(chimera->markerValuesPerPilot);
    free(chimera->chips);
    
    #ifdef CHIMERA_PROFILING
    ReportProfilingData(&(chimera->dsProfiling));
    ReportProfilingData(&(chimera->slowKeyProfiling));
    ReportProfilingData(&(chimera->oneCiphertextProfiling));
    ReportProfilingData(&(chimera->tenMarkerLocationProfiling));
    ReportProfilingData(&(chimera->tenMarkerValueProfiling));
    ReportProfilingData(&(chimera->markerGenerationPerPilotProfiling));
    ReportProfilingData(&(chimera->markerLocationGenerationPerPilotProfiling));
    ReportProfilingData(&(chimera->markerValueGenerationPerPilotProfiling));
    #endif

}



int Digital_SignatureVerification_fromEVPKEY(unsigned char *epoch,TChimeraData *chimera, int *toi)
{
    unsigned char pageNumber={0x00};
    int byteIdxInToRead=0;
    int bitIdxInToRead=0;
    int byteIdxOutToRead=0;
    int bitIdxOutToRead=0;
    int byteIdxOut=0;
    int bitIdxOut=0;
    int byteIdxIn=0;
    int bitIdxIn=0;
    int byteIdxOutDS=0;
    int bitIdxOutDS=0;
    int indexMsg;
    int startByte=0;
    int endByte=0;
    int startBit=0;
    int endBit=0;
    unsigned char iTow;
    unsigned char towChar[3];
    int tow;
    
    

    EVP_MD_CTX *evpx = NULL;	/* EVP_MD context */
    unsigned char *signatureASN1DEREncoded = NULL;
    size_t sigLen = 0;
    size_t sigLenReencoded;


    ArrayClean((char *)chimera->M,chimera->chimeraPars.MSizeBytes);
    ArrayClean((char *)chimera->DS,chimera->chimeraPars.DSSizeBytes);

    //**Prepare the message
    for(indexMsg=0;indexMsg<NUMBER_OF_MESSAGE_IN_A_EPOCH;indexMsg++)
    {
        
        //*****
        #ifdef DEBUG
        int i;
        for (i = 883*indexMsg/8; i < (883*indexMsg/8+3); i++) 
        {
            printf("%02X", epoch[i]);
        }
        printf("\n");
        #endif
        
        //******TOI in SUBFRAME 1
        startByte=(BIT_INDEX_TOI_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)/8;
        endByte=(BIT_INDEX_TOI_FIELD_IN_A_MESSAGE+LENGTH_TOI_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)/8;
        startBit=8-(BIT_INDEX_TOI_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)%8;
        endBit=(BIT_INDEX_TOI_FIELD_IN_A_MESSAGE+LENGTH_TOI_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)%8;
        (*toi) = ExtractField(epoch,startByte,endByte,startBit,endBit);
        (*toi) = (*toi) -1;
       
        //******WN in SUBFRAME 2
        ArrayClean((char *)chimera->zCount[indexMsg],chimera->chimeraPars.zCountSizeBytes);
        byteIdxInToRead=(byteIdxIn*8+bitIdxIn+LENGTH_TOI_FIELD_IN_A_MESSAGE)/8;
        byteIdxOutToRead=0;
        bitIdxInToRead=(byteIdxIn*8+bitIdxIn+LENGTH_TOI_FIELD_IN_A_MESSAGE)%8;
        bitIdxOutToRead=0;
        CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,chimera->zCount[indexMsg],LENGTH_WN_FIELD_IN_A_MESSAGE);

        //******ITOW in SUBFRAME 2
        iTow=0;
        byteIdxInToRead=(byteIdxIn*8+bitIdxIn+LENGTH_TOI_FIELD_IN_A_MESSAGE+LENGTH_WN_FIELD_IN_A_MESSAGE)/8;
        byteIdxOutToRead=0;
        bitIdxInToRead=(byteIdxIn*8+bitIdxIn+LENGTH_TOI_FIELD_IN_A_MESSAGE+LENGTH_WN_FIELD_IN_A_MESSAGE)%8;
        bitIdxOutToRead=0;
        CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&iTow,LENGTH_ITOW_FIELD_IN_A_MESSAGE);
        #ifdef DEBUG
        printf("ITOW= %d\n",iTow);
        #endif
        /*Build the TOW as required by Z-count definition, i.e. number of seconds since the start of the GPS Week, expressed as multiple of 1.5 s*/
        tow=(iTow*TWO_HOUR_PERIOD_DURATION+(*toi)*MESSAGE_DURATION)/1.5;
        #ifdef DEBUG
        printf("tow= %d\n",tow);
        #endif
        ArrayClean((char *)towChar,3);
        towChar[2] = tow & 0xFF;
        towChar[1] = (tow >> 8) & 0xFF;
        towChar[0] = (tow >> 16) & 0xFF;

        byteIdxInToRead=0;
        byteIdxOutToRead=1;
        bitIdxInToRead=5;
        bitIdxOutToRead=5;
        CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,towChar,&byteIdxOutToRead,&bitIdxOutToRead,chimera->zCount[indexMsg],19);

        #ifdef DEBUG
        int i;
        for (i = 0; i < 4; i++) 
        {
            printf("%02X", chimera->zCount[indexMsg][i]);
        }
        printf("\n");
        #endif


        CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOut,&bitIdxOut,chimera->M,585);
        byteIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg+BIT_INDEX_START_SUBFRAME3_IN_A_MESSAGE)/8;
        bitIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg+BIT_INDEX_START_SUBFRAME3_IN_A_MESSAGE)%8;
        /**/
        chimera->prn=0;
        byteIdxInToRead=(BIT_INDEX_PRN_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)/8;
        byteIdxOutToRead=0;
        bitIdxInToRead=(BIT_INDEX_PRN_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)%8;
        bitIdxOutToRead=0;
        CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&chimera->prn,8);

        byteIdxInToRead=(BIT_INDEX_PAGE_NUMBER_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)/8;
        byteIdxOutToRead=0;
        bitIdxInToRead=(BIT_INDEX_PAGE_NUMBER_FIELD_IN_A_MESSAGE+NUMBER_OF_BITS_IN_A_MESSAGE*indexMsg)%8;
        bitIdxOutToRead=2;
        CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&pageNumber,6);
        pageNumber=pageNumber & 0x3F;
        //printf("pageNumber = %d\n", pageNumber);

        /**/
        if(pageNumber==8)
        {
            CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOut,&bitIdxOut,chimera->M,38);
            
            //**
            chimera->fastEpochPeriod=0;
            byteIdxInToRead=(byteIdxIn*8+bitIdxIn+18)/8;
            byteIdxOutToRead=0;
            bitIdxInToRead=(byteIdxIn*8+bitIdxIn+18)%8;
            bitIdxOutToRead=7;
            CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&chimera->fastEpochPeriod,1);
            
            chimera->fastDF=0;
            byteIdxInToRead=(byteIdxIn*8+bitIdxIn+19)/8;
            byteIdxOutToRead=0;
            bitIdxInToRead=(byteIdxIn*8+bitIdxIn+19)%8;
            bitIdxOutToRead=5;
            CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&chimera->fastDF,3);
            
            chimera->slowDF=0;
            byteIdxInToRead=(byteIdxIn*8+bitIdxIn+22)/8;
            byteIdxOutToRead=0;
            bitIdxInToRead=(byteIdxIn*8+bitIdxIn+22)%8;
            bitIdxOutToRead=5;
            CopyBitsUnaligned(byteIdxInToRead,bitIdxInToRead,epoch,&byteIdxOutToRead,&bitIdxOutToRead,&chimera->slowDF,3);
            
            //**
            
            byteIdxIn=(byteIdxIn*8+bitIdxIn+38)/8;
            bitIdxIn=(byteIdxIn*8+bitIdxIn+38)%8;
            CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOutDS,&bitIdxOutDS,chimera->DS,212);
            byteIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*(indexMsg+1))/8;
            bitIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*(indexMsg+1))%8;
        }
        else
        {
            if(pageNumber==9)
            {
                CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOut,&bitIdxOut,chimera->M,14);
                byteIdxIn=(byteIdxIn*8+bitIdxIn+14)/8;
                bitIdxIn=(byteIdxIn*8+bitIdxIn+14)%8;
                CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOutDS,&bitIdxOutDS,chimera->DS,236);
            }
            else
            {
                CopyBitsUnaligned(byteIdxIn,bitIdxIn,epoch,&byteIdxOut,&bitIdxOut,chimera->M,250);
                byteIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*(indexMsg+1))/8;
                bitIdxIn=(NUMBER_OF_BITS_IN_A_MESSAGE*(indexMsg+1))%8;
            }
        }
    }
    
    #ifdef DEBUG
    int i;
    for (i = 0; i < chimera->chimeraPars.MSizeBytes; i++) 
    {
    printf("%02X", chimera->M[i]);
    }
    printf("\n");
    for (i = 0; i < chimera->chimeraPars.DSSizeBytes; i++) 
    {
        printf("%02X", chimera->DS[i]);
    }
    printf("\n");
    #endif
    

    /* EVP_MD context */
    if ((evpx = EVP_MD_CTX_create()) == NULL) {
        handleErrors();
    }

    /* Return the maximum number of bytes required for the signature */
    sigLen = EVP_PKEY_size(chimera->evp_key);


    signatureASN1DEREncoded = (unsigned char *) OPENSSL_malloc(sigLen);
    if (signatureASN1DEREncoded == NULL) {
        handleErrors();
    }
    sigLenReencoded=chimera->chimeraPars.DSSizeBytes;
    memcpy(signatureASN1DEREncoded, chimera->DS, chimera->chimeraPars.DSSizeBytes);
    sigLenReencoded=br_ecdsa_raw_to_asn1(signatureASN1DEREncoded, sigLenReencoded);

    if(1 != EVP_DigestVerifyInit(evpx,NULL ,chimera->function,NULL,chimera->evp_key))
        handleErrors();

    if(1 != EVP_DigestVerifyUpdate(evpx, chimera->M, chimera->chimeraPars.MSizeBytes))
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



    return SUCCESS_RETURN;
}


void SlowChannelMarkerKeyGeneration(unsigned char *digitalSignature,TChimeraData *chimera, int prn)
{
    int byteIdxOut=0;
    int bitIdxOut=0;
    
    ArrayClean((char *)chimera->DSHashed,chimera->chimeraPars.DSHashedSizeBytes);
    ArrayClean((char *)chimera->slowMarkerKey[prn-1],chimera->chimeraPars.markerKeySizeBytes);


    EVP_HASH(digitalSignature,chimera->chimeraPars.DSSizeBytes, chimera->DSHashed,EVP_sha512());
    CopyBitsUnaligned(chimera->chimeraPars.markerKeySizeBytes,0,chimera->DSHashed,&byteIdxOut,&bitIdxOut,chimera->slowMarkerKey[prn-1],chimera->chimeraPars.markerKeySizeBits);

    
    #ifdef DEBUG
    int i;
    for (i = 0; i < chimera->chimeraPars.markerKeySizeBytes; i++) 
    {
        printf("%02X", chimera->slowMarkerKey[prn-1][i]);
    }
    printf("\n");
    #endif
}


//****************************************************************
int AESEncrypt(unsigned char *plaintext, int textLength, unsigned char *markerKey, unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;
    
    ArrayClean((char *)ciphertext,textLength);


    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

    /*
     * Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, markerKey, NULL))
        handleErrors();
        
            /*Set padding to 0, so that ciphertext length is exacly the same as plaintext length*/
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, textLength))
        handleErrors();
    ciphertext_len = len;
    
    /*
     * Finalise the encryption.     
     * If padding is disabled then EVP_EncryptFinal_ex() will not encrypt any more data and
    it will return an error if any data remains in a partial block:
    that is if the total data length is not a multiple of the block size.*/
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        handleErrors();
    ciphertext_len += len;
    
    //***********
    /*Refer below DOC NOTE from openssl:-
    If padding is disabled then EVP_EncryptFinal_ex() will not encrypt any more data and
    it will return an error if any data remains in a partial block:
    that is if the total data length is not a multiple of the block size.*/
    /*if(EVP_EncryptFinal_ex(e_ctx, ciphertext+c_len, &f_len) == EVP_FAILURE)
    return rc;*/
    //EVP_CIPHER_CTX_free(e_ctx);
    //*len = c_len + f_len;
    //cout<<"cipher len::"<<c_len<<endl;//ciphering length check
    //return 0;
    
    //***********

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}


int AESDecrypt(unsigned char *ciphertext, int ciphertextLen, unsigned char *markerKey, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int plaintextLen;


    ArrayClean((char *)plaintext,ciphertextLen);

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

    /*
     * Initialise the decryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits
     */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, markerKey, NULL))
        handleErrors();

    /*Set padding to 0, so that ciphertext length is exacly the same as plaintext length*/
    EVP_CIPHER_CTX_set_padding(ctx, 0);

    /*
     * Provide the message to be decrypted, and obtain the decrypted output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertextLen))
        handleErrors();
    plaintextLen = len;

    /*
     * Finalise the decryption.
     * If padding is disabled then EVP_EncryptFinal_ex() will not decrypt any more data and
    it will return an error if any data remains in a partial block:
    that is if the total data length is not a multiple of the block size.*/
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
        handleErrors();
    plaintextLen += len;

    //***********
    /*Refer below DOC NOTE from openssl:-
    If padding is disabled then EVP_EncryptFinal_ex() will not encrypt any more data and
    it will return an error if any data remains in a partial block:
    that is if the total data length is not a multiple of the block size.*/
    /*if(EVP_EncryptFinal_ex(e_ctx, ciphertext+c_len, &f_len) == EVP_FAILURE)
    return rc;*/
    //EVP_CIPHER_CTX_free(e_ctx);
    //*len = c_len + f_len;
    //cout<<"cipher len::"<<c_len<<endl;//ciphering length check
    //return 0;

    //***********

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintextLen;
}


void BuildPlaintext(TChimeraData *chimera, TMarkerFunctionEnum function, unsigned char ciphertextBlockIndex, int pilotSymbolIndex, int indexMsg,TChimeraChannelEnum channel)
{
    
    unsigned char zeroByte={0x00};
    unsigned char oneByte={0x01};
    unsigned char fillBytes[]="AF";
    
    ArrayClean((char *)chimera->pilotSymbolIndexCharArray,2);
    chimera->pilotSymbolIndexCharArray[1] = pilotSymbolIndex & 0xFF;
    chimera->pilotSymbolIndexCharArray[0] = (pilotSymbolIndex >> 8) & 0xFF;
    /*Src*/
    memcpy(chimera->plaintext, &zeroByte, 1);
    /*PRN*/
    memcpy(&chimera->plaintext[1], &chimera->prn, 1);
    /*Lnk for L1*/
    memcpy(&chimera->plaintext[2], &zeroByte, 1);
    /*Z-Count: DA CAPIRE*/
    memcpy(&chimera->plaintext[3], chimera->zCount[indexMsg], 4);
    #ifdef DEBUG
    int i;
    for (i = 0; i < 4; i++) 
    {
        printf("%02X", chimera->zCount[indexMsg][i]);
    }
    printf("\n");
    #endif
    
    /*Prd: Period is either 00000000 (1.5 seconds) or 00000001 (6 seconds). This field is set to 00000000 for processing slow channel Chimera*/
    /*S/F: Slow/fast are respectively 00000000 and 00000001.*/
    if (channel==SLOW_CHANNEL)
    {
        memcpy(&chimera->plaintext[7], &zeroByte, 1);
        memcpy(&chimera->plaintext[8], &zeroByte, 1);
    }
    else /*Fast Channel*/
    {
        memcpy(&chimera->plaintext[7], &chimera->fastEpochPeriod, 1);
        memcpy(&chimera->plaintext[8], &oneByte, 1);
    }


    /*Duty Factor DF read from Subframe 3 Page 8 */
    memcpy(&chimera->plaintext[9], &chimera->slowDF, 1);
    /*Function: 00000000 for marker location and 00000001 for marker values. */
    if(function==MARKER_LOCATION)
    {
        memcpy(&chimera->plaintext[10], &zeroByte, 1);
    }
    else /*MARKER_VALUE*/
    {
        memcpy(&chimera->plaintext[10], &oneByte, 1);
    }
    /* Index*/
    memcpy(&chimera->plaintext[11], &ciphertextBlockIndex, 1);
    /* Symbol Count Cntr: index of L1C pilot overlay symbols from start of 3-minute Chimera epoch*/
    memcpy(&chimera->plaintext[12], chimera->pilotSymbolIndexCharArray, 2);
    /* Fill*/
    memcpy(&chimera->plaintext[14], &fillBytes, 2);
    
    #ifdef DEBUG
    int i;
    for (i = 0; i < chimera->chimeraPars.textSizeBytes; i++) 
    {
        printf("%02X", chimera->plaintext[i]);
    }
    printf("\n");
    #endif

}


void MarkersGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int pilotSymbolsToGenerate)
{
    int pilotSymbolIndex=0;
    int numberOfciphertextBlocksForLocation=0;
    int numberOfciphertextBlocksForValue=0;
    int ciphertextBytesForLocation=0;
    int numberOfSegmentsPerSector=0;
    int ciphertextBytesForValue=0;
    int ciphertextResidualBitForValue=0;
    int indexMsg=0;
    unsigned char DF=0;
    int autenticationScope=0;
    int fastEpochPeriod=0;

    
    //**
    ArrayClean((char *)chimera->markerLocationListPerSector,MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION);
    ArrayClean((char *)chimera->occupiedMarkerLocationFlag,MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION);
    
    if(channel==SLOW_CHANNEL)
    {
        DF=chimera->slowDF;
        autenticationScope=NUMBER_OF_PILOT_SYMBOLS_IN_A_EPOCH;
    }
    else
    {
        DF=chimera->fastDF;
        /*Prd: Period is either 00000000 (1.5 seconds) or 00000001 (6 seconds). This field is set to 00000000 for processing slow channel Chimera*/
        if(chimera->fastEpochPeriod==0)
        {
            fastEpochPeriod=1.5; /*seconds*/
        }
        else
        {
            fastEpochPeriod=6; /*seconds*/
        }
        autenticationScope=(int)((double)fastEpochPeriod/PILOT_DURATION*1000);
    }
    
    switch(DF) 
    {
        case 1:
                numberOfciphertextBlocksForLocation=1;
                ciphertextBytesForLocation=40/8;
                numberOfSegmentsPerSector=1;
                numberOfciphertextBlocksForValue=3;
                ciphertextBytesForValue=(int)ceil(290/8.0);
                ciphertextResidualBitForValue=290%8;
            break;
        case 2:
                numberOfciphertextBlocksForLocation=1;
                ciphertextBytesForLocation=80/8;
                numberOfSegmentsPerSector=2;
                numberOfciphertextBlocksForValue=5;
                ciphertextBytesForValue=(int)ceil(580/8.0);
                ciphertextResidualBitForValue=580%8;
            break;
        case 3:
                numberOfciphertextBlocksForLocation=1;
                ciphertextBytesForLocation=120/8;
                numberOfSegmentsPerSector=3;
                numberOfciphertextBlocksForValue=7;
                ciphertextBytesForValue=(int)ceil(870/8.0);
                ciphertextResidualBitForValue=870%8;
            break;
        case 4:
                numberOfciphertextBlocksForLocation=2;
                ciphertextBytesForLocation=160/8;
                numberOfSegmentsPerSector=4;
                numberOfciphertextBlocksForValue=10;
                ciphertextBytesForValue=(int)ceil(1160/8.0);
                ciphertextResidualBitForValue=1160%8;
            break;
        case 5:
                numberOfciphertextBlocksForLocation=2;
                ciphertextBytesForLocation=200/8;
                numberOfSegmentsPerSector=5;
                numberOfciphertextBlocksForValue=12;
                ciphertextBytesForValue=(int)ceil(1450/8.0);
                ciphertextResidualBitForValue=1450%8;
            break;
        case 6:
                numberOfciphertextBlocksForLocation=2;
                ciphertextBytesForLocation=240/8;
                numberOfSegmentsPerSector=6;
                numberOfciphertextBlocksForValue=14;
                ciphertextBytesForValue=(int)ceil(1740/8.0);
                ciphertextResidualBitForValue=1740%8;
            break;
        case 7:
                numberOfciphertextBlocksForLocation=3;
                ciphertextBytesForLocation=280/8;
                numberOfSegmentsPerSector=7;
                numberOfciphertextBlocksForValue=16;
                ciphertextBytesForValue=(int)ceil(2030/8.0);
                ciphertextResidualBitForValue=2030%8;
            break;
        default:
                numberOfciphertextBlocksForLocation=1;
                ciphertextBytesForLocation=40/8;
                numberOfSegmentsPerSector=1;
                numberOfciphertextBlocksForValue=3;
                ciphertextBytesForValue=(int)ceil(290/8.0);
                ciphertextResidualBitForValue=290%8;
            break;
    }
    
    
    //for(pilotSymbolIndex=0; pilotSymbolIndex<NUMBER_OF_PILOT_SYMBOLS_IN_A_EPOCH; pilotSymbolIndex++)
    for(pilotSymbolIndex=0; pilotSymbolIndex<pilotSymbolsToGenerate; pilotSymbolIndex++)
    {
        if (pilotSymbolIndex%1800==0 && pilotSymbolIndex>0)
        {
            indexMsg++;
        }
        
        if((channel==FAST_CHANNEL) && (pilotSymbolIndex%autenticationScope==0))
        {
            /*Read a new fast marker key*/
            memcpy(chimera->fastMarkerKey, chimera->fastMarkerKeyList[chimera->indexFastKey], chimera->chimeraPars.markerKeySizeBytes);
            chimera->indexFastKey++;
            #ifdef DEBUG
            int rdIdx;
            for(rdIdx=0;rdIdx<chimera->chimeraPars.markerKeySizeBytes;rdIdx++)
            {
                printf("%02X",chimera->fastMarkerKey[rdIdx]);
            }
            printf("\n");
            #endif
            
        }

        /*Marker Location*/ 
        #ifdef CHIMERA_PROFILING
        StartProfilingUpdate(&chimera->markerLocationGenerationPerPilotProfiling);
        #endif
        MarkersLocationsGeneration(chimera, channel, prn,numberOfciphertextBlocksForLocation, pilotSymbolIndex, indexMsg, ciphertextBytesForLocation, numberOfSegmentsPerSector);
        #ifdef CHIMERA_PROFILING
        StopProfilingUpdate(&chimera->markerLocationGenerationPerPilotProfiling,1);
        #endif


        /*Marker Value*/
        #ifdef CHIMERA_PROFILING
        StartProfilingUpdate(&chimera->markerValueGenerationPerPilotProfiling);
        #endif
        
        #ifdef CHIMERA_PROFILING
        MarkersValuesGeneration(chimera, channel, prn, numberOfciphertextBlocksForValue, pilotSymbolIndex, indexMsg, ciphertextBytesForValue, ciphertextResidualBitForValue, numberOfSegmentsPerSector);
        #else
        MarkersValuesGeneration(chimera, channel, prn, numberOfciphertextBlocksForValue, pilotSymbolIndex, indexMsg, ciphertextBytesForValue, ciphertextResidualBitForValue);
        #endif
        
            
        #ifdef CHIMERA_PROFILING
        StopProfilingUpdate(&chimera->markerValueGenerationPerPilotProfiling,1);
        #endif

    } 


}
    
int LoadSectorPatternTable(TChimeraData *chimera)
{
   int pattern, segment;
   FILE *sectorPatternFile = fopen("./SectorPatternsTable","r");
    if(sectorPatternFile==NULL)
    {
        printf("File SectorPatternsTable does not exist!\n");
        return FAILURE_RETURN;
    }
    for(pattern=0;pattern<SECTOR_PATTERNS_ENTRIES;pattern++)
    {
        for(segment=0;segment<SECTOR_PATTERNS_ENTRIES;segment++)
        {
            fscanf(sectorPatternFile,"%hhu ", &chimera->sectorPattern[pattern][segment]);
            #ifdef DEBUG
            printf("%d ",chimera->sectorPattern[pattern][segment]);
            #endif
        }
        fscanf(sectorPatternFile,"\n");   
        #ifdef DEBUG
        printf("\n");
        #endif
 
    }    
    fclose(sectorPatternFile);
    
    return SUCCESS_RETURN;
}

void GeneratePatternList(TChimeraData *chimera)
{
    int segment, rowSectorPattern, indexSlowPatterList, indexFastPatterList;
    
    rowSectorPattern=((int)chimera->prn)%31;
    
    indexSlowPatterList=0;
    indexFastPatterList=0;
    for(segment=0;segment<SECTOR_PATTERNS_ENTRIES;segment++)
    {
        if(chimera->sectorPattern[rowSectorPattern][segment]==0)
        {
            chimera->slowPatternList[indexSlowPatterList]=segment;
            indexSlowPatterList++;
        }
        else
        {
            chimera->fastPatternList[indexFastPatterList]=segment;
            indexFastPatterList++;
        }

    }
    
    #ifdef DEBUG
    int index;
    for(index=0;index<16;index++)
    {
        printf("%d,",chimera->slowPatternList[index]);
    }
    printf("\n");
    for(index=0;index<15;index++)
    {
        printf("%d,",chimera->fastPatternList[index]);
    }
    printf("\n");
    #endif
}


void CiphertextGeneration(TChimeraData *chimera, int ciphertextBlockNeeded, TMarkerFunctionEnum function, TChimeraChannelEnum channel,int pilotSymbolIndex, int indexMsg, int prn)
{
    int ciphertextBlockIndex;
    unsigned char *markerBlock, *markerKey;
    
    if(function== MARKER_LOCATION)
    {
        markerBlock=chimera->markerIndicatorBlock;
    }
    else
    {
        markerBlock=chimera->markerValueBlock;
    }
    
    if(channel==SLOW_CHANNEL)
    {
        markerKey=chimera->slowMarkerKey[prn-1];
    }
    else
    {
        markerKey=chimera->fastMarkerKey;
    }

    
    for(ciphertextBlockIndex=0; ciphertextBlockIndex<ciphertextBlockNeeded; ciphertextBlockIndex++)
    {
        //OneCiphertextBlockGeneration(chimera, ciphertextBlockIndex, function, channel,pilotSymbolIndex, indexMsg, markerKey,markerBlock);
        BuildPlaintext(chimera, function, ciphertextBlockIndex, pilotSymbolIndex, indexMsg,channel);
        AESEncrypt(chimera->plaintext,chimera->chimeraPars.textSizeBytes, markerKey, chimera->ciphertext);
        memcpy(&markerBlock[ciphertextBlockIndex*chimera->chimeraPars.textSizeBytes], chimera->ciphertext, chimera->chimeraPars.textSizeBytes);
        #ifdef DEBUG
        int i;
        //if (pilotSymbolIndex%100==0 && pilotSymbolIndex>0)
        //{
            for (i = 0; i < chimera->chimeraPars.textSizeBytes; i++) 
            {
                printf("%02X", markerBlock[ciphertextBlockIndex*chimera->chimeraPars.textSizeBytes+i]);
            }
            printf("\n");
        //}
        #endif
    }
}

#ifdef DEBUG
void LocationsListGeneration(TChimeraData *chimera, TChimeraChannelEnum channel,int ciphertextBytes, int numberOfSegmentsPerSector, int pilotSymbolIndex)
#else
void LocationsListGeneration(TChimeraData *chimera, TChimeraChannelEnum channel,int ciphertextBytes, int numberOfSegmentsPerSector)
#endif
{

    int indexMarkerList=0;
    int indexToOrder=0;
    int flagInsert=1;
    int indexMs=0;
    int indexList;
    unsigned char markerByte=0;
    unsigned char markerIndicator=0;
    int indexMarkerIndicator=0;
    int numberOfElementsInTheList=0;
    int indexNibble=0;
    int lengthPatternList;
    unsigned char *patternList;
    
    if(channel==SLOW_CHANNEL)
    {
        lengthPatternList=16;
        patternList=chimera->slowPatternList;
    }
    else
    {
         lengthPatternList=15;
         patternList=chimera->fastPatternList;
    }
    
    while(indexMarkerIndicator<ciphertextBytes)
    {
        if(indexNibble%2==0)
        {
            memcpy(&markerByte, &chimera->markerIndicatorBlock[indexMarkerIndicator], 1);
            markerIndicator=(markerByte>>4) & 0x0F;
            indexNibble++;
        }
        else
        {
            markerIndicator=markerByte & 0x0F;
            indexMarkerIndicator++;
            indexNibble=0;
        }
        if(channel==FAST_CHANNEL && (markerIndicator==lengthPatternList))
        {
            markerIndicator=0;
        }
        /*Insert Marker into Marker Lcation List*/
        if(numberOfElementsInTheList==0 || (numberOfSegmentsPerSector==1))
        {
            memcpy(&chimera->markerLocationListPerSector[0], &markerIndicator, 1);
            chimera->occupiedMarkerLocationFlag[0]=1;
            numberOfElementsInTheList++;
        }
        else
        {
            indexMarkerList=0;
            while((chimera->occupiedMarkerLocationFlag[indexMarkerList]==1) && (indexMarkerList<numberOfSegmentsPerSector))
            {
                
                if(markerIndicator==chimera->markerLocationListPerSector[indexMarkerList])
                {
                    markerIndicator++;
                    if(markerIndicator==lengthPatternList)
                    {
                        markerIndicator=0;
                        indexMarkerList=0;
                    }
                    else
                    {
                        indexMarkerList++;
                    }
                }
                else
                {
                    if(markerIndicator<chimera->markerLocationListPerSector[indexMarkerList])
                    {
                        /*Put it in the right position*/
                        for(indexToOrder=numberOfElementsInTheList;indexToOrder>indexMarkerList; indexToOrder--)
                        {
                            chimera->markerLocationListPerSector[indexToOrder]=chimera->markerLocationListPerSector[indexToOrder-1];
                            chimera->occupiedMarkerLocationFlag[indexToOrder]=1;
                        }
                        memcpy(&chimera->markerLocationListPerSector[indexMarkerList], &markerIndicator, 1);
                        chimera->occupiedMarkerLocationFlag[indexMarkerList]=1;
                        numberOfElementsInTheList++;
                        flagInsert=0;
                        indexMarkerList=numberOfSegmentsPerSector;
                    }
                    else
                    {
                        indexMarkerList++;
                    }
                }

            }
            if(flagInsert!=0)
            {
                memcpy(&chimera->markerLocationListPerSector[indexMarkerList], &markerIndicator, 1);
                chimera->occupiedMarkerLocationFlag[indexMarkerList]=1;
                numberOfElementsInTheList++;
            }
            flagInsert=1;
        }
        if(numberOfElementsInTheList==numberOfSegmentsPerSector)
        {
            for(indexList=0; indexList<numberOfSegmentsPerSector; indexList++)
            {
                chimera->markerLocationListPerPilot[indexMs*numberOfElementsInTheList+indexList]=patternList[(int)chimera->markerLocationListPerSector[indexList]];
                //printf("%d,", chimera->markerLocationListPerPilot[indexMs*numberOfElementsInTheList+indexList]);

            }
            //printf("\n");
            //memcpy(&chimera->markerLocationListPerPilot[indexMs*numberOfElementsInTheList], indexPattern, numberOfElementsInTheList);
            ArrayClean((char *)chimera->markerLocationListPerSector,MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION);
            ArrayClean((char *)chimera->occupiedMarkerLocationFlag,MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION);
            numberOfElementsInTheList=0;
            indexMs=(indexMs+1)%10;
        }


        
    }
    
    //printf("\n");
    /**/
    #ifdef DEBUG
    int i;
    //if (pilotSymbolIndex%100==0 && pilotSymbolIndex>0)
    //{
        for (i = 0; i < numberOfSegmentsPerSector*10; i++) 
        {
            printf("%d,", (chimera->markerLocationListPerPilot[i]+1)+31*(i/numberOfSegmentsPerSector));
        }
        printf("\n");
    //}
    #endif
    /**/
        
}


void ValuesListGeneration(TChimeraData *chimera, int ciphertextBytesForValue,int ciphertextResidualBitForValue)
{
    int indexBit;
    int indexByte;
    int lowIndexToPrint;
    int countBits;
    unsigned char mask;
    int indexChip=0;
    
    ArrayClean((char *)chimera->chips,MAX_NUMBER_OF_MARKER_FOR_PILOT);

    
    memcpy(chimera->markerValuesPerPilot, chimera->markerValueBlock, ciphertextBytesForValue);
    //#ifdef DEBUG
//    if (pilotSymbolIndex%100==0 && pilotSymbolIndex>0)
//    {
        countBits=0;
        for (indexByte=0; indexByte < ciphertextBytesForValue; indexByte++)
        {
            if(indexByte==(ciphertextBytesForValue-1))
            {
                lowIndexToPrint=8-ciphertextResidualBitForValue;
            }
            else
            {
                lowIndexToPrint=0;
            }
            mask=0x80;
            for (indexBit=7; indexBit >= lowIndexToPrint; indexBit--)
            {
                //printf("%02X\n",chimera->markerValuesPerPilot[indexByte]);
                chimera->chips[indexChip]=(chimera->markerValuesPerPilot[indexByte] & mask)>>indexBit;
                //printf("%d",chimera->chips[indexChip]); 
                indexChip++;
                mask = mask >> 1; 
                countBits++;
                if(countBits==29)
                {
                    countBits=0;
                    //printf("\n");
                }
            }
        }
    //}
    //#endif

}


int LoadFastMarkerKey(TChimeraData *chimera)
{
        /*Read a new fast marker key*/
        char row[512];
        int wn, pos;
        float tow;
        int indexFastKey=0;
        
    /*Open Fast Marker Key File*/
    chimera->chimeraPars.fastMarkerKeyFile=fopen(chimera->chimeraPars.fastMarkerKeysFilename,"r");

	while(fgets(row,NUMBER_OF_BITS_IN_A_MESSAGE,chimera->chimeraPars.fastMarkerKeyFile)!=NULL)
    {
        sscanf(row,"%d,%f,%n",&wn,&tow,&pos);
        ReadHexBytes(&(row[pos]),chimera->fastMarkerKeyList[indexFastKey],chimera->chimeraPars.markerKeySizeBytes);
        //**
        #ifdef DEBUG
        int rdIdx;
        for(rdIdx=0;rdIdx<chimera->chimeraPars.markerKeySizeBytes;rdIdx++)
        {
            printf("%02X",chimera->fastMarkerKeyList[indexFastKey][rdIdx]);
        }
        printf("\n");
        #endif
        //**
         indexFastKey++;
    
    }
    fclose(chimera->chimeraPars.fastMarkerKeyFile);

    
    return SUCCESS_RETURN;
}

void MarkersLocationsGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForLocation, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForLocation, int numberOfSegmentsPerSector)
{
    #ifdef CHIMERA_PROFILING
    StartProfilingUpdate(&chimera->oneCiphertextProfiling);
    #endif
    CiphertextGeneration(chimera, numberOfciphertextBlocksForLocation, MARKER_LOCATION, channel,pilotSymbolIndex, indexMsg, prn);
    #ifdef CHIMERA_PROFILING
    StopProfilingUpdate(&chimera->oneCiphertextProfiling,numberOfciphertextBlocksForLocation);
    StartProfilingUpdate(&chimera->tenMarkerLocationProfiling);
    #endif
    #ifdef DEBUG
    LocationsListGeneration(chimera, channel,ciphertextBytesForLocation, numberOfSegmentsPerSector,pilotSymbolIndex);
    #else
    LocationsListGeneration(chimera, channel,ciphertextBytesForLocation, numberOfSegmentsPerSector);
    #endif
    #ifdef CHIMERA_PROFILING
    StopProfilingUpdate(&chimera->tenMarkerLocationProfiling,numberOfSegmentsPerSector);
    #endif
    
}

#ifdef CHIMERA_PROFILING
void MarkersValuesGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForValue, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForValue, int ciphertextResidualBitForValue, int numberOfSegmentsPerSector)
#else
void MarkersValuesGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForValue, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForValue, int ciphertextResidualBitForValue)
#endif
{
    #ifdef CHIMERA_PROFILING
    StartProfilingUpdate(&chimera->oneCiphertextProfiling);
    #endif
    CiphertextGeneration(chimera, numberOfciphertextBlocksForValue, MARKER_VALUE, channel,pilotSymbolIndex, indexMsg, prn);
    #ifdef CHIMERA_PROFILING
    StopProfilingUpdate(&chimera->oneCiphertextProfiling,numberOfciphertextBlocksForValue);
    StartProfilingUpdate(&chimera->tenMarkerValueProfiling);
    #endif
    ValuesListGeneration(chimera, ciphertextBytesForValue, ciphertextResidualBitForValue);
    #ifdef CHIMERA_PROFILING
    StopProfilingUpdate(&chimera->tenMarkerValueProfiling,numberOfSegmentsPerSector);
    #endif

}

/*void OneCiphertextBlockGeneration(TChimeraData *chimera, int ciphertextBlockIndex, TMarkerFunctionEnum function, TChimeraChannelEnum channel,int pilotSymbolIndex, int indexMsg, unsigned char *markerKey,unsigned char *markerBlock)
{
    BuildPlaintext(chimera, function, ciphertextBlockIndex, pilotSymbolIndex, indexMsg,channel);
    AESEncrypt(chimera->plaintext,chimera->chimeraPars.textSizeBytes, markerKey, chimera->ciphertext);
    memcpy(&markerBlock[ciphertextBlockIndex*chimera->chimeraPars.textSizeBytes], chimera->ciphertext, chimera->chimeraPars.textSizeBytes);
    #ifdef DEBUG
    int i;
    //if (pilotSymbolIndex%100==0 && pilotSymbolIndex>0)
    //{
        for (i = 0; i < chimera->chimeraPars.textSizeBytes; i++) 
        {
            printf("%02X", markerBlock[ciphertextBlockIndex*chimera->chimeraPars.textSizeBytes+i]);
        }
        printf("\n");
    //}
    #endif
        
}*/
