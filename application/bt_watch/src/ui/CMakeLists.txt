# SPDX-License-Identifier: Apache-2.0

file(GLOB SOURCES *.c)

target_include_directories(app PRIVATE .)
target_sources(app PRIVATE ${SOURCES})

add_subdirectory_ifdef(CONFIG_LVGL widgets)
add_subdirectory_ifdef(CONFIG_VG_LITE svgrender)
add_subdirectory_ifdef(CONFIG_LV_GIF gif)
add_subdirectory_ifdef(CONFIG_LV_TEXT_ROLLER text_roller)
add_subdirectory_ifdef(CONFIG_LV_TEXT_ARC text_arc)

