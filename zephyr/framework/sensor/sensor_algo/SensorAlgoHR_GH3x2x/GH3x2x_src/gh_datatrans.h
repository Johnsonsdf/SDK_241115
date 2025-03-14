/**
 * @copyright (c) 2003 - 2022, Goodix Co., Ltd. All rights reserved.
 *
 * @file    gh3x2x_drv_dump.h
 *
 * @brief   gh3x2x data transform
 *
 * @version ref gh3x2x_drv_datatrans.h
 *
 */

#ifndef _GH3X2X_DEMO_COMMUNICATE_H_
#define _GH3X2X_DEMO_COMMUNICATE_H_

#include "gh_drv.h"

#define __PROTOCOL_PACK_ZIP_DISABLE__ (0)
#define __PROTOCOL_PACK_ZIP_ENABLE__ (1)

#define PROTOCOL_RET_NEED_TO_SEND (1)
#define PROTOCOL_RET_CONTINUE (0)
#define PROTOCOL_RET_BUFFER_FULL (-1)

#define PAYLOAD_INFO_FLAG_OFFSET_FUNCID     (0)
#define PAYLOAD_INFO_FLAG_OFFSET_FLAG       (1)
#define PAYLOAD_INFO_FLAG_OFFSET_FRAMEID    (2)
#define PAYLOAD_INFO_FLAG_OFFSET_PAYLOADID  (3)

typedef enum
{
    GH_PROTOCOL_FUNCID_TOP_PACKTYPE_ID = 0,
    GH_PROTOCOL_FUNCINFO_TOP_PACKTYPE_ID,
    GH_PROTOCOL_CHNLMAP_TOP_PACKTYPE_ID,
    GH_PROTOCOL_RAWDATA_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_AGC_INFO_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_AMB_DATA_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_FLAG_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_ALGO_RESULT_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_GS_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_CAP_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_TEMP_FRAME_PACKTYPE_ID,
    GH_PROTOCOL_PACK_NUM_MAX,
} EMProtocolPacktypeID;

typedef enum
{
    GH_PROTOCOL_SENDMODE_FAST = 0,
    GH_PROTOCOL_SENDMODE_FRAME = 1,
    GH_PROTOCOL_SENDMODE_FRAME_FAST = 2,
} EMProtocolSendMode;

typedef enum
{
    GH_PROTOCOL_SENDHANDLE_FULLSEND = 0,
    GH_PROTOCOL_SENDHANDLE_NOWSEND = 1,
} EMProtocolSendHandle;

typedef enum
{
    GH_PAYLOAD_HANDLE_CONTINUE = 0,
    GH_PAYLOAD_HANDLE_PAYLOADFULL,
    GH_PAYLOAD_HANDLE_NOINIT,
    GH_PAYLOAD_HANDLE_DATAERR,
    GH_PAYLOAD_HANDLE_ADDRNULL,
} EMPayloadHandleRet;

typedef enum
{
    GH_PAYLOAD_HANDLE_FRAME_NOT_FINISH = 0,
    GH_PAYLOAD_HANDLE_FRAME_FINISH,
    GH_PAYLOAD_HANDLE_FRAME_LOST_PAYLOAD,
} EMPayloadReceiveRet;

typedef enum
{
    GH_PAYLOAD_LOST_NOLOST = 0,
    GH_PAYLOAD_LOST_PREVIOUS_LOST,
    GH_PAYLOAD_LOST_LAST_LOST,
    GH_PAYLOAD_LOST_MID_LOST,
    GH_PAYLOAD_LOST_DOUBLE_LOST,
} EMPacyloadLostRet;

typedef enum
{
    GH_PAYLOAD_PACKHEADER_OFFSET_PACKID = 0,
    GH_PAYLOAD_PACKHEADER_OFFSET_FLAG,
    GH_PAYLOAD_PACKHEADER_OFFSET_BYTETYPE,
    GH_PAYLOAD_PACKHEADER_OFFSET_DATANUM,
} EMPackHeaderByteOffset;

typedef enum
{
    GH_PACK_FLAG_FRAME_START = 0,
    GH_PACK_FLAG_ZIP_FLAG,
    GH_PACK_FLAG_ZIP_TYPE,
    GH_PACK_FLAG_BYTETYPE,
    GH_PACK_FLAG_DATANUM,
} EMPackFlagBitOffset;

typedef enum
{
    GH_PAYLOADHEADER_OFFSET_FLAG= 0,
    GH_PAYLOADHEADER_OFFSET_FUNCID,
    GH_PAYLOADHEADER_OFFSET_SEPARATEID,
} EMPayloadHeaderByteOffset;

typedef enum
{
    GH_PAYLOAD_FLAG_SEPARATE_ENABLE = 0,
    GH_PAYLOAD_FLAG_SEPARATE_STATUS = 1,
    GH_PAYLOAD_FLAG_SEND_MODE = 3,
} EMPayloadFlagBitOffset;

typedef enum
{
    GH_PAYLOAD_HANDLE_OVER = 0,
    GH_PAYLOAD_HANDLE_FULL_HEADER,
    GH_PAYLOAD_HANDLE_FULL_DATA,
} EMPayloadHandleProcess;

typedef struct
{
    GU8 uchPackID;
    GU8 uchFlag;
    GU8 uchByteType;
    GU8 uchDataNum;
    GU8 *pSrc;
} STProtocolPackInfo;

typedef void (*pfnSendProtocolDataFunc)(GU8 uchCmd, GU8* puchProtocolDataBuffer, GU16 usProtocolDataLen);
typedef void (*pfnFrameDataOutputFunc)(GU8* puchFunctionID, GU8* puchFrameID, STProtocolPackInfo** pstReceivePackInfo, GU8* puchPackNum);

EMPayloadHandleRet GH3X2X_ProtocolSectionInit(GU8 uchProtocolCmd,
                                              GU8 *puchPayloadDataBuffer,
                                              GU16 usPayloadDataBufferLength,
                                              void (*pfnSendProtocolDataFunc)(GU8 uchCmd, GU8* puchBuffer, GU16 usLen),
                                              void (*pfnFrameDataOutputFunc)(GU8* puchFunctionID,
                                                                             GU8* puchFrameID,
                                                                             STProtocolPackInfo** pstReceivePackInfo,
                                                                             GU8* puchPackNum));

EMPayloadHandleRet GH3X2X_ProtocolPayloadHandle(EMProtocolSendMode emSendMode,
    EMProtocolSendHandle emSendHandle,
    GU8 uchFunctionID,
    GU8 uchFrameID,
    STProtocolPackInfo* pstPackInfo,
    GU8 uchPackNum);

EMPayloadReceiveRet GH3X2X_ProtocolPayloadDecode(GU8 *puchPayloadBuffer,
                                                 GU16 usPayloadLen);

#endif /* _GH3X2X_DRV_DUMP_H_ */

/********END OF FILE********* Copyright (c) 2003 - 2022, Goodix Co., Ltd. ********/
