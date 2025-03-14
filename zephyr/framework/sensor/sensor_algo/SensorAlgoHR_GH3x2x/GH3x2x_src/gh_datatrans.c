#include "gh_drv.h"
#include "gh_drv_common.h"
#include "gh_datatrans.h"
//#include "goodix_mem.h"

static GU8 g_uchPayloadHandleInitFlag = 0;
static GU8 g_uchProtocolCmd = 0;
static GU16 g_usPayloadBufferLength = 0;
static GU16 g_usPayloadBufferOffset = 0;
static GU8 *g_puchPayloadBuffer = GH3X2X_PTR_NULL;
static pfnSendProtocolDataFunc g_pGh3x2xProtocolSendFunc;
static pfnFrameDataOutputFunc g_pGh3x2xProtocolFrameDataOutputFunc;

void* GH3X2X_UserMalloc(GU32 mem_len)
{
    void* addr = 0;//goodix_mem_malloc(mem_len);
    return addr;
}

void GH3X2X_UserFree(void* mem_addr)
{
    //goodix_mem_free(mem_addr);
}

EMPayloadHandleRet GH3X2X_ProtocolSectionInit(GU8 uchProtocolCmd,
                                              GU8 *puchPayloadDataBuffer,
                                              GU16 usPayloadDataBufferLength,
                                              void (*pfnSendProtocolDataFunc)(GU8 uchCmd, GU8* puchBuffer, GU16 usLen),
                                              void (*pfnFrameDataOutputFunc)(GU8* puchFunctionID,
                                                                             GU8* puchFrameID,
                                                                             STProtocolPackInfo** pstReceivePackInfo,
                                                                             GU8* puchPackNum))
{
    if ((pfnSendProtocolDataFunc != GH3X2X_PTR_NULL) ||
        (pfnFrameDataOutputFunc != GH3X2X_PTR_NULL) ||
        (usPayloadDataBufferLength != 0) ||
        (g_puchPayloadBuffer != GH3X2X_PTR_NULL))
    {
        g_uchProtocolCmd = uchProtocolCmd;
        g_pGh3x2xProtocolSendFunc = pfnSendProtocolDataFunc;
        g_puchPayloadBuffer = puchPayloadDataBuffer;
        g_usPayloadBufferLength = usPayloadDataBufferLength;
        g_usPayloadBufferOffset = usPayloadDataBufferLength;
        g_pGh3x2xProtocolFrameDataOutputFunc = pfnFrameDataOutputFunc;
        g_uchPayloadHandleInitFlag = 1;
        return GH_PAYLOAD_HANDLE_CONTINUE;
    }
    else
    {
        g_uchPayloadHandleInitFlag = 0;
        return GH_PAYLOAD_HANDLE_NOINIT;
    }
}

EMPayloadHandleRet GH3X2X_ProtocolPackCheck(STProtocolPackInfo *pstPackInfo)
{
    if (pstPackInfo != GH3X2X_PTR_NULL)
    {
        if ((pstPackInfo->uchByteType != 0) &&
            (pstPackInfo->uchDataNum != 0) &&
            (pstPackInfo->pSrc != GH3X2X_PTR_NULL))
        {
            return GH_PAYLOAD_HANDLE_CONTINUE;
        }
        else
        {
            if (pstPackInfo->pSrc == GH3X2X_PTR_NULL)
            {
                return GH_PAYLOAD_HANDLE_ADDRNULL;
            }
            else
            {
                return GH_PAYLOAD_HANDLE_DATAERR;
            }
        }
    }
    else
    {
        return GH_PAYLOAD_HANDLE_ADDRNULL;
    }
}

