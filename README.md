# OBS Asynchronous Audio Filter Plugin

## Introduction

This plugin tries to fix an asynchronous audio by synchronizing to the master clock of OBS Studio.

I hope this plugin is useful to fixdebug the issue.
https://github.com/obsproject/obs-studio/issues/4600

## Properties

### Use OBS time

This plugin assumes the timestamp set by the audio source is calculated from the function `os_gettime_ns`.
If not, it is suggested to enable this option.

### Verbosity

Increase or decrease verbosity in the log file.

## Build and install
### Linux
Use cmake to build on Linux. After checkout, run these commands.
```
sed -i 's;${CMAKE_INSTALL_FULL_LIBDIR};/usr/lib;' CMakeLists.txt
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

### macOS
Use cmake to build on Linux. After checkout, run these commands.
```
mkdir build && cd build
cmake ..
make
```
