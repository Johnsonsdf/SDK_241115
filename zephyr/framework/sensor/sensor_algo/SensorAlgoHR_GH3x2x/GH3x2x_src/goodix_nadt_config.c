#ifndef Goodix_DSP_EXPORTS
#include "goodix_nadt.h"
#include "gh3x2x_demo_algo_call.h"
#if 0
const goodix_nadt_config external_cfg =
{
	.sampleRateNADT = 25,				//������
	.unwearDetectLevel = 3,				//����ȼ�
	.sleepStatus = 0,					//˯��״̬
	//
	.checkTimeoutThr = 1200,			//�����ⳬʱʱ��
	.unwearCheckCntThr = 4,				//����ȷ��ʱ��
	.wearCheckCntThr = 2,				//����ȷ��ʱ��
	//
	//ADT�ж�
	.adtWearOnThr = 8638608,			//ADT wear on��ֵ
	.adtWearOffThr = 8588608,			//ADT wear off��ֵ
	//
	//�˶��ж�
	.accStdThr = 25,					//ACC��׼��
	//
	//������
	.liveDetectEnable = 1,				//������ʹ��
	.rawPVLenThr = 300,					//�ߴ�г����ֵ
	.heartBeatThrLow = 50,				//��������
	.heartBeatThrHigh = 140,			//��������
	.difRrInterThr = 8,					//���ʱ仯��ֵ
	.wearAcfScoreThr = 50,				//����ط�����ֵ
	.baseLinePosRatioThr = 30,			//���ߵ�������ֵ
	.baseLineDiffThr = 15,			    //����ƽ̹����ֵ
	.sigScaleThr = 1000,
	//
	//��Ƶ��������
	.bgJumpDetectEnable = 1,			//������ͻ����ʹ��
	.jumpBgDiffThr = 100,				//������仯����ֵ
	.bgPeakValleyNumThr = 12,			//�������ȸ�����ֵ
	//
	//���������ڼ��
	.bgPeriodDetectEnable = 1,			//���������ڼ��ʹ��
	.periodBgDiffThr = 10,				//������仯����ֵ
	.bgPeriodThr = 10,					//������������ֵ
	.bgPeriodDiffThr = 5				//���������ڲ�����ֵ
};

const void *goodix_nadt_config_get_arr()
{
	return &external_cfg;
}

GS32 goodix_nadt_config_get_size()
{
	return sizeof(goodix_nadt_config);
}
#endif

void goodix_nadt_config_get_version(char* ver, GU8 ver_len)
{
	GU32 copy_num = ver_len < (strlen(NADT_INTERFACE_VERSION) + 1) ? ver_len : (strlen(NADT_INTERFACE_VERSION) + 1);
	GH3X2X_Memcpy(ver, NADT_INTERFACE_VERSION, copy_num);
	ver[copy_num - 1] = '\0'; //���NADT_CONFIG_VER�ĳ��ȴ���size����Ҫ��ӽ�����
}

#endif
