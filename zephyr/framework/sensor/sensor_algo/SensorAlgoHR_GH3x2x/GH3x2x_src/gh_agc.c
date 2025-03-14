/**
 * @copyright (c) 2003 - 2022, Goodix Co., Ltd. All rights reserved.
 *
 * @file    gh3x2x_drv_soft_led_agc.c
 *
 * @brief   gh3x2x led agc process functions
 *
 * @version see gh3x2x_drv_version.h
 *
 */
 
#include <stdio.h>
#include "gh_drv_version.h"
#include "gh_drv_common.h"
#include "gh_drv.h"
#include "gh_drv_interface.h"
#include "gh_drv_control.h"
#include "gh_uprotocol.h"
#include "gh_agc.h"

#include "gh_drv_dump.h"
#include "gh_demo_config.h"

//#define __SUPPORT_SOFT_AGC_CONFIG__    1

#define __GH3X2X_AGC_GENERAL_TRIG_H_THD__  ((8388608 + 8388608*0.85f))
#define __GH3X2X_AGC_GENERAL_TRIG_L_THD__  ((8388608 + 8388608*0.15f))
#define __GH3X2X_AGC_GENERAL_IDEAL_VALUE__ ((8388608 + 8388608*0.56f))
#define __GH3X2X_AGC_SPO2_TRIG_H_THD__  ((8388608 + 8388608*0.9f))
#define __GH3X2X_AGC_SPO2_TRIG_L_THD__  ((8388608 + 8388608*0.5f))
#define __GH3X2X_AGC_SPO2_IDEAL_VALUE__  ((8388608 + 8388608*0.7f))
#define __GH3X2X_AGC_CONVERGENCE_LEVEL__  (8)  // The values that can be set are 8, 16 and 32, and the recommended value is 16. The larger the value, the smaller the convergence range of AGC. 

//sub channel

#define GH3X2X_NEW_AGC_SUB_CHNNL_ANAYLSIS_TIME_SEC 1

#define GH3X2X_AGC_REG_AGC_EN                     0
#define GH3X2X_AGC_REG_LED_CURRENT_UP_LIMIT       1
#define GH3X2X_AGC_REG_LED_CURRENT_DOWN_LIMIT     2
#define GH3X2X_AGC_REG_LED_CURRENT_DRV0           3
#define GH3X2X_AGC_REG_LED_CURRENT_DRV1           4
#define GH3X2X_AGC_REG_TIA_GAIN_RX0               5
#define GH3X2X_AGC_REG_TIA_GAIN_RX1               6
#define GH3X2X_AGC_REG_TIA_GAIN_RX2               7
#define GH3X2X_AGC_REG_TIA_GAIN_RX3               8
#define GH3X2X_AGC_REG_AGC_SRC                    9
#define GH3X2X_AGC_REG_BG_CANCEL_RX0              10
#define GH3X2X_AGC_REG_BG_CANCEL_RX1              11
#define GH3X2X_AGC_REG_BG_CANCEL_RX2              12
#define GH3X2X_AGC_REG_BG_CANCEL_RX3              13
#define GH3X2X_AGC_REG_SR_MULIPLIPER              14
#define GH3X2X_AGC_ECG_PPG_RX_EN                  15


#if __SUPPORT_SOFT_AGC_CONFIG__
//main chnl
STGh3x2xNewAgcMainChnlInfo  g_pstGh3x2xNewAgcMainChnlInfoEntity[GH3X2X_NEW_AGC_SLOT_NUM_LIMIT];
STGh3x2xNewAgcMainChnlMeanInfo g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[GH3X2X_NEW_AGC_SLOT_NUM_LIMIT];
STGh3x2xNewAgcMainChnlIdaelAdjInfo g_pstGh3x2xNewAgcMainChnlIdaelAdjInfoEntity[GH3X2X_NEW_AGC_SLOT_NUM_LIMIT];
//sub chnl
#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
STGh3x2xNewAgcSubChnlRxInfo    g_pstGh3x2xNewAgcSubChnlRxInfoEntity[GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT];
#endif


STGh3x2xNewAgcMainChnlSat g_pstGh3x2xNewAgcMainChnlSat;
const GU32 g_unNewAgcSpo2Trig_H_Thd = __GH3X2X_AGC_SPO2_TRIG_H_THD__;
const GU32 g_unNewAgcSpo2Trig_L_Thd = __GH3X2X_AGC_SPO2_TRIG_L_THD__;
const GU32 g_unNewAgcSpo2Ideal_value = __GH3X2X_AGC_SPO2_IDEAL_VALUE__;
const GU32 g_unNewAgcGeneTrig_H_Thd = __GH3X2X_AGC_GENERAL_TRIG_H_THD__;
const GU32 g_unNewAgcGeneTrig_L_Thd = __GH3X2X_AGC_GENERAL_TRIG_L_THD__;
const GU32 g_unNewAgcGeneIdeal_value = __GH3X2X_AGC_GENERAL_IDEAL_VALUE__;
const GU8  g_uchAgcConvergence_level = __GH3X2X_AGC_CONVERGENCE_LEVEL__;

// soft agc parameter
static STSoftAgcCfg g_stSoftAgcCfg;
extern const GU16 g_usGH3x2xSlotRegBase[8];
extern const STGh3x2xAgcReg g_pstGH3x2xAgcReg[];
extern const GU16 g_usGH3x2xTiaGainR[];

GU8 g_Gh3x2xNewAgcInitFlag = 0;

void GH3X2X_StoreRxTiaGainAfterAgc(GU8 uchGain, GU8 uchSlotNo, GU8 uchAdcNo, GU8* puchGainRecord)
{
    puchGainRecord[uchSlotNo*4 + uchAdcNo] = uchGain;
}

void GH3X2X_StoreDrvCurrentAfterAgc(GU8 DrvCurrent, GU8 uchSlotNo, GU8 uchDrvIndex, GU16* puchDrvCurrentRecord)
{
	if(!uchDrvIndex)
	{
		puchDrvCurrentRecord[uchSlotNo] = (puchDrvCurrentRecord[uchSlotNo]&0xFF00)|DrvCurrent;
	}
	else
	{
	  puchDrvCurrentRecord[uchSlotNo] = (puchDrvCurrentRecord[uchSlotNo]&0x00FF)|(DrvCurrent<<8);
	}	
}


void GH3x2xNewAgcModuleReset(void)
{
    g_Gh3x2xNewAgcInitFlag = 0;
    GH3X2X_Memset((GU8*)g_pstGh3x2xNewAgcMainChnlInfoEntity,0,(GU16)GH3X2X_NEW_AGC_SLOT_NUM_LIMIT*sizeof(STGh3x2xNewAgcMainChnlInfo));
    GH3X2X_Memset( (GU8*)(&g_pstGh3x2xNewAgcMainChnlSat),0,sizeof(STGh3x2xNewAgcMainChnlSat)); // init  variable
    GH3X2X_Memset((GU8*)g_pstGh3x2xNewAgcMainChnlMeanInfoEntity,0,(GU16)GH3X2X_NEW_AGC_SLOT_NUM_LIMIT*sizeof(STGh3x2xNewAgcMainChnlMeanInfo)); 
    GH3X2X_Memset((GU8*)g_pstGh3x2xNewAgcMainChnlIdaelAdjInfoEntity,0,(GU16)GH3X2X_NEW_AGC_SLOT_NUM_LIMIT*sizeof(STGh3x2xNewAgcMainChnlIdaelAdjInfo));                           
		
	  #if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
    Gh3x2xNewAgcSubChnlModuleReset();   
		#endif
}

void GH3X2X_NewAgcRecoveryAgcRegToMainChnl(GU8 uchSlotIdx, STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo)
{
    GH3x2xSetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_UP_LIMIT, uchSlotIdx, pstAgcMainChnlInfo ->uchBakRegLedCurrentMax);
    GH3x2xSetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DOWN_LIMIT, uchSlotIdx, pstAgcMainChnlInfo ->uchBakRegLedCurrentMin);
    GH3x2xSetAgcReg(GH3X2X_AGC_REG_AGC_SRC, uchSlotIdx, pstAgcMainChnlInfo ->uchBakRegAgcSrc);
    GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] recover agc src:agc_src = %d, SlotNo = %d\r\n",pstAgcMainChnlInfo ->uchBakRegAgcSrc,uchSlotIdx);
}

