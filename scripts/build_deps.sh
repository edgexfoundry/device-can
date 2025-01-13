#!/bin/sh

# Build dependencies
#
#   build_deps.sh <build-csdk>
#
#   Options:
#   build-csdk: 1 to build EdgeX C SDK, 0 to skip
#
# Assumes WORKDIR is /device-can
set -e -x

BUILD_CSDK=$1

CSDK_VERSION=4.0.0-dev.14

if [ -d deps ]
then
  exit 0
fi

mkdir deps
cd /device-can/deps

# get c-sdk from edgexfoundry and build
if [ "$BUILD_CSDK" = "1" ]
then
  cd /device-can/deps

  wget https://github.com/edgexfoundry/device-sdk-c/archive/v${CSDK_VERSION}.zip
  unzip v${CSDK_VERSION}.zip
  cd device-sdk-c-${CSDK_VERSION}

  ./scripts/build.sh
  cp -rf include/* /usr/include/
  cp build/release/c/libcsdk.so /usr/lib/

  mkdir -p /usr/share/doc/edgex-csdk
  cp Attribution.txt /usr/share/doc/edgex-csdk
  cp LICENSE /usr/share/doc/edgex-csdk
  rm -rf /device-can/deps
fi
