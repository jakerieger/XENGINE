# Xen

## Notes

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
- Engine/
    - XEN.Shaders.xpak
    - XEN.Environment.xpak (contains IBL cubemaps, BRDF maps, etc...)
- Bin64/
    - GameDist.exe
- Data1.xpak