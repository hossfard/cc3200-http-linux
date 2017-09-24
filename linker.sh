#!/bin/bash

#
# Hack to get cmake link the object files and create .axf/.bin files
#
# Usage: linker.sh $1 $2 $3
#    $1: path to linker script
#    $2: path to cmake build directory
#    $3: Name of outputfile. Extensions '.axf'/'.bin' will be added
#

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 buildScript.ld buildDirectory output"
  exit 1
fi

# linker script
linkerScript=$1

# path to build directory
buildDir=$2

# Name output file
outAxfFile="$3.axf"
outBinFile="$3.bin"

COMPILER="arm-none-eabi-gcc"
LINKER="arm-none-eabi-ld"
OBJCOPY="arm-none-eabi-objcopy"

# Get list of object files from build directory
obj="$(find $2 -type f -name *.obj)"

# Get location of libgcc, libgc, libm from GCC front-end
# (see $CC3200SDK/tools/gcc_scripts/makedefs)
# Flags must be set, otherwise a wrong library may be selected
CPU="-mcpu=cortex-m4"
FLAGS="-mthumb $CPU -ffunction-sections -fdata-sections -MD -std=c99 -g -c"

libgcc=$($COMPILER $FLAGS -print-libgcc-file-name)
libm=$($COMPILER $FLAGS -print-file-name=libm.a)
libc=$($COMPILER $FLAGS -print-file-name=libc.a)

# driver and simplelink libraries
libdriver=$CC3200SDK/driverlib/gcc/exe/libdriver.a
libsl=$CC3200SDK/simplelink/gcc/exe/libsimplelink.a
libfreertos=$CC3200SDK/oslib/gcc/exe/FreeRTOS.a

# Linker flags
FLAGS="--entry ResetISR --gc-sections"

# Link object files
GREEN="\033[0;32m"
NC="\033[0m"
BOLD="\e[1m"
RESET="\e[0m"
echo -e ">${GREEN}${BOLD} Calling $LINKER ${NC} ${RESET}"
# Link the object files
$LINKER -T $linkerScript $FLAGS -o $outAxfFile $obj $libsl $libdriver $libfreertos $libm $libc $libgcc
# Create a binary file from linked file
$OBJCOPY -O binary $outAxfFile $outBinFile
