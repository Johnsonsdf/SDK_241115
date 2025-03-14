/******************************************************************************/
/*                                                                            */
/*    Copyright 2023 by AICXTEK TECHNOLOGIES CO.,LTD. All rights reserved.    */
/*                                                                            */
/******************************************************************************/

/**
 *  DESCRIPTION
 *
 *    This file declares the interfaces of SMS in telephony service.
 *    SMS service is managed to maintain SMS list, including concatecated cases, indexes, encoding and
 * decoding SMS protocol data units(PDU). It is based on tele sms with pdu service.
 */
#ifndef __AIC_SRV_TELE_SMS_H__
#define __AIC_SRV_TELE_SMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**********************
 * INCLUDES
 **********************/
#include "aic_srv_bus.h"
#include <aic_type.h>

/*********************
 *      DEFINES
 *********************/
#define TS_SMS_INVALID_HANDLE (-1)
/*10-octet BCD plus +*/
#define TS_SMS_NUM_MAX_LEN (21)
#define TS_SMS_PDU_MAX_LEN (176 * 2)

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    TS_EVENT_SMS_WITH_PDU_BASE = (0x07 << 16),
    /* responses */
    TS_EVENT_SMS_SEND_WITH_PDU_RESP,
    TS_EVENT_SMS_SEND_ACK_WITH_PDU_RESP,
    TS_EVENT_SMS_WRITE_WITH_PDU_TO_SIM_RESP,
    TS_EVENT_SMS_DEL_IN_SIM_WITH_PDU_RESP,
    TS_EVENT_SMS_SET_SMSC_WITH_PDU_RESP,
    TS_EVENT_SMS_GET_SMSC_WITH_PDU_RESP,
    TS_EVENT_SMS_GET_LIST_IN_SIM_WITH_PDU_RESP,
    TS_EVENT_SMS_READ_IN_SIM_WITH_PDU_RESP,
    TS_EVENT_SMS_GET_STOR_STAT_IN_SIM_WITH_PDU_RESP,

    /* indicator */
    TS_EVENT_SMS_IND_WITH_PDU_START = (TS_EVENT_SMS_WITH_PDU_BASE + 100),
    /* NEW SMS ARRIVAL - type: ts_sms_new_with_idx_ind_t */
    TS_EVENT_SMS_NEW_WITH_IDX_IND,
    /* NEW SMS STATUS REPORT - type: ts_sms_status_report_with_pdu_ind_t */
    TS_EVENT_SMS_STATUS_REPORT_WITH_PDU_IND,
    /* SMS STORAGE STATUS CHANGED - type: ts_sms_storage_status_with_pdu_ind_t */
    TS_EVENT_SMS_STORAGE_STATUS_WITH_PDU_IND,
    /* NEW SMS ARRIVAL WITH PDU - type: ts_sms_new_with_pdu_ind_t */
    TS_EVENT_SMS_NEW_WITH_PDU_IND,

    TS_EVENT_SMS_WITH_PDU_MAX
} ts_event_sms_with_pdu_e;

/**
 * enum ts_sms_rec_status_e - sms record status
 * Note: the value must align with <stat> in 3GPP TS 27.005
 * @TS_SMS_REC_STATUS_UNREAD: unread record
 * @TS_SMS_REC_STATUS_READ: read record
 * @TS_SMS_REC_STATUS_UNSENT: unsent record
 * @TS_SMS_REC_STATUS_SENT: sent record
 * @TS_SMS_REC_STATUS_ALL: all records
 */
typedef enum {
    TS_SMS_REC_STATUS_UNREAD = 0,
    TS_SMS_REC_STATUS_READ = 1,
    TS_SMS_REC_STATUS_UNSENT = 2,
    TS_SMS_REC_STATUS_SENT = 3,
    TS_SMS_REC_STATUS_ALL = 4,
} ts_sms_rec_status_e;

/**
 * enum ts_sms_box_type_e - sms box type
 * @TS_SMS_BOX_TYPE_INBOX: inbox
 * @TS_SMS_BOX_TYPE_OUTBOX: outbox
 * @TS_SMS_BOX_TYPE_ALL: all
 */
typedef enum {
    TS_SMS_BOX_TYPE_INBOX = 0,
    TS_SMS_BOX_TYPE_OUTBOX = 1,
    TS_SMS_BOX_TYPE_ALL = 2,
} ts_sms_box_type_e;