GU8 GH3X2X_NewAgcFindSubChnlSlotInfo(GU8 uchSlot)
{
    for(GU8 uchSlotCnt = 0; uchSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT; uchSlotCnt ++)
    {
        STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt];
        if(pstAgcMainChnlInfo ->uchEnable)
        {
            if(uchSlot == (pstAgcMainChnlInfo ->uchSlotAdc >> 2))
            {
                return uchSlotCnt;
            }
        }
    }
    return 0;
}

GU16 GH3x2xCalTrigerUpLimitForBiggerGain(GU8 uchCurrentGain ,GU8 uchSlotNo)
{
    GU32 unTempRawdata;
    GU32 unTempTrigThd_H;

    GU8 uchSlotCnt = GH3X2X_NewAgcFindSubChnlSlotInfo(uchSlotNo);
    STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt];
    if(pstAgcMainChnlInfo->uchSpo2ChnlIndex)
    {
        unTempTrigThd_H = g_unNewAgcSpo2Trig_H_Thd;
    }else
    {
        unTempTrigThd_H = g_unNewAgcGeneTrig_H_Thd;
    }
        
    if(12 <= uchCurrentGain)
    {
        return 0xFFFF;
    }
    else
    {
        unTempRawdata = unTempTrigThd_H - 8388608;
        unTempRawdata = 8388608 + (GU32)(((float)unTempRawdata)/(float)1.3*(float)g_usGH3x2xTiaGainR[uchCurrentGain]/(float)g_usGH3x2xTiaGainR[uchCurrentGain + 1]);
        return unTempRawdata>>8;
    }
}


#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
void GH3x2xNewAgcSubChnlClearAnalysisCntAndSetGain(GU8 uchSlotNo, GU8 uchRxEn ,GU8 uchNextGain)
{
    for(GU8 uchRxCnt = 0; uchRxCnt < 4; uchRxCnt ++)
    {
        if(uchRxEn&(1 << uchRxCnt))
        {
            for(GU8 uchChnlCnt = 0; uchChnlCnt < GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT; uchChnlCnt ++)
            {
                STGh3x2xNewAgcSubChnlRxInfo *pstSTGh3x2xNewAgcSubChnlRxInfo = &(g_pstGh3x2xNewAgcSubChnlRxInfoEntity[uchChnlCnt]);
                if(pstSTGh3x2xNewAgcSubChnlRxInfo->uchEn)
                {
                    if(((uchSlotNo << 2) + uchRxCnt) == pstSTGh3x2xNewAgcSubChnlRxInfo ->uchSlotAdcNo)
                    {
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->usAnalysisCnt = 0;
                        if(0xFF != uchNextGain)
                        {
                            GH3x2xSetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + uchRxCnt, uchSlotNo, 0);
                            pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain = uchNextGain;
                            pstSTGh3x2xNewAgcSubChnlRxInfo ->usTrigeUpLimitForBiggerGain = GH3x2xCalTrigerUpLimitForBiggerGain(uchNextGain,pstSTGh3x2xNewAgcSubChnlRxInfo->uchSlotAdcNo >> 2);
                        }
                    }    
                }
            }
        }
    }
}
#endif
  
  
void GH3x2xNewAgcMeanInfoReset(GU8 uchSlotEnChange)
{
     for(GU8 uchSlotCnt = 0; uchSlotCnt < 8; uchSlotCnt++)
     {
        if( uchSlotEnChange & (1<<uchSlotCnt)) // SlotEn Change 
        {
            for(GU8 uchAgcSlotCnt = 0; uchAgcSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT; uchAgcSlotCnt ++) //// uchAgcSlotCntʹ�ܵ����slot����
            {
                GU8  uchSlotIdx = ((g_pstGh3x2xNewAgcMainChnlInfoEntity[uchAgcSlotCnt].uchSlotAdc >> 2) & 0x0f);
                GU8  uchEnable	= g_pstGh3x2xNewAgcMainChnlInfoEntity[uchAgcSlotCnt].uchEnable;
                if( uchEnable )  
                { 
                   if( uchSlotIdx == uchSlotCnt)  
                   {
                       g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchAgcSlotCnt].usAnalysisCnt = 0;
                       #if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
                       GH3x2xNewAgcSubChnlClearAnalysisCntAndSetGain(uchSlotIdx, 0x0F, 0xFF); //clear all rx analysis cnt in this slot
                       #endif 
                   }
                }
            }
        }
     }
}    

GU8 GH3x2xGetDrvCurrent(GU8 uchSlotIdx ,GU8 uchAgcEn)
{
    GU8 uchTempDrv;
    if(2 == uchAgcEn)
    {    
        uchTempDrv = GH3x2xGetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DRV1, uchSlotIdx);
    }
    else
    {
        uchTempDrv = GH3x2xGetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DRV0, uchSlotIdx);
    }
    return uchTempDrv;
}

