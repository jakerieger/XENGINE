# Creates a new Xen distribution release (for devs only).
# Requires Inno Setup 6.7 and Gpg4Win (must be installed locally on machine)

$InnoCompiler = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path -Path $InnoCompiler))
{
    Write-Host "ISCC.exe not found. Exiting."
    exit
}

$Version = ""
$CMakePath = ".\CMakeLists.txt"
$CMakeContent = Get-Content $CMakePath -Raw

if ($CMakeContent -match 'project\s*\([^)]*VERSION\s+([0-9.]+)')
{
    $Version = $Matches[1]
    Write-Host "Engine Version: $Version"
}
else
{
    Write-Host "Could not fetch engine version. Exiting."
    exit
}

$DistPath = ".\dist\XenTech"
if (-not (Test-Path -Path $DistPath))
{
    Write-Host "No distribution found. Generating one now..."
    .\INSTALL.bat
    if (-not (Test-Path -Path $DistPath))
    {
        Write-Host "Failed to generate distribution. Exiting."
        exit
    }
}

$ReleasePath = ".\dist\v$Version"
if (Test-Path -Path $ReleasePath)
{
    Remove-Item -Force -Recurse $ReleasePath;
}
New-Item -Path $ReleasePath -ItemType Directory

$InstallerScriptPath = "$ReleasePath\XENGINE.iss"
$ReleaseFilename = "XENGINE-$Version-x64"
$InstallerScript = @"
[Setup]
AppName=XENGINE
AppVersion=$Version
DefaultDirName={localappdata}\Programs\XenTech
DefaultGroupName=XENGINE
OutputDir=.
OutputBaseFilename=$ReleaseFilename
Compression=lzma2
SolidCompression=yes
SetupIconFile=..\..\Res\xed-icon.ico
PrivilegesRequired=lowest

[Files]
Source: "..\XenTech\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Icons]
Name: "{group}\XED"; Filename: "{app}\XED\Bin64\XED.exe"
Name: "{autodesktop}\XED"; Filename: "{app}\XED\Bin64\XED.exe"; Tasks: desktopicon
"@

# Generate installer exe
Set-Content -Path $InstallerScriptPath -Value $InstallerScript -Encoding UTF8
& $InnoCompiler $InstallerScriptPath
Remove-Item $InstallerScriptPath

# Generate exe signature
& gpg --detach-sign --armor --output "$ReleasePath\$ReleaseFilename.exe.sig" "$ReleasePath\$ReleaseFilename.exe"

Write-Host ""
Write-Host "== Generated release -> $ReleasePath\$ReleaseFilename.exe"

exit $LASTEXITCODE