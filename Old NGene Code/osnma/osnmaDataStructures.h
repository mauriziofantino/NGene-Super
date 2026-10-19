/**
 * @file osnmaDataStructures.h
 *
 * @author Mario Nicola
 *
 * @date 12/04/2019
 *
 * @version 1.0.0
 *
 * @brief data structures used to manage OSNMA
 *        References to
 *        Galileo Navigation Message Authentication Specification for Signal-In-Space Testing v1.1
 */
#ifndef OSNMA_DATA_STRUCTURES_H_INCLUDED
#define OSNMA_DATA_STRUCTURES_H_INCLUDED

#include <stdio.h>
#include "osnmaConstants.h"
#include "spreadingCodes.h"
#include "ngeneConstants.h"
#include <openssl/ossl_typ.h>
#include "profilingSupport.h"
#include <bits/pthreadtypes.h>

/**
 * @enum ValidationStatusEnum
 * @brief Used to represent the current status of the validation through OSNMA
 */
typedef enum ValidationStatusEnum {
    TO_BE_VALIDATED = 0, /**< This information has not yet been checked through OSNMA */
    VALIDATION_PENDING = 1, /**< Indicates an on-going validation on the current information */
    NOT_VALID = 2, /**< OSNMA validation failed for the current information */
    VALIDATED = 3 /**< OSNMA validation successful for the current information */
} TValidationStatus;

/**
 * @enum NmaStatusEnum
 * @brief OSNMA Status according to Table 2, page 8
 */
typedef enum NmaStatusEnum {
    NMA_VALUE_NOT_AVAILABLE = 0,     /**< This values should not be found (nb when OSNMA is not present in the navigation message, all the related reserved bits are set to zero...) */
    NMA_TEST = 1,				 /**< No operational guarantees. The receiver may use [...] at its own risk) */
    NMA_OPERATIONAL = 2,	     /**< Service provided according the specifications */
    NMA_DO_NOT_USE = 3			     /**< The receiver MUST NOT navigate using OSNMA data */
} TNmaStatus;

/**
 * @enum ChainAndPublicKeyStatusEnum
 * @brief TESLA Chain and Public Key Status according to Table 3, page 9
 */
typedef enum ChainAndPublicKeyStatusEnum {
    RESERVED_0 = 0,	/**< Reserved for future use */
    NOMINAL = 1,	/**< The status of the chain in force and the public key in force */
    EOC = 2,		    /**< End Of Chain */
    CREV = 3,		    /**< Chain REVoked */
    NPK = 4,		    /**< New Public Key */
    PKREV = 5,		    /**< Public Key REVoked */
    RESERVED_6 = 6,	/**< Reserved for future use */
    RESERVED_7 = 7	/**< Reserved for future use */
} TChainAndPublicKeyStatus;

/**
 * @enum DsmTypeEnum
 * @brief Type of DSM data according to page 10
 */
typedef enum DsmTypeEnum {
    DSM_INVALID_TYPE,	  /**< Invalid value (added, not in the document) */
    DSM_KROOT = 1,	  /**< DSM that provides a digital signed KROOT from a TESLA chain */
    DSM_PKR = 2		  /**< A DSM for Public Key Renewal */
} TDsmType;

/**
 * @enum HashFunctionEnum
 * @brief Hash function used for the chain, according to Table 10 at page 12
 */
typedef enum HashFunctionEnum {
    SHA_256 = 0,      /**<  */
    SHA3_224 = 1,     /**<  */
    SHA3_256 = 2,     /**<  */
    HF_RSVD = 3	      /**<  */
} THashFunction;

/**
 * @enum MacFunctionEnum
 * @brief MAC function used to authenticate the navigation data, according to Table 11 at page 12
 */
typedef enum MacFunctionEnum {
    HMAC_SHA_256 = 0,	  /**<  */
    CMAC_AES = 1,	  /**<  */
    MF_2_RSVD = 2,	  /**<  */
    MF_3_RSVD = 3	  /**<  */
} TMacFunction;

/**
 * @enum NewPublicKeyTypeEnum
 * @brief New Public Key Type, according to Table 17 at page 16
 */
typedef enum NewPublicKeyTypeEnum {
    ECDSA_P224 = 0,	/**<  */
    ECDSA_P256 = 1,	/**<  */
    ECDSA_P384 = 2,	/**<  */
    ECDSA_P521 = 3,	/**<  */
    EMERGENCY_SERVICE_MESSAGE = 4,     /**< aka OSNMA Alert Message */
    NPKT_RSVD = 5	/**< used to represent all reserved values, from 5 to 15 */
} TNewPublicKeyType;

/**
 * @def NUM_OF_ADKD_VALUES
 * @brief Is the number of possible values defined in the TAdkd enumeration type
 *        which is 16 since ADKD is represented y using 4 bits
 *        Not all values will be used
 *        It is not possible to automatically get the number of values of a enumeration type (alas!)
 */
#define NUM_OF_ADKD_VALUES    16

/**
 * @enum AdkdEnum
 * @brief Type of DSM data according to page 10
 */
