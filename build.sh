#!/bin/bash 
#
# Kernel Build script for GT-I9003 Custom Titanium Kernel 
#
# Written by Aditya Patange aka Adi_Pat adithemagnificent@gmail.com 
#
# release defconfig (latona_galaxysl_defconfig) - Stripped debug stuff for faster Kernel
# debug defconfig (debug_defconfig) - Has debug Stuff enabled for testing purposes
#
# TO BUILD THE KERNEL- 
#
# Edit TOOLCHAIN path and RAMDISK path accordingly. 
#
# Make sure you have mkbootimg , mkbootfs binaries in $ROOT directory.
#
# ./build.sh release/debug .version-number (default is 0)
#
# EXAMPLE: ./build.sh release 10
# 
# 

## Misc Stuff ##

red='tput setaf 1'
green='tput setaf 2'
yellow='tput setaf 3'
blue='tput setaf 4'
violet='tput setaf 5'
cyan='tput setaf 6'
white='tput setaf 7'
normal='tput sgr0'
bold='setterm -bold'

$normal
### ### ### ### 


# SET SOME PATH VARIABLES
# Modify these as per requirements
ROOT="/home/dhiru1602/AndroidProject/CM10/out/host/linux-x86/bin/"
# Toolchain path
TOOLCHAIN="/home/dhiru1602/AndroidProject/CM10/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi"
KERNEL_DIR="/home/dhiru1602/AndroidProject/CM10/kernel/samsung/latona_3"
RAMDISK_DIR="$KERNEL_DIR/usr/ramdisk"
MODULES_DIR="$RAMDISK_DIR/lib/modules"
OUT="/home/dhiru1602/i9003/out"
# Device Specific flags

# Defconfigs
RELEASE_DEFCONFIG=latona_defconfig
DEBUG_DEFCONFIG=debug_defconfig
DEFCONFIG=latona_defconfig # Default
# Kernel addresses 
BASE="81800000"
PAGESIZE="4096"
# Kernel format
KERNEL=normalboot.img
MKBOOTIMG=$ROOT/mkbootimg
KERNEL_BUILD="Test-`date '+%Y%m%d-%H%M'`" 
# Threads
THREADS=84
TARBALL=I9003_$KERNEL_BUILD.tar

# More Misc stuff
echo $2 > VERSION
VERSION='cat VERSION'
clear
clear
clear
clear

###################### DONE ##########################
$bold
$cyan
echo "***********************************************"
echo "|~~~~~~~~COMPILING TITANIUM KERNEL ~~~~~~~~~~~|"
echo "|---------------------------------------------|"
echo "|      GT-I9003 Samsung Galaxy SL             |"
echo "***********************************************"
echo "-----------------------------------------------"
echo "---------- Adi_Pat @ XDA-DEVELOPERS -----------"
echo "-----------------------------------------------"
echo "   Aditya Patange adithemagnificent@gmail.com  "
echo "-----------------------------------------------"
echo "***********************************************"
sleep 5
$normal

clear
clear
clear
clear
clear

$bold 
$yellow
echo "  START    PROJECT-LINUX KERNEL 3.0 FOR GALAXY SL GT-I9003" 
$normal
sleep 2
# Clean old built kernel in out folder 
if [ -d $OUT ]; then
echo "  CLEAN    $OUT" 
rm -rf $OUT
fi

mkdir $OUT
$white
# Import Defconfig
cd $KERNEL_DIR 
export ARCH=arm CROSS_COMPILE=$TOOLCHAIN-
make -j$THREADS clean mrproper
if [ $1 = "release" ]; then
echo echo "  IMPORT    $RELEASE_DEFCONFIG" 
make -j$THREADS $RELEASE_DEFCONFIG
elif [ $1 = "debug" ]; then 
echo echo "  IMPORT    $DEBUG_DEFCONFIG" 
make -j$THREADS $DEBUG_DEFCONFIG
else
echo echo "  IMPORT    $DEFCONFIG" 
make -j$THREADS $DEFCONFIG
fi

# Set Release Version 
if [ -n VERSION ]; then
echo "  VERSION    0" 
echo "0" > .version
else 
echo "  VERSION    $VERSION" 
echo $VERSION > .version
rm VERSION
fi

# Build Modules

echo "----------START----------"
echo "  COMPILE    modules"
make -j$THREADS modules
echo "  OBJCOPY    'find -name *.ko'"
find -name '*.ko' -exec cp -av {} $MODULES_DIR/ \;

# Strip unneeded symbols
cd $MODULES_DIR
echo echo "  ARMEABI-STRIP    'find -name *.ko'"

for m in $(find . | grep .ko | grep './')
do echo $m
$TOOLCHAIN-strip --strip-unneeded $m
done

# Build zImage
$white
echo echo "  COMPILE    arch/arm/boot/zImage"
cd $KERNEL_DIR 
make -j$THREADS zImage
if [ -a $KERNEL_DIR/arch/arm/boot/zImage ];
then
	cd $RAMDISK_DIR
	echo "  CPIO    usr/ramdisk.cpio"
	find . -print0 | cpio --null -ov --format=newc > ../ramdisk.cpio 2> /dev/null
	cd ..
	echo "  LZMA    usr/ramdisk.cpio.lzma"
	lzma -z -c ramdisk.cpio > ramdisk.cpio.lzma
	cd ..

	echo "  BOOTIMG $KERNEL using ramdisk (usr/ramdisk.cpio.lzma)"
	$MKBOOTIMG --kernel $KERNEL_DIR/arch/arm/boot/zImage --ramdisk usr/ramdisk.cpio.lzma --pagesize $PAGESIZE -o $KERNEL
	echo "  TAR     $TARBALL"
	tar -cvf $TARBALL $KERNEL
	cp $TARBALL $OUT/$TARBALL
	cp $KERNEL $OUT/$KERNEL
	rm $TARBALL
	rm $KERNEL
	rm usr/ramdisk.cpio
$normal
else
$red
echo "No compiled zImage at $KERNEL_DIR/arch/arm/boot/zImage"
echo "Compilation failed - Fix errors and recompile "
echo "Press enter to end script"
$normal
read ANS
fi 
