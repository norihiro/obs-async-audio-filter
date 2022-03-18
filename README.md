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

If you are using this filter with other video sources,
I recommend to set your video source(s) to unbuffered mode.
| Type | Instruction |
| --- | ---
| V4L2 | Uncheck `Use Buffering` |
| Decklink | Uncheck `Use Buffering` |
| Video Capture Device on macOS | Uncheck `Use Buffering` |
| Video Capture Device on Windows | Uncheck `Buffering` |
| NDI Source | Set `Latency Mode` to `Low (experimental)` |

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
