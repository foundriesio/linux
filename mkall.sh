#!/bin/bash

CPU_NUM=$1
RAMDISK_CONFIG=$2
if [ "$RAMDISK_CONFIG" == "" ]; then
CPU_NUM=-j1
RAM_DISK_CONFIG=$1
fi

echo "# kernel module build"
make modules $CPU_NUM
if [ $? -ne 0 ]; then
	echo "Fatal error on kernel make modules"
	cd ../kernel
	exit 1
fi

cd ../ramdisk
./tcc_mk_initramfs.sh $2
if [ $? -ne 0 ]; then
	echo "Fatal error on tcc_mk_initramfs.sh."
	echo "breaking..."
	cd ../kernel
	exit 1
fi
cd ../kernel


echo "# kernel build"
make $CPU_NUM

if [ $? -ne 0 ]; then
	echo "Fatal error on kernel make"
	cd ../kernel
	exit 1
fi

./mklinuxmtdimg.sh

if [ $LINUX_PLATFORM_SECURE = "true" ]; then
./mksecureimg.sh
fi