typedef enum {
    TS_EVENT_SMS_BASE = TS_EVENT_SMS_WITH_PDU_MAX + 100,
    /* responses */
    TS_EVENT_SMS_SEND_RESP,
    TS_EVENT_SMS_DEL_IN_SIM_RESP,
    TS_EVENT_SMS_SET_SMSC_RESP,
    TS_EVENT_SMS_GET_SMSC_RESP,
    TS_EVENT_SMS_GET_LIST_IN_SIM_RESP,
    TS_EVENT_SMS_READ_IN_SIM_RESP,
    TS_EVENT_SMS_LOAD_FROM_SIM_RESP,

    /* notifications */
    TS_EVENT_SMS_IND_START = TS_EVENT_SMS_BASE + 100,
    /* NEW SMS ARRIVAL - type: ts_sms_new_ind_t */
    TS_EVENT_SMS_NEW_IND,
    /* NEW SMS STATUS REPORT - type: ts_sms_status_report_ind_t */
    TS_EVENT_SMS_STATUS_REPORT_IND,
    /* SMS STORAGE STATUS UPDATE - type: ts_sms_storage_status_ind_t */
    TS_EVENT_SMS_STORAGE_STATUS_IND,
    TS_EVENT_SMS_MAX
} ts_event_sms_e;

/**
 * enum - sms class. refer to 3GPP TS 23.038
 * @TS_SMS_CLASS_0: class 0
 * @TS_SMS_CLASS_1: class 1
 * @TS_SMS_CLASS_2: class 2
 * @TS_SMS_CLASS_3: class 3
 * @TS_SMS_CLASS_NO: no message class
 */
typedef enum {
    TS_SMS_CLASS_0 = 0,
    TS_SMS_CLASS_1,
    TS_SMS_CLASS_2,
    TS_SMS_CLASS_3,
    TS_SMS_CLASS_NO,
} ts_sms_class_t;

/**
 * struct ts_sms_time_stamp_t - SMS time stamp type descriptor
 * @year: year
 * @month: month: 1~12
 * @day: day: 1~31
 * @hour: hour: 0~23
 * @min: minute: 0~59
 * @sec: second: 0~59
 */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} ts_sms_time_stamp_t;

/**
 * struct ts_sms_entry_t - sms record entry
 * @index: index
 * @status: status of the item
 */
typedef struct {
    int32_t index;
    ts_sms_rec_status_e status;
} ts_sms_entry_t;

/**
 * struct ts_sms_send_info_t - info of sms sending
 * @p_smsc: sms center, use "" or NULL to set smsc as default one
 * @p_number: recipient number
 * @p_text: sms text to send, encoded as UTF8
 * @is_write_to_sim: set true to write the sms to SIM
 */
typedef struct {
    const char *p_smsc;
    const char *p_number;
    const char *p_text;
    bool is_write_to_sim;
} ts_sms_send_info_t;

/**
 * struct ts_sms_general_resp_t - general response of sms operation
 * @event_id: event id, refer to ts_event_sms_e
 * @card_id: sim slot
 * @err_code: error code. 0 for success, otherwise refers to HRilErrNumber
 */
typedef struct {
    ts_event_sms_e event_id;
    uint8_t card_id;
    int32_t err_code;
} ts_sms_general_resp_t;
/**
 * struct ts_sms_send_resp_t - response of sms sending
 * @base_info: basic response info
 * @index: index of the pdu written into SIM. -1 means not saved.
 */
typedef struct {
    ts_sms_general_resp_t base_info;
    int32_t index;
} ts_sms_send_resp_t;

/**
 * struct ts_sms_del_in_sim_resp_t - response of sms deleting
 * @base_info: basic response info
 */
typedef struct {
    ts_sms_general_resp_t base_info;
} ts_sms_del_in_sim_resp_t;

/**
 * struct ts_sms_set_smsc_resp_t - response of sms setting smsc
 * @base_info: basic response info
 */
typedef struct {
    ts_sms_general_resp_t base_info;
} ts_sms_set_smsc_resp_t;

/**
 * struct ts_sms_get_smsc_resp_t - response of sms getting smsc
 * @base_info: basic response info
 * @smsc: smsc of string type
 */
typedef struct {
    ts_sms_general_resp_t base_info;
    char smsc[TS_SMS_NUM_MAX_LEN + 1];
} ts_sms_get_smsc_resp_t;

