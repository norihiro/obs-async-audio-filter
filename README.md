# Asynchronous Audio Filter Plugin for OBS Studio

## Introduction

This plugin attempts to fix asynchronous audio by synchronizing to the master clock of OBS Studio.

OBS Studio is running with a clock provided by the OS.
On Linux, the clock is synchronized to the NTP server.
In contrast, audio interfaces often use their own internal clocks to sample audio,
which are asynchronous relative to OBS's clock.
The difference between these clocks causes the audio buffer to eventually overflow or underflow after several minutes or more,
resulting in OBS dropping a number of audio frames or introducing silent frames.

This filter detects the clock difference and slowly adjust the compensation amount.
Since the compensation will take several minutes, distortion should not be noticable.

> [!Note]
> This filter won't resolve asynchronous issues when monitoring audio.
> In some cases, the monitoring device has yet another sampling clock other than OBS Studio or source audio.

## Background

### In short: Why Do Audio Glitches Occur in OBS Studio?

OBS Studio is designed to capture and mix audio from multiple sources simultaniously
such as microphones, capture cards, desktop audio.
Each of these sources is typically driven by its own independent hardware clock.
Even if set to the same sample rate, they drift over time, leading to glitches.

### Independent Clocks
Every audio device (USB microphones, HDMI capture cards, sound cards, etc.)
uses its own hardware clock to time audio sampling.
Even if all devices are configured to the same nominal sample rate (e.g., 48000 Hz),
their actual hardware clocks will run at slightly different speeds due to manufacturing tolerances, temperature, and environmental factors.

### Clock Drift
Over time, these tiny differences in clock speed (even a few parts per million) accumulate.
Each source provides audio data at a steady rate, according to its own clock,
but that rate does not exactly match the internal clock in OBS Studio, which is the clock of your OS.

### Buffer Overruns/Underruns
This drift causes the number of audio samples produced by each device to slowly diverge from what OBS Studio expects.
As a result, overruns or underruns will occur.
- **Overruns:** If a source's clock is slightly faster than OBS's, its buffer fills up and excess samples must be dropped, causing pops or audio artifacts.
- **Underruns:** If a source's clock is slightly slower, the buffer empties and OBS runs out of samples, resulting in gaps or dropouts.

> [!Tip]
> The underlying problem is not the mismatched sample rate settings (e.g., 44100 Hz vs. 48000 Hz),
> but unsynchronized hardware clocks even when all devices are set to the same sample rate.

### Prior Solutions

- **Word Clock**:
  The best solution is to synchronize every hawrdware to the one common clock.
  However, this requires every hardware and software to support the synchronization.
  Some enterprise hardware has the input named "word clock", however, OBS Studio does not support this for now.
- **Synchronizing to the audio device**:
  Some consumer products synchronize to the audio device clock to avoid the issue.
  However, OBS Studio supports multiple inputs, even if you have one audio input, usually there are also video inputs.
  If you synchronize to one audio device, you will suffer the asynchronous issue on other devices.
- **Asynchronous sample rate conversion**:
  Instead of expecting the sample rates to match exactly,
  the difference of the sample rates will be estimated and
  the sample rate of the audio source can be converted.
  This plugin implements this solution.

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
> Do not apply this plugin to a local file player such as Media Source and VLC Video Source.

## Build and install
### Linux
Install `libsamplerate`. Use `apt` as below on Ubuntu-22.04.
```shell
sudo apt install libsamplerate0 libsamplerate0-dev
```

Use cmake to build on Linux. After checkout, run these commands.
The flags `-D USE_ERIKD_LIBSAMPLERATE_DEPS=OFF -D USE_ERIKD_LIBSAMPLERATE_SYSTEM=ON` are optional
but recommended to ensure `libsamplerate` from the system will be linked.
```shell
mkdir build && cd build
cmake \
  -D CMAKE_INSTALL_PREFIX=/usr \
  -D USE_ERIKD_LIBSAMPLERATE_DEPS=OFF \
  -D USE_ERIKD_LIBSAMPLERATE_SYSTEM=ON \
  ..
make
sudo make install
```

If you prefer `libswresample` instead of `libsamplerate`, configure with these flags.
```shell
mkdir build && cd build
cmake \
  -D CMAKE_INSTALL_PREFIX=/usr \
  -D USE_ERIKD_LIBSAMPLERATE_DEPS=OFF \
  -D USE_ERIKD_LIBSAMPLERATE_SYSTEM=OFF \
  -D USE_FFMPEG_SWRESAMPLE=ON \
  ..
make
sudo make install
```

### macOS
Use cmake to build on macOS. After checkout with a submodule, run these commands.
```shell
mkdir build && cd build
cmake ..
make
```