void GH3x2xNewAgcChnlInfoInit(STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    if(0 == GH3X2X_NEW_AGC_SLOT_NUM_LIMIT)
    {
        return;
    }
    if(0 == g_Gh3x2xNewAgcInitFlag)
    {
        GU8 uchNewAgcSlotCnt = 0;
        for(GU8 uchSlotCnt = 0; uchSlotCnt < 8; uchSlotCnt ++)
        {
            if((g_stSoftAgcCfg.uchSlotxSoftAgcAdjEn & (1 << uchSlotCnt)))
            {
                if(uchNewAgcSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT)
                {
                    g_pstGh3x2xNewAgcMainChnlInfoEntity[uchNewAgcSlotCnt].uchEnable = 1;
                    g_pstGh3x2xNewAgcMainChnlInfoEntity[uchNewAgcSlotCnt].uchSlotAdc = (uchSlotCnt << 2); 
                                    
                    /******* slot function Distinguish   START******************************************************/          
                    if((g_stSoftAgcCfg.uchSpo2RedSlotCfgEn & (1 << uchSlotCnt))||(g_stSoftAgcCfg.uchSpo2IrSlotCfgEn & (1 << uchSlotCnt)))
                    {
                        g_pstGh3x2xNewAgcMainChnlInfoEntity[uchNewAgcSlotCnt].uchSpo2ChnlIndex =  1;
                        GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] uchNewAgcSpo2SlotCnt = %d\r\n",uchNewAgcSlotCnt);
                    }
                    /******* slot function Distinguish   END******************************************************/
                    GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] uchNewAgcSlotCnt = %d\r\n",uchNewAgcSlotCnt);
                    uchNewAgcSlotCnt ++;
                }
                else
                {
                    GH3X2X_DEBUG_LOG_PARAM("[NewAgc] error !!! main chnl info is no enough! \r\n");
                    while(1);
                }
            }
        }
        g_Gh3x2xNewAgcInitFlag = 1;
    }

    for(GU8 uchSlotCnt = 0; uchSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT; uchSlotCnt ++)
    {    
        STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt];
        GH3X2X_INFO_LOG_PARAM_AGC("AGC_InitValue %d: Enable = %d, AgcAdjEn = %d\r\n",uchSlotCnt, pstAgcMainChnlInfo->uchEnable,g_stSoftAgcCfg.uchSlotxSoftAgcAdjEn);
        if(pstAgcMainChnlInfo->uchEnable)
        {
            GU8 uchSlotIdx;
            uchSlotIdx = pstAgcMainChnlInfo ->uchSlotAdc >> 2;
            GH3X2X_INFO_LOG_PARAM_AGC("AGC_InitFlag%d: SlotCfgX=%d, Vaule = %d\r\n",uchSlotCnt, uchSlotIdx, pstAgcMainChnlInfo->uchHaveBakRegFlag);
            if(0 == pstAgcMainChnlInfo ->uchHaveBakRegFlag)
            {
                GU8 uchBgCancel;
                GU8 uchGainLimit;
                GU16 usSampleRate;
                GU16 usEcgPpgRxEn;

                //bak reg
                pstAgcMainChnlInfo ->uchBakRegAgcEn = GH3x2xGetAgcReg(GH3X2X_AGC_REG_AGC_EN, uchSlotIdx);
                pstAgcMainChnlInfo ->uchBakRegAgcSrc = GH3x2xGetAgcReg(GH3X2X_AGC_REG_AGC_SRC, uchSlotIdx);
                pstAgcMainChnlInfo ->uchSlotAdc = (uchSlotIdx << 2) + pstAgcMainChnlInfo ->uchBakRegAgcSrc;
                pstAgcMainChnlInfo ->uchBakRegLedCurrentMax = GH3x2xGetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_UP_LIMIT, uchSlotIdx);                
                pstAgcMainChnlInfo ->uchBakRegLedCurrentMin = GH3x2xGetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DOWN_LIMIT, uchSlotIdx);    
                pstAgcMainChnlInfo ->uchBakTiaGain = GH3x2xGetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + pstAgcMainChnlInfo ->uchBakRegAgcSrc, uchSlotIdx);
                pstAgcMainChnlInfo ->uchCurrentTiaGain = pstAgcMainChnlInfo ->uchBakTiaGain;
                pstAgcMainChnlInfo ->uchCurrentDrv =  GH3x2xGetDrvCurrent(uchSlotIdx, pstAgcMainChnlInfo->uchBakRegAgcEn); 
                //GH3X2X_StoreDrvCurrentAfterAgc(pstAgcMainChnlInfo ->uchCurrentDrv, uchSlotIdx,g_pstGh3x2xAgcInfo->uchDrvCurrentAfterSoftAgc);  							
                uchBgCancel = GH3x2xGetAgcReg(GH3X2X_AGC_REG_BG_CANCEL_RX0 + pstAgcMainChnlInfo ->uchBakRegAgcSrc, uchSlotIdx);
                GH3X2X_StoreRxTiaGainAfterAgc(pstAgcMainChnlInfo->uchBakTiaGain, uchSlotIdx, pstAgcMainChnlInfo->uchBakRegAgcSrc, STGh3x2xAgcInfo->uchTiaGainAfterSoftAgc);
                if(0 == uchBgCancel)
                {
                    uchGainLimit = g_stSoftAgcCfg.stSoftAgcGainLimit.uchGainLimitBg32uA;
                }
                else if(1 == uchBgCancel)
                {
                    uchGainLimit = g_stSoftAgcCfg.stSoftAgcGainLimit.uchGainLimitBg64uA;
                }
                else if(2 == uchBgCancel)
                {
                    uchGainLimit = g_stSoftAgcCfg.stSoftAgcGainLimit.uchGainLimitBg128uA;
                }
                else
                {
                    uchGainLimit = g_stSoftAgcCfg.stSoftAgcGainLimit.uchGainLimitBg256uA;
                }
                GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] uchGainLimit = %d, uchSlotIdx = %d\r\n",uchGainLimit, uchSlotIdx);
                pstAgcMainChnlInfo ->uchTiaGainUpLimit = uchGainLimit;
                
                usEcgPpgRxEn = GH3x2xGetAgcReg(GH3X2X_AGC_ECG_PPG_RX_EN, uchSlotIdx);
                GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] usEcgPpgRxEn = 0x%X\r\n",usEcgPpgRxEn);
                if(usEcgPpgRxEn&0x0001)  // ecg is enable
                {
                    usEcgPpgRxEn = ((usEcgPpgRxEn>>1) & (~0x0008));  //clear RX3 enable bit
                }
                else
                {
                    usEcgPpgRxEn = (usEcgPpgRxEn>>1);
                }
                usEcgPpgRxEn &= (~(1 << pstAgcMainChnlInfo ->uchBakRegAgcSrc));   //clear main channel RX enable bit
                
                GU16 usFastestSampleRate =  GH3X2X_ReadRegBitField(GH3X2X_FASTEST_SAMPLERATE_ADDR,0,7);
                usSampleRate = GH3X2X_SYS_SAMPLE_RATE/(usFastestSampleRate + 1)/(GH3x2xGetAgcReg(GH3X2X_AGC_REG_SR_MULIPLIPER, uchSlotIdx) + 1);
                g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchSlotCnt].usSampleRate = usSampleRate;  //init
                //sub channel slot
#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
                GH3x2xNewAgcSetNewSubChnl(uchSlotIdx,usEcgPpgRxEn,STGh3x2xAgcInfo);
#endif
                pstAgcMainChnlInfo ->uchHaveBakRegFlag = 1;                
                GH3x2xSetAgcReg(GH3X2X_AGC_REG_AGC_EN, uchSlotIdx, 0);   // disable hardware agc 
            }
            else
            {
              pstAgcMainChnlInfo ->uchCurrentTiaGain = GH3x2xGetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + pstAgcMainChnlInfo ->uchBakRegAgcSrc, uchSlotIdx);
              pstAgcMainChnlInfo ->uchCurrentDrv =  GH3x2xGetDrvCurrent(uchSlotIdx, pstAgcMainChnlInfo->uchBakRegAgcEn);
              GH3X2X_INFO_LOG_PARAM_AGC("Drv Gian inite read %d: IniteDrv = %d, IniteGain = %d\r\n",uchSlotIdx, pstAgcMainChnlInfo ->uchCurrentDrv, pstAgcMainChnlInfo ->uchCurrentTiaGain);
              ////get initial DRV and current in case them change before StartSampling()
//              GH3X2X_NewAgcRecoveryAgcRegToMainChnl(uchSlotIdx,pstAgcMainChnlInfo);
            }
        }
    }
}


GU32 GH32x2xMedSel(GU32 unData0, GU32 unData1, GU32 unData2)
{
    if((unData0 >= unData1)&&(unData0 <= unData2))
    {
        return unData0;
    }
    if((unData0 >= unData2)&&(unData0 <= unData1))
    {
        return unData0;
    }
    if((unData1 >= unData0)&&(unData1 <= unData2))
    {
        return unData1;
    }
    if((unData1 >= unData2)&&(unData1 <= unData0))
    {
        return unData1;
    }
    return unData2;
}

float GH3X2X_CalcfRelativeVpp(GU32 unRawdata, GU8 uchDrv, GU8 uchGain)
{
    return ((float)unRawdata/(float)uchDrv/(float)g_usGH3x2xTiaGainR[uchGain]);
}

void GH3X2X_StoreTiaGainAfterAgc(GU16 usGainRegValue, GU8 uchSlotNo, GU8* puchGainRecord)
{
    for(GU8 uchAdcCnt = 0; uchAdcCnt < 4; uchAdcCnt ++)
    {
        puchGainRecord[uchSlotNo*4 + uchAdcCnt] = (usGainRegValue>>(uchAdcCnt*4))&0x000F;
        GH3X2X_INFO_LOG_PARAM_AGC("StoreTiaGainAfterAgc slot no = %d, adc no = %d, usGainRegValue = %d\r\n", uchSlotNo,uchAdcCnt,usGainRegValue);
    }
}

void GH3X_AgcGetThreshold( GU8 uchSpo2ChnlIndex, GU32* punTemp_H_Thd, GU32* punTemp_L_Thd )
{  
   if(uchSpo2ChnlIndex) // is  spo2 slot
   {
      *punTemp_H_Thd = g_unNewAgcSpo2Trig_H_Thd; 
      *punTemp_L_Thd = g_unNewAgcSpo2Trig_L_Thd; 
   }else
   { 
      *punTemp_H_Thd = g_unNewAgcGeneTrig_H_Thd;
      *punTemp_L_Thd = g_unNewAgcGeneTrig_L_Thd;      
   }
}

#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
void Gh3x2xNewAgcSubChnlModuleReset(void)
{
    #if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
    GH3X2X_Memset((GU8*)g_pstGh3x2xNewAgcSubChnlRxInfoEntity,0,(GU16)GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT*sizeof(STGh3x2xNewAgcSubChnlRxInfo));
    #endif
}


