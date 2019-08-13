#!/bin/bash

#terminal colors
CTAG_S="\x1b[1;33m"
CTAG_S_R="\x1b[1;31m"
CTAG_E="\x1b[0m"



WORK_ROOT=`pwd`/..

sfile=.config
str_a="CONFIG_ARCH_TCC[589]"
str_b="=y"

ARCH=$(cat $sfile | grep "$str_a" | grep "$str_b")
ARCH=${ARCH#CONFIG_ARCH_}
ARCH=${ARCH%=y}

TCC_TARGET=$ARCH
case $TCC_TARGET in
	TCC898X|TCC899X|TCC802X|TCC803X|TCC901X) BASE_ADDR="0x20000000"
		;;
	TCC897X|TCC570X) BASE_ADDR="0x80000000"
		;;
	*)
		echo Unknown architecture!
		exit 1
		;;
esac
KERNEL_OFFSET=0x8000

if [ -z `grep CONFIG_SERIAL_AMBA_PL011=y .config` ]; then
	CONSOLE_NAME=ttyS0
else
	CONSOLE_NAME=ttyAMA0
fi

CMDLINE=""

SOURCE_CHECK=`echo $0 | grep mklinuxmtdimg.sh`

if [ "${#SOURCE_CHECK}" -ge 1 ]; then
BUILDRETURN=exit
else
BUILDRETURN=return
fi

#===============================================
# Build Directtory
#===============================================
if [ ${#LINUX_PLATFORM_BUILDDIR} -le 1"" ]
then
LINUX_PLATFORM_BUILDDIR=BUILD_$ARCH
KERNEL_OUT_DIR=$LINUX_PLATFORM_BUILDDIR
ALS="1"
else
KERNEL_OUT_DIR=$LINUX_PLATFORM_BUILDDIR/kernel
fi

mkdir -p $KERNEL_OUT_DIR

#===============================================
# Version info
#===============================================
SVN_VER=$(svnversion)
DATE=$(date +"%y%m%d-%T")
WHOAMI=$(whoami)
#HW_VER=$(cat $WORK_ROOT/ramdisk/version.info)
#KERNEL_VERSION="[KE_VER:$WHOAMI-$SVN_VER-$DATE-$HW_VER]"
#echo "$KERNEL_VERSION"

#echo -en "initramfs use o.k. [Y/n] ? "
#echo -en "Choose Ramdisk Type: (\x1b[1;34mRAMDISK Use: 1 / Initramfs: 2 / nandfs: 3\x1b[0m) ? (default: Initramfs)  "
#read USERINPUT
#if [ "$USERINPUT" == 1 ] || [ "$USERINPUT" == 2 ] || [ "$USERINPUT" == 3 ]
#then
#	echo "Userinput: $USERINPUT"
#else
	USERINPUT="2"
#fi

if [ "$ALS" = "1" ]; then
	rm -f ./ramdisk_dummy.rom
	touch ./ramdisk_dummy.rom
	RAMDISK_IMG_PATH="./ramdisk_dummy.rom"
else
	if [ "$USERINPUT" == 1 ]
	then
		echo "RAMDISK Use"
		RAMDISK_IMG_PATH="$WORK_ROOT/ramdisk/ramdisk.rom"
	elif [ "$USERINPUT" == 2 ]
	then
		if [ -e "$RAMDISK_IMG_PATH" ]
		rm -f $WORK_ROOT/ramdisk/ramdisk_dummy.rom
		touch $WORK_ROOT/ramdisk/ramdisk_dummy.rom
		RAMDISK_IMG_PATH="$WORK_ROOT/ramdisk/ramdisk_dummy.rom"
		then
		echo "Using initramfs"
		else 
		echo -en "\x1b[1;31mno ramdisk_dummy.rom !! \n\x1b[0m"
		$BUILDRETURN 1
		fi
	elif [ "$USERINPUT" == 3 ]
	then
		if [ -e "$RAMDISK_IMG_PATH" ]
		RAMDISK_IMG_PATH="$WORK_ROOT/ramdisk/ramdisk_dummy.rom"
		then
		echo "nandfs Use"
		else 
		echo -en "\x1b[1;31mno ramdisk_dummy.rom !! \n\x1b[0m"
		$BUILDRETURN 1
		fi
	fi
fi

#rm ./arch/arm/boot/cImage
#$WORK_ROOT/util/mkimage/lz4demo -c1 arch/arm/boot/Image arch/arm/boot/cImage

#===================================
# Add Kernel CRC
#===================================
#cat $WORK_ROOT/util/tcc_crc/head.bin arch/arm/boot/cImage > arch/arm/boot/head_cImage
#$WORK_ROOT/util/tcc_crc/tcc_crc -o arch/arm/boot/cImage.rom -v 1700 arch/arm/boot/head_cImage
#KERNEL_IMG_PATH="arch/arm/boot/cImage"
KERNEL_IMG_PATH="arch/arm/boot/zImage"
KERNEL_UNCOMPRESSED_IMG_PATH="arch/arm/boot/Image"
MKBOOTIMG=./scripts/mkbootimg

if [ -n "$TCC_TARGET" ]
then
echo -e "[\x1b[1;33mMAKE Parameter\x1b[0m]"
echo -e "Architecture : $ARCH"
echo -e "Kernel Image : $KERNEL_IMG_PATH"
echo -e "Uncompressed Kernel Image : $KERNEL_UNCOMPRESSED_IMG_PATH"
echo -e "Ramdisk Image: $RAMDISK_IMG_PATH"
echo -e "BASE Address : $BASE_ADDR"
echo -e "Kernel offset: $KERNEL_OFFSET"
echo -e "boot cmdline : $CMDLINE"

# compressed image not used 
$MKBOOTIMG --kernel $KERNEL_IMG_PATH --ramdisk $RAMDISK_IMG_PATH \
	--base $BASE_ADDR --kernel_offset $KERNEL_OFFSET --cmdline "$CMDLINE" \
	--output "$KERNEL_OUT_DIR/$TCC_TARGET"_boot.img
$MKBOOTIMG --kernel $KERNEL_UNCOMPRESSED_IMG_PATH --ramdisk $RAMDISK_IMG_PATH \
	--base $BASE_ADDR --kernel_offset $KERNEL_OFFSET --cmdline "$CMDLINE" \
	--output "$KERNEL_OUT_DIR/$TCC_TARGET"_uncompressed_boot.img
#$WORK_ROOT/util/mkimage/mkmtdimg --boot "$KERNEL_OUT_DIR/$TCC_TARGET"_boot.img --output "$KERNEL_OUT_DIR/$TCC_TARGET"_mtd.rom

echo
echo -e "[\x1b[1;33mMAKE\x1b[0m] \x1b[1;31m"$KERNEL_OUT_DIR/$TCC_TARGET"_boot.img\x1b[0m"
echo -e "[\x1b[1;33mMAKE\x1b[0m] \x1b[1;31m"$KERNEL_OUT_DIR/$TCC_TARGET"_uncompressed_boot.img\x1b[0m"
#echo -e "[\x1b[1;33mMAKE\x1b[0m] \x1b[1;31m"$KERNEL_OUT_DIR/$TCC_TARGET"_mtd.rom\x1b[0m"
echo
echo "========================================================="
echo
fi

$BUILDRETURN 0
