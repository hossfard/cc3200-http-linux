#!/bin/bash

#
# Hack to make cmake link the object files and create an .axf file
#
# Usage: buildAxf.sh $1 $2 $3
#    $1: path to linker script
#    $2: path to cmake build directory
#    $3: Name of outputfile. Extension '.axf' will be added
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
outFile="$3.axf"

COMPILER="arm-none-eabi-gcc"
LINKER="arm-none-eabi-ld"

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
libsl=$CC3200SDK/simplelink/gcc/exe/libsimplelink_nonos.a

# Linker flags
FLAGS="--entry ResetISR --gc-sections"

# Link object files
GREEN="\033[0;32m"
NC="\033[0m"
BOLD="\e[1m"
RESET="\e[0m"
echo -e ">${GREEN}${BOLD} Calling $LINKER ${NC} ${RESET}"
$LINKER -T $linkerScript $FLAGS -o $outFile $obj $libsl $libdriver $libm $libc $libgcc
