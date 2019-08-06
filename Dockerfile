FROM debian:buster
ENV PATH="/gcc-linaro-7.4.1-2019.02-x86_64_arm-linux-gnueabihf/bin/:$PATH" \
    ARCH="arm" \
    CROSS_COMPILE="arm-linux-gnueabihf-"
RUN \
     rm -rf /var/lib/apt/lists/* &&\
     apt-get clean -y &&\
     apt-get update -y &&\
     apt-get install -y \
     bc \
     build-essential \
     git \
     libncurses5-dev \
     lzop \
     perl\ 
     wget \
     u-boot-tools \
     tar
#     apt-get install -y uboot-mkimage \
RUN  wget -c https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/arm-linux-gnueabihf/gcc-linaro-7.4.1-2019.02-x86_64_arm-linux-gnueabihf.tar.xz &&\
     tar xvf gcc-linaro-7.4.1-2019.02-x86_64_arm-linux-gnueabihf.tar.xz
CMD ["/bin/bash"]