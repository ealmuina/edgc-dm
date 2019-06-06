#!/usr/bin/env bash
mkdir -p build-dir
cd build-dir/
mkdir -p input kernel output scripts
cmake ..
make