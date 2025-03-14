#include "gh3x2x_demo_algo_call.h"
#include "gh3x2x_demo_algorithm_calc.h"
#include "gh3x2x_demo_algo_config.h"
#include "goodix_algo.h"
#include "goodix_spo2.h"

#if (__USE_GOODIX_SPO2_ALGORITHM__)

goodix_spo2_ret goodix_spo2_init_func(GU32 fs)
{
    goodix_spo2_ret stAlgoRet = GX_ALGO_SPO2_SUCCESS;
    GCHAR uchSpo2Version[150] = {0};

    GH3X2X_Spo2AlgoChnlMapDefultSet();
    goodix_spo2_version((uint8_t *)uchSpo2Version);
    GH3X2X_DEBUG_ALGO_LOG("spo2 algorithm version : %s\r\n", uchSpo2Version);
    GH3X2X_DEBUG_ALGO_LOG("spo2 algorithm legal chnl num : %d\r\n", g_stSpo2AlgoChnlMap.uchNum);

    if (g_stSpo2AlgoChnlMap.uchNum > Gh3x2xGetSpo2AlgoSupportChnl())
    {
        GH3X2X_DEBUG_ALGO_LOG("ERROR: spo2 algorithm legal chnl num is too big, check cfg !!! \r\n");
        return GX_ALGO_SPO2_RWONG_INPUT;
    }

    goodix_spo2_config stSpo2Cfg = {
            g_stSpo2AlgoChnlMap.uchNum, // 有效通道数
            fs,                         // 采样率
            g_nSpo2ParamssArr[0], g_nSpo2ParamssArr[1], g_nSpo2ParamssArr[2], g_nSpo2ParamssArr[3],
            g_nSpo2ParamssArr[4], g_nSpo2ParamssArr[5], g_nSpo2ParamssArr[6], g_nSpo2ParamssArr[7],
            g_nSpo2ParamssArr[8], g_nSpo2ParamssArr[9], g_nSpo2ParamssArr[10], g_nSpo2ParamssArr[11],
            g_nSpo2ParamssArr[12], g_nSpo2ParamssArr[13], g_nSpo2ParamssArr[14], g_nSpo2ParamssArr[15],
            g_nSpo2ParamssArr[16], g_nSpo2ParamssArr[17], g_nSpo2ParamssArr[18], g_nSpo2ParamssArr[19],
            g_nSpo2ParamssArr[20], g_nSpo2ParamssArr[21], g_nSpo2ParamssArr[22], g_nSpo2ParamssArr[23],
            g_nSpo2ParamssArr[24], g_nSpo2ParamssArr[25]
    };

    GH3X2X_INFO_ALGO_LOG("[%s]: params1 = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", __FUNCTION__,
                                    stSpo2Cfg.valid_chl_num,
                                    stSpo2Cfg.raw_fs,
                                    stSpo2Cfg.cali_coef_a4,
                                    stSpo2Cfg.cali_coef_a3,
                                    stSpo2Cfg.cali_coef_a2,
                                    stSpo2Cfg.cali_coef_a1,
                                    stSpo2Cfg.cali_coef_a0,
                                    stSpo2Cfg.hb_en_flg,
                                    stSpo2Cfg.wear_mode,
                                    stSpo2Cfg.acc_thr_max,
                                    stSpo2Cfg.acc_thr_min,
                                    stSpo2Cfg.acc_thr_scale,
                                    stSpo2Cfg.acc_thr_num,
                                    stSpo2Cfg.acc_thr_angle,
                                    stSpo2Cfg.ctr_en_flg,
                                    stSpo2Cfg.ctr_red_thr);

    GH3X2X_INFO_ALGO_LOG("[%s]: params2 = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", __FUNCTION__,
                                    stSpo2Cfg.ppg_jitter_thr,
                                    stSpo2Cfg.ppg_noise_thr,
                                    stSpo2Cfg.ppg_coeff_thr,
                                    stSpo2Cfg.quality_module_key,
                                    stSpo2Cfg.low_spo2_thr,
                                    stSpo2Cfg.fast_out_time,
                                    stSpo2Cfg.slow_out_time,
                                    stSpo2Cfg.min_stable_time_high,
                                    stSpo2Cfg.min_stable_time_low,
                                    stSpo2Cfg.max_spo2_variation_high,
                                    stSpo2Cfg.max_spo2_variation_low,
                                    stSpo2Cfg.ouput_module_key);

    const void *p_CfgInstance = (void *)&stSpo2Cfg;
    char chVer[SPO2_INTERFACE_VERSION_LEN_MAX] = {0};

    goodix_spo2_config_get_version(chVer, SPO2_INTERFACE_VERSION_LEN_MAX);
    //const char *p_InterfaceVer = chVer;

    stAlgoRet = goodix_spo2_init(p_CfgInstance, sizeof(stSpo2Cfg), chVer);

    return stAlgoRet;
}

