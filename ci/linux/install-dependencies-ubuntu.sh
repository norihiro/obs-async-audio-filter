#!/bin/sh
set -ex

sudo add-apt-repository -y ppa:obsproject/obs-studio
sudo apt-get -qq update

sudo apt-get install -y \
	libc-dev-bin \
	libc6-dev git \
	build-essential \
	checkinstall \
	cmake \
	obs-studio \
	qtbase5-dev

# Dirty hack
OBS_VER="$(dpkg -s obs-studio | awk '$1=="Version:"{ gsub(/[^0-9.].*/, "", $2); print $2}')"
sudo wget -O /usr/include/obs/obs-frontend-api.h https://raw.githubusercontent.com/obsproject/obs-studio/${OBS_VER}/UI/obs-frontend-api/obs-frontend-api.h

sudo ldconfig
