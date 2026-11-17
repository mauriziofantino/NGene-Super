/** @file broadcast.c @brief Classic GPS SF1..3 and Galileo words0..5 orbit/clock parsing.
 * Field maps and scale factors adapted from gpsMessageParser.c and galileoMessageParser.c,
 * Mario Nicola, 2014. Disk/global receiver effects removed; cache is per channel. */
#include "ngene/navigation.h"
#include <math.h>
#include <string.h>
#define NG_BROADCAST_PI 3.14159265358979323846
#define TRUE_FLAG 1
#define FALSE_FLAG 0
#define SUCCESS_RETURN 0
#define FAILURE_RETURN -1
#define SIGNAL_OK 0
#define SIGNAL_OUT_OF_SERVICE 1
#define GPS_MAXIMUM_TOC 604784.0
#define GPS_MAXIMUM_ECCENTRICITY 0.03
#define GPS_SUBF1_WN_FIRST_BYTE_IDX (8)
#define GPS_SUBF1_WN_LAST_BYTE_IDX (9)
#define GPS_SUBF1_WN_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_WN_BITS_OF_LAST_BYTE (4)
#define GPS_SUBF1_HEALTH_FIRST_BYTE_IDX (10)
#define GPS_SUBF1_HEALTH_LAST_BYTE_IDX (10)
#define GPS_SUBF1_HEALTH_BITS_OF_FIRST_BYTE (0)
#define GPS_SUBF1_HEALTH_BITS_OF_LAST_BYTE (6)
#define GPS_SUBF1_URA_FIRST_BYTE_IDX (9)
#define GPS_SUBF1_URA_LAST_BYTE_IDX (10)
#define GPS_SUBF1_URA_BITS_OF_FIRST_BYTE (2)
#define GPS_SUBF1_URA_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_IODC_MS_FIRST_BYTE_IDX (11)
#define GPS_SUBF1_IODC_MS_LAST_BYTE_IDX (11)
#define GPS_SUBF1_IODC_MS_BITS_OF_FIRST_BYTE (2)
#define GPS_SUBF1_IODC_MS_BITS_OF_LAST_BYTE (0)
#define GPS_SUBF1_IODC_LS_FIRST_BYTE_IDX (28)
#define GPS_SUBF1_IODC_LS_LAST_BYTE_IDX (29)
#define GPS_SUBF1_IODC_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_IODC_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_TOC_FIRST_BYTE_IDX (29)
#define GPS_SUBF1_TOC_LAST_BYTE_IDX (31)
#define GPS_SUBF1_TOC_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_TOC_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_AF0_FIRST_BYTE_IDX (36)
#define GPS_SUBF1_AF0_LAST_BYTE_IDX (38)
#define GPS_SUBF1_AF0_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_AF0_BITS_OF_LAST_BYTE (8)
#define GPS_SUBF1_AF0_FIELD_DIM (22)
#define GPS_SUBF1_AF1_FIRST_BYTE_IDX (33)
#define GPS_SUBF1_AF1_LAST_BYTE_IDX (35)
#define GPS_SUBF1_AF1_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_AF1_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_AF1_FIELD_DIM (16)
#define GPS_SUBF1_AF2_FIRST_BYTE_IDX (32)
#define GPS_SUBF1_AF2_LAST_BYTE_IDX (33)
#define GPS_SUBF1_AF2_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_AF2_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_AF2_FIELD_DIM (8)
#define GPS_SUBF1_TGD_FIRST_BYTE_IDX (26)
#define GPS_SUBF1_TGD_LAST_BYTE_IDX (27)
#define GPS_SUBF1_TGD_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF1_TGD_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF1_TGD_FIELD_DIM (8)
#define GPS_SUBF2_IODE_FIRST_BYTE_IDX (8)
#define GPS_SUBF2_IODE_LAST_BYTE_IDX (9)
#define GPS_SUBF2_IODE_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_IODE_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CRS_FIRST_BYTE_IDX (9)
#define GPS_SUBF2_CRS_LAST_BYTE_IDX (11)
#define GPS_SUBF2_CRS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_CRS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CRS_FIELD_DIM (16)
#define GPS_SUBF2_DN_FIRST_BYTE_IDX (12)
#define GPS_SUBF2_DN_LAST_BYTE_IDX (14)
#define GPS_SUBF2_DN_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_DN_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_DN_FIELD_DIM (16)
#define GPS_SUBF2_M0_MS_FIRST_BYTE_IDX (14)
#define GPS_SUBF2_M0_MS_LAST_BYTE_IDX (15)
#define GPS_SUBF2_M0_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_M0_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_M0_MS_FIELD_DIM (8)
#define GPS_SUBF2_M0_LS_FIRST_BYTE_IDX (16)
#define GPS_SUBF2_M0_LS_LAST_BYTE_IDX (19)
#define GPS_SUBF2_M0_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_M0_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CUC_FIRST_BYTE_IDX (20)
#define GPS_SUBF2_CUC_LAST_BYTE_IDX (22)
#define GPS_SUBF2_CUC_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_CUC_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CUC_FIELD_DIM (16)
#define GPS_SUBF2_E_MS_FIRST_BYTE_IDX (22)
#define GPS_SUBF2_E_MS_LAST_BYTE_IDX (23)
#define GPS_SUBF2_E_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_E_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_E_LS_FIRST_BYTE_IDX (24)
#define GPS_SUBF2_E_LS_LAST_BYTE_IDX (27)
#define GPS_SUBF2_E_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_E_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CUS_FIRST_BYTE_IDX (28)
#define GPS_SUBF2_CUS_LAST_BYTE_IDX (30)
#define GPS_SUBF2_CUS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_CUS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_CUS_FIELD_DIM (16)
#define GPS_SUBF2_SQRTA_MS_FIRST_BYTE_IDX (30)
#define GPS_SUBF2_SQRTA_MS_LAST_BYTE_IDX (31)
#define GPS_SUBF2_SQRTA_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_SQRTA_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_SQRTA_LS_FIRST_BYTE_IDX (32)
#define GPS_SUBF2_SQRTA_LS_LAST_BYTE_IDX (35)
#define GPS_SUBF2_SQRTA_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_SQRTA_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF2_TOE_FIRST_BYTE_IDX (36)
#define GPS_SUBF2_TOE_LAST_BYTE_IDX (38)
#define GPS_SUBF2_TOE_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF2_TOE_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CIC_FIRST_BYTE_IDX (8)
#define GPS_SUBF3_CIC_LAST_BYTE_IDX (10)
#define GPS_SUBF3_CIC_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_CIC_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CIC_FIELD_DIM (16)
#define GPS_SUBF3_OMEGA0_MS_FIRST_BYTE_IDX (10)
#define GPS_SUBF3_OMEGA0_MS_LAST_BYTE_IDX (11)
#define GPS_SUBF3_OMEGA0_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_OMEGA0_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_OMEGA0_MS_FIELD_DIM (8)
#define GPS_SUBF3_OMEGA0_LS_FIRST_BYTE_IDX (12)
#define GPS_SUBF3_OMEGA0_LS_LAST_BYTE_IDX (15)
#define GPS_SUBF3_OMEGA0_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_OMEGA0_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CIS_FIRST_BYTE_IDX (16)
#define GPS_SUBF3_CIS_LAST_BYTE_IDX (18)
#define GPS_SUBF3_CIS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_CIS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CIS_FIELD_DIM (16)
#define GPS_SUBF3_I0_MS_FIRST_BYTE_IDX (18)
#define GPS_SUBF3_I0_MS_LAST_BYTE_IDX (19)
#define GPS_SUBF3_I0_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_I0_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_I0_MS_FIELD_DIM (8)
#define GPS_SUBF3_I0_LS_FIRST_BYTE_IDX (20)
#define GPS_SUBF3_I0_LS_LAST_BYTE_IDX (23)
#define GPS_SUBF3_I0_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_I0_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CRC_FIRST_BYTE_IDX (24)
#define GPS_SUBF3_CRC_LAST_BYTE_IDX (26)
#define GPS_SUBF3_CRC_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_CRC_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_CRC_FIELD_DIM (16)
#define GPS_SUBF3_OMEGA_MS_FIRST_BYTE_IDX (26)
#define GPS_SUBF3_OMEGA_MS_LAST_BYTE_IDX (27)
#define GPS_SUBF3_OMEGA_MS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_OMEGA_MS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_OMEGA_MS_FIELD_DIM (8)
#define GPS_SUBF3_OMEGA_LS_FIRST_BYTE_IDX (28)
#define GPS_SUBF3_OMEGA_LS_LAST_BYTE_IDX (31)
#define GPS_SUBF3_OMEGA_LS_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_OMEGA_LS_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_OMEGADOT_FIRST_BYTE_IDX (32)
#define GPS_SUBF3_OMEGADOT_LAST_BYTE_IDX (35)
#define GPS_SUBF3_OMEGADOT_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_OMEGADOT_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_OMEGADOT_FIELD_DIM (24)
#define GPS_SUBF3_IODE_FIRST_BYTE_IDX (36)
#define GPS_SUBF3_IODE_LAST_BYTE_IDX (37)
#define GPS_SUBF3_IODE_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_IODE_BITS_OF_LAST_BYTE (2)
#define GPS_SUBF3_IDOT_FIRST_BYTE_IDX (37)
#define GPS_SUBF3_IDOT_LAST_BYTE_IDX (38)
#define GPS_SUBF3_IDOT_BITS_OF_FIRST_BYTE (6)
#define GPS_SUBF3_IDOT_BITS_OF_LAST_BYTE (8)
#define GPS_SUBF3_IDOT_FIELD_DIM (14)
#define GAL_TOE_SCALE_FACTOR 60
#define GAL_TYPE0_TIME_FIRST_BYTE_IDX (0)
#define GAL_TYPE0_TIME_LAST_BYTE_IDX (0)
#define GAL_TYPE0_TIME_BITS_OF_FIRST_BYTE (0)
#define GAL_TYPE0_TIME_BITS_OF_LAST_BYTE (2)
#define GAL_TYPE0_WN_FIRST_BYTE_IDX (12)
#define GAL_TYPE0_WN_LAST_BYTE_IDX (13)
#define GAL_TYPE0_WN_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE0_WN_BITS_OF_LAST_BYTE (4)
#define GAL_TYPE0_TOW_FIRST_BYTE_IDX (13)
#define GAL_TYPE0_TOW_LAST_BYTE_IDX (15)
#define GAL_TYPE0_TOW_BITS_OF_FIRST_BYTE (4)
#define GAL_TYPE0_TOW_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE1_IOD_FIRST_BYTE_IDX (0)
#define GAL_TYPE1_IOD_LAST_BYTE_IDX (1)
#define GAL_TYPE1_IOD_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE1_IOD_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE1_M0_FIRST_BYTE_IDX (3)
#define GAL_TYPE1_M0_LAST_BYTE_IDX (7)
#define GAL_TYPE1_M0_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE1_M0_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE1_M0_BITS_FIELD_DIM (32)
#define GAL_TYPE2_IOD_FIRST_BYTE_IDX (0)
#define GAL_TYPE2_IOD_LAST_BYTE_IDX (1)
#define GAL_TYPE2_IOD_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE2_IOD_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_IOD_FIRST_BYTE_IDX (0)
#define GAL_TYPE3_IOD_LAST_BYTE_IDX (1)
#define GAL_TYPE3_IOD_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE3_IOD_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_SISA_FIRST_BYTE_IDX (15)
#define GAL_TYPE3_SISA_LAST_BYTE_IDX (15)
#define GAL_TYPE3_SISA_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_SISA_BITS_OF_LAST_BYTE (0)
#define GAL_TYPE4_IOD_FIRST_BYTE_IDX (0)
#define GAL_TYPE4_IOD_LAST_BYTE_IDX (1)
#define GAL_TYPE4_IOD_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE4_IOD_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE5_A0I_FIRST_BYTE_IDX (0)
#define GAL_TYPE5_A0I_LAST_BYTE_IDX (2)
#define GAL_TYPE5_A0I_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE5_A0I_BITS_OF_LAST_BYTE (1)
#define GAL_TYPE5_A1I_FIRST_BYTE_IDX (2)
#define GAL_TYPE5_A1I_LAST_BYTE_IDX (3)
#define GAL_TYPE5_A1I_BITS_OF_FIRST_BYTE (7)
#define GAL_TYPE5_A1I_BITS_OF_LAST_BYTE (4)
#define GAL_TYPE5_A1I_BITS_FIELD_DIM (11)
#define GAL_TYPE5_A2I_FIRST_BYTE_IDX (3)
#define GAL_TYPE5_A2I_LAST_BYTE_IDX (5)
#define GAL_TYPE5_A2I_BITS_OF_FIRST_BYTE (4)
#define GAL_TYPE5_A2I_BITS_OF_LAST_BYTE (2)
#define GAL_TYPE5_A2I_BITS_FIELD_DIM (14)
#define GAL_TYPE5_SF1_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_SF1_LAST_BYTE_IDX (5)
#define GAL_TYPE5_SF1_BITS_OF_FIRST_BYTE (3)
#define GAL_TYPE5_SF1_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE5_SF2_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_SF2_LAST_BYTE_IDX (5)
#define GAL_TYPE5_SF2_BITS_OF_FIRST_BYTE (4)
#define GAL_TYPE5_SF2_BITS_OF_LAST_BYTE (5)
#define GAL_TYPE5_SF3_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_SF3_LAST_BYTE_IDX (5)
#define GAL_TYPE5_SF3_BITS_OF_FIRST_BYTE (5)
#define GAL_TYPE5_SF3_BITS_OF_LAST_BYTE (4)
#define GAL_TYPE5_SF4_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_SF4_LAST_BYTE_IDX (5)
#define GAL_TYPE5_SF4_BITS_OF_FIRST_BYTE (6)
#define GAL_TYPE5_SF4_BITS_OF_LAST_BYTE (3)
#define GAL_TYPE5_SF5_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_SF5_LAST_BYTE_IDX (5)
#define GAL_TYPE5_SF5_BITS_OF_FIRST_BYTE (7)
#define GAL_TYPE5_SF5_BITS_OF_LAST_BYTE (2)
#define GAL_TYPE5_TGD_E1E5A_FIRST_BYTE_IDX (5)
#define GAL_TYPE5_TGD_E1E5A_LAST_BYTE_IDX (7)
#define GAL_TYPE5_TGD_E1E5A_BITS_OF_FIRST_BYTE (1)
#define GAL_TYPE5_TGD_E1E5A_BITS_OF_LAST_BYTE (1)
#define GAL_TYPE5_TGD_E1E5A_BITS_FIELD_DIM (10)
#define GAL_TYPE5_TGD_E1E5B_FIRST_BYTE_IDX (7)
#define GAL_TYPE5_TGD_E1E5B_LAST_BYTE_IDX (8)
#define GAL_TYPE5_TGD_E1E5B_BITS_OF_FIRST_BYTE (7)
#define GAL_TYPE5_TGD_E1E5B_BITS_OF_LAST_BYTE (3)
#define GAL_TYPE5_TGD_E1E5B_BITS_FIELD_DIM (10)
#define GAL_TYPE5_WN_FIRST_BYTE_IDX (9)
#define GAL_TYPE5_WN_LAST_BYTE_IDX (10)
#define GAL_TYPE5_WN_BITS_OF_FIRST_BYTE (7)
#define GAL_TYPE5_WN_BITS_OF_LAST_BYTE (5)
#define GAL_TYPE5_TOW_FIRST_BYTE_IDX (10)
#define GAL_TYPE5_TOW_LAST_BYTE_IDX (13)
#define GAL_TYPE5_TOW_BITS_OF_FIRST_BYTE (3)
#define GAL_TYPE5_TOW_BITS_OF_LAST_BYTE (1)
#define GAL_TYPE5_E1BHS_FIRST_BYTE_IDX (8)
#define GAL_TYPE5_E1BHS_LAST_BYTE_IDX (8)
#define GAL_TYPE5_E1BHS_BITS_OF_FIRST_BYTE (7)
#define GAL_TYPE5_E1BHS_BITS_OF_LAST_BYTE (3)
#define GAL_TYPE5_E1BDVS_FIRST_BYTE_IDX (9)
#define GAL_TYPE5_E1BDVS_LAST_BYTE_IDX (9)
#define GAL_TYPE5_E1BDVS_BITS_OF_FIRST_BYTE (1)
#define GAL_TYPE5_E1BDVS_BITS_OF_LAST_BYTE (0)
#define GAL_PAGE_LENGTH 500
#define POW_TWO_M31 4.656612873077393e-10
#define POW_TWO_M43 1.1368683772161603e-13
#define POW_TWO_M55 2.7755575615628914e-17
#define POW_TWO_M05 0.03125
#define POW_TWO_P24 16777216
#define POW_TWO_M29 1.862645149230957e-9
#define POW_TWO_M33 1.1641532182693481e-10
#define POW_TWO_M19 0.0000019073486328125
#define POW_TWO_M34 5.820766091346741e-11
#define POW_TWO_M46 1.4210854715202004e-14
#define POW_TWO_M59 1.734723475976807e-18
#define POW_TWO_M02 0.25
#define POW_TWO_M08 0.00390625
#define POW_TWO_M15 0.000030517578125
#define POW_TWO_M32 2.3283064365386963e-10
#define GAL_TYPE1_toe_FIRST_BYTE_IDX (2)
#define GAL_TYPE1_toe_LAST_BYTE_IDX (3)
#define GAL_TYPE1_toe_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE1_toe_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE1_e_FIRST_BYTE_IDX (7)
#define GAL_TYPE1_e_LAST_BYTE_IDX (11)
#define GAL_TYPE1_e_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE1_e_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE1_sqrtA_FIRST_BYTE_IDX (11)
#define GAL_TYPE1_sqrtA_LAST_BYTE_IDX (15)
#define GAL_TYPE1_sqrtA_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE1_sqrtA_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE2_omega0_FIRST_BYTE_IDX (2)
#define GAL_TYPE2_omega0_LAST_BYTE_IDX (5)
#define GAL_TYPE2_omega0_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE2_omega0_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE2_omega0_BITS_FIELD_DIM (32)
#define GAL_TYPE2_i0_FIRST_BYTE_IDX (6)
#define GAL_TYPE2_i0_LAST_BYTE_IDX (9)
#define GAL_TYPE2_i0_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE2_i0_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE2_i0_BITS_FIELD_DIM (32)
#define GAL_TYPE2_omega_FIRST_BYTE_IDX (10)
#define GAL_TYPE2_omega_LAST_BYTE_IDX (13)
#define GAL_TYPE2_omega_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE2_omega_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE2_omega_BITS_FIELD_DIM (32)
#define GAL_TYPE2_idot_FIRST_BYTE_IDX (14)
#define GAL_TYPE2_idot_LAST_BYTE_IDX (15)
#define GAL_TYPE2_idot_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE2_idot_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE2_idot_BITS_FIELD_DIM (14)
#define GAL_TYPE3_omegadot_FIRST_BYTE_IDX (2)
#define GAL_TYPE3_omegadot_LAST_BYTE_IDX (4)
#define GAL_TYPE3_omegadot_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_omegadot_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_omegadot_BITS_FIELD_DIM (24)
#define GAL_TYPE3_deltan_FIRST_BYTE_IDX (5)
#define GAL_TYPE3_deltan_LAST_BYTE_IDX (6)
#define GAL_TYPE3_deltan_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_deltan_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_deltan_BITS_FIELD_DIM (16)
#define GAL_TYPE3_cuc_FIRST_BYTE_IDX (7)
#define GAL_TYPE3_cuc_LAST_BYTE_IDX (8)
#define GAL_TYPE3_cuc_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_cuc_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_cuc_BITS_FIELD_DIM (16)
#define GAL_TYPE3_cus_FIRST_BYTE_IDX (9)
#define GAL_TYPE3_cus_LAST_BYTE_IDX (10)
#define GAL_TYPE3_cus_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_cus_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_cus_BITS_FIELD_DIM (16)
#define GAL_TYPE3_crc_FIRST_BYTE_IDX (11)
#define GAL_TYPE3_crc_LAST_BYTE_IDX (12)
#define GAL_TYPE3_crc_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_crc_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_crc_BITS_FIELD_DIM (16)
#define GAL_TYPE3_crs_FIRST_BYTE_IDX (13)
#define GAL_TYPE3_crs_LAST_BYTE_IDX (14)
#define GAL_TYPE3_crs_BITS_OF_FIRST_BYTE (8)
#define GAL_TYPE3_crs_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE3_crs_BITS_FIELD_DIM (16)
#define GAL_TYPE4_prn_FIRST_BYTE_IDX (2)
#define GAL_TYPE4_prn_LAST_BYTE_IDX (2)
#define GAL_TYPE4_prn_BITS_OF_FIRST_BYTE (6)
#define GAL_TYPE4_prn_BITS_OF_LAST_BYTE (0)
#define GAL_TYPE4_cic_FIRST_BYTE_IDX (2)
#define GAL_TYPE4_cic_LAST_BYTE_IDX (4)
#define GAL_TYPE4_cic_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE4_cic_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE4_cic_BITS_FIELD_DIM (16)
#define GAL_TYPE4_cis_FIRST_BYTE_IDX (4)
#define GAL_TYPE4_cis_LAST_BYTE_IDX (6)
#define GAL_TYPE4_cis_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE4_cis_BITS_OF_LAST_BYTE (6)
#define GAL_TYPE4_cis_BITS_FIELD_DIM (16)
#define GAL_TYPE4_coc_FIRST_BYTE_IDX (6)
#define GAL_TYPE4_coc_LAST_BYTE_IDX (8)
#define GAL_TYPE4_coc_BITS_OF_FIRST_BYTE (2)
#define GAL_TYPE4_coc_BITS_OF_LAST_BYTE (4)
#define GAL_TYPE4_af0_FIRST_BYTE_IDX (8)
#define GAL_TYPE4_af0_LAST_BYTE_IDX (12)
#define GAL_TYPE4_af0_BITS_OF_FIRST_BYTE (4)
#define GAL_TYPE4_af0_BITS_OF_LAST_BYTE (3)
#define GAL_TYPE4_af0_BITS_FIELD_DIM (31)
#define GAL_TYPE4_af1_FIRST_BYTE_IDX (12)
#define GAL_TYPE4_af1_LAST_BYTE_IDX (14)
#define GAL_TYPE4_af1_BITS_OF_FIRST_BYTE (5)
#define GAL_TYPE4_af1_BITS_OF_LAST_BYTE (8)
#define GAL_TYPE4_af1_BITS_FIELD_DIM (21)
#define GAL_TYPE4_af2_FIRST_BYTE_IDX (15)
#define GAL_TYPE4_af2_LAST_BYTE_IDX (15)
#define GAL_TYPE4_af2_BITS_OF_FIRST_BYTE (6)
#define GAL_TYPE4_af2_BITS_OF_LAST_BYTE (0)
#define GAL_TYPE4_af2_BITS_FIELD_DIM (6)

