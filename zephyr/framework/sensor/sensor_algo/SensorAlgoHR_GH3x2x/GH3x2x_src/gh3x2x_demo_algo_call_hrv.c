#include <stdio.h>
#include "gh3x2x_demo_algo_call.h"
#include "gh3x2x_demo_algorithm_calc.h"
#include "goodix_hba.h"
#include "goodix_hrv.h"

#if (__USE_GOODIX_HRV_ALGORITHM__)

GU32 g_unHrvLastHrResult = 0;

//hrv
goodix_hrv_ret goodix_hrv_init_func(GU32 fs)
{
    goodix_hrv_ret stAlgoRet = GX_HRV_ALGO_OK;
    goodix_hrv_config stHrvConfig = {
                                        g_nHrvParamsArr[0],
                                        fs,
                                        {g_nHrvParamsArr[1] * 10000, g_nHrvParamsArr[2] * 10000, g_nHrvParamsArr[3] * 10000, g_nHrvParamsArr[4] * 10000}
                                    };

    GCHAR uchHrvVersion[100] = {0};
    GH3X2X_Memcpy(uchHrvVersion, goodix_hrv_version(), strlen((GCHAR *)goodix_hrv_version()) + 1);
    GH3X2X_DEBUG_ALGO_LOG("hrv algorithm version : %s\r\n", uchHrvVersion);

    GH3X2X_INFO_ALGO_LOG("[%s]:params = %d,%d,%d,%d,%d,%d,\r\n", __FUNCTION__,
                                    stHrvConfig.need_ipl,
                                    stHrvConfig.fs,
                                    stHrvConfig.acc_thr[0],
                                    stHrvConfig.acc_thr[1],
                                    stHrvConfig.acc_thr[2],
                                    stHrvConfig.acc_thr[3]);

    const void *p_CfgInstance = (void *)&stHrvConfig;
    char chVer[HRV_INTERFACE_VERSION_LEN_MAX] = {0};

    goodix_hrv_config_get_version(chVer, HRV_INTERFACE_VERSION_LEN_MAX);
    const char *p_InterfaceVer = chVer;

    stAlgoRet = goodix_hrv_init(p_CfgInstance, sizeof(stHrvConfig), p_InterfaceVer);

    return stAlgoRet;
}

uint32_t goodix_hrv_deinit_func(void)
{
    uint32_t nRet = GX_HRV_ALGO_OK;

    nRet = goodix_hrv_deinit();
    if (GX_HRV_ALGO_OK != nRet)
    {
        GH3X2X_DEBUG_ALGO_LOG("hrv init errorcode : 0x%x\r\n", nRet);
    }

    return nRet;
}

GS8 GH3x2xHrvAlgoInit(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    GH3X2X_DEBUG_ALGO_LOG("%s\r\n", __FUNCTION__);

    GS8 chRet = GH3X2X_RET_GENERIC_ERROR;

    uint32_t ret = goodix_hrv_init_func(pstFrameInfo->pstFunctionInfo->usSampleRate);
    if (GX_ALGO_HBA_SUCCESS == ret)
    {
        pstFrameInfo->pstAlgoResult->usResultBit = 0x7F;
        chRet = GH3X2X_RET_OK;
    }
    else
    {
        GH3X2X_DEBUG_ALGO_LOG("hrv init error! error code = 0x%x\r\n", ret);
    }
    g_pstGh3x2xAlgoFrameInfo[GH3X2X_FUNC_OFFSET_HR]->pstAlgoRecordResult->uchUpdateFlag = 0;
    g_unHrvLastHrResult = 0;

    return chRet;
}

GS8 GH3x2xHrvAlgoDeinit(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    GH3X2X_DEBUG_ALGO_LOG("%s\r\n", __FUNCTION__);

    GS8 chRet = GH3X2X_RET_GENERIC_ERROR;

    if (GX_ALGO_HBA_SUCCESS == goodix_hrv_deinit_func())
    {
        chRet = GH3X2X_RET_OK;
    }
    else
    {
        GH3X2X_DEBUG_ALGO_LOG("hrv deinit error!\r\n");
    }

    return chRet;
}

