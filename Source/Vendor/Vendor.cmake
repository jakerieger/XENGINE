set(IMGUI_SOURCES
        # Core
        ${VENDOR_ROOT}/imgui/imgui.cpp
        ${VENDOR_ROOT}/imgui/imgui_draw.cpp
        ${VENDOR_ROOT}/imgui/imgui_tables.cpp
        ${VENDOR_ROOT}/imgui/imgui_widgets.cpp
        # Backends
        ${VENDOR_ROOT}/imgui/backends/imgui_impl_dx12.cpp
        ${VENDOR_ROOT}/imgui/backends/imgui_impl_win32.cpp
)

# Included flat (<imgui.h>, <imgui_impl_dx12.h>), so both imgui/ and
# imgui/backends/ end up on Xen's public include path.
set(IMGUI_HEADERS
        ${VENDOR_ROOT}/imgui/imconfig.h
        ${VENDOR_ROOT}/imgui/imgui.h
        ${VENDOR_ROOT}/imgui/imgui_internal.h
        ${VENDOR_ROOT}/imgui/imstb_rectpack.h
        ${VENDOR_ROOT}/imgui/imstb_textedit.h
        ${VENDOR_ROOT}/imgui/imstb_truetype.h
)
set(IMGUI_BACKEND_HEADERS
        ${VENDOR_ROOT}/imgui/backends/imgui_impl_dx12.h
        ${VENDOR_ROOT}/imgui/backends/imgui_impl_win32.h
)
