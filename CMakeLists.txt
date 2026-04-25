cmake_minimum_required(VERSION 3.20)

project(liryna
    VERSION 0.1.0
    DESCRIPTION "A C game project"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE LIRYNA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(NOT LIRYNA_SOURCES)
    message(FATAL_ERROR "No C source files found. Add at least one .c file under src/.")
endif()

add_executable(liryna ${LIRYNA_SOURCES})

set(SDL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL")

if(NOT EXISTS "${SDL_SOURCE_DIR}/CMakeLists.txt")
    file(GLOB SDL_VERSIONED_DIRS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL3-*"
    )

    list(LENGTH SDL_VERSIONED_DIRS SDL_VERSIONED_DIR_COUNT)
    if(SDL_VERSIONED_DIR_COUNT EQUAL 1)
        list(GET SDL_VERSIONED_DIRS 0 SDL_SOURCE_DIR)
    elseif(SDL_VERSIONED_DIR_COUNT GREATER 1)
        message(FATAL_ERROR "Multiple versioned SDL source directories found under third_party/. Keep only one SDL3-* directory or set SDL_SOURCE_DIR explicitly.")
    endif()
endif()

if(NOT EXISTS "${SDL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SDL source not found. Expected third_party/SDL/CMakeLists.txt or one versioned third_party/SDL3-*/CMakeLists.txt directory.")
endif()

add_subdirectory("${SDL_SOURCE_DIR}" EXCLUDE_FROM_ALL)

if(TARGET SDL3::SDL3)
    set(LIRYNA_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3::SDL3-shared)
    set(LIRYNA_SDL_TARGET SDL3::SDL3-shared)
elseif(TARGET SDL3::SDL3-static)
    set(LIRYNA_SDL_TARGET SDL3::SDL3-static)
else()
    message(FATAL_ERROR "SDL was added, but no SDL3 CMake target was found.")
endif()

set(PRISM_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/prism-sdk-v0.11.6")

# Support archives extracted with an extra top-level prism-sdk-vX.Y.Z directory.
if(NOT EXISTS "${PRISM_SDK_ROOT}/include/prism.h")
    file(GLOB PRISM_SDK_NESTED_DIRS LIST_DIRECTORIES true
        "${PRISM_SDK_ROOT}/prism-sdk-v*"
    )
    list(LENGTH PRISM_SDK_NESTED_DIRS PRISM_SDK_NESTED_DIR_COUNT)
    if(PRISM_SDK_NESTED_DIR_COUNT EQUAL 1)
        list(GET PRISM_SDK_NESTED_DIRS 0 PRISM_SDK_ROOT)
    endif()
endif()

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PRISM_PLATFORM_DIR "${PRISM_SDK_ROOT}/windows/x64")
    else()
        message(FATAL_ERROR "The bundled Prism SDK does not include 32-bit Windows binaries.")
    endif()

    set(PRISM_LIBRARY "${PRISM_PLATFORM_DIR}/dynamic/release/lib/prism.lib")
    set(PRISM_RUNTIME "${PRISM_PLATFORM_DIR}/dynamic/release/bin/prism.dll")
elseif(APPLE)
    set(PRISM_LIBRARY "${PRISM_SDK_ROOT}/macos/universal/dynamic/release/lib/libprism.dylib")
    set(PRISM_RUNTIME "${PRISM_LIBRARY}")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(PRISM_PLATFORM_DIR "${PRISM_SDK_ROOT}/linux/arm64")
    else()
        set(PRISM_PLATFORM_DIR "${PRISM_SDK_ROOT}/linux/x64")
    endif()

    set(PRISM_LIBRARY "${PRISM_PLATFORM_DIR}/dynamic/release/lib/libprism.so")
    set(PRISM_RUNTIME "${PRISM_LIBRARY}")
else()
    message(FATAL_ERROR "Unsupported platform for the bundled Prism SDK.")
endif()

if(NOT EXISTS "${PRISM_SDK_ROOT}/include/prism.h")
    message(FATAL_ERROR "Prism SDK header not found at ${PRISM_SDK_ROOT}/include/prism.h")
endif()

if(NOT EXISTS "${PRISM_LIBRARY}")
    message(FATAL_ERROR "Prism library not found at ${PRISM_LIBRARY}")
endif()

add_library(prism_sdk SHARED IMPORTED GLOBAL)
set_target_properties(prism_sdk PROPERTIES
    IMPORTED_LOCATION "${PRISM_RUNTIME}"
    INTERFACE_INCLUDE_DIRECTORIES "${PRISM_SDK_ROOT}/include"
)

if(WIN32)
    set_target_properties(prism_sdk PROPERTIES
        IMPORTED_IMPLIB "${PRISM_LIBRARY}"
    )
endif()

target_include_directories(liryna
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio"
)

target_link_libraries(liryna PRIVATE prism_sdk ${LIRYNA_SDL_TARGET})
if(WIN32)
    target_link_libraries(liryna PRIVATE winmm ole32 uuid avrt)
elseif(APPLE)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
    find_library(AUDIOUNIT_LIBRARY AudioUnit REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    target_link_libraries(liryna PRIVATE
        "${COREAUDIO_LIBRARY}"
        "${AUDIOTOOLBOX_LIBRARY}"
        "${AUDIOUNIT_LIBRARY}"
        "${COREFOUNDATION_LIBRARY}"
    )
endif()
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(liryna PRIVATE Threads::Threads m dl)
endif()

if(MSVC)
    target_compile_options(liryna PRIVATE /W4)
else()
    target_compile_options(liryna PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(liryna PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET liryna POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:prism_sdk>"
        "$<TARGET_FILE_DIR:liryna>"
)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET liryna POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>"
            "$<TARGET_FILE_DIR:liryna>"
    )
endif()

include(CTest)

file(GLOB_RECURSE LIRYNA_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c"
)

if(BUILD_TESTING AND LIRYNA_TEST_SOURCES)
    add_executable(liryna_tests ${LIRYNA_TEST_SOURCES})

    target_include_directories(liryna_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    if(MSVC)
        target_compile_options(liryna_tests PRIVATE /W4)
    else()
        target_compile_options(liryna_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME liryna_tests COMMAND liryna_tests)
endif()