/** Local view over the already validated payload. */
typedef struct {
    TNgBroadcast *navMsg;
    unsigned char subframe[40], bitsForParse[16];
    int wnCorrection, bitIndexTotal;
} TNgNavParser;

/** Galileo storm/ionosphere view. */
typedef struct {
    double alphaGal[3];
    int sf[5];
} TNgIono;

/** Classic L1 health combinations. */
static int NgGpsL1CaHealthy(unsigned health)
{
    unsigned signal = health & 31;
    return !(health & 32) && signal != 2 && signal != 3 && signal != 23 && signal != 24 &&
           signal != 28 && signal != 31;
}

static double NgExtractTwoCompField(unsigned char dataBytes[], int startByte, int endByte,
                                    int bitsOfFirstByte, int bitsOfLastByte, int numBits)
{
    int msByteShift;
    int lsByteShift;
    unsigned int temp;
    double res;
    /* number of bits of dataBits[startByte] to shift */
    msByteShift = 8 - bitsOfFirstByte;
    /* number of bits of dataBits[endByte] to shift */
    lsByteShift = 8 - bitsOfLastByte;
    /* the bits of the field to extract are on 1 dataBits element */
    if (startByte == endByte) {
        /* take the less-significant bitsOfLastByte bits */
        if (msByteShift == 8) {
            temp = dataBytes[startByte];
            temp = (temp << (24 + lsByteShift)) >> (24 + lsByteShift);
            res = temp;
        }
        /* take the most-significant bitsOfFirstByte bits */
        else {
            res = dataBytes[startByte] >> msByteShift;
        }
    }
    /* the bits of the field to extract are on several (endByte-startByte+1) consecutive dataBits
       element */
    else {
        temp = dataBytes[startByte];
        /* take the bitsOfFirstByte less-significant bits of the dataBits[startByte] and copy in a
         * integer of 32 bit variable (temp) */
        if (msByteShift != 0) {
            temp = (temp << (24 + msByteShift)) >> (24 + msByteShift);
        }
        /* left shift to give space to the consecutive bytes */
        temp = (temp << ((8 * (endByte - startByte)) - lsByteShift));
        /* assignment to the final result */
        res = temp;
        /*
         * if present, take the full bytes in the middle (between dataBits[startByte] and
         * dataBits[endByte]) and put them in the right position inside the 32 bit variable (temp)
         */
        int i;
        for (i = startByte + 1; i < endByte; i++) {
            temp = dataBytes[i];
            temp = (temp << ((8 * (endByte - i)) - lsByteShift));
            res += temp;
        }
        /* take the bitsOfLastByte most-significant bit inside dataBits[endByte] */
        temp = dataBytes[endByte];
        temp = temp >> lsByteShift;
        // sum to the final result
        res += temp;
    }
    /* calculate twos complement */
    if ((ldexp(1.0, numBits - 1) <= res) && (res <= ldexp(1.0, numBits) - 1)) {
        res = res - ldexp(1.0, numBits);
    }
    return res;
}