EMPayloadHandleRet GH3X2X_ProtocolPackHandle(STProtocolPackInfo *pstPackInfo,
                                             GU8 *puchPayloadDest,
                                             GU16 *pusPayloadMaxLen,
                                             GU8 uchSeparateFlag,
                                             GU32 *punSeparateDataOffset)
{
    GU16 usPayloadMaxLen = *pusPayloadMaxLen;
    GU32 unSeparateDataOffset = *punSeparateDataOffset;
    GU8 uchHeaderLength = 0;
    //pack id
    puchPayloadDest[GH_PAYLOAD_PACKHEADER_OFFSET_PACKID] = pstPackInfo->uchPackID;
    uchHeaderLength++;
    //pack flag
    puchPayloadDest[GH_PAYLOAD_PACKHEADER_OFFSET_FLAG] = pstPackInfo->uchFlag;
    uchHeaderLength++;
    //byte type
    if (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfo->uchFlag, GH_PACK_FLAG_BYTETYPE))
    {
        puchPayloadDest[GH_PAYLOAD_PACKHEADER_OFFSET_BYTETYPE] = pstPackInfo->uchByteType;
        uchHeaderLength++;
    }
    //data num
    if (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfo->uchFlag, GH_PACK_FLAG_DATANUM))
    {
        puchPayloadDest[GH_PAYLOAD_PACKHEADER_OFFSET_DATANUM] = pstPackInfo->uchDataNum;
        uchHeaderLength++;
    }
    if (uchHeaderLength >= usPayloadMaxLen)
    {
        SET_VAL_VIA_PTR(pusPayloadMaxLen, usPayloadMaxLen);
        SET_VAL_VIA_PTR(punSeparateDataOffset, unSeparateDataOffset);
        return GH_PAYLOAD_HANDLE_PAYLOADFULL;
    }
    usPayloadMaxLen -= uchHeaderLength;
    GU16 usDataLength = pstPackInfo->uchByteType * pstPackInfo->uchDataNum;
    if ((usPayloadMaxLen >= usDataLength) && (unSeparateDataOffset == 0))
    {
        GH3X2X_Memcpy((puchPayloadDest + uchHeaderLength),
                pstPackInfo->pSrc,
                usDataLength);
        usPayloadMaxLen -= usDataLength;
        SET_VAL_VIA_PTR(pusPayloadMaxLen, usPayloadMaxLen);
        SET_VAL_VIA_PTR(punSeparateDataOffset, unSeparateDataOffset);
        return GH_PAYLOAD_HANDLE_CONTINUE;
    }
    else
    {
        if (uchSeparateFlag)
        {
            GU16 usCpyLength = 0;
            usCpyLength = (usDataLength - unSeparateDataOffset) > usPayloadMaxLen ? usPayloadMaxLen : (usDataLength - unSeparateDataOffset);
            GH3X2X_Memcpy((puchPayloadDest + uchHeaderLength),
                    &(((GU8*)pstPackInfo->pSrc)[unSeparateDataOffset]),
                    usCpyLength);
            unSeparateDataOffset += usCpyLength;
            usPayloadMaxLen -= usCpyLength;
            if (unSeparateDataOffset == usDataLength)
            {
                unSeparateDataOffset = 0;
                SET_VAL_VIA_PTR(pusPayloadMaxLen, usPayloadMaxLen);
                SET_VAL_VIA_PTR(punSeparateDataOffset, unSeparateDataOffset);
                return GH_PAYLOAD_HANDLE_CONTINUE;
            }
        }
        SET_VAL_VIA_PTR(pusPayloadMaxLen, usPayloadMaxLen);
        SET_VAL_VIA_PTR(punSeparateDataOffset, unSeparateDataOffset);
        return GH_PAYLOAD_HANDLE_PAYLOADFULL;
    }
}

