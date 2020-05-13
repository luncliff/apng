#!/bin/bash
#
# References
#   - https://docs.sonarqube.org/latest/analysis/languages/cfamily/
#   - https://docs.sonarqube.org/latest/analysis/scan/sonarscanner/
#   - https://docs.travis-ci.com/user/sonarcloud/
#
build_dir=${1:-"build"}
wrapper=${2:-"build-wrapper-macosx-x86"}

# Download and unzip SonarQube Tools
zip_path="${wrapper}.zip"
if [[ -f ${zip_path} ]]; then
  unzip -qq -o ${zip_path}
else
  wget -q https://sonarcloud.io/static/cpp/${wrapper}.zip
  unzip -qq -o ${zip_path}
fi

export PATH="${PATH}:$(pwd)/${wrapper}"
echo "using wrapper: $(which ${wrapper})"

${wrapper} --version
${wrapper} --out-dir bw-output \
  cmake --build ${build_dir} --clean-first
