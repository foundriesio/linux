#!/bin/bash

#terminal colors
CTAG_S="\x1b[1;33m"
CTAG_S_R="\x1b[1;31m"
CTAG_E="\x1b[0m"

WORK_ROOT=`pwd`/..

sfile=../kernel/.config
str_a="CONFIG_ARCH_TCC[89]"
str_b="=y"

ARCH=$(cat $sfile | grep "$str_a" | grep "$str_b")
ARCH=${ARCH#CONFIG_ARCH_}
ARCH=${ARCH%=y}

echo $ARCH
TCC_TARGET=$ARCH

KERNEL_OUT_DIR=$LINUX_PLATFORM_BUILDDIR/kernel
NONE_SECURE_IMAGE="$KERNEL_OUT_DIR/$TCC_TARGET"_uncompressed_boot.img
SECURE_IMAGE="$KERNEL_OUT_DIR/tcsb_$TCC_TARGET"_uncompressed_boot.img
SIGNED_IMAGE="$KERNEL_OUT_DIR/signed_$TCC_TARGET"_uncompressed_boot.img

COMPRESSED_NONE_SECURE_IMAGE="$KERNEL_OUT_DIR/$TCC_TARGET"_boot.img
COMPRESSED_SECURE_IMAGE="$KERNEL_OUT_DIR/tcsb_$TCC_TARGET"_boot.img
COMPRESSED_SIGNED_IMAGE="$KERNEL_OUT_DIR/signed_$TCC_TARGET"_boot.img


SIGNTOOL=$LINUX_PLATFORM_ROOTDIR/bsp/util/tcsb/tcsb_signtool
MKTCSB=$LINUX_PLATFORM_ROOTDIR/bsp/util/tcsb/tcsb_mkimg


#######################################
# KEY Definition 
#######################################
DEF_KEYDIR=$LINUX_PLATFORM_ROOTDIR/bsp/sign_key
SIGN=sign
TYPE=BLw
SIGNALGO=ecdsa224
HASHALGO=sha224
PRIKEYFILE=$DEF_KEYDIR/tcsb_keypair.der
WRAPKEYFILE=$DEF_KEYDIR/tcsb_sbcr.bin
#######################################


if [ ! -e $DEF_KEYDIR ]; then 
 echo " there is not exist the $DEF_KEYDIR dir"
 echo " please create default key store directory and key files for signing"
 exit 4
fi

if [ ! -f $PRIKEYFILE -o ! -f $WRAPKEYFILE ]; then 
 echo " there is missing key files check key store !! "
 exit 7
fi

if [ ! -f $BOOTFILE ]; then 
 echo " there is not exist none secure boot image  in $KERNEL_OUT_DIR dir !!"
 echo " before using this signing script,  please build whole system first !!"
 exit 10
fi

CMD_MAKE_SIGNED_IMAGE="$SIGNTOOL $SIGN -type $TYPE -signalgo $SIGNALGO -hashalgo $HASHALGO
			-prikey $PRIKEYFILE -wrapkey $WRAPKEYFILE -infile $NONE_SECURE_IMAGE -outfile $SIGNED_IMAGE"
CMD_MAKE_SBOOT_IMAGE="$MKTCSB $SIGNED_IMAGE $SECURE_IMAGE "

CMD_MAKE_COMPRESSED_SIGNED_IMAGE="$SIGNTOOL $SIGN -type $TYPE -signalgo $SIGNALGO -hashalgo $HASHALGO
			-prikey $PRIKEYFILE -wrapkey $WRAPKEYFILE -infile $COMPRESSED_NONE_SECURE_IMAGE -outfile $COMPRESSED_SIGNED_IMAGE"
CMD_MAKE_COMPRESSED_SBOOT_IMAGE="$MKTCSB $COMPRESSED_SIGNED_IMAGE $COMPRESSED_SECURE_IMAGE "

echo $CMD_MAKE_SIGNED_IMAGE
$CMD_MAKE_SIGNED_IMAGE

echo $CMD_MAKE_SBOOT_IMAGE
$CMD_MAKE_SBOOT_IMAGE

echo $CMD_MAKE_COMPRESSED_SIGNED_IMAGE
$CMD_MAKE_COMPRESSED_SIGNED_IMAGE

echo $CMD_MAKE_COMPRESSED_SBOOT_IMAGE
$CMD_MAKE_COMPRESSED_SBOOT_IMAGE


rm -f $SIGNED_IMAGE
rm -f $COMPRESSED_SIGNED_IMAGE
