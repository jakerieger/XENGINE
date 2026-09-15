include_guard(GLOBAL)

# Determine a single human-readable platform name plus a matching set of
# boolean flags so both CMake code and C++ code can branch on the platform.
if (WIN32)
    set(PLATFORM_NAME "Windows")
    set(PLATFORM_WINDOWS TRUE)
    set(PLATFORM_MACOS FALSE)
    set(PLATFORM_LINUX FALSE)
elseif (APPLE)
    set(PLATFORM_NAME "macOS")
    set(PLATFORM_WINDOWS FALSE)
    set(PLATFORM_MACOS TRUE)
    set(PLATFORM_LINUX FALSE)
elseif (UNIX)
    set(PLATFORM_NAME "Linux")
    set(PLATFORM_WINDOWS FALSE)
    set(PLATFORM_MACOS FALSE)
    set(PLATFORM_LINUX TRUE)
else ()
    message(FATAL_ERROR "DetectPlatform: unsupported/unrecognized target platform")
endif ()

message(STATUS "DetectPlatform: building for ${PLATFORM_NAME}")

# Make the same information available to C++ as preprocessor macros.
add_compile_definitions(
        PLATFORM_NAME="${PLATFORM_NAME}"
        PLATFORM_WINDOWS=$<BOOL:${PLATFORM_WINDOWS}>
        PLATFORM_MACOS=$<BOOL:${PLATFORM_MACOS}>
        PLATFORM_LINUX=$<BOOL:${PLATFORM_LINUX}>
)
