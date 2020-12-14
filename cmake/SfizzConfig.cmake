include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(GNUWarnings)

set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard to be used")
set(CMAKE_C_STANDARD 99 CACHE STRING "C standard to be used")

# Export the compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only install what's explicitely said
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Set Windows compatibility level to 7
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x601)
endif()

# Set macOS compatibility level
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
endif()

# Do not define macros `min` and `max`
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Find macOS system libraries
if(APPLE)
    find_library(APPLE_COREFOUNDATION_LIBRARY "CoreFoundation")
    find_library(APPLE_FOUNDATION_LIBRARY "Foundation")
    find_library(APPLE_COCOA_LIBRARY "Cocoa")
    find_library(APPLE_CARBON_LIBRARY "Carbon")
    find_library(APPLE_OPENGL_LIBRARY "OpenGL")
    find_library(APPLE_ACCELERATE_LIBRARY "Accelerate")
    find_library(APPLE_QUARTZCORE_LIBRARY "QuartzCore")
    find_library(APPLE_AUDIOTOOLBOX_LIBRARY "AudioToolbox")
    find_library(APPLE_AUDIOUNIT_LIBRARY "AudioUnit")
    find_library(APPLE_COREAUDIO_LIBRARY "CoreAudio")
    find_library(APPLE_COREMIDI_LIBRARY "CoreMIDI")
    # See https://stackoverflow.com/a/54103956
    # and https://stackoverflow.com/a/21692023
    # Apparently this is not needed in Travis CI using addons
    # but it is in Appveyor instead
    list(APPEND CMAKE_PREFIX_PATH /usr/local)
endif()

# The variable CMAKE_SYSTEM_PROCESSOR is incorrect on Visual studio...
# see https://gitlab.kitware.com/cmake/cmake/issues/15170

if(NOT SFIZZ_SYSTEM_PROCESSOR)
    if(MSVC)
        set(SFIZZ_SYSTEM_PROCESSOR "${MSVC_CXX_ARCHITECTURE_ID}")
    else()
        set(SFIZZ_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

# Add required flags for the builds
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    gw_warn(-Wall -Wextra -Wno-multichar -Werror=return-type)
    if(SFIZZ_SYSTEM_PROCESSOR MATCHES "^(i.86|x86_64)$")
        add_compile_options(-msse2)
    elseif(SFIZZ_SYSTEM_PROCESSOR MATCHES "^(arm.*)$")
        add_compile_options(-mfpu=neon)
        if(NOT ANDROID)
            add_compile_options(-mfloat-abi=hard)
        endif()
    endif()
    check_cxx_compiler_flag(-faligned-new SFIZZ_HAVE_FALIGNED_NEW)
    if(SFIZZ_HAVE_FALIGNED_NEW)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-faligned-new>)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_STANDARD 17)
    add_compile_options(/Zc:__cplusplus)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(sfizz_enable_fast_math NAME)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options("${NAME}" PRIVATE "-ffast-math")
    elseif(MSVC)
        target_compile_options("${NAME}" PRIVATE "/fp:fast")
    endif()
endfunction()

# The jsl utility library for C++
add_library(sfizz_jsl INTERFACE)
add_library(sfizz::jsl ALIAS sfizz_jsl)
target_include_directories(sfizz_jsl INTERFACE "external/jsl/include")

# The cxxopts library
add_library(sfizz_cxxopts INTERFACE)
add_library(sfizz::cxxopts ALIAS sfizz_cxxopts)
target_include_directories(sfizz_cxxopts INTERFACE "external/cxxopts")

# The sndfile library
if(SFIZZ_USE_SNDFILE OR SFIZZ_DEMOS OR SFIZZ_BENCHMARKS OR SFIZZ_RENDER)
    add_library(sfizz_sndfile INTERFACE)
    add_library(sfizz::sndfile ALIAS sfizz_sndfile)
    if(SFIZZ_USE_VCPKG OR CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        find_package(SndFile CONFIG REQUIRED)
        find_path(SNDFILE_INCLUDE_DIR "sndfile.hh")
        target_include_directories(sfizz_sndfile INTERFACE "${SNDFILE_INCLUDE_DIR}")
        target_link_libraries(sfizz_sndfile INTERFACE SndFile::sndfile)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SNDFILE "sndfile" REQUIRED)
        target_include_directories(sfizz_sndfile INTERFACE ${SNDFILE_INCLUDE_DIRS})
        if(SFIZZ_STATIC_DEPENDENCIES)
            target_link_libraries(sfizz_sndfile INTERFACE ${SNDFILE_STATIC_LIBRARIES})
        else()
            target_link_libraries(sfizz_sndfile INTERFACE ${SNDFILE_LIBRARIES})
        endif()
        link_directories(${SNDFILE_LIBRARY_DIRS})
    endif()
endif()