GU8 GH3x2xNewAgcSetNewSubChnl(GU8 uchSlotNo, GU8 uchRxEn, STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    GU8 uchSetSuccess = 1;
    for(GU8 uchRxCnt = 0; uchRxCnt < 4; uchRxCnt ++)
    {
        if(uchRxEn&(1 << uchRxCnt))
        {
            uchSetSuccess = 0;
            for(GU8 uchChnlCnt = 0; uchChnlCnt < GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT; uchChnlCnt ++)
            {
                STGh3x2xNewAgcSubChnlRxInfo *pstSTGh3x2xNewAgcSubChnlRxInfo = &(g_pstGh3x2xNewAgcSubChnlRxInfoEntity[uchChnlCnt]);
                if(0 ==  pstSTGh3x2xNewAgcSubChnlRxInfo->uchEn)
                {
                    pstSTGh3x2xNewAgcSubChnlRxInfo->uchEn = 1;
                    pstSTGh3x2xNewAgcSubChnlRxInfo->uchSlotAdcNo = (uchSlotNo << 2) + uchRxCnt;
                    //read current gain
                    pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain = GH3x2xGetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + uchRxCnt, uchSlotNo);
                    GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] Sub chnl current Gain = %d, Slot = %d, Rx = %d\r\n",pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain,uchSlotNo,uchRxCnt);
                    pstSTGh3x2xNewAgcSubChnlRxInfo ->usTrigeUpLimitForBiggerGain = GH3x2xCalTrigerUpLimitForBiggerGain(pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain, uchSlotNo);
                    GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] usTrigeUpLimitForBiggerGain = %d, Slot = %d, Rx = %d\r\n",((GU32)pstSTGh3x2xNewAgcSubChnlRxInfo ->usTrigeUpLimitForBiggerGain)<<8,uchSlotNo,uchRxCnt);
                    GH3X2X_StoreRxTiaGainAfterAgc(pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain, uchSlotNo, uchRxCnt, STGh3x2xAgcInfo->uchTiaGainAfterSoftAgc);
                    uchSetSuccess = 1;
                    break;
                }
            }
            if(0 == uchSetSuccess)
            {
                GH3X2X_DEBUG_LOG_PARAM("[NewAgc] Set new sub chnl fail !!! Slot = %d, Rx = %d\r\n",uchSlotNo,uchRxCnt);
                while(1);
            }
        }
    }
    return uchSetSuccess;
}

#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
void GH3X2X_NewAgcSubChnlGainAdj(GU8* puchReadFifoBuffer, GU16 usFifoLen, STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    for(GU8 uchChnlCnt = 0; uchChnlCnt < GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT; uchChnlCnt ++)
    {    
        GU16 usAnalysisDiscardLen;
        GU16 usAnalysisEndLen;
        GU8  uchSlotCnt;
        GU8  uchSlotIdx;
        GU8  uchAdcNo;
        GU32 unTempTrigThd_L;
        GU32 unTempTrigThd_H;
        STGh3x2xNewAgcSubChnlRxInfo *pstSTGh3x2xNewAgcSubChnlRxInfo = &(g_pstGh3x2xNewAgcSubChnlRxInfoEntity[uchChnlCnt]);
        uchSlotIdx = pstSTGh3x2xNewAgcSubChnlRxInfo ->uchSlotAdcNo >> 2;
        uchAdcNo = pstSTGh3x2xNewAgcSubChnlRxInfo ->uchSlotAdcNo&0x03;
        uchSlotCnt = GH3X2X_NewAgcFindSubChnlSlotInfo(uchSlotIdx);
        GU16 usSampleRate = g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchSlotCnt].usSampleRate;
        GU8 uchTiaGainLimit = g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt].uchTiaGainUpLimit;
        usAnalysisDiscardLen = (usSampleRate<<1)/5;    //  2/5 Fs
        usAnalysisEndLen = usAnalysisDiscardLen + usSampleRate * GH3X2X_NEW_AGC_SUB_CHNNL_ANAYLSIS_TIME_SEC;
 
        GH3X_AgcGetThreshold(g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt].uchSpo2ChnlIndex,&unTempTrigThd_H, &unTempTrigThd_L);    
             
        if(pstSTGh3x2xNewAgcSubChnlRxInfo ->uchEn)
        {
           // prevent unRawdataSum(GU32) overflow
           if( (usAnalysisEndLen - usAnalysisDiscardLen) > 200 )  
           {
              usAnalysisDiscardLen = usAnalysisEndLen - 200;   // maximum number of accumulated rawdata is 200
           }
   
            for (GU16 usIndex = 0; usIndex < usFifoLen; usIndex += GH3X2X_FIFO_RAWDATA_SIZE) 
            {
                GU32 unFifoVal;
                GU8  uchSlotAdcNo;
                GU32 unDataRaw; 
                unFifoVal = GH3X2X_MAKEUP_DWORD(puchReadFifoBuffer[usIndex], puchReadFifoBuffer[usIndex + 1], 
                                                puchReadFifoBuffer[usIndex + 2], puchReadFifoBuffer[usIndex + 3]);
                uchSlotAdcNo = ((unFifoVal >> 27)&0x0000001F);
                unDataRaw = GH3X2X_RAWDATA_CLEAR_ALL_FLAG(unFifoVal);
                if(uchSlotAdcNo == pstSTGh3x2xNewAgcSubChnlRxInfo ->uchSlotAdcNo)
                {
                    if(usAnalysisDiscardLen == pstSTGh3x2xNewAgcSubChnlRxInfo->usAnalysisCnt) //init
                    {
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unRawdataSum = 0;
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastLastRawdata = unDataRaw;
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastRawdata = unDataRaw;
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCnt = 0;
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCntForBiggerGain = 0;
                    }
                    if(pstSTGh3x2xNewAgcSubChnlRxInfo->usAnalysisCnt < usAnalysisEndLen)  //analysising
                    {                
                        GU32 unTempRawdata = GH32x2xMedSel(pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastLastRawdata, pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastRawdata, unDataRaw);
                        if(usAnalysisDiscardLen == pstSTGh3x2xNewAgcSubChnlRxInfo->usAnalysisCnt)
                        {
                            GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] sub chnl unTempRawdata  = %d, Slot = %d, rx = %d\r\n",unTempRawdata, uchSlotIdx, uchAdcNo);
                        }
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unRawdataSum += unTempRawdata;
                        if(unTempRawdata > unTempTrigThd_H)
                        {
                            pstSTGh3x2xNewAgcSubChnlRxInfo->usMoreThanUpLimitPointCnt ++;
                        }
                        if((unTempRawdata >> 8) > pstSTGh3x2xNewAgcSubChnlRxInfo->usTrigeUpLimitForBiggerGain)
                        {
                            pstSTGh3x2xNewAgcSubChnlRxInfo->usMoreThanUpLimitPointCntForBiggerGain ++;
                        }
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastLastRawdata = pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastRawdata;
                        pstSTGh3x2xNewAgcSubChnlRxInfo ->unLastRawdata = unDataRaw;
                        pstSTGh3x2xNewAgcSubChnlRxInfo->usAnalysisCnt ++;                    
                    }
                    else //anlysis finished
                    {
                        GU32 unAvaRawdata;
                        GU16 unRealAnaylsisNum = usAnalysisEndLen - usAnalysisDiscardLen;
                        GU8  uchNextGain = 0xFF;
                        GU8  uchCurrentGain = pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain;
                        unAvaRawdata = pstSTGh3x2xNewAgcSubChnlRxInfo ->unRawdataSum/unRealAnaylsisNum;
                        GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] sub chnl rawdata ava  = %d,DownLimit = %d, Slot = %d, rx = %d\r\n",unAvaRawdata, unTempTrigThd_L,uchSlotIdx, uchAdcNo);
                        GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] usMoreThanUpLimitPointCnt  = %d,UpLimit = %d, Slot = %d, rx = %d\r\n",pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCnt, unTempTrigThd_H,uchSlotIdx, uchAdcNo);
                        GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] usMoreThanUpLimitPointCntForBiggerGain = %d,UpLimit = %d, Slot = %d, rx = %d\r\n",pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCntForBiggerGain, ((GU32)pstSTGh3x2xNewAgcSubChnlRxInfo->usTrigeUpLimitForBiggerGain)<<8, uchSlotIdx, uchAdcNo);
                        if(pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCnt > (unRealAnaylsisNum >> 3))  // 1/8 points is bigger than trige up limit
                        {
                            if(uchCurrentGain > 0)
                            {
                                uchNextGain = uchCurrentGain - 1;
                            }
                        }
                        if((pstSTGh3x2xNewAgcSubChnlRxInfo ->usMoreThanUpLimitPointCntForBiggerGain < (unRealAnaylsisNum >> 3))&&(unAvaRawdata < unTempTrigThd_L))
                        {
                            if(uchCurrentGain < uchTiaGainLimit)
                            {
                                uchNextGain = uchCurrentGain + 1;
                            }
                        }
                        if(0xFF != uchNextGain)
                        {                            
                            GH3X2X_INFO_LOG_PARAM_AGC("[NewAgc] sub chnl gain change: NewGain = %d, OldGain = %d, Slot = %d, rx = %d\r\n",uchNextGain,uchCurrentGain, uchSlotIdx, uchAdcNo);
                            GH3x2xSetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + uchAdcNo, uchSlotIdx, uchNextGain);
                            pstSTGh3x2xNewAgcSubChnlRxInfo ->uchCurrentGain = uchNextGain;
                            pstSTGh3x2xNewAgcSubChnlRxInfo ->usTrigeUpLimitForBiggerGain = GH3x2xCalTrigerUpLimitForBiggerGain(uchNextGain, uchSlotIdx);                                    
                            GH3X2X_StoreRxTiaGainAfterAgc(uchNextGain, uchSlotIdx, uchAdcNo, STGh3x2xAgcInfo->uchTiaGainAfterSoftAgc);
                        }
                        pstSTGh3x2xNewAgcSubChnlRxInfo->usAnalysisCnt = 0;  
                        break;// 完成一次均值计算后面的数据丢弃
                    }
                }
            }
        }
    }
}

