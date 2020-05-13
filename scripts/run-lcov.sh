#!/bin/bash
#
# See Also
#   - https://github.com/codecov/example-cpp11-cmake
#   - https://medium.com/@kelvin_sp/generating-code-coverage-with-qt-5-and-gcov-on-mac-os-4999857f4676
#   - https://docs.sonarqube.org/latest/analysis/coverage/
#
# Usage
#   brew install lcov
#
#   mkdir build && pushd build
#       cmake .. -DCMAKE_CXX_FLAGS="--coverage -g -O0" -DBUILD_TESTING=true
#       cmake --build .
#       ctest
#   popd
#   bash ./scripts/run-lcov.sh build   # <--- this file
#   open ./docs/coverage/index.html
#
build_dir=${1:-"build"}
save_dir=${2:-"docs/coverage"}
info_file=${3:-"coverage.info"}

lcov --directory ${build_dir} --capture --output-file ${info_file}

lcov --remove ${info_file} '*/usr/*'          --output-file ${info_file}
lcov --remove ${info_file} '*/externals/*'   --output-file ${info_file}
lcov --remove ${info_file} '*_test.cpp'      --output-file ${info_file}
lcov --list ${info_file}

mkdir -p ${save_dir}
genhtml ${info_file} -o ${save_dir}
