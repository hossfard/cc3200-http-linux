message(">>>>> Loading toolchain.cmake <<<<<")

INCLUDE(CMakeForceCompiler)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
#SET(CMAKE_C_COMPILER arm-none-eabi-gcc)
#SET(CMAKE_CXX_COMPILER arm-none-eabi-g++)

find_program(CROSS_GCC_FULL_PATH "arm-none-eabi-gcc")
get_filename_component(CROSS_COMPIL_TOOLS ${CROSS_GCC_FULL_PATH} PATH)
if (NOT EXISTS ${CROSS_COMPIL_TOOLS})
	message (FATAL_ERROR "Can't find cross compilation tool chain")
endif()

SET(CPREF ${CROSS_COMPIL_TOOLS}/arm-none-eabi)
SET(CMAKE_C_COMPILER ${CPREF}-gcc CACHE STRING "arm-none-eabi-gcc" FORCE)
SET(CMAKE_CXX_COMPILER ${CPREF}-g++ CACHE STRING "arm-none-eabi-g++" FORCE)
SET(AS "${CROSS_COMPIL_TOOLS}/arm-none-eabi-gcc -x -assembler-with-cpp" CACHE STRING "arm-none-eabi-as")
SET(CMAKE_AR ${CPREF}-ar CACHE STRING "arm-none-eabi-ar" FORCE)
SET(CMAKE_LD ${CPREF}-ld CACHE STRING "arm-none-eabi-ld" FORCE)
SET(NM	     ${CPREF}-nm CACHE STRING "arm-none-eabi-nm" FORCE)
SET(OBJCOPY  ${CPREF}-objcopy CACHE STRING "arm-none-eabi-objcopy" FORCE)
SET(OBJDUMP  ${CPREF}-objdump CACHE STRING "arm-none-eabi-objdump" FORCE)
SET(READELF  ${CPREF}-readelf CACHE STRING "arm-none-eabi-readelf" FORCE)
SET(CMAKE_RANLIB ${CPREF}-ranlib CACHE STRING "arm-none-eabi-ranlib" FORCE)
SET(CMAKE_LD ${CPREF}-ld CACHE STRING "arm-none-eabi-ld" FORCE)

CMAKE_FORCE_C_COMPILER(${CPREF}-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(${CPREF}-g++ GNU)
find_program(LINKER arm-none-eabi-ld)

#SET(COMMON_FLAGS "-mthumb -mcpu=cortex-m4 -ffunction-sections -fdata-sections -MD -std=c99 -g -O0 -c")
SET(CMAKE_CXX_FLAGS "${COMMON_FLAGS}  -std=gnu++0x")
SET(CMAKE_C_FLAGS "-mthumb -mcpu=cortex-m4 -ffunction-sections -fdata-sections -MD -std=c99 -g -O0 -c -Dgcc -DUSER_INPUT_ENABLE")
#set(CMAKE_EXE_LINKER_FLAGS "-Wl,-gc-sections ")

#SET(CMAKE_C_LINK_EXECUTABLE arm-none-eabi-ld)
SET(LINKER_SCRIPT "linker.ld")
