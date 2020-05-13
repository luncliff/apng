#!/bin/bash
#
# Usage
#   mkdir build && pushd build
#       cmake .. -DCMAKE_CXX_FLAGS="--coverage -g -O0" -DBUILD_TESTING=true
#       cmake --build .
#       ctest
#   popd
#   bash ./scripts/run-gcov.sh  # <--- this file
#
build_dir=${1:-"build"}
pushd ${build_dir}
    for fpath in $(find $(pwd) -name *.gcda)
    do
        gcov -abcf ${fpath}
    done
popd
