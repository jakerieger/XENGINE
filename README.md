<p align="center">
  <img alt="XENGINE-Logo" src="Res/icon.png" width="224" height="224"/>
</p>

<h1 align="center">
XENGINE
</h1>

<p align="center">
    <a href="#building">Building</a> | <a href="#changelog">Changelog</a> | <a href="#license">License</a>
</p>

**XEN**GINE (_zen-jin_), or XEN, is a DirectX 12-powered 3D game engine for Windows written in C++.

> [!NOTE]
> This is **NOT** a production-ready engine (the code will make that obvious). This is a proof-of-concept developed for
my own amusement.

## Building

> [!NOTE]
> Despite being a Win32/DirectX project, XENGINE does **not** use Visual Studio project files for anything. It is a
standard CMake project that can be edited in any editor/IDE one chooses, with builds generated via Ninja.

### System Requirements

In order to build XENGINE from source, your system must meet the following requirements:

- Windows 11
    - *XENGINE will compile and run fine on Windows 10, but Direct3D debug validation layers will not be enabled as
      Windows 10 removed support for them.*
- Windows 11 SDK
- MSVC
- CMake (>= v3.14)
- Ninja
- GPU with DirectX 12 support
- Python 3 (**optional**: for running scripts in [Scripts/](Scripts))
- VS Developer Powershell

> [!IMPORTANT]
> Before trying to compile XENGINE or any of its modules, you **must** run the [bootstrap.ps1](Scripts/bootstrap.ps1)
script (from project root). This will set up this repo's git submodules as well as compile the engine shaders:
>
> ```powershell
> .\Scripts\bootstrap.ps1
> ```
>
> If you don't run this, you will have too pull the submodules and compile engine shaders yourself before
configuring/compiling.

### Build Steps

> [!NOTE]
> Execute the following commands from within a VS Developer Powershell instance.

#### 1. Clone the repository

```powershell
git clone https://github.com/jakerieger/XENGINE
```

#### 2. Configure CMake

```powershell
cmake -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
# or
cmake -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
```

#### 3. Build XENGINE

```powershell
cmake --build build/Debug
# or
cmake --build build/Release
```

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for commit change notes.

## Dependencies

XENGINE utilizes several 3rd-party libraries for certain components of the engine and its tools. These are automatically
pulled in by git/CMake and does not require them to be installed manually
(see [CMake/FetchDeps.cmake](CMake/FetchDeps.cmake) and [Source/Vendor](Source/Vendor)).

## License

XENGINE is in very early development and is therefore unlicensed. Once a stable MVP has been developed, this will change
and this repository will be open to contributions.