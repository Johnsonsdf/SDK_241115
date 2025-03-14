#ifndef Goodix_DSP_EXPORTS
#include "goodix_hba.h"
#include "gh3x2x_demo_algo_call.h"
#if 0
const goodix_hba_config external_hba_cfg =
{
	.mode = HBA_TEST_DYNAMIC,			// ��Чͨ����
	.scence = HBA_SCENES_DEFAULT,
	.fs = 25,			    // ԭʼ����
	.valid_channel_num = 4,
};

const void *goodix_hba_config_get_arr()
{
	return &external_hba_cfg;
}

int32_t  goodix_hba_config_get_size()
{
	return sizeof(external_hba_cfg);
}
#endif

void goodix_hba_config_get_version(char* ver, uint8_t ver_len)
{
	uint32_t copy_num = ver_len < (strlen(HBA_INTERFACE_VERSION) + 1) ? ver_len : (strlen(HBA_INTERFACE_VERSION) + 1);
	GH3X2X_Memcpy(ver, HBA_INTERFACE_VERSION, copy_num);
	ver[copy_num - 1] = '\0'; //���NET_VERSION�ĳ��ȴ���size����Ҫ��ӽ�����
}

#endif //Goodix_DSP_EXPORTS
