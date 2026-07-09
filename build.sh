#!/usr/bin/env bash

set -e

rm -rf build
mkdir build

cmake -S . -B build

cmake --build build -j