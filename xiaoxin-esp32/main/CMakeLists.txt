set(SOURCES "audio_codecs/audio_codec.cc"
            "audio_codecs/no_audio_codec.cc"
            "audio_codecs/box_audio_codec.cc"
            "audio_codecs/es8311_audio_codec.cc"
            "audio_codecs/es8388_audio_codec.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "protocols/protocol.cc"
            "iot/thing.cc"
            "iot/thing_manager.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "background_task.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "audio_codecs" "protocols" "audio_processing")

# 添加 IOT 相关文件
file(GLOB IOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/iot/things/*.cc)
list(APPEND SOURCES ${IOT_SOURCES})

# 添加板级公共文件
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

# 根据 BOARD_TYPE 配置添加对应的板级文件
if(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3)
    set(BOARD_TYPE "xiaoxin-esp32s3")
elseif(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3_TOUCH)
    set(BOARD_TYPE "xiaoxin-esp32s3-touch")
elseif(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3_096)
    set(BOARD_TYPE "xiaoxin-esp32s3-096")
elseif(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3_028)
    set(BOARD_TYPE "xiaoxin-esp32s3-028")
elseif(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3_154)
    set(BOARD_TYPE "xiaoxin-esp32s3-154")
elseif(CONFIG_BOARD_TYPE_XIAOXIN_ESP32S3_4G)
    set(BOARD_TYPE "xiaoxin-esp32s3-4G")
elseif(CONFIG_BOARD_TYPE_xiaoxin_zhi)
    set(BOARD_TYPE "xiaoxin-zhi")
endif()
file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})

if(CONFIG_CONNECTION_TYPE_MQTT_UDP)
    list(APPEND SOURCES "protocols/mqtt_protocol.cc")
elseif(CONFIG_CONNECTION_TYPE_WEBSOCKET)
    list(APPEND SOURCES "protocols/websocket_protocol.cc")
endif()

if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio_processing/audio_processor.cc")
endif()
if(CONFIG_USE_WAKE_WORD_DETECT)
    list(APPEND SOURCES "audio_processing/wake_word_detect.cc")
endif()

# 根据Kconfig选择语言目录
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
endif()

# 定义生成路径
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/${LANG_DIR}/*.p3)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.p3)

# 如果目标芯片是 ESP32，则排除特定文件
if(CONFIG_IDF_TARGET_ESP32)
    list(REMOVE_ITEM SOURCES "audio_codecs/box_audio_codec.cc"
                             "audio_codecs/es8388_audio_codec.cc"
                             "led/gpio_led.cc"
                             )
endif()

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    )

# 使用 target_compile_definitions 来定义 BOARD_TYPE, BOARD_NAME
# 如果 BOARD_NAME 为空，则使用 BOARD_TYPE
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    )

# 添加生成规则
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --input "${LANG_JSON}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)

# 强制建立生成依赖
add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER}
)