typedef enum AdkdEnum {
    ADKD_EPH_CLK_HEALTH = 0,	    /**<  */
    ADKD_GPS_LNAV_IONO = 1,	    /**<  */
    ADKD_GAL_SUBF_GPS_FRAME = 2,    /**<  */
    ADKD_ALMANAC = 3,		    /**<  */
    ADKD_GTS_UTC_GST_GPS = 4,	    /**<  */
    ADKD_OTHER_NAV_MSG = 5,	    /**<  */
    ADKD_REDCED = 6,		    /**<  */
    ADKD_SAR_RLM = 7,		    /**<  */
    ADKD_8_RSVD = 8,		    /**<  */
    ADKD_9_RSVD = 9,		    /**<  */
    ADKD_10_RSVD = 10,		    /**<  */
    ADKD_SLMAC_1_EXTRA = 11,	    /**<  */
    ADKD_SLMAC_10_EXTRA = 12,	    /**<  */
    ADKD_13_RSVD = 13,		    /**<  */
    ADKD_14_RSVD = 14,		    /**<  */
    ADKD_15_RSVD = 15		    /**<  */
} TAdkd;

/* Channel variables */

/**
 * @enum OsnmaStatusEnum
 * @brief status used to manage transients
 */
typedef enum RxOsnmaStatusEnum {
    RX_OSNMA_NOT_PRESENT = -1,		   /**< No valid NMA ststus, CPKS found */
    RX_OSNMA_OP_NOMINAL = 0,	       /**< Operational/Test - Nominal */
    RX_OSNMA_DU_WAIT_OP = 1,	       /**< A revocation has been found, wait for Operational/Test */
    RX_OSNMA_EOC_WAIT_NOMINAL = 2,     /**< An End-of-Chain has been found, wait for Nominal */
    RX_OSNMA_NPK_WAIT_NOMINAL = 3,     /**< A Public Key Renewal has been found, wait for Nominal */
    RX_OSNMA_EMERGENCY = 4	       /**< An Emergency/Alert message has been received */
} TRxOsnmaStatus;

/**
 * @enum ChannelOsnmaStatusEnum
 * @brief Status of the OSNM operations performed separately by each channel
 */
typedef enum ChannelOsnmaStatusEnum {
    CH_OSNMA_IDLE = -1,	    /**< OSNMA not usable (e.g. all bits in Reserved1 field==0) */
    CH_OSNMA_WAIT_SUBFRAME_START = 0,	  /**< wait for word2 to get headers. Default state at initialization (used when OSNMA parameters are not available) */
    CH_OSNMA_WAIT_DSMKROOT_BID0 = 1,	 /**< wait for the first packet of a DSM-KROOT to get all OSNMA parameters (used when OSNMA parameters are not available) */
    CH_OSNMA_SYNC_MACK_DSM = 2,	    /**< both MACK and DSM need synchronization */
    CH_OSNMA_SYNC_MACK = 3,	/**< MACK needs synchronization */
    CH_OSNMA_SYNC_DSM = 4,     /**< DSM needs synchronization */
    CH_OSNMA_OPERATIONAL = 5,	  /**< save all the DSMs and MACKs received */
    CH_OSNMA_WAIT_VALID_NMA_HEADER = 6	   /**< used when the OSNMA quits temporarily to be operational */
} TChannelOsnmaStatus;

/**
 * @struct ChannelOsnmaVariablesStruct
 *
 * @brief This structure contains variables used to manage OSNMA at a channel level.
 */
typedef struct ChannelOsnmaVariablesStruct {
    TSatConstellation satType;	   /**< constellation of the PRN tracked by the channel */
    TSatSignal satSignal;     /**< signal tracked by the channel */
    int prn;	 /**< prn tracked by the channel */
    int chId;	  /**< index of the channel */
    int wnSubframeStart;     /**< GST WN of the subframe start */
    int towSubframeStart;     /**< GST TOW of the subframe start */
     /**/ TNmaStatus nmaStatus;
    TChainAndPublicKeyStatus cpks;
    int cidInForce;
    TRxOsnmaStatus rxOsnmaStatus;
     /**/ int crossAuth;
		       /**< TRUE_FLAG if the current PRN is cross-authenticated */
    TChannelOsnmaStatus chOsnmaStatus;	   /**< status of OSNMA operation per channel basis */
    int chDsmIndex;	/**< index of the expected DSM packet */
    unsigned char chDsm[HKROOT_PER_SUBFRAME_SIZE];     /**< received DSM packets */
    int chWordTypes[GAL_PAGES_PER_SUBFRAME];	 /**< received word types in the navigation message */
    unsigned char chSubframe[GAL_BYTES_PER_SUBFRAME];	  /**< received navigation message (saved for the validation) */
    int chMackIndex;	 /**< index of the expected MACK packet */
    unsigned char chMack[MACK_PER_SUBFRAME_SIZE];     /**< received MACK packets */
    int chMackOffset;	  /**< seconds (depends on the current PRN) */
     /**/ TValidationStatus * dsmStatusPtr;
					 /**< Points to the validity of the received DMS */
    TValidationStatus *keyStatusPtr;	 /**< Points to the validity of the received TESLA key */
    TValidationStatus *msgStatusPtr;	 /**< Points to the validity of the received MAC */
    TValidationStatus *macseqStatusPtr;	    /**< Points to the validity of the received MACSEQ */
    char chOsnmaStatusLog[MAX_ROW_LENGTH];     /**< string used to print the status of the OSNMA for the current channel */
    int chOsnmaStatusLogColor;	   /**< Integer number used to manage the color for the plot with ncurses */
} TChannelOsnmaVariables;

