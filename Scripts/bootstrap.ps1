$cwd = $PWD # Should be project root. DO NOT execute scripts from within the 'Scripts` folder.

echo "Pulling submodules..."
git submodule update --init --recursive

echo "Patching ImGui submodule..."
Set-Location Source/Vendor/imgui
git checkout docking
git sparse-checkout init --no-cone
git sparse-checkout set '/*.h' '/*.cpp' '/backends/imgui_impl_win32.*' '/backends/imgui_impl_dx12.*'
Set-Location $cwd

echo "Done."