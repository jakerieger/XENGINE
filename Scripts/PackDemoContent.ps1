# Intended to be ran from Xen2D project root: .\Scripts\PackDemoContent.ps1

& "$PSScriptRoot\RunWithEnv.ps1" Release -- PAKTool pack "$PWD\Code\XenPong\Content" -o "$PWD\build\Release\bin\XenPong\Data1.xpak"
& "$PSScriptRoot\RunWithEnv.ps1" Debug -- PAKTool pack "$PWD\Code\XenPong\Content" -o "$PWD\build\Debug\bin\XenPong\Data1.xpak"

if ($LASTEXITCODE -ne 0)
{
    throw "PAKTool failed with exit code $LASTEXITCODE."
}