#!/bin/bash

CUR_DIR=$(cd $(dirname $0); pwd)

set -e

# 获取当前系统核心数
CPU_COUNT=$(grep -c ^processor /proc/cpuinfo)
BUILD_COUNT=$((${CPU_COUNT}-1))
echo "CPU_COUNT: $CPU_COUNT, BUILD_COUNT: $BUILD_COUNT"


build_project() {
    if [ $# -eq 0 ]; then
        return -1
    fi
    build_type=$1
    echo "build_type = $build_type"

    if [[ $build_type != "Debug" ]] && [[ $build_type != "Release" ]] && [[ $build_type != "Clean" ]]; then
        return -2;
    fi
    
    if [[ $build_type == "Clean" ]]; then
        if [ -d ${CUR_DIR}/build ]; then
            echo "rm -rf ${CUR_DIR}/build"
            rm -rf ${CUR_DIR}/build
            return 0
        fi
    fi

    if [ -d ${CUR_DIR}/build ]; then
        echo "build..."
        cd ${CUR_DIR}/build
        cmake .. -DCMAKE_BUILD_TYPE=${build_type}
        make -j${BUILD_COUNT}
    else
        echo "mkdir ${CUR_DIR}/build"
        echo "build..."
        mkdir ${CUR_DIR}/build
        cmake .. -DCMAKE_BUILD_TYPE=${build_type}
        make -j${BUILD_COUNT}
    fi
    echo "build succeed!"
    return 0
}


start(){
    type="Debug"

    if [ $# -eq 0 ]; then
       type="Debug"
    elif [ $1 == "Debug" ] || [ $1 == "debug" ]; then
        type="Debug"
    elif [ $1 == "Release" ] || [ $1 == "release" ]; then
        type="Release"
    elif [ $1 == "Clean" ] || [ $1 == "clean" ]; then
        type="Clean"
    fi
    build_project ${type}
}

start $@