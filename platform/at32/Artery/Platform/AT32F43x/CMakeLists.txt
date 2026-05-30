cmake_minimum_required(VERSION 3.13)

# Generate compile_commands.json for IDE/tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Toolchain must be set before project()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/arm-none-eabi.cmake"
      CACHE PATH "Toolchain file")
endif()

project(X-TRACK-EVO-AT32 C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# ==============================================================================
# Directory aliases
# ==============================================================================
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VENDOR_DIR ${PLATFORM_DIR}/../..)
set(PROJECT_ROOT ${VENDOR_DIR}/../..)
set(APP_DIR ${PROJECT_ROOT}/App)
set(EXTERNAL_DIR ${PROJECT_ROOT}/External)
set(FRAMEWORKS_DIR ${PROJECT_ROOT}/Frameworks)
set(FWLIBS_DIR ${PLATFORM_DIR}/FWLibs)
set(LIBRARIES_DIR ${VENDOR_DIR}/Libraries)
set(APPLICATION_DIR ${VENDOR_DIR}/Application)
set(ARDUINO_API_DIR ${VENDOR_DIR}/ArduinoAPI)
set(CORE_DIR ${PLATFORM_DIR}/Core)

# ==============================================================================
# Kconfig integration
# ==============================================================================
include(${PROJECT_ROOT}/CMake/kconfig.cmake)
load_kconfig(${APP_DIR}/Config/autoconf.h)

# ==============================================================================
# Build type configuration
# ==============================================================================
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type (Debug/Release)" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Debug configuration
set(CMAKE_C_FLAGS_DEBUG
    "-O0 -g3 -DDEBUG"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG
    "-O0 -g3 -DDEBUG"
    CACHE STRING "" FORCE)

# Release configuration (include -g for DWARF debug info, does not affect
# .bin/.hex size)
set(CMAKE_C_FLAGS_RELEASE
    "-O3 -g -DNDEBUG"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -g -DNDEBUG"
    CACHE STRING "" FORCE)

# Debug-specific definitions
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(CONFIG_WATCHDOG_TIMEOUT=0 LV_USE_MEM_MONITOR=1
                          LV_USE_PERF_MONITOR=1)
endif()

# ==============================================================================
# Compile definitions (matching PIO build)
# ==============================================================================
add_compile_definitions(
  # MCU
  AT32F437VMT7
  USE_STDPERIPH_DRIVER
  __STDC_FORMAT_MACROS
  # Arduino compat
  ARDUINO=111
  # LVGL
  LV_USE_DEV_VERSION=1
  LV_USE_LOG=1
  LV_USE_FS_FATFS=1
  # FreeType
  FT2_BUILD_LIBRARY
  FT_CONFIG_OPTIONS_H="lvgl/src/libs/freetype/ftoption.h"
  FT_CONFIG_MODULES_H="lvgl/src/libs/freetype/ftmodule.h")

# ==============================================================================
# Compile flags
# ==============================================================================
# Common flags for all code (C and C++)
add_compile_options(
  -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-exceptions
  -fno-unwind-tables -fno-asynchronous-unwind-tables)

# C++ specific flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

# ==============================================================================
# Linker script
# ==============================================================================
set(LINKER_SCRIPT
    ${FWLIBS_DIR}/cmsis/cm4/device_support/startup/gcc/linker/AT32F437xM_FLASH.ld
)
add_link_options(
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
  # Wrap __cxa_demangle to use our stub (saves ~18KB)
  -Wl,--wrap=__cxa_demangle)