EMPayloadHandleRet GH3X2X_ProtocolPayloadHandle(EMProtocolSendMode emSendMode,
                                                EMProtocolSendHandle emSendHandle,
                                                GU8 uchFunctionID,
                                                GU8 uchFrameID,
                                                STProtocolPackInfo *pstPackInfo,
                                                GU8 uchPackNum)
{
    if (g_uchPayloadHandleInitFlag != 1)
    {
        return GH_PAYLOAD_HANDLE_NOINIT;
    }

    static GU8 uchSeparateID = 0;

    GU8 uchSeparateFlag = 0;
    GU8 uchPackCnt = 0;
    GU8 uchPackCntRecord = 0;
    GU8 uchSeparateStatus = 1;
    GU8 uchFrameHeaderLength = 0;
    GU32 unPackSrcByteRecord = 0;
    GU32 unAllPackByteNum = 0;
    EMPayloadHandleRet emRet = GH_PAYLOAD_HANDLE_CONTINUE;
    EMPayloadHandleProcess emHandleFlag = GH_PAYLOAD_HANDLE_FULL_HEADER;

    //check all packs
    for (GU8 PackCnt = 0; PackCnt < uchPackNum;  PackCnt++)
    {
        emRet = GH3X2X_ProtocolPackCheck(&pstPackInfo[PackCnt]);
        if (emRet)
        {
            GH3X2X_DEBUG_LOG_PARAM("[%s]:pack %d is err, ret = %d\r\n", __FUNCTION__, uchPackCnt, (int)emRet);
            return emRet;
        }
        STProtocolPackInfo* pstPackInfoTemp = &pstPackInfo[PackCnt];
        GU8 uchPackHeaderLength = 2 + \
            (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfoTemp->uchFlag, GH_PACK_FLAG_BYTETYPE)) + \
            (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfoTemp->uchFlag, GH_PACK_FLAG_DATANUM));
        unAllPackByteNum += (pstPackInfoTemp->uchByteType * pstPackInfoTemp->uchDataNum + uchPackHeaderLength);
    }

    //pack
    while(emHandleFlag != GH_PAYLOAD_HANDLE_OVER)
    {
        switch (emHandleFlag)
        {
        case GH_PAYLOAD_HANDLE_FULL_HEADER://full header
            //flag
            g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG] = 0;
            g_usPayloadBufferOffset--;
            //frame id
            if (emSendMode != GH_PROTOCOL_SENDMODE_FAST)
            {
                g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG] |= ((0x1) << GH_PAYLOAD_FLAG_SEND_MODE);
                g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FUNCID] = uchFunctionID;
                g_usPayloadBufferOffset--;
            }
            //check size
            uchFrameHeaderLength = 2;
            //separate flag
            uchSeparateFlag = (unAllPackByteNum + uchFrameHeaderLength > g_usPayloadBufferOffset);
            //separate payload id
            if (uchSeparateFlag)
            {
                g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG] |= (0x1 << GH_PAYLOAD_FLAG_SEPARATE_ENABLE);
                g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG] |= (uchSeparateStatus << GH_PAYLOAD_FLAG_SEPARATE_STATUS);
                uchSeparateStatus = 0;
                g_puchPayloadBuffer[g_usPayloadBufferLength - g_usPayloadBufferOffset] = uchSeparateID;
                uchSeparateID++;
                g_usPayloadBufferOffset--;
            }
            emHandleFlag = GH_PAYLOAD_HANDLE_FULL_DATA;
            break;
        case GH_PAYLOAD_HANDLE_FULL_DATA://full data
            //frame id
            if (emSendMode != GH_PROTOCOL_SENDMODE_FAST)
            {
                g_puchPayloadBuffer[g_usPayloadBufferLength - g_usPayloadBufferOffset] = uchFrameID;
                g_usPayloadBufferOffset--;
            }
            g_puchPayloadBuffer[g_usPayloadBufferLength - g_usPayloadBufferOffset] = uchPackNum;
            g_usPayloadBufferOffset--;
            //pack pack
            for (uchPackCnt = uchPackCntRecord; uchPackCnt < uchPackNum;  uchPackCnt++)
            {
                emRet = GH3X2X_ProtocolPackHandle(&pstPackInfo[uchPackCnt],
                                                  &g_puchPayloadBuffer[g_usPayloadBufferLength - g_usPayloadBufferOffset],
                                                  &g_usPayloadBufferOffset,
                                                  uchSeparateFlag,
                                                  &unPackSrcByteRecord);
                if (emRet == GH_PAYLOAD_HANDLE_PAYLOADFULL)
                {
                    if (uchSeparateFlag)
                    {
                        uchPackCntRecord = uchPackCnt;
                    }
                    HOOK_FUNC_CALL(g_pGh3x2xProtocolSendFunc, (g_uchProtocolCmd, g_puchPayloadBuffer, g_usPayloadBufferLength - g_usPayloadBufferOffset));
                    g_usPayloadBufferOffset = g_usPayloadBufferLength;
                    emHandleFlag = GH_PAYLOAD_HANDLE_FULL_HEADER;
                    break;
                }
            }
            if (uchPackCnt == uchPackNum)
            {
                if (emSendMode != GH_PROTOCOL_SENDMODE_FRAME || (unAllPackByteNum + uchFrameHeaderLength > g_usPayloadBufferOffset))
                {
                    uchSeparateStatus = 2;
                    g_puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG] |= (uchSeparateStatus << GH_PAYLOAD_FLAG_SEPARATE_STATUS);
                    if (emRet == GH_PAYLOAD_HANDLE_CONTINUE)
                    {
                        HOOK_FUNC_CALL(g_pGh3x2xProtocolSendFunc, (g_uchProtocolCmd, g_puchPayloadBuffer, g_usPayloadBufferLength - g_usPayloadBufferOffset));
                        g_usPayloadBufferOffset = g_usPayloadBufferLength;
                    }
                    uchSeparateStatus = 0;
                    uchPackCntRecord = 0;
                    uchSeparateID = 0;
                    emHandleFlag = GH_PAYLOAD_HANDLE_OVER;
                }
            }
            break;
        default:
            break;
        }
    }
    return GH_PAYLOAD_HANDLE_CONTINUE;
}

