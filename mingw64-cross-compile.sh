#!/usr/bin/env bash

USAGE='usage: '`basename $0`' boost_source_path'

if [[ ! -n ${1} ]] ; then
    echo $USAGE
    exit 1
fi

script_path="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
build_path=`pwd`
boost_source_path=`cd "$1"; pwd`

bitness=64

if [[ ${bitness} -eq 32 ]] ; then
    host=i686-w64-mingw32
elif [[ ${bitness} -eq 64 ]] ; then
    host=x86_64-w64-mingw32
fi

echo 'using gcc : mingw64 : x86_64-w64-mingw32-g++ ;' > "${build_path}/mingw64-config.jam"

cd "${boost_source_path}"
./bootstrap.sh

#${boost_source_path}/bjam --user-config=mingw64-config.jam --layout=versioned --toolset=gcc-mingw64 define=BOOST_USE_WINDOWS_H address-model=32 threadapi=win32 target-os=windows --with-filesystem --with-system --with-test boost-libs32

./bjam -j8 --build-dir="${build_path}/boost_build" --user-config="${build_path}/mingw64-config.jam" --layout=versioned --toolset=gcc-mingw64 define=BOOST_USE_WINDOWS_H address-model=64 threadapi=win32 target-os=windows --with-filesystem --with-system --with-test --prefix="${build_path}/local_root" install

cd "${build_path}"

cmake -DCMAKE_TOOLCHAIN_FILE="${script_path}/cmake/x86_64-w64-mingw32.cmake" -DLOCAL_FIND_ROOT="${build_path}/local_root" -DBoost_COMPILER="-mgw" ..
