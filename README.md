# XENGINE

**XEN**GINE (X-Engine), or XEN is a 3D game engine for Windows written in C++ and utilizing DirectX 12.

> [!IMPORTANT]
> `bootstrap_engine.py` should be run prior to executing any engine code.

> [!WARNING]
> This is **NOT** a production-ready engine. This is a proof-of-concept developed for my own amusement.

## Building

To build XEN from source, ensure your system meets the requirements below.

### System Requirements

- Windows 11
- A GPU that supports DirectX 12
- MSVC with Windows 11 SDK
- DirectX 12 SDK (d3d12 headers, dxc.exe, DirectXMath headers)
- Python 3 installed and in PATH
- CMake >= v3.14

## License

XENGINE is currently unlicensed. Once tthe foundation has been laid, this will change.

## Notes

These are here for my own convenience.

### Project Format

- Runtime/
    - Source/
        - \<CustomComponent\>.hpp
        - \<CustomComponent\>.cpp
    - CMakeLists.txt
    - main.cpp
- Content/
    - scenes/
    - textures/
    - audio/
    - etc...
- Config/
    - EngineConfig.ini
    - InputConfig.ini
    - AudioConfig.ini

### Game Distribution Output

- Config/
    - EngineConfig.ini
    - InputConfig.ini
    - AudioConfig.ini
- EngineContent/
    - XEN.Shaders.xpak
    - XEN.Environment.xpak (contains IBL cubemaps, BRDF maps, etc...)
- Bin64/
    - GameDist.exe
- Data1.xpak