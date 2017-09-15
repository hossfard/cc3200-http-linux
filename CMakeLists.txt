cmake_minimum_required(VERSION 3.0)

#
# MUST have CC3200SDK as a variable pointing to path of CC3200 SDK
# directory
#

project(app C)

# Application name
set(app "http-server")

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MUST have CC3200SDK in path
if(NOT DEFINED ENV{CC3200SDK})
  message(FATAL_ERROR "$CC3200SDK must be set")
endif()

# Source files
list(
  APPEND app_sources
  main.c
  pinmux.c
  HttpRequest.c
  Router.c
  EventHandlers_impl.c
  jsmn/jsmn.c
  $ENV{CC3200SDK}/example/common/uart_if.c
  $ENV{CC3200SDK}/example/common/gpio_if.c
  $ENV{CC3200SDK}/example/common/udma_if.c
  $ENV{CC3200SDK}/example/common/network_common.c
  $ENV{CC3200SDK}/example/common/startup_gcc.c
)

# Include directories
include_directories(".")
include_directories("jsmn")
include_directories($ENV{CC3200SDK}/example/common)
include_directories($ENV{CC3200SDK}/simplelink)
include_directories($ENV{CC3200SDK}/simplelink/include)
include_directories($ENV{CC3200SDK}/simplelink/source)
include_directories($ENV{CC3200SDK}/simplelink_extlib/provisioninglib)
include_directories($ENV{CC3200SDK}/driverlib)
include_directories($ENV{CC3200SDK}/oslib)
include_directories($ENV{CC3200SDK}/inc)
include_directories($ENV{CC3200SDK}/third_party/FreeRTOS)
include_directories($ENV{CC3200SDK}/third_party/FreeRTOS/source)
include_directories($ENV{CC3200SDK}/third_party/FreeRTOS/source/portable/GCC/ARM_CM4)
include_directories($ENV{CC3200SDK}/third_party/FreeRTOS/source/include)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSL_PLATFORM_MULTI_THREADED -DUSE_FREERTOS")


add_executable(${app} ${app_sources})

# Hack to call the linker
add_custom_command(TARGET ${app} POST_BUILD COMMAND
  ${CMAKE_SOURCE_DIR}/buildAxf.sh ${CMAKE_SOURCE_DIR}/linker.ld ${CMAKE_BIN_DIR}. ${app})
