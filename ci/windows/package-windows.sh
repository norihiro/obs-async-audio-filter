#! /bin/bash

set -ex
. build/ci/ci_includes.generated.sh

PackageVersion="$(git describe --tags --always)"

mkdir package
cd package

cp ../LICENSE ../release/data/obs-plugins/${PLUGIN_NAME}/LICENCE-${PLUGIN_NAME}.txt
cp ../deps/libsamplerate/COPYING ../release/data/obs-plugins/${PLUGIN_NAME}/LICENCE-libsamplerate.txt

7z a "${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows.zip" ../release/*
cmd.exe <<EOF
iscc ..\\build\\installer-Windows.generated.iss /O. /F"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-Installer"
EOF
sleep 2 && echo

sha1sum \
	"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows.zip" \
	"${PLUGIN_NAME}-${PackageVersion}-obs$1-Windows-Installer.exe"