/**
 * struct ts_sms_get_list_in_sim_resp_t - response of sms getting entry list in sim
 * @base_info: basic response info
 * @box: box type
 * @count: total count of info list
 * @list_start: place holder of list start
 * Note: the size of ts_sms_get_list_in_sim_resp_t is variable.
 * Suggestion to use:
 * ts_sms_get_list_in_sim_resp_t *resp = (ts_sms_get_list_in_sim_resp_t *)p_param;
 * ts_sms_entry_t *list = (ts_sms_entry_t *)(&resp->list_start);
 * for (uint16_t i = 0; i < resp->count; i++) {
 *    // try to use list[i] as each item
 * }
 */
typedef struct {
    ts_sms_general_resp_t base_info;
    ts_sms_box_type_e box;
    uint16_t count;
    uint32_t list_start;
} ts_sms_get_list_in_sim_resp_t;

/**
 * struct ts_sms_read_in_sim_resp_t - response of sms reading entry in sim
 * @base_info: basic response info
 * @index: index
 * @sms_class: sms class
 * @number: recipient(submit) / origination(deliver) number
 * @time_stamp: timestamp of the sms
 * @text_start: place holder of text start, encoded as UTF8, ended with '\0'
 * Note: the size of ts_sms_read_in_sim_resp_t is variable.
 * Suggestion to use:
 * ts_sms_read_in_sim_resp_t *resp = (ts_sms_read_in_sim_resp_t *)p_param;
 * char *text = (char *)(&resp->text_start);
 */
typedef struct {
    ts_sms_general_resp_t base_info;
    int32_t index;
    ts_sms_class_t sms_class;
    char number[TS_SMS_NUM_MAX_LEN + 1];
    ts_sms_time_stamp_t time_stamp;
    uint32_t text_start;
} ts_sms_read_in_sim_resp_t;

/**
 * struct ts_sms_load_from_sim_resp_t - response of load from sim
 * @base_info: basic response info
 *             err_code: 0 - success, -1 - sim is not present, -2 - sim is lock
 * @is_full: true for full, false for not full
 * @count: the sms count in sim, and the list index is started from 1 to count.
 */
typedef struct {
    ts_sms_general_resp_t base_info;
    bool is_full;
    uint16_t count;
}ts_sms_load_from_sim_resp_t;

/**
 * struct ts_sms_new_ind_t - indicator of new sms arrival
 * @event_id: event id
 * @card_id: sim slot
 * @index: index (not record number in EFsms)
 * @status: status of the sms
 */
typedef struct {
    ts_event_sms_e event_id;
    uint8_t card_id;
    int32_t index;
    ts_sms_rec_status_e status;
} ts_sms_new_ind_t;

/**
 * struct ts_sms_status_report_ind_t - indicator of sms status report
 * @event_id: event id, i.e. TS_EVENT_SMS_STATUS_REPORT_IND
 * @card_id: sim slot
 * @tp_st: refer to 3GPP TS 23.040 9.2.3.15 TP-Status (TP-ST)
 * @number: recipient(submit) / origination(deliver) number
 * @time_stamp: timestamp of the sms
 * @text_start: place holder of text start, encoded as UTF8, ended with '\0'
 * Note: the size of ts_sms_read_in_sim_resp_t is variable.
 * Suggestion to use:
 * ts_sms_read_in_sim_resp_t *resp = (ts_sms_read_in_sim_resp_t *)p_param;
 * char *text = (char *)(&resp->text_start);
 */
typedef struct {
    ts_event_sms_e event_id;
    uint8_t card_id;
    uint8_t tp_st;
    char number[TS_SMS_NUM_MAX_LEN + 1];
    ts_sms_time_stamp_t time_stamp;
    uint32_t text_start;
} ts_sms_status_report_ind_t;

/**
 * struct ts_sms_storage_status_ind_t - indicator of sms sim storage status
 * @event_id: event id, i.e. TS_EVENT_SMS_STORAGE_STATUS_IND
 * @card_id: sim slot
 * @is_full: true for full, false for not full
 * @is_ready: true for ready, false for not exception(e.g. SIM is removed or PUK triggerred)
 */
typedef struct {
    ts_event_sms_e event_id;
    uint8_t card_id;
    bool is_full;
    bool is_ready;
} ts_sms_storage_status_ind_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/**
 * aic_srv_tele_sms_load_from_sim
 * @card_id: [IN] sim slot
 * @_response_callback: [IN] callback to receive response
 * Remarks
 *   response event: TS_EVENT_SMS_LOAD_FROM_SIM_RESP
 *   response struct: ts_sms_load_from_sim_resp_t
 *
 * Return: 0 if success, otherwise -1
 */
