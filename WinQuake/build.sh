#!/bin/bash

# This script builds q1 for raspberry pi
# invoke with ./build.sh
# or ./build.sh clean to clean before build

### Globals settings ###

SYSTEM_ROOT="--sysroot=/" #overridden if we are cross compiling

# cross compile suff, edit to suit your local build

CCPREFIX=/home/pi/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-
CCSYSROOT=/home/pi/raspberrypi/sysroot

# include directories

# directory to find khronos linux make files (with include/ containing headers! Make needs them.
# the = in I=/ is so we can make cross compiling easier as it lets up replace the start with a different
# system root https://gcc.gnu.org/onlinedocs/gcc/Directory-Options.html
INCLUDES="-I=/usr/include -I=/opt/vc/include -I=/opt/vc/include/interface/vcos/pthreads -I=/opt/vc/include/interface/vmcs_host/linux "

# directory containing the ARM shared libraries (rootfs, lib/ of SD card)
# specifically libEGL.so and libGLESv2.so
ARM_LIBS="-L=/opt/vc/lib"

# $# <- the number of parameters
# $@ <- all arguments

args=("$@") # <- sets args to the current arguments
while test $# -gt 0
do
	case "$1" in
		-cc)
			echo "Cross Compile"
			CROSS_COMPILE=$CCPREFIX
			SYSTEM_ROOT="--sysroot="$CCSYSROOT
			;;
		clean)
			#echo "Clean Build"
			#rm -rf ./releasearm/*
			#rm -rf ./debugarm/*
			;;

		-*)
			echo "bad option $1"
			;;
		#*)
		#	echo "argument $1"
		#	;;
	esac
	shift
done
# since we shifted the args away we need to reset them
set -- $args

echo "- - - - - - - - - - - - - - - "
echo "Start Make"
echo "- - - - - - - - - - - - - - - "

make -j4 -f make.rpi $1 ARCH=arm \
	CC=""$CROSS_COMPILE"gcc" USE_SVN=0 USE_CURL=0 USE_OPENAL=0 \
	CFLAGS="$SYSTEM_ROOT $INCLUDES" \
	LDFLAGS="-lm -pthread $ARM_LIBS -lvchostif -lbcm_host -lkhrn_static -lvchiq_arm -lopenmaxil -lbrcmEGL -lbrcmGLESv2 -lvcos -lrt -lSDL -ludev"
	
exit 0