# ==============================================================================
# Include directories
# ==============================================================================
include_directories(
  ${PLATFORM_DIR}/PlatformIO/include
  # at32f435_437_conf.h, RTE_Components.h
  ${PLATFORM_DIR}/Config
  # MCU config
  ${FWLIBS_DIR}/cmsis/cm4/core_support
  # FWLibs
  ${FWLIBS_DIR}/cmsis/cm4/device_support
  ${FWLIBS_DIR}/drivers/inc
  ${CORE_DIR}
  # Core
  ${ARDUINO_API_DIR}
  # ArduinoAPI
  ${APPLICATION_DIR}
  # Application ports
  ${APPLICATION_DIR}/HAL
  ${APPLICATION_DIR}/btstack_port
  ${APPLICATION_DIR}/flashdb_port
  ${APPLICATION_DIR}/usb_msc_port
  ${APPLICATION_DIR}/lv_port
  ${APPLICATION_DIR}/diskio_port
  ${PROJECT_ROOT}
  # for "Frameworks/..." includes
  ${APP_DIR}
  # App
  ${LIBRARIES_DIR}
  # for "BMP280/..." style includes
  ${EXTERNAL_DIR}/graphics
  # External
  ${EXTERNAL_DIR}/graphics/lvgl/src/libs/freetype
  ${EXTERNAL_DIR}/freetype2/include
  ${EXTERNAL_DIR}/wasm3/source
  ${EXTERNAL_DIR}/xWrapUp/inc
  ${EXTERNAL_DIR}/btstack/src
  ${EXTERNAL_DIR}/btstack/src/ble
  ${EXTERNAL_DIR}/btstack/src/ble/gatt-service
  ${EXTERNAL_DIR}/btstack/chipset/zephyr
  ${EXTERNAL_DIR}/btstack/platform/embedded
  ${EXTERNAL_DIR}/FlashDB/inc
  ${EXTERNAL_DIR}/FlashDB/port/fal/inc
  ${EXTERNAL_DIR}/Fusion/Fusion
  ${EXTERNAL_DIR}/cJSON
  ${FRAMEWORKS_DIR}
  # Frameworks
  ${FRAMEWORKS_DIR}/wApp/wasm3_port
  ${LIBRARIES_DIR}/Adafruit_GFX_Library
  # Libraries
  ${LIBRARIES_DIR}/Adafruit_GFX_Library/Fonts
  ${LIBRARIES_DIR}/Adafruit_ST7789
  ${LIBRARIES_DIR}/at32_sdio
  ${LIBRARIES_DIR}/BMP280
  ${LIBRARIES_DIR}/cm_backtrace
  ${LIBRARIES_DIR}/cm_backtrace/fault_handler/gcc
  ${LIBRARIES_DIR}/DRV2605L
  ${LIBRARIES_DIR}/fatfs
  ${LIBRARIES_DIR}/FT6336G
  ${LIBRARIES_DIR}/HWire
  ${LIBRARIES_DIR}/lsm6dsr
  ${LIBRARIES_DIR}/MAX17048
  ${LIBRARIES_DIR}/MemoryStress
  ${LIBRARIES_DIR}/MillisTaskManager
  ${LIBRARIES_DIR}/QMC5883LCompass
  ${LIBRARIES_DIR}/Shell
  ${LIBRARIES_DIR}/StackInfo
  ${LIBRARIES_DIR}/TinyGPSPlus/src
  ${LIBRARIES_DIR}/TPL0401
  ${LIBRARIES_DIR}/usb
  ${LIBRARIES_DIR}/usb/usb_drivers/inc
  ${LIBRARIES_DIR}/usb/usbd_class/cdc
  ${LIBRARIES_DIR}/usb/usbd_class/msc)

# ==============================================================================
# Sources: AT32 FWLibs (startup + system + peripheral drivers)
# ==============================================================================
set(FWLIBS_SOURCES
    ${FWLIBS_DIR}/cmsis/cm4/device_support/startup/gcc/startup_at32f435_437.s
    ${FWLIBS_DIR}/cmsis/cm4/device_support/system_at32f435_437.c)
