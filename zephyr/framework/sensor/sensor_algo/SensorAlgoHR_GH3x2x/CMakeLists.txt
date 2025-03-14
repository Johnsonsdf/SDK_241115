# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(
	.
	GH3x2x_src
	# GH3x2x_algo_application
	# GH3x2x_algo_call
	# GH3x2x_kernel_code
)


# FILE(GLOB algo_application GH3x2x_algo_application/*.c)
# FILE(GLOB algo_call GH3x2x_algo_call/*.c)
# FILE(GLOB kernel_code GH3x2x_kernel_code/*.c)
FILE(GLOB GH3x2x_src_code GH3x2x_src/*.c)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

zephyr_sources(
	# ${algo_application}
	# ${algo_call}
	# ${kernel_code}
	${GH3x2x_src_code}
	hr_algo.c
)
target_link_libraries(${ZEPHYR_CURRENT_LIBRARY} PRIVATE 
	-Wl,--gc-sections,--allow-multiple-definition,--start-group
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_spo2.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_ecg.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_hr.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_hrv.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_nadt.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/cortexM4_eabi-gcc-hard_common.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/libgh3x2x_drv_cortexM4l_band_gcc_hard_agc.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/libgh3x2x_drv_cortexM4l_band_gcc_hard_ecg.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/libgh3x2x_drv_cortexM4l_band_gcc_hard_kernel.a
	# ${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib/libgh3x2x_drv_cortexM4l_band_gcc_hard_zip.a

	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_common_dl.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_common_dsp.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_ecg.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_hr.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_nadt_cardiff.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortex-m4_arm-gcc-4.9.3_os-hard_spo2.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/cortexM4_arm-none-eabi-gcc493-hard.a
	${CMAKE_CURRENT_LIST_DIR}/GH3x2x_Lib_new/libgh3x2x_drv_cortexM4l_band_gcc_hard_common.a
	-Wl,--end-group
)