#endif
#endif


/*********************************************self-test function start*****************************************/
#define GH3X2X_SPO2_SELF_TEST 0

#if GH3X2X_SPO2_SELF_TEST
const GU32 g_uchSpo2AgcSelftestSlotAdc = 0x01;  //slot0, adc1
const GU32 g_usSpo2AgcFifoLen = 25*4;
const GU32 g_unSpo2AgcRawdata[] = 
{
    10111744,9818116,10074798,9832931,10038819,9812989,10067119,9791632,10220862,9843610,9888564,9974512,10041146,10117796,9858828,10260863,9900963,10159857,10259720,10054459,10247477,9957985,10078474,9916146,9821879
};

GU8 g_uchSpo2AgcFifoBuffer[4*100];

void GH3X2X_Spo2AgcFifoDataGen(void)
{
    for (GU16 usIndex = 0; usIndex < g_usSpo2AgcFifoLen; usIndex += GH3X2X_FIFO_RAWDATA_SIZE) 
    {
        if(usIndex >= sizeof(g_uchSpo2AgcFifoBuffer))
        {
            break;
        }
        g_uchSpo2AgcFifoBuffer[usIndex + 0] = (g_uchSpo2AgcSelftestSlotAdc<<3);
        g_uchSpo2AgcFifoBuffer[usIndex + 1] = (0x000000FF&(g_unSpo2AgcRawdata[usIndex/4] >> 16));
        g_uchSpo2AgcFifoBuffer[usIndex + 2] = (0x000000FF&(g_unSpo2AgcRawdata[usIndex/4] >> 8));
        g_uchSpo2AgcFifoBuffer[usIndex + 3] = (0x000000FF&(g_unSpo2AgcRawdata[usIndex/4] >> 0));
    }
}
#endif
/*********************************************self-test function end*****************************************/


