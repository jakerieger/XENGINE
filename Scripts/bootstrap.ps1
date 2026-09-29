$cwd = $PWD # Should be project root. DO NOT execute scripts from within the 'Scripts` folder.

echo "XENGINE bootstrapper"
echo "----------------------------------------"

function PullSubmodules
{
    echo " - Pulling submodules..."
    git submodule update --init --recursive
    echo " - Done."
}

function PatchImGui
{
    echo " - Patching ImGui submodule..."
    Set-Location Source/Vendor/imgui
    git checkout docking
    git sparse-checkout init --no-cone
    git sparse-checkout set '/*.h' '/*.cpp' '/backends/imgui_impl_win32.*' '/backends/imgui_impl_dx12.*'
    Set-Location $cwd
    echo " - Done."
}

if (-not (Test-Path -Path "$cwd\EngineContent"))
{
    PullSubmodules
}

if (-not (Test-Path -Path "$cwd\Source\Vendor\imgui\imgui.h"))
{
    PatchImGui
}

echo "----------------------------------------"
echo "Done. XENGINE can now be compiled."