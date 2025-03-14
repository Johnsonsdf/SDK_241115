/*******************************************************************************
 * @file    hr_algo.c
 * @author  MEMS Application Team
 * @version V1.0
 * @date    2021-5-25
 * @brief   sensor algorithm api
*******************************************************************************/

/******************************************************************************/
//includes
/******************************************************************************/
#include <string.h>
#include <hr_algo.h>
#include <gh_demo.h>

// #include <gh3x2x_drv.h>
// #include <gh3x2x_demo.h>
/******************************************************************************/
//functions
/******************************************************************************/
/* Init sensor algo */
int hr_algo_init(const hr_os_api_t *api)
{
	// printk("func[%s]line[%d]enter 3x2x init\n",__func__,__LINE__);
	Gh3x2xDemoInit();
	// printk("func[%s]line[%d]end 3x2x init\n",__func__,__LINE__);
	return 0;
}

/* Start hr sensor */
int hr_algo_start(int mode)
{
	if (HB==mode)
	{
		Gh3x2xDemoStartSampling(GH3X2X_FUNCTION_HR);
	}
	else if (SPO2==mode)
	{
		Gh3x2xDemoStartSampling(GH3X2X_FUNCTION_SPO2);
	}
	else if (HRV==mode)
	{
		Gh3x2xDemoStartSampling(GH3X2X_FUNCTION_HRV);	
	}
	
	return 0;
}

/* Stop hr sensor */
int hr_algo_stop(void)
{
	Gh3x2xDemoStopSampling(GH3X2X_FUNCTION_HR);
	return 0;
}

/* Process data through call-back handler */
int hr_algo_process(void)
{
	Gh3x2xDemoInterruptProcess();
	return 0;
}
