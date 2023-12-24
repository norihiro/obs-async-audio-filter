# OBS Asynchronous Audio Filter Plugin

## Introduction

This plugin tries to fix an asynchronous audio by synchronizing to the master clock of OBS Studio.

OBS Studio is running with a clock provided by the OS.
On Linux, the clock is synchronized to the NTP server.
On the other hand, some audio interfaces are running with their own clock to sample the audio,
which is asynchronous from the OBS's clock.
The difference of these clocks causes the audio buffer to reach overflow or underflow after several minutes or more and
causes OBS to drop a bunch of audio frames or to introduce silent frames.

This filter will detect the difference and periodically add or remove an audio sample.
Since just one sample is added or removed at each time, distortion should not be noticable.

## Properties

### Use OBS time

This plugin assumes the timestamp set by the audio source is calculated from the function `os_gettime_ns`.
If not, it is suggested to enable this option.

### Verbosity

Increase or decrease verbosity in the log file.

## Recommended settings

Recommended setting for each source type is listed below.
| Type | Source settings | This filter settings |
| --- | --- | --- |
| Audio Output Capture, Audio Input Capture | Uncheck `Use Device Timestamps` | Leave it as it is. |
| Application Audio Capture | n/a | Enable `Use OBS time instead of source time`. |
| V4L2 | Uncheck `Use Buffering` | Leave it as it is. |
| Decklink | Uncheck `Use Buffering` | Leave it as it is. |
| Video Capture Device on macOS | Uncheck `Use Buffering` | Leave it as it is. |
| Video Capture Device on Windows | Uncheck `Buffering` | Leave it as it is. |
| NDI Source | Set `Latency Mode` to `Low (experimental)` | Enable `Use OBS time instead of source time`. |

> [!NOTE]
> Do not use this plugin for local file players such as Media Source and VLC Video Source.

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
