cmake_minimum_required(VERSION 3.16)

# Toolchain must be set before project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/at32/Artery/Platform/AT32F43x/arm-none-eabi.cmake)

project(ExhaustNote_MCU C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

# Target MCU
set(MCU_TARGET "AT32F437VMT7")
add_definitions(-DAT32F437VMT7 -DUSE_STDPERIPH_DRIVER -DEXHAUST_MCU=1 -DAT_START_F437_V1)

# Paths
set(ARTERY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/at32/Artery)
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/at32)
set(BSP_DIR ${PLATFORM_DIR}/bsp)
set(FATFS_DIR ${PLATFORM_DIR}/fatfs)
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../core)
set(CJSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cjson)

# X-TRACK base library paths
set(FWLIBS ${ARTERY_DIR}/Platform/AT32F43x/FWLibs)
set(AT32_CORE ${ARTERY_DIR}/Platform/AT32F43x/Core)
set(ARDUINO_API ${ARTERY_DIR}/ArduinoAPI)

# Startup and linker
set(STARTUP_FILE ${FWLIBS}/cmsis/cm4/device_support/startup/gcc/startup_at32f435_437.s)
set(LINKER_SCRIPT ${FWLIBS}/cmsis/cm4/device_support/startup/gcc/linker/AT32F437xM_FLASH.ld)

# Compiler flags
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS "${MCU_FLAGS} -ffunction-sections -fdata-sections -O3 -ffast-math -fsingle-precision-constant")
set(CMAKE_CXX_FLAGS "${MCU_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -O3 -ffast-math -fsingle-precision-constant")
set(CMAKE_ASM_FLAGS "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${MCU_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs -lm")

# ============================================================
# AT32 HAL Library
# ============================================================
file(GLOB AT32_DRIVER_SRCS ${FWLIBS}/drivers/src/*.c)
add_library(at32_hal STATIC
    ${FWLIBS}/cmsis/cm4/device_support/system_at32f435_437.c
    ${AT32_DRIVER_SRCS}
)
target_include_directories(at32_hal PUBLIC
    ${FWLIBS}/cmsis/cm4/core_support
    ${FWLIBS}/cmsis/cm4/device_support
    ${FWLIBS}/drivers/inc
    ${PLATFORM_DIR}  # for at32f435_437_conf.h
)

# ============================================================
# BSP (Board Support Package)
# ============================================================
add_library(at32_bsp STATIC
    # NOTE: audio.c excluded (has player-specific IRQ handler)
    # We implement our own I2S DMA handler in main.cpp
    ${BSP_DIR}/at_surf_f437_board_sdram.c
    ${BSP_DIR}/at_surf_f437_board_pca9555.c
    ${BSP_DIR}/at_surf_f437_board_key.c
    ${BSP_DIR}/at_surf_f437_board_joystick.c
    ${BSP_DIR}/at_surf_f437_board_variable_resistor.c
    ${BSP_DIR}/at_surf_f437_board_sd_card.c
    ${BSP_DIR}/i2c_application.c
)
target_include_directories(at32_bsp PUBLIC
    ${BSP_DIR}
    ${FATFS_DIR}/fatfs_lib/source
    ${FATFS_DIR}
)
target_link_libraries(at32_bsp PUBLIC at32_hal)

# ============================================================
# FatFS + SDIO
# ============================================================
add_library(fatfs STATIC
    ${FATFS_DIR}/fatfs_lib/source/ff.c
    ${FATFS_DIR}/fatfs_lib/source/ffsystem.c
    ${FATFS_DIR}/fatfs_lib/source/ffunicode.c
    # NOTE: at32_sdio.c REMOVED — using at_surf_f437_board_sd_card.c from BSP instead
    # (at32_sdio.c has duplicate sd_init/sd_power_on without V2.2.4 fixes)
    ${BSP_DIR}/at_surf_f437_board_diskio.c
)
target_include_directories(fatfs PUBLIC
    ${FATFS_DIR}/fatfs_lib/source
    ${FATFS_DIR}
    ${BSP_DIR}
)
target_link_libraries(fatfs PUBLIC at32_hal at32_bsp)

# ============================================================
# ExhaustNote Core Library (platform-independent)
# ============================================================
add_library(exhaust_core_mcu STATIC
    ${CORE_DIR}/src/crossfade.cpp
    ${CORE_DIR}/src/sample_player.cpp
    ${CORE_DIR}/src/dsp.cpp
    ${CORE_DIR}/src/mixer.cpp
    ${CORE_DIR}/src/transmission.cpp
    ${CORE_DIR}/src/engine_voice.cpp
    ${CORE_DIR}/src/engine_effects.cpp
    ${CORE_DIR}/src/car_config.cpp
    ${CJSON_DIR}/cJSON.c
)
target_include_directories(exhaust_core_mcu PUBLIC
    ${CORE_DIR}/include
    ${CJSON_DIR}
)
target_compile_definitions(exhaust_core_mcu PRIVATE EXHAUST_MCU=1)

# ============================================================
# Arduino Core (Serial, Print, Stream, GPIO, etc.)
# ============================================================
file(GLOB ARDUINO_CORE_SRCS ${AT32_CORE}/*.c ${AT32_CORE}/*.cpp)
file(GLOB ARDUINO_API_SRCS ${ARDUINO_API}/*.c ${ARDUINO_API}/*.cpp)
add_library(arduino_core STATIC
    ${ARDUINO_CORE_SRCS}
    ${ARDUINO_API_SRCS}
)
target_include_directories(arduino_core PUBLIC
    ${AT32_CORE}
    ${ARDUINO_API}
    ${ARTERY_DIR}/Platform/AT32F43x/Config
)
target_link_libraries(arduino_core PUBLIC at32_hal)

# ============================================================
# MCU Application
# ============================================================
set(BUTTON_EVENT_DIR ${ARTERY_DIR}/Libraries/ButtonEvent)

add_executable(exhaust_mcu
    main.cpp
    audio_i2s.c
    sd_loader.c
    car_loader.cpp
    system_call.c
    ${BUTTON_EVENT_DIR}/ButtonEvent.cpp
    ${ARTERY_DIR}/Platform/AT32F43x/Core/cxx_stubs.cpp
    ${STARTUP_FILE}
)
target_include_directories(exhaust_mcu PRIVATE
    ${PLATFORM_DIR}
    ${BUTTON_EVENT_DIR}
)
target_link_libraries(exhaust_mcu PRIVATE
    exhaust_core_mcu
    at32_bsp
    fatfs
    arduino_core
    at32_hal
)

# Post-build: generate .bin and .hex, print size
add_custom_command(TARGET exhaust_mcu POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:exhaust_mcu> exhaust_mcu.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:exhaust_mcu> exhaust_mcu.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:exhaust_mcu>
    COMMENT "Generating .bin and .hex, printing size"
)
