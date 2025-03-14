#ifndef Goodix_DSP_EXPORTS
#include <string.h>
#include "goodix_hrv.h"
#include "gh3x2x_demo_algo_call.h"
#if 0
const goodix_hrv_config external_hrv_cfg =
{
	.need_ipl = 1,			// �Ƿ���Ҫ��ֵ��1KHz
	.fs = 100,			    // ������
	.acc_thr[0] = 200000,   // acc �����ֵ��Ĭ��ֵΪ��20/10/3/3
	.acc_thr[1] = 100000,
	.acc_thr[2] = 30000,
	.acc_thr[3] = 30000,
};

const void *goodix_hrv_config_get_arr()
{
	return &external_hrv_cfg;
}

int32_t goodix_hrv_config_get_size()
{
	return sizeof(external_hrv_cfg);
}
#endif

void goodix_hrv_config_get_version(char* ver, uint8_t ver_len)
{
	uint32_t copy_num = ver_len < (strlen(HRV_INTERFACE_VERSION) + 1) ? ver_len : (strlen(HRV_INTERFACE_VERSION) + 1);
	GH3X2X_Memcpy(ver, HRV_INTERFACE_VERSION, copy_num);
	ver[copy_num - 1] = '\0'; //���NET_VERSION�ĳ��ȴ���size����Ҫ��ӽ�����
}

#endif //Goodix_DSP_EXPORTS