static double NgExtractField(unsigned char dataBytes[], int startByte, int endByte,
                             int bitsOfFirstByte, int bitsOfLastByte)
{
    int msByteShift;
    int lsByteShift;
    unsigned int temp;
    double res;
    /* number of bits of dataBits[startByte] to shift */
    msByteShift = 8 - bitsOfFirstByte;
    /* number of bits of dataBits[endByte] to shift */
    lsByteShift = 8 - bitsOfLastByte;
    /* the bits of the field to extract are on 1 dataBits element */
    if (startByte == endByte) {
        /* take the less-significant bitsOfLastByte bits */
        if (msByteShift == 8) {
            // res = dataBits[startByte] >> lsByteShift;
            temp = dataBytes[startByte];
            temp = (temp << (24 + lsByteShift)) >> (24 + lsByteShift);
            res = temp;
        }
        /* take the most-significant bitsOfFirstByte bits */
        else {
            temp = (unsigned int)(dataBytes[startByte] >> msByteShift);
            if (lsByteShift < 8) {
                temp =
                    (temp << (24 + lsByteShift + msByteShift)) >> (24 + lsByteShift + msByteShift);
            }
            res = temp;
        }
    }
    /* the bits of the field to extract are on several (endByte-startByte+1) consecutive dataBits
       element */
    else {
        temp = dataBytes[startByte];
        /* take the bitsOfFirstByte less-significant bits of the dataBits[startByte] and copy in a
         * integer of 32 bit variable (temp) */
        if (msByteShift != 0) {
            temp = (temp << (24 + msByteShift)) >> (24 + msByteShift);
        }
        /* left shift to give space to the consecutive bytes */
        temp = (temp << ((8 * (endByte - startByte)) - lsByteShift));
        /* assignment to the final result */
        res = temp;
        /*
         * if present, take the full bytes in the middle (between dataBits[startByte] and
         * dataBits[endByte]) and put them in the right position inside the 32 bit variable (temp)
         */
        int i;
        for (i = startByte + 1; i < endByte; i++) {
            temp = dataBytes[i];
            temp = (temp << ((8 * (endByte - i)) - lsByteShift));
            /* sum to the final result */
            res += temp;
        }
        /* take the bitsOfLastByte most-significant bit inside dataBits[endByte] */
        temp = dataBytes[endByte];
        temp = temp >> lsByteShift;
        /* sum to the final result */
        res += temp;
    }
    return res;
}