/* Receiver variables */

/**
 * @enum MacLutElementEnum
 * @brief Define the type of each element in the sequence of the MAC Lookup Table
 *        according to the description in ICD v1.1, Annex B, pages 45 and 46
 */
typedef enum MacLutElementEnum {
    MAC_LUT_INVALID = -1,      /**< Slot not valid */
    MAC_LUT_FLEX = 0,	       /**< FLX, i.e. flexible slot in the sequence */
    MAC_LUT_SELF = 1,	       /**< Slot devoted to Self authentication (Galileo) */
    MAC_LUT_CROSS_GAL = 2,     /**< Slot devoted to Galileo cross authentication */
    MAC_LUT_CROSS_GPS = 3      /**< Slot devoted to GPS cross authentication */
} TMacLutElement;

/**
 * @struct MacLutSequenceStruct
 *
 * @brief This structure stores a sequence from the MAC LUT.
 *        This sequence refers to the MACs in a MACK section,
 *        according to the description in ICD v1.1, Annex B, pages 45 and 46
 */
typedef struct MacLutSequenceStruct {
    TAdkd *adkdSequence;	  /**< ADKD in the sequence (set to zero when FLEX) */
    TMacLutElement *sequence;	  /**< Elements in the sequence */
    int flexLen;		  /**< Number of flex elements in the sequence */
} TMacLutSequence;

/**
 * @struct MacLutStruct
 *
 * @brief This structure stores the current row of the LUT
 *        (i.e. whose index is read from the navigation message).
 *        according to the description in ICD v1.1, Annex B, pages 45 and 46
 */
typedef struct MacLutStruct {
    int lutValid;	  /**< validity flag. FALSE if not initialized, or not present, or with non-coherent parameters */
    /* parameters */
    int id;		  /**< index of the LUT row currently stored */
    int sct;		  /**< number of MACK sections in the sequence (it should be 1 or 2) */
    int nmack;		  /**< number of MACK block in each MACK section */
    int macsPerBlock;	  /**< maximum number of MACs in one MACK block */
    int sequenceLen;	  /**< length of each MAC sequence */
    /* All the sequences */
    TMacLutSequence *sequences;	    /**< sequence for each specified MACK section.
	                                 According to the ICD the MACK Sections in the MAC LUT are 1 or 2 */
} TMacLut;

/**
 * @struct OsnmaParametersStruct
 *
 * @brief .
 */
