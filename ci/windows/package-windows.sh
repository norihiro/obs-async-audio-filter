#! /bin/bash

set -ex
. build/ci/ci_includes.generated.sh

PackageVersion="$(git describe --tags --always)"

mkdir package
cd package

7z a "${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-$2.zip" ../release/*
cmd.exe <<EOF
iscc ..\\build\\installer-Windows.generated.iss /O. /F"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-$2-Installer"
EOF
sleep 2 && echo

sha1sum \
	"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-$2.zip" \
	"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-$2-Installer.exe"