static int NgParseGpsSubframe1(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    int health;
    TNgOrbit *eph = &(navMsg->ephTmp);
    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    navMsg->wn = (int)(NgExtractField(parser->subframe, GPS_SUBF1_WN_FIRST_BYTE_IDX,
                                      GPS_SUBF1_WN_LAST_BYTE_IDX, GPS_SUBF1_WN_BITS_OF_FIRST_BYTE,
                                      GPS_SUBF1_WN_BITS_OF_LAST_BYTE));
    navMsg->wn += (int)(parser->wnCorrection);
    navMsg->wnValid = TRUE_FLAG;
    health = (int)(NgExtractField(
        parser->subframe, GPS_SUBF1_HEALTH_FIRST_BYTE_IDX, GPS_SUBF1_HEALTH_LAST_BYTE_IDX,
        GPS_SUBF1_HEALTH_BITS_OF_FIRST_BYTE, GPS_SUBF1_HEALTH_BITS_OF_LAST_BYTE));
    if (NgGpsL1CaHealthy((unsigned)health)) {
        eph->health = (int)(SIGNAL_OK);
    } else {
        eph->health = (int)(SIGNAL_OUT_OF_SERVICE);
    }
    eph->sisa = (int)(NgExtractField(parser->subframe, GPS_SUBF1_URA_FIRST_BYTE_IDX,
                                     GPS_SUBF1_URA_LAST_BYTE_IDX, GPS_SUBF1_URA_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF1_URA_BITS_OF_LAST_BYTE));
    clk->iodc = (int)(256.0 * NgExtractField(parser->subframe, GPS_SUBF1_IODC_MS_FIRST_BYTE_IDX,
                                             GPS_SUBF1_IODC_MS_LAST_BYTE_IDX,
                                             GPS_SUBF1_IODC_MS_BITS_OF_FIRST_BYTE,
                                             GPS_SUBF1_IODC_MS_BITS_OF_LAST_BYTE));
    clk->iodc += (int)(NgExtractField(
        parser->subframe, GPS_SUBF1_IODC_LS_FIRST_BYTE_IDX, GPS_SUBF1_IODC_LS_LAST_BYTE_IDX,
        GPS_SUBF1_IODC_LS_BITS_OF_FIRST_BYTE, GPS_SUBF1_IODC_LS_BITS_OF_LAST_BYTE));
    clk->toc = 16.0 * NgExtractField(parser->subframe, GPS_SUBF1_TOC_FIRST_BYTE_IDX,
                                     GPS_SUBF1_TOC_LAST_BYTE_IDX, GPS_SUBF1_TOC_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF1_TOC_BITS_OF_LAST_BYTE);
    clk->af[0] = POW_TWO_M31 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF0_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF0_LAST_BYTE_IDX, GPS_SUBF1_AF0_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF0_BITS_OF_LAST_BYTE, GPS_SUBF1_AF0_FIELD_DIM);
    clk->af[1] = POW_TWO_M43 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF1_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF1_LAST_BYTE_IDX, GPS_SUBF1_AF1_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF1_BITS_OF_LAST_BYTE, GPS_SUBF1_AF1_FIELD_DIM);
    clk->af[2] = POW_TWO_M55 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF2_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF2_LAST_BYTE_IDX, GPS_SUBF1_AF2_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF2_BITS_OF_LAST_BYTE, GPS_SUBF1_AF2_FIELD_DIM);
    clk->tGroupDelay =
        POW_TWO_M31 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF1_TGD_FIRST_BYTE_IDX,
                              GPS_SUBF1_TGD_LAST_BYTE_IDX, GPS_SUBF1_TGD_BITS_OF_FIRST_BYTE,
                              GPS_SUBF1_TGD_BITS_OF_LAST_BYTE, GPS_SUBF1_TGD_FIELD_DIM);
    if (clk->toc > GPS_MAXIMUM_TOC) {
        return FAILURE_RETURN;
    }
    navMsg->subframe1Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGpsSubframe2(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode2 = (int)(NgExtractField(
        parser->subframe, GPS_SUBF2_IODE_FIRST_BYTE_IDX, GPS_SUBF2_IODE_LAST_BYTE_IDX,
        GPS_SUBF2_IODE_BITS_OF_FIRST_BYTE, GPS_SUBF2_IODE_BITS_OF_LAST_BYTE));
    eph->capitalCrs =
        POW_TWO_M05 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CRS_FIRST_BYTE_IDX,
                              GPS_SUBF2_CRS_LAST_BYTE_IDX, GPS_SUBF2_CRS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CRS_BITS_OF_LAST_BYTE, GPS_SUBF2_CRS_FIELD_DIM);
    eph->deltaN = POW_TWO_M43 * NG_BROADCAST_PI *
                  NgExtractTwoCompField(parser->subframe, GPS_SUBF2_DN_FIRST_BYTE_IDX,
                                        GPS_SUBF2_DN_LAST_BYTE_IDX, GPS_SUBF2_DN_BITS_OF_FIRST_BYTE,
                                        GPS_SUBF2_DN_BITS_OF_LAST_BYTE, GPS_SUBF2_DN_FIELD_DIM);
    eph->meanAnomaly0 =
        POW_TWO_P24 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_M0_MS_FIRST_BYTE_IDX,
                              GPS_SUBF2_M0_MS_LAST_BYTE_IDX, GPS_SUBF2_M0_MS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_M0_MS_BITS_OF_LAST_BYTE, GPS_SUBF2_M0_MS_FIELD_DIM);
    eph->meanAnomaly0 += NgExtractField(
        parser->subframe, GPS_SUBF2_M0_LS_FIRST_BYTE_IDX, GPS_SUBF2_M0_LS_LAST_BYTE_IDX,
        GPS_SUBF2_M0_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_M0_LS_BITS_OF_LAST_BYTE);
    eph->meanAnomaly0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCuc =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CUC_FIRST_BYTE_IDX,
                              GPS_SUBF2_CUC_LAST_BYTE_IDX, GPS_SUBF2_CUC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CUC_BITS_OF_LAST_BYTE, GPS_SUBF2_CUC_FIELD_DIM);
    eph->eccentricity =
        POW_TWO_P24 * NgExtractField(parser->subframe, GPS_SUBF2_E_MS_FIRST_BYTE_IDX,
                                     GPS_SUBF2_E_MS_LAST_BYTE_IDX,
                                     GPS_SUBF2_E_MS_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF2_E_MS_BITS_OF_LAST_BYTE);
    eph->eccentricity += NgExtractField(
        parser->subframe, GPS_SUBF2_E_LS_FIRST_BYTE_IDX, GPS_SUBF2_E_LS_LAST_BYTE_IDX,
        GPS_SUBF2_E_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_E_LS_BITS_OF_LAST_BYTE);
    eph->eccentricity *= POW_TWO_M33;
    eph->capitalCus =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CUS_FIRST_BYTE_IDX,
                              GPS_SUBF2_CUS_LAST_BYTE_IDX, GPS_SUBF2_CUS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CUS_BITS_OF_LAST_BYTE, GPS_SUBF2_CUS_FIELD_DIM);
    eph->sqrtA = POW_TWO_P24 * NgExtractField(parser->subframe, GPS_SUBF2_SQRTA_MS_FIRST_BYTE_IDX,
                                              GPS_SUBF2_SQRTA_MS_LAST_BYTE_IDX,
                                              GPS_SUBF2_SQRTA_MS_BITS_OF_FIRST_BYTE,
                                              GPS_SUBF2_SQRTA_MS_BITS_OF_LAST_BYTE);
    eph->sqrtA += NgExtractField(
        parser->subframe, GPS_SUBF2_SQRTA_LS_FIRST_BYTE_IDX, GPS_SUBF2_SQRTA_LS_LAST_BYTE_IDX,
        GPS_SUBF2_SQRTA_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_SQRTA_LS_BITS_OF_LAST_BYTE);
    eph->sqrtA *= POW_TWO_M19;
    eph->toe = 16.0 * NgExtractField(parser->subframe, GPS_SUBF2_TOE_FIRST_BYTE_IDX,
                                     GPS_SUBF2_TOE_LAST_BYTE_IDX, GPS_SUBF2_TOE_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF2_TOE_BITS_OF_LAST_BYTE);
    if (eph->eccentricity > GPS_MAXIMUM_ECCENTRICITY)
        return FAILURE_RETURN;
    if (eph->toe > GPS_MAXIMUM_TOC)
        return FAILURE_RETURN;
    navMsg->subframe2Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGpsSubframe3(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->capitalCic =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CIC_FIRST_BYTE_IDX,
                              GPS_SUBF3_CIC_LAST_BYTE_IDX, GPS_SUBF3_CIC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CIC_BITS_OF_LAST_BYTE, GPS_SUBF3_CIC_FIELD_DIM);
    eph->capitalOmega0 =
        POW_TWO_P24 * NgExtractTwoCompField(
                          parser->subframe, GPS_SUBF3_OMEGA0_MS_FIRST_BYTE_IDX,
                          GPS_SUBF3_OMEGA0_MS_LAST_BYTE_IDX, GPS_SUBF3_OMEGA0_MS_BITS_OF_FIRST_BYTE,
                          GPS_SUBF3_OMEGA0_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGA0_MS_FIELD_DIM);
    eph->capitalOmega0 += NgExtractField(
        parser->subframe, GPS_SUBF3_OMEGA0_LS_FIRST_BYTE_IDX, GPS_SUBF3_OMEGA0_LS_LAST_BYTE_IDX,
        GPS_SUBF3_OMEGA0_LS_BITS_OF_FIRST_BYTE, GPS_SUBF3_OMEGA0_LS_BITS_OF_LAST_BYTE);
    eph->capitalOmega0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCis =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CIS_FIRST_BYTE_IDX,
                              GPS_SUBF3_CIS_LAST_BYTE_IDX, GPS_SUBF3_CIS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CIS_BITS_OF_LAST_BYTE, GPS_SUBF3_CIS_FIELD_DIM);
    eph->i0 = POW_TWO_P24 * NgExtractTwoCompField(
                                parser->subframe, GPS_SUBF3_I0_MS_FIRST_BYTE_IDX,
                                GPS_SUBF3_I0_MS_LAST_BYTE_IDX, GPS_SUBF3_I0_MS_BITS_OF_FIRST_BYTE,
                                GPS_SUBF3_I0_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_I0_MS_FIELD_DIM);
    eph->i0 += NgExtractField(parser->subframe, GPS_SUBF3_I0_LS_FIRST_BYTE_IDX,
                              GPS_SUBF3_I0_LS_LAST_BYTE_IDX, GPS_SUBF3_I0_LS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_I0_LS_BITS_OF_LAST_BYTE);
    eph->i0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCrc =
        POW_TWO_M05 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CRC_FIRST_BYTE_IDX,
                              GPS_SUBF3_CRC_LAST_BYTE_IDX, GPS_SUBF3_CRC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CRC_BITS_OF_LAST_BYTE, GPS_SUBF3_CRC_FIELD_DIM);
    eph->omega =
        POW_TWO_P24 * NgExtractTwoCompField(
                          parser->subframe, GPS_SUBF3_OMEGA_MS_FIRST_BYTE_IDX,
                          GPS_SUBF3_OMEGA_MS_LAST_BYTE_IDX, GPS_SUBF3_OMEGA_MS_BITS_OF_FIRST_BYTE,
                          GPS_SUBF3_OMEGA_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGA_MS_FIELD_DIM);
    eph->omega += NgExtractField(
        parser->subframe, GPS_SUBF3_OMEGA_LS_FIRST_BYTE_IDX, GPS_SUBF3_OMEGA_LS_LAST_BYTE_IDX,
        GPS_SUBF3_OMEGA_LS_BITS_OF_FIRST_BYTE, GPS_SUBF3_OMEGA_LS_BITS_OF_LAST_BYTE);
    eph->omega *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalOmegaDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_OMEGADOT_FIRST_BYTE_IDX,
                              GPS_SUBF3_OMEGADOT_LAST_BYTE_IDX,
                              GPS_SUBF3_OMEGADOT_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_OMEGADOT_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGADOT_FIELD_DIM);
    eph->iode3 = (int)(NgExtractField(
        parser->subframe, GPS_SUBF3_IODE_FIRST_BYTE_IDX, GPS_SUBF3_IODE_LAST_BYTE_IDX,
        GPS_SUBF3_IODE_BITS_OF_FIRST_BYTE, GPS_SUBF3_IODE_BITS_OF_LAST_BYTE));
    eph->iDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_IDOT_FIRST_BYTE_IDX,
                              GPS_SUBF3_IDOT_LAST_BYTE_IDX, GPS_SUBF3_IDOT_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_IDOT_BITS_OF_LAST_BYTE, GPS_SUBF3_IDOT_FIELD_DIM);
    navMsg->subframe3Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGalWord00(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    int time;
    time =
        (int)(NgExtractField(dataBytes, GAL_TYPE0_TIME_FIRST_BYTE_IDX, GAL_TYPE0_TIME_LAST_BYTE_IDX,
                             GAL_TYPE0_TIME_BITS_OF_FIRST_BYTE, GAL_TYPE0_TIME_BITS_OF_LAST_BYTE));
    if (time == 2) {
        /* when the field 'Time' is not set to '10', the fields WN and TOW do not contain valid data
         */
        navMsg->wn =
            (int)(NgExtractField(dataBytes, GAL_TYPE0_WN_FIRST_BYTE_IDX, GAL_TYPE0_WN_LAST_BYTE_IDX,
                                 GAL_TYPE0_WN_BITS_OF_FIRST_BYTE, GAL_TYPE0_WN_BITS_OF_LAST_BYTE));
        navMsg->wn += (int)(parser->wnCorrection);
        navMsg->wnValid = TRUE_FLAG;
        navMsg->tow = (int)(NgExtractField(
            dataBytes, GAL_TYPE0_TOW_FIRST_BYTE_IDX, GAL_TYPE0_TOW_LAST_BYTE_IDX,
            GAL_TYPE0_TOW_BITS_OF_FIRST_BYTE, GAL_TYPE0_TOW_BITS_OF_LAST_BYTE));
        parser->bitIndexTotal = GAL_PAGE_LENGTH;
        navMsg->towValid = TRUE_FLAG;
    }

    return SUCCESS_RETURN;
}