GS8 GH3x2xHrvAlgoExe(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    if (0 == pstFrameInfo)
    {
        return GH3X2X_RET_GENERIC_ERROR;
    }

    GS8 chAlgoRet = GH3X2X_RET_OK;
    GU8 uchGainValue = 0;
    GS32 ppg_rawdata[PPG_CHANNEL_NUM] = {0};
    GS32 cur_adj_flg[PPG_CHANNEL_NUM] = {0};
    GS32 gain_adj_flg[PPG_CHANNEL_NUM] = {0};
    goodix_hrv_result stResult = {0};
    goodix_hrv_input_rawdata stRawdataInput = {0};

    stRawdataInput.ppg_rawdata = ppg_rawdata;
    stRawdataInput.cur_adj_flg = cur_adj_flg;
    stRawdataInput.gain_adj_flg = gain_adj_flg;

    stRawdataInput.bit_num = 24;
    stRawdataInput.acc_x = pstFrameInfo->pusFrameGsensordata[0];
    stRawdataInput.acc_y = pstFrameInfo->pusFrameGsensordata[1];
    stRawdataInput.acc_z = pstFrameInfo->pusFrameGsensordata[2];
    stRawdataInput.frame_id = GH3X2X_GET_BYTE0_FROM_DWORD(*(pstFrameInfo->punFrameCnt));
    if (g_pstGh3x2xAlgoFrameInfo[GH3X2X_FUNC_OFFSET_HR]->pstAlgoRecordResult->uchUpdateFlag)
    {
        g_unHrvLastHrResult = g_pstGh3x2xAlgoFrameInfo[GH3X2X_FUNC_OFFSET_HR]->pstAlgoRecordResult->snResult[0];
    }
    stRawdataInput.hr = g_unHrvLastHrResult;

    for (GS32 nRawdatacnt = 0; nRawdatacnt < PPG_CHANNEL_NUM; nRawdatacnt++)
    {
        stRawdataInput.ppg_rawdata[nRawdatacnt] = pstFrameInfo->punFrameRawdata[nRawdatacnt];
        uchGainValue = GH3X2X_GET_LOW_4BITS(pstFrameInfo->punFrameAgcInfo[nRawdatacnt]);
        if ((pstFrameInfo->puchFrameLastGain[nRawdatacnt] != uchGainValue) && \
            (pstFrameInfo->puchFrameLastGain[nRawdatacnt] != GH3X2X_GAIN_VALUE_INVALID))
        {
            stRawdataInput.gain_adj_flg[nRawdatacnt] = 1;
            pstFrameInfo->puchFrameLastGain[nRawdatacnt] = uchGainValue;
        }
        stRawdataInput.cur_adj_flg[nRawdatacnt] = ((pstFrameInfo->punFrameFlag[0]) & 0x01);

        GH3X2X_DEBUG_ALGO_LOG("[%s]ppg_rawdata = %d, gain_adj_flg = %d, cur_adj_flg = %d\r\n", __FUNCTION__, \
            (int)stRawdataInput.ppg_rawdata[nRawdatacnt], (int)stRawdataInput.gain_adj_flg[nRawdatacnt], \
            (int)stRawdataInput.cur_adj_flg[nRawdatacnt]);
    }

    goodix_hrv_ret ret = goodix_hrv_calc(&stRawdataInput, &stResult);
    if (GX_HRV_ALGO_OK == ret || GX_HRV_ALGO_UPDATE == ret)
    {
        if (GX_HRV_ALGO_UPDATE == ret)
        {
            pstFrameInfo->pstAlgoResult->uchUpdateFlag = 1;
            pstFrameInfo->pstAlgoResult->snResult[0] = (GS32)stResult.rri[0];
            pstFrameInfo->pstAlgoResult->snResult[1] = (GS32)stResult.rri[1];
            pstFrameInfo->pstAlgoResult->snResult[2] = (GS32)stResult.rri[2];
            pstFrameInfo->pstAlgoResult->snResult[3] = (GS32)stResult.rri[3];
            pstFrameInfo->pstAlgoResult->snResult[4] = (GS32)stResult.rri_confidence;
            pstFrameInfo->pstAlgoResult->snResult[5] = (GS32)stResult.rri_valid_num;
            pstFrameInfo->pstAlgoResult->usResultBit = 0x7F;
            pstFrameInfo->pstAlgoResult->uchResultNum = GH3x2x_BitCount(pstFrameInfo->pstAlgoResult->usResultBit);
            GH3X2X_HrvAlgorithmResultReport(pstFrameInfo->pstAlgoResult, pstFrameInfo->punFrameCnt[0]);
        }
    }
    else
    {
        chAlgoRet = GH3X2X_RET_RESOURCE_ERROR;
        GH3X2X_DEBUG_ALGO_LOG("GH3x2xHrAlgoExe error! error code = 0x%x\r\n", ret);
        GH3X2X_DEBUG_ALGO_LOG("please feedback to GOODIX!\r\n");
    }

    return chAlgoRet;
}

#endif