/****************************************************************
* Description: calculate mean value of main channel  
* Input:  rawdata  
* Return: none
******************************************************************/
static void GH3X2X_NewAgcMainChnlMeanValueCal(GU8* puchReadFifoBuffer, GU16 usFifoLen, GU32 *punRawdataMean, GU8 *pAdjFlag)
{
    for(GU8 uchSlotCnt = 0; uchSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT; uchSlotCnt ++)
    {
        STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt];
        GU8 uchMainChnlSlotAdcNo = pstAgcMainChnlInfo ->uchSlotAdc;
        
        if(pstAgcMainChnlInfo ->uchEnable)   // is  enabled  slot
        {        
            GU32 unTemp_Trig_H_Thd;    
            GU32 unTemp_Trig_L_Thd;    
            GU32 unTemp_Ideal_value;
            GU16 usAnalysisEndLen; 
            GU16 usAnalysisDiscardLenAdjust = 0;
            GU16 usAnalysisDiscardLenTrige;
            GU32 TrigeMean  = 0; 

            if(pstAgcMainChnlInfo->uchSpo2ChnlIndex) // is  spo2 slot
            {
                unTemp_Ideal_value= g_unNewAgcSpo2Ideal_value;
            }else
            {                        
                unTemp_Ideal_value= g_unNewAgcGeneIdeal_value;
            }
            GH3X_AgcGetThreshold( pstAgcMainChnlInfo->uchSpo2ChnlIndex, &unTemp_Trig_H_Thd, &unTemp_Trig_L_Thd);  
                      
            STGh3x2xNewAgcMainChnlMeanInfo *pstNewAgcMainChnlMeanInfo = &g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchSlotCnt];
            STGh3x2xNewAgcMainChnlIdaelAdjInfo *pstNewAgcMainChnlIdaelAdjInfo = &g_pstGh3x2xNewAgcMainChnlIdaelAdjInfoEntity[uchSlotCnt];
            GU16 usSampleRate =  pstNewAgcMainChnlMeanInfo->usSampleRate;
            usAnalysisEndLen = (usSampleRate<<2)/5;            // 4/5 Fs  
            usAnalysisDiscardLenTrige =  (usSampleRate<<1)/5;  // drop the first 2/5 data    
            //have agc motion at last fifo read period or first init                        
            if(pstNewAgcMainChnlMeanInfo->uchHaveAgcAtLastReadPrd)   
            {
                usAnalysisDiscardLenAdjust = (usSampleRate<<1)/5;  // drop the first 2/5 data
                //// After AGC, the previous data is drop to calcluate precise mean
                if(!pstNewAgcMainChnlMeanInfo->uchDropFlag)
                {  // The frame count is only cleared for the first time to prevent data num cannot to be SampleRate
                    pstNewAgcMainChnlMeanInfo->usAnalysisCnt = 0;  
                    pstNewAgcMainChnlMeanInfo->uchDropFlag = 1;
                }         
            }
            //The data fluctuates when initializing the early stage  
            if(!pstNewAgcMainChnlIdaelAdjInfo->uchIniteFirst)  
            {
                usAnalysisDiscardLenAdjust =  (usSampleRate<<1)/5;  // drop the first 2/5 data
            }

           // prevent unSumAdjust(GU32) overflow
           if( (usAnalysisEndLen - usAnalysisDiscardLenAdjust) > 200 )
           {
              usAnalysisDiscardLenAdjust = usAnalysisEndLen - 200; // maximum number of accumulated rawdata is 200
           }
           
           if( (usAnalysisEndLen - usAnalysisDiscardLenTrige) > 200 )
           {
              usAnalysisDiscardLenTrige = usAnalysisEndLen - 200;
           }

            for (GU16 usIndex = 0; usIndex < usFifoLen; usIndex += GH3X2X_FIFO_RAWDATA_SIZE) 
            {
                GU32 unFifoVal;
                GU8  uchSlotAdcNo;
                GU32 unDataRaw; 
                unFifoVal = GH3X2X_MAKEUP_DWORD(puchReadFifoBuffer[usIndex], puchReadFifoBuffer[usIndex + 1], 
                                                puchReadFifoBuffer[usIndex + 2], puchReadFifoBuffer[usIndex + 3]);
                uchSlotAdcNo = ((unFifoVal >> 27)&0x0000001F);
                unDataRaw = GH3X2X_RAWDATA_CLEAR_ALL_FLAG(unFifoVal); 
 
                if(uchSlotAdcNo == uchMainChnlSlotAdcNo)
                {
                    // begin Saturation judge   
                    {  
                        GU16 usSat_Check_Num_Max =   (usSampleRate<<2) + usSampleRate;   //// 5 seconds window
                        g_pstGh3x2xNewAgcMainChnlSat.Sat_CheckCnt[uchSlotCnt]++;
                        if( unDataRaw > unTemp_Trig_H_Thd)
                        {
                            g_pstGh3x2xNewAgcMainChnlSat.Sat_cnt[uchSlotCnt]++;
                        }
                        //// as soon as posible set the Saturation flag 
                        if( g_pstGh3x2xNewAgcMainChnlSat.Sat_cnt[uchSlotCnt] >= (usSat_Check_Num_Max>>4) ) // about 8% of all checked data
                        {
                            g_pstGh3x2xNewAgcMainChnlSat.Sat_flag |= (1<<uchSlotCnt);  
                            g_pstGh3x2xNewAgcMainChnlSat.Sat_cnt[uchSlotCnt] = 0; 
                        }     

                        if(g_pstGh3x2xNewAgcMainChnlSat.Sat_CheckCnt[uchSlotCnt] >= usSat_Check_Num_Max ) 
                        {                                      
                            g_pstGh3x2xNewAgcMainChnlSat.Sat_cnt[uchSlotCnt] = 0;
                            g_pstGh3x2xNewAgcMainChnlSat.Sat_CheckCnt[uchSlotCnt] = 0;  
                        }                                 
                    } 
                    // end Saturation judge   
                    
                    //// begin mean calculate    
                   GH3X2X_INFO_LOG_PARAM_AGC("slotCnt=%d,MainChnlSlotAdc=%d,SlotAdc=%d,Raw=%d\r\n",uchSlotCnt,uchMainChnlSlotAdcNo,uchSlotAdcNo,unDataRaw);
                    
                   if(usAnalysisDiscardLenAdjust == pstNewAgcMainChnlMeanInfo->usAnalysisCnt) //init
                    {
                        pstNewAgcMainChnlMeanInfo->unSumAdjust =  0;
                        pstNewAgcMainChnlMeanInfo->unMaxRawdata =  0;      
                        pstNewAgcMainChnlMeanInfo->unMinRawdata =  16777216;
                    }
                                        
                    if(usAnalysisDiscardLenTrige == pstNewAgcMainChnlMeanInfo->usAnalysisCnt) //init
                    {
                        pstNewAgcMainChnlMeanInfo->unSumTrige  =  0;
                    }    
                                        
                    if(pstNewAgcMainChnlMeanInfo->usAnalysisCnt  < usAnalysisEndLen)   //analysising
                    {
                    pstNewAgcMainChnlMeanInfo->unSumAdjust +=  unDataRaw;
                    pstNewAgcMainChnlMeanInfo->unSumTrige +=  unDataRaw;
                    pstNewAgcMainChnlMeanInfo->usAnalysisCnt++;   
                    pstNewAgcMainChnlMeanInfo->unMaxRawdata = ((unDataRaw > pstNewAgcMainChnlMeanInfo->unMaxRawdata)? unDataRaw: pstNewAgcMainChnlMeanInfo->unMaxRawdata ); // Max of  rawdata
                    pstNewAgcMainChnlMeanInfo->unMinRawdata = ((unDataRaw < pstNewAgcMainChnlMeanInfo->unMinRawdata)? unDataRaw: pstNewAgcMainChnlMeanInfo->unMinRawdata ); // Min of  rawdata
                    }
                    else //anlysis finished   calculate mean and judge whether AGC
                    {    
                        // mean value to calculate gain and current
                        if(usAnalysisEndLen > usAnalysisDiscardLenAdjust)
                        {
                            punRawdataMean[uchSlotCnt] = pstNewAgcMainChnlMeanInfo->unSumAdjust/(usAnalysisEndLen - usAnalysisDiscardLenAdjust);    
                            TrigeMean = pstNewAgcMainChnlMeanInfo->unSumTrige / (usAnalysisEndLen - usAnalysisDiscardLenTrige);
                        }else //  The normal case will not be executed, in case the division by 0 in abnormal case
                        {
                            punRawdataMean[uchSlotCnt] =   unDataRaw; 
                            TrigeMean =   unDataRaw; 
                        }
                        pstNewAgcMainChnlMeanInfo ->unRawdataMean = punRawdataMean[uchSlotCnt];
                        pstNewAgcMainChnlMeanInfo->usAnalysisCnt=0;
                        // The mean calculation key variable is cleared    (Prevent mean calculation from being too small:Accumulation lost some data, but numble of raw is all)    
                        usAnalysisDiscardLenAdjust = 0; 
                        pstNewAgcMainChnlMeanInfo->uchHaveAgcAtLastReadPrd = 0; //  clear  after caculate mean!!!       
                        //// end mean calculate

                        ///////////////////// whether AGC  and adjust the mean value to calculate gain and current
                        if( (TrigeMean < unTemp_Trig_L_Thd) || (TrigeMean > unTemp_Trig_H_Thd) )
                        {
                            *pAdjFlag |= (1<<uchSlotCnt);  //need adjust gain and current 
                        }
                        
                        if( g_pstGh3x2xNewAgcMainChnlSat.Sat_flag & (1<<uchSlotCnt)) //if the channel is saturation
                        {
                            GU32 unTempMean = 0;         //Pay attention to initialization of local variables
                            if( pstNewAgcMainChnlMeanInfo->unMaxRawdata > unTemp_Trig_H_Thd)
                            {
                                /////////// Pay attention to data truncation, use 64-bi
                                unTempMean =   (GS64)unTemp_Ideal_value*(GS64)(pstNewAgcMainChnlMeanInfo->unMaxRawdata)/15099494;  // to adjust the max data to (0.8*2^23 + 2^23)
                                *pAdjFlag |= (1<<uchSlotCnt);  //need adjust gain and current 
                            }
                            
                            if( punRawdataMean[uchSlotCnt] > unTemp_Ideal_value)
                            {
                                // adjust the mean value to calculate gain and current (reduce the DC amount)
                                punRawdataMean[uchSlotCnt] = ( (punRawdataMean[uchSlotCnt]>unTempMean)? punRawdataMean[uchSlotCnt]:unTempMean );
                                *pAdjFlag |= (1<<uchSlotCnt);  //need adjust gain and current 
                            } 
                            else if( pstNewAgcMainChnlMeanInfo->unMaxRawdata > unTemp_Trig_H_Thd)
                            {
                                punRawdataMean[uchSlotCnt] = unTempMean; //  Increasing the mean value to reduce the DC amount
                            } 
                        }
                        ////////////////////////////////////////////////////
                        //////////////begin ideal value adjust 
                        if( (!((*pAdjFlag) & (1<<uchSlotCnt))) && (!( g_pstGh3x2xNewAgcMainChnlSat.Sat_flag & (1<<uchSlotCnt))))//if the channel not adjust and satuartion
                        {
                            //// The deviation of the mean and the ideal target value is above 1/16  and   Data fluctuation is less than a certain value
                            if(((pstNewAgcMainChnlMeanInfo->unRawdataMean*g_uchAgcConvergence_level < (unTemp_Ideal_value*(g_uchAgcConvergence_level-1)) ) \
                                || (pstNewAgcMainChnlMeanInfo->unRawdataMean*g_uchAgcConvergence_level > (unTemp_Ideal_value*(g_uchAgcConvergence_level+1)))) \
                                &&( 10*pstNewAgcMainChnlMeanInfo->unMaxRawdata< 12*pstNewAgcMainChnlMeanInfo->unMinRawdata) ) 
                            {
                                if( !pstNewAgcMainChnlIdaelAdjInfo->uchIniteFirst ) //// Inite First time 
                                {
                                    pstNewAgcMainChnlIdaelAdjInfo->uchIniteFirst = 1;
                                    *pAdjFlag |= (1<<uchSlotCnt);  //need adjust gain and current 
                                }
                                else
                                {
                                    pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt++;
                                    if(pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt >= 19)
                                    {
                                        *pAdjFlag |= (1<<uchSlotCnt);  //need adjust gain and current 
                                        pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt  = 0; 
                                    }    
                                }             
                            }
                            else
                            {
                                 pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt  = 0;
                            }                    
                        }
                        else
                        {
                             pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt  = 0;
                        }
                        pstNewAgcMainChnlIdaelAdjInfo->uchIniteFirst = 1;  //// Must be set to 1 after the first second!!!    
                        ////end ideal value adjust 
                        GH3X2X_INFO_LOG_PARAM_AGC("Mean cal: Slot_num = %d, Rawmean = %d, Adjmean = %d \r\n", uchSlotCnt,pstNewAgcMainChnlMeanInfo->unRawdataMean, punRawdataMean[uchSlotCnt]);
                        GH3X2X_INFO_LOG_PARAM_AGC("Sat flag: SatCnt = %d, SatCheck = %d, Satflag = %d\r\n", g_pstGh3x2xNewAgcMainChnlSat.Sat_cnt[uchSlotCnt], g_pstGh3x2xNewAgcMainChnlSat.Sat_CheckCnt[uchSlotCnt], g_pstGh3x2xNewAgcMainChnlSat.Sat_flag);
                        GH3X2X_INFO_LOG_PARAM_AGC("Ideal adjust: SeconsCnt = %d\r\n", pstNewAgcMainChnlIdaelAdjInfo->uchSeconsCnt);
                        GH3X2X_INFO_LOG_PARAM_AGC("Adj flag: Slot_num = %d, Trigmean = %d, L_Thd = %d, H_Thd = %d, Adjflag = %d\r\n", uchSlotCnt,TrigeMean, unTemp_Trig_L_Thd, unTemp_Trig_H_Thd, *pAdjFlag);                                     
                        g_pstGh3x2xNewAgcMainChnlSat.Sat_flag &= (~(1<<uchSlotCnt)) ; ////clear the Sat flag  
                        break; //完成一次均值计算后面的数据丢弃
                    }
                }  
             }
        }
        
    }
}

