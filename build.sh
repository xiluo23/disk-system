#!/bin/bash

set -e

BUILD_DIR=build

mkdir -p ${BUILD_DIR}

cd ${BUILD_DIR}

rm -rf *

cmake ..

make -j$(nproc)

echo "Build Success!"