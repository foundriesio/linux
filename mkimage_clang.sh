echo "kernel clang build"
echo $1
make ARCH=arm64 CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-androidkernel- CC=../prebuilts/clang/host/linux-x86/clang-r353983c/bin/clang $1