/**
 * @fn     GU8 GH3X2X_NewAgcCalDrvCurrentAndGain(GU32 CurRawdata, GU32 IdealRawdata, STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo, GU8* puchIdealDrvCurrent, GU8* puchIdealGain)
 *
 * @brief  mark abnormal chnl
 *
 * @attention   
 *
 * @param[in]   CurRawdata, IdealRawdata, uchMainChnlInfoIndex
 * @param[out]  puchIdealDrvCurrent, puchIdealGain
 *
 * @return  0:DrvCurrent or gain will NOT change  1: DrvCurrent or gain will  change
 */
GU8 GH3X2X_NewAgcCalDrvCurrentAndGain(GU32 CurRawdata, GU8 uchMainChnlInfoIndex,  GU8* puchIdealDrvCurrent, GU8* puchIdealGain)
{
    STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchMainChnlInfoIndex];
    STGh3x2xNewAgcMainChnlMeanInfo *pstMainChnlExtralInfo = &g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchMainChnlInfoIndex];

    GU8 uchDefaultGain = pstAgcMainChnlInfo->uchBakTiaGain;
    GU8 uchGainUpLimit = pstAgcMainChnlInfo->uchTiaGainUpLimit;
    GU8 uchLedCurrentMin = pstAgcMainChnlInfo->uchBakRegLedCurrentMin;
    GU8 uchLedCurrentMax = pstAgcMainChnlInfo->uchBakRegLedCurrentMax;
    GU8 uchCurrentDrv = pstAgcMainChnlInfo->uchCurrentDrv;
    GU8 uchCurrentTiaGain = pstAgcMainChnlInfo->uchCurrentTiaGain;
 
    GU8 uchNewGain = uchDefaultGain;
    GU32 unIdealRawdata_no;
    GU32 unTempDrvCurrent;
    GU8 uchChangeFlag = 0;  //bit0: drv current flag  bit1: gain flag
    GU8 uchCalFinish = 0;
    GU32 unTrigUpLimit;
    GU8  unCalNum = 12;
  
    if(0 != pstAgcMainChnlInfo->uchSpo2ChnlIndex) // is  spo2 slot
    {
        unTrigUpLimit = g_unNewAgcSpo2Trig_H_Thd;
        unIdealRawdata_no= g_unNewAgcSpo2Ideal_value - 8388608;
    }else
    {
        unTrigUpLimit = g_unNewAgcGeneTrig_H_Thd;
        unIdealRawdata_no = g_unNewAgcGeneIdeal_value - 8388608;
    }
    
    GH3X2X_INFO_LOG_PARAM_AGC("Drv cal: UpLimit = %d, Mean = %d, LastMean = %d\r\n",unTrigUpLimit, pstMainChnlExtralInfo->unRawdataMean, pstMainChnlExtralInfo->unLastRawdataMean);
    // reduce rawdata as soon as posible
    if((unTrigUpLimit < pstMainChnlExtralInfo->unRawdataMean)&&(unTrigUpLimit < pstMainChnlExtralInfo->unLastRawdataMean)&&(uchCurrentTiaGain > 0)) 
    {             
       uchNewGain = 0;  
       unTempDrvCurrent =  uchLedCurrentMin;
       uchCalFinish = 1;
    }
    pstMainChnlExtralInfo ->unLastRawdataMean = pstMainChnlExtralInfo ->unRawdataMean;
         
    if(0 == uchCalFinish)
    {
        //remove offset
        if(CurRawdata > (8388608 + 2048))  //// Can't open DC Cance !!!
        {
            CurRawdata -= 8388608;
        }
        else
        {
            //The value may be less than 2^23, preventing division by 0
            CurRawdata = 2048;  // Take an appropriately large number to prevent calculated DRV current overflow 32bit
        }
        //Rawdata stretch  (The principle of magnification: the mean divided by the ideal target value is the data reduction factor )
        if(CurRawdata < 8000000)
        {
        }
        else if(CurRawdata < 8100000)
        {
            CurRawdata = 8000000 + (CurRawdata - 8000000)*6;
        }
        else if(CurRawdata < 8200000)
        {
            CurRawdata = 8000000 + ((CurRawdata - 8000000)<<3);
        }
        else
        {
            // The more saturated the data, the bigger the stretch, the key is that the data is really saturated
            CurRawdata = 8000000 + (CurRawdata - 8000000)*14;   //  Blood oxygen drops 1 times, heart rate drops 2.5 times  
        }

        //find new drv current and gain
        do
        {   
            unTempDrvCurrent = (GS64)(((GU32)uchCurrentDrv*(GU32)g_usGH3x2xTiaGainR[uchCurrentTiaGain]))* \
            (GS64)unIdealRawdata_no/g_usGH3x2xTiaGainR[uchNewGain]/CurRawdata;

           GH3X2X_INFO_LOG_PARAM_AGC("Drv cal %d: CurrentDrv=%d, CurrentGain=%d, IdealRawdata_no=%d\r\n",uchMainChnlInfoIndex, uchCurrentDrv, uchCurrentTiaGain, unIdealRawdata_no);
           GH3X2X_INFO_LOG_PARAM_AGC("Drv cal %d: TempDrvCurrent=%d, NewGain=%d, Meanraw=%d, CurRawdata=%d\r\n",uchMainChnlInfoIndex,\
                                   unTempDrvCurrent, uchNewGain, pstMainChnlExtralInfo->unRawdataMean, CurRawdata);
          
            if(unTempDrvCurrent > uchLedCurrentMax)  //drv is too big, need increase gain
            {
                if(uchNewGain < uchGainUpLimit)
                {
                    uchNewGain ++ ;
                }
                else
                {
                    unTempDrvCurrent = uchLedCurrentMax;
                    break;
                }
            }
            else if(unTempDrvCurrent < uchLedCurrentMin)  //drv is too small, need dncrease gain
            {
                if(uchNewGain > 0)
                {
                    uchNewGain -- ;
                }
                else
                {
                    unTempDrvCurrent = uchLedCurrentMin;
                    break;
                }
            }
            else   //got right drv
            {
                break;
            }
            unCalNum --;
        }
        while (unCalNum);
    }

    puchIdealDrvCurrent[0] = (GU8)unTempDrvCurrent;
    puchIdealGain[0] = (GU8)uchNewGain;

    if(puchIdealDrvCurrent[0] != uchCurrentDrv)
    {
        uchChangeFlag |= 0x01;
//        GH3X2X_INFO_LOG_PARAM_AGC("Drv change %d: CurrentDrv=%d,  IdealDrv=%d\r\n",uchMainChnlInfoIndex, uchCurrentDrv, puchIdealDrvCurrent[0]);
    }
    if(puchIdealGain[0] != uchCurrentTiaGain)
    {
        uchChangeFlag |= 0x02;
//        GH3X2X_INFO_LOG_PARAM_AGC("Gain change %d: CurrentGain=%d,  IdealGain=%d\r\n",uchMainChnlInfoIndex, uchCurrentTiaGain, puchIdealGain[0]);
    }
        
    return uchChangeFlag;
}

GU8 NewAgcMainChelFlag = 0;

