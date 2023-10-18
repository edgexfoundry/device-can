#!/bin/sh
set -e -x

CMAKEOPTS=-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Find root directory and system type

ROOT=$(dirname $(dirname $(readlink -f $0)))
echo $ROOT
cd $ROOT

# Cmake release build

mkdir -p $ROOT/build/release
cd $ROOT/build/release
cmake $CMAKEOPTS -DCMAKE_BUILD_TYPE=Release $ROOT/src
make all 2>&1 | tee release.log