static int NgParseGalWord01(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode1 =
        (int)(NgExtractField(dataBytes, GAL_TYPE1_IOD_FIRST_BYTE_IDX, GAL_TYPE1_IOD_LAST_BYTE_IDX,
                             GAL_TYPE1_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE1_IOD_BITS_OF_LAST_BYTE));
    eph->toe = GAL_TOE_SCALE_FACTOR *
               NgExtractField(dataBytes, GAL_TYPE1_toe_FIRST_BYTE_IDX, GAL_TYPE1_toe_LAST_BYTE_IDX,
                              GAL_TYPE1_toe_BITS_OF_FIRST_BYTE, GAL_TYPE1_toe_BITS_OF_LAST_BYTE);
    eph->meanAnomaly0 =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE1_M0_FIRST_BYTE_IDX, GAL_TYPE1_M0_LAST_BYTE_IDX,
                              GAL_TYPE1_M0_BITS_OF_FIRST_BYTE, GAL_TYPE1_M0_BITS_OF_LAST_BYTE,
                              GAL_TYPE1_M0_BITS_FIELD_DIM);
    eph->eccentricity =
        POW_TWO_M33 * NgExtractField(dataBytes, GAL_TYPE1_e_FIRST_BYTE_IDX,
                                     GAL_TYPE1_e_LAST_BYTE_IDX, GAL_TYPE1_e_BITS_OF_FIRST_BYTE,
                                     GAL_TYPE1_e_BITS_OF_LAST_BYTE);
    eph->sqrtA = POW_TWO_M19 * NgExtractField(dataBytes, GAL_TYPE1_sqrtA_FIRST_BYTE_IDX,
                                              GAL_TYPE1_sqrtA_LAST_BYTE_IDX,
                                              GAL_TYPE1_sqrtA_BITS_OF_FIRST_BYTE,
                                              GAL_TYPE1_sqrtA_BITS_OF_LAST_BYTE);
    navMsg->subframe1Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGalWord02(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode2 =
        (int)(NgExtractField(dataBytes, GAL_TYPE2_IOD_FIRST_BYTE_IDX, GAL_TYPE2_IOD_LAST_BYTE_IDX,
                             GAL_TYPE2_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE2_IOD_BITS_OF_LAST_BYTE));
    eph->capitalOmega0 =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_omega0_FIRST_BYTE_IDX,
                              GAL_TYPE2_omega0_LAST_BYTE_IDX, GAL_TYPE2_omega0_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_omega0_BITS_OF_LAST_BYTE, GAL_TYPE2_omega0_BITS_FIELD_DIM);
    eph->i0 = POW_TWO_M31 * NG_BROADCAST_PI *
              NgExtractTwoCompField(dataBytes, GAL_TYPE2_i0_FIRST_BYTE_IDX,
                                    GAL_TYPE2_i0_LAST_BYTE_IDX, GAL_TYPE2_i0_BITS_OF_FIRST_BYTE,
                                    GAL_TYPE2_i0_BITS_OF_LAST_BYTE, GAL_TYPE2_i0_BITS_FIELD_DIM);
    eph->omega =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_omega_FIRST_BYTE_IDX,
                              GAL_TYPE2_omega_LAST_BYTE_IDX, GAL_TYPE2_omega_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_omega_BITS_OF_LAST_BYTE, GAL_TYPE2_omega_BITS_FIELD_DIM);
    eph->iDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_idot_FIRST_BYTE_IDX,
                              GAL_TYPE2_idot_LAST_BYTE_IDX, GAL_TYPE2_idot_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_idot_BITS_OF_LAST_BYTE, GAL_TYPE2_idot_BITS_FIELD_DIM);
    navMsg->subframe2Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGalWord03(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode3 =
        (int)(NgExtractField(dataBytes, GAL_TYPE3_IOD_FIRST_BYTE_IDX, GAL_TYPE3_IOD_LAST_BYTE_IDX,
                             GAL_TYPE3_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE3_IOD_BITS_OF_LAST_BYTE));
    eph->capitalOmegaDot = POW_TWO_M43 * NG_BROADCAST_PI *
                           NgExtractTwoCompField(dataBytes, GAL_TYPE3_omegadot_FIRST_BYTE_IDX,
                                                 GAL_TYPE3_omegadot_LAST_BYTE_IDX,
                                                 GAL_TYPE3_omegadot_BITS_OF_FIRST_BYTE,
                                                 GAL_TYPE3_omegadot_BITS_OF_LAST_BYTE,
                                                 GAL_TYPE3_omegadot_BITS_FIELD_DIM);
    eph->deltaN =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE3_deltan_FIRST_BYTE_IDX,
                              GAL_TYPE3_deltan_LAST_BYTE_IDX, GAL_TYPE3_deltan_BITS_OF_FIRST_BYTE,
                              GAL_TYPE3_deltan_BITS_OF_LAST_BYTE, GAL_TYPE3_deltan_BITS_FIELD_DIM);
    eph->capitalCuc = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_cuc_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_cuc_LAST_BYTE_IDX,
                                                          GAL_TYPE3_cuc_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_cuc_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_cuc_BITS_FIELD_DIM);
    eph->capitalCus = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_cus_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_cus_LAST_BYTE_IDX,
                                                          GAL_TYPE3_cus_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_cus_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_cus_BITS_FIELD_DIM);
    eph->capitalCrc = POW_TWO_M05 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_crc_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_crc_LAST_BYTE_IDX,
                                                          GAL_TYPE3_crc_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_crc_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_crc_BITS_FIELD_DIM);
    eph->capitalCrs = POW_TWO_M05 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_crs_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_crs_LAST_BYTE_IDX,
                                                          GAL_TYPE3_crs_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_crs_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_crs_BITS_FIELD_DIM);
    eph->sisa = (int)((
        NgExtractField(dataBytes, GAL_TYPE3_SISA_FIRST_BYTE_IDX, GAL_TYPE3_SISA_LAST_BYTE_IDX,
                       GAL_TYPE3_SISA_BITS_OF_FIRST_BYTE, GAL_TYPE3_SISA_BITS_OF_LAST_BYTE)));
    navMsg->subframe3Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGalWord04(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    int svId;
    TNgOrbit *eph = &(navMsg->ephTmp);
    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    eph->iode4 =
        (int)(NgExtractField(dataBytes, GAL_TYPE4_IOD_FIRST_BYTE_IDX, GAL_TYPE4_IOD_LAST_BYTE_IDX,
                             GAL_TYPE4_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE4_IOD_BITS_OF_LAST_BYTE));
    svId =
        (int)(NgExtractField(dataBytes, GAL_TYPE4_prn_FIRST_BYTE_IDX, GAL_TYPE4_prn_LAST_BYTE_IDX,
                             GAL_TYPE4_prn_BITS_OF_FIRST_BYTE, GAL_TYPE4_prn_BITS_OF_LAST_BYTE));
    navMsg->satelliteId = svId;
    eph->capitalCic = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE4_cic_FIRST_BYTE_IDX,
                                                          GAL_TYPE4_cic_LAST_BYTE_IDX,
                                                          GAL_TYPE4_cic_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE4_cic_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE4_cic_BITS_FIELD_DIM);
    eph->capitalCis = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE4_cis_FIRST_BYTE_IDX,
                                                          GAL_TYPE4_cis_LAST_BYTE_IDX,
                                                          GAL_TYPE4_cis_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE4_cis_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE4_cis_BITS_FIELD_DIM);
    clk->toc =
        60 * NgExtractField(dataBytes, GAL_TYPE4_coc_FIRST_BYTE_IDX, GAL_TYPE4_coc_LAST_BYTE_IDX,
                            GAL_TYPE4_coc_BITS_OF_FIRST_BYTE, GAL_TYPE4_coc_BITS_OF_LAST_BYTE);
    clk->af[0] = POW_TWO_M34 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_af0_FIRST_BYTE_IDX,
                                   GAL_TYPE4_af0_LAST_BYTE_IDX, GAL_TYPE4_af0_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_af0_BITS_OF_LAST_BYTE, GAL_TYPE4_af0_BITS_FIELD_DIM);
    clk->af[1] = POW_TWO_M46 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_af1_FIRST_BYTE_IDX,
                                   GAL_TYPE4_af1_LAST_BYTE_IDX, GAL_TYPE4_af1_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_af1_BITS_OF_LAST_BYTE, GAL_TYPE4_af1_BITS_FIELD_DIM);
    clk->af[2] = POW_TWO_M59 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_af2_FIRST_BYTE_IDX,
                                   GAL_TYPE4_af2_LAST_BYTE_IDX, GAL_TYPE4_af2_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_af2_BITS_OF_LAST_BYTE, GAL_TYPE4_af2_BITS_FIELD_DIM);
    navMsg->subframe4Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