file(GLOB FWLIBS_DRIVER_SOURCES ${FWLIBS_DIR}/drivers/src/*.c)
list(APPEND FWLIBS_SOURCES ${FWLIBS_DRIVER_SOURCES})

# ==============================================================================
# Sources: Core (MCU platform drivers)
# ==============================================================================
file(GLOB CORE_SOURCES ${CORE_DIR}/*.c ${CORE_DIR}/*.cpp)

# ==============================================================================
# Sources: ArduinoAPI
# ==============================================================================
file(GLOB ARDUINO_SOURCES ${ARDUINO_API_DIR}/*.c ${ARDUINO_API_DIR}/*.cpp)

# ==============================================================================
# Sources: Application (HAL + ports)
# ==============================================================================
file(GLOB APPLICATION_HAL_SOURCES ${APPLICATION_DIR}/HAL/*.cpp
     ${APPLICATION_DIR}/HAL/*.c)
file(
  GLOB
  APPLICATION_PORT_SOURCES
  ${APPLICATION_DIR}/btstack_port/*.c
  ${APPLICATION_DIR}/diskio_port/*.c
  ${APPLICATION_DIR}/flashdb_port/*.c
  ${APPLICATION_DIR}/flashdb_port/*.cpp
  ${APPLICATION_DIR}/lv_port/*.c
  ${APPLICATION_DIR}/lv_port/*.cpp
  ${APPLICATION_DIR}/usb_msc_port/*.c
  ${APPLICATION_DIR}/usb_msc_port/*.cpp)

# ==============================================================================
# Sources: App (recursive, with exclusions)
# ==============================================================================
file(GLOB_RECURSE APP_SOURCES ${APP_DIR}/*.c ${APP_DIR}/*.cpp)
# Exclude WString/Stream (conflicts with ArduinoAPI), wApp, and third-party
# utils
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/Utils/WString/.*")
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/Utils/Stream/.*")
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/wApps/.*")
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/Utils/lv_lodepng/.*")
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/Utils/lv_tjpgd/.*")
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/Test/.*")

# Third-party utils in App directory (suppress warnings)
file(GLOB APP_THIRDPARTY_SOURCES ${APP_DIR}/Utils/lv_lodepng/*.c
     ${APP_DIR}/Utils/lv_tjpgd/*.c)

# ==============================================================================
# Sources: Frameworks
# ==============================================================================
file(
  GLOB
  FRAMEWORKS_SOURCES
  ${FRAMEWORKS_DIR}/DataBroker/*.cpp
  ${FRAMEWORKS_DIR}/DeviceManager/*.cpp
  ${FRAMEWORKS_DIR}/FontManager/*.c
  ${FRAMEWORKS_DIR}/PageManager/*.cpp
  ${FRAMEWORKS_DIR}/ResourceManager/*.cpp)

# ==============================================================================
# Sources: External - btstack (selective, conditional)
# ==============================================================================
if(CONFIG_EXT_BTSTACK_ENABLE)
  set(BTSTACK_SOURCES
      ${EXTERNAL_DIR}/btstack/src/ad_parser.c
      ${EXTERNAL_DIR}/btstack/src/btstack_crypto.c
      ${EXTERNAL_DIR}/btstack/src/btstack_linked_list.c
      ${EXTERNAL_DIR}/btstack/src/btstack_memory.c
      ${EXTERNAL_DIR}/btstack/src/btstack_memory_pool.c
      ${EXTERNAL_DIR}/btstack/src/btstack_run_loop.c
      ${EXTERNAL_DIR}/btstack/src/btstack_tlv.c
      ${EXTERNAL_DIR}/btstack/src/btstack_util.c
      ${EXTERNAL_DIR}/btstack/src/hci.c
      ${EXTERNAL_DIR}/btstack/src/hci_cmd.c
      ${EXTERNAL_DIR}/btstack/src/hci_dump.c
      ${EXTERNAL_DIR}/btstack/src/hci_transport_h4.c
      ${EXTERNAL_DIR}/btstack/src/l2cap.c
      ${EXTERNAL_DIR}/btstack/src/l2cap_signaling.c
      ${EXTERNAL_DIR}/btstack/src/ble/att_db.c
      ${EXTERNAL_DIR}/btstack/src/ble/att_dispatch.c
      ${EXTERNAL_DIR}/btstack/src/ble/att_server.c
      ${EXTERNAL_DIR}/btstack/src/ble/gatt_client.c
      ${EXTERNAL_DIR}/btstack/src/ble/sm.c
      ${EXTERNAL_DIR}/btstack/src/ble/le_device_db_memory.c
      ${EXTERNAL_DIR}/btstack/src/ble/gatt-service/battery_service_server.c
      ${EXTERNAL_DIR}/btstack/src/ble/gatt-service/device_information_service_server.c
      ${EXTERNAL_DIR}/btstack/chipset/zephyr/btstack_chipset_zephyr.c
      ${EXTERNAL_DIR}/btstack/platform/embedded/btstack_uart_block_embedded.c
      ${EXTERNAL_DIR}/btstack/platform/embedded/btstack_run_loop_embedded.c)
else()
  set(BTSTACK_SOURCES "")
  message(STATUS "BTstack disabled by Kconfig")
endif()

# ==============================================================================
# Sources: External - freetype (selective, conditional)
# ==============================================================================
if(CONFIG_EXT_FREETYPE_ENABLE)
  set(FREETYPE_SOURCES
      ${EXTERNAL_DIR}/freetype2/src/base/ftbase.c
      ${EXTERNAL_DIR}/freetype2/src/base/ftbitmap.c
      ${EXTERNAL_DIR}/freetype2/src/base/ftdebug.c
      ${EXTERNAL_DIR}/freetype2/src/base/ftglyph.c
      ${EXTERNAL_DIR}/freetype2/src/base/ftinit.c
      ${EXTERNAL_DIR}/freetype2/src/base/ftmm.c
      ${EXTERNAL_DIR}/freetype2/src/cache/ftcache.c
      ${EXTERNAL_DIR}/freetype2/src/gzip/ftgzip.c
      ${EXTERNAL_DIR}/freetype2/src/sfnt/sfnt.c
      ${EXTERNAL_DIR}/freetype2/src/smooth/smooth.c
      ${EXTERNAL_DIR}/freetype2/src/truetype/truetype.c)
else()
  set(FREETYPE_SOURCES "")
  message(STATUS "FreeType disabled by Kconfig")
endif()

# ==============================================================================
# Sources: External - others
# ==============================================================================
file(GLOB CJSON_SOURCES ${EXTERNAL_DIR}/cJSON/cJSON.c
     ${EXTERNAL_DIR}/cJSON/cJSON_Utils.c)
file(GLOB FLASHDB_SOURCES ${EXTERNAL_DIR}/FlashDB/src/*.c
     ${EXTERNAL_DIR}/FlashDB/port/fal/src/*.c)
file(GLOB FUSION_SOURCES ${EXTERNAL_DIR}/Fusion/Fusion/*.c)

# lvgl - only src core, exclude platform-specific backends
file(GLOB_RECURSE LVGL_SOURCES ${EXTERNAL_DIR}/graphics/lvgl/src/*.c)
# Exclude platform-specific drivers not needed for embedded
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/dev/disp/.*") # Linux DRM/fbdev
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/dev/sdl/.*") # SDL
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/sdl/.*") # SDL draw
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/nxp/.*") # NXP GPU
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/gd32_ipa/.*") # GD32 IPA
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/arm2d/.*") # ARM-2D
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/stm32_dma2d/.*") # STM32 DMA2D
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/draw/swm341_dma2d/.*") # SWM341 DMA2D

# wasm3 (conditional)
if(CONFIG_EXT_WASM3_ENABLE)
  file(GLOB WASM3_SOURCES ${EXTERNAL_DIR}/wasm3/source/*.c)
  list(FILTER WASM3_SOURCES EXCLUDE REGEX ".*m3_api_libc\\.c$")
  list(FILTER WASM3_SOURCES EXCLUDE REGEX ".*/extensions/.*")
else()
  set(WASM3_SOURCES "")
  message(STATUS "WASM3 disabled by Kconfig")
endif()

# ==============================================================================
# Sources: Libraries (vendor-specific hardware drivers)
# ==============================================================================
set(LIB_SOURCES
    # Adafruit_GFX_Library
    ${LIBRARIES_DIR}/Adafruit_GFX_Library/Adafruit_GFX.cpp
    ${LIBRARIES_DIR}/Adafruit_GFX_Library/Adafruit_SPITFT.cpp
    ${LIBRARIES_DIR}/Adafruit_GFX_Library/glcdfont.c
    # Adafruit_ST7789
    ${LIBRARIES_DIR}/Adafruit_ST7789/Adafruit_ST7789_XMC.cpp
    # at32_sdio
    ${LIBRARIES_DIR}/at32_sdio/at32_sdio.c
    # BMP280
    ${LIBRARIES_DIR}/BMP280/BMP280.cpp
    # cm_backtrace (cmb_fault.S excluded - HAL_FaultHandle.cpp provides
    # HardFault_Handler)
    ${LIBRARIES_DIR}/cm_backtrace/cm_backtrace.c
    # DRV2605L
    ${LIBRARIES_DIR}/DRV2605L/DRV2605L.cpp
    # fatfs
    ${LIBRARIES_DIR}/fatfs/diskio.c
    ${LIBRARIES_DIR}/fatfs/ff.c
    ${LIBRARIES_DIR}/fatfs/ffsystem.c
    ${LIBRARIES_DIR}/fatfs/ffunicode.c
    # FT6336G
    ${LIBRARIES_DIR}/FT6336G/FT6336G.cpp
    # HWire
    ${LIBRARIES_DIR}/HWire/HWire.cpp
    ${LIBRARIES_DIR}/HWire/i2c_application.c
    # lsm6dsr
    ${LIBRARIES_DIR}/lsm6dsr/lsm6dsr.c
    ${LIBRARIES_DIR}/lsm6dsr/lsm6dsr_port.cpp
    # MAX17048
    ${LIBRARIES_DIR}/MAX17048/MAX17048.cpp
    # MemoryStress
    ${LIBRARIES_DIR}/MemoryStress/MemoryStress.c
    # MillisTaskManager
    ${LIBRARIES_DIR}/MillisTaskManager/MillisTaskManager.cpp
    # QMC5883LCompass
    ${LIBRARIES_DIR}/QMC5883LCompass/QMC5883LCompass.cpp
    # Shell
    ${LIBRARIES_DIR}/Shell/Shell.c
    # StackInfo
    ${LIBRARIES_DIR}/StackInfo/StackInfo.c
    # TinyGPSPlus
    ${LIBRARIES_DIR}/TinyGPSPlus/src/TinyGPS++.cpp
    # TPL0401
    ${LIBRARIES_DIR}/TPL0401/TPL0401.cpp
    # USB
    ${LIBRARIES_DIR}/usb/usb_drivers/src/usb_core.c
    ${LIBRARIES_DIR}/usb/usb_drivers/src/usbd_core.c
    ${LIBRARIES_DIR}/usb/usb_drivers/src/usbd_int.c
    ${LIBRARIES_DIR}/usb/usb_drivers/src/usbd_sdr.c
    ${LIBRARIES_DIR}/usb/usbd_class/cdc/cdc_class.c
    ${LIBRARIES_DIR}/usb/usbd_class/cdc/cdc_desc.c
    ${LIBRARIES_DIR}/usb/usbd_class/msc/msc_bot_scsi.c
    ${LIBRARIES_DIR}/usb/usbd_class/msc/msc_class.c
    ${LIBRARIES_DIR}/usb/usbd_class/msc/msc_desc.c)

# ==============================================================================
# Sources: FPBInject (runtime code injection)
# ==============================================================================
option(FPBINJECT_ENABLE "Enable FPBInject runtime code injection" ON)
if(FPBINJECT_ENABLE)
  include(${PROJECT_ROOT}/CMake/fpbinject.cmake)
  include_directories(${FPBINJECT_INCLUDES})
  add_compile_definitions(${FPBINJECT_DEFINITIONS})
endif()

# ==============================================================================
# Sources: PlatformIO src (main + system_call)
# ==============================================================================
set(PIO_SOURCES ${PLATFORM_DIR}/PlatformIO/src/main.cpp
                ${PLATFORM_DIR}/PlatformIO/src/system_call.c)

# ==============================================================================
# Executable - Link order optimized for AT32F437 zero-wait-state region LVGL
# rendering core placed early to fit in first 256KB
# ==============================================================================
add_executable(
  ${PROJECT_NAME}
  # 1. Essential startup and drivers (must be first)
  ${FWLIBS_SOURCES}
  ${CORE_SOURCES}
  ${ARDUINO_SOURCES}
  # 1. LVGL core - high priority for zero-wait-state region
  ${LVGL_SOURCES}
  # 1. Application code
  ${APPLICATION_HAL_SOURCES}
  ${APPLICATION_PORT_SOURCES}
  ${APP_SOURCES}
  ${APP_THIRDPARTY_SOURCES}
  ${FRAMEWORKS_SOURCES}
  # 1. Libraries
  ${LIB_SOURCES}
  # 1. External libraries (lower priority, can be in slow region)
  ${CJSON_SOURCES}
  ${FLASHDB_SOURCES}
  ${FUSION_SOURCES}
  ${BTSTACK_SOURCES}
  ${FREETYPE_SOURCES}
  ${WASM3_SOURCES}
  # 1. FPBInject and main
  ${FPBINJECT_SOURCES}
  ${PIO_SOURCES})

# ==============================================================================
# Apply warning flags per source type
# ==============================================================================
# Project code: strict warnings (append to existing flags)
set(PROJECT_WARNING_FLAGS
    "-Wall;-Wextra;-Werror;-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-unused-function;-Wno-cast-function-type"
)
set_source_files_properties(
  ${CORE_SOURCES} ${APPLICATION_HAL_SOURCES} ${APPLICATION_PORT_SOURCES}
  ${APP_SOURCES} ${FRAMEWORKS_SOURCES} ${PIO_SOURCES}
  PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNING_FLAGS}")

# Third-party libraries: suppress warnings
set_source_files_properties(
  ${FWLIBS_SOURCES}
  ${ARDUINO_SOURCES}
  ${APP_THIRDPARTY_SOURCES}
  ${BTSTACK_SOURCES}
  ${FREETYPE_SOURCES}
  ${CJSON_SOURCES}
  ${FLASHDB_SOURCES}
  ${FUSION_SOURCES}
  ${LVGL_SOURCES}
  ${WASM3_SOURCES}
  ${LIB_SOURCES}
  ${FPBINJECT_SOURCES}
  PROPERTIES COMPILE_OPTIONS "-w")

target_link_libraries(${PROJECT_NAME} PRIVATE m c)

# ==============================================================================
# Post-build: generate .bin and .hex, print size
# ==============================================================================
set(ELF_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf)
set_target_properties(
  ${PROJECT_NAME} PROPERTIES SUFFIX ".elf" RUNTIME_OUTPUT_DIRECTORY
                                           ${CMAKE_BINARY_DIR})

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}>
          ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}>
          ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "Generating .bin and .hex")