goodix_spo2_ret goodix_spo2_deinit_func(void)
{
    return goodix_spo2_deinit();
}

GS8 GH3x2xSpo2AlgoInit(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    GS8 chRet = GH3X2X_RET_GENERIC_ERROR;

    GH3X2X_DEBUG_ALGO_LOG("%s\r\n", __FUNCTION__);

    goodix_spo2_ret ret = goodix_spo2_init_func(pstFrameInfo->pstFunctionInfo->usSampleRate);
    if (GX_ALGO_SPO2_SUCCESS == ret)
    {
        chRet = GH3X2X_RET_OK;
    }
    else
    {
        GH3X2X_DEBUG_ALGO_LOG("spo2 init error! error code:0x%x\r\n", ret);
    }

    return chRet;
}

GS8 GH3x2xSpo2AlgoDeinit(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    GS8 chRet = GH3X2X_RET_GENERIC_ERROR;

    GH3X2X_DEBUG_ALGO_LOG("%s\r\n", __FUNCTION__);

    goodix_spo2_ret ret = goodix_spo2_deinit_func();
    if (GX_ALGO_SPO2_SUCCESS == ret)
    {
        chRet = GH3X2X_RET_OK;
    }
    else
    {
        GH3X2X_DEBUG_ALGO_LOG("spo2 deinit error! error code:0x%x\r\n", ret);
    }

    return chRet;
}