typedef struct OsnmaParametersStruct {
    int parametersValid;     /**< true if values in this structure are meaningful */
    /* from DSM-KROOT */
    int chainId;     /**< ID of the chain */
    int pkId;	  /**< Public Key ID */
    int dsmKrootNb;	/**< number of blocks composing a DSM-KROOT */
    int nmack;	   /**< number of mack blocks within a subframe */
    THashFunction hashFunction;	    /**< value that identifies the function used for the chain */
    TMacFunction macFunction;	  /**< value that identifies the function used to authenticate the navigation data */
    int keySize;     /**< size of the keys in the chain [bits] */
    int keySizeBytes;	 /**< size of the keys in the chain [bytes] */
    int macSize;     /**< size of truncated MAC (a.k.a. tag) [bits] */
    int macLutIndex;	 /**< index for the MAC Lookup Table) */
    int mackOffset;	/**< MACK offset enabled [boolean] */
    int krootWn;     /**< Week number of the time associated to the Key Root */
    int krootTow;     /**< Time of the week of the time associated to the Key Root [s]
	                  In the navigation message it's in hours, parser translates it into seconds */
    unsigned char alpha[6];	/*Random pattern to be included in the hashing process of the chain */
    /* from DSM Header */
    int dsmId;	   /**< The DSM ID valid when the DMS-KROOT parameters are parsed */
    /* DSM-PKR */
    int dsmPkrNb;     /**< number of blocks composing a DSM-PKR */
    int dsmPkrMessageId;     /**< index of the leaf in the Merkle Tree */
    TNewPublicKeyType dsmPkrNpkt;     /**< New Public Key Type */
    int dsmPkrNpkId;	 /**< ID of the current public key */

    /* parameters derived from the previous ones */

    const EVP_MD *function;
    const EVP_MD *functionThread;
    unsigned char nmaHeader;	/*NMA Header (8 bits) */

    int macSizeBytes;	   /**< MAC size in bytes, sometimes macSize is not multiple of 8! */
    int mackBlockSize;	   /**< size of a MACK block [bits] */
    int mackBlockSizeBytes;	/**< size of a MACK block [bytes] */
    int mackBlockDuration;     /**< time between the start of two consecutive MAC blocks [s] */
    int macPerMackBlock;     /**< how many MAC are included in one MAC block */
    int MSize;	  /**< size of Message M to verify the DSM-KROOT Digital Signature Verification [bytes] */
    int DSSizeBytes;		/*Size of the DSM-KROOT Digital Signature [bytes] */
    int DSSizeBits;		/*Size of the DSM-KROOT Digital Signature [bits] */
    int M_DSSizeBytes;	   /**< Size of the concatenation of M and DS [bytes] */
    int M_DSSizeBits; /**< Size of the concatenation of M and DS [bits] */
    int LBits;			/*Size of the Padding Field P1 of the DSM-KROOT [bits] */
    int LBytes;			/*Size of the Padding Field P1 of the DSM-KROOT [bytes] */

    /* Parameters read from NGene2 configuration file */
    int mackBlockOffset;     /**<  Flag that enables an extra delay between the truncated MAC (tag) and the related key.
						   *   If present, the delay is 1 MACK block. Described at page 36 of the ICD */
    int maxNs;	   /**< Supported Galileo PRNs */
    int minimumMacSize;	    /**< to be used for accumulations */
    int requiredMacAccumulations;     /**< number of macs required to reach authentications */
    char keyIndexTableFilename[2 * MAX_ROW_LENGTH];
    int ns;	 /**< Number of TESLA keys simultaneously transmitted */
    int *keyIndexTable;	    /**<  Relationship between the PRN and the index of the transmitted TESLA key */
    char publicKeyFilename[2 * MAX_ROW_LENGTH];
    char mtRootFilename[2 * MAX_ROW_LENGTH];	// mt => Merkle Tree
    unsigned char mtRoot[MERKLE_TREE_NODE_SIZE / BITS_PER_BYTE];
    char macLutFilename[2 * MAX_ROW_LENGTH];
    TMacLut macLut;
    int osnmaTimeout;	  /**< [s]. If this timeout expires after a successful validation, the validation is removed.
	                       If zero the timeout is disabled. */
} TOsnmaParameters;

/**
 * @struct MacListStruct
 *
 * @brief Used to store received MACs. Time values coherent with
 *        the definition of GST_{SF} in the standard, pages 26-27
 */
typedef struct MacListStruct {
    /* time values */
    int wnSubframeStart;     /**< Starting time of the subframe containing the MAC, WN */
    int towSubframeStart;     /**< Starting time of the subframe containing the MAC, TOW [s] */
    int wnMackStart;	 /**< Starting time of the mack block containing the MAC, WN */
    int towMackStart;	  /**< Starting time of the mack block containing the MAC, TOW [s] */
    /* key time refers to the TESLA key that must be used for authentication,
     * so it takes into account SLOW MACK modes and the additional key delay (page 36)*/
    int wnKey;	   /**< Starting time of the mack block containing the TESLA key required to verify the MAC, WN */
    int towKey;	    /**< Starting time of the mack block containing the TESLA key required to verify the MAC, TOW [s] */
    /* data */
    unsigned char ctr;	   /**< counter identifying the position of the MAC within the MACK bloc (defined in the standard, page 27) */
    unsigned char *mac;
    unsigned char *macseq;
    unsigned char *macInfoFlex;
    size_t macInfoFlexSizeBytes;
    TAdkd adkd;
    unsigned int iod;	  /**< IOD is contained in the MAC info. If the IOD is disabled through PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION
	                       this values is overwritten with the IOD read from the navigation message to manage MAC accumulation. */
    int macToBeVerified;
    /* previous/next element in the list */
    struct MacListStruct *prevMac;
    struct MacListStruct *nextMac;
} TMacList;

/**
 * @struct KeyListStruct
 *
 * @brief Used to store received TESLA keys.
 */
typedef struct KeyListStruct {
    /* time values */
    int wnSubframeStart;     /**< Starting time of the subframe containing the MAC, WN */
    int towSubframeStart;     /**< Starting time of the subframe containing the MAC, TOW [s] */
    int wnKey;	   /**< Starting time of the mack block containing the TESLA key, WN */
    int towKey;	    /**< Starting time of the mack block containing the TESLA key, TOW [s] */
    /* index */
    int indexKey;     /**< Index of the key in the TESLA chain, according to ICD v1.1, Figure 5, page 26:
	  	  	  	  	   The index of the Root Key is 0.
	  	  	  	  	   The first key in the chain, after the Root, has index 1. */
    /* data */
    unsigned char *key;
    /* previous/next element in the list */
    struct KeyListStruct *prevKey;
    struct KeyListStruct *nextKey;
} TKeyList;

/**
 * @struct NavDataListStruct
 *
 * @brief Used to store received navdata bits (for authentication).
 */
