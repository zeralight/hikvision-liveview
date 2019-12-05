# Hikvision camera control
![Screenshot](https://github.com/zeralight/hikvision-liveview/raw/master/screenshot.jpg)

This is a GUI application for streaming a Hikvision Camera and has some other features:
- PTZ Control
- Record videos
- Light/IR controls, etc.

# How to build the binary
## Prerequisites
- MSYS2 Mingw with a C++14 compiler.

## Instructions
Inside an MSYS2 shell:
- `cd src`
### To build the debug version
- `make -f Makefile-Debug`
### To build the release Version
- `make -f`
Output will be under **src/../build/hikvision-liveview.exe**

### Packaging
- Place all the **lib** content inside the **build** directory
- Software can be launched by running **build/hikvision-liveview.exe**