# The st_audiofile library
if(SFIZZ_USE_SNDFILE)
    set(ST_AUDIO_FILE_USE_SNDFILE ON CACHE BOOL "" FORCE)
    set(ST_AUDIO_FILE_EXTERNAL_SNDFILE "sfizz::sndfile" CACHE STRING ""  FORCE)
else()
    set(ST_AUDIO_FILE_USE_SNDFILE OFF CACHE BOOL "" FORCE)
    set(ST_AUDIO_FILE_EXTERNAL_SNDFILE "" CACHE STRING ""  FORCE)
endif()
add_subdirectory("external/st_audiofile" EXCLUDE_FROM_ALL)

# If we build with Clang, optionally use libc++. Enabled by default on Apple OS.
cmake_dependent_option(USE_LIBCPP "Use libc++ with clang" "${APPLE}"
    "CMAKE_CXX_COMPILER_ID MATCHES Clang" OFF)
if(USE_LIBCPP)
    add_compile_options(-stdlib=libc++)
    # Presumably need the above for linking too, maybe other options missing as well
    add_link_options(-stdlib=libc++)   # New command on CMake master, not in 3.12 release
    add_link_options(-lc++abi)   # New command on CMake master, not in 3.12 release
endif()

add_library(sfizz_pugixml STATIC "src/external/pugixml/src/pugixml.cpp")
add_library(sfizz::pugixml ALIAS sfizz_pugixml)
target_include_directories(sfizz_pugixml PUBLIC "src/external/pugixml/src")

add_library(sfizz_spline STATIC "src/external/spline/spline/spline.cpp")
add_library(sfizz::spline ALIAS sfizz_spline)
target_include_directories(sfizz_spline PUBLIC "src/external/spline")

add_library(sfizz_tunings STATIC "src/external/tunings/src/Tunings.cpp")
add_library(sfizz::tunings ALIAS sfizz_tunings)
target_include_directories(sfizz_tunings PUBLIC "src/external/tunings/include")

add_library(sfizz_hiir INTERFACE)
add_library(sfizz::hiir ALIAS sfizz_hiir)
target_include_directories(sfizz_hiir INTERFACE "src/external/hiir")

add_library(sfizz_filesystem INTERFACE)
add_library(sfizz::filesystem ALIAS sfizz_filesystem)
target_include_directories(sfizz_filesystem INTERFACE "external/filesystem/include")

add_library(sfizz_atomic INTERFACE)
add_library(sfizz::atomic ALIAS sfizz_atomic)
if(UNIX AND NOT APPLE)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic/check_libatomic.c" "int main() { return 0; }")
    try_compile(SFIZZ_LINK_LIBATOMIC "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic"
        SOURCES "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic/check_libatomic.c"
        LINK_LIBRARIES "atomic")
    if(SFIZZ_LINK_LIBATOMIC)
        target_link_libraries(sfizz_atomic INTERFACE "atomic")
    endif()
else()
    set(SFIZZ_LINK_LIBATOMIC FALSE)
endif()

# Don't show build information when building a different project
function(show_build_info_if_needed)
    if(CMAKE_PROJECT_NAME STREQUAL "sfizz")
        message(STATUS "
Project name:                  ${PROJECT_NAME}
Build type:                    ${CMAKE_BUILD_TYPE}
Build processor:               ${SFIZZ_SYSTEM_PROCESSOR}
Build using LTO:               ${ENABLE_LTO}
Build as shared library:       ${SFIZZ_SHARED}
Build JACK stand-alone client: ${SFIZZ_JACK}
Build LV2 plug-in:             ${SFIZZ_LV2}
Build LV2 user interface:      ${SFIZZ_LV2_UI}
Build VST plug-in:             ${SFIZZ_VST}
Build AU plug-in:              ${SFIZZ_AU}
Build benchmarks:              ${SFIZZ_BENCHMARKS}
Build tests:                   ${SFIZZ_TESTS}
Use sndfile:                   ${SFIZZ_USE_SNDFILE}
Use vcpkg:                     ${SFIZZ_USE_VCPKG}
Statically link dependencies:  ${SFIZZ_STATIC_DEPENDENCIES}
Link libatomic:                ${SFIZZ_LINK_LIBATOMIC}
Use clang libc++:              ${USE_LIBCPP}
Release asserts:               ${SFIZZ_RELEASE_ASSERTS}

Install prefix:                ${CMAKE_INSTALL_PREFIX}
LV2 destination directory:     ${LV2PLUGIN_INSTALL_DIR}

Compiler CXX debug flags:      ${CMAKE_CXX_FLAGS_DEBUG}
Compiler CXX release flags:    ${CMAKE_CXX_FLAGS_RELEASE}
Compiler CXX min size flags:   ${CMAKE_CXX_FLAGS_MINSIZEREL}
")
    endif()
endfunction()

find_package(Threads REQUIRED)
