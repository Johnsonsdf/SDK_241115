/**
 * @copyright (c) 2003 - 2022, Goodix Co., Ltd. All rights reserved.
 *
 * @file    gh3x2x_drv_soft_led_agc.h
 *
 * @brief   gh3x2x led agc process functions
 *
 * @version ref gh3x2x_drv_version.h
 *
 */


#ifndef _GH3X2X_DRV_SOFT_LED_AGC_H_
#define _GH3X2X_DRV_SOFT_LED_AGC_H_
 
 
#include "gh_drv_config.h"
#include "gh_drv.h"

#define  GH3X2X_LED_AGC_DISABLE             (0)
#define  GH3X2X_LED_AGC_ENABLE              (1)


/// led agc adc num max
#define  GH3X2X_AGC_ADC_NUM_MAX             (4u)

/// led agc gain limit number
#define  GH3X2X_AGC_LIMIT_NUM_MAX           (4u)

/// raw data base line value
#define  GH3X2X_DATA_BASE_LINE              (8388608)   //2^23 

/// sys sample rate, 32kHz
#define  GH3X2X_SYS_SAMPLE_RATE             (32000)
#define GH3X2X_FASTEST_SAMPLERATE_ADDR          0x0004

/**
 * @brief soft lead result
 */
typedef struct
{
    GU8 uchAmbSampleEn  : 2;                /**< AMB sampling enable */
    GU8 uchAmbSlot      : 3;                /**< slot of amb sampling */
    GU8 uchRes          : 3;                /**< reserved */
} STAmbSlotCtrl;
 
 /**
 * @brief gain limit
 */
typedef struct
{
    GU8 uchGainLimitBg32uA  : 4;          /**< gain limit bg current */
    GU8 uchGainLimitBg64uA  : 4;          
    GU8 uchGainLimitBg128uA : 4;        
    GU8 uchGainLimitBg256uA : 4;
} STSoftAgcGainLimit;

 /**
  * @brief soft agc parameter
  */
typedef struct
{
    GU8 uchSlotxSoftAgcAdjEn;              /**< soft agc enable */
    GU8 uchSlotxSoftBgAdjEn;               /**< soft bg cancel adjust enable */
    STAmbSlotCtrl stAmbSlotCtrl;           /**< amb slot ctrl */
    GU8 uchRes0;                           /**< reserved */
    GU8 uchSpo2RedSlotCfgEn;
    GU8 uchSpo2IrSlotCfgEn;
    STSoftAgcGainLimit stSoftAgcGainLimit; /**< soft gain limit */
    GU32 unAgcTrigThd_H;                   /**< trig threshold(high) of soft agc */
    GU32 unAgcTrigThd_L;                   /**< trig threshold(low) of soft agc */
    GU32 unAgcRestrainThd_H;               /**< restrain threshold(high) of soft agc */
    GU32 unAgcRestrainThd_L;               /**< restrain threshold(low) of soft agc */
} STSoftAgcCfg;


void GH3X2X_LedAgcReset(void);
void GH3X2X_LedAgcPramWrite(GU16 usRegAddr, GU16 usRegData);
void GH3X2X_LedAgcInit(STGh3x2xAgcInfo *STGh3x2xAgcInfo);
void GH3X2X_LedAgcProcess(GU8* puchReadFifoBuffer, GU16 usFifoLen, STGh3x2xAgcInfo *STGh3x2xAgcInfo);
void GH3X2X_LedAgc_Close(void);
void GH3X2X_LedAgc_Open(void);



#endif  // _GH3X2X_DRV_SOFT_LED_AGC_H_

/********END OF FILE********* Copyright (c) 2003 - 2022, Goodix Co., Ltd. ********/