GU8 malloc_flag = 0;

EMPacyloadLostRet GH3X2X_PacketLossCheck(GU8 uchSeparateStatusRecord,
                                         GU8 uchSeparateStatus,
                                         GU8 uchSeparateIDRecord,
                                         GU8 uchSeparateID, 
                                         GU8 uchFrameIDRecord,
                                         GU8 uchFrameID)
{
    if (uchFrameID == 0 && uchSeparateID == 0 && uchFrameIDRecord == 0 && uchSeparateIDRecord == 0)
    {
        return GH_PAYLOAD_LOST_NOLOST;
    }
    if ((uchFrameID != uchFrameIDRecord && uchSeparateStatusRecord == 2 && uchSeparateStatus != 1)||
        (uchFrameID == uchFrameIDRecord && uchSeparateStatusRecord == 2))
    {
        GH3X2X_DEBUG_LOG_PARAM("GH_PAYLOAD_LOST_PREVIOUS_LOST\n");
        return GH_PAYLOAD_LOST_PREVIOUS_LOST;
    }
    if (uchFrameID != uchFrameIDRecord && uchSeparateStatusRecord == 1 && (uchSeparateID != uchSeparateIDRecord + 1 || uchSeparateStatus == 1))
    {
        GH3X2X_DEBUG_LOG_PARAM("GH_PAYLOAD_LOST_LAST_LOST\n");
        return GH_PAYLOAD_LOST_LAST_LOST;
    }
    if (uchFrameID == uchFrameIDRecord && uchSeparateID != uchSeparateIDRecord + 1)
    {
        GH3X2X_DEBUG_LOG_PARAM("GH_PAYLOAD_LOST_MID_LOST\n");
        return GH_PAYLOAD_LOST_MID_LOST;
    }
    if (uchFrameID != uchFrameIDRecord && uchSeparateStatusRecord != 2 && uchSeparateStatus != 1)
    {
        GH3X2X_DEBUG_LOG_PARAM("GH_PAYLOAD_LOST_DOUBLE_LOST\n");
        return GH_PAYLOAD_LOST_DOUBLE_LOST;
    }
    return GH_PAYLOAD_LOST_NOLOST;
}

void GH3X2X_MallocPackStruct(GU8 uchPackNum,  GU8* puchPackCntRecord, STProtocolPackInfo*** pstPackInfo, GU8** puchPackMallocFlag)
{
    *puchPackCntRecord = 0;
    *pstPackInfo = GH3X2X_UserMalloc(uchPackNum * sizeof(STProtocolPackInfo*));
    GH3X2X_Memset(*pstPackInfo, 0, uchPackNum * sizeof(STProtocolPackInfo*));
    *puchPackMallocFlag = GH3X2X_UserMalloc(uchPackNum * sizeof(GU8));
    GH3X2X_Memset(*puchPackMallocFlag, 0, uchPackNum * sizeof(GU8));
    for (GU8 uchPackCnt = 0; uchPackCnt < uchPackNum; uchPackCnt++)
    {
        (*pstPackInfo)[uchPackCnt] = GH3X2X_UserMalloc(sizeof(STProtocolPackInfo));
        GH3X2X_Memset((*pstPackInfo)[uchPackCnt], 0, sizeof(STProtocolPackInfo));
    }
}

void GH3X2X_FreePackStruct(GU8 uchPackNum, GU8* puchPackCntRecord, STProtocolPackInfo*** pstPackInfo, GU8** puchPackMallocFlag)
{
    for (GS8 uchPackCnt = uchPackNum - 1; uchPackCnt >= 0; uchPackCnt--)
    {

        GH3X2X_UserFree((*pstPackInfo)[uchPackCnt]->pSrc);
    }
    for (GS8 uchPackCnt = uchPackNum - 1; uchPackCnt >= 0; uchPackCnt--)
    {
        GH3X2X_UserFree((*pstPackInfo)[uchPackCnt]);
    }
    GH3X2X_UserFree(*puchPackMallocFlag);
    GH3X2X_UserFree(*pstPackInfo);
    *pstPackInfo = GH3X2X_PTR_NULL;
    *puchPackCntRecord = 0;
}

