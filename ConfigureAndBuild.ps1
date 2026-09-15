function Enter-VsEnv
{
    param([string]$Arch = 'x64')

    if ($env:VSCMD_VER)
    {
        return
    }   # already in a dev shell

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere))
    {
        throw "vswhere.exe not found. Is Visual Studio installed?"
    }

    $installPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $installPath)
    {
        throw "No VS installation with the C++ toolset was found."
    }

    Import-Module (Join-Path $installPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')

    Enter-VsDevShell -VsInstallPath $installPath `
        -SkipAutomaticLocation `
        -DevCmdArguments "-arch=$Arch -host_arch=x64" | Out-Null
}

function Invoke-Checked
{
    param([Parameter(Mandatory)][scriptblock]$Command)

    & $Command
    if ($LASTEXITCODE -ne 0)
    {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command"
    }
}

Enter-VsEnv

Remove-Item -Force -Recurse "$PWD\build"

Invoke-Checked { cmake -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug }
Invoke-Checked { cmake -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release }

Invoke-Checked { cmake --build build/Debug --config Debug }
Invoke-Checked { cmake --build build/Release --config Release }

Invoke-Checked { & "$PWD\Scripts\PackDemoContent.ps1" }
