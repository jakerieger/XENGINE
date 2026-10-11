#Requires -Version 7.2
<#
.SYNOPSIS
    Compiles engine shaders located in Source/Shaders and outputs them to EngineContent/Shaders.

.PARAMETER Root
    Repository root. Defaults to the parent of this script's directory
    (Scripts/ lives one level below the repo root).
#>
[CmdletBinding()]
param(
    [string] $Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

enum ShaderType {
    Graphics
    Compute
}

$GraphicsEntrypoints = @('VSMain', 'PSMain')

$ShaderProfiles = @{ vertex = 'vs_6_0'; pixel = 'ps_6_0'; compute = 'cs_6_0' }
$Entrypoints    = @{ vertex = 'VSMain'; pixel = 'PSMain'; compute = 'CSMain' }
$Suffixes       = @{ vertex = '.vs';    pixel = '.ps';    compute = '.cs' }
$Stages         = @{
    Graphics = @('vertex', 'pixel')
    Compute  = @('compute')
}

if (-not $Root) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$ShaderSources = Join-Path $Root 'Source' 'Shaders'
$ShaderOutput  = Join-Path $Root 'EngineContent' 'Shaders'

function Read-ShaderSource([System.IO.FileInfo] $File) {
    # UTF-8, invalid bytes replaced rather than throwing (matches errors="replace").
    $encoding = [System.Text.UTF8Encoding]::new($false, $false)
    return [System.IO.File]::ReadAllText($File.FullName, $encoding)
}

function Get-ShaderType([System.IO.FileInfo] $File) {
    $source = Read-ShaderSource $File
    foreach ($entry in $GraphicsEntrypoints) {
        # String.Contains is case-sensitive, like Python's `in`.
        if ($source.Contains($entry)) {
            return [ShaderType]::Graphics
        }
    }
    return [ShaderType]::Compute
}

function Invoke-Dxc([System.IO.FileInfo] $File, [string] $Stage) {
    $dxc = Get-Command 'dxc.exe' -CommandType Application -ErrorAction SilentlyContinue |
            Select-Object -First 1
    if (-not $dxc) {
        throw ('dxc.exe was not found on PATH. It ships in the Windows SDK ' +
                '(e.g. C:\Program Files (x86)\Windows Kits\10\bin\<ver>\x64).')
    }

    $outputFile = Join-Path $ShaderOutput ('xen.shader.' + $File.BaseName.ToLowerInvariant() + $Suffixes[$Stage])
    $arguments = @(
        '-T',  $ShaderProfiles[$Stage]
        '-E',  $Entrypoints[$Stage]
        '-Fo', $outputFile
        $File.FullName
    )

    $log = & $dxc.Source @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        foreach ($line in $log) {
            [Console]::Error.WriteLine("$line")
        }
        throw "Failed to compile $Stage shader: $($File.Name)"
    }
}

function Build-Shader([System.IO.FileInfo] $File, [ShaderType] $Type) {
    $source = Read-ShaderSource $File
    foreach ($stage in $Stages[$Type.ToString()]) {
        # A graphics shader only gets the stages it actually defines - a
        # depth-only pass (Shadow.hlsl) has a vertex stage and no pixel one.
        if ($Type -eq [ShaderType]::Graphics -and -not $source.Contains($Entrypoints[$stage])) {
            continue
        }
        Invoke-Dxc $File $stage
    }
    Write-Host "Compiled '$($File.BaseName)' ($Type)"
}

function Build-EngineShaders {
    Write-Host "---[ $(Split-Path -Leaf $PSCommandPath) ]---"
    Write-Host "[Source] => $ShaderSources"
    Write-Host "[Output] => $ShaderOutput"

    if (-not (Test-Path -LiteralPath $ShaderSources -PathType Container)) {
        throw "No shader source directory: $ShaderSources"
    }

    New-Item -ItemType Directory -Path $ShaderOutput -Force | Out-Null

    Get-ChildItem -LiteralPath $ShaderSources -File |
            Where-Object { $_.Extension -eq '.hlsl' } |   # -eq is case-insensitive
    Sort-Object Name |
            ForEach-Object { Build-Shader $_ (Get-ShaderType $_) }
}

try {
    Build-EngineShaders
    exit 0
}
catch {
    [Console]::Error.WriteLine("error: $($_.Exception.Message)")
    exit 1
}