EMPayloadReceiveRet GH3X2X_ProtocolPayloadDecode(GU8 *puchPayloadBuffer,
                                                 GU16 usPayloadLen)
{
    static GU8 uchPackCntRecord = 0;
    static GU16 unPackSrcByteRecord = 0;
    static GU8 *puchPackMallocFlag = 0;
    static STProtocolPackInfo** pstPackInfo = GH3X2X_PTR_NULL;

    static GU8 uchFrameIDRecord = 0;
    static GU8 uchSeparateIDRecord = 0;
    static GU8 uchSeparateStatusRecord = 2;
    static EMPacyloadLostRet emLostRet = GH_PAYLOAD_LOST_NOLOST;

    GU16 usPayloadOffset = 0;
    GU8 uchSeparateID = 0;
    GU8 uchPackNum = 0;
    GU8 uchPackCnt = 0;
    GU8 uchFunctionID = 0;
    GU8 uchFrameID = 0;

    GU8 uchPayloadFlag = puchPayloadBuffer[GH_PAYLOADHEADER_OFFSET_FLAG];
    usPayloadOffset++;
    GU8 uchSeparateFlag = GH3X2X_CHECK_LEFT_BIT_SET(uchPayloadFlag, GH_PAYLOAD_FLAG_SEPARATE_ENABLE);
    GU8 uchSeparateStatus = GH3X2X_CHECK_LEFT_BIT_SET(uchPayloadFlag, GH_PAYLOAD_FLAG_SEPARATE_STATUS)| (GH3X2X_CHECK_LEFT_BIT_SET(uchPayloadFlag, GH_PAYLOAD_FLAG_SEPARATE_STATUS + 1) << 1);
    EMProtocolSendMode emPayloadMode = (EMProtocolSendMode)GH3X2X_CHECK_LEFT_BIT_SET(uchPayloadFlag, GH_PAYLOAD_FLAG_SEND_MODE);

    if (emPayloadMode != GH_PROTOCOL_SENDMODE_FAST)
    {
        uchFunctionID = puchPayloadBuffer[usPayloadOffset];
        usPayloadOffset++;
    }

    if (uchSeparateFlag)
    {
        uchSeparateID = puchPayloadBuffer[usPayloadOffset];
        usPayloadOffset++;
    }

    if (emPayloadMode != GH_PROTOCOL_SENDMODE_FAST)
    {
        uchFrameID = puchPayloadBuffer[usPayloadOffset];
        usPayloadOffset++;
    }

    //check lost payload
    if (uchSeparateFlag)
    {
        EMPayloadReceiveRet emRet = GH_PAYLOAD_HANDLE_FRAME_LOST_PAYLOAD;

        if (uchSeparateID == 0)
        {
            emRet = GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH;;
        }

        switch (emLostRet)
        {
        case GH_PAYLOAD_LOST_MID_LOST:
        case GH_PAYLOAD_LOST_DOUBLE_LOST:
        case GH_PAYLOAD_LOST_PREVIOUS_LOST:
            break;
        default:
            emRet = GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH;
            break;
        }

        if (emRet == GH_PAYLOAD_HANDLE_FRAME_LOST_PAYLOAD)
        {
            return emRet;
        }

        emLostRet = GH3X2X_PacketLossCheck(uchSeparateStatusRecord,
                                           uchSeparateStatus,
                                           uchSeparateIDRecord,
                                           uchSeparateID,
                                           uchFrameIDRecord,
                                           uchFrameID);

        emRet = GH_PAYLOAD_HANDLE_FRAME_LOST_PAYLOAD;
        switch (emLostRet)
        {
        case GH_PAYLOAD_LOST_LAST_LOST:
            emRet = GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH;
        case GH_PAYLOAD_LOST_MID_LOST:
        case GH_PAYLOAD_LOST_DOUBLE_LOST:
            GH3X2X_FreePackStruct(uchPackNum, &uchPackCntRecord, &pstPackInfo, &puchPackMallocFlag);
            if (malloc_flag == 1)
            {
                malloc_flag = 0;
            }
            unPackSrcByteRecord = 0;
            break;
        case GH_PAYLOAD_LOST_PREVIOUS_LOST:
            break;
        default:
            emRet = GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH;
            break;
        }

        if (emRet == GH_PAYLOAD_HANDLE_FRAME_LOST_PAYLOAD)
        {
            return emRet;
        }

        uchSeparateStatusRecord = uchSeparateStatus;
        uchFrameIDRecord = uchFrameID;
        uchSeparateIDRecord = uchSeparateID;
    }

    uchPackNum = puchPayloadBuffer[usPayloadOffset];
    
    if ((uchSeparateFlag && uchSeparateStatus == 1) || (uchSeparateFlag == 0))
    {
        GH3X2X_MallocPackStruct(uchPackNum, &uchPackCntRecord, &pstPackInfo, &puchPackMallocFlag);
        if (malloc_flag == 0)
        {
            malloc_flag = 1;
        }
    }

    usPayloadOffset++;

    for (uchPackCnt = uchPackCntRecord; uchPackCnt < uchPackNum; uchPackCnt++)
    {
        //pack id
        pstPackInfo[uchPackCnt]->uchPackID = puchPayloadBuffer[usPayloadOffset];
        usPayloadOffset++;
        //pack flag
        pstPackInfo[uchPackCnt]->uchFlag = puchPayloadBuffer[usPayloadOffset];
        usPayloadOffset++;
        //byte type
        if (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfo[uchPackCnt]->uchFlag, GH_PACK_FLAG_BYTETYPE))
        {
            pstPackInfo[uchPackCnt]->uchByteType = puchPayloadBuffer[usPayloadOffset];
            usPayloadOffset++;
        }
        //data num
        if (GH3X2X_CHECK_LEFT_BIT_SET(pstPackInfo[uchPackCnt]->uchFlag, GH_PACK_FLAG_DATANUM))
        {
            pstPackInfo[uchPackCnt]->uchDataNum = puchPayloadBuffer[usPayloadOffset];
            usPayloadOffset++;
        }
        if (puchPackMallocFlag[uchPackCnt] == 0)
        {
            pstPackInfo[uchPackCnt]->pSrc = GH3X2X_UserMalloc(pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum);
            GH3X2X_Memset(pstPackInfo[uchPackCnt]->pSrc, 0, pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum);
            puchPackMallocFlag[uchPackCnt] = 1;
        }
        //src
        GU16 usCpyLength = 0;
        usCpyLength = ((pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum - unPackSrcByteRecord) > (usPayloadLen - usPayloadOffset)\
                        ? (usPayloadLen - usPayloadOffset) : (pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum - unPackSrcByteRecord));
        GH3X2X_Memcpy(&(((GU8*)pstPackInfo[uchPackCnt]->pSrc)[unPackSrcByteRecord]), &puchPayloadBuffer[usPayloadOffset], usCpyLength);
        unPackSrcByteRecord += usCpyLength;
        usPayloadOffset += usCpyLength;
        uchPackCntRecord = uchPackCnt;
        if (pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum == unPackSrcByteRecord)
        {
            if (usPayloadOffset == usPayloadLen)
            {
                uchPackCntRecord++;
            }
            unPackSrcByteRecord = 0;
        }        
        if (pstPackInfo[uchPackCnt]->uchByteType * pstPackInfo[uchPackCnt]->uchDataNum == (usPayloadLen - usPayloadOffset))
        {
            uchPackCntRecord++;
        }
        if (usPayloadOffset == usPayloadLen)
        {
            break;
        }
    }
    if (uchPackCnt == uchPackNum || (unPackSrcByteRecord == 0 && uchPackCnt == (uchPackNum - 1)))
    {
        HOOK_FUNC_CALL(g_pGh3x2xProtocolFrameDataOutputFunc, (&uchFunctionID, &uchFrameID, pstPackInfo, &uchPackNum));
        GH3X2X_FreePackStruct(uchPackNum, &uchPackCntRecord, &pstPackInfo, &puchPackMallocFlag);
        uchSeparateStatusRecord = 2;
        uchFrameIDRecord = 0;
        uchSeparateIDRecord = 0;
        unPackSrcByteRecord = 0;
        if (malloc_flag == 1)
        {
            malloc_flag = 0;
        }
        return GH_PAYLOAD_HANDLE_FRAME_FINISH;
    }
    return GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH;
}


