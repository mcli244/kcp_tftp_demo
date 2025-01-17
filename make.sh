#!/bin/bash
###
 # @Author: your name
 # @Date: 2021-09-29 11:44:09
 # @LastEditors: Please set LastEditors
 # @Description: In User Settings Edit
 # @FilePath: /open_fota/make.sh
### 


if [ $# -eq 0 ]; then
    echo "Usage: ./make.sh [arch] arch:arm or x86"
    exit 1
fi

if [ $1 == "arm" ]; then
    echo "arch:arm"
    sptl
    # export CXX=arm-linux-gnueabihf-g++
    export CC=arm-linux-gnueabihf-gcc
    export LD=arm-linux-gnueabihf-ld
    export OBJCOPY=arm-linux-gnueabihf-objcopy
    export OBJDUMP=arm-linux-gnueabihf-objdump
    export AR=arm-linux-gnueabihf-ar
    export STRIP=arm-linux-gnueabihf-strip
    export RANLIB=arm-linux-gnueabihf-ranlib
    
    export build_dir=build_$1

elif [ $1 == "x86" ]; then
    echo "arch:x86"
    export CC=gcc
    export LD=ld
    export AR=ar
    export STRIP=strip
    export OBJCOPY=objcopy
    export OBJDUMP=objdump
    export RANLIB=ranlib
    export CXX=g++

    export build_dir=build_$1
else
    echo "Usage: ./make.sh [arch] arch:arm or x86"
    exit 1
fi 

if [ -d ${build_dir} ]; then
    echo ${build_dir}" dir exist"
else
    mkdir ${build_dir}
fi
cd ${build_dir}
cmake .. 
make

