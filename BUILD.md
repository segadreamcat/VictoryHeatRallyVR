# Rebuilding the native bridge
The package includes prebuilt bin/VHRVR.dll and bin/openvr_api.dll. Ordinary installation only requires UndertaleModTool CLI; C++ tools are needed only to rebuild the bridge.

Use 64-bit Windows MinGW-w64 (the tested toolchain was w64devkit), the OpenVR C API header and 64-bit Windows import library, and MinHook 1.3.4 sources. Upstream repositories:
- https://github.com/ValveSoftware/openvr
- https://github.com/TsudaKageyu/minhook
- https://github.com/skeeto/w64devkit

Compile MinHook src/buffer.c, src/hook.c, src/trampoline.c and src/hde/hde64.c with gcc into object files. Include MinHook/include and src as needed. Link those objects with src/VHRVR.cpp and src/Wheel.cpp, OpenVR's Windows x64 openvr_api.lib, and d3d11, dxgi, dinput8, dxguid using g++:

```text
g++ -std=c++17 -O2 -g -Wall -Wextra -Werror -Wno-unused-variable -shared -static-libgcc -static-libstdc++ src/VHRVR.cpp src/Wheel.cpp buffer.o hook.o trampoline.o hde64.o openvr_api.lib -I <OpenVR headers> -I <MinHook include> -ld3d11 -ldxgi -ldinput8 -ldxguid -o bin/VHRVR.dll
```

Use the flat OpenVR C function-table ABI. Do not replace it with C++ virtual matrix-return calls: that previously caused an ABI-related crash.

To test the local GameMaker build without installing, pass -BuildOnly to Install.ps1. Generated game data stays in ignored build/. Never upload it.