static int NgParseGalWord05(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);

    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    int flag;
    navMsg->iono.alphaGal[0] =
        POW_TWO_M02 * NgExtractField(dataBytes, GAL_TYPE5_A0I_FIRST_BYTE_IDX,
                                     GAL_TYPE5_A0I_LAST_BYTE_IDX, GAL_TYPE5_A0I_BITS_OF_FIRST_BYTE,
                                     GAL_TYPE5_A0I_BITS_OF_LAST_BYTE);
    navMsg->iono.alphaGal[1] =
        POW_TWO_M08 *
        NgExtractTwoCompField(dataBytes, GAL_TYPE5_A1I_FIRST_BYTE_IDX, GAL_TYPE5_A1I_LAST_BYTE_IDX,
                              GAL_TYPE5_A1I_BITS_OF_FIRST_BYTE, GAL_TYPE5_A1I_BITS_OF_LAST_BYTE,
                              GAL_TYPE5_A1I_BITS_FIELD_DIM);
    navMsg->iono.alphaGal[2] =
        POW_TWO_M15 *
        NgExtractTwoCompField(dataBytes, GAL_TYPE5_A2I_FIRST_BYTE_IDX, GAL_TYPE5_A2I_LAST_BYTE_IDX,
                              GAL_TYPE5_A2I_BITS_OF_FIRST_BYTE, GAL_TYPE5_A2I_BITS_OF_LAST_BYTE,
                              GAL_TYPE5_A2I_BITS_FIELD_DIM);
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF1_FIRST_BYTE_IDX, GAL_TYPE5_SF1_LAST_BYTE_IDX,
                               GAL_TYPE5_SF1_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF1_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[0] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF2_FIRST_BYTE_IDX, GAL_TYPE5_SF2_LAST_BYTE_IDX,
                               GAL_TYPE5_SF2_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF2_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[1] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF3_FIRST_BYTE_IDX, GAL_TYPE5_SF3_LAST_BYTE_IDX,
                               GAL_TYPE5_SF3_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF3_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[2] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF4_FIRST_BYTE_IDX, GAL_TYPE5_SF4_LAST_BYTE_IDX,
                               GAL_TYPE5_SF4_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF4_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[3] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF5_FIRST_BYTE_IDX, GAL_TYPE5_SF5_LAST_BYTE_IDX,
                               GAL_TYPE5_SF5_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF5_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[4] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    clk->bgd_E1E5a =
        POW_TWO_M32 * NgExtractTwoCompField(dataBytes, GAL_TYPE5_TGD_E1E5A_FIRST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5A_LAST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5A_BITS_OF_FIRST_BYTE,
                                            GAL_TYPE5_TGD_E1E5A_BITS_OF_LAST_BYTE,
                                            GAL_TYPE5_TGD_E1E5A_BITS_FIELD_DIM);
    clk->bgd_E1E5b =
        POW_TWO_M32 * NgExtractTwoCompField(dataBytes, GAL_TYPE5_TGD_E1E5B_FIRST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5B_LAST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5B_BITS_OF_FIRST_BYTE,
                                            GAL_TYPE5_TGD_E1E5B_BITS_OF_LAST_BYTE,
                                            GAL_TYPE5_TGD_E1E5B_BITS_FIELD_DIM);
    clk->tGroupDelay = clk->bgd_E1E5b;
    navMsg->wn =
        (int)(NgExtractField(dataBytes, GAL_TYPE5_WN_FIRST_BYTE_IDX, GAL_TYPE5_WN_LAST_BYTE_IDX,
                             GAL_TYPE5_WN_BITS_OF_FIRST_BYTE, GAL_TYPE5_WN_BITS_OF_LAST_BYTE));
    navMsg->wn += (int)(parser->wnCorrection);
    navMsg->wnValid = TRUE_FLAG;
    navMsg->tow =
        (int)(NgExtractField(dataBytes, GAL_TYPE5_TOW_FIRST_BYTE_IDX, GAL_TYPE5_TOW_LAST_BYTE_IDX,
                             GAL_TYPE5_TOW_BITS_OF_FIRST_BYTE, GAL_TYPE5_TOW_BITS_OF_LAST_BYTE));
    navMsg->towValid = TRUE_FLAG;
    parser->bitIndexTotal = GAL_PAGE_LENGTH;
    eph->health = (int)(NgExtractField(
        dataBytes, GAL_TYPE5_E1BHS_FIRST_BYTE_IDX, GAL_TYPE5_E1BHS_LAST_BYTE_IDX,
        GAL_TYPE5_E1BHS_BITS_OF_FIRST_BYTE, GAL_TYPE5_E1BHS_BITS_OF_LAST_BYTE));
    eph->galileoDataValidity = (int)(NgExtractField(
        dataBytes, GAL_TYPE5_E1BDVS_FIRST_BYTE_IDX, GAL_TYPE5_E1BDVS_LAST_BYTE_IDX,
        GAL_TYPE5_E1BDVS_BITS_OF_FIRST_BYTE, GAL_TYPE5_E1BDVS_BITS_OF_LAST_BYTE));
    navMsg->subframe5Received = TRUE_FLAG;
    navMsg->ionoReceived = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Commit a complete broadcast issue without mixing orbit versions. */
