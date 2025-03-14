# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LVGL)

set(ZEPHYR_CURRENT_LIBRARY lvgl)

zephyr_interface_library_named(lvgl)
zephyr_library()

zephyr_include_directories(${LVGL_ROOT_DIR} ${LVGL_ROOT_DIR}/../)
#zephyr_compile_definitions(LV_CONF_INCLUDE_SIMPLE=1)
zephyr_compile_definitions(LV_CONF_PATH=lvgl/porting/zephyr/lv_conf.h)

file(GLOB_RECURSE SOURCES ${LVGL_ROOT_DIR}/src/*.c ${LVGL_ROOT_DIR}/demos/*.c)
file(GLOB SOURCES_PORTING ${LVGL_ROOT_DIR}/porting/*.c)
file(GLOB_RECURSE SOURCES_PORTING_DECODER ${LVGL_ROOT_DIR}/porting/decoder/*.c)
file(GLOB_RECURSE SOURCES_PORTING_GPU ${LVGL_ROOT_DIR}/porting/gpu/*.c)
file(GLOB_RECURSE SOURCES_PORTING_ENV ${LVGL_ROOT_DIR}/porting/zephyr/*.c)

zephyr_library_sources(
    ${SOURCES}
    ${SOURCES_PORTING}
    ${SOURCES_PORTING_DECODER}
    ${SOURCES_PORTING_GPU}
    ${SOURCES_PORTING_ENV}
)

if(CONFIG_LV_USE_THORVG_INTERNAL)
    file(GLOB_RECURSE THORVG_SOURCES ${LVGL_ROOT_DIR}/src/extra/libs/thorvg/*.cpp)
    zephyr_library_sources(${THORVG_SOURCES})
    zephyr_library_compile_definitions(
        _GNU_SOURCE
    )
endif()

zephyr_library_link_libraries(lvgl)
target_link_libraries(lvgl INTERFACE zephyr_interface)

endif()