void GH3X2X_NewAgcMainChnlPro(GU8* puchReadFifoBuffer, GU16 usFifoLen, STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    GU32 unNewAgcRawdataMeanChnl[8] = {0};
    GU8 uchAdjFlag = 0;     // Local variables used as judgments must be initialized to 0
    GU8 uchNewDrvCurrent;
    GU8 uchNewGain;    
    GU8 uchChangeFlag = 0;  //bit0: drv current flag  bit1: gain flag
    
    if(0 == GH3X2X_NEW_AGC_SLOT_NUM_LIMIT)
    {
        return;
    }

#if GH3X2X_SPO2_SELF_TEST

    GH3X2X_Spo2AgcFifoDataGen();
    puchReadFifoBuffer = g_uchSpo2AgcFifoBuffer;
    usFifoLen = g_usSpo2AgcFifoLen;
#endif
           
    GH3X2X_NewAgcMainChnlMeanValueCal( puchReadFifoBuffer,usFifoLen,unNewAgcRawdataMeanChnl,&uchAdjFlag); // caculate mean and whther adjust gain  current 
		
    for(GU8 uchSlotCnt = 0; uchSlotCnt < GH3X2X_NEW_AGC_SLOT_NUM_LIMIT; uchSlotCnt ++)
    {
        if( uchAdjFlag & (1<<uchSlotCnt)) // need to AGC
        {
            STGh3x2xNewAgcMainChnlInfo *pstAgcMainChnlInfo = &g_pstGh3x2xNewAgcMainChnlInfoEntity[uchSlotCnt];
            uchChangeFlag = GH3X2X_NewAgcCalDrvCurrentAndGain(unNewAgcRawdataMeanChnl[uchSlotCnt], uchSlotCnt, &uchNewDrvCurrent,&uchNewGain);
            GU8  uchSlotIdx = ((pstAgcMainChnlInfo ->uchSlotAdc >> 2) & 0x0f);
            if(uchChangeFlag&0x02)  //write tia gain to chip
            {    
                GH3x2xSetAgcReg(GH3X2X_AGC_REG_TIA_GAIN_RX0 + pstAgcMainChnlInfo ->uchBakRegAgcSrc, uchSlotIdx, uchNewGain);    
                pstAgcMainChnlInfo->uchCurrentTiaGain = uchNewGain;
                GH3X2X_StoreRxTiaGainAfterAgc(uchNewGain, uchSlotIdx, pstAgcMainChnlInfo ->uchBakRegAgcSrc, STGh3x2xAgcInfo->uchTiaGainAfterSoftAgc);
                GH3X2X_INFO_LOG_PARAM_AGC("AGC Change gain Slotcfg=%d, AgcSrc=%d, NewGain=%d\r\n", uchSlotIdx,pstAgcMainChnlInfo->uchBakRegAgcSrc, uchNewGain); 
            }
            if(uchChangeFlag&0x01)  //write drv current to chip
            {
                //change drv0/1
                pstAgcMainChnlInfo ->uchCurrentDrv = uchNewDrvCurrent;
                
                if((pstAgcMainChnlInfo ->uchBakRegAgcEn)&0x01)
                {
                    GH3x2xSetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DRV0, uchSlotIdx, uchNewDrvCurrent);    
                    GH3X2X_StoreDrvCurrentAfterAgc(pstAgcMainChnlInfo ->uchCurrentDrv, uchSlotIdx,0,STGh3x2xAgcInfo->uchDrvCurrentAfterSoftAgc); 
                }
                if((pstAgcMainChnlInfo ->uchBakRegAgcEn)&0x02)
                {
                    GH3x2xSetAgcReg(GH3X2X_AGC_REG_LED_CURRENT_DRV1, uchSlotIdx, uchNewDrvCurrent);    
                    GH3X2X_StoreDrvCurrentAfterAgc(pstAgcMainChnlInfo ->uchCurrentDrv, uchSlotIdx,1,STGh3x2xAgcInfo->uchDrvCurrentAfterSoftAgc);  
                }
                #if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
                GH3x2xNewAgcSubChnlClearAnalysisCntAndSetGain(uchSlotIdx, 0x0F, 0xFF); //clear all rx analysis cnt in this slot
                #endif
                GH3X2X_INFO_LOG_PARAM_AGC("AGC Change Current Slotcfg=%d, AgcEn=%d, NewCurrent=%d\r\n", uchSlotIdx,pstAgcMainChnlInfo->uchBakRegAgcEn, uchNewDrvCurrent); 
            }
            if(uchChangeFlag)
            {
                g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchSlotCnt].uchHaveAgcAtLastReadPrd = 1; // current or  gain change at last time
                g_pstGh3x2xNewAgcMainChnlMeanInfoEntity[uchSlotCnt].uchDropFlag = 0;
                NewAgcMainChelFlag |= (1<<uchSlotCnt); // ADjFlag write to CSV file
            }                          
        }                   
    }     
}

#if  __SUPPORT_SOFT_AGC_CONFIG__
/**
 * @fn     static void GH3X2X_LedAgcInit(STGh3x2xAgcInfo *STGh3x2xAgcInfo)
 * 
 * @brief  init led agc
 *
 * @attention   just use to show function support
 *
 * @param[in]   None
 * @param[out]  None
 *
 * @return  None
 */
void GH3X2X_LedAgcInit(STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    GH3x2xNewAgcChnlInfoInit(STGh3x2xAgcInfo);
}
/**
 * @fn     void GH3X2X_LedAgcPramWrite(GU16 usRegVal, GU16 usRegPosi)
 *
 * @brief  write AGC parameter
 *
 * @attention   None
 *
 * @param[in]   usRegVal         reg value
 * @param[in]   uchRegPosi         reg posi
 * @param[out]  None
 *
 * @return  None
 */
void GH3X2X_LedAgcPramWrite(GU16 usRegVal, GU16 usRegPosi)
{

    GU16 usByteOffset;
    GU8 uchByteVal;
    GU8 uchByteCnt;
    for(uchByteCnt = 0; uchByteCnt < 2; uchByteCnt ++)
    {
        uchByteVal = ((usRegVal >> (8*uchByteCnt))&0x00FF);
        usByteOffset = (usRegPosi * 2) + uchByteCnt;
        if(usByteOffset < sizeof(g_stSoftAgcCfg))
        {
            GH3X2X_Memcpy(((GU8 *)(&g_stSoftAgcCfg)) + usByteOffset, (GU8*)(&uchByteVal), 1);
        }      
    }
}

/**
 * @fn  void GH3X2X_LedAgcReset(void)
 *
 * @brief   reset agc parameter
 *
 * @attention    None
 *
 * @param[in]    None
 * @param[out]   None
 *
 * @return  None
 */
void GH3X2X_LedAgcReset(void)
{
    GH3X2X_Memset(&g_stSoftAgcCfg, 0, sizeof(STSoftAgcCfg));
    GH3x2xNewAgcModuleReset();
}

/**
 * @fn     void GH3X2X_LedAgcProcess(GU8* puchReadFifoBuffer, GU16 usFifoLen)
 * 
 * @brief  led agc process function
 *
 * @attention   None
 *
 * @param[in]   puchReadFifoBuffer         pointer to read fifo data buffer
 * @param[in]   usFifoLen           read fifo data buffer length
 * @param[out]  None
 *
 * @return  None
 */

void GH3X2X_LedAgcProcess(GU8* puchReadFifoBuffer, GU16 usFifoLen, STGh3x2xAgcInfo *STGh3x2xAgcInfo)
{
    GH3X2X_INFO_LOG_PARAM_AGC("GH3X2X_LedAgcProcess call \n");
    if(usFifoLen==0)
    {
        return;
    }
    
    GH3X2X_NewAgcMainChnlPro(puchReadFifoBuffer, usFifoLen,STGh3x2xAgcInfo);
    
#if GH3X2X_NEW_AGC_SUB_CHNL_NUM_LIMIT > 0
    GH3X2X_NewAgcSubChnlGainAdj(puchReadFifoBuffer, usFifoLen,STGh3x2xAgcInfo);
#endif
		
}

#define GH3X2X_SOFT_AGC_VERSION "SoftAgc_V4.2.0.0"

char* GH3X2X_GetSoftAgcVersion(void)
{
	return GH3X2X_SOFT_AGC_VERSION;
}
#endif

#endif
/********END OF FILE********* Copyright (c) 2003 - 2022, Goodix Co., Ltd. ********/