static void Commit(TNgBroadcast *b, int galileo)
{
    int complete = b->subframe1Received && b->subframe2Received && b->subframe3Received;
    if (galileo)
        complete = complete && b->subframe4Received && b->subframe5Received &&
                   b->ephTmp.iode1 == b->ephTmp.iode2 && b->ephTmp.iode2 == b->ephTmp.iode4;
    else
        complete = complete && ((b->clkTmp.iodc & 255) == b->ephTmp.iode2);
    complete = complete && b->ephTmp.iode2 == b->ephTmp.iode3;
    if (complete && isfinite(b->ephTmp.sqrtA) && b->ephTmp.sqrtA > 4000 && b->ephTmp.sqrtA < 7000 &&
        b->ephTmp.eccentricity >= 0 && b->ephTmp.eccentricity < 1 && b->ephTmp.toe >= 0 &&
        b->ephTmp.toe < 604800 && b->clkTmp.toc >= 0 && b->clkTmp.toc < 604800) {
        b->eph = b->ephTmp;
        b->clk = b->clkTmp;
        b->ephemeridesReceived = 1;
    }
}

/** Extract parity-validated LNAV subframe 4, page 18 ionosphere coefficients. */
static void NgParseGpsIono(TNgNavParser *p)
{
    static const unsigned first[8] = {9, 10, 12, 13, 14, 16, 17, 18};
    static const int shift[8] = {-30, -27, -24, -24, 11, 14, 16, 16};
    TNgBroadcast *b = p->navMsg;
    unsigned page = (unsigned)NgExtractField(p->subframe, 8, 9, 4, 2);
    unsigned dataId = (unsigned)NgExtractField(p->subframe, 8, 8, 4, 6);
    unsigned k;
    if (dataId != 1 || page != 56)
        return;
    for (k = 0; k < 8; ++k) {
        double value = ldexp((double)NgExtractTwoCompField(p->subframe, first[k], first[k] + 1,
                                                            6, 2, 8), shift[k]);
        if (k < 4)
            b->iono.alphaGps[k] = value;
        else
            b->iono.betaGps[k - 4] = value;
    }
    b->ionoReceived = 1;
}

