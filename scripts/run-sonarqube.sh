#!/bin/bash
#
# References
#   - https://docs.sonarqube.org/latest/analysis/languages/cfamily/
#   - https://docs.sonarqube.org/latest/analysis/scan/sonarscanner/
#   - https://docs.travis-ci.com/user/sonarcloud/
#
sonar_key=${1:-"$SONAR_KEY"}
build_dir=${2:-"build"}

if [ -z ${sonar_key} ]; then
  echo "sonar_key must be specified !!!"
  echo ""
  echo "run-sonarqube.sh \${sonar_login_key}"
  echo ""
  echo "or,"
  echo ""
  echo "SONAR_KEY=\${sonar_login_key} run-sonarqube.sh"
  echo ""
  exit 1
fi

# Sonar build wrapper for Mac OS X
wrapper="build-wrapper-macosx-x86"
zip_path="${wrapper}.zip"
if [[ -f ${zip_path} ]]; then
  unzip -qq -o ${zip_path}
else
  wget -q https://sonarcloud.io/static/cpp/${wrapper}.zip
  unzip -qq -o ${zip_path}
fi
export PATH="${PATH}:$(pwd)/${wrapper}"
echo "using wrapper: $(which ${wrapper})"

# Sonar scanner cli has different name for unzipped directory
scanner="sonar-scanner"
zip_path="${scanner}-cli-4.2.0.1873-macosx.zip"
if [[ -f ${zip_path} ]]; then
  unzip -qq -o ${zip_path}
else
  wget -q https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/${zip_path}
  unzip -qq -o ${zip_path}
fi
export PATH="${PATH}:$(pwd)/${scanner}-4.2.0.1873-macosx/bin"
echo "using scanner: $(which ${scanner})"


${wrapper} --version
${wrapper} --out-dir bw-output \
  cmake --build ${build_dir} --clean-first

# Report with the properties
# ${scanner} --version
${scanner} -Dproject.settings=sonar-project.properties \
           -Dsonar.login=${sonar_key}