int32_t aic_srv_tele_sms_load_from_sim(uint8_t card_id, srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_register - register a new client to recevie notification from SMS service
 * @p_bus_info: [IN] client information. Reserved for future use
 * @_srv_callback: [IN] client callback
 *
 * Return: client handle if success, TS_SMS_INVALID_HANDLE for failure
 * Remarks
 *   _srv_callback will be invoked once any of the indication events listed in ts_event_sms_e occurs.
 */
int32_t aic_srv_tele_sms_register(aic_srv_bus_info_t *p_bus_info, srv_func_callback _srv_callback);

/**
 * aic_srv_tele_sms_unregister - deregister a client
 * @client_handle: [IN] client handle
 *
 * Return: 0 if success, otherwise -1
 */
int32_t aic_srv_tele_sms_unregister(int32_t client_handle);

/**
 * aic_srv_tele_sms_send - send a sms
 * @card_id: [IN] sim slot. Reserved for future use
 * @p_sms_info: [IN] pointer to sms info
 * @_response_callback: [IN] callback to receive response
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_SEND_RESP
 *   response struct: ts_sms_send_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_send(uint8_t card_id, const ts_sms_send_info_t *p_sms_info,
                                 srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_del_in_sim - delete sms in sim
 * @card_id: [IN] sim slot. Reserved for future use
 * @p_indexes: [IN] pointer to an array of indexes
 * @count: [IN] count of indexes
 * @_response_callback: [IN] callback to receive response
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_DEL_IN_SIM_RESP
 *   response struct: ts_sms_del_in_sim_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_del_in_sim(uint8_t card_id, const int32_t *p_indexes, int32_t count,
                                       srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_set_smsc - set sms center address
 * @card_id: [IN] sim slot. Reserved for future use
 * @p_address: [IN] smsc of string type
 * @_response_callback: [IN] callback to receive response
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_SET_SMSC_RESP
 *   response struct: ts_sms_set_smsc_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_set_smsc(uint8_t card_id, const char *p_address, srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_get_smsc - get sms center address
 * @card_id: [IN] sim slot. Reserved for future use
 * @_response_callback: [IN] callback to receive response
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_GET_SMSC_RESP
 *   response struct: ts_sms_get_smsc_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_get_smsc(uint8_t card_id, srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_get_list_in_sim_with_pdu - get sms pdu list of specified box in sim
 * @card_id: [IN] sim slot. Reserved for future use
 * @box: [IN] box type
 * @_response_callback: [IN] callback to receive response
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_GET_LIST_IN_SIM_RESP
 *   response struct: ts_sms_get_list_in_sim_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_get_list_in_sim(uint8_t card_id, ts_sms_box_type_e box,
                                            srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_read_in_sim - read an sms from sim
 * @card_id: [IN] sim slot. Reserved for future use
 * @index: [IN] index of the record in SIM
 * @_response_callback: [IN] callback to receive response
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   response event: TS_EVENT_SMS_READ_IN_SIM_RESP
 *   response struct: ts_sms_read_in_sim_resp_t
 *   editor note: Multi-SIM is not supported yet.
 */
int32_t aic_srv_tele_sms_read_in_sim(uint8_t card_id, uint32_t index, srv_func_callback _response_callback);

/**
 * aic_srv_tele_sms_enable_status_report - enable status report
 * @card_id: [IN] sim slot. Reserved for future use
 * @enable: [IN] true to enable, false to disable
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   editor note: Multi-SIM is not supported yet
 *   1. this interface is addressed to switch on/off the notifiation upon status report received rather than to enable
 * status report in sms sending
 */
int32_t aic_srv_tele_sms_enable_status_report(uint8_t card_id, bool enable);

/**
 * aic_srv_tele_sms_set_valid_period - set valid period
 * @card_id: [IN] sim slot. Reserved for future use
 * @is_present: [IN] tp-vp is present or not
 * @tp_vp: [IN] value options refer to 3GPP TS 23.040 9.2.3.12.1 TP-VP (Relative format)
 *
 * Return: 0 if success, otherwise -1
 * Remarks
 *   editor note: Multi-SIM is not supported yet
 */
int32_t aic_srv_tele_sms_set_valid_period(uint8_t card_id, bool is_present, uint8_t tp_vp);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* __AIC_SRV_TELE_SMS_H__ */