/** Retain Classic GPS field positions, including word parity gaps. */
int NgBroadcastGps(TNgBroadcast *broadcast, const uint32_t words[10], unsigned id)
{
    TNgNavParser p = {0};
    int result = 0;
    unsigned k;
    p.navMsg = broadcast;
    for (k = 0; k < 10; k++) {
        p.subframe[4 * k] = (unsigned char)(words[k] >> 24);
        p.subframe[4 * k + 1] = (unsigned char)(words[k] >> 16);
        p.subframe[4 * k + 2] = (unsigned char)(words[k] >> 8);
        p.subframe[4 * k + 3] = (unsigned char)words[k];
    }
    if (id == 1)
        result = NgParseGpsSubframe1(&p);
    else if (id == 2)
        result = NgParseGpsSubframe2(&p);
    else if (id == 3)
        result = NgParseGpsSubframe3(&p);
    else if (id == 4)
        NgParseGpsIono(&p);
    if (!result)
        Commit(broadcast, 0);
    return result;
}

/** Retain Classic Galileo word fields; optional authentication and RS are separate modules. */
int NgBroadcastGalileo(TNgBroadcast *broadcast, const unsigned char data[16])
{
    TNgNavParser p = {0};
    unsigned type = data[0] >> 2;
    p.navMsg = broadcast;
    memcpy(p.bitsForParse, data, 16);
    switch (type) {
    case 0:
        NgParseGalWord00(&p);
        break;
    case 1:
        NgParseGalWord01(&p);
        break;
    case 2:
        NgParseGalWord02(&p);
        break;
    case 3:
        NgParseGalWord03(&p);
        break;
    case 4:
        NgParseGalWord04(&p);
        break;
    case 5:
        NgParseGalWord05(&p);
        break;
    default:
        break;
    }
    Commit(broadcast, 1);
    return broadcast->towValid && (type == 5 || (type == 0 && (data[0] & 3u) == 2));
}