typedef struct NavDataListStruct {
    /* time values */
    int wnSubframeStart;     /**< Starting time of the subframe containing the MAC, WN */
    int towSubframeStart;     /**< Starting time of the subframe containing the MAC, TOW [s] */
    /* data */
    int *wordTypes;
    unsigned char *navData;
    /* previous/next element in the list */
    struct NavDataListStruct *prevNavData;
    struct NavDataListStruct *nextNavData;
} TNavDataList;

/**
 * @struct MackListStruct
 *
 * @brief .
 */
typedef struct MackListStruct {
    TSatConstellation satType;	   /**< Constellation of the satellite to which saved info refer */
    TSatSignal satSignal;     /**< Signal to which saved info refer (e.g. E1_BC or E5_A for Galileo) */
    int satId;	   /**< ID of the satellite to which saved info refer */
    /* chid is the channel index FROM WHICH information arrived
     *   - MACK, from the channel of the authenticating PRN (current channel if self-authenticating)
     *   - navdata, from the current channel */
    int chId;	  /**< channel index */
    /* PRN with the OSNMA convention */
    int prnN;	  /**< authenticated PRN (the same as satType+satId) */
    int prnA;	  /**< authenticating PRN (equal to prnN when self-authenticating) */
     /**/ TMacList * macList;
    TKeyList *keyList;
    TNavDataList *navDataList;
     /**/ struct MackListStruct *prevMack;
    struct MackListStruct *nextMack;
} TMackList;

/**
 * @struct DsmArrayStruct
 *
 * @brief .
 */
typedef struct DsmArrayStruct {
    TSatConstellation satType;	   /**< Constellation of the satellite to which saved info refer */
    int satId;	   /**< ID of the satellite to which saved info refer */
    /* these arrays store a complete DSM-KROOT or DSM-PKR */
    unsigned char *validity;	 /**< TRUE_FLAG when the correspondent DSM block is updated */
    unsigned char *dsmBlocks;	  /**< all the bits forming the DSM data, EXCLUDED the headers */
    unsigned char *nmaHeaders;	   /**< all NMA headers received with the current DSM-KROOT/DSM-PKR */
    unsigned char *dsmHeaders;	   /**< all DSM headers received with the current DSM-KROOT/DSM-PKR */
} TDsmArray;

/**
 * @struct DsmDataStruct
 *
 * @brief .
 */
typedef struct DsmDataStruct {
    TDsmType dsmType;	  /**< discriminate between KROOT and PKR */
    int currDsmId;     /**< Used to check when DSM changes */
    int currNb;	    /**< Current number of blocks (changes when DSM-KROOT or DSM-PKR is stored ) */
    int wnDsmStart;	/**< Starting time of the subframe with the first block of current DSM data, WN */
    int towDsmStart;	 /**< Starting time of the subframe with the first block of current DSM data, TOW [s] */
    /* these arrays are used to store DSM blocks from all satellites */
    unsigned char *validity;	 /**< TRUE_FLAG when the correspondent DSM block is updated */
    unsigned char *dsmBlocks;	  /**< all the bits forming the DSM data, EXCLUDED the headers */
    /* here information are separated per channel from where they come */
    int dsmArrayLen;	 /**< number of elements in dsmArray (number of receiver channels) */
    TDsmArray *dsmArray;     /**< DSM data saved on a PRN/channel basis */
} TDsmData;

/**
 * @struct OsnmaDataStruct
 *
 * @brief .
 */