GS8 GH3x2xSpo2AlgoExe(const STGh3x2xFrameInfo *const pstFrameInfo)
{
    if (0 == pstFrameInfo)
    {
        return GH3X2X_RET_GENERIC_ERROR;
    }

    GU8 uchChannelNum = 0;
	GS32 ppg_rawdata[3 * CHIP_PPG_CHL_NUM] = {0};
	GS8 cur_adj_flg[3 * CHIP_PPG_CHL_NUM] = {0};
	GS8 gain_adj_flg[3 * CHIP_PPG_CHL_NUM] = {0};
	GS8 enable_flg[3 * CHIP_PPG_CHL_NUM] = {0};
	GS32 ch_agc_drv0[3 * CHIP_PPG_CHL_NUM] = {0};
	GS32 ch_agc_drv1[3 * CHIP_PPG_CHL_NUM] = {0};
	GS8 ch_agc_gain[3 * CHIP_PPG_CHL_NUM] = {0};

    goodix_spo2_input_rawdata stRawdata = {0};
    goodix_spo2_result stResult = {0};
    GS8 emAlgoRet = GH3X2X_RET_OK;

    stRawdata.frameid = GH3X2X_GET_BYTE0_FROM_DWORD(*(pstFrameInfo->punFrameCnt));
    stRawdata.bit_num = 24;
    stRawdata.ppg_rawdata = ppg_rawdata;
    stRawdata.cur_adj_flg = cur_adj_flg;
    stRawdata.gain_adj_flg = gain_adj_flg;
    stRawdata.enable_flg = enable_flg;
    stRawdata.ch_agc_drv0 = ch_agc_drv0;
    stRawdata.ch_agc_drv1 = ch_agc_drv1;
    stRawdata.ch_agc_gain = ch_agc_gain;
    stRawdata.acc_x = pstFrameInfo->pusFrameGsensordata[0]; // 46
    stRawdata.acc_y = pstFrameInfo->pusFrameGsensordata[1]; // -183
    stRawdata.acc_z = pstFrameInfo->pusFrameGsensordata[2]; // 482
    stRawdata.wear_on_flag = 1;

    /* call algorithm, update result */
    for (GU8 uchChMapCnt = 0; uchChMapCnt < CHIP_PPG_CHL_NUM * 3; ++uchChMapCnt)
    {
        if (g_stSpo2AlgoChnlMap.uchAlgoChnlMap[uchChMapCnt + 4 * (uchChMapCnt / 4)] < 0xff)
        {
            GU8 uchRawdataIndex = g_stSpo2AlgoChnlMap.uchAlgoChnlMap[uchChMapCnt + 4 * (uchChMapCnt / 4)];

            stRawdata.ch_agc_drv0[uchChMapCnt]  = ((pstFrameInfo->punFrameAgcInfo[uchRawdataIndex] >> 8) & 0xFF);
            stRawdata.ch_agc_drv1[uchChMapCnt]  = ((pstFrameInfo->punFrameAgcInfo[uchRawdataIndex] >> 16) & 0xFF);
            stRawdata.ch_agc_gain[uchChMapCnt]  = (pstFrameInfo->punFrameAgcInfo[uchRawdataIndex] & 0xF);

            if (((pstFrameInfo->puchFrameLastGain[uchRawdataIndex] != stRawdata.ch_agc_gain[uchChMapCnt]) && \
                (pstFrameInfo->puchFrameLastGain[uchRawdataIndex] != GH3X2X_GAIN_VALUE_INVALID)) || \
                ((pstFrameInfo->punFrameFlag[0]) & 0x01))
            {
                stRawdata.gain_adj_flg[uchChMapCnt] = 1;
                pstFrameInfo->puchFrameLastGain[uchRawdataIndex] = stRawdata.ch_agc_gain[uchChMapCnt];
            }
            stRawdata.cur_adj_flg[uchChMapCnt] = (((pstFrameInfo->punFrameFlag[0]) >> uchRawdataIndex) & 0x01);
            stRawdata.enable_flg[uchChMapCnt]  = 1;
            stRawdata.ppg_rawdata[uchChMapCnt] = pstFrameInfo->punFrameRawdata[uchRawdataIndex];
            uchChannelNum++;

            GH3X2X_DEBUG_ALGO_LOG("[%s] ch%d ppg_rawdata = %d,gain_adj_flg = %d,cur_adj_flg = %d\r\n", __FUNCTION__, uchChMapCnt, \
                (int)stRawdata.ppg_rawdata[uchChMapCnt], (int)stRawdata.gain_adj_flg[uchChMapCnt], \
                (int)stRawdata.cur_adj_flg[uchChMapCnt]);
        }
    }
    stRawdata.ch_num = CHIP_PPG_CHL_NUM;

    goodix_spo2_ret ret = goodix_spo2_calc(&stRawdata, &stResult);
    // goodix_spo2_essential_info_print(&stRawdata);

    if (GX_ALGO_SPO2_SUCCESS == ret || GX_ALGO_SPO2_FRAME_UNCOMPLETE == ret || GX_ALGO_SPO2_WIN_UNCOMPLETE == ret)
    {
        if (1 == stResult.final_calc_flg)
        {
            pstFrameInfo->pstAlgoResult->uchUpdateFlag = (GU8)stResult.final_calc_flg;
            pstFrameInfo->pstAlgoResult->snResult[0] = (GS32)(GH3x2x_Round((float)stResult.final_spo2 / 10000));
            pstFrameInfo->pstAlgoResult->snResult[1] = (GS32)stResult.final_r_val;
            pstFrameInfo->pstAlgoResult->snResult[2] = (GS32)stResult.final_confi_coeff;
            pstFrameInfo->pstAlgoResult->snResult[3] = (GS32)stResult.final_valid_level;
            pstFrameInfo->pstAlgoResult->snResult[4] = (GS32)stResult.final_hb_mean;
            pstFrameInfo->pstAlgoResult->snResult[5] = (GS32)stResult.final_invalidFlg;
            pstFrameInfo->pstAlgoResult->usResultBit = 0x7F;
            pstFrameInfo->pstAlgoResult->uchResultNum = GH3x2x_BitCount(pstFrameInfo->pstAlgoResult->usResultBit);
            GH3X2X_Spo2AlgorithmResultReport(pstFrameInfo->pstAlgoResult, pstFrameInfo->punFrameCnt[0]);
            GH3X2X_DEBUG_ALGO_LOG("[%s] final_spo2:%d, r = %d, final_valid_level:%d\r\n", __FUNCTION__, pstFrameInfo->pstAlgoResult->snResult[0], \
                pstFrameInfo->pstAlgoResult->snResult[1], pstFrameInfo->pstAlgoResult->snResult[3]);
        }
    }
    else
    {
        emAlgoRet = GH3X2X_RET_RESOURCE_ERROR;
        GH3X2X_DEBUG_ALGO_LOG("GH3x2xSpo2AlgoExe error! error code = 0x%x\r\n", ret);
        GH3X2X_DEBUG_ALGO_LOG("please feedback to GOODIX!\r\n");
    }

    return (GS8)emAlgoRet;
}

#endif