typedef struct OsnmaDataStruct {
    int osnmaTransmitted;	       /**< TRUE_FLAG if at least one satellite is transmitting bits
	                                    different from zero in the NMA header */
    int revocationStarted;	       /**< TRUE_FLAG if a revocation is on-going */
     /**/ int receiverClockWn;		   /**< Galileo WN from the emulated receiver clock, -1 if not valid */
    double receiverClockTow;	       /**< Galileo TOW [s] from the emulated receiver clock, -1 if not valid */
    int receiverClockError;	       /**< Uncertainty of the emulated receiver clock, absolute value [s] */
     /**/ TOsnmaParameters osnmaPars;  /**< OSNMA parameters */
    TOsnmaParameters osnmaParsNew;     /**< OSNMA parameters (to be used during a chain change) */
    TMackList *mackList;	       /**<  */
    TDsmData dsmData;		       /**<  */
    /* temporary data for MAC verification */
    unsigned char navdataBitsForMac[MAX_NAVDATA_BYTES_FOR_MAC_VERIFICATION];	 /**< bits selected from navigation message for MAC verification */
    int navdataBitsForMacLen;	  /**< if >0 indicates how many bits are valid in navdataBitsForMac. If <=0, then the content of the variable is not valid. */

    /* Logs */
    FILE *parsLogFile;	       /**< log of the OSNMA parameters found in the DSM-KROOT , plus others */
    FILE *macLogFile;	      /**< log of the MACs as they are parsed from the navigation message */
    FILE *macseqLogFile;      /**< log of the MACSEQs as they are parsed from the navigation message */
    FILE *teslaChainFile;     /**< log with all the keys in the TESLA chain, as they are generated by TESLAKeyVerification
	                           (the PrepareTeslaChainLog.m script post-processes and sorts the log into a CSV file) */
    FILE *dsmLogFile;	      /**< log with all the bits of the DSM for all the satellites */
    FILE *osnmaStatusLogFile;	  /**<  log of the state machine that manages OSNMA for each channel */
    FILE *navdataLogFile;     /**< log about the navigation data used in the MAC authentication */
    FILE *validationLogFile;	 /**< log that reports all the verification performed by the receiver (PK, Root Key, MACs, MACSEQ,...)  */
    FILE *authLogFile;	      /**< log that reports the authentication status for each channel (MAC accumulation included) */
    FILE *simpleLogFile;	/**< simple log file of OSNMA, reporting only the authentication flag for each channel in tracking */
    FILE *simpleLogBinFile;	/**< simple log file of OSNMA, reporting only the authentication flag for each channel in tracking (bin) */
    TRxOsnmaStatus rxOsnmaStatusLastPrint;     /**< flag used to manage the print of the OSNMA Status */
    long int lastSimpleLogSaving;     /**< time of the last simple log update [s] including WN */

#ifdef OSNMA_PROFILING
    TProfilingSupport dsmKrootProfiling;     /**< profiling of the DSM K-ROOT verification */
    TProfilingSupport P1Verification;	  /**< profiling of the P1 field in DSM K-ROOT verification */
    TProfilingSupport merkleTreeVerification;	     /**< profiling of the Public Key Verification through Merkle Tree */
    TProfilingSupport P2Verification;	  /**< profiling of the P2 field in DSM PKR verification */
    TProfilingSupport pubPEMToEVPKey;	     /**< profiling of the Uploading of the public key from .pem file */
    TProfilingSupport teslaProfiling;	     /**< profiling of the TESLA chain verification */
    TProfilingSupport oneStepOfTeslaChainProfiling;	   /**< profiling of the TESLA chain verification */
    TProfilingSupport allMacsProfiling;		 /**< profiling of all MAC verification*/
    TProfilingSupport macProfiling;	     /**< profiling of the MAC verification (one single MAC)*/
    TProfilingSupport macSeqProfiling;	      /**< profiling of the MACSEQ verification */
    TProfilingSupport saveMackInMackList;     /**< profiling of the Save Mack in Mack List function */
    TProfilingSupport saveMackInMackListExcludingTESLA;	    /**< profiling of the Save Mack in Mack List function, excluding the TESLA chain verification */
    TProfilingSupport saveNavdataInMackList;	 /**< profiling of the Save Navdata in Mack List function */
    TProfilingSupport saveDsmBlock;	/**< profiling of the Save DSM Block function */
    TProfilingSupport extractAndCheckBitsForMACVerification;	 /**< profiling of the Exctract and Check Bits for MAC verification function */
    TProfilingSupport findKeyForMACVerification;     /**< profiling of the Find Key for MAC verification function */
    TProfilingSupport mainLoop;	    /**< profiling of the main loop, excluding the time required the TESLA chain verification */
#endif

    unsigned char *mtIntermediateNodes;	    /**< Four Merkle Tree intermediate nodes received from DSM-PKR */
    unsigned char *pkrPublicKeyTmp;	/**< The ECDSA compressed public key received from DSM-PKR (temporary variable) */
    unsigned char *pkrPublicKey;     /**< The ECDSA compressed public key received from DSM-PKR */
    int pkrNpkLen;     /**< size [bits] of the pkrPublicKey */
    int pkrValid;     /**< TRUE_FLAG if mtIntermediateNodes, pkrPublicKey and pkrNpkLen are valid */
    int pkrOam;	    /**< TRUE_FLAG if a valid OSNMA Alert Message has been found  */
    int pkrP2Len;     /**< Length of the Padding bits in the DSM-PKR [bits] */
    unsigned char *pkrP2;     /**< Padding bits in the DSM-PKR */
    unsigned char **leaves;	/**< Merkle Tree leaves */
    int leafSizeBytes;	  /**< Size of the Merkle Tree Leaf [bytes] */
    unsigned char ***nodeMatrix;    /**< Merkle Tree Structure, described as a matrix of nodes */
    unsigned char *nodesConcatenated;
    unsigned char *mtRoot_leaf;	/*Concatenation of Merkle Tree Root and and Leaf */
    unsigned char *mtRoot_leafHashed;	/*Hashed value of mtRoot_leaf */
    unsigned char *P2Computed;	/*Computed Padding Field in the DSM-PKR */
    unsigned char *KROOT;	/*Root Key */
    int KROOTValidityFLag;	/*Root Key Validity Flag */
    unsigned char *newKROOT;	/*Root Key */
    int newKROOTValidityFLag;	/*Root Key Validity Flag */
    EVP_PKEY *evp_key;		/*Public Key (internal format for OPEN SSL Library functions) */
    int evp_keyPkid;	 /**< PKID of the public key stored in evp_key. It is INVALID_VALUE (-1) if not valid. */
    int evp_keyValid;	  /**< TRUE_FLAG if evp_key is valid */
    const EVP_MD *function;	/*Hash function used to generate the DSM-KROOT Digital Signature */
    unsigned char *M;		/*Message to be signed, as at page 23 of ICD v1.1 */
    size_t MSizeMax;		/*Maximum size of M (bytes) */
    unsigned char *DS;		/*Received DSM-KROOT Digital Signature */
    unsigned char *DSToVerify;	/*Temporary Received DSM-KROOT Digital Signature, used for DS verification */
    unsigned char *M_DS;	/*Concatenation of M and DS */
    unsigned char *M_DSHashed;	/*Hashed value of M_DS */
    unsigned char *P1;		/*Received Padding Field in the DSM-KROOT */
    unsigned char *P1Computed;	/*Computed Padding Field in the DSM-KROOT */

    /* Warning!
     *
     * With the new definition of MACK Offset (ICD 2.1)
     * the ...Prn19Key... variables are used to manage the key for the MACs transmitted in the offset MACK Sections
     * regardless of the PRN that is transmitting it
     * (i.e. depending on the NS parameter, the PRN transmitting the key could be different from 19) */

     /**/ unsigned char *currentKey;	/*Temporary variable containing the current TESLA key */
    unsigned char *referenceKey;	/*Temporary variable containing the previous TESLA key */
    unsigned char *concatenatedInput;	/*Concatenation of Key, GST_SF,alpha and P3, to be hashed */
    unsigned char *hashedInput;	/*Hashed value of concatenatedInput */
    int indexLastKeyVerified;	/*Index of the last TESLA Key successfully verified */
    unsigned char *lastKeyVerified;	/*Last TESLA Key successfully verified */
    TValidationStatus keyVerificationStatus;
    unsigned char **lastPrn1Key;	/*Last PRN1 key saved for each MACK Block */
    int indexLastPrn1Key[MAX_NMACK_BLOCK];	/*Index of the last PRN1 key saved for each MACK Block */
    int wnLastPrn1Key[MAX_NMACK_BLOCK];	/*GST WN of the last PRN1 key saved */
    int towLastPrn1Key[MAX_NMACK_BLOCK];	/*GST TOW of the last PRN1 key saved */
    unsigned char **lastPrn19Key;	/*Last PRN19 key saved for each MACK Block */
    int indexLastPrn19Key[MAX_NMACK_BLOCK];	/*Index of the last PRN19 key saved for each MACK Block */
    int wnLastPrn19Key[MAX_NMACK_BLOCK];	/*GST WN of the last PRN19 key saved */
    int towLastPrn19Key[MAX_NMACK_BLOCK];	/*GST TOW of the last PRN19 key saved */
    unsigned char **lastPrn1KeyTmp;	/*Temporary version of lastPrn1Key */
    int indexLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of indexLastPrn1Key */
    int wnLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of wnLastPrn1Key */
    int towLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of towLastPrn1Key */
    unsigned char **lastPrn19KeyTmp;	/*Temporary version of lastPrn1Key */
    int indexLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of lastPrn19Key */
    int wnLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of wnLastPrn19Key */
    int towLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of towLastPrn19Key */
    int indexLastPrn19KeyToCheck;	/*Index of the MACK Block where the temporary version of last PRN19 key is saved */
    int newMacsAvailable;     /**< If TRUE_FLAG, then new MACs are available for the verification */
    unsigned char *message;	/*Message to be authenticated, as at page 27 of ICD v1.1 */
    int messageSizeMax;		/*Maximum size of message (bytes) */
    unsigned char *messageHashed;	/*Hashed value of message */
    unsigned char *tag;		/*Truncated message authentication code computed */
    unsigned char *messageForMacseq;	/*Message to be build to check MACSEQ field, as at page 28 of ICD v1.1 */
    int messageForMacseqSizeMax;	/*Maximum size of messageForMacseq (bytes) */
    int tagSizeMax;		/*Maximum size of tag (bytes) */
    unsigned char *macSeqComputed;	/*MACSEQ computed */


    /*Variables used by the thread to verify the TESLA chain */
    unsigned char *currentKeyThread;	/*Temporary variable containing the current TESLA key */
    unsigned char *referenceKeyThread;	/*Temporary variable containing the previous TESLA key */
    unsigned char *keyThread;
    unsigned char *concatenatedInputThread;	/*Concatenation of Key, GST_SF,alpha and P3, to be hashed */
    unsigned char *hashedInputThread;	/*Hashed value of concatenatedInput */
    int indexLastKeyVerifiedThread;	/*Index of the last TESLA Key successfully verified */
    unsigned char *lastKeyVerifiedThread;	/*Last TESLA Key successfully verified */
    TValidationStatus keyVerificationStatusThread;
    unsigned char **lastPrn1KeyThread;	/*Last PRN1 key saved for each MACK Block */
    int indexLastPrn1KeyThread[MAX_NMACK_BLOCK];	/*Index of the last PRN1 key saved for each MACK Block */
    int wnLastPrn1KeyThread[MAX_NMACK_BLOCK];	/*GST WN of the last PRN1 key saved */
    int towLastPrn1KeyThread[MAX_NMACK_BLOCK];	/*GST TOW of the last PRN1 key saved */
    unsigned char **lastPrn19KeyThread;	/*Last PRN19 key saved for each MACK Block */
    int indexLastPrn19KeyThread[MAX_NMACK_BLOCK];	/*Index of the last PRN19 key saved for each MACK Block */
    int wnLastPrn19KeyThread[MAX_NMACK_BLOCK];	/*GST WN of the last PRN19 key saved */
    int towLastPrn19KeyThread[MAX_NMACK_BLOCK];	/*GST TOW of the last PRN19 key saved */
    unsigned char **lastPrn1KeyThreadTmp;	/*Last PRN1 key saved for each MACK Block */
    unsigned char **lastPrn19KeyThreadTmp;	/*Last PRN19 key saved for each MACK Block */
    pthread_mutex_t TESLAKeyVerificationMutex;	  /**< This mutex protects the threadTESLAKeyVerificationFunctionStopFlag, defined in ngene*/
} TOsnmaData;

/**
 * @struct KeyToVerifyMackStruct
 *
 * @brief .
 */
typedef struct KeyToVerifyMackStruct {
    unsigned char **lastKey;	/*Last PRN1 or PRN19 (in case of MACK offsetting) key saved for each MACK Block */
    int indexLastKey[MAX_NMACK_BLOCK];	/*Index of the last PRN1 or PRN 19 (in case of MACK offsetting) key saved for each MACK Block */
    int GST_TOW_LastKey[MAX_NMACK_BLOCK];	/*GST TOW of the last PRN1 or PRN19 (in case of MACK offsetting) key saved */
} TKeyToVerifyMack;

typedef struct TeslaKeyVerificationThreadFunctionParametersStruct {
    //TOsnmaData *rxOsnma;
    int prnA;	   /**<   the PRN (in OSNMA convention) that transmitted the key */
    int wnMackStart;	 /**< the start time of the current MACK Block (WN) */
    int towMackStart;	  /**<the start time of the current MACK Block (TOW, [s])  */
    unsigned char *key;
    unsigned char *currentKey;
    unsigned char *lastKeyVerified;
    unsigned char *referenceKey;
    int keySize;
    int keySizeBytes;
    int indexKeyToStart;
    int *indexLastKeyVerified;
    TValidationStatus keyVerificationStatus;
    TChannelOsnmaVariables *chOsnma;
    int indexKeyToEnd;
    unsigned int P3;
    int blockIdx;
    int index;
    int ns;
    int nmack;
    int mackBlockDuration;
    int mackOffset;
    unsigned char alpha[6];	/*Random pattern to be included in the hashing process of the chain */
    int *indexLastPrn1Key;	/*Index of the last PRN1 key saved for each MACK Block */
    int *wnLastPrn1Key;		/*GST WN of the last PRN1 key saved */
    int *towLastPrn1Key;	/*GST TOW of the last PRN1 key saved */
    unsigned char **lastPrn1Key;	/*Last PRN1 key saved for each MACK Block */
    unsigned char **lastPrn19Key;	/*Last PRN19 key saved for each MACK Block */
    int *indexLastPrn19Key;	/*Index of the last PRN19 key saved for each MACK Block */
    int *wnLastPrn19Key;	/*GST WN of the last PRN19 key saved */
    int *towLastPrn19Key;	/*GST TOW of the last PRN19 key saved */
    int *keyIndexTable;
    unsigned char **lastPrn1KeyTmp;	/*Temporary version of lastPrn1Key */
    int indexLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of indexLastPrn1Key */
    int wnLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of wnLastPrn1Key */
    int towLastPrn1KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of towLastPrn1Key */
    unsigned char **lastPrn19KeyTmp;	/*Temporary version of lastPrn1Key */
    int indexLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of lastPrn19Key */
    int wnLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of wnLastPrn19Key */
    int towLastPrn19KeyTmp[MAX_NMACK_BLOCK];	/*Temporary version of towLastPrn19Key */
    int indexLastPrn19KeyToCheck;	/*Index of the MACK Block where the temporary version of last PRN19 key is saved */
    unsigned char *concatenatedInput;	/*Concatenation of Key, GST_SF,alpha and P3, to be hashed */
    unsigned char *hashedInput;	/*Hashed value of concatenatedInput */
    const EVP_MD *function;
    int maxNs;

    pthread_mutex_t *TESLAKeyVerificationMutex;	   /**< This mutex protects the threadTESLAKeyVerificationFunctionStopFlag, defined in ngene.*/
    FILE *teslaChainFile;     /**< log with all the keys in the TESLA chain, as they are generated by TESLAKeyVerification
		                           (the PrepareTeslaChainLog.m script post-processes and sorts the log into a CSV file) */
    int krootWn;
    int krootTow;
    int returnValue;
} TTeslaKeyVerificationThreadFunctionParametersStruct;

#endif				/* OSNMA_DATA_STRUCTURES_H_INCLUDED